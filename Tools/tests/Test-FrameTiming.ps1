# Frame timing method: the statistics of Tools/FrameTiming.psm1 and the public performance-timing.ps1 runner. The runner
# runs in isolated PowerShell children with a shimmed executable, powercfg, build, identity helpers and host observer;
# nothing here compiles, renders, changes the real power scheme or produces timing evidence.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $repository 'Tools/FrameTiming.psm1') -Force
$fixturePlatform = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }

function New-TimingRun([int] $Frames = 1000, [double] $Seconds = 2.5, [double] $Scale = 1.0, [string] $Renderer = 'Hardware', [int] $Disjoint = 0,
    [int] $MinimumFrames = 1000, [double] $MinimumSeconds = 2.0, [switch] $Quantized, [double] $SiblingBusy = 0.01, [double] $OtherProcessors = 2.0,
    [int] $RunningPermyriad = 9990) {
    # Cycle samples vary in steps of 10 cycles and submission times in 0.5 µs steps, so every statistic is resolved; with
    # -Quantized the run looks like a frame that does nothing: preparation and submission constant, composition one quantum.
    $valid = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0 } else { 1 } })
    return [ordered]@{
        schema = 1; method = 'dxui-frame-timing-v1'; fixture = 'FX5'; phase = 'dirty'; renderer = $Renderer
        adapter = [ordered]@{ description = 'Synthetic'; vendorId = 1; deviceId = 2; software = ($Renderer -eq 'WARP'); driverVersion = '1.2.3.4'; dedicatedVideoBytes = 0; luid = '0x00000000_0x0001bdd0' }
        interference = [ordered]@{ tscAvailable = $true; wallSeconds = $Seconds; logicalProcessors = 32; ownProcessors = 1.2; threadShare = 0.9
            measuringBusy = 0.95; siblingBusy = $SiblingBusy; otherOnMeasuring = 0.02; otherProcessors = $OtherProcessors; processorBusy = @(for ($i = 0; $i -lt 32; $i++) { 0.05 }) }
        pinning = [ordered]@{ requested = 'auto'; pinned = $true; processor = 3; efficiencyClass = 1; maximumEfficiencyClass = 1; isolated = $true; scope = 'measuring UI thread only' }
        warmupFrames = 60; minimumFrames = $MinimumFrames; minimumSeconds = $MinimumSeconds; frames = $Frames; elapsedSeconds = $Seconds
        gpuSlots = 2; gpuReadLatencyFrames = 1; timestampFrequency = 1000000000; disjointFrames = $Disjoint
        preparedFrames = $Frames; timedSteps = 0; rebuilds = 0
        samples = [ordered]@{
            frames = $Frames
            prepareCycles = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($Quantized) { 858 } else { [long]((100000 + 10 * ($frame % 10)) * $Scale) } })
            compositeCycles = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($Quantized) { 858 + 430 * ($frame % 2) } else { [long]((50000 + 10 * ($frame % 7)) * $Scale) } })
            submitUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($Quantized) { 2.4 } else { (400.0 + 0.5 * ($frame % 5)) * $Scale } })
            gpuPrepareUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0.0 } else { 200.0 * $Scale } })
            gpuCompositeUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0.0 } else { 100.0 * $Scale } })
            gpuWaitUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { 5.0 })
            gpuValid = $valid
            gpuReadAfterSubmitted = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame + 1 -lt $Frames) { $frame + 2 } else { $Frames } })
            runningPermyriad = @(for ($frame = 0; $frame -lt $Frames; $frame++) { $RunningPermyriad })
        }
    }
}
function New-Session([double[]] $Scales, [string[]] $Hashes = @('E','E','E','E'), [switch] $Quantized) {
    $roles = @('A','B','B','A')
    return [ordered]@{ order = 'ABBA'; runs = @(for ($slot = 0; $slot -lt 4; $slot++) {
        [ordered]@{ role = $roles[$slot]; slot = $slot; executableSha256 = $Hashes[$slot]; summary = (Get-FrameTimingRunSummary (New-TimingRun -Frames 20 -Scale $Scales[$slot] -MinimumFrames 1 -MinimumSeconds 0 -Quantized:$Quantized)) }
    }) }
}
function Get-Metric($Table, [string] $Name) { return @($Table | Where-Object { $_['metric'] -ceq $Name })[0] }

Invoke-TestCase 'six judged metrics declare 3% CPU and 5% GPU margins' {
    $metrics = Get-FrameTimingMetrics
    Assert-Equal 6 $metrics.Count 'judged metric count'
    Assert-Equal 4 @($metrics | Where-Object { $_.kind -eq 'cpu' -and $_.marginPercent -eq 3.0 }).Count 'CPU metrics at 3%'
    Assert-Equal 2 @($metrics | Where-Object { $_.kind -eq 'gpu' -and $_.marginPercent -eq 5.0 }).Count 'GPU metrics at 5%'
    Assert-Contains @($metrics | ForEach-Object { $_.name }) 'prepareCyclesMedian' 'separate preparation cycles'
    Assert-Contains @($metrics | ForEach-Object { $_.name }) 'compositeCyclesMedian' 'separate composition cycles'
}

Invoke-TestCase 'nearest-rank percentile' {
    Assert-Equal 95.0 (Get-FrameTimingPercentile ([double[]](1..100)) 0.95) 'p95 of 1..100'
    Assert-Equal 7.0 (Get-FrameTimingPercentile ([double[]]@(7)) 0.95) 'p95 of one sample'
}

Invoke-TestCase 'run validation accepts a judged run and refuses short, late-read, WARP and disjoint runs' {
    Assert-FrameTimingRun -Run (New-TimingRun) -Judged
    Assert-Throws { Assert-FrameTimingRun -Run (New-TimingRun -Seconds 1.5) } 'run shorter than its minimum duration'
    Assert-Throws { Assert-FrameTimingRun -Run (New-TimingRun -Frames 999) } 'run with fewer than its minimum frames'
    $late = New-TimingRun; $late.samples.gpuReadAfterSubmitted[10] = 11
    Assert-Throws { Assert-FrameTimingRun -Run $late } 'GPU result read before the next frame was submitted'
    $wait = New-TimingRun; $wait.gpuReadLatencyFrames = 0
    Assert-Throws { Assert-FrameTimingRun -Run $wait } 'zero-latency GPU read'
    Assert-FrameTimingRun -Run (New-TimingRun -Renderer 'WARP')
    Assert-Throws { Assert-FrameTimingRun -Run (New-TimingRun -Renderer 'WARP') -Judged } 'judged WARP run'
    Assert-Throws { Assert-FrameTimingRun -Run (New-TimingRun -Frames 100 -Seconds 0.2 -MinimumFrames 100 -MinimumSeconds 0.1) -Judged } 'judged short run'
    Assert-Throws { Assert-FrameTimingRun -Run (New-TimingRun -Disjoint 60) -Judged } 'judged run with 6% disjoint frames'
    Assert-FrameTimingRun -Run (New-TimingRun -Disjoint 40) -Judged
    $miscount = New-TimingRun -Disjoint 10; $miscount.disjointFrames = 0
    Assert-Throws { Assert-FrameTimingRun -Run $miscount } 'unaccounted GPU frames'
    $idle = New-TimingRun; $idle.preparedFrames = 385
    Assert-FrameTimingRun -Run $idle
    Assert-Throws { Assert-FrameTimingRun -Run $idle -Judged } 'judged dirty run in which frames prepared nothing'
    $unknown = New-TimingRun; $unknown.Remove('preparedFrames')
    Assert-Throws { Assert-FrameTimingRun -Run $unknown -Judged } 'judged dirty run that does not report its prepared frames'
    $clean = New-TimingRun; $clean.phase = 'clean'; $clean.preparedFrames = 0
    Assert-FrameTimingRun -Run $clean -Judged
}

