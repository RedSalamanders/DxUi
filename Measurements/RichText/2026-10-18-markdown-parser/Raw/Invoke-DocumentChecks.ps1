[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-markdown-parser-20261018'
$started=[datetime]::UtcNow
$results=[Collections.Generic.List[object]]::new()
$steps=@(@{name='skills';script='validate-skills.ps1';arguments=@()},
 @{name='specs';script='validate-specs.ps1';arguments=@()},
 @{name='dependencies';script='validate-dependencies.ps1';arguments=@()},
 @{name='test-port';script='validate-test-port.ps1';arguments=@()},
 @{name='build-matrix';script='validate-build-matrix.ps1';arguments=@()},
 @{name='format';script='format.ps1';arguments=@('-Check','-FormatterPath','Z:\src\DxUi\.build\format\clang_format\data\bin\clang-format.exe')},
 @{name='prepush-scope';script='Test-Changes.ps1';arguments=@('-Mode','PrePush','-Explain')})
try{
 foreach($step in $steps){
  $remaining=[int][math]::Floor([math]::Min(90,180-([datetime]::UtcNow-$started).TotalSeconds))
  if($remaining -le 0){throw 'Document-check overall deadline reached.'}
  $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
  $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
  foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $Root $step.script))+$step.arguments){$start.ArgumentList.Add($argument)}
  $process=[Diagnostics.Process]::Start($start)
  try{
   $out=$process.StandardOutput.ReadToEndAsync();$err=$process.StandardError.ReadToEndAsync()
   $timedOut=-not$process.WaitForExit($remaining*1000)
   if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
   $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($out,$err),5000)
   if($drained){[IO.File]::WriteAllText((Join-Path $work ('document-'+$step.name+'.log')),$out.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $work ('document-'+$step.name+'.err')),$err.GetAwaiter().GetResult())}
   $code=if($process.HasExited){$process.ExitCode}else{$null}
   $results.Add(@{name=$step.name;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained;arguments=$start.ArgumentList})
   if($code -ne 0 -or $timedOut -or -not$drained){throw "Post-closeout document check failed: $($step.name)"}
  }finally{$process.Dispose()}
 }
}finally{
 @{classification='Post-qualification prose/evidence checks; compiled inputs unchanged, complete tooling already passed on the frozen run';scheduled=7;attempted=$results.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()}|
  ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $work 'document-checks.receipt.txt')
}
Write-Host 'All five document validators, pinned format and PrePush scope explanation passed.'
