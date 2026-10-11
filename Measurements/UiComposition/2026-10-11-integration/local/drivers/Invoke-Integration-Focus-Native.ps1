[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Debug','Release','ASan Debug')][string] $Configuration,
      [Parameter(Mandatory)][int] $Attempt)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$tag = $Configuration.Replace(' ','')
$log = Join-Path $root ".build/logs/merge-s0-s2-focus-$tag-attempt$Attempt.txt"
try {
    & (Join-Path $root 'test.ps1') -Platform x64 -Configuration $Configuration -SkipBuild -SkipTooling -Interactive -InteractiveAutoStart -Suites Menu,NativeTextInput,MenuResources,MenuResourceScaling *> $log
    exit $LASTEXITCODE
} catch {
    $_ | Out-File -LiteralPath $log -Append -Encoding utf8
    exit 1
}
