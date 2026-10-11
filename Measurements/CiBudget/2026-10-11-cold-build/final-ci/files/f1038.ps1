<# Descriptive raw-sample investigation after Audit-PairedRun399. No judgment or measurement. #>
[CmdletBinding()] param([Parameter(Mandatory)][string] $ReplayReceipt, [Parameter(Mandatory)][string] $OutputPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$OutputPath=[IO.Path]::GetFullPath($OutputPath)
$approved=[IO.Path]::GetFullPath('C:/Users/eric/.codex/tmp/dpf-s0-s2-merge')
if(-not $OutputPath.StartsWith($approved+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $OutputPath)){throw 'Use a new external derived file.'}
$utf8=[Text.UTF8Encoding]::new($false,$true)
function Require([bool]$Value,[string]$Message){if(-not $Value){throw $Message}}
function Digest([byte[]]$Bytes){[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes))}
$replayBytes=[IO.File]::ReadAllBytes($ReplayReceipt)
$replay=$utf8.GetString($replayBytes)|ConvertFrom-Json -AsHashtable
Require ($replay.evidenceReplayStatus -ceq 'REPRODUCED_ORIGINAL_BYTES_WITH_CHANGED_VERDICTS' -and $replay.qualificationPassed -eq $false) 'Require the retained independent full replay.'
$packet=[IO.Path]::GetFullPath([string]$replay.collection)
Require (-not $OutputPath.StartsWith($packet+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) 'Output must stay outside the original collection.'
$sealPath=Join-Path $packet 'SHA256SUMS';$sealHash=Digest ([IO.File]::ReadAllBytes($sealPath))
Require ($sealHash -ceq $replay.originalSealSha256) 'Original seal changed.'
$reports=Join-Path $packet 'artifacts/paired-benchmark-x64-Release/paired/20261011T202547Z-ec680e866a21/reports'
$summaryBytes=[IO.File]::ReadAllBytes((Join-Path $reports 'summary.json'))
Require ((Digest $summaryBytes) -ceq $replay.originalSummarySha256) 'Original summary changed.'
$summary=$utf8.GetString($summaryBytes)|ConvertFrom-Json -AsHashtable
$scenes=[Collections.Generic.List[object]]::new()
foreach($scene in $summary.scenarios){
    $records=[Collections.Generic.List[object]]::new();$phaseMemory=[Collections.Generic.List[object]]::new()
    $samples=@{baseline=[Collections.Generic.List[double]]::new();candidate=[Collections.Generic.List[double]]::new()}
    foreach($record in $scene.judgeComparison.sameReceiptsSha256){
        $path=Join-Path $reports "$($scene.scenario)-$($record.run).json"
        $bytes=[IO.File]::ReadAllBytes($path);Require ((Digest $bytes) -ceq $record.sha256) 'Original raw bytes changed.'
        $receipt=$utf8.GetString($bytes)|ConvertFrom-Json -AsHashtable
        $role=if($record.run.StartsWith('A',[StringComparison]::Ordinal)){'baseline'}else{'candidate'}
        $clean=@($receipt.scenarios|Where-Object {$_.name -ceq 'clean'})[0]
        foreach($round in $clean.rounds){foreach($value in $round.timingSamplesMs.prepare){$samples[$role].Add([double]$value)}}
        foreach($phase in $receipt.scenarios){
            $roundIndex=0
            foreach($round in $phase.rounds){
                $records.Add([ordered]@{file=[IO.Path]::GetFileName($path);run=$record.run;role=$role;phase=$phase.name;round=$roundIndex++;
                    fps=$round.fps;frameP95Ms=$round.frameP95Ms;prepareP95Ms=$round.prepareP95Ms;composeCpuP95Ms=$round.composeCpuP95Ms;
                    privateBytes=$round.privateBytes;privatePeakBytes=$round.privatePeakBytes;privateGrowthBytes=$round.privateGrowthBytes;
                    composeSamplesMs=$round.timingSamplesMs.composeCpu;frameSamplesMs=$round.timingSamplesMs.frame})
            }
        }
        foreach($stage in $receipt.memoryPhases){$phaseMemory.Add([ordered]@{role=$role;name=$stage.name;privateBytes=$stage.privateBytes;workingSetBytes=$stage.workingSetBytes})}
    }
    $quantization=@();foreach($role in @('baseline','candidate')){
        $histogram=@{};$nonIntegral=0;$ticksPerMillisecond=[double]$replay.roleIdentities[$role].clock.ticksPerSecond/1000.0
        foreach($value in $samples[$role]){$ticks=$value*$ticksPerMillisecond;$rounded=[Math]::Round($ticks);if([Math]::Abs($ticks-$rounded) -gt 1e-8){$nonIntegral++};$key=[long]$rounded;if(-not $histogram.ContainsKey($key)){$histogram[$key]=0};$histogram[$key]++}
        $quantization+=,[ordered]@{role=$role;sampleCount=$samples[$role].Count;nonIntegralTickSamples=$nonIntegral;histogram=@($histogram.Keys|Sort-Object|ForEach-Object{[ordered]@{ticks=$_;count=$histogram[$_]}});
            minimumMs=($samples[$role]|Measure-Object -Minimum).Minimum;maximumMs=($samples[$role]|Measure-Object -Maximum).Maximum}
    }
    $memory=@();foreach($role in @('baseline','candidate')){foreach($name in @('entry','device','scene','warm','capture','hidden')){
        $values=@($phaseMemory|Where-Object {$_.role -ceq $role -and $_.name -ceq $name})
        Require ($values.Count -eq 24) 'Memory phase inventory differs.'
        $memory+=,[ordered]@{role=$role;stage=$name;privateMinimum=($values.privateBytes|Measure-Object -Minimum).Minimum;privateMaximum=($values.privateBytes|Measure-Object -Maximum).Maximum;
            workingMinimum=($values.workingSetBytes|Measure-Object -Minimum).Minimum;workingMaximum=($values.workingSetBytes|Measure-Object -Maximum).Maximum}
    }}
    $scenes.Add([ordered]@{scenario=$scene.scenario;cleanPrepareTickSamples=$quantization;memoryPhaseRanges=$memory;
        largestCleanCompositionRounds=@($records|Where-Object {$_.phase -ceq 'clean'}|Sort-Object { $_.composeCpuP95Ms } -Descending|Select-Object -First 8);
        slowestCompletedRounds=@($records|Sort-Object { $_.fps }|Select-Object -First 4);
        b06Clean=@($records|Where-Object {$_.run -ceq 'B06' -and $_.phase -ceq 'clean'})})
}
Require ((Digest ([IO.File]::ReadAllBytes($sealPath))) -ceq $sealHash -and (Digest ([IO.File]::ReadAllBytes($ReplayReceipt))) -ceq (Digest $replayBytes)) 'Evidence changed during descriptive investigation.'
$result=[ordered]@{status='DESCRIPTIVE_INVESTIGATION_ONLY';qualificationPassed=$false;driverSha256=(Get-FileHash -LiteralPath $PSCommandPath).Hash;
    replayReceipt=$ReplayReceipt;replayReceiptSha256=(Digest $replayBytes);originalSealSha256=$sealHash;summarySha256=(Digest $summaryBytes);
    clock=$replay.roleIdentities.baseline.clock;scenarios=$scenes.ToArray();
    limits=@('Keep the raw B06 outlier and all controls; do not remove runs, change canonical metrics, or silently rebaseline.',
        'Few-QPC-tick preparation samples warrant quantization investigation, not a noise waiver or a speedup claim.',
        'Memory stage/range observations and signed changes do not identify heap ownership or prove a causal code regression.',
        'No thread placement, scheduler-delay, presented FPS, consumer adoption or independently qualified policy is established.')}
[IO.File]::WriteAllText($OutputPath,(($result|ConvertTo-Json -Depth 40)+[char]10),$utf8)
[ordered]@{output=$OutputPath;sha256=(Get-FileHash -LiteralPath $OutputPath).Hash;status=$result.status}|ConvertTo-Json
