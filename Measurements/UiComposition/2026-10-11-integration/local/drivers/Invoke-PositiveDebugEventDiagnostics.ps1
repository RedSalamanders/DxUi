[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot 'positive-debug-event-diagnostics'
if (Test-Path -LiteralPath $folder) { throw 'Preserve previous positive evidence.' }
if (@(& git -C $root status --porcelain).Count) { throw 'Require the restored clean source candidate.' }
New-Item -ItemType Directory -Path $folder | Out-Null
& (Join-Path $root 'build.ps1') -Platform x64 -Configuration Debug *> (Join-Path $folder 'build.log')
if ($LASTEXITCODE -ne 0) { throw "Positive build failed: $LASTEXITCODE" }
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
[IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
$executable = Join-Path $root '.build/x64/Debug/DxUi.EmbeddedTests.exe'
& $executable --test-prepared-tree-events *> (Join-Path $folder 'embedded-events.log')
$nativeExit = $LASTEXITCODE
$after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
Assert-PerformanceMeasurementState -Before $before -After $after
[IO.File]::WriteAllText((Join-Path $folder 'after-events.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
if ($nativeExit -ne 0) { throw "Positive embedded events failed: $nativeExit" }
$tests = @(Get-Content -LiteralPath (Join-Path $root 'Tests/Controls/Scope.Tests.FormAccessibility.h') | ForEach-Object {
    if ($_ -cmatch '^void (TestFormAccessibility\w+)\(\);$') { $Matches[1] }
})
if ($tests.Count -ne 10) { throw 'The focused form accessibility inventory changed.' }
& (Join-Path $root 'test.ps1') -Platform x64 -Configuration Debug -SkipBuild -SkipTooling -Suites Accessibility -Tests $tests *> (Join-Path $folder 'form-runner.log')
$formExit = $LASTEXITCODE
Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
$final = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
Assert-PerformanceMeasurementState -Before $before -After $final
[IO.File]::WriteAllText((Join-Path $folder 'after-forms.state.receipt.txt'), ((ConvertTo-Json -InputObject $final -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($pair in @(
    @('.build/reports/Accessibility-x64-Debug.filtered.json','form.receipt.txt'),
    @('.build/logs/test-Accessibility-x64-Debug.filtered.log','form.log'),
    @('.build/reports/performance-build/x64-Debug/build.json','performance-build.receipt.txt')
)) {
    Copy-Item -LiteralPath (Join-Path $root $pair[0]) -Destination (Join-Path $folder $pair[1])
}
$receipt = Get-Content -LiteralPath (Join-Path $folder 'form.receipt.txt') -Raw | ConvertFrom-Json -AsHashtable -Depth 32
$performance = [string]$receipt.performanceReport
Copy-Item -LiteralPath $performance -Destination (Join-Path $folder 'canonical-performance.json')
Copy-Item -LiteralPath ($performance+'.comparison.json') -Destination (Join-Path $folder 'canonical-performance.comparison.receipt.txt')
if ($formExit -ne 0 -or $receipt.exitCode -ne 0 -or @($receipt.skips).Count -ne 0 -or @($receipt.tests).Count -ne 10) {
    throw "Positive form accessibility failed or partial: $formExit"
}
$proof = [ordered]@{ purpose='root-reviewed-integrated-focused-functional-validation'; sourceCommit=$final.sourceCommit;
    sourceFingerprint=$final.sourceFingerprint; sourceDirty=$final.sourceDirty; configuration='Debug'; platform='x64';
    embeddedArguments=@('--test-prepared-tree-events'); embeddedExitCode=$nativeExit; formExitCode=$formExit;
    formTests=$tests; formNativeSkips=@($receipt.skips).Count; embeddedSha256=(Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash;
    controlSha256=$receipt.sha256; exactSourceDependencyToolchainArtifactEnvironmentMatched=$true; performanceAcceptance='unpaired-observation-only' }
[IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
Write-Output 'PASS: six embedded real-client event cases and ten focused form accessibility tests; exact pre/post identity matched.'
Get-Content -LiteralPath (Join-Path $folder 'embedded-events.log') -Tail 12
Get-Content -LiteralPath (Join-Path $folder 'form.log') -Tail 12
