[CmdletBinding()]param([string]$Packet=$PSScriptRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$Packet=[IO.Path]::GetFullPath($Packet)
$expectedSource='852642DBAFCA43F38DCE9218EDF8FB78A84DC703FC641E6AD6623E7B446EDCCF'
$expectedMethod='38F507301DAB9D0A8612D20365ECD09F856209E7611575084665E33DF76DDB91'
Add-Type -AssemblyName System.Text.Json
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Join-Path $Packet $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
function Assert-Hash([string]$Relative,[string]$Expected){
 $path=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not$path.StartsWith(($Packet+[IO.Path]::DirectorySeparatorChar),[StringComparison]::OrdinalIgnoreCase)){throw 'Sealed path escapes packet.'}
 if((Get-FileHash -LiteralPath $path).Hash -cne $Expected){throw "SHA-256 differs: $Relative"}
}
function Assert-Success($Operation){if($Operation.exitCode -ne 0 -or $Operation.timedOut -or -not$Operation.drained){throw 'Owned operation failed, timed out or did not drain.'}}
$sealed=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($line in Get-Content (Join-Path $Packet 'SHA256SUMS')){
 if($line -cnotmatch '^([A-F0-9]{64})  (.+)$'){throw 'Malformed seal.'}
 if(-not$sealed.Add($Matches[2])){throw 'Duplicate seal entry.'}
 Assert-Hash $Matches[2] $Matches[1]
}
foreach($file in Get-ChildItem -LiteralPath $Packet -Recurse -File){
 $relative=[IO.Path]::GetRelativePath($Packet,$file.FullName).Replace('\','/')
 if($relative -cne 'SHA256SUMS' -and -not$sealed.Contains($relative)){throw 'Unsealed extra file.'}
 if($file.Extension -in @('.exe','.dll','.pdb','.lib','.pyc')){throw 'Ordinary binary or bytecode in reviewed packet.'}
}
$inventory=Read-Receipt 'inventory.receipt.txt'
if($inventory.sourceFingerprint -cne $expectedSource){throw 'Qualified library source differs.'}
foreach($copy in $inventory.copies){Assert-Hash $copy.path $copy.sha256}
function Work-Path([string]$Relative){
 $matches=@($inventory.copies|Where-Object workPath -CEQ $Relative)
 if(-not$matches.Count){throw "Archived work input missing: $Relative"}
 return $matches[0].path
}
function Work-Receipt([string]$Relative){Read-Receipt (Work-Path $Relative)}
function Work-Text([string]$Relative){[IO.File]::ReadAllText((Join-Path $Packet (Work-Path $Relative)))}
$matrix=Work-Receipt 'Final/qualification-completion.receipt.txt'
$frozen=Work-Receipt 'Final/qualified-source.receipt.txt'
$end=Work-Receipt 'Final/frozen-inputs-end.receipt.txt'
if($matrix.completed -ne 6 -or $matrix.attempted -ne 6 -or $matrix.attemptedOperations -ne 14 -or $matrix.operations.Count -ne 14 -or
 $matrix.elapsedSeconds -gt 7200 -or $end.fileCount -ne $frozen.files.Count){throw 'Frozen qualification inventory differs.'}
foreach($operation in $matrix.operations){Assert-Success $operation}
Assert-Hash (Work-Path 'Final/qualified-source.receipt.txt') $end.manifestSha256
Assert-Hash (Work-Path 'Final/qualification-completion.receipt.txt') $end.matrixSha256
foreach($file in $frozen.files){Assert-Hash (Work-Path ('Final/QualifiedSource/'+$file.path)) $file.sha256}
foreach($path in $frozen.unchangedCanonicalInputs.Keys){Assert-Hash ('Source/'+$path) $frozen.unchangedCanonicalInputs[$path]}
if($frozen.unchangedCanonicalBenchmarkSha256 -cne 'E9DE41251A9186D4EC25F8178C4398E6E80C14E80FF948251067B1465549047E'){throw 'Canonical fixture changed.'}
if($inventory.nativeSuiteCount -ne 57 -or $inventory.nativeSuites.Count -ne 57){throw 'Ordinary native scope count differs.'}
$keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($entry in $inventory.nativeSuites){
 if(-not$keys.Add($entry.configuration+'/'+$entry.suite)){throw 'Duplicate native scope.'}
 $native=Read-Receipt ('Native/'+$entry.suite+'-x64-'+$entry.configuration+'.receipt.txt')
 if($native.suite -cne $entry.suite -or $native.configuration -cne $entry.configuration -or $native.platform -cne 'x64' -or
  $native.nativeArchitecture -cne 'X64' -or $native.exitCode -ne 0 -or $native.skips.Count -or $native.Contains('tests') -or
  $native.sha256 -cne $entry.sha256){throw 'Ordinary native receipt differs or contains skips/selection.'}
 $canonical=Read-Receipt ('Performance/'+$entry.performance)
 $build=Read-Receipt ('Build/x64-'+$entry.configuration.Replace(' ','')+'-build.receipt.txt')
 if($canonical.sourceFingerprint -cne $expectedSource -or $canonical.configuration -cne $entry.configuration -or
  $canonical.executableSha256 -cne $native.performanceExecutableSha256 -or $canonical.buildAttestationIdentity -cne $build.identity){throw 'Native/canonical/build identity differs.'}
 $nativeValues=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $native.performanceScenarios -Depth 48 -Compress))
 $canonicalValues=[Text.Json.Nodes.JsonNode]::Parse((ConvertTo-Json $canonical.scenarios -Depth 48 -Compress))
 if(-not[Text.Json.Nodes.JsonNode]::DeepEquals($nativeValues,$canonicalValues)){throw 'Original canonical values differ.'}
}
foreach($configuration in @('Debug','Release','ASan Debug')){
 if(@($inventory.nativeSuites|Where-Object configuration -CEQ $configuration).Count -ne 19){throw 'Ordinary profile inventory differs.'}
 foreach($platform in @('x64','ARM64')){
  $build=Read-Receipt ('Build/'+$platform+'-'+$configuration.Replace(' ','')+'-build.receipt.txt')
  if($build.outcome -cne 'PASSED' -or -not$build.identity){throw 'Root profile build did not pass.'}
 }
 $profile='x64-'+$configuration.Replace(' ','-')
 $log=[IO.File]::ReadAllText((Join-Path $Packet ('Native/EditorControls-x64-'+$configuration+'.log')))
 if($log.Contains('[START] TestRichTextEditorRetainedPreparationFixedTrials') -or $log.Contains('rich retained preparation trial=')){throw 'Repeated timing entered an ordinary suite.'}
 if(-not$log.Contains('rich positioned raster exactPairs=108 fontProfiles=6 directions=2 transforms=3 viewports=3')){throw 'Expanded raster inventory did not execute.'}
 foreach($name in @('TestRichDocumentLayoutHeightChangesRepositionSuffix','TestRichDocumentLayoutPrivateOriginsDoNotEnterProjection',
  'TestRichTextAccessibilityWindowScrollingCommands','TestRichTextAccessibilityEmbeddedScrollingCommands')){
  if(-not$log.Contains('[DONE] '+$name+' (')){throw 'Required boundary or native/embedded UIA test did not complete.'}
 }
 if($log -cnotmatch 'rich native compatibility sourceUnits=26624 dpi=96 bodyFontSize=13 chunks=26 wholeGeometry=0 wholeRaster=0 preparedChunkSourceUnits=1040 wholeLayoutsPrepared=0 wholeLayoutSourceUnits=0'){throw 'Actual native default-font reuse is unproven.'}
 $pairs=0
 for($font=0;$font -lt 6;++$font){for($direction=0;$direction -lt 2;++$direction){for($variant=0;$variant -lt 3;++$variant){for($viewport=0;$viewport -lt 3;++$viewport){
  $stem="Final/$profile/Raster/profile-$font-direction-$direction-variant-$variant-viewport-$viewport"
  $expected=Work-Path ($stem+'-expected.png');$actual=Work-Path ($stem+'-actual.png')
  if((Get-FileHash -LiteralPath (Join-Path $Packet $expected)).Hash -cne (Get-FileHash -LiteralPath (Join-Path $Packet $actual)).Hash){throw 'Actual/reference PNG bytes differ.'}
  ++$pairs
 }}}}
 if($pairs -ne 108){throw 'Raster pair inventory differs.'}
 $embedded=[IO.File]::ReadAllText((Join-Path $Packet ('Native/Embedded-x64-'+$configuration+'.log')))
 foreach($dpi in @(96,144,192)){
  if(-not$embedded.Contains("rich embedded positioned dpi=$dpi bodyFontSize=13 chunks=26 reusedChunks=25 preparedSourceUnits=1040")){throw 'Actual embedded default-font reuse is unproven.'}
 }
 if(-not$embedded.Contains('rich layout clear CppAllocations=0 CppRequestedBytes=0') -or
  -not$embedded.Contains('rich geometry unchanged repeats=4 lines=512 CppAllocations=0 CppRequestedBytes=0')){throw 'Clear/unchanged allocation contract differs.'}
 $maximum=[regex]::Matches($embedded,'rich geometry maximum kind=(\d+) sourceUnits=65536 chunks=(\d+) retainedCppBytes=(\d+) releasedDelta=(-?\d+)')
 if($maximum.Count -ne 3){throw 'Maximum-source resource inventory differs.'}
 foreach($value in $maximum){if([int]$value.Groups[2].Value -gt 128 -or [long]$value.Groups[3].Value -gt 24MB -or [long]$value.Groups[4].Value -ne 0){throw 'Maximum-source resource bound failed.'}}
 $replacement=[regex]::Match($embedded,'rich geometry replacement lines=1024 edits=512 retainedCppBytes=(\d+) peakCppBytes=(\d+) releasedDelta=(-?\d+)')
 if(-not$replacement.Success -or [long]$replacement.Groups[1].Value -gt 8MB -or [long]$replacement.Groups[2].Value -gt 16MB -or [long]$replacement.Groups[3].Value -ne 0){throw 'Retained/replacement resource bound failed.'}
}
foreach($profile in @('x64-Debug','x64-ASan-Debug')){
 $stdout=Work-Text ($profile.Replace('x64-','Final/x64-')+'-geometry-probe.log')
 $stderr=Work-Text ('Final/'+$profile+'-geometry-probe.err')
 $counts=[regex]::Matches($stdout,'rich document geometry allocation-failure whole=([01]) sweptRequests=(\d+)')
 if($counts.Count -ne 2){throw 'Complete normal/whole allocation schedules missing.'}
 $points=[regex]::Matches($stderr,'rich geometry failure-point=(\d+)');$cursor=0
 foreach($count in $counts){for($point=0;$point -lt [int]$count.Groups[2].Value;++$point){
  if($cursor -ge $points.Count -or [int]$points[$cursor++].Groups[1].Value -ne $point){throw 'An allocation-failure point was omitted or reordered.'}
 }}
 if($cursor -ne $points.Count -or $stderr.Contains('unexpected noexcept termination')){throw 'Allocation sweep has extra points or termination.'}
 $copy=[regex]::Match($stdout,'rich layout copy allocation-failure sweptRequests=(\d+)')
 $copyPoints=[regex]::Matches($stderr,'rich layout copy failure-point=(\d+)')
 if(-not$copy.Success -or $copyPoints.Count -ne [int]$copy.Groups[1].Value){throw 'Copy allocation sweep incomplete.'}
 for($point=0;$point -lt $copyPoints.Count;++$point){if([int]$copyPoints[$point].Groups[1].Value -ne $point){throw 'Copy failure point differs.'}}
 $flat=Work-Text ('Final/'+$profile+'-flat-projection-probe.log')
 $flatTrace=Work-Text ('Final/'+$profile+'-flat-projection-probe.err')
 $flatCount=[regex]::Match($flat,'rich flat projection allocation-failure sweptRequests=(\d+)')
 $flatPoints=[regex]::Matches($flatTrace,'rich flat projection failure-point=(\d+)')
 if(-not$flatCount.Success -or $flatPoints.Count -ne [int]$flatCount.Groups[1].Value){throw 'Flat projection failure sweep incomplete.'}
 for($point=0;$point -lt $flatPoints.Count;++$point){if([int]$flatPoints[$point].Groups[1].Value -ne $point){throw 'Flat projection point differs.'}}
}
$trial=Work-Receipt 'Trials/native-trials.receipt.txt'
Assert-Hash (Work-Path 'BeforeOriginal/before-native.log') $trial.beforeLogSha256
Assert-Hash (Work-Path 'Trials/selected-native.log') $trial.afterLogSha256
if($trial.methodSha256 -cne $expectedMethod){throw 'Fixed timing method changed.'}
function Method-Text([string]$Text){
 $first=$Text.IndexOf('void TestRichTextEditorRetainedPreparationFixedTrials(')
 if($first -lt 0){throw 'Fixed method missing.'};$body=$Text.IndexOf('{',$first);$depth=0
 for($index=$body;$index -lt $Text.Length;++$index){if($Text[$index] -eq '{'){++$depth}elseif($Text[$index] -eq '}' -and --$depth -eq 0){return $Text.Substring($first,$index-$first+1)}}
 throw 'Fixed method unbalanced.'
}
foreach($path in @('BeforeOriginal/Scope.Tests.RichTextEditor.cpp','Final/QualifiedSource/Tests/Controls/Scope.Tests.RichTextEditor.cpp')){
 $method=Method-Text (Work-Text $path)
 if([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($method))) -cne $expectedMethod){throw 'Original/current method bytes differ.'}
}
foreach($side in @('before','after')){
 $log=if($side -ceq 'before'){Work-Text 'BeforeOriginal/before-native.log'}else{Work-Text 'Trials/selected-native.log'}
 $rows=[regex]::Matches($log,'rich retained preparation trial=(\d+) sourceUnits=26624 edits=128 initialMs=([\d.]+) editMs=([\d.]+)')
 if($rows.Count -ne 5 -or $trial[$side].samples.Count -ne 5){throw 'Fixed trial count differs.'}
 foreach($row in $rows){
  $sample=$trial[$side].samples[[int]$row.Groups[1].Value]
  if($sample.trial -ne [int]$row.Groups[1].Value -or $sample.initialMilliseconds -ne [double]::Parse($row.Groups[2].Value,[Globalization.CultureInfo]::InvariantCulture) -or
   $sample.editMilliseconds -ne [double]::Parse($row.Groups[3].Value,[Globalization.CultureInfo]::InvariantCulture)){throw 'Original timing sample differs.'}
 }
 if($trial[$side].initialMedianMilliseconds -ne @($trial[$side].samples.initialMilliseconds|Sort-Object)[2] -or
  $trial[$side].editMedianMilliseconds -ne @($trial[$side].samples.editMilliseconds|Sort-Object)[2]){throw 'Timing median differs.'}
}
$selected=Work-Receipt 'Trials/selected-native.receipt.txt'
$completion=Work-Receipt 'Trials/completion.receipt.txt'
Assert-Success $completion
if($selected.exitCode -ne 0 -or $selected.skips.Count -or $selected.tests.Count -ne 1 -or $selected.tests[0] -cne $trial.method){throw 'Explicit diagnostic selection differs.'}
$binding=Work-Receipt 'Trials/artifact-binding.receipt.txt'
if($binding.sha256 -cne $selected.sha256){throw 'Trial artifact binding differs.'}
$probe=Work-Text 'ThirdProbe/native.log'
$geometry=@($probe -split '\r?\n'|Where-Object {$_ -like 'rich positioned geometry *'})
$glyphs=@($probe -split '\r?\n'|Where-Object {$_ -like 'rich positioned glyphs *'})
if($geometry.Count -ne 9 -or $glyphs.Count -ne 54 -or @($geometry|Where-Object {$_ -notmatch 'heightEqual=1 caretMismatches=0 '}).Count -or
 @($glyphs|Where-Object {$_ -notmatch ' mismatches=0$'}).Count){throw 'Independent origin probe inventory differs.'}
$failed=Work-Receipt 'FirstPositioned/completion.receipt.txt'
if($failed.attempted -ne 1 -or $failed.operations[0].exitCode -ne 1 -or $failed.operations[0].timedOut -or -not$failed.operations[0].drained -or
 -not(Work-Text 'FirstPositioned/release-build.log').Contains('C2248')){throw 'Original WRL compile failure was not preserved.'}
$tooling=Work-Receipt 'ToolingFinal/tooling-completion.receipt.txt'
if($tooling.attempted -ne 2 -or $tooling.scheduled -ne 2 -or $tooling.elapsedSeconds -gt 900){throw 'Final tooling inventory differs.'}
foreach($result in $tooling.results){Assert-Success $result}
Write-Host 'Verified sealed bytes, 57 ordinary suites, six builds, 324 exact WARP pairs, failure/resource boundaries, unchanged trial method/samples and original negative evidence. No scheduler, full-engine or frame-performance acceptance follows.'
