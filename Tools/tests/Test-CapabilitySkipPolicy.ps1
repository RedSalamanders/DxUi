[CmdletBinding()] param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TestSupport.psm1') -Force
Import-Module (Join-Path $PSScriptRoot '../CapabilitySkipPolicy.psm1') -Force
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))

Invoke-TestCase 'moved Menu-only skip cases are no longer allowed in the NewControls lane' {
    $previous = @{ RUNNER_ENVIRONMENT=$env:RUNNER_ENVIRONMENT; RUNNER_OS=$env:RUNNER_OS }
    try {
        $env:RUNNER_ENVIRONMENT = 'github-hosted'; $env:RUNNER_OS = 'Windows'
        $moved = @(
            [pscustomobject]@{ suite='NewControls'; test='TestWindowHostTabRaisesAutomationFocusChanges'; reason='DxUi window-host UIA focus changes require an interactive desktop' },
            [pscustomobject]@{ suite='NewControls'; test='TestMenuChoosesTheCursorWhenItOpensAndCloses'; reason='the menu open-and-close cursor test moves the physical pointer and is limited to the interactive Menu lane' },
            [pscustomobject]@{ suite='NewControls'; test='TestPlainMenuFractionalDpiRoundingDoesNotCreateScrollbar'; reason='plain-menu half-pixel layout case requires a 125% or 175% monitor' },
            [pscustomobject]@{ suite='NewControls'; test='TestModalMenuDispatchesUnrelatedTimerWithSubmenuId'; reason='the modal timer-dispatch regression opens a capturing menu and is limited to the interactive Menu lane' }
        )
        foreach ($profile in @(@('x64','Debug'), @('ARM64','ASan Debug'))) {
            Assert-Equal $moved.Count @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform $profile[0] -Configuration $profile[1] -Skips $moved).Count 'moved cases now fail closed in every native profile'
        }
    } finally { $env:RUNNER_ENVIRONMENT=$previous.RUNNER_ENVIRONMENT; $env:RUNNER_OS=$previous.RUNNER_OS }
}
Invoke-TestCase 'new, changed, duplicated and self-hosted skips fail closed' {
    $previous = @{ RUNNER_ENVIRONMENT=$env:RUNNER_ENVIRONMENT; RUNNER_OS=$env:RUNNER_OS }
    try {
        $env:RUNNER_ENVIRONMENT = 'github-hosted'; $env:RUNNER_OS = 'Windows'
        $known = [pscustomobject]@{ suite='Menu'; test='TestWindowHostTabRaisesAutomationFocusChanges'; reason='DxUi window-host UIA focus changes require an interactive desktop' }
        $new = [pscustomobject]@{ suite='NewControls'; test='TestNewFocusScenario'; reason='needs a desktop' }
        Assert-Equal 1 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips @($new)).Count 'new skip fails'
        $changed = [pscustomobject]@{ suite='Menu'; test=$known.test; reason=$known.reason }
        Assert-Equal 1 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips @($changed)).Count 'wrong suite fails'
        Assert-Equal 2 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips @($known,$known)).Count 'duplicate and unallowed skips fail independently'
        $env:RUNNER_ENVIRONMENT = 'self-hosted'
        Assert-Equal 1 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips @($known)).Count 'hosted allowance cannot mask self-hosted skip'
    } finally { $env:RUNNER_ENVIRONMENT=$previous.RUNNER_ENVIRONMENT; $env:RUNNER_OS=$previous.RUNNER_OS }
}
Invoke-TestCase 'skip logs are associated with exact test and suite names' {
    $fixture = New-FixtureRoot
    try {
        Set-FixtureFile $fixture 'suite.log' "[START] NewControls`n  [START] TestOne`nSKIPPED: requirement missing`n  [DONE] TestOne (0.1 s)`n"
        $entries = @(Get-DxUiCapabilitySkipEntries -LogPath (Join-Path $fixture 'suite.log') -Suite NewControls)
        Assert-Equal 'NewControls' $entries[0].suite 'suite is retained'
        Assert-Equal 'TestOne' $entries[0].test 'test is retained'
        Assert-Equal 'requirement missing' $entries[0].reason 'reason is retained'
    } finally { Remove-FixtureRoot $fixture }
}
Invoke-TestCase 'empty and multiple skip logs preserve collection boundaries' {
    $fixture = New-FixtureRoot
    try {
        Set-FixtureFile $fixture 'suite.log' "Foundation tests: PASS`n"
        $entries = @(Get-DxUiCapabilitySkipEntries -LogPath (Join-Path $fixture 'suite.log') -Suite Foundation)
        Assert-Equal 0 $entries.Count 'a complete suite without skips produces an empty collection'
        Assert-Equal 0 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips $entries).Count 'an empty collection is valid input to the CI policy'
        Set-FixtureFile $fixture 'suite.log' "[START] Menu`nSKIPPED: suite capability missing`n  [START] TestOne`nSKIPPED: first requirement missing`n  [DONE] TestOne (0.1 s)`n  [START] TestTwo`nSKIPPED: second requirement missing`n  [DONE] TestTwo (0.1 s)`n"
        $entries = @(Get-DxUiCapabilitySkipEntries -LogPath (Join-Path $fixture 'suite.log') -Suite Menu)
        Assert-Equal 3 $entries.Count 'each skip remains a separate entry'
        Assert-Equal 'Menu' $entries[0].test 'a suite-level skip keeps its owner'
        Assert-Equal 'TestOne' $entries[1].test 'the first named skip keeps its owner'
        Assert-Equal 'TestTwo' $entries[2].test 'the second named skip keeps its owner'
        Assert-Equal 3 @(Get-DxUiUnexpectedCapabilitySkips -Root $repository -Platform x64 -Configuration Debug -Skips $entries).Count 'every unallowed entry is reported'
    } finally { Remove-FixtureRoot $fixture }
}
Invoke-TestCase 'parsing preserves the caller suite-reporting functions' {
    Import-Module (Join-Path $repository 'Tools/SuiteFailure.psm1') -Force
    $fixture = New-FixtureRoot
    try {
        Set-FixtureFile $fixture 'suite.log' "Foundation tests: PASS`n"
        $null = Get-DxUiCapabilitySkipEntries -LogPath (Join-Path $fixture 'suite.log') -Suite Foundation
        Assert-True ([bool](Get-Command Write-SuiteSkips -ErrorAction SilentlyContinue)) 'parsing does not unload the caller reporter'
        Write-SuiteSkips -LogPath (Join-Path $fixture 'suite.log')
    } finally { Remove-FixtureRoot $fixture }
}
Complete-TestRun 'Capability skip policy'
