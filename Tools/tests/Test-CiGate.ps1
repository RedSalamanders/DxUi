[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$scriptPath = Join-Path $PSScriptRoot '../Test-CiGate.ps1'

function Invoke-Gate([hashtable] $Values) {
    $names = @('EVENT_NAME','VALIDATION_RESULT','WINDOWS_TOOLING_RESULT','FORMAT_RESULT','NATIVE_SCOPE_RESULT',
        'NATIVE_SCOPE_OUTPUT','NATIVE_RESULT','PAIRED_RESULT','PAIRED_RELEVANT','BENCHMARK_BASELINE',
        'EXPECTED_BASE_SHA','EXPECTED_MERGE_SHA','PAIRED_BASELINE','PAIRED_CANDIDATE','VERDICT_RESULT','VERDICT_CONCLUSION')
    $previous = @{}
    foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name) }
    try {
        foreach ($name in $names) { Restore-TestEnvironmentVariable $name $Values[$name] }
        $global:LASTEXITCODE = 0
        $output = & $scriptPath 6>&1 2>&1
        return [pscustomobject]@{ Exit = $LASTEXITCODE; Text = (@($output | ForEach-Object { "$_" }) -join "`n") }
    } catch { return [pscustomobject]@{ Exit = 1; Text = $_.Exception.Message } }
    finally { foreach ($name in $names) { Restore-TestEnvironmentVariable $name $previous[$name] } }
}
function New-Results([string] $Event='pull_request', [string] $NativeScope='true', [string] $Native='success', [string] $Paired='success', [string] $Relevant='true', [string] $Baseline='') {
    return @{ EVENT_NAME=$Event; VALIDATION_RESULT='success'; WINDOWS_TOOLING_RESULT='success'; FORMAT_RESULT='success';
        NATIVE_SCOPE_RESULT='success'; NATIVE_SCOPE_OUTPUT=$NativeScope; NATIVE_RESULT=$Native; PAIRED_RESULT=$Paired; PAIRED_RELEVANT=$Relevant; BENCHMARK_BASELINE=$Baseline }
}

Invoke-TestCase 'gate fixture restores an absent workflow event variable' {
    $name = 'EVENT_NAME'
    $previous = [Environment]::GetEnvironmentVariable($name)
    try {
        Restore-TestEnvironmentVariable $name $null
        Assert-Equal 0 (Invoke-Gate (New-Results)).Exit 'the workflow fixture completes'
        Assert-True (-not (Test-Path -LiteralPath "Env:$name")) 'the wrapper restores the variable to absent, not empty'
    } finally { Restore-TestEnvironmentVariable $name $previous }
}

