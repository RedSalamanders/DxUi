<#
.SYNOPSIS Decides whether a run of the validation workflow needs its six native jobs.
.DESCRIPTION
Run by the native-scope job of the validation workflow, in the checkout of the run's commit. A push (to main, the only branch
whose pushes the workflow validates) and a manual run always need the native jobs: consumers adopt the main commit whose latest
push run succeeded. A pull request needs them unless every path between its merge commit's first parent (the base as the merge
ref was made) and the merge ref is documentation (Get-NativeScope, Tools/NativeScope.psm1). Writes the step output native (true
or false) and a job summary that names the paths that decided. Exits 0 either way: a documentation change is a decision, not a
failure. A pull request whose paths cannot be read throws, which fails the job, and the native jobs then run anyway.
.PARAMETER EventName The event that started the run; defaults to GITHUB_EVENT_NAME.
.PARAMETER BaseRef The pull request's base branch; defaults to GITHUB_BASE_REF.
.PARAMETER Repository The checkout; defaults to the current directory.
.PARAMETER Candidate The commit validated; defaults to HEAD.
#>
[CmdletBinding()]
param(
    [string] $EventName = $env:GITHUB_EVENT_NAME,
    [string] $BaseRef = $env:GITHUB_BASE_REF,
    [string] $Repository = '.',
    [string] $Candidate = 'HEAD'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'BenchmarkGate.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'NativeScope.psm1') -Force

if ($EventName -cne 'pull_request') {
    $trigger = if ($EventName) { "A ``$EventName`` run" } else { 'A run outside a pull request' }
    Add-WorkflowSummary "## Native jobs: run`n`n$trigger always builds and tests natively: consumers adopt the main commit whose latest push run of this workflow succeeded, so that run is never cut short.`n"
    Add-WorkflowOutput 'native' 'true'
    Write-Host "Native jobs: run ($(if ($EventName) { $EventName } else { 'no event' }))"
    exit 0
}
if (-not $BaseRef) { throw 'Name the pull request''s base branch with -BaseRef (the runner sets GITHUB_BASE_REF).' }

$pair = Resolve-PullRequestPair -Repository $Repository -Candidate $Candidate -BaseRef $BaseRef
$paths = @(Get-PullRequestChangedPaths -Repository $Repository -Baseline $pair.Baseline -Candidate $pair.Candidate)
$scope = Get-NativeScope -ChangedPaths $paths

$lines = [Collections.Generic.List[string]]::new()
if (-not $scope.Native) {
    $lines.Add('## Native jobs: skipped')
    $lines.Add('')
    $lines.Add("This pull request changes $($scope.Total) path(s), all of them documentation (Markdown, specifications, plans, changelog fragments, retained measurements, the docs and gallery, skills, the license or another workflow), which the validation job checks and no native job builds, runs or reads. The rules are in ``Get-DocumentationScopeRules`` (``Tools/NativeScope.psm1``). Its merge to main still runs every native job.")
} elseif ($scope.Total -eq 0) {
    $lines.Add('## Native jobs: run')
    $lines.Add('')
    $lines.Add('No changed path could be read between the merge ref and the base it was made on, so this is not known to be a documentation change.')
} else {
    $lines.Add('## Native jobs: run')
    $lines.Add('')
    $lines.Add("This pull request changes $($scope.Total) path(s), $($scope.NativePaths.Count) of which are not documentation:")
    $lines.Add('')
    foreach ($path in @($scope.NativePaths | Select-Object -First 25)) { $lines.Add("- ``$path``") }
    if ($scope.NativePaths.Count -gt 25) { $lines.Add("- and $($scope.NativePaths.Count - 25) more") }
}
Add-WorkflowSummary (($lines -join "`n") + "`n")
Add-WorkflowOutput 'native' $(if ($scope.Native) { 'true' } else { 'false' })
Write-Host "Native jobs: $(if ($scope.Native) { 'run' } else { 'skipped' }); $($scope.Total) changed path(s), $($scope.NativePaths.Count) not documentation; compared $($pair.Baseline) ($($pair.Method)) with $($pair.Candidate)"
exit 0
