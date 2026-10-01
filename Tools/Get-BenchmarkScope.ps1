<#
.SYNOPSIS Decides whether a pull request can change what the paired benchmark measures, and which two commits it compares.
.DESCRIPTION
Run in the checkout of a pull request's merge ref by the benchmark-scope job of the validation workflow. The candidate is the
checkout, the baseline is the merge commit's first parent (the base branch as the merge ref was made), and the paths between
them are matched against the rules of Get-BenchmarkScopeRules (Tools/BenchmarkGate.psm1). Writes the step outputs relevant
(true or false), baseline, candidate and method for the paired-benchmark job, and a job summary that names the files that
decided. Exits 0 either way: skipping is a decision, not a failure.
.PARAMETER BaseRef The pull request's base branch; defaults to GITHUB_BASE_REF.
.PARAMETER Repository The checkout; defaults to the current directory.
.PARAMETER Candidate The commit measured; defaults to HEAD.
#>
[CmdletBinding()]
param(
    [string] $BaseRef = $env:GITHUB_BASE_REF,
    [string] $Repository = '.',
    [string] $Candidate = 'HEAD'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'BenchmarkGate.psm1') -Force
if (-not $BaseRef) { throw 'Name the pull request''s base branch with -BaseRef (the runner sets GITHUB_BASE_REF).' }

$pair = Resolve-PullRequestPair -Repository $Repository -Candidate $Candidate -BaseRef $BaseRef
$paths = @(Get-PullRequestChangedPaths -Repository $Repository -Baseline $pair.Baseline -Candidate $pair.Candidate)
$scope = Get-BenchmarkScope -ChangedPaths $paths

$short = { param([string] $Commit) $Commit.Substring(0, [Math]::Min(12, $Commit.Length)) }
$lines = [Collections.Generic.List[string]]::new()
if ($scope.Relevant) {
    $lines.Add('## Paired benchmark scope: measured')
    $lines.Add('')
    $lines.Add("This pull request changes $($scope.Total) path(s), $($scope.Matches.Count) of which the paired benchmark measures, so the hosted run compares ``$(& $short $pair.Baseline)`` ($($pair.Method)) with ``$(& $short $pair.Candidate)`` (the merge ref).")
    $lines.Add('')
    $lines.Add('| Changed path | Why it is measured |')
    $lines.Add('|---|---|')
    foreach ($hit in @($scope.Matches | Select-Object -First 25)) { $lines.Add("| ``$($hit.Path)`` | $($hit.Reason) |") }
    if ($scope.Matches.Count -gt 25) { $lines.Add("| and $($scope.Matches.Count - 25) more | |") }
} else {
    $lines.Add('## Paired benchmark scope: skipped')
    $lines.Add('')
    $lines.Add("This pull request changes $($scope.Total) path(s) and none of them can change what the paired benchmark measures (the library, its build, the benchmark executable and fixtures, the measurement tooling and this workflow), so the hosted run is skipped. The rules are in ``Get-BenchmarkScopeRules`` (``Tools/BenchmarkGate.psm1``); a run can still be started by hand with the ``benchmark_baseline`` input.")
}
Add-WorkflowSummary (($lines -join "`n") + "`n")
Add-WorkflowOutput 'relevant' $(if ($scope.Relevant) { 'true' } else { 'false' })
Add-WorkflowOutput 'baseline' $pair.Baseline
Add-WorkflowOutput 'candidate' $pair.Candidate
Add-WorkflowOutput 'method' $pair.Method
Write-Host "Paired benchmark scope: $(if ($scope.Relevant) { 'measured' } else { 'skipped' }); $($scope.Total) changed path(s), $($scope.Matches.Count) measured; baseline $($pair.Baseline) ($($pair.Method)), candidate $($pair.Candidate)"
exit 0
