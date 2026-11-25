# Hardware attribution method: the summaries and decision reading of Tools/FrameAttribution.psm1 and the public
# performance-attribution.ps1 runner. The runner runs in isolated PowerShell children with shimmed executables, powercfg, wpr,
# identity helpers and host observer; nothing here compiles, renders, changes the power scheme or produces timing evidence.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $repository 'Tools/FrameAttribution.psm1') -Force
$fixturePlatform = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }
$script:HighPerformance = '8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c'
$script:Balanced = '381b4222-f694-41f0-9685-ff5bb260df2e'

# A synthetic pass report: $Frames frames at 1 GHz ticks; preparation 2,000,000 ticks (2 ms) and composition 500,000 (0.5 ms)
# per frame, GPU 1 ms per frame; the phases pass splits preparation 40% draw calls, 25% EndDraw, 5% layouts, 30% DxUi.
function New-AttributionReport([string] $Pass = 'timing', [int] $Frames = 10, [double] $PrepareTicks = 2000000, [double] $GpuUs = 1000, [int] $Unprepared = 0, [double] $GpuBusy100ns = -1, [int] $RunningPermyriad = 9990, [double] $SiblingBusy = 0.02) {
    $timing = [ordered]@{
        schema = 1; method = 'dxui-frame-timing-v1'; fixture = 'FX12@uhd-192dpi'; phase = 'dirty'; renderer = 'Hardware'; width = 3840; height = 2160; dpi = 192
        adapter = [ordered]@{ description = 'Synthetic'; vendorId = 1; deviceId = 2; software = $false; driverVersion = '1.2.3.4'; dedicatedVideoBytes = 0 }
        pinning = [ordered]@{ requested = 'auto'; pinned = $true; processor = 3; core = 3; isolationRequested = $true; isolated = $true; isolationError = 0 }
        warmupFrames = 60; minimumFrames = 1; minimumSeconds = 0; frames = $Frames; elapsedSeconds = 1.0
        gpuSlots = 2; gpuReadLatencyFrames = 1; timestampFrequency = 1000000000; disjointFrames = 0
        samples = [ordered]@{
            frames = $Frames
            prepareCycles = @(for ($i = 0; $i -lt $Frames; $i++) { $PrepareTicks })
            compositeCycles = @(for ($i = 0; $i -lt $Frames; $i++) { 500000 })
            submitUs = @(for ($i = 0; $i -lt $Frames; $i++) { 2600.0 })
            gpuPrepareUs = @(for ($i = 0; $i -lt $Frames; $i++) { $GpuUs * 0.8 })
            gpuCompositeUs = @(for ($i = 0; $i -lt $Frames; $i++) { $GpuUs * 0.2 })
            gpuWaitUs = @(for ($i = 0; $i -lt $Frames; $i++) { 5.0 })
            gpuValid = @(for ($i = 0; $i -lt $Frames; $i++) { 1 })
            gpuReadAfterSubmitted = @(for ($i = 0; $i -lt $Frames; $i++) { if ($i + 1 -lt $Frames) { $i + 2 } else { $Frames } })
            runningPermyriad = @(for ($i = 0; $i -lt $Frames; $i++) { $RunningPermyriad })
        }
        # An undisturbed run under the interim admission rule (Tools/FrameTiming.psm1).
        interference = [ordered]@{ logicalProcessors = 32; ownProcessors = 1.2; threadShare = 0.95; measuringBusy = 0.96; siblingBusy = $SiblingBusy; otherOnMeasuring = 0.01; otherProcessors = 1.5 }
    }
    $frameSeries = [ordered]@{ count = $Frames; prepareTicks = @(for ($i = 0; $i -lt $Frames; $i++) { $PrepareTicks }); compositeTicks = @(for ($i = 0; $i -lt $Frames; $i++) { 500000 }) }
    $frameSeries['preparations'] = @(for ($i = 0; $i -lt $Frames; $i++) { if ($i -lt $Unprepared) { 0 } else { 1 } })
    $report = [ordered]@{ schema = 1; method = 'dxui-frame-attribution-v1'; pass = $Pass; frame = 'full'; configuration = 'Release'; tickSource = 'rdtsc'; ticksPerSecond = 1e9; clockSeconds = 1.0; otherGpuBusy = 0.01 }
    if ($Pass -eq 'phases') {
        $each = { param($value) @(for ($i = 0; $i -lt $Frames; $i++) { $value }) }
        $frameSeries['prepareBeginDrawTicks'] = & $each 0; $frameSeries['prepareFlushTicks'] = & $each 0
        $frameSeries['prepareEndDrawTicks'] = & $each ($PrepareTicks * 0.25); $frameSeries['prepareDrawCallTicks'] = & $each ($PrepareTicks * 0.40)
        $frameSeries['prepareTextLayoutTicks'] = & $each ($PrepareTicks * 0.05); $frameSeries['prepareTextFormatTicks'] = & $each 0
        $frameSeries['compositeWrappedTicks'] = & $each 0; $frameSeries['drawCalls'] = & $each 4000; $frameSeries['paintExecutions'] = & $each 2001
        $frameSeries['pixelsPainted'] = & $each 8294400
        $frameSeries['textLayoutsCreated'] = & $each 0; $frameSeries['textFormatsCreated'] = & $each 0; $frameSeries['implicitTextLayouts'] = & $each 0; $frameSeries['createdResources'] = & $each 0
        $report['wrappers'] = [ordered]@{ wrappedContexts = 1; writeFactories = 1; escapedInterfaces = 0; unwrappedTargets = 0 }
    }
    if ($Pass -eq 'sampling') {
        $report['sampling'] = [ordered]@{ intervalMicroseconds = 1000; wakes = 120; outside = 10; failed = 0; unknown = 0; modules = @(
            [ordered]@{ module = 'dxui.embeddedtests.exe'; class = 'dxui'; prepareRunning = 30; compositeRunning = 0; prepareWaiting = 0; compositeWaiting = 0 }
            [ordered]@{ module = 'd2d1.dll'; class = 'd2d1'; prepareRunning = 40; compositeRunning = 0; prepareWaiting = 0; compositeWaiting = 0 }
            [ordered]@{ module = 'nvwgf2umx.dll'; class = 'd3d11AndDriver'; prepareRunning = 15; compositeRunning = 5; prepareWaiting = 0; compositeWaiting = 0 }
            [ordered]@{ module = 'ntdll.dll'; class = 'system'; prepareRunning = 10; compositeRunning = 0; prepareWaiting = 10; compositeWaiting = 0 }) }
    }
    $report['frames'] = $frameSeries
    if ($GpuBusy100ns -ge 0) { $report['gpuEngine'] = [ordered]@{ available = $true; instances = 2; runningTime100ns = $GpuBusy100ns * $Frames; runningTime3d100ns = $GpuBusy100ns * $Frames } }
    $report['timing'] = $timing
    return $report
}
function New-PresentReport([int] $Frames = 10) {
    return [ordered]@{ schema = 1; method = 'dxui-frame-attribution-v1'; pass = 'present'; fixture = 'FX12'; width = 1920; height = 1080; dpi = 96; renderer = 'Hardware'
        adapter = [ordered]@{ description = 'Synthetic' }; window = [ordered]@{ left = -32000; top = -32000; nonActivating = $true; monitor = '\\.\DISPLAY1'; monitorOnAdapter = $true }
        presentation = 'HwndSwapChain, Present(1, 0)'; pacing = 'DwmFlush after every frame'; warmupFrames = 60; frames = $Frames; elapsedSeconds = 1.0
        minimumFrames = 1; minimumSeconds = 0; framesWithoutMetrics = 0; tickSource = 'rdtsc'; ticksPerSecond = 1e9
        samples = [ordered]@{ frameCycles = @(1..$Frames | ForEach-Object { 3000000 }); renderUs = @(1..$Frames | ForEach-Object { 2500 }); presentUs = @(1..$Frames | ForEach-Object { 300 })
            totalUs = @(1..$Frames | ForEach-Object { 3000 }); presents = @(1..$Frames | ForEach-Object { 1 }); renders = @(1..$Frames | ForEach-Object { 1 }) } }
}

