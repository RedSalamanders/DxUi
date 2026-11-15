<#
.SYNOPSIS Hardware attribution receipt (plan W0.7): where a frame's time goes on FX5, FX3 and FX12 at 1920x1080 and 3840x2160.
.DESCRIPTION Specs/Core/Core_PerformanceAndResources.md, "Hardware attribution method". For every case of the matrix
(Get-FrameAttributionCases: each fixture at both sizes, a full frame and a typical dirty frame, on the embedded host) three fresh
DxUi.EmbeddedTests.exe --frame-attribution processes run one pass each for at least -Seconds: timing (the frame timing method,
uninstrumented), phases (time inside EndDraw, Direct2D draw calls and DirectWrite creation through the counting wrappers, and the
W0.1 counters) and sampling (in-process sampling of the UI thread's instruction pointer by module). Two window-host cases run
DxUi.ControlTests.exe --present-attribution: FX12's full frames on a hardware WindowHost of the non-activating test window, off
every monitor, for Present. Kernel-trace sampling is probed once (wpr, unelevated) and recorded as refused or available.
Nothing on the machine is changed: the power scheme is read, never switched (a receipt refuses unless High performance is
already active), the idle preflight is an observation of Tools/Watch-BenchmarkHost.ps1, and nothing takes focus.
A pass is clean when the frame timing method's admission rule (-Admission, Tools/FrameTiming.psm1) admits it and at least
-MinimumWorkSharePercent of its frames did work; the sampling pass is not judged on running share, the present pass on host load. In a receipt run an unclean pass is measured again, after the host is quiet
again, up to -MaxPassAttempts times per pass and -MaxRetries times in all; every attempt is recorded. After every pass the run
writes attribution.checkpoint.json beside its raw reports; -ResumeFrom reuses the clean passes of earlier runs of the same
commit, build, adapter, driver and power scheme, no older than -ResumeMaxAgeHours, so a later slot measures only what is missing.
The receipt is published only when every case of the matrix has every pass clean; a run of part of the matrix (-CaseSet
Decision measures the two cases the W0.7 decision rule reads) writes attribution.partial.json, and an unclean run writes
attribution.rejected.json, both beside the raw reports. The retained receipt holds summaries only; raw reports stay in the
build tree with their SHA-256 in the receipt. -Purpose Smoke runs the same path with relaxed admission and no retries, and
labels everything as smoke.
.EXAMPLE ./performance-attribution.ps1 -Purpose Receipt -SkipBuild -CaseSet Decision
.EXAMPLE ./performance-attribution.ps1 -Purpose Receipt -SkipBuild -ResumeFrom .build/reports/frame-attribution/Receipt-Hardware-<utc>
.EXAMPLE ./performance-attribution.ps1 -Purpose Smoke -Adapter Warp -Seconds 1 -Cases 'FX5@fhd-144dpi full','FX12 present'
#>
[CmdletBinding()]
param(
    [ValidateSet('Receipt','Smoke')][string] $Purpose = 'Receipt',
    [ValidateSet('Hardware','Warp')][string] $Adapter = 'Hardware',
    [ValidateSet('All','Decision','Rest')][string] $CaseSet = 'All',
    [string[]] $Cases = @(),
    [ValidateSet('timing','phases','sampling')][string[]] $Passes = @('timing','phases','sampling'),
    [ValidateRange(0.1,600)][double] $Seconds = 10,
    [ValidateRange(1,200000)][int] $MinimumFrames = 100,
    [ValidatePattern('^(auto|none|[0-9]{1,2})$')][string] $PinCore = 'auto',
    [ValidateSet('Require','Record')][string] $IdlePreflight = 'Require',
    # The admission rule of the frame timing method (Tools/FrameTiming.psm1): Interim judges interference with the measuring
    # thread on an isolated core (interference-interim-v2); InterimV1 is the 15 November rule, Legacy10 the original 10% rule.
    [ValidateSet('Interim','InterimV1','Legacy10')][string] $Admission = 'Interim',
    [ValidateRange(1,60)][int] $PreflightSeconds = 5,
    [ValidateRange(0,3600)][int] $IdleWaitSeconds = 120,
    [ValidateRange(1,10)][int] $MaxPassAttempts = 3,
    [ValidateRange(0,200)][int] $MaxRetries = 12,
    [string[]] $ResumeFrom = @(),
    [ValidateRange(0.1,720)][double] $ResumeMaxAgeHours = 24,
    [string] $OutputDirectory = '',
    [string] $ReceiptPath = '',
    [string[]] $NoiseFloorReceipt = @(),
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [ValidateRange(0,100)][double] $MinimumWorkSharePercent = 95,
    # Leaves out the two window-host Present cases (an off-screen, non-activating test window).
    [switch] $SkipWindow,
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/FrameTiming.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/FrameAttribution.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceFixtures.psm1') -Force
$culture = [Globalization.CultureInfo]::InvariantCulture

$native = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64' -and $native -ne 'Arm64') -or ($Platform -eq 'x64' -and $native -ne 'X64')) {
    throw 'Frame attribution requires native execution matching the requested architecture.'
}
$powerScheme = Get-FrameTimingActiveScheme
$highPerformance = $powerScheme.guid -eq (Get-FrameTimingHighPerformanceScheme)
if ($Purpose -eq 'Receipt') {
    $refusals = @()
    if ($Adapter -ne 'Hardware') { $refusals += 'a hardware adapter (WARP is for counters and pixels only)' }
    if ($Configuration -ne 'Release') { $refusals += 'the Release configuration' }
    if ($Seconds -lt 10) { $refusals += 'at least 10 s per pass' }
    if ($IdlePreflight -ne 'Require') { $refusals += 'a required idle preflight' }
    if ($Platform -ne 'x64') { $refusals += 'x64 (thread cycles convert to time at the time-stamp counter rate)' }
    # Read, never switched: the developer sets the scheme for a hardware slot.
    if (-not $highPerformance) { $refusals += "the High performance power scheme already active (active: $($powerScheme.text); this script never switches it)" }
    if ($refusals.Count) { throw "A receipt requires $($refusals -join ', '). Use -Purpose Smoke to exercise the method without a receipt." }
} elseif ($ResumeFrom.Count) { throw '-ResumeFrom applies to -Purpose Receipt only.' }
$matrix = @(Get-FrameAttributionCases)
if ($SkipWindow) { $matrix = @($matrix | Where-Object { $_.host -ne 'window' }) }
$decisionCases = @('FX12@uhd-192dpi full','FX12@form-layout-10k-uhd full')
$selected = if ($Cases.Count) {
    @($Cases | ForEach-Object { $id = $_; $match = @($matrix | Where-Object { $_.id -ceq $id }); if (-not $match.Count) { throw "Unknown attribution case '$id'. Cases: $(($matrix | ForEach-Object id) -join '; ')" }; $match[0] })
} else {
    switch ($CaseSet) { 'All' { $matrix } 'Decision' { @($matrix | Where-Object { $_.id -in $decisionCases }) } 'Rest' { @($matrix | Where-Object { $_.id -notin $decisionCases }) } }
}
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ', $culture)
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $PSScriptRoot ".build/reports/frame-attribution/$Purpose-$Adapter-$stamp" }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$runId = Split-Path -Leaf $OutputDirectory
if (-not $ReceiptPath) {
    $ReceiptPath = if ($Purpose -eq 'Receipt') { Join-Path $PSScriptRoot 'Measurements/PerformanceAndMotion_2026-11-13/W0.7/attribution.json' } else { Join-Path $OutputDirectory 'attribution.json' }
}
$ReceiptPath = [IO.Path]::GetFullPath($ReceiptPath)
if (Test-Path -LiteralPath $ReceiptPath) { throw "Preserve the existing attribution receipt: $ReceiptPath" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$commandLine = 'performance-attribution.ps1 ' + (@($PSBoundParameters.GetEnumerator() | Sort-Object Key | ForEach-Object {
    if ($_.Value -is [switch]) { "-$($_.Key)" } else { "-$($_.Key) '$(@($_.Value) -join "','")'" } }) -join ' ')

if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform
    if ($LASTEXITCODE -ne 0) { throw "Frame attribution build failed with exit code $LASTEXITCODE." }
}
$executables = [ordered]@{ embedded = (Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.EmbeddedTests.exe")
    window = (Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.ControlTests.exe") }
foreach ($path in $executables.Values) { if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing $path; build first." } }
$rule = Get-FrameTimingAdmissionRule -Name $(switch ($Admission) { 'Legacy10' { 'host-busy-10pct-v1' } 'InterimV1' { 'interference-interim-v1' } default { 'interference-interim-v2' } })
$logicalProcessors = [Environment]::ProcessorCount
$hashes = [ordered]@{}; foreach ($key in $executables.Keys) { $hashes[$key] = (Get-FileHash -LiteralPath $executables[$key] -Algorithm SHA256).Hash }

function Invoke-AttributionChild([string] $Executable, [string[]] $Arguments, [string] $Log) {
    $startedUtc = [DateTime]::UtcNow
    $global:LASTEXITCODE = 0
    & $Executable @Arguments *> $Log
    return [ordered]@{ exitCode = $LASTEXITCODE; startedUtc = $startedUtc; completedUtc = [DateTime]::UtcNow }
}
function Start-HostObserver([string] $Path) {
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $PSScriptRoot 'Tools/Watch-BenchmarkHost.ps1'),'-OutputPath',$Path,'-IntervalMilliseconds','500')) { $start.ArgumentList.Add($argument) }
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
            if (-not $Process.WaitForExit(30000)) { $Process.Kill(); $Process.WaitForExit() }
        }
    } finally { $Process.Dispose() }
}
# The observer's log is read incrementally: each line is parsed once and kept compact (the host counters and only this run's
# own processes: the measured child, the observer and this runner), so reading it never grows with the run.
$script:ObserverOffset = 0L
$script:ObserverRemainder = ''
$script:ObserverSamples = [Collections.Generic.List[object]]::new()
function Update-HostObservations([string] $Path) {
    $stream = [IO.FileStream]::new($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        [void]$stream.Seek($script:ObserverOffset, [IO.SeekOrigin]::Begin)
        $reader = [IO.StreamReader]::new($stream, [Text.UTF8Encoding]::new($false))
        $text = $script:ObserverRemainder + $reader.ReadToEnd()
        $script:ObserverOffset = $stream.Position
    } finally { $stream.Dispose() }
    $lines = $text.Split("`n")
    $script:ObserverRemainder = $lines[-1]
    foreach ($line in $lines[0..($lines.Count - 2)]) {
        if (-not $line.Trim()) { continue }
        $sample = $line | ConvertFrom-Json -AsHashtable -Depth 6 -DateKind String
        $own = @(@($sample['processes']) | Where-Object { $null -ne $_ -and $_.Contains('name') -and (
            $_['name'] -in @('DxUi.EmbeddedTests','DxUi.ControlTests') -or $_['pid'] -eq $sample['observerPid'] -or $_['pid'] -eq $PID) } |
            ForEach-Object { @{ pid = $_['pid']; cpuTicks = $_['cpuTicks'] } })
        $script:ObserverSamples.Add(@{ utc = [datetime]::Parse([string]$sample['utc'], $culture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
            idle100ns = $sample['idle100ns']; kernelIncludingIdle100ns = $sample['kernelIncludingIdle100ns']; user100ns = $sample['user100ns']; own = $own })
    }
}
function Get-HostSamples([datetime] $From, [datetime] $To) {
    return @($script:ObserverSamples | Where-Object { $_['utc'] -ge $From -and $_['utc'] -le $To })
}
function Get-HostLoad([datetime] $From, [datetime] $To) {
    <# Whole-host busy share while a child ran, and what is left after this run's own CPU (the measured child, the observer and
       this runner, from the observer's process snapshots): the other processes' load. #>
    $window = Get-HostSamples $From.AddSeconds(-1) $To.AddSeconds(1)
    if ($window.Count -lt 2) { return [ordered]@{ samples = $window.Count; busyPercent = $null; ownPercent = $null; othersBusyPercent = $null } }
    $busy = Get-FrameTimingHostBusyPercent -Samples $window
    $total = ([double]$window[-1]['kernelIncludingIdle100ns'] - [double]$window[0]['kernelIncludingIdle100ns']) + ([double]$window[-1]['user100ns'] - [double]$window[0]['user100ns'])
    $own = @{}
    foreach ($sample in $window) {
        foreach ($process in @($sample['own'])) {
            $key = [string]$process['pid']; $ticks = [double]$process['cpuTicks']
            if (-not $own.ContainsKey($key)) { $own[$key] = @{ first = $ticks; last = $ticks; seenFirst = ($sample -eq $window[0]) } } else { $own[$key].last = $ticks }
        }
    }
    $ownTicks = 0.0
    foreach ($entry in $own.Values) { $ownTicks += $(if ($entry.seenFirst) { $entry.last - $entry.first } else { $entry.last }) }
    $ownPercent = if ($total -gt 0) { [Math]::Min(100.0, $ownTicks / $total * 100.0) } else { 0.0 }
    return [ordered]@{ samples = $window.Count; busyPercent = $busy; ownPercent = $ownPercent; othersBusyPercent = [Math]::Max(0.0, $busy - $ownPercent) }
}
function Wait-HostQuiet([string] $Path, [int] $MaximumSeconds) {
    <# Observes the host for PreflightSeconds at a time until the rule's preflight admits it, or MaximumSeconds pass. #>
    $started = [DateTime]::UtcNow
    $check = $null
    do {
        $from = [DateTime]::UtcNow
        Start-Sleep -Seconds $PreflightSeconds
        Update-HostObservations $Path
        $window = Get-HostSamples $from ([DateTime]::UtcNow)
        $check = if ($window.Count -ge 2) { Test-FrameTimingPreflight -Samples $window -Rule $rule -LogicalProcessors $logicalProcessors } else { $null }
        if ($check -and $check.admitted) { break }
    } while (([DateTime]::UtcNow - $started).TotalSeconds -lt $MaximumSeconds)
    return [ordered]@{ preflight = $check; busyPercent = $(if ($check) { $check.busyPercent } else { $null }); waitedSeconds = ([DateTime]::UtcNow - $started).TotalSeconds
        quiet = [bool]($check -and $check.admitted) }
}
function Test-KernelTracing {
    <# The documented unelevated route, once: wpr's built-in CPU profile. Refusal changes nothing; an accepted start is cancelled at once. #>
    $wpr = Join-Path $env:SystemRoot 'System32/wpr.exe'
    if (-not (Test-Path -LiteralPath $wpr)) { return [ordered]@{ available = $false; reason = 'wpr.exe is not installed' } }
    $instance = 'DxUiAttribution-' + [guid]::NewGuid().ToString('N')
    $global:LASTEXITCODE = 0
    $output = @(& $wpr -start CPU -instancename $instance 2>&1 | ForEach-Object { "$_".Trim() } | Where-Object { $_ })
    $exit = $LASTEXITCODE
    if ($exit -eq 0) { & $wpr -cancel -instancename $instance 2>&1 | Out-Null }
    return [ordered]@{
        available = [bool]($exit -eq 0); probe = "wpr.exe -start CPU -instancename $instance (unelevated; cancelled at once when accepted)"
        exitCode = $exit; exitCodeHex = ('0x{0:x8}' -f ([int64]$exit -band 4294967295)); output = $output
        reason = $(if ($exit -eq 0) { 'kernel sampling is available in this session; this receipt still uses in-process sampling' } else { 'this session lacks the rights to start kernel sampling (SeSystemProfilePrivilege); nothing was elevated' })
        administratorCommand = 'From an elevated prompt: wpr.exe -start CPU -filemode; run the timing pass of a case (DxUi.EmbeddedTests.exe --frame-attribution <out.json> FX12@uhd-192dpi full Hardware auto timing 200 10000); wpr.exe -stop attribution.etl; open attribution.etl in Windows Performance Analyzer, CPU Usage (Sampled) by module for the measuring thread.'
    }
}
function Get-PassSummary([string] $Pass, [Collections.IDictionary] $Parsed) {
    switch ($Pass) {
        'timing' { return Get-FrameAttributionTimingSummary -Report $Parsed }
        'phases' { return Get-FrameAttributionPhasesSummary -Report $Parsed }
        'sampling' { return Get-FrameAttributionSamplingSummary -Report $Parsed }
        'present' { return Get-FrameAttributionPresentSummary -Report $Parsed }
    }
}
function Get-PassAdmission([string] $Pass, [Collections.IDictionary] $Summary, [Collections.IDictionary] $Parsed, [Collections.IDictionary] $Load) {
    <# Whether a pass is clean: at least MinimumWorkSharePercent of its frames did work, and the frame timing method's
       admission rule admits it. Embedded passes are judged on the indicators the timing child recorded itself
       (Test-FrameTimingRunAdmission). The sampling pass suspends the measuring thread about every millisecond, so its running
       share is low by design (0.96 to 0.99): it is judged on every indicator but the running share. The window-host present
       pass has no pinned measuring thread or interference record: it is judged on other processes' load from the host
       observer, against the rule's cap (or the legacy 10%). Returns the admission with a problem text, or none. #>
    $problems = [Collections.Generic.List[string]]::new()
    $share = $Summary['workSharePercent']
    if ($null -eq $share) { $problems.Add('the frames'' work was not recorded') }
    elseif ([double]$share -lt $MinimumWorkSharePercent) { $problems.Add([string]::Format($culture, 'only {0:0.0}% of {1} frames did work', [double]$share, $Summary['frames'])) }
    $otherProcessors = if ($null -ne $Load['othersBusyPercent']) { [double]$Load['othersBusyPercent'] / 100.0 * $logicalProcessors } else { $null }
    if ($Pass -eq 'present') {
        $limit = if ($rule['id'] -ceq 'host-busy-10pct-v1') { [double]$rule['hostBusyPercentMax'] / 100.0 * $logicalProcessors } else { Get-FrameTimingOtherProcessorCap -Rule $rule -LogicalProcessors $logicalProcessors }
        $reasons = @()
        if ($null -eq $otherProcessors) { $reasons += 'no host observation during the pass' }
        elseif ($otherProcessors -gt $limit) { $reasons += [string]::Format($culture, 'other processes kept {0:0.00} logical processors busy, above {1}', $otherProcessors, $limit) }
        $passAdmission = [ordered]@{ rule = $rule['id']; judged = 'other processes'' load from the host observer (the window host''s thread is not pinned)'
            admitted = ($reasons.Count -eq 0); reasons = $reasons; indicators = [ordered]@{ otherProcessors = $otherProcessors; logicalProcessors = $logicalProcessors } }
    } else {
        $judgedRule = $rule
        $judged = 'every indicator of the rule'
        if ($Pass -eq 'sampling' -and $rule.Contains('runningShareMedianMin')) {
            $judgedRule = [ordered]@{}; foreach ($key in $rule.Keys) { $judgedRule[$key] = $rule[$key] }
            $judgedRule['runningShareMedianMin'] = 0.0; $judgedRule['runningShareP05Min'] = 0.0
            $judged = 'every indicator but the running share, which the sampler lowers by design'
        }
        $runSummary = Get-FrameTimingRunSummary -Run $Parsed['timing']
        $otherGpu = if ($Parsed.Contains('otherGpuBusy')) { $Parsed['otherGpuBusy'] } else { $null }
        $passAdmission = Test-FrameTimingRunAdmission -Summary $runSummary -OtherGpuBusy $otherGpu -Rule $judgedRule
        $passAdmission['judged'] = $judged
        # The measuring core's isolation from this process's other threads (FrameTiming::PinMeasuringThread, CPU sets).
        $pinning = $Parsed['timing']['pinning']
        foreach ($key in @('isolationRequested','isolated','isolationError','processor','core')) { if ($pinning -is [Collections.IDictionary] -and $pinning.Contains($key)) { $passAdmission['indicators'][$key] = $pinning[$key] } }
        if ($rule['id'] -ceq 'host-busy-10pct-v1' -and $null -ne $Load['othersBusyPercent'] -and [double]$Load['othersBusyPercent'] -gt [double]$rule['hostBusyPercentMax']) {
            # The legacy rule judges each pass on the host's other-process load, as this tool always did.
            $passAdmission['admitted'] = $false
            $passAdmission['reasons'] = @($passAdmission['reasons']) + [string]::Format($culture, 'other processes kept the host {0:0.0}% busy', [double]$Load['othersBusyPercent'])
        }
    }
    foreach ($reason in @($passAdmission['reasons'])) { $problems.Add($reason) }
    $passAdmission['problem'] = $(if ($problems.Count) { $problems -join '; ' } else { $null })
    return $passAdmission
}
function New-CaseRecord($Case) {
    $record = [ordered]@{}; foreach ($key in $Case.Keys) { $record[$key] = $Case[$key] }
    foreach ($key in @('raw','hostLoad','admission','attempts','origin')) { $record[$key] = [ordered]@{} }
    return $record
}
function Complete-CaseRecord([Collections.IDictionary] $Record) {
    $passList = @($Record['origin'].Keys)
    if ($Record.Contains('timing') -and $Record.Contains('phases')) {
        $plain = [double]$Record['timing']['prepareWallMsMedian']; $instrumented = [double]$Record['phases']['prepareWallMsMedian']
        $Record['phasesInstrumentationOverheadPercent'] = $(if ($plain -gt 0) { ($instrumented / $plain - 1.0) * 100.0 } else { $null })
    }
    # The case's conditions at a glance: frames per pass, the share that did real work, the other processes' load.
    $quality = [ordered]@{ frames = [ordered]@{}; workSharePercent = [ordered]@{}; afterRebuildSharePercent = [ordered]@{}; othersBusyPercent = [ordered]@{}; admitted = [ordered]@{}; attempts = [ordered]@{} }
    foreach ($pass in $passList) {
        $quality.frames[$pass] = $Record[$pass]['frames']; $quality.workSharePercent[$pass] = $Record[$pass]['workSharePercent']
        $quality.afterRebuildSharePercent[$pass] = $(if ($Record[$pass].Contains('afterRebuildSharePercent')) { $Record[$pass]['afterRebuildSharePercent'] } else { $null })
        $others = $Record['hostLoad'][$pass]['othersBusyPercent']
        $quality.othersBusyPercent[$pass] = $others
        $quality.admitted[$pass] = $(if ($Record['admission'].Contains($pass)) { $Record['admission'][$pass]['admitted'] } else { $null })
        $quality.attempts[$pass] = @($Record['attempts'][$pass]).Count
    }
    $Record['quality'] = $quality
}

# Earlier runs whose clean passes this run may reuse: the same commit, build, executables, power scheme and run length, and
# young enough. The adapter and driver are compared once this run knows its own.
$resumed = @()
$identity = $null
$kernel = Test-KernelTracing
$state = $null
if ($Configuration -eq 'Release' -and $Purpose -eq 'Receipt') {
    Import-Module (Join-Path $PSScriptRoot 'Tools/PerformancePolicy.psm1') -Force
    $state = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
}
$commit = @(& git -C $PSScriptRoot rev-parse HEAD)[0].Trim()
$dirty = [bool]@(& git -C $PSScriptRoot status --porcelain).Count
$identity = [ordered]@{ commit = $commit; dirty = $dirty; attestation = $(if ($state) { $state['buildAttestationIdentity'] } else { $null }); executablesSha256 = $hashes
    powerScheme = $powerScheme.guid; platform = $Platform; configuration = $Configuration; secondsPerPass = $Seconds; minimumFrames = $MinimumFrames; pinCore = $PinCore
    admissionRule = $rule['id']; minimumWorkSharePercent = $MinimumWorkSharePercent }
foreach ($source in $ResumeFrom) {
    $path = [IO.Path]::GetFullPath($source)
    if (Test-Path -LiteralPath $path -PathType Container) { $path = Join-Path $path 'attribution.checkpoint.json' }
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "No attribution checkpoint to resume from: $path" }
    $previous = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json -AsHashtable -Depth 40 -DateKind String
    $age = ([DateTime]::UtcNow - [datetime]::Parse([string]$previous['startedUtc'], $culture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()).TotalHours
    if ($age -gt $ResumeMaxAgeHours) { throw ([string]::Format($culture, 'The run at {0} is {1:0.0} h old; -ResumeMaxAgeHours is {2}.', $path, $age, $ResumeMaxAgeHours)) }
    foreach ($key in $identity.Keys) {
        if ((ConvertTo-Json -Compress -Depth 5 -InputObject $identity[$key]) -cne (ConvertTo-Json -Compress -Depth 5 -InputObject $previous['identity'][$key])) {
            throw "The run at $path cannot be resumed: its $key differs from this run's."
        }
    }
    $resumed += , [ordered]@{ path = $path; runId = $previous['runId']; adapter = $previous['adapter']; cases = @($previous['cases']) }
}

$observerPath = Join-Path $OutputDirectory 'host.jsonl'
$checkpointPath = Join-Path $OutputDirectory 'attribution.checkpoint.json'
$observer = $null
$records = [Collections.Generic.List[object]]::new()
$adapterIdentity = $null
$preflight = $null
$retriesUsed = 0
$startedUtc = [DateTime]::UtcNow.ToString('o')
function Save-Checkpoint {
    $checkpoint = [ordered]@{ schema = 1; method = (Get-FrameAttributionMethod); runId = $runId; startedUtc = $startedUtc; identity = $identity; adapter = $adapterIdentity
        cases = $records.ToArray() }
    [IO.File]::WriteAllText($checkpointPath, (ConvertTo-Json -InputObject $checkpoint -Depth 30) + "`n", [Text.UTF8Encoding]::new($false))
}
try {
    $observer = Start-HostObserver $observerPath
    Start-Sleep -Seconds $PreflightSeconds
    Update-HostObservations $observerPath
    $check = Test-FrameTimingPreflight -Samples @($script:ObserverSamples) -Rule $rule -LogicalProcessors $logicalProcessors
    $preflight = [ordered]@{ mode = $IdlePreflight; seconds = $PreflightSeconds; rule = $rule['id']; busyPercent = $check.busyPercent; busyProcessors = $check.busyProcessors
        logicalProcessors = $logicalProcessors; quiet = [bool]$check.admitted; reason = $check.reason }
    if ($IdlePreflight -eq 'Require' -and -not $preflight.quiet) {
        # A refused preflight waits for the host to become quiet, as the frame timing method's does, before giving up.
        $quiet = Wait-HostQuiet $observerPath $IdleWaitSeconds
        $preflight['retry'] = $quiet
        if ($quiet.quiet) { $preflight.quiet = $true; $check = $quiet.preflight }
    }
    if ($IdlePreflight -eq 'Require' -and -not $preflight.quiet) {
        throw "Idle preflight refused ($($rule['id'])): $($check.reason). Never stop other people's work to make it quiet; rerun in a quiet slot."
    }
    $milliseconds = [int][Math]::Ceiling($Seconds * 1000)
    foreach ($case in $selected) {
        $record = New-CaseRecord $case
        $records.Add($record)
        $slug = ($case.id -replace '[^A-Za-z0-9-]+', '-').Trim('-')
        $passList = if ($case.host -eq 'window') { @('present') } else { $Passes }
        foreach ($pass in $passList) {
            # A clean pass of an earlier run of the same build is reused as it was measured.
            $reuse = $null
            foreach ($run in $resumed) {
                $earlier = @($run.cases | Where-Object { $_['id'] -ceq $case.id -and $_.Contains($pass) })
                if ($earlier.Count -and $earlier[0].Contains('admission') -and $earlier[0]['admission'].Contains($pass) -and $earlier[0]['admission'][$pass]['rule'] -ceq $rule['id'] -and $null -eq $earlier[0]['admission'][$pass]['problem']) { $reuse = @{ run = $run; case = $earlier[0] } }
            }
            if ($reuse) {
                Write-Host "[$Purpose] $($case.id): $pass pass reused from $($reuse.run.runId)"
                foreach ($key in @('width','height','dpi')) { if ($reuse.case.Contains($key)) { $record[$key] = $reuse.case[$key] } }
                $record[$pass] = $reuse.case[$pass]; $record['raw'][$pass] = $reuse.case['raw'][$pass]; $record['hostLoad'][$pass] = $reuse.case['hostLoad'][$pass]
                $record['admission'][$pass] = $reuse.case['admission'][$pass]
                $record['attempts'][$pass] = @($reuse.case['attempts'][$pass]); $record['origin'][$pass] = $reuse.case['origin'][$pass]
                Save-Checkpoint
                continue
            }
            $attempts = [Collections.Generic.List[object]]::new()
            for ($attempt = 1; ; $attempt++) {
                $suffix = if ($attempt -gt 1) { "-attempt$attempt" } else { '' }
                $report = Join-Path $OutputDirectory "$slug-$pass$suffix.json"
                $log = Join-Path $OutputDirectory "$slug-$pass$suffix.log"
                $arguments = if ($pass -eq 'present') { @('--present-attribution', $report, $case.selection, $Adapter, "$MinimumFrames", "$milliseconds") }
                    else { @('--frame-attribution', $report, $case.selection, $case.frame, $Adapter, $PinCore, $pass, "$MinimumFrames", "$milliseconds") }
                Write-Host "[$Purpose] $($case.id): $pass pass$(if ($attempt -gt 1) { ", attempt $attempt" })"
                $child = Invoke-AttributionChild $executables[$(if ($pass -eq 'present') { 'window' } else { 'embedded' })] $arguments $log
                if ($child.exitCode -ne 0) { throw "The $pass pass of $($case.id) failed with exit code $($child.exitCode). Log: $log" }
                $bytes = [IO.File]::ReadAllBytes($report)
                $parsed = [Text.UTF8Encoding]::new($false, $true).GetString($bytes) | ConvertFrom-Json -AsHashtable -Depth 30
                $summary = Get-PassSummary $pass $parsed
                if ($pass -ne 'present') {
                    if ($Adapter -eq 'Hardware' -and $parsed['timing']['adapter']['software']) { throw "The $pass pass of $($case.id) ran on a software adapter." }
                    if ($null -eq $adapterIdentity) { $adapterIdentity = $parsed['timing']['adapter'] }
                }
                Update-HostObservations $observerPath
                $load = Get-HostLoad $child.startedUtc $child.completedUtc
                $passAdmission = Get-PassAdmission $pass $summary $parsed $load
                $problem = $passAdmission['problem']
                $raw = [ordered]@{ report = $report; sha256 = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)); log = $log }
                $attempts.Add([ordered]@{ attempt = $attempt; clean = ($null -eq $problem); problem = $problem; othersBusyPercent = $load['othersBusyPercent']
                    workSharePercent = $summary['workSharePercent']; frames = $summary['frames']; admission = $passAdmission; report = $raw })
                $retry = $Purpose -eq 'Receipt' -and $null -ne $problem -and $attempt -lt $MaxPassAttempts -and $retriesUsed -lt $MaxRetries
                if (-not $retry) {
                    if ($pass -ne 'present') { $record['width'] = [int]$parsed['timing']['width']; $record['height'] = [int]$parsed['timing']['height']; $record['dpi'] = [double]$parsed['timing']['dpi'] }
                    $record[$pass] = $summary; $record['raw'][$pass] = $raw; $record['hostLoad'][$pass] = $load; $record['admission'][$pass] = $passAdmission
                    break
                }
                $retriesUsed++
                $quiet = Wait-HostQuiet $observerPath $IdleWaitSeconds
                Write-Host ([string]::Format($culture, '[{0}] {1}: {2} pass not clean ({3}); host {4:0.0}% busy after {5:0} s of waiting, measuring again', $Purpose, $case.id, $pass, $problem, $quiet.busyPercent, $quiet.waitedSeconds))
                $attempts[$attempts.Count - 1]['waitBeforeNext'] = $quiet
            }
            $record['attempts'][$pass] = $attempts.ToArray()
            $record['origin'][$pass] = $runId
            Save-Checkpoint
        }
        Complete-CaseRecord $record
        Save-Checkpoint
    }
    if ($state) {
        $after = Get-PerformanceMeasurementState -Root $PSScriptRoot -Platform $Platform -Configuration $Configuration
        Assert-PerformanceMeasurementState -Before $state -After $after
    }
} finally {
    if ($observer) { Stop-HostObserver $observer $observerPath }
}
foreach ($key in $executables.Keys) {
    if ((Get-FileHash -LiteralPath $executables[$key] -Algorithm SHA256).Hash -cne $hashes[$key]) { throw "The $key executable changed during the run; the receipt cannot be published." }
}
foreach ($run in $resumed) {
    if ($null -ne $run.adapter -and $null -ne $adapterIdentity -and (ConvertTo-Json -Compress -InputObject $run.adapter) -cne (ConvertTo-Json -Compress -InputObject $adapterIdentity)) {
        throw "The run at $($run.path) measured another adapter or driver; its passes cannot be combined with this run's."
    }
    if ($null -eq $adapterIdentity) { $adapterIdentity = $run.adapter }
}

