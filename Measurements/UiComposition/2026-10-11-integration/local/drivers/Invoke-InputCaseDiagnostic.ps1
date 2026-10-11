[CmdletBinding()]
param([Parameter(Mandatory)][string]$Tag, [Parameter(Mandatory)][string]$Test, [switch]$SkipBuild)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot $Tag
if ($Tag -notmatch '^input-diagnostic-[a-z0-9-]+$' -or $Test -notmatch '^Test[A-Za-z0-9]+$' -or (Test-Path -LiteralPath $folder)) { throw 'Require a new bounded named-case diagnostic folder.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require a clean frozen source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $root 'build.ps1') -Platform x64 -Configuration 'ASan Debug' *> (Join-Path $folder 'build.log')
    if ($LASTEXITCODE -ne 0) { throw 'Diagnostic build failed.' }
    Copy-Item -LiteralPath (Join-Path $root '.build/reports/performance-build/x64-ASanDebug/build.json') -Destination (Join-Path $folder 'build.receipt.txt')
}
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $root 'Tools/InteractiveRun.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration 'ASan Debug'
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
& pwsh -NoLogo -NoProfile -File (Join-Path $root 'test.ps1') -Platform x64 -Configuration 'ASan Debug' -SkipBuild -SkipTooling -Interactive -InteractiveAutoStart -Suites NativeTextInput -Tests $Test *> (Join-Path $folder 'runner.log')
$nativeExit = $LASTEXITCODE
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
$lease = Read-DxUiLeaseResult -Text ([IO.File]::ReadAllText((Join-Path $folder 'interactive-result.txt')))
if (@($receipt.tests).Count -ne 1 -or $receipt.tests[0] -cne $Test -or $receipt.sha256 -cne (Get-FileHash -LiteralPath (Join-Path $root '.build/x64/ASan Debug/DxUi.ControlTests.exe') -Algorithm SHA256).Hash) { throw 'Filtered selection/artifact differs from the frozen diagnostic.' }
if ($lease.State -cne 'completed' -or @($lease.Children).Count -ne 1 -or -not $lease.Children[0].Launched) { throw 'The selected native case did not run in a completed lease.' }
foreach ($part in @('Foreground','Focus','Cursor')) { if ($lease.$part -cnotin @('unchanged','restored')) { throw "Unverified restoration: $part" } }
Copy-Item -LiteralPath $receipt.performanceReport -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($receipt.performanceReport+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
$performance = Get-Content -LiteralPath (Join-Path $folder 'canonical-performance.json') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
if ($performance.sourceCommit -cne $before.sourceCommit -or $performance.sourceFingerprint -cne $before.sourceFingerprint) { throw 'Stale canonical performance receipt.' }
$proof = [ordered]@{purpose='named-native-IME-undo-diagnostic'; sourceCommit=$after.sourceCommit; sourceFingerprint=$after.sourceFingerprint;
    sourceDirty=$after.sourceDirty; nativeExitCode=$nativeExit; tests=@($receipt.tests); nativeSkips=@($receipt.skips).Count; controlSha256=$receipt.sha256;
    confirmation=$lease.Confirmation; foreground=$lease.Foreground; focus=$lease.Focus; pointer=$lease.Cursor;
    exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only'}
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath (Join-Path $folder 'input.log') -Tail 15
Get-Content -LiteralPath (Join-Path $folder 'interactive-result.txt') -Tail 12
Write-Output "Named native input diagnostic exit: $nativeExit"
exit $nativeExit