Invoke-TestCase 'the matrix holds every fixture at both sizes, a full and a dirty frame, and two window present cases' {
    $cases = @(Get-FrameAttributionCases)
    Assert-Equal 18 $cases.Count 'case count'
    foreach ($fixture in @('FX5','FX3','FX12')) {
        foreach ($resolution in @('1920x1080','3840x2160')) {
            foreach ($role in @('full','dirty')) {
                Assert-True (@($cases | Where-Object { $_.fixture -eq $fixture -and $_.resolution -eq $resolution -and $_.role -eq $role -and $_.host -eq 'embedded' }).Count -ge 1) "$fixture $resolution $role"
            }
        }
    }
    Assert-Equal 2 @($cases | Where-Object { $_.host -eq 'window' }).Count 'window present cases'
    Assert-Equal 'full' @($cases | Where-Object { $_.id -eq 'FX12@uhd-192dpi full' })[0].frame 'the decision case is a full frame'
    Assert-Equal 'phase:hover' @($cases | Where-Object { $_.id -eq 'FX12@uhd-192dpi dirty' })[0].frame 'FX12 dirty is one hover change'
    Assert-Equal $cases.Count @($cases | ForEach-Object id | Sort-Object -Unique).Count 'unique case ids'
}

Invoke-TestCase 'timing summary converts thread cycles at the tick rate and compares GPU with CPU' {
    $summary = Get-FrameAttributionTimingSummary -Report (New-AttributionReport)
    Assert-Equal 10 $summary.frames 'frames'
    Assert-True ([Math]::Abs($summary.cpuPrepareMsMedian - 2.0) -lt 1e-9) 'preparation 2 ms'
    Assert-True ([Math]::Abs($summary.cpuFrameMsMedian - 2.5) -lt 1e-9) 'frame 2.5 ms'
    Assert-True ([Math]::Abs($summary.gpuFrameMsMedian - 1.0) -lt 1e-9) 'GPU 1 ms'
    Assert-True $summary.gpuBelowCpu 'GPU below CPU'
    Assert-True ([Math]::Abs($summary.threadCyclesPerTickMedian - 1.0) -lt 1e-9) 'thread cycles per tick'
    Assert-Throws { Get-FrameAttributionTimingSummary -Report (New-AttributionReport -Pass 'phases') } 'a phases report is not a timing pass'
    $short = New-AttributionReport; $short['frames']['prepareTicks'] = @(1, 2)
    Assert-Throws { Get-FrameAttributionTimingSummary -Report $short } 'one tick sample per frame'
    $noRate = New-AttributionReport; $noRate['ticksPerSecond'] = 0
    Assert-Throws { Get-FrameAttributionTimingSummary -Report $noRate } 'a tick rate is required'
}

Invoke-TestCase 'frames right after a scene rebuild are counted and left out of the settled median' {
    $report = New-AttributionReport -Frames 10
    $report['timing']['rebuilds'] = 2; $report['timing']['timedSteps'] = 5
    $report['frames']['framesSinceRebuild'] = @(0, 1, 2, 3, 4, 0, 1, 2, 3, 4)
    $report['timing']['samples']['prepareCycles'] = @(9000000, 9000000, 9000000, 2000000, 2000000, 9000000, 9000000, 9000000, 2000000, 2000000)
    $summary = Get-FrameAttributionTimingSummary -Report $report
    Assert-Equal 2 $summary.rebuilds 'rebuilds'
    Assert-Equal 6 $summary.framesAfterRebuild 'frames within three of a rebuild'
    Assert-True ([Math]::Abs($summary.afterRebuildSharePercent - 60.0) -lt 1e-9) 'share after rebuilds'
    Assert-True ([Math]::Abs($summary.cpuFrameMsMedianSettled - 2.5) -lt 1e-9) 'settled median leaves the first frames out'
}

