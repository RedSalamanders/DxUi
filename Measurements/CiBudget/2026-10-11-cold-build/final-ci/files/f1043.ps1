[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root='C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$replayPath=Join-Path $root 'paired-root-replay-run399-01/paired-replay.receipt.txt'
$diagPath=Join-Path $root 'paired-root-replay-run399-01/diagnostics.receipt.txt'
$agentPath=Join-Path $root 'paired-independent-run399-01/diagnostics.receipt.txt'
$r=Get-Content -LiteralPath $replayPath -Raw|ConvertFrom-Json -AsHashtable
$d=Get-Content -LiteralPath $diagPath -Raw|ConvertFrom-Json -AsHashtable
$a=Get-Content -LiteralPath $agentPath -Raw|ConvertFrom-Json -AsHashtable
if((ConvertTo-Json -InputObject $d.scenarios -Depth 40 -Compress) -cne (ConvertTo-Json -InputObject $a.scenarios -Depth 40 -Compress)){throw 'Independent/root descriptive numeric outputs differ.'}
$default=@($r.scenarios|Where-Object scenario -CEQ 'Default')[0]
$distinct=@($r.scenarios|Where-Object scenario -CEQ 'MultilineGridDistinct')[0]
$memory=@($default.migratedDecision.metrics|Where-Object{$_.phase -ceq 'dirty' -and $_.metric -ceq 'privatePeakBytes'})[0]
$compose=@($distinct.migratedDecision.metrics|Where-Object{$_.phase -ceq 'clean' -and $_.metric -ceq 'composeCpuP95Ms'})[0]
$memoryControl=@($default.controls|Where-Object{$_.beforeRun -ceq 'B17' -and $_.afterRun -ceq 'B18'})[0]
$composeControl=@($distinct.controls|Where-Object{$_.beforeRun -ceq 'B05' -and $_.afterRun -ceq 'B06'})[0]
$memoryChange=@($memoryControl.outsideBand|Where-Object{$_.scenario -ceq 'dirty' -and $_.metric -ceq 'privatePeakBytes'})[0]
$composeChange=@($composeControl.outsideBand|Where-Object{$_.scenario -ceq 'clean' -and $_.metric -ceq 'composeCpuP95Ms'})[0]
$positive=@($memory.blockEffects|Where-Object{$_ -gt 0}).Count
$count=0;$nonIntegral=0
foreach($scene in $d.scenarios){foreach($role in $scene.cleanPrepareTickSamples){$count+=$role.sampleCount;$nonIntegral+=$role.nonIntegralTickSamples}}
if($r.qualificationPassed -ne $false -or $d.qualificationPassed -ne $false -or $count -ne 28800 -or $nonIntegral -ne 0 -or $d.clock.tickNanoseconds -ne 100 -or $positive -ne 11 -or $memory.candidateMedian-$memory.baselineMedian -ne 92160 -or $memory.verdict -cne 'within-noise' -or $compose.verdict -cne 'within-noise'){throw 'Descriptive findings differ from reviewed evidence.'}
$inputs=[ordered]@{}
foreach($path in @($replayPath,$diagPath,$agentPath)){ $inputs[$path]=(Get-FileHash -LiteralPath $path).Hash }
$review=[ordered]@{
    status='ROOT_REPRODUCED_AND_REVIEWED_DESCRIPTIVE_FINDINGS_RETAINED_OPEN';qualificationPassed=$false;ciGateSucceeded=$false;
    runId=38172114803;head=$r.expectedHead;testedMerge=$r.testedMerge;testedTree=$r.testedTree;inputs=$inputs;
    quantization=[ordered]@{samples=$count;nonIntegralTickSamples=$nonIntegral;tickNanoseconds=100;legacyMedianBeforeMs=0.0006;legacyMedianAfterMs=0.0005;interpretation='Few-QPC-tick preparation warrants investigation. Different full statistical methods truthfully disagree; no speedup or noise waiver is inferred.'};
    defaultDirtyPrivatePeak=[ordered]@{pairedChangePercent=$memory.changePercent;positiveBlocks=$positive;blocks=12;holmP=$memory.adjustedPValue;medianDifferenceBytes=($memory.candidateMedian-$memory.baselineMedian);sameMetricControl=$memoryControl.name;controlDriftPercent=$memoryChange.changePercent;verdict=$memory.verdict;interpretation='Preserve the small directional memory trend for bounded attribution. It is below the declared2% band, its same metric control drifts and no confirmed regression verdict exists. Do not claim causal acceptance or dismiss it as noise.'};
    distinctCleanComposition=[ordered]@{pairedChangePercent=$compose.changePercent;p=$compose.pValue;holmP=$compose.adjustedPValue;candidateMaximumMs=$compose.candidateMaximum;rawWorseBand=$compose.rawRegressed;verdict=$compose.verdict;sameMetricControl=$composeControl.name;controlDriftPercent=$composeChange.changePercent;interpretation='Preserve B06 and all five native rounds; four elevated composition rounds and the raw worse-band effect remain an investigation target. Statistical/control qualification supplies no confirmed/held regression. Do not remove runs, rebaseline, attribute cause or waive the finding.'};
    nextQualificationWork='Retain previous Grid/FPS findings; investigate quantized preparation, B06 scheduling/composition and the Default memory trend with matched fixtures and stable controls before any policy or non-regression acceptance.';
    review='Root read and separately executed both agent drivers; complete replay reproduces raw judgments and all controls. Root compared complete descriptive scenario payloads, then independently inspected the metric arrays, native sample histograms, B06 rounds and exact B17/B18 and B05/B06 control rows. Agent conclusions were checked before archival.';
    completedUtc=[DateTimeOffset]::UtcNow;driver=$PSCommandPath;driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash
}
$path=Join-Path $root 'root-descriptive-performance-review-399.receipt.txt'
if(Test-Path -LiteralPath $path){throw 'Root descriptive review already exists.'}
$bytes=[Text.UTF8Encoding]::new($false).GetBytes(($review|ConvertTo-Json -Depth 20)+[char]10)
$stream=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
try{$stream.Write($bytes,0,$bytes.Length)}finally{$stream.Dispose()}
[ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path).Hash;status=$review.status}|ConvertTo-Json
