# Source-restore identity shared by consumers. No build, filesystem cleanup or checkout mutation occurs here.
Set-StrictMode -Version Latest

function Test-DxUiConsumerFatalException {
    param([Parameter(Mandatory)][Exception] $Exception)
    # PowerShell wraps failures from .NET calls in MethodInvocationException.
    $cause = $Exception.GetBaseException()
    return ($cause -is [OutOfMemoryException] -or $cause -is [StackOverflowException] -or
        $cause -is [AccessViolationException])
}

function ConvertTo-DxUiConsumerArgument {
    param([AllowEmptyString()][string] $Argument)
    # The public consumer module supports Windows PowerShell 5.1, where
    # ProcessStartInfo.ArgumentList is unavailable. Always quote, including "".
    # This is the same Windows escaping policy as the internal interactive
    # runner, without importing that PowerShell-7/desktop dependency.
    return '"' + ($Argument -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
}

function Start-DxUiConsumerQueryProcess {
    param([Parameter(Mandatory)][Diagnostics.ProcessStartInfo] $StartInfo)
    if ($StartInfo.UseShellExecute -or -not $StartInfo.CreateNoWindow -or
        -not $StartInfo.RedirectStandardOutput -or -not $StartInfo.RedirectStandardError) {
        throw 'Consumer identity queries require captured no-window process flags.'
    }
    $process = [Diagnostics.Process]::new()
    $transferred = $false
    try {
        $process.StartInfo = $StartInfo
        if (-not $process.Start()) { throw 'Could not start the consumer identity query.' }
        $transferred = $true
        return $process
    } finally {
        if (-not $transferred) { $process.Dispose() }
    }
}

function Invoke-DxUiConsumerNativeQuery {
    param(
        [Parameter(Mandatory)][string] $FilePath,
        [string[]] $Arguments = @(),
        [string] $WorkingDirectory = (Get-Location).ProviderPath,
        [switch] $Stream,
        [switch] $BootstrapBatch
    )
    # This standalone consumer module must not import a consumer repository or
    # the advisory credential-probe helper: identity errors need both complete
    # streams and no advisory timeout. Only the process started here is owned.
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $FilePath
    if ($BootstrapBatch) {
        # Only this fixed producer-owned batch command is admitted. No caller
        # path enters cmd syntax; the checkout is selected solely through CWD.
        if ($Arguments.Count -ne 0 -or -not $Stream -or
            -not [string]::Equals($FilePath, (Join-Path $env:SystemRoot 'System32/cmd.exe'), [StringComparison]::OrdinalIgnoreCase)) {
            throw 'The bootstrap batch requires the fixed captured cmd route.'
        }
        $start.Arguments = '/d /v:off /s /c "bootstrap-vcpkg.bat -disableMetrics"'
    } else {
        $start.Arguments = (@($Arguments | ForEach-Object { ConvertTo-DxUiConsumerArgument $_ }) -join ' ')
    }
    $start.WorkingDirectory = $WorkingDirectory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    # Match the native-command decoder selected by the calling PowerShell host.
    $start.StandardOutputEncoding = [Console]::OutputEncoding
    $start.StandardErrorEncoding = [Console]::OutputEncoding
    $process = $null
    $primaryFailure = $false
    try {
        $process = Start-DxUiConsumerQueryProcess -StartInfo $start
        if ($Stream) {
            # Await pipe tasks without a polling timer. All PowerShell and
            # console writes run on this caller thread, never an event thread.
            $pending = @($process.StandardOutput.ReadLineAsync(), $process.StandardError.ReadLineAsync())
            while ($null -ne $pending[0] -or $null -ne $pending[1]) {
                $active = @($pending | Where-Object { $null -ne $_ })
                [void][Threading.Tasks.Task]::WaitAny([Threading.Tasks.Task[]]$active)
                for ($index = 0; $index -lt 2; ++$index) {
                    if ($null -eq $pending[$index] -or -not $pending[$index].IsCompleted) { continue }
                    $line = $pending[$index].GetAwaiter().GetResult()
                    if ($null -eq $line) { $pending[$index] = $null; continue }
                    Write-DxUiConsumerProcessLine -Line $line -IsError ($index -eq 1)
                    $reader = if ($index -eq 0) { $process.StandardOutput } else { $process.StandardError }
                    $pending[$index] = $reader.ReadLineAsync()
                }
            }
            $process.WaitForExit()
            return [int]$process.ExitCode
        }
        # Start both drains before waiting; neither pipe can block the other.
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        return [pscustomobject]@{
            ExitCode = [int]$process.ExitCode
            StandardOutput = $stdout.GetAwaiter().GetResult()
            StandardError = $stderr.GetAwaiter().GetResult()
        }
    } catch {
        $primaryFailure = $true
        throw
    } finally {
        if ($null -ne $process) {
            try {
                if (-not $process.HasExited) {
                    try { $process.Kill() }
                    catch [InvalidOperationException] {
                        # The same owned process can exit between the check
                        # and Kill. An unrelated cleanup failure still escapes.
                        if (-not $process.HasExited) { throw }
                    }
                    $process.WaitForExit()
                }
            } catch {
                if (-not $primaryFailure -or (Test-DxUiConsumerFatalException $_.Exception)) { throw }
                # Preserve the primary query/pipe failure; cleanup is secondary.
                Write-Warning 'Consumer process cleanup also failed after the primary operation error.' -WarningAction Continue
            } finally { $process.Dispose() }
        }
    }
}

function Write-DxUiConsumerProcessLine {
    param([AllowEmptyString()][string] $Line, [bool] $IsError)
    if ($IsError) { [Console]::Error.WriteLine($Line) }
    else { [Console]::Out.WriteLine($Line) }
}

function Test-DxUiConsumerInteractiveConsole {
    # Unlike build.ps1's terminal-link rendering predicate, installer execution
    # must not treat WT_SESSION (or another inherited marker) as a console.
    # Keep native inheritance only for an actual unredirected interactive host.
    try {
        if (-not [Environment]::UserInteractive -or [Console]::IsInputRedirected -or
            [Console]::IsOutputRedirected -or [Console]::IsErrorRedirected) { return $false }
        if ($null -eq $Host -or $null -eq $Host.UI -or $null -eq $Host.UI.RawUI) { return $false }
        $null = $Host.UI.RawUI.WindowTitle
        return $true
    } catch {
        if (Test-DxUiConsumerFatalException $_.Exception) { throw }
        return $false
    }
}

function Get-DxUiConsumerBuildIdentity {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $DxUiRoot,
        [Parameter(Mandatory)][string] $MSBuildPath,
        [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string] $Platform,
        [switch] $DisableStlAnnotations
    )
    $ErrorActionPreference = 'Stop'
    $git = Get-Command git -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $headResult = Invoke-DxUiConsumerNativeQuery -FilePath $git.Source -Arguments @('-C', $DxUiRoot, 'rev-parse', 'HEAD')
    $head = $headResult.StandardOutput.Trim()
    if ($headResult.ExitCode -ne 0 -or $head -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot identify the pinned DxUi source.' }
    # The API revision of the pinned source, as validate_consumer.ps1 reads it.
    $apiRevision = (Get-Content -LiteralPath (Join-Path $DxUiRoot 'capabilities.json') -Raw | ConvertFrom-Json).apiRevision
    if ($apiRevision -isnot [long] -and $apiRevision -isnot [int]) { throw 'The pinned DxUi capabilities.json has no API revision.' }
    $names ='PlatformToolset,VCToolsInstallDir,VCToolsVersion,WindowsSdkDir,WindowsTargetPlatformVersion,PreferredToolArchitecture'
    # Evaluation only: this works before WIL restore and never compiles into the source checkout.
    $buildResult = Invoke-DxUiConsumerNativeQuery -FilePath $MSBuildPath -Arguments @((Join-Path $DxUiRoot 'src/DxUi.vcxproj'), '/nologo', '/p:Configuration=Debug', "/p:Platform=$Platform", "-getProperty:$names")
    $json = $buildResult.StandardOutput
    if ($buildResult.ExitCode -ne 0) { throw 'Could not evaluate the DxUi toolchain identity.' }
    $properties = ($json -join "`n" | ConvertFrom-Json).Properties
    foreach ($name in $names.Split(',')) {
        if ([string]::IsNullOrWhiteSpace([string]$properties.$name)) { throw "Missing DxUi build identity property: $name" }
    }
    $hostArchitecture = [string]$properties.PreferredToolArchitecture
    if ($hostArchitecture -notin @('x86','x64','arm64')) { throw "Unsupported compiler host architecture: $hostArchitecture" }
    $hostTool = "Host$hostArchitecture"
    $compiler = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/$Platform/cl.exe"
    $linker = Join-Path $properties.VCToolsInstallDir "bin/$hostTool/$Platform/link.exe"
    $sdk = $properties.WindowsTargetPlatformVersion.TrimEnd('\','/')
    $identity = [ordered]@{
        commit=$head; apiRevision=$apiRevision; platform=$Platform; toolset=$properties.PlatformToolset;
        vcToolsVersion=$properties.VCToolsVersion; windowsSdkVersion=$sdk; runtimeFamily='MD';
        preferredToolArchitecture=$hostArchitecture;
        disableStlAnnotations=$DisableStlAnnotations.IsPresent;
        releaseOptimizationPolicy='Gy-REF-GL-LTCG-full-v1';
        msbuildSha256=(Get-FileHash -LiteralPath $MSBuildPath -Algorithm SHA256).Hash;
        compilerSha256=(Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash;
        linkerSha256=(Get-FileHash -LiteralPath $linker -Algorithm SHA256).Hash;
        windowsHeaderSha256=(Get-FileHash -LiteralPath (Join-Path $properties.WindowsSdkDir "Include/$sdk/um/Windows.h") -Algorithm SHA256).Hash;
        kernel32Sha256=(Get-FileHash -LiteralPath (Join-Path $properties.WindowsSdkDir "Lib/$sdk/um/$Platform/kernel32.lib") -Algorithm SHA256).Hash;
        ucrtSha256=(Get-FileHash -LiteralPath (Join-Path $properties.WindowsSdkDir "Lib/$sdk/ucrt/$Platform/ucrt.lib") -Algorithm SHA256).Hash
    }
    $serialized = $identity | ConvertTo-Json -Compress
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($hasher.ComputeHash([Text.Encoding]::UTF8.GetBytes($serialized)))).Replace('-','').ToLowerInvariant() }
    finally { $hasher.Dispose() }
    [pscustomobject]@{Fingerprint=$digest; Identity=$identity}
}

Export-ModuleMember -Function Get-DxUiConsumerBuildIdentity
