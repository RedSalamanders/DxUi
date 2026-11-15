# Hardware attribution method (Specs/Core/Core_PerformanceAndResources.md, "Hardware attribution method"; plan W0.7): the
# case matrix, the summaries of the native pass reports (DxUi.EmbeddedTests.exe --frame-attribution, DxUi.ControlTests.exe
# --present-attribution) and the reading of W0.7's decision rule. Summaries keep the retained receipt small; per-frame data
# stays in the build tree. Nothing here changes machine state.
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FrameTiming.psm1') -Function Get-FrameTimingMedian, Get-FrameTimingPercentile, Get-FrameTimingRunSummary, Assert-FrameTimingRun

$script:Method = 'dxui-frame-attribution-v1'
$script:ModuleClasses = @('dxui','d2d1','dwrite','d3d11AndDriver','crt','system','other')
# W0.7's decision rule: a full frame of FX12 at 4K (Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
$script:FullFrameBudgetMs = 4.0
$script:W10EntryMs = 8.33
# Frames after a scene rebuild that the settled statistics leave out.
$script:RebuildSettleFrames = 3

function Get-FrameAttributionMethod { return $script:Method }

function Get-FrameAttributionCases {
    <# The receipt's matrix: every fixture at 1920x1080 and 3840x2160, a full frame (ControlHost::Invalidate) and a typical dirty
       frame each, on the embedded host; and the window host's present pass on FX12's dense screen at both sizes. #>
    $cases = [Collections.Generic.List[object]]::new()
    $embedded = @(
        @{ fixture = 'FX5'; resolution = '1920x1080'; selection = 'FX5@fhd-144dpi'; dirty = 'timed' }
        @{ fixture = 'FX5'; resolution = '3840x2160'; selection = 'FX5@uhd-192dpi'; dirty = 'timed' }
        @{ fixture = 'FX3'; resolution = '1920x1080'; selection = 'FX3@fhd'; dirty = 'timed' }
        @{ fixture = 'FX3'; resolution = '3840x2160'; selection = 'FX3@uhd-192dpi'; dirty = 'timed' }
        @{ fixture = 'FX12'; resolution = '1920x1080'; selection = 'FX12'; dirty = 'phase:hover' }
        @{ fixture = 'FX12'; resolution = '3840x2160'; selection = 'FX12@uhd-192dpi'; dirty = 'phase:hover' }
        @{ fixture = 'FX12'; resolution = '1920x1080'; selection = 'FX12@form-layout-10k'; dirty = 'phase:hover' }
        @{ fixture = 'FX12'; resolution = '3840x2160'; selection = 'FX12@form-layout-10k-uhd'; dirty = 'phase:hover' }
    )
    foreach ($entry in $embedded) {
        foreach ($role in @('full','dirty')) {
            $cases.Add([ordered]@{ id = "$($entry.selection) $role"; fixture = $entry.fixture; selection = $entry.selection; resolution = $entry.resolution
                host = 'embedded'; role = $role; frame = $(if ($role -eq 'full') { 'full' } else { $entry.dirty }) })
        }
    }
    foreach ($entry in @(@{ selection = 'FX12'; resolution = '1920x1080' }, @{ selection = 'FX12@uhd-192dpi'; resolution = '3840x2160' })) {
        $cases.Add([ordered]@{ id = "$($entry.selection) present"; fixture = 'FX12'; selection = $entry.selection; resolution = $entry.resolution
            host = 'window'; role = 'full'; frame = 'full' })
    }
    return $cases.ToArray()
}

function ConvertTo-FrameAttributionMs([double] $Ticks, [double] $TicksPerSecond) {
    if ($TicksPerSecond -le 0) { throw 'A tick rate is required to convert ticks to time.' }
    return $Ticks / $TicksPerSecond * 1000.0
}

function Get-FrameAttributionSeries([Collections.IDictionary] $Frames, [string] $Name) {
    if (-not $Frames.Contains($Name)) { throw "Invalid frame attribution report: frames.$Name is absent." }
    $values = [double[]]@($Frames[$Name])
    if ($values.Length -ne [int]$Frames['count']) { throw "Invalid frame attribution report: frames.$Name must hold one value per frame." }
    return , $values
}