Invoke-TestCase 'clock steps: the smallest gap above rounding, or none for a constant series' {
    Assert-Equal 428.0 (Get-FrameTimingStep ([double[]]@(858, 859, 1287, 1288, 1716)) 2.0) 'cycle quantum through rounding gaps'
    Assert-Equal $null (Get-FrameTimingStep ([double[]]@(858, 858, 859)) 2.0) 'a series of one value (and rounding) has no observable step'
    Assert-True ([Math]::Abs((Get-FrameTimingStep ([double[]]@(2.4, 2.5, 2.7)) 1e-6) - 0.1) -lt 1e-9) 'submission time step'
    $summary = Get-FrameTimingRunSummary (New-TimingRun -Quantized)
    Assert-Equal $null $summary.resolutionPercent.submitUsMedian 'constant submission time is unresolved'
    Assert-True ($summary.resolutionPercent.prepareCyclesMedian -gt 40.0) 'a one-quantum preparation statistic is resolution-limited'
    Assert-True ((Get-FrameTimingRunSummary (New-TimingRun)).resolutionPercent.prepareCyclesMedian -lt 0.3) 'a large statistic is resolved'
}

Invoke-TestCase 'run summary separates preparation and composition and excludes disjoint GPU frames' {
    $run = New-TimingRun -Disjoint 40
    $summary = Get-FrameTimingRunSummary $run
    Assert-Equal 100045.0 $summary.prepareCyclesMedian 'preparation cycle median'
    Assert-Equal 50030.0 $summary.compositeCyclesMedian 'composition cycle median'
    $ui = [double[]]@(for ($frame = 0; $frame -lt 1000; $frame++) { [double]$run.samples.prepareCycles[$frame] + [double]$run.samples.compositeCycles[$frame] })
    Assert-Equal (Get-FrameTimingPercentile $ui 0.95) $summary.uiThreadCyclesP95 'UI-thread cycle p95 over per-frame sums'
    Assert-Equal 1000 $summary.preparedFrames 'prepared frames carried into the summary'
    Assert-Equal 300.0 $summary.gpuFrameUsMedian 'GPU median over valid frames only'
    Assert-Equal 960 $summary.validGpuFrames 'valid GPU frames'
}

Invoke-TestCase 'A/B/B/A session effect balances drift and keeps both pair ratios' {
    $effects = Get-FrameTimingSessionEffects (New-Session @(1.0, 1.1, 1.1, 1.0))
    Assert-True ([Math]::Abs([Math]::Exp($effects.submitUsMedian.effect) - 1.1) -lt 1e-9) 'B/A effect'
    Assert-Equal 2 $effects.submitUsMedian.pairRatios.Count 'two pair ratios'
    $drift = Get-FrameTimingSessionEffects (New-Session @(1.0, 1.1, 1.21, 1.331))
    Assert-True ([Math]::Abs($drift.submitUsMedian.effect) -lt 1e-9) 'drift linear in log time cancels in ABBA'
    Assert-Throws { Get-FrameTimingSessionEffects ([ordered]@{ order = 'AABB'; runs = @() }) } 'non-ABBA session'
}

Invoke-TestCase 'equivalence needs five sessions inside the margin' {
    $tiny = [double[]]@(0.001, -0.002, 0.0015, -0.001, 0.0005)
    Assert-True (Test-FrameTimingEquivalence $tiny 3.0).equivalent 'five small effects are equivalent within 3%'
    Assert-True (-not (Test-FrameTimingEquivalence ([double[]]$tiny[0..3]) 3.0).equivalent) 'four sessions can never establish equivalence'
    Assert-Equal 0.03125 (Test-FrameTimingEquivalence $tiny 3.0).minimumAttainableP 'smallest one-sided p for five sessions'
    $shifted = [double[]]@($tiny | ForEach-Object { $_ + [Math]::Log(1.04) })
    Assert-True (-not (Test-FrameTimingEquivalence $shifted 3.0).equivalent) 'a 4% shift is not equivalent within 3%'
    Assert-True (Test-FrameTimingEquivalence $shifted 5.0).equivalent 'a 4% shift is equivalent within 5%'
    Assert-Equal 1.0 (Get-FrameTimingUpperSignFlipPValue ([double[]]@(-1,-1,-1))) 'one-sided p of uniformly negative effects'
    Assert-Equal 0.125 (Get-FrameTimingUpperSignFlipPValue ([double[]]@(1,1,1))) 'one-sided p of uniformly positive effects'
}

Invoke-TestCase 'minimum detectable effect follows the session spread' {
    Assert-Equal $null (Get-FrameTimingMinimumDetectableEffect ([double[]]@(0.01))) 'one session has no spread'
    $mde = Get-FrameTimingMinimumDetectableEffect ([double[]]@(0.01, -0.01, 0.01, -0.01, 0.0))
    $sd = [Math]::Sqrt(0.0004 / 4)
    Assert-True ([Math]::Abs($mde - ([Math]::Exp((2.776 + 0.941) * $sd / [Math]::Sqrt(5)) - 1) * 100) -lt 1e-9) 'MDE with four degrees of freedom'
    Assert-True ((Get-FrameTimingMinimumDetectableEffect ([double[]]@(0.02, -0.02, 0.02, -0.02, 0.0))) -gt $mde) 'a wider spread raises the MDE'
}

Invoke-TestCase 'calibration judges quiet metrics and records noisy ones as advisory with their noise floor' {
    $sessions = @(
        (New-Session @(1.00, 1.01, 0.99, 1.00)), (New-Session @(1.00, 1.00, 1.01, 0.99)), (New-Session @(1.00, 0.99, 1.00, 1.01)),
        (New-Session @(1.00, 1.00, 1.00, 1.00)), (New-Session @(1.00, 1.01, 1.00, 1.00)))
    $quiet = Get-FrameTimingCalibration -Sessions $sessions
    Assert-Equal 6 $quiet.judgedMetrics 'all metrics judged below their margins'
    $noisy = @($sessions[0..3]) + @(New-Session @(1.00, 1.04, 1.00, 1.00))
    $result = Get-FrameTimingCalibration -Sessions $noisy
    $cpu = Get-Metric $result.metrics 'submitUsMedian'
    Assert-Equal 'advisory' $cpu.status 'a 4% same-binary deviation exceeds the 3% CPU margin'
    Assert-True ([Math]::Abs($cpu.noiseFloorPercent - 4.0) -lt 1e-9) 'noise floor is the largest pair deviation'
    Assert-Equal 'judged' (Get-Metric $result.metrics 'gpuFrameUsMedian').status 'a 4% deviation stays within the 5% GPU margin'
    Assert-Throws { Get-FrameTimingCalibration -Sessions @(New-Session @(1,1,1,1) @('E','F','F','E')) } 'calibration with two executables'
    # A 0.00% floor from frames that do nothing (FX3's wheel played past the end of its list) is never judged.
    $empty = Get-FrameTimingCalibration -Sessions @(for ($i = 0; $i -lt 5; $i++) { New-Session @(1,1,1,1) -Quantized })
    $prepare = Get-Metric $empty.metrics 'prepareCyclesMedian'
    Assert-Equal 0.0 ([double]$prepare.noiseFloorPercent) 'a constant statistic has a zero floor'
    Assert-Equal 'advisory' $prepare.status 'a zero floor at one clock quantum is not judged'
    Assert-True $prepare.reason.Contains('resolution') 'the reason names the clock resolution'
    Assert-Equal 'advisory' (Get-Metric $empty.metrics 'submitUsMedian').status 'a constant submission time is not judged'
    Assert-True (Get-Metric $empty.metrics 'submitUsMedian').reason.Contains('not observable') 'the reason says the step cannot be observed'
}

