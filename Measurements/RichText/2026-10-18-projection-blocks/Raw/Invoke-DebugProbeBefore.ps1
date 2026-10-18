[CmdletBinding()] param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow
$results=[Collections.Generic.List[object]]::new()
@{scheduled=2;startedUtc=$started.ToString('o');overallBudgetSeconds=900;buildChildBudgetSeconds=600;probeChildBudgetSeconds=60;
 expectedProbeExit=197;probe='--test-rich-snapshot-slice-failures';
 notes='Root x64 Debug build, then one isolated owned CPU-only native diagnostic. Its terminate handler exits 197 without a CRT dialog. This is not a suite success or a canonical frame measurement. Source remains frozen; original build/stream/source/artifact records are retained.'}|
 ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'debug-before-plan.receipt.txt')
function Invoke-Owned([string]$Name,[string]$Executable,[string[]]$Arguments,[int]$Seconds){
 $start=[Diagnostics.ProcessStartInfo]::new($Executable)
 $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in $Arguments){$start.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($start)
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  $timedOut=-not$process.WaitForExit($Seconds*1000)
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.err')),$stderr.GetAwaiter().GetResult())}
  $code=if($process.HasExited){$process.ExitCode}else{$null}
  $record=@{name=$Name;processId=$process.Id;arguments=$Arguments;exitCode=$code;timedOut=$timedOut;drained=$drained;completedUtc=[datetime]::UtcNow.ToString('o')}
  $results.Add($record)
  if($timedOut -or -not$drained){throw 'Owned diagnostic child did not finish/drain within its budget.'}
  return $code
 }finally{$process.Dispose()}
}
try{
 $code=Invoke-Owned 'debug-before-build' (Get-Command pwsh).Source @('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'build.ps1'),'-Platform','x64','-Configuration','Debug') 600
 if($code -ne 0){throw 'Root diagnostic build failed.'}
 $buildPath=Join-Path $Root '.build/reports/performance-build/x64-Debug/build.json'
 $build=Get-Content -LiteralPath $buildPath -Raw|ConvertFrom-Json
 if($build.outcome -cne 'PASSED'){throw 'Root build has no successful attestation.'}
 Copy-Item -LiteralPath $buildPath -Destination (Join-Path $OutputDirectory 'debug-before-build.receipt.txt')
 Copy-Item -LiteralPath (Join-Path $Root '.build/reports/performance-build/x64-Debug/incremental.json') -Destination (Join-Path $OutputDirectory 'debug-before-incremental.receipt.txt')
 $executable=Join-Path $Root '.build/x64/Debug/DxUi.EmbeddedTests.exe'
 $hash=(Get-FileHash -LiteralPath $executable).Hash
 $code=Invoke-Owned 'debug-before-probe' $executable @('--test-rich-snapshot-slice-failures') 60
 @{classification='isolated-expected-noexcept-failure';executable=$executable;sha256=$hash;buildAttestationIdentity=$build.identity;exitCode=$code}|
  ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'debug-before-probe.receipt.txt')
 if((Get-FileHash -LiteralPath $executable).Hash -cne $hash){throw 'Diagnostic artifact changed during execution.'}
 if($code -ne 197 -or -not(Select-String -LiteralPath (Join-Path $OutputDirectory 'debug-before-probe.err') -SimpleMatch 'rich snapshot slice failure-point=0') -or
  -not(Select-String -LiteralPath (Join-Path $OutputDirectory 'debug-before-probe.err') -SimpleMatch 'rich preparation probe: unexpected noexcept termination')){throw 'Expected default-string termination was not observed; preserve and reassess the hypothesis.'}
 Write-Host 'Original Debug snapshot-slice noexcept failure reproduced at allocation point 0, bound to a successful root build and exact native artifact.'
}finally{
 @{scheduled=2;attempted=$results.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()}|
  ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'debug-before-completion.receipt.txt')
}