function Assert-FrameAttributionReport {
    <# Validates one native pass report and its embedded frame timing report. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Report, [Parameter(Mandatory)][ValidateSet('timing','phases','sampling','present')][string] $Pass)
    if ($Report['schema'] -ne 1 -or $Report['method'] -cne $script:Method -or $Report['pass'] -cne $Pass) { throw "Invalid frame attribution report: expected a $Pass pass of $script:Method." }
    if (-not $Report.Contains('ticksPerSecond') -or [double]$Report['ticksPerSecond'] -le 0) { throw 'Invalid frame attribution report: no tick rate.' }
    if ($Pass -eq 'present') {
        foreach ($name in @('frameCycles','renderUs','presentUs','totalUs','presents','renders')) {
            if (@($Report['samples'][$name]).Count -ne [int]$Report['frames']) { throw "Invalid present attribution report: samples.$name must hold one value per frame." }
        }
        return
    }
    Assert-FrameTimingRun -Run $Report['timing']
    if ([int]$Report['frames']['count'] -ne [int]$Report['timing']['frames']) { throw 'Invalid frame attribution report: its frame records and its timing frames differ.' }
    $null = Get-FrameAttributionSeries $Report['frames'] 'prepareTicks'
    $null = Get-FrameAttributionSeries $Report['frames'] 'compositeTicks'
    if ($Pass -eq 'sampling' -and -not $Report.Contains('sampling')) { throw 'Invalid frame attribution report: a sampling pass without samples.' }
}

function Get-FrameAttributionCommon([Collections.IDictionary] $Report) {
    $frames = $Report['frames']; $tps = [double]$Report['ticksPerSecond']
    $prepare = Get-FrameAttributionSeries $frames 'prepareTicks'
    $composite = Get-FrameAttributionSeries $frames 'compositeTicks'
    $cycles = [double[]]@($Report['timing']['samples']['prepareCycles'])
    $ratios = [Collections.Generic.List[double]]::new()
    for ($i = 0; $i -lt $prepare.Length; $i++) { if ($prepare[$i] -gt 0) { $ratios.Add($cycles[$i] / $prepare[$i]) } }
    # Frames whose preparation painted the surface: a frame that prepared nothing measured an idle preparation.
    $working = $null
    if ($frames.Contains('preparations')) { $working = @((Get-FrameAttributionSeries $frames 'preparations') | Where-Object { $_ -gt 0 }).Count }
    # A fixture with a finite timed phase rebuilds its scene, untimed, before the phase starts again (FrameTiming): the frames
    # right after a rebuild are reported as a share, and the settled median leaves them out.
    $afterRebuild = $null
    if ($frames.Contains('framesSinceRebuild')) { $afterRebuild = @((Get-FrameAttributionSeries $frames 'framesSinceRebuild') | Where-Object { $_ -lt $script:RebuildSettleFrames }).Count }
    return [ordered]@{
        rebuilds = $(if ($Report['timing'].Contains('rebuilds')) { [int]$Report['timing']['rebuilds'] } else { 0 })
        timedSteps = $(if ($Report['timing'].Contains('timedSteps')) { [int]$Report['timing']['timedSteps'] } else { 0 })
        framesAfterRebuild = $afterRebuild
        afterRebuildSharePercent = $(if ($null -ne $afterRebuild -and [int]$frames['count'] -gt 0) { $afterRebuild / [int]$frames['count'] * 100.0 } else { $null })
        framesWithWork = $working
        workSharePercent = $(if ($null -ne $working -and [int]$frames['count'] -gt 0) { $working / [int]$frames['count'] * 100.0 } else { $null })
        frames = [int]$frames['count']; elapsedSeconds = [double]$Report['timing']['elapsedSeconds']; ticksPerSecond = $tps; tickSource = $Report['tickSource']
        # On x64 QueryThreadCycleTime counts at the time-stamp counter's rate: thread cycles per tick near 1 confirm the conversion.
        threadCyclesPerTickMedian = $(if ($ratios.Count) { Get-FrameTimingMedian $ratios.ToArray() } else { $null })
        prepareWallMsMedian = (ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $prepare) $tps)
        compositeWallMsMedian = (ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $composite) $tps)
    }
}

