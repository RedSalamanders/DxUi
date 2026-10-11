[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$expectedHead = '061748aab88c8db0b623e8a244ba884d9193b933'
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) { throw 'Require the clean formatted native source.' }
foreach ($validationProfile in @(@('ASan Debug','asan'),@('Debug','debug'),@('Release','release'))) {
    $configuration = $validationProfile[0]
    Write-Output "FORMATTED LOCAL PROFILE: $configuration at $expectedHead"
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focused.ps1') -Configuration $configuration -Tag "qualified-$($validationProfile[1])-formatted"
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focus.ps1') -Configuration $configuration -Attempt 4 -SkipBuild
}
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) { throw 'Source changed during formatted native validation.' }
Write-Output 'PASS all three formatted local profiles: 20 selected cases and four full foreground suites each.'