Invoke-TestCase 'comparison reports faster, equivalent, inconclusive and advisory verdicts' {
    $faster = @(for ($i = 0; $i -lt 6; $i++) { New-Session @(1.0, (0.90 + $i * 0.002), (0.90 + $i * 0.001), 1.0) @('A','B','B','A') })
    $result = Compare-FrameTimingSessions -Sessions $faster
    Assert-Equal 'faster' (Get-Metric $result.metrics 'submitUsMedian').verdict 'a consistent 10% reduction over six sessions'
    $five = Compare-FrameTimingSessions -Sessions @($faster[0..4])
    Assert-Equal 'inconclusive' (Get-Metric $five.metrics 'submitUsMedian').verdict 'five sessions cannot reach two-sided p < 0.05'
    $same = @(for ($i = 0; $i -lt 6; $i++) { New-Session @(1.0, 1.001, 0.999, 1.0) @('A','B','B','A') })
    Assert-Equal 'equivalent' (Get-Metric (Compare-FrameTimingSessions -Sessions $same).metrics 'gpuFrameUsMedian').verdict 'equivalent within 5%'
    $calibration = [ordered]@{ metrics = @(
        [ordered]@{ metric = 'submitUsMedian'; status = 'advisory'; noiseFloorPercent = 12.0 },
        [ordered]@{ metric = 'gpuFrameUsMedian'; status = 'judged'; noiseFloorPercent = 2.0 }) }
    $calibrated = Compare-FrameTimingSessions -Sessions $faster -Calibration $calibration
    Assert-Equal 'advisory' (Get-Metric $calibrated.metrics 'submitUsMedian').verdict 'advisory metric gets no verdict'
    Assert-Equal 'faster' (Get-Metric $calibrated.metrics 'gpuFrameUsMedian').verdict 'judged metric beyond its noise floor'
}

Invoke-TestCase 'host busy share from observer counters' {
    $samples = @([ordered]@{ idle100ns = 0; kernelIncludingIdle100ns = 0; user100ns = 0 }, [ordered]@{ idle100ns = 900; kernelIncludingIdle100ns = 950; user100ns = 50 })
    Assert-True ([Math]::Abs((Get-FrameTimingHostBusyPercent $samples) - 10.0) -lt 1e-9) 'busy percent'
    Assert-Throws { Get-FrameTimingHostBusyPercent @($samples[0]) } 'one observation'
}

function New-FullCalibrationReceipt {
    $roles = @('A','B','B','A')
    $scales = @(@(1.00, 1.01, 0.99, 1.00), @(1.00, 1.00, 1.01, 0.99), @(1.00, 0.99, 1.00, 1.01), @(1.00, 1.00, 1.00, 1.00), @(1.00, 1.01, 1.00, 1.00))
    $sessions = @(for ($session = 0; $session -lt 5; $session++) {
        [ordered]@{ session = $session + 1; order = 'ABBA'; runs = @(for ($slot = 0; $slot -lt 4; $slot++) {
            $raw = New-TimingRun -Frames 40 -Scale $scales[$session][$slot] -MinimumFrames 1 -MinimumSeconds 0
            [ordered]@{ role = $roles[$slot]; slot = $slot; report = "raw-$session-$slot.json"; reportSha256 = ('{0:X64}' -f ($session * 4 + $slot)); raw = $raw
                executableSha256 = 'E'; summary = (Get-FrameTimingRunSummary $raw) }
        }) }
    })
    $receipt = [ordered]@{ schema = 1; purpose = 'Calibration'; workloadOwner = 'DxUi'; fixture = 'dxui-frame-timing-v1'; fixtureName = 'FX5'; sessions = $sessions }
    $receipt['calibration'] = Get-FrameTimingCalibration -Sessions $sessions
    return (ConvertTo-Json -InputObject $receipt -Depth 40 -Compress) | ConvertFrom-Json -AsHashtable -Depth 40
}

Invoke-TestCase 'retained receipt: no samples, same judged values, floors and verdicts, refused when they would differ' {
    $full = New-FullCalibrationReceipt
    $sizes = @{}; foreach ($run in @($full.sessions | ForEach-Object { $_.runs })) { $sizes[$run.reportSha256] = 1234 }
    $retained = ConvertTo-FrameTimingRetainedReceipt -Receipt $full -ReportSizes $sizes
    Assert-Equal 0 @($retained.sessions | ForEach-Object { $_.runs } | Where-Object { $_.Contains('raw') }).Count 'no raw report retained'
    Assert-Equal 1234 $retained.sessions[0].runs[0].reportSizeBytes 'report size kept'
    foreach ($metric in $full.calibration.metrics) {
        $kept = Get-Metric $retained.calibration.metrics $metric.metric
        foreach ($key in @('noiseFloorPercent','minimumDetectableEffectPercent','status','pairRatios','sessionEffectsPercent')) {
            Assert-True (Test-FrameTimingSameValue $metric[$key] $kept[$key]) "$($metric.metric).$key unchanged"
        }
        Assert-True (Test-FrameTimingSameValue $metric.equivalence $kept.equivalence) "$($metric.metric) equivalence unchanged"
    }
    $recomputed = Get-FrameTimingCalibration -Sessions $retained.sessions
    Assert-Equal (ConvertTo-Json -InputObject $retained.calibration -Depth 20 -Compress) (ConvertTo-Json -InputObject $recomputed -Depth 20 -Compress) 'the retained summaries reproduce the judgement'
    $again = ConvertTo-FrameTimingRetainedReceipt -Receipt (New-FullCalibrationReceipt) -ReportSizes $sizes
    Assert-Equal (ConvertTo-Json -InputObject $retained -Depth 40 -Compress) (ConvertTo-Json -InputObject $again -Depth 40 -Compress) 'conversion is deterministic'
    $second = ConvertTo-FrameTimingRetainedReceipt -Receipt $retained
    Assert-Equal (ConvertTo-Json -InputObject $retained -Depth 40 -Compress) (ConvertTo-Json -InputObject $second -Depth 40 -Compress) 'a retained receipt converts to itself'
    $tampered = New-FullCalibrationReceipt; $samples = $tampered.sessions[1].runs[2].raw.samples
    for ($frame = 0; $frame -lt $samples.submitUs.Count; $frame++) { $samples.submitUs[$frame] = 9999.0 }
    Assert-Throws { ConvertTo-FrameTimingRetainedReceipt -Receipt $tampered } 'a summary that its raw report does not reproduce'
    $floor = New-FullCalibrationReceipt; $floor.calibration.metrics[0].noiseFloorPercent = 0.0
    Assert-Throws { ConvertTo-FrameTimingRetainedReceipt -Receipt $floor } 'a stored floor that the samples do not reproduce'
    $verdict = New-FullCalibrationReceipt; $verdict.calibration.metrics[0].status = 'advisory'
    Assert-Throws { ConvertTo-FrameTimingRetainedReceipt -Receipt $verdict } 'a stored status that the samples do not reproduce'
}

