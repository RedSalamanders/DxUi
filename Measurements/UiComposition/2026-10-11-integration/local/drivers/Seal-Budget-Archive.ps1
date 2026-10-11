[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$packet = Join-Path $root 'Measurements/UiComposition/2026-10-11-integration'
$sealed = 0
foreach ($line in [IO.File]::ReadAllLines((Join-Path $packet 'SHA256SUMS'))) {
    if ($line -notmatch '^(?<hash>[A-Fa-f0-9]{64})  (?<path>.+)$') { throw 'Malformed previous integration seal.' }
    $expected = $Matches['hash']
    $relative = $Matches['path']
    if ($relative -ceq 'README.md') { continue }
    $path = [IO.Path]::GetFullPath((Join-Path $packet $relative))
    if (-not $path.StartsWith([IO.Path]::GetFullPath($packet)+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Previous seal path escapes the packet.' }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $expected) { throw "Historical sealed raw bytes changed: $relative" }
    $sealed++
}
foreach ($receiptPath in @('local/copy.receipt.txt','formatted-copy.receipt.txt','budget-copy.receipt.txt')) {
    $absoluteReceipt = Join-Path $packet $receiptPath
    $bindings = Get-Content -LiteralPath $absoluteReceipt -Raw | ConvertFrom-Json -AsHashtable -Depth 24
    foreach ($binding in $bindings) {
        $copy = Join-Path (Split-Path -Parent $absoluteReceipt) $binding.copy
        if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ine $binding.sha256 -or
            (Get-Item -LiteralPath $copy).Length -ne $binding.bytes) { throw "Original-copy binding failed: $($binding.copy)" }
        if ($receiptPath -ceq 'budget-copy.receipt.txt' -and
            ((Get-FileHash -LiteralPath $binding.original -Algorithm SHA256).Hash -ine $binding.sha256 -or
             (Get-Item -LiteralPath $binding.original).Length -ne $binding.bytes)) { throw "Fresh original changed: $($binding.original)" }
    }
    Write-Output "PASS all $($bindings.Count) retained original-copy bindings in $receiptPath."
}
$lines = @(foreach ($file in @(Get-ChildItem -LiteralPath $packet -File -Recurse | Where-Object { $_.FullName -cne (Join-Path $packet 'SHA256SUMS').Replace('/','\') } | Sort-Object FullName)) {
    $relative = [IO.Path]::GetRelativePath($packet,$file.FullName).Replace('\','/')
    "$(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256 | Select-Object -ExpandProperty Hash)  $relative"
})
if (@($lines | Where-Object { $_ -match '  SHA256SUMS$' }).Count) { throw 'Do not seal the seal itself.' }
[IO.File]::WriteAllText((Join-Path $packet 'SHA256SUMS'), (($lines -join [char]10)+[char]10), [Text.UTF8Encoding]::new($false))
foreach ($line in $lines) {
    if ($line -notmatch '^(?<hash>[A-Fa-f0-9]{64})  (?<path>.+)$') { throw 'Malformed new seal.' }
    $expected = $Matches['hash']
    if ((Get-FileHash -LiteralPath (Join-Path $packet $Matches['path']) -Algorithm SHA256).Hash -ine $expected) { throw 'New seal verification failed.' }
}
$budget = Join-Path $root 'Measurements/CiBudget/2026-10-11-arm64-asan'
foreach ($line in [IO.File]::ReadAllLines((Join-Path $budget 'SHA256SUMS'))) {
    if ($line -notmatch '^(?<hash>[A-Fa-f0-9]{64})  (?<path>.+)$') { throw 'Malformed CI-budget seal.' }
    $expected = $Matches['hash']
    if ((Get-FileHash -LiteralPath (Join-Path $budget $Matches['path']) -Algorithm SHA256).Hash -ine $expected) { throw 'Historical CI-budget bytes changed.' }
}
Write-Output "PASS $sealed unchanged historical entries, $($lines.Count) final integration entries and the historical CI-budget seal."
