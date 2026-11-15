# Frame timing method (Specs/Core/Core_PerformanceAndResources.md, "Frame timing method"): validation of the native
# per-frame reports, six judged metrics, the same-binary noise floor, equivalence against a declared margin and the
# minimum detectable effect. It supplies a method and calibration receipts to D-P1; it does not change the paired
# acceptance judge (Tools/PerformanceComparison.psm1), whose exact sign-flip test it reuses.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PerformanceComparison.psm1') -Function Get-ExactSignFlipPValue

$script:Method = 'dxui-frame-timing-v1'
$script:JudgedMinimumFrames = 1000
$script:JudgedMinimumSeconds = 2.0
$script:MinimumValidGpuShare = 0.95
$script:Alpha = 0.05
$script:HighPerformanceScheme = '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
# Declared equivalence margins: 3% for UI-thread CPU, 5% for GPU time.
$script:Metrics = @(
    [ordered]@{ name='prepareCyclesMedian'; series='prepareCycles'; statistic='median'; kind='cpu'; marginPercent=3.0 }
    [ordered]@{ name='compositeCyclesMedian'; series='compositeCycles'; statistic='median'; kind='cpu'; marginPercent=3.0 }
    [ordered]@{ name='uiThreadCyclesP95'; series='uiThreadCycles'; statistic='p95'; kind='cpu'; marginPercent=3.0 }
    [ordered]@{ name='submitUsMedian'; series='submitUs'; statistic='median'; kind='cpu'; marginPercent=3.0 }
    [ordered]@{ name='gpuFrameUsMedian'; series='gpuFrameUs'; statistic='median'; kind='gpu'; marginPercent=5.0 }
    [ordered]@{ name='gpuFrameUsP95'; series='gpuFrameUs'; statistic='p95'; kind='gpu'; marginPercent=5.0 }
)
# Two-sided 97.5% and one-sided 80% Student t quantiles for 1 to 30 degrees of freedom; normal quantiles beyond.
$script:T975 = @(12.706,4.303,3.182,2.776,2.571,2.447,2.365,2.306,2.262,2.228,2.201,2.179,2.160,2.145,2.131,2.120,2.110,2.101,2.093,2.086,2.080,2.074,2.069,2.064,2.060,2.056,2.052,2.048,2.045,2.042)
$script:T80 = @(1.376,1.061,0.978,0.941,0.920,0.906,0.896,0.889,0.883,0.879,0.876,0.873,0.870,0.868,0.866,0.865,0.863,0.862,0.861,0.860,0.859,0.858,0.858,0.857,0.856,0.856,0.855,0.855,0.854,0.854)

function Get-FrameTimingMethod { return $script:Method }
function Get-FrameTimingHighPerformanceScheme { return $script:HighPerformanceScheme }
function Get-FrameTimingMetrics {
    return @($script:Metrics | ForEach-Object { $copy = [ordered]@{}; foreach ($key in $_.Keys) { $copy[$key] = $_[$key] }; $copy })
}

function Get-FrameTimingPercentile {
    <# Nearest-rank percentile: the smallest sample with at least Quantile of the samples at or below it. #>
    param([Parameter(Mandatory)][double[]] $Values, [Parameter(Mandatory)][double] $Quantile)
    if ($Values.Length -eq 0) { throw 'A percentile needs at least one sample.' }
    $sorted = [double[]]$Values.Clone()
    [Array]::Sort($sorted)
    $rank = [int][Math]::Ceiling($Quantile * $sorted.Length)
    return $sorted[[Math]::Max(0, $rank - 1)]
}

function Get-FrameTimingMedian {
    <# The middle sample, or the mean of the middle two; the paired judge keeps its own median private. #>
    param([Parameter(Mandatory)][double[]] $Values)
    if ($Values.Length -eq 0) { throw 'A median needs at least one sample.' }
    $sorted = [double[]]$Values.Clone()
    [Array]::Sort($sorted)
    $middle = [int][Math]::Floor($sorted.Length / 2)
    if ($sorted.Length % 2) { return $sorted[$middle] }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2.0
}

function Assert-FrameTimingNumberArray([Collections.IDictionary] $Samples, [string] $Name, [int] $Count) {
    if (-not $Samples.Contains($Name) -or $Samples[$Name] -isnot [array] -or $Samples[$Name].Count -ne $Count) {
        throw "Invalid frame timing report: samples.$Name must hold one value per frame."
    }
    foreach ($value in $Samples[$Name]) {
        if ($null -eq $value -or -not [double]::IsFinite([double]$value) -or [double]$value -lt 0) { throw "Invalid frame timing report: samples.$Name holds a negative or non-finite value." }
    }
}

function Assert-FrameTimingRun {
    <# Validates one native report. A judged run (-Judged) also requires a hardware adapter, the judged run length and
       at least 95% of frames with a valid (non-disjoint) GPU result. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Run, [switch] $Judged)
    if ($Run['schema'] -ne 1 -or $Run['method'] -cne $script:Method) { throw 'Invalid frame timing report: unknown schema or method.' }
    foreach ($key in @('fixture','phase','renderer','adapter','pinning','frames','elapsedSeconds','minimumFrames','minimumSeconds','samples')) {
        if (-not $Run.Contains($key) -or $null -eq $Run[$key]) { throw "Invalid frame timing report: $key is absent." }
    }
    $frames = [int]$Run['frames']
    if ($frames -lt 1 -or $frames -lt [int]$Run['minimumFrames'] -or [double]$Run['elapsedSeconds'] -lt [double]$Run['minimumSeconds']) {
        throw 'Invalid frame timing report: the run ended before its minimum frames and minimum duration.'
    }
    if ($Run['gpuReadLatencyFrames'] -ne 1) { throw 'Invalid frame timing report: GPU results must be read exactly one frame late.' }
    $samples = $Run['samples']
    if ($samples -isnot [Collections.IDictionary]) { throw 'Invalid frame timing report: samples must be an object.' }
    foreach ($name in @('prepareCycles','compositeCycles','submitUs','gpuPrepareUs','gpuCompositeUs','gpuWaitUs','gpuValid','gpuReadAfterSubmitted')) {
        Assert-FrameTimingNumberArray $samples $name $frames
    }
    for ($frame = 0; $frame -lt $frames; $frame++) {
        $expected = if ($frame + 1 -lt $frames) { $frame + 2 } else { $frames }
        if ([int]$samples['gpuReadAfterSubmitted'][$frame] -ne $expected) { throw "Invalid frame timing report: frame $frame was not read one frame late." }
    }
    $valid = @($samples['gpuValid'] | Where-Object { [int]$_ -eq 1 }).Count
    if ($valid + [int]$Run['disjointFrames'] -ne $frames) { throw 'Invalid frame timing report: valid and disjoint GPU frames do not account for every frame.' }
    if ($Judged) {
        if ($Run['renderer'] -cne 'Hardware' -or $Run['adapter']['software']) { throw 'A judged frame timing run requires a hardware adapter; WARP is for counters and pixels only.' }
        if ([int]$Run['minimumFrames'] -lt $script:JudgedMinimumFrames -or [double]$Run['minimumSeconds'] -lt $script:JudgedMinimumSeconds) {
            throw "A judged frame timing run requires at least $script:JudgedMinimumFrames frames and $script:JudgedMinimumSeconds s."
        }
        if ($valid -lt $script:MinimumValidGpuShare * $frames) { throw 'A judged frame timing run requires at least 95% of frames with a valid GPU timestamp.' }
        # A dirty frame that prepares nothing times no work: a timed phase played past its declared steps (FX3's wheel at the
        # end of its list) would otherwise calibrate empty frames.
        if ($Run['phase'] -ceq 'dirty' -and (-not $Run.Contains('preparedFrames') -or [int]$Run['preparedFrames'] -ne $frames)) {
            throw "A judged dirty frame timing run requires every timed frame to prepare work; $($Run['preparedFrames']) of $frames did."
        }
    }
}

