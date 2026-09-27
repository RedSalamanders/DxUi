$ErrorActionPreference = 'Stop'
$repo = 'Z:/src/DxUi-worktrees/i26-grid-line-clamp'
Set-Location -LiteralPath $repo
foreach ($configuration in @('Release','Debug','ASan Debug')) {
    $evidence = Join-Path $PSScriptRoot ('arm64-' + $configuration.Replace(' ','-').ToLowerInvariant())
    if (Test-Path -LiteralPath $evidence) { throw 'Preserve existing cross-build attempts.' }
    New-Item -ItemType Directory -Path $evidence | Out-Null
    foreach ($inputFile in (Get-Content -LiteralPath "$PSScriptRoot/candidate-inputs.json" -Raw | ConvertFrom-Json)) {
        if ((Get-FileHash -LiteralPath (Join-Path $repo $inputFile.path) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $inputFile.sha256) { throw "Frozen input changed: $($inputFile.path)" }
    }
    $started = [DateTimeOffset]::UtcNow
    [ordered]@{startedUtc=$started.ToString('o');configuration=$configuration;platform='ARM64'} | ConvertTo-Json | Set-Content -LiteralPath "$evidence/start.json"
    $code = 0
    try {
        $global:LASTEXITCODE = 0
        & ./build.ps1 -Configuration $configuration -Platform ARM64 *> "$evidence/build.log"
        $code = $LASTEXITCODE
    } catch {
        $_ | Out-String | Add-Content -LiteralPath "$evidence/build.log"
        $code = 1
    }
    $artifacts = @()
    if ($code -eq 0) {
        foreach ($name in @('DxUi.lib','DxUi.ControlTests.exe','DxUi.EmbeddedTests.exe','DxUi.FoundationTests.exe','DxUi.EmbeddedControls.exe')) {
            $path = "$repo/.build/ARM64/$configuration/$name"
            $artifacts += [ordered]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
        }
    }
    [ordered]@{configuration=$configuration;platform='ARM64';nativeRuntimeExecuted=$false;startedUtc=$started.ToString('o');completedUtc=[DateTimeOffset]::UtcNow.ToString('o');exit=$code;artifacts=$artifacts} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$evidence/outcome.json"
    Get-Content -LiteralPath "$evidence/build.log" -Tail 8
    if ($code -ne 0) { exit $code }
}
exit 0
