[CmdletBinding()]param([string]$TestOutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Analyze-StateControls.ps1') -FunctionsOnly
$frames=@(@{window=0;frame=0;qpc=@(0L,10L,20L,30L,40L,50L,60L,70L,80L)},@{window=1;frame=0;qpc=@(90L,100L,110L,120L,130L,140L,150L,160L,170L)})
$cases=@(@(51L,59L,'Map'),@(49L,51L,'mixed-within-frame'),@(79L,91L,'outside-or-cross-frame'),@(-1L,1L,'outside-or-cross-frame'),
 @(50L,50L,'Map'),@(60L,60L,'Unmap'),@(81L,85L,'outside-or-cross-frame'),@(141L,149L,'Map'))
foreach($test in $cases){$result=Get-ContainedBoundary $frames $test[0] $test[1];if($result.kind -cne $test[2]){throw 'Boundary synthetic test failed.'}}
$reversed=$false;try{[void](Get-ContainedBoundary $frames 60L 50L)}catch [System.Exception]{$reversed=$true}
if(-not$reversed){throw 'Reverse interval accepted.'}
if((Get-NearestRank @(1.0,4.0,2.0,3.0) .5) -ne 2.0 -or (Get-NearestRank @(1.0,4.0,2.0,3.0) .95) -ne 4.0){throw 'Nearest-rank statistic failed.'}
# A FILETIME cannot safely pass through JavaScript Number; keep its exact Int64.
$creation=134051337123456789L
$roundTrip=@{creation=$creation}|ConvertTo-Json|ConvertFrom-Json -AsHashtable
if($roundTrip.creation -isnot [long] -or $roundTrip.creation -ne $creation){throw 'Int64 identity round trip failed.'}
New-Item -ItemType Directory -Path $TestOutputDirectory -Force|Out-Null
$fixture=Join-Path $TestOutputDirectory ('synthetic-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture|Out-Null
$fixtureCases=[Collections.Generic.List[object]]::new()
for($index=0;$index -lt 4;++$index){
 $stem='state-{0:d2}-{1}' -f ($index+1),$(if($index -in @(1,2)){'On'}else{'Off'})
 $report=$stem+'.native.receipt.txt';$path=Join-Path $fixture $report
 $native=@{mainThreadId=71;mainThreadCreation100ns=$creation;clock=@{ticksPerSecond=1000};windows=@(
  @{clear=$true;composite=$true;frames=@(@{qpc=$frames[0].qpc})},@{clear=$false;composite=$false;frames=@(@{qpc=$frames[1].qpc})})}
 $native|ConvertTo-Json -Depth 12|Set-Content $path
 [IO.File]::WriteAllBytes(($path+'.png'),[byte[]]@(1,2,3)) # Synthetic hash fixture, not an image claim.
 $samples=@();if($index -in @(1,2)){$samples=@(@{queryStartedQpc=51L;queryEndedQpc=59L;status='complete';errors=@();
   rows=@(@(71,$(if($index -eq 1){5}else{2}),$(if($index -eq 1){6}else{$null})),@(72,1,$null))})}
 @{samples=$samples}|ConvertTo-Json -Depth 12|Set-Content (Join-Path $fixture ($stem+'.states.receipt.txt'))
 $fixtureCases.Add(@{index=$index+1;observer=$(if($samples.Count){'On'}else{'Off'});nativeReport=$report;
  nativeSha256=(Get-FileHash $path).Hash;captureSha256=(Get-FileHash ($path+'.png')).Hash;identityAndNativeValidationPassed=$true;
  mainIdentityVerified=$true;child=@{samplingTruncated=$false}})
}
@{attempted=4;overallBudgetExceeded=$false;cases=$fixtureCases.ToArray()}|ConvertTo-Json -Depth 16|Set-Content (Join-Path $fixture 'completion.receipt.txt')
& (Join-Path $PSScriptRoot 'Analyze-StateControls.ps1') -OutputDirectory $fixture
$analysis=Get-Content (Join-Path $fixture 'analysis.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($analysis.cases.Count -ne 4 -or $analysis.cases[0].snapshotCount -ne 0 -or $analysis.cases[1].mapMainStateCounts['5'] -ne 1 -or
 $analysis.cases[1].mapMainWaitReasonCounts['6'] -ne 1 -or $analysis.cases[2].mapMainStateCounts['2'] -ne 1 -or
 $analysis.cases[1].mapSnapshotsWithReadyWorkers -ne 1 -or $analysis.cases[1].windows[0].frameP95Milliseconds -ne 80.0){throw 'Full synthetic analyzer replay differs.'}
$firstPath=Join-Path $fixture $fixtureCases[0].nativeReport
[IO.File]::AppendAllText($firstPath,' ')
$tamperRejected=$false
try{& (Join-Path $PSScriptRoot 'Analyze-StateControls.ps1') -OutputDirectory $fixture}catch [System.Exception]{
 if($_.Exception.Message -cne 'Native report hash mismatch.'){throw};$tamperRejected=$true
}
if(-not$tamperRejected){throw 'Altered raw input was accepted.'}
@{classification='Deliberately altered synthetic fixture; not native benchmark evidence';tamperRejected=$tamperRejected}|
 ConvertTo-Json|Set-Content (Join-Path $fixture 'negative-synthetic.receipt.txt')
@{classification='Functional analyzer tests only';containmentCases=$cases.Count;reversedIntervalRejected=$reversed;nearestRankCases=2;int64CreationRoundTrip=$true;
 analyzerSha256=(Get-FileHash (Join-Path $PSScriptRoot 'Analyze-StateControls.ps1')).Hash;testSha256=(Get-FileHash $PSCommandPath).Hash}|
 ConvertTo-Json|Set-Content (Join-Path $TestOutputDirectory 'state-analysis-tests.receipt.txt')
Write-Host 'Frame/Map containment, mixed/outside intervals, statistics and exact creation identity passed.'
