<#
.SYNOPSIS Measure a baseline against a candidate on one machine with an identical benchmark harness.
.DESCRIPTION
The baseline (A) is a revision, measured in a detached worktree under .build/paired/<run>, or an existing tree named with
BaselinePath. The candidate (B) is this checkout, another revision, or an existing tree named with CandidatePath. This
checkout's measurement driver, comparator and benchmark inputs (the harness list) are copied onto both trees, so one
harness measures them and a comparison rejects any fixture difference; a revision older than the PowerShell comparator
still gets one. A named tree is measured as it is, uncommitted work included: the harness files that differ are written
into it, with the originals saved under the run directory, and restored when the run ends. A tree made for the run has
its dependencies restored (a named tree, like this checkout, must already have them), and every tree is built unless
SkipBuild reuses its build. Then each scenario runs serially as A1, B1, B2, A2 (A = baseline, B = candidate), and that interleaved pass
repeats Repetitions times (A3, B3, B4, A4, and so on), so each side ends with 2 x Repetitions runs. Within a pass, B1/A1
and B2/A2 cross the change and A2/A1 and B2/B1 are same-source controls; those per-pass comparisons are kept for
continuity, and a control that drifts beyond a band in either direction is listed as unstable-control, as context.
The verdict is the set's: for every phase and metric, an exact two-sided Mann-Whitney U test of the baseline run
medians against the candidate run medians, with the investigation band. A metric is regressed or improved only when
p < 0.05 and the median shift exceeds the band, and any rise in an exact budget (surface bytes, replacement peak,
allocations) is regressed; a set with a regressed metric is advice-required. Every receipt and comparison is retained
under the run's reports directory with summary.json, which records each side's revision or path, commit and library
source fingerprint and each scenario's set verdict. A set that is advice-required is a finding that needs developer
advice, not a script failure; invalid evidence fails. Worktrees made for the run are left in place for inspection; the
harness overlay dirties them, so remove them with git worktree remove --force.
Revisions are refused when both name one commit. A pair with a named tree is refused when the two trees have identical
library sources (nothing to compare), which is how uncommitted work on a revision's own commit is measured against it.
.PARAMETER BaselineRevision Commit, branch or tag measured as A in a detached worktree. Give this or BaselinePath.
.PARAMETER BaselinePath Existing DxUi working tree measured as A as it is. Give this or BaselineRevision.
.PARAMETER CandidateRevision Optional commit, branch or tag measured as B in a detached worktree instead of this checkout.
.PARAMETER CandidatePath Optional existing DxUi working tree measured as B as it is instead of this checkout.
.PARAMETER Scenario One or more performance.ps1 scenarios.
.PARAMETER Repetitions How many times the A, B, B, A pass repeats, 1 to 10 (default 3), giving each side twice as many runs. Complete separation reaches p = 0.0022 with three (six runs against six) and 0.029 with two; one repetition (two runs against two) cannot reach p < 0.05.
.PARAMETER OutputDirectory Parent of the run directory; defaults to .build/paired.
.PARAMETER SkipBuild Reuses the existing build of this checkout and of named trees; a named tree's harness overlay must not have changed a compiled input, and it needs its build. Worktrees made for the run are new and always build.
#>
[CmdletBinding()]
param(
    [string] $BaselineRevision = '',
    [string] $BaselinePath = '',
    [string] $CandidateRevision = '',
    [string] $CandidatePath = '',
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateSet('Default','MultilineGrid','MultilineGridDistinct','MultilineGridRetention','MultilineGridHeap','MultilineGridHeapPaced')][string[]] $Scenario = @('Default'),
    [ValidateRange(1, 10)][int] $Repetitions = 3,
    [string] $OutputDirectory = '',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceComparison.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/PairedRun.psm1') -Force

$harnessRoot = $PSScriptRoot
# The measurement driver with its comparator, and every input performance.ps1 hashes into benchmarkSha256. Both trees
# use this checkout's copies, so a comparison between them rejects any fixture difference, and a revision older than
# the PowerShell comparator still has one.
$harness = @(Get-PairedHarness)