function Get-FrameTimingRunSummary {
    <# The six judged metrics of one run, plus advisory context. GPU statistics use valid frames only. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Run)
    $samples = $Run['samples']
    $frames = [int]$Run['frames']
    $series = @{
        prepareCycles = [double[]]@($samples['prepareCycles'])
        compositeCycles = [double[]]@($samples['compositeCycles'])
        submitUs = [double[]]@($samples['submitUs'])
        uiThreadCycles = [double[]]::new($frames)
    }
    $gpu = [Collections.Generic.List[double]]::new()
    for ($frame = 0; $frame -lt $frames; $frame++) {
        $series.uiThreadCycles[$frame] = [double]$samples['prepareCycles'][$frame] + [double]$samples['compositeCycles'][$frame]
        if ([int]$samples['gpuValid'][$frame] -eq 1) { $gpu.Add([double]$samples['gpuPrepareUs'][$frame] + [double]$samples['gpuCompositeUs'][$frame]) }
    }
    $series.gpuFrameUs = $gpu.ToArray()
    # The smallest step each clock resolves in this run. QueryThreadCycleTime advances in quanta (about 429 cycles on the
    # reference machine), so a statistic of a few quanta cannot show a change smaller than one quantum.
    $frequency = [double]$Run['timestampFrequency']
    $steps = [ordered]@{
        cycles = (Get-FrameTimingStep ([double[]]($series.prepareCycles + $series.compositeCycles)) 2.0)
        submitUs = (Get-FrameTimingStep $series.submitUs 1e-6)
        gpuUs = $(if ($frequency -gt 0) { 1e6 / $frequency } else { $null })
    }
    $summary = [ordered]@{}
    $resolution = [ordered]@{}
    foreach ($metric in $script:Metrics) {
        $values = $series[$metric.series]
        $value = if ($values.Length -eq 0) { $null } elseif ($metric.statistic -eq 'median') { (Get-FrameTimingMedian $values) } else { Get-FrameTimingPercentile $values 0.95 }
        $summary[$metric.name] = $value
        $step = $steps[$(switch ($metric.series) { 'submitUs' { 'submitUs' } 'gpuFrameUs' { 'gpuUs' } default { 'cycles' } })]
        $resolution[$metric.name] = if ($null -eq $value -or $null -eq $step -or [double]$value -le 0) { $null } else { 100.0 * $step / [double]$value }
    }
    $summary['frames'] = $frames
    $summary['elapsedSeconds'] = [double]$Run['elapsedSeconds']
    $summary['validGpuFrames'] = $gpu.Count
    $summary['gpuWaitUsMedian'] = (Get-FrameTimingMedian ([double[]]@($samples['gpuWaitUs'])))
    foreach ($key in @('preparedFrames','timedSteps','rebuilds')) { if ($Run.Contains($key)) { $summary[$key] = $Run[$key] } }
    $running = Get-FrameTimingRunningShare -Run $Run
    if ($null -ne $running) { $summary['runningShareMedian'] = $running.median; $summary['runningShareP05'] = $running.p05 }
    if ($Run.Contains('interference') -and $Run['interference'] -is [Collections.IDictionary]) {
        $interference = [ordered]@{}
        foreach ($key in @('logicalProcessors','ownProcessors','threadShare','measuringBusy','siblingBusy','otherOnMeasuring','otherProcessors')) {
            if ($Run['interference'].Contains($key)) { $interference[$key] = $Run['interference'][$key] }
        }
        $summary['interference'] = $interference
    }
    $summary['clockSteps'] = $steps
    $summary['resolutionPercent'] = $resolution
    return $summary
}

function Get-FrameTimingRunningShare {
    <# Median and 5th percentile of the measuring thread's per-frame running share (cycles over TSC ticks during
       preparation and composition): 1.0 when the thread ran the whole frame. Null when the run has no TSC record. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Run)
    if (-not $Run.Contains('samples') -or -not $Run['samples'].Contains('runningPermyriad')) { return $null }
    $values = [double[]]@($Run['samples']['runningPermyriad'] | ForEach-Object { [double]$_ / 10000.0 })
    if ($values.Length -eq 0 -or @($values | Where-Object { $_ -gt 0 }).Count -ne $values.Length) { return $null }
    return [ordered]@{ median = (Get-FrameTimingMedian $values); p05 = (Get-FrameTimingPercentile $values 0.05) }
}

function Get-FrameTimingStep {
    <# The smallest gap between adjacent distinct sample values that exceeds IgnoreBelow (rounding of a quantized clock
       produces gaps of a unit or two); null when fewer than two distinct values exist, so the step cannot be observed. #>
    param([Parameter(Mandatory)][AllowEmptyCollection()][double[]] $Values, [Parameter(Mandatory)][double] $IgnoreBelow)
    if ($Values.Length -lt 2) { return $null }
    $sorted = [double[]]$Values.Clone()
    [Array]::Sort($sorted)
    $step = [double]::PositiveInfinity
    for ($index = 1; $index -lt $sorted.Length; $index++) {
        $gap = $sorted[$index] - $sorted[$index - 1]
        if ($gap -gt $IgnoreBelow -and $gap -lt $step) { $step = $gap }
    }
    if ([double]::IsPositiveInfinity($step)) { return $null }
    return $step
}

