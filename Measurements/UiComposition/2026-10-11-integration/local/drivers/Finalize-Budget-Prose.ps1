[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = 'C:/Users/eric/.codex/worktrees/performance-forms/DxUi'
$source = 'bf11ba44fbd661fd5e1e6015f95d5719a4bfba83'
$packet = Join-Path $root 'Measurements/UiComposition/2026-10-11-integration'
if ((& git -C $root rev-parse HEAD).Trim() -cne $source) { throw 'Wrong source for the final local packet.' }
if (-not (Test-Path -LiteralPath (Join-Path $packet 'budget-copy.receipt.txt'))) { throw 'Archive the reviewed original evidence first.' }
function Write-Prose([string] $Path, [string] $Text) {
    [IO.File]::WriteAllText($Path,($Text.TrimEnd()+[char]10),[Text.UTF8Encoding]::new($false))
}
$readmePath = Join-Path $packet 'README.md'
$text = Get-Content -LiteralPath $readmePath -Raw
$start = $text.IndexOf('## Final ARM64 ASan budget candidate: PENDING', [StringComparison]::Ordinal)
$end = $text.IndexOf('## Historical formatted run at 061748aa', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Expected pending final-candidate section.' }
$final = @'
## Final native and CI-budget source

The final native run binds clean source `bf11ba44fbd661fd5e1e6015f95d5719a4bfba83`, incorporating main
`ec680e866a218b57000a1780018c00101a437f91`. Its normalized CI workflow digest is
`7d4a1afd4c0a74ae8b93185f5ca4f78eda76d0eda4eff3e368bffd8c207730c8`; the coverage manifest binds that reviewed
workflow. Historical results at `061748aa` and `284a9ec6` retain their own source identities.

The CI repair raises only the ARM64 ASan enclosing job budget from 40 to 60 minutes after the retained job
exhausted its 40-minute bound. The [budget investigation](../../CiBudget/2026-10-11-arm64-asan/README.md) retains
the failed job, original inputs, annotations and actual timings. Existing 25/15/15-minute native/consumer stage
watchdogs and all eight consumer qualifications remain required. The native-scope tests pass 12/12, including
the six-profile budget check and its in-memory 60-to-40-minute falsification. Successful completion of the revised
CI job remains PENDING; the larger enclosing budget establishes no performance-policy or degradation acceptance.

| Final local profile | Focused correctness | Foreground suites |
| --- | --- | --- |
| x64 Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips in `local/budget-debug/` | All four full suites pass; zero skips; restoration verified in `local/budget-focus-debug/` |
| x64 Release | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips in `local/budget-release/` | All four full suites pass; zero skips; restoration verified in `local/budget-focus-release/` |
| x64 ASan Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips; deliberate sanitizer detection passes in `local/budget-asan/` | All four full suites pass; zero skips; restoration verified in `local/budget-focus-asan/` |

CONFIRMATION_RESULT All three leases verify foreground, keyboard focus
and pointer restoration. Both original and forced IME undo cases complete inside every full NativeTextInput
suite. Actual before/after and post-focus identities match; the lead independently rehashed the current
executables and checked raw completion, skip, delivery and restoration witnesses. These selected tests and four
suites do not establish a full repository pass. Canonical complex-UI reports are unpaired observations.

The library fingerprint remains `D1C3380BFDDAA3A1BD37534C666FF24A29C5936A27185DBFA4D26EE8B0D11A46`.
`budget-inputs-before.receipt.txt` binds compiled-input identity
`3dc3cdc29207fa2ea624d66a36f6516ce7fcd3cd5a32b0cbb99c35e1b95b73c2`.
`budget-archival-inputs/` compares all 24 actual compiler/SDK, dependency, artifact, harness and environment fields
across this evidence/prose archival, allowing only source commit/dirty bookkeeping to differ. `budget-copy.receipt.txt`
binds copied original bytes; `SHA256SUMS` seals the packet.

The first final-source Release foreground attempt retains four zero native exits but reports its saved RedXe
foreground window and keyboard focus `gone` at restoration. The root snapshot rejected it. Its raw receipts and
failed parent driver remain in `local/budget-release-focus-failed-attempt5/` and `budget-local-driver.failed.log`;
they establish neither successful restoration nor the cause of that window's disappearance. The separate
same-source, same-binary Release retry has its own admitted lease, receipts and verified restoration.

All five validators and tooling tests pass on the clean `bf11ba44` source. Final archival validation/format checks
are recorded at their own commit when complete. Synthetic-consumer outcomes, required ARM64 builds and exact-head
PR/merge-tree CI remain PENDING until their actual scoped receipts are reviewed. The broader earlier qualification
remains historical at its own source and does not establish final-candidate acceptance.

'@
$confirmations = foreach ($configuration in @('Debug','Release','ASan Debug')) {
    $lower = switch ($configuration) { 'ASan Debug' { 'asan' } default { $configuration.ToLowerInvariant() } }
    $lease = Get-Content -LiteralPath (Join-Path $packet "local/budget-focus-$lower/interactive-result.txt") -Raw
    if ($lease -notmatch '(?m)^confirmation: (started|autostarted)\r?$') { throw 'Missing admitted native confirmation.' }
    "$configuration`: ``$($Matches[1])``"
}
$final = $final.Replace('CONFIRMATION_RESULT', ('Recorded confirmations under the approved 30-second warning: '+($confirmations -join '; ')+'.'))
$text = $text.Substring(0,$start)+$final+[char]10+$text.Substring($end)
$text = $text.Replace('The final budget candidate requires its own pending source/provenance review above.',
    'The final `bf11ba44` budget source retains the same production fingerprint and its own reviewed native/provenance results above.')
$text = $text.Replace('Later `061748aa` results remain historical as described above; the final'+[char]13+[char]10+'budget candidate remains PENDING.',
    'Later `061748aa` results remain historical; the final `bf11ba44` source has its own three passing local profiles above.')
$text = $text.Replace('Later `061748aa` results remain historical as described above; the final'+[char]10+'budget candidate remains PENDING.',
    'Later `061748aa` results remain historical; the final `bf11ba44` source has its own three passing local profiles above.')
$oldEvidenceStart = $text.IndexOf('`061748aa` run, with its own formatted compiled-input/archival proof. Pending entries above', [StringComparison]::Ordinal)
$oldEvidenceEnd = $text.IndexOf('seal before closeout. Archived production inputs', [StringComparison]::Ordinal)
if ($oldEvidenceStart -lt 0 -or $oldEvidenceEnd -le $oldEvidenceStart) { throw 'Expected pending evidence-layout paragraph.' }
$replacement = @'
`061748aa` run, with its own formatted compiled-input/archival proof. `local/budget-<profile>/` and
`local/budget-focus-<profile>/` retain the final `bf11ba44` native run, with separate `budget-inputs-before.receipt.txt`,
`budget-archival-inputs/` and `budget-copy.receipt.txt` provenance. Each added raw receipt belongs in its original-copy
receipt and packet
'@
$text = $text.Substring(0,$oldEvidenceStart)+$replacement+[char]10+$text.Substring($oldEvidenceEnd)
Write-Prose -Path $readmePath -Text $text

$donePath = Join-Path $root 'Specs/Plans/Done/ConsumerUiImprovements_S0_S2_2026-10-04.md'
$text = Get-Content -LiteralPath $donePath -Raw
$start = $text.IndexOf('On 11 October the developer requested remote-main integration', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Expected dated Done integration paragraph.' }
$replacement = @'
On 11 October the developer requested remote-main integration of this completed implementation, with S3-S7 and
unfinished qualification retained in WIP. The [integration packet](../../../Measurements/UiComposition/2026-10-11-integration/README.md)
retains final native source `bf11ba44fbd661fd5e1e6015f95d5719a4bfba83`: each of three x64 profiles passes 20 selected
form/quiet/embedded client cases and all four full foreground suites, with zero skips and verified restoration.
Both original and forced IME undo cases complete. The discarded-edit reset has a controlled pre-fix failure;
original composition, two-undo and late-result assertions remain. Embedded notification ownership/coalescing and
the form client's message-aware owner wait have reviewed real-client evidence. Actual compiler/SDK, dependency,
artifact, harness and environment inputs are compared across archival; `284a9ec6` and `061748aa` remain distinct
historical runs. The final budget repair assigns 60 minutes only to ARM64 ASan while preserving the stage caps
and existing checks. Exact-head/base/merge-tree PR CI requires its own reviewed results. The dated
[validation decision](../../Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage) retains unfinished
performance, native-platform, physical-input and consumer gates without recording a successful `ci-gate`.
'@
Write-Prose -Path $donePath -Text ($text.Substring(0,$start)+$replacement)

$indexPath = Join-Path $root 'Measurements/README.md'
$text = Get-Content -LiteralPath $indexPath -Raw
$start = $text.IndexOf('- [S0-S2 current-main integration, 2026-10-11]', [StringComparison]::Ordinal)
$end = $text.IndexOf('- [Intrinsic layout/forms and Grid profiling, 2026-10-10]', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Expected integration index entry.' }
$replacement = @'
- [S0-S2 current-main integration, 2026-10-11](UiComposition/2026-10-11-integration/README.md): final native source
  `bf11ba44` passes three focused x64 profiles and four full foreground suites per profile, with zero skips and
  verified restoration. Actual inputs are compared across archival; `284a9ec6` and `061748aa` remain historical.
  Controlled IME/UIA records and the bounded ARM64 ASan CI-budget repair are retained separately. Exact-source PR CI,
  performance-policy, native-platform, physical-input/device and consumer adoption remain separately tracked.

'@
Write-Prose -Path $indexPath -Text ($text.Substring(0,$start)+$replacement+[char]10+$text.Substring($end))

$layoutPath = Join-Path $root 'docs/layout.md'
$text = Get-Content -LiteralPath $layoutPath -Raw
$start = $text.IndexOf('The [11 October integration packet]', [StringComparison]::Ordinal)
$end = $text.IndexOf('Native/embedded library tests,', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Expected layout integration paragraph.' }
$replacement = @'
The [11 October integration packet](../Measurements/UiComposition/2026-10-11-integration/README.md) binds final
native source `bf11ba44` to three passing focused x64/four-suite foreground profiles, zero skips, restoration and
actual input comparisons across archival. The `284a9ec6` and `061748aa` runs remain historical. Performance-policy,
native ARM64 runtime/foreground, physical-input/device and consumer adoption remain separate; PR CI requires
review at its own exact source/build identities.

'@
Write-Prose -Path $layoutPath -Text ($text.Substring(0,$start)+$replacement+[char]10+$text.Substring($end))
Write-Output 'Updated the final source-scoped integration prose; PR CI and unfinished qualification remain pending.'
