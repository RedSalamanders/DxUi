[CmdletBinding()]param([string]$Root,[string]$OutputDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$matrix=Get-Content (Join-Path $Root '.build/reports/rich-retained-geometry-20261018/Final02/qualification-completion.receipt.txt') -Raw|ConvertFrom-Json
if($matrix.completed -ne 6){throw 'Fixed matrix is not complete.'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Fresh artifact destination required.'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$paths=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($configuration in @('Debug','Release','ASan Debug')){
 foreach($scope in @('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')){
  $native=Get-Content -LiteralPath (Join-Path $Root ('.build/reports/'+$scope+'-x64-'+$configuration+'.json')) -Raw|ConvertFrom-Json -AsHashtable -Depth 32
  if(-not$native.Contains('suite') -or -not$native.Contains('performanceReport')){continue}
  $performance=Get-Content -LiteralPath $native.performanceReport -Raw|ConvertFrom-Json
  if($native.exitCode -ne 0 -or $performance.sourceFingerprint -cne '4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C'){throw 'Native artifact is not from the final qualified source.'}
  $paths[$native.executable]=$native.sha256
 }
 $directory=Join-Path $Root ('.build/x64/'+$configuration)
 foreach($file in Get-ChildItem -LiteralPath $directory -File|Where-Object Extension -In @('.dll','.pdb')){
  $paths[$file.FullName]=(Get-FileHash -LiteralPath $file.FullName).Hash
 }
}
$asan=Get-Content (Join-Path $Root '.build/reports/AddressSanitizer-x64.json') -Raw|ConvertFrom-Json -AsHashtable
if($asan.Contains('executable')){$paths[$asan.executable]=(Get-FileHash -LiteralPath $asan.executable).Hash}
$copies=[Collections.Generic.List[object]]::new()
foreach($entry in $paths.GetEnumerator()|Sort-Object Key){
 if((Get-FileHash -LiteralPath $entry.Key).Hash -cne $entry.Value){throw 'Artifact changed after its successful run.'}
 $relative=[IO.Path]::GetRelativePath((Join-Path $Root '.build/x64'),$entry.Key)
 if($relative.StartsWith('..')){throw 'Native artifact escaped x64 outputs.'}
 $destination=Join-Path $OutputDirectory $relative
 New-Item -ItemType Directory -Path (Split-Path $destination) -Force|Out-Null
 Copy-Item -LiteralPath $entry.Key -Destination $destination
 if((Get-FileHash -LiteralPath $destination).Hash -cne $entry.Value){throw 'Private artifact copy changed bytes.'}
 $copies.Add(@{original=$entry.Key;copy=$destination;sha256=$entry.Value;bytes=(Get-Item -LiteralPath $entry.Key).Length})
}
@{classification='Private exact final native binaries, matching symbols/deployed DLLs and deliberate ASan probe. Never publish ordinary build products in Measurements.';
 copies=$copies.ToArray();fileCount=$copies.Count}|ConvertTo-Json -Depth 10|Set-Content ($OutputDirectory+'.receipt.txt')
Write-Host "Preserved $($copies.Count) private qualified artifact files."
