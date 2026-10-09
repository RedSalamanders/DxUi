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
            if ($body.Extent.StartOffset -le $Node.Extent.StartOffset -and $Node.Extent.EndOffset -le $body.Extent.EndOffset -and $clause.Item1.Extent.Text -match '\$Interactive\b') { return $true }
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
    'state: completed', 'exit: 1', 'confirmation: started', 'foreground.saved: class ''CASCADIA_HOSTING_WINDOW_CLASS'' of WindowsTerminal.exe (process 4242)',
    'restoration.foreground: restored', 'restoration.focus: unchanged', 'restoration.cursor: restored', 'cursor.saved: 400,300', 'cursor.atExit: 900,700',
    'restoration.runMovedSomething: 1', 'child: Menu launched=1 exit=0 seconds=43.3 timedout=0 interrupted=0',
    'child: NativeTextInput launched=1 exit=3221225477 seconds=1.2 timedout=0 interrupted=0') -join "`r`n"
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
Invoke-TestCase 'what is wrong with a finished lease is said in a message' {
    $fine = Read-DxUiLeaseResult -Text ($completed -replace 'exit=3221225477', 'exit=0')
    Assert-Equal 0 @(Get-DxUiLeaseProblems -Result $fine -ExitCode 0).Count 'a lease that completed with its desktop restored has no problem'
    $noResult = @(Get-DxUiLeaseProblems -Result $null -ExitCode 21)
    Assert-True ($noResult.Count -eq 1 -and $noResult[0].Contains('wrote no result') -and $noResult[0].Contains('cancelled')) "no result is named with what its exit code means: $noResult"
    $declined = Read-DxUiLeaseResult -Text "state: declined`nexit: 21`nreason: the person chose Cancel`nconfirmation: declined`n"
    $problems = @(Get-DxUiLeaseProblems -Result $declined -ExitCode 21)
    Assert-True ($problems.Count -eq 1 -and $problems[0].Contains('(declined)') -and $problems[0].Contains('the person chose Cancel')) "a cancelled run did not take place: $problems"
    $failed = Read-DxUiLeaseResult -Text ($completed -replace 'restoration.focus: unchanged', 'restoration.focus: failed' -replace 'restoration.cursor: restored', 'restoration.cursor: failed')
    $problems = @(Get-DxUiLeaseProblems -Result $failed -ExitCode 1)
    Assert-True ($problems.Count -eq 1 -and $problems[0].Contains('focus, cursor failed') -and $problems[0].Contains('WindowsTerminal.exe')) "a part that was not given back is named, with the window: $problems"
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
Invoke-TestCase 'desktop takeover requires -Interactive; only the read-only probe also permits a verified hosted runner' {
    $calls = Get-CommandCalls @('Invoke-DxUiInteractiveLease', 'Test-DxUiDesktopAvailable', 'Resolve-DxUiInteractiveSuites')
    Assert-True ($calls.Count -ge 3) 'test.ps1 uses each of them'
    foreach ($call in $calls) { Assert-True (Test-GuardedByInteractive $call -AllowHostedProbe) "$($call.GetCommandName()) has the required desktop authorization guard at line $($call.Extent.StartLineNumber)" }
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
        $Platform = 'x64'; $Configuration = 'Debug'; $logs = Join-Path $fakeRoot 'logs'; $instrumentation = @()
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
    $run = Invoke-TestScript @('-Interactive') @{ CI = 'true' } $fixture
    Assert-True $run.Exited 'the refusal ended the run'
    Assert-True ($run.Exit -ne 0) 'with a failing exit code'
    # On Windows the CI variable is the reason; where this runs on another system (CI's validation job is Ubuntu) there is no desktop at all.
    $reason = if ($IsWindows) { 'CI is set' } else { 'this is not Windows' }
    Assert-True ((Get-FlatText $run.Output).Contains('Interactive tests need an interactive desktop, and there is none') -and (Get-FlatText $run.Output).Contains($reason)) "saying why: $($run.Output)"
    Assert-True (-not $run.Output.Contains('Interactive run:') -and -not $run.Output.Contains('Running ') -and -not $run.Output.Contains('== ')) 'and nothing ran: no tooling test, no build, no suite'
    Assert-Equal ((ConvertTo-OrdinalOrder $before) -join "`n") ((ConvertTo-OrdinalOrder (Get-OutputFiles $fixture)) -join "`n") 'no log or receipt was written'
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
Complete-TestRun 'Interactive mode'
