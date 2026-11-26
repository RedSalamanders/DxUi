<#
.SYNOPSIS Runs the presentation-mode comparison (plan W0.9) and writes presentation-mode.json under Measurements.
.DESCRIPTION Tests/README.md, "Presentation-mode comparison". For each fixture (FX5 and FX1's text page), adapter (WARP and the default
hardware adapter) and order of the two swap-chain modes, a fresh DxUi.ControlTests.exe process runs `--presentation-modes`: the same
fixture on a WindowHost in HwndSwapChain and in CompositionSwapChain, with the pixel, cost and behaviour parts, the scope and the
presented-image read-back. This script adds provenance (source commit and dirty flag, build profile, OS, displays, power plan,
host-idle record, input hashes), merges the case documents into one receipt, and checks that the receipt carries all four parts on
both adapters. It changes nothing on the machine: the power plan is recorded, never switched, and nothing here runs
performance-timing.ps1 or a calibration. Every window it shows is non-activating and off-screen.
Timing is advisory. It is recorded as "not judged, machine busy" unless -Judged is given. -Judged takes at least 1,000 timed frames
per mode and judges only when the idle preflight (Tools/Watch-BenchmarkHost.ps1 and the interference rule of the W0.2 timing
harness) admits the host; otherwise the script stops before it measures anything. An existing receipt is never replaced without -Replace.
.EXAMPLE ./performance-presentation.ps1
.EXAMPLE ./performance-presentation.ps1 -Adapter Hardware -Judged -TimingFrames 2000 -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W0.9/judged
#>
[CmdletBinding()]
param(
    [ValidateSet('All','Warp','Hardware')][string] $Adapter = 'All',
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Release',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [string] $OutputRoot = 'Measurements/PerformanceAndMotion_2026-11-13/W0.9',
    [ValidateRange(30,100000)][int] $TimingFrames = 120,
    [ValidateRange(1,60)][int] $IdleSeconds = 3,
    [switch] $Judged,
    [switch] $Replace,
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'Tools/PresentationModes.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/FrameTiming.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/PerformanceFixtures.psm1') -Force

if ($Judged -and $TimingFrames -lt 1000) { throw '-Judged needs at least 1,000 timed frames per mode: pass -TimingFrames 1000 or more.' }
$target = Join-Path (Join-Path $PSScriptRoot $OutputRoot) 'presentation-mode.json'
if ((Test-Path -LiteralPath $target) -and -not $Replace) { throw "Preserve the existing receipt $target (use -Replace to measure it again deliberately)." }

if (-not $SkipBuild) {
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform -Projects 'DxUi.ControlTests'
    if ($LASTEXITCODE -ne 0) { throw "The presentation-mode build failed with exit code $LASTEXITCODE." }
}
$executable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.ControlTests.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Missing $executable; build first." }
$raw = Join-Path $PSScriptRoot ('.build/reports/presentation-modes/' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ', [Globalization.CultureInfo]::InvariantCulture))
New-Item -ItemType Directory -Force -Path $raw | Out-Null
function Read-Json([string] $Path) { return (Get-Content -Raw -LiteralPath $Path | ConvertFrom-Json -AsHashtable -Depth 60 -DateKind String) }

function Get-HostIdleRecord([string] $Path) {
    <# Tools/Watch-BenchmarkHost.ps1 for $IdleSeconds before the runs: an observation, and with -Judged the admission rule's input. #>
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $PSHOME 'pwsh.exe'))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    foreach ($argument in @('-NoProfile','-NonInteractive','-File',(Join-Path $PSScriptRoot 'Tools/Watch-BenchmarkHost.ps1'),'-OutputPath',$Path,'-IntervalMilliseconds','500')) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    try {
        $deadline = [DateTime]::UtcNow.AddSeconds(30)
        while (-not (Test-Path -LiteralPath ($Path + '.ready'))) {
            if ($process.HasExited -or [DateTime]::UtcNow -gt $deadline) { throw "The host observer did not start: $Path" }
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Seconds $IdleSeconds
        [IO.File]::WriteAllText($Path + '.stop', [DateTime]::UtcNow.ToString('o'))
        # The observer is this script's own child; only it is ever stopped forcibly.
        if (-not $process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit() }
    } finally { $process.Dispose() }
    $samples = @(Get-Content -LiteralPath $Path | Where-Object { $_ } | ForEach-Object { $_ | ConvertFrom-Json -AsHashtable -Depth 6 })
    $logical = [Environment]::ProcessorCount
    $preflight = Test-FrameTimingPreflight -Samples $samples -Rule (Get-FrameTimingAdmissionRule) -LogicalProcessors $logical
    return [ordered]@{ seconds = $IdleSeconds; busyPercent = $preflight['busyPercent']; busyProcessors = $preflight['busyProcessors']; logicalProcessors = $logical
        rule = $preflight['rule']; admitted = $preflight['admitted']; reason = $preflight['reason']; observer = 'Tools/Watch-BenchmarkHost.ps1' }
}