function Get-FrameTimingSchedule {
    <# Interleaved A/B/B/A in every session: both roles see the same linear drift inside a session. #>
    param([Parameter(Mandatory)][ValidateRange(1,20)][int] $Sessions)
    return @(for ($session = 0; $session -lt $Sessions; $session++) { 'ABBA' })
}

function Get-FrameTimingSessionEffects {
    <# Per metric: the session effect ln(B/A) as the mean log of B runs minus the mean log of A runs, and both A/B pair
       ratios (A1 with B1, B2 with A2). A positive effect means B took longer. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Session)
    $runs = @($Session['runs'])
    if ($Session['order'] -cne 'ABBA' -or $runs.Count -ne 4) { throw 'A frame timing session holds exactly the four runs A, B, B, A.' }
    $effects = [ordered]@{}
    foreach ($metric in $script:Metrics) {
        $values = @($runs | ForEach-Object { $_['summary'][$metric.name] })
        if (@($values | Where-Object { $null -eq $_ -or [double]$_ -le 0 }).Count) { $effects[$metric.name] = $null; continue }
        $a1, $b1, $b2, $a2 = [double[]]$values
        $effects[$metric.name] = [ordered]@{
            effect = (([Math]::Log($b1) + [Math]::Log($b2)) - ([Math]::Log($a1) + [Math]::Log($a2))) / 2.0
            pairRatios = @(($b1 / $a1), ($b2 / $a2))
        }
    }
    return $effects
}

function Get-FrameTimingUpperSignFlipPValue {
    <# One-sided exact sign-flip p-value: the share of the 2^n sign assignments whose mean is at least the observed mean. #>
    param([Parameter(Mandatory)][double[]] $Effects)
    if ($Effects.Length -lt 1 -or $Effects.Length -gt 20) { throw 'Exact sign-flip inference requires 1 to 20 independent session effects.' }
    $observed = ($Effects | Measure-Object -Average).Average
    $assignments = [int64]1 -shl $Effects.Length
    $extreme = [int64]0
    for ([int64]$mask = 0; $mask -lt $assignments; $mask++) {
        $sum = 0.0
        for ($i = 0; $i -lt $Effects.Length; $i++) { $sum += if (($mask -band ([int64]1 -shl $i)) -ne 0) { $Effects[$i] } else { -$Effects[$i] } }
        if ($sum / $Effects.Length -ge $observed - 1e-12) { $extreme++ }
    }
    return [double]$extreme / [double]$assignments
}

function Test-FrameTimingEquivalence {
    <# Two one-sided exact sign-flip tests against +/- MarginPercent: equivalent when both reject at alpha 0.05. With n
       sessions the smallest attainable one-sided p is 2^-n, so fewer than five sessions can never establish equivalence. #>
    param([Parameter(Mandatory)][double[]] $Effects, [Parameter(Mandatory)][double] $MarginPercent)
    $upper = [Math]::Log(1.0 + $MarginPercent / 100.0)
    $lower = [Math]::Log(1.0 - $MarginPercent / 100.0)
    # H0 effect <= lower: test that (effect - lower) has a positive mean. H0 effect >= upper: (upper - effect) positive.
    $pLower = Get-FrameTimingUpperSignFlipPValue ([double[]]@($Effects | ForEach-Object { $_ - $lower }))
    $pUpper = Get-FrameTimingUpperSignFlipPValue ([double[]]@($Effects | ForEach-Object { $upper - $_ }))
    return [ordered]@{
        marginPercent = $MarginPercent; pLower = $pLower; pUpper = $pUpper
        minimumAttainableP = [Math]::Pow(2.0, -$Effects.Length)
        equivalent = [bool]([Math]::Max($pLower, $pUpper) -lt $script:Alpha)
    }
}

function Get-FrameTimingMinimumDetectableEffect {
    <# The smallest true change, in percent, that n sessions with this effect spread detect two-sided at alpha 0.05 with
       80% power: (t(0.975, n-1) + t(0.80, n-1)) * sd / sqrt(n). Null below two sessions. #>
    param([Parameter(Mandatory)][double[]] $Effects)
    $n = $Effects.Length
    if ($n -lt 2) { return $null }
    $mean = ($Effects | Measure-Object -Average).Average
    $sd = [Math]::Sqrt((($Effects | ForEach-Object { ($_ - $mean) * ($_ - $mean) } | Measure-Object -Sum).Sum) / ($n - 1))
    $df = $n - 1
    $t = if ($df -le 30) { $script:T975[$df - 1] + $script:T80[$df - 1] } else { 1.960 + 0.8416 }
    return ([Math]::Exp($t * $sd / [Math]::Sqrt($n)) - 1.0) * 100.0
}

