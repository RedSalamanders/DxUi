# Exercise DxUi.InteractiveLease.exe (Tests/InteractiveLease), the program behind `test.ps1 -Interactive`, without taking anything of a
# person's desktop (test.ps1 runs this after the build). Its --self-test starts children without a foreground grant and opens the
# confirmation and the warning on a private desktop that is never the input desktop, where no one sees them; --check only reads the
# session. The lease's decisions (every path, with a fake desktop) are in the InteractiveLease control suite, and its PowerShell side
# in Test-InteractiveMode.ps1. No case here starts a run: that would ask the person at the desktop, and take it.
[CmdletBinding()] param(
    [ValidateSet('Debug','Release','ASan Debug')][string] $Configuration = 'Debug',
    [ValidateSet('x64','ARM64')][string] $Platform = 'x64'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../InteractiveRun.psm1') -Force
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$exe = Join-Path $repo ".build/$Platform/$Configuration/DxUi.InteractiveLease.exe"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "The interactive desktop lease is missing: $exe" }

# Runs the lease from the repository root for at most $BoundSeconds and returns how it ended. A run still going at the bound is this
# script's own child: it is killed with its children and reported as not exited.
function Invoke-Bounded([string[]] $Arguments, [int] $BoundSeconds) {
    $info = [Diagnostics.ProcessStartInfo]::new($exe)
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
        # Only the process this test started, through its own handle: the lease's children are in its kill-on-close job and
        # end with it. Never a process-tree walk, which follows reused parent ids into unrelated processes.
        if (-not $exited) { $process.Kill(); $process.WaitForExit() }
        [void][Threading.Tasks.Task]::WaitAll(@($stdout, $stderr), 10000)
        return [pscustomobject]@{
            Exited = $exited; Exit = $(if ($exited) { $process.ExitCode }); Seconds = $seconds
            Output = @(@($stdout.Result -split "\r?\n") + @($stderr.Result -split "\r?\n") | Where-Object { $_ })
        }
    } finally { $process.Dispose() }
}

$onCi = [bool](Get-DxUiInteractiveRefusal -Environment @{ CI = $env:CI; GITHUB_ACTIONS = $env:GITHUB_ACTIONS } -UserInteractive $true -OnWindows $true)