Invoke-TestCase 'admission: decisions on recorded samples (quiet, loaded elsewhere, loaded on the sibling, GPU busy, thread preempted)' {
    $rule = Get-FrameTimingAdmissionRule
    $admit = { param($Run, $Gpu = 0.02) Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary $Run) -OtherGpuBusy $Gpu -Rule $rule }
    $quiet = & $admit (New-TimingRun)
    Assert-True $quiet.admitted "a quiet run is admitted: $($quiet.reasons -join '; ')"
    Assert-Equal 0.999 $quiet.indicators.runningShareMedian 'running share recorded'
    Assert-True (& $admit (New-TimingRun -OtherProcessors 7.5)).admitted 'seven and a half busy logical processors elsewhere on 32 stay under the cap of 8'
    $elsewhere = & $admit (New-TimingRun -OtherProcessors 9.0)
    Assert-True (-not $elsewhere.admitted -and $elsewhere.reasons[0].Contains('cap of 8')) 'nine busy logical processors exceed the cap'
    $sibling = & $admit (New-TimingRun -SiblingBusy 0.4)
    Assert-True (-not $sibling.admitted -and $sibling.reasons[0].Contains('sibling')) 'a busy sibling is refused'
    $gpu = & $admit (New-TimingRun) 0.2
    Assert-True (-not $gpu.admitted -and $gpu.reasons[0].Contains('GPU')) 'another process busy on the measured GPU is refused'
    $preempted = & $admit (New-TimingRun -RunningPermyriad 9700)
    Assert-True (-not $preempted.admitted -and $preempted.reasons[0].Contains('median')) 'a thread that lost 3% of its frame time is refused'
    $old = New-TimingRun; $old.Remove('interference'); $old.samples.Remove('runningPermyriad')
    $unrecorded = & $admit $old
    Assert-True (-not $unrecorded.admitted -and ($unrecorded.reasons -join ' ').Contains('not recorded')) 'a run without interference records is refused by the interim rule'
    $legacy = Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary (New-TimingRun -SiblingBusy 0.9)) -OtherGpuBusy 0.9 -Rule (Get-FrameTimingAdmissionRule -Name 'host-busy-10pct-v1')
    Assert-True $legacy.admitted 'the legacy rule judges no run'
    Assert-Equal 0.9 $legacy.indicators.siblingBusy 'the legacy rule still records the indicators'
}

Invoke-TestCase 'admission: interim v2 widens the own-processor limit, records core isolation and judges a session''s load steadiness' {
    $v2 = Get-FrameTimingAdmissionRule
    $v1 = Get-FrameTimingAdmissionRule -Name 'interference-interim-v1'
    Assert-Equal 'interference-interim-v2' $v2.id 'v2 is the default'
    $run = New-TimingRun; $run.interference.otherOnMeasuring = 0.058
    Assert-True (Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary $run) -OtherGpuBusy 0.0 -Rule $v2).admitted 'other work of 0.058 on the measuring processor is admitted by v2'
    Assert-True (-not (Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary $run) -OtherGpuBusy 0.0 -Rule $v1).admitted) 'and still refused by v1'
    $run.interference.otherOnMeasuring = 0.11
    Assert-True (-not (Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary $run) -OtherGpuBusy 0.0 -Rule $v2).admitted) 'above 0.10 it is refused'
    Assert-Equal $true (Test-FrameTimingRunAdmission -Summary (Get-FrameTimingRunSummary (New-TimingRun)) -OtherGpuBusy 0.0 -Rule $v2).indicators.coreIsolated 'core isolation recorded'
    $steady = Test-FrameTimingSessionSteadiness -Summaries @(2.0, 3.5, 2.5, 3.9 | ForEach-Object { Get-FrameTimingRunSummary (New-TimingRun -OtherProcessors $_) }) -Rule $v2
    Assert-True ($steady.admitted -and [Math]::Abs($steady.spread - 1.9) -lt 1e-9) 'a spread of 1.9 logical processors is steady'
    $unsteady = Test-FrameTimingSessionSteadiness -Summaries @(2.0, 6.5, 2.5, 3.0 | ForEach-Object { Get-FrameTimingRunSummary (New-TimingRun -OtherProcessors $_) }) -Rule $v2
    Assert-True (-not $unsteady.admitted -and $unsteady.reason.Contains('4.50')) 'a run under 4.5 more busy logical processors makes the session unsteady'
    Assert-True (Test-FrameTimingSessionSteadiness -Summaries @(2.0, 9.0 | ForEach-Object { Get-FrameTimingRunSummary (New-TimingRun -OtherProcessors $_) }) -Rule $v1).admitted 'v1 has no steadiness clause'
}

Invoke-TestCase 'admission: the preflight leaves out the study''s own processes (observer, runner, load children)' {
    $process = { param($id, $ticks) [ordered]@{ pid = $id; status = 'observed'; cpuTicks = $ticks } }
    $samples = @(
        [ordered]@{ idle100ns = 0; kernelIncludingIdle100ns = 0; user100ns = 0; qpcStart = 0; qpcEnd = 0; qpcFrequency = 10000000; observerPid = 77
            processes = @((& $process 77 0), (& $process 88 0), (& $process 99 0)) },
        [ordered]@{ idle100ns = 6000000; kernelIncludingIdle100ns = 10000000; user100ns = 0; qpcStart = 10000000; qpcEnd = 10000000; qpcFrequency = 10000000; observerPid = 77
            processes = @((& $process 77 3000000), (& $process 88 5000000), (& $process 99 1000000)) })
    # 40% of 32 = 12.8 busy logical processors over one second; the observer used 0.3 and the load child 0.5 of them.
    $result = Test-FrameTimingPreflight -Samples $samples -Rule (Get-FrameTimingAdmissionRule) -LogicalProcessors 32 -ExcludeProcessIds @(88)
    Assert-True ([Math]::Abs($result.ownProcessors - 0.8) -lt 1e-9) 'own processes measured'
    Assert-True ([Math]::Abs($result.busyProcessors - 12.0) -lt 1e-9) 'only other processes count'
    Assert-Equal 0.0 (Get-FrameTimingProcessProcessors -Samples @($samples[0]) -ProcessIds @(77)) 'one sample has no window'
}

Invoke-TestCase 'admission: the preflight caps other load in logical processors; the legacy rule caps 10% of the host' {
    $samples = @([ordered]@{ idle100ns = 0; kernelIncludingIdle100ns = 0; user100ns = 0 }, [ordered]@{ idle100ns = 800; kernelIncludingIdle100ns = 1000; user100ns = 0 })
    $interim = Test-FrameTimingPreflight -Samples $samples -Rule (Get-FrameTimingAdmissionRule) -LogicalProcessors 32
    Assert-True $interim.admitted '20% of 32 logical processors (6.4) is under the cap of 8'
    $busier = @($samples[0], [ordered]@{ idle100ns = 500; kernelIncludingIdle100ns = 1000; user100ns = 0 })
    Assert-True (-not (Test-FrameTimingPreflight -Samples $busier -Rule (Get-FrameTimingAdmissionRule) -LogicalProcessors 8).admitted) '50% of 8 (4) is over the cap of 2'
    $legacy = Test-FrameTimingPreflight -Samples $samples -Rule (Get-FrameTimingAdmissionRule -Name 'host-busy-10pct-v1') -LogicalProcessors 32
    Assert-True (-not $legacy.admitted -and $legacy.reason.Contains('20.0%')) 'the legacy rule refuses 20% of the host'
    Assert-Equal 1.0 (Get-FrameTimingOtherProcessorCap -Rule (Get-FrameTimingAdmissionRule) -LogicalProcessors 2) 'the cap is at least one logical processor'
}

