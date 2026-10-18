[CmdletBinding()]param([string]$OutputDirectory,[switch]$FunctionsOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Get-ContainedBoundary([object[]]$Frames,[long]$Started,[long]$Ended){
 if($Ended -lt $Started){throw 'Reversed snapshot acquisition interval.'}
 $low=0;$high=$Frames.Count
 while($low -lt $high){$middle=$low+[int][math]::Floor(($high-$low)/2);if([long]$Frames[$middle].qpc[0] -le $Started){$low=$middle+1}else{$high=$middle}}
 if($low -eq 0){return @{kind='outside-or-cross-frame';window=$null;frame=$null}}
 $frame=$Frames[$low-1]
 if($Ended -gt [long]$frame.qpc[8]){return @{kind='outside-or-cross-frame';window=$null;frame=$null}}
 $names=@('prepare','bind','clear','composite','copy','Map','Unmap','tail')
 for($edge=0;$edge -lt 8;++$edge){
  # Zero-width edges do not own observations. A point exactly on a boundary
  # belongs to the following nonempty interval, never to two segments.
  if([long]$frame.qpc[$edge] -le $Started -and $Started -lt [long]$frame.qpc[$edge+1] -and $Ended -le [long]$frame.qpc[$edge+1]){
   return @{kind=$names[$edge];window=$frame.window;frame=$frame.frame}
  }
 }
 return @{kind='mixed-within-frame';window=$frame.window;frame=$frame.frame}
}
function Get-NearestRank([double[]]$Values,[double]$Percentile){
 if($Values.Count -eq 0){throw 'Empty statistic input.'}
 $ordered=@($Values|Sort-Object)
 return $ordered[[math]::Max(0,[int][math]::Ceiling($Percentile*$ordered.Count)-1)]
}
if($FunctionsOnly){return}
$completion=Get-Content (Join-Path $OutputDirectory 'completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($completion.attempted -ne 4 -or $completion.overallBudgetExceeded){throw 'Incomplete fixed observer schedule.'}
$results=[Collections.Generic.List[object]]::new()
foreach($case in $completion.cases){
 if(-not$case.identityAndNativeValidationPassed){throw 'Original case validation failed.'}
 $stem=[IO.Path]::GetFileNameWithoutExtension($case.nativeReport).Replace('.native.receipt','')
 $nativePath=Join-Path $OutputDirectory $case.nativeReport
 if((Get-FileHash $nativePath).Hash -cne $case.nativeSha256){throw 'Native report hash mismatch.'}
 if((Get-FileHash ($nativePath+'.png')).Hash -cne $case.captureSha256){throw 'Capture hash mismatch.'}
 $native=Get-Content $nativePath -Raw|ConvertFrom-Json -AsHashtable -Depth 32
 $statesPath=Join-Path $OutputDirectory ($stem+'.states.receipt.txt')
 $states=Get-Content $statesPath -Raw|ConvertFrom-Json -AsHashtable -Depth 32
 $frames=[Collections.Generic.List[object]]::new();$windowResults=[Collections.Generic.List[object]]::new()
 for($window=0;$window -lt $native.windows.Count;++$window){
  $times=[Collections.Generic.List[double]]::new();$mapTimes=[Collections.Generic.List[double]]::new();$frameRecords=[Collections.Generic.List[object]]::new()
  for($index=0;$index -lt $native.windows[$window].frames.Count;++$index){
   $qpc=$native.windows[$window].frames[$index].qpc
   $frames.Add(@{window=$window;frame=$index;qpc=$qpc})
   $elapsed=1000.0*([long]$qpc[8]-[long]$qpc[0])/$native.clock.ticksPerSecond
   $mapElapsed=1000.0*([long]$qpc[6]-[long]$qpc[5])/$native.clock.ticksPerSecond
   $times.Add($elapsed);$mapTimes.Add($mapElapsed);$frameRecords.Add(@{frame=$index;elapsedMilliseconds=$elapsed;mapMilliseconds=$mapElapsed})
  }
  $rank=@($frameRecords|Sort-Object elapsedMilliseconds,frame)[[int][math]::Ceiling(.95*$frameRecords.Count)-1]
  $windowResults.Add(@{window=$window;clear=$native.windows[$window].clear;composite=$native.windows[$window].composite;frameCount=$times.Count;
   frameMedianMilliseconds=(Get-NearestRank $times.ToArray() .5);frameP95Milliseconds=(Get-NearestRank $times.ToArray() .95);
   frameMaximumMilliseconds=($times|Measure-Object -Maximum).Maximum;mapP95Milliseconds=(Get-NearestRank $mapTimes.ToArray() .95);
   p95Frame=$rank.frame;mapShareAtFrameP95Percent=$(if($rank.elapsedMilliseconds -gt 0){100*$rank.mapMilliseconds/$rank.elapsedMilliseconds}else{$null})})
 }
 $joinRows=[Collections.Generic.List[object]]::new();$queryTimes=[Collections.Generic.List[double]]::new();$statuses=@{};$groups=@{};$mapMainStates=@{};$mapMainWaitReasons=@{};$mapWindowCounts=@{};$threadCounts=[Collections.Generic.List[double]]::new()
 $mainMissing=0;$mapMainSamples=0;$mapWithReadyWorker=0;$mapReadyWorkerRows=0;$errors=0
 foreach($sample in $states.samples){
  $threadCounts.Add(@($sample.rows).Count)
  $queryTimes.Add(1000.0*([long]$sample.queryEndedQpc-[long]$sample.queryStartedQpc)/$native.clock.ticksPerSecond)
  $status=[string]$sample.status;if(-not$statuses.ContainsKey($status)){$statuses[$status]=0};++$statuses[$status];$errors+=@($sample.errors).Count
  $joined=Get-ContainedBoundary $frames.ToArray() $sample.queryStartedQpc $sample.queryEndedQpc
  $kind=[string]$joined.kind;if(-not$groups.ContainsKey($kind)){$groups[$kind]=0};++$groups[$kind]
  $main=@($sample.rows|Where-Object {$_[0] -eq $native.mainThreadId})
  if($main.Count -gt 1){throw 'Duplicate main thread row in snapshot.'}
  if($main.Count -eq 0){++$mainMissing}
  $readyWorkers=@($sample.rows|Where-Object {$_[0] -ne $native.mainThreadId -and $_[1] -eq 1}).Count
  if($kind -ceq 'Map' -and $main.Count -eq 1){
   ++$mapMainSamples;$windowKey=[string]$joined.window;if(-not$mapWindowCounts.ContainsKey($windowKey)){$mapWindowCounts[$windowKey]=0};++$mapWindowCounts[$windowKey];$state=[string]$main[0][1];if(-not$mapMainStates.ContainsKey($state)){$mapMainStates[$state]=0};++$mapMainStates[$state]
   if($main[0][1] -eq 5){$reason=[string]$main[0][2];if(-not$mapMainWaitReasons.ContainsKey($reason)){$mapMainWaitReasons[$reason]=0};++$mapMainWaitReasons[$reason]}
   if($readyWorkers -gt 0){++$mapWithReadyWorker};$mapReadyWorkerRows+=$readyWorkers
  }
  $joinRows.Add(@{queryStartedQpc=$sample.queryStartedQpc;queryEndedQpc=$sample.queryEndedQpc;kind=$kind;window=$joined.window;frame=$joined.frame;
   status=$status;mainState=$(if($main.Count){$main[0][1]}else{$null});mainWaitReason=$(if($main.Count){$main[0][2]}else{$null});readyWorkerRows=$readyWorkers})
 }
 if($case.observer -ceq 'On' -and -not$case.mainIdentityVerified){throw 'Main thread creation is unverified.'}
 $joinRows.ToArray()|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory ($stem+'.joined.receipt.txt'))
 $results.Add(@{index=$case.index;observer=$case.observer;nativeReport=$case.nativeReport;nativeSha256=$case.nativeSha256;captureSha256=$case.captureSha256;
  stateReportSha256=(Get-FileHash $statesPath).Hash;snapshotCount=@($states.samples).Count;samplingTruncated=$case.child.samplingTruncated;
  queryMedianMilliseconds=$(if($queryTimes.Count){Get-NearestRank $queryTimes.ToArray() .5}else{$null});
  queryP95Milliseconds=$(if($queryTimes.Count){Get-NearestRank $queryTimes.ToArray() .95}else{$null});
  statuses=$statuses;snapshotErrors=$errors;alignmentGroups=$groups;mainMissingSnapshots=$mainMissing;mapMainSamples=$mapMainSamples;
  mapMainStateCounts=$mapMainStates;mapSnapshotCountsByWindow=$mapWindowCounts;
  threadRowsMedian=$(if($threadCounts.Count){Get-NearestRank $threadCounts.ToArray() .5}else{$null});threadRowsMaximum=$(if($threadCounts.Count){($threadCounts|Measure-Object -Maximum).Maximum}else{$null});mapMainWaitReasonCounts=$mapMainWaitReasons;mapSnapshotsWithReadyWorkers=$mapWithReadyWorker;mapReadyWorkerRows=$mapReadyWorkerRows;windows=$windowResults.ToArray()})
}
@{classification='Same-binary external-observer diagnostic; no baseline/candidate comparison or acceptance';cases=$results.ToArray();
 analyzerSha256=(Get-FileHash $PSCommandPath).Hash;stateEnums='System.Diagnostics.ThreadState: Ready=1, Running=2, Standby=3, Wait=5; raw enums retained in each state report.';
 alignment='The full Refresh/Threads acquisition bracket must fit inside one segment; mixed/cross-frame/outside observations remain separate. Creation identity is checked as Int64. Rows are cached snapshot values, not a simultaneous multi-thread sample.';
 limitations='Snapshots are unweighted counts and do not estimate occupancy, scheduler delay, ready-time latency, stacks, GPU operation duration or causality. Snapshot acquisition and observer process scheduling can perturb timings. WaitReason Unknown is not decoded into an invented native kernel reason.'}|
 ConvertTo-Json -Depth 24|Set-Content (Join-Path $OutputDirectory 'analysis.receipt.txt')
Write-Host 'Native/frame alignment, unweighted main/worker state counts and observer controls replayed.'
