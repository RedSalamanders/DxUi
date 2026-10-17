<#
.SYNOPSIS Run one bounded opt-in complex-UI startup/CPU/heap diagnostic.
.DESCRIPTION Fixed 20/100/200 dirty warm checkpoints and 40-frame clean/dirty probes. Diagnostic only; no acceptance or policy verdict.
.PARAMETER Heap Include bounded process-local heap accounting outside frame windows. Run paired fresh heap-off/on processes to identify observer effects.
.PARAMETER SkipBuild Reuse only an exact successful current v2 build. Both before/after identities and the diagnostic helper closure are verified.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateSet('Default','MultilineGrid')][string] $Scenario = 'Default',
    [switch] $Heap,
    [string] $OutputPath = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$native = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Profiling diagnostics require native execution matching the requested architecture.'
}
if (-not $OutputPath) { $OutputPath = Join-Path $PSScriptRoot ".build/reports/Profiling-$Scenario-$Platform-$Configuration-$(if ($Heap) { 'Heap' } else { 'NoHeap' }).json" }
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ([IO.Path]::GetExtension($OutputPath) -ine '.json') { throw 'Profiling output must name a JSON report.' }
$rootPrefix = [IO.Path]::GetFullPath($PSScriptRoot) + [IO.Path]::DirectorySeparatorChar
$reportsPrefix = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '.build/reports')) + [IO.Path]::DirectorySeparatorChar
if ($OutputPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -and -not $OutputPath.StartsWith($reportsPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Profiling output inside this repository must stay under .build/reports.'
}
New-Item -ItemType Directory -Path (Split-Path -Parent $OutputPath) -Force | Out-Null
foreach ($path in @($OutputPath,($OutputPath + '.raw.json'),($OutputPath + '.png'),($OutputPath + '.log'),($OutputPath + '.child.json'))) {
    if (Test-Path -LiteralPath $path -PathType Leaf) { Move-Item -LiteralPath $path -Destination ($path + '.' + [guid]::NewGuid().ToString('N') + '.stale.json') }
}
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceProfiling.psm1') -Force
if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Platform $Platform -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "Profiling build failed with exit code $LASTEXITCODE." }
}
Invoke-ProfilingMeasurement -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -Scenario $Scenario -Heap ([bool]$Heap) -OutputPath $OutputPath
Write-Host "Diagnostic report: $OutputPath"
Write-Host "Exact native bytes: $OutputPath.raw.json"
Write-Host 'Diagnostic only; no acceptance or policy verdict.'