Invoke-TestCase 'GPU engine running time, when the counters exist, decides whether GPU time is below CPU time' {
    # A CPU-bound frame: the timestamp timeline (3 ms) follows the CPU (2.5 ms), the engines ran 0.4 ms.
    $busy = Get-FrameAttributionTimingSummary -Report (New-AttributionReport -GpuUs 3000 -GpuBusy100ns 4000)
    Assert-True $busy.gpuEngineAvailable 'engine counters available'
    Assert-True ([Math]::Abs($busy.gpuBusyMsMean - 0.4) -lt 1e-9) 'engine running time 0.4 ms per frame'
    Assert-True $busy.gpuBelowCpu 'busy GPU below CPU although the timeline is not'
    $timeline = Get-FrameAttributionTimingSummary -Report (New-AttributionReport -GpuUs 3000)
    Assert-True (-not $timeline.gpuEngineAvailable -and -not $timeline.gpuBelowCpu) 'without counters the timeline decides'
    $decision = Get-FrameAttributionDecision -Timing $busy
    Assert-Equal 'W4 proceeds; W10.4 stays closed' $decision.outcome 'the engine time is the GPU side of the rule'
    Assert-True ([Math]::Abs($decision.gpuMs - 0.4) -lt 1e-9) 'decision records the GPU time it used'
}

Invoke-TestCase 'dominance is read inclusively from the phases pass, before the sampled leaf share' {
    $timing = Get-FrameAttributionTimingSummary -Report (New-AttributionReport -PrepareTicks 9000000)
    $phases = Get-FrameAttributionPhasesSummary -Report (New-AttributionReport -Pass 'phases' -PrepareTicks 9000000)
    $light = New-AttributionReport -Pass 'sampling'; $light['sampling']['modules'][0]['prepareRunning'] = 200
    $decision = Get-FrameAttributionDecision -Timing $timing -Phases $phases -Sampling (Get-FrameAttributionSamplingSummary -Report $light)
    # Inside Direct2D: (25% + 40%) of 9 ms preparation over 9.5 ms = 61.6%; the sampled leaf share is 20/280.
    Assert-Equal 'W10.4 entry condition is evaluated' $decision.outcome 'inclusive Direct2D share decides'
    Assert-True ([Math]::Abs($decision.insideDirect2DSharePercent - 65.0 * 9.0 / 9.5) -lt 1e-9) 'inclusive share recorded'
    Assert-True ($decision.sampledD2d1AndD3d11DriverLeafSharePercent -lt 50.0) 'leaf share recorded beside it'
}

Invoke-TestCase 'phases summary splits preparation into wrapped calls and DxUi''s own code' {
    $summary = Get-FrameAttributionPhasesSummary -Report (New-AttributionReport -Pass 'phases' -Unprepared 2)
    Assert-True ([Math]::Abs($summary.meanMs.endDraw - 0.5) -lt 1e-9) 'EndDraw 0.5 ms'
    Assert-True ([Math]::Abs($summary.meanMs.drawCalls - 0.8) -lt 1e-9) 'draw calls 0.8 ms'
    Assert-True ([Math]::Abs($summary.meanMs.dxuiOwn - 0.6) -lt 1e-9) 'DxUi own 0.6 ms'
    $total = 0.0; foreach ($value in $summary.sharePercent.Values) { $total += $value }
    Assert-True ([Math]::Abs($total - 100.0) -lt 1e-9) 'shares add to the measured frame'
    Assert-True ([Math]::Abs($summary.sharePercent.composite - 20.0) -lt 1e-9) 'composition share'
    Assert-Equal 2 $summary.framesWithoutPreparation 'frames that prepared nothing are counted'
    Assert-Equal 4000 ([int]$summary.counters.drawCalls.median) 'draw calls per frame'
}

Invoke-TestCase 'sampling summary shares running samples by module class and lists the leading modules' {
    $summary = Get-FrameAttributionSamplingSummary -Report (New-AttributionReport -Pass 'sampling') -TopModules 2
    Assert-Equal 100 ([int]$summary.runningSamples) 'running samples'
    Assert-Equal 10 ([int]$summary.waitingSamples) 'waiting samples kept apart'
    Assert-True ([Math]::Abs($summary.sharePercentByClass.d2d1 - 40.0) -lt 1e-9) 'd2d1 share'
    Assert-True ([Math]::Abs($summary.sharePercentByClass.d3d11AndDriver - 20.0) -lt 1e-9) 'driver share'
    Assert-Equal 'd2d1.dll|dxui.embeddedtests.exe' ((@($summary.topModules | ForEach-Object { $_.module })) -join '|') 'leading modules'
    $bad = New-AttributionReport -Pass 'sampling'; $bad['sampling']['modules'][0]['class'] = 'gpu'
    Assert-Throws { Get-FrameAttributionSamplingSummary -Report $bad } 'unknown module class'
}

Invoke-TestCase 'present summary reads Present and render time and counts single presents' {
    $summary = Get-FrameAttributionPresentSummary -Report (New-PresentReport)
    Assert-True ([Math]::Abs($summary.presentMsMedian - 0.3) -lt 1e-9) 'Present 0.3 ms'
    Assert-True ([Math]::Abs($summary.uiThreadCpuMsMedian - 3.0) -lt 1e-9) 'paint CPU 3 ms'
    Assert-Equal 10 $summary.framesWithOnePresent 'one present per frame'
}

Invoke-TestCase 'the decision rule is read in each of its regions and never assigns the regions it leaves open' {
    $sampling = Get-FrameAttributionSamplingSummary -Report (New-AttributionReport -Pass 'sampling')
    $read = { param($ticks, $gpuUs) Get-FrameAttributionDecision -Timing (Get-FrameAttributionTimingSummary -Report (New-AttributionReport -PrepareTicks $ticks -GpuUs $gpuUs)) -Sampling $sampling }
    Assert-Equal 'W4 proceeds; W10.4 stays closed' (& $read 3000000 1000).outcome 'within 4 ms, GPU below CPU'
    Assert-True ((& $read 3000000 9000).outcome.StartsWith('outside the rule')) 'within 4 ms with GPU above CPU is outside the rule'
    Assert-Equal 'W4 proceeds; the receipt is repeated after W4 before W10.4 is judged' (& $read 5000000 1000).outcome 'between the thresholds'
    $slow = & $read 9000000 1000
    Assert-Equal 'W10.4 entry condition is evaluated' $slow.outcome 'above 8.33 ms with d2d1 and the driver at 60% of the samples'
    Assert-True ([Math]::Abs($slow.sampledD2d1AndD3d11DriverLeafSharePercent - 60.0) -lt 1e-9) 'dominance share recorded'
    Assert-True ([Math]::Abs($slow.marginToW10EntryMs - (8.33 - 9.5)) -lt 1e-9) 'distance from the 8.33 ms threshold'
    $light = New-AttributionReport -Pass 'sampling'; $light['sampling']['modules'][0]['prepareRunning'] = 200
    $lightSummary = Get-FrameAttributionSamplingSummary -Report $light
    $undominated = Get-FrameAttributionDecision -Timing (Get-FrameAttributionTimingSummary -Report (New-AttributionReport -PrepareTicks 9000000)) -Sampling $lightSummary
    Assert-True ($undominated.outcome.StartsWith('outside the rule')) 'above 8.33 ms without Direct2D and driver dominance is outside the rule'
    $unsampled = Get-FrameAttributionDecision -Timing (Get-FrameAttributionTimingSummary -Report (New-AttributionReport -PrepareTicks 9000000))
    Assert-True ($unsampled.outcome.Contains('no attribution')) 'above 8.33 ms without an attribution is not assigned'
}