function Get-FrameAttributionTimingSummary {
    <# The frame's cost from the uninstrumented timing pass: UI-thread CPU of preparation and composition (thread cycles at the
       tick rate), submission wall time and GPU time from timestamps around preparation and composition. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Report)
    Assert-FrameAttributionReport -Report $Report -Pass 'timing'
    $tps = [double]$Report['ticksPerSecond']
    $samples = $Report['timing']['samples']
    $prepare = [double[]]@($samples['prepareCycles']); $composite = [double[]]@($samples['compositeCycles'])
    $cpu = [double[]]::new($prepare.Length)
    for ($i = 0; $i -lt $prepare.Length; $i++) { $cpu[$i] = $prepare[$i] + $composite[$i] }
    $gpu = [Collections.Generic.List[double]]::new(); $gpuPrepare = [Collections.Generic.List[double]]::new(); $gpuComposite = [Collections.Generic.List[double]]::new()
    for ($i = 0; $i -lt $prepare.Length; $i++) {
        if ([int]$samples['gpuValid'][$i] -eq 1) {
            $gpuPrepare.Add([double]$samples['gpuPrepareUs'][$i]); $gpuComposite.Add([double]$samples['gpuCompositeUs'][$i])
            $gpu.Add([double]$samples['gpuPrepareUs'][$i] + [double]$samples['gpuCompositeUs'][$i])
        }
    }
    $summary = Get-FrameAttributionCommon $Report
    $summary['cpuPrepareMsMedian'] = ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $prepare) $tps
    $summary['cpuCompositeMsMedian'] = ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $composite) $tps
    $summary['cpuFrameMsMedian'] = ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $cpu) $tps
    $summary['cpuFrameMsP95'] = ConvertTo-FrameAttributionMs (Get-FrameTimingPercentile $cpu 0.95) $tps
    $summary['submitMsMedian'] = (Get-FrameTimingMedian ([double[]]@($samples['submitUs']))) / 1000.0
    $summary['validGpuFrames'] = $gpu.Count
    $summary['disjointFrames'] = [int]$Report['timing']['disjointFrames']
    $summary['gpuFrameMsMedian'] = $(if ($gpu.Count) { (Get-FrameTimingMedian $gpu.ToArray()) / 1000.0 } else { $null })
    $summary['gpuFrameMsP95'] = $(if ($gpu.Count) { (Get-FrameTimingPercentile $gpu.ToArray() 0.95) / 1000.0 } else { $null })
    $summary['gpuPrepareMsMedian'] = $(if ($gpu.Count) { (Get-FrameTimingMedian $gpuPrepare.ToArray()) / 1000.0 } else { $null })
    $summary['gpuCompositeMsMedian'] = $(if ($gpu.Count) { (Get-FrameTimingMedian $gpuComposite.ToArray()) / 1000.0 } else { $null })
    if ($Report['frames'].Contains('framesSinceRebuild')) {
        $since = Get-FrameAttributionSeries $Report['frames'] 'framesSinceRebuild'
        $settled = [Collections.Generic.List[double]]::new()
        for ($i = 0; $i -lt $cpu.Length; $i++) { if ($since[$i] -ge $script:RebuildSettleFrames) { $settled.Add($cpu[$i]) } }
        $summary['cpuFrameMsMedianSettled'] = $(if ($settled.Count) { ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $settled.ToArray()) $tps } else { $null })
    }
    $cpuTotal = 0.0; foreach ($value in $cpu) { $cpuTotal += $value }
    $summary['cpuFrameMsMean'] = ConvertTo-FrameAttributionMs ($cpuTotal / [Math]::Max(1, $cpu.Length)) $tps
    # GPU busy time: the engines' running time for this process over the measured frames (100 ns units). The timestamps above
    # are the GPU's elapsed timeline, which includes waiting for the CPU's next submission in a CPU-bound frame.
    $engine = if ($Report.Contains('gpuEngine')) { $Report['gpuEngine'] } else { $null }
    $available = $null -ne $engine -and [bool]$engine['available']
    $summary['gpuEngineAvailable'] = $available
    $summary['gpuEngineResets'] = $(if ($null -ne $engine -and $engine.Contains('resets')) { [int]$engine['resets'] } else { $null })
    $summary['gpuEngineNewInstances'] = $(if ($null -ne $engine -and $engine.Contains('newInstances')) { [int]$engine['newInstances'] } else { $null })
    $summary['gpuBusyMsMean'] = $(if ($available) { [double]$engine['runningTime100ns'] / 1e4 / [Math]::Max(1, $prepare.Length) } else { $null })
    $summary['gpu3dBusyMsMean'] = $(if ($available) { [double]$engine['runningTime3d100ns'] / 1e4 / [Math]::Max(1, $prepare.Length) } else { $null })
    if ($available) {
        $summary['gpuComparison'] = 'GPU engine running time (mean) against UI-thread CPU (mean)'
        $summary['gpuBelowCpu'] = [bool]($summary['gpuBusyMsMean'] -lt $summary['cpuFrameMsMean'])
    } else {
        $summary['gpuComparison'] = 'GPU timestamp timeline (median) against UI-thread CPU (median); the timeline includes waits for the CPU'
        $summary['gpuBelowCpu'] = $(if ($null -ne $summary['gpuFrameMsMedian']) { [bool]($summary['gpuFrameMsMedian'] -lt $summary['cpuFrameMsMedian']) } else { $null })
    }
    return $summary
}