Invoke-TestCase 'aggregate accepts complete PR qualification and positively classified skips' {
    Assert-Equal 0 (Invoke-Gate (New-Results)).Exit 'all required jobs passed'
    Assert-Equal 0 (Invoke-Gate (New-Results -NativeScope 'false' -Native 'skipped' -Relevant 'false')).Exit 'native skip follows an explicit doc-only decision'
}
Invoke-TestCase 'aggregate rejects missing, failed and cancelled required jobs' {
    foreach ($key in @('VALIDATION_RESULT','WINDOWS_TOOLING_RESULT','FORMAT_RESULT','NATIVE_SCOPE_RESULT','NATIVE_RESULT','PAIRED_RESULT')) {
        foreach ($status in @('', 'skipped', 'failure', 'cancelled', 'timed_out', 'action_required', 'unknown')) {
            $values = New-Results
            $values[$key] = $status
            Assert-True ((Invoke-Gate $values).Exit -ne 0) "$key=$status fails closed"
        }
    }
}
Invoke-TestCase 'native skipped without positive classification and unclassifiable outputs fail closed' {
    Assert-True ((Invoke-Gate (New-Results -NativeScope 'true' -Native 'skipped')).Exit -ne 0) 'native=true cannot skip'
    Assert-True ((Invoke-Gate (New-Results -NativeScope '' -Native 'skipped')).Exit -ne 0) 'missing classifier output cannot skip'
    Assert-True ((Invoke-Gate (New-Results -NativeScope 'false' -Native 'cancelled')).Exit -ne 0) 'classified skip cannot hide cancellation'
    Assert-True ((Invoke-Gate (New-Results -NativeScope 'false' -Native 'success' -Event 'push')).Exit -ne 0) 'non-PR cannot positively skip native profiles'
}
Invoke-TestCase 'paired benchmark is required on stacked and main PRs and explicit manual runs' {
    Assert-True ((Invoke-Gate (New-Results -Paired 'skipped')).Exit -ne 0) 'PR cannot skip paired gate'
    Assert-Equal 0 (Invoke-Gate (New-Results -Event 'push' -Paired 'skipped')).Exit 'push may skip paired gate'
    $manual = New-Results -Event 'workflow_dispatch' -Paired 'success'
    $manual.BENCHMARK_BASELINE = 'base'
    Assert-Equal 0 (Invoke-Gate $manual).Exit 'manual baseline requires paired result'
    $manual.PAIRED_RESULT = 'skipped'
    Assert-True ((Invoke-Gate $manual).Exit -ne 0) 'manual baseline cannot skip paired run'
}
Invoke-TestCase 'a measured pull request compared exactly the commits the workflow names' {
    $base = 'a' * 40; $merge = 'b' * 40
    $values = New-Results
    $values.EXPECTED_BASE_SHA = $base; $values.EXPECTED_MERGE_SHA = $merge; $values.PAIRED_BASELINE = $base; $values.PAIRED_CANDIDATE = $merge
    Assert-Equal 0 (Invoke-Gate $values).Exit 'the named pair was measured'
    foreach ($case in @(@{ Key = 'PAIRED_BASELINE'; Value = 'c' * 40 }, @{ Key = 'PAIRED_CANDIDATE'; Value = 'c' * 40 }, @{ Key = 'PAIRED_CANDIDATE'; Value = '' },
            @{ Key = 'EXPECTED_BASE_SHA'; Value = 'main' }, @{ Key = 'EXPECTED_MERGE_SHA'; Value = ('B' * 40) })) {
        $changed = $values.Clone()
        $changed[$case.Key] = $case.Value
        Assert-True ((Invoke-Gate $changed).Exit -ne 0) "$($case.Key)='$($case.Value)' fails closed"
    }
    $skipped = $values.Clone()
    $skipped.PAIRED_RELEVANT = 'false'; $skipped.PAIRED_BASELINE = ''; $skipped.PAIRED_CANDIDATE = ''
    Assert-Equal 0 (Invoke-Gate $skipped).Exit 'nothing measured has no pair to compare'
}
Invoke-TestCase 'a separate verdict must pass a measured pull request and stay skipped otherwise' {
    $values = New-Results
    $values.VERDICT_RESULT = 'success'; $values.VERDICT_CONCLUSION = 'pass'
    Assert-Equal 0 (Invoke-Gate $values).Exit 'a passing verdict'
    foreach ($case in @(@{ Result = 'success'; Conclusion = 'inconclusive' }, @{ Result = 'success'; Conclusion = 'degraded' }, @{ Result = 'success'; Conclusion = '' },
            @{ Result = 'failure'; Conclusion = 'pass' }, @{ Result = 'skipped'; Conclusion = '' }, @{ Result = 'cancelled'; Conclusion = '' })) {
        $changed = $values.Clone()
        $changed.VERDICT_RESULT = $case.Result; $changed.VERDICT_CONCLUSION = $case.Conclusion
        Assert-True ((Invoke-Gate $changed).Exit -ne 0) "verdict $($case.Result)/$($case.Conclusion) fails a measured pull request"
    }
    $unmeasured = New-Results -Relevant 'false'
    $unmeasured.VERDICT_RESULT = 'skipped'
    Assert-Equal 0 (Invoke-Gate $unmeasured).Exit 'nothing measured skips the verdict'
    $unmeasured.VERDICT_RESULT = 'success'; $unmeasured.VERDICT_CONCLUSION = 'pass'
    Assert-True ((Invoke-Gate $unmeasured).Exit -ne 0) 'a verdict for nothing measured is unexpected'
    $manual = New-Results -Event 'workflow_dispatch' -Paired 'success'
    $manual.BENCHMARK_BASELINE = 'base'; $manual.VERDICT_RESULT = 'success'; $manual.VERDICT_CONCLUSION = 'degraded'
    Assert-Equal 0 (Invoke-Gate $manual).Exit 'a manual baseline reports its verdict without gating on it'
    $manual.VERDICT_RESULT = 'failure'
    Assert-True ((Invoke-Gate $manual).Exit -ne 0) 'but its verdict job must succeed'
    $push = New-Results -Event 'push' -Paired 'skipped'
    $push.VERDICT_RESULT = 'skipped'
    Assert-Equal 0 (Invoke-Gate $push).Exit 'a push skips the verdict'
}
Complete-TestRun 'CI gate'