# --- Runner ---------------------------------------------------------------------------------------------------------
function New-AttributionRunnerFixture([string] $Root) {
    Copy-Item -LiteralPath (Join-Path $repository 'performance-attribution.ps1') -Destination (Join-Path $Root 'performance-attribution.ps1')
    New-Item -ItemType Directory -Force -Path (Join-Path $Root 'Tools') | Out-Null
    foreach ($module in @('FrameTiming.psm1','FrameAttribution.psm1','PerformanceComparison.psm1','PerformanceFixtures.psm1','InteractiveRun.psm1')) {
        Copy-Item -LiteralPath (Join-Path $repository "Tools/$module") -Destination (Join-Path $Root "Tools/$module")
    }
    foreach ($input in @(Get-FrameAttributionInputPaths -EmbeddedTestFiles @('Embedded.Tests.PerformanceFixture.h','Embedded.Tests.PerformanceFixtureFx5.h'))) {
        if (-not (Test-Path -LiteralPath (Join-Path $Root $input))) { Set-FixtureFile $Root $input "synthetic attribution input $input" }
    }
    Set-FixtureFile $Root ".build/$fixturePlatform/Release/DxUi.EmbeddedTests.exe" 'synthetic embedded bytes; a function shim runs instead'
    Set-FixtureFile $Root ".build/$fixturePlatform/Release/DxUi.ControlTests.exe" 'synthetic control bytes; a function shim runs instead'
    Set-FixtureFile $Root 'host-busy.txt' '2'
    Set-FixtureFile $Root 'power.txt' $script:Balanced
    Set-FixtureFile $Root 'Tools/PerformancePolicy.psm1' @'
Set-StrictMode -Version Latest
function Get-PerformanceMeasurementState { param($Root, $Platform, $Configuration) [IO.File]::AppendAllText((Join-Path $Root 'calls.txt'), "state`n"); return [ordered]@{ buildAttestationIdentity = 'synthetic' } }
function Assert-PerformanceMeasurementState { param($Before, $After) [IO.File]::AppendAllText((Join-Path $global:AttributionFixtureRoot 'calls.txt'), "compare`n") }
Export-ModuleMember -Function Get-PerformanceMeasurementState, Assert-PerformanceMeasurementState
'@
    # The fake observer writes counters whose busy share is host-busy.txt percent, then waits for the stop file.
    Set-FixtureFile $Root 'Tools/Watch-BenchmarkHost.ps1' @'
param([string] $OutputPath, [int] $IntervalMilliseconds = 1000)
$busy = [double][IO.File]::ReadAllText((Join-Path $PSScriptRoot '../host-busy.txt'))
[IO.File]::AppendAllText((Join-Path $PSScriptRoot '../calls.txt'), "observer:start`n")
$sequence = 0
while ($true) {
    $line = ConvertTo-Json -Compress -Depth 4 -InputObject ([ordered]@{ sequence=$sequence; utc=[DateTime]::UtcNow.ToString('o'); idle100ns=[long]($sequence * (100 - $busy) * 10); kernelIncludingIdle100ns=($sequence * 1000); user100ns=0; observerPid=$PID; processes=@() })
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
    Set-FixtureJson $Root 'templates.json' ([ordered]@{ timing = (New-AttributionReport -Pass 'timing'); phases = (New-AttributionReport -Pass 'phases'); sampling = (New-AttributionReport -Pass 'sampling'); present = (New-PresentReport) })
    Set-FixtureFile $Root 'Invoke-Runner.ps1' @'
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$global:AttributionFixtureRoot = $PSScriptRoot
$arguments = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'arguments.json')) | ConvertFrom-Json -AsHashtable
$platform = if ([Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString() -eq 'Arm64') { 'ARM64' } else { 'x64' }
Set-Item -Path 'Function:global:powercfg.exe' -Value {
    $root = $global:AttributionFixtureRoot
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "powercfg:$($args -join ' ')`n")
    $global:LASTEXITCODE = 0
    if ($args[0] -eq '/getactivescheme') { "Power Scheme GUID: $([IO.File]::ReadAllText((Join-Path $root 'power.txt')))  (Synthetic)" }
}
Set-Item -Path ('Function:global:' + (Join-Path $env:SystemRoot 'System32/wpr.exe')) -Value {
    [IO.File]::AppendAllText((Join-Path $global:AttributionFixtureRoot 'calls.txt'), "wpr:$($args -join ' ')`n")
    'Failed to enable the policy to profile system performance.'
    $global:LASTEXITCODE = -984068079
}
$templates = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'templates.json')) | ConvertFrom-Json -AsHashtable -Depth 20
$native = {
    $root = $global:AttributionFixtureRoot
    [IO.File]::AppendAllText((Join-Path $root 'calls.txt'), "native:$($args -join ':')`n")
    $pass = if ($args[0] -eq '--present-attribution') { 'present' } else { $args[6] }
    $report = $templates[$pass]
    # idle-frames.txt: every pass prepares nothing; idle-calls.txt holds how many of the next passes prepare nothing.
    $idle = $pass -ne 'present' -and (Test-Path -LiteralPath (Join-Path $root 'idle-frames.txt'))
    $countPath = Join-Path $root 'idle-calls.txt'
    if ($pass -ne 'present' -and (Test-Path -LiteralPath $countPath)) { $left = [int][IO.File]::ReadAllText($countPath); if ($left -gt 0) { $idle = $true; [IO.File]::WriteAllText($countPath, [string]($left - 1)) } }
    if ($pass -ne 'present' -and (Test-Path -LiteralPath (Join-Path $root 'low-running.txt'))) { $report = $report.Clone(); $report['timing'] = $report['timing'].Clone(); $report['timing']['samples'] = $report['timing']['samples'].Clone(); $report['timing']['samples']['runningPermyriad'] = @($report['timing']['samples']['runningPermyriad'] | ForEach-Object { 9700 }) }
    if ($idle) { $report = $report.Clone(); $report['frames'] = $report['frames'].Clone(); $report['frames']['preparations'] = @($report['frames']['preparations'] | ForEach-Object { 0 }) }
    if ($args[0] -eq '--frame-attribution' -and $args[4] -eq 'Warp') { $report['timing']['renderer'] = 'WARP'; $report['timing']['adapter']['software'] = $true }
    [IO.File]::WriteAllText($args[1], (ConvertTo-Json -Depth 20 -Compress -InputObject $report))
    $global:LASTEXITCODE = 0
}
foreach ($name in @('DxUi.EmbeddedTests.exe','DxUi.ControlTests.exe')) { Set-Item -Path ('Function:global:' + (Join-Path $PSScriptRoot ".build/$platform/Release/$name")) -Value $native }
try { & (Join-Path $PSScriptRoot 'performance-attribution.ps1') @arguments; exit 0 }
catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
'@
}
function Invoke-AttributionRunner([string] $Root, [hashtable] $Arguments) {
    if (-not $Arguments.ContainsKey('OutputDirectory')) { $Arguments['OutputDirectory'] = Join-Path $Root 'out' }
    $Arguments['Platform'] = $fixturePlatform
    $Arguments['PreflightSeconds'] = 1
    $Arguments['SkipBuild'] = $true
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

Invoke-FixtureCase 'runner: a smoke run reads the power scheme without switching it, probes kernel tracing and labels the receipt' {
    param($root)
    New-AttributionRunnerFixture $root
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Smoke'; Adapter = 'Warp'; Seconds = 0.5; IdlePreflight = 'Record'; Cases = @('FX12@uhd-192dpi full','FX12 present') }
    Assert-Equal 0 $run.Exit "smoke run failed: $($run.Text)"
    $calls = Get-Calls $root
    Assert-Equal 0 @($calls | Where-Object { $_ -like 'powercfg:*' -and $_ -notlike 'powercfg:/getactivescheme*' }).Count 'the scheme is only read'
    Assert-Equal 3 @($calls | Where-Object { $_ -like 'native:--frame-attribution:*:FX12@uhd-192dpi:full:Warp:auto:*:100:500' }).Count 'three embedded passes'
    Assert-Equal 1 @($calls | Where-Object { $_ -like 'native:--present-attribution:*:FX12:Warp:100:500' }).Count 'one present pass'
    Assert-Equal 1 @($calls | Where-Object { $_ -like 'wpr:-start CPU -instancename DxUiAttribution-*' }).Count 'one unelevated kernel-trace probe'
    Assert-Equal 1 @($calls | Where-Object { $_ -eq 'observer:stop' }).Count 'observer stopped'
    $receipt = Get-FixtureJson $root 'out/attribution.json'
    Assert-True (-not [IO.File]::ReadAllText((Join-Path $root 'out/attribution.json')).Contains("`r")) 'the receipt is written with LF line endings'
    Assert-Equal 'Smoke' $receipt.purpose 'purpose'
    Assert-True $receipt.classification.StartsWith('smoke run') 'smoke label'
    Assert-True (-not $receipt.kernelSampling.available) 'refused kernel sampling recorded'
    Assert-Equal '0xc5585011' $receipt.kernelSampling.exitCodeHex 'refusal code'
    Assert-True (-not $receipt.powerScheme.changedByThisRun) 'scheme unchanged'
    Assert-Equal 2 @($receipt.cases).Count 'two cases'
    Assert-Equal 'W4 proceeds; W10.4 stays closed' $receipt.decision[0].outcome 'decision read on the FX12 4K full frame'
    Assert-True ($receipt.cases[0].raw.timing.sha256.Length -eq 64) 'raw report hash recorded'
    Assert-True ([Math]::Abs([double]$receipt.idle.preflight.busyPercent - 2.0) -lt 1e-6) 'preflight busy share recorded'
}

Invoke-FixtureCase 'runner: a receipt refuses WARP, short runs and a power scheme it would have to switch, before any work' {
    param($root)
    New-AttributionRunnerFixture $root
    foreach ($case in @(
            @{ arguments = @{ Purpose = 'Receipt'; Adapter = 'Warp' }; text = 'hardware adapter' },
            @{ arguments = @{ Purpose = 'Receipt'; Seconds = 2 }; text = '10 s per pass' },
            @{ arguments = @{ Purpose = 'Receipt'; IdlePreflight = 'Record' }; text = 'idle preflight' },
            @{ arguments = @{ Purpose = 'Receipt' }; text = 'never switches it' })) {
        $run = Invoke-AttributionRunner $root $case.arguments
        Assert-True ($run.Exit -ne 0 -and $run.Text.Contains($case.text)) "weaker admission accepted ($($case.text)): $($run.Text)"
    }
    Assert-Equal 0 @(Get-Calls $root | Where-Object { $_ -notlike 'powercfg:/getactivescheme*' }).Count 'refusals reached no build, wpr, observer or native work'
}

Invoke-TestCase 'conditions name every pass whose frames did no work or whose host was busy' {
    $case = [ordered]@{ id = 'FX3@fhd dirty'; timing = (Get-FrameAttributionTimingSummary -Report (New-AttributionReport -Frames 20 -Unprepared 2))
        hostLoad = [ordered]@{ timing = [ordered]@{ othersBusyPercent = 3.0 } } }
    $met = Get-FrameAttributionConditions -Cases @($case) -MinimumWorkSharePercent 85
    Assert-True $met.met "90% working frames meet an 85% minimum: $($met.problems -join '; ')"
    Assert-Equal 18 ([int]$case.timing.framesWithWork) 'working frames counted'
    $failed = Get-FrameAttributionConditions -Cases @($case) -MinimumWorkSharePercent 95
    Assert-True (-not $failed.met -and $failed.problems[0].Contains('only 90.0% of 20 frames')) 'idle frames refused'
    $case.hostLoad.timing.othersBusyPercent = 25.0
    $busy = Get-FrameAttributionConditions -Cases @($case) -MinimumWorkSharePercent 85
    Assert-True (-not $busy.met -and $busy.problems[0].Contains('25.0% busy')) 'busy pass refused'
}

Invoke-TestCase 'runner: host observations of exactly one sample are one sample, not the key count of a lone hashtable' {
    # The runner's functions are taken from its own text (the script runs a measurement when invoked); nothing of it executes.
    $parsed = [Management.Automation.Language.Parser]::ParseFile((Join-Path $repository 'performance-attribution.ps1'), [ref]$null, [ref]$null)
    $functions = $parsed.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -in @('Get-HostSamples', 'Get-HostLoad') }, $true)
    Assert-Equal 2 @($functions).Count 'the host observation functions are found'
    Import-Module (Join-Path $repository 'Tools/FrameTiming.psm1') -Force -Global
    $script:ObserverSamples = [Collections.Generic.List[object]]::new()
    foreach ($function in $functions) { . ([scriptblock]::Create($function.Extent.Text)) }
    $now = [DateTime]::UtcNow
    $one = @{ utc = $now; idle100ns = 10; kernelIncludingIdle100ns = 100; user100ns = 0; own = @() }
    $script:ObserverSamples.Add($one)
    $window = @(Get-HostSamples $now.AddSeconds(-1) $now.AddSeconds(1))
    Assert-Equal 1 $window.Count 'one sample in the window'
    $load = Get-HostLoad $now.AddSeconds(1) $now.AddSeconds(1)
    Assert-Equal 1 $load.samples 'one observed sample is reported as one'
    Assert-True ($null -eq $load.busyPercent -and $null -eq $load.othersBusyPercent) 'a lone sample gives no busy share'
    $script:ObserverSamples.Add(@{ utc = $now.AddMilliseconds(500); idle100ns = 20; kernelIncludingIdle100ns = 200; user100ns = 0; own = @() })
    Assert-Equal 2 (Get-HostLoad $now.AddSeconds(1) $now.AddSeconds(1)).samples 'two samples are two'
}

