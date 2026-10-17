[CmdletBinding()]
param([string] $PacketRoot=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))

function Get-Median($Values) {
    $sorted=@($Values | Sort-Object)
    if (-not $sorted.Count) { throw 'Missing analytical series.' }
    $middle=[int][Math]::Floor($sorted.Count/2)
    if ($sorted.Count % 2) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle-1]+[double]$sorted[$middle])/2
}
function Get-Range($Values) {
    $valuesArray=@($Values)
    [ordered]@{ minimum=($valuesArray | Measure-Object -Minimum).Minimum; median=(Get-Median $valuesArray)
        maximum=($valuesArray | Measure-Object -Maximum).Maximum }
}

foreach ($line in Get-Content -LiteralPath (Join-Path $PacketRoot 'SHA256SUMS')) {
    if ($line -cnotmatch '^([A-F0-9]{64})  (.+)$') { throw "Invalid packet hash line: $line" }
    if ((Get-FileHash -LiteralPath (Join-Path $PacketRoot $Matches[2]) -Algorithm SHA256).Hash -cne $Matches[1]) {
        throw "Packet hash mismatch: $($Matches[2])"
    }
}
$summary=Get-Content (Join-Path $PacketRoot 'Paired/summary.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
if ($summary.studyVersion -ne 3 -or $summary.blocks -ne 12 -or $summary.scenarios.Count -ne 1 -or
    $summary.scenarios[0].scenario -cne 'Default' -or $summary.steps.Count -ne 48) { throw 'Unexpected focused study inventory.' }
foreach ($path in $summary.harness.Keys) {
    if ((Get-FileHash (Join-Path $repositoryRoot $path) -Algorithm SHA256).Hash -cne $summary.harness[$path]) {
        throw "Replay harness differs from recorded bytes: $path"
    }
}
Import-Module (Join-Path $repositoryRoot 'Tools/PerformanceComparison.psm1') -Force
$receipts=@{}
$runRows=[Collections.Generic.List[object]]::new()
$tailRows=[Collections.Generic.List[object]]::new()
foreach ($step in $summary.steps) {
    $name='Default-'+$step.Name+'.json'
    $receipt=Read-PerformanceReceipt (Join-Path $PacketRoot ('Paired/'+$name))
    $receipts[$step.Name]=$receipt
    foreach ($phase in $receipt.scenarios) {
        $runRows.Add([ordered]@{ run=$step.Name; side=$step.Side; phase=$phase.name
            frameP95Ms=(Get-Median $phase.rounds.frameP95Ms); fps=(Get-Median $phase.rounds.fps)
            prepareP95Ms=(Get-Median $phase.rounds.prepareP95Ms); composeCpuP95Ms=(Get-Median $phase.rounds.composeCpuP95Ms)
            privateBytes=(Get-Median $phase.rounds.privateBytes); workingSetBytes=(Get-Median $phase.rounds.workingSetBytes) })
        for ($roundIndex=0; $roundIndex -lt $phase.rounds.Count; $roundIndex++) {
            $round=$phase.rounds[$roundIndex]
            # Match the unchanged native percentile index. Components refer to this same
            # tail frame, rather than subtracting independently ranked percentiles.
            $indices=@(0..39 | Sort-Object { [double]$round.timingSamplesMs.frame[$_] })
            $index=$indices[38]
            $frame=[double]$round.timingSamplesMs.frame[$index]
            $prepare=[double]$round.timingSamplesMs.prepare[$index]
            $compose=[double]$round.timingSamplesMs.composeCpu[$index]
            $tailRows.Add([ordered]@{ run=$step.Name; side=$step.Side; phase=$phase.name; round=$roundIndex+1; frameIndex=$index
                frameP95Ms=$frame; prepareMs=$prepare; composeCpuMs=$compose; remainderMs=($frame-$prepare-$compose)
                remainderPercent=(100*($frame-$prepare-$compose)/$frame) })
        }
    }
}
$blocks=@($summary.blockSchedule | ForEach-Object { [ordered]@{ name=$_.Name; order=$_.Order
    Baseline=@($_.BaselineRuns | ForEach-Object {$receipts[$_]}); Candidate=@($_.CandidateRuns | ForEach-Object {$receipts[$_]}) } })
$replayed=Compare-PairedBlockSet -Blocks $blocks -MinimumBlocks 12 -AssignmentProtocol $summary.allocation
if ((ConvertTo-Json $replayed -Depth 16 -Compress) -cne (ConvertTo-Json $summary.scenarios[0].set -Depth 16 -Compress)) {
    throw 'Replayed paired judge differs from the original retained verdict.'
}
$controls=@($summary.scenarios[0].comparisons | Where-Object {$_.name -like '*-control'})
$controlP95=@(foreach ($control in $controls) {
    $comparison=Get-Content (Join-Path $PacketRoot ('Paired/'+$control.file)) -Raw | ConvertFrom-Json -AsHashtable
    $metric=@($comparison.changes | Where-Object {$_.scenario -ceq 'clean' -and $_.metric -ceq 'frameP95Ms'})[0]
    [ordered]@{ name=$control.name; status=$control.status; changePercent=$metric.changePercent; allDrift=$control.drift }
})
$sides=@(foreach ($side in @('baseline','candidate')) {
    $runs=@($runRows | Where-Object {$_.side -ceq $side -and $_.phase -ceq 'clean'})
    $tails=@($tailRows | Where-Object {$_.side -ceq $side -and $_.phase -ceq 'clean'})
    [ordered]@{ side=$side; runs=$runs.Count; cleanRunP95Ms=(Get-Range $runs.frameP95Ms)
        cleanTailPrepareMs=(Get-Range $tails.prepareMs); cleanTailComposeCpuMs=(Get-Range $tails.composeCpuMs)
        cleanTailRemainderMs=(Get-Range $tails.remainderMs); cleanTailRemainderPercent=(Get-Range $tails.remainderPercent) }
})

$plan=Get-Content (Join-Path $PacketRoot 'Profiles/plan.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
$completion=Get-Content (Join-Path $PacketRoot 'Profiles/completion.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
if ($plan.reportBudget -ne 8 -or $completion.attempted -ne 8 -or $completion.results.Count -ne 8) { throw 'Incomplete profile inventory.' }
foreach ($role in @('A','B')) {
    $canonical=$receipts[($role+'01')]
    foreach ($key in @('sourceCommit','sourceFingerprint','executableSha256','buildArtifactIdentity','buildInputIdentity',
        'buildAttestationIdentity','toolchainIdentity','benchmarkSha256','harnessIdentity','dependencyIdentity','environmentIdentity')) {
        if ($plan.anchors[$role][$key] -cne $canonical[$key]) { throw "Profile/canonical identity differs: $role/$key" }
    }
}
foreach ($path in $plan.anchors.A.profilingInputs.Keys) {
    if ((Get-FileHash (Join-Path $repositoryRoot $path) -Algorithm SHA256).Hash -cne $plan.anchors.A.profilingInputs[$path]) { throw "Profiling replay helper differs: $path" }
}
Import-Module (Join-Path $repositoryRoot 'Tools/PerformanceProfiling.psm1') -Force
Import-Module (Join-Path $repositoryRoot 'Tools/PerformancePolicy.psm1')
$profileRows=@(for ($index=0; $index -lt 8; $index++) {
    $result=$completion.results[$index]
    $role=@('A','B','B','A','B','A','A','B')[$index]
    $heap=$index -ge 4
    if ($result.role -cne $role -or $result.heap -ne $heap -or $result.exitCode -ne 0 -or
        $result.timedOut -or -not $result.identityMatched) { throw 'Unexpected profile schedule/result.' }
    $name=$result.report
    $report=Get-Content (Join-Path $PacketRoot ('Profiles/'+$name)) -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    $state=[ordered]@{}
    foreach ($key in $plan.anchors[$role].Keys) { $state[$key]=$report[$key] }
    Assert-PerformanceMeasurementState -Before $plan.anchors[$role] -After $state
    $companions=[ordered]@{rawReportSha256='.raw.receipt.txt'; childMetadataSha256='.child.receipt.txt'; logSha256='.log'; captureSha256='.png'}
    foreach ($key in $companions.Keys) {
        if ((Get-FileHash (Join-Path $PacketRoot ('Profiles/'+$name+$companions[$key])) -Algorithm SHA256).Hash -cne $report[$key]) { throw "Profile companion differs: $name/$key" }
    }
    $child=Get-Content (Join-Path $PacketRoot ('Profiles/'+$name+'.child.receipt.txt')) -Raw | ConvertFrom-Json -AsHashtable
    if ($child.exitCode -ne 0 -or $child.timedOut -or $child.terminationError -or $child.drainError -or
        $child.arguments.Count -ne 4 -or $child.arguments[0] -cne '--benchmark-complex-ui-profile' -or
        $child.arguments[2] -cne 'Default' -or $child.arguments[3] -cne $(if ($heap) {'On'} else {'Off'})) { throw "Invalid native child metadata: $name" }
    $raw=Get-Content (Join-Path $PacketRoot ('Profiles/'+$name+'.raw.receipt.txt')) -Raw | ConvertFrom-Json -AsHashtable -Depth 32
    $completeness=Assert-ProfilingReport -Receipt $raw -Scenario Default -Heap $heap -Configuration Release -ProcessId $child.processId
    foreach ($key in $raw.Keys) {
        if ((ConvertTo-Json $raw[$key] -Depth 32 -Compress) -cne (ConvertTo-Json $report[$key] -Depth 32 -Compress)) { throw "Stamped native profile field differs: $name/$key" }
    }
    $windows=@(foreach ($window in $report.windows) {
        $cpu=[ordered]@{}
        foreach ($kind in @('main','process')) {
            $before=$window.cpuBefore[$kind]; $after=$window.cpuAfter[$kind]
            $cpu[$kind]=[ordered]@{ timeMs=$(if ($before.timesAvailable -and $after.timesAvailable) {
                ([double]$after.kernel100ns+$after.user100ns-$before.kernel100ns-$before.user100ns)/10000 } else {$null})
                cycles=$(if ($before.cyclesAvailable -and $after.cyclesAvailable) { [decimal]$after.cycles-[decimal]$before.cycles } else {$null}) }
        }
        [ordered]@{ window=$window.window; frames=$window.frames.Count
            completedFps=($window.frames.Count*1000/($window.frames.completedMs | Measure-Object -Sum).Sum)
            completedMs=(Get-Range $window.frames.completedMs); prepareMs=(Get-Range $window.frames.prepareMs)
            composeCpuMs=(Get-Range $window.frames.compositeMs); cpu=$cpu }
    })
    [ordered]@{ report=$name; role=$role; heap=$heap; telemetryCompleteness=$completeness; windows=$windows
        samplingMs=($report.stages.samplingMs | Measure-Object -Sum).Sum
        heapSamplingMs=($report.stages.heapSampler.durationMs | Measure-Object -Sum).Sum
        stages=@($report.stages | ForEach-Object {[ordered]@{stage=$_.stage; elapsedMs=$_.elapsedMs; samplingMs=$_.samplingMs
            privateBytes=$_.process.privateBytes; workingSetBytes=$_.process.workingSetBytes; observedThreads=$_.scheduling.threads.Count
            threadError=$_.scheduling.threadError; grid=$_.grid; view=$_.view; heapSampler=$_.heapSampler}}) }
})
$analysis=[ordered]@{ classification='focused-diagnostic-only'; acceptance='HOLD'; seed=$summary.seed; order=$summary.order
    pairedStatus=$replayed.status; pairedMetrics=$replayed.metrics; replayMatches=$true; sides=$sides
    sameBinaryControls=$controlP95; unstableControls=@($controls | Where-Object {$_.status -ceq 'unstable-control'}).Count
    cleanP95ControlBandCrossings=@($controlP95 | Where-Object {[Math]::Abs($_.changePercent) -gt 5}).Count
    runMedians=$runRows.ToArray(); roundTailFrames=$tailRows.ToArray(); profiles=$profileRows
    limits='Default only; shared sampler and longer diagnostic warmup differ from canonical frames. The same-frame remainder includes target bind/clear, copy, blocking readback and other overhead; it does not identify GPU execution or scheduling delay. CPU times are coarse; raw cycles describe consumption. Heap sampling may perturb later work. No policy, full-scene, causal or regression acceptance follows.' }
$analysis | ConvertTo-Json -Depth 32 | Set-Content (Join-Path $PacketRoot 'analysis.receipt.txt') -Encoding utf8
Write-Host 'Verified 48 canonical and eight diagnostic reports, their retained identities/companions, native fields and unchanged paired-judge replay. Acceptance remains HOLD.'
