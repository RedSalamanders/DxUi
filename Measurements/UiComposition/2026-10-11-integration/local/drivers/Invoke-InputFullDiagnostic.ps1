[CmdletBinding()]
param([Parameter(Mandatory)][string]$Tag)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot $Tag
if ($Tag -notmatch '^input-diagnostic-[a-z0-9-]+$' -or (Test-Path -LiteralPath $folder)) { throw 'Require a new bounded input diagnostic folder.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require a clean frozen source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
& (Join-Path $root 'build.ps1') -Platform x64 -Configuration 'ASan Debug' *> (Join-Path $folder 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Diagnostic build failed.' }
Copy-Item -LiteralPath (Join-Path $root '.build/reports/performance-build/x64-ASanDebug/build.json') -Destination (Join-Path $folder 'build.receipt.txt')
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration 'ASan Debug'
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
& pwsh -NoLogo -NoProfile -File (Join-Path $root 'test.ps1') -Platform x64 -Configuration 'ASan Debug' -SkipBuild -SkipTooling -Interactive -InteractiveAutoStart -Suites NativeTextInput *> (Join-Path $folder 'runner.log')
$nativeExit = $LASTEXITCODE
$after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration 'ASan Debug'
Assert-PerformanceMeasurementState -Before $before -After $after
[IO.File]::WriteAllText((Join-Path $folder 'after.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($pair in @(@('.build/reports/NativeTextInput-x64-ASan Debug.interactive.json','input.receipt.txt'),
                    @('.build/logs/test-NativeTextInput-x64-ASan Debug.interactive.log','input.log'),
                    @('.build/logs/interactive-plan.txt','interactive-plan.txt'),
                    @('.build/logs/interactive-result.txt','interactive-result.txt'))) {
    Copy-Item -LiteralPath (Join-Path $root $pair[0]) -Destination (Join-Path $folder $pair[1])
}
$receipt = Get-Content -LiteralPath (Join-Path $folder 'input.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
if ($receipt.ContainsKey('tests')) { throw 'A filtered packet cannot establish full-suite diagnostics.' }
Copy-Item -LiteralPath $receipt.performanceReport -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($receipt.performanceReport+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
$proof = [ordered]@{purpose='full-suite-native-IME-undo-passive-diagnostic'; sourceCommit=$after.sourceCommit; sourceFingerprint=$after.sourceFingerprint;
    sourceDirty=$after.sourceDirty; nativeExitCode=$nativeExit; suite='NativeTextInput'; filtered=$false; nativeSkips=@($receipt.skips).Count; controlSha256=$receipt.sha256;
    exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only'}
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath (Join-Path $folder 'input.log') -Tail 15
Get-Content -LiteralPath (Join-Path $folder 'interactive-result.txt') -Tail 12
Write-Output "Full native input diagnostic exit: $nativeExit"
exit $nativeExit
