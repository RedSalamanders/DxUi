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
    [int] $MinimumFrames = 1000, [double] $MinimumSeconds = 2.0) {
    $valid = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0 } else { 1 } })
    return [ordered]@{
        schema = 1; method = 'dxui-frame-timing-v1'; fixture = 'FX5'; phase = 'dirty'; renderer = $Renderer
        adapter = [ordered]@{ description = 'Synthetic'; vendorId = 1; deviceId = 2; software = ($Renderer -eq 'WARP'); driverVersion = '1.2.3.4'; dedicatedVideoBytes = 0 }
        pinning = [ordered]@{ requested = 'auto'; pinned = $true; processor = 3; efficiencyClass = 1; maximumEfficiencyClass = 1; scope = 'measuring UI thread only' }
        warmupFrames = 60; minimumFrames = $MinimumFrames; minimumSeconds = $MinimumSeconds; frames = $Frames; elapsedSeconds = $Seconds
        gpuSlots = 2; gpuReadLatencyFrames = 1; timestampFrequency = 1000000000; disjointFrames = $Disjoint
        samples = [ordered]@{
            frames = $Frames
            prepareCycles = @(for ($frame = 0; $frame -lt $Frames; $frame++) { [long](1000 * $Scale) + ($frame % 10) })
            compositeCycles = @(for ($frame = 0; $frame -lt $Frames; $frame++) { [long](500 * $Scale) })
            submitUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { 400.0 * $Scale })
            gpuPrepareUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0.0 } else { 200.0 * $Scale } })
            gpuCompositeUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame -lt $Disjoint) { 0.0 } else { 100.0 * $Scale } })
            gpuWaitUs = @(for ($frame = 0; $frame -lt $Frames; $frame++) { 5.0 })
            gpuValid = $valid
            gpuReadAfterSubmitted = @(for ($frame = 0; $frame -lt $Frames; $frame++) { if ($frame + 1 -lt $Frames) { $frame + 2 } else { $Frames } })
        }
    }
}
function New-Session([double[]] $Scales, [string[]] $Hashes = @('E','E','E','E')) {
    $roles = @('A','B','B','A')
    return [ordered]@{ order = 'ABBA'; runs = @(for ($slot = 0; $slot -lt 4; $slot++) {
        [ordered]@{ role = $roles[$slot]; slot = $slot; executableSha256 = $Hashes[$slot]; summary = (Get-FrameTimingRunSummary (New-TimingRun -Frames 20 -Scale $Scales[$slot] -MinimumFrames 1 -MinimumSeconds 0)) }
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
}

Invoke-TestCase 'run summary separates preparation and composition and excludes disjoint GPU frames' {
    $summary = Get-FrameTimingRunSummary (New-TimingRun -Disjoint 40)
    Assert-Equal 1004.5 $summary.prepareCyclesMedian 'preparation cycle median'
    Assert-Equal 500.0 $summary.compositeCyclesMedian 'composition cycle median'
    Assert-Equal 1509.0 $summary.uiThreadCyclesP95 'UI-thread cycle p95'
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
    $run = [IO.File]::ReadAllText((Join-Path $root 'run-template.json')) | ConvertFrom-Json -AsHashtable
    $run['minimumFrames'] = [int]$Frames; $run['minimumSeconds'] = [double]$Milliseconds / 1000.0
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
    Assert-Equal 1000 @($receipt.sessions[0].runs[0].raw.samples.submitUs).Count 'raw run report embedded'
    Assert-True ([IO.File]::ReadAllText((Join-Path $root 'out/README.md')).Contains('same-binary calibration')) 'generated explanation'
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
    $run = Invoke-TimingRunner $root @{ Purpose = 'Calibration'; Sessions = 5; SkipBuild = $true }
    Assert-True ($run.Exit -ne 0 -and $run.Text.Contains('Idle preflight refused')) "busy host accepted: $($run.Text)"
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
