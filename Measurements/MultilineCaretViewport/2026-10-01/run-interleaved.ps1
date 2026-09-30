$ErrorActionPreference = 'Stop'
$evidence = $PSScriptRoot
$workspace = 'C:/Users/eric/.codex/worktrees/fileops-ui-qualified/DxUi'
Push-Location $workspace
try {
    foreach ($configuration in @('Debug','Release','ASan Debug')) {
        $index = 0
        foreach ($variant in @('A','B','B','A','A','B','B','A')) {
            ++$index
            $exe = "$evidence/variants/$variant/$configuration/DxUi.EmbeddedTests.exe"
            $output = "$evidence/interleaved/$configuration-$index-$variant.json"
            & $exe --benchmark $output *> ($output + '.log')
            if ($LASTEXITCODE -ne 0) { throw "Benchmark failed: $configuration/$index/$variant" }
            $receipt = Get-Content -Raw $output | ConvertFrom-Json -AsHashtable
            $templatePath = if ($variant -eq 'A') { "$evidence/baseline/$configuration.json" } else { "$evidence/variant-B-$configuration.json" }
            $template = Get-Content -Raw $templatePath | ConvertFrom-Json -AsHashtable
            foreach ($key in $template.Keys) {
                if (-not $receipt.ContainsKey($key)) { $receipt[$key] = $template[$key] }
            }
            $receipt.executableSha256 = (Get-FileHash $exe -Algorithm SHA256).Hash
            $receipt.completedUtc = [DateTime]::UtcNow.ToString('o')
            $receipt.sourceDirty = $true
            $receipt.variant = $variant
            $receipt.seriesIndex = $index
            $receipt | ConvertTo-Json -Depth 12 | Set-Content $output -Encoding utf8
            Write-Output "Completed $configuration $index $variant"
        }
    }
} finally { Pop-Location }
