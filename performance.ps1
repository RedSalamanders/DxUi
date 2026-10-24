<#
.SYNOPSIS Measure completed complex-UI WARP frames and resources, optionally comparing a matched baseline.
.PARAMETER Scenario Default uses single-line grid cells. MultilineGrid uses French multiline cells; MultilineGridRetention adds six complete scroll passes with resource samples.
MultilineGridHeap adds process-local heap walks outside timed rounds for attribution; its retention timing is diagnostic, not performance acceptance.
MultilineGridHeapPaced additionally targets 50 retention frames/second to distinguish frame-count retention from wall-clock cleanup effects.
MultilineGridDistinct gives every grid cell its own French multiline text, so a layout cache must hold one layout per visible cell.
.PARAMETER SkipBuild Reuse only a successful v2 default-output build attested against its complete per-side compiled projects, resolved compiler/shader/link profiles and actual artifacts. Older attestations require a fresh build. Every measurement verifies its complete identity before and after native execution.
.PARAMETER OutputPath Logical receipt path retained for evidence reads and diagnostics; the native Windows argument uses an extended-length spelling when needed.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateSet('Default','MultilineGrid','MultilineGridDistinct','MultilineGridRetention','MultilineGridHeap','MultilineGridHeapPaced')][string] $Scenario = 'Default',
    [string] $OutputPath = '',
    [string] $Baseline = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# The comparator module also owns the receipt's source fingerprint and benchmark inputs; a paired run copies it with this file.
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceComparison.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformancePolicy.psm1') -Force
$native = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Performance evidence requires native execution matching the requested architecture.'
}
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
if (-not $OutputPath) {
    # One default receipt per scenario: a MultilineGrid run must not overwrite a retained Default receipt.
    $scenarioSuffix = if ($Scenario -eq 'Default') { '' } else { "-$Scenario" }
    $OutputPath     = Join-Path $PSScriptRoot ".build/reports/Performance-$Platform-$Configuration$scenarioSuffix.json"
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)
if ($Baseline) {
    $Baseline = [IO.Path]::GetFullPath($Baseline)
    if (-not (Test-Path -LiteralPath $Baseline -PathType Leaf)) { throw "Missing performance baseline: $Baseline" }
    if ($Baseline -eq $OutputPath) { throw 'Baseline and candidate paths must differ.' }
}
New-Item -ItemType Directory -Path (Split-Path $OutputPath) -Force | Out-Null
$exe = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
Push-Location $PSScriptRoot
try {
    $benchmarkArgument = switch ($Scenario) {
        'MultilineGrid' { '--benchmark-multiline-grid' }
        'MultilineGridDistinct' { '--benchmark-multiline-grid-distinct' }
        'MultilineGridRetention' { '--benchmark-multiline-grid-retention' }
        'MultilineGridHeap' { '--benchmark-multiline-grid-heap' }
        'MultilineGridHeapPaced' { '--benchmark-multiline-grid-heap-paced' }
        default { '--benchmark' }
    }
    # A receipt left at this path by an earlier run must never be re-stamped as this run's evidence: an executable
    # that does not know the scenario flag runs its functional tests, exits 0 and writes nothing.
    if (Test-Path -LiteralPath $OutputPath -PathType Leaf) { Remove-Item -LiteralPath $OutputPath }
    $before = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
    Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $before
    try {
        # Keep the logical evidence path; only the Windows native argument needs the extended-length spelling.
        $nativeOutputPath = if ($OutputPath.StartsWith('\\?\', [StringComparison]::Ordinal)) { $OutputPath }
            elseif ($OutputPath.StartsWith('\\', [StringComparison]::Ordinal)) { '\\?\UNC\' + $OutputPath.Substring(2) }
            else { '\\?\' + $OutputPath }
        & $exe $benchmarkArgument $nativeOutputPath
        if ($LASTEXITCODE -ne 0) { throw "Complex-UI benchmark failed with exit code $LASTEXITCODE. Executable: $exe" }
        if (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf)) { throw "The benchmark wrote no receipt for scenario ${Scenario}: $OutputPath. Executable: $exe" }
        $completedUtc = [DateTime]::UtcNow.ToString('o')
        $after = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
        Assert-PerformanceMeasurementState -Before $before -After $after
        Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $after
    } catch {
        # Retain invalid native samples separately; never leave a qualifying receipt at the requested path.
        if (Test-Path -LiteralPath $OutputPath -PathType Leaf) {
            Move-Item -LiteralPath $OutputPath -Destination ($OutputPath + '.' + [guid]::NewGuid().ToString('N') + '.invalid.json')
        }
        throw
    }
    $receipt = Get-Content -Raw -LiteralPath $OutputPath | ConvertFrom-Json -AsHashtable
    foreach ($key in $before.Keys) { $receipt[$key] = $before[$key] }
    $receipt.buildSkipped = [bool]$SkipBuild
    $receipt.completedUtc = $completedUtc
    $receipt.workloadOwner = 'DxUi'
    $receipt.measurement = 'Completed offscreen WARP frames, including clear and blocking one-pixel readback; no swap-chain presentation or vsync.'
    $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $OutputPath -Encoding utf8
    $comparison = $OutputPath + '.comparison.json'
    & (Join-Path $PSScriptRoot 'Tools/Compare-Performance.ps1') -Candidate $OutputPath -Baseline $Baseline -Output $comparison
    if ($LASTEXITCODE -ne 0) { throw "The performance comparison requires advice or found invalid evidence: $comparison" }
    Write-Host "Performance receipt: $OutputPath"
} finally { Pop-Location }