function Get-FrameTimingCalibration {
    <# Same-binary calibration: every session runs one executable in all four slots. Per metric, the noise floor is the
       largest same-binary pair deviation |B/A - 1| over all sessions, and the resolution is the largest clock step of any
       run relative to that run's statistic. A metric is judged when its noise floor stays below its declared margin and
       its resolution is at most a tenth of the margin (a margin-sized change spans ten clock steps); otherwise it is
       advisory, with its noise floor and the reason, and later comparisons report it without a verdict. A floor of 0.00%
       is therefore judged only when the clock resolves the margin: a statistic stuck on one quantum is resolution-limited. #>
    param([Parameter(Mandatory)][object[]] $Sessions)
    foreach ($session in $Sessions) {
        $hashes = @($session['runs'] | ForEach-Object { [string]$_['executableSha256'] } | Select-Object -Unique)
        if ($hashes.Count -ne 1 -or [string]::IsNullOrWhiteSpace($hashes[0])) { throw 'A calibration session must measure one executable in all four slots.' }
    }
    if (@($Sessions | ForEach-Object { [string]$_['runs'][0]['executableSha256'] } | Select-Object -Unique).Count -ne 1) {
        throw 'Every calibration session must measure the same executable.'
    }
    $metrics = foreach ($metric in $script:Metrics) {
        $effects = @($Sessions | ForEach-Object { (Get-FrameTimingSessionEffects $_)[$metric.name] })
        if (@($effects | Where-Object { $null -eq $_ }).Count) {
            [ordered]@{ metric=$metric.name; kind=$metric.kind; marginPercent=$metric.marginPercent; status='advisory'; reason='missing or zero samples'; noiseFloorPercent=$null }
            continue
        }
        $ratios = @($effects | ForEach-Object { $_['pairRatios'] })
        $floor = (($ratios | ForEach-Object { [Math]::Abs([double]$_ - 1.0) } | Measure-Object -Maximum).Maximum) * 100.0
        $sessionEffects = [double[]]@($effects | ForEach-Object { $_['effect'] })
        $resolutions = @($Sessions | ForEach-Object { $_['runs'] } | ForEach-Object {
                $summary = $_['summary']
                if ($summary.Contains('resolutionPercent') -and $summary['resolutionPercent'].Contains($metric.name)) { $summary['resolutionPercent'][$metric.name] } else { $null } })
        $resolution = if (@($resolutions | Where-Object { $null -eq $_ }).Count) { $null } else { [double](($resolutions | Measure-Object -Maximum).Maximum) }
        $reason = if ($floor -ge $metric.marginPercent) { 'noise floor at or above the margin' }
            elseif ($null -eq $resolution) { 'clock resolution not observable (a constant statistic)' }
            elseif ($resolution -gt $metric.marginPercent / 10.0) { 'clock resolution coarser than a tenth of the margin' }
            else { $null }
        [ordered]@{
            metric = $metric.name; kind = $metric.kind; statistic = $metric.statistic; marginPercent = $metric.marginPercent
            noiseFloorPercent = $floor
            resolutionPercent = $resolution
            pairRatios = [double[]]$ratios
            sessionEffectsPercent = [double[]]@($sessionEffects | ForEach-Object { ([Math]::Exp($_) - 1.0) * 100.0 })
            minimumDetectableEffectPercent = (Get-FrameTimingMinimumDetectableEffect $sessionEffects)
            equivalence = (Test-FrameTimingEquivalence -Effects $sessionEffects -MarginPercent $metric.marginPercent)
            status = $(if ($null -eq $reason) { 'judged' } else { 'advisory' })
            reason = $reason
        }
    }
    return [ordered]@{
        method = $script:Method; sessions = $Sessions.Count
        rule = 'judged when the largest same-binary pair deviation is below the declared margin (3% CPU, 5% GPU) and the clock resolution is at most a tenth of the margin; otherwise advisory with its noise floor and reason'
        judgedMetrics = @($metrics | Where-Object { $_['status'] -ceq 'judged' }).Count
        metrics = @($metrics)
    }
}

