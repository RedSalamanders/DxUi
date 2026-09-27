$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$evidence = 'C:/RedSalamander.Perf/evidence/i26-ui/grid-uia-debug-20260923'
if (Test-Path -LiteralPath $evidence) { throw 'Evidence exists; inspect before retrying.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
Set-Location -LiteralPath $repo
git rev-parse HEAD | Set-Content "$evidence/head.txt"
git diff -- Tests/Controls/DxUiTests.Accessibility.cpp | Set-Content "$evidence/witness.patch"
Copy-Item Tests/Controls/DxUiTests.Accessibility.cpp "$evidence/DxUiTests.Accessibility.cpp"
Get-FileHash Tests/Controls/DxUiTests.Accessibility.cpp,src/Controls/DxUi.Grid.cpp,include/DxUi/DxUi.h | ConvertTo-Json | Set-Content "$evidence/inputs.json"
& ./build.ps1 -Configuration Debug -Platform x64 -Rebuild *> "$evidence/build.log"
$buildExit = $LASTEXITCODE
Set-Content "$evidence/build-exit.txt" $buildExit
Get-Content "$evidence/build.log" -Tail 10
if ($buildExit -ne 0) { exit $buildExit }
# This supplemental test runs the standard automatic benchmark but does not
# replace the original paired resource gate or the required final six profiles.
$testStart = [DateTime]::UtcNow
try { & ./test.ps1 -Configuration Debug -Platform x64 -SkipBuild -Suites Grid,Rendering,Embedded,Accessibility *> "$evidence/test.log"
$testExit = $LASTEXITCODE } catch { $_ | Out-String | Add-Content "$evidence/test.log"; $testExit = 1 }
Set-Content "$evidence/test-exit.txt" $testExit
foreach ($suite in @('Grid','Rendering','Embedded','Accessibility')) {
    $receipt = ".build/reports/$suite-x64-Debug.json"
    if (Test-Path -LiteralPath $receipt) {
        $data = Get-Content $receipt -Raw | ConvertFrom-Json
        if ([DateTime]$data.completedUtc -lt $testStart) { continue }
        if ((Get-FileHash -LiteralPath $data.executable).Hash -ne $data.sha256) { throw "Stale executable receipt: $suite" }
        Copy-Item -LiteralPath $receipt -Destination "$evidence/$suite-receipt.json"
        Copy-Item -LiteralPath $data.performanceReport -Destination $evidence
        Copy-Item -LiteralPath ($data.performanceReport + '.comparison.json') -Destination $evidence
    }
    $log = ".build/logs/test-$suite-x64-Debug.log"
    if (Test-Path -LiteralPath $log) { Copy-Item -LiteralPath $log -Destination $evidence }
}
Get-Content "$evidence/test.log" -Tail 25
exit $testExit


