#Requires -Version 7.2
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../ConsumerQualification.psm1') -Force

function Read-ConsumerProgressFromHeldStream([IO.FileStream] $Stream) {
    # This test polls one mutable, atomically replaced progress path. The normal
    # JSON helpers read finished reports; this reader must allow rename/delete so
    # Save-ConsumerQualificationProgress can replace the path while it is open.
    [void]$Stream.Seek(0, [IO.SeekOrigin]::Begin)
    $reader = [IO.StreamReader]::new($Stream, [Text.UTF8Encoding]::new($false, $true), $false, 1024, $true)
    try { return $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
}
function Read-ConsumerProgressSnapshot([string] $Path) {
    $share=[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,$share)
    try { return Read-ConsumerProgressFromHeldStream $stream } finally { $stream.Dispose() }
}

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
Invoke-FixtureCase 'initial progress publication does not overwrite an occupied target' {
    param($fixture)
    $p=New-ConsumerQualificationProgress (Join-Path $fixture 'progress.json') @{commit='h'*40}
    $target=Join-Path $fixture 'occupied.json'
    $sentinel='preserve-existing-owned-target'
    [IO.File]::WriteAllText($target,$sentinel,[Text.UTF8Encoding]::new($false))
    $collision=[pscustomobject]@{Path=$target;Record=$p.Record}
    $module=Get-Module|Where-Object Name -EQ 'ConsumerQualification'
    Assert-True ($null-ne $module) 'consumer qualification module is loaded for its private publisher control'
    Assert-Throws {& $module {param($progress) Save-ConsumerQualificationProgress $progress -Initial} $collision} 'initial publication rejects an occupied target'
    Assert-Equal $sentinel ([IO.File]::ReadAllText($target)) 'initial publication preserves the existing target contents'
}
if($IsWindows){Invoke-FixtureCase 'progress replacement fails while a reader denies delete sharing' {
    param($fixture)
    $p=New-ConsumerQualificationProgress (Join-Path $fixture 'progress.json') @{commit='f'*40}
    $held=[IO.File]::Open($p.Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $failure=$null;$bodyState=[pscustomobject]@{Ran=$false}
    try{
        try{Invoke-ConsumerQualificationPhase $p 'DeniedReader' {$bodyState.Ran=$true} -NegativeCheck}
        catch{$failure=$_.Exception}
        while($failure -and $failure.InnerException){$failure=$failure.InnerException}
        Assert-True (($failure -is [IO.IOException]) -or ($failure -is [UnauthorizedAccessException])) 'replacement surfaces the owned reader sharing failure'
        Assert-True (-not $bodyState.Ran) 'a failed publication prevents phase execution'
        $onDisk=[IO.File]::ReadAllText($p.Path)|ConvertFrom-Json
        Assert-Equal $null $onDisk.activePhase 'denied replacement leaves the prior snapshot intact'
        Assert-Equal 0 ([int]$onDisk.negativeChecks) 'denied replacement cannot publish completion'
        Assert-Equal 0 (@($onDisk.phases).Count) 'denied replacement leaves the prior phase list intact'
    }finally{$held.Dispose()}
}}
Invoke-FixtureCase 'delete-sharing reader keeps an old coherent snapshot across active and completed publication' {
    param($fixture)
    $p=New-ConsumerQualificationProgress (Join-Path $fixture 'progress.json') @{commit='g'*40}
    $held=[IO.File]::Open($p.Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try{
        $old=Read-ConsumerProgressFromHeldStream $held
        Assert-Equal 'incomplete' $old.status 'initial retained snapshot is incomplete'
        Assert-Equal $null $old.activePhase 'initial retained snapshot has no active case'
        Invoke-ConsumerQualificationPhase $p 'VisibleNegative' {
            $heldSnapshot=Read-ConsumerProgressFromHeldStream $held
            Assert-Equal $null $heldSnapshot.activePhase 'open handle remains bound to coherent old snapshot during replacement'
            Assert-Equal 0 ([int]$heldSnapshot.negativeChecks) 'old snapshot count remains unchanged during replacement'
            $active=[IO.File]::ReadAllText($p.Path)|ConvertFrom-Json
            Assert-Equal 'incomplete' $active.status 'new path remains incomplete while phase runs'
            Assert-Equal 'VisibleNegative' $active.activePhase 'new path exposes the active case'
            Assert-Equal 0 ([int]$active.negativeChecks) 'active case is not counted as complete'
        } -NegativeCheck
        $heldAfter=Read-ConsumerProgressFromHeldStream $held
        Assert-Equal $null $heldAfter.activePhase 'old open handle stays on the same coherent old version after replacement'
        Assert-Equal 0 ([int]$heldAfter.negativeChecks) 'old open handle retains old completion count'
        $current=[IO.File]::ReadAllText($p.Path)|ConvertFrom-Json
        Assert-Equal 'incomplete' $current.status 'current progress remains incomplete until final receipt'
        Assert-Equal $null $current.activePhase 'completed phase clears active name but does not complete the overall journal'
        Assert-Equal 1 ([int]$current.negativeChecks) 'only the successful negative case is counted'
        Assert-Equal 'passed' $current.phases[0].state 'published case outcome is complete'
    }finally{$held.Dispose()}
}
function Invoke-OwnedProcessFixtureCase([string]$Name,[scriptblock]$Body) {
    Invoke-TestCase $Name {
        $fixture=New-FixtureRoot
        $lifecycle=[pscustomobject]@{Preserve=$false;Reason=$null}
        try{& $Body $fixture $lifecycle}
        finally{
            # Stock Invoke-FixtureCase always removes its root; these cases must keep it if the owned child survives closure.
            if($lifecycle.Preserve){Write-Warning "Preserving live-child fixture root '$fixture': $($lifecycle.Reason)"}
            else{Remove-FixtureRoot $fixture}
        }
    }.GetNewClosure()
}
Invoke-OwnedProcessFixtureCase 'external timeout retains the active incomplete phase without polling the mutable journal' {
    param($fixture,$lifecycle)
    $childScript=Join-Path $fixture 'Child.ps1'
    [IO.File]::WriteAllText($childScript,@'
param($Module,$Output,$Ready,$PendingReady,$Release)
$ErrorActionPreference='Stop'
Import-Module -Name $Module -Force -ErrorAction Stop
$p=New-ConsumerQualificationProgress $Output @{commit='e'*40}
Invoke-ConsumerQualificationPhase $p 'DelayedNegative' {
    # The pending file is fully closed before its same-directory, no-overwrite publication.
    [IO.File]::WriteAllText($PendingReady,'phase-body-entered',[Text.UTF8Encoding]::new($false))
    $releaseTimer=[Diagnostics.Stopwatch]::StartNew()
    while(-not (Test-Path -LiteralPath $Release -PathType Leaf) -and $releaseTimer.Elapsed -lt [TimeSpan]::FromSeconds(60)){Start-Sleep -Milliseconds 10}
    if(-not (Test-Path -LiteralPath $Release -PathType Leaf)){throw 'Parent did not release the readiness publication.'}
    [IO.File]::Move($PendingReady,$Ready)
    [Threading.Thread]::Sleep(60000)
} -NegativeCheck
'@)
    $path=Join-Path $fixture 'progress.json'
    $readyPath=Join-Path $fixture 'phase-body-ready.marker'
    $pendingPath=Join-Path $fixture 'phase-body-ready.pending'
    $releasePath=Join-Path $fixture 'phase-body-ready.release'
    $start=[Diagnostics.ProcessStartInfo]::new('pwsh')
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($arg in @('-NoProfile','-File',$childScript,'-Module',(Join-Path $PSScriptRoot '../ConsumerQualification.psm1'),'-Output',$path,'-Ready',$readyPath,'-PendingReady',$pendingPath,'-Release',$releasePath)){$start.ArgumentList.Add($arg)}
    $child=$null;$stdoutTask=$null;$stderrTask=$null;$waitAttempted=$false;$terminated=$false
    try{
        $child=[Diagnostics.Process]::Start($start)
        $stdoutTask=$child.StandardOutput.ReadToEndAsync()
        $stderrTask=$child.StandardError.ReadToEndAsync()
        $readinessTimer=[Diagnostics.Stopwatch]::StartNew();$pendingObserved=$false;$readyBeforeRelease=$false;$ready=$false
        while($readinessTimer.Elapsed -lt [TimeSpan]::FromSeconds(10) -and -not $child.HasExited){
            if(Test-Path -LiteralPath $pendingPath -PathType Leaf){$pendingObserved=$true;break}
            Start-Sleep -Milliseconds 25
        }
        if(-not $pendingObserved){$pendingObserved=Test-Path -LiteralPath $pendingPath -PathType Leaf}
        if($pendingObserved){
            $readyBeforeRelease=Test-Path -LiteralPath $readyPath -PathType Leaf
            if(-not $readyBeforeRelease){[IO.File]::WriteAllText($releasePath,'publish',[Text.UTF8Encoding]::new($false))}
        }
        while($readinessTimer.Elapsed -lt [TimeSpan]::FromSeconds(10) -and -not $child.HasExited){
            if(Test-Path -LiteralPath $readyPath -PathType Leaf){$ready=$true;break}
            Start-Sleep -Milliseconds 25
        }
        if(-not $ready){$ready=Test-Path -LiteralPath $readyPath -PathType Leaf}
        $childExitedBeforeTermination=$child.HasExited
        $deadlineExpired=$readinessTimer.Elapsed -ge [TimeSpan]::FromSeconds(10)
        if(-not $childExitedBeforeTermination){$child.Kill()}
        $waitAttempted=$true
        $terminated=$child.WaitForExit(3000)
        $stdout=try{if($terminated -and $stdoutTask.Wait(500)){$stdoutTask.GetAwaiter().GetResult()}else{'<stdout drain exceeded 500 ms after exit>'}}catch{"<stdout drain failed: $($_.Exception.GetBaseException().Message)>"}
        $stderr=try{if($terminated -and $stderrTask.Wait(500)){$stderrTask.GetAwaiter().GetResult()}else{'<stderr drain exceeded 500 ms after exit>'}}catch{"<stderr drain failed: $($_.Exception.GetBaseException().Message)>"}
        $progress=if($terminated -and (Test-Path -LiteralPath $path -PathType Leaf)){try{Read-ConsumerProgressSnapshot $path | ConvertTo-Json -Depth 12 -Compress}catch{"<progress read failed: $($_.Exception.Message)>"}}elseif($terminated){'<progress file missing>'}else{'<progress not read: child did not close>'}
        $message="owned helper did not publish a complete immutable body-readiness marker before interruption; pendingObserved=$pendingObserved; readyBeforeRelease=$readyBeforeRelease; exitedBeforeTermination=$childExitedBeforeTermination; deadlineExpired=$deadlineExpired; exitCode=$(if($child.HasExited){$child.ExitCode}else{'unknown'}); elapsedMs=$([int]$readinessTimer.Elapsed.TotalMilliseconds); progress=$progress; stdout=$stdout; stderr=$stderr"
        if(-not $terminated){$lifecycle.Preserve=$true;$lifecycle.Reason="child PID $($child.Id) did not close within the single three-second wait; $message"}
        Assert-True $terminated "owned helper exited within the unchanged termination bound; $message"
        Assert-True $pendingObserved "child body did not stage its readiness marker; $message"
        Assert-True (-not $readyBeforeRelease) "pending marker was mistaken for published readiness before release; $message"
        Assert-True $ready "owned helper did not publish the final marker after release; $message"
        Assert-Equal 'phase-body-entered' ([IO.File]::ReadAllText($readyPath)) 'readiness marker is moved into place only after its complete content is closed'
        $r=Read-ConsumerProgressSnapshot $path # Read only after the child has exited and its writer is closed.
        Assert-Equal 'incomplete' $r.status 'watchdog termination is never completion'
        Assert-Equal 'DelayedNegative' $r.activePhase 'the exact unfinished case remains visible'
        Assert-Equal 0 ([int]$r.negativeChecks) 'unfinished negative case is not counted'
        Assert-Equal 'running' $r.phases[0].state 'the interrupted phase remains explicitly running'
    }finally{
        if($null -ne $child){
            if(-not $waitAttempted){
                if(-not $child.HasExited){
                    try{$child.Kill()}catch{if(-not $child.HasExited){$lifecycle.Preserve=$true;$lifecycle.Reason="cleanup could not terminate child PID $($child.Id): $($_.Exception.Message)"}}
                }
                $waitAttempted=$true;$terminated=$child.WaitForExit(3000)
            }
            if(-not $child.HasExited){$lifecycle.Preserve=$true;if(-not $lifecycle.Reason){$lifecycle.Reason="child PID $($child.Id) remained live after its only bounded closure attempt"}}
            else{$child.Dispose()}
        }
    }
}
if($IsWindows){Invoke-OwnedProcessFixtureCase 'failed active publication cannot produce timeout readiness or progress' {
    param($fixture,$lifecycle)
    $childScript=Join-Path $fixture 'Child.ps1'
    [IO.File]::WriteAllText($childScript,@'
param($Module,$Output,$Ready,$PendingReady)
$ErrorActionPreference='Stop'
Import-Module -Name $Module -Force -ErrorAction Stop
$p=New-ConsumerQualificationProgress $Output @{commit='i'*40}
$held=[IO.File]::Open($Output,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
try{
    try{
        Invoke-ConsumerQualificationPhase $p 'DelayedNegative' {
            [IO.File]::WriteAllText($PendingReady,'phase-body-entered',[Text.UTF8Encoding]::new($false))
            [IO.File]::Move($PendingReady,$Ready)
            [Threading.Thread]::Sleep(60000)
        } -NegativeCheck
    }catch{
        [Console]::Error.WriteLine("CHILD_PHASE_FAILURE: $($_.Exception.GetBaseException().Message)")
        throw
    }
}finally{$held.Dispose()}
'@)
    $path=Join-Path $fixture 'progress.json'
    $readyPath=Join-Path $fixture 'phase-body-ready.marker'
    $pendingPath=Join-Path $fixture 'phase-body-ready.pending'
    $start=[Diagnostics.ProcessStartInfo]::new('pwsh')
    $start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
    foreach($arg in @('-NoProfile','-File',$childScript,'-Module',(Join-Path $PSScriptRoot '../ConsumerQualification.psm1'),'-Output',$path,'-Ready',$readyPath,'-PendingReady',$pendingPath)){$start.ArgumentList.Add($arg)}
    $child=$null;$stdoutTask=$null;$stderrTask=$null;$waitAttempted=$false;$terminated=$false
    try{
        $child=[Diagnostics.Process]::Start($start)
        $stdoutTask=$child.StandardOutput.ReadToEndAsync()
        $stderrTask=$child.StandardError.ReadToEndAsync()
        $readinessTimer=[Diagnostics.Stopwatch]::StartNew();$ready=$false
        while($readinessTimer.Elapsed -lt [TimeSpan]::FromSeconds(10) -and -not $child.HasExited){
            if(Test-Path -LiteralPath $readyPath -PathType Leaf){$ready=$true;break}
            Start-Sleep -Milliseconds 25
        }
        if(-not $ready){$ready=Test-Path -LiteralPath $readyPath -PathType Leaf}
        $earlyExit=$child.HasExited
        if(-not $earlyExit){$child.Kill()}
        $waitAttempted=$true
        $terminated=$child.WaitForExit(3000)
        $stdout=try{if($terminated -and $stdoutTask.Wait(500)){$stdoutTask.GetAwaiter().GetResult()}else{'<stdout drain exceeded 500 ms after exit>'}}catch{"<stdout drain failed: $($_.Exception.GetBaseException().Message)>"}
        $stderr=try{if($terminated -and $stderrTask.Wait(500)){$stderrTask.GetAwaiter().GetResult()}else{'<stderr drain exceeded 500 ms after exit>'}}catch{"<stderr drain failed: $($_.Exception.GetBaseException().Message)>"}
        Assert-True $terminated "failed-publication helper exited within the unchanged termination bound; exitCode=$(if($child.HasExited){$child.ExitCode}else{'unknown'}); stdout=$stdout; stderr=$stderr"
        $r=Read-ConsumerProgressSnapshot $path # Read only after child closure, never between path check and replacement.
        $failureDetails="earlyExit=$earlyExit; readinessMarker=$ready; exitCode=$($child.ExitCode); status=$($r.status); activePhase=$($r.activePhase); negativeChecks=$($r.negativeChecks); phases=$(@($r.phases).Count); stdout=$stdout; stderr=$stderr"
        Assert-True $earlyExit "active-publication failure exits before watchdog termination; $failureDetails"
        Assert-True ($child.ExitCode -ne 0) "active-publication failure remains a nonzero child result; $failureDetails"
        Assert-True (-not $ready) "failed active publication never creates the body-readiness marker; $failureDetails"
        Assert-True ($stderr -match 'CHILD_PHASE_FAILURE:') "publisher failure remains visible in child stderr; $failureDetails"
        Assert-Equal 'incomplete' $r.status "failed publication cannot change journal status; $failureDetails"
        Assert-Equal $null $r.activePhase "failed publication cannot claim an active phase; $failureDetails"
        Assert-Equal 0 ([int]$r.negativeChecks) "failed publication cannot count a negative case; $failureDetails"
        Assert-Equal 0 (@($r.phases).Count) "failed publication cannot add a phase; $failureDetails"
    }finally{
        if($null -ne $child){
            if(-not $waitAttempted){
                if(-not $child.HasExited){
                    try{$child.Kill()}catch{if(-not $child.HasExited){$lifecycle.Preserve=$true;$lifecycle.Reason="cleanup could not terminate child PID $($child.Id): $($_.Exception.Message)"}}
                }
                $waitAttempted=$true;$terminated=$child.WaitForExit(3000)
            }
            if(-not $child.HasExited){$lifecycle.Preserve=$true;if(-not $lifecycle.Reason){$lifecycle.Reason="child PID $($child.Id) remained live after its only bounded closure attempt"}}
            else{$child.Dispose()}
        }
    }
}}
Complete-TestRun 'Consumer qualification progress'
