[CmdletBinding()]param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-markdown-parser-20261018'
$matrix=Get-Content (Join-Path $work 'Final/qualification-completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
$tooling=Get-Content (Join-Path $work 'Final/tooling-completion.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
if($matrix.completed -ne 6 -or $matrix.attemptedOperations -ne 16 -or $tooling.attempted -ne 2){throw 'Qualification is incomplete.'}
foreach($operation in @($matrix.operations)+@($tooling.results)){
 if($operation.exitCode -ne 0 -or $operation.timedOut -or -not$operation.drained){throw 'Cannot close documentation over a failed operation.'}
}
$frozen=Get-Content (Join-Path $work 'Final/qualified-source.receipt.txt') -Raw|ConvertFrom-Json -AsHashtable -Depth 32
foreach($entry in $frozen.files){if((Get-FileHash (Join-Path $Root $entry.path)).Hash -cne $entry.sha256){throw 'Frozen input changed before prose closeout.'}}
$allowed=@('Specs/Plans/WIP/RichTextEditor_2026-10-17.md','docs/rich-text.md','ThirdParty/Markdown/README.md')
$plan=Join-Path $Root $allowed[0]
$text=[IO.File]::ReadAllText($plan).Replace("`r`n","`n")
$text=$text.Replace('Owned lossless Markdown root/lexical analysis is being qualified next.','Owned lossless Markdown root/lexical analysis is qualified as an opt-in bounded preview.')
$old=@'
- [ ] M1: Lossless source/block/lexeme partitions, explicit transformed-value
  affinity, bounded resumable root/lexical scans and opt-in host analysis.
  Code/tests/docs are implemented; functional/resource/toolchain qualification
  and exact evidence closeout remain before marking this stage complete.
'@
$new=@'
- [x] M1: Lossless source/block/lexeme partitions, explicit transformed-value
  affinity, bounded resumable root/lexical scans and opt-in host analysis.
  Code/tests/docs pass all 57 ordinary x64 suites, six profile builds and full
  tooling/format checks. See the [sealed parser-stage evidence](../../../Measurements/RichText/2026-10-18-markdown-parser/README.md).
  This qualifies root/lexical analysis, not complete CommonMark/GFM rendering.
'@
if(-not$text.Contains($old)){throw 'M1 qualification paragraph changed unexpectedly.'}
$text=$text.Replace($old,$new)
[IO.File]::WriteAllText($plan,$text)
$guide=Join-Path $Root $allowed[1]
$text=[IO.File]::ReadAllText($guide)
$text=$text.Replace('auto rootBlocks = syntax->Blocks();','const auto& rootBlocks = syntax->Blocks();')
$text+="`nThe [parser-stage qualification](../Measurements/RichText/2026-10-18-markdown-parser/README.md) retains all 57 ordinary x64 suite receipts, six root builds, 324 exact rendering pairs, complete parser failure traces/storage measurements and the original failed fixture. Full grammar/HTML conformance, semantic rendering/editing, extensions and the original platform/performance gates remain open.`n"
[IO.File]::WriteAllText($guide,$text)
$notice=Join-Path $Root $allowed[2]
$text=[IO.File]::ReadAllText($notice)
$text=$text.Replace('UTF-16 transcription of the original example inputs.','UTF-16 transcription of the original example inputs. GFM visible-tab arrows are decoded to tabs according to the upstream spec example convention; the original spec bytes are unchanged.')
[IO.File]::WriteAllText($notice,$text)
foreach($entry in $frozen.files){
 if($entry.path -notin $allowed -and (Get-FileHash (Join-Path $Root $entry.path)).Hash -cne $entry.sha256){throw 'Code, test, data or project input changed during documentation closeout.'}
}
@{classification='Prose-only qualification closeout after all frozen native/build/tooling operations passed';
 paths=@(foreach($path in $allowed){@{path=$path;sha256=(Get-FileHash (Join-Path $Root $path)).Hash}});
 unchangedFrozenInputCount=$frozen.files.Count-$allowed.Count;verifiedUtc=[datetime]::UtcNow.ToString('o')}|
 ConvertTo-Json -Depth 8|Set-Content (Join-Path $work 'prose-closeout.receipt.txt')
