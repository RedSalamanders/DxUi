[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$started=[datetime]::UtcNow;$records=[Collections.Generic.List[object]]::new()
foreach($case in @(@{leaf='2026-10-18-positioned-lines';target='Native/Embedded-x64-Release.receipt.txt'},
 @{leaf='2026-10-18-positioned-lines';target='Source/src/Controls/DxUi.RichTextLayout.cpp'})){
 $original=Join-Path $Root ('Measurements/RichText/'+$case.leaf)
 $copy=Join-Path $Root ('.build/reports/rich-line-origins-20261018/tamper-'+[guid]::NewGuid().ToString('N'))
 Copy-Item -LiteralPath $original -Destination $copy -Recurse
 $target=Join-Path $copy $case.target;$originalHash=(Get-FileHash -LiteralPath (Join-Path $original $case.target)).Hash
 $stream=[IO.File]::Open($target,[IO.FileMode]::Append,[IO.FileAccess]::Write)
 try{$stream.WriteByte(0x20)}finally{$stream.Dispose()}
 $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
 $start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true;$start.WorkingDirectory=$Root
 foreach($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $copy 'Verify.ps1'))){$start.ArgumentList.Add($argument)}
 $process=[Diagnostics.Process]::Start($start)
 try{
  $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
  $timedOut=-not$process.WaitForExit(120000)
  if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
  if(-not$drained){throw 'Tamper child output did not drain.'}
  [IO.File]::WriteAllText(($copy+'.log'),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText(($copy+'.err'),$stderr.GetAwaiter().GetResult())
  $expected='SHA-256 differs: '+$case.target
  $matched=$stderr.GetAwaiter().GetResult().Contains($expected)
  $unchanged=(Get-FileHash -LiteralPath (Join-Path $original $case.target)).Hash -ceq $originalHash
  $records.Add(@{packet=$case.leaf;target=$case.target;originalSha256=$originalHash;tamperedSha256=(Get-FileHash $target).Hash;
    copiedDirectory=$copy;nativeChildPid=$process.Id;exitCode=$process.ExitCode;timedOut=$timedOut;drained=$drained;expectedHashFailure=$matched;originalUnchanged=$unchanged})
  if($timedOut -or $process.ExitCode -eq 0 -or -not$matched -or -not$unchanged){throw 'One-byte tamper was not specifically rejected.'}
 }finally{$process.Dispose()}
}
@{classification='Negative integrity tests on disposable preserved copies; actual packets unchanged';scheduled=2;attempted=$records.Count;
 childBudgetSeconds=120;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds;cases=$records.ToArray();scriptSha256=(Get-FileHash $PSCommandPath).Hash}|
 ConvertTo-Json -Depth 10|Set-Content (Join-Path $Root '.build/reports/rich-line-origins-20261018/packet-tamper-tests.receipt.txt')
Write-Host 'Both one-byte altered copies rejected at the expected hash; original packets unchanged.'
