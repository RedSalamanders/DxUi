param([ValidateSet('Release','Debug','ASan Debug')][string]$Configuration)
$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
$evidence = Join-Path $PSScriptRoot $Configuration.Replace(' ','-').ToLowerInvariant()
if (Test-Path -LiteralPath $evidence) { throw 'Preserve the existing attempt; use a new attempt directory.' }
New-Item -ItemType Directory -Path $evidence | Out-Null
Set-Location -LiteralPath $repo
foreach ($inputFile in (Get-Content -LiteralPath "$PSScriptRoot/candidate-inputs.json" -Raw | ConvertFrom-Json)) {
    if ((Get-FileHash -LiteralPath (Join-Path $repo $inputFile.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputFile.sha256) {
        throw "Frozen input changed: $($inputFile.path)"
    }
}
$started = [DateTimeOffset]::UtcNow
$code = 0
try {
    # The default Menu/NativeTextInput suites take focus; this source change uses only nonactivating suites.
    $global:LASTEXITCODE = 0
    & ./test.ps1 -Configuration $Configuration -Platform x64 -Suites @('Grid','Rendering','Embedded','Accessibility') *> "$evidence/test.log"
    $code = $LASTEXITCODE
} catch {
    $_ | Out-String | Add-Content -LiteralPath "$evidence/test.log"
    $code = 1
}
$ended = [DateTimeOffset]::UtcNow
$copied = @()
$performancePaths = @()
foreach ($suite in @('Grid','Rendering','Embedded','Accessibility','AddressSanitizer')) {
    $name = if ($suite -eq 'AddressSanitizer') { 'AddressSanitizer-x64' } else { "$suite-x64-$Configuration" }
    $receiptPath = Join-Path "$repo/.build/reports" "$name.json"
    if (-not (Test-Path -LiteralPath $receiptPath)) { continue }
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    if ([DateTimeOffset]::Parse($receipt.completedUtc) -lt $started) { continue } # Never copy stale previous-suite passes.
    if ($receipt.configuration -ne $Configuration) { throw 'Fresh receipt profile mismatch.' }
    if ((Get-FileHash -LiteralPath $receipt.executable -Algorithm SHA256).Hash -ne $receipt.sha256) { throw 'Receipt binary changed.' }
    Copy-Item -LiteralPath $receiptPath -Destination "$evidence/$suite-receipt.json"
    $suiteLog = if ($suite -eq 'AddressSanitizer') { $receipt.log } else { "$repo/.build/logs/test-$suite-x64-$Configuration.log" }
    Copy-Item -LiteralPath $suiteLog -Destination "$evidence/$suite.log"
    $copied += $suite
    if ($suite -ne 'AddressSanitizer') { $performancePaths += $receipt.performanceReport }
}
foreach ($performancePath in ($performancePaths | Select-Object -Unique)) {
    Copy-Item -LiteralPath $performancePath -Destination $evidence
    Copy-Item -LiteralPath "$performancePath.comparison.json" -Destination $evidence
}
[ordered]@{configuration=$Configuration;platform='x64';startedUtc=$started.ToString('o');completedUtc=$ended.ToString('o');exit=$code;freshSuites=$copied;performance='unpaired; functional witness only'} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$evidence/outcome.json"
Get-Content -LiteralPath "$evidence/test.log" -Tail 16
exit $code
