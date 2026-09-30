# What test.ps1 tells the developer about a control suite that exits nonzero (Tools/SuiteFailure.psm1), on fixture logs. The same
# report on a log written by the built executable is in Test-TestWatchdog.ps1.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../SuiteFailure.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

function Set-Log([string] $Root, [string[]] $Lines) {
    Set-FixtureFile $Root 'suite.log' (($Lines -join "`r`n") + "`r`n")
    return (Join-Path $Root 'suite.log')
}

Invoke-FixtureCase 'a run the watchdog ended is reported with its exit code and the hung test' {
    param($root)
    $log = Set-Log $root @('[START] Menu', '  [START] TestFirst', '  [DONE] TestFirst (0.100 s)', '  [START] TestHung', 'TIMEOUT: TestHung after 300 s')
    $report = Get-SuiteFailureReport -Suite 'Menu' -ExitCode 124 -LogPath $log
    Assert-Equal 'Menu exited with code 124 (TIMEOUT: TestHung after 300 s)' $report.Summary 'the summary names the suite, the code and the test'
    Assert-Equal 'TIMEOUT: TestHung after 300 s' $report.Timeout 'the TIMEOUT line'
    Assert-Contains $report.Tail 'TIMEOUT: TestHung after 300 s' 'the printed lines include it'
    Assert-Equal 5 $report.Tail.Count 'a short log is printed whole'
}
Invoke-FixtureCase 'the TIMEOUT line is found even when it is not among the last lines' {
    param($root)
    $log = Set-Log $root (@('TIMEOUT: TestHung after 300 s') + @(1..30 | ForEach-Object { "later line $_" }))
    $report = Get-SuiteFailureReport -Suite 'Menu' -ExitCode 124 -LogPath $log
    Assert-Equal 'TIMEOUT: TestHung after 300 s' $report.Timeout 'the line is searched for, not assumed to be last'
    Assert-Equal 12 $report.Tail.Count 'the last twelve lines are printed'
    Assert-Equal 'later line 30' $report.Tail[-1] 'the tail ends at the end of the log'
}
Invoke-FixtureCase 'a failed check is not called a timeout' {
    param($root)
    $log = Set-Log $root @('  [START] TestX', 'FAILED: something', 'TIMEOUT: quoted by the test itself after 5 s')
    $report = Get-SuiteFailureReport -Suite 'Grid' -ExitCode 1 -LogPath $log
    Assert-Equal 'Grid exited with code 1' $report.Summary 'only the watchdog exit code claims a timeout'
    Assert-True (-not $report.Timeout) 'no TIMEOUT line is reported'
}
Invoke-FixtureCase 'a watchdog exit code without a TIMEOUT line says so' {
    param($root)
    $report = Get-SuiteFailureReport -Suite 'Menu' -ExitCode 124 -LogPath (Set-Log $root @('  [START] TestHung'))
    Assert-Equal 'Menu exited with code 124 (the runner reported a timeout without naming the test)' $report.Summary 'the summary'
}
Invoke-FixtureCase 'a missing log does not hide the exit code' {
    param($root)
    $report = Get-SuiteFailureReport -Suite 'Menu' -ExitCode 3 -LogPath (Join-Path $root 'absent.log')
    Assert-Equal 'Menu exited with code 3' $report.Summary 'the summary'
    Assert-Equal 0 @($report.Tail).Count 'nothing to print'
}
Invoke-FixtureCase 'each capability skip is named after the test it belongs to' {
    param($root)
    $log = Set-Log $root @('[WATCHDOG] each test may run 300 s', '[START] Menu', '  [START] TestFirst', 'SKIPPED: needs a desktop',
        '    [TRACE] cleanup', '  [DONE] TestFirst (0.100 s)', '  [START] TestRan', '  [DONE] TestRan (0.200 s)', '  [START] TestLast',
        'SKIPPED: another window covers it', '  [DONE] TestLast (0.050 s)', '[DONE] Menu (0.400 s)')
    $skips = Get-SuiteSkips -LogPath $log
    Assert-Equal 2 $skips.Count 'one entry per skip'
    Assert-Equal 'TestFirst: needs a desktop' $skips[0] 'the first skip names its test'
    Assert-Equal 'TestLast: another window covers it' $skips[1] 'a test that ran in between is not named'
}
Invoke-FixtureCase 'a skip outside a test is named after its suite, or said to come before any test' {
    param($root)
    Assert-Equal 'Fixture: no desktop' (Get-SuiteSkips -LogPath (Set-Log $root @('[START] Fixture', 'SKIPPED: no desktop')))[0] 'a fixture suite without named tests'
    Assert-Equal 'Menu: late' (Get-SuiteSkips -LogPath (Set-Log $root @('[START] Menu', '  [START] TestA', '  [DONE] TestA (0.1 s)', 'SKIPPED: late')))[0] 'after a test finished'
    Assert-Equal '(before any test): early' (Get-SuiteSkips -LogPath (Set-Log $root @('SKIPPED: early')))[0] 'before any marker'
    Assert-Equal 0 (Get-SuiteSkips -LogPath (Set-Log $root @('[START] Menu', '  [START] TestA', '  [DONE] TestA (0.1 s)'))).Count 'a run without skips'
    Assert-Equal 0 (Get-SuiteSkips -LogPath (Join-Path $root 'absent.log')).Count 'a missing log'
}
Invoke-FixtureCase 'each skip is printed on a line of its own, and a suite without skips prints nothing' {
    param($root)
    $log = Set-Log $root @('[START] Menu', '  [START] TestA', 'SKIPPED: no desktop', '  [DONE] TestA (0.1 s)', '  [START] TestB',
        'SKIPPED: covered', '  [DONE] TestB (0.1 s)')
    $printed = @(Write-SuiteSkips -LogPath $log 6>&1 | ForEach-Object { "$_" })
    Assert-Equal 2 $printed.Count "one line per skip: $($printed -join ' | ')"
    Assert-Equal '  skipped TestA: no desktop' $printed[0] 'the first skip'
    Assert-Equal '  skipped TestB: covered' $printed[1] 'the second skip'
    $quiet = @(Write-SuiteSkips -LogPath (Set-Log $root @('[START] Grid', '  [START] TestA', '  [DONE] TestA (0.1 s)')) 6>&1)
    Assert-Equal 0 $quiet.Count 'a suite without skips prints nothing'
}
Invoke-TestCase 'test.ps1 reports a failing suite through it, gives its receipt the timeout and passes -TestTimeout on' {
    $script = [IO.File]::ReadAllText((Join-Path $repository 'test.ps1'))
    Assert-True $script.Contains("Tools/SuiteFailure.psm1") 'test.ps1 imports the module'
    Assert-True $script.Contains('Get-SuiteFailureReport -Suite $suite -ExitCode $testExit -LogPath $log') 'and asks it about a failing suite'
    Assert-True $script.Contains("`$receipt['timeout'] = `$failure.Timeout") 'the receipt records the timeout'
    Assert-True $script.Contains('"--test-timeout=$TestTimeout"') 'the option reaches the control executable'
    Assert-True $script.Contains("Tools/tests/Test-TestWatchdog.ps1") 'the watchdog test runs after the build'
    Assert-True $script.Contains('Write-SuiteSkips -LogPath $log') 'every suite prints its skips by test'
}
Complete-TestRun 'Suite failure report'
