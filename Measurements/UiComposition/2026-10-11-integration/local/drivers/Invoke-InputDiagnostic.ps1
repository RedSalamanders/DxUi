[CmdletBinding()]
param([Parameter(Mandatory)][string]$Tag)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot $Tag
if ($Tag -notmatch '^input-diagnostic-[a-z0-9-]+$' -or (Test-Path -LiteralPath $folder)) { throw 'Require a new bounded input diagnostic folder.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require a clean frozen source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration 'ASan Debug'
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
& pwsh -NoLogo -NoProfile -File (Join-Path $root 'test.ps1') -Platform x64 -Configuration 'ASan Debug' -SkipBuild -SkipTooling -Interactive -InteractiveAutoStart -Suites NativeTextInput -Tests TestNativeImeCommitOwnsOneUndoUnitAndLateResultPreservesApplicationText *> (Join-Path $folder 'runner.log')
$nativeExit = $LASTEXITCODE
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration 'ASan Debug'
Assert-PerformanceMeasurementState -Before $before -After $after
[IO.File]::WriteAllText((Join-Path $folder 'after.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($pair in @(@('.build/reports/NativeTextInput-x64-ASan Debug.interactive.filtered.json','input.receipt.txt'),
                    @('.build/logs/test-NativeTextInput-x64-ASan Debug.interactive.filtered.log','input.log'),
                    @('.build/logs/interactive-plan.txt','interactive-plan.txt'),
                    @('.build/logs/interactive-result.txt','interactive-result.txt'))) {
    Copy-Item -LiteralPath (Join-Path $root $pair[0]) -Destination (Join-Path $folder $pair[1])
}
$receipt = Get-Content -LiteralPath (Join-Path $folder 'input.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
Copy-Item -LiteralPath $receipt.performanceReport -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($receipt.performanceReport+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
$proof = [ordered]@{purpose='isolated-same-binary-native-IME-undo-diagnostic'; sourceCommit=$after.sourceCommit; sourceFingerprint=$after.sourceFingerprint;
    sourceDirty=$after.sourceDirty; nativeExitCode=$nativeExit; tests=@($receipt.tests); nativeSkips=@($receipt.skips).Count; controlSha256=$receipt.sha256;
    exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only'}
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath (Join-Path $folder 'input.log') -Tail 12
Get-Content -LiteralPath (Join-Path $folder 'interactive-result.txt') -Tail 12
Write-Output "Isolated IME diagnostic exit: $nativeExit"
exit $nativeExit