function Compare-FrameTimingSessions {
    <# A against B over interleaved sessions. Per metric: the mean session effect, a two-sided exact sign-flip test
       (the paired judge's own test), equivalence against the declared margin and the minimum detectable effect. With a
       calibration, its advisory metrics get no verdict and a change must also exceed the metric's noise floor. #>
    param([Parameter(Mandatory)][object[]] $Sessions, [Collections.IDictionary] $Calibration)
    $floors = @{}
    if ($Calibration) { foreach ($entry in $Calibration['metrics']) { $floors[$entry['metric']] = $entry } }
    $metrics = foreach ($metric in $script:Metrics) {
        $effects = @($Sessions | ForEach-Object { (Get-FrameTimingSessionEffects $_)[$metric.name] })
        if (@($effects | Where-Object { $null -eq $_ }).Count) {
            [ordered]@{ metric=$metric.name; verdict='invalid'; reason='missing or zero samples' }
            continue
        }
        $sessionEffects = [double[]]@($effects | ForEach-Object { $_['effect'] })
        $mean = ($sessionEffects | Measure-Object -Average).Average
        $change = ([Math]::Exp($mean) - 1.0) * 100.0
        $p = Get-ExactSignFlipPValue -Effects $sessionEffects
        $equivalence = Test-FrameTimingEquivalence -Effects $sessionEffects -MarginPercent $metric.marginPercent
        $calibrated = $floors[$metric.name]
        $floor = if ($calibrated) { $calibrated['noiseFloorPercent'] } else { $null }
        $verdict = if ($calibrated -and $calibrated['status'] -cne 'judged') { 'advisory' }
            elseif ($equivalence.equivalent) { 'equivalent' }
            elseif ($p -lt $script:Alpha -and ($null -eq $floor -or [Math]::Abs($change) -gt $floor)) { if ($change -gt 0) { 'slower' } else { 'faster' } }
            else { 'inconclusive' }
        [ordered]@{
            metric = $metric.name; kind = $metric.kind; marginPercent = $metric.marginPercent
            changePercent = $change; sessionEffectsPercent = [double[]]@($sessionEffects | ForEach-Object { ([Math]::Exp($_) - 1.0) * 100.0 })
            pValue = $p; equivalence = $equivalence; noiseFloorPercent = $floor
            minimumDetectableEffectPercent = (Get-FrameTimingMinimumDetectableEffect $sessionEffects); verdict = $verdict
        }
    }
    return [ordered]@{ method = $script:Method; sessions = $Sessions.Count; calibrated = [bool]$Calibration; metrics = @($metrics) }
}

function Test-FrameTimingSameValue([object] $Left, [object] $Right) {
    # Equal JSON values, numbers to a relative 1e-12 (a value that went through JSON once), arrays and objects recursively.
    if ($null -eq $Left -or $null -eq $Right) { return $null -eq $Left -and $null -eq $Right }
    if ($Left -is [Collections.IDictionary] -and $Right -is [Collections.IDictionary]) {
        foreach ($key in $Left.Keys) { if (-not $Right.Contains($key) -or -not (Test-FrameTimingSameValue $Left[$key] $Right[$key])) { return $false } }
        return $Left.Count -eq $Right.Count
    }
    if (($Left -is [Collections.IList]) -and ($Right -is [Collections.IList])) {
        if ($Left.Count -ne $Right.Count) { return $false }
        for ($index = 0; $index -lt $Left.Count; $index++) { if (-not (Test-FrameTimingSameValue $Left[$index] $Right[$index])) { return $false } }
        return $true
    }
    $numeric = { param($v) $v -is [int] -or $v -is [long] -or $v -is [double] -or $v -is [decimal] -or $v -is [single] -or $v -is [bigint] }
    if ((& $numeric $Left) -and (& $numeric $Right)) {
        $a = [double]$Left; $b = [double]$Right
        return $a -eq $b -or [Math]::Abs($a - $b) -le 1e-12 * [Math]::Max([Math]::Abs($a), [Math]::Abs($b))
    }
    return [object]::Equals($Left, $Right)
}

function Get-FrameTimingJudgement {
    <# The judged statistics a receipt holds, recomputed from its sessions' run summaries: the calibration, the comparison
       (against the receipt's own calibration input, if any) or a smoke run's same-binary observation. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Receipt, [Collections.IDictionary] $Calibration)
    $sessions = @($Receipt['sessions'])
    switch ($Receipt['purpose']) {
        'Calibration' { return [ordered]@{ key = 'calibration'; value = (Get-FrameTimingCalibration -Sessions $sessions) } }
        'Comparison' { return [ordered]@{ key = 'comparison'; value = (Compare-FrameTimingSessions -Sessions $sessions -Calibration $Calibration) } }
        'LoadSensitivity' { return [ordered]@{ key = 'loadSensitivity'; value = (Get-FrameTimingLoadSensitivity -Sessions $sessions -Calibration $Calibration) } }
        default { return [ordered]@{ key = 'sameBinaryObservation'; value = (Get-FrameTimingCalibration -Sessions $sessions)['metrics'] } }
    }
}

function Assert-FrameTimingSameJudgement([object] $Stored, [object] $Recomputed, [string] $Context) {
    # Every field the stored judgement holds must be unchanged; fields it lacks (added by a later judge) are new only.
    if ($Stored -is [Collections.IDictionary]) {
        foreach ($key in $Stored.Keys) {
            if ($key -ceq 'rule') { continue }
            if (-not $Recomputed.Contains($key)) { throw "Judgement changed: $Context.$key is gone." }
            Assert-FrameTimingSameJudgement $Stored[$key] $Recomputed[$key] "$Context.$key"
        }
        return
    }
    if ($Stored -is [Collections.IList] -and $Recomputed -is [Collections.IList] -and $Stored.Count -eq $Recomputed.Count -and
        ($Stored.Count -eq 0 -or $Stored[0] -is [Collections.IDictionary])) {
        for ($index = 0; $index -lt $Stored.Count; $index++) { Assert-FrameTimingSameJudgement $Stored[$index] $Recomputed[$index] "$Context[$index]" }
        return
    }
    if (-not (Test-FrameTimingSameValue $Stored $Recomputed)) { throw "Judgement changed: $Context was $Stored, is $Recomputed." }
}

function ConvertTo-FrameTimingRetainedReceipt {
    <# The committed form of a receipt: per-run summaries, each raw run report's SHA-256 and size, and the judged
       statistics; no per-frame samples (the raw reports stay in the build tree). From a full receipt, whose runs embed
       their raw reports, every run summary and the judgement are recomputed and must equal what the receipt holds; the
       retained receipt then carries the recomputed values, which also hold the clock resolution. ReportSizes maps a
       report's SHA-256 to its size in bytes. Deterministic: the same input gives the same output. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Receipt, [Collections.IDictionary] $ReportSizes = @{}, [Collections.IDictionary] $Calibration)
    $retained = (ConvertTo-Json -InputObject $Receipt -Depth 40 -Compress) | ConvertFrom-Json -AsHashtable -Depth 40
    foreach ($session in @($retained['sessions'])) {
        foreach ($run in @($session['runs'])) {
            if ($run.Contains('raw')) {
                $summary = Get-FrameTimingRunSummary -Run $run['raw']
                foreach ($key in @($run['summary'].Keys)) {
                    if (-not $summary.Contains($key) -or -not (Test-FrameTimingSameValue $run['summary'][$key] $summary[$key])) {
                        throw "A run summary differs from its embedded raw report: session $($session['session']) slot $($run['slot']) $key."
                    }
                }
                $run['summary'] = $summary
                $run.Remove('raw')
            }
            $size = if ($ReportSizes.Contains([string]$run['reportSha256'])) { $ReportSizes[[string]$run['reportSha256']] } elseif ($run.Contains('reportSizeBytes')) { $run['reportSizeBytes'] } else { $null }
            $run['reportSizeBytes'] = $size
        }
    }
    $judgement = Get-FrameTimingJudgement -Receipt $retained -Calibration $Calibration
    if ($Receipt.Contains($judgement.key)) { Assert-FrameTimingSameJudgement $Receipt[$judgement.key] $judgement.value $judgement.key }
    $retained[$judgement.key] = $judgement.value
    $retained['retainedForm'] = 'dxui-frame-timing-retained-v1'
    $retained['rawReports'] = 'Not committed: each run keeps its report path, SHA-256 and size; the reports and the full receipt stay under rawDirectory.'
    return $retained
}

# --- Admission ------------------------------------------------------------------------------------------------------
# A run is admitted on what disturbs the measurement: the measuring thread's own running share, other work on its
# physical core, other processes' work on the measured GPU, and only a loose cap on everything else.
$script:AdmissionRules = [ordered]@{
    'interference-interim-v1' = [ordered]@{
        id = 'interference-interim-v1'; interim = $true
        description = 'Interim (until the load-sensitivity run sets the limits): the measuring thread ran at least 99% of its frame time at the median and 97% at the 5th percentile; the sibling of its processor was at most 10% busy and its own processor at most 5% busy with other work; other processes kept at most a quarter of the logical processors busy (at least one) and the busiest engine of the measured GPU at most 15% busy.'
        runningShareMedianMin = 0.99; runningShareP05Min = 0.97; siblingBusyMax = 0.10; otherOnMeasuringMax = 0.05
        otherProcessorsFraction = 0.25; otherGpuBusyMax = 0.15
    }
    'host-busy-10pct-v1' = [ordered]@{
        id = 'host-busy-10pct-v1'; interim = $false
        description = 'The original rule: the idle preflight refuses a session when the whole host was more than 10% busy; runs are not judged on interference.'
        hostBusyPercentMax = 10.0
    }
}

function Get-FrameTimingAdmissionRule {
    <# A copy of a named admission rule: interference-interim-v1 (default) or host-busy-10pct-v1 (the original rule,
       kept so earlier receipts stay reproducible). #>
    param([ValidateSet('interference-interim-v1','host-busy-10pct-v1')][string] $Name = 'interference-interim-v1')
    $copy = [ordered]@{}
    foreach ($key in $script:AdmissionRules[$Name].Keys) { $copy[$key] = $script:AdmissionRules[$Name][$key] }
    return $copy
}

