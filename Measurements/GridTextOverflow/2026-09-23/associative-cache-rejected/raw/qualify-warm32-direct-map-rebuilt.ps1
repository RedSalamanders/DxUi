$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-associative-20260923/warm32-direct-map-rebuilt'
Set-Location -LiteralPath $repo
if ((& git rev-parse --short=7 HEAD).Trim() -ne '0ae8362') { throw 'Grid qualification source changed.' }
$grid = Get-Content -LiteralPath 'src/Controls/DxUi.Grid.cpp' -Raw
if ($grid -notmatch '% cacheSlots') { throw 'Expected original direct-map policy.' }
if (Test-Path -LiteralPath $evidence) { throw 'Refusing to overwrite an existing experiment.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
$paths = @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Controls/DxUiTests.Rendering.cpp','Tests/Controls/DxUiTestHelpers.h')
$inputs = foreach ($path in $paths) {
    $copy = Join-Path "$evidence/source" $path
    New-Item -ItemType Directory -Path (Split-Path $copy) -Force | Out-Null
    Copy-Item -LiteralPath $path -Destination $copy
    [ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$inputs | ConvertTo-Json | Set-Content -LiteralPath "$evidence/inputs.json"
& ./build.ps1 -Configuration Release -Platform x64 -Rebuild *> "$evidence/build.log"
$buildExit=$LASTEXITCODE
Set-Content -LiteralPath "$evidence/build-exit.txt" $buildExit
if ($buildExit -ne 0) { Get-Content "$evidence/build.log" -Tail 20; exit $buildExit }
try {
    & ./test.ps1 -Configuration Release -Platform x64 -SkipBuild -Suites Grid,Embedded,Rendering *> "$evidence/tests.log"
    $testExit=$LASTEXITCODE
} catch {
    $testExit=1
    $_ | Out-String | Add-Content -LiteralPath "$evidence/tests.log"
}
Set-Content -LiteralPath "$evidence/tests-exit.txt" $testExit
foreach ($suite in @('Grid','Embedded','Rendering')) {
    Copy-Item -LiteralPath ".build/reports/$suite-x64-Release.json" -Destination $evidence
    Copy-Item -LiteralPath ".build/logs/test-$suite-x64-Release.log" -Destination $evidence
}
$receipt=Get-Content -LiteralPath '.build/reports/Rendering-x64-Release.json' -Raw | ConvertFrom-Json
Copy-Item -LiteralPath $receipt.performanceReport -Destination "$evidence/performance.json"
Copy-Item -LiteralPath ($receipt.performanceReport+'.comparison.json') -Destination "$evidence/performance.comparison.json"
Get-Content -LiteralPath "$evidence/tests.log" -Tail 20
exit $testExit