Invoke-TestCase 'admission: other-process GPU busy from per-instance counter deltas on the measured adapter' {
    $luid = '0x00000000_0x0001bdd0'
    $before = @{ 'pid_10_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 1000; 'pid_11_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 2000
        'pid_10_luid_0x00000000_0x0001bdd0_phys_0_eng_3_engtype_copy' = 0; 'pid_12_luid_0x00000000_0x00099999_phys_0_eng_0_engtype_3d' = 0
        'pid_13_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 9000000 }
    $after = @{ 'pid_10_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 1001000; 'pid_11_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 1002000
        'pid_10_luid_0x00000000_0x0001bdd0_phys_0_eng_3_engtype_copy' = 500000; 'pid_12_luid_0x00000000_0x00099999_phys_0_eng_0_engtype_3d' = 50000000
        'pid_13_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 100000; 'pid_14_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d' = 90000000 }
    # Engine 0: 0.1 s + 0.1 s, plus 0.01 s from the reset counter of pid 13; pid 14 appeared (left out); the other adapter is left out.
    Assert-True ([Math]::Abs((Get-FrameTimingOtherGpuBusy -Before $before -After $after -Luid $luid -Seconds 1.0) - 0.21) -lt 1e-9) 'busiest engine share'
    Assert-True ([Math]::Abs((Get-FrameTimingOtherGpuBusy -Before $before -After $after -Luid $luid -Seconds 1.0 -ExcludeProcessIds 10) - 0.11) -lt 1e-9) 'an excluded process is left out'
    Assert-Equal $null (Get-FrameTimingOtherGpuBusy -Before @{} -After $after -Luid $luid -Seconds 1.0) 'no snapshot, no figure'
}

Invoke-TestCase 'load sensitivity: placement away from the measuring core or on its sibling, and the admissible level' {
    $topology = @(for ($index = 0; $index -lt 16; $index++) { [ordered]@{ index = $index; core = [int][Math]::Floor($index / 2); efficiencyClass = 0; lastLevelCache = 0 } })
    $arms = Get-FrameTimingLoadPlan -Processors $topology -MeasuringProcessor 14 -Levels @(0, 2, 4, 8, 12) -Sibling
    Assert-Equal '0|2|4|8|12|sibling' (($arms | ForEach-Object { $_.name }) -join '|') 'arms in order'
    Assert-Equal '0,2,4,6' (($arms[2].processors) -join ',') 'one thread per other core, farthest first'
    Assert-Equal 12 $arms[4].processors.Count 'twelve processors'
    Assert-True (-not (@($arms | Where-Object { $_.placement -eq 'away' } | ForEach-Object { $_.processors }) | Where-Object { $_ -in 14, 15 })) 'nothing placed on the measuring core'
    Assert-Equal '15' (($arms[5].processors) -join ',') 'the sibling arm loads the measuring processor''s sibling'
    Assert-Throws { Get-FrameTimingLoadPlan -Processors $topology -MeasuringProcessor 14 -Levels @(15) } 'more load than processors away from the core'
    $session = { param([string] $Arm, [int] $Level, [string] $Placement, [double] $Loaded)
        $s = New-Session @(1.0, $Loaded, $Loaded, 1.0); $s['arm'] = $Arm; $s['level'] = $Level; $s['placement'] = $Placement; $s }
    $sessions = foreach ($i in 1..5) {
        & $session '0' 0 'away' 1.0
        & $session '2' 2 'away' 1.002
        & $session '4' 4 'away' 1.005
        & $session '8' 8 'away' 1.08
        & $session '12' 12 'away' 1.001
        & $session 'sibling' 1 'sibling' 1.2
    }
    $result = Get-FrameTimingLoadSensitivity -Sessions @($sessions)
    Assert-Equal 4 $result.admissibleLevel 'the largest level below the first non-equivalent one'
    Assert-Equal $false $result.siblingEquivalent 'a 20% slower sibling arm is not equivalent'
}

Invoke-TestCase 'load sensitivity: the load process is this tool''s own child and stops with it' {
    $script = Join-Path ([IO.Path]::GetTempPath()) ('fake-load-' + [guid]::NewGuid().ToString('N') + '.ps1')
    Set-Content -LiteralPath $script -Value 'param($Flag, $List) "busy load running on $List"; Start-Sleep -Seconds 120'
    try {
        $process = Start-FrameTimingLoad -Executable (Join-Path $PSHOME 'pwsh.exe') -PrefixArguments @('-NoProfile', '-File', $script) -Processors @(3, 5)
        $id = $process.Id
        Assert-True (-not $process.HasExited) 'the load runs'
        Stop-FrameTimingLoad -Process $process
        Assert-True ($null -eq (Get-Process -Id $id -ErrorAction SilentlyContinue)) 'the load process ended'
    } finally { Remove-Item -LiteralPath $script -ErrorAction SilentlyContinue }
}

# A fake powercfg: the active scheme lives in a file; 'absent' hides High Performance, 'stuck' ignores /setactive.
$script:PowerState = $null
function global:Invoke-FakePowerCfg {
    $state = $script:PowerState
    $state.calls.Add(($args -join ' '))
    $global:LASTEXITCODE = 0
    switch ($args[0]) {
        '/getactivescheme' { "Power Scheme GUID: $($state.active)  (Synthetic)" }
        '/list' { 'Existing Power Schemes'; 'Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced) *'; if (-not $state.absent) { 'Power Scheme GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c  (High performance)' } }
        '/setactive' { if ($state.stuck) { $global:LASTEXITCODE = 1 } else { $state.active = $args[1] } }
    }
}
function New-PowerState([switch] $Absent, [switch] $Stuck, [string] $Active = '381b4222-f694-41f0-9685-ff5bb260df2e') {
    $script:PowerState = [pscustomobject]@{ active = $Active; absent = [bool]$Absent; stuck = [bool]$Stuck; calls = [Collections.Generic.List[string]]::new() }
    return $script:PowerState
}

Invoke-TestCase 'power plan: High Performance is entered and the original scheme restored' {
    $state = New-PowerState
    $record = Enter-FrameTimingPowerPlan -PowerCfg 'Invoke-FakePowerCfg'
    Assert-Equal '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c' $state.active 'High Performance active while measuring'
    Assert-True $record.changed 'the record says the scheme changed'
    Exit-FrameTimingPowerPlan -Record $record -PowerCfg 'Invoke-FakePowerCfg'
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' $state.active 'original scheme restored'
    Assert-True $record.restored 'restoration verified'
}

Invoke-TestCase 'power plan: Keep and an already active High Performance scheme change nothing' {
    $state = New-PowerState
    $record = Enter-FrameTimingPowerPlan -Mode Keep -PowerCfg 'Invoke-FakePowerCfg'
    Exit-FrameTimingPowerPlan -Record $record -PowerCfg 'Invoke-FakePowerCfg'
    Assert-Equal 0 @($state.calls | Where-Object { $_ -like '/setactive*' }).Count 'Keep never sets a scheme'
    $state = New-PowerState -Active '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
    $record = Enter-FrameTimingPowerPlan -PowerCfg 'Invoke-FakePowerCfg'
    Assert-True (-not $record.changed) 'already active High Performance is not changed'
    Assert-Equal 0 @($state.calls | Where-Object { $_ -like '/setactive*' }).Count 'no scheme change'
}

Invoke-TestCase 'power plan: an absent or refused High Performance scheme is never created or elevated' {
    $state = New-PowerState -Absent
    Assert-Throws { Enter-FrameTimingPowerPlan -PowerCfg 'Invoke-FakePowerCfg' } 'absent High Performance scheme'
    Assert-Equal 0 @($state.calls | Where-Object { $_ -like '/setactive*' -or $_ -like '/duplicatescheme*' }).Count 'absent scheme neither created nor activated'
    $state = New-PowerState -Stuck
    Assert-Throws { Enter-FrameTimingPowerPlan -PowerCfg 'Invoke-FakePowerCfg' } 'refused activation'
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' $state.active 'original scheme unchanged after refused activation'
}

# --- Runner ---------------------------------------------------------------------------------------------------------
function New-TimingRunnerFixture([string] $Root) {
    Copy-Item -LiteralPath (Join-Path $repository 'performance-timing.ps1') -Destination (Join-Path $Root 'performance-timing.ps1')
    New-Item -ItemType Directory -Force -Path (Join-Path $Root 'Tools') | Out-Null
    foreach ($module in @('FrameTiming.psm1','PerformanceComparison.psm1')) { Copy-Item -LiteralPath (Join-Path $repository "Tools/$module") -Destination (Join-Path $Root "Tools/$module") }
    Set-FixtureFile $Root ".build/$fixturePlatform/Release/DxUi.EmbeddedTests.exe" 'synthetic candidate bytes; a function shim runs instead'
    Set-FixtureFile $Root 'baseline/DxUi.EmbeddedTests.exe' 'synthetic baseline bytes; a function shim runs instead'
    foreach ($path in @('Tests/Embedded/Embedded.Tests.FrameTiming.h','Tests/Embedded/Embedded.Tests.Embedded.cpp','Samples/ComplexUi/ComplexUiScene.h','Samples/EmbeddedControls/GraphicsFixture.h')) {
        Set-FixtureFile $Root $path "synthetic timing input $path"
    }
    Set-FixtureFile $Root 'mode.txt' 'valid'
    Set-FixtureFile $Root 'host-busy.txt' '2'
    Set-FixtureFile $Root 'power.txt' '381b4222-f694-41f0-9685-ff5bb260df2e'
    Set-FixtureFile $Root 'Tools/PerformancePolicy.psm1' @'
Set-StrictMode -Version Latest
function Get-PerformanceMeasurementState {
    param($Root, $Platform, $Configuration)
    [IO.File]::AppendAllText((Join-Path $Root 'calls.txt'), "state:$([IO.File]::ReadAllText((Join-Path $Root 'power.txt')))`n")
    return [ordered]@{ platform=$Platform; configuration=$Configuration; sourceCommit=('a' * 40); identityStatus='verifiable' }
}
function Assert-PerformanceBuildAttestation { param($Root, $Platform, $Configuration, $State) [IO.File]::AppendAllText((Join-Path $Root 'calls.txt'), "attest`n") }
function Assert-PerformanceMeasurementState { param($Before, $After) [IO.File]::AppendAllText((Join-Path $global:TimingFixtureRoot 'calls.txt'), "compare`n") }
Export-ModuleMember -Function Get-PerformanceMeasurementState, Assert-PerformanceBuildAttestation, Assert-PerformanceMeasurementState
'@
    # The fake observer writes counters whose busy share is host-busy.txt percent, then waits for the stop file.
    Set-FixtureFile $Root 'Tools/Watch-BenchmarkHost.ps1' @'
param([string] $OutputPath, [int] $IntervalMilliseconds = 1000)
$busy = [double][IO.File]::ReadAllText((Join-Path $PSScriptRoot '../host-busy.txt'))
# host-busy-next.txt, when present, is the busy share every later observer reads (a burst of desktop work that passes).
$next = Join-Path $PSScriptRoot '../host-busy-next.txt'
if (Test-Path -LiteralPath $next) { Move-Item -LiteralPath $next -Destination (Join-Path $PSScriptRoot '../host-busy.txt') -Force }
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '../calls.txt'), "observer:start`n")
$sequence = 0
while ($true) {
    $line = ConvertTo-Json -Compress -InputObject ([ordered]@{ sequence=$sequence; idle100ns=[long]($sequence * (100 - $busy) * 10); kernelIncludingIdle100ns=($sequence * 1000); user100ns=0 })
    [IO.File]::AppendAllText($OutputPath, $line + "`n")
    if ($sequence -eq 0) { [IO.File]::WriteAllText($OutputPath + '.ready', 'ready') }
    $sequence++
    if (Test-Path -LiteralPath ($OutputPath + '.stop')) { break }
    Start-Sleep -Milliseconds 100
}
[IO.File]::WriteAllText($OutputPath + '.completion.json', (ConvertTo-Json -InputObject ([ordered]@{ reason='stop-requested'; samples=$sequence })))
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '../calls.txt'), "observer:stop`n")
'@
    Set-FixtureFile $Root 'build.ps1' @'
