[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Debug','Release','ASan Debug')][string] $Configuration,
      [int] $Attempt=1, [switch] $SkipBuild)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$tag = $Configuration.Replace(' ','')
if (@(& git -C $root status --porcelain).Count) { throw 'Fresh foreground qualification requires the frozen clean source candidate.' }
$head = (& git -C $root rev-parse HEAD).Trim()
if (-not $SkipBuild) {
    $buildFolder = Join-Path $PSScriptRoot "build-$tag-attempt$Attempt"
    if (Test-Path -LiteralPath $buildFolder) { throw 'Preserve the previous build evidence.' }
    New-Item -ItemType Directory -Path $buildFolder | Out-Null
    & (Join-Path $root 'build.ps1') -Platform x64 -Configuration $Configuration *> (Join-Path $buildFolder 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Build $Configuration failed: $LASTEXITCODE" }
    Copy-Item -LiteralPath (Join-Path $root ".build/reports/performance-build/x64-$tag/build.json") -Destination (Join-Path $buildFolder 'build.receipt.txt')
}
& pwsh -NoLogo -NoProfile -File (Join-Path $PSScriptRoot 'Invoke-Integration-Focus-Native.ps1') -Configuration $Configuration -Attempt $Attempt
$exitCode = $LASTEXITCODE
& (Join-Path $PSScriptRoot 'Snapshot-Integration-Focus.ps1') -Configuration $Configuration -Attempt $Attempt -ParentExitCode $exitCode
if ($LASTEXITCODE -ne 0) { throw 'Foreground evidence review failed.' }
if ($exitCode -ne 0) { throw "Foreground run failed: $exitCode; raw attempt retained." }
if ((& git -C $root rev-parse HEAD).Trim() -cne $head -or @(& git -C $root status --porcelain).Count) { throw 'Source candidate changed during foreground qualification.' }
Write-Output "Fresh $Configuration foreground qualification completed at $head."