Invoke-FixtureCase 'runner: a receipt whose frames did no work is never published and the run fails' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    Set-FixtureFile $root 'idle-frames.txt' 'yes'
    $receiptPath = Join-Path $root 'receipt/attribution.json'
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX3@fhd dirty'); ReceiptPath = $receiptPath; IdleWaitSeconds = 1; MaxRetries = 4 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('was not published')) "idle receipt accepted: $($run.Text)"
    # Three passes that never pass: the first two get their 3 attempts, the third only what is left of the 4 retries.
    Assert-Equal 7 @(Get-Calls $root | Where-Object { $_ -like 'native:--frame-attribution:*' }).Count 'attempts bounded per pass and in all'
    Assert-True (-not (Test-Path -LiteralPath $receiptPath)) 'nothing at the receipt path'
    $rejected = Get-FixtureJson $root 'out/attribution.rejected.json'
    Assert-True $rejected.classification.StartsWith('rejected') 'rejected record labelled'
    Assert-True (@($rejected.conditions.problems).Count -ge 3) 'every idle pass named'
    Assert-Equal 3 @($rejected.cases[0].attempts.timing).Count 'every attempt of a pass recorded'
    Assert-True (-not $rejected.cases[0].attempts.timing[2].clean -and $rejected.cases[0].attempts.timing[0].problem.Contains('frames did work')) 'each attempt says why it was not clean'
    Assert-Equal 4 ([int]$rejected.retries.used) 'retries used'
    Assert-Equal 0 @(Get-Calls $root | Where-Object { $_ -like 'powercfg:/setactive*' }).Count 'the scheme is never switched'
}

