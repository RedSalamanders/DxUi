[CmdletBinding()] param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow
$arguments=@('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'test.ps1'),'-Configuration','Release','-Platform','x64','-SkipTooling',
 '-Suites','EditorControls,Embedded,Accessibility')
@{startedUtc=$started.ToString('o');scheduled=1;overallBudgetSeconds=900;ownedChildBudgetSeconds=900;arguments=$arguments;
 expectedExit=0;expectedPredicate='all fixed retained trials pass';
 notes='First retained geometry candidate. Three affected native scopes, exact source frozen; original failures retained. No performance acceptance.'}|
 ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'before-plan.receipt.txt')
$start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
$start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
$process=[Diagnostics.Process]::Start($start)
try{
 $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
 $timedOut=-not $process.WaitForExit(900000)
 if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
 $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
 if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory 'before-runner.log'),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory 'before-runner.err'),$stderr.GetAwaiter().GetResult())}
 $code=if($process.HasExited){$process.ExitCode}else{$null}
 @{scheduled=1;attempted=1;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained}|
  ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'before-completion.receipt.txt')
 if($timedOut -or -not$drained -or $code -ne 0){throw 'Retained baseline did not pass.'}
 $receipt=Join-Path $Root '.build/reports/EditorControls-x64-Release.json'
 $log=Join-Path $Root '.build/logs/test-EditorControls-x64-Release.log'
 Copy-Item -LiteralPath $receipt -Destination (Join-Path $OutputDirectory 'before-native.receipt.txt')
 Copy-Item -LiteralPath $log -Destination (Join-Path $OutputDirectory 'before-native.log')

 Write-Host 'Fixed real retained-control trials preserved.'
}finally{$process.Dispose()}


