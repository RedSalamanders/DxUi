# Exercise the control-test runner's watchdog against the built DxUi.ControlTests.exe (test.ps1 runs it after the build). A test
# that never returns must end the run with its name and exit code 124 instead of holding a CI job until its time limit. The
# runner's hidden --watchdog-self-test switch runs a test (or a fixture suite) that blocks on an event nobody sets; every run
# here is bounded, so a watchdog that does not work fails a case instead of hanging this script.
[CmdletBinding()] param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64'
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../SuiteFailure.psm1') -Force
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo ".build/$Platform/$Configuration/DxUi.ControlTests.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Test executable is missing: $exe" }
$hungTest = 'TestWatchdogSelfTestBlocksForever'

# Runs a program from the repository root for at most $BoundSeconds and returns how it ended. A program still running at the
# bound is this script's own child: it is killed with its children and reported as not exited.
function Invoke-Bounded([string[]] $Arguments, [int] $BoundSeconds, [string] $FilePath = $exe) {
    $info = [Diagnostics.ProcessStartInfo]::new($FilePath)
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $exited = $process.WaitForExit($BoundSeconds * 1000)
        $seconds = $clock.Elapsed.TotalSeconds
        if (-not $exited) { $process.Kill($true); $process.WaitForExit() }
        [void] [Threading.Tasks.Task]::WaitAll(@($stdout, $stderr), 10000)
        return [pscustomobject]@{
            Exited = $exited; Exit = $(if ($exited) { $process.ExitCode }); Seconds = $seconds
            Output = @(@($stderr.Result -split "\r?\n") + @($stdout.Result -split "\r?\n") | Where-Object { $_ })
        }
    } finally { $process.Dispose() }
}
function Get-Lines($Run, [string] $Pattern) { @($Run.Output | Where-Object { $_ -match $Pattern }) }

# The CPU seconds a hanging run spends in two seconds once its test has started (its marker is the last line it prints before it
# hangs): a thread that polled would spend about two of them, one that waits on an event or a condition variable spends none. The
# run is this script's own child and is killed afterwards.
function Measure-HangingCpu([string[]] $Arguments) {
    $info = [Diagnostics.ProcessStartInfo]::new($exe)
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $info.WorkingDirectory = $repo
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $settled = $false
        $clock = [Diagnostics.Stopwatch]::StartNew()
        while (-not $settled -and $clock.Elapsed.TotalSeconds -lt 30) {
            $pending = $process.StandardError.ReadLineAsync()
            while (-not $pending.Wait(250) -and $clock.Elapsed.TotalSeconds -lt 30) { }
            if (-not $pending.IsCompleted -or $null -eq $pending.Result) { break }
            $settled = $pending.Result -match '^  \[START\] '
        }
        if (-not $settled) { return [pscustomobject]@{ Started = $false; Alive = $false; Cpu = [double]::NaN } }
        $before = $process.TotalProcessorTime.TotalSeconds
        Start-Sleep -Seconds 2
        $after = $process.TotalProcessorTime.TotalSeconds
        return [pscustomobject]@{ Started = $true; Alive = -not $process.HasExited; Cpu = $after - $before }
    } finally {
        if (-not $process.HasExited) { $process.Kill($true) }
        $process.WaitForExit()
        $process.Dispose()
    }
}

