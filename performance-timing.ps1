<#
.SYNOPSIS Frame timing method: interleaved A/B/B/A sessions of per-frame UI-thread cycles and GPU timestamps on one fixture.
.DESCRIPTION Specs/Core/Core_PerformanceAndResources.md, "Frame timing method". Each run is a fresh
DxUi.EmbeddedTests.exe --timing-method process holding at least 1,000 frames and 2 s. Calibration measures this tree's
attested executable in all four slots of every session and records each judged metric's same-binary noise floor;
Comparison measures -BaselineExecutable (A) against this tree's executable (B). Smoke runs the same path with relaxed
admission and is labelled as such: it never calibrates or judges. LoadSensitivity measures, per arm, quiet runs (A)
against runs under a synthetic background load (B) of 0, 2, 4, 8 and 12 busy logical processors away from the measuring
core and one on its sibling, and reports the largest level at which every judged metric stays equivalent.
The High Performance scheme is activated only when it exists, without elevation, and the original scheme is restored
however the study ends. Only the measuring UI thread of each child is pinned, by default to the quietest physical core.
Every session starts with an idle preflight from Tools/Watch-BenchmarkHost.ps1, which keeps observing until the session
ends. -Admission Interim (default) judges every run on what disturbs the measurement (the measuring thread's running
share, its sibling and own processor, other processes' load in logical processors and on the measured GPU) and repeats a
rejected session up to -SessionAttempts times; -Admission Legacy10 is the original 10% whole-host preflight.
.EXAMPLE ./performance-timing.ps1 -Purpose Calibration -Fixture FX5 -Phase Dirty -OutputDirectory Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty
.EXAMPLE ./performance-timing.ps1 -Purpose LoadSensitivity -Fixture FX5 -Phase Dirty -CalibrationPath Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json -OutputDirectory Measurements/PerformanceAndMotion_2026-11-13/W0.2/load-sensitivity-FX5-dirty
#>
[CmdletBinding()]
param(
    [ValidateSet('Calibration','Comparison','Smoke','LoadSensitivity')][string] $Purpose = 'Calibration',
    [ValidatePattern('^FX[0-9]+(@[a-z0-9-]+)?$')][string] $Fixture = 'FX5',
    [ValidateSet('Dirty','Clean')][string] $Phase = 'Dirty',
    [ValidateSet('Hardware','Warp')][string] $Adapter = 'Hardware',
    [ValidateRange(1,20)][int] $Sessions = 5,
    [string] $BaselineExecutable = '',
    [string] $CalibrationPath = '',
    [string] $OutputDirectory = '',
    [ValidateSet('HighPerformance','Keep')][string] $PowerPlan = 'HighPerformance',
    [ValidateSet('Require','Record')][string] $IdlePreflight = 'Require',
    [ValidateSet('Interim','Legacy10')][string] $Admission = 'Interim',
    [ValidateRange(1,5)][int] $SessionAttempts = 3,
    [ValidateRange(0.5,100)][double] $IdleThresholdPercent = 10,
    [ValidateRange(1,60)][int] $PreflightSeconds = 5,
    [ValidateRange(0,62)][int[]] $LoadLevels = @(0, 2, 4, 8, 12),
    [switch] $NoSiblingArm,
    [ValidatePattern('^(auto|none|[0-9]{1,2})$')][string] $PinCore = 'auto',
    [ValidateRange(1,200000)][int] $MinimumFrames = 1000,
    [ValidateRange(0,600000)][int] $MinimumMilliseconds = 2000,
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/FrameTiming.psm1') -Force

$native = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Frame timing requires native execution matching the requested architecture.'
}
# Calibration and comparison admit only the declared method; anything weaker is a smoke run and says so.
if ($Purpose -ne 'Smoke') {
    $refusals = @()
    if ($Adapter -ne 'Hardware') { $refusals += 'a hardware adapter (WARP is for counters and pixels only)' }
    if ($Configuration -ne 'Release') { $refusals += 'the Release configuration' }
    if ($Sessions -lt 5) { $refusals += 'at least five sessions (fewer can never establish equivalence)' }
    if ($Purpose -eq 'Comparison' -and $Sessions -lt 6) { $refusals += 'at least six sessions for a comparison (five cannot reach two-sided p < 0.05)' }
    if ($Purpose -eq 'LoadSensitivity' -and $LoadLevels -notcontains 0) { $refusals += 'a level-0 arm (the same-binary control)' }
    if ($MinimumFrames -lt 1000 -or $MinimumMilliseconds -lt 2000) { $refusals += 'runs of at least 1,000 frames and 2 s' }
    if ($IdlePreflight -ne 'Require') { $refusals += 'a required idle preflight' }
    if ($PowerPlan -ne 'HighPerformance') { $refusals += 'the High Performance power scheme' }
    if ($refusals.Count) { throw "$Purpose requires $($refusals -join ', '). Use -Purpose Smoke to exercise the method without a verdict." }
}
if ($Purpose -eq 'Comparison') {
    if (-not $BaselineExecutable) { throw 'Comparison requires -BaselineExecutable (A); B is this tree''s attested executable.' }
    $BaselineExecutable = [IO.Path]::GetFullPath($BaselineExecutable)
    if (-not (Test-Path -LiteralPath $BaselineExecutable -PathType Leaf)) { throw "Missing baseline executable: $BaselineExecutable" }
} elseif ($BaselineExecutable) { throw '-BaselineExecutable applies to -Purpose Comparison only.' }
$calibration = $null
if ($CalibrationPath) {
    $calibrationReceipt = Get-Content -Raw -LiteralPath $CalibrationPath | ConvertFrom-Json -AsHashtable -Depth 30
    if ($calibrationReceipt['purpose'] -cne 'Calibration' -or $calibrationReceipt['fixtureName'] -cne $Fixture -or $calibrationReceipt['phase'] -cne $Phase.ToLowerInvariant()) {
        throw 'The calibration receipt is not a calibration of this fixture and phase.'
    }
    $calibration = $calibrationReceipt['calibration']
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $PSScriptRoot (".build/reports/frame-timing/$Purpose-$Fixture-$Phase-" + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ', [Globalization.CultureInfo]::InvariantCulture))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$receiptPath = Join-Path $OutputDirectory 'receipt.json'
if (Test-Path -LiteralPath $receiptPath) { throw "Preserve the existing frame timing receipt: $receiptPath" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
# Raw native reports and host logs stay in the build tree; the receipt embeds every parsed report with its SHA-256,
# so a receipt under Measurements is self-contained and holds the only JSON file there.
$rawDirectory = Join-Path $PSScriptRoot ('.build/reports/frame-timing/raw/' + (Split-Path -Leaf $OutputDirectory) + '-' +
    [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ', [Globalization.CultureInfo]::InvariantCulture))
New-Item -ItemType Directory -Force -Path $rawDirectory | Out-Null
$timingInputPaths = @('Tests/Embedded/Embedded.Tests.FrameTiming.h','Tests/Embedded/Embedded.Tests.Embedded.cpp','Samples/ComplexUi/ComplexUiScene.h',
    'Samples/EmbeddedControls/GraphicsFixture.h','performance-timing.ps1','Tools/FrameTiming.psm1')
# Every performance fixture header (plan W0.4): the timed fixture is one of them, defined once for every consumer.
$timingInputPaths += @(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'Tests/Embedded') -Filter 'Embedded.Tests.PerformanceFixture*.h' -ErrorAction SilentlyContinue |
    Sort-Object Name | ForEach-Object { 'Tests/Embedded/' + $_.Name })
function Get-TimingInputs {
    $inputs = [ordered]@{}
    foreach ($path in $timingInputPaths) { $inputs[$path] = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $path) -Algorithm SHA256).Hash }
    return $inputs
}
$timingInputs = Get-TimingInputs

Import-Module (Join-Path $PSScriptRoot 'Tools/PerformancePolicy.psm1') -Force
if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) { throw "Frame timing build failed with exit code $LASTEXITCODE." }
}
$candidate = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe"
if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "The frame timing executable is missing: $candidate" }
# An interactive-only fixture (Definition::interactiveOnly) takes the foreground: the timing harness never runs it, says so
# and succeeds, leaving no receipt and removing the empty directories this run made.
$catalogPath = [IO.Path]::GetFullPath((Join-Path $rawDirectory 'catalog.json'))
$global:LASTEXITCODE = 0
& $candidate '--fixture-catalog' $catalogPath | Out-Host
if ($LASTEXITCODE -ne 0) { throw "The fixture catalog could not be read (exit code $LASTEXITCODE)." }
$fixtureId = $Fixture.Split('@')[0]
$catalogEntry = @((Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json -AsHashtable -Depth 40)['fixtures'] | Where-Object { $_['id'] -ceq $fixtureId })
if ($catalogEntry.Count -and $catalogEntry[0].Contains('interactiveOnly') -and $catalogEntry[0]['interactiveOnly']) {
    Write-Host "${fixtureId}: interactive, not run"
    foreach ($directory in @($OutputDirectory, $rawDirectory)) {
        if (@(Get-ChildItem -LiteralPath $directory -Recurse -File | Where-Object { $_.FullName -ne $catalogPath }).Count -eq 0) { Remove-Item -LiteralPath $directory -Recurse }
    }
    return
}
$rule = Get-FrameTimingAdmissionRule -Name $(if ($Admission -eq 'Legacy10') { 'host-busy-10pct-v1' } else { 'interference-interim-v1' })
if ($Admission -eq 'Legacy10') { $rule['hostBusyPercentMax'] = $IdleThresholdPercent }
$logicalProcessors = [Environment]::ProcessorCount
# Calibrations and comparisons repeat a rejected session; smoke runs and load-sensitivity studies (deliberately loaded)
# record every admission decision and enforce none after the preflight.
$enforceRuns = $Purpose -in @('Calibration','Comparison')
$arms = @($null)
if ($Purpose -eq 'LoadSensitivity') {
    # One measuring processor for every run, so the load can be placed away from its core or on its sibling.
    $topologyPath = [IO.Path]::GetFullPath((Join-Path $rawDirectory 'topology.json'))
    $global:LASTEXITCODE = 0
    & $candidate '--processor-topology' $topologyPath | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "The processor topology could not be read (exit code $LASTEXITCODE)." }
    $topology = Get-Content -Raw -LiteralPath $topologyPath | ConvertFrom-Json -AsHashtable -Depth 10
    if ($PinCore -eq 'auto') { $PinCore = [string]$topology['quietest']['processor'] }
    elseif ($PinCore -eq 'none') { throw 'LoadSensitivity pins the measuring thread: use -PinCore auto or a processor index.' }
    $arms = @(Get-FrameTimingLoadPlan -Processors @($topology['processors']) -MeasuringProcessor ([int]$PinCore) -Levels $LoadLevels -Sibling:(-not $NoSiblingArm))
}
$roles = @{ A = $(if ($Purpose -eq 'Comparison') { $BaselineExecutable } else { $candidate }); B = $candidate }
$hashes = @{ A = (Get-FileHash -LiteralPath $roles.A -Algorithm SHA256).Hash; B = (Get-FileHash -LiteralPath $roles.B -Algorithm SHA256).Hash }
if ($Purpose -eq 'Comparison' -and $hashes.A -ceq $hashes.B) { throw 'A comparison needs two different executables; use -Purpose Calibration for a same-binary study.' }

