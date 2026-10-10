[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$scriptPath = Join-Path $PSScriptRoot '../Test-CiGate.ps1'

function Invoke-Gate([hashtable] $Values) {
    $names = @('EVENT_NAME','VALIDATION_RESULT','WINDOWS_TOOLING_RESULT','FORMAT_RESULT','NATIVE_SCOPE_RESULT',
        'NATIVE_SCOPE_OUTPUT','NATIVE_RESULT','PAIRED_RESULT','PAIRED_RELEVANT','BENCHMARK_BASELINE')
    $previous = @{}
    foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name) }
    try {
        foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, [string]$Values[$name]) }
        $global:LASTEXITCODE = 0
        $output = & $scriptPath 6>&1 2>&1
        return [pscustomobject]@{ Exit = $LASTEXITCODE; Text = (@($output | ForEach-Object { "$_" }) -join "`n") }
    } catch { return [pscustomobject]@{ Exit = 1; Text = $_.Exception.Message } }
    finally { foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $previous[$name]) } }
}
function New-Results([string] $Event='pull_request', [string] $NativeScope='true', [string] $Native='success', [string] $Paired='success', [string] $Relevant='true', [string] $Baseline='') {
    return @{ EVENT_NAME=$Event; VALIDATION_RESULT='success'; WINDOWS_TOOLING_RESULT='success'; FORMAT_RESULT='success';
        NATIVE_SCOPE_RESULT='success'; NATIVE_SCOPE_OUTPUT=$NativeScope; NATIVE_RESULT=$Native; PAIRED_RESULT=$Paired; PAIRED_RELEVANT=$Relevant; BENCHMARK_BASELINE=$Baseline }
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
Complete-TestRun 'CI gate'