Invoke-TestCase 'a test that never returns ends the run at its deadline, named' {
    $run = Invoke-Bounded @('--watchdog-self-test', '--test-timeout=2') 30
    Assert-True $run.Exited 'the watchdog ended the run instead of leaving it hanging'
    Assert-Equal 124 $run.Exit 'a run ended by the watchdog exits with 124'
    Assert-Contains $run.Output "TIMEOUT: $hungTest after 2 s" 'the run names the hung test and its deadline'
    Assert-Contains $run.Output "  [START] $hungTest" 'the hung test had started'
    Assert-Equal 0 (Get-Lines $run '\[DONE\]').Count 'the hung test never finished'
    Assert-True ($run.Seconds -ge 1.9) "the run did not end before the deadline ($($run.Seconds) s)"
    Assert-True ($run.Seconds -lt 20) "the run ended within seconds of the deadline ($($run.Seconds) s)"
}
Invoke-TestCase 'a fixture suite without named tests is bounded as a whole' {
    $run = Invoke-Bounded @('--watchdog-self-test=fixture', '--test-timeout=2') 30
    Assert-True $run.Exited 'the watchdog ended the run instead of leaving it hanging'
    Assert-Equal 124 $run.Exit 'a run ended by the watchdog exits with 124'
    Assert-Contains $run.Output 'TIMEOUT: WatchdogSelfTestFixture after 2 s' 'the run names the fixture suite and its deadline'
    Assert-True ($run.Seconds -ge 1.9 -and $run.Seconds -lt 20) "the run ended at the deadline ($($run.Seconds) s)"
}
Invoke-TestCase 'a fixture that reports progress outlives its deadline as a whole' {
    # Four one-second cycles under a three-second deadline: each cycle reports progress, which starts the deadline over.
    $run = Invoke-Bounded @('--watchdog-self-test=progress', '--test-timeout=3') 30
    Assert-True $run.Exited 'the run ended by itself'
    Assert-Equal 0 $run.Exit 'a fixture that keeps reporting progress is not ended'
    Assert-Equal 0 (Get-Lines $run '^TIMEOUT:').Count 'no TIMEOUT line'
    Assert-True ($run.Seconds -ge 3.5) "the fixture outlived its deadline ($($run.Seconds) s)"
}
Invoke-TestCase 'without a deadline the same test never returns' {
    # The falsification of the cases above: --test-timeout=0 turns the watchdog off, and the test really hangs. Nothing else
    # ends it, so this script gives up on it after the bound.
    $run = Invoke-Bounded @('--watchdog-self-test', '--test-timeout=0') 6
    Assert-True (-not $run.Exited) 'the hung test was still hanging when this script gave up on it'
    Assert-Equal 0 (Get-Lines $run '^TIMEOUT:').Count 'no TIMEOUT line'
    Assert-Contains $run.Output '[WATCHDOG] off (--test-timeout=0)' 'the run says the watchdog is off'
    Assert-Contains $run.Output "  [START] $hungTest" 'the hung test had started'
}
Invoke-TestCase 'an armed watchdog waits instead of polling' {
    # Armed with a deadline a minute away, the hung run sits in its test and the watchdog sits on its condition variable.
    $idle = Measure-HangingCpu @('--watchdog-self-test', '--test-timeout=60')
    Assert-True $idle.Started 'the run reached its hung test'
    Assert-True $idle.Alive 'the run is still hanging'
    Assert-True ($idle.Cpu -lt 0.5) "the hung run spent $($idle.Cpu) CPU seconds in two seconds"
}
Invoke-TestCase 'the deadline defaults to 300 seconds and --test-timeout sets it' {
    $fast = @('--suite=Grid', '--test=TestSortCycle', '--no-activate')
    Assert-Contains (Invoke-Bounded $fast 60).Output '[WATCHDOG] each test may run 300 s; a test that outlives it ends the run with exit code 124' 'the default deadline is the documented 300 s'
    Assert-Contains (Invoke-Bounded ($fast + '--test-timeout=7') 60).Output '[WATCHDOG] each test may run 7 s; a test that outlives it ends the run with exit code 124' '--test-timeout sets the deadline'
    Assert-Contains (Invoke-Bounded ($fast + '--test-timeout=0') 60).Output '[WATCHDOG] off (--test-timeout=0)' '--test-timeout=0 turns the watchdog off'
}
Invoke-TestCase 'a test that finishes in time is not ended and reports its duration' {
    $run = Invoke-Bounded @('--suite=Grid', '--test=TestSortCycle', '--no-activate', '--test-timeout=60') 60
    Assert-Equal 0 $run.Exit 'a test inside its deadline passes'
    Assert-Equal 1 (Get-Lines $run '^  \[DONE\] TestSortCycle \(\d+\.\d{3} s\)$').Count 'the test marker carries its duration'
    Assert-Equal 1 (Get-Lines $run '^\[DONE\] Grid \(\d+\.\d{3} s\)$').Count 'the suite marker carries its duration'
    Assert-Equal 0 (Get-Lines $run '^TIMEOUT:').Count 'no TIMEOUT line'
}
Invoke-TestCase 'a malformed --test-timeout is rejected before any test runs' {
    foreach ($value in @('--test-timeout=', '--test-timeout=abc', '--test-timeout=-1', '--test-timeout=1.5', '--test-timeout=1234567')) {
        $run = Invoke-Bounded @('--suite=Grid', '--no-activate', $value) 60
        Assert-Equal 2 $run.Exit "$value exits with the usage code"
        Assert-Equal 0 (Get-Lines $run '\[START\]').Count "$value runs no test"
        Assert-True (@(Get-Lines $run '^Expected --test-timeout=<seconds>').Count -eq 1) "$value is explained"
    }
}
Invoke-TestCase 'the self-test is a switch of its own' {
    foreach ($arguments in @(@('--watchdog-self-test', '--suite=Grid'), @('--watchdog-self-test', '--test=TestSortCycle'), @('--watchdog-self-test=other'))) {
        $run = Invoke-Bounded ($arguments + '--test-timeout=2') 20
        Assert-True $run.Exited "$($arguments -join ' ') exits at once instead of hanging"
        Assert-Equal 2 $run.Exit "$($arguments -join ' ') exits with the usage code"
        Assert-Equal 0 (Get-Lines $run '\[START\]').Count "$($arguments -join ' ') starts nothing"
    }
}
Invoke-TestCase 'test.ps1 surfaces the timeout: the exit code and the TIMEOUT line among the printed lines' {
    # test.ps1 redirects the executable's streams into a log and reports a failing suite through Get-SuiteFailureReport: the
    # exit code, the TIMEOUT line and the last lines of that log. This produces the log the way test.ps1 does.
    $root = New-FixtureRoot
    try {
        $log = Join-Path $root 'test-Menu-x64-Debug.log'
        $command = "& '$exe' --watchdog-self-test --test-timeout=2 *> '$log'; exit `$LASTEXITCODE"
        $run = Invoke-Bounded @('-NoProfile', '-NonInteractive', '-Command', $command) 60 (Get-Process -Id $PID).Path
        Assert-True $run.Exited 'the redirected run ended'
        Assert-Equal 124 $run.Exit 'the exit code reaches the caller through the redirection'
        $report = Get-SuiteFailureReport -Suite 'Menu' -ExitCode $run.Exit -LogPath $log
        Assert-Equal "Menu exited with code 124 (TIMEOUT: $hungTest after 2 s)" $report.Summary 'the failure names the suite, the exit code and the hung test'
        Assert-Equal "TIMEOUT: $hungTest after 2 s" $report.Timeout 'the TIMEOUT line is found'
        Assert-Contains $report.Tail "TIMEOUT: $hungTest after 2 s" 'the TIMEOUT line is among the last lines test.ps1 prints'
        Assert-Contains $report.Tail "  [START] $hungTest" 'the last lines start at the hung test'
        $ordinary = Get-SuiteFailureReport -Suite 'Grid' -ExitCode 1 -LogPath $log
        Assert-Equal 'Grid exited with code 1' $ordinary.Summary 'a failed check is not called a timeout because its log holds a TIMEOUT line'
    } finally { Remove-FixtureRoot $root }
}
Invoke-TestCase 'every driver thread of the Menu suite starts with the guard that closes its menu when it fails' {
    # A blocking ContextMenu::Show returns only when the menu closes and the driver thread closes it, so a driver that gives up
    # before it has a popup would leave the owner thread hanging: the guard first, before the driver's first wait.
    $source = [IO.File]::ReadAllText((Join-Path $repo 'Tests/Controls/DxUiTests.Menu.cpp'))
    $drivers = [regex]::Matches($source, '(?m)^([ ]*)std::thread driver\(\[&\]\r?\n\1\{\r?\n(?<first>[^\r\n]*)')
    Assert-True ($drivers.Count -gt 0) 'the scan finds the driver threads'
    Assert-Equal ([regex]::Matches($source, 'std::thread driver\(').Count) $drivers.Count 'the scan sees every driver thread'
    $unguarded = @($drivers | Where-Object { $_.Groups['first'].Value -notmatch '\bDismissMenusIfDriverFails\b' })
    Assert-Equal 0 $unguarded.Count "drivers starting with anything else: $(@($unguarded | ForEach-Object { $_.Groups['first'].Value.Trim() }) -join ' | ')"
}
Complete-TestRun 'Test watchdog'
