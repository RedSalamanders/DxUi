# What `test.ps1 -Interactive` selects, refuses and asks of the interactive desktop lease (Tests/InteractiveLease, DxUi.InteractiveLease.exe).
# The control suites that need real focus take the person's foreground window, keyboard focus and pointer; the lease asks first, warns
# while they run and gives all three back however the run ends. These are the pure parts, so Tools/tests/Test-InteractiveMode.ps1
# covers them without a desktop: which suites are interactive, when a run must refuse, the plan the lease reads, the result it
# writes and what that result means.
Set-StrictMode -Version Latest

# The control suites whose contract needs real focus: DxUi.ControlTests.exe rejects --no-activate for them (its suiteCanActivate), and
# a run that cannot get the foreground records a capability skip. Test-InteractiveMode.ps1 requires this list to be that one.
# -Interactive runs the first two when -Suites is not given; the resource fixtures are diagnostics that run only when named.
$script:InteractiveSuites = @('Menu', 'NativeTextInput')
$script:OptInInteractiveSuites = @('MenuResources', 'MenuResourceScaling')

# Seconds each suite takes under a lease, for the confirmation's "about N minutes". Menu and NativeTextInput are the [DONE] durations of
# the six interactive runs of 2026-09-30 (Measurements/MenuDescriptions/2026-09-30/directed-input), rounded up; the two fixtures
# had no timing there and 120 is an estimate until one is run.
$script:SuiteSeconds = @{
    Menu                = @{ 'Debug' = 45; 'Release' = 45; 'ASan Debug' = 55 }
    NativeTextInput     = @{ 'Debug' = 20; 'Release' = 15; 'ASan Debug' = 25 }
    MenuResources       = @{ 'Debug' = 120; 'Release' = 120; 'ASan Debug' = 120 }
    MenuResourceScaling = @{ 'Debug' = 120; 'Release' = 120; 'ASan Debug' = 120 }
}
# Process start, the warning, the three-attempt restoration and the log handling around the suites.
$script:LeaseOverheadSeconds = 20

# What a run's environment says about who is there. A CI job has no one at its desktop, and its hosted desktop is not a person's.
$script:CiVariables = @('CI', 'GITHUB_ACTIONS', 'TF_BUILD', 'APPVEYOR', 'BUILDKITE', 'JENKINS_URL', 'TEAMCITY_VERSION')

# The exit codes of DxUi.InteractiveLease.exe (DxUi::TestSupport::LeaseExit in Tests/Support/InteractiveLease.h).
$script:LeaseExit = [ordered]@{
    Passed = 0; ChildFailed = 1; Usage = 2; NoDesktop = 20; Declined = 21; Busy = 22; NoWarning = 23; LaunchFailed = 24; NotRestored = 25; Interrupted = 26
}

function Get-DxUiInteractiveSuiteNames {
    # The suites -Interactive runs by default, or with -IncludeOptIn every suite that needs the desktop.
    [CmdletBinding()] param([switch] $IncludeOptIn)
    if ($IncludeOptIn) { return @($script:InteractiveSuites + $script:OptInInteractiveSuites) }
    return @($script:InteractiveSuites)
}

function Test-DxUiInteractiveSuite {
    # Whether DxUi.ControlTests.exe runs the suite with real focus, so test.ps1 never gives it --no-activate.
    [CmdletBinding()] param([Parameter(Mandatory)][AllowEmptyString()][string] $Suite)
    return $Suite -in (Get-DxUiInteractiveSuiteNames -IncludeOptIn)
}

function Get-DxUiLeaseExitCodes { return $script:LeaseExit }

