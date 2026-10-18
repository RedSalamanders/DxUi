[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh CLI-refusal output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$exe=Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe';$hash=(Get-FileHash $exe).Hash
$cases=@(@{name='missing-timer';tail=@();message='Boundary diagnostic requires output'},
 @{name='unknown-timer';tail=@('Unknown');message='Unknown boundary diagnostic timer mode.'},
 @{name='unknown-mode';tail=@('On','SingleThreaded');message='Unknown boundary diagnostic WARP raster mode.'},
 @{name='extra-argument';tail=@('On','Default','Extra');message='Boundary diagnostic requires output'})
$started=[datetime]::UtcNow;$records=[Collections.Generic.List[object]]::new()
foreach($case in $cases){
 $path=Join-Path $OutputDirectory ($case.name+'.json');$start=[Diagnostics.ProcessStartInfo]::new($exe)
 $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in (@('--benchmark-complex-ui-boundaries',$path)+$case.tail)){$start.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($start)
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$timedOut=-not$process.WaitForExit(30000)
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if(-not$drained){throw 'CLI child output did not drain.'}
  [IO.File]::WriteAllText((Join-Path $OutputDirectory ($case.name+'.log')),$stdout.GetAwaiter().GetResult())
  [IO.File]::WriteAllText((Join-Path $OutputDirectory ($case.name+'.err')),$stderr.GetAwaiter().GetResult())
  $matched=$stderr.GetAwaiter().GetResult().Contains($case.message)
  $noOutput=-not(Test-Path -LiteralPath $path)-and-not(Test-Path -LiteralPath ($path+'.png'))
  $records.Add(@{name=$case.name;processId=$process.Id;arguments=$start.ArgumentList;exitCode=$process.ExitCode;timedOut=$timedOut;drained=$drained;
   expectedMessage=$matched;noNativeReportOrCapture=$noOutput})
  if($process.ExitCode -ne 1 -or $timedOut -or -not$matched -or -not$noOutput){throw 'Malformed native request was not refused before rendering/output.'}
 }finally{$process.Dispose()}
}
if((Get-FileHash $exe).Hash -cne $hash){throw 'CLI artifact changed.'}
@{classification='Owned native invalid-argument tests; no render, benchmark or scheduler acceptance';scheduled=4;attempted=$records.Count;childBudgetSeconds=30;
 elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;executableSha256=$hash;cases=$records.ToArray()}|
 ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'cli-refusal.receipt.txt')
Write-Host 'Four malformed native boundary requests refused with expected diagnostics and no output.'
