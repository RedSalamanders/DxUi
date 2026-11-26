# Presentation-mode comparison receipts (plan item W0.9; Tests/README.md, "Presentation-mode comparison"): the merge of the
# native case documents into presentation-mode.json, the cases this tooling cannot run, and the completeness check that decides
# whether the receipt carries all four parts on both adapters. Read-only with respect to the machine.
Set-StrictMode -Version Latest

$script:Inputs = @(
    'performance-presentation.ps1',
    'Tools/PresentationModes.psm1',
    'Tests/Controls/Controls.Tests.PresentationModes.cpp',
    'Tests/Controls/Controls.Tests.PresentationModes.h',
    'Tests/Controls/Controls.Tests.PerformanceFixtures.cpp',
    'Tests/Controls/Controls.Tests.PresentedFrame.h',
    'Tests/Support/Support.Tests.PresentedWindowCapture.h',
    'Tests/Embedded/Embedded.Tests.PerformanceFixture.h',
    'Tests/Embedded/Embedded.Tests.PerformanceFixtureFx1.h',
    'Tests/Embedded/Embedded.Tests.PerformanceFixtureFx5.h',
    'Samples/ComplexUi/ComplexUiScene.h')

$script:Fixtures = @('FX5', 'FX1')
$script:Adapters = @('Warp', 'Hardware')
$script:Modes = @('HwndSwapChain', 'CompositionSwapChain')

# The behaviour cases of the item, each with the only status it may carry in a complete receipt.
$script:BehaviorCases = [ordered]@{
    liveResize = 'run'; minimizeAndRestore = 'run'; occlusion = 'run'; deviceLoss = 'run'; dpiChangeSimulated = 'simulated'
    childHwnd = 'run'; monitorMoveThatChangesDpi = 'not run'; remoteSession = 'not run'
}

function Get-PresentationModeInputs { return @($script:Inputs) }
function Get-PresentationModeFixtures { return @($script:Fixtures) }

function Get-PresentationModeInputHashes {
    <# SHA-256 of the files the comparison is built from, keyed by repository path. #>
    param([Parameter(Mandatory)][string] $Root)
    $hashes = [ordered]@{}
    foreach ($path in $script:Inputs) {
        $full = Join-Path $Root $path
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "Presentation-mode comparison input is missing: $path" }
        $hashes[$path] = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash
    }
    return $hashes
}

function Get-PresentationModeNotRun {
    <# The cases that cannot be driven from the test program, and what a person does instead. Never filled with a simulation. #>
    return @(
        [ordered]@{
            case = 'monitorMoveThatChangesDpi'
            reason = 'needs a second monitor of another scale and a window dragged across them; the test program has no monitor to move to and takes no focus or pointer. dpiChangeSimulated delivers WM_DPICHANGED to the host and is a simulation of the host side only.'
            manualSteps = @(
                'Use a machine with two monitors at different scales (for example 100% and 150%), or one monitor whose scale is changed in Settings > System > Display while the window is open.',
                'Build and run a small program that attaches ControlHost to a window with ControlHost::AttachOptions{.presentationMode = ControlHost::PresentationMode::HwndSwapChain}, shows FX5''s scene (or any text-heavy page) and draws a frame; repeat with CompositionSwapChain.',
                'Drag the window slowly from one monitor to the other and back, and also drop it straddling both. Look for: a frame at the wrong scale, text that re-wraps late, a frame stretched by the compositor, a transparent or black band, and a frame that never repaints.',
                'Record for each mode: the frames presented during the drag (a DxUi frame metric sink or the DxUi trace provider), the DPI the host reports after the drop, and a Print Screen of the window after the drop.'
            )
        },
        [ordered]@{
            case = 'remoteSession'
            reason = 'needs a Remote Desktop session; the test program runs in the local console session and records only GetSystemMetrics(SM_REMOTESESSION) = 0.'
            manualSteps = @(
                'Connect to the test machine with Remote Desktop (mstsc) from a second machine and start the same program inside the session; SM_REMOTESESSION must read 1.',
                'Run it in HwndSwapChain and in CompositionSwapChain with the session at 100% and at a scaling above 100%, and with and without "Use all my monitors".',
                'Look for: DXGI device creation failing or falling back to WARP in the session, a swap chain that never presents, text antialiasing that differs from the console (ClearType over RDP), a transparent area that shows black or the desktop, and visible tearing or lag on resize.',
                'Record the adapter the host reports in the session, the present results and a screenshot taken from the client.'
            )
        })
}

function Get-PresentationModeSimulated {
    return @([ordered]@{
        case = 'dpiChangeSimulated'
        what = 'WM_DPICHANGED (96 to 144 and back, the client area scaled with it) delivered to the host through the seam the performance fixtures use for a DPI change. No monitor moves, the system does not virtualize or rescale anything and the compositor sees no scale change.'
    })
}

