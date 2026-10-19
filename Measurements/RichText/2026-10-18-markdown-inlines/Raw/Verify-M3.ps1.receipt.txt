[CmdletBinding()]param([string]$Packet=$PSScriptRoot,[string]$ExpectedSeal='')
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Text.Json
$Packet=[IO.Path]::GetFullPath($Packet)
foreach($entry in @(Get-Item -LiteralPath $Packet)+@(Get-ChildItem -LiteralPath $Packet -Recurse -Force)){
 if($entry.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Packet reparse points are unsupported.'}
}
function Path-InPacket([string]$Relative){
 if([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Packet path escaped its root.'}
 $full=[IO.Path]::GetFullPath((Join-Path $Packet $Relative))
 if(-not $full.StartsWith($Packet.TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Packet path containment failed.'}
 return $full
}
function Read-Receipt([string]$Relative){Get-Content -LiteralPath (Path-InPacket $Relative) -Raw|ConvertFrom-Json -AsHashtable -Depth 64}
$seal=(Get-Content -LiteralPath (Join-Path $Packet 'manifest.sha256') -Raw).Trim()
if($ExpectedSeal -and $seal -cne $ExpectedSeal){throw 'Expected packet seal differs.'}
if((Get-FileHash -LiteralPath (Join-Path $Packet 'manifest.receipt.txt')).Hash -cne $seal){throw 'Manifest seal mismatch.'}
$manifest=Read-Receipt 'manifest.receipt.txt'
$paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($row in $manifest.files){
 if(-not $paths.Add($row.path)){throw 'Duplicate manifest path.'}
 $file=Path-InPacket $row.path
 if(-not(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-Item -LiteralPath $file).Length -ne $row.bytes -or (Get-FileHash -LiteralPath $file).Hash -cne $row.sha256){throw "Packet file hash/length mismatch: $($row.path)"}
 if([IO.Path]::GetExtension($row.path) -in @('.exe','.dll','.pdb','.lib')){throw 'Build products must remain private.'}
}
$actual=@(Get-ChildItem -LiteralPath $Packet -Recurse -File|Where-Object Name -NotIn @('manifest.receipt.txt','manifest.sha256'))
if($actual.Count -ne $paths.Count){throw 'Unsealed files were added or sealed files removed.'}
foreach($file in $actual){$relative=[IO.Path]::GetRelativePath($Packet,$file.FullName).Replace('\','/');if(-not $paths.Contains($relative)){throw 'An unsealed file is present.'}}
$summary=Read-Receipt 'summary.receipt.txt'
$aliases=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($row in $summary.workAliases){$aliases.Add($row.original,$row.path)}
function Work-Receipt([string]$Original){if(-not $aliases.ContainsKey($Original)){throw "Original receipt missing: $Original"};Read-Receipt $aliases[$Original]}
function Work-Text([string]$Original){if(-not $aliases.ContainsKey($Original)){throw "Original log missing: $Original"};[IO.File]::ReadAllText((Path-InPacket $aliases[$Original]))}
$matrix=Work-Receipt 'Final/qualification-completion.receipt.txt'
if($matrix.completed -ne 6 -or $matrix.operations.Count -ne 21){throw 'Six-profile matrix incomplete.'}
foreach($operation in $matrix.operations){if($operation.exitCode -ne 0 -or $operation.timedOut -or -not $operation.drained){throw 'A qualification operation did not pass.'}}
if(@($matrix.operations|Where-Object name -Like '*-probe').Count -ne 12){throw 'Twelve isolated profile probes are required.'}
$qualified=Work-Receipt 'Final/qualified-source.receipt.txt'
if($qualified.sourceFingerprint -cne $summary.sourceFingerprint -or $qualified.ordinaryScopeCount -ne 57){throw 'Frozen source binding differs.'}
foreach($file in $qualified.files){
 $original='Final/Source/'+$file.path
 if(-not $aliases.ContainsKey($original) -or (Get-FileHash -LiteralPath (Path-InPacket $aliases[$original])).Hash -cne $file.sha256){throw 'Frozen source bytes differ.'}
}
$artifacts=Work-Receipt 'FinalArtifacts.receipt.txt'
$keys=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
if($summary.native.Count -ne 57){throw '57 ordinary scope obligations are required.'}
foreach($record in $summary.native){
 $key=$record.configuration+'/'+$record.scope;if(-not $keys.Add($key)){throw 'Ordinary scope duplicated.'}
 $receipt=Read-Receipt $record.receipt;$performance=Read-Receipt $record.performance
 if($receipt.suite -cne $record.scope -or $receipt.configuration -cne $record.configuration -or $receipt.platform -cne 'x64' -or $receipt.nativeArchitecture -cne 'X64' -or $receipt.exitCode -ne 0 -or $receipt.skips.Count -or $receipt.Contains('tests')){throw 'Ordinary scope is partial, skipped or unsuccessful.'}
 if($performance.sourceFingerprint -cne $summary.sourceFingerprint -or $performance.executableSha256 -cne $receipt.performanceExecutableSha256 -or $receipt.sha256 -cne $record.sha256){throw 'Native/canonical/artifact binding differs.'}
 $a=[System.Text.Json.Nodes.JsonNode]::Parse(($receipt.performanceScenarios|ConvertTo-Json -Depth 48 -Compress))
 $b=[System.Text.Json.Nodes.JsonNode]::Parse(($performance.scenarios|ConvertTo-Json -Depth 48 -Compress))
 if(-not[System.Text.Json.Nodes.JsonNode]::DeepEquals($a,$b)){throw 'Canonical observations were modified.'}
 if(-not @($artifacts.copies|Where-Object { $_.original -ceq $receipt.executable -and $_.sha256 -ceq $receipt.sha256 }).Count){throw 'Successful native artifact identity is absent.'}
}
$scopes=@('Foundation','Embedded','Grid','Theme','Control','MenuExitLifetime','NewControls','EditorControls','TextField','MultilineText','ReadOnly','ComboBox','Tree','Tooltip','Rendering','Animation','Accessibility','WindowHost','InteractiveLease')
foreach($configuration in @('Debug','Release','ASan Debug')){
 foreach($scope in $scopes){if(-not $keys.Contains($configuration+'/'+$scope)){throw 'Required scope/profile missing.'}}
 $editor=[IO.File]::ReadAllText((Path-InPacket ('Native/EditorControls-x64-'+$configuration+'.log')))
 foreach($test in @('TestLosslessMarkdownOriginalCorporaPreservation','TestLosslessMarkdownIndependentBlockCorpus','TestLosslessMarkdownBlockScenariosAndNodeIdentity',
  'TestLosslessMarkdownContentSpansAndDependencies','TestLosslessMarkdownNestedPrefixWorkIsBounded','TestLosslessMarkdownIdentityAtUnicodeParagraphEdges',
  'TestLosslessMarkdownHtmlRawNameAndVersionBoundaries','TestRichTextEditorNativeBlockGrammarAndIdentity','TestLosslessMarkdownIndependentSemanticCorpora','TestLosslessMarkdownInlineScenariosAndExtensionIdentity','TestRichTextEditorNativeInlineGrammarAndReferenceEdits')){
  if(-not $editor.Contains('[DONE] '+$test+' (')){throw "Required parser/native behavior missing: $test"}
 }
 if(-not $editor.Contains('independent block-section structural expectations: 568; failures: 0')){throw 'Independent block corpus did not pass completely.'}
 if(-not $editor.Contains('Independent original semantic expectations: 1324; failures: 0')){throw 'Independent semantic corpus did not pass completely.'}
 if(-not $editor.Contains('Directed inline scenarios: ')){throw 'Directed Unicode/extension fixtures are absent.'}
 $embedded=[IO.File]::ReadAllText((Path-InPacket ('Native/Embedded-x64-'+$configuration+'.log')))
 $bounds=[regex]::Match($embedded,'lossless Markdown sweptRequests=(\d+) pendingBeginRequests=(\d+) maximumRetainedCppBytes=(\d+) replacementPeakCppBytes=(\d+) releasedDelta=(-?\d+) unchangedAndRetireAllocations=(\d+)')
 if(-not $bounds.Success -or [long]$bounds.Groups[3].Value -gt 24MB -or [long]$bounds.Groups[4].Value -gt 32MB -or [long]$bounds.Groups[5].Value -ne 0 -or [long]$bounds.Groups[6].Value -ne 0){throw 'Parser storage/retirement predicate failed.'}
}
if($summary.rasterPairs.Count -ne 324){throw '324 exact raster pairs are required.'}
foreach($pair in $summary.rasterPairs){if((Get-FileHash -LiteralPath (Path-InPacket $pair.expected)).Hash -cne (Get-FileHash -LiteralPath (Path-InPacket $pair.actual)).Hash){throw 'A raster oracle differs.'}}
foreach($profile in @('x64-Debug','x64-ASan-Debug','x64-Release')){
 $text=Work-Text ('Final/'+$profile+'-markdown-probe.log');$trace=Work-Text ('Final/'+$profile+'-markdown-probe.err')
 $counts=[regex]::Match($text,'lossless Markdown sweptRequests=(\d+) pendingBeginRequests=(\d+)')
 if(-not $counts.Success -or $trace.Contains('unexpected noexcept termination')){throw 'Parser failure sweep did not complete normally.'}
 foreach($kind in @(@{index=1;pattern='lossless Markdown failure-point=(\d+)'},@{index=2;pattern='lossless Markdown pending-begin-failure-point=(\d+)'})){
  $points=[regex]::Matches($trace,$kind.pattern);if($points.Count -ne [int]$counts.Groups[$kind.index].Value){throw 'Failure schedule incomplete.'}
  for($i=0;$i -lt $points.Count;++$i){if([int]$points[$i].Groups[1].Value -ne $i){throw 'Failure point omitted or reordered.'}}
 }
}
$tooling=Work-Receipt 'Final/tooling-completion.receipt.txt'
if($tooling.attempted -ne 2 -or @($tooling.results|Where-Object { $_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained }).Count){throw 'Full validation/format tooling did not pass.'}
$pins=@{
 'corpora/CommonMark-0.31.2-spec.json'='D431B29D97B6F73E69D547109CF5081578FAC931E72AFE95639EBE766C1B2A20'
 'corpora/CommonMark-0.31.2-LICENSE.txt'='3E4806BA6F20073E8CE40DA5A0C4B59F7F44287965F538E195A4D734D833557B'
 'corpora/GFM-0.29.0.gfm.13-spec.txt'='7D8E5814BEFEC287AC116786D81FF14E0ADC9B13295B4494649E995408FD871C'
 'corpora/GFM-0.29.0.gfm.13-COPYING.txt'='C22E885F33B821BDDB24CF007145E5540655B6C0F403E49E6C76A93C28E6D9A9'
 'whatwg-2026-10-18/entities.json'='D741D877AC77C4194C4AD526B5B4A19AEF8DFE411AB840A466891CDBB9F362E6'
 'whatwg-2026-10-18/LICENSE.txt'='85DC6F5CCB57A6FE8C33D158F9FC8FC7EE5655A5D3DB2CDD131C6A3D0F48A864'
}
foreach($pin in $pins.GetEnumerator()){if((Get-FileHash -LiteralPath (Path-InPacket ('References/'+$pin.Key+'.receipt.txt'))).Hash -cne $pin.Value){throw 'Original reference data/license hash differs.'}}
$unicodePins=@{
 'CaseFolding.txt'='FF8D8FEFBF123574205085D6714C36149EB946D717A0C585C27F0F4EF58C4183'
 'UnicodeData.txt'='2E1EFC1DCB59C575EEDF5CCAE60F95229F706EE6D031835247D843C11D96470C'
 'LICENSE.txt'='E7A93B009565CFCE55919A381437AC4DB883E9DA2126FA28B91D12732BC53D96'
}
foreach($pin in $unicodePins.GetEnumerator()){
 if((Get-FileHash -LiteralPath (Path-InPacket ('References/Unicode17/'+$pin.Key+'.receipt.txt'))).Hash -cne $pin.Value){throw 'Original Unicode data/license differs'}
}
Write-Output "Verified M3 seal $seal; frozen source, 57 ordinary x64 scope obligations, six builds, 12 failure probes, 324 exact raster pairs, standards/data and full tooling receipts. No native ARM64, physical IME/AT, complete semantic renderer/editing or frame-performance acceptance claim."