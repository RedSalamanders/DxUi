foreach($configuration in @('Debug','Release','ASan Debug')){
 $editor=[IO.File]::ReadAllText((Join-Path $Packet ('Native/EditorControls-x64-'+$configuration+'.log')))
 foreach($name in @('TestLosslessMarkdownOriginalCorporaPreservation','TestLosslessMarkdownRootLeafBlocks','TestLosslessMarkdownFencedCodeBoundaries',
   'TestLosslessMarkdownLexicalValuesAndAffinity','TestLosslessMarkdownSchedulingCancellationAndIdentity','TestRichTextEditorNativeLosslessMarkdownPreparation')){
  if(-not$editor.Contains('[DONE] '+$name+' (')){throw 'Required owned parser/native behavior did not complete.'}
 }
 $accessibility=[IO.File]::ReadAllText((Join-Path $Packet ('Native/Accessibility-x64-'+$configuration+'.log')))
 foreach($name in @('TestRichTextAccessibilityWindowMarkdownSourceAndRanges','TestRichTextAccessibilityEmbeddedMarkdownSourceAndRanges')){
  if(-not$accessibility.Contains('[DONE] '+$name+' (')){throw 'Analysis-enabled native/embedded UIA behavior did not complete.'}
 }
 $embedded=[IO.File]::ReadAllText((Join-Path $Packet ('Native/Embedded-x64-'+$configuration+'.log')))
 $bounds=[regex]::Match($embedded,'lossless Markdown sweptRequests=(\d+) pendingBeginRequests=(\d+) maximumRetainedCppBytes=(\d+) replacementPeakCppBytes=(\d+) releasedDelta=(-?\d+) unchangedAndRetireAllocations=(\d+)')
 if(-not$bounds.Success -or [int]$bounds.Groups[1].Value -le 0 -or [int]$bounds.Groups[2].Value -le 0 -or
  [long]$bounds.Groups[3].Value -gt 24MB -or [long]$bounds.Groups[4].Value -gt 32MB -or
  [long]$bounds.Groups[5].Value -ne 0 -or [int]$bounds.Groups[6].Value -ne 0){throw 'Parser allocation/storage predicates differ.'}
}
foreach($profile in @('x64-Debug','x64-ASan-Debug')){
 $out=Work-Text ('Final/'+$profile+'-markdown-probe.log')
 $err=Work-Text ('Final/'+$profile+'-markdown-probe.err')
 $counts=[regex]::Match($out,'lossless Markdown sweptRequests=(\d+) pendingBeginRequests=(\d+)')
 if(-not$counts.Success -or $err.Contains('unexpected noexcept termination')){throw 'Isolated parser probe did not finish normally.'}
 foreach($kind in @(@{field=1;pattern='lossless Markdown failure-point=(\d+)'},@{field=2;pattern='lossless Markdown pending-begin-failure-point=(\d+)'})){
  $points=[regex]::Matches($err,$kind.pattern)
  if($points.Count -ne [int]$counts.Groups[$kind.field].Value){throw 'Parser allocation failure points are missing.'}
  for($point=0;$point -lt $points.Count;++$point){if([int]$points[$point].Groups[1].Value -ne $point){throw 'Parser allocation failure points were omitted or reordered.'}}
 }
}
$first=Work-Receipt 'First/completion.receipt.txt'
if($first.attempted -ne 2 -or $first.operations[0].exitCode -ne 0 -or $first.operations[1].exitCode -ne 1 -or
 $first.operations[1].timedOut -or -not$first.operations[1].drained -or
 -not(Work-Text 'First/EditorControls.log').Contains('FAILED: pending analysis preserves exact native text-store source')){throw 'Original failed native fixture is not preserved.'}
foreach($attempt in @('Second','Third')){
 $receipt=Work-Receipt ($attempt+'/completion.receipt.txt')
 if($receipt.attempted -ne 2){throw 'Affected/resource candidate did not complete its declared operations.'}
 foreach($operation in $receipt.operations){Assert-Success $operation}
}
$tooling=Work-Receipt 'Final/tooling-completion.receipt.txt'
if($tooling.attempted -ne 2 -or $tooling.scheduled -ne 2 -or $tooling.elapsedSeconds -gt 900){throw 'Full tooling/format qualification incomplete.'}
foreach($result in $tooling.results){Assert-Success $result}
$pins=@{
 'ThirdParty/Markdown/whatwg-2026-10-18/entities.json'='D741D877AC77C4194C4AD526B5B4A19AEF8DFE411AB840A466891CDBB9F362E6'
 'ThirdParty/Markdown/whatwg-2026-10-18/LICENSE.txt'='85DC6F5CCB57A6FE8C33D158F9FC8FC7EE5655A5D3DB2CDD131C6A3D0F48A864'
 'ThirdParty/Markdown/corpora/CommonMark-0.31.2-spec.json'='D431B29D97B6F73E69D547109CF5081578FAC931E72AFE95639EBE766C1B2A20'
 'ThirdParty/Markdown/corpora/GFM-0.29.0.gfm.13-spec.txt'='7D8E5814BEFEC287AC116786D81FF14E0ADC9B13295B4494649E995408FD871C'
}
foreach($name in $pins.Keys){Assert-Hash (Work-Path ('Final/QualifiedSource/'+$name)) $pins[$name]}
$parser=Read-Receipt 'References/Parsers/reference-inputs.receipt.txt'
if($parser.files.Count -ne 9 -or @($parser.files.project|Sort-Object -Unique).Count -ne 3){throw 'Parser research input inventory differs.'}
foreach($entry in $parser.files){
 if(-not@($inventory.copies|Where-Object {$_.path.StartsWith('References/Parsers/') -and $_.sha256 -ceq $entry.sha256}).Count){throw 'Pinned parser research bytes missing.'}
}
Write-Host 'Verified sealed source/data, 57 ordinary x64 suites, six profile builds, 324 exact WARP pairs, all parser/corpus/affinity cases, native/embedded UIA, complete allocation traces and storage predicates. Full grammar/semantic rendering, hard latency, ARM64 runtime, physical IME/AT and frame acceptance remain open.'
