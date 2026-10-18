[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow
$names=@('TestRichDocumentLayoutEditsAndFailurePublication','TestRichDocumentLayoutLongHeightAndChunkBudget','TestRichDocumentLayoutWarpRasterOracle',
 'TestRichDocumentLayoutBranchesOptionsAndCompaction','TestRichTextEditorNativeChunkReuseAndRetirement','TestRichTextEditorRetainedPreparationFixedTrials')
New-Item -ItemType Directory -Path $OutputDirectory -Force|Out-Null
$buildFile=Join-Path $Root '.build/reports/performance-build/x64-Release/build.json'
$build=Get-Content -LiteralPath $buildFile -Raw|ConvertFrom-Json
if($build.outcome -cne 'PASSED'){throw 'Diagnostics require a successful root build.'}
$executable=Join-Path $Root '.build/x64/Release/DxUi.ControlTests.exe'
$sha=(Get-FileHash -LiteralPath $executable).Hash
Copy-Item -LiteralPath $buildFile -Destination (Join-Path $OutputDirectory 'root-build.receipt.txt')
@{startedUtc=$started.ToString('o');classification='isolated diagnostic children, not root suite qualification or a performance gate';
 scheduled=$names.Count;childBudgetSeconds=90;overallBudgetSeconds=600;buildAttestationIdentity=$build.identity;executableSha256=$sha;tests=$names}|
 ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
$operations=[Collections.Generic.List[object]]::new()
try{
 foreach($name in $names){
  if((Get-FileHash -LiteralPath $executable).Hash -cne $sha){throw 'Diagnostic artifact changed.'}
  $limit=[math]::Floor([math]::Min(90,600-([datetime]::UtcNow-$started).TotalSeconds))
  if($limit -le 0){throw 'Diagnostic overall budget exhausted.'}
  $arguments=@('--suite=EditorControls',('--test='+$name),'--test-timeout=80')
  $start=[Diagnostics.ProcessStartInfo]::new($executable)
  $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
  foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
  Write-Host "Diagnostic: $name"
  $childStarted=[datetime]::UtcNow
  $process=[Diagnostics.Process]::Start($start)
  try{
   $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
   $timedOut=-not$process.WaitForExit([int]($limit*1000))
   if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
   $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
   $output=if($drained){$stdout.GetAwaiter().GetResult()}else{''}
   if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($name+'.log')),$output);[IO.File]::WriteAllText((Join-Path $OutputDirectory ($name+'.err')),$stderr.GetAwaiter().GetResult())}
   $code=if($process.HasExited){$process.ExitCode}else{$null}
   $operations.Add(@{name=$name;arguments=$arguments;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained;
    elapsedSeconds=([datetime]::UtcNow-$childStarted).TotalSeconds;actualStartPresent=$output.Contains('[START] '+$name);actualDonePresent=$output.Contains('[DONE] '+$name)})
  }finally{$process.Dispose()}
 }
}finally{
 @{scheduled=$names.Count;attempted=$operations.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;operations=$operations.ToArray()}|
  ConvertTo-Json -Depth 10|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
Write-Host 'Diagnostic outcomes retained independently; a failed oracle is not replaced by another passing probe.'
