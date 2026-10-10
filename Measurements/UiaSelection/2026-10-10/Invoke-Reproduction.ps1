# Fixed, nonactivating reproduction inventory; failures are retained and never replaced.
[CmdletBinding()]
param([Parameter(Mandatory)][string] $RepositoryRoot, [Parameter(Mandatory)][string] $OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Use a fresh UIA output directory; preserve previous evidence.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformancePolicy.psm1') -Force
Import-Module (Join-Path $RepositoryRoot 'Tools/PerformanceProfiling.psm1') -Force
$before = Get-ProfilingMeasurementState -Root $RepositoryRoot -Platform x64 -Configuration 'ASan Debug'
$exe = Join-Path $RepositoryRoot '.build/x64/ASan Debug/DxUi.ControlTests.exe'
$exeHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
$test = 'TestSingleGridWindowRowAndCellEventsReachAClientSubscribedToTheWindow'
$prefix = @('TestSingleTreeWindowItemEventsReachAClientSubscribedToTheWindow',
    'TestTreeBesideAnotherControlItemEventsReachAClientSubscribedToTheWindow', $test)
$started = [DateTime]::UtcNow
$plan = [ordered]@{ purpose='bounded-uia-selection-investigation'; configuration='ASan Debug'; platform='x64';
    reportBudget=11; isolatedRuns=10; precedingTreePrefixRuns=1; executable=$exe; executableSha256=$exeHash;
    anchor=$before; startedUtc=$started.ToString('o'); originalFailureCause='unproven';
    notes='Eleven serial fresh nonactivating processes, ten isolated Grid tests then one Tree/Tree/Grid prefix. Existing semantic event assertions and 500 ms quiet duration remain. Each process has a 120-second owned-child limit and each test a 60-second native watchdog. No replacement runs or focus takeover.' }
$encoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'plan.receipt.txt'), ((ConvertTo-Json $plan -Depth 32) + "`n"), $encoding)
$results = [Collections.Generic.List[object]]::new()
for ($index = 0; $index -lt 11; $index++) {
    $kind = if ($index -eq 10) { 'preceding-tree-prefix' } else { 'isolated' }
    $selected = if ($index -eq 10) { $prefix -join ',' } else { $test }
    $stem = 'uia-{0:d2}-{1}' -f ($index + 1), $kind
    Write-Host "UIA reproduction $($index + 1)/11: $kind"
    $child = Invoke-OwnedProfilingProcess -Executable $exe -WorkingDirectory $RepositoryRoot -Arguments @(
        '--suite=Accessibility','--no-activate',"--test=$selected",'--test-timeout=60')
    $logPath = Join-Path $OutputDirectory ($stem + '.log')
    [IO.File]::WriteAllText($logPath, ($child.stdout + $child.stderr), $encoding)
    $metadata = [ordered]@{}
    foreach ($key in $child.Keys) { if ($key -notin @('stdout','stderr')) { $metadata[$key] = $child[$key] } }
    $metadata['logSha256'] = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
    $metadata['executableSha256'] = $exeHash
    $metadata['inventoryIndex'] = $index + 1
    $metadata['kind'] = $kind
    $metadataPath = Join-Path $OutputDirectory ($stem + '.receipt.txt')
    [IO.File]::WriteAllText($metadataPath, ((ConvertTo-Json $metadata -Depth 8) + "`n"), $encoding)
    $results.Add([ordered]@{ index=$index + 1; kind=$kind; receipt=$stem + '.receipt.txt'; log=$stem + '.log';
        exitCode=$child.exitCode; timedOut=$child.timedOut; terminationError=$child.terminationError; drainError=$child.drainError })
}
$identityError = ''
try {
    $after = Get-ProfilingMeasurementState -Root $RepositoryRoot -Platform x64 -Configuration 'ASan Debug'
    Assert-PerformanceMeasurementState -Before $before -After $after
    if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -cne $exeHash) { throw 'UIA executable changed during reproduction.' }
} catch { $identityError = $_.Exception.Message }
$completion = [ordered]@{ scheduled=11; attempted=$results.Count; results=$results.ToArray();
    wallSeconds=([DateTime]::UtcNow - $started).TotalSeconds; identityMatched=(-not $identityError); identityError=$identityError;
    originalFailureCause='unproven'; notes='Successful repetitions do not establish the cause of the archived failure or guarantee no later repeats.' }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'completion.receipt.txt'), ((ConvertTo-Json $completion -Depth 8) + "`n"), $encoding)
if ($identityError -or @($results | Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or $_.terminationError -or $_.drainError }).Count) {
    throw 'The fixed UIA inventory contains a failure or changed identity. Retain every result; do not silently replace it.'
}
Write-Host "Completed all eleven declared UIA reproductions in $([Math]::Round($completion.wallSeconds,1)) seconds."