function Get-FrameTimingOtherProcessorCap {
    <# The loose cap on other processes' load, in logical processors: a quarter of them, at least one. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Rule, [Parameter(Mandatory)][int] $LogicalProcessors)
    if (-not $Rule.Contains('otherProcessorsFraction')) { return $null }
    return [Math]::Max(1.0, [Math]::Floor($LogicalProcessors * [double]$Rule['otherProcessorsFraction']))
}

function Test-FrameTimingPreflight {
    <# The idle preflight before a session, from Watch-BenchmarkHost.ps1 samples taken while nothing of the study ran.
       host-busy-10pct-v1: at most 10% of all processors busy. interference-interim-v1: at most the other-process cap,
       in logical processors. #>
    param([Parameter(Mandatory)][object[]] $Samples, [Parameter(Mandatory)][Collections.IDictionary] $Rule, [Parameter(Mandatory)][int] $LogicalProcessors)
    $busy = Get-FrameTimingHostBusyPercent -Samples $Samples
    $busyProcessors = $busy / 100.0 * $LogicalProcessors
    if ($Rule['id'] -ceq 'host-busy-10pct-v1') {
        $admitted = $busy -le [double]$Rule['hostBusyPercentMax']
        $reason = if ($admitted) { $null } else { [string]::Format([Globalization.CultureInfo]::InvariantCulture, 'the host was {0:0.0}% busy, above {1}%', $busy, $Rule['hostBusyPercentMax']) }
    } else {
        $cap = Get-FrameTimingOtherProcessorCap -Rule $Rule -LogicalProcessors $LogicalProcessors
        $admitted = $busyProcessors -le $cap
        $reason = if ($admitted) { $null } else { [string]::Format([Globalization.CultureInfo]::InvariantCulture, 'other processes kept {0:0.0} logical processors busy, above the cap of {1}', $busyProcessors, $cap) }
    }
    return [ordered]@{ rule = $Rule['id']; busyPercent = $busy; busyProcessors = $busyProcessors; logicalProcessors = $LogicalProcessors; admitted = [bool]$admitted; reason = $reason }
}

function Test-FrameTimingRunAdmission {
    <# Whether one run's measurement was disturbed, from what the run recorded itself (running share, processor busy
       shares) and the other-process GPU busy share measured around its session. Every indicator is recorded whatever
       the rule; host-busy-10pct-v1 admits every run. A run without interference records (an older executable) is
       refused by the interim rule. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Summary, [AllowNull()][object] $OtherGpuBusy, [Parameter(Mandatory)][Collections.IDictionary] $Rule)
    $interference = if ($Summary.Contains('interference')) { $Summary['interference'] } else { $null }
    $indicators = [ordered]@{
        runningShareMedian = $(if ($Summary.Contains('runningShareMedian')) { $Summary['runningShareMedian'] } else { $null })
        runningShareP05 = $(if ($Summary.Contains('runningShareP05')) { $Summary['runningShareP05'] } else { $null })
        siblingBusy = $(if ($interference) { $interference['siblingBusy'] } else { $null })
        otherOnMeasuring = $(if ($interference) { $interference['otherOnMeasuring'] } else { $null })
        otherProcessors = $(if ($interference) { $interference['otherProcessors'] } else { $null })
        logicalProcessors = $(if ($interference) { $interference['logicalProcessors'] } else { $null })
        otherGpuBusy = $OtherGpuBusy
    }
    $reasons = [Collections.Generic.List[string]]::new()
    if ($Rule['id'] -cne 'host-busy-10pct-v1') {
        $check = { param($Name, $Value, $Limit, [switch] $Minimum, $Text)
            if ($null -eq $Value) { $reasons.Add("$Name was not recorded") }
            elseif (($Minimum -and [double]$Value -lt $Limit) -or (-not $Minimum -and [double]$Value -gt $Limit)) {
                $reasons.Add([string]::Format([Globalization.CultureInfo]::InvariantCulture, $Text, [double]$Value, $Limit)) } }
        & $check 'the running share' $indicators.runningShareMedian $Rule['runningShareMedianMin'] -Minimum 'the measuring thread ran {0:0.0000} of its frame time at the median, below {1}'
        & $check 'the running share' $indicators.runningShareP05 $Rule['runningShareP05Min'] -Minimum 'the measuring thread ran {0:0.0000} of its frame time at the 5th percentile, below {1}'
        & $check 'the sibling load' $indicators.siblingBusy $Rule['siblingBusyMax'] 'the measuring processor''s sibling was {0:0.000} busy, above {1}'
        & $check 'the measuring processor load' $indicators.otherOnMeasuring $Rule['otherOnMeasuringMax'] 'other work kept the measuring processor {0:0.000} busy, above {1}'
        $cap = if ($null -ne $indicators.logicalProcessors) { Get-FrameTimingOtherProcessorCap -Rule $Rule -LogicalProcessors ([int]$indicators.logicalProcessors) } else { $null }
        & $check 'the other-process load' $indicators.otherProcessors $cap 'other processes kept {0:0.00} logical processors busy, above the cap of {1}'
        & $check 'the other-process GPU load' $OtherGpuBusy $Rule['otherGpuBusyMax'] 'other processes kept the measured GPU''s busiest engine {0:0.000} busy, above {1}'
    }
    return [ordered]@{ rule = $Rule['id']; admitted = ($reasons.Count -eq 0); reasons = $reasons.ToArray(); indicators = $indicators }
}

function Get-FrameTimingGpuEngineSnapshot {
    <# Cumulative running time (100 ns) of every "GPU Engine" counter instance (pid_<pid>_luid_<luid>_phys_<n>_eng_<n>
       _engtype_<type>), readable without elevation. Slow (seconds), so it is taken around a session, not a frame. #>
    $snapshot = @{}
    $samples = @((Get-Counter -Counter '\GPU Engine(*)\Running Time' -ErrorAction SilentlyContinue).CounterSamples)
    # RawValue is unsigned: an engine instance being created or reset can read a wrapped value near 2^64, which is no
    # running time; such an instance is left out of this snapshot, and so out of any delta that uses it.
    foreach ($sample in $samples) {
        $value = [double]$sample.RawValue
        if ($sample.InstanceName -and $value -lt 9.2e18) { $snapshot[[string]$sample.InstanceName] = $value }
    }
    return $snapshot
}

