# The PowerShell side of `test.ps1 -Interactive` (Tools/InteractiveRun.psm1 and test.ps1): which suites it selects, when it refuses, what
# it hands the lease and what it reads back. Nothing here takes or asks for a desktop; the few runs of test.ps1 are refusals that end
# before anything is built, under an environment that says CI, and are bounded. The lease's own Windows services are in
# Test-InteractiveLease.ps1, which needs the built executables.
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../InteractiveRun.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$testScript = Join-Path $repository 'test.ps1'
$tokens = $null
$parseErrors = $null
$testAst = [Management.Automation.Language.Parser]::ParseFile($testScript, [ref]$tokens, [ref]$parseErrors)
Assert-Equal 0 @($parseErrors).Count 'test.ps1 parses'

function Get-NamesIn([string] $Text, [string] $Pattern) {
    return , @([regex]::Matches($Text, $Pattern) | ForEach-Object { $_.Groups[1].Value } | Sort-Object)
}

# Whether a node of test.ps1 sits in the body of an `if` whose condition names $Interactive.
function Test-GuardedByInteractive($Node, [switch] $AllowHostedProbe) {
    for ($parent = $Node.Parent; $null -ne $parent; $parent = $parent.Parent) {
        if ($parent -isnot [Management.Automation.Language.IfStatementAst]) { continue }
        foreach ($clause in $parent.Clauses) {
            $body = $clause.Item2
            if ($body.Extent.StartOffset -le $Node.Extent.StartOffset -and $Node.Extent.EndOffset -le $body.Extent.EndOffset -and $clause.Item1.Extent.Text -match '\$(Interactive|foregroundLeaseRequired)\b') { return $true }
            if ($AllowHostedProbe -and $Node.GetCommandName() -eq 'Test-DxUiDesktopAvailable' -and
                $body.Extent.StartOffset -le $Node.Extent.StartOffset -and $Node.Extent.EndOffset -le $body.Extent.EndOffset -and
                $clause.Item1.Extent.Text -match '\$verifiedHostedRunner\b') { return $true }
        }
    }
    return $false
}

function Get-CommandCalls([string[]] $Names) {
    return , @($testAst.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -in $Names }, $true))
}

# The text of a run as one line. PowerShell's error view colors a message, and the child on CI's Ubuntu runner writes the color
# codes although its output is redirected; it also wraps the message at the console's width behind a "|" gutter. Either splits
# a phrase the tests look for.
function Get-FlatText([string] $Text) {
    return ($Text -replace '\x1b\[[0-9;?]*[ -/]*[@-~]', '' -replace '(?m)^\s*\|\s?', '' -replace '\s+', ' ')
}

# Runs test.ps1 in a child process of its own under an environment that says CI, bounded, and reports how it ended.
function Invoke-TestScript([string[]] $Arguments, [hashtable] $Environment, [string] $Root) {
    $info = [Diagnostics.ProcessStartInfo]::new((Get-Process -Id $PID).Path)
    foreach ($argument in @('-NoProfile', '-NonInteractive', '-File', (Join-Path $Root 'test.ps1')) + $Arguments) { $info.ArgumentList.Add($argument) }
    $info.WorkingDirectory = $Root
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($name in $Environment.Keys) { $info.Environment[$name] = [string]$Environment[$name] }
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $exited = $process.WaitForExit(90000)
        # Only the process this test started, through its own handle: the lease's children are in its kill-on-close job and
        # end with it. Never a process-tree walk, which follows reused parent ids into unrelated processes.
        if (-not $exited) { $process.Kill(); $process.WaitForExit() }
        [void][Threading.Tasks.Task]::WaitAll(@($stdout, $stderr), 10000)
        return [pscustomobject]@{ Exited = $exited; Exit = $(if ($exited) { $process.ExitCode }); Output = (@($stdout.Result, $stderr.Result) -join "`n") }
    } finally { $process.Dispose() }
}

# Ordinal order: Sort-Object compares paths by culture, which is not a total order for them, so two listings of the same
# files could sort differently.
function ConvertTo-OrdinalOrder([string[]] $Values) {
    $sorted = [string[]]@($Values)
    [Array]::Sort($sorted, [StringComparer]::Ordinal)
    return , $sorted
}

function New-RefusalFixture([string] $Root) {
    # The refused child needs the real entry point and its imports, but no build or test
    # artifacts. An isolated output tree distinguishes its writes from the parent log
    # and unrelated builds that legitimately continue during this check.
    Set-FixtureFile $Root 'test.ps1' ([IO.File]::ReadAllText($testScript))
    foreach ($module in @('SuiteFailure.psm1', 'InteractiveRun.psm1', 'ScopedTesting.psm1', 'CapabilitySkipPolicy.psm1')) {
        Set-FixtureFile $Root "Tools/$module" ([IO.File]::ReadAllText((Join-Path $repository "Tools/$module")))
    }
    Set-FixtureFile $Root 'Tests/test-scopes.json' ([IO.File]::ReadAllText((Join-Path $repository 'Tests/test-scopes.json')))
    Set-FixtureFile $Root '.build/logs/existing.log' 'existing log'
    Set-FixtureFile $Root '.build/reports/existing.json' '{}'
}

function Get-OutputFiles([string] $Root) {
    return , @(foreach ($directory in @('.build/logs', '.build/reports')) {
        $path = Join-Path $Root $directory
        if (Test-Path -LiteralPath $path) {
            Get-ChildItem -LiteralPath $path -File -Recurse | ForEach-Object {
                "$($_.FullName)|$($_.LastWriteTimeUtc.Ticks)|$($_.Length)|$((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)"
            }
        }
    })
}

function Get-RunnerLeasePresentation($Result, $Warnings = $null) {
    # Only the runner's final presentation block, with a synthetic result: no suite, desktop probe or lease runs here.
    $statements = @($testAst.FindAll({ param($node)
        $node -is [Management.Automation.Language.IfStatementAst] -and
        $node.Clauses[0].Item1.Extent.Text -match '^\$foregroundLeaseRequired(?:\s+-and\b|$)' -and
        $node.Clauses[0].Item2.Extent.Text -match 'Foreground lease|The suites left the pointer|Get-DxUiLeaseSummary'
    }, $true))
    Assert-Equal 1 $statements.Count 'test.ps1 has one guarded foreground lease presentation, including inside evidence finalization'
    $show = [scriptblock]::Create(($statements.Extent.Text -join "`n"))
    $records = @(& {
        param($leaseResult, $show)
        $foregroundLeaseRequired = $true
        & $show 3>&1 6>&1
    } $Result $show)
    if ($null -ne $Warnings) { $Warnings.Value = @($records | Where-Object { $_ -is [Management.Automation.WarningRecord] }) }
    return ($records | Out-String)
}

