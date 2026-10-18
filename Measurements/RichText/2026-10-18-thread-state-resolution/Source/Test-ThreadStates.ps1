[CmdletBinding()]param([string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'ThreadStates.ps1') -FunctionsOnly
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
$running=[pscustomobject]@{Id=41;ThreadState=[Diagnostics.ThreadState]::Running}
$running|Add-Member ScriptProperty WaitReason {throw 'WaitReason must only be read for Wait.'}
$row=Get-ObservedThreadRow $running
if($row.Count -ne 3 -or $row[0] -ne 41 -or $row[1] -ne 2 -or $null -ne $row[2]){throw 'Running thread row is invalid.'}
$waiting=[pscustomobject]@{Id=42;ThreadState=[Diagnostics.ThreadState]::Wait;WaitReason=[Diagnostics.ThreadWaitReason]::ExecutionDelay}
$row=Get-ObservedThreadRow $waiting
if($row[0] -ne 42 -or $row[1] -ne 5 -or $row[2] -ne 4){throw 'Wait row does not retain the reported reason.'}
$target=Join-Path $OutputDirectory 'OwnedThreadTarget.ps1'
@'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class OwnedThreadIdentity {
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
}
"@
$identity=@{threadId=[OwnedThreadIdentity]::GetCurrentThreadId();creation100ns=(Get-Process -Id $PID).Threads|Where-Object Id -EQ ([OwnedThreadIdentity]::GetCurrentThreadId())|ForEach-Object {$_.StartTime.ToFileTimeUtc()}}
$identity|ConvertTo-Json -Compress
[Threading.Thread]::Sleep(500)
$clock=[Diagnostics.Stopwatch]::StartNew()
while($clock.ElapsedMilliseconds -lt 500){[void][Diagnostics.Stopwatch]::GetTimestamp()}
'@|Set-Content -LiteralPath $target
$started=[datetime]::UtcNow
$on=Invoke-OwnedThreadStateCase -Executable (Get-Command pwsh).Source -CommandArguments @('-NoProfile','-NonInteractive','-File',$target) -WorkingDirectory $OutputDirectory -OutputStem (Join-Path $OutputDirectory 'self-on') -Observe $true -IntervalMilliseconds 10 -TimeoutSeconds 30
if($on.exitCode -ne 0 -or $on.timedOut -or $on.drainError -or $on.sampleCount -le 0){throw 'Owned on observer did not complete.'}
$identity=Get-Content (Join-Path $OutputDirectory 'self-on.log') -Raw|ConvertFrom-Json
$states=Get-Content (Join-Path $OutputDirectory 'self-on.states.receipt.txt') -Raw|ConvertFrom-Json
$creation=@($states.firstObservedCreations|Where-Object threadId -EQ $identity.threadId)
if($creation.Count -ne 1 -or $creation[0].creation100ns -ne $identity.creation100ns){throw 'Native thread identity does not match observed creation.'}
$ownRows=@(foreach($sample in $states.samples){foreach($observed in $sample.rows){if($observed[0] -eq $identity.threadId){,$observed}}})
$waits=@($ownRows|Where-Object {$_[1] -eq 5}).Count
$active=@($ownRows|Where-Object {$_[1] -in @(1,2,3)}).Count
if($waits -eq 0 -or $active -eq 0){throw 'Owned wait/busy fixture does not independently exercise both state groups.'}
$off=Invoke-OwnedThreadStateCase -Executable (Get-Command pwsh).Source -CommandArguments @('-NoProfile','-NonInteractive','-File',$target) -WorkingDirectory $OutputDirectory -OutputStem (Join-Path $OutputDirectory 'self-off') -Observe $false -TimeoutSeconds 30
if($off.exitCode -ne 0 -or $off.sampleCount -ne 0 -or $off.timedOut -or $off.drainError){throw 'Disabled observer did not remain disabled.'}
@{classification='functional sampler checks, not benchmark or scheduler-event qualification';runningReasonGetterNotRead=$true;waitReasonPreserved=$true;
 nativeThreadId=$identity.threadId;creationMatched=$true;waitSamples=$waits;readyRunningStandbySamples=$active;enabledSnapshots=$on.sampleCount;disabledSnapshots=$off.sampleCount;
 elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;helperSha256=(Get-FileHash (Join-Path $PSScriptRoot 'ThreadStates.ps1')).Hash}|
 ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'self-test.receipt.txt')
Write-Host 'Owned-child identity, wait/busy rows, conditional wait-reason reads and disabled observer validated.'
