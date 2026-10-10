<#
.SYNOPSIS Accounts for all six native DxUi profiles and shares exact reusable tooling evidence.
.DESCRIPTION Runs Test-Changes.ps1 -Mode PrePush once for each architecture/configuration pair. Matching tooling
receipts are profile independent and therefore execute only once. A profile may be delegated only through the
verified open pull request described by the scoped-testing contract; otherwise it remains a local obligation.
ARM64 runtime coverage on an x64 host fails with an explicit native-host requirement instead of counting a cross-build.
#>
[CmdletBinding()]
param(
    [string] $BaseRef = '',
    [switch] $Force,
    [switch] $SkipBuild,
    [switch] $Explain
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$profiles = @(
    @{ platform='x64'; configuration='Debug' },
    @{ platform='x64'; configuration='Release' },
    @{ platform='x64'; configuration='ASan Debug' },
    @{ platform='ARM64'; configuration='Debug' },
    @{ platform='ARM64'; configuration='Release' },
    @{ platform='ARM64'; configuration='ASan Debug' }
)
$pwsh = Join-Path $PSHOME 'pwsh.exe'
if (-not (Test-Path -LiteralPath $pwsh -PathType Leaf)) { $pwsh = Join-Path $PSHOME 'pwsh' }
if (-not (Test-Path -LiteralPath $pwsh -PathType Leaf)) { throw 'PowerShell 7 executable was not found beside this process.' }
$failures = [Collections.Generic.List[string]]::new()
foreach ($profile in $profiles) {
    $start = [Diagnostics.ProcessStartInfo]::new($pwsh)
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)
    $start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)
    $arguments = @('-NoProfile','-File',(Join-Path $PSScriptRoot 'Test-Changes.ps1'),'-Mode','PrePush',
        '-Platform',$profile.platform,'-Configuration',$profile.configuration)
    if ($BaseRef) { $arguments += @('-BaseRef',$BaseRef) }
    if ($Force) { $arguments += '-Force' }
    if ($SkipBuild) { $arguments += '-SkipBuild' }
    if ($Explain) { $arguments += '-Explain' }
    foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }
    Write-Host "== PrePush $($profile.platform) $($profile.configuration)"
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $output = $stdout.GetAwaiter().GetResult()
        $errorOutput = $stderr.GetAwaiter().GetResult()
        if ($output) { Write-Host $output.TrimEnd() }
        if ($errorOutput) { Write-Host $errorOutput.TrimEnd() }
        if ($process.ExitCode -ne 0) { $failures.Add("$($profile.platform) $($profile.configuration): exit $($process.ExitCode)") }
    } finally { $process.Dispose() }
}
if ($failures.Count) { throw "PrePush accounting is incomplete: $($failures -join '; ')" }
Write-Host 'PREPUSH_ACCOUNTING_FINISHED: inspect each profile result; CI_PENDING is not passed, capability skips remain partial, and interactive/native-hardware obligations remain INTERACTIVE_NOT_RUN or NOT_RUN.'
exit 0