Invoke-TestCase 'the interactive suites are the suites DxUi.ControlTests.exe runs with real focus' {
    $runner = [IO.File]::ReadAllText((Join-Path $repository 'Tests/Controls/DxUi.Tests.Runner.cpp'))
    $lambda = [regex]::Match($runner, '(?s)suiteCanActivate\s*=\s*\[\]\(const char\* name\) noexcept\s*\{(.*?)\};')
    Assert-True $lambda.Success 'the runner decides which suites may activate windows'
    $activating = Get-NamesIn $lambda.Groups[1].Value '_stricmp\(name, "(\w+)"\)'
    $selected = Get-NamesIn ([regex]::Match($runner, '(?s)selectedSuiteCanActivate\s*=(.*?);').Groups[1].Value) 'L"(\w+)"'
    Assert-Equal (($activating) -join ',') ((Get-DxUiInteractiveSuiteNames -IncludeOptIn | Sort-Object) -join ',') 'the module lists exactly the suites the runner lets activate'
    Assert-Equal ($activating -join ',') ($selected -join ',') 'and the runner decides the same for a selected suite'
    foreach ($suite in $activating) { Assert-True (Test-DxUiInteractiveSuite $suite) "$suite is interactive" }
    foreach ($suite in @('Grid', 'MenuExitLifetime', 'NewControls', 'MenuTextLayoutResources', 'Foundation', 'Embedded', 'InteractiveLease', '')) {
        Assert-True (-not (Test-DxUiInteractiveSuite $suite)) "'$suite' is not interactive"
    }
}
Invoke-TestCase 'a run without -Suites takes Menu and NativeTextInput, and the fixtures only when they are named' {
    Assert-Equal 'Menu,NativeTextInput' ((Resolve-DxUiInteractiveSuites -Suites @('Foundation', 'Grid') -Requested $false) -join ',') 'the default is the two suites, whatever the parameter default holds'
    Assert-Equal 'Menu,NativeTextInput' ((Get-DxUiInteractiveSuiteNames) -join ',') 'the names'
    Assert-Equal 'MenuResources,MenuResourceScaling' ((Resolve-DxUiInteractiveSuites -Suites @('MenuResources', 'MenuResourceScaling') -Requested $true) -join ',') 'a named fixture is run'
    Assert-Equal 'NativeTextInput,Menu' ((Resolve-DxUiInteractiveSuites -Suites @('NativeTextInput', 'Menu', 'NativeTextInput') -Requested $true) -join ',') 'named suites keep their order and are run once'
    Assert-Equal 'Menu' ((Resolve-DxUiInteractiveSuites -Suites @('menu') -Requested $true) -join ',') 'a name is matched without regard to case and returned in its own spelling'
}
Invoke-TestCase 'a suite that does not need the desktop is refused by name, and the interactive ones are listed' {
    foreach ($request in @(@('Grid'), @('Menu', 'Grid'), @('Foundation'), @('Embedded', 'NativeTextInput'), @('MenuTextLayoutResources'))) {
        $message = $null
        try { [void](Resolve-DxUiInteractiveSuites -Suites $request -Requested $true) } catch { $message = $_.Exception.Message }
        Assert-True ($null -ne $message) "-Interactive -Suites $($request -join ',') is refused"
        $others = @($request | Where-Object { $_ -notin (Get-DxUiInteractiveSuiteNames -IncludeOptIn) })
        Assert-True ($message.Contains("not interactive: $($others -join ', ')")) "the refusal names $($others -join ', '): $message"
        Assert-True ($message.Contains('Menu, NativeTextInput, MenuResources, MenuResourceScaling')) 'and lists the suites that are interactive'
    }
    Assert-Throws { [void](Resolve-DxUiInteractiveSuites -Suites @() -Requested $true) } 'an explicit but empty list of suites is refused'
}
Invoke-TestCase 'a run refuses where there is no desktop to take, and says why' {
    $clean = @{ CI = ''; GITHUB_ACTIONS = '' }
    Assert-Equal $null (Get-DxUiInteractiveRefusal -Environment $clean -UserInteractive $true -OnWindows $true) 'an interactive Windows session outside CI is not refused'
    Assert-Equal $null (Get-DxUiInteractiveRefusal -Environment @{} -UserInteractive $true -OnWindows $true) 'an environment that sets nothing is not refused'
    foreach ($name in @('CI', 'GITHUB_ACTIONS', 'TF_BUILD', 'APPVEYOR', 'BUILDKITE', 'JENKINS_URL', 'TEAMCITY_VERSION')) {
        $reason = Get-DxUiInteractiveRefusal -Environment @{ $name = 'true' } -UserInteractive $true -OnWindows $true
        Assert-True ($reason -and $reason.Contains("$name is set") -and $reason.Contains('CI job')) "$name makes it a CI job: $reason"
    }
    foreach ($off in @('false', 'False', '0', ' ', '')) {
        Assert-Equal $null (Get-DxUiInteractiveRefusal -Environment @{ CI = $off } -UserInteractive $true -OnWindows $true) "CI='$off' is not a CI job"
    }
    Assert-Equal 'this process has no interactive window station: it runs as a service, a scheduled task or a remote shell' (Get-DxUiInteractiveRefusal -Environment $clean -UserInteractive $false -OnWindows $true) 'a service, a scheduled task or a remote shell is refused'
    Assert-Equal 'this is not Windows' (Get-DxUiInteractiveRefusal -Environment $clean -UserInteractive $true -OnWindows $false) 'another operating system is refused'
    Assert-Equal 'this is not Windows' (Get-DxUiInteractiveRefusal -Environment @{ CI = 'true' } -UserInteractive $false -OnWindows $false) 'the first reason that applies is the one given'
}
Invoke-TestCase 'only verified GitHub-hosted Windows jobs bypass the interactive desktop lease' {
    $hosted = @{ GITHUB_ACTIONS='true'; CI='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows' }
    Assert-True (Test-DxUiVerifiedGitHubHostedRunner -Environment $hosted) 'complete hosted metadata qualifies'
    foreach ($mutation in @(
        @{ GITHUB_ACTIONS='true'; CI='true'; RUNNER_ENVIRONMENT='self-hosted'; RUNNER_OS='Windows' },
        @{ GITHUB_ACTIONS='true'; CI='true'; RUNNER_ENVIRONMENT=''; RUNNER_OS='Windows' },
        @{ GITHUB_ACTIONS='true'; CI=''; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows' },
        @{ GITHUB_ACTIONS='true'; CI='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Linux' }
    )) { Assert-True (-not (Test-DxUiVerifiedGitHubHostedRunner -Environment $mutation)) 'partial/self-hosted/non-Windows metadata never bypasses' }
}
Invoke-TestCase 'hosted foreground mode is selected only for verified hosted jobs that contain a foreground suite' {
    $hosted = @{ GITHUB_ACTIONS='true'; CI='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows' }
    Assert-True (Test-DxUiHostedForegroundLeaseRequired -Suites @('Menu') -VerifiedHostedRunner (Test-DxUiVerifiedGitHubHostedRunner -Environment $hosted)) 'a verified hosted Menu run uses the foreground lease'
    Assert-True (Test-DxUiHostedForegroundLeaseRequired -Suites @('MenuResources') -VerifiedHostedRunner $true) 'the named menu fixture also uses it'
    Assert-True (-not (Test-DxUiHostedForegroundLeaseRequired -Suites @('Grid','Embedded') -VerifiedHostedRunner $true)) 'ordinary hosted suites do not use it'
    Assert-True (-not (Test-DxUiHostedForegroundLeaseRequired -Suites @('Menu') -VerifiedHostedRunner $false)) 'a local run never gets hosted mode'
    Assert-Equal 'Menu,NativeTextInput' ((Resolve-DxUiForegroundLeaseSuites -Suites @('Foundation','Menu','Grid','NativeTextInput') -Interactive $false -VerifiedHostedRunner $true) -join ',') 'a hosted full run leases only its foreground suites'
    Assert-Equal 'Menu,NativeTextInput' ((Resolve-DxUiForegroundLeaseSuites -Suites @('Menu','NativeTextInput') -Interactive $true -VerifiedHostedRunner $false) -join ',') 'a local interactive run leases its selected suites'
    Assert-Equal 0 @(Resolve-DxUiForegroundLeaseSuites -Suites @('Grid','Embedded') -Interactive $false -VerifiedHostedRunner $false).Count 'a local noninteractive run has no lease'
}
Invoke-TestCase 'the estimate is the suites and the lease overhead, per configuration' {
    Assert-Equal 85 (Get-DxUiInteractiveEstimateSeconds -Suites @('Menu', 'NativeTextInput') -Configuration 'Debug') 'Debug: 20 + 45 + 20 seconds'
    Assert-Equal 80 (Get-DxUiInteractiveEstimateSeconds -Suites @('Menu', 'NativeTextInput') -Configuration 'Release') 'Release'
    Assert-Equal 100 (Get-DxUiInteractiveEstimateSeconds -Suites @('Menu', 'NativeTextInput') -Configuration 'ASan Debug') 'ASan Debug runs longer'
    Assert-Equal 140 (Get-DxUiInteractiveEstimateSeconds -Suites @('MenuResourceScaling') -Configuration 'Release') 'a fixture, estimated'
    foreach ($suite in Get-DxUiInteractiveSuiteNames -IncludeOptIn) {
        foreach ($configuration in @('Debug', 'Release', 'ASan Debug')) { Assert-True ((Get-DxUiInteractiveEstimateSeconds -Suites @($suite) -Configuration $configuration) -gt 20) "$suite has an estimate in $configuration" }
    }
}
Invoke-TestCase 'a command line is quoted as CreateProcess reads it' {
    Assert-Equal '"C:\a\tests.exe" --suite=Menu' (ConvertTo-DxUiCommandLine -Program 'C:\a\tests.exe' -Arguments @('--suite=Menu')) 'plain arguments are left alone'
    Assert-Equal '"C:\Program Files\x\tests.exe" "a b" c' (ConvertTo-DxUiCommandLine -Program 'C:\Program Files\x\tests.exe' -Arguments @('a b', 'c')) 'the program is always quoted, an argument when it holds a space'
    Assert-Equal '"p" "say \"hi\""' (ConvertTo-DxUiCommandLine -Program 'p' -Arguments @('say "hi"')) 'a quote is escaped'
    Assert-Equal '"p" "C:\dir with space\\"' (ConvertTo-DxUiCommandLine -Program 'p' -Arguments @('C:\dir with space\')) 'a backslash before the closing quote is doubled'
    Assert-Equal '"p" --test=A,B' (ConvertTo-DxUiCommandLine -Program 'p' -Arguments @('--test=A,B')) 'a comma is not special'
}
Invoke-TestCase 'the plan is one tab-separated line per child' {
    $text = New-DxUiLeasePlanText -Runs @([pscustomobject]@{ Name = 'Menu'; Log = 'C:\l\Menu.log'; CommandLine = '"C:\t.exe" --suite=Menu' }, [pscustomobject]@{ Name = 'NativeTextInput'; Log = 'C:\l\N.log'; CommandLine = '"C:\t.exe" --suite=NativeTextInput' })
    Assert-Equal "Menu`tC:\l\Menu.log`t`"C:\t.exe`" --suite=Menu`nNativeTextInput`tC:\l\N.log`t`"C:\t.exe`" --suite=NativeTextInput`n" $text 'each child has a line of name, log and command line'
    foreach ($bad in @(@('', 'l', 'c'), @('n', "l`tx", 'c'), @('n', 'l', "c`nd"), @('n', '', 'c'))) {
        Assert-Throws { [void](New-DxUiLeasePlanText -Runs @([pscustomobject]@{ Name = $bad[0]; Log = $bad[1]; CommandLine = $bad[2] })) } "a field that is empty or holds a tab or a line break is refused: $($bad -join '|')"
    }
}
$completed = @(
    'state: completed', 'exit: 1', 'confirmation: started', 'confirmation.autoStart: 0', 'confirmation.seconds: 120',
    'foreground.saved: class ''CASCADIA_HOSTING_WINDOW_CLASS'' of WindowsTerminal.exe (process 4242)',
    'focus.capture.reason: recorded', 'focus.capture.error: 0',
    'restoration.foreground: restored', 'restoration.focus: unchanged', 'restoration.cursor: restored', 'cursor.saved: 400,300', 'cursor.atExit: 900,700',
    'restoration.runMovedSomething: 1', 'child: Menu launched=1 exit=0 seconds=43.3 timedout=0 interrupted=0',
    'child: NativeTextInput launched=1 exit=3221225477 seconds=1.2 timedout=0 interrupted=0') -join "`r`n"
$successful = $completed.Replace("exit: 1`r`n", "exit: 0`r`n").Replace('exit=3221225477', 'exit=0')
Invoke-TestCase 'the result is read back as the lease writes it' {
    $result = Read-DxUiLeaseResult -Text $completed
    Assert-Equal 'completed' $result.State 'the state'
    Assert-Equal 1 $result.Exit 'the exit code'
    Assert-Equal 'restored' $result.Foreground 'the foreground'
    Assert-Equal 'unchanged' $result.Focus 'the focus'
    Assert-Equal 'restored' $result.Cursor 'the pointer'
    Assert-Equal '400,300' $result.CursorSaved 'where the person had the pointer'
    Assert-Equal '900,700' $result.CursorAtExit 'where the run left it'
    Assert-True $result.RunMovedSomething 'the run had moved something'
    Assert-True $result.SavedForeground.Contains('WindowsTerminal.exe') 'the window is named'
    Assert-Equal 2 @($result.Children).Count 'both children'
    Assert-Equal 'Menu' $result.Children[0].Name 'the first child'
    Assert-Equal 0 $result.Children[0].ExitCode 'its exit code'
    Assert-Equal 43.3 $result.Children[0].Seconds 'its seconds'
    Assert-Equal (-1073741819) $result.Children[1].ExitCode 'a crash code reads as $LASTEXITCODE shows it'
    Assert-True ($result.Children[1].Launched -and -not $result.Children[1].TimedOut -and -not $result.Children[1].Interrupted) 'and its flags'
}
Invoke-TestCase 'cursor restore failure diagnostics are bounded, typed observations and remain optional for historical receipts' {
    $cursorDiagnostics = 'readBefore=900,700;readBeforeError=0;setCalled=1;setAccepted=0;setError=5;readback=900,700;readbackError=0;' +
        'clipCapture=0,0,1920,1080;clipCaptureError=0;clipAtExit=0,0,1920,1080;clipAtExitError=0;clipAfter=0,0,1920,1080;clipAfterError=0;' +
        'inputCapture=Default;inputCaptureError=0;inputAtExit=Default;inputAtExitError=0;inputAfter=Default;inputAfterError=0;' +
        'threadCapture=Default;threadCaptureError=0;threadAtExit=Default;threadAtExitError=0;threadAfter=Default;threadAfterError=0'
    $failureText = $successful.Replace('exit: 0', 'exit: 25').Replace('restoration.cursor: restored', 'restoration.cursor: failed') +
        "`ncursor.restore.diagnostics: $cursorDiagnostics"
    $result = Read-DxUiLeaseResult -Text $failureText
    Assert-Equal 0 @($result.ReceiptProblems).Count 'the full failure diagnostic validates without turning it into a pass'
    Assert-Equal '900,700' $result.CursorDiagnostics.ReadBefore 'the immediate pre-set readback is retained'
    Assert-Equal ([uint32]5) $result.CursorDiagnostics.SetError 'the setter error observation is retained'
    Assert-True (-not $result.CursorDiagnostics.SetAccepted) 'setter rejection is distinct from readback'
    Assert-Equal '0,0,1920,1080' $result.CursorDiagnostics.ClipAfter 'the final clip rectangle is retained'
    Assert-Equal 'Default' $result.CursorDiagnostics.InputDesktopAfter 'the bounded desktop name is retained'
    Assert-Equal $null (Read-DxUiLeaseResult -Text $successful).CursorDiagnostics 'old results remain readable without invented diagnostics'
    $contradictory = Read-DxUiLeaseResult -Text ($successful + "`ncursor.restore.diagnostics: $cursorDiagnostics")
    Assert-True (@($contradictory.ReceiptProblems).Count -gt 0) 'diagnostics cannot accompany a successful pointer restoration'
    $preReadFailure = $cursorDiagnostics.Replace('readBefore=900,700', 'readBefore=unknown').Replace('readBeforeError=0', 'readBeforeError=13').
        Replace('setCalled=1', 'setCalled=0').Replace('setError=5', 'setError=0').Replace('readback=900,700', 'readback=unknown')
    $preReadResult = Read-DxUiLeaseResult -Text ($successful.Replace('exit: 0', 'exit: 25').Replace('restoration.cursor: restored', 'restoration.cursor: failed') +
        "`ncursor.restore.diagnostics: $preReadFailure")
    Assert-Equal 0 @($preReadResult.ReceiptProblems).Count 'a failed pre-set read is distinct from an unattempted set/readback'

    foreach ($mutation in @(
        @('threadAfterError=0', ''),
        @('setAccepted=0', 'setAccepted=true'),
        @('setError=5', 'setError=4294967296'),
        @('readback=900,700', 'readback=2147483648,0'),
        @('clipAfter=0,0,1920,1080', 'clipAfter=0,0,broken,1080'),
        @('inputAfter=Default', 'inputAfter=desktop with spaces'),
        @('threadAfterError=0', 'extra=0'),
        @('setAccepted=0', 'setAccepted=0;setAccepted=1'),
        @('setCalled=1;setAccepted=0', 'setCalled=0;setAccepted=1')
    )) {
        $changedDiagnostics = $cursorDiagnostics.Replace($mutation[0], $mutation[1])
        $changedText = $successful.Replace('exit: 0', 'exit: 25').Replace('restoration.cursor: restored', 'restoration.cursor: failed') +
            "`ncursor.restore.diagnostics: $changedDiagnostics"
        $parsed = Read-DxUiLeaseResult -Text $changedText
        Assert-True (@($parsed.ReceiptProblems).Count -gt 0) "malformed cursor restoration observations fail receipt validation: $($mutation -join ' -> ')"
    }
}
Invoke-TestCase 'a result that is missing, empty or not the lease''s is no result' {
    Assert-Equal $null (Read-DxUiLeaseResult -Text $null) 'no text'
    Assert-Equal $null (Read-DxUiLeaseResult -Text '') 'empty text'
    Assert-Equal $null (Read-DxUiLeaseResult -Text "  `r`n") 'blank text'
    Assert-Equal $null (Read-DxUiLeaseResult -Text "exit: 0`nconfirmation: started`n") 'text without a state'
    $refused = Read-DxUiLeaseResult -Text "state: refused`nexit: 20`nreason: the session is locked`nconfirmation: none`n"
    Assert-Equal 'refused' $refused.State 'a refusal reads'
    Assert-Equal 'the session is locked' $refused.Reason 'with its reason'
    Assert-Equal 'none' $refused.Foreground 'and no restoration'
    Assert-Equal 0 @($refused.Children).Count 'and no child'
    Assert-True (-not $refused.RunMovedSomething) 'nothing was moved'
}
Invoke-TestCase 'capture reasons retain skipped, failed, null and recorded query outcomes without changing restoration claims' {
    foreach ($reason in 'no-foreground','no-foreground-thread','foreground-minimized','query-failed','query-returned-null','recorded') {
        $focus = if ($reason -ceq 'recorded') { 'restored' } else { 'none' }
        $queryError = if ($reason -ceq 'query-failed') { 5 } else { 0 }
        $text = $successful.Replace('restoration.focus: unchanged', "restoration.focus: $focus").
            Replace('focus.capture.reason: recorded', "focus.capture.reason: $reason").Replace('focus.capture.error: 0', "focus.capture.error: $queryError")
        $result = Read-DxUiLeaseResult -Text $text
        Assert-Equal $reason $result.FocusCapture.Reason 'the original reason survives parsing'
        Assert-Equal ([uint32]$queryError) $result.FocusCapture.QueryError 'with the immediate query error, meaningful only on failure'
        $problems = @(Get-DxUiLeaseProblems -Result $result -ExitCode 0)
        if ($reason -cin @('recorded','query-returned-null','foreground-minimized')) {
            Assert-Equal 0 $problems.Count 'recorded focus or a definitive absent focus has a complete truthful result'
            $qualified = @(Get-DxUiLeaseProblems -Result $result -ExitCode 0 -RequireRecordedFocus)
            if ($reason -ceq 'recorded') { Assert-Equal 0 $qualified.Count 'recorded focus qualifies the full focus round trip' }
            else { Assert-True ($qualified -match 'requires recorded initial active focus') 'known absence cannot qualify a full focus round trip' }
        } else {
            Assert-True ($problems -match 'keyboard focus capture was not verified') 'failed or unavailable focus capture cannot qualify a successful lease'
        }
        $shown = Get-RunnerLeasePresentation $result
        Assert-True ($shown.Contains("Focus capture before confirmation: $reason")) 'the canonical runner presents the original reason'
        if ($reason -ceq 'recorded') {
            Assert-True ($shown.Contains('query succeeded with focus') -and $shown.Contains('states were restored or unchanged')) 'a recorded focus can have a verified restoration'
        } else {
            Assert-True ($shown.Contains('focus none') -and $shown.Contains('not fully verified')) 'a reason cannot promote none into verified restoration'
        }
        if ($reason -ceq 'query-failed') { Assert-True ($shown.Contains('query failed, error 5')) 'query failure includes its bounded error' }
        elseif ($reason -ceq 'query-returned-null') { Assert-True ($shown.Contains('query succeeded with no focus')) 'success with null is distinct from failure' }
        elseif ($reason -cne 'recorded') { Assert-True ($shown.Contains('query not attempted')) 'guards do not imply a query happened' }
    }
    $legacyText = (($completed -split '\r?\n') | Where-Object { $_ -notlike 'focus.capture.*' }) -join "`n"
    $legacy = Read-DxUiLeaseResult -Text ($legacyText -replace 'restoration.focus: unchanged', 'restoration.focus: none')
    Assert-Equal $null $legacy.FocusCapture 'historical receipts remain readable without inventing capture facts'
    Assert-True ((Get-RunnerLeasePresentation $legacy).Contains('diagnostic unavailable in this receipt')) 'historical absence remains explicit'
}
Invoke-TestCase 'partial, repeated, unknown and contradictory capture diagnostics fail closed' {
    $baseline = "state: completed`nrestoration.foreground: restored`nrestoration.focus: none`nrestoration.cursor: restored`n"
    foreach ($facts in @(
        "focus.capture.reason: foreground-minimized`n",
        "focus.capture.error: 0`n",
        "focus.capture.reason: unknown`nfocus.capture.error: 0`n",
        "focus.capture.reason: Recorded`nfocus.capture.error: 0`n",
        "focus.capture.reason: query-failed`nfocus.capture.error: -1`n",
        "focus.capture.reason: query-failed`nfocus.capture.error: 4294967296`n",
        "focus.capture.reason: query-failed`nfocus.capture.error: text`n",
        "focus.capture.reason: foreground-minimized`nfocus.capture.error: 5`n",
        "focus.capture.reason: recorded`nfocus.capture.error: 0`n",
        "focus.capture.reason: query-failed`nfocus.capture.error: 5`nfocus.capture.reason: foreground-minimized`n",
        "focus.capture.reason: foreground-minimized`nfocus.capture.error: 0`nfocus.capture.error: 0`n"
    )) {
        $result = Read-DxUiLeaseResult -Text ($baseline + $facts)
        Assert-Equal $null $result.FocusCapture 'invalid diagnostics establish no reason'
        Assert-True (@(Get-DxUiLeaseProblems -Result $result -ExitCode 0) -match 'invalid focus capture diagnostic') 'malformed diagnostic makes the result fail validation'
        Assert-True ((Get-RunnerLeasePresentation $result).Contains('invalid diagnostic; no capture reason established')) 'presentation does not infer a reason from malformed metadata'
    }
    foreach ($errorCode in 0, 4294967295) {
        $result = Read-DxUiLeaseResult -Text ($baseline + "focus.capture.reason: query-failed`nfocus.capture.error: $errorCode`n")
        Assert-Equal ([uint32]$errorCode) $result.FocusCapture.QueryError 'the entire DWORD error domain is retained, including a failure without an error code'
    }
    $contradiction = Read-DxUiLeaseResult -Text ($baseline.Replace('restoration.focus: none', 'restoration.focus: unchanged') + "focus.capture.reason: query-returned-null`nfocus.capture.error: 0`n")
    Assert-True (@(Get-DxUiLeaseProblems -Result $contradiction -ExitCode 0) -match 'contradicts focus restoration') 'unrecorded focus cannot claim unchanged restoration'
}
Invoke-FixtureCase 'both child receipts copy one original capture diagnostic and keep separate durations' { param($fixture)
    $assignment = @($testAst.FindAll({ param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -ceq '$receipt[''lease'']'
    }, $true))
    Assert-Equal 1 $assignment.Count 'one canonical lease receipt mapping'
    $writeLease = [scriptblock]::Create($assignment[0].Extent.Text)
    $leaseResult = Read-DxUiLeaseResult -Text ($completed.Replace('restoration.focus: unchanged', 'restoration.focus: none').
        Replace('focus.capture.reason: recorded', 'focus.capture.reason: foreground-minimized'))
    $lease = [pscustomobject]@{ ExitCode=0; RequestedAutoStart=$false; RequestedConfirmationSeconds=0 }
    $hostedForegroundLease = $true
    $InteractiveAutoStart = $false
    Set-FixtureFile $fixture 'lease.exe' 'synthetic identity only'
    $leaseExecutable = Join-Path $fixture 'lease.exe'
    $copies = foreach ($child in $leaseResult.Children) {
        $receipt = [ordered]@{}
        . $writeLease
        ($receipt | ConvertTo-Json -Depth 8 | ConvertFrom-Json).lease
    }
    Assert-Equal 2 @($copies).Count 'one receipt per child'
    Assert-Equal ($copies[0].focusCapture | ConvertTo-Json -Compress) ($copies[1].focusCapture | ConvertTo-Json -Compress) 'the same diagnostic is copied, not recaptured per child'
    Assert-Equal 'foreground-minimized' $copies[0].focusCapture.Reason 'the original reason survives JSON'
    Assert-True ($copies[0].focusCapture.QueryError -eq 0) 'with no fabricated query error'
    Assert-Equal $null $copies[0].cursorDiagnostics 'legacy results preserve absence rather than inventing cursor diagnostics'
    Assert-Equal 43.3 $copies[0].seconds 'Menu duration remains its own'
    Assert-Equal 1.2 $copies[1].seconds 'NativeTextInput duration remains its own'
    Assert-Equal 'QueryError,Reason' (($copies[0].focusCapture.PSObject.Properties.Name | Sort-Object) -join ',') 'diagnostics expose no raw window, process or thread identities'
    $cursorDiagnostic = 'readBefore=900,700;readBeforeError=0;setCalled=1;setAccepted=0;setError=5;readback=900,700;readbackError=0;' +
        'clipCapture=unknown;clipCaptureError=0;clipAtExit=unknown;clipAtExitError=0;clipAfter=unknown;clipAfterError=0;' +
        'inputCapture=Default;inputCaptureError=0;inputAtExit=Default;inputAtExitError=0;inputAfter=Default;inputAfterError=0;' +
        'threadCapture=Default;threadCaptureError=0;threadAtExit=Default;threadAtExitError=0;threadAfter=Default;threadAfterError=0'
    $leaseResult = Read-DxUiLeaseResult -Text ($successful.Replace('exit: 0', 'exit: 25').Replace('restoration.cursor: restored', 'restoration.cursor: failed') +
        "`ncursor.restore.diagnostics: $cursorDiagnostic")
    $lease.ExitCode = 25
    $diagnosticCopies = foreach ($child in $leaseResult.Children) {
        $receipt = [ordered]@{}
        . $writeLease
        ($receipt | ConvertTo-Json -Depth 8 | ConvertFrom-Json).lease
    }
    Assert-Equal 2 @($diagnosticCopies).Count 'cursor failure diagnostics are carried to each child receipt'
    Assert-Equal ($diagnosticCopies[0].cursorDiagnostics | ConvertTo-Json -Compress) ($diagnosticCopies[1].cursorDiagnostics | ConvertTo-Json -Compress) 'both children retain the same native lease snapshot'
    Assert-Equal '5' ([string]$diagnosticCopies[0].cursorDiagnostics.SetError) 'the exact setter observation survives receipt JSON'
    Assert-True (-not $diagnosticCopies[0].cursorDiagnostics.SetAccepted) 'a rejected setter remains a failure in child evidence'
    $lease.ExitCode = 0
    $missing = @($testAst.FindAll({ param($node)
        $node -is [Management.Automation.Language.AssignmentStatementAst] -and $node.Left.Extent.Text -ceq '$leaseResult' -and
        $node.Right.Extent.Text.Contains('the native lease wrote no result')
    }, $true))
    Assert-Equal 1 $missing.Count 'one canonical fallback when the native result is missing'
    $lease = [pscustomobject]@{ Result=$null; ExitCode=24; RequestedAutoStart=$false; RequestedConfirmationSeconds=0 }
    . ([scriptblock]::Create($missing[0].Extent.Text))
    $receipt = [ordered]@{}
    $child = $null
    . $writeLease
    Assert-Equal 'missing' $receipt.lease.state 'a missing result still writes its failed receipt under strict mode'
    Assert-Equal $null $receipt.lease.focusCapture 'the fallback invents no capture reason'
    Assert-Equal $null $receipt.lease.cursorDiagnostics 'the fallback invents no pointer restoration evidence'
    Assert-Equal 0 @($receipt.lease.focusCaptureProblems).Count 'absence of a result is distinct from malformed metadata'
    Assert-True (@(Get-DxUiLeaseProblems -Result $leaseResult -ExitCode 24) -match 'native lease wrote no result') 'the missing result remains a failure'
}
Invoke-TestCase 'what is wrong with a finished lease is said in a message' {
    $fine = Read-DxUiLeaseResult -Text $successful
    Assert-Equal 0 @(Get-DxUiLeaseProblems -Result $fine -ExitCode 0 -ExpectedChildren @('Menu','NativeTextInput')).Count 'a lease that completed its exact children with its desktop restored has no problem'
    $noResult = @(Get-DxUiLeaseProblems -Result $null -ExitCode 21)
    Assert-True ($noResult.Count -eq 1 -and $noResult[0].Contains('wrote no result') -and $noResult[0].Contains('cancelled')) "no result is named with what its exit code means: $noResult"
    $declined = Read-DxUiLeaseResult -Text "state: declined`nexit: 21`nreason: the person chose Cancel`nconfirmation: declined`n"
    $problems = @(Get-DxUiLeaseProblems -Result $declined -ExitCode 21)
    Assert-True ($problems.Count -eq 1 -and $problems[0].Contains('(declined)') -and $problems[0].Contains('the person chose Cancel')) "a cancelled run did not take place: $problems"
    $failed = Read-DxUiLeaseResult -Text ($completed -replace 'restoration.focus: unchanged', 'restoration.focus: failed' -replace 'restoration.cursor: restored', 'restoration.cursor: failed')
    $problems = @(Get-DxUiLeaseProblems -Result $failed -ExitCode 1)
    Assert-True ($problems -match 'focus, cursor failed.*WindowsTerminal.exe') "a part that was not given back is named, with the window: $problems"
}
Invoke-TestCase 'completed receipts reject missing, duplicate, unknown and malformed protocol facts without throwing' {
    $required = @('state','exit','confirmation','confirmation.autoStart','confirmation.seconds','foreground.saved',
        'focus.capture.reason','focus.capture.error','restoration.foreground','restoration.focus','restoration.cursor',
        'cursor.saved','cursor.atExit','restoration.runMovedSomething')
    foreach ($key in $required) {
        $lines = @($successful -split '\r?\n')
        $missing = ($lines | Where-Object { -not $_.StartsWith($key + ': ') }) -join "`n"
        Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $missing) -ExitCode 0).Count -gt 0) "missing $key cannot pass"
        $line = @($lines | Where-Object { $_.StartsWith($key + ': ') })[0]
        $duplicate = $successful + "`n" + $line
        Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $duplicate) -ExitCode 0).Count -gt 0) "repeated $key cannot pass"
    }
    foreach ($extra in @('new.unrecognized: fact','a damaged line','exit: 99999999999999999999999',
        'confirmation.seconds: 4294967296','confirmation.autoStart: true','restoration.runMovedSomething: true',
        'restoration.foreground: Restored','cursor.saved: 2147483648,0','cursor.atExit: 0,-2147483649')) {
        $parsed = Read-DxUiLeaseResult -Text ($successful + "`n" + $extra)
        Assert-True (@(Get-DxUiLeaseProblems -Result $parsed -ExitCode 0).Count -gt 0) "invalid protocol fact cannot pass: $extra"
    }
    $truncated = "state: completed`nexit: 0`nconfirmation: autostarted`nconfirmation.autoStart: 1`nconfirmation.seconds: 30`n"
    Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $truncated) -ExitCode 0).Count -gt 0) 'a truncated approved receipt cannot establish restoration or child execution'
    $mismatch = @(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $successful) -ExitCode 25)
    Assert-True ($mismatch -match 'differs from process exit') 'receipt and actual process exit must agree'
}
Invoke-TestCase 'child receipt integrity binds exact requested names, order and successful execution' {
    foreach ($children in @(@(), @('Menu'), @('Menu','NativeTextInput','Extra'), @('NativeTextInput','Menu'), @('Menu','Other'))) {
        $parsed = Read-DxUiLeaseResult -Text $successful
        Assert-True (@(Get-DxUiLeaseProblems -Result $parsed -ExitCode 0 -ExpectedChildren $children).Count -gt 0) "a different plan cannot pass: $($children -join ',')"
    }
    $childLines = @($successful -split '\r?\n' | Where-Object { $_ -like 'child:*' })
    $prefix = (($successful -split '\r?\n' | Where-Object { $_ -notlike 'child:*' }) -join "`n") + "`n"
    foreach ($rows in @('', $childLines[0], "$($childLines[0])`n$($childLines[0])", "$($childLines[1])`n$($childLines[0])")) {
        Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text ($prefix + $rows)) -ExitCode 0 -ExpectedChildren @('Menu','NativeTextInput')).Count -gt 0) 'missing, duplicated or reordered executions cannot qualify'
    }
    foreach ($mutation in @(@('launched=1','launched=0'), @('exit=0','exit=124'), @('timedout=0','timedout=1'),
        @('interrupted=0','interrupted=1'), @('seconds=43.3','seconds=-1'), @('seconds=43.3','seconds=1.2.3'),
        @('exit=0','exit=4294967296'), @('timedout=0','timedout=true'))) {
        $changed = $successful.Replace($mutation[0], $mutation[1])
        Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $changed) -ExitCode 0).Count -gt 0) "invalid child execution cannot pass: $($mutation -join ' -> ')"
    }
    $withSpaces = $successful.Replace('child: NativeTextInput ', 'child: Native text input ')
    Assert-Equal 0 @(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $withSpaces) -ExitCode 0 -ExpectedChildren @('Menu','Native text input')).Count 'names with spaces round-trip exactly like plan fields'
}
Invoke-TestCase 'truthful gone and unknown restoration remains readable but cannot qualify a restored lease' {
    foreach ($part in 'foreground','focus','cursor') {
        foreach ($status in 'gone','skipped','none','failed','unknown') {
            $old = if ($part -ceq 'focus') { 'unchanged' } else { 'restored' }
            $text = $successful.Replace("restoration.${part}: $old", "restoration.${part}: $status")
            $parsed = Read-DxUiLeaseResult -Text $text
            Assert-Equal $status $parsed.$part 'the original restoration observation remains visible'
            Assert-True (@(Get-DxUiLeaseProblems -Result $parsed -ExitCode 0).Count -gt 0) "$part=$status cannot establish a restored desktop"
        }
    }
    $unknownPointer = $successful.Replace('cursor.saved: 400,300', 'cursor.saved: unknown')
    Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $unknownPointer) -ExitCode 0) -match 'no recorded starting position') 'pointer restoration requires a known original position'
    foreach ($sentinel in 'none','unknown','no window') {
        $text = $successful -replace 'foreground.saved: [^\r\n]*', "foreground.saved: $sentinel"
        Assert-True (@(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $text) -ExitCode 0) -match 'no foreground target') 'a textual sentinel is not a recorded foreground target'
    }
    # atExit is read BEFORE restoration, not after it. A moved coordinate or transient failure reading atExit
    # can coexist with restored/unchanged: native RestoreCursor independently reads, sets and verifies later.
    $unknownAtExit = $successful.Replace('cursor.atExit: 900,700', 'cursor.atExit: unknown')
    Assert-Equal 0 @(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $unknownAtExit) -ExitCode 0).Count 'pre-restoration pointer observation does not replace the native restoration readback'
    $unchanged = $successful.Replace('restoration.foreground: restored','restoration.foreground: unchanged').
        Replace('restoration.cursor: restored','restoration.cursor: unchanged').Replace('cursor.atExit: 900,700','cursor.atExit: 400,300').
        Replace('restoration.runMovedSomething: 1','restoration.runMovedSomething: 0')
    Assert-Equal 0 @(Get-DxUiLeaseProblems -Result (Read-DxUiLeaseResult -Text $unchanged) -ExitCode 0).Count 'a complete unchanged desktop remains a valid success'
}
Invoke-TestCase 'the runner cannot describe failed restoration as an unchanged desktop' {
    $result = Read-DxUiLeaseResult -Text "state: timedout`nexit: 25`nconfirmation: timedout`nrestoration.foreground: failed`nrestoration.focus: skipped`nrestoration.cursor: failed`ncursor.saved: 400,300`ncursor.atExit: unknown`nrestoration.runMovedSomething: 0`n"
    $text = Get-RunnerLeasePresentation $result
    Assert-True ($text.Contains('foreground failed, focus skipped, pointer failed')) 'the recorded failed and skipped parts remain visible'
    Assert-True (-not $text.Contains('the run left the desktop as the person had it')) 'a false moved flag cannot imply successful restoration'
    Assert-True ($text.Contains('Desktop restoration failed')) 'the summary explicitly reports restoration failure'
}
Invoke-TestCase 'unchanged and restored summaries depend on the recorded statuses, not the moved flag' {
    foreach ($moved in 0, 1) {
        $unchanged = Read-DxUiLeaseResult -Text "state: completed`nrestoration.foreground: unchanged`nrestoration.focus: unchanged`nrestoration.cursor: unchanged`nrestoration.runMovedSomething: $moved`n"
        $summary = Get-DxUiLeaseSummary -Result $unchanged
        Assert-Equal 'Foreground lease (completed): foreground unchanged, focus unchanged, pointer unchanged; All three desktop states were unchanged.' $summary.Message "unchanged parts with moved flag $moved"
        Assert-Equal $null $summary.PointerNotice 'an unchanged pointer has no restoration notice'
        $restored = Read-DxUiLeaseResult -Text ($completed -replace 'restoration.runMovedSomething: 1', "restoration.runMovedSomething: $moved")
        Assert-Equal 'Foreground lease (completed): foreground restored, focus unchanged, pointer restored; The desktop states were restored or unchanged.' (Get-DxUiLeaseSummary -Result $restored).Message "restored parts with moved flag $moved"
    }
}
Invoke-TestCase 'every unsuccessful or unknown restoration status stays visible and cannot imply an unchanged desktop' {
    $baseline = "state: completed`nrestoration.foreground: unchanged`nrestoration.focus: unchanged`nrestoration.cursor: unchanged`nrestoration.runMovedSomething: 1`n"
    foreach ($part in @(@{ Key='foreground'; Label='foreground' }, @{ Key='focus'; Label='focus' }, @{ Key='cursor'; Label='pointer' })) {
        foreach ($status in 'gone', 'failed', 'skipped', 'none', 'unknown', 'Restored') {
            $text = $baseline -replace [regex]::Escape("restoration.$($part.Key): unchanged"), "restoration.$($part.Key): $status"
            $summary = Get-DxUiLeaseSummary -Result (Read-DxUiLeaseResult -Text $text)
            Assert-True ($summary.Message.Contains("$($part.Label) $status")) "the literal $($part.Key) status $status is retained"
            $conclusion = if ($status -ceq 'failed') { 'Desktop restoration failed.' } else { 'Desktop restoration was not fully verified.' }
            Assert-True ($summary.Message.EndsWith($conclusion)) "$($part.Key)=$status does not establish complete restoration"
            Assert-Equal $null $summary.PointerNotice 'only an exact recorded pointer restoration produces a notice'
        }
    }
}
Invoke-TestCase 'a missing result or missing restoration fields produces an unverified summary' {
    foreach ($result in @($null, [pscustomobject]@{}, [pscustomobject]@{ State=''; Foreground=''; Focus=$null; Cursor='' })) {
        $summary = Get-DxUiLeaseSummary -Result $result
        Assert-Equal 'Foreground lease (missing): foreground none, focus none, pointer none; Desktop restoration was not fully verified.' $summary.Message 'missing facts never establish an unchanged desktop'
        Assert-Equal $null $summary.PointerNotice 'no pointer restoration is invented'
    }
    $missingFields = Read-DxUiLeaseResult -Text "state: completed`n"
    Assert-True ((Get-DxUiLeaseSummary -Result $missingFields).Message.EndsWith('Desktop restoration was not fully verified.')) 'a completed state does not fill in absent restoration facts'
    foreach ($part in @(@{ Key='foreground'; Label='foreground' }, @{ Key='focus'; Label='focus' }, @{ Key='cursor'; Label='pointer' })) {
        $text = (($completed -split '\r?\n') | Where-Object { $_ -notlike "restoration.$($part.Key):*" }) -join "`n"
        $summary = Get-DxUiLeaseSummary -Result (Read-DxUiLeaseResult -Text $text)
        Assert-True ($summary.Message.Contains("$($part.Label) none") -and $summary.Message.EndsWith('Desktop restoration was not fully verified.')) "the missing $($part.Key) is not inferred from the other parts"
    }
    Assert-True ((Get-RunnerLeasePresentation $null).Contains('Desktop restoration was not fully verified.')) 'the canonical runner presentation accepts a missing result'
}
Invoke-TestCase 'mixed restoration reports retain successful parts without hiding a failure or mutating the result' {
    $result = Read-DxUiLeaseResult -Text "state: completed`nexit: 25`nrestoration.foreground: restored`nrestoration.focus: failed`nrestoration.cursor: restored`ncursor.saved: 400,300`ncursor.atExit: 900,700`nrestoration.runMovedSomething: 1`n"
    $before = $result | ConvertTo-Json -Depth 4 -Compress
    $summary = Get-DxUiLeaseSummary -Result $result
    Assert-Equal 'Foreground lease (completed): foreground restored, focus failed, pointer restored; Desktop restoration failed.' $summary.Message 'a successful pointer restoration does not hide failed focus restoration'
    Assert-True ($summary.PointerNotice.StartsWith('The lease restored the pointer from 900,700 to 400,300.')) 'the independently restored pointer remains explicit'
    Assert-Equal $before ($result | ConvertTo-Json -Depth 4 -Compress) 'presentation changes no result facts'
    $problems = @(Get-DxUiLeaseProblems -Result $result -ExitCode 25)
    Assert-True ($problems -match 'focus failed') 'the existing restoration failure accounting stays failing'
    $result.Focus = 'unknown'
    Assert-True ((Get-DxUiLeaseSummary -Result $result).Message.EndsWith('Desktop restoration was not fully verified.')) 'a restored pointer also cannot verify an unknown focus state'
}
Invoke-TestCase 'Cancel and confirmation timeout retain their incomplete state even when desktop restoration succeeded' {
    foreach ($state in 'declined', 'timedout') {
        foreach ($cursor in 'unchanged', 'restored') {
            $result = Read-DxUiLeaseResult -Text "state: $state`nexit: 21`nreason: $state before a child started`nconfirmation: $state`nrestoration.foreground: unchanged`nrestoration.focus: unchanged`nrestoration.cursor: $cursor`ncursor.saved: 400,300`ncursor.atExit: 900,700`nrestoration.runMovedSomething: 0`n"
            $text = Get-RunnerLeasePresentation $result
            Assert-True ($text.Contains("Foreground lease ($state):") -and $text.Contains("pointer $cursor")) 'presentation preserves the incomplete state and restoration facts separately'
            Assert-True (-not $text.Contains('suites passed')) 'restoration does not assert that suites ran or passed'
            Assert-Equal 0 @($result.Children).Count 'the confirmation failure has no child'
            $problems = @(Get-DxUiLeaseProblems -Result $result -ExitCode 21)
            Assert-True ($problems.Count -eq 1 -and $problems[0].Contains("($state)")) 'the existing Cancel or timeout accounting still fails'
        }
    }
}
Invoke-TestCase 'the pointer notice reports positions without attributing movement to a suite or inventing missing coordinates' {
    $result = Read-DxUiLeaseResult -Text ($completed -replace 'cursor.saved: 400,300', 'cursor.saved: -400,-300' -replace 'cursor.atExit: 900,700', 'cursor.atExit: -900,700')
    $notice = (Get-DxUiLeaseSummary -Result $result).PointerNotice
    Assert-True ($notice.StartsWith('The lease restored the pointer from -900,700 to -400,-300.')) 'negative physical desktop coordinates remain intact'
    Assert-True ($notice.Contains('before confirmation') -and $notice.Contains('does not identify which action moved the pointer')) 'the notice states the capture boundary and attribution limit'
    $warnings = @()
    $shown = Get-FlatText (Get-RunnerLeasePresentation $result ([ref]$warnings))
    Assert-True ($shown.Contains($notice)) 'the canonical runner prints the shared neutral notice'
    Assert-Equal 0 $warnings.Count 'pointer position observations are information, not a fixture warning'
    Assert-True (-not $shown.Contains('The suites left the pointer') -and -not $shown.Contains('fixture to look at')) 'the runner no longer blames a fixture from aggregate lease evidence'
    foreach ($position in 'CursorSaved', 'CursorAtExit') {
        foreach ($value in @($null, 'none', 'unknown')) {
            $incomplete = Read-DxUiLeaseResult -Text $completed
            if ($null -eq $value) { $incomplete.PSObject.Properties.Remove($position) } else { $incomplete.$position = $value }
            $notice = (Get-DxUiLeaseSummary -Result $incomplete).PointerNotice
            Assert-True ($notice.StartsWith('The lease restored the pointer; one or both positions were not recorded.')) "an absent or unreadable $position does not invent coordinates"
        }
    }
}
Invoke-TestCase 'a missing result stays a failure even when the lease exit code is zero' {
    Assert-Equal 'the lease returned success' (Get-DxUiLeaseExitMeaning -ExitCode 0) 'exit zero describes the exit, not unrecorded suite or restoration evidence'
    $problems = @(Get-DxUiLeaseProblems -Result $null -ExitCode 0)
    Assert-True ($problems.Count -eq 1 -and $problems[0].Contains('wrote no result') -and $problems[0].Contains('exit code 0')) 'missing evidence still produces a failure'
    Assert-True (-not $problems[0].Contains('every suite passed') -and -not $problems[0].Contains('desktop is as it was')) 'the failure message does not simultaneously assert success'
}
Invoke-TestCase 'the exit codes of the lease are the ones the PowerShell side reads' {
    $header = [IO.File]::ReadAllText((Join-Path $repository 'Tests/Support/Support.Tests.InteractiveLease.h'))
    $namespace = [regex]::Match($header, '(?s)namespace LeaseExit\s*\{(.*?)\} // namespace LeaseExit')
    Assert-True $namespace.Success 'the exit codes are in one namespace'
    $cpp = [ordered]@{}
    foreach ($match in [regex]::Matches($namespace.Groups[1].Value, 'inline constexpr int k(\w+)\s*=\s*(\d+);')) { $cpp[$match.Groups[1].Value] = [int]$match.Groups[2].Value }
    $powershell = Get-DxUiLeaseExitCodes
    Assert-Equal (($cpp.Keys | Sort-Object) -join ',') (($powershell.Keys | Sort-Object) -join ',') 'the same names'
    foreach ($name in $cpp.Keys) {
        Assert-Equal $cpp[$name] $powershell[$name] "$name"
        $meaning = Get-DxUiLeaseExitMeaning -ExitCode $cpp[$name]
        Assert-True (-not $meaning.Contains('unexpected exit code')) "exit code $($cpp[$name]) has a meaning: $meaning"
    }
    Assert-True ((Get-DxUiLeaseExitMeaning -ExitCode 77).Contains('unexpected exit code 77')) 'an exit code it does not know is said to be unexpected'
}
Invoke-TestCase 'desktop takeover requires local -Interactive or the verified hosted foreground wrapper' {
    $calls = Get-CommandCalls @('Invoke-DxUiInteractiveLease', 'Test-DxUiDesktopAvailable', 'Resolve-DxUiInteractiveSuites')
    Assert-True ($calls.Count -ge 3) 'test.ps1 uses each of them'
    foreach ($call in $calls) { Assert-True (Test-GuardedByInteractive $call -AllowHostedProbe) "$($call.GetCommandName()) has the required desktop authorization guard at line $($call.Extent.StartLineNumber)" }
    $summaryCalls = Get-CommandCalls @('Get-DxUiLeaseSummary')
    Assert-Equal 1 $summaryCalls.Count 'local and hosted runs use one shared final presentation'
    Assert-True (Test-GuardedByInteractive $summaryCalls[0]) 'ordinary runs do not print foreground lease summaries'
    $hostedCall = Get-CommandCalls @('Invoke-DxUiHostedForegroundLease') | Select-Object -First 1
    Assert-True ($null -ne $hostedCall) 'test.ps1 has a hosted lease call'
    $hostedGuard = $false
    for ($parent = $hostedCall.Parent; $null -ne $parent; $parent = $parent.Parent) {
        if ($parent -is [Management.Automation.Language.IfStatementAst] -and $parent.Clauses.Item1.Extent.Text -match '\$hostedForegroundLease\b' -and
            $parent.Clauses.Item2.Extent.StartOffset -le $hostedCall.Extent.StartOffset -and $hostedCall.Extent.EndOffset -le $parent.Clauses.Item2.Extent.EndOffset) { $hostedGuard = $true }
    }
    Assert-True $hostedGuard 'the hosted wrapper is called only in the hosted-selected branch'
    $module = [IO.File]::ReadAllText((Join-Path $repository 'Tools/InteractiveRun.psm1'))
    Assert-True ($module -match '(?s)function Invoke-DxUiHostedForegroundLease\s*\{.*?Test-DxUiVerifiedGitHubHostedRunner') 'the PowerShell wrapper rechecks the verified hosted markers before writing the plan or launching the executable'
    Assert-True $module.Contains("'--run-hosted'") 'the wrapper selects the native hosted-only mode'
    $native = [IO.File]::ReadAllText((Join-Path $repository 'Tests/InteractiveLease/InteractiveLease.Tests.Runner.cpp'))
    foreach ($marker in @('L"CI", L"true"', 'L"GITHUB_ACTIONS", L"true"', 'L"RUNNER_ENVIRONMENT", L"github-hosted"', 'L"RUNNER_OS", L"Windows"')) {
        Assert-True $native.Contains($marker) "native authorization requires exact $marker"
    }
    $nativeGate = $native.IndexOf('mode == Mode::RunHosted && ! IsVerifiedGitHubHostedRunner()')
    Assert-True ($nativeGate -gt 0 -and $nativeGate -lt $native.IndexOf('SetProcessDpiAwarenessContext') -and $nativeGate -lt $native.LastIndexOf('case Mode::RunHosted:')) 'the native gate runs before process or desktop mutation and dispatch'
    Assert-True ($native -match '(?s)if \(_hosted\).*?Confirmation::Started') 'verified hosted mode records authorization and skips only the person-facing confirmation'
    $failureHandler = $native.IndexOf('DxUiTestFailureReports::RouteAwayFromDialogs();', $nativeGate)
    $childRoute = $native.IndexOf('return SelfTestChild(*selfTestChildMode);')
    Assert-True ($failureHandler -gt $nativeGate -and $childRoute -gt $failureHandler) 'the hosted gate still precedes UI setup, and the intentional runtime-failure child still runs only after the no-dialog handler is installed'
    $text = [IO.File]::ReadAllText($testScript)
    Assert-True $text.Contains('DxUi.InteractiveLease.exe') 'test.ps1 names the lease executable'
    foreach ($other in Get-ChildItem -LiteralPath $repository -Filter '*.ps1' | Where-Object { $_.Name -ne 'test.ps1' }) {
        Assert-True (-not [IO.File]::ReadAllText($other.FullName).Contains('Invoke-DxUiInteractiveLease')) "$($other.Name) never runs the lease"
    }
}
Invoke-TestCase 'an interactive run is settled before anything is built or run' {
    $text = [IO.File]::ReadAllText($testScript)
    $refusal = $text.IndexOf('Get-DxUiInteractiveRefusal')
    $selection = $text.IndexOf('Resolve-DxUiInteractiveSuites')
    Assert-True ($selection -gt 0 -and $refusal -gt $selection) 'the suites are checked first, then the environment'
    foreach ($later in @('Invoke-ToolingTests.ps1', "'build.ps1'", 'performance.ps1', 'Test-TestWatchdog.ps1')) {
        Assert-True ($text.IndexOf($later) -gt $refusal) "$later runs after the refusal"
    }
    $tooling = Get-Content (Join-Path $repository 'Tools/tests/Invoke-ToolingTests.ps1') -Raw
    Assert-True ($tooling.Contains('Test-ConsumerUpdate.ps1')) 'consumer-update checks remain in the portable tooling aggregation'
    $nativeCheck = $text.IndexOf('Test-DxUiDesktopAvailable')
    Assert-True ($nativeCheck -gt $text.IndexOf("'build.ps1'") -and $nativeCheck -lt $text.IndexOf('performance.ps1')) 'the native desktop check follows the build and comes before the benchmark and the suites'
    Assert-True ($text.IndexOf('Invoke-DxUiInteractiveLease') -gt $text.IndexOf('performance.ps1')) 'the lease starts after everything else the run checks, so the desktop is held only for the suites'
    Assert-True ($text.Contains('Resolve-DxUiForegroundLeaseSuites') -and $text.Contains('foreach ($name in $leaseSuites)')) 'the plan contains only suites that need the foreground lease, leaving other requested suites on the normal runner path'
}
Invoke-TestCase 'a suite that needs real focus is never given --no-activate, and every other control suite always is' {
    $function = $testAst.Find({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Get-SuiteRun' }, $true)
    Assert-True ($null -ne $function) 'test.ps1 decides what a suite is run with in one function'
    $default = ($testAst.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq 'Suites' }).DefaultValue.Extent.Text
    $suites = Get-NamesIn $default "'(\w+)'"
    Assert-True ($suites.Count -gt 15 -and $suites -contains 'InteractiveLease') 'the default list is read from the parameter'
    # Fake roots under the temp directory, which exists on every system the tooling tests run on (CI's validation job is Ubuntu,
    # where a C: path names no drive). Nothing is created there: only the names are compared.
    $fakeRoot = Join-Path ([IO.Path]::GetTempPath()) 'dxui-interactive-fake'
    $runOf = {
        param([string] $Suite, [bool] $Interactive, [string[]] $Tests, $TestTimeout)
        $Platform = 'x64'; $Configuration = 'Debug'; $logs = Join-Path $fakeRoot 'logs'; $instrumentation = @(); $hostedForegroundLease = $false
        . ([scriptblock]::Create($function.Extent.Text.Replace('$PSScriptRoot', "'$fakeRoot'"))) # A dynamic scriptblock has no $PSScriptRoot.
        Get-SuiteRun $Suite
    }
    foreach ($suite in @($suites + @('MenuResources', 'MenuResourceScaling'))) {
        foreach ($interactive in @($false, $true)) {
            $run = & $runOf $suite $interactive @() $null
            $needsDesktop = $suite -in @('Menu', 'NativeTextInput', 'MenuResources', 'MenuResourceScaling')
            if ($suite -in @('Foundation', 'Embedded')) { Assert-Equal 0 $run.Arguments.Count "$suite takes no arguments" }
            else {
                Assert-True ($run.Arguments -contains "--suite=$suite") "$suite is selected"
                Assert-Equal (-not $needsDesktop) ($run.Arguments -contains '--no-activate') "$suite gets --no-activate exactly when it does not need the desktop"
            }
        }
    }
    $plain = & $runOf 'Menu' $false @() $null
    $leased = & $runOf 'Menu' $true @() $null
    Assert-True ($plain.Log.EndsWith('test-Menu-x64-Debug.log') -and $leased.Log.EndsWith('test-Menu-x64-Debug.interactive.log')) 'an interactive run keeps its own log beside the run that records the suite''s skips'
    $filtered = & $runOf 'Menu' $true @('TestA', 'TestB') 7
    Assert-True ($filtered.Arguments -contains '--test=TestA,TestB' -and $filtered.Arguments -contains '--test-timeout=7' -and $filtered.Log.EndsWith('.interactive.filtered.log') -and $filtered.Filtered) 'its filter and deadline reach the runner, and its log says both'
    Assert-True ((& $runOf 'Foundation' $true @('TestA') $null).Arguments.Count -eq 0) 'Foundation ignores a filter'
}
Invoke-FixtureCase 'test.ps1 -Interactive names a suite that does not need the desktop and stops before anything is built' { param($fixture)
    New-RefusalFixture $fixture
    $before = Get-OutputFiles $fixture
    $run = Invoke-TestScript @('-Interactive', '-Suites', 'Grid') @{ CI = 'true' } $fixture
    Assert-True $run.Exited 'the refusal ended the run'
    Assert-True ($run.Exit -ne 0) 'with a failing exit code'
    Assert-True ((Get-FlatText $run.Output).Contains('not interactive: Grid')) "naming the suite: $($run.Output)"
    Assert-True (-not $run.Output.Contains('Interactive run:') -and -not $run.Output.Contains('Running ')) 'and nothing ran'
    Assert-Equal ((ConvertTo-OrdinalOrder $before) -join "`n") ((ConvertTo-OrdinalOrder (Get-OutputFiles $fixture)) -join "`n") 'no log or receipt was written'
}
Invoke-FixtureCase 'test.ps1 -Interactive refuses in a CI job before it builds or runs anything' { param($fixture)
    # Whatever happens here must not reach the rest of test.ps1: it would run these tooling tests again and the lease's desktop with
    # them. So the refusal is proved in this process first, and the child's CI variable is set on its process, never inherited.
    Assert-True ($null -ne (Get-DxUiInteractiveRefusal -Environment @{ CI = 'true' } -UserInteractive $true -OnWindows $true)) 'a CI environment is refused'
    New-RefusalFixture $fixture
    $before = Get-OutputFiles $fixture
    foreach ($environment in @(
        @{ CI = 'true' },
        @{ CI='true'; GITHUB_ACTIONS='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows' }
    )) {
        $run = Invoke-TestScript @('-Interactive') $environment $fixture
        Assert-True $run.Exited 'the refusal ended the run'
        Assert-True ($run.Exit -ne 0) 'with a failing exit code'
        # Even a verified hosted runner uses its distinct hosted mode; -Interactive still requires a person.
        $reason = if ($IsWindows) { 'CI is set' } else { 'this is not Windows' }
        Assert-True ((Get-FlatText $run.Output).Contains('Interactive tests need an interactive desktop, and there is none') -and (Get-FlatText $run.Output).Contains($reason)) "saying why: $($run.Output)"
        Assert-True (-not $run.Output.Contains('Interactive run:') -and -not $run.Output.Contains('Running ') -and -not $run.Output.Contains('== ')) 'and nothing ran: no tooling test, no build, no suite'
        Assert-Equal ((ConvertTo-OrdinalOrder $before) -join "`n") ((ConvertTo-OrdinalOrder (Get-OutputFiles $fixture)) -join "`n") 'no log or receipt was written'
    }
}
Invoke-FixtureCase 'approved automatic starts retain their mode, bounded countdown and distinct result' { param($fixture)
    $fake = Join-Path $fixture 'auto-start-lease.ps1'
    @'
param([Parameter(ValueFromRemainingArguments)][string[]] $Arguments)
$resultPath = ($Arguments | Where-Object { $_ -like '--result=*' }) -replace '^--result=', ''
[IO.File]::WriteAllText(($resultPath + '.arguments.txt'), ($Arguments -join "`n"))
$confirmation = if ($Arguments -contains '--auto-start') { 'autostarted' } else { 'started' }
$autoStart = [int]($Arguments -contains '--auto-start')
$seconds = ($Arguments | Where-Object { $_ -like '--confirm-timeout=*' }) -replace '^--confirm-timeout=', ''
$label = ($Arguments | Where-Object { $_ -like '--label=*' }) -replace '^--label=', ''
$policy = "confirmation.autoStart: $autoStart`nconfirmation.seconds: $seconds`n"
$action = "confirmation: $confirmation`n"
switch ($label) {
    'missing' { $policy = '' }
    'wrong-mode' { $policy = "confirmation.autoStart: 0`nconfirmation.seconds: $seconds`n" }
    'wrong-countdown' { $policy = "confirmation.autoStart: $autoStart`nconfirmation.seconds: 999`n" }
    'invalid-countdown' { $policy = "confirmation.autoStart: $autoStart`nconfirmation.seconds: invalid`n" }
    'missing-action' { $action = '' }
    'declined-action' { $action = "confirmation: declined`n" }
    'unknown-action' { $action = "confirmation: unexpected`n" }
    'duplicate-mode' { $policy = "confirmation.autoStart: 0`n" + $policy }
    'duplicate-countdown' { $policy = "confirmation.seconds: invalid`n" + $policy }
    'duplicate-action' { $action = "confirmation: declined`n" + $action }
    'unrequested' { $policy = "confirmation.autoStart: 1`nconfirmation.seconds: 30`n"; $confirmation = 'autostarted' }
}
if ($label -eq 'unrequested') { $action = "confirmation: $confirmation`n" }
"state: completed`nexit: 0`n${action}${policy}foreground.saved: invented fixture window`nfocus.capture.reason: recorded`nfocus.capture.error: 0`nrestoration.foreground: unchanged`nrestoration.focus: unchanged`nrestoration.cursor: unchanged`ncursor.saved: 1,2`ncursor.atExit: 1,2`nrestoration.runMovedSomething: 0`nchild: Menu launched=1 exit=0 seconds=1 timedout=0 interrupted=0`n" | Set-Content -LiteralPath $resultPath
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $fake
    $parameters = @{ Executable=$fake; Runs=@([pscustomobject]@{ Name='Menu'; CommandLine='unused.exe'; Log='unused.log' }); Label='Menu'; EstimateSeconds=1; WorkDirectory=$fixture }
    $ordinary = Invoke-DxUiInteractiveLease @parameters
    $argumentsPath = $ordinary.ResultPath + '.arguments.txt'
    $arguments = [IO.File]::ReadAllLines($argumentsPath)
    Assert-True ($arguments -contains '--run' -and $arguments -notcontains '--auto-start' -and $arguments -contains '--confirm-timeout=120') 'ordinary calls retain explicit confirmation and its two-minute cancellation'
    foreach ($case in @(@{ Bound=@{}; Seconds=30 }, @{ Bound=@{ ConfirmSeconds=7 }; Seconds=7 })) {
        $bound = $case.Bound
        $answer = Invoke-DxUiInteractiveLease @parameters @bound -AutoStart
        $arguments = [IO.File]::ReadAllLines($argumentsPath)
        Assert-True ($arguments -contains '--run' -and $arguments -contains '--auto-start' -and $arguments -contains "--confirm-timeout=$($case.Seconds)") "approved local automatic mode sends its $($case.Seconds)-second countdown: $($arguments -join ' | ')"
        Assert-Equal 'autostarted' $answer.Result.Confirmation 'an automatic start is recorded separately from a clicked Start'
        Assert-True ($answer.Result.AutoStart -and $answer.Result.ConfirmationSeconds -eq $case.Seconds) 'actual native policy and countdown survive parsing'
        Assert-Equal 0 @($answer.PolicyProblems).Count 'the native policy matches the requested automatic run'
        Assert-Equal 0 @(Get-DxUiLeaseProblems -Result $answer.Result -ExitCode $answer.ExitCode).Count 'a complete automatic result has no lease problem'
    }
    $before = [IO.File]::ReadAllText($argumentsPath)
    $thrown = $false
    try { [void](Invoke-DxUiInteractiveLease @parameters -AutoStart -ConfirmSeconds 0) } catch { $thrown = $_.Exception.Message.Contains('positive countdown') }
    Assert-True $thrown 'automatic mode refuses a zero countdown before native launch'
    Assert-Equal $before ([IO.File]::ReadAllText($argumentsPath)) 'the refused call did not launch or replace the prior arguments'
    $acceptedInvalid = [Collections.Generic.List[string]]::new()
    foreach ($label in @('missing','wrong-mode','wrong-countdown','invalid-countdown','missing-action','declined-action','unknown-action','duplicate-mode','duplicate-countdown','duplicate-action')) {
        $parameters.Label = $label
        $answer = Invoke-DxUiInteractiveLease @parameters -AutoStart
        if (@($answer.PolicyProblems).Count -eq 0) { $acceptedInvalid.Add($label) }
    }
    Assert-Equal 0 $acceptedInvalid.Count "invalid automatic results are rejected; accepted: $($acceptedInvalid -join ', ')"
    $parameters.Label = 'unrequested'
    $answer = Invoke-DxUiInteractiveLease @parameters
    Assert-True (@($answer.PolicyProblems).Count -gt 0) 'ordinary calls reject an unrequested automatic policy'
    $parameters.Label = 'missing'
    $answer = Invoke-DxUiInteractiveLease @parameters
    Assert-Equal 'completed' $answer.Result.State 'ordinary historical results remain readable'
    Assert-True (@($answer.PolicyProblems) -match 'missing confirmation.autoStart') 'historical metadata cannot qualify a new execution'
}
Invoke-FixtureCase 'public lease wrappers validate restoration and exact children without consumer-local checks' { param($fixture)
    $fake = Join-Path $fixture 'receipt-lease.ps1'
    $receiptPath = Join-Path $fixture 'receipt-source.txt'
    @'
param([Parameter(ValueFromRemainingArguments)][string[]] $Arguments)
$resultPath = ($Arguments | Where-Object { $_ -like '--result=*' }) -replace '^--result=', ''
[IO.File]::Copy((Join-Path $PSScriptRoot 'receipt-source.txt'), $resultPath, $true)
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $fake
    $parameters = @{ Executable=$fake; Runs=@([pscustomobject]@{ Name='Menu'; CommandLine='unused.exe'; Log='unused.log' },
        [pscustomobject]@{ Name='NativeTextInput'; CommandLine='unused.exe'; Log='unused2.log' }); Label='fixture'; EstimateSeconds=1; WorkDirectory=$fixture }
    [IO.File]::WriteAllText($receiptPath, $successful)
    Assert-Equal 0 @((Invoke-DxUiInteractiveLease @parameters).PolicyProblems).Count 'the complete requested plan succeeds through the shared wrapper'
    foreach ($text in @(
        "state: completed`nexit: 0`nconfirmation: started`nconfirmation.autoStart: 0`nconfirmation.seconds: 120`n",
        $successful.Replace('child: NativeTextInput ', 'child: Other '),
        $successful.Replace('timedout=0', 'timedout=1'),
        $successful.Replace('restoration.foreground: restored', 'restoration.foreground: gone'),
        $successful.Replace('restoration.focus: unchanged', 'restoration.focus: none').
            Replace('focus.capture.reason: recorded','focus.capture.reason: query-failed').Replace('focus.capture.error: 0','focus.capture.error: 5'))) {
        [IO.File]::WriteAllText($receiptPath, $text)
        Assert-True (@((Invoke-DxUiInteractiveLease @parameters).PolicyProblems).Count -gt 0) 'zero native exit cannot hide incomplete restoration or a different child plan'
    }
}
Invoke-FixtureCase 'automatic start without an interactive request stops before tooling, build or output writes' { param($fixture)
    New-RefusalFixture $fixture
    $before = Get-OutputFiles $fixture
    $run = Invoke-TestScript @('-InteractiveAutoStart', '-Suites', 'Grid') @{ CI='true' } $fixture
    Assert-True ($run.Exited -and $run.Exit -ne 0) 'the invalid combination ends with a failure'
    Assert-True ((Get-FlatText $run.Output).Contains('-InteractiveAutoStart requires -Interactive')) 'it names the required explicit interactive request'
    Assert-Equal ((ConvertTo-OrdinalOrder $before) -join "`n") ((ConvertTo-OrdinalOrder (Get-OutputFiles $fixture)) -join "`n") 'nothing was built, logged or reported'
}
Invoke-FixtureCase 'an approved automatic request still refuses a CI desktop before building or running' { param($fixture)
    New-RefusalFixture $fixture
    $before = Get-OutputFiles $fixture
    $run = Invoke-TestScript @('-Interactive', '-InteractiveAutoStart') @{ CI='true' } $fixture
    Assert-True ($run.Exited -and $run.Exit -ne 0) 'the unavailable desktop is refused'
    Assert-True ((Get-FlatText $run.Output).Contains('Interactive tests need an interactive desktop, and there is none')) 'automatic mode keeps the existing desktop refusal'
    Assert-Equal ((ConvertTo-OrdinalOrder $before) -join "`n") ((ConvertTo-OrdinalOrder (Get-OutputFiles $fixture)) -join "`n") 'no test or build output was written'
}
Invoke-FixtureCase 'a native lease inherits the orchestrator filesystem location while staying attached to the console' {
    param($fixture)
    # CR-31 of the 10 October review: the lease was piped through Out-Host, and PowerShell kills a native command whose output
    # it reads when Ctrl+C stops the pipeline, before the lease restores the desktop.
    $pwsh = (Get-Process -Id $PID).Path
    $deepLocation = Join-Path $fixture 'unrelated/deep/lease/orchestrator/location'
    [void](New-Item -ItemType Directory -Path $deepLocation -Force)
    $currentDirectory = [Environment]::CurrentDirectory
    Push-Location -LiteralPath $deepLocation
    try {
        $expectedWorkingDirectory = (Get-Location).ProviderPath
        $ambientBeforeLaunch = [Environment]::CurrentDirectory
        $childDirectoryPath = Join-Path $fixture 'child-current-directory.txt'
        $quotedChildDirectoryPath = $childDirectoryPath.Replace("'", "''")
        $childCommand = "[Console]::Out.WriteLine('[LEASE] native progress'); [IO.File]::WriteAllText('$quotedChildDirectoryPath', [Environment]::CurrentDirectory); exit 7"
        $answer = @(Invoke-DxUiLeaseExecutable -Executable $pwsh -Arguments @('-NoProfile', '-NonInteractive', '-Command', $childCommand))
        $childWorkingDirectory = [IO.File]::ReadAllText($childDirectoryPath)
        $ambientAfterLaunch = [Environment]::CurrentDirectory
    } finally { Pop-Location }
    $ambientAfterFixture = [Environment]::CurrentDirectory
    Assert-Equal 1 $answer.Count 'only its exit code is returned: the lease writes its progress to the console itself'
    Assert-Equal 7 $answer[0] 'the exit code is the lease''s'
    Assert-Equal $expectedWorkingDirectory $childWorkingDirectory 'the native lease uses the orchestrator''s current filesystem location'
    Assert-Equal $currentDirectory $ambientBeforeLaunch 'changing the PowerShell location does not change the parent process directory'
    Assert-Equal $currentDirectory $ambientAfterLaunch 'starting the native lease does not change the parent process directory'
    Assert-Equal $currentDirectory $ambientAfterFixture 'the test restores the original PowerShell location without changing the parent process directory'
    $module = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../InteractiveRun.psm1'))
    Assert-True $module.Contains('Invoke-DxUiLeaseExecutable -Executable $Executable -Arguments $arguments') 'the lease core starts a native lease that way'
}
Invoke-FixtureCase 'a native lease refuses a non-filesystem location without launching or changing the process directory' {
    param($fixture)
    $pwsh = (Get-Process -Id $PID).Path
    $launchMarker = Join-Path $fixture 'child-launched.txt'
    $quotedLaunchMarker = $launchMarker.Replace("'", "''")
    $childCommand = "[IO.File]::WriteAllText('$quotedLaunchMarker', 'launched')"
    $ambientBefore = [Environment]::CurrentDirectory
    Push-Location -LiteralPath 'Variable:\'
    try {
        $location = Get-Location
        $ambientDuring = [Environment]::CurrentDirectory
        $refused = $false
        $refusal = ''
        try {
            [void](Invoke-DxUiLeaseExecutable -Executable $pwsh -Arguments @('-NoProfile', '-NonInteractive', '-Command', $childCommand))
        } catch {
            $refused = $true
            $refusal = $_.Exception.Message
        }
        $ambientAfterAttempt = [Environment]::CurrentDirectory
    } finally { Pop-Location }
    $ambientAfterFixture = [Environment]::CurrentDirectory
    Assert-Equal 'Variable' $location.Provider.Name 'the fixture exercises a non-filesystem PowerShell provider'
    Assert-True $refused 'the lease launcher rejects a non-filesystem location'
    Assert-True $refusal.Contains('current filesystem location') 'the refusal explains the required location'
    Assert-True (-not (Test-Path -LiteralPath $launchMarker)) 'a refused lease never launches its child'
    Assert-Equal $ambientBefore $ambientDuring 'the non-filesystem location does not change the parent process directory'
    Assert-Equal $ambientBefore $ambientAfterAttempt 'refusal leaves the parent process directory unchanged'
    Assert-Equal $ambientBefore $ambientAfterFixture 'restoring the PowerShell location leaves the process directory unchanged'
}
Invoke-FixtureCase 'lease console output cannot replace the result object or hide a failed child' { param($fixture)
    $fake = Join-Path $fixture 'lease.ps1'
    @'
param([Parameter(ValueFromRemainingArguments)][string[]] $Arguments)
$resultPath = ($Arguments | Where-Object { $_ -like '--result=*' }) -replace '^--result=', ''
$exitCode = if (($Arguments | Where-Object { $_ -like '--label=*' }) -eq '--label=failed') { 1 } else { 0 }
"state: completed`nexit: $exitCode`nconfirmation: started`nchild: Menu launched=1 exit=$exitCode seconds=1 timedout=0 interrupted=0`n" | Set-Content -LiteralPath $resultPath
'[LEASE] suite output'
'Restoration: foreground=restored focus=restored cursor=restored'
$global:LASTEXITCODE = $exitCode
'@
    | Set-Content -LiteralPath $fake
    foreach ($case in @(@{ Label = 'passed'; Exit = 0 }, @{ Label = 'failed'; Exit = 1 })) {
        $answer = @(Invoke-DxUiInteractiveLease -Executable $fake -Runs @([pscustomobject]@{ Name = 'Menu'; CommandLine = 'unused.exe'; Log = 'unused.log' }) -Label $case.Label -EstimateSeconds 1 -WorkDirectory $fixture)
        Assert-Equal 1 $answer.Count 'only the structured result is returned'
        Assert-Equal $case.Exit $answer[0].ExitCode 'the executable exit code is preserved'
        Assert-Equal $case.Exit $answer[0].Result.Children[0].ExitCode 'the child result is preserved'
    }
}
Invoke-FixtureCase 'the hosted wrapper rechecks authorization and the shared lease core removes stale results' { param($fixture)
    $fake = Join-Path $fixture 'hosted-lease.ps1'
    $marker = Join-Path $fixture 'launched.txt'
    @'
param([Parameter(ValueFromRemainingArguments)][string[]] $Arguments)
[IO.File]::WriteAllText($env:DXUI_HOSTED_LEASE_MARKER, ($Arguments -join "`n"))
$resultPath = ($Arguments | Where-Object { $_ -like '--result=*' }) -replace '^--result=', ''
"state: completed`nexit: 0`nconfirmation: started`nrestoration.foreground: unchanged`nrestoration.focus: unchanged`nrestoration.cursor: restored`ncursor.saved: 400,300`ncursor.atExit: 900,700`nrestoration.runMovedSomething: 0`nchild: Menu launched=1 exit=0 seconds=1 timedout=0 interrupted=0`n" | Set-Content -LiteralPath $resultPath
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $fake
    $names = @('CI', 'GITHUB_ACTIONS', 'RUNNER_ENVIRONMENT', 'RUNNER_OS')
    $original = @{}
    foreach ($name in $names) { $original[$name] = [Environment]::GetEnvironmentVariable($name) }
    $oldMarker = [Environment]::GetEnvironmentVariable('DXUI_HOSTED_LEASE_MARKER')
    try {
        foreach ($name in $names) { Restore-TestEnvironmentVariable $name $null }
        Restore-TestEnvironmentVariable 'DXUI_HOSTED_LEASE_MARKER' $marker
        try {
            $thrown = $false
            try { [void](Invoke-DxUiHostedForegroundLease -Executable $fake -Runs @([pscustomobject]@{ Name='Menu'; Log='m.log'; CommandLine='unused.exe' }) -Label 'Menu' -EstimateSeconds 1 -WorkDirectory $fixture) } catch { $thrown = $_.Exception.Message.Contains('verified GitHub-hosted') }
            Assert-True $thrown 'an unverified direct call is refused by the PowerShell wrapper'
            Assert-True (-not (Test-Path -LiteralPath $marker) -and -not (Test-Path -LiteralPath (Join-Path $fixture 'interactive-plan.txt'))) 'refusal happens before plan creation or native launch'

            $local = Invoke-DxUiInteractiveLease -Executable $fake -Runs @([pscustomobject]@{ Name='Menu'; Log='m.log'; CommandLine='unused.exe' }) -Label 'Menu' -EstimateSeconds 1 -WorkDirectory $fixture
            Assert-True ([IO.File]::ReadAllText($marker).StartsWith("--run`n")) 'the local wrapper retains its distinct native mode'
            $localPresentation = Get-RunnerLeasePresentation $local.Result
            foreach ($pair in @{ CI='true'; GITHUB_ACTIONS='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows' }.GetEnumerator()) { Restore-TestEnvironmentVariable $pair.Key $pair.Value }
            Set-Content -LiteralPath (Join-Path $fixture 'interactive-result.txt') 'stale result'
            $answer = Invoke-DxUiHostedForegroundLease -Executable $fake -Runs @([pscustomobject]@{ Name='Menu'; Log='m.log'; CommandLine='unused.exe' }) -Label 'Menu' -EstimateSeconds 1 -WorkDirectory $fixture
            Assert-True ([IO.File]::ReadAllText($marker).StartsWith('--run-hosted')) 'the verified wrapper selects the hosted native mode'
            Assert-Equal 'completed' $answer.Result.State 'the hosted wrapper returns the shared structured result'
            Assert-Equal 1 @($answer.Result.Children).Count 'the hosted child result survives console streaming'
            Assert-Equal $localPresentation (Get-RunnerLeasePresentation $answer.Result) 'local and hosted restoration facts have the same neutral presentation'
            Assert-True ($localPresentation.Contains('The desktop states were restored or unchanged.')) 'the recorded restored pointer overrides the false moved flag in both modes'
        } finally {
            foreach ($name in $names) { Restore-TestEnvironmentVariable $name $null }
            Restore-TestEnvironmentVariable 'DXUI_HOSTED_LEASE_MARKER' $oldMarker
        }
        Assert-True (-not (Test-Path -LiteralPath 'Env:GITHUB_ACTIONS')) 'the hosted-lease fixture restores the forced absent marker as absent'
    } finally {
        foreach ($name in $names) { Restore-TestEnvironmentVariable $name $original[$name] }
        Restore-TestEnvironmentVariable 'DXUI_HOSTED_LEASE_MARKER' $oldMarker
    }

    $staleProbe = Join-Path $fixture 'stale-probe.ps1'
    @'
param([Parameter(ValueFromRemainingArguments)][string[]] $Arguments)
$resultPath = ($Arguments | Where-Object { $_ -like '--result=*' }) -replace '^--result=', ''
$global:leaseSawStale = Test-Path -LiteralPath $resultPath
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $staleProbe
    Set-Content -LiteralPath (Join-Path $fixture 'interactive-result.txt') 'stale result'
    $global:leaseSawStale = $true
    $answer = Invoke-DxUiInteractiveLease -Executable $staleProbe -Runs @([pscustomobject]@{ Name='Menu'; Log='m.log'; CommandLine='unused.exe' }) -Label 'Menu' -EstimateSeconds 1 -WorkDirectory $fixture
    Assert-True (-not $global:leaseSawStale) 'the shared lease core removes a stale result before launch'
    Assert-Equal $null $answer.Result 'a launch that writes no current result cannot reuse stale data'
}
Invoke-FixtureCase 'a missing lease child replaces an existing suite log with the current launch failure' { param($fixture)
    $log = Join-Path $fixture 'test-Menu-x64-Debug.hosted.log'
    Set-Content -LiteralPath $log 'previous successful suite output' -Encoding utf8
    Write-DxUiLeaseFailureLog -Path $log -Suite 'Menu'
    $current = Get-Content -Raw -LiteralPath $log
    Assert-True ($current.Contains('[LEASE] Menu did not run under the foreground lease')) 'the receipt source log names this launch failure'
    Assert-True (-not $current.Contains('previous successful suite output')) 'a previous suite log cannot hide this failure'
    $failureCall = Get-CommandCalls @('Write-DxUiLeaseFailureLog') | Select-Object -First 1
    $childFailureBranch = $false
    $logExistenceGuard = $false
    for ($parent = $failureCall.Parent; $null -ne $parent; $parent = $parent.Parent) {
        if ($parent -isnot [Management.Automation.Language.IfStatementAst]) { continue }
        foreach ($clause in $parent.Clauses) {
            $containsCall = $clause.Item2.Extent.StartOffset -le $failureCall.Extent.StartOffset -and $failureCall.Extent.EndOffset -le $clause.Item2.Extent.EndOffset
            if ($containsCall -and $clause.Item1.Extent.Text -match '\$child\.Launched') { $childFailureBranch = $true }
            if ($containsCall -and $clause.Item1.Extent.Text -match 'Test-Path.*\$log') { $logExistenceGuard = $true }
        }
    }
    Assert-True $childFailureBranch 'test.ps1 replaces the log in the child launch-failure branch'
    Assert-True (-not $logExistenceGuard) 'log replacement is unconditional and cannot preserve a previous suite log'
}
Invoke-FixtureCase 'a refused lease preserves raw prior output without qualifying it' { param($fixture)
    $log = Join-Path $fixture 'test-[Menu] é space.hosted.log'
    # Include invalid UTF-8, a NUL, and apparent success/skip output: retention must copy bytes,
    # while the canonical receipt source must contain only this launch failure.
    $original = [Text.Encoding]::UTF8.GetBytes("PASS previous child`nSKIPPED old capability`n") + [byte[]]@(0, 0xff, 0xc3, 0x28)
    [IO.File]::WriteAllBytes($log, $original)
    Write-DxUiLeaseFailureLog -Path $log -Suite 'Menu'
    $retained = @(Get-ChildItem -LiteralPath $fixture -File | Where-Object { $_.FullName -ne $log })
    Assert-Equal 1 $retained.Count 'one prior output file is retained'
    Assert-Equal '.log' $retained[0].Extension 'the normal CI log glob includes retained output'
    Assert-Equal ([Convert]::ToBase64String($original)) ([Convert]::ToBase64String([IO.File]::ReadAllBytes($retained[0].FullName))) 'all original bytes survive without decoding'
    $current = Get-Content -Raw -LiteralPath $log
    Assert-True ($current.Contains('[LEASE] Menu did not run under the foreground lease')) 'the canonical log still fails this launch'
    Assert-True (-not $current.Contains('PASS previous child') -and -not $current.Contains('SKIPPED old capability')) 'unqualified output cannot satisfy or skip the current suite'
}
Invoke-FixtureCase 'a refused lease without prior output creates only the failure marker' { param($fixture)
    $log = Join-Path $fixture 'test-NativeTextInput.hosted.log'
    Write-DxUiLeaseFailureLog -Path $log -Suite 'NativeTextInput'
    Assert-Equal 1 @(Get-ChildItem -LiteralPath $fixture -File).Count 'missing output does not invent retained evidence'
    Assert-True ((Get-Content -Raw -LiteralPath $log).Contains('[LEASE] NativeTextInput did not run under the foreground lease')) 'missing output still records failure'
}
Invoke-FixtureCase 'repeated lease refusals never overwrite retained output' { param($fixture)
    $log = Join-Path $fixture 'test-Menu.hosted.log'
    [IO.File]::WriteAllBytes($log, [byte[]]@(0, 0xff, 1))
    Write-DxUiLeaseFailureLog -Path $log -Suite 'Menu'
    $first = @(Get-ChildItem -LiteralPath $fixture -File | Where-Object { $_.FullName -ne $log })
    Assert-Equal 1 $first.Count 'the first refusal retains one original'
    $firstHash = (Get-FileHash -LiteralPath $first[0].FullName).Hash
    [IO.File]::WriteAllBytes($log, [byte[]]@(2, 0xfe, 3))
    Write-DxUiLeaseFailureLog -Path $log -Suite 'Menu'
    $retained = @(Get-ChildItem -LiteralPath $fixture -File | Where-Object { $_.FullName -ne $log })
    Assert-Equal 2 $retained.Count 'a subsequent refusal creates a different retained file'
    Assert-Equal $firstHash (Get-FileHash -LiteralPath $first[0].FullName).Hash 'the first retained file is unchanged'
    $second = @($retained | Where-Object { $_.FullName -ne $first[0].FullName })
    Assert-Equal ([Convert]::ToBase64String([byte[]]@(2, 0xfe, 3))) ([Convert]::ToBase64String([IO.File]::ReadAllBytes($second[0].FullName))) 'the second original is also retained'
}
if ($IsWindows) {
    Invoke-FixtureCase 'failed retention leaves exclusively locked original bytes untouched' { param($fixture)
        $log = Join-Path $fixture 'test-Menu.hosted.log'
        $original = [byte[]]@(0, 0xff, 1, 2)
        [IO.File]::WriteAllBytes($log, $original)
        # Windows denies the copy while this writer owns the file. This exercises the
        # retention error path without a desktop or a simulated file-copy implementation.
        $writer = [IO.File]::Open($log, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
        try {
            Assert-Throws { Write-DxUiLeaseFailureLog -Path $log -Suite 'Menu' } 'retention failure must stop log replacement'
        } finally { $writer.Dispose() }
        Assert-Equal ([Convert]::ToBase64String($original)) ([Convert]::ToBase64String([IO.File]::ReadAllBytes($log))) 'failed retention does not destroy original evidence'
    }
}
Invoke-TestCase 'hosted ARM desktop preparation refuses incomplete runner authorization before Windows changes' {
    $preparation = Join-Path $repository 'Tools/Prepare-HostedArmDesktop.ps1'
    $valid = @{ CI='true'; GITHUB_ACTIONS='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows'; RUNNER_ARCH='ARM64' }
    $previousRunnerArch = [Environment]::GetEnvironmentVariable('RUNNER_ARCH')
    $original = @{}
    foreach ($name in $valid.Keys) { $original[$name] = [Environment]::GetEnvironmentVariable($name) }
    try {
        Restore-TestEnvironmentVariable 'RUNNER_ARCH' $null
        $original['RUNNER_ARCH'] = $null
        try {
            foreach ($name in $valid.Keys) { Restore-TestEnvironmentVariable $name $valid[$name] }
            foreach ($name in $valid.Keys) {
                Restore-TestEnvironmentVariable $name 'invalid'
                $refused = $false
                try { & $preparation } catch { $refused = $_.Exception.Message -like 'Hosted ARM64 desktop preparation requires*' }
                Assert-True $refused "an invalid $name refuses before registry or desktop access"
                Restore-TestEnvironmentVariable $name $valid[$name]
            }
        } finally {
            foreach ($name in $valid.Keys) { Restore-TestEnvironmentVariable $name $original[$name] }
        }
        Assert-True (-not (Test-Path -LiteralPath 'Env:RUNNER_ARCH')) 'the hosted-ARM fixture restores the forced absent architecture as absent'
    } finally { Restore-TestEnvironmentVariable 'RUNNER_ARCH' $previousRunnerArch }
    # Off ARM64 the system architecture alone refuses above, so the script must take its whole decision from the one guard that
    # the next case checks condition by condition, before it loads Win32 or writes the registry.
    $script = [Management.Automation.Language.Parser]::ParseFile($preparation, [ref]$null, [ref]$null)
    $guard = @($script.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'Test-DxUiHostedArmDesktopRunner' }, $true))
    $firstChange = @($script.FindAll({ param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -in @('Add-Type', 'New-Item', 'New-ItemProperty') }, $true) |
        ForEach-Object { $_.Extent.StartOffset } | Sort-Object)[0]
    Assert-True ($guard.Count -eq 1 -and $guard[0].CommandElements.Count -eq 1) 'the preparation asks the shared guard once, about this process and this machine'
    Assert-True ($guard[0].Extent.StartOffset -lt $firstChange) 'and before it loads Win32 or writes the registry'
    Assert-True (-not ((Get-Content -Raw -LiteralPath $preparation) -match 'RUNNER_ARCH|OSArchitecture')) 'no second runner condition lives in the script itself'
}
Invoke-TestCase 'hosted ARM desktop preparation requires every hosted marker, an ARM64 runner and a native ARM64 system' {
    $valid = @{ CI='true'; GITHUB_ACTIONS='true'; RUNNER_ENVIRONMENT='github-hosted'; RUNNER_OS='Windows'; RUNNER_ARCH='ARM64' }
    Assert-True (Test-DxUiHostedArmDesktopRunner -Environment $valid -OSArchitecture Arm64) 'a verified hosted ARM64 job on a native ARM64 system qualifies'
    foreach ($architecture in @('X64', 'X86', 'Arm')) {
        Assert-True (-not (Test-DxUiHostedArmDesktopRunner -Environment $valid -OSArchitecture $architecture)) "a $architecture system never qualifies"
    }
    foreach ($name in $valid.Keys) {
        $mutation = $valid.Clone()
        $mutation[$name] = 'invalid'
        Assert-True (-not (Test-DxUiHostedArmDesktopRunner -Environment $mutation -OSArchitecture Arm64)) "an invalid $name never qualifies"
        $mutation.Remove($name)
        Assert-True (-not (Test-DxUiHostedArmDesktopRunner -Environment $mutation -OSArchitecture Arm64)) "a missing $name never qualifies"
    }
    $lowercase = $valid.Clone()
    $lowercase['RUNNER_ARCH'] = 'arm64'
    Assert-True (-not (Test-DxUiHostedArmDesktopRunner -Environment $lowercase -OSArchitecture Arm64)) 'RUNNER_ARCH is matched exactly as GitHub sets it'
}
Complete-TestRun 'Interactive mode'
