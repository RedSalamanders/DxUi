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
$map=Get-Content -LiteralPath (Join-Path $root 'original-file-map.receipt') -Raw|ConvertFrom-Json
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
Require (@($map.entries|Where-Object original_relative -like 'ci-d45/*').Count -ge 350) 'CI artifact source inventory incomplete'
Require (@($map.entries|Where-Object original_relative -like 'ci-d45-diagnosis/*').Count -ge 109) 'Diagnosis source inventory incomplete'
Require (@($map.entries|Where-Object original_relative -like 'final-abba-20260927/*').Count -eq 49) 'ABBA source inventory differs'
$corr=@($map.entries|Where-Object original_relative -ceq 'correction-metadata.json')
Require ($corr.Count -eq 1) 'B1/B2 correction metadata missing'
function ReadMappedText([string]$relative){
 $row=@($map.entries|Where-Object original_relative -ceq $relative)
 Require ($row.Count -eq 1) "Mapped source missing: $relative"
 $path=Join-Path $root (([string]$row[0].staged).Replace('/',[IO.Path]::DirectorySeparatorChar))
 return [IO.File]::ReadAllText($path,[Text.Encoding]::UTF8)
}
$navigationCorrection=ReadMappedText 'ci-d45-diagnosis/paired-navigation-correction.json'|ConvertFrom-Json
Require ($navigationCorrection.excludedBaseline -eq 'paired-navigation-a') 'Invalid incremental baseline correction missing'
$pairResult=ReadMappedText 'ci-d45-diagnosis/rebuilt-navigation-pair-result.json'|ConvertFrom-Json
Require ($pairResult.baselineExit -eq 1 -and $pairResult.candidateExit -eq 0) 'Forced pair outcome differs'
foreach($name in @('rebuilt-navigation-a','rebuilt-navigation-b')){
 $build=ReadMappedText "ci-d45-diagnosis/$name/build.log"
 Require (@($build -split "`r?`n"|Where-Object {$_.Trim() -ceq 'DxUi.Accessibility.cpp'}).Count -ge 1) "Forced build did not compile Accessibility.cpp: $name"
}
Write-Output "PASS: $($map.entry_count) logical evidence files, $($map.unique_payload_count) unique byte payloads, exact membership and SHA-256."
