# Source-restore identity shared by consumers. No build, cleanup or checkout mutation occurs here.
Set-StrictMode -Version Latest

function Get-DxUiConsumerBuildIdentity {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $DxUiRoot,
        [Parameter(Mandatory)][string] $MSBuildPath,
        [Parameter(Mandatory)][ValidateSet('x64','ARM64')][string] $Platform,
        [switch] $DisableStlAnnotations
    )
    $ErrorActionPreference = 'Stop'
    $head = (& git -C $DxUiRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cnotmatch '^[0-9a-f]{40}$') { throw 'Cannot identify the pinned DxUi source.' }
    $names = 'PlatformToolset,VCToolsInstallDir,VCToolsVersion,WindowsSdkDir,WindowsTargetPlatformVersion,PreferredToolArchitecture'
    # Evaluation only: this works before WIL restore and never compiles into the source checkout.
    $json = & $MSBuildPath (Join-Path $DxUiRoot 'src/DxUi.vcxproj') /nologo /p:Configuration=Debug "/p:Platform=$Platform" "-getProperty:$names"
    if ($LASTEXITCODE -ne 0) { throw 'Could not evaluate the DxUi toolchain identity.' }
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
        commit=$head; apiRevision=2; platform=$Platform; toolset=$properties.PlatformToolset;
        vcToolsVersion=$properties.VCToolsVersion; windowsSdkVersion=$sdk; runtimeFamily='MD';
        preferredToolArchitecture=$hostArchitecture;
        disableStlAnnotations=$DisableStlAnnotations.IsPresent;
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
