[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$expectedHead = '284a9ec60cc767cad16775e4131b597ff1101b23'
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) {
    throw 'Require the frozen, clean repaired native source.'
}
foreach ($profile in @(@('ASan Debug','asan'),@('Debug','debug'),@('Release','release'))) {
    $configuration = $profile[0]
    Write-Output "FINAL LOCAL PROFILE: $configuration at $expectedHead"
    $focusedArguments = @{Configuration=$configuration; Tag="qualified-$($profile[1])-ime-final"}
    if ($configuration -ceq 'ASan Debug') { $focusedArguments['SkipBuild'] = $true }
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focused.ps1') @focusedArguments
    & (Join-Path $PSScriptRoot 'Invoke-Integration-Focus.ps1') -Configuration $configuration -Attempt 3 -SkipBuild
}
if ((& git -C $root rev-parse HEAD).Trim() -cne $expectedHead -or @(& git -C $root status --porcelain).Count) {
    throw 'Source changed during final native validation.'
}
Write-Output 'PASS all three final local profiles: 20 selected form/quiet/embedded cases and four full foreground suites each.'