function Get-FrameTimingOtherGpuBusy {
    <# The busiest engine's share of the wall time spent on other processes' work, on the adapter with Luid
       ("0xHIGH_0xLOW"), between two snapshots. Per-instance deltas: an instance present in only one snapshot (a process
       that started or ended, including the study's own children) is left out, and a counter that went backwards (an
       engine reset) counts from zero. Null when either snapshot is empty. #>
    param([Parameter(Mandatory)][hashtable] $Before, [Parameter(Mandatory)][hashtable] $After, [Parameter(Mandatory)][string] $Luid,
        [Parameter(Mandatory)][double] $Seconds, [int[]] $ExcludeProcessIds = @())
    if ($Before.Count -eq 0 -or $After.Count -eq 0 -or $Seconds -le 0) { return $null }
    $engines = @{}
    $needle = '_luid_' + $Luid.ToLowerInvariant() + '_'
    foreach ($name in $After.Keys) {
        if (-not $Before.ContainsKey($name)) { continue }
        $lower = $name.ToLowerInvariant()
        if (-not $lower.Contains($needle)) { continue }
        if ($lower -match '^pid_(\d+)_' -and [int]$Matches[1] -in $ExcludeProcessIds) { continue }
        if ($lower -notmatch '_phys_(\d+)_eng_(\d+)_') { continue }
        $engine = "$($Matches[1]).$($Matches[2])"
        $delta = [double]$After[$name] - [double]$Before[$name]
        if ($delta -lt 0) { $delta = [double]$After[$name] }
        $engines[$engine] = [double]$(if ($engines.ContainsKey($engine)) { $engines[$engine] } else { 0 }) + $delta
    }
    $busiest = 0.0
    foreach ($value in $engines.Values) { $busiest = [Math]::Max($busiest, [double]$value / 1.0e7 / $Seconds) }
    return $busiest
}

function Get-FrameTimingLoadPlan {
    <# The arms of a load-sensitivity study and where each places its synthetic load. Numeric levels place that many
       busy logical processors away from the measuring core: one per other physical core first (cores far from the
       measuring core first), then their second hardware threads. 'sibling' places one busy thread on the measuring
       processor's sibling. Level 0 is the same-binary control. #>
    param([Parameter(Mandatory)][object[]] $Processors, [Parameter(Mandatory)][int] $MeasuringProcessor, [Parameter(Mandatory)][int[]] $Levels,
        [switch] $Sibling)
    $measuring = @($Processors | Where-Object { [int]$_['index'] -eq $MeasuringProcessor })[0]
    if ($null -eq $measuring) { throw "The measuring processor $MeasuringProcessor is not in the topology." }
    $core = [int]$measuring['core']
    $others = @($Processors | Where-Object { [int]$_['core'] -ne $core })
    $byCore = @($others | Group-Object { [int]$_['core'] } | Sort-Object { [Math]::Abs([int]$_.Name - $core) } -Descending)
    $order = [Collections.Generic.List[int]]::new()
    $depth = ($byCore | ForEach-Object { $_.Count } | Measure-Object -Maximum).Maximum
    for ($thread = 0; $thread -lt $depth; $thread++) {
        foreach ($group in $byCore) {
            $members = @($group.Group | Sort-Object { [int]$_['index'] })
            if ($thread -lt $members.Count) { $order.Add([int]$members[$thread]['index']) }
        }
    }
    $arms = foreach ($level in $Levels) {
        if ($level -gt $order.Count) { throw "Level $level exceeds the $($order.Count) logical processors away from the measuring core." }
        [ordered]@{ name = "$level"; level = $level; placement = 'away'; processors = [int[]]@($order | Select-Object -First $level) }
    }
    if ($Sibling) {
        $siblings = @($Processors | Where-Object { [int]$_['core'] -eq $core -and [int]$_['index'] -ne $MeasuringProcessor } | ForEach-Object { [int]$_['index'] })
        if ($siblings.Count -eq 0) { throw 'The measuring core has no sibling: the sibling arm cannot run.' }
        $arms = @($arms) + @([ordered]@{ name = 'sibling'; level = 1; placement = 'sibling'; processors = [int[]]@($siblings[0]) })
    }
    return @($arms)
}

function Get-FrameTimingLoadSensitivity {
    <# Per arm, the loaded runs (B) against the quiet runs (A) of its sessions: each judged metric's equivalence within its
       margin. The admissible level is the largest numeric level at which every judged metric of that arm and of every
       lower numeric arm is equivalent; the sibling arm is reported on its own. Judged metrics come from a calibration
       when one is given, otherwise all six. #>
    param([Parameter(Mandatory)][object[]] $Sessions, [Collections.IDictionary] $Calibration)
    $judged = if ($Calibration) { @($Calibration['metrics'] | Where-Object { $_['status'] -ceq 'judged' } | ForEach-Object { $_['metric'] }) } else { @($script:Metrics | ForEach-Object { $_.name }) }
    $arms = foreach ($group in ($Sessions | Group-Object { [string]$_['arm'] })) {
        $comparison = Compare-FrameTimingSessions -Sessions @($group.Group) -Calibration $Calibration
        $rows = @($comparison.metrics | Where-Object { $_['metric'] -in $judged })
        [ordered]@{
            arm = $group.Name; level = $group.Group[0]['level']; placement = $group.Group[0]['placement']; sessions = $group.Count
            judgedMetrics = $judged; equivalent = [bool](@($rows | Where-Object { -not $_['equivalence']['equivalent'] }).Count -eq 0)
            metrics = @($comparison.metrics)
        }
    }
    $numeric = @($arms | Where-Object { $_['placement'] -ceq 'away' } | Sort-Object { [int]$_['level'] })
    $admissible = $null
    foreach ($arm in $numeric) { if ($arm['equivalent']) { $admissible = [int]$arm['level'] } else { break } }
    $siblingArm = @($arms | Where-Object { $_['placement'] -ceq 'sibling' })
    return [ordered]@{
        rule = 'the largest numeric level at which every judged metric of it and of every lower level is equivalent to the quiet arm within its margin'
        admissibleLevel = $admissible
        siblingEquivalent = $(if ($siblingArm.Count) { [bool]$siblingArm[0]['equivalent'] } else { $null })
        arms = @($arms)
    }
}