param($Platform, $Configuration)
[IO.File]::AppendAllText((Join-Path $PSScriptRoot 'calls.txt'), "build`n")
$global:LASTEXITCODE = 0
'@
    $runTemplate = ConvertTo-Json -Depth 6 -Compress -InputObject (New-TimingRun -Frames 1000 -Seconds 2.1)
    Set-FixtureFile $Root 'run-template.json' $runTemplate
    Set-FixtureFile $Root 'Invoke-Runner.ps1' @'
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$global:TimingFixtureRoot = $PSScriptRoot
$arguments = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'arguments.json')) | ConvertFrom-Json -AsHashtable
$platform = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }
# A fake "GPU Engine" counter: another process's 3D engine on the measured adapter advances by 100 s per sample in
# gpu-busy mode, never otherwise.
Set-Item -Path 'Function:global:Get-Counter' -Value {
    $root = $global:TimingFixtureRoot
    $statePath = Join-Path $root 'gpu-samples.txt'
    $count = if (Test-Path -LiteralPath $statePath) { [int][IO.File]::ReadAllText($statePath) } else { 0 }
    [IO.File]::WriteAllText($statePath, [string]($count + 1))
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "gpu-counter`n")
    $busy = [IO.File]::ReadAllText((Join-Path $root 'mode.txt')) -eq 'gpu-busy'
    [pscustomobject]@{ CounterSamples = @(
        [pscustomobject]@{ InstanceName = 'pid_4242_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d'; RawValue = [long]$(if ($busy) { $count * 1000000000 } else { 5000 }) },
        [pscustomobject]@{ InstanceName = 'pid_4242_luid_0x00000000_0x0009ffff_phys_0_eng_0_engtype_3d'; RawValue = [long]($count * 1000000000) },
        [pscustomobject]@{ InstanceName = 'pid_4243_luid_0x00000000_0x0001bdd0_phys_0_eng_0_engtype_3d'; RawValue = [uint64]18446744072192316715 }) }
}
Set-Item -Path 'Function:global:powercfg.exe' -Value {
    $root = $global:TimingFixtureRoot
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "powercfg:$($args -join ' ')`n")
    $global:LASTEXITCODE = 0
    switch ($args[0]) {
        '/getactivescheme' { "Power Scheme GUID: $([IO.File]::ReadAllText((Join-Path $root 'power.txt')))  (Synthetic)" }
        '/list' { 'Power Scheme GUID: 381b4222-f694-41f0-9685-ff5bb260df2e  (Balanced)'; 'Power Scheme GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c  (High performance)' }
        '/setactive' { [IO.File]::WriteAllText((Join-Path $root 'power.txt'), $args[1]) }
    }
}
$native = {
    param($Flag, $Output, $Fixture, $Phase, $Adapter, $Pin, $Frames, $Milliseconds)
    $root = $global:TimingFixtureRoot
    if ($Flag -eq '--fixture-catalog') {
        # The catalog the runner reads to skip an interactive-only fixture: FX4 is one, FX5 is not.
        [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "catalog`n")
        [IO.File]::WriteAllText($Output, '{"schema":1,"fixtures":[{"id":"FX4","built":true,"interactiveOnly":true},{"id":"FX5","built":true,"interactiveOnly":false}]}')
        $global:LASTEXITCODE = 0
        return
    }
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "native:${Flag}:${Fixture}:${Phase}:${Adapter}:${Pin}:${Frames}:${Milliseconds}`n")
    $count = @([IO.File]::ReadAllLines((Join-Path $root 'calls.txt')) | Where-Object { $_.StartsWith('native:') }).Count
    $global:LASTEXITCODE = 0
    if ([IO.File]::ReadAllText((Join-Path $root 'mode.txt')) -eq 'fail-third' -and $count -eq 3) { $global:LASTEXITCODE = 7; return }
    if ([IO.File]::ReadAllText((Join-Path $root 'mode.txt')) -eq 'fail-ninth' -and $count -eq 9) { $global:LASTEXITCODE = 9; return }
    $run = [IO.File]::ReadAllText((Join-Path $root 'run-template.json')) | ConvertFrom-Json -AsHashtable
    $run['minimumFrames'] = [int]$Frames; $run['minimumSeconds'] = [double]$Milliseconds / 1000.0
    $mode = [IO.File]::ReadAllText((Join-Path $root 'mode.txt'))
    if ($mode -eq 'sibling-loaded' -or ($mode -eq 'reject-first-session' -and $count -le 4)) { $run['interference']['siblingBusy'] = 0.6 }
    if ($Adapter -eq 'Warp') { $run['renderer'] = 'WARP'; $run['adapter']['software'] = $true }
    [IO.File]::WriteAllText($Output, (ConvertTo-Json -Depth 6 -Compress -InputObject $run))
}
foreach ($path in @((Join-Path $PSScriptRoot ".build/$platform/Release/DxUi.EmbeddedTests.exe"), (Join-Path $PSScriptRoot 'baseline/DxUi.EmbeddedTests.exe'))) {
    Set-Item -Path ('Function:global:' + $path) -Value $native
}
if ($arguments.Contains('BaselineExecutable')) { $arguments['BaselineExecutable'] = Join-Path $PSScriptRoot $arguments['BaselineExecutable'] }
try { & (Join-Path $PSScriptRoot 'performance-timing.ps1') @arguments; exit 0 }
catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
'@
}
function Invoke-TimingRunner([string] $Root, [hashtable] $Arguments) {
    if (-not $Arguments.ContainsKey('OutputDirectory')) { $Arguments['OutputDirectory'] = Join-Path $Root 'out' }
    $Arguments['Platform'] = $fixturePlatform
    $Arguments['PreflightSeconds'] = 1
    Set-FixtureJson $Root 'arguments.json' $Arguments
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME $(if ($IsWindows) { 'pwsh.exe' } else { 'pwsh' })))
    $start.UseShellExecute = $false
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile','-File',(Join-Path $Root 'Invoke-Runner.ps1'))) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        return [pscustomobject]@{ Exit = $process.ExitCode; Text = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult() }
    } finally { $process.Dispose() }
}
function Get-Calls([string] $Root) {
    $path = Join-Path $Root 'calls.txt'
    if (-not (Test-Path -LiteralPath $path)) { return @() }
    return @([IO.File]::ReadAllLines($path))
}

