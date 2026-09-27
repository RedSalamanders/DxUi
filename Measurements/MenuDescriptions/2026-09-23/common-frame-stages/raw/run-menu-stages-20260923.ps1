$ErrorActionPreference = 'Stop'
$evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/menu-frame-stages-20260923'
if (Test-Path -LiteralPath $evidence) { throw 'Evidence already exists.' }
New-Item -ItemType Directory $evidence | Out-Null
$repos = @{'A'='Z:/src/DxUi-worktrees/i26-menu-intern-baseline'; 'B'='Z:/src/DxUi-worktrees/i26-menu-description'}
foreach ($variant in @('A','B')) {
    Set-Location -LiteralPath $repos[$variant]
    $folder = Join-Path $evidence $variant
    New-Item -ItemType Directory $folder | Out-Null
    git rev-parse HEAD | Set-Content "$folder/head.txt"
    git diff -- Tests/Embedded | Set-Content "$folder/instrumentation.patch"
    $inputs = @('Tests/Embedded/BenchmarkMain.h','Tests/Embedded/ComplexUiBenchmark.h','Tests/Embedded/ComplexUiFrameStages.h','Samples/ComplexUi/ComplexUiScene.h','Samples/EmbeddedControls/GraphicsFixture.h')
    $manifest = foreach ($file in $inputs) {
        $target = Join-Path "$folder/inputs" $file
        New-Item -ItemType Directory (Split-Path $target) -Force | Out-Null
        Copy-Item -LiteralPath $file -Destination $target
        [ordered]@{path=$file;sha256=(Get-FileHash $file).Hash.ToLowerInvariant()}
    }
    $manifest | ConvertTo-Json | Set-Content "$folder/inputs.json"
    & ./build.ps1 -Configuration Release -Platform x64 *> "$folder/build.log"
    $buildExit = $LASTEXITCODE
    Set-Content "$folder/build-exit.txt" $buildExit
    if ($buildExit -ne 0) { Get-Content "$folder/build.log" -Tail 25; exit $buildExit }
    Get-FileHash .build/x64/Release/DxUi.EmbeddedTests.exe,.build/x64/Release/DxUi.lib | ConvertTo-Json | Set-Content "$folder/binary-hashes.json"
}
if ((Get-Content "$evidence/A/inputs.json" -Raw) -cne (Get-Content "$evidence/B/inputs.json" -Raw)) { throw 'Diagnostic fixture drift' }
foreach ($run in @('A1','A2','B1','B2','A3')) {
    $variant = $run.Substring(0,1)
    Set-Location -LiteralPath $repos[$variant]
    & ./.build/x64/Release/DxUi.EmbeddedTests.exe --benchmark-frame-stages "$evidence/$run.json" *> "$evidence/$run.log"
    $runExit = $LASTEXITCODE
    Set-Content "$evidence/$run-exit.txt" $runExit
    if ($runExit -ne 0) { Get-Content "$evidence/$run.log" -Tail 25; exit $runExit }
    Write-Output "$run complete; diagnostic stages only."
}
