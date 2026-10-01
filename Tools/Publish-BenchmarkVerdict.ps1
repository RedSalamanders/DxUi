<#
.SYNOPSIS Turns a retained paired run into the check's conclusion: the job summary, annotations, the step output and the exit code.
.DESCRIPTION
Reads the newest .build/paired/<run>/reports/summary.json (or the one under -Reports) and its same-binary control comparisons,
maps the set verdicts to pass, inconclusive, degraded or invalid (Get-BenchmarkConclusion in Tools/BenchmarkGate.psm1, which
explains the rules), appends the report to the job summary, prints the annotations, writes verdict.json and verdict.md beside
the summary (the artifact keeps them) and sets the step output conclusion.
With -Gate (a pull request) a degraded or inconclusive run exits 1, so it fails the check and an inconclusive run never reads
as a pass; without it (a run started by hand) only invalid evidence or a missing summary fails, as the manual job always did,
and the finding is reported at warning level. A flagged result is never hidden in either mode.
.PARAMETER Reports The reports directory holding summary.json; defaults to the newest run under .build/paired.
.PARAMETER Gate Fail on a degraded or inconclusive run.
.PARAMETER StrictControls Any unstable same-binary control makes its scenario inconclusive (the literal all-metrics reading).
#>
[CmdletBinding()]
param(
    [string] $Reports = '',
    [switch] $Gate,
    [switch] $StrictControls
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'BenchmarkGate.psm1') -Force
$root = Split-Path -Parent $PSScriptRoot

function Stop-WithoutVerdict([string] $Message) {
    # Whatever the trigger, no verdict is a failure: it proves nothing about performance, and the summary says why.
    Add-WorkflowSummary "## Paired benchmark: no verdict`n`n$Message`n"
    Write-Host (Format-WorkflowCommand 'error' 'Paired benchmark: no verdict' $Message)
    Add-WorkflowOutput 'conclusion' 'none'
    exit 1
}

$summaryPath = if ($Reports) { Join-Path ([IO.Path]::GetFullPath($Reports)) 'summary.json' } else { Find-PairedSummary -Root $root }
if (-not $summaryPath -or -not (Test-Path -LiteralPath $summaryPath -PathType Leaf)) {
    Stop-WithoutVerdict 'The paired run wrote no summary.json, so there is no verdict: it failed before it finished measuring. The log of the measurement step says why (a baseline that does not build with this checkout''s harness, a restore failure or a timeout).'
}

$reportsDirectory = Split-Path -Parent $summaryPath
try { $summary = Read-GateJson $summaryPath } catch { Stop-WithoutVerdict "summary.json could not be read, so there is no verdict: $($_.Exception.Message)" }
$conclusion = Get-BenchmarkConclusion -Summary $summary -ReportsDirectory $reportsDirectory -StrictControls:$StrictControls
$trigger = switch ($env:GITHUB_EVENT_NAME) {
    'pull_request' { "pull request merge ref ($($env:GITHUB_REF))" }
    'workflow_dispatch' { "manual dispatch on $($env:GITHUB_REF_NAME)" }
    default { [string]$env:GITHUB_EVENT_NAME }
}
$markdown = ConvertTo-BenchmarkMarkdown -Conclusion $conclusion -Summary $summary -Event $trigger -Pair $env:BENCHMARK_PAIR -Gate:$Gate -Hosted:($env:GITHUB_ACTIONS -eq 'true')
Add-WorkflowSummary $markdown
foreach ($line in Get-BenchmarkAnnotations -Conclusion $conclusion -Gate:$Gate) { Write-Host $line }
[IO.File]::WriteAllText((Join-Path $reportsDirectory 'verdict.md'), $markdown, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $reportsDirectory 'verdict.json'), ((ConvertTo-Json -InputObject $conclusion -Depth 8) + "`n"), [Text.UTF8Encoding]::new($false))
Add-WorkflowOutput 'conclusion' $conclusion.Conclusion
Write-Host "Paired benchmark conclusion: $($conclusion.Conclusion) ($(Get-BenchmarkHeadline $conclusion.Conclusion)); summary $summaryPath"

if ($conclusion.Conclusion -ceq 'invalid') { exit 1 }
if ($conclusion.Conclusion -cne 'pass' -and $Gate) { exit 1 }
exit 0