Invoke-FixtureCase 'runner: a smoke session keeps the power scheme, records the preflight and labels the receipt' {
    param($root)
    New-TimingRunnerFixture $root
    $run = Invoke-TimingRunner $root @{ Purpose = 'Smoke'; Adapter = 'Warp'; Sessions = 1; PowerPlan = 'Keep'; IdlePreflight = 'Record'; SkipBuild = $true }
    Assert-Equal 0 $run.Exit "smoke run failed: $($run.Text)"
    $calls = Get-Calls $root
    Assert-Equal 0 @($calls | Where-Object { $_ -like 'powercfg:/setactive*' }).Count 'Keep changes no scheme'
    Assert-Equal 4 @($calls | Where-Object { $_ -eq "native:--timing-method:FX5:Dirty:Warp:auto:1000:2000" }).Count 'four A/B/B/A runs with the method arguments'
    Assert-Equal 1 @($calls | Where-Object { $_ -eq 'observer:stop' }).Count 'observer stopped'
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-Equal 'Smoke' $receipt.purpose 'purpose'
    Assert-True $receipt.classification.StartsWith('smoke run') 'smoke label'
    Assert-True (-not $receipt.Contains('calibration') -and -not $receipt.Contains('comparison')) 'a smoke run has no calibration or verdict'
    Assert-Equal 'ABBA' $receipt.sessions[0].order 'interleaved order'
    Assert-True ([Math]::Abs([double]$receipt.sessions[0].idlePreflight.busyPercent - 2.0) -lt 1e-6) 'preflight busy share recorded'
}

Invoke-FixtureCase 'runner: an interactive-only fixture is never timed, says so and succeeds' {
    param($root)
    New-TimingRunnerFixture $root
    $run = Invoke-TimingRunner $root @{ Purpose = 'Smoke'; Fixture = 'FX4'; Adapter = 'Warp'; Sessions = 1; PowerPlan = 'Keep'; IdlePreflight = 'Record'; SkipBuild = $true }
    Assert-Equal 0 $run.Exit "FX4 smoke run failed: $($run.Text)"
    Assert-True ($run.Text.Contains('FX4: interactive, not run')) 'the runner says FX4 is not run'
    $calls = Get-Calls $root
    Assert-Equal 0 @($calls | Where-Object { $_.StartsWith('native:') -or $_ -like 'powercfg:/setactive*' }).Count 'no timing run and no power scheme change'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'out'))) 'no receipt directory is left'
}

Invoke-FixtureCase 'runner: calibration activates High Performance, measures it and restores the original scheme' {
    param($root)
    New-TimingRunnerFixture $root
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-Equal 0 $run.Exit "calibration failed: $($run.Text)"
    Assert-Equal 10 @(Get-Calls $root | Where-Object { $_ -eq 'gpu-counter' }).Count 'one GPU engine snapshot before and after every session'
    $calls = Get-Calls $root
    Assert-Contains $calls 'powercfg:/setactive 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c' 'High Performance activated'
    Assert-Equal 'powercfg:/setactive 381b4222-f694-41f0-9685-ff5bb260df2e' @($calls | Where-Object { $_ -like 'powercfg:/setactive*' })[-1] 'original scheme restored last'
    Assert-Contains $calls 'state:8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c' 'identity captured under the measured scheme'
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' ([IO.File]::ReadAllText((Join-Path $root 'power.txt'))) 'scheme restored'
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-Equal 5 ([int]$receipt.calibration.sessions) 'five calibration sessions'
    Assert-Equal 6 ([int]$receipt.calibration.judgedMetrics) 'identical synthetic runs judge every metric'
    Assert-True $receipt.powerPlan.restored 'receipt records the verified restoration'
    Assert-Equal 20 @(Get-ChildItem -LiteralPath (Join-Path $root '.build/reports/frame-timing/raw') -Recurse -Filter 'session-*-slot-*.json').Count 'every raw run report retained'
    Assert-Equal 'README.md|receipt.json' ((@(Get-ChildItem -LiteralPath (Join-Path $root 'out') | ForEach-Object Name | Sort-Object)) -join '|') 'the receipt directory holds only the receipt and its explanation'
    Assert-Equal 'DxUi' $receipt.workloadOwner 'library-owned workload'
    Assert-True $receipt.fixture.StartsWith('dxui-') 'library fixture identity'
    Assert-Equal 6 @($receipt.benchmarkInputs.Keys).Count 'timing input hashes'
    Assert-True (-not $receipt.sessions[0].runs[0].Contains('raw')) 'the retained receipt embeds no raw report'
    Assert-Equal 'dxui-frame-timing-retained-v1' $receipt.retainedForm 'retained form'
    $raw = $receipt.sessions[0].runs[0].report
    Assert-Equal ([long](Get-Item -LiteralPath $raw).Length) ([long]$receipt.sessions[0].runs[0].reportSizeBytes) 'raw report size recorded'
    Assert-Equal (Get-FileHash -LiteralPath $raw -Algorithm SHA256).Hash $receipt.sessions[0].runs[0].reportSha256 'raw report hash recorded'
    Assert-True (Test-Path -LiteralPath $receipt.fullReceipt.path) 'the full receipt stays in the build tree'
    # The converter, run on the full receipt, reproduces the retained receipt the runner wrote.
    $converted = Join-Path $root 'converted.json'
    & (Join-Path $repository 'Tools/Convert-FrameTimingReceipt.ps1') -Path $receipt.fullReceipt.path -OutputPath $converted | Out-Null
    $again = Get-FixtureJson $root 'converted.json'
    $receipt.Remove('fullReceipt')
    Assert-Equal (ConvertTo-Json -InputObject $receipt -Depth 40 -Compress) (ConvertTo-Json -InputObject $again -Depth 40 -Compress) 'conversion of the full receipt equals the runner''s retained receipt'
    Assert-True ([IO.File]::ReadAllText((Join-Path $root 'out/README.md')).Contains('same-binary calibration')) 'generated explanation'
}

