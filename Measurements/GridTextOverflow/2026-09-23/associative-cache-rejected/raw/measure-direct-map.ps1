$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/grid-cache-associative-20260923'
Set-Location -LiteralPath $repo
if ((& git rev-parse --short=7 HEAD).Trim() -ne '0ae8362') { throw 'Grid qualification source changed.' }
$grid = Get-Content -LiteralPath 'src/Controls/DxUi.Grid.cpp' -Raw
if ($grid -notmatch '% cacheSlots' -or $grid -notmatch 'layoutCreationAttempts') { throw 'Expected instrumented original direct-mapped implementation.' }
$inputs = @('src/Controls/DxUi.Grid.cpp','include/DxUi/DxUi.h','Tests/Embedded/ComplexUiBenchmark.h') | ForEach-Object {
    [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$inputs | ConvertTo-Json | Set-Content -LiteralPath "$evidence/direct-map-inputs.json"
& ./build.ps1 -Configuration Release -Platform x64 *> "$evidence/direct-map-build.log"
if ($LASTEXITCODE -ne 0) { throw "Baseline build failed: $LASTEXITCODE" }
& ./performance.ps1 -Configuration Release -Platform x64 -Scenario MultilineGridHeap -SkipBuild -OutputPath "$evidence/direct-map-heap.json" *> "$evidence/direct-map-heap.log"
$performanceExit = $LASTEXITCODE
Set-Content -LiteralPath "$evidence/direct-map-heap-exit.txt" $performanceExit
Get-Content -LiteralPath "$evidence/direct-map-heap.log" -Tail 12
if (-not (Test-Path -LiteralPath "$evidence/direct-map-heap.json")) { throw 'Baseline report is missing.' }
$report = Get-Content -LiteralPath "$evidence/direct-map-heap.json" -Raw | ConvertFrom-Json
$report.gridTextCache | ConvertTo-Json -Compress
exit $performanceExit
