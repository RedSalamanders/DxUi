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
        $unsigned=[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$failure.HResult),0);$win32Code=[int]($unsigned-band 0xffff)
        Assert-True ($win32Code -in @(32,1175)) "persistent denied-delete lock preserves the numeric sharing failure, got $win32Code"
        Assert-True (-not $bodyState.Ran) 'a failed publication prevents phase execution'
        $onDisk=[IO.File]::ReadAllText($p.Path)|ConvertFrom-Json
        Assert-Equal $null $onDisk.activePhase 'denied replacement leaves the prior snapshot intact'
        Assert-Equal 0 ([int]$onDisk.negativeChecks) 'denied replacement cannot publish completion'
        Assert-Equal 0 (@($onDisk.phases).Count) 'denied replacement leaves the prior phase list intact'
    }finally{$held.Dispose()}
}}
Invoke-FixtureCase 'missing and invalid progress inputs fail without counting a phase' {
    param($fixture)
    $missing=Join-Path $fixture 'missing-progress.json'
    $record=[ordered]@{schema=1;suite='ConsumerProgress';status='incomplete';startedUtc=[DateTime]::UtcNow.ToString('o');context=@{};activePhase=$null;negativeChecks=0;phases=[Collections.Generic.List[object]]::new()}
    $missingProgress=[pscustomobject]@{Path=$missing;Owner=$PID;Record=$record}
    $missingBody=[pscustomobject]@{Ran=$false};$missingCapture=[pscustomobject]@{Failure=$null;Output=@()}
    $missingCapture.Output=@(& {try{Invoke-ConsumerQualificationPhase $missingProgress 'MissingDestination' {$missingBody.Ran=$true} -NegativeCheck -Verbose}catch{$missingCapture.Failure=$_.Exception.GetBaseException()}} 4>&1)
    $missingFailure=$missingCapture.Failure
    Assert-True ($null-ne $missingFailure) 'missing destination remains a publication failure'
    Assert-True (-not $missingBody.Ran) 'missing destination prevents body execution'
    Assert-Equal 0 ([int]$record.negativeChecks) 'missing destination is not counted'
    Assert-True (-not [IO.File]::Exists($missing)) 'missing destination is not recreated'
    Assert-Equal 0 (@($missingCapture.Output|Where-Object {$_ -is [Management.Automation.VerboseRecord] -and $_.Message -like 'DXUI_CONSUMER_PUBLISH_RETRY*'}).Count) 'missing destination emits no retry diagnostic'

    $invalidRecord=[ordered]@{schema=1;suite='ConsumerProgress';status='incomplete';startedUtc=[DateTime]::UtcNow.ToString('o');context=@{};activePhase=$null;negativeChecks=0;phases=[Collections.Generic.List[object]]::new()}
    $invalidProgress=[pscustomobject]@{Path="bad`0path";Owner=$PID;Record=$invalidRecord}
    $invalidBody=[pscustomobject]@{Ran=$false};$invalidCapture=[pscustomobject]@{Failure=$null;Output=@()}
    $invalidCapture.Output=@(& {try{Invoke-ConsumerQualificationPhase $invalidProgress 'InvalidPath' {$invalidBody.Ran=$true} -NegativeCheck -Verbose}catch{$invalidCapture.Failure=$_.Exception.GetBaseException()}} 4>&1)
    $invalidFailure=$invalidCapture.Failure
    Assert-True ($null-ne $invalidFailure) 'invalid publisher path remains a failure'
    Assert-True (-not $invalidBody.Ran) 'invalid path prevents body execution'
    Assert-Equal 0 ([int]$invalidRecord.negativeChecks) 'invalid path is not counted'
    Assert-Equal 0 (@($invalidCapture.Output|Where-Object {$_ -is [Management.Automation.VerboseRecord] -and $_.Message -like 'DXUI_CONSUMER_PUBLISH_RETRY*'}).Count) 'invalid input emits no retry diagnostic'
}
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
        catch{$lifecycle.Preserve=$true;$lifecycle.Reason="owned child fixture failed: $($_.Exception.GetBaseException().Message)";throw}
        finally{
            # Stock Invoke-FixtureCase always removes its root; these cases retain owned fixture evidence when needed.
            if($lifecycle.Preserve){Write-Warning "Preserving owned fixture root '$fixture': $($lifecycle.Reason)"}
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
try {
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
} catch {
    $exception=$_.Exception.GetBaseException();$unsigned=[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exception.HResult),0);$hrHex=[Convert]::ToString($unsigned,16).PadLeft(8,'0')
    [Console]::Error.WriteLine("CHILD_PHASE_FAILURE: type=$($exception.GetType().FullName) HResult=0x$hrHex")
    [Console]::Error.WriteLine($_.Exception.ToString())
    throw
}
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
if($IsWindows){Invoke-FixtureCase 'retry is bounded, verified, and never repeats phase execution' {
    param($fixture)
    $module=Get-Module|Where-Object Name -EQ 'ConsumerQualification'
    Assert-True ($null-ne $module) 'consumer module is present for isolated private sleep shadow'
    function Install-RetryWaitShadow($state) {
        $hook={
            param([int]$Milliseconds)
            $state.WaitCalls++
            if(-not $state.Released){
                if($state.MutatePending){
                    try{
                        $pending=Get-ChildItem -LiteralPath (Split-Path -Parent $state.ProgressPath) -File -Filter ((Split-Path -Leaf $state.ProgressPath)+'.pending-*')|Select-Object -First 1
                        if($null -eq $pending){throw 'Expected pending file was absent at first retry wait.'}
                        $handle=[TestRunEvidenceV2.Native]::Pin($pending.FullName,$false)
                        try{$state.PendingIdentityBefore=[TestRunEvidenceV2.Native]::Identity($handle)}finally{$handle.Dispose()}
                        $bytes=[IO.File]::ReadAllBytes($pending.FullName);$bytes[0]=[byte][char]'[';[IO.File]::WriteAllBytes($pending.FullName,$bytes)
                        $handle=[TestRunEvidenceV2.Native]::Pin($pending.FullName,$false)
                        try{$state.PendingIdentityAfter=[TestRunEvidenceV2.Native]::Identity($handle)}finally{$handle.Dispose()}
                    }catch{$state.HookFailure=$_.Exception.ToString();throw}
                }
                $state.Handle.Dispose();$state.Released=$true
            }
            Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds $Milliseconds
        }.GetNewClosure()
        & $module {
            param($hook)
            if(Get-Item Function:script:Start-Sleep -ErrorAction SilentlyContinue){throw 'Unexpected module-local Start-Sleep function exists before test shadow.'}
            Set-Item -Path Function:script:Start-Sleep -Value $hook
        } $hook
    }
    function Remove-RetryWaitShadow {
        & $module {
            Remove-Item Function:Start-Sleep -Force -ErrorAction SilentlyContinue
            (Get-Command Start-Sleep -ErrorAction Stop).CommandType.ToString()
        }
    }
    $verbosePreferenceBefore=$VerbosePreference
    try{
        $path=Join-Path $fixture 'transient-progress.json';$p=New-ConsumerQualificationProgress $path @{commit='j'*40}
        $held=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        $state=[pscustomobject]@{Handle=$held;Released=$false;WaitCalls=0;MutatePending=$false;ProgressPath=$path}
        Install-RetryWaitShadow $state
        $body=[pscustomobject]@{Runs=0};$captured=@()
        try{$VerbosePreference='Continue';$captured=@(Invoke-ConsumerQualificationPhase $p 'TransientRetry' {$body.Runs++} -NegativeCheck -Verbose 4>&1)}
        finally{$VerbosePreference=$verbosePreferenceBefore;$restored=Remove-RetryWaitShadow;if(-not $held.SafeFileHandle.IsClosed){$held.Dispose()}}
        $messages=@($captured|Where-Object {$_ -is [Management.Automation.VerboseRecord]}|ForEach-Object Message)
        Assert-True $state.Released 'test shadow releases the exact held reader only after the real sharing failure reaches retry wait'
        Assert-Equal 1 $state.WaitCalls 'exactly one failed ReplaceFile attempt enters the bounded retry wait'
        Assert-True (($messages -join "`n") -match 'DXUI_CONSUMER_PUBLISH_RETRY code=32 attempt=1 max=3 elapsedMs=') 'first sharing retry is captured as a diagnostic'
        Assert-True (($messages -join "`n") -match 'DXUI_CONSUMER_PUBLISH_RETRY_SUCCESS attempt=2') 'success on the second real ReplaceFile call is diagnosed'
        Assert-Equal 'Cmdlet' $restored 'module-local Start-Sleep shadow is removed after the test'
        Assert-Equal 1 $body.Runs 'phase body runs exactly once after retry success'
        $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
        Assert-Equal 1 ([int]$record.negativeChecks) 'one successful phase is counted exactly once'
        Assert-Equal 1 (@($record.phases).Count) 'one phase record is written'
        Assert-Equal 'passed' $record.phases[0].state 'phase completes after the single retry'

        $path2=Join-Path $fixture 'changed-pending-progress.json';$p2=New-ConsumerQualificationProgress $path2 @{commit='k'*40}
        $held2=[IO.File]::Open($path2,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        $state2=[pscustomobject]@{Handle=$held2;Released=$false;WaitCalls=0;MutatePending=$true;ProgressPath=$path2;PendingIdentityBefore=$null;PendingIdentityAfter=$null;HookFailure=$null}
        Install-RetryWaitShadow $state2
        $body2=[pscustomobject]@{Runs=0};$captured2=@();$failure2=$null
        try{
            $VerbosePreference='Continue'
            try{Invoke-ConsumerQualificationPhase $p2 'ChangedPending' {$body2.Runs++} -NegativeCheck -Verbose 4>&1|ForEach-Object{$captured2+=,$_}}catch{$failure2=$_.Exception.GetBaseException()}
        }finally{$VerbosePreference=$verbosePreferenceBefore;$restored2=Remove-RetryWaitShadow;if(-not $held2.SafeFileHandle.IsClosed){$held2.Dispose()}}
        $messages2=@($captured2|Where-Object {$_ -is [Management.Automation.VerboseRecord]}|ForEach-Object Message)
        $hookFailure=if($state2.PSObject.Properties['HookFailure']){$state2.HookFailure}else{'<none>'}
        $idBefore=if($state2.PSObject.Properties['PendingIdentityBefore']){$state2.PendingIdentityBefore}else{'<none>'};$idAfter=if($state2.PSObject.Properties['PendingIdentityAfter']){$state2.PendingIdentityAfter}else{'<none>'}
        Assert-True $state2.Released "the mutation control releases its held reader after the first real replace failure; waitCalls=$($state2.WaitCalls), firstStateWaitCalls=$($state.WaitCalls), hookFailure=$hookFailure, released=$($state2.Released), handleClosed=$($held2.SafeFileHandle.IsClosed), pendingIdentity=$idBefore/$idAfter, messages=$($messages2 -join '; ')"
        Assert-Equal 1 $state2.WaitCalls 'changed bytes are checked before a second retry wait'
        Assert-Equal $state2.PendingIdentityBefore $state2.PendingIdentityAfter 'the control changes pending bytes without changing the pending file identity'
        Assert-True ($null-ne $failure2) 'changed pending bytes refuse another replace call'
        Assert-Equal 32 ([int]([BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$failure2.HResult),0)-band 0xffff)) 'state-change refusal retains the original Win32 sharing exception'
        Assert-True (($messages2 -join "`n") -match 'DXUI_CONSUMER_PUBLISH_RETRY_REFUSED code=32 attempt=1 reason=identity-or-content-changed') "state change is explicitly diagnosed before retry; messages=$($messages2 -join '; '); failure=$($failure2.Message)"
        Assert-True (($messages2 -join "`n") -notmatch 'DXUI_CONSUMER_PUBLISH_RETRY_SUCCESS|DXUI_CONSUMER_PUBLISH_RETRY code=.*attempt=2') 'changed pending bytes never reach a second ReplaceFile attempt'
        Assert-Equal 'Cmdlet' $restored2 'module-local sleep shadow is removed after the control'
        Assert-Equal 0 $body2.Runs 'phase body never runs when pending bytes change'
        $record2=Get-Content -LiteralPath $path2 -Raw|ConvertFrom-Json
        Assert-Equal 0 ([int]$record2.negativeChecks) 'unverified phase is never counted'
        Assert-Equal 0 (@($record2.phases).Count) 'unverified phase never reaches the journal'
        Assert-Equal $null $record2.activePhase 'unverified phase is never published as active'
    }finally{$VerbosePreference=$verbosePreferenceBefore}
}
}
if($IsWindows){Invoke-FixtureCase 'retry deadline is rechecked after the pre-retry identity verification' {
    param($fixture)
    $module=Get-Module|Where-Object Name -EQ 'ConsumerQualification'
    Assert-True ($null-ne $module) 'consumer module is present for the deadline verification shadow'
    $path=Join-Path $fixture 'deadline-progress.json';$p=New-ConsumerQualificationProgress $path @{commit='m'*40}
    $held=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
    $state=[pscustomobject]@{Calls=0;DelayAt=6;DelayMilliseconds=300;Original=$null;Held=$held;Released=$false;PendingPath=$null}
    $snapshotHook={
        param([string]$Path)
        $state.Calls++
        $snapshot=& $state.Original $Path
        if($state.Calls -eq $state.DelayAt){
            $state.PendingPath=$Path
            $state.Held.Dispose();$state.Released=$true
            Start-Sleep -Milliseconds $state.DelayMilliseconds
        }
        return $snapshot
    }.GetNewClosure()
    $body=[pscustomobject]@{Runs=0};$captured=@();$failure=$null;$originalModuleFunction=$null;$verbosePreferenceBefore=$VerbosePreference
    try{
        & $module {param($state) $state.Original=(Get-Command Get-ConsumerProgressFileSnapshot -CommandType Function).ScriptBlock} $state
        $originalModuleFunction=$state.Original
        & $module {param($hook) Set-Item -Path Function:script:Get-ConsumerProgressFileSnapshot -Value $hook} $snapshotHook
        try{$VerbosePreference='Continue';Invoke-ConsumerQualificationPhase $p 'DeadlineAfterVerify' {$body.Runs++} -NegativeCheck -Verbose 4>&1|ForEach-Object{$captured+=,$_}}catch{$failure=$_.Exception.GetBaseException()}finally{$VerbosePreference=$verbosePreferenceBefore}
    }finally{
        & $module {param($original) Set-Item -Path Function:script:Get-ConsumerProgressFileSnapshot -Value $original} $originalModuleFunction
        if(-not $held.SafeFileHandle.IsClosed){$held.Dispose()}
    }
    $messages=@($captured|Where-Object {$_ -is [Management.Automation.VerboseRecord]}|ForEach-Object Message)
    Write-Host "DEADLINE_CONTROL_RESULT released=$($state.Released) pendingExists=$([IO.File]::Exists($state.PendingPath)) bodyRuns=$($body.Runs) failureType=$(if($failure){$failure.GetType().FullName}else{'<none>'})"
    Assert-True $state.Released 'the exact held reader is released during the final pre-retry identity verification'
    Assert-True ([IO.File]::Exists($state.PendingPath)) 'the verified pending file remains present throughout the delay'
    Assert-True ($null-ne $failure) 'elapsed verification time refuses a new Replace call'
    Assert-Equal 32 ([int]([BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$failure.HResult),0)-band 0xffff)) 'deadline refusal preserves the original sharing failure'
    Assert-Equal 6 $state.Calls 'the deterministic delay occurs on the first pre-retry target/pending verification pair'
    Assert-True (($messages -join "`n") -match 'DXUI_CONSUMER_PUBLISH_RETRY_REFUSED code=32 attempt=1 reason=deadline-after-identity elapsedMs=') "deadline refusal is diagnosed after identity verification; calls=$($state.Calls), messages=$($messages -join '; '), failure=$($failure.Message)"
    Assert-True (($messages -join "`n") -notmatch 'DXUI_CONSUMER_PUBLISH_RETRY_SUCCESS|attempt=2 max=3') 'no second Replace call starts after the deadline expires'
    Assert-Equal 0 $body.Runs 'active publication failure prevents the phase body from running'
    $record=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json
    Assert-Equal 0 ([int]$record.negativeChecks) 'deadline refusal is not counted'
    Assert-Equal 0 (@($record.phases).Count) 'deadline refusal does not publish an active phase'
} }
if($IsWindows){Invoke-FixtureCase 'read-only target retry behavior records the actual Replace error and changes only its owned attribute' {
    param($fixture)
    $module=Get-Module|Where-Object Name -EQ 'ConsumerQualification'
    Assert-True ($null-ne $module) 'consumer module is present for the owned read-only control'
    $path=Join-Path $fixture 'readonly-progress.json';$p=New-ConsumerQualificationProgress $path @{commit='n'*40}
    [IO.File]::SetAttributes($path,[IO.File]::GetAttributes($path)-bor [IO.FileAttributes]::ReadOnly)
    $state=[pscustomobject]@{Path=$path;WaitCalls=0;Before=$null;After=$null}
    $hook={
        param([int]$Milliseconds)
        $state.WaitCalls++
        if($state.WaitCalls -eq 1){
            $state.Before=[IO.File]::GetAttributes($state.Path)
            [IO.File]::SetAttributes($state.Path,$state.Before-band (-bnot [IO.FileAttributes]::ReadOnly))
            $state.After=[IO.File]::GetAttributes($state.Path)
        }
        Microsoft.PowerShell.Utility\Start-Sleep -Milliseconds $Milliseconds
    }.GetNewClosure()
    $body=[pscustomobject]@{Runs=0};$captured=@();$failure=$null;$restored=$null;$verbosePreferenceBefore=$VerbosePreference
    try{
        & $module {param($hook) Set-Item -Path Function:script:Start-Sleep -Value $hook} $hook
        $VerbosePreference='Continue'
        try{$captured=@(Invoke-ConsumerQualificationPhase $p 'OwnedReadOnlyTarget' {$body.Runs++} -NegativeCheck -Verbose 4>&1)}catch{$failure=$_.Exception.GetBaseException()}
    }finally{
        $VerbosePreference=$verbosePreferenceBefore
        & $module {$null=Remove-Item Function:Start-Sleep -Force -ErrorAction SilentlyContinue;$script:retrySleepRestored=(Get-Command Start-Sleep -ErrorAction Stop).CommandType.ToString()}
        $restored=& $module {$script:retrySleepRestored}
        if(Test-Path -LiteralPath $path -PathType Leaf){[IO.File]::SetAttributes($path,[IO.File]::GetAttributes($path)-band (-bnot [IO.FileAttributes]::ReadOnly))}
    }
    $messages=@($captured|Where-Object {$_ -is [Management.Automation.VerboseRecord]}|ForEach-Object Message)
    $retryMatch=[regex]::Match(($messages -join "`n"),'DXUI_CONSUMER_PUBLISH_RETRY code=(\d+) attempt=1')
    $actualCode=if($retryMatch.Success){[int]$retryMatch.Groups[1].Value}else{if($null-ne $failure){$unsigned=[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$failure.HResult),0);if(($unsigned-shr 16)-eq 0x8007){[int]($unsigned-band 0xffff)}else{$null}}else{$null}}
    Write-Host "READONLY_TARGET_RESULT win32Code=$actualCode retryWaits=$($state.WaitCalls)"
    if($retryMatch.Success){
        Assert-True ($actualCode -in @(32,1175)) "observed read-only failure is one of the declared retryable codes, got $actualCode"
        Assert-Equal 1 $state.WaitCalls 'one retry wait handles the observed read-only failure'
        Assert-True (($state.Before-band [IO.FileAttributes]::ReadOnly)-ne 0) 'retry clears ReadOnly only on the exact owned destination'
        Assert-Equal 0 ([int]($state.After-band [IO.FileAttributes]::ReadOnly)) 'the owned target is writable before retry'
        Assert-Equal $null $failure 'retry succeeds after clearing the owned ReadOnly attribute'
        Assert-Equal 1 $body.Runs 'the phase body runs once after successful retry'
        Assert-Equal 1 ([int]((Get-Content -LiteralPath $path -Raw|ConvertFrom-Json).negativeChecks)) 'read-only recovery counts one successful phase'
    }else{
        Assert-True ($null-ne $failure) 'non-retryable read-only publication failure remains fatal'
        Assert-True ($actualCode -notin @(32,1175)) "actual read-only failure code is reported without inventing a retry, got $actualCode"
        Assert-Equal 0 $state.WaitCalls 'non-retryable read-only failure does not enter retry wait'
        Assert-Equal 0 $body.Runs 'non-retryable read-only failure prevents body execution'
        Assert-Equal 0 ([int]((Get-Content -LiteralPath $path -Raw|ConvertFrom-Json).negativeChecks)) 'non-retryable read-only failure is not counted'
    }
    Assert-Equal 'Cmdlet' $restored 'module-local sleep shadow is removed after the read-only control'
} }
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
        $exception=$_.Exception.GetBaseException();$unsigned=[BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$exception.HResult),0);$hrHex=[Convert]::ToString($unsigned,16).PadLeft(8,'0')
        [Console]::Error.WriteLine("CHILD_PHASE_FAILURE: type=$($exception.GetType().FullName) HResult=0x$hrHex")
        [Console]::Error.WriteLine($_.Exception.ToString())
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
        Write-Host "PERSISTENT_DENIED_DIAGNOSTIC: $failureDetails"
        Assert-True $earlyExit "active-publication failure exits before watchdog termination; $failureDetails"
        Assert-True ($child.ExitCode -ne 0) "active-publication failure remains a nonzero child result; $failureDetails"
        Assert-True (-not $ready) "failed active publication never creates the body-readiness marker; $failureDetails"
        Assert-True ($stderr -match 'CHILD_PHASE_FAILURE:') "publisher failure remains visible in child stderr; $failureDetails"
        Assert-True ($stderr -match 'HResult=0x80070020|HResult=0x80070497') "persistent lock retains its original numeric sharing error; $failureDetails"
        Assert-True ($readinessTimer.Elapsed -lt [TimeSpan]::FromSeconds(10)) "persistent publication denial fails within the unchanged readiness bound; $failureDetails"
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
