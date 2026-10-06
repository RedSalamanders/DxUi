# What test.ps1 tells the developer about a suite's run: which tests a missing capability skipped, and when its executable exits
# nonzero, its exit code, the TIMEOUT line when the runner's watchdog ended it (a hung test the runner cannot unwind; see
# Tests/Support/Support.Tests.TestWatchdog.h) and the last lines of its log.
Set-StrictMode -Version Latest

# The exit code of a run the watchdog ended, as DxUi.ControlTests.exe returns it (timeout(1)'s code).
$script:TimeoutExitCode = 124

function Get-SuiteFailureReport {
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string] $Suite,
        [Parameter(Mandatory)][int] $ExitCode,
        [Parameter(Mandatory)][string] $LogPath,
        [int] $TailLines = 12
    )
    $lines = @(if (Test-Path -LiteralPath $LogPath -PathType Leaf) { Get-Content -LiteralPath $LogPath })
    # The watchdog names the hung test on a line of its own. It is searched for instead of assumed to be among the last lines,
    # and a nonzero exit code that is not the watchdog's never claims a timeout because a test printed such a line.
    $timeout = if ($ExitCode -eq $script:TimeoutExitCode) { @($lines | Where-Object { $_ -match '^TIMEOUT: ' }) | Select-Object -First 1 }
    $summary = "$Suite exited with code $ExitCode"
    if ($timeout) { $summary += " ($timeout)" }
    elseif ($ExitCode -eq $script:TimeoutExitCode) { $summary += ' (the runner reported a timeout without naming the test)' }
    return [pscustomobject]@{ Summary = $summary; Timeout = $timeout; Tail = @($lines | Select-Object -Last $TailLines) }
}

# Each capability skip in a suite's log as "<name>: <reason>", named after the test it belongs to, so a CI log shows which tests
# a missing capability (an interactive desktop, the foreground) left unrun without the suite log. The runner prints
# "[START] <Suite>", then "  [START] <Test>" and "  [DONE] <Test> (<seconds> s)" around each test, and "SKIPPED: <reason>" in
# it; a skip outside a test is named after its suite.
function Get-SuiteSkips {
    [CmdletBinding()] param([Parameter(Mandatory)][string] $LogPath)
    $skips = [Collections.Generic.List[string]]::new()
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) { return , $skips.ToArray() }
    $suite = $null
    $test = $null
    foreach ($line in Get-Content -LiteralPath $LogPath) {
        if ($line -match '^\[START\] (?<name>\S+)') { $suite = $Matches['name']; $test = $null }
        elseif ($line -match '^\s+\[START\] (?<name>\S+)') { $test = $Matches['name'] }
        elseif ($line -match '^\s+\[DONE\] ') { $test = $null }
        elseif ($line -match '^SKIPPED: (?<reason>.*)$') {
            $owner = if ($test) { $test } elseif ($suite) { $suite } else { '(before any test)' }
            $skips.Add("${owner}: $($Matches['reason'])")
        }
    }
    return , $skips.ToArray()
}

# Prints each skip on a line of its own under the suite's PASS or FAIL line; a suite without skips prints nothing.
function Write-SuiteSkips {
    [CmdletBinding()] param([Parameter(Mandatory)][string] $LogPath)
    # Assigned first: a foreach statement over the call itself would iterate once, over the whole array.
    $skips = Get-SuiteSkips -LogPath $LogPath
    foreach ($skip in $skips) { Write-Host "  skipped $skip" }
}

Export-ModuleMember -Function Get-SuiteFailureReport, Get-SuiteSkips, Write-SuiteSkips