Invoke-FixtureCase 'runner: a session rejected for its sibling load is repeated and recorded' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'mode.txt' 'reject-first-session'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-Equal 0 $run.Exit "calibration with one rejected session failed: $($run.Text)"
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-Equal 1 @($receipt.rejectedSessions).Count 'one rejected attempt recorded'
    Assert-True ($receipt.rejectedSessions[0].reasons[0].Contains('sibling')) 'the rejection names the sibling load'
    Assert-Equal 5 @($receipt.sessions).Count 'five admitted sessions'
    Assert-Equal 2 ([int]$receipt.sessions[0].attempt) 'the first session was admitted on its second attempt'
    Assert-Equal 'interference-interim-v2' $receipt.admission.rule.id 'the interim rule admitted the receipt'
    Assert-True $receipt.admission.interim 'the receipt says the rule is interim'
    Assert-Equal 24 @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count 'the rejected session was measured again'
}

Invoke-FixtureCase 'runner: a sibling loaded in every attempt refuses the study and restores the scheme' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'mode.txt' 'sibling-loaded'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; SessionAttempts = 2 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('rejected 2 times')) "a loaded sibling was admitted: $($run.Text)"
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'out/receipt.json'))) 'no receipt'
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' ([IO.File]::ReadAllText((Join-Path $root 'power.txt'))) 'scheme restored'
}

Invoke-FixtureCase 'runner: another process busy on the measured GPU rejects the session; the legacy rule ignores it' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'mode.txt' 'gpu-busy'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; SessionAttempts = 1 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('GPU')) "a busy GPU was admitted: $($run.Text)"
    Remove-Item -LiteralPath (Join-Path $root 'calls.txt'), (Join-Path $root 'gpu-samples.txt') -ErrorAction SilentlyContinue
    $legacy = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; Admission = 'Legacy10'; OutputDirectory = (Join-Path $root 'legacy') }
    Assert-Equal 0 $legacy.Exit "the legacy rule refused: $($legacy.Text)"
    $receipt = Get-FixtureJson $root 'legacy/receipt.json'
    Assert-Equal 'host-busy-10pct-v1' $receipt.admission.rule.id 'legacy rule recorded'
    Assert-True ($receipt.sessions[0].otherGpuBusy -gt 0.1) 'the GPU load is still recorded'
}

Invoke-FixtureCase 'runner: a refused preflight waits and retries; a burst that passes does not end the study' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'host-busy.txt' '40'
    Set-FixtureFile $root 'host-busy-next.txt' '2'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; PreflightWaitSeconds = 0 }
    Assert-Equal 0 $run.Exit "a passing burst ended the study: $($run.Text)"
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-Equal 1 @($receipt.sessions[0].idlePreflight.refusals).Count 'the refused attempt is recorded'
    Assert-Equal 2 ([int]$receipt.sessions[0].idlePreflight.attempt) 'the second preflight admitted the session'
}

Invoke-FixtureCase 'runner: completed sessions are checkpointed and the same command resumes the study' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'mode.txt' 'fail-ninth'
    $first = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-True ($first.Exit -ne 0 -and $first.Text.Contains('exit code 9')) "the failing run was accepted: $($first.Text)"
    $checkpoint = Get-ChildItem -LiteralPath (Join-Path $root '.build/reports/frame-timing/checkpoints') -Filter '*.checkpoint.json'
    Assert-Equal 1 @($checkpoint).Count 'a checkpoint was kept'
    Assert-Equal 2 @((Get-Content -Raw -LiteralPath $checkpoint[0].FullName | ConvertFrom-Json -AsHashtable -Depth 60).sessions).Count 'two completed sessions checkpointed'
    Set-FixtureFile $root 'mode.txt' 'valid'
    $second = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-Equal 0 $second.Exit "resume failed: $($second.Text)"
    Assert-True $second.Text.Contains('Resuming after 2 checkpointed sessions') 'the study resumed'
    Assert-Equal 21 @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count 'nine calls before the failure, twelve to finish'
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-Equal 5 @($receipt.sessions).Count 'five sessions in the receipt'
    Assert-True $receipt.admission.resumed 'the receipt says it was resumed'
    Assert-True (-not (Test-Path -LiteralPath $checkpoint[0].FullName)) 'the checkpoint is removed when the study completes'
    $other = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; OutputDirectory = (Join-Path $root 'out2') }
    Assert-Equal 0 $other.Exit "an unrelated study failed: $($other.Text)"
}

Invoke-FixtureCase 'runner: a failed run still restores the scheme and stops the observer' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'mode.txt' 'fail-third'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('exit code 7')) "failed run accepted: $($run.Text)"
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' ([IO.File]::ReadAllText((Join-Path $root 'power.txt'))) 'scheme restored after failure'
    Assert-Equal 1 @(Get-Calls $root | Where-Object { $_ -eq 'observer:stop' }).Count 'observer stopped after failure'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'out/receipt.json'))) 'no receipt after failure'
}

Invoke-FixtureCase 'runner: a busy host refuses the session before any measurement and restores the scheme' {
    param($root)
    New-TimingRunnerFixture $root
    Set-FixtureFile $root 'host-busy.txt' '40'
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true; PreflightAttempts = 2; PreflightWaitSeconds = 0 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('refused session 1 2 times')) "busy host accepted: $($run.Text)"
    Assert-Equal 0 @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count 'no run on a busy host'
    Assert-Equal '381b4222-f694-41f0-9685-ff5bb260df2e' ([IO.File]::ReadAllText((Join-Path $root 'power.txt'))) 'scheme restored after refusal'
}

Invoke-FixtureCase 'runner: weaker calibration and comparison admissions are refused before any machine change' {
    param($root)
    New-TimingRunnerFixture $root
    foreach ($case in @(
            @{ arguments = @{ Purpose = 'Calibration'; Adapter = 'Warp'; SkipBuild = $true }; text = 'hardware adapter' },
            @{ arguments = @{ Purpose = 'Calibration'; Sessions = 4; SkipBuild = $true }; text = 'five sessions' },
            @{ arguments = @{ Purpose = 'Calibration'; PowerPlan = 'Keep'; SkipBuild = $true }; text = 'High Performance' },
            @{ arguments = @{ Purpose = 'Calibration'; MinimumFrames = 500; SkipBuild = $true }; text = '1,000 frames' },
            @{ arguments = @{ Purpose = 'Comparison'; Sessions = 6; SkipBuild = $true }; text = '-BaselineExecutable' },
            @{ arguments = @{ Purpose = 'Comparison'; Sessions = 5; BaselineExecutable = 'baseline/DxUi.EmbeddedTests.exe'; SkipBuild = $true }; text = 'six sessions' })) {
        $run = Invoke-TimingRunner $root $case.arguments
        Assert-True ($run.Exit -ne 0 -and $run.Text.Contains($case.text)) "weaker admission accepted ($($case.text)): $($run.Text)"
    }
    Assert-Equal 0 @(Get-Calls $root).Count 'refusals reached no build, powercfg, observer or native work'
}

Invoke-FixtureCase 'runner: a comparison measures the baseline as A and this tree as B' {
    param($root)
    New-TimingRunnerFixture $root
    $run = Invoke-TimingRunner $root @{ Purpose = 'Comparison'; Sessions = 6; BaselineExecutable = 'baseline/DxUi.EmbeddedTests.exe'; SkipBuild = $true }
    Assert-Equal 0 $run.Exit "comparison failed: $($run.Text)"
    $receipt = Get-FixtureJson $root 'out/receipt.json'
    Assert-True ($receipt.executables.A.sha256 -cne $receipt.executables.B.sha256) 'two executables'
    Assert-Equal 6 @($receipt.comparison.metrics).Count 'six metric verdicts'
    Assert-Equal 'equivalent' (Get-Metric $receipt.comparison.metrics 'gpuFrameUsMedian').verdict 'identical synthetic timings are equivalent'
    Assert-Equal 'B' $receipt.sessions[0].runs[1].role 'second slot is B'
}

Complete-TestRun 'Frame timing method'