function Start-FrameTimingLoad {
    <# Starts the synthetic background load: one child process (DxUi.EmbeddedTests.exe --busy-load) spinning one thread
       on each listed logical processor, waiting until it says it runs. Only Stop-FrameTimingLoad ends it. #>
    param([Parameter(Mandatory)][string] $Executable, [Parameter(Mandatory)][int[]] $Processors, [string[]] $PrefixArguments = @())
    $start = [Diagnostics.ProcessStartInfo]::new($Executable)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    foreach ($argument in @($PrefixArguments) + @('--busy-load', ($Processors -join ','))) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $line = $process.StandardOutput.ReadLineAsync()
    if (-not $line.Wait(30000) -or $null -eq $line.Result -or -not $line.Result.StartsWith('busy load running', [StringComparison]::Ordinal)) {
        Stop-FrameTimingLoad -Process $process
        throw 'The synthetic load did not start.'
    }
    return $process
}

function Stop-FrameTimingLoad {
    <# Ends the load process Start-FrameTimingLoad started; never any other process. #>
    param([AllowNull()][Diagnostics.Process] $Process)
    if ($null -eq $Process) { return }
    try {
        if (-not $Process.HasExited) { $Process.Kill(); [void]$Process.WaitForExit(30000) }
    } finally { $Process.Dispose() }
}

function Get-FrameTimingHostBusyPercent {
    <# Whole-host busy share between the first and last Watch-BenchmarkHost.ps1 samples. GetSystemTimes kernel time
       includes idle time, so busy = 1 - idle / (kernel + user). #>
    param([Parameter(Mandatory)][object[]] $Samples)
    if ($Samples.Count -lt 2) { throw 'The idle preflight needs at least two host observations.' }
    $first = $Samples[0]; $last = $Samples[-1]
    $idle = [double]$last['idle100ns'] - [double]$first['idle100ns']
    $total = ([double]$last['kernelIncludingIdle100ns'] - [double]$first['kernelIncludingIdle100ns']) + ([double]$last['user100ns'] - [double]$first['user100ns'])
    if ($total -le 0) { throw 'The idle preflight observed no elapsed processor time.' }
    return [Math]::Max(0.0, (1.0 - $idle / $total) * 100.0)
}

function Get-FrameTimingActiveScheme {
    <# The active power scheme GUID from powercfg; never changes machine state. #>
    param([string] $PowerCfg = 'powercfg.exe')
    $global:LASTEXITCODE = 0
    $text = (@(& $PowerCfg '/getactivescheme') -join ' ')
    if ($LASTEXITCODE -ne 0 -or $text -notmatch '([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})') {
        throw 'Cannot read the active power scheme.'
    }
    return [ordered]@{ guid = $Matches[1].ToLowerInvariant(); text = $text.Trim() }
}

function Enter-FrameTimingPowerPlan {
    <# Activates the existing High Performance scheme and returns what Exit-FrameTimingPowerPlan restores. Refuses
       rather than creating a scheme or asking for elevation when the scheme is absent or cannot be activated. #>
    param([ValidateSet('HighPerformance','Keep')][string] $Mode = 'HighPerformance', [string] $PowerCfg = 'powercfg.exe')
    $original = Get-FrameTimingActiveScheme -PowerCfg $PowerCfg
    $record = [ordered]@{ mode = $Mode; original = $original; measured = $original; changed = $false; restored = $null }
    if ($Mode -eq 'Keep' -or $original.guid -eq $script:HighPerformanceScheme) { return $record }
    $global:LASTEXITCODE = 0
    $listed = (@(& $PowerCfg '/list') -join "`n")
    if ($LASTEXITCODE -ne 0 -or $listed -notmatch [regex]::Escape($script:HighPerformanceScheme)) {
        throw 'The High Performance power scheme is not available on this machine. It is not created automatically; use -PowerPlan Keep, which records the active scheme.'
    }
    $global:LASTEXITCODE = 0
    & $PowerCfg '/setactive' $script:HighPerformanceScheme | Out-Null
    $record.changed = $true
    $active = $null
    try { $active = Get-FrameTimingActiveScheme -PowerCfg $PowerCfg } catch { $active = $null }
    if ($null -eq $active -or $active.guid -ne $script:HighPerformanceScheme) {
        Exit-FrameTimingPowerPlan -Record $record -PowerCfg $PowerCfg
        throw 'Activating the High Performance power scheme failed (it may require rights this session lacks). Nothing was elevated; the original scheme was restored.'
    }
    $record.measured = $active
    return $record
}

function Exit-FrameTimingPowerPlan {
    <# Restores the scheme active before Enter-FrameTimingPowerPlan, when it changed one, and verifies the restoration. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Record, [string] $PowerCfg = 'powercfg.exe')
    if (-not $Record.changed) { $Record.restored = $true; return }
    $global:LASTEXITCODE = 0
    & $PowerCfg '/setactive' $Record.original.guid | Out-Null
    $active = $null
    try { $active = Get-FrameTimingActiveScheme -PowerCfg $PowerCfg } catch { $active = $null }
    $Record.restored = [bool]($null -ne $active -and $active.guid -eq $Record.original.guid)
    if (-not $Record.restored) { throw "Restoring the original power scheme $($Record.original.guid) failed; restore it with: powercfg /setactive $($Record.original.guid)" }
}

Export-ModuleMember -Function Get-FrameTimingMethod, Get-FrameTimingHighPerformanceScheme, Get-FrameTimingMetrics, Get-FrameTimingMedian, Get-FrameTimingPercentile,
    Assert-FrameTimingRun, Get-FrameTimingRunSummary, Get-FrameTimingSchedule, Get-FrameTimingSessionEffects, Get-FrameTimingUpperSignFlipPValue,
    Test-FrameTimingEquivalence, Get-FrameTimingMinimumDetectableEffect, Get-FrameTimingCalibration, Compare-FrameTimingSessions,
    Get-FrameTimingHostBusyPercent, Get-FrameTimingActiveScheme, Get-FrameTimingStep, Test-FrameTimingSameValue, Get-FrameTimingJudgement,
    ConvertTo-FrameTimingRetainedReceipt, Get-FrameTimingRunningShare, Get-FrameTimingAdmissionRule, Get-FrameTimingOtherProcessorCap,
    Test-FrameTimingPreflight, Test-FrameTimingRunAdmission, Get-FrameTimingGpuEngineSnapshot, Get-FrameTimingOtherGpuBusy, Get-FrameTimingLoadPlan,
    Get-FrameTimingLoadSensitivity, Start-FrameTimingLoad, Stop-FrameTimingLoad, Enter-FrameTimingPowerPlan, Exit-FrameTimingPowerPlan
