[CmdletBinding()]param([string]$Root,[string]$Packet,[string]$ExpectedSource)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Packet){throw 'Fresh packet required; preserve previous attempts.'}
$Packet=[IO.Path]::GetFullPath($Packet)
$work=Join-Path $Root '.build/reports/rich-retained-geometry-20261018'
New-Item -ItemType Directory -Path $Packet|Out-Null
$copies=[Collections.Generic.List[object]]::new()
$destinations=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$restoreAliases=[Collections.Generic.List[object]]::new()
function Get-CompactArchiveRelative([string]$Relative,[string]$Hash){
 $original=$Relative
 $Relative=[regex]::Replace($Relative,'/synthetic-([a-f0-9]{32})/',{param($match)'/s-'+$match.Groups[1].Value.Substring(0,8)+'/'})
 if([IO.Path]::GetRelativePath($Root,(Join-Path $Packet $Relative)).Length -gt 150){$Relative=[IO.Path]::GetDirectoryName($Relative).Replace('\','/')+'/r-'+$Hash.Substring(0,16)+'.receipt.txt'}
 if([IO.Path]::GetRelativePath($Root,(Join-Path $Packet $Relative)).Length -gt 150){throw 'Archive alias exceeds the consumer restore path limit.'}
 if($Relative -cne $original){$restoreAliases.Add(@{originalArchivePath=$original;path=$Relative;sha256=$Hash})}
 return $Relative
}
function Copy-Exact([string]$From,[string]$Relative){
 $hash=(Get-FileHash -LiteralPath $From).Hash
 $Relative=Get-CompactArchiveRelative $Relative $hash
 $to=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not$to.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Archive destination escapes packet.'}
 
 if($destinations.ContainsKey($Relative)){if($destinations[$Relative] -cne $hash){throw 'Conflicting archive destination.'};return}
 New-Item -ItemType Directory -Path (Split-Path $to) -Force|Out-Null
 Copy-Item -LiteralPath $From -Destination $to
 if((Get-FileHash -LiteralPath $to).Hash -cne $hash){throw 'Archive copy changed bytes.'}
 $destinations.Add($Relative,$hash)
 $copies.Add(@{source=$From;path=$Relative.Replace('\','/');sha256=$hash;bytes=(Get-Item -LiteralPath $From).Length})
}
function Read-Receipt([string]$Path){Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Get-ArchivedName([string]$Relative){
 if($Relative.EndsWith('.json')){return $Relative.Substring(0,$Relative.Length-5)+'.receipt.txt'}
 if($Relative -match '\.(md|vcxproj|sln|props|targets|filters)$'){return $Relative+'.receipt.txt'}
 return $Relative
}
$scopes=@('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')
$native=[Collections.Generic.List[object]]::new();$performancePaths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($configuration in @('Release','Debug','ASan Debug')){foreach($scope in $scopes){
 $stem="$scope-x64-$configuration";$file=Join-Path $Root ".build/reports/$stem.json";$receipt=Read-Receipt $file
 if($receipt.suite -cne $scope -or $receipt.configuration -cne $configuration -or $receipt.nativeArchitecture -cne 'X64' -or
  $receipt.platform -cne 'x64' -or $receipt.exitCode -ne 0 -or $receipt.skips.Count -or $receipt.Contains('tests')){throw 'Not a full matching native success.'}
 if((Get-FileHash -LiteralPath $receipt.executable).Hash -cne $receipt.sha256){throw 'Native artifact differs from successful run.'}
 $performance=Read-Receipt $receipt.performanceReport
 if($performance.sourceFingerprint -cne $ExpectedSource -or $performance.configuration -cne $configuration -or
  $performance.executableSha256 -cne $receipt.performanceExecutableSha256){throw 'Native/performance binding differs.'}
 if((ConvertTo-Json $receipt.performanceScenarios -Depth 16 -Compress) -cne (ConvertTo-Json $performance.scenarios -Depth 16 -Compress)){throw 'Original performance values differ.'}
 [void]$performancePaths.Add($receipt.performanceReport)
 Copy-Exact $file "Native/$stem.receipt.txt"
 Copy-Exact (Join-Path $Root ".build/logs/test-$stem.log") "Native/$stem.log"
 $native.Add(@{suite=$scope;configuration=$configuration;sha256=$receipt.sha256;performance=(Split-Path $receipt.performanceReport -Leaf)})
}}
$privateArtifacts=[Collections.Generic.List[object]]::new()
foreach($artifactDirectory in Get-ChildItem -LiteralPath $work -Directory|Where-Object Name -Like '*Artifacts'){
 foreach($artifact in Get-ChildItem -LiteralPath $artifactDirectory.FullName -Recurse -File){
  $privateArtifacts.Add(@{path=[IO.Path]::GetRelativePath($Root,$artifact.FullName).Replace('\','/');sha256=(Get-FileHash $artifact.FullName).Hash;bytes=$artifact.Length})
 }
}
foreach($file in Get-ChildItem -LiteralPath $work -Recurse -File){
 $relative=[IO.Path]::GetRelativePath($work,$file.FullName).Replace('\','/')
 if($relative -match '(^|/)[^/]*Artifacts/' -or $relative -match '(^|/)prior-tracked-replay[^/]*/' -or
  $relative -match '(^|/)__pycache__/' -or $relative.EndsWith('.pyc')){continue}
 if($relative -match '^Archive[^/]*\.ps1$' -or $relative -ceq 'Verify.ps1'){Copy-Exact $file.FullName $relative;continue}
 Copy-Exact $file.FullName ('Raw/'+(Get-ArchivedName $relative))
 if($file.Name -like 'Performance-*.json' -and $file.Name -notlike '*.comparison.json'){[void]$performancePaths.Add($file.FullName)}
 # Older baseline/candidate suite receipts contain absolute canonical bindings.
 if($file.Name -like '*native*.receipt.txt' -or $file.Name -in @('EditorControls.json','Embedded.json','Accessibility.json')){
  $candidate=Read-Receipt $file.FullName
  if($candidate.Contains('performanceReport')){[void]$performancePaths.Add([string]$candidate.performanceReport)}
 }
 if($file.Extension -ceq '.log'){
  foreach($line in Get-Content -LiteralPath $file.FullName){
   if($line -match '^Log: (.+)$' -and (Test-Path -LiteralPath $Matches[1])){Copy-Exact $Matches[1] ('Build/Logs/'+(Split-Path $Matches[1] -Leaf))}
  }
 }
}
$canonicalNames=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($path in $performancePaths){
 [void]$canonicalNames.Add((Split-Path $path -Leaf))
 Copy-Exact $path ('Performance/'+(Split-Path $path -Leaf))
 if(Test-Path -LiteralPath ($path+'.comparison.json')){Copy-Exact ($path+'.comparison.json') ('Performance/'+(Split-Path ($path+'.comparison.json') -Leaf))}
}
foreach($platform in @('x64','ARM64')){foreach($configuration in @('Release','Debug','ASan Debug')){
 $profile=$platform+'-'+$configuration.Replace(' ','')
 foreach($kind in @('build','incremental')){Copy-Exact (Join-Path $Root ".build/reports/performance-build/$profile/$kind.json") "Build/$profile-$kind.receipt.txt"}
}}
Copy-Exact (Join-Path $Root '.build/reports/AddressSanitizer-x64.json') 'Native/AddressSanitizer-x64.receipt.txt'
Copy-Exact (Join-Path $Root '.build/logs/test-AddressSanitizer-x64.log') 'Native/AddressSanitizer-x64.log'
$source=Read-Receipt (Join-Path $work 'Final02/qualified-source.receipt.txt')
foreach($entry in $source.files){
 $path=Join-Path $Root $entry.path
 if($entry.path -match '^(src/|include/|Tests/)'){
  if((Get-FileHash -LiteralPath $path).Hash -cne $entry.sha256){throw 'Qualified code/test source changed before archival.'}
  Copy-Exact $path ('Source/'+(Get-ArchivedName $entry.path))
 }
}
@{classification='Guarded retained geometry functional/resource evidence; exact universal incremental rendering and frame-performance acceptance remain open';
 sourceFingerprint=$ExpectedSource;nativeSuiteCount=$native.Count;nativeSuites=$native.ToArray();canonicalReportCount=$canonicalNames.Count;
 copies=$copies.ToArray();restorePathAliases=$restoreAliases.ToArray();privateArtifacts=$privateArtifacts.ToArray();
 exclusions='Original executables/PDBs remain in their recorded private .build artifact directories; old tracked replay copies and incidental Python bytecode are excluded.'}|
 ConvertTo-Json -Depth 20|Set-Content (Join-Path $Packet 'inventory.receipt.txt')
Write-Host "Archived $($native.Count) native successes, $($canonicalNames.Count) canonical diagnostics and all original source/failure attempts."
