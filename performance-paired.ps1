<#
.SYNOPSIS Measure a baseline revision against a candidate on one machine with an identical benchmark harness.
.DESCRIPTION
Creates a detached worktree of BaselineRevision under .build/paired/<run>, copies this checkout's measurement
driver and benchmark inputs into it, restores and builds both trees, then runs each scenario serially as A1, B1,
B2, A2 (A = baseline, B = candidate). The candidate is this checkout, or a second detached worktree when
CandidateRevision is given; both historical trees then use this checkout's harness. B1/A1 and B2/A2 cross the
change; A2/A1 and B2/B1 are same-source controls. Every receipt and comparison is retained under the run's
reports directory with summary.json. Flagged comparisons are findings that need developer advice, not script
failures; invalid evidence fails. Worktrees are left in place for inspection; remove them with git worktree remove.
.PARAMETER BaselineRevision Commit, branch or tag measured as A.
.PARAMETER CandidateRevision Optional commit, branch or tag measured as B instead of this checkout.
.PARAMETER Scenario One or more performance.ps1 scenarios.
.PARAMETER OutputDirectory Parent of the run directory; defaults to .build/paired.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $BaselineRevision,
    [string] $CandidateRevision = '',
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateSet('Default','MultilineGrid','MultilineGridRetention','MultilineGridHeap','MultilineGridHeapPaced')][string[]] $Scenario = @('Default'),
    [string] $OutputDirectory = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# The measurement driver and every input performance.ps1 hashes into benchmarkSha256. Both builds use this
# checkout's copies, so a comparison between them rejects any fixture difference.
$harness = @('performance.ps1', 'Tests/Embedded/BenchmarkMain.h', 'Tests/Embedded/ComplexUiBenchmark.h',
    'Tests/Support/HeapDiagnostic.h', 'Samples/ComplexUi/ComplexUiScene.h', 'Samples/EmbeddedControls/GraphicsFixture.h')

function Resolve-Commit([string] $Revision) {
    $commit = & git -C $PSScriptRoot rev-parse --verify --quiet "$Revision^{commit}"
    if ($LASTEXITCODE -ne 0 -or -not $commit) { throw "Unknown revision: $Revision" }
    return $commit.Trim()
}

$harnessRoot = $PSScriptRoot
$baselineCommit = Resolve-Commit $BaselineRevision
$candidateCommit = if ($CandidateRevision) { Resolve-Commit $CandidateRevision } else { (& git -C $harnessRoot rev-parse HEAD).Trim() }
if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the candidate commit.' }
if ($baselineCommit -eq $candidateCommit) { throw 'The baseline and candidate are the same commit.' }
$harnessDirty = [bool](& git -C $harnessRoot status --porcelain)

if (-not $OutputDirectory) { $OutputDirectory = Join-Path $harnessRoot '.build/paired' }
$runName = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'), $baselineCommit.Substring(0, 12)
$runRoot = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $runName
if (Test-Path -LiteralPath $runRoot) { throw "The paired run directory already exists: $runRoot" }
$baselineRoot = Join-Path $runRoot 'baseline'
$candidateRoot = if ($CandidateRevision) { Join-Path $runRoot 'candidate' } else { $harnessRoot }
$reports = Join-Path $runRoot 'reports'
New-Item -ItemType Directory -Path $reports -Force | Out-Null

$harnessHashes = [ordered]@{}
foreach ($path in $harness) {
    $source = Join-Path $harnessRoot $path
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing benchmark harness input: $path" }
    $harnessHashes[$path] = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
}
$worktrees = @(, @($baselineRoot, $baselineCommit))
if ($CandidateRevision) { $worktrees += , @($candidateRoot, $candidateCommit) }
foreach ($worktree in $worktrees) {
    & git -C $harnessRoot worktree add --detach $worktree[0] $worktree[1]
    if ($LASTEXITCODE -ne 0) { throw "Cannot create the worktree: $($worktree[0])" }
    foreach ($path in $harness) {
        $target = Join-Path $worktree[0] $path
        New-Item -ItemType Directory -Path (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $harnessRoot $path) -Destination $target -Force
    }
}

if (-not $SkipBuild) {
    foreach ($worktree in $worktrees) { & (Join-Path $worktree[0] 'vcpkg-install.ps1') -Platform $Platform }
    foreach ($root in @($baselineRoot, $candidateRoot)) {
        & (Join-Path $root 'build.ps1') -Configuration $Configuration -Platform $Platform
    }
}