function Get-FrameAttributionPhasesSummary {
    <# Where preparation's time goes, from the phases pass (wrapped calls timed; instrumented): means per frame in milliseconds and
       shares of the measured frame (preparation plus composition wall ticks), DxUi's own code being preparation minus every
       wrapped call; and the W0.1 counters per frame. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Report)
    Assert-FrameAttributionReport -Report $Report -Pass 'phases'
    $frames = $Report['frames']; $tps = [double]$Report['ticksPerSecond']; $count = [int]$frames['count']
    if ($count -lt 1) { throw 'A phases pass needs at least one frame.' }
    $series = @{}
    foreach ($name in @('prepareTicks','compositeTicks','prepareBeginDrawTicks','prepareEndDrawTicks','prepareFlushTicks','prepareDrawCallTicks','prepareTextLayoutTicks',
        'prepareTextFormatTicks','compositeWrappedTicks','drawCalls','paintExecutions','pixelsPainted','preparations','textLayoutsCreated','textFormatsCreated',
        'implicitTextLayouts','createdResources')) { $series[$name] = Get-FrameAttributionSeries $frames $name }
    $sum = { param($name) $total = 0.0; foreach ($v in $series[$name]) { $total += $v }; $total }
    $prepare = & $sum 'prepareTicks'; $composite = & $sum 'compositeTicks'
    $parts = [ordered]@{
        endDraw = (& $sum 'prepareEndDrawTicks'); drawCalls = (& $sum 'prepareDrawCallTicks'); textLayouts = (& $sum 'prepareTextLayoutTicks')
        textFormats = (& $sum 'prepareTextFormatTicks'); beginDrawAndFlush = (& $sum 'prepareBeginDrawTicks') + (& $sum 'prepareFlushTicks')
    }
    $wrapped = 0.0; foreach ($value in $parts.Values) { $wrapped += $value }
    $parts['dxuiOwn'] = [Math]::Max(0.0, $prepare - $wrapped)
    $parts['composite'] = $composite
    $whole = $prepare + $composite
    $meanMs = [ordered]@{}; $share = [ordered]@{}
    foreach ($key in $parts.Keys) {
        $meanMs[$key] = ConvertTo-FrameAttributionMs ($parts[$key] / $count) $tps
        $share[$key] = $(if ($whole -gt 0) { $parts[$key] / $whole * 100.0 } else { 0.0 })
    }
    $counters = [ordered]@{}
    foreach ($name in @('drawCalls','paintExecutions','pixelsPainted','preparations','textLayoutsCreated','textFormatsCreated','implicitTextLayouts','createdResources')) {
        $values = $series[$name]
        $counters[$name] = [ordered]@{ median = (Get-FrameTimingMedian $values); minimum = ($values | Measure-Object -Minimum).Minimum; maximum = ($values | Measure-Object -Maximum).Maximum; total = (& $sum $name) }
    }
    $summary = Get-FrameAttributionCommon $Report
    $summary['meanMs'] = $meanMs
    $summary['sharePercent'] = $share
    $summary['compositeWrappedMsMean'] = ConvertTo-FrameAttributionMs ((& $sum 'compositeWrappedTicks') / $count) $tps
    $summary['counters'] = $counters
    $summary['framesWithoutPreparation'] = @($series['preparations'] | Where-Object { $_ -eq 0 }).Count
    $summary['framesWithoutDrawCalls'] = @($series['drawCalls'] | Where-Object { $_ -eq 0 }).Count
    $summary['wrappers'] = $Report['wrappers']
    return $summary
}

