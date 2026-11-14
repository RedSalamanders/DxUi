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
    $summary = [ordered]@{}
    foreach ($metric in $script:Metrics) {
        $values = $series[$metric.series]
        $summary[$metric.name] = if ($values.Length -eq 0) { $null } elseif ($metric.statistic -eq 'median') { (Get-FrameTimingMedian $values) } else { Get-FrameTimingPercentile $values 0.95 }
    }
    $summary['frames'] = $frames
    $summary['elapsedSeconds'] = [double]$Run['elapsedSeconds']
    $summary['validGpuFrames'] = $gpu.Count
    $summary['gpuWaitUsMedian'] = (Get-FrameTimingMedian ([double[]]@($samples['gpuWaitUs'])))
    return $summary
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
       largest same-binary pair deviation |B/A - 1| over all sessions. A metric is judged when its noise floor stays below
       its declared margin; otherwise it is advisory and later comparisons report it without a verdict. #>
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
        [ordered]@{
            metric = $metric.name; kind = $metric.kind; statistic = $metric.statistic; marginPercent = $metric.marginPercent
            noiseFloorPercent = $floor
            pairRatios = [double[]]$ratios
            sessionEffectsPercent = [double[]]@($sessionEffects | ForEach-Object { ([Math]::Exp($_) - 1.0) * 100.0 })
            minimumDetectableEffectPercent = (Get-FrameTimingMinimumDetectableEffect $sessionEffects)
            equivalence = (Test-FrameTimingEquivalence -Effects $sessionEffects -MarginPercent $metric.marginPercent)
            status = $(if ($floor -lt $metric.marginPercent) { 'judged' } else { 'advisory' })
        }
    }
    return [ordered]@{
        method = $script:Method; sessions = $Sessions.Count
        rule = 'judged when the largest same-binary pair deviation is below the declared margin (3% CPU, 5% GPU); otherwise advisory with its noise floor'
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
    Get-FrameTimingHostBusyPercent, Get-FrameTimingActiveScheme, Enter-FrameTimingPowerPlan, Exit-FrameTimingPowerPlan
