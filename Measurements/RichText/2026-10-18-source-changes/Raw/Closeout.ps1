[CmdletBinding()] param([string]$Root)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$work=Join-Path $Root '.build/reports/rich-source-changes-20261018'
$completion=Get-Content -LiteralPath (Join-Path $work 'qualification-completion.receipt.txt') -Raw|ConvertFrom-Json
if($completion.attempted -ne 6 -or @($completion.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Finish qualification first.'}
$tooling=Get-Content -LiteralPath (Join-Path $work 'tooling-completion.receipt.txt') -Raw|ConvertFrom-Json
if($tooling.attempted -ne 2 -or @($tooling.results|Where-Object{$_.exitCode -ne 0 -or $_.timedOut -or -not $_.drained}).Count){throw 'Finish tooling first.'}
$elapsed=$completion.elapsedSeconds.ToString('F3',[Globalization.CultureInfo]::InvariantCulture)
$text=Get-Content -LiteralPath (Join-Path $work 'README.Draft.md') -Raw
$replacement="All **57 final ordinary x64 suite runs** pass across Release, Debug and ASan Debug with zero native skips. A fixed six-slot schedule completes all six in $elapsed seconds within 3,600 seconds overall / 900 per owned child. Six successful build attestations cover x64/ARM64 in all three configurations. ARM64 remains cross-compilation only. The isolated ASan probe actually diagnoses heap-use-after-free; its expected nonzero result is separate from suite success. Embedded passes 5,771 Release checks and 5,917 Debug/ASan checks."
$text=$text.Replace('FINAL_MATRIX_PLACEHOLDER',$replacement)
[IO.File]::WriteAllText((Join-Path $work 'README.Final.md'),$text)
$plan=Join-Path $Root 'Specs/Plans/WIP/RichTextEditor_2026-10-17.md'
$text=[IO.File]::ReadAllText($plan)
$text=$text.Replace('shared Unicode 17 segmentation and persistent piece-tree storage functionally qualified.','shared Unicode 17 segmentation, persistent piece-tree storage and bounded source-change metadata functionally qualified.')
$text=$text.Replace('- [ ] Add immutable bounded change metadata','- [x] Add immutable bounded change metadata')
$text+="`r`n### Bounded source-change qualification`r`n`r`n"+@'
The [source-change packet](../../../Measurements/RichText/2026-10-18-source-changes/README.md)
seals library fingerprint 3FEED65870EA98787EAC461CCB7AA6BA1CD9E5BC73C21684AB650B873517BAC4.
All 57 ordinary native suite runs pass with zero skips across x64 Release,
Debug and ASan, alongside all six build profiles and full tooling/format checks.
Seven new source-change cases cover localized previews, scalar/CRLF joins,
undo/redo/branches, expiry, reset, independent reconstruction and concurrent reads.
The 512-query/64-preview fixture allocates zero C++ bytes. After 4,096 previews,
tracked retained storage is 22,911 Release / 22,999 Debug/ASan bytes, released to
zero afterward. All page-rollover and five ordinary operation allocation failures
preserve complete state and histories. Embedded passes 5,771 Release and 5,917
Debug/ASan checks; 1,000 warm composites remain allocation-free in every profile.

Numeric metadata adds 188,664 C++ retained bytes to the existing history fixture:
1,400,710 Release / 1,499,086 Debug/ASan, below the unchanged 4 MiB limit. Five
unchanged within-process methods retain descriptive medians: fragmented edits
15.7147 -> 16.6739 ms, lookup 0.5189 -> 0.5270 ms, and backward queries
18.2838 -> 18.7574 ms. Record that overhead without a controlled timing claim.
Original baseline and first-candidate failures are preserved, with exact source,
canonical reports and the attached-supervisor limitation. ARM64 remains a build
gate only; native execution and physical input/AT remain open.

Next implement immutable physical-line projection reuse for the bounded v0
grammar, with exact full-projection text/style/source-unit/ACP-affinity comparison,
unchanged fixed initial/edit trials, bounded mapping allocations and complete
preparation-failure atomicity. This supporting service must not be mistaken for
retained paragraph geometry, shared host scheduling or CommonMark conformance.
Then integrate those geometry/scheduling contracts with native/embedded caret,
scroll, input and UIA tests before completing D2a. Gallery fixtures, typography,
rendering behavior and consumer pins are unchanged by the source-only slice.
'@
$text+="`r`nThe six-slot schedule completes in $elapsed seconds within its declared 3,600-second overall / 900-second child limits.`r`n"
[IO.File]::WriteAllText($plan,$text)
$guide=Join-Path $Root 'docs/rich-text.md'
[IO.File]::AppendAllText($guide,@'

The [bounded source-change qualification](../Measurements/RichText/2026-10-18-source-changes/README.md)
retains all 57 ordinary x64 suite successes, six cross/native build profiles,
original failures and unchanged fixed operation methods. Source-change queries
allocate zero C++ bytes; localized transient previews retain about 23 KB in the
fixed 4,096-preview fixture. Numeric metadata has an explicit 188,664-byte history
fixture cost. These are functional/resource observations. Incremental geometry,
shared preparation and frame-performance acceptance remain open in the plan.
'@)
Write-Host 'Reconciled source-change plan/docs and final packet explanation. Compiled inputs were not changed.'