function Get-FrameAttributionSamplingSummary {
    <# Sampled UI-thread CPU by module class (running samples inside preparation and composition), with the leading modules. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Report, [ValidateRange(1,64)][int] $TopModules = 8)
    Assert-FrameAttributionReport -Report $Report -Pass 'sampling'
    $sampling = $Report['sampling']
    $byClass = [ordered]@{}; foreach ($class in $script:ModuleClasses) { $byClass[$class] = 0 }
    $running = 0; $waiting = 0
    $modules = @($sampling['modules'] | ForEach-Object {
        $r = [int64]$_['prepareRunning'] + [int64]$_['compositeRunning']; $w = [int64]$_['prepareWaiting'] + [int64]$_['compositeWaiting']
        $class = [string]$_['class']; if (-not $byClass.Contains($class)) { throw "Unknown module class $class." }
        $byClass[$class] += $r; $running += $r; $waiting += $w
        [ordered]@{ module = [string]$_['module']; class = $class; running = $r; prepareRunning = [int64]$_['prepareRunning']; compositeRunning = [int64]$_['compositeRunning']; waiting = $w }
    })
    $share = [ordered]@{}
    foreach ($class in $byClass.Keys) { $share[$class] = $(if ($running -gt 0) { $byClass[$class] / $running * 100.0 } else { 0.0 }) }
    $summary = Get-FrameAttributionCommon $Report
    $summary['intervalMicroseconds'] = [int]$sampling['intervalMicroseconds']
    $summary['runningSamples'] = $running
    $summary['waitingSamples'] = $waiting
    $summary['outsideWakes'] = [int64]$sampling['outside']
    $summary['failedSamples'] = [int64]$sampling['failed']
    $summary['unknownSamples'] = [int64]$sampling['unknown']
    $summary['samplesByClass'] = $byClass
    $summary['sharePercentByClass'] = $share
    $summary['topModules'] = @($modules | Sort-Object -Property @{ Expression = { $_.running }; Descending = $true }, @{ Expression = { $_.module } } | Select-Object -First $TopModules)
    return $summary
}