Invoke-FixtureCase 'runner: -SkipWindow leaves out the window-host cases' {
    param($root)
    New-AttributionRunnerFixture $root
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Smoke'; Adapter = 'Warp'; Seconds = 0.5; IdlePreflight = 'Record'; SkipWindow = $true; Cases = @('FX5@uhd-192dpi full') }
    Assert-Equal 0 $run.Exit "smoke run failed: $($run.Text)"
    Assert-Equal 0 @(Get-Calls $root | Where-Object { $_ -like 'native:--present-attribution*' }).Count 'no present pass'
    $refused = Invoke-AttributionRunner $root @{ Purpose = 'Smoke'; Adapter = 'Warp'; Seconds = 0.5; IdlePreflight = 'Record'; SkipWindow = $true; Cases = @('FX12 present') }
    Assert-True ($refused.Exit -ne 0 -and $refused.Text.Contains('Unknown attribution case')) 'a window case is unknown with -SkipWindow'
}

Invoke-FixtureCase 'runner: a busy host refuses the receipt before any measurement' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    Set-FixtureFile $root 'host-busy.txt' '40'
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; ReceiptPath = (Join-Path $root 'receipt/attribution.json'); IdleWaitSeconds = 1 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('Idle preflight refused')) "busy host accepted: $($run.Text)"
    Assert-Equal 0 @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count 'no run on a busy host'
    Assert-Equal 1 @(Get-Calls $root | Where-Object { $_ -eq 'observer:stop' }).Count 'observer stopped after refusal'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'receipt/attribution.json'))) 'no receipt'
}

