<#
.SYNOPSIS Run one bounded opt-in complex-UI startup/CPU/heap diagnostic.
.DESCRIPTION Fixed 20/100/200 dirty warm checkpoints and 40-frame clean/dirty probes. Diagnostic only; no acceptance or policy verdict.
.PARAMETER Heap Include bounded process-local heap accounting outside frame windows. Run paired fresh heap-off/on processes to identify observer effects.
.PARAMETER SkipBuild Reuse only an exact successful current v2 build. Both before/after identities and the diagnostic helper closure are verified.
.PARAMETER BoundaryTimers Select the separate Default-only clear/composition/readback diagnostic, with interior QPC timers Off or On. Cannot combine with Heap.
.PARAMETER WarpRasterMode Explicit Default or CallingThread WARP execution policy for the separate v2 boundary diagnostic. Requires BoundaryTimers; canonical device creation is unchanged.
.PARAMETER BoundaryScenario Explicit Default, MultilineGrid or MultilineGridDistinct selects boundary v3 with frame-aligned owner-thread CPU probes. Requires BoundaryTimers; Scenario remains Default. CPU probes include their own overhead.
.PARAMETER BoundaryPhase Clean (default) or Dirty for explicit boundary v3. Dirty updates frames 200 through 239 independently in each command-control window.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateSet('Default','MultilineGrid')][string] $Scenario = 'Default',
    [switch] $Heap,
    [ValidateSet('','Off','On')][string] $BoundaryTimers = '',
    [ValidateSet('','Default','CallingThread')][string] $WarpRasterMode = '',
    [ValidateSet('','Default','MultilineGrid','MultilineGridDistinct')][string] $BoundaryScenario = '',
    [ValidateSet('Clean','Dirty')][string] $BoundaryPhase = 'Clean',
    [string] $OutputPath = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($BoundaryTimers -and ($Heap -or $Scenario -ne 'Default')) { throw 'Boundary diagnostics require Default without heap sampling.' }
if ($WarpRasterMode -and -not $BoundaryTimers) { throw 'WARP raster mode requires boundary diagnostics.' }
if ($BoundaryScenario -and -not $BoundaryTimers) { throw 'Boundary scenario requires boundary diagnostics.' }
if ($BoundaryPhase -ne 'Clean' -and -not $BoundaryScenario) { throw 'Dirty boundary phase requires an explicit boundary scenario.' }
$native = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Profiling diagnostics require native execution matching the requested architecture.'
}
if (-not $OutputPath) {
    $kind = if ($BoundaryTimers) { "Boundaries-$BoundaryTimers" } elseif ($Heap) { 'Heap' } else { 'NoHeap' }
    if ($WarpRasterMode) { $kind += "-$WarpRasterMode" }
    if ($BoundaryScenario) { $kind += "-v3-$BoundaryScenario-$BoundaryPhase" }
    $OutputPath = Join-Path $PSScriptRoot ".build/reports/Profiling-$Scenario-$Platform-$Configuration-$kind.json"
}
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
Invoke-ProfilingMeasurement -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -Scenario $Scenario -Heap ([bool]$Heap) -BoundaryTimers $BoundaryTimers -WarpRasterMode $WarpRasterMode -BoundaryScenario $BoundaryScenario -BoundaryPhase $BoundaryPhase -OutputPath $OutputPath
Write-Host "Diagnostic report: $OutputPath"
Write-Host "Exact native bytes: $OutputPath.raw.json"
Write-Host 'Diagnostic only; no acceptance or policy verdict.'