function Get-FrameAttributionPresentSummary {
    <# The window host's full frames: Present and render wall time from the host's public frame metrics, UI-thread CPU per paint. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Report)
    Assert-FrameAttributionReport -Report $Report -Pass 'present'
    $samples = $Report['samples']; $tps = [double]$Report['ticksPerSecond']
    $present = [double[]]@($samples['presentUs']); $render = [double[]]@($samples['renderUs']); $total = [double[]]@($samples['totalUs']); $cycles = [double[]]@($samples['frameCycles'])
    return [ordered]@{
        frames = [int]$Report['frames']; elapsedSeconds = [double]$Report['elapsedSeconds']; renderer = $Report['renderer']; adapter = $Report['adapter']; window = $Report['window']
        presentation = $Report['presentation']; pacing = $Report['pacing']; framesWithoutMetrics = [int]$Report['framesWithoutMetrics']
        presentMsMedian = (Get-FrameTimingMedian $present) / 1000.0; presentMsP95 = (Get-FrameTimingPercentile $present 0.95) / 1000.0
        renderMsMedian = (Get-FrameTimingMedian $render) / 1000.0; totalMsMedian = (Get-FrameTimingMedian $total) / 1000.0
        uiThreadCpuMsMedian = (ConvertTo-FrameAttributionMs (Get-FrameTimingMedian $cycles) $tps)
        framesWithOnePresent = @($samples['presents'] | Where-Object { [int]$_ -eq 1 }).Count
        framesWithOneRender = @($samples['renders'] | Where-Object { [int]$_ -eq 1 }).Count
        framesWithWork = @($samples['renders'] | Where-Object { [int]$_ -ge 1 }).Count
        workSharePercent = $(if ([int]$Report['frames'] -gt 0) { @($samples['renders'] | Where-Object { [int]$_ -ge 1 }).Count / [int]$Report['frames'] * 100.0 } else { $null })
    }
}

