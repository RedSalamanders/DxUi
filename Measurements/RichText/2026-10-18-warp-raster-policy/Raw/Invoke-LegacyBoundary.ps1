[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh legacy-boundary output required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
Import-Module (Join-Path $Root 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $Root 'Tools/PerformancePolicy.psm1') -Force
$anchor=Get-ProfilingMeasurementState -Root $Root -Platform x64 -Configuration Release
Assert-PerformanceBuildAttestation -Root $Root -Platform x64 -Configuration Release -State $anchor
$path=Join-Path $OutputDirectory 'legacy.json';$started=[datetime]::UtcNow
$arguments=@('-NoProfile','-NonInteractive','-File',(Join-Path $Root 'performance-profile.ps1'),'-Configuration','Release','-Platform','x64',
 '-SkipBuild','-BoundaryTimers','on','-OutputPath',$path)
@{classification='Legacy schema1 diagnostic dispatch smoke; no policy argument and no acceptance claim';anchor=$anchor;
 arguments=$arguments;wrapperBudgetSeconds=180;nativeChildBudgetSeconds=120;startedUtc=$started.ToString('o');
 sourceSha256=(Get-FileHash $PSCommandPath).Hash}|ConvertTo-Json -Depth 32|Set-Content (Join-Path $OutputDirectory 'plan.receipt.txt')
$start=[Diagnostics.ProcessStartInfo]::new((Get-Command pwsh).Source)
$start.WorkingDirectory=$Root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true;$start.RedirectStandardError=$true
foreach($argument in $arguments){$start.ArgumentList.Add($argument)}
$process=[Diagnostics.Process]::Start($start)
try{
 $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync();$timedOut=-not$process.WaitForExit(180000)
 if($timedOut){$process.Kill($true);[void]$process.WaitForExit(5000)}
 $drained=[Threading.Tasks.Task]::WaitAll([Threading.Tasks.Task[]]@($stdout,$stderr),5000)
 if($drained){[IO.File]::WriteAllText((Join-Path $OutputDirectory 'runner.log'),$stdout.GetAwaiter().GetResult());[IO.File]::WriteAllText((Join-Path $OutputDirectory 'runner.err'),$stderr.GetAwaiter().GetResult())}
 $code=if($process.HasExited){$process.ExitCode}else{$null}
 @{exitCode=$code;processId=$process.Id;timedOut=$timedOut;drained=$drained;elapsedSeconds=([datetime]::UtcNow-$started).TotalSeconds}|
  ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'completion.receipt.txt')
 if($code -ne 0 -or $timedOut -or -not$drained){throw 'Legacy boundary wrapper failed.'}
}finally{$process.Dispose()}
$report=Get-Content $path -Raw|ConvertFrom-Json -AsHashtable -Depth 32
$child=Get-Content ($path+'.child.json') -Raw|ConvertFrom-Json -AsHashtable -Depth 16
[void](Assert-BoundaryReport -Receipt $report -Timers $true -Configuration Release -ProcessId $child.processId)
$observed=[ordered]@{};foreach($key in $anchor.Keys){$observed[$key]=$report[$key]}
Assert-PerformanceMeasurementState -Before $anchor -After $observed
if((Get-FileHash ($path+'.png')).Hash -cne '6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD' -or
 (Get-FileHash ($path+'.raw.json')).Hash -cne $report.rawReportSha256){throw 'Legacy scene/native bytes differ.'}
@{schema=$report.schema;nativePid=$child.processId;nativeExitCode=$child.exitCode;nativeArguments=$child.arguments;
 sourceFingerprint=$report.sourceFingerprint;buildAttestationIdentity=$report.buildAttestationIdentity;nativeExecutableSha256=$report.executableSha256;
 captureSha256=$report.captureSha256;rawSha256=$report.rawReportSha256}|ConvertTo-Json -Depth 16|Set-Content (Join-Path $OutputDirectory 'legacy-binding.receipt.txt')
Write-Host 'Absent policy preserves native schema1, three arguments, source identity, full frame invariants and the populated capture.'
