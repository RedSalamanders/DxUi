<#
.SYNOPSIS Verifies the fixed aggregate CI check against every required job result.
.DESCRIPTION A skipped native matrix is accepted only after a successful PR scope decision explicitly says native=false.
Paired benchmarks are required for every pull request and are internally skipped only after a successful scope decision.
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
    [string] $BenchmarkBaseline = $env:BENCHMARK_BASELINE
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
Write-Host 'CI_GATE_PASSED: required validation, tooling, format, native classification/profiles, and paired-work accounting are complete; accepted capability skips remain explicit open qualification obligations.'
exit 0
