#Requires -Version 7.2
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../ConsumerQualification.psm1') -Force
Invoke-FixtureCase 'pending case, measured completion and final receipt are distinct' {
    param($fixture)
    $path=Join-Path $fixture 'progress.json'
    $p=New-ConsumerQualificationProgress $path @{commit='a'*40}
    function Invoke-LocalCase([string]$scenario){
        Invoke-ConsumerQualificationPhase $p $scenario {
            $pending=Get-Content $path -Raw|ConvertFrom-Json
            Assert-Equal 'incomplete' $pending.status 'a running phase is incomplete'
            Assert-Equal $scenario $pending.activePhase 'function-local case identity survives module invocation'
        } -NegativeCheck
    }
    foreach($case in 1..10){Invoke-LocalCase "negative-$case"}
    $record=Get-Content $path -Raw|ConvertFrom-Json
    Assert-Equal 'incomplete' $record.status 'ten checks alone cannot publish final success'
    Assert-Equal 10 ([int]$record.negativeChecks) 'all ten actual cases were counted'
    Assert-Throws {Invoke-ConsumerQualificationPhase $p 'negative-1' {} -NegativeCheck} 'duplicate cases cannot inflate completion'
    Assert-Throws {Complete-ConsumerQualificationProgress $p (Join-Path $fixture 'missing.json')} 'missing final receipt cannot complete progress'
    $receipt=Join-Path $fixture 'ExternalConsumer.json'
    @{suite='ExternalConsumer';exitCode=0;negativeChecks=10;nativeModuleChecks=12;commit='a'*40}|ConvertTo-Json|Set-Content $receipt
    Complete-ConsumerQualificationProgress $p $receipt
    Assert-Equal 'completed' ((Get-Content $path -Raw|ConvertFrom-Json).status) 'only matching final receipt completes progress'
    Assert-Throws {Invoke-ConsumerQualificationPhase $p 'late' {}} 'a completed progress record is immutable'
}
Invoke-FixtureCase 'failed operation keeps its exception and incomplete receipt' {
    param($fixture)
    $p=New-ConsumerQualificationProgress (Join-Path $fixture 'progress.json') @{commit='b'*40}
    try{Invoke-ConsumerQualificationPhase $p 'failure' {throw 'original operation failed'} -NegativeCheck;throw 'Expected failure was swallowed'}
    catch{Assert-Equal 'original operation failed' $_.Exception.Message 'original failure propagates'}
    $r=Get-Content $p.Path -Raw|ConvertFrom-Json
    Assert-Equal 'incomplete' $r.status 'failure is never a successful consumer receipt'
    Assert-Equal 0 ([int]$r.negativeChecks) 'failed cases are not counted'
    Assert-Equal 'failed' $r.phases[0].state 'failed case retained'
    Assert-True ($r.phases[0].elapsedMs-ge 0) 'duration retained'
}
Invoke-FixtureCase 'missing conformance cases and another source cannot complete progress' {
    param($fixture)
    $p=New-ConsumerQualificationProgress (Join-Path $fixture 'progress.json') @{commit='c'*40}
    $receipt=Join-Path $fixture 'ExternalConsumer.json'
    @{suite='ExternalConsumer';exitCode=0;negativeChecks=10;nativeModuleChecks=12;commit='d'*40}|ConvertTo-Json|Set-Content $receipt
    Assert-Throws {Complete-ConsumerQualificationProgress $p $receipt} 'missing cases cannot be hidden by a final-looking file'
    foreach($case in 1..10){Invoke-ConsumerQualificationPhase $p "negative-$case" {} -NegativeCheck}
    Assert-Throws {Complete-ConsumerQualificationProgress $p $receipt} 'receipt must bind the current source'
}
Invoke-FixtureCase 'external timeout retains the active incomplete phase' {
    param($fixture)
    $childScript=Join-Path $fixture 'Child.ps1'
    [IO.File]::WriteAllText($childScript,@'
param($Module,$Output)
Import-Module $Module
$p=New-ConsumerQualificationProgress $Output @{commit='e'*40}
Invoke-ConsumerQualificationPhase $p 'DelayedNegative' {[Threading.Thread]::Sleep(60000)} -NegativeCheck
'@)
    $path=Join-Path $fixture 'progress.json'
    $start=[Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME $(if($IsWindows){'pwsh.exe'}else{'pwsh'})))
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($arg in @('-NoProfile','-File',$childScript,'-Module',(Join-Path $PSScriptRoot '../ConsumerQualification.psm1'),'-Output',$path)){$start.ArgumentList.Add($arg)}
    $child=[Diagnostics.Process]::Start($start)
    $stdout=$child.StandardOutput.ReadToEndAsync();$stderr=$child.StandardError.ReadToEndAsync()
    try{
        $deadline=[DateTime]::UtcNow.AddSeconds(10);$ready=$false
        while([DateTime]::UtcNow-lt $deadline -and -not $child.HasExited){
            if(Test-Path $path){$r=Get-Content $path -Raw|ConvertFrom-Json;if($r.activePhase-eq 'DelayedNegative'){$ready=$true;break}}
            Start-Sleep -Milliseconds 25
        }
        Assert-True $ready 'owned helper published its active phase before interruption; child streams are retained in the fixture'
        $child.Kill();Assert-True ($child.WaitForExit(3000)) 'owned helper exited'
        $r=Get-Content $path -Raw|ConvertFrom-Json
        Assert-Equal 'incomplete' $r.status 'watchdog termination is never completion'
        Assert-Equal 'DelayedNegative' $r.activePhase 'the exact unfinished case remains visible'
        Assert-Equal 0 ([int]$r.negativeChecks) 'unfinished negative case is not counted'
    }finally{
        if(-not $child.HasExited){$child.Kill();[void]$child.WaitForExit(3000)}
        if($stdout.IsCompletedSuccessfully){[IO.File]::WriteAllText((Join-Path $fixture 'child.stdout.log'),$stdout.Result)}
        if($stderr.IsCompletedSuccessfully){[IO.File]::WriteAllText((Join-Path $fixture 'child.stderr.log'),$stderr.Result)}
        $child.Dispose()
    }
}
Complete-TestRun 'Consumer qualification progress'