function Get-FrameAttributionConditions {
    <# The conditions a receipt's cases were measured under: every pass did real work in at least MinimumWorkSharePercent of its
       frames, and the frame timing method's admission rule admitted every pass (a record without an admission decision falls back
       to other processes' host load against BusyThresholdPercent). Returns each problem found. #>
    param([Parameter(Mandatory)][object[]] $Cases, [double] $MinimumWorkSharePercent = 95.0, [double] $BusyThresholdPercent = 10.0)
    $problems = [Collections.Generic.List[string]]::new()
    foreach ($case in $Cases) {
        foreach ($pass in @('timing','phases','sampling','present')) {
            if (-not $case.Contains($pass)) { continue }
            $share = $case[$pass]['workSharePercent']
            if ($null -eq $share) { $problems.Add("$($case['id']) ${pass}: the frames' work was not recorded") }
            elseif ([double]$share -lt $MinimumWorkSharePercent) {
                $problems.Add([string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0} {1}: only {2:0.0}% of {3} frames did work (minimum {4}%)', $case['id'], $pass, [double]$share, $case[$pass]['frames'], $MinimumWorkSharePercent))
            }
            if ($case.Contains('admission') -and $case['admission'].Contains($pass)) {
                # The pass's admission decision under the frame timing method's rule (performance-attribution.ps1).
                if (-not $case['admission'][$pass]['admitted']) { $problems.Add("$($case['id']) ${pass}: not admitted ($($case['admission'][$pass]['rule'])): $(@($case['admission'][$pass]['reasons']) -join '; ')") }
            } elseif ($case.Contains('hostLoad') -and $case['hostLoad'].Contains($pass)) {
                $load = $case['hostLoad'][$pass]
                if ($null -eq $load['othersBusyPercent']) { $problems.Add("$($case['id']) ${pass}: no host observation during the pass") }
                elseif ([double]$load['othersBusyPercent'] -gt $BusyThresholdPercent) {
                    $problems.Add([string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0} {1}: other processes kept the host {2:0.0}% busy (threshold {3}%)', $case['id'], $pass, [double]$load['othersBusyPercent'], $BusyThresholdPercent))
                }
            }
        }
    }
    return [ordered]@{ minimumWorkSharePercent = $MinimumWorkSharePercent; busyThresholdPercent = $BusyThresholdPercent; met = ($problems.Count -eq 0); problems = $problems.ToArray() }
}

function Get-FrameAttributionDecision {
    <# Reads W0.7's decision rule on the full frame of one FX12 case at 4K. CPU is the timing pass's median UI-thread frame
       (preparation plus composition); GPU below CPU is the timing summary's comparison (engine running time when the
       counters exist). "d2d1, d3d11 and driver submission dominate" is read inclusively: the phases pass's share of the frame
       spent inside Direct2D calls (draw calls, BeginDraw, EndDraw, Flush, everything they call: DirectWrite rendering, the
       runtime, the driver) is at least half; without a phases pass, the sampled leaf share of d2d1 plus d3d11 and the driver.
       The rule leaves two regions open (a fast frame whose GPU time is not below its CPU time; a slow frame that Direct2D does
       not dominate): they are reported as outside the rule, never assigned. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Timing, [Collections.IDictionary] $Sampling, [Collections.IDictionary] $Phases, [string] $Case = 'FX12@uhd-192dpi full')
    $cpu = [double]$Timing['cpuFrameMsMedian']
    $gpu = if ($Timing.Contains('gpuBusyMsMean') -and $null -ne $Timing['gpuBusyMsMean']) { $Timing['gpuBusyMsMean'] } else { $Timing['gpuFrameMsMedian'] }
    $sampledShare = if ($Sampling) { [double]$Sampling['sharePercentByClass']['d2d1'] + [double]$Sampling['sharePercentByClass']['d3d11AndDriver'] } else { $null }
    $phasesShare = if ($Phases) { [double]$Phases['sharePercent']['endDraw'] + [double]$Phases['sharePercent']['drawCalls'] + [double]$Phases['sharePercent']['beginDrawAndFlush'] } else { $null }
    $gpuBelow = if ($Timing.Contains('gpuBelowCpu') -and $null -ne $Timing['gpuBelowCpu']) { [bool]$Timing['gpuBelowCpu'] } else { $null -ne $gpu -and [double]$gpu -lt $cpu }
    $dominanceShare = if ($null -ne $phasesShare) { $phasesShare } else { $sampledShare }
    $dominates = $null -ne $dominanceShare -and $dominanceShare -ge 50.0
    $outcome = if ($cpu -le $script:FullFrameBudgetMs -and $gpuBelow) { 'W4 proceeds; W10.4 stays closed' }
        elseif ($cpu -le $script:FullFrameBudgetMs) { 'outside the rule: the frame is within 4 ms but its GPU time is not below its CPU time' }
        elseif ($cpu -gt $script:W10EntryMs -and $dominates) { 'W10.4 entry condition is evaluated' }
        elseif ($cpu -gt $script:W10EntryMs -and $null -eq $dominanceShare) { 'outside the rule: above 8.33 ms with no attribution' }
        elseif ($cpu -gt $script:W10EntryMs) { 'outside the rule: above 8.33 ms, but d2d1, d3d11 and the driver do not dominate' }
        else { 'W4 proceeds; the receipt is repeated after W4 before W10.4 is judged' }
    return [ordered]@{
        case = $Case; cpuFrameMsMedian = $cpu; gpuMs = $gpu; gpuMeasure = $(if ($Timing.Contains('gpuComparison')) { $Timing['gpuComparison'] } else { 'GPU timestamp timeline (median)' }); gpuBelowCpu = $gpuBelow
        budgetMs = $script:FullFrameBudgetMs; w10EntryMs = $script:W10EntryMs
        marginToBudgetMs = $script:FullFrameBudgetMs - $cpu; marginToW10EntryMs = $script:W10EntryMs - $cpu
        insideDirect2DSharePercent = $phasesShare; sampledD2d1AndD3d11DriverLeafSharePercent = $sampledShare
        dominanceMeasure = $(if ($null -ne $phasesShare) { 'inclusive time inside Direct2D calls (phases pass)' } elseif ($null -ne $sampledShare) { 'sampled leaf share of d2d1 plus d3d11 and the driver' } else { $null })
        outcome = $outcome; decidedBy = 'the developer; this is the receipt''s reading of the rule'
    }
}

Export-ModuleMember -Function Get-FrameAttributionMethod, Get-FrameAttributionCases, ConvertTo-FrameAttributionMs, Assert-FrameAttributionReport,
    Get-FrameAttributionTimingSummary, Get-FrameAttributionPhasesSummary, Get-FrameAttributionSamplingSummary, Get-FrameAttributionPresentSummary,
    Get-FrameAttributionDecision, Get-FrameAttributionConditions
