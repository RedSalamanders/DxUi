<#
.SYNOPSIS Verifies the fixed aggregate CI check against every required job result.
.DESCRIPTION A skipped native matrix is accepted only after a successful PR scope decision explicitly says native=false.
Paired benchmarks are required for every pull request and are internally skipped only after a successful scope decision.
When the workflow names the pull request's base and merge commits (EXPECTED_BASE_SHA, EXPECTED_MERGE_SHA), a measured pull
request's paired job must have compared exactly them (PAIRED_BASELINE, PAIRED_CANDIDATE). When a separate verdict job judges
the paired run (VERDICT_RESULT, VERDICT_CONCLUSION), it must have succeeded with a pass for a measured pull request, succeeded
for a manual baseline, and been skipped for everything else.
#>
[CmdletBinding()]
param(
    [string] $EventName = $env:EVENT_NAME,
    [string] $ValidationResult = $env:VALIDATION_RESULT,
    [string] $WindowsToolingResult = $env:WINDOWS_TOOLING_RESULT,
    [string] $FormatResult = $env:FORMAT_RESULT,
    [string] $NativeScopeResult = $env:NATIVE_SCOPE_RESULT,
    [string] $NativeScopeOutput = $env:NATIVE_SCOPE_OUTPUT,
    [string] $NativeResult = $env:NATIVE_RESULT,
    [string] $PairedResult = $env:PAIRED_RESULT,
    [string] $PairedRelevant = $env:PAIRED_RELEVANT,
    [string] $BenchmarkBaseline = $env:BENCHMARK_BASELINE,
    [string] $ExpectedBaseSha = $env:EXPECTED_BASE_SHA,
    [string] $ExpectedMergeSha = $env:EXPECTED_MERGE_SHA,
    [string] $PairedBaseline = $env:PAIRED_BASELINE,
    [string] $PairedCandidate = $env:PAIRED_CANDIDATE,
    [string] $VerdictResult = $env:VERDICT_RESULT,
    [string] $VerdictConclusion = $env:VERDICT_CONCLUSION
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Require-Success([string] $Name, [string] $Result) {
    if ($Result -cne 'success') { throw "$Name must succeed; GitHub reported '$Result'." }
}

Require-Success 'Ubuntu validation/tooling' $ValidationResult
Require-Success 'Windows tooling' $WindowsToolingResult
Require-Success 'format' $FormatResult
Require-Success 'native scope classifier' $NativeScopeResult

if ($NativeScopeOutput -cnotin @('true','false')) { throw "Native classifier output must be true or false, got '$NativeScopeOutput'." }
if ($EventName -cne 'pull_request' -and $NativeScopeOutput -cne 'true') {
    throw "A non-PR run must require native profiles; classifier said '$NativeScopeOutput'."
}
if ($NativeResult -ceq 'cancelled' -or $NativeResult -ceq 'failure' -or $NativeResult -ceq 'timed_out' -or $NativeResult -ceq 'action_required') {
    throw "Native profile matrix failed or was cancelled: '$NativeResult'."
}
if ($NativeResult -ceq 'skipped' -and -not ($EventName -ceq 'pull_request' -and $NativeScopeOutput -ceq 'false')) {
    throw "Native profiles were skipped without a successful positive documentation-only decision (event=$EventName, native=$NativeScopeOutput)."
}
if ($NativeResult -cnotin @('success','skipped')) { throw "Native profile matrix has missing or unknown result '$NativeResult'." }

$pairedRequired = $EventName -ceq 'pull_request' -or ($EventName -ceq 'workflow_dispatch' -and -not [string]::IsNullOrWhiteSpace($BenchmarkBaseline))
if ($pairedRequired) {
    Require-Success 'paired benchmark/scope job' $PairedResult
    if ($EventName -ceq 'pull_request' -and $PairedRelevant -cnotin @('true','false')) {
        throw "Pull-request benchmark classifier output must be true or false, got '$PairedRelevant'."
    }
} elseif ($PairedResult -cne 'skipped') {
    throw "Paired benchmark ran without a pull-request or explicit manual baseline; unexpected result '$PairedResult'."
}

$pairedMeasured = ($EventName -ceq 'pull_request' -and $PairedRelevant -ceq 'true') -or ($EventName -ceq 'workflow_dispatch' -and $pairedRequired)
if ($EventName -ceq 'pull_request' -and ($ExpectedBaseSha -or $ExpectedMergeSha)) {
    foreach ($commit in @($ExpectedBaseSha, $ExpectedMergeSha)) {
        if ($commit -cnotmatch '^[0-9a-f]{40}$') { throw "The workflow must name the pull request's base and merge commits as full lowercase commit ids, got '$commit'." }
    }
    if ($PairedRelevant -ceq 'true' -and ($PairedBaseline -cne $ExpectedBaseSha -or $PairedCandidate -cne $ExpectedMergeSha)) {
        throw "The paired job compared $PairedBaseline with $PairedCandidate, not the pull request's base $ExpectedBaseSha with its merge commit $ExpectedMergeSha."
    }
}
if ($VerdictResult) {
    if ($pairedMeasured) {
        Require-Success 'paired verdict' $VerdictResult
        if ($EventName -ceq 'pull_request' -and $VerdictConclusion -cne 'pass') {
            throw "The paired verdict concluded '$VerdictConclusion'; a measured pull request needs a pass."
        }
    } elseif ($VerdictResult -cne 'skipped') {
        throw "The paired verdict ran for nothing measured; unexpected result '$VerdictResult'."
    }
}
Write-Host 'CI_GATE_PASSED: required validation, tooling, format, native classification/profiles, and paired-work accounting are complete; accepted capability skips remain explicit open qualification obligations.'
exit 0
