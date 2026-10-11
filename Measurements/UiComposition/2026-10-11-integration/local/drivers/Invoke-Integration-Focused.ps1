[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Debug','Release','ASan Debug')][string]$Configuration,
      [Parameter(Mandatory)][string]$Tag, [switch]$SkipBuild)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot $Tag
if ($Tag -notmatch '^qualified-[a-z0-9-]+$' -or (Test-Path -LiteralPath $folder)) { throw 'Require a new bounded focused-evidence folder.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require a clean frozen source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $root 'build.ps1') -Platform x64 -Configuration $Configuration *> (Join-Path $folder 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Focused build failed: $LASTEXITCODE" }
}
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration $Configuration
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
$executable = Join-Path $root ".build/x64/$Configuration/DxUi.EmbeddedTests.exe"
& $executable --test-prepared-tree-events *> (Join-Path $folder 'embedded-events.log')
$embeddedExit = $LASTEXITCODE
& pwsh -NoLogo -NoProfile -File (Join-Path $PSScriptRoot 'Invoke-Integration-Focused-Native.ps1') -Configuration $Configuration -Folder $folder
$formExit = $LASTEXITCODE
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration $Configuration
Assert-PerformanceMeasurementState -Before $before -After $after
[IO.File]::WriteAllText((Join-Path $folder 'after.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($pair in @(
    @(".build/reports/Accessibility-x64-$Configuration.filtered.json",'form.receipt.txt'),
    @(".build/logs/test-Accessibility-x64-$Configuration.filtered.log",'form.log'),
    @(".build/reports/performance-build/x64-$($Configuration.Replace(' ',''))/build.json",'build.receipt.txt')
)) {
    Copy-Item -LiteralPath (Join-Path $root $pair[0]) -Destination (Join-Path $folder $pair[1])
}
$receipt = Get-Content -LiteralPath (Join-Path $folder 'form.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
$performance = [string]$receipt.performanceReport
Copy-Item -LiteralPath $performance -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($performance+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
$proof = [ordered]@{purpose='root-reviewed-focused-integrated-form-and-real-client-validation'; sourceCommit=$after.sourceCommit;
    sourceFingerprint=$after.sourceFingerprint; sourceDirty=$after.sourceDirty; configuration=$Configuration; platform='x64';
    embeddedArguments=@('--test-prepared-tree-events'); embeddedExitCode=$embeddedExit; formExitCode=$formExit;
    tests=@($receipt.tests); nativeSkips=@($receipt.skips).Count;
    embeddedSha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash; controlSha256=$receipt.sha256;
    exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only'}
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath (Join-Path $folder 'embedded-events.log') -Tail 12
Get-Content -LiteralPath (Join-Path $folder 'form.log') -Tail 12
if ($embeddedExit -ne 0 -or $formExit -ne 0 -or $receipt.exitCode -ne 0 -or @($receipt.skips).Count -ne 0 -or @($receipt.tests).Count -ne 14) {
    throw "Focused native run failed or partial: embedded=$embeddedExit; form=$formExit"
}
Write-Output "PASS ${Configuration}: six real-client embedded cases and fourteen form/quiet-boundary tests with exact pre/post identity."