function Test-PresentationModeCase {
    <# The problems of one case document (empty when it carries every part for both modes). #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Case, [Parameter(Mandatory)][string] $Name)
    $problems = [Collections.Generic.List[string]]::new()
    $fail = { param($text) $problems.Add("${Name}: $text") }
    if ($Case['kind'] -cne 'presentation-mode-case') { & $fail 'not a presentation-mode case document'; return $problems }
    foreach ($mode in $script:Modes) {
        $entry = $Case['modes'][$mode]
        if ($null -eq $entry -or $entry['ran'] -ne $true) { & $fail "$mode did not run"; continue }
        $pixels = $entry['pixels']
        if ($null -eq $pixels['presentedAlpha'] -or [int64]$pixels['presentedAlpha']['pixels'] -le 0) { & $fail "$mode has no presented alpha" }
        elseif ([int64]$pixels['presentedAlpha']['channelsDisagree'] -ne 0) { & $fail "$mode's two backdrop reads disagree on the alpha of some pixels" }
        if ($null -eq $pixels['textAntialiasing']) { & $fail "$mode has no text antialiasing record" }
        if ($null -eq $pixels['captureCalibration'] -or [int64]$pixels['captureCalibration']['presentedOverWindowBackgroundVsBackBufferComposed'] -ne 0) { & $fail "$mode's capture does not calibrate against its back buffer" }
        $cost = $entry['cost']
        if ($null -eq $cost['fullFrameCounters'] -or [int64]$cost['fullFrameCounters']['framesThatPresented'] -le 0) { & $fail "$mode has no full-frame counters" }
        if ($null -eq $cost['frameTime'] -or $cost['frameTime']['judged'] -isnot [bool] -or [string]::IsNullOrEmpty([string]$cost['frameTime']['status'])) { & $fail "$mode has no frame-time record or verdict" }
        if ($null -eq $cost['inputToPresent'] -or [int64]$cost['inputToPresent']['inputReceivedEvents'] -le 0) { & $fail "$mode has no input-to-present record" }
        if (@($entry['memory']['snapshots']).Count -lt 4) { & $fail "$mode has no memory timeline" }
        foreach ($caseName in $script:BehaviorCases.Keys) {
            $found = @($entry['behavior']['cases'] | Where-Object { $_['name'] -ceq $caseName })
            if (-not $found.Count) { & $fail "$mode has no behaviour case $caseName" }
            elseif ($found[0]['status'] -cne $script:BehaviorCases[$caseName]) { & $fail "$mode's $caseName has status '$($found[0]['status'])', expected '$($script:BehaviorCases[$caseName])'" }
        }
    }
    if ($null -eq $Case['comparison']['pixels'] -or $null -eq $Case['comparison']['cost']) { & $fail 'no comparison between the modes' }
    $scope = $Case['scope']
    if ($null -eq $scope -or @($scope['hosts']).Count -lt 5) { & $fail 'the scope lists too few hosts' }
    else {
        foreach ($hostEntry in @($scope['hosts'])) {
            foreach ($anchor in @($hostEntry['chosenAt'])) { if ([int64]$anchor['line'] -le 0) { & $fail "scope anchor not found: $($anchor['file']): $($anchor['text'])" } }
        }
    }
    return $problems
}

function Test-PresentationModeReceipt {
    <# The problems of a receipt, and which parts it carries per adapter. A part is complete on an adapter when every fixture's
       case document has it for both modes. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $Receipt)
    $problems = [Collections.Generic.List[string]]::new()
    if ($Receipt['workloadOwner'] -cne 'DxUi' -or -not ([string]$Receipt['fixture']).StartsWith('dxui-', [StringComparison]::Ordinal)) { $problems.Add('not a DxUi receipt') }
    $inputs = $Receipt['benchmarkInputs']
    if ($null -eq $inputs -or -not $inputs.Count -or @($inputs.Values | Where-Object { $_ -notmatch '^[0-9A-Fa-f]{64}$' }).Count) { $problems.Add('missing input hashes') }
    $parts = [ordered]@{}
    foreach ($adapter in $script:Adapters) {
        $complete = $true
        foreach ($fixture in $script:Fixtures) {
            $found = @($Receipt['cases'] | Where-Object { $_['fixture'] -ceq $fixture -and $_['adapter'] -ceq $adapter })
            if (-not $found.Count) { $problems.Add("no case for $fixture on $adapter"); $complete = $false; continue }
            $caseProblems = @(Test-PresentationModeCase -Case $found[0]['primary'] -Name "$fixture/$adapter")
            if ($caseProblems.Count) { foreach ($text in $caseProblems) { $problems.Add($text) }; $complete = $false }
        }
        $parts[$adapter] = [ordered]@{ pixels = $complete; cost = $complete; behavior = $complete; scope = $complete }
    }
    foreach ($name in @('monitorMoveThatChangesDpi', 'remoteSession')) {
        if (-not @($Receipt['notRun'] | Where-Object { $_['case'] -ceq $name -and @($_['manualSteps']).Count -ge 3 }).Count) { $problems.Add("the not-run case $name is missing or has no manual steps") }
    }
    if (-not @($Receipt['simulated'] | Where-Object { $_['case'] -ceq 'dpiChangeSimulated' }).Count) { $problems.Add('the DPI-change simulation is not named as a simulation') }
    if ($null -eq $Receipt['timing'] -or $Receipt['timing']['judged'] -isnot [bool]) { $problems.Add('the timing verdict is missing') }
    if ([string]::IsNullOrWhiteSpace([string]$Receipt['judgedTimingCommand'])) { $problems.Add('the command that takes the judged timing is missing') }
    return [pscustomobject]@{ Problems = @($problems); Parts = $parts }
}

