$ErrorActionPreference='Stop'
$root=$PSScriptRoot
function ShaFile([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
function Require([bool]$valid,[string]$message){if(-not $valid){throw $message}}
$lines=@(Get-Content -LiteralPath (Join-Path $root 'SHA256SUMS'))
$expected=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($line in $lines){
 Require ($line -match '^([0-9a-f]{64})  (.+)$') "Malformed SHA256SUMS row: $line"
 $hash=$Matches[1];$relative=$Matches[2]
 Require ($expected.Add($relative)) "Duplicate SHA256SUMS path: $relative"
 $path=Join-Path $root ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))
 Require ((Test-Path -LiteralPath $path -PathType Leaf) -and (ShaFile $path) -eq $hash) "Missing/changed file: $relative"
}
$actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($file in (Get-ChildItem -LiteralPath $root -Recurse -File)){
 if($file.Name -eq 'SHA256SUMS'){continue}
 [void]$actual.Add([IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/'))
}
Require ($actual.SetEquals($expected)) 'Exact packet membership differs from SHA256SUMS'
$map=Get-Content -LiteralPath (Join-Path $root 'original-file-map.json.receipt.txt') -Raw|ConvertFrom-Json
Require ($map.entry_count -eq @($map.entries).Count) 'Logical entry count differs'
$staged=@($map.entries|ForEach-Object staged|Select-Object -Unique)
Require ($map.unique_payload_count -eq $staged.Count) 'Unique payload count differs'
$raw=@(Get-ChildItem -LiteralPath (Join-Path $root 'raw') -Recurse -File)
Require ($raw.Count -eq $staged.Count) 'Raw payload membership differs'
$paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($entry in $map.entries){
 Require ($paths.Add([string]$entry.original_relative)) "Duplicate logical source: $($entry.original_relative)"
 $file=Join-Path $root (([string]$entry.staged).Replace('/',[IO.Path]::DirectorySeparatorChar))
 Require ((Test-Path -LiteralPath $file -PathType Leaf) -and (Get-Item -LiteralPath $file).Length -eq [long]$entry.size_bytes -and (ShaFile $file) -eq ([string]$entry.sha256).ToLowerInvariant()) "Mapped payload differs: $($entry.original_relative)"
}
Require (@($map.entries|Where-Object original_relative -like 'profiles-navigation-final/*').Count -ge 127) 'Completed profile inventory incomplete'
Require (@($map.entries|Where-Object original_relative -like 'profiles-navigation-comparison-20260927/*').Count -ge 9) 'Comparison inventory incomplete'
Require (@($map.entries|Where-Object original_relative -ceq 'profiles-navigation-comparison-20260927/assessment.md').Count -eq 1) 'Assessment missing'
Require (@($map.entries|Where-Object original_relative -ceq 'profiles-navigation-comparison-20260927/resource-sample-summary.json').Count -eq 1) 'Resource samples missing'
Write-Output "PASS: $($map.entry_count) logical evidence files, $($map.unique_payload_count) unique byte payloads, exact membership and SHA-256."
