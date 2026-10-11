[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Tag,
    [string[]]$Tests = @('TestFormAccessibilityEmbeddedPresentationEventsAreIdempotent'),
    [switch]$SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot $Tag
if ($Tag -notmatch '^form-diagnostics-[a-z0-9-]+$' -or (Test-Path -LiteralPath $folder)) { throw 'Require a new bounded diagnostic folder.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require a clean frozen source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $root 'build.ps1') -Platform x64 -Configuration Debug *> (Join-Path $folder 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Diagnostic build failed: $LASTEXITCODE" }
}
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
& pwsh -NoLogo -NoProfile -File (Join-Path $root 'test.ps1') -Platform x64 -Configuration Debug -SkipBuild -SkipTooling -Suites Accessibility -Tests $Tests *> (Join-Path $folder 'form-runner.log')
$nativeExit = $LASTEXITCODE
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
Assert-PerformanceMeasurementState -Before $before -After $after
[IO.File]::WriteAllText((Join-Path $folder 'after.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($pair in @(
    @('.build/reports/Accessibility-x64-Debug.filtered.json','form.receipt.txt'),
    @('.build/logs/test-Accessibility-x64-Debug.filtered.log','form.log')
)) {
    Copy-Item -LiteralPath (Join-Path $root $pair[0]) -Destination (Join-Path $folder $pair[1])
}
$receipt = Get-Content -LiteralPath (Join-Path $folder 'form.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
$performance = [string]$receipt.performanceReport
Copy-Item -LiteralPath $performance -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($performance+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
$proof = [ordered]@{purpose='form-real-client-missing-event-diagnostic'; sourceCommit=$after.sourceCommit; sourceFingerprint=$after.sourceFingerprint;
    sourceDirty=$after.sourceDirty; nativeExitCode=$nativeExit; tests=$Tests; controlSha256=$receipt.sha256;
    exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only'}
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath (Join-Path $folder 'form.log') -Tail 45
Write-Output "Form diagnostics completed with native exit $nativeExit and matched pre/post identity."
exit $nativeExit
