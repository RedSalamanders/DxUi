Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$archive = [IO.Path]::GetFullPath((Join-Path $repo 'Measurements/CiBudget/2026-10-11-cold-build'))
$external = 'C:/Users/eric/.codex/tmp/dpf-s0-s2-merge'
$utf8 = [Text.UTF8Encoding]::new($false)
$sealPath = Join-Path $archive 'SHA256SUMS'
$originalSeal = [IO.File]::ReadAllText($sealPath)
foreach ($line in [IO.File]::ReadAllLines($sealPath)) {
    if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'Malformed original seal.' }
    if (-not [string]::Equals((Get-FileHash -LiteralPath (Join-Path $archive $Matches[2])).Hash, $Matches[1], [StringComparison]::OrdinalIgnoreCase)) { throw 'Original archive seal changed.' }
}
[IO.File]::WriteAllText((Join-Path $external 'cold-budget-archive-before-shortening.SHA256SUMS'), $originalSeal, $utf8)
$copyPath = Join-Path $archive 'copy.receipt.txt'
$receipt = [IO.File]::ReadAllText($copyPath) | ConvertFrom-Json -AsHashtable
[IO.File]::WriteAllBytes((Join-Path $external 'cold-budget-copy-before-shortening.receipt.txt'), [IO.File]::ReadAllBytes($copyPath))
$index = 0
foreach ($entry in $receipt['copies']) {
    if (-not ([string]$entry['archive']).StartsWith('native-artifacts/', [StringComparison]::Ordinal)) { continue }
    $oldRelative = [string]$entry['archive']
    $parts = $oldRelative -split '/', 3
    if ($parts.Count -ne 3) { throw 'Malformed artifact copy entry.' }
    $index++
    $extension = if ($oldRelative.EndsWith('.png', [StringComparison]::OrdinalIgnoreCase)) { '.png' } elseif ($oldRelative.EndsWith('.log', [StringComparison]::OrdinalIgnoreCase)) { '.log' } else { '.receipt.txt' }
    $newRelative = 'native-artifacts/' + $parts[1] + '/' + ('{0:d3}' -f $index) + $extension
    $oldPath = [IO.Path]::GetFullPath((Join-Path $archive $oldRelative))
    $newPath = [IO.Path]::GetFullPath((Join-Path $archive $newRelative))
    foreach ($path in @($oldPath, $newPath)) {
        if (-not $path.StartsWith($archive + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Move escapes the named archive.' }
    }
    if ((Test-Path -LiteralPath $newPath) -or (Get-Item -LiteralPath $oldPath).Length -ne $entry['bytes'] -or (Get-FileHash -LiteralPath $oldPath).Hash -cne $entry['sha256']) { throw 'Original copy changed or target exists.' }
    Move-Item -LiteralPath $oldPath -Destination $newPath
    if ((Get-Item -LiteralPath $newPath).Length -ne $entry['bytes'] -or (Get-FileHash -LiteralPath $newPath).Hash -cne $entry['sha256']) { throw 'Moved evidence bytes differ.' }
    $entry['firstArchiveName'] = $oldRelative
    $entry['archive'] = $newRelative
}
if ($index -ne 99) { throw 'Expected all 99 original native artifact copies.' }
$driver = Join-Path $archive 'archive-driver.ps1'
$receipt['archiveDriverCopy'] = [ordered]@{ original = (Join-Path $external 'Archive-ColdBudget.ps1'); archive = 'archive-driver.ps1'; bytes = (Get-Item -LiteralPath $driver).Length; sha256 = (Get-FileHash -LiteralPath $driver).Hash }
if ((Get-FileHash -LiteralPath $receipt['archiveDriverCopy']['original']).Hash -cne $receipt['archiveDriverCopy']['sha256']) { throw 'Original archive driver differs.' }
$receipt['pathShortening'] = [ordered]@{ reason = 'Dependency validation rejects committed paths longer than 150 characters.'; renamedFiles = $index; originalSealSha256 = (Get-FileHash -LiteralPath (Join-Path $external 'cold-budget-archive-before-shortening.SHA256SUMS')).Hash; completedUtc = [DateTime]::UtcNow.ToString('o'); bytesChanged = $false }
[IO.File]::WriteAllText($copyPath, ($receipt | ConvertTo-Json -Depth 15), $utf8)
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $archive 'shorten-archive-driver.ps1')
$readmePath = Join-Path $archive 'README.md'
$readme = [IO.File]::ReadAllText($readmePath)
$readme += "`nArtifact leaf names are indexed to keep every committed path within the 150-character consumer checkout limit.`nThe copy receipt maps each retained name to its original path and first archive name; every original byte, length`nand hash is preserved. The separate archive driver record accounts for its exact copy outside the original 129`nsource/artifact-copy entries. The initial dependency-validation failure is retained with repair validation below.`n"
[IO.File]::WriteAllText($readmePath, $readme, $utf8)
$seal = foreach ($file in Get-ChildItem -LiteralPath $archive -File -Recurse | Where-Object { $_.FullName -cne $sealPath } | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($repo, $file.FullName).Replace('\', '/')
    if ($relative.Length -gt 150) { throw "Archive path still too long: $relative" }
    (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() + '  ' + [IO.Path]::GetRelativePath($archive, $file.FullName).Replace('\', '/')
}
[IO.File]::WriteAllText($sealPath, (($seal -join "`n") + "`n"), $utf8)
foreach ($line in [IO.File]::ReadAllLines($sealPath)) {
    if ($line -cnotmatch '^([a-f0-9]{64})  (.+)$') { throw 'New archive seal malformed.' }
    if (-not [string]::Equals((Get-FileHash -LiteralPath (Join-Path $archive $Matches[2])).Hash, $Matches[1], [StringComparison]::OrdinalIgnoreCase)) { throw 'Final indexed archive seal verification failed.' }
}
Write-Host "COLD_BUDGET_PATHS_REPAIRED: 99 files moved with unchanged original bytes; $($seal.Count) files sealed; all committed archive paths <=150 characters."
