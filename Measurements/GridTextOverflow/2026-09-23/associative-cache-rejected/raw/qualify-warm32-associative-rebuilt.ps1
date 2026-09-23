$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$experiment = 'C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-associative-20260923'
$baseline = "$experiment/warm32-direct-map-rebuilt"
$evidence = "$experiment/warm32-associative-rebuilt"
Set-Location -LiteralPath $repo
if ((& git rev-parse --short=7 HEAD).Trim() -ne '0ae8362') { throw 'Grid qualification source changed.' }
if (Test-Path -LiteralPath $evidence) { throw 'Refusing to overwrite an existing experiment.' }
$baseInputs = Get-Content -LiteralPath "$baseline/inputs.json" -Raw | ConvertFrom-Json
foreach ($inputFile in $baseInputs) {
    if ((Get-FileHash -LiteralPath $inputFile.path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputFile.sha256) { throw "Original source drift: $($inputFile.path)" }
}
if (!(Test-Path -LiteralPath "$baseline/performance.json")) { throw 'Missing paired original performance receipt.' }
$saved = Get-Content -LiteralPath "$experiment/warm-candidate-inputs.json" -Raw | ConvertFrom-Json
foreach ($path in @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h')) {
    $source = Join-Path "$experiment/warm-candidate-source" (Split-Path $path -Leaf)
    $expected = @($saved | Where-Object path -EQ $path)
    if ($expected.Count -ne 1 -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $expected[0].sha256) { throw "Saved candidate mismatch: $path" }
}
New-Item -ItemType Directory -Path $evidence | Out-Null
foreach ($path in @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h')) {
    Copy-Item -LiteralPath (Join-Path "$experiment/warm-candidate-source" (Split-Path $path -Leaf)) -Destination $path
}
# The newer thirty-two-cell fixture and matching benchmark instrumentation are unchanged.
$inputs = foreach ($inputFile in $baseInputs) {
    $copy = Join-Path "$evidence/source" $inputFile.path
    New-Item -ItemType Directory -Path (Split-Path $copy) -Force | Out-Null
    Copy-Item -LiteralPath $inputFile.path -Destination $copy
    $hash = (Get-FileHash -LiteralPath $inputFile.path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($inputFile.path -notin @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h') -and $hash -ne $inputFile.sha256) { throw 'Fixture changed between policies.' }
    [ordered]@{path=$inputFile.path;sha256=$hash}
}
$inputs | ConvertTo-Json | Set-Content -LiteralPath "$evidence/inputs.json"
& ./build.ps1 -Configuration Release -Platform x64 -Rebuild *> "$evidence/build.log"
$buildExit = $LASTEXITCODE
Set-Content -LiteralPath "$evidence/build-exit.txt" $buildExit
if ($buildExit -ne 0) { Get-Content "$evidence/build.log" -Tail 20; exit $buildExit }
try {
    & ./test.ps1 -Configuration Release -Platform x64 -SkipBuild -Suites Grid,Embedded,Rendering -PerformanceBaseline "$baseline/performance.json" *> "$evidence/tests.log"
    $testExit = $LASTEXITCODE
} catch {
    $testExit = 1
    $_ | Out-String | Add-Content -LiteralPath "$evidence/tests.log"
}
Set-Content -LiteralPath "$evidence/tests-exit.txt" $testExit
foreach ($suite in @('Grid','Embedded','Rendering')) {
    Copy-Item -LiteralPath ".build/reports/$suite-x64-Release.json" -Destination $evidence
    Copy-Item -LiteralPath ".build/logs/test-$suite-x64-Release.log" -Destination $evidence
}
$receipt = Get-Content -LiteralPath '.build/reports/Rendering-x64-Release.json' -Raw | ConvertFrom-Json
Copy-Item -LiteralPath $receipt.performanceReport -Destination "$evidence/performance.json"
Copy-Item -LiteralPath ($receipt.performanceReport+'.comparison.json') -Destination "$evidence/performance.comparison.json"
Get-Content -LiteralPath "$evidence/tests.log" -Tail 20
exit $testExit
