$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-associative-20260923'
Set-Location -LiteralPath $repo
if ((& git rev-parse --short=7 HEAD).Trim() -ne '0ae8362') { throw 'Grid qualification source changed.' }
$grid = Get-Content -LiteralPath 'src/Controls/DxUi.Grid.cpp' -Raw
if ($grid -notmatch 'previousAge' -or $grid -notmatch 'layoutCreationAttempts') { throw 'Expected instrumented associative candidate.' }
$inputs = @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h','Tests/Embedded/ComplexUiBenchmark.h') | ForEach-Object {
    [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$inputs | ConvertTo-Json | Set-Content -LiteralPath "$evidence/associative-inputs.json"
& ./build.ps1 -Configuration Release -Platform x64 *> "$evidence/associative-build.log"
if ($LASTEXITCODE -ne 0) { throw "Baseline build failed: $LASTEXITCODE" }
& ./performance.ps1 -Configuration Release -Platform x64 -Scenario MultilineGridHeap -SkipBuild -OutputPath "$evidence/associative-heap.json" -Baseline "$evidence/direct-map-heap.json" *> "$evidence/associative-heap.log"
$performanceExit = $LASTEXITCODE
Set-Content -LiteralPath "$evidence/associative-heap-exit.txt" $performanceExit
Get-Content -LiteralPath "$evidence/associative-heap.log" -Tail 12
if (-not (Test-Path -LiteralPath "$evidence/associative-heap.json")) { throw 'Baseline report is missing.' }
$report = Get-Content -LiteralPath "$evidence/associative-heap.json" -Raw | ConvertFrom-Json
$report.gridTextCache | ConvertTo-Json -Compress
exit $performanceExit