Invoke-TestCase 'the self-test passes: children, the session''s lease, the confirmation and the warning' {
    $run = Invoke-Bounded @('--self-test') 180
    Assert-True $run.Exited "the self-test ended instead of hanging: $($run.Output -join ' | ')"
    Assert-Equal 0 $run.Exit "the self-test passes: $($run.Output -join ' | ')"
    Assert-True (@($run.Output | Where-Object { $_ -match '^PASS: \d+ of \d+ passed$' }).Count -eq 1) 'and says so'
    Assert-Equal 0 @($run.Output | Where-Object { $_ -like 'FAIL*' }).Count 'with no failing check'
    foreach ($proof in @(
        'its exit code is reported as the child''s own', 'its standard output and error go to its log',
        'a child that outlives the lease''s bound is ended with the watchdog''s exit code, 124', 'a stopped run ends its child and says it was stopped',
        'a program that cannot be started is a launch failure with its reason', 'a second run in the session finds it held')) {
        Assert-Contains $run.Output "ok   $proof" "the self-test proves: $proof"
    }
    $skipped = @($run.Output | Where-Object { $_ -like 'SKIPPED:*' })
    if ($skipped.Count) {
        # A session that cannot make a private desktop (a hosted runner's) cannot show the dialog to anyone; a person's can, so a skip
        # there is the dialog failing to open, which the lease must not ship with.
        Assert-True $onCi "a private desktop could be made here, so the dialog and the warning were to be exercised: $($skipped -join ' | ')"
        Write-Host "     skipped: $($skipped -join ' | ')"
    } else {
        foreach ($proof in @(
            'the confirmation starts the run when Start is chosen', 'and does not when Cancel is chosen', 'nobody answering is a cancellation',
            'a run stopped while the person is asked ends the confirmation', 'a confirmation without a time limit waits for the person',
            'the warning is shown', 'above every window, and the pointer and keys pass through it', 'and is gone when hidden')) {
            Assert-Contains $run.Output "ok   $proof" "the self-test proves: $proof"
        }
    }
}
Invoke-FixtureCase '--check reads the session, writes the result the PowerShell side reads, and shows and takes nothing' {
    param($root)
    $result = Join-Path $root 'check.txt'
    $run = Invoke-Bounded @('--check', "--result=$result") 60
    Assert-True ($run.Exited -and $run.Exit -in @(0, 20)) "the check answers with 0 or 20: $($run.Exit)"
    $parsed = Read-DxUiLeaseResult -Text ([IO.File]::ReadAllText($result))
    Assert-True ($null -ne $parsed) 'the result file is one the PowerShell reader reads (the C++ writer and the reader agree)'
    Assert-Equal $run.Exit $parsed.Exit 'it records the exit code'
    if ($run.Exit -eq 0) {
        Assert-Equal 'checked' $parsed.State 'an available desktop is checked'
        Assert-Equal $null (Test-DxUiDesktopAvailable -Executable $exe) 'and the module reports it available'
    } else {
        Assert-Equal 'refused' $parsed.State 'a session without a desktop is refused'
        Assert-True ($parsed.Reason.Length -gt 10) "with a reason: $($parsed.Reason)"
        Assert-Equal $parsed.Reason (Test-DxUiDesktopAvailable -Executable $exe) 'which the module passes on as it is'
    }
    Assert-Equal 'none' $parsed.Confirmation 'no one was asked'
    Assert-Equal 0 @($parsed.Children).Count 'and no child ran'
    $found = @($run.Output | Where-Object { $_.StartsWith('[LEASE] ') }) | Select-Object -First 1
    $expected = if ($run.Exit -eq 0) { '[LEASE] an interactive desktop is available' } else { '[LEASE] no interactive desktop: ' }
    Assert-True ($found -and $found.StartsWith($expected)) "and says what it found: $found"
}
Invoke-FixtureCase 'a malformed command line is a usage error before anything is shown' {
    param($root)
    Set-FixtureFile $root 'no-tabs.txt' "Menu only-a-name`n"
    Set-FixtureFile $root 'empty.txt' "# no child`n"
    $result = Join-Path $root 'result.txt'
    $cases = [ordered]@{
        'no mode' = @()
        '--run without a plan' = @('--run', "--result=$result")
        '--run without a result' = @('--run', "--plan=$(Join-Path $root 'no-tabs.txt')")
        'a plan that does not exist' = @('--run', "--plan=$(Join-Path $root 'absent.txt')", "--result=$result")
        'a plan line without its three fields' = @('--run', "--plan=$(Join-Path $root 'no-tabs.txt')", "--result=$result")
        'a plan with no child' = @('--run', "--plan=$(Join-Path $root 'empty.txt')", "--result=$result")
        'an unknown argument' = @('--check', '--surprise')
        'a seconds value that is not a number' = @('--run', '--estimate=soon')
        'an empty time limit' = @('--run', '--confirm-timeout=')
        'a negative bound' = @('--run', '--child-timeout=-1')
    }
    foreach ($name in $cases.Keys) {
        $run = Invoke-Bounded $cases[$name] 30
        Assert-True $run.Exited "$name ends at once"
        Assert-Equal 2 $run.Exit "$name is a usage error"
        Assert-True ($run.Seconds -lt 20) "$name is refused without a dialog's wait ($($run.Seconds) s)"
        Assert-True (@($run.Output | Where-Object { $_ -like 'usage:*' }).Count -eq 1) "$name prints the usage"
    }
    Assert-True (-not (Test-Path -LiteralPath $result)) 'and none of them wrote a result'
}
Invoke-TestCase 'the confirmation defaults to Cancel, and cancels itself when nobody answers' {
    # The self-test proves each answer; what no run can observe is which button is the default, so that is read from the source.
    $dialog = [IO.File]::ReadAllText((Join-Path $repo 'Tests/InteractiveLease/InteractiveLease.Tests.Confirmation.h'))
    Assert-True ($dialog -match 'nDefaultButton\s*=\s*IDCANCEL;') 'Cancel is the default button: a key pressed by accident as the dialog appears cannot start a takeover'
    Assert-True ($dialog -match 'TDF_ALLOW_DIALOG_CANCELLATION') 'and Esc and the close button cancel'
    Assert-True ($dialog -match 'dwCommonButtons\s*=\s*TDCBF_CANCEL_BUTTON;') 'with a Cancel button of its own'
    Assert-True ($dialog -match 'Confirmation::TimedOut;\s*SendMessageW\(dialog, TDM_CLICK_BUTTON, IDCANCEL') 'a confirmation that runs out of time clicks Cancel'
    $main = [IO.File]::ReadAllText((Join-Path $repo 'Tests/InteractiveLease/InteractiveLease.Tests.Runner.cpp'))
    Assert-True ($main -match 'unsigned confirmSeconds\s*=\s*120u;') 'the lease waits two minutes for an answer unless told otherwise'
    $module = [IO.File]::ReadAllText((Join-Path $repo 'Tools/InteractiveRun.psm1'))
    Assert-True ($module -match '\[int\] \$ConfirmSeconds = 120') 'and so does the script that starts it, which never passes a longer time'
    Assert-True (-not ([IO.File]::ReadAllText((Join-Path $repo 'test.ps1')) -match '-ConfirmSeconds')) 'test.ps1 offers no way to wait longer, or to skip the question'
}
Invoke-TestCase 'the lease ends only the processes it started' {
    # The repository's rule for every tool: never end an application that was launched independently, never find one by name. The
    # lease starts its children itself, ends exactly them through their job or handle, and has no way to name anything else.
    $sources = foreach ($path in @('Tests/InteractiveLease/InteractiveLease.Tests.Runner.cpp', 'Tests/InteractiveLease/InteractiveLease.Tests.ChildProcess.h', 'Tests/InteractiveLease/InteractiveLease.Tests.Confirmation.h', 'Tests/InteractiveLease/InteractiveLease.Tests.WarningBanner.h',
            'Tests/Support/Support.Tests.DesktopLease.h', 'Tests/Support/Support.Tests.InteractiveLease.h')) { [pscustomobject]@{ Path = $path; Text = [IO.File]::ReadAllText((Join-Path $repo $path)) } }
    foreach ($source in $sources) {
        foreach ($match in [regex]::Matches($source.Text, 'TerminateProcess\(([^,]+),')) {
            Assert-Equal 'process.hProcess' $match.Groups[1].Value.Trim() "$($source.Path) ends only the process it created"
        }
        foreach ($call in @('CreateToolhelp32Snapshot', 'EnumProcesses', 'Process32First', 'FindWindow', 'AllowSetForegroundWindow(ASFW_ANY')) {
            Assert-True (-not $source.Text.Contains($call)) "$($source.Path) never looks for another process or grants the foreground to any: $call"
        }
    }
    $grants = @($sources | ForEach-Object { [regex]::Matches($_.Text, 'AllowSetForegroundWindow\(([^)]*)\)') } | ForEach-Object { $_.Groups[1].Value })
    Assert-True ($grants.Count -ge 1 -and @($grants | Where-Object { $_ -ne 'process.dwProcessId' }).Count -eq 0) "the foreground is granted only to the child it just created: $($grants -join ', ')"
    foreach ($script in @('test.ps1', 'Tools/InteractiveRun.psm1')) {
        $text = [IO.File]::ReadAllText((Join-Path $repo $script))
        Assert-True ($text -notmatch 'Stop-Process|taskkill|\.Kill\(') "$script ends no process"
    }
}
Complete-TestRun 'Interactive lease'
