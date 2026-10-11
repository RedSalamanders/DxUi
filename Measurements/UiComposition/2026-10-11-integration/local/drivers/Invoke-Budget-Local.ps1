[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$expectedHead = 'bf11ba44fbd661fd5e1e6015f95d5719a4bfba83'
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) { throw 'Require the clean budget native source.' }
foreach ($validationProfile in @(@('ASan Debug','asan'),@('Debug','debug'),@('Release','release'))) {
    $configuration = $validationProfile[0]
    Write-Output "FINAL BUDGET PROFILE: $configuration at $expectedHead"
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focused.ps1') -Configuration $configuration -Tag "qualified-$($validationProfile[1])-budget-final"
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focus.ps1') -Configuration $configuration -Attempt 5 -SkipBuild
}
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) { throw 'Source changed during budget native validation.' }
Write-Output 'PASS all three final budget local profiles: 20 selected cases and four full foreground suites each.'
