[CmdletBinding()] param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow
$results=[Collections.Generic.List[object]]::new()
$steps=@(@{name='validation-full';script='validate.ps1';arguments=@()},
 @{name='format-check';script='format.ps1';arguments=@('-Check','-FormatterPath','Z:\src\DxUi\.build\format\clang_format\data\bin\clang-format.exe')})
@{startedUtc=$started.ToString('o');overallBudgetSeconds=900;ownedChildBudgetSeconds=600;steps=$steps;notes='Starts only after all native benchmark/test slots; ARM64 cross-builds may run concurrently. All repository inputs, including prose, remain frozen.'}|
 ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'tooling-plan.receipt.txt')
try{
 foreach($step in $steps){
  $remaining=[math]::Min(600,[math]::Floor(900-([datetime]::UtcNow-$started).TotalSeconds))
  if($remaining -le 0){throw 'Tooling schedule deadline reached.'}
  $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
  $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
  foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root $step.script))+$step.arguments){$start.ArgumentList.Add($argument)}
  $process=[Diagnostics.Process]::Start($start)
  try{
   $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
   $timedOut=-not $process.WaitForExit([int]($remaining*1000))
   if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
   $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
   if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($step.name+'.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($step.name+'.err')),$stderr.GetAwaiter().GetResult())}
   $code=if($process.HasExited){$process.ExitCode}else{$null}
   $results.Add(@{step=$step.name;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained})
  }finally{$process.Dispose()}
  if($code -ne 0 -or $timedOut -or -not$drained){throw "Tooling step failed: $($step.name)"}
 }
}finally{@{scheduled=2;attempted=$results.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'tooling-completion.receipt.txt')}
Write-Host 'Full validation and pinned formatting passed.'
