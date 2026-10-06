<#
.SYNOPSIS Builds and runs Foundation, inherited controls, and supplied-device tests with per-suite receipts.
.PARAMETER Tests Test function names (exact, case-sensitive) that limit each control suite to those tests, passed to
DxUi.ControlTests.exe as --test=. Every name must be a test of every requested control suite, so pair it with one -Suites value;
an unknown name fails the run. Foundation and Embedded ignore it. The receipt of a filtered run is written to a separate
*.filtered.json file, never over the receipt of the whole suite.
.PARAMETER TestTimeout Seconds each control test (or fixture suite without named tests) may run before the runner's watchdog
ends the run with its name and exit code 124, passed to DxUi.ControlTests.exe as --test-timeout=. 0 turns the watchdog off; the
executable's own default applies when omitted. Foundation and Embedded ignore it.
.PARAMETER Interactive Runs only the control suites that need the real desktop (real focus, foreground and pointer): Menu and
NativeTextInput, or the suites named in -Suites, every one of which must be such a suite (the MenuResources and MenuResourceScaling
fixtures are too). They run under DxUi.InteractiveLease.exe, after everything else a run checks, and take the desktop for about two
minutes. The lease refuses where there is no interactive desktop (a CI job, a service, a locked or disconnected session), asks first
(Cancel is the default button and the answer when nobody answers in two minutes), shows a warning while the suites run, and puts the
foreground window, its keyboard focus and the pointer position back however the run ends: a failing suite, the watchdog's exit code
124 and Ctrl+C included. A suite that records a capability skip fails the run, since an interactive run exists to run what other runs
skip. Run it only when the person at the desktop has agreed to the time. Without -Interactive the lease is never used.
.PARAMETER SkipTooling
Leaves independent tooling tests to the selected Tooling scope or the CI validation job. Native runner/watchdog checks remain.
.PARAMETER Full
Runs the complete noninteractive gate; ordinary calls use Test-Changes affected iteration.
#>
[CmdletBinding()]
param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64',
    [switch] $SkipBuild,
    [string] $PerformanceBaseline = '',
    [string[]] $Suites = @('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease'),
    [string[]] $Tests = @(),
    [ValidateRange(0, 999999)][Nullable[int]] $TestTimeout = $null,
    [switch] $Interactive,
    [switch] $SkipTooling,
    [switch] $Full
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Ordinary iteration uses source impact. Explicit suites and Full retain the lower-level runner surface.
if (-not $Full -and -not $PSBoundParameters.ContainsKey('Suites') -and
    @($PSBoundParameters.Keys | Where-Object { $_ -notin @('Platform','Configuration','SkipBuild') }).Count -eq 0) {
    & (Join-Path $PSScriptRoot 'Test-Changes.ps1') -Configuration $Configuration -Platform $Platform -SkipBuild:$SkipBuild
    exit $LASTEXITCODE
}
Import-Module (Join-Path $PSScriptRoot 'Tools/SuiteFailure.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/InteractiveRun.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Tools/ScopedTesting.psm1') -Force
# A comma-separated single value is accepted like an array, so -Tests 'A,B' and -Tests A,B are the same request.
$Tests = @($Tests | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$Suites = @($Suites | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
# Hosted jobs own their desktop. Preserve the existing CI coverage while local iteration requires the person's lease.
if ($Full -and -not $PSBoundParameters.ContainsKey('Suites') -and $env:GITHUB_ACTIONS -eq 'true') {
    $Suites += @('Menu', 'NativeTextInput')
}
$knownSuites = @((Read-ScopedTestManifest $PSScriptRoot).scopes.name | Where-Object { $_ -ne 'Tooling' }) + @('Menu','NativeTextInput','MenuResources','MenuResourceScaling','MenuTextLayoutResources','Gallery','ButtonContrast')
foreach ($suite in $Suites) { if ($suite -notin $knownSuites) { throw "Unknown test suite '$suite'." } }
if (-not $Interactive -and $env:GITHUB_ACTIONS -ne 'true' -and @($Suites | Where-Object { Test-DxUiInteractiveSuite $_ }).Count) {
    throw 'Local foreground suites require -Interactive and agreement to the time.'
}
# -Interactive is settled before anything is built or run: its suites must need the desktop, and there must be one to take.
if ($Interactive) {
    $Suites = @(Resolve-DxUiInteractiveSuites -Suites $Suites -Requested $PSBoundParameters.ContainsKey('Suites'))
    $refusal = Get-DxUiInteractiveRefusal
    if ($refusal) { throw "Interactive tests need an interactive desktop, and there is none: $refusal." }
}
if ($Tests.Count -and -not @($Suites | Where-Object { $_ -notin @('Foundation','Embedded') }).Count) { throw '-Tests selects tests within DxUi.ControlTests.exe suites; none of the requested suites is one.' }
if (-not $SkipTooling) {
    & (Join-Path $PSScriptRoot 'Tools/tests/Invoke-ToolingTests.ps1')
    & (Join-Path $PSScriptRoot 'Tools/tests/Test-AsanRuntime.ps1')
}
$nativeArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
if (($Platform -eq 'ARM64') -and ($nativeArchitecture -ne 'Arm64')) { throw 'ARM64 runtime tests require an ARM64 host; use build.ps1 for cross-compilation.' }
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Platform $Platform }
$leaseExecutable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.InteractiveLease.exe"
if ($Interactive) {
    # The session is asked again, natively, for what the environment cannot say (a locked screen, a disconnected session, a screen
    # saver), before the rest of the run spends its minutes: nothing is shown or taken by this check.
    if (-not (Test-Path -LiteralPath $leaseExecutable -PathType Leaf)) { throw "The interactive desktop lease is missing: $leaseExecutable. Build it with build.ps1 (not -SkipBuild after an older build)." }
    $noDesktop = Test-DxUiDesktopAvailable -Executable $leaseExecutable
    if ($noDesktop) { throw "Interactive tests need an interactive desktop, and there is none: $noDesktop." }
}
# The runner's --test contract (single tests, unknown names) and its watchdog (a hung test ends the run, named) are checked
# against the executable this run is about to use; an interactive run needs the second most, since the watchdog is what bounds a
# hung test while the suites hold the person's desktop.
if (@($Suites | Where-Object { $_ -notin @('Foundation','Embedded') }).Count) {
    & (Join-Path $PSScriptRoot 'Tools/tests/Test-TestFilter.ps1') -Configuration $Configuration -Platform $Platform
    & (Join-Path $PSScriptRoot 'Tools/tests/Test-TestWatchdog.ps1') -Configuration $Configuration -Platform $Platform
}
# The lease's Windows services (its children, its confirmation and its warning, on a private desktop) are proved with the suite that
# covers the lease's decisions, and before an interactive run takes a person's desktop.
if ($Interactive -or 'InteractiveLease' -in $Suites) {
    & (Join-Path $PSScriptRoot 'Tools/tests/Test-InteractiveLease.ps1') -Configuration $Configuration -Platform $Platform
}
$reports = Join-Path $PSScriptRoot '.build/reports'
$logs = Join-Path $PSScriptRoot '.build/logs'
New-Item -ItemType Directory -Path $reports,$logs -Force | Out-Null
# Each test invocation retains its own raw rounds, including noisy or failing comparisons.
$performanceReport = Join-Path $reports "Performance-$Platform-$Configuration-$([guid]::NewGuid().ToString('N')).json"
& (Join-Path $PSScriptRoot 'performance.ps1') -Configuration $Configuration -Platform $Platform -SkipBuild -OutputPath $performanceReport -Baseline $PerformanceBaseline
$performance = Get-Content -Raw -LiteralPath $performanceReport | ConvertFrom-Json
$performanceComparison = Get-Content -Raw -LiteralPath ($performanceReport + '.comparison.json') | ConvertFrom-Json
$failures = @()
if ($Configuration -eq 'ASan Debug') {
    $probe = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/DxUi.FoundationTests.exe"
    $probeLog = Join-Path $logs "test-AddressSanitizer-$Platform.log"
    $previousOptions = $env:ASAN_OPTIONS
    try {
        $env:ASAN_OPTIONS = 'halt_on_error=1:abort_on_error=0:detect_leaks=0'
        & $probe --asan-probe *> $probeLog
        $probeExit = $LASTEXITCODE
    } finally { $env:ASAN_OPTIONS = $previousOptions }
    $detected = $probeExit -ne 0 -and [bool](Select-String -LiteralPath $probeLog -SimpleMatch 'AddressSanitizer: heap-use-after-free')
    [ordered]@{suite='AddressSanitizer';configuration=$Configuration;platform=$Platform;nativeArchitecture=$nativeArchitecture;
        completedUtc=[DateTime]::UtcNow.ToString('o');executable=$probe;sha256=(Get-FileHash -LiteralPath $probe -Algorithm SHA256).Hash;
        exitCode=$probeExit;detected=$detected;log=$probeLog} | ConvertTo-Json | Set-Content (Join-Path $reports "AddressSanitizer-$Platform.json")
    if (-not $detected) { throw "AddressSanitizer did not diagnose the deliberate isolated use-after-free. See $probeLog" }
    Write-Host 'PASS AddressSanitizer detection probe (isolated expected failure)'
}
# What one suite is run with. A suite that needs real focus never gets --no-activate; every other control suite always does, so a
# run without -Interactive never reaches for the desktop through the lease.
function Get-SuiteRun([string] $Suite) {
    $name = switch ($Suite) { Foundation {'DxUi.FoundationTests.exe'} Embedded {'DxUi.EmbeddedTests.exe'} default {'DxUi.ControlTests.exe'} }
    $executable = Join-Path $PSScriptRoot ".build/$Platform/$Configuration/$name"
    $arguments = @(if ($Suite -in @('Foundation','Embedded')) { @() } elseif (Test-DxUiInteractiveSuite $Suite) { @("--suite=$Suite") } else { @("--suite=$Suite",'--no-activate') })
    $filtered = $Tests.Count -and $Suite -notin @('Foundation','Embedded')
    if ($filtered) { $arguments += "--test=$($Tests -join ',')" }
    if ($null -ne $TestTimeout -and $Suite -notin @('Foundation','Embedded')) { $arguments += "--test-timeout=$TestTimeout" }
    # A filtered run is partial evidence: its log and receipt never replace those of the whole suite, and an interactive run's never
    # replace those of the run that records the same suite's skips.
    $suffix = "$(if ($Interactive) { '.interactive' })$(if ($filtered) { '.filtered' })"
    return [pscustomobject]@{ Executable = $executable; Arguments = $arguments; Filtered = [bool]$filtered; Suffix = $suffix; Log = (Join-Path $logs "test-$Suite-$Platform-$Configuration$suffix.log") }
}
Push-Location $PSScriptRoot
try {
    $lease = $null
    $leaseProblems = @()
    if ($Interactive) {
        # Every suite of the run under one lease: one confirmation, one warning, one restoration. The suites run before any of them is
        # reported, and are reported below exactly as a run without -Interactive reports them.
        $label = "$($Suites -join ', ') ($Platform $Configuration)"
        $estimate = Get-DxUiInteractiveEstimateSeconds -Suites $Suites -Configuration $Configuration
        $plan = @(foreach ($name in $Suites) {
            $planned = Get-SuiteRun $name
            if (-not (Test-Path -LiteralPath $planned.Executable)) { throw "Test executable is missing: $($planned.Executable)" }
            [pscustomobject]@{ Name = $name; Log = $planned.Log; CommandLine = (ConvertTo-DxUiCommandLine -Program $planned.Executable -Arguments $planned.Arguments) }
        })
        # The lease's bound on one suite is for what the runner's watchdog cannot reach; it never cuts a suite the watchdog allows.
        $bound = if ($TestTimeout -eq 0) { 0 } elseif ($null -ne $TestTimeout) { [Math]::Max(900, 3 * $TestTimeout) } else { 900 }
        Write-Host "Interactive run: $label takes the desktop for about $([Math]::Ceiling($estimate / 60.0)) minute(s) once it is confirmed."
        $lease = Invoke-DxUiInteractiveLease -Executable $leaseExecutable -Runs $plan -Label $label -EstimateSeconds $estimate -WorkDirectory $logs -ChildTimeoutSeconds $bound
        $leaseProblems = @(Get-DxUiLeaseProblems -Result $lease.Result -ExitCode $lease.ExitCode)
        if ($null -eq $lease.Result -or @($lease.Result.Children).Count -eq 0) { throw "Interactive run: $($leaseProblems -join '; '). See $($lease.ResultPath)." }
    }
    foreach ($suite in $Suites) {
        $run = Get-SuiteRun $suite
        $executable = $run.Executable
        if (-not (Test-Path -LiteralPath $executable)) { throw "Test executable is missing: $executable" }
        $arguments = $run.Arguments
        $filtered = $run.Filtered
        $suffix = $run.Suffix
        $log = $run.Log
        if ($Interactive) {
            $child = @($lease.Result.Children | Where-Object { $_.Name -eq $suite }) | Select-Object -First 1
            if (-not $child -or -not $child.Launched) { $failures += "$suite did not run under the interactive lease"; Write-Host "FAIL $suite did not run under the interactive lease"; continue }
            Write-Host "Ran $suite ($Platform $Configuration) under the interactive lease$(if ($filtered) { ", tests: $($Tests -join ', ')" })"
            $testExit = $child.ExitCode
        } else {
            Write-Host "Running $suite ($Platform $Configuration)$(if ($filtered) { ", tests: $($Tests -join ', ')" })"
            & $executable @arguments *> $log
            $testExit = $LASTEXITCODE
        }
        $skips = @(Get-Content -LiteralPath $log | Where-Object { $_ -match '^SKIPPED:' })
        # A test that never returned ends the run through the runner's watchdog: exit code 124 and a TIMEOUT line naming it.
        $failure = if ($testExit -ne 0) { Get-SuiteFailureReport -Suite $suite -ExitCode $testExit -LogPath $log }
        $receipt = [ordered]@{
            suite=$suite; configuration=$Configuration; platform=$Platform; nativeArchitecture=$nativeArchitecture
            completedUtc=[DateTime]::UtcNow.ToString('o'); executable=$executable
            sha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; exitCode=$testExit; skips=$skips
            performanceReport=$performanceReport; performanceScenarios=$performance.scenarios
            performanceComparison=$performanceComparison.status; performanceExecutableSha256=$performance.executableSha256
        }
        if ($filtered) { $receipt['tests'] = @($Tests) }
        if ($Interactive) {
            $receipt['interactive'] = $true
            $receipt['lease'] = [ordered]@{
                state=$lease.Result.State; confirmation=$lease.Result.Confirmation; foreground=$lease.Result.Foreground; focus=$lease.Result.Focus
                cursor=$lease.Result.Cursor; cursorSaved=$lease.Result.CursorSaved; cursorAtExit=$lease.Result.CursorAtExit
                runMovedSomething=$lease.Result.RunMovedSomething; savedForeground=$lease.Result.SavedForeground; seconds=$child.Seconds
                executable=$leaseExecutable; sha256=(Get-FileHash -LiteralPath $leaseExecutable -Algorithm SHA256).Hash
            }
        }
        if ($failure -and $failure.Timeout) { $receipt['timeout'] = $failure.Timeout }
        $receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $reports "$suite-$Platform-$Configuration$suffix.json") -Encoding utf8
        if ($failure) { $failures += $failure.Summary; Write-Host "FAIL $($failure.Summary)"; $failure.Tail }
        elseif ($Interactive -and $skips.Count) {
            # An interactive run exists to run what a run without the desktop skips: a skip means the desktop did not provide it.
            $failures += "$suite recorded $($skips.Count) capability skip(s) in an interactive run"
            Write-Host "FAIL $suite recorded $($skips.Count) capability skip(s) in an interactive run, which exists to run what other runs skip"
        }
        else { Write-Host "PASS $suite ($($skips.Count) capability skips recorded$(if ($filtered) { "; filtered to $($Tests -join ', ')" }))" }
        # Which tests a missing capability left unrun, by name, so a CI log shows it without the suite log.
        Write-SuiteSkips -LogPath $log
    }
    $failures += $leaseProblems
} finally { Pop-Location }
if ($Interactive) { Write-Host "Interactive lease: foreground $($lease.Result.Foreground), focus $($lease.Result.Focus), pointer $($lease.Result.Cursor)$(if ($lease.Result.RunMovedSomething) { '; the lease gave back what the run had moved' } else { '; the run left the desktop as the person had it' })." }
if ($Interactive -and $lease.Result.Cursor -eq 'restored') { Write-Warning "The suites left the pointer at $($lease.Result.CursorAtExit), and the lease moved it back to $($lease.Result.CursorSaved). A fixture that moves the pointer puts it back itself, so a pointer left behind is a fixture to look at." }
if ($failures.Count) { throw "DxUi failed suites: $($failures -join '; '). See .build/logs and .build/reports." }
Write-Host "All $($Suites.Count) requested suites passed."