function Get-Median([double[]] $Values) {
    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

function Get-ReportMedians([string] $Path) {
    $receipt = Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json
    $medians = [ordered]@{}
    foreach ($entry in $receipt.scenarios) {
        $values = [ordered]@{}
        foreach ($metric in @('fps', 'frameP95Ms', 'privateBytes', 'workingSetBytes', 'cppAllocations', 'surfaceBytes')) {
            $values[$metric] = Get-Median @($entry.rounds | ForEach-Object { [double]$_.$metric })
        }
        $medians[$entry.name] = $values
    }
    return [ordered]@{ sourceCommit = $receipt.sourceCommit; sourceDirty = $receipt.sourceDirty
        executableSha256 = $receipt.executableSha256; benchmarkSha256 = $receipt.benchmarkSha256; medians = $medians }
}

function Invoke-Measurement([string] $Root, [string] $Name, [string] $ScenarioName) {
    $path = Join-Path $reports "$ScenarioName-$Name.json"
    # Console output must not join the function's result.
    & (Join-Path $Root 'performance.ps1') -Configuration $Configuration -Platform $Platform -Scenario $ScenarioName -OutputPath $path -SkipBuild | Out-Host
    return $path
}

function Compare-Measurement([string] $Candidate, [string] $Baseline, [string] $Name) {
    $output = Join-Path $reports "$Name.comparison.json"
    # Invoke-Python throws when the comparator flags a regression; the written status is the result.
    try {
        & (Join-Path $harnessRoot 'Tools/Invoke-Python.ps1') -Script (Join-Path $harnessRoot 'Tools/compare_performance.py') `
            -Arguments @($Candidate, '--baseline', $Baseline, '--output', $output) | Out-Host
    } catch {
        Write-Host "Comparator exit for ${Name}: $($_.Exception.Message)"
    }
    if (-not (Test-Path -LiteralPath $output -PathType Leaf)) { throw "The comparator wrote no result: $output" }
    return [ordered]@{ name = $Name; status = (Get-Content -Raw -LiteralPath $output | ConvertFrom-Json).status; file = (Split-Path $output -Leaf) }
}

$results = @()
foreach ($scenarioName in $Scenario) {
    $runs = [ordered]@{}
    foreach ($step in @(@('A1', $baselineRoot), @('B1', $candidateRoot), @('B2', $candidateRoot), @('A2', $baselineRoot))) {
        $runs[$step[0]] = Invoke-Measurement -Root $step[1] -Name $step[0] -ScenarioName $scenarioName
    }
    $comparisons = @(
        Compare-Measurement -Candidate $runs.B1 -Baseline $runs.A1 -Name "$scenarioName-B1-vs-A1"
        Compare-Measurement -Candidate $runs.B2 -Baseline $runs.A2 -Name "$scenarioName-B2-vs-A2"
        Compare-Measurement -Candidate $runs.A2 -Baseline $runs.A1 -Name "$scenarioName-A2-vs-A1-control"
        Compare-Measurement -Candidate $runs.B2 -Baseline $runs.B1 -Name "$scenarioName-B2-vs-B1-control"
    )
    $reportsByRun = [ordered]@{}
    foreach ($name in $runs.Keys) { $reportsByRun[$name] = Get-ReportMedians -Path $runs[$name] }
    $results += [ordered]@{ scenario = $scenarioName; runs = $reportsByRun; comparisons = $comparisons }
}

$summary = [ordered]@{
    command = 'performance-paired.ps1'; baselineRevision = $BaselineRevision; baselineCommit = $baselineCommit
    candidateRevision = $(if ($CandidateRevision) { $CandidateRevision } else { 'HEAD' }); candidateCommit = $candidateCommit
    harnessCommit = (& git -C $harnessRoot rev-parse HEAD).Trim(); harnessDirty = $harnessDirty
    configuration = $Configuration; platform = $Platform
    machine = [Environment]::MachineName; completedUtc = [DateTime]::UtcNow.ToString('o'); order = 'A1, B1, B2, A2'
    harness = $harnessHashes; scenarios = $results
}
$summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $reports 'summary.json') -Encoding utf8

foreach ($result in $results) {
    Write-Host "Scenario $($result.scenario):"
    foreach ($name in $result.runs.Keys) {
        foreach ($phase in $result.runs[$name].medians.Keys) {
            $m = $result.runs[$name].medians[$phase]
            Write-Host ('  {0} {1,-5} fps {2,9:N3}  private {3,12:N0}  working set {4,12:N0}' -f $name, $phase, $m.fps, $m.privateBytes, $m.workingSetBytes)
        }
    }
    foreach ($comparison in $result.comparisons) { Write-Host "  $($comparison.name): $($comparison.status)" }
}
Write-Host "Paired reports: $reports"
$invalid = @($results | ForEach-Object { $_.comparisons } | Where-Object { $_.status -eq 'invalid-evidence' })
if ($invalid.Count -gt 0) { throw "Invalid paired evidence: $(($invalid | ForEach-Object { $_.name }) -join ', ')" }
# A flagged comparison leaves the comparator's exit code in $LASTEXITCODE; it is a finding, not a failure.
exit 0
