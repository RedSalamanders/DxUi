# Derive descriptive statistics from this retained A/A packet. No native program is built or executed.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$packet = $PSScriptRoot
$repository = [IO.Path]::GetFullPath((Join-Path $packet '../../..'))
$summary = [IO.File]::ReadAllText((Join-Path $packet 'summary.receipt.txt')) | ConvertFrom-Json -AsHashtable
$judgeSource = @(& git -C $repository show "$($summary.baseline.commit):Tools/PerformanceComparison.psm1")
if ($LASTEXITCODE -ne 0) { throw 'The declared frozen judge source cannot be recovered.' }
$judge = New-Module -ScriptBlock ([scriptblock]::Create(($judgeSource -join "`n") + "`n"))
$metricNames = @(& $judge { Get-PerformanceMetricNames })
$stageNames = @('entry','device','scene','warm','capture','hidden')
foreach ($path in @('Tools/PerformancePolicy.psm1','Tools/ScopedTesting.psm1','Tools/BenchmarkGate.psm1')) {
    if ((Get-FileHash -LiteralPath (Join-Path $repository $path) -Algorithm SHA256).Hash -cne $summary.harness[$path]) {
        throw "Declared analysis helper bytes differ: $path"
    }
}
Import-Module (Join-Path $repository 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $repository 'Tools/ScopedTesting.psm1') -Force
$gate = Import-Module (Join-Path $repository 'Tools/BenchmarkGate.psm1') -Force -PassThru
$buildReceiptPath = Join-Path $packet 'baseline-build.receipt.txt'
$buildReceipt = [IO.File]::ReadAllText($buildReceiptPath) | ConvertFrom-Json -AsHashtable
if (-not (Test-ScopedReceipt -Path $buildReceiptPath -Identity $buildReceipt.identity)) { throw 'The retained build receipt envelope is invalid.' }
if ($summary.studyPurpose -cne 'aa-calibration' -or $summary.harnessDirty -or $summary.blocks -ne 12 -or $summary.seed -ne 20261012) {
    throw 'The retained study purpose, clean frozen harness or declared schedule has changed.'
}

function Median([double[]] $Values) {
    if ($Values.Count -eq 0) { throw 'A descriptive statistic requires observations.' }
    $ordered = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($ordered.Count / 2.0)
    if ($ordered.Count % 2) { return [double]$ordered[$middle] }
    return ([double]$ordered[$middle - 1] + [double]$ordered[$middle]) / 2.0
}

function Stats([double[]] $Values) {
    $minimum = [double](($Values | Measure-Object -Minimum).Minimum)
    $maximum = [double](($Values | Measure-Object -Maximum).Maximum)
    $median = Median $Values
    return [ordered]@{n=$Values.Count;median=$median;minimum=$minimum;maximum=$maximum;span=$maximum-$minimum;
        spanPercentOfMedian=$(if ($median -ne 0) { 100.0 * ($maximum-$minimum) / [Math]::Abs($median) } else { $null })}
}

function RoleStats([System.Collections.IDictionary] $ValuesByRun) {
    $a = @($summary.steps | Where-Object { $_.Side -ceq 'baseline' } | ForEach-Object { [double]$ValuesByRun[$_.Name] })
    $b = @($summary.steps | Where-Object { $_.Side -ceq 'candidate' } | ForEach-Object { [double]$ValuesByRun[$_.Name] })
    $blocks = @(foreach ($block in $summary.blockSchedule) {
        $aMedian = Median ([double[]]@($block.BaselineRuns | ForEach-Object { $ValuesByRun[$_] }))
        $bMedian = Median ([double[]]@($block.CandidateRuns | ForEach-Object { $ValuesByRun[$_] }))
        [ordered]@{block=$block.Name;order=$block.Order;aMedian=$aMedian;bMedian=$bMedian;bMinusA=$bMedian-$aMedian;
            bVsAPercent=$(if ($aMedian -ne 0) { 100.0 * ($bMedian/$aMedian-1.0) } else { $null })}
    })
    $aCenter = Median ([double[]]$a); $bCenter = Median ([double[]]$b)
    $byOrder = @(foreach ($order in @('ABBA','BAAB')) {
        $observations = @($blocks | Where-Object { $_.order -ceq $order })
        [ordered]@{order=$order;blocks=$observations.Count;bMinusA=(Stats ([double[]]@($observations | ForEach-Object { $_.bMinusA })));
            bVsAPercent=$(if (@($observations | Where-Object { $null -ne $_.bVsAPercent }).Count) {
                Stats ([double[]]@($observations | Where-Object { $null -ne $_.bVsAPercent } | ForEach-Object { $_.bVsAPercent })) } else { $null })}
    })
    return [ordered]@{all=(Stats ([double[]]@($ValuesByRun.Values)));a=(Stats ([double[]]$a));b=(Stats ([double[]]$b));
        bMinusAMedian=$bCenter-$aCenter;bVsAMedianPercent=$(if ($aCenter -ne 0) { 100.0*($bCenter/$aCenter-1.0) } else { $null });
        pairedBMinusA=(Stats ([double[]]@($blocks | ForEach-Object { $_.bMinusA })));byOrder=$byOrder;blocks=$blocks}
}