function Start-HostObserver([string] $Path) {
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME $(if ($IsWindows) { 'pwsh.exe' } else { 'pwsh' })))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $PSScriptRoot 'Tools/Watch-BenchmarkHost.ps1'),'-OutputPath',$Path,'-IntervalMilliseconds','500')) {
        $start.ArgumentList.Add($argument)
    }
    $process = [Diagnostics.Process]::Start($start)
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while (-not (Test-Path -LiteralPath ($Path + '.ready'))) {
        if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { Stop-HostObserver $process $Path; throw "The host observer did not start: $Path" }
        Start-Sleep -Milliseconds 100
    }
    return $process
}
function Stop-HostObserver([Diagnostics.Process] $Process, [string] $Path) {
    if ($null -eq $Process) { return }
    try {
        if (-not $Process.HasExited) {
            [IO.File]::WriteAllText($Path + '.stop', [DateTime]::UtcNow.ToString('o'))
            # The observer is this runner's own child; only it is ever stopped forcibly.
            if (-not $Process.WaitForExit(30000)) { $Process.Kill() ; $Process.WaitForExit() }
        }
    } finally { $Process.Dispose() }
}
function Read-HostObservations([string] $Path) {
    return @(Get-Content -LiteralPath $Path | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json -AsHashtable -Depth 6 })
}
function Invoke-TimingRun([string] $Executable, [string] $Output) {
    $global:LASTEXITCODE = 0
    & $Executable '--timing-method' $Output $Fixture $Phase $Adapter $PinCore $MinimumFrames $MinimumMilliseconds | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Frame timing run failed with exit code $LASTEXITCODE. Executable: $Executable" }
    if (-not (Test-Path -LiteralPath $Output -PathType Leaf)) { throw "The frame timing run wrote no report: $Output" }
    $bytes = [IO.File]::ReadAllBytes($Output)
    $run = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) | ConvertFrom-Json -AsHashtable -Depth 10
    try { Assert-FrameTimingRun -Run $run -Judged:($Purpose -ne 'Smoke') }
    catch { Move-Item -LiteralPath $Output -Destination ($Output + '.invalid.json'); throw }
    return [ordered]@{ run = $run; sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)); size = $bytes.Length }
}
# One A/B/B/A session: preflight, GPU engine snapshot, four runs (B under the arm's load, if any), GPU snapshot, and an
# admission decision for every run. Throws when the preflight refuses; returns the session otherwise.
function Invoke-TimingSession([int] $Number, [int] $Attempt, [object] $Arm) {
    $tag = if ($Attempt -gt 1) { "session-$Number-attempt-$Attempt" } else { "session-$Number" }
    if ($Arm) { $tag += "-arm-$($Arm['name'])" }
    $observerPath = Join-Path $rawDirectory "$tag-host.jsonl"
    $script:observerPath = $observerPath
    $script:observer = Start-HostObserver $observerPath
    Start-Sleep -Seconds $PreflightSeconds
    $preflight = Test-FrameTimingPreflight -Samples (Read-HostObservations $observerPath) -Rule $rule -LogicalProcessors $logicalProcessors
    $preflight['mode'] = $IdlePreflight
    $preflight['seconds'] = $PreflightSeconds
    if ($IdlePreflight -eq 'Require' -and -not $preflight.admitted) {
        throw ("Idle preflight refused session {0}: {1}. Never stop other people's work to make it quiet; rerun later." -f $Number, $preflight.reason)
    }
    $gpuBefore = if ($Purpose -ne 'LoadSensitivity') { Get-FrameTimingGpuEngineSnapshot } else { @{} }
    $gpuStart = [Diagnostics.Stopwatch]::StartNew()
    $runs = [Collections.Generic.List[object]]::new()
    for ($slot = 0; $slot -lt 4; $slot++) {
        $role = [string]$schedule[0][$slot]
        $output = Join-Path $rawDirectory ("$tag-slot-{0}-{1}.json" -f $slot, $role)
        $load = $null
        try {
            if ($Arm -and $role -ceq 'B' -and $Arm['processors'].Count) { $load = Start-FrameTimingLoad -Executable $candidate -Processors $Arm['processors'] }
            $measured = Invoke-TimingRun $roles[$role] $output
        } finally { Stop-FrameTimingLoad -Process $load }
        $script:reportSizes[$measured.sha256] = $measured.size
        $runs.Add([ordered]@{ role = $role; slot = $slot; report = $output; reportSha256 = $measured.sha256; raw = $measured.run
            executableSha256 = $hashes[$role]; summary = (Get-FrameTimingRunSummary -Run $measured.run)
            adapter = $measured.run['adapter']; pinning = $measured.run['pinning']; disjointFrames = $measured.run['disjointFrames']
            timestampFrequency = $measured.run['timestampFrequency'] })
    }
    $gpuSeconds = $gpuStart.Elapsed.TotalSeconds
    $gpuAfter = if ($Purpose -ne 'LoadSensitivity') { Get-FrameTimingGpuEngineSnapshot } else { @{} }
    Stop-HostObserver $script:observer $observerPath
    $script:observer = $null
    $luid = if ($runs[0]['adapter'] -is [Collections.IDictionary] -and $runs[0]['adapter'].Contains('luid')) { [string]$runs[0]['adapter']['luid'] } else { '' }
    $otherGpu = if ($luid) { Get-FrameTimingOtherGpuBusy -Before $gpuBefore -After $gpuAfter -Luid $luid -Seconds $gpuSeconds } else { $null }
    foreach ($run in $runs) { $run['admission'] = Test-FrameTimingRunAdmission -Summary $run['summary'] -OtherGpuBusy $otherGpu -Rule $rule }
    $record = [ordered]@{ session = $Number; attempt = $Attempt; order = $schedule[0]; idlePreflight = $preflight
        otherGpuBusy = $otherGpu; gpuWindowSeconds = $gpuSeconds; hostObservations = $observerPath
        hostObservationCompletion = (Get-Content -Raw -LiteralPath ($observerPath + '.completion.json') | ConvertFrom-Json -AsHashtable)
        admitted = [bool](@($runs | Where-Object { -not $_['admission']['admitted'] }).Count -eq 0); runs = $runs.ToArray() }
    if ($Arm) { $record['arm'] = $Arm['name']; $record['level'] = $Arm['level']; $record['placement'] = $Arm['placement']; $record['loadProcessors'] = $Arm['processors'] }
    return $record
}