Invoke-FixtureCase 'runner: a pass that fails once is measured again and only the clean attempt enters the run' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    Set-FixtureFile $root 'idle-calls.txt' '1'
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX3@fhd dirty'); ReceiptPath = (Join-Path $root 'receipt/attribution.json'); IdleWaitSeconds = 1 }
    Assert-Equal 0 $run.Exit "retried run failed: $($run.Text)"
    Assert-Equal 4 @(Get-Calls $root | Where-Object { $_ -like 'native:--frame-attribution:*' }).Count 'one pass measured twice'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $root 'receipt/attribution.json'))) 'part of the matrix is never published as the receipt'
    $partial = Get-FixtureJson $root 'out/attribution.partial.json'
    Assert-True $partial.conditions.met 'every pass in the run is clean'
    Assert-Equal 2 @($partial.cases[0].attempts.timing).Count 'two attempts recorded'
    Assert-True (-not $partial.cases[0].attempts.timing[0].clean -and $partial.cases[0].attempts.timing[1].clean) 'the rejected attempt first, the clean one kept'
    Assert-True ($null -ne $partial.cases[0].attempts.timing[0].othersBusyPercent) 'the rejected attempt''s load recorded'
    Assert-True ($partial.cases[0].raw.timing.report.EndsWith('-attempt2.json')) 'the kept report is the clean attempt'
    Assert-Equal 1 ([int]$partial.retries.used) 'one retry used'
}

Invoke-FixtureCase 'runner: -CaseSet Decision measures the two decision cases, and a resumed run reuses their clean passes' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    $first = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; CaseSet = 'Decision'; OutputDirectory = (Join-Path $root 'first'); ReceiptPath = (Join-Path $root 'receipt/attribution.json') }
    Assert-Equal 0 $first.Exit "decision run failed: $($first.Text)"
    $partial = Get-FixtureJson $root 'first/attribution.partial.json'
    Assert-Equal 'FX12@uhd-192dpi full|FX12@form-layout-10k-uhd full' ((@($partial.cases | ForEach-Object id)) -join '|') 'the decision cases'
    Assert-Equal 2 @($partial.decision).Count 'both decision readings'
    $before = @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count
    $second = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; OutputDirectory = (Join-Path $root 'second'); ReceiptPath = (Join-Path $root 'receipt/attribution.json'); ResumeFrom = @((Join-Path $root 'first')) }
    Assert-Equal 0 $second.Exit "resumed run failed: $($second.Text)"
    $measured = @(Get-Calls $root | Where-Object { $_ -like 'native:*' }).Count - $before
    Assert-Equal (16 * 3 + 2 - 6) $measured 'only the passes the first run did not measure'
    $receipt = Get-FixtureJson $root 'receipt/attribution.json'
    Assert-Equal 'hardware attribution receipt' $receipt.classification 'a complete clean run is published'
    Assert-Equal 'first' $receipt.cases[@($receipt.cases | ForEach-Object id).IndexOf('FX12@uhd-192dpi full')].origin.timing 'reused passes name their run'
    Assert-Equal 'second' $receipt.cases[0].origin.timing 'new passes name this run'
    Assert-Equal 'first' $receipt.resumedFrom[0].runId 'the resumed run is recorded'
}

Invoke-FixtureCase 'runner: a run of another build or an old run is never resumed' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    $first = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX5@uhd-192dpi full'); OutputDirectory = (Join-Path $root 'first') }
    Assert-Equal 0 $first.Exit "first run failed: $($first.Text)"
    $checkpointPath = Join-Path $root 'first/attribution.checkpoint.json'
    $checkpoint = Get-Content -Raw -LiteralPath $checkpointPath | ConvertFrom-Json -AsHashtable -Depth 40
    $checkpoint['identity']['executablesSha256']['embedded'] = 'OTHER'
    [IO.File]::WriteAllText($checkpointPath, (ConvertTo-Json -InputObject $checkpoint -Depth 40))
    $other = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX5@uhd-192dpi full'); OutputDirectory = (Join-Path $root 'second'); ResumeFrom = @((Join-Path $root 'first')) }
    Assert-True ($other.Exit -ne 0 -and $other.Text.Contains('executablesSha256 differs')) "another build resumed: $($other.Text)"
    $checkpoint['identity']['executablesSha256']['embedded'] = (Get-FileHash -LiteralPath (Join-Path $root ".build/$fixturePlatform/Release/DxUi.EmbeddedTests.exe") -Algorithm SHA256).Hash
    $checkpoint['startedUtc'] = [DateTime]::UtcNow.AddHours(-30).ToString('o')
    [IO.File]::WriteAllText($checkpointPath, (ConvertTo-Json -InputObject $checkpoint -Depth 40))
    $old = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX5@uhd-192dpi full'); OutputDirectory = (Join-Path $root 'third'); ResumeFrom = @((Join-Path $root 'first')) }
    Assert-True ($old.Exit -ne 0 -and $old.Text.Contains('-ResumeMaxAgeHours')) "an old run resumed: $($old.Text)"
}
Invoke-FixtureCase 'runner: the interim admission rule judges running share, except on the sampling pass; Legacy10 stays selectable' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    Set-FixtureFile $root 'low-running.txt' 'yes'
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Cases = @('FX5@uhd-192dpi full'); IdleWaitSeconds = 1; MaxPassAttempts = 1 }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('running share') -or $run.Text.Contains('ran 0.9700')) "a disturbed run admitted: $($run.Text)"
    $rejected = Get-FixtureJson $root 'out/attribution.rejected.json'
    Assert-Equal 'interference-interim-v2' $rejected.admission.rule.id 'the rule is named in the record'
    Assert-True (-not $rejected.cases[0].admission.timing.admitted -and -not $rejected.cases[0].admission.phases.admitted) 'timing and phases passes refused on running share'
    Assert-True $rejected.cases[0].admission.sampling.admitted 'the sampling pass is not judged on running share'
    Assert-True ($rejected.cases[0].admission.sampling.judged.Contains('but the running share')) 'the sampling pass says how it was judged'
    Assert-True ($null -ne $rejected.cases[0].admission.timing.indicators.siblingBusy) 'indicators recorded per pass'
    Assert-True ($rejected.cases[0].admission.timing.indicators.isolated -and [int]$rejected.cases[0].admission.timing.indicators.isolationError -eq 0) 'the measuring core''s isolation recorded per pass'
    $legacy = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; Admission = 'Legacy10'; Cases = @('FX5@uhd-192dpi full'); OutputDirectory = (Join-Path $root 'legacy') }
    Assert-Equal 0 $legacy.Exit "the legacy rule refused an idle host: $($legacy.Text)"
    Assert-Equal 'host-busy-10pct-v1' (Get-FixtureJson $root 'legacy/attribution.partial.json').admission.rule.id 'the legacy rule is named'
}
Invoke-TestCase 'the published receipt reads back into the README tables' {
    $receiptPath = Join-Path $repository 'Measurements/PerformanceAndMotion_2026-11-13/W0.7/attribution.json'
    $receipt = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json -AsHashtable -Depth 60
    Assert-Equal 'hardware attribution receipt' $receipt.classification 'a published receipt'
    Assert-True $receipt.conditions.met 'every pass of the published receipt was admitted'
    $tables = Format-FrameAttributionReceipt -Receipt $receipt
    $rows = @($tables.Split("`n") | Where-Object { $_.StartsWith('| `FX') })
    Assert-Equal 16 @($rows | Where-Object { $_.Contains(' full` |') -or $_.Contains(' dirty` |') } | Where-Object { -not $_.Contains('W10.4') -and -not $_.Contains('W4 proceeds') -and -not $_.Contains('outside the rule') }).Count 'one row per embedded case'
    Assert-True ($tables.Contains('Window host, full frames: `FX12` `Present`')) 'the window host line'
    Assert-Equal 2 @($rows | Where-Object { $_.Contains('W10.4') -or $_.Contains('W4 proceeds') -or $_.Contains('outside the rule') }).Count 'both decision readings'
    $readme = [IO.File]::ReadAllText((Join-Path $repository 'Measurements/PerformanceAndMotion_2026-11-13/W0.7/README.md')).Replace("`r`n", "`n")
    Assert-True ($readme.Contains($tables)) 'the README carries the tables exactly as generated from the receipt'
}
Invoke-FixtureCase 'runner: a published receipt is a library measurement with its input hashes, as validate-specs requires' {
    param($root)
    New-AttributionRunnerFixture $root
    Set-FixtureFile $root 'power.txt' $script:HighPerformance
    $receiptPath = Join-Path $root 'Measurements/Study/attribution.json'
    $run = Invoke-AttributionRunner $root @{ Purpose = 'Receipt'; SkipWindow = $true; ReceiptPath = $receiptPath; IdleWaitSeconds = 1 }
    Assert-Equal 0 $run.Exit "receipt run failed: $($run.Text)"
    $receipt = Get-FixtureJson $root 'Measurements/Study/attribution.json'
    Assert-Equal 'DxUi' $receipt.workloadOwner 'library workload'
    Assert-Equal 'dxui-frame-attribution-v1' $receipt.fixture 'library fixture identity'
    Assert-Equal (Get-FileHash -LiteralPath (Join-Path $root 'Tests/Embedded/Embedded.Tests.FrameAttribution.h') -Algorithm SHA256).Hash $receipt.benchmarkInputs['Tests/Embedded/Embedded.Tests.FrameAttribution.h'] 'input hash'
    Set-FixtureFile $root 'Measurements/Study/README.md' '# Study'
    Import-Module (Join-Path $repository 'Tools/Validation.psm1') -Force
    $result = Test-DxUiMeasurements -Root $root
    Assert-Equal 0 @($result.Failures).Count "the measurement check accepts the published receipt: $(@($result.Failures) -join '; ')"
}