function Get-PresentationModeDeterminism {
    <# Whether the same case run in the opposite order gives the same counters, alpha and pixel differences. #>
    param([Parameter(Mandatory)][Collections.IDictionary] $First, [Parameter(Mandatory)][Collections.IDictionary] $Second)
    $a = $First['comparison']; $b = $Second['comparison']
    $namesA = @($a['cost']['counterDifferencesPerFullFrame'] | ForEach-Object { $_['counter'] })
    $namesB = @($b['cost']['counterDifferencesPerFullFrame'] | ForEach-Object { $_['counter'] })
    $pixelsA = $a['pixels']['presentedImageDifference']; $pixelsB = $b['pixels']['presentedImageDifference']
    return [ordered]@{
        counterDifferencesEqual = (($namesA -join ',') -ceq ($namesB -join ','))
        presentedDifferingPixelsEqual = ([int64]$pixelsA['differing'] -eq [int64]$pixelsB['differing'])
        presentedMaxChannelDifferenceEqual = ([int64]$pixelsA['maxChannelDifference'] -eq [int64]$pixelsB['maxChannelDifference'])
        compositionTransparentPixelsEqual = ([int64]$a['pixels']['compositionAlpha']['fullyTransparent'] -eq [int64]$b['pixels']['compositionAlpha']['fullyTransparent'])
    }
}

function New-PresentationModeReceipt {
    <# Merges the case documents (an entry per fixture and adapter: the primary run and the run in the opposite order) with
       provenance, the timing verdict and the cases that were not run. #>
    param([Parameter(Mandatory)][object[]] $Runs, [Parameter(Mandatory)][Collections.IDictionary] $Provenance, [Parameter(Mandatory)][Collections.IDictionary] $Inputs,
        [Parameter(Mandatory)][Collections.IDictionary] $Timing, [Parameter(Mandatory)][string] $JudgedCommand)
    $cases = foreach ($run in $Runs) {
        $reversed = [ordered]@{ order = $run.Reversed['order'] }
        foreach ($mode in $script:Modes) { $reversed[$mode] = [ordered]@{ memory = $run.Reversed['modes'][$mode]['memory'] } }
        $reversed['sameResultsAsThePrimaryOrder'] = Get-PresentationModeDeterminism -First $run.Primary -Second $run.Reversed
        [ordered]@{ fixture = $run.Fixture; adapter = $run.Adapter; primary = $run.Primary; reversedOrder = $reversed }
    }
    $receipt = [ordered]@{
        schema = 1; workloadOwner = 'DxUi'; fixture = 'dxui-presentation-mode-v1'; item = 'W0.9'
        title = 'Presentation-mode comparison: HwndSwapChain against CompositionSwapChain'
        benchmarkInputs = $Inputs; provenance = $Provenance; timing = $Timing; judgedTimingCommand = $JudgedCommand
        cases = @($cases); notRun = @(Get-PresentationModeNotRun); simulated = @(Get-PresentationModeSimulated)
    }
    $check = Test-PresentationModeReceipt -Receipt $receipt
    $receipt['parts'] = $check.Parts
    $receipt['problems'] = @($check.Problems)
    return $receipt
}

Export-ModuleMember -Function Get-PresentationModeInputs, Get-PresentationModeFixtures, Get-PresentationModeInputHashes, Get-PresentationModeNotRun,
    Get-PresentationModeSimulated, Test-PresentationModeCase, Test-PresentationModeReceipt, Get-PresentationModeDeterminism, New-PresentationModeReceipt
