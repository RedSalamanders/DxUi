[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh cooperative candidate output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$started=[datetime]::UtcNow;$operations=[Collections.Generic.List[object]]::new();$built=$false
$paths=@(git -C $Root diff --name-only HEAD;git -C $Root ls-files --others --exclude-standard)|Sort-Object -Unique
$source=@(foreach($path in $paths){
 $from=Join-Path $Root $path;$to=Join-Path $OutputDirectory ('Source/'+$path)
 New-Item -ItemType Directory -Path (Split-Path $to) -Force|Out-Null
 Copy-Item -LiteralPath $from -Destination $to
 @{path=$path;sha256=(Get-FileHash $from).Hash}
})
@{classification='Cooperative-chunk candidate; affected Release only, not final qualification or accepted performance';
 source=$source;startedUtc=$started.ToString('o');overallBudgetSeconds=1200;buildChildBudgetSeconds=600;rootTestChildBudgetSeconds=480;
 perNativeTestBudgetSeconds=120}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
function Invoke-Owned([string]$Name,[string[]]$Arguments,[int]$Limit){
 $seconds=[int][math]::Floor([math]::Min($Limit,1200-([datetime]::UtcNow-$started).TotalSeconds))
 if($seconds -le 0){throw 'Cooperative candidate overall deadline reached.'}
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
  if($code -ne 0 -or $timedOut -or -not$drained){throw "Cooperative candidate operation failed: $Name ($code)"}
 }finally{$process.Dispose()}
}
try{
 Invoke-Owned 'release-build' @((Join-Path $Root 'build.ps1'),'-Platform','x64','-Configuration','Release') 600
 $built=$true
 Invoke-Owned 'release-affected' @((Join-Path $Root 'test.ps1'),'-Platform','x64','-Configuration','Release','-Suites','Embedded,EditorControls,Accessibility',
  '-SkipBuild','-SkipTooling','-TestTimeout','120') 480
 foreach($entry in $source){if((Get-FileHash (Join-Path $Root $entry.path)).Hash -cne $entry.sha256){throw 'Candidate input changed while running.'}}
}finally{
 foreach($scope in @('Embedded','EditorControls','Accessibility')){
  $file=Join-Path $Root ('.build/reports/'+$scope+'-x64-Release.json')
  if(Test-Path -LiteralPath $file){
   $receipt=Get-Content $file -Raw|ConvertFrom-Json -AsHashtable -Depth 32
   if([datetime]$receipt.completedUtc -ge $started){
    Copy-Item -LiteralPath $file -Destination (Join-Path $OutputDirectory ($scope+'.receipt.txt'))
    Copy-Item -LiteralPath (Join-Path $Root ('.build/logs/test-'+$scope+'-x64-Release.log')) -Destination (Join-Path $OutputDirectory ($scope+'.log'))
    Copy-Item -LiteralPath $receipt.performanceReport -Destination (Join-Path $OutputDirectory ([IO.Path]::GetFileName($receipt.performanceReport)))
   }
  }
 }
 foreach($file in Get-ChildItem (Join-Path $Root '.build/reports/performance-build/x64-Release') -File){Copy-Item -LiteralPath $file.FullName -Destination $OutputDirectory}
 foreach($file in Get-ChildItem (Join-Path $Root '.build/logs') -File|Where-Object {$_.Name -like 'build-x64-Release-*' -and $_.LastWriteTimeUtc -ge $started}){Copy-Item -LiteralPath $file.FullName -Destination $OutputDirectory}
 if(Test-Path -LiteralPath (Join-Path $Root '.build/reports/rich-document-raster')){Copy-Item -LiteralPath (Join-Path $Root '.build/reports/rich-document-raster') -Destination (Join-Path $OutputDirectory 'Raster') -Recurse}
 if($built){
  $artifacts=Join-Path $OutputDirectory 'ExactArtifacts';New-Item -ItemType Directory -Path $artifacts|Out-Null
  foreach($stem in @('DxUi.ControlTests','DxUi.EmbeddedTests')){foreach($extension in @('.exe','.pdb')){
   $file=Join-Path $Root ('.build/x64/Release/'+$stem+$extension)
   Copy-Item -LiteralPath $file -Destination $artifacts
  }}
 }
 @{scheduled=2;attempted=$operations.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;operations=$operations.ToArray()}|
  ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
Write-Host 'Cooperative candidate affected Release results retained; final qualification remains separate.'