[CmdletBinding()]param([string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$completion=Get-Content (Join-Path $OutputDirectory 'completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($completion.scheduled -ne 4 -or $completion.attempted -ne 4 -or $completion.elapsedSeconds -gt 900 -or
 ($completion.cases.mode -join ',') -cne 'Default,CallingThread,CallingThread,Default'){throw 'Incomplete fixed device-policy schedule.'}
$results=@(foreach($case in $completion.cases){
 if(-not$case.validationPassed){throw 'Original control failed validation.'}
 $path=Join-Path $OutputDirectory $case.report
 if((Get-FileHash ($path+'.raw.json')).Hash -cne $case.rawSha256 -or (Get-FileHash ($path+'.png')).Hash -cne $case.captureSha256){throw 'Original raw report/capture changed.'}
 $native=Get-Content ($path+'.raw.json') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
 $windows=@(for($index=0;$index -lt 8;++$index){
  $window=$native.windows[$index];$frames=@(for($frame=0;$frame -lt 40;++$frame){
   $qpc=$window.frames[$frame].qpc
   @{frame=$frame;elapsedMs=1000.0*([long]$qpc[8]-[long]$qpc[0])/$native.clock.ticksPerSecond;
    mapMs=1000.0*([long]$qpc[6]-[long]$qpc[5])/$native.clock.ticksPerSecond}
  })
  $ordered=@($frames|Sort-Object elapsedMs,frame);$p95=$ordered[37];$cpu=@{}
  foreach($scope in @('main','process')){
   $before=$window.cpuBefore[$scope];$after=$window.cpuAfter[$scope]
   $cpu[$scope]=@{timesAvailable=$before.timesAvailable -and $after.timesAvailable;cyclesAvailable=$before.cyclesAvailable -and $after.cyclesAvailable;
    cpuMs=$(if($before.timesAvailable -and $after.timesAvailable){([decimal]$after.kernel100ns+$after.user100ns-$before.kernel100ns-$before.user100ns)/10000}else{$null});
    cycles=$(if($before.cyclesAvailable -and $after.cyclesAvailable){[decimal]$after.cycles-$before.cycles}else{$null})}
  }
  @{window=$index;clear=$window.clear;composite=$window.composite;frameCount=40;frameMedianMs=$ordered[19].elapsedMs;frameP95Ms=$p95.elapsedMs;
   maximumMs=$ordered[39].elapsedMs;mapP95Ms=@($frames.mapMs|Sort-Object)[37];p95Frame=$p95.frame;mapShareAtFrameP95Percent=100*$p95.mapMs/$p95.elapsedMs;
   windowElapsedMs=1000.0*([long]$window.endedQpc-[long]$window.startedQpc)/$native.clock.ticksPerSecond;cpu=$cpu}
 })
 @{index=$case.index;mode=$case.mode;report=$case.report;rawSha256=$case.rawSha256;captureSha256=$case.captureSha256;
  requestedFlags=$native.requestedDeviceFlags;actualFlags=$native.deviceCreationFlags;windows=$windows}
})
@{classification='Descriptive same-binary execution-policy controls; no scheduler-latency, GPU-duration or acceptance verdict';
 cases=$results;analyzerSha256=(Get-FileHash $PSCommandPath).Hash;
 limitations='Two fresh processes per policy do not establish stability or an accepted baseline/candidate comparison. Device policy changes work placement/runtime behavior; component costs cannot be inferred by subtracting windows. CPU time counters are quantized and are not simultaneous.'}|
 ConvertTo-Json -Depth 32|Set-Content (Join-Path $OutputDirectory 'analysis.receipt.txt')
Write-Host 'All original policy-control frames, CPU deltas and descriptive window statistics replayed.'
