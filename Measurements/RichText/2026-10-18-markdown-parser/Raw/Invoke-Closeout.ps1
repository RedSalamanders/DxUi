[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-markdown-parser-20261018'
$packet=Join-Path $Root 'Measurements/RichText/2026-10-18-markdown-parser'
$source='EFBADBD8239B1918D1C31517B6EF2BD3B6B73D51F53AA1B39518F2C1372A5743'
$started=[datetime]::UtcNow
$operations=[Collections.Generic.List[object]]::new()
function Invoke-Owned([string]$Name,[string]$Script,[string[]]$Options,[int]$Limit=180){
 Write-Host "Closeout: $Name"
 $start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
 $start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
 foreach($argument in @('-NoProfile','-NonInteractive','-File',$Script)+$Options){$start.ArgumentList.Add($argument)}
 $child=[Diagnostics.Process]::Start($start)
 try{
  $out=$child.StandardOutput.ReadToEndAsync();$err=$child.StandardError.ReadToEndAsync();$childStarted=[datetime]::UtcNow
  $timedOut=-not$child.WaitForExit($Limit*1000)
  if($timedOut){$child.Kill($true);[void]$child.WaitForExit(5000)}
  $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($out,$err),5000)
  if($drained){[IO.File]::WriteAllText((Join-Path $work ($Name+'.log')),$out.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $work ($Name+'.err')),$err.GetAwaiter().GetResult())}
  $code=if($child.HasExited){$child.ExitCode}else{$null}
  $operations.Add(@{name=$Name;exitCode=$code;timedOut=$timedOut;drained=$drained;childProcessId=$child.Id;elapsedSeconds=([datetime]::UtcNow-$childStarted).TotalSeconds})
  if($code -ne 0 -or $timedOut -or -not$drained){throw "Closeout operation failed: $Name ($code)"}
 }finally{$child.Dispose()}
}
function Seal-Packet {
 $lines=@(Get-ChildItem -LiteralPath $packet -File -Recurse|Where-Object Name -CNE 'SHA256SUMS'|Sort-Object FullName|ForEach-Object {
  (Get-FileHash -LiteralPath $_.FullName).Hash+'  '+[IO.Path]::GetRelativePath($packet,$_.FullName).Replace('\','/')
 })
 [IO.File]::WriteAllLines((Join-Path $packet 'SHA256SUMS'),$lines,[Text.UTF8Encoding]::new($false))
}
try{
 Invoke-Owned 'closeout-docs' (Join-Path $work 'Finalize-Docs.ps1') @('-Root',$Root)
 Invoke-Owned 'closeout-archive' (Join-Path $work 'Archive.ps1') @('-Root',$Root,'-Packet',$packet,'-ExpectedSource',$source) 300
 Invoke-Owned 'closeout-readme' (Join-Path $work 'Write-PacketReadme.ps1') @('-Root',$Root,'-Packet',$packet)
 Seal-Packet
 Invoke-Owned 'verify-first' (Join-Path $packet 'Verify.ps1') @() 180
 Invoke-Owned 'tamper-tests' (Join-Path $work 'Test-PacketTamper.ps1') @('-Root',$Root) 300
 $review=Join-Path $packet 'Review';New-Item -ItemType Directory -Path $review -Force|Out-Null
 Copy-Item -LiteralPath (Join-Path $work 'packet-tamper-tests.receipt.txt') -Destination $review
 Copy-Item -LiteralPath (Join-Path $work 'x64-replay.receipt.txt') -Destination $review
 Copy-Item -LiteralPath (Join-Path $work 'verify-first.log') -Destination $review
 Copy-Item -LiteralPath (Join-Path $work 'verify-first.err') -Destination $review
 foreach($file in Get-ChildItem (Join-Path $Root '.build/reports/rich-markdown-integrity-20261018') -File){Copy-Item -LiteralPath $file.FullName -Destination $review}
 Invoke-Owned 'document-checks' (Join-Path $work 'Invoke-DocumentChecks.ps1') @('-Root',$Root) 240
 foreach($file in Get-ChildItem $work -File|Where-Object {$_.Name -like 'document-*' -or $_.Name -ceq 'prose-closeout.receipt.txt'}){Copy-Item -LiteralPath $file.FullName -Destination $review}
 Seal-Packet
 Invoke-Owned 'verify-final' (Join-Path $packet 'Verify.ps1') @() 180
}finally{
 @{classification='Prose and curated evidence closeout; no compiled inputs edited';startedUtc=$started.ToString('o');completedUtc=[datetime]::UtcNow.ToString('o');
  operations=$operations.ToArray();elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds}|
  ConvertTo-Json -Depth 12|Set-Content (Join-Path $work 'closeout-completion.receipt.txt')
}
Write-Host 'Packet/docs/integrity closeout passed; staged Git replay and local commit remain.'