function Resolve-Commit([string] $Revision) {
    $commit = & git -C $harnessRoot rev-parse --verify --quiet "$Revision^{commit}"
    if ($LASTEXITCODE -ne 0 -or -not $commit) { throw "Unknown revision: $Revision" }
    return $commit.Trim()
}

function Get-HeadCommit([string] $Root) {
    $commit = & git -C $Root rev-parse HEAD
    if ($LASTEXITCODE -ne 0 -or -not $commit) { throw "Cannot identify the commit of $Root" }
    return $commit.Trim()
}

$selection = Get-PairedSelection -BaselineRevision $BaselineRevision -BaselinePath $BaselinePath -CandidateRevision $CandidateRevision -CandidatePath $CandidatePath
$sides = @($selection.Baseline, $selection.Candidate)
foreach ($side in $sides) {
    switch ($side.Kind) {
        'revision' { $side['Commit'] = Resolve-Commit $side.Spec; $side['Existing'] = $false }
        'path' { $side['Root'] = Assert-PairedTree (Resolve-Path -LiteralPath $side.Spec).ProviderPath; $side['Commit'] = Get-HeadCommit $side.Root; $side['Existing'] = $true }
        'checkout' { $side['Root'] = $harnessRoot; $side['Commit'] = Get-HeadCommit $harnessRoot; $side['Existing'] = $true }
    }
}
# Revisions and this checkout differ by commit; a pair with a named tree is judged by fingerprint once both are known.
Assert-PairedSidesDiffer $sides[0] $sides[1]
$baseline = $sides[0]
$candidate = $sides[1]
$harnessDirty = [bool](& git -C $harnessRoot status --porcelain)

if (-not $OutputDirectory) { $OutputDirectory = Join-Path $harnessRoot '.build/paired' }
$runName = '{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ'), $baseline.Commit.Substring(0, 12)
$runRoot = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $runName
if (Test-Path -LiteralPath $runRoot) { throw "The paired run directory already exists: $runRoot" }
$reports = Join-Path $runRoot 'reports'
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$created = @($sides | Where-Object { $_.Kind -eq 'revision' })
foreach ($side in $created) { $side['Root'] = Join-Path $runRoot $side.Role }

$harnessHashes = [ordered]@{}
foreach ($path in $harness) {
    $source = Join-Path $harnessRoot $path
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing benchmark harness input: $path" }
    $harnessHashes[$path] = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
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
    return [ordered]@{ sourceCommit = $receipt.sourceCommit; sourceDirty = $receipt.sourceDirty; sourceFingerprint = $receipt.sourceFingerprint
        executableSha256 = $receipt.executableSha256; benchmarkSha256 = $receipt.benchmarkSha256; medians = $medians }
}

function Invoke-Measurement([string] $Root, [string] $Name, [string] $ScenarioName) {
    $path = Join-Path $reports "$ScenarioName-$Name.json"
    # Console output must not join the function's result.
    & (Join-Path $Root 'performance.ps1') -Configuration $Configuration -Platform $Platform -Scenario $ScenarioName -OutputPath $path -SkipBuild | Out-Host
    return $path
}

function Compare-Measurement([string] $Candidate, [string] $Baseline, [string] $Name, [switch] $Control) {
    $output = Join-Path $reports "$Name.comparison.json"
    # The comparator exits 1 when it flags a regression or invalid evidence; the written status is the result.
    try {
        & (Join-Path $harnessRoot 'Tools/Compare-Performance.ps1') -Candidate $Candidate -Baseline $Baseline -Output $output | Out-Host
    } catch {
        Write-Host "Comparator failure for ${Name}: $($_.Exception.Message)"
    }
    if (-not (Test-Path -LiteralPath $output -PathType Leaf)) { throw "The comparator wrote no result: $output" }
    $comparison = Get-Content -Raw -LiteralPath $output | ConvertFrom-Json
    $status = $comparison.status
    $drift = @()
    if ($Control -and $status -ne 'invalid-evidence') {
        # The comparator tests only the regression direction. A same-source control measures noise, so a swing
        # beyond a band either way (an unchanged binary 30% faster on its second run) makes the set unstable.
        $drift = @($comparison.changes | Where-Object {
                if ($null -eq $_.changePercent) { $_.before -ne $_.after } else { [Math]::Abs([double]$_.changePercent) -gt [double]$_.noisePercent }
            } | ForEach-Object { '{0}/{1} {2}' -f $_.scenario, $_.metric, $(if ($null -eq $_.changePercent) { "$($_.before) -> $($_.after)" } else { '{0:+0.00;-0.00}%' -f [double]$_.changePercent }) })
        $status = if ($drift.Count -gt 0) { 'unstable-control' } else { 'stable-control' }
    }
    return [ordered]@{ name = $Name; status = $status; file = (Split-Path $output -Leaf); drift = $drift }
}

