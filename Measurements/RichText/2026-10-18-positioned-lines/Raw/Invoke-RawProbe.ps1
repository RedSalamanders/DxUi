[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh raw-origin probe output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$started=[datetime]::UtcNow;$operations=[Collections.Generic.List[object]]::new()
$method='TestRichDocumentLayoutRawLineOriginDiagnostics'
$source='Tests/Controls/Scope.Tests.RichDocumentLayout.cpp'
$sourceHash=(Get-FileHash (Join-Path $Root $source)).Hash
Copy-Item -LiteralPath (Join-Path $Root $source) -Destination (Join-Path $OutputDirectory 'compiled-source.cpp')
@{classification='Independent raw DirectWrite investigation, not a renderer fix, full qualification or performance acceptance';
 startedUtc=$started.ToString('o');baseCommit=(& git -C $Root rev-parse HEAD).Trim();sourcePath=$source;sourceSha256=$sourceHash;
 method=$method;overallBudgetSeconds=900;buildChildBudgetSeconds=600;rootTestChildBudgetSeconds=240;nativeTestBudgetSeconds=120}|
 ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
function Invoke-Owned([string]$Name,[string[]]$Arguments,[int]$Limit){
 $seconds=[int][math]::Floor([math]::Min($Limit,900-([datetime]::UtcNow-$started).TotalSeconds))
 if($seconds -le 0){throw 'Raw probe overall deadline reached.'}
 $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
 $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in @('-NoProfile','-NonInteractive','-File')+$Arguments){$start.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($start);$stepStarted=[datetime]::UtcNow
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$timedOut=-not$process.WaitForExit($seconds*1000)
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.err')),$stderr.GetAwaiter().GetResult())}
  $code=if($process.HasExited){$process.ExitCode}else{$null}
  $operations.Add(@{name=$Name;processId=$process.Id;arguments=$start.ArgumentList;exitCode=$code;timedOut=$timedOut;drained=$drained;elapsedSeconds=([datetime]::UtcNow-$stepStarted).TotalSeconds})
  if($code -ne 0 -or $timedOut -or -not$drained){throw "Raw probe operation failed: $Name ($code)"}
 }finally{$process.Dispose()}
}
try{
 Invoke-Owned 'release-build' @((Join-Path $Root 'build.ps1'),'-Platform','x64','-Configuration','Release') 600
 Invoke-Owned 'release-probe' @((Join-Path $Root 'test.ps1'),'-Platform','x64','-Configuration','Release','-Suites','EditorControls',
  '-Tests',$method,'-SkipBuild','-SkipTooling','-TestTimeout','120') 240
 $receiptPath=Join-Path $Root '.build/reports/EditorControls-x64-Release.filtered.json'
 $native=Get-Content $receiptPath -Raw|ConvertFrom-Json -AsHashtable -Depth 32
 if($native.exitCode -ne 0 -or $native.tests.Count -ne 1 -or $native.tests[0] -cne $method -or $native.skips.Count -or
  [datetime]$native.completedUtc -lt $started -or (Get-FileHash $native.executable).Hash -cne $native.sha256){throw 'Raw probe filtered identity differs.'}
 Copy-Item -LiteralPath $receiptPath -Destination (Join-Path $OutputDirectory 'native.receipt.txt')
 Copy-Item -LiteralPath (Join-Path $Root '.build/logs/test-EditorControls-x64-Release.filtered.log') -Destination (Join-Path $OutputDirectory 'native.log')
 Copy-Item -LiteralPath $native.performanceReport -Destination (Join-Path $OutputDirectory 'canonical-diagnostic.receipt.txt')
 $artifacts=Join-Path $OutputDirectory 'ExactArtifacts';New-Item -ItemType Directory -Path $artifacts|Out-Null
 foreach($path in @($native.executable,[IO.Path]::ChangeExtension($native.executable,'.pdb'))){Copy-Item -LiteralPath $path -Destination $artifacts}
 if((Get-FileHash (Join-Path $Root $source)).Hash -cne $sourceHash){throw 'Raw probe source changed while running.'}
}finally{
 $build=Join-Path $Root '.build/reports/performance-build/x64-Release'
 foreach($file in Get-ChildItem -LiteralPath $build -File){Copy-Item -LiteralPath $file.FullName -Destination $OutputDirectory}
 foreach($log in Get-ChildItem (Join-Path $Root '.build/logs') -File|Where-Object {$_.Name -like 'build-x64-Release-*' -and $_.LastWriteTimeUtc -ge $started}){
  Copy-Item -LiteralPath $log.FullName -Destination $OutputDirectory
 }
 @{scheduled=2;attempted=$operations.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;operations=$operations.ToArray()}|
  ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
Write-Host 'Independent DirectWrite raw-origin observations retained with exact source/artifact and filtered native receipt.'
