[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh original-probe output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$started=[datetime]::UtcNow;$results=[Collections.Generic.List[object]]::new()
$files=@(git -C $Root diff --name-only;git -C $Root ls-files --others --exclude-standard)|Sort-Object -Unique
$source=@(foreach($relative in $files){$path=Join-Path $Root $relative;$target=Join-Path $OutputDirectory ('Source/'+$relative);New-Item -ItemType Directory -Path (Split-Path $target) -Force|Out-Null;Copy-Item -LiteralPath $path -Destination $target;@{path=$relative;sha256=(Get-FileHash $path).Hash}})
@{classification='Original flat Project allocation diagnostic; passing original sweep must be retained and cannot be called a reproduced defect';baseCommit=(git -C $Root rev-parse HEAD);
 sourceFiles=$source;scheduled=2;overallBudgetSeconds=900;buildBudgetSeconds=600;nativeBudgetSeconds=180;allowedDiagnosticExits=@(0,197);ordinarySuiteQualification=$false}|
 ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
function Invoke-Owned([string]$Name,[string]$Executable,[string[]]$CommandArguments,[int]$Budget){
 $remaining=[int][math]::Floor([math]::Min($Budget,900-([datetime]::UtcNow-$started).TotalSeconds))
 if($remaining -le 0){throw 'Original probe deadline reached.'}
 $start=[Diagnostics.ProcessStartInfo]::new($Executable);$start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
 $start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in $CommandArguments){$start.ArgumentList.Add($argument)}
 Write-Host "Original diagnostic: $Name"
 $process=[Diagnostics.Process]::Start($start)
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$timedOut=-not$process.WaitForExit($remaining*1000)
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.log')),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory ($Name+'.err')),$stderr.GetAwaiter().GetResult())}
  $code=if($process.HasExited){$process.ExitCode}else{$null}
  $results.Add(@{name=$Name;processId=$process.Id;exitCode=$code;timedOut=$timedOut;drained=$drained;arguments=$CommandArguments})
  if($timedOut -or -not$drained){throw 'Original diagnostic did not complete and drain within budget.'}
  return $code
 }finally{$process.Dispose()}
}
try{
 $code=Invoke-Owned 'build' (Get-Command pwsh).Source @('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'build.ps1'),'-Platform','x64','-Configuration','Debug') 600
 if($code -ne 0){throw 'Original diagnostic build failed.'}
 $buildPath=Join-Path $Root '.build/reports/performance-build/x64-Debug/build.json';$build=Get-Content $buildPath -Raw|ConvertFrom-Json
 if($build.outcome -cne 'PASSED'){throw 'Original probe lacks a successful root build.'}
 Copy-Item -LiteralPath $buildPath -Destination (Join-Path $OutputDirectory 'build.receipt.txt')
 Copy-Item -LiteralPath (Join-Path (Split-Path $buildPath) 'incremental.json') -Destination (Join-Path $OutputDirectory 'incremental.receipt.txt')
 $exe=Join-Path $Root '.build/x64/Debug/DxUi.EmbeddedTests.exe';$hash=(Get-FileHash $exe).Hash
 $code=Invoke-Owned 'flat-projection' $exe @('--test-rich-flat-projection') 180
 if((Get-FileHash $exe).Hash -cne $hash){throw 'Original probe artifact changed.'}
 $errorText=Get-Content (Join-Path $OutputDirectory 'flat-projection.err') -Raw
 $log=Get-Content (Join-Path $OutputDirectory 'flat-projection.log') -Raw
 if($code -eq 0){
  if($log -notmatch 'rich flat projection allocation-failure sweptRequests=([1-9][0-9]*)'){throw 'Passing original sweep has no schedule.'}
  $classification='original-flat-helper-passed-no-defect-reproduced'
 }elseif($code -eq 197){
  if(-not$errorText.Contains('rich flat projection failure-point=') -or -not$errorText.Contains('rich preparation probe: unexpected noexcept termination')){throw 'Unexpected termination has no original flat-probe marker.'}
  $classification='original-flat-helper-noexcept-termination-reproduced'
 }else{throw "Unexpected original diagnostic outcome $code; preserve and investigate."}
 @{classification=$classification;exitCode=$code;executableSha256=$hash;buildAttestationIdentity=$build.identity;librarySourceUnchanged=$true;
  sourceFingerprint='4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C'}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'flat-projection.receipt.txt')
 Write-Host $classification
}finally{
 @{scheduled=2;attempted=$results.Count;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;results=$results.ToArray()}|
 ConvertTo-Json -Depth 12|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
}
