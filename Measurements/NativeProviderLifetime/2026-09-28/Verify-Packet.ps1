$ErrorActionPreference='Stop'
$root=$PSScriptRoot
function ShaFile([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Need([bool]$ok,[string]$msg){if(-not $ok){throw $msg}}
$map=Get-Content -LiteralPath (Join-Path $root 'original-file-map.json.receipt.txt') -Raw|ConvertFrom-Json
Need ($map.entry_count -eq @($map.entries).Count) 'logical count mismatch'
$payloads=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$logical=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($e in $map.entries){
 Need ($logical.Add([string]$e.original_relative)) "duplicate path: $($e.original_relative)"
 $p=Join-Path $root ([string]$e.staged)
 Need (Test-Path -LiteralPath $p -PathType Leaf) "missing payload: $($e.staged)"
 Need ((Get-Item -LiteralPath $p).Length -eq [long]$e.size_bytes -and (ShaFile $p) -eq ([string]$e.sha256).ToLowerInvariant()) "bad payload: $($e.original_relative)"
 [void]$payloads.Add([string]$e.staged)
}
Need ($payloads.Count -eq $map.unique_payload_count) 'unique payload count mismatch'
$actualRaw=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($f in Get-ChildItem (Join-Path $root 'raw') -File -Recurse){[void]$actualRaw.Add([IO.Path]::GetRelativePath($root,$f.FullName).Replace('\','/'))}
Need ($actualRaw.SetEquals($payloads)) 'raw payload set mismatch'
$lines=@(Get-Content (Join-Path $root 'SHA256SUMS'))
$expected=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($line in $lines){Need ($line -match '^([0-9a-f]{64})  (.+)$') "bad sum line: $line"; $h=$Matches[1];$r=$Matches[2];Need ($expected.Add($r)) "duplicate sum path: $r"; $p=Join-Path $root $r;Need ((Test-Path -LiteralPath $p -PathType Leaf) -and (ShaFile $p) -eq $h) "bad file: $r"}
$actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($f in Get-ChildItem $root -File -Recurse){if($f.Name -ne 'SHA256SUMS'){[void]$actual.Add([IO.Path]::GetRelativePath($root,$f.FullName).Replace('\','/'))}}
Need ($actual.SetEquals($expected)) 'exact packet membership mismatch'
Write-Output "PASS: $($map.entry_count) logical files, $($map.unique_payload_count) unique byte payloads, exact membership and SHA-256."