$receipts = [Collections.Generic.List[object]]::new()
$scenes = [Collections.Generic.List[object]]::new()
$controlChecks = 0
$controlCells = 0
$driftedObservations = 0
$driftedMetricCells = 0
foreach ($scenario in $summary.scenarios) {
    $byRun = [ordered]@{}
    $rawFiles = @()
    foreach ($step in $summary.steps) {
        $file = "$($scenario.scenario)-$($step.Name).json"
        $path = Join-Path $packet $file
        $receipt = [IO.File]::ReadAllText($path) | ConvertFrom-Json -AsHashtable
        & $judge { param($Receipt) Assert-PerformanceReceipt $Receipt } $receipt
        $identity = Test-PerformanceReceiptIdentity -Receipt $receipt
        if (-not $identity.verifiable) { throw "Missing v2 receipt identity: $file ($($identity.missing -join ', '))" }
        if ($receipt.buildAttestationIdentity -cne $buildReceipt.identity) { throw "Build receipt binding mismatch: $file" }
        $attestation = @($scenario.judgeComparison.sameReceiptsSha256 | Where-Object { $_.run -ceq $step.Name })
        if ($attestation.Count -ne 1 -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $attestation[0].sha256) { throw "Receipt attestation mismatch: $file" }
        if (($receipt.memoryPhases.name -join ',') -cne ($stageNames -join ',')) { throw "Missing/changed stage inventory: $file" }
        $byRun[$step.Name] = $receipt
        $receipts.Add($receipt)
        $rawFiles += $file
    }
    $controlFiles = @($scenario.comparisons | Where-Object { $_.name.EndsWith('-control', [StringComparison]::Ordinal) })
    $controls = @(foreach ($record in $controlFiles) {
        $comparison = [IO.File]::ReadAllText((Join-Path $packet $record.file)) | ConvertFrom-Json -AsHashtable
        if (@($comparison.changes).Count -ne 26) { throw "Incomplete control comparison: $($record.file)" }
        $drift = @($comparison.changes | Where-Object {
            if ($null -eq $_.changePercent) { $_.before -ne $_.after }
            else { [Math]::Abs([double]$_.changePercent) -gt [double]$_.noisePercent }
        })
        $derivedStatus = if ($drift.Count) { 'unstable-control' } else { 'stable-control' }
        if ($derivedStatus -cne $record.status) { throw "Control status differs from its complete comparison: $($record.file)" }
        [ordered]@{name=$record.name;retainedStatus=$record.status;file=$record.file;comparison=$comparison}
    })
    foreach ($block in $summary.blockSchedule) {
        foreach ($side in @('Baseline','Candidate')) {
            $runNames = $block["${side}Runs"]
            $controlName = "$($scenario.scenario)-$($block.Name)-$($side.ToLowerInvariant())-control"
            $control = @($controls | Where-Object { $_.name -ceq $controlName })
            if ($control.Count -ne 1) { throw "Missing/duplicate control: $controlName" }
            $canonical = & $judge { param($Before,$After) Compare-PerformanceReceipt -Before $Before -After $After } $byRun[$runNames[0]] $byRun[$runNames[1]]
            $equivalent = & $gate { param($Retained,$Canonical) Test-EquivalentGateDecision $Retained $Canonical } $control[0].comparison $canonical
            if (-not $equivalent) { throw "The complete control differs from frozen raw replay: $controlName" }
            # Subsequent drift statistics consume the canonical percentage and unchanged bands, not saved labels.
            $control[0].comparison = $canonical
            foreach ($change in $control[0].comparison.changes) {
                $before = Median ([double[]]@(@($byRun[$runNames[0]].scenarios | Where-Object { $_.name -ceq $change.scenario })[0].rounds | ForEach-Object { $_[$change.metric] }))
                $after = Median ([double[]]@(@($byRun[$runNames[1]].scenarios | Where-Object { $_.name -ceq $change.scenario })[0].rounds | ForEach-Object { $_[$change.metric] }))
                if ($before -ne [double]$change.before -or $after -ne [double]$change.after) { throw "Control medians differ from raw rounds: $controlName/$($change.scenario)/$($change.metric)" }
                $controlChecks += 2
            }
        }
    }
    $cells = @(foreach ($phase in @('clean','dirty')) {
        foreach ($metric in $metricNames) {
            $values = [ordered]@{}
            foreach ($run in $byRun.Keys) {
                $values[$run] = Median ([double[]]@(@($byRun[$run].scenarios | Where-Object { $_.name -ceq $phase })[0].rounds | ForEach-Object { $_[$metric] }))
            }
            $changes = @($controls | ForEach-Object { $_.comparison.changes } | Where-Object { $_.scenario -ceq $phase -and $_.metric -ceq $metric })
            if ($changes.Count -ne 24) { throw "Missing metric controls: $($scenario.scenario)/$phase/$metric" }
            $drifted = @($changes | Where-Object { if ($null -eq $_.changePercent) { $_.before -ne $_.after } else { [Math]::Abs([double]$_.changePercent) -gt [double]$_.noisePercent } })
            $judgeMetric = @($scenario.set.metrics | Where-Object { $_.phase -ceq $phase -and $_.metric -ceq $metric })
            if ($judgeMetric.Count -ne 1) { throw 'The retained migrated inventory is incomplete.' }
            $roleStats = RoleStats $values
            if ($roleStats.a.median -ne [double]$judgeMetric[0].baselineMedian -or $roleStats.b.median -ne [double]$judgeMetric[0].candidateMedian) { throw "Set medians differ from raw rounds: $($scenario.scenario)/$phase/$metric" }
            $controlCells += $changes.Count
            $driftedObservations += $drifted.Count
            if ($drifted.Count) { $driftedMetricCells++ }
            [ordered]@{phase=$phase;metric=$metric;retainedJudge=$judgeMetric[0];rawRunMedians=$roleStats;controlObservations=$changes.Count;
                driftedControls=$drifted.Count;maximumAbsoluteControlDriftPercent=$(if (@($changes | Where-Object { $null -ne $_.changePercent }).Count) {
                    [double](($changes | Where-Object { $null -ne $_.changePercent } | ForEach-Object { [Math]::Abs([double]$_.changePercent) } | Measure-Object -Maximum).Maximum) } else { 0.0 })}
        }
    })
    $stages = @(foreach ($stage in $stageNames) {
        foreach ($metric in @('privateBytes','workingSetBytes')) {
            $values = [ordered]@{}
            foreach ($run in $byRun.Keys) { $values[$run] = [double]@($byRun[$run].memoryPhases | Where-Object { $_.name -ceq $stage })[0][$metric] }
            [ordered]@{stage=$stage;metric=$metric;bytes=(RoleStats $values)}
        }
    })
    $increments = @(for ($i = 1; $i -lt $stageNames.Count; $i++) {
        foreach ($metric in @('privateBytes','workingSetBytes')) {
            $values = [ordered]@{}
            foreach ($run in $byRun.Keys) { $values[$run] = [double]$byRun[$run].memoryPhases[$i][$metric] - [double]$byRun[$run].memoryPhases[$i-1][$metric] }
            [ordered]@{from=$stageNames[$i-1];to=$stageNames[$i];metric=$metric;bytes=(RoleStats $values)}
        }
    })
    $timingResolution = @(foreach ($phase in @('clean','dirty')) {
        $rounds = @($byRun.Values | ForEach-Object { @($_.scenarios | Where-Object { $_.name -ceq $phase })[0].rounds })
        $samples = [double[]]@($rounds | ForEach-Object { $_.timingSamplesMs.prepare })
        $tickMs = [double]$byRun.Values[0].clock.tickNanoseconds / 1000000.0
        [ordered]@{phase=$phase;prepareSamples=$samples.Count;zeroPrepareSamples=@($samples | Where-Object { $_ -eq 0.0 }).Count;
            prepareSamplesAtMostOneTick=@($samples | Where-Object { $_ -le $tickMs+1e-12 }).Count;
            prepareSampleTicks=(Stats ([double[]]@($samples | ForEach-Object { $_/$tickMs })));
            samplesOffTickGrid=@($samples | Where-Object { [Math]::Abs($_/$tickMs - [Math]::Round($_/$tickMs)) -gt 1e-7 }).Count;
            prepareP95Ticks=(Stats ([double[]]@($rounds | ForEach-Object { [double]$_.prepareP95Ms/$tickMs })))}
    })
    $scenes.Add([ordered]@{scenario=$scenario.scenario;rawReceipts=$byRun.Count;setStatus=$scenario.set.status;
        unstableControls=@($controls | Where-Object { $_.retainedStatus -ceq 'unstable-control' }).Count;controls=$controls.Count;
        metricCells=$cells;memoryStages=$stages;memoryIncrements=$increments;timingResolution=$timingResolution;rawFiles=$rawFiles})
}
$sourceIdentity = @(foreach ($key in @('sourceCommit','sourceFingerprint','executableSha256','benchmarkSha256','harnessIdentity','toolchainIdentity','dependencyIdentity','environmentIdentity','warpSha256','identityStatus','identityScope','sourceDirty','buildIdentityVersion','resolvedBuildIdentity','buildInputIdentity','buildArtifactIdentity','buildAttestationIdentity')) {
    $values = @($receipts | ForEach-Object { [string]$_[$key] } | Sort-Object -Unique)
    [ordered]@{field=$key;distinctValues=$values;distinctCount=$values.Count}
})
$clockGroups = @($receipts | Group-Object { ConvertTo-Json -InputObject $_.clock -Compress } | ForEach-Object {
    [ordered]@{receipts=$_.Count;clock=($_.Name | ConvertFrom-Json -AsHashtable)}
})
$unstableControls = [int](($scenes.ToArray() | ForEach-Object { $_.unstableControls } | Measure-Object -Sum).Sum)
$result = [ordered]@{schemaVersion=1;kind='descriptive-v2-aa-analysis';nativeProgramsExecuted=$false;
    originalBaselineCommit=$summary.baseline.commit;studyPurpose=$summary.studyPurpose;policyStatus=$summary.acceptancePolicy.status;
    harnessCommit=$summary.harnessCommit;harnessDirty=$summary.harnessDirty;
    buildReceipt=[ordered]@{envelopeVersion=$buildReceipt.version;outcome=$buildReceipt.outcome;identity=$buildReceipt.identity;digest=$buildReceipt.digest};
    derivedFrom='The v2-guarded summary.json retained as summary.receipt.txt, 144 original raw receipts and 72 original controls; original bytes are unchanged';
    counts=[ordered]@{rawReceipts=$receipts.Count;rawRounds=$receipts.Count*10;timedFrames=$receipts.Count*10*40;controls=$scenes.Count*24;
        metricCells=$scenes.Count*26;controlMetricObservations=$controlCells;rawControlBeforeAfterValuesVerified=$controlChecks;
        unstableControls=$unstableControls;driftedMetricCells=$driftedMetricCells;
        driftedControlMetricObservations=$driftedObservations};
    orders=@($summary.blockSchedule | Group-Object Order | ForEach-Object { [ordered]@{order=$_.Name;blocks=$_.Count} });
    clockGroups=$clockGroups;receiptIdentity=$sourceIdentity;
    firstReceiptUtc=(@($receipts | ForEach-Object { [datetime]$_.completedUtc } | Sort-Object)[0]).ToUniversalTime().ToString('o');
    lastReceiptUtc=(@($receipts | ForEach-Object { [datetime]$_.completedUtc } | Sort-Object)[-1]).ToUniversalTime().ToString('o');
    sourceSummarySha256=(Get-FileHash -LiteralPath (Join-Path $packet 'summary.receipt.txt') -Algorithm SHA256).Hash;
    statisticDefinition='Each raw run contributes the median of its five rounds. Across-run statistics are descriptive medians/minima/maxima, not new inferential tests. Memory stages use one snapshot per run. Each block compares the median of its two B-labelled runs with its two A-labelled runs; order summaries show those paired differences within ABBA or BAAB blocks. A and B are the same executable.';
    limitations=@('A/A cannot qualify an implementation change.',"$unstableControls controls are unstable in at least one metric.",'The person at this machine agreed to a quiet window; no continuous ambient-load trace establishes a dedicated host.','No new policy qualification.','Stage differences localize observations between snapshots and cannot attribute a component, allocator or leak.','Hidden snapshot follows all timed rounds and visibility change and precedes teardown.');
    scenarios=$scenes.ToArray()}
[IO.File]::WriteAllText((Join-Path $packet 'analysis.txt'), ((ConvertTo-Json -InputObject $result -Depth 32) + "`n"), [Text.UTF8Encoding]::new($false))
$names = [string[]]@(Get-ChildItem -LiteralPath $packet -File | Where-Object { $_.Name -cne 'SHA256SUMS' } | ForEach-Object { $_.Name })
[Array]::Sort($names, [StringComparer]::Ordinal)
$manifest = @($names | ForEach-Object { "$((Get-FileHash -LiteralPath (Join-Path $packet $_) -Algorithm SHA256).Hash)  $_" })
[IO.File]::WriteAllText((Join-Path $packet 'SHA256SUMS'), (($manifest -join "`n") + "`n"), [Text.UTF8Encoding]::new($false))
Write-Host "Analyzed $($receipts.Count) raw receipts, $($scenes.Count*24) controls and $($scenes.Count*26) metric cells; verified $controlChecks before/after control medians."