$powerRecord = $null
$observer = $null
$sessionRecords = [Collections.Generic.List[object]]::new()
$rejectedSessions = [Collections.Generic.List[object]]::new()
$reportSizes = @{}
$schedule = @(Get-FrameTimingSchedule -Sessions $Sessions)
$startedUtc = [DateTime]::UtcNow.ToString('o')
Push-Location $PSScriptRoot
try {
    $powerRecord = Enter-FrameTimingPowerPlan -Mode $PowerPlan
    # Captured under the measured power scheme, so the before/after identity check spans the whole study.
    $before = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
    Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $before
    # Load-sensitivity sessions run arm after arm within each round, so drift reaches every arm alike.
    $number = 0
    for ($round = 0; $round -lt $Sessions; $round++) {
        foreach ($arm in $arms) {
            $number++
            for ($attempt = 1; ; $attempt++) {
                $record = Invoke-TimingSession -Number $number -Attempt $attempt -Arm $arm
                if ($record['admitted'] -or -not $enforceRuns) { $sessionRecords.Add($record); break }
                $reasons = @($record['runs'] | Where-Object { -not $_['admission']['admitted'] } | ForEach-Object { "slot $($_['slot']): $($_['admission']['reasons'] -join '; ')" })
                $rejectedSessions.Add([ordered]@{ session = $number; attempt = $attempt; reasons = $reasons; runs = @($record['runs'] | ForEach-Object {
                    [ordered]@{ slot = $_['slot']; role = $_['role']; reportSha256 = $_['reportSha256']; admission = $_['admission'] } }) })
                Write-Host "Session $number attempt $attempt rejected: $($reasons -join ' | ')"
                if ($attempt -ge $SessionAttempts) { throw "Session $number was rejected $attempt times; the study is refused: $($reasons -join ' | ')" }
            }
        }
    }
    $after = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
    Assert-PerformanceMeasurementState -Before $before -After $after
    Assert-PerformanceBuildAttestation -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration -State $after
    if ((ConvertTo-Json -Compress -InputObject (Get-TimingInputs)) -cne (ConvertTo-Json -Compress -InputObject $timingInputs)) {
        throw 'The frame timing inputs changed during the study; the receipt cannot be published.'
    }
} finally {
    try { if ($observer) { Stop-HostObserver $observer $observerPath } }
    finally {
        if ($powerRecord) { Exit-FrameTimingPowerPlan -Record $powerRecord }
        Pop-Location
    }
}