function Get-SideSummary([System.Collections.IDictionary] $Side) {
    return [ordered]@{
        kind = $Side.Kind
        revision = $(if ($Side.Kind -eq 'revision') { $Side.Spec } elseif ($Side.Kind -eq 'checkout') { 'HEAD' } else { $null })
        path = $Side.Root; commit = $Side.Commit; sourceFingerprint = $Side.Fingerprint
        # The harness files written into the tree; a named tree has them removed again when the run ends.
        harnessOverlay = @($Side.Overlay | Where-Object { $_['action'] -ne 'unchanged' } | ForEach-Object { '{0} {1}' -f $_['action'], $_['path'] })
    }
}

$results = @()
try {
    foreach ($side in $created) {
        & git -C $harnessRoot worktree add --detach $side.Root $side.Commit
        if ($LASTEXITCODE -ne 0) { throw "Cannot create the worktree: $($side.Root)" }
    }
    foreach ($side in $sides) {
        # A named tree is put back afterwards, so the originals are saved; a worktree made for this run is disposable.
        $side['Backup'] = if ($side.Existing) { Join-Path $runRoot "overlay-backup/$($side.Role)" } else { '' }
        $side['Overlay'] = @(Copy-HarnessOverlay -Source $harnessRoot -Target $side.Root -Paths $harness -BackupDirectory $side.Backup)
        $side['Fingerprint'] = Get-SourceFingerprint -Root $side.Root
    }
    try { Assert-PairedSidesDiffer $baseline $candidate }
    catch {
        # Nothing was built or measured; do not leave the worktrees made for this refusal behind.
        foreach ($side in $created) { & git -C $harnessRoot worktree remove --force $side.Root }
        foreach ($directory in @($reports, $runRoot)) { if (-not [IO.Directory]::GetFileSystemEntries($directory).Length) { Remove-Item -LiteralPath $directory -Force } }
        throw
    }

    foreach ($side in $created) { & (Join-Path $side.Root 'vcpkg-install.ps1') -Platform $Platform }
    foreach ($side in $sides) {
        if ($SkipBuild -and $side.Existing) {
            # Only an existing tree has a build to reuse, and only one made from the harness the receipts will name.
            $executable = Join-Path $side.Root ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
            if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "-SkipBuild needs an existing $Platform $Configuration build of the $($side.Role); there is none at $executable" }
            $changed = @(Get-OverlayCompiledChanges $side.Overlay)
            if ($changed.Count) { throw "-SkipBuild cannot reuse the $($side.Role)'s build: the harness overlay changed $($changed -join ', '), which that build does not contain. Omit -SkipBuild to build it." }
            continue
        }
        & (Join-Path $side.Root 'build.ps1') -Configuration $Configuration -Platform $Platform
    }

    $schedule = Get-PairedRunSchedule -Repetitions $Repetitions
    $roots = @{ baseline = $baseline.Root; candidate = $candidate.Root }
    foreach ($scenarioName in $Scenario) {
        $runs = [ordered]@{}
        foreach ($step in $schedule.Steps) {
            $runs[$step.Name] = Invoke-Measurement -Root $roots[$step.Side] -Name $step.Name -ScenarioName $scenarioName
        }
        $comparisons = @(foreach ($pair in $schedule.Comparisons) {
                Compare-Measurement -Candidate $runs[$pair.Candidate] -Baseline $runs[$pair.Baseline] -Name "$scenarioName-$($pair.Name)" -Control:$pair.Control
            })
        $reportsByRun = [ordered]@{}
        foreach ($name in $runs.Keys) { $reportsByRun[$name] = Get-ReportMedians -Path $runs[$name] }
        # The verdict weighs every run of a side at once, from the full receipts, not the pass-by-pass comparisons above.
        $set = try {
            Compare-PerformanceSet -Baseline @($schedule.Steps | Where-Object { $_.Side -eq 'baseline' } | ForEach-Object { Read-PerformanceReceipt $runs[$_.Name] }) `
                -Candidate @($schedule.Steps | Where-Object { $_.Side -eq 'candidate' } | ForEach-Object { Read-PerformanceReceipt $runs[$_.Name] })
        } catch {
            [ordered]@{ status = 'invalid-evidence'; error = $_.Exception.Message; metrics = @() }
        }
        $results += [ordered]@{ scenario = $scenarioName; runs = $reportsByRun; comparisons = $comparisons; set = $set }
    }

    $summary = [ordered]@{
        command = 'performance-paired.ps1'
        baseline = Get-SideSummary $baseline; candidate = Get-SideSummary $candidate
        # Kept for the summaries written before named trees: what was typed, or null for a tree, and the commit.
        baselineRevision = $(if ($baseline.Kind -eq 'path') { $null } else { $baseline.Spec }); baselineCommit = $baseline.Commit
        candidateRevision = $(if ($candidate.Kind -eq 'path') { $null } else { $candidate.Spec }); candidateCommit = $candidate.Commit
        harnessCommit = (& git -C $harnessRoot rev-parse HEAD).Trim(); harnessDirty = $harnessDirty
        configuration = $Configuration; platform = $Platform
        machine = [Environment]::MachineName; completedUtc = [DateTime]::UtcNow.ToString('o'); repetitions = $Repetitions; order = $schedule.Order
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
        # Each pass's crossings and controls, kept for continuity; the set's verdict below is the result.
        foreach ($comparison in $result.comparisons) {
            Write-Host "  $($comparison.name): $($comparison.status)"
            foreach ($entry in $comparison.drift) { Write-Host "    drift $entry" }
        }
        if ($result.set.status -eq 'invalid-evidence') { Write-Host "  Set: invalid-evidence: $($result.set.error)" }
        else { foreach ($line in Format-PerformanceSetVerdict $result.set -Label '  Set') { Write-Host $line } }
    }
    Write-Host "Paired reports: $reports"
    $invalid = @($results | ForEach-Object { $_.comparisons } | Where-Object { $_.status -eq 'invalid-evidence' })
    $invalidSets = @($results | Where-Object { $_.set.status -eq 'invalid-evidence' } | ForEach-Object { "$($_.scenario) set" })
    if ($invalid.Count -gt 0 -or $invalidSets.Count -gt 0) { throw "Invalid paired evidence: $((@($invalid | ForEach-Object { $_.name }) + $invalidSets) -join ', ')" }
    # The set decides; the passes' own flags and same-binary drift are context, listed above.
    $findings = @($results | Where-Object { $_.set.status -eq 'advice-required' } | ForEach-Object {
            '{0} ({1})' -f $_.scenario, ((@($_.set.metrics | Where-Object { $_.verdict -eq 'regressed' } | ForEach-Object { '{0}/{1}' -f $_.phase, $_.metric })) -join ', ')
        })
    if ($findings.Count -gt 0) {
        $message = "Paired findings need developer advice; regressed metrics by scenario: $($findings -join '; ')"
        # A hosted job stays green for findings; the annotation keeps them visible on the run.
        if ($env:GITHUB_ACTIONS -eq 'true') { Write-Host "::warning::$message" } else { Write-Warning $message }
    }
} finally {
    # Put every named tree back as it was found, whatever ended the run.
    foreach ($side in $sides) {
        if ($side.Contains('Overlay') -and $side['Backup']) {
            try { Restore-HarnessOverlay -Target $side.Root -Records $side.Overlay -BackupDirectory $side.Backup }
            catch { Write-Warning "Cannot restore the harness files of the $($side.Role) tree $($side.Root): $($_.Exception.Message) The originals are under $($side.Backup)." }
        }
    }
}
# A flagged comparison leaves the comparator's exit code in $LASTEXITCODE; it is a finding, not a failure.
exit 0
