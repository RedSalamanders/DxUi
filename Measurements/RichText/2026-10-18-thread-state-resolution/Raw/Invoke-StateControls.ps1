[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh output required; preserve earlier evidence.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
. (Join-Path $PSScriptRoot 'ThreadStates.ps1') -FunctionsOnly
Import-Module (Join-Path $Root 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $Root 'Tools/PerformancePolicy.psm1') -Force
$anchor=Get-ProfilingMeasurementState -Root $Root -Platform x64 -Configuration Release
Assert-PerformanceBuildAttestation -Root $Root -Platform x64 -Configuration Release -State $anchor
$executable=Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe'
$executableSha=(Get-FileHash -LiteralPath $executable).Hash
$sourceFiles=@('ThreadStates.ps1','Test-ThreadStates.ps1','Invoke-StateControls.ps1')
$observerInputs=@($sourceFiles|ForEach-Object{@{name=$_;sha256=(Get-FileHash (Join-Path $PSScriptRoot $_)).Hash}})
$schedule=@($false,$true,$true,$false)
$started=[datetime]::UtcNow;$clock=[Diagnostics.Stopwatch]::StartNew();$cases=[Collections.Generic.List[object]]::new()
@{classification='same-binary external-observer diagnostic, not a baseline/candidate or acceptance verdict';anchor=$anchor;sourceFiles=$observerInputs;
 executableSha256=$executableSha;scheduled=4;observers=@('Off','On','On','Off');boundaryTimers='On';nominalSampleIntervalMilliseconds=2;
 maximumSamplesPerChild=4096;maximumThreadsPerSnapshot=256;childBudgetSeconds=120;overallBudgetSeconds=600;startedUtc=$started.ToString('o');
 runtime=[Environment]::Version.ToString();powerShell=$PSVersionTable.PSVersion.ToString();
 scope='Read only the retained native child through Process.Threads. Align query brackets to absolute frame/Map QPC edges; verify native PID/TID/creation. WaitReason is read only for Wait. No scheduler events, ready-time durations, stacks, GPU timestamps, elevation or policy changes.'}|
 ConvertTo-Json -Depth 32|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
try{
 for($index=0;$index -lt $schedule.Count;++$index){
  $remaining=[int][math]::Floor(600-$clock.Elapsed.TotalSeconds)
  if($remaining -le 0){throw 'Fixed diagnostic overall deadline reached.'}
  $observe=$schedule[$index];$mode=if($observe){'On'}else{'Off'}
  $stem='state-{0:d2}-{1}' -f ($index+1),$mode
  $path=Join-Path $OutputDirectory ($stem+'.native.receipt.txt')
  if((Get-FileHash -LiteralPath $executable).Hash -cne $executableSha){throw 'Native artifact changed.'}
  Write-Host "State control $($index+1)/4: $mode"
  $child=Invoke-OwnedThreadStateCase -Executable $executable -CommandArguments @('--benchmark-complex-ui-boundaries',$path,'On') -WorkingDirectory $Root `
    -OutputStem (Join-Path $OutputDirectory $stem) -Observe $observe -IntervalMilliseconds 2 -TimeoutSeconds ([math]::Min(120,$remaining))
  $valid=$false;$errorText='';$mainIdentity=$false;$captureSha='';$rawSha=''
  try{
   if($child.timedOut -or $child.exitCode -ne 0 -or $child.terminationError -or $child.drainError){throw 'Owned child did not complete cleanly.'}
   if(-not(Test-Path -LiteralPath $path) -or (Get-Item -LiteralPath $path).Length -gt 8MB){throw 'Missing/oversized native receipt.'}
   $native=Get-Content -LiteralPath $path -Raw|ConvertFrom-Json -AsHashtable -Depth 32
   [void](Assert-BoundaryReport -Receipt $native -Timers $true -Configuration Release -ProcessId $child.processId)
   if($native.clock.ticksPerSecond -ne [Diagnostics.Stopwatch]::Frequency){throw 'External QPC frequency differs.'}
   $rawSha=(Get-FileHash -LiteralPath $path).Hash
   $captureSha=(Get-FileHash -LiteralPath ($path+'.png')).Hash
   $states=Get-Content -LiteralPath (Join-Path $OutputDirectory ($stem+'.states.receipt.txt')) -Raw|ConvertFrom-Json -AsHashtable -Depth 16
   if($observe){
    if($child.sampleCount -le 0){throw 'Enabled observer collected no snapshots.'}
    $creation=@($states.firstObservedCreations|Where-Object threadId -EQ $native.mainThreadId)
    $mainIdentity=$creation.Count -eq 1 -and $creation[0].creation100ns -eq $native.mainThreadCreation100ns
    if(-not$mainIdentity){throw 'Native main thread creation identity does not match observer metadata.'}
   }elseif($child.sampleCount -ne 0){throw 'Disabled observer collected snapshots.'}
   $after=Get-ProfilingMeasurementState -Root $Root -Platform x64 -Configuration Release
   Assert-PerformanceMeasurementState -Before $anchor -After $after
   Assert-PerformanceBuildAttestation -Root $Root -Platform x64 -Configuration Release -State $after
   $valid=$true
  }catch [System.Exception]{$errorText=$_.Exception.Message}
  $cases.Add(@{index=$index+1;observer=$mode;nativeReport=Split-Path $path -Leaf;exitCode=$child.exitCode;timedOut=$child.timedOut;
    identityAndNativeValidationPassed=$valid;validationError=$errorText;mainIdentityVerified=$mainIdentity;sampleCount=$child.sampleCount;
    nativeSha256=$rawSha;captureSha256=$captureSha;child=$child})
  if(-not$valid){throw "Diagnostic case failed: $stem ($errorText)"}
 }
}finally{
 @{scheduled=4;attempted=$cases.Count;elapsedSeconds=$clock.Elapsed.TotalSeconds;overallBudgetExceeded=$clock.Elapsed.TotalSeconds -gt 600;
   cases=$cases.ToArray();acceptance='not-established'}|ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
Write-Host 'Four same-binary observer controls retained; scheduler-event and performance acceptance remain open.'