$receipt = [ordered]@{
    schema = 1; method = (Get-FrameTimingMethod); purpose = $Purpose; workloadOwner = 'DxUi'
    fixture = 'dxui-frame-timing-v1'; fixtureName = $Fixture
    benchmarkInputs = $timingInputs
    benchmarkInputsScope = 'The frame timing harness, its dispatcher, the FX5 scene, the performance fixture framework and catalog, the device fixture, the runner and the judge; build.benchmarkInputs is the canonical ComplexUi closure.'
    rawDirectory = $rawDirectory
    classification = $(switch ($Purpose) { 'Calibration' { 'same-binary calibration' } 'Comparison' { 'interleaved A/B comparison' } 'LoadSensitivity' { 'load sensitivity: quiet against synthetically loaded runs of one executable, per load level' } default { 'smoke run: exercises the method only; no calibration, noise floor or verdict' } })
    admission = [ordered]@{ rule = $rule; interim = [bool]$rule['interim']; enforcedPerRun = $enforceRuns; logicalProcessors = $logicalProcessors
        otherProcessorsCap = (Get-FrameTimingOtherProcessorCap -Rule $rule -LogicalProcessors $logicalProcessors); sessionAttempts = $SessionAttempts }
    rejectedSessions = $rejectedSessions.ToArray()
    phase = $Phase.ToLowerInvariant(); adapterMode = $Adapter
    startedUtc = $startedUtc; completedUtc = [DateTime]::UtcNow.ToString('o')
    runLength = [ordered]@{ minimumFrames = $MinimumFrames; minimumMilliseconds = $MinimumMilliseconds }
    pinCore = $PinCore; powerPlan = $powerRecord; schedule = @($sessionRecords | ForEach-Object { $_['order'] })
    executables = [ordered]@{ A = [ordered]@{ path = $roles.A; sha256 = $hashes.A }; B = [ordered]@{ path = $roles.B; sha256 = $hashes.B } }
    metrics = (Get-FrameTimingMetrics)
    sessions = $sessionRecords.ToArray()
    # Identity of this tree's build (B, and A in calibration and smoke runs), checked before and after the study.
    build = $before
}
if ($Purpose -eq 'Calibration') { $receipt['calibration'] = Get-FrameTimingCalibration -Sessions $sessionRecords.ToArray() }
elseif ($Purpose -eq 'Comparison') { $receipt['comparison'] = Compare-FrameTimingSessions -Sessions $sessionRecords.ToArray() -Calibration $calibration }
elseif ($Purpose -eq 'LoadSensitivity') { $receipt['loadSensitivity'] = Get-FrameTimingLoadSensitivity -Sessions $sessionRecords.ToArray() -Calibration $calibration; $receipt['arms'] = $arms }
else { $receipt['sameBinaryObservation'] = (Get-FrameTimingCalibration -Sessions $sessionRecords.ToArray())['metrics'] }
# The full receipt, every raw run report embedded, stays in the build tree; the retained form, recomputed from it and
# checked equal (Tools/Convert-FrameTimingReceipt.ps1 does the same for an existing full receipt), is what may be committed.
$fullReceiptPath = Join-Path $rawDirectory 'receipt-full.json'
[IO.File]::WriteAllText($fullReceiptPath, (ConvertTo-Json -InputObject $receipt -Depth 30 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
$receipt = ConvertTo-FrameTimingRetainedReceipt -Receipt $receipt -ReportSizes $reportSizes -Calibration $calibration
$receipt['fullReceipt'] = [ordered]@{ path = $fullReceiptPath; sha256 = (Get-FileHash -LiteralPath $fullReceiptPath -Algorithm SHA256).Hash; sizeBytes = (Get-Item -LiteralPath $fullReceiptPath).Length }
[IO.File]::WriteAllText($receiptPath, (ConvertTo-Json -InputObject $receipt -Depth 40 -Compress) + "`n", [Text.UTF8Encoding]::new($false))
$readmePath = Join-Path $OutputDirectory 'README.md'
if (-not (Test-Path -LiteralPath $readmePath)) {
    $adapterRun = $sessionRecords[0]['runs'][0]
    $busyText = @($sessionRecords | ForEach-Object { [string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0:0.0}%', $_['idlePreflight']['busyPercent']) }) -join ', '
    $admittedText = if ($rule['interim']) { "$($rule['id']) (interim), $($rejectedSessions.Count) rejected session attempts" } else { "$($rule['id']), $($rejectedSessions.Count) rejected session attempts" }
    $lines = @(
        "# Frame timing $Purpose`: $Fixture $($Phase.ToLowerInvariant())", '',
        "Generated by ``performance-timing.ps1``; method: Frame timing method in ``Specs/Core/Core_PerformanceAndResources.md``.", '',
        "- Classification: $($receipt.classification)",
        "- Source commit: $($before['sourceCommit']) (uncommitted changes: $($before['sourceDirty'])); build: $Platform $Configuration",
        "- Adapter: $($adapterRun['adapter']['description']), driver $($adapterRun['adapter']['driverVersion']); requested $Adapter",
        "- Sessions: $Sessions of A/B/B/A, each run at least $MinimumFrames frames and $MinimumMilliseconds ms; pinning: $PinCore",
        "- Power scheme: $($powerRecord.mode), original $($powerRecord.original.text), restored: $($powerRecord.restored)",
        "- Idle preflight: $IdlePreflight; busy $busyText",
        "- Admission: $admittedText",
        '- `receipt.json` is the retained form: per-run summaries, each raw report''s SHA-256 and size, and the judged statistics. The raw reports, the full receipt and the host observation logs stayed in the build tree (`rawDirectory`).')
    [IO.File]::WriteAllText($readmePath, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
}

if ($Purpose -eq 'LoadSensitivity') {
    foreach ($arm in $receipt['loadSensitivity']['arms']) { Write-Host ("arm {0,-8} level {1,-3} equivalent: {2}" -f $arm['arm'], $arm['level'], $arm['equivalent']) }
    Write-Host "Admissible level: $($receipt['loadSensitivity']['admissibleLevel']) busy logical processors away from the measuring core; sibling load equivalent: $($receipt['loadSensitivity']['siblingEquivalent'])"
    Write-Host "Frame timing receipt ($Purpose): $receiptPath"
    return
}
$table = if ($Purpose -eq 'Comparison') { $receipt['comparison']['metrics'] } elseif ($Purpose -eq 'Calibration') { $receipt['calibration']['metrics'] } else { $receipt['sameBinaryObservation'] }
foreach ($row in $table) {
    $state = if ($row.Contains('verdict')) { $row['verdict'] } elseif ($Purpose -eq 'Smoke') { 'smoke' } else { $row['status'] }
    $floor = if ($row.Contains('noiseFloorPercent') -and $null -ne $row['noiseFloorPercent']) { '{0:0.00}%' -f $row['noiseFloorPercent'] } else { '-' }
    $mde = if ($row.Contains('minimumDetectableEffectPercent') -and $null -ne $row['minimumDetectableEffectPercent']) { '{0:0.00}%' -f $row['minimumDetectableEffectPercent'] } else { '-' }
    Write-Host ("{0,-22} margin {1}%  noise floor {2,-8} MDE {3,-8} {4}" -f $row['metric'], $row['marginPercent'], $floor, $mde, $state)
}
Write-Host "Frame timing receipt ($Purpose): $receiptPath"