Invoke-FixtureCase 'runner: -StampReceipt adds the input hashes of a published receipt''s recorded commit and changes nothing else' {
    param($root)
    New-AttributionRunnerFixture $root
    & git -C $root init -q 2>&1 | Out-Null
    & git -C $root -c user.name=fixture -c user.email=fixture@invalid add -A 2>&1 | Out-Null
    & git -C $root -c user.name=fixture -c user.email=fixture@invalid commit -q -m fixture 2>&1 | Out-Null
    $commit = @(& git -C $root rev-parse HEAD)[0].Trim()
    $published = [ordered]@{ schema = 1; method = 'dxui-frame-attribution-v1'; item = 'W0.7'; purpose = 'Receipt'; source = [ordered]@{ commit = $commit; dirty = $false }
        cases = @([ordered]@{ id = 'FX12@uhd-192dpi full'; timing = [ordered]@{ cpuFrameMsMedian = 9.465122716711987; gpuBusyMsMean = 3.335800362E-05 } }) }
    Set-FixtureJson $root 'Measurements/Study/attribution.json' $published
    Set-FixtureFile $root 'Measurements/Study/README.md' '# Study'
    Import-Module (Join-Path $repository 'Tools/Validation.psm1') -Force
    Assert-True (@((Test-DxUiMeasurements -Root $root).Failures).Count -gt 0) 'an unstamped receipt fails the measurement check'
    $run = Invoke-AttributionRunner $root @{ StampReceipt = (Join-Path $root 'Measurements/Study/attribution.json') }
    Assert-Equal 0 $run.Exit "stamping failed: $($run.Text)"
    $stamped = Get-FixtureJson $root 'Measurements/Study/attribution.json'
    Assert-Equal 'DxUi' $stamped.workloadOwner 'library workload'
    Assert-Equal (Get-FileHash -LiteralPath (Join-Path $root 'Tests/Embedded/Embedded.Tests.FrameAttribution.h') -Algorithm SHA256).Hash $stamped.benchmarkInputs['Tests/Embedded/Embedded.Tests.FrameAttribution.h'] 'hash of the committed file'
    Assert-True ($stamped.benchmarkInputsSource.Contains($commit)) 'the stamp names its commit'
    Assert-Equal 9.465122716711987 ([double]$stamped.cases[0].timing.cpuFrameMsMedian) 'a recorded figure is unchanged'
    Assert-True (-not [IO.File]::ReadAllText((Join-Path $root 'Measurements/Study/attribution.json')).Contains("`r")) 'written with LF'
    Assert-Equal 0 @((Test-DxUiMeasurements -Root $root).Failures).Count 'the stamped receipt passes the measurement check'
    $again = Invoke-AttributionRunner $root @{ StampReceipt = (Join-Path $root 'Measurements/Study/attribution.json') }
    Assert-True ($again.Exit -ne 0 -and $again.Text.Contains('already carries')) 'a stamped receipt is not stamped twice'
}
Complete-TestRun 'Frame attribution method'