$conditions = Get-FrameAttributionConditions -Cases $records.ToArray() -MinimumWorkSharePercent $MinimumWorkSharePercent
$decisions = @()
foreach ($caseId in $decisionCases) {
    $record = @($records | Where-Object { $_.id -ceq $caseId })
    if ($record.Count -and $record[0].Contains('timing')) {
        $decisions += Get-FrameAttributionDecision -Case $caseId -Timing $record[0]['timing'] -Sampling $(if ($record[0].Contains('sampling')) { $record[0]['sampling'] } else { $null }) -Phases $(if ($record[0].Contains('phases')) { $record[0]['phases'] } else { $null })
    }
}
$noiseFloors = @($NoiseFloorReceipt | ForEach-Object {
    $calibration = Get-Content -Raw -LiteralPath $_ | ConvertFrom-Json -AsHashtable -Depth 30
    if ($calibration['purpose'] -cne 'Calibration') { throw "Not a frame timing calibration receipt: $_" }
    [ordered]@{ receipt = [IO.Path]::GetRelativePath($PSScriptRoot, [IO.Path]::GetFullPath($_)).Replace('\','/'); fixture = $calibration['fixtureName']; phase = $calibration['phase']
        metrics = @($calibration['calibration']['metrics'] | ForEach-Object { [ordered]@{ metric = $_['metric']; noiseFloorPercent = $_['noiseFloorPercent']; status = $_['status'] } }) }
})
$complete = @($matrix | Where-Object { $id = $_.id; -not @($records | Where-Object { $_.id -ceq $id }).Count }).Count -eq 0
$receipt = [ordered]@{
    schema = 1; method = (Get-FrameAttributionMethod); item = 'W0.7'; purpose = $Purpose; runId = $runId
    classification = $(if ($Purpose -eq 'Smoke') { 'smoke run: exercises the method only; no receipt, reading or verdict' } elseif ($complete) { 'hardware attribution receipt' } else { 'partial hardware attribution run: not every case of the matrix' })
    command = $commandLine; startedUtc = $startedUtc; completedUtc = [DateTime]::UtcNow.ToString('o')
    source = [ordered]@{ commit = $commit; dirty = $dirty }
    build = [ordered]@{ platform = $Platform; configuration = $Configuration; executablesSha256 = $hashes; attestation = $identity['attestation'] }
    machine = [ordered]@{ cpu = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
        os = (Get-PerformanceFixtureOsRecord); displays = @(Get-PerformanceFixtureDisplayRecord) }
    adapter = $adapterIdentity; adapterMode = $Adapter
    powerScheme = [ordered]@{ guid = $powerScheme.guid; text = $powerScheme.text; highPerformance = $highPerformance; changedByThisRun = $false }
    pinCore = $PinCore; secondsPerPass = $Seconds; minimumFrames = $MinimumFrames; skipWindow = [bool]$SkipWindow
    retries = [ordered]@{ maxPassAttempts = $MaxPassAttempts; maxRetries = $MaxRetries; used = $retriesUsed; idleWaitSeconds = $IdleWaitSeconds }
    resumedFrom = @($resumed | ForEach-Object { [ordered]@{ runId = $_.runId; checkpoint = $_.path } }); passOrigins = 'cases[].origin'
    admission = [ordered]@{ rule = $rule; logicalProcessors = $logicalProcessors; perPass = 'cases[].admission (indicators and decision), cases[].attempts[].admission'
        samplingPass = 'judged on every indicator but the running share, which the sampler lowers by design'
        presentPass = 'judged on other processes'' load from the host observer: the window host''s thread is not pinned and records no interference'
        coreIsolation = 'every embedded pass pins its measuring thread with the isolating form (FrameTiming::PinMeasuringThread); cases[].admission[].indicators carry isolated and isolationError'
        sessionSpread = 'the rule''s within-session spread applies to A/B/B/A sessions of the frame timing method; an attribution pass is one run and has none' }
    idle = [ordered]@{ preflight = $preflight; observer = 'Tools/Watch-BenchmarkHost.ps1'; perPass = 'cases[].hostLoad, cases[].attempts' }
    kernelSampling = $kernel
    moduleClasses = [ordered]@{ dxui = 'DxUi.EmbeddedTests.exe: the statically linked library and the fixture model code it calls'; d2d1 = 'd2d1.dll'; dwrite = 'dwrite.dll, textshaping.dll'
        d3d11AndDriver = 'd3d11.dll, dxgi.dll, dxcore.dll, d3d10warp.dll and every module under the driver store (the user-mode driver)'; crt = 'ucrtbase.dll, vcruntime140, msvcp140'
        system = 'ntdll.dll, kernelbase.dll, kernel32.dll, win32u.dll (system calls and waits)'; other = 'every other module' }
    cases = $records.ToArray()
    decision = $decisions
    noiseFloors = $noiseFloors
    conditions = $conditions
    rawDirectory = $OutputDirectory
}
foreach ($record in $records) {
    $line = "{0,-34} {1}" -f $record.id, $(if ($record.Contains('timing')) { [string]::Format($culture, 'cpu {0:0.000} ms  gpu busy {1:0.000} ms  gpu timeline {2:0.000} ms', $record['timing']['cpuFrameMsMedian'], $record['timing']['gpuBusyMsMean'], $record['timing']['gpuFrameMsMedian']) }
        elseif ($record.Contains('present')) { [string]::Format($culture, 'present {0:0.000} ms  render {1:0.000} ms', $record['present']['presentMsMedian'], $record['present']['renderMsMedian']) } else { '' })
    Write-Host $line
}
foreach ($decision in $decisions) { Write-Host ([string]::Format($culture, 'Decision rule on {0}: {1} (cpu {2:0.000} ms, gpu {3:0.000} ms, inside Direct2D {4:0.0}%)', $decision.case, $decision.outcome, $decision.cpuFrameMsMedian, $decision.gpuMs, $decision.insideDirect2DSharePercent)) }
if ($Purpose -eq 'Receipt' -and -not $conditions.met) {
    # Never published where a good receipt would be: kept beside the raw reports, and the run fails. Its clean passes stay
    # in the checkpoint for -ResumeFrom.
    $rejected = Join-Path $OutputDirectory 'attribution.rejected.json'
    $receipt['classification'] = 'rejected: measured under conditions a receipt does not admit (see conditions.problems)'
    [IO.File]::WriteAllText($rejected, (ConvertTo-Json -InputObject $receipt -Depth 30) + "`n", [Text.UTF8Encoding]::new($false))
    throw "The attribution receipt was not published: $($conditions.problems -join '; '). Rejected record: $rejected. Resume with -ResumeFrom $OutputDirectory"
}
if ($Purpose -eq 'Receipt' -and -not $complete) {
    $partial = Join-Path $OutputDirectory 'attribution.partial.json'
    [IO.File]::WriteAllText($partial, (ConvertTo-Json -InputObject $receipt -Depth 30) + "`n", [Text.UTF8Encoding]::new($false))
    Write-Host "Partial attribution run (not a receipt): $partial. Complete it with -ResumeFrom $OutputDirectory"
    return
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ReceiptPath) | Out-Null
[IO.File]::WriteAllText($ReceiptPath, (ConvertTo-Json -InputObject $receipt -Depth 30) + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Frame attribution receipt ($Purpose): $ReceiptPath"
