$ErrorActionPreference = 'Stop'
$evidenceRoot = 'C:/RedSalamander.Perf/evidence/i26-ui/menu-heap-phases-20260923'
$variants = [ordered]@{
    baseline = 'Z:/src/DxUi-worktrees/i26-menu-intern-baseline'
    candidate = 'Z:/src/DxUi-worktrees/i26-menu-description'
}
foreach ($entry in $variants.GetEnumerator()) {
    $repo = $entry.Value
    $evidence = Join-Path $evidenceRoot $entry.Key
    Set-Location -LiteralPath $repo
    foreach ($inputFile in (Get-Content "$evidence/inputs.json" -Raw | ConvertFrom-Json)) {
        if ((Get-FileHash -LiteralPath (Join-Path $repo $inputFile.path)).Hash.ToLowerInvariant() -ne $inputFile.sha256) { throw "Changed input $($inputFile.path)" }
    }
    if ((git rev-parse HEAD).Trim() -ne (Get-Content "$evidence/head.txt" -Raw).Trim()) { throw 'Changed source HEAD' }
    & ./build.ps1 -Configuration Release -Platform x64 -Rebuild *> "$evidence/build.log"
    $buildExit = $LASTEXITCODE
    Set-Content "$evidence/build-exit.txt" $buildExit
    Get-Content "$evidence/build.log" -Tail 12
    if ($buildExit -ne 0) { exit $buildExit }
    Get-FileHash ./.build/x64/Release/DxUi.EmbeddedTests.exe,./.build/x64/Release/DxUi.lib | ConvertTo-Json | Set-Content "$evidence/binary-hashes.json"
}
# All build work completes before the serial diagnostic pair. These instrumented
# timings are not acceptance benchmarks, and no global tracing is enabled.
foreach ($entry in $variants.GetEnumerator()) {
    $repo = $entry.Value
    $evidence = Join-Path $evidenceRoot $entry.Key
    Set-Location -LiteralPath $repo
    & ./.build/x64/Release/DxUi.EmbeddedTests.exe --benchmark-heap-phases "$evidence/report.json" "$evidence/heaps.jsonl" *> "$evidence/probe.log"
    $probeExit = $LASTEXITCODE
    Set-Content "$evidence/probe-exit.txt" $probeExit
    if ($probeExit -ne 0) { Get-Content "$evidence/probe.log" -Tail 25; exit $probeExit }
    $rows = @(Get-Content "$evidence/heaps.jsonl" | ForEach-Object { $_ | ConvertFrom-Json })
    if ($rows.Count -ne 9) { throw 'Expected exactly nine ordered phase records.' }
    for ($index = 0; $index -lt $rows.Count; ++$index) {
        if ($rows[$index].sample -ne $index -or @($rows[$index].heaps | Where-Object error -ne 0).Count -ne 0) { throw "Invalid heap census at $index" }
    }
    Write-Output "$($entry.Key): nine valid phase records; timings are diagnostic only."
}
