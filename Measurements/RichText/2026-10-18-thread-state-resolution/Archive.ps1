[CmdletBinding()]param([string]$Root,[string]$Packet)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-thread-states-20261018'
$Packet=[IO.Path]::GetFullPath($Packet)
if(Test-Path -LiteralPath $Packet){throw 'Fresh packet required.'}
New-Item -ItemType Directory -Path $Packet|Out-Null
$copies=[Collections.Generic.List[object]]::new()
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
 $path=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not$path.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escapes packet.'}
 
 New-Item -ItemType Directory -Path (Split-Path $path) -Force|Out-Null
 Copy-Item -LiteralPath $From -Destination $path
 if((Get-FileHash -LiteralPath $path).Hash -cne $hash){throw 'Archive copy changed bytes.'}
 $copies.Add(@{source=$From;path=$Relative.Replace('\','/');sha256=$hash;bytes=(Get-Item -LiteralPath $From).Length})
}
$plan=Get-Content (Join-Path $work 'Controls/plan.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
$completion=Get-Content (Join-Path $work 'Controls/completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($completion.attempted -ne 4 -or $completion.overallBudgetExceeded -or @($completion.cases|Where-Object{-not$_.identityAndNativeValidationPassed}).Count){throw 'Original fixed observer schedule did not complete.'}
foreach($file in Get-ChildItem -LiteralPath $work -Recurse -File){
 $relative=[IO.Path]::GetRelativePath($work,$file.FullName).Replace('\','/')
 if($relative -match '(^|/)__pycache__/' -or $relative.EndsWith('.pyc')){continue}
 if($relative -in @('Archive.ps1','Verify.ps1')){Copy-Exact $file.FullName $relative;continue}
 $alias=$relative
 if($alias.EndsWith('.md')){$alias+='.receipt.txt'}
 if($alias -match '^AnalysisSelfTest[^/]*/synthetic-[^/]+/.*\.png$'){$alias+='.synthetic.receipt.txt'}
 Copy-Exact $file.FullName ('Raw/'+$alias)
 if($file.DirectoryName -eq $work -and $file.Extension -ceq '.ps1'){
  Copy-Exact $file.FullName ('Source/'+$file.Name)
 }
}
foreach($group in @('profilingInputs','benchmarkInputs')){
 foreach($entry in $plan.anchor[$group].GetEnumerator()){
  $path=Join-Path $Root $entry.Key
  if((Get-FileHash -LiteralPath $path).Hash -cne $entry.Value){throw 'Native/profile source changed after observation.'}
  Copy-Exact $path ('Source/'+$group+'/'+$entry.Key)
 }
}
$buildPath=Join-Path $Root '.build/reports/performance-build/x64-Release/build.json'
$build=Get-Content $buildPath -Raw|ConvertFrom-Json
if($build.outcome -cne 'PASSED' -or $build.identity -cne $plan.anchor.buildAttestationIdentity){throw 'Original Release build identity differs.'}
Copy-Exact $buildPath 'Build/release-build.receipt.txt'
$native=Join-Path $Root '.build/x64/Release/DxUi.EmbeddedTests.exe'
if((Get-FileHash $native).Hash -cne $plan.executableSha256){throw 'Observed executable changed.'}
@{classification='Same-binary external-observer resolution diagnostic; no baseline/candidate, scheduler-event or acceptance verdict';
 actualFrames=1280;nativeCaseCount=4;enabledSnapshots=35;fullyContainedFrameSnapshots=0;
 copies=$copies.ToArray();restorePathAliases=$restoreAliases.ToArray();privateArtifact=@{path='.build/reports/rich-retained-geometry-20261018/FinalArtifacts/Release/DxUi.EmbeddedTests.exe';sha256=$plan.executableSha256};
 limitations='All 35 acquisition brackets are outside or cross frame boundaries. Query p95 exceeds frame p95. Preserve inconclusive state attribution and all outliers. Three-byte synthetic hash fixtures are aliased as receipt text, never presented as native PNG captures.'}|
 ConvertTo-Json -Depth 24|Set-Content (Join-Path $Packet 'inventory.receipt.txt')
Write-Host 'Archived original owned-child observer controls, source/build bindings, functional checks and explicitly labeled synthetic/failed helper attempts.'
