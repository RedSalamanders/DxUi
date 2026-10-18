[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh policy-control output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
Import-Module (Join-Path $Root 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $Root 'Tools/PerformancePolicy.psm1') -Force
$anchor=Get-ProfilingMeasurementState -Root $Root -Platform x64 -Configuration Release
Assert-PerformanceBuildAttestation -Root $Root -Platform x64 -Configuration Release -State $anchor
$started=[datetime]::UtcNow;$schedule=@('Default','CallingThread','CallingThread','Default');$records=[Collections.Generic.List[object]]::new()
@{classification='Same-binary WARP execution-policy intervention; not baseline/candidate acceptance or scheduler latency';anchor=$anchor;
 schedule=$schedule;overallBudgetSeconds=900;wrapperBudgetSeconds=180;nativeChildBudgetSeconds=120;boundaryTimers='On';observer='Off';
 sourceSha256=(Get-FileHash $PSCommandPath).Hash;expectedCaptureSha256='6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD'}|
 ConvertTo-Json -Depth 32|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
try{
 for($index=0;$index -lt $schedule.Count;++$index){
  $mode=$schedule[$index];$stem='policy-{0:d2}-{1}' -f ($index+1),$mode;$path=Join-Path $OutputDirectory ($stem+'.json')
  $budget=[int][math]::Floor([math]::Min(180,900-([datetime]::UtcNow-$started).TotalSeconds));if($budget -le 0){throw 'Fixed policy-control deadline reached.'}
  $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source);$start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
  $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
  foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'performance-profile.ps1'),'-Configuration','Release','-Platform','x64','-SkipBuild',
   '-BoundaryTimers','On','-WarpRasterMode',$mode,'-OutputPath',$path)){$start.ArgumentList.Add($argument)}
  Write-Host "WARP control $($index+1)/4: $mode"
  $process=[Diagnostics.Process]::Start($start)
  try{
   $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$timedOut=-not$process.WaitForExit($budget*1000)
   if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
   $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
   if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($stem+'.runner.err')),$stderr.GetAwaiter().GetResult())}
   $valid=$false;$errorText='';$capture='';$rawHash='';$childId=$null
   try{
    if($timedOut -or -not$drained -or $process.ExitCode -ne 0){throw 'Owned policy wrapper failed or did not drain.'}
    $report=Get-Content $path -Raw|ConvertFrom-Json -AsHashtable -Depth 32
    $child=Get-Content ($path+'.child.json') -Raw|ConvertFrom-Json -AsHashtable -Depth 16;$childId=$child.processId
    [void](Assert-BoundaryReport -Receipt $report -Timers $true -WarpRasterMode $mode -Configuration Release -ProcessId $childId)
    $observed=[ordered]@{};foreach($key in $anchor.Keys){$observed[$key]=$report[$key]}
    Assert-PerformanceMeasurementState -Before $anchor -After $observed
    $capture=(Get-FileHash ($path+'.png')).Hash;$rawHash=(Get-FileHash ($path+'.raw.json')).Hash
    if($capture -cne '6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD' -or $capture -cne $report.captureSha256 -or $rawHash -cne $report.rawReportSha256){throw 'Policy capture/native bytes differ from the required scene or receipt.'}
    $valid=$true
   }catch [System.Exception]{$errorText=$_.Exception.Message}
   $records.Add(@{index=$index+1;mode=$mode;report=Split-Path $path -Leaf;wrapperPid=$process.Id;nativePid=$childId;exitCode=$process.ExitCode;timedOut=$timedOut;
    drained=$drained;validationPassed=$valid;validationError=$errorText;captureSha256=$capture;rawSha256=$rawHash})
   if(-not$valid){throw "Policy-control validation failed: $errorText"}
  }finally{$process.Dispose()}
 }
}finally{
 @{scheduled=4;attempted=$records.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;cases=$records.ToArray();acceptance='not-established'}|
 ConvertTo-Json -Depth 24|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
Write-Host 'Four source-bound WARP policy controls retained, with exact matching populated captures and native invariants.'