function Resolve-DxUiInteractiveSuites {
    # The suites an interactive run takes: the default two when -Suites was not given, otherwise exactly those named, every one of
    # which has to need the desktop. A suite that does not is refused rather than run, so the lease holds only what needs it.
    [CmdletBinding()] param([AllowEmptyCollection()][string[]] $Suites = @(), [Parameter(Mandatory)][bool] $Requested)
    if (-not $Requested) { return @(Get-DxUiInteractiveSuiteNames) }
    $all = Get-DxUiInteractiveSuiteNames -IncludeOptIn
    $others = @($Suites | Where-Object { $_ -notin $all })
    if ($others.Count) {
        throw "-Interactive runs only suites that need the real desktop ($($all -join ', ')); not interactive: $($others -join ', '). Leave -Suites out for $((Get-DxUiInteractiveSuiteNames) -join ' and ')."
    }
    if ($Suites.Count -eq 0) { throw "-Interactive was given no suite; leave -Suites out for $((Get-DxUiInteractiveSuiteNames) -join ' and ')." }
    # The canonical spelling, each once, in the order given.
    return @($Suites | ForEach-Object { $name = $_; $all | Where-Object { $_ -eq $name } | Select-Object -First 1 } | Select-Object -Unique)
}

function Get-DxUiInteractiveRefusal {
    # Why an interactive run cannot take this session's desktop, or $null when nothing stands in the way. This is the quick check made
    # before anything is built; DxUi.InteractiveLease.exe checks again, natively, for a locked screen, a disconnected session and a
    # screen saver, right before it asks the person. Every input can be given, so a test needs no real environment.
    [CmdletBinding()] param(
        [hashtable] $Environment = $null,
        [Nullable[bool]] $UserInteractive = $null,
        [Nullable[bool]] $OnWindows = $null
    )
    $windows = if ($null -ne $OnWindows) { [bool]$OnWindows } else { [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT }
    if (-not $windows) { return 'this is not Windows' }
    foreach ($name in $script:CiVariables) {
        $value = if ($null -ne $Environment) { if ($Environment.ContainsKey($name)) { [string]$Environment[$name] } } else { [Environment]::GetEnvironmentVariable($name) }
        if (-not [string]::IsNullOrWhiteSpace($value) -and $value.Trim() -notin @('0', 'false')) {
            return "a CI job runs this ($name is set), and its desktop is not a person's to take over"
        }
    }
    $interactive = if ($null -ne $UserInteractive) { [bool]$UserInteractive } else { [Environment]::UserInteractive }
    if (-not $interactive) { return 'this process has no interactive window station: it runs as a service, a scheduled task or a remote shell' }
    return $null
}

function Get-DxUiInteractiveEstimateSeconds {
    # About how long a lease takes for these suites in this configuration, from the confirmation to the restoration.
    [CmdletBinding()] param([Parameter(Mandatory)][string[]] $Suites, [Parameter(Mandatory)][string] $Configuration)
    $seconds = $script:LeaseOverheadSeconds
    foreach ($suite in $Suites) { $seconds += $script:SuiteSeconds[$suite][$Configuration] }
    return [int]$seconds
}

function ConvertTo-DxUiCommandLine {
    # One process's command line as CreateProcess takes it: the program always quoted, an argument only when it needs it.
    [CmdletBinding()] param([Parameter(Mandatory)][string] $Program, [string[]] $Arguments = @())
    $quote = { param([string] $Text) if ($Text -match '[\s"]' ) { '"' + ($Text -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"' } else { $Text } }
    return (@('"' + $Program + '"') + @($Arguments | ForEach-Object { & $quote $_ })) -join ' '
}

function New-DxUiLeasePlanText {
    # The plan the lease reads: one child per line, name, log path and command line separated by tabs.
    [CmdletBinding()] param([Parameter(Mandatory)][object[]] $Runs)
    $lines = foreach ($run in $Runs) {
        foreach ($field in @($run.Name, $run.Log, $run.CommandLine)) {
            if ([string]::IsNullOrEmpty($field) -or $field -match "[`t`r`n]") { throw "A lease plan field cannot be empty or hold a tab or a line break: '$field'" }
        }
        "$($run.Name)`t$($run.Log)`t$($run.CommandLine)"
    }
    return ($lines -join "`n") + "`n"
}

function Read-DxUiLeaseResult {
    # The `key: value` lines DxUi.InteractiveLease.exe writes, as an object; $null for a result that is missing or empty.
    [CmdletBinding()] param([AllowNull()][AllowEmptyString()][string] $Text)
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $facts = [ordered]@{}
    $children = [Collections.Generic.List[object]]::new()
    foreach ($line in $Text -split '\r?\n') {
        if ($line -notmatch '^(?<key>[A-Za-z][A-Za-z0-9.]*): (?<value>.*)$') { continue }
        $key = $Matches['key']
        $value = $Matches['value']
        if ($key -eq 'child') {
            if ($value -match '^(?<name>\S+) launched=(?<launched>[01]) exit=(?<exit>\d+) seconds=(?<seconds>[\d.]+) timedout=(?<timedout>[01]) interrupted=(?<interrupted>[01])$') {
                # An exit code is a 32-bit pattern ($LASTEXITCODE shows 0xC0000005 as -1073741819), so it is read as one.
                $children.Add([pscustomobject]@{
                    Name = $Matches['name']; Launched = $Matches['launched'] -eq '1'
                    ExitCode = [BitConverter]::ToInt32([BitConverter]::GetBytes([uint32][uint64]$Matches['exit']), 0)
                    Seconds = [double]::Parse($Matches['seconds'], [Globalization.CultureInfo]::InvariantCulture)
                    TimedOut = $Matches['timedout'] -eq '1'; Interrupted = $Matches['interrupted'] -eq '1'
                })
            }
        } else { $facts[$key] = $value }
    }
    if (-not $facts.Contains('state')) { return $null }
    $part = { param([string] $Key) if ($facts.Contains($Key)) { $facts[$Key] } else { 'none' } }
    return [pscustomobject]@{
        State = $facts['state']
        Exit = if ($facts.Contains('exit')) { [int]$facts['exit'] } else { -1 }
        Reason = if ($facts.Contains('reason')) { $facts['reason'] } else { '' }
        Confirmation = & $part 'confirmation'
        SavedForeground = if ($facts.Contains('foreground.saved')) { $facts['foreground.saved'] } else { '' }
        Foreground = & $part 'restoration.foreground'
        Focus = & $part 'restoration.focus'
        Cursor = & $part 'restoration.cursor'
        CursorSaved = & $part 'cursor.saved'
        CursorAtExit = & $part 'cursor.atExit'
        RunMovedSomething = $facts.Contains('restoration.runMovedSomething') -and $facts['restoration.runMovedSomething'] -eq '1'
        Children = $children.ToArray()
    }
}

function Get-DxUiLeaseExitMeaning {
    # What a lease exit code says, for a message.
    [CmdletBinding()] param([Parameter(Mandatory)][int] $ExitCode)
    switch ($ExitCode) {
        0 { 'every suite passed and the desktop is as it was' }
        1 { 'a suite failed' }
        2 { 'the lease was given a malformed command line' }
        20 { 'there is no interactive desktop to take' }
        21 { 'the confirmation was cancelled, or nobody answered it' }
        22 { 'another interactive run holds this session''s lease' }
        23 { 'the warning could not be shown, so the desktop was not taken' }
        24 { 'a suite could not be started' }
        25 { 'the suites passed, but the desktop could not be given back' }
        26 { 'the run was stopped' }
        default { "the lease ended with the unexpected exit code $ExitCode" }
    }
}

function Get-DxUiLeaseProblems {
    # What is wrong with a finished lease, as messages; nothing when every suite ran and the desktop is as the person had it.
    # `Result` is the parsed result and `ExitCode` the lease's own exit code.
    [CmdletBinding()] param([AllowNull()] $Result, [Parameter(Mandatory)][int] $ExitCode)
    $problems = [Collections.Generic.List[string]]::new()
    if ($null -eq $Result) {
        $problems.Add("the lease wrote no result: $(Get-DxUiLeaseExitMeaning $ExitCode) (exit code $ExitCode)")
        return $problems.ToArray()
    }
    if ($Result.State -ne 'completed') {
        $why = if ($Result.Reason) { $Result.Reason } else { Get-DxUiLeaseExitMeaning $ExitCode }
        $problems.Add("the interactive run did not take place ($($Result.State)): $why")
    }
    $failed = [Collections.Generic.List[string]]::new()
    foreach ($part in 'Foreground', 'Focus', 'Cursor') { if ($Result.$part -eq 'failed') { $failed.Add($part.ToLowerInvariant()) } }
    if ($failed.Count) {
        $window = if ($Result.SavedForeground) { " (the window was $($Result.SavedForeground))" } else { '' }
        $problems.Add("the desktop could not be given back: $($failed -join ', ') failed$window")
    }
    return $problems.ToArray()
}

function Test-DxUiDesktopAvailable {
    # Asks DxUi.InteractiveLease.exe whether this session has a desktop to take, without taking or showing anything. $null when it
    # does, otherwise the reason it says.
    [CmdletBinding()] param([Parameter(Mandatory)][string] $Executable)
    $output = @(& $Executable --check 2>&1 | ForEach-Object { "$_" })
    $exit = $LASTEXITCODE
    if ($exit -eq 0) { return $null }
    if ($exit -eq $script:LeaseExit.NoDesktop) { return ((@($output | Where-Object { $_ -like '[[]LEASE]*' }) | Select-Object -First 1) -replace '^\[LEASE\] (no interactive desktop: )?', '') }
    throw "DxUi.InteractiveLease.exe --check failed with exit code ${exit}: $($output -join ' ')"
}

function Invoke-DxUiInteractiveLease {
    # Runs the suites of an interactive run under one lease and returns what happened: the lease's exit code and its parsed result.
    # The lease refuses without a desktop, asks the person, shows its warning, runs each suite as a child of its own (its output
    # going to the log named in the run) and restores the person's desktop. Only ever called for -Interactive.
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string] $Executable,
        [Parameter(Mandatory)][object[]] $Runs,
        [Parameter(Mandatory)][string] $Label,
        [Parameter(Mandatory)][int] $EstimateSeconds,
        [Parameter(Mandatory)][string] $WorkDirectory,
        [int] $ConfirmSeconds = 120,
        [int] $ChildTimeoutSeconds = 900
    )
    $plan = Join-Path $WorkDirectory 'interactive-plan.txt'
    $result = Join-Path $WorkDirectory 'interactive-result.txt'
    # A result left by an earlier run must never be taken for this run's.
    if (Test-Path -LiteralPath $result -PathType Leaf) { Remove-Item -LiteralPath $result }
    [IO.File]::WriteAllText($plan, (New-DxUiLeasePlanText -Runs $Runs), [Text.UTF8Encoding]::new($false))
    # Native stdout is progress for the person, not part of this function's returned object.
    & $Executable --run "--plan=$plan" "--result=$result" "--label=$Label" "--estimate=$EstimateSeconds" "--confirm-timeout=$ConfirmSeconds" "--child-timeout=$ChildTimeoutSeconds" | Out-Host
    $exit = $LASTEXITCODE
    $parsed = if (Test-Path -LiteralPath $result -PathType Leaf) { Read-DxUiLeaseResult -Text ([IO.File]::ReadAllText($result)) }
    return [pscustomobject]@{ ExitCode = $exit; Result = $parsed; ResultPath = $result; PlanPath = $plan }
}

Export-ModuleMember -Function Get-DxUiInteractiveSuiteNames, Test-DxUiInteractiveSuite, Get-DxUiLeaseExitCodes, Resolve-DxUiInteractiveSuites,
    Get-DxUiInteractiveRefusal, Get-DxUiInteractiveEstimateSeconds, ConvertTo-DxUiCommandLine, New-DxUiLeasePlanText, Read-DxUiLeaseResult,
    Get-DxUiLeaseExitMeaning, Get-DxUiLeaseProblems, Test-DxUiDesktopAvailable, Invoke-DxUiInteractiveLease
