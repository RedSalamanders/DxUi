[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$folder = Join-Path $PSScriptRoot 'negative-control'
if (Test-Path -LiteralPath $folder) { throw 'Preserve the existing negative control.' }
New-Item -ItemType Directory -Path $folder | Out-Null
$head = (& git -C $root rev-parse HEAD).Trim()
if ($head -cne '01e82de396c11ff869b123dd5421dea73d70825f' -or @(& git -C $root status --porcelain).Count) {
    throw 'Negative control requires the reviewed clean candidate.'
}
$source = Join-Path $root 'src/Controls/DxUi.Accessibility.cpp'
$candidate = [IO.File]::ReadAllBytes($source)
$candidateHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
[IO.File]::WriteAllBytes((Join-Path $folder 'candidate-accessibility.cpp'), $candidate)
$baseline = Join-Path $PSScriptRoot 'original-integrated-accessibility.cpp'
$negativeHash = (Get-FileHash -LiteralPath $baseline -Algorithm SHA256).Hash
$proof = [ordered]@{ purpose='negative-control-integrated-production-with-identical-new-tests'; candidateCommit=$head;
    negativeProductionCommit='a0269ddda16f9891ced22fc9b8dccda2145e10da'; candidateProductionSha256=$candidateHash;
    negativeProductionSha256=$negativeHash; arguments=@('--test-prepared-tree-events'); nativeExitCode=$null;
    artifactSha256=$null; buildExitCode=$null; identityMatched=$false; restored=$false }
try {
    [IO.File]::WriteAllBytes($source, [IO.File]::ReadAllBytes($baseline))
    $changes = @(& git -C $root status --porcelain)
    if ($changes.Count -ne 1 -or $changes[0] -cne ' M src/Controls/DxUi.Accessibility.cpp') { throw 'Unexpected source mutation before negative control.' }
    & (Join-Path $root 'build.ps1') -Platform x64 -Configuration Debug *> (Join-Path $folder 'build.log')
    $proof.buildExitCode = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "Negative-control build failed: $LASTEXITCODE" }
    Import-Module (Join-Path $root 'Tools/PerformancePolicy.psm1') -Force
    $before = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
    [IO.File]::WriteAllText((Join-Path $folder 'before.state.receipt.txt'), ((ConvertTo-Json -InputObject $before -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
    $executable = Join-Path $root '.build/x64/Debug/DxUi.EmbeddedTests.exe'
    $proof.artifactSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
    & $executable --test-prepared-tree-events *> (Join-Path $folder 'native.log')
    $proof.nativeExitCode = $LASTEXITCODE
    $after = Get-PerformanceMeasurementState -Root $root -Platform x64 -Configuration Debug
    Assert-PerformanceMeasurementState -Before $before -After $after
    [IO.File]::WriteAllText((Join-Path $folder 'after.state.receipt.txt'), ((ConvertTo-Json -InputObject $after -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
    $proof.identityMatched = $true
    if ($proof.nativeExitCode -eq 0) { throw 'The pre-fix production unexpectedly passed the new regression; investigate before claiming reproduction.' }
} finally {
    [IO.File]::WriteAllBytes($source, $candidate)
    if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -cne $candidateHash) { throw 'Candidate source restoration changed bytes.' }
    $proof.restored = $true
    [IO.File]::WriteAllText((Join-Path $folder 'root-review.receipt.txt'), ((ConvertTo-Json -InputObject $proof -Depth 32)+[char]10), [Text.UTF8Encoding]::new($false))
}
if (@(& git -C $root status --porcelain).Count) { throw 'Negative-control restoration did not recover the clean candidate.' }
Write-Output "Negative control failed as expected: exit $($proof.nativeExitCode); identity matched; candidate restored exactly."
Get-Content -LiteralPath (Join-Path $folder 'native.log') -Tail 18
