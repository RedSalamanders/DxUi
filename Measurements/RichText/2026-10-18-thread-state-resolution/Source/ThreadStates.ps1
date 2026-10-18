[CmdletBinding()]param([switch]$FunctionsOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'

function Get-ObservedThreadRow([object]$Thread){
 $state=$Thread.ThreadState
 $reason=if($state -eq [Diagnostics.ThreadState]::Wait){[int]$Thread.WaitReason}else{$null}
 return ,([object[]]@([int]$Thread.Id,[int]$state,$reason))
}
function Get-OwnedThreadSnapshot([Diagnostics.Process]$Process,[Collections.Generic.Dictionary[int,long]]$Creations){
 $queryStarted=[Diagnostics.Stopwatch]::GetTimestamp()
 try{
  $Process.Refresh()
  $threads=$Process.Threads
  $queryEnded=[Diagnostics.Stopwatch]::GetTimestamp()
 }catch [InvalidOperationException]{return @{queryStartedQpc=$queryStarted;queryEndedQpc=[Diagnostics.Stopwatch]::GetTimestamp();status='process-ended';rows=@();errors=@('InvalidOperationException')}}
 catch [ComponentModel.Win32Exception]{return @{queryStartedQpc=$queryStarted;queryEndedQpc=[Diagnostics.Stopwatch]::GetTimestamp();status='unavailable';rows=@();errors=@($_.Exception.NativeErrorCode)}}
 $rows=[Collections.Generic.List[object]]::new();$errors=[Collections.Generic.List[object]]::new();$truncated=$false
 try{
  foreach($thread in $threads){
   try{
    if($rows.Count -ge 256){$truncated=$true;continue}
    $rows.Add((Get-ObservedThreadRow $thread))
    if(-not$Creations.ContainsKey($thread.Id)){
     try{$Creations[$thread.Id]=$thread.StartTime.ToFileTimeUtc()}
     catch [InvalidOperationException]{$errors.Add(@{id=$thread.Id;property='creation';error='thread-ended'})}
     catch [ComponentModel.Win32Exception]{$errors.Add(@{id=$thread.Id;property='creation';error=$_.Exception.NativeErrorCode})}
    }
   }catch [InvalidOperationException]{$errors.Add(@{id=$thread.Id;property='state';error='thread-ended'})}
   catch [ComponentModel.Win32Exception]{$errors.Add(@{id=$thread.Id;property='state';error=$_.Exception.NativeErrorCode})}
   finally{$thread.Dispose()}
  }
 }finally{
  # All ProcessThread wrappers are disposed in the loop, including truncated rows.
 }
 return @{queryStartedQpc=$queryStarted;queryEndedQpc=$queryEnded;readEndedQpc=[Diagnostics.Stopwatch]::GetTimestamp();
   status=$(if($truncated){'truncated'}elseif($errors.Count){'partial'}else{'complete'});rows=$rows.ToArray();errors=$errors.ToArray()}
}

# Instrumented private derivative of Tools/PerformanceProfiling.psm1's bounded
# owned-process runner. Only the retained child is observed/terminated. Sampling
# stays in this PowerShell process; no target thread, privilege or policy changes.
function Invoke-OwnedThreadStateCase {
 [CmdletBinding()]param([string]$Executable,[string[]]$CommandArguments,[string]$WorkingDirectory,[string]$OutputStem,
  [bool]$Observe,[ValidateRange(1,50)][int]$IntervalMilliseconds=2,[ValidateRange(1,120)][int]$TimeoutSeconds=120)
 $start=[Diagnostics.ProcessStartInfo]::new($Executable)
 $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 $start.WorkingDirectory=$WorkingDirectory
 foreach($argument in $CommandArguments){$start.ArgumentList.Add($argument)}
 $samples=[Collections.Generic.List[object]]::new();$creations=[Collections.Generic.Dictionary[int,long]]::new()
 $samplingTruncated=$false;$maximumSamples=4096
 $started=[datetime]::UtcNow;$process=[Diagnostics.Process]::Start($start);$clock=[Diagnostics.Stopwatch]::StartNew()
 try{
  $pipes=@(foreach($reader in @($process.StandardOutput,$process.StandardError)){
   $buffer=[char[]]::new(4096)
   @{reader=$reader;buffer=$buffer;task=$reader.ReadAsync($buffer,0,$buffer.Length);text=[Text.StringBuilder]::new();closed=$false;truncated=$false;error=''}
  })
  $exitTask=$process.WaitForExitAsync();$timedOut=$false;$terminationError='';$drainError='';$drainDeadline=$null
  $nextSampleMs=0.0;$samplingEnabled=$Observe
  while($true){
   if($null -eq $drainDeadline -and -not$process.HasExited -and $clock.Elapsed.TotalSeconds -ge $TimeoutSeconds){
    $timedOut=$true
    try{$process.Kill()}catch [InvalidOperationException]{if(-not$process.HasExited){$terminationError=$_.Exception.Message}}
    catch [ComponentModel.Win32Exception]{$terminationError=$_.Exception.Message}
    if(-not$process.WaitForExit(5000)){$terminationError='Owned native profiling child did not terminate.'}
    $drainDeadline=$clock.Elapsed.TotalMilliseconds+5000
   }
   if($null -eq $drainDeadline -and $process.HasExited){$drainDeadline=$clock.Elapsed.TotalMilliseconds+5000}
   foreach($pipe in $pipes){
    if($pipe.closed -or -not$pipe.task.IsCompleted){continue}
    try{
     $count=$pipe.task.GetAwaiter().GetResult()
     if($count -eq 0){$pipe.closed=$true;continue}
     $retained=[math]::Min($count,1048576-$pipe.text.Length)
     if($retained -gt 0){[void]$pipe.text.Append($pipe.buffer,0,$retained)}
     if($retained -lt $count){$pipe.truncated=$true}
     $pipe.task=$pipe.reader.ReadAsync($pipe.buffer,0,$pipe.buffer.Length)
    }catch [IO.IOException]{$pipe.error=$_.Exception.Message;$pipe.closed=$true}
   }
   if(($process.HasExited -or $timedOut) -and $pipes[0].closed -and $pipes[1].closed){break}
   if($null -ne $drainDeadline -and $clock.Elapsed.TotalMilliseconds -ge $drainDeadline){$drainError='Output drain exceeded five seconds.';break}
   if($samplingEnabled -and -not$process.HasExited -and -not$timedOut -and $clock.Elapsed.TotalMilliseconds -ge $nextSampleMs){
    if($samples.Count -lt $maximumSamples){$samples.Add((Get-OwnedThreadSnapshot $process $creations))}else{$samplingTruncated=$true;$samplingEnabled=$false}
    $nextSampleMs=$clock.Elapsed.TotalMilliseconds+$IntervalMilliseconds
   }
   $tasks=[Collections.Generic.List[Threading.Tasks.Task]]::new()
   if(-not$process.HasExited -and -not$timedOut){$tasks.Add($exitTask)}
   foreach($pipe in $pipes){if(-not$pipe.closed){$tasks.Add($pipe.task)}}
   $deadline=if($null -eq $drainDeadline){$TimeoutSeconds*1000}else{$drainDeadline}
   $waitMs=[math]::Max(1,[int][math]::Ceiling($deadline-$clock.Elapsed.TotalMilliseconds))
   if($samplingEnabled -and -not$process.HasExited -and -not$timedOut){$waitMs=[math]::Min($waitMs,[math]::Max(1,[int][math]::Ceiling($nextSampleMs-$clock.Elapsed.TotalMilliseconds)))}
   [void][Threading.Tasks.Task]::WaitAny($tasks.ToArray(),$waitMs)
  }
  foreach($pipe in $pipes){if($pipe.error){$drainError+=' Pipe error: '+$pipe.error};if($pipe.truncated){$drainError+=' One-MiB pipe limit reached; prefix retained.'}}
  $metadata=@{processId=$process.Id;executable=$Executable;arguments=$CommandArguments;startedUtc=$started.ToString('o');completedUtc=[datetime]::UtcNow.ToString('o');
   elapsedSeconds=$clock.Elapsed.TotalSeconds;timeoutSeconds=$TimeoutSeconds;exitCode=$(if($process.HasExited){$process.ExitCode}else{$null});timedOut=$timedOut;
   terminationError=$terminationError;drainError=$drainError;observerEnabled=$Observe;nominalIntervalMilliseconds=$IntervalMilliseconds;maximumSamples=$maximumSamples;
   samplingTruncated=$samplingTruncated;sampleCount=$samples.Count;clockFrequency=[Diagnostics.Stopwatch]::Frequency;
   measurement='Owned-child thread-state snapshots and wait reasons; query brackets are QPC. These are sampled observations, without context-switch events, ready-time latency, stacks, GPU duration or performance acceptance.'}
  [IO.File]::WriteAllText(($OutputStem+'.log'),$pipes[0].text.ToString())
  [IO.File]::WriteAllText(($OutputStem+'.err'),$pipes[1].text.ToString())
  $metadata|ConvertTo-Json -Depth 8|Set-Content ($OutputStem+'.child.receipt.txt')
  @{rowColumns=@('threadId','System.Diagnostics.ThreadState','System.Diagnostics.ThreadWaitReasonOrNull');
    states=@([Enum]::GetValues([Diagnostics.ThreadState])|ForEach-Object{@{value=[int]$_;name=$_.ToString()}});
    waitReasons=@([Enum]::GetValues([Diagnostics.ThreadWaitReason])|ForEach-Object{@{value=[int]$_;name=$_.ToString()}});
    firstObservedCreations=@($creations.GetEnumerator()|Sort-Object Key|ForEach-Object{@{threadId=$_.Key;creation100ns=$_.Value}});
    workerIdReuseScope='Creation is recorded on first observation. Worker-ID reuse between snapshots is not attributed; verify the reported native main-thread ID/creation before main-thread analysis.';
    samples=$samples.ToArray()}|ConvertTo-Json -Depth 12 -Compress|Set-Content ($OutputStem+'.states.receipt.txt')
  return $metadata
 }finally{
  if(-not$process.HasExited){$process.Kill();[void]$process.WaitForExit(5000)}
  $process.Dispose()
 }
}

if(-not$FunctionsOnly){throw 'Private function library: dot-source with -FunctionsOnly from the fixed diagnostic controller.'}
