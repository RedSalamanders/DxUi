# What test.ps1 tells the developer when a suite's executable exits nonzero: its exit code, the TIMEOUT line when the runner's
# watchdog ended it (a hung test the runner cannot unwind; see Tests/Support/TestWatchdog.h) and the last lines of its log.
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

Export-ModuleMember -Function Get-SuiteFailureReport
