<# .SYNOPSIS Proves the receipt side of the presentation-mode comparison (Tools/PresentationModes.psm1, plan W0.9): the input hashes cover the
runner and the fixtures, a complete receipt reports all four parts on both adapters, every missing or mislabelled part is a named
problem, the two cases that cannot be run stay "not run" with manual steps and the DPI change stays a simulation. No native build is
needed. #>
[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Import-Module (Join-Path $root 'Tools/PresentationModes.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force

function New-ModeDocument([string] $Mode) {
    $cases = foreach ($pair in @(@('liveResize','run'), @('minimizeAndRestore','run'), @('occlusion','run'), @('deviceLoss','run'), @('dpiChangeSimulated','simulated'),
            @('childHwnd','run'), @('monitorMoveThatChangesDpi','not run'), @('remoteSession','not run'))) { [ordered]@{ name = $pair[0]; status = $pair[1] } }
    return [ordered]@{ ran = $true
        pixels = [ordered]@{ presentedAlpha = [ordered]@{ pixels = 921600; channelsDisagree = 0; fullyTransparent = $(if ($Mode -eq 'CompositionSwapChain') { 1200 } else { 0 }) }
            textAntialiasing = [ordered]@{ direct2dTextAntialiasMode = 'DEFAULT' }
            captureCalibration = [ordered]@{ presentedOverWindowBackgroundVsBackBufferComposed = 0 } }
        cost = [ordered]@{ fullFrameCounters = [ordered]@{ frames = 20; framesThatPresented = 20 }
            frameTime = [ordered]@{ judged = $false; status = 'not judged, machine busy' }
            inputToPresent = [ordered]@{ inputReceivedEvents = 40; presentEvents = 40 } }
        memory = [ordered]@{ snapshots = @(1..5 | ForEach-Object { [ordered]@{ at = "s$_" } }) }
        behavior = [ordered]@{ cases = @($cases) } }
}
function New-CaseDocument([string] $Order = 'hwnd-first', [int] $Differing = 5000) {
    return [ordered]@{ kind = 'presentation-mode-case'; order = $Order
        modes = [ordered]@{ HwndSwapChain = (New-ModeDocument 'HwndSwapChain'); CompositionSwapChain = (New-ModeDocument 'CompositionSwapChain') }
        comparison = [ordered]@{
            pixels = [ordered]@{ presentedImageDifference = [ordered]@{ differing = $Differing; maxChannelDifference = 31 }
                compositionAlpha = [ordered]@{ fullyTransparent = 1200 } }
            cost = [ordered]@{ counterDifferencesPerFullFrame = @([ordered]@{ counter = 'frame.swapChains' }) } }
        scope = [ordered]@{ hosts = @(1..6 | ForEach-Object { [ordered]@{ host = "h$_"; chosenAt = @([ordered]@{ file = 'a.h'; text = 't'; line = 10 + $_ }) } }) } }
}
function New-Runs([string[]] $Adapters = @('Warp', 'Hardware')) {
    $runs = foreach ($adapter in $Adapters) { foreach ($fixture in (Get-PresentationModeFixtures)) {
        [pscustomobject]@{ Fixture = $fixture; Adapter = $adapter; Primary = (New-CaseDocument); Reversed = (New-CaseDocument 'composition-first') } } }
    return @($runs)
}
function New-Receipt([object[]] $Runs = (New-Runs)) {
    $hashes = [ordered]@{}; foreach ($path in (Get-PresentationModeInputs)) { $hashes[$path] = ('A' * 64) }
    return New-PresentationModeReceipt -Runs $Runs -Provenance ([ordered]@{ sourceCommit = ('a' * 40) }) -Inputs $hashes `
        -Timing ([ordered]@{ judged = $false; status = 'not judged, machine busy' }) -JudgedCommand './performance-presentation.ps1 -Adapter Hardware -Judged'
}

Invoke-TestCase 'input hashes cover the runner, the capture helper and both fixtures' {
    $hashes = Get-PresentationModeInputHashes -Root $root
    foreach ($path in @('Tests/Controls/Controls.Tests.PresentationModes.cpp', 'Tests/Support/Support.Tests.PresentedWindowCapture.h', 'Tests/Embedded/Embedded.Tests.PerformanceFixtureFx5.h',
            'Tests/Embedded/Embedded.Tests.PerformanceFixtureFx1.h', 'Samples/ComplexUi/ComplexUiScene.h', 'performance-presentation.ps1')) {
        Assert-True ($hashes.Contains($path) -and $hashes[$path] -match '^[0-9A-F]{64}$') "hash of $path"
    }
}

Invoke-TestCase 'a complete receipt carries all four parts on both adapters' {
    $receipt = New-Receipt
    Assert-Equal 0 @($receipt['problems']).Count "no problems: $(@($receipt['problems']) -join '; ')"
    foreach ($adapter in @('Warp', 'Hardware')) { foreach ($part in @('pixels', 'cost', 'behavior', 'scope')) { Assert-True $receipt['parts'][$adapter][$part] "$adapter $part" } }
    Assert-Equal 4 @($receipt['cases']).Count 'two fixtures on two adapters'
    Assert-Equal 0 @((Test-PresentationModeReceipt -Receipt $receipt).Problems).Count 'the check agrees'
}

Invoke-TestCase 'a receipt without the hardware adapter is incomplete there and says why' {
    $receipt = New-Receipt (New-Runs @('Warp'))
    Assert-True $receipt['parts']['Warp']['pixels'] 'WARP is complete'
    Assert-True (-not $receipt['parts']['Hardware']['pixels'] -and -not $receipt['parts']['Hardware']['behavior']) 'Hardware is not'
    Assert-True (@($receipt['problems'] | Where-Object { $_ -like 'no case for FX5 on Hardware' }).Count -eq 1) 'a named problem'
}

Invoke-TestCase 'every missing or mislabelled part of a case is a named problem' {
    $mutations = [ordered]@{
        'a mode that did not run' = { param($c) $c['modes']['CompositionSwapChain']['ran'] = $false }
        'a missing behaviour case' = { param($c) $c['modes']['HwndSwapChain']['behavior']['cases'] = @($c['modes']['HwndSwapChain']['behavior']['cases'] | Where-Object { $_['name'] -ne 'occlusion' }) }
        'a simulation reported as run' = { param($c) @($c['modes']['HwndSwapChain']['behavior']['cases'] | Where-Object { $_['name'] -eq 'dpiChangeSimulated' })[0]['status'] = 'run' }
        'a case that cannot run reported as run' = { param($c) @($c['modes']['HwndSwapChain']['behavior']['cases'] | Where-Object { $_['name'] -eq 'remoteSession' })[0]['status'] = 'run' }
        'an uncalibrated capture' = { param($c) $c['modes']['HwndSwapChain']['pixels']['captureCalibration']['presentedOverWindowBackgroundVsBackBufferComposed'] = 3 }
        'backdrop reads that disagree' = { param($c) $c['modes']['HwndSwapChain']['pixels']['presentedAlpha']['channelsDisagree'] = 2 }
        'a frame-time record without a verdict' = { param($c) $c['modes']['HwndSwapChain']['cost']['frameTime'] = [ordered]@{ judged = 'maybe' } }
        'no input to present' = { param($c) $c['modes']['HwndSwapChain']['cost']['inputToPresent']['inputReceivedEvents'] = 0 }
        'no memory timeline' = { param($c) $c['modes']['CompositionSwapChain']['memory']['snapshots'] = @() }
        'a scope anchor that is not found' = { param($c) $c['scope']['hosts'][0]['chosenAt'][0]['line'] = 0 }
        'too few scope hosts' = { param($c) $c['scope']['hosts'] = @($c['scope']['hosts'][0]) }
    }
    Assert-Equal 0 @(Test-PresentationModeCase -Case (New-CaseDocument) -Name 'ok').Count 'the unmutated case passes'
    foreach ($name in $mutations.Keys) {
        $case = New-CaseDocument
        & $mutations[$name] $case
        Assert-True (@(Test-PresentationModeCase -Case $case -Name 'x').Count -ge 1) "$name is reported"
    }
}

Invoke-TestCase 'the cases that cannot be run stay not run, with manual steps; the DPI change is a named simulation' {
    $receipt = New-Receipt
    foreach ($name in @('monitorMoveThatChangesDpi', 'remoteSession')) {
        $entry = @($receipt['notRun'] | Where-Object { $_['case'] -ceq $name })
        Assert-Equal 1 $entry.Count "$name is listed once"
        Assert-True (@($entry[0]['manualSteps']).Count -ge 3 -and -not [string]::IsNullOrWhiteSpace($entry[0]['reason'])) "$name has a reason and manual steps"
    }
    Assert-Equal 1 @($receipt['simulated'] | Where-Object { $_['case'] -ceq 'dpiChangeSimulated' }).Count 'the DPI change is a simulation'
    $broken = New-Receipt
    $broken['notRun'] = @($broken['notRun'] | Where-Object { $_['case'] -ne 'remoteSession' })
    Assert-True (@((Test-PresentationModeReceipt -Receipt $broken).Problems | Where-Object { $_ -like '*remoteSession*' }).Count -eq 1) 'dropping a not-run case is a problem'
}

Invoke-TestCase 'a receipt without the judged-timing command or the timing verdict is incomplete' {
    $receipt = New-Receipt
    $receipt['judgedTimingCommand'] = ''
    Assert-True (@((Test-PresentationModeReceipt -Receipt $receipt).Problems | Where-Object { $_ -like '*judged timing*' }).Count -eq 1) 'the command is required'
    $receipt = New-Receipt
    $receipt['timing'] = [ordered]@{ status = 'x' }
    Assert-True (@((Test-PresentationModeReceipt -Receipt $receipt).Problems | Where-Object { $_ -like '*timing verdict*' }).Count -eq 1) 'the verdict is required'
}

Invoke-TestCase 'the run in the opposite order is compared for determinism' {
    $same = Get-PresentationModeDeterminism -First (New-CaseDocument) -Second (New-CaseDocument 'composition-first')
    foreach ($key in $same.Keys) { Assert-True $same[$key] "$key holds for equal runs" }
    $other = Get-PresentationModeDeterminism -First (New-CaseDocument) -Second (New-CaseDocument 'composition-first' 5001)
    Assert-True (-not $other['presentedDifferingPixelsEqual'] -and $other['counterDifferencesEqual']) 'a changed pixel count is the only difference found'
}

Invoke-TestCase 'a receipt round-trips through JSON and the check still passes' {
    $receipt = New-Receipt
    $text = ConvertTo-Json -InputObject $receipt -Depth 60
    $back = $text | ConvertFrom-Json -AsHashtable -Depth 60
    $check = Test-PresentationModeReceipt -Receipt $back
    Assert-Equal 0 @($check.Problems).Count "no problems after a round trip: $(@($check.Problems) -join '; ')"
}

Complete-TestRun 'Test-PresentationModes'
