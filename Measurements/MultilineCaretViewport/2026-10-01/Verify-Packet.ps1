$ErrorActionPreference = 'Stop'
$packet = [IO.Path]::GetFullPath($PSScriptRoot)
$expected = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $packet 'SHA256SUMS')) {
    if ($line -notmatch '^([0-9A-F]{64})  (.+)$') { throw "Invalid manifest row: $line" }
    $relative = $Matches[2]
    $hash = $Matches[1]
    $path = [IO.Path]::GetFullPath((Join-Path $packet $relative))
    if (-not $path.StartsWith($packet + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest path escapes packet: $relative"
    }
    if ($expected.ContainsKey($relative)) { throw "Duplicate packet member: $relative" }
    $expected[$relative] = $hash
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $relative" }
}
$actual = @(Get-ChildItem -LiteralPath $packet -Recurse -File | Where-Object Name -ne 'SHA256SUMS')
if ($actual.Count -ne $expected.Count) { throw 'Packet membership count differs from manifest.' }
foreach ($file in $actual) {
    $relative = [IO.Path]::GetRelativePath($packet, $file.FullName).Replace('\', '/')
    if (-not $expected.ContainsKey($relative)) { throw "Unexpected packet member: $relative" }
}
Write-Output "Verified $($expected.Count) packet files."