$idle = Get-HostIdleRecord (Join-Path $raw 'host.jsonl')
if ($Judged -and -not $idle['admitted']) { throw "The machine is not quiet enough to judge timing: $($idle['reason']). Nothing was measured." }

$adapters = if ($Adapter -eq 'All') { @('Warp','Hardware') } else { @($Adapter) }
$runs = [Collections.Generic.List[object]]::new()
foreach ($adapterName in $adapters) {
    foreach ($fixture in (Get-PresentationModeFixtures)) {
        $documents = @{}
        foreach ($order in @('hwnd-first','composition-first')) {
            $path = Join-Path $raw ("{0}-{1}-{2}.json" -f $fixture.ToLowerInvariant(), $adapterName.ToLowerInvariant(), $order)
            $global:LASTEXITCODE = 0
            & $executable '--presentation-modes' $path $fixture $adapterName $order $TimingFrames $(if ($Judged) { '1' } else { '0' }) '--no-activate' | Out-Host
            if ($LASTEXITCODE -eq 3 -and $adapterName -eq 'Hardware') { throw 'The hardware case needs a hardware Direct3D adapter and the machine has none; no receipt is written.' }
            if ($LASTEXITCODE -ne 0) { throw "The $fixture $adapterName case ($order) failed with exit code $LASTEXITCODE." }
            $documents[$order] = Read-Json $path
        }
        $runs.Add([pscustomobject]@{ Fixture = $fixture; Adapter = $adapterName; Primary = $documents['hwnd-first']; Reversed = $documents['composition-first'] })
    }
}

$commit = @(& git -C $PSScriptRoot rev-parse HEAD)[0].Trim()
$dirty = [bool]@(& git -C $PSScriptRoot status --porcelain).Count
$warp = Join-Path $env:SystemRoot 'System32/d3d10warp.dll'
$provenance = [ordered]@{
    sourceCommit = $commit; sourceDirty = $dirty; platform = $Platform; configuration = $Configuration
    command = "performance-presentation.ps1 -Adapter $Adapter -Configuration '$Configuration' -Platform $Platform -TimingFrames $TimingFrames$(if ($Judged) { ' -Judged' })"
    executableSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    os = (Get-PerformanceFixtureOsRecord); warpVersion = (Get-Item -LiteralPath $warp).VersionInfo.FileVersion
    displays = @(Get-PerformanceFixtureDisplayRecord); powerPlan = (Get-FrameTimingActiveScheme); hostIdle = $idle
    cpu = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
    recordedUtc = [DateTime]::UtcNow.ToString('o')
}
$timing = if ($Judged) {
    [ordered]@{ judged = $true; status = 'judged: the idle preflight admitted the host'; framesPerMode = $TimingFrames; rule = $idle['rule'] }
} else {
    [ordered]@{ judged = $false; status = 'not judged, machine busy'; framesPerMode = $TimingFrames
        reason = 'recorded while other work ran on the machine; timing advises only (A6: a timing verdict needs a quiet hardware GPU). Counters and read-back decide.' }
}
$judgedCommand = './performance-presentation.ps1 -Adapter Hardware -Judged -TimingFrames 2000 -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W0.9/judged -Replace'
$inputs = Get-PresentationModeInputHashes -Root $PSScriptRoot
$receipt = New-PresentationModeReceipt -Runs @($runs) -Provenance $provenance -Inputs $inputs -Timing $timing -JudgedCommand $judgedCommand
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
[IO.File]::WriteAllText($target, (ConvertTo-Json -InputObject $receipt -Depth 60) + "`n", [Text.UTF8Encoding]::new($false))
Write-Host "Presentation-mode receipt: $target"
foreach ($name in $receipt['parts'].Keys) { Write-Host ("  {0}: pixels {1}, cost {2}, behavior {3}, scope {4}" -f $name, $receipt['parts'][$name]['pixels'], $receipt['parts'][$name]['cost'], $receipt['parts'][$name]['behavior'], $receipt['parts'][$name]['scope']) }
foreach ($problem in $receipt['problems']) { Write-Host "  PROBLEM: $problem" }
if (@($receipt['problems']).Count -and $Adapter -eq 'All') { exit 1 }
