# Bounded source changes and localized Unicode previews

`SourceSnapshot::ChangedRangeFrom` now reports an unchanged source, a conservative
tracked replacement interval, or a complete-source fallback. Intervals preserve
UTF-16 scalar and CRLF boundaries in both snapshots. They reconstruct the new
source exactly, but may include unchanged text. Forward edits, undo/redo and
sibling branches use bounded numeric ancestry, without retaining expired source
buffers merely to recognize a change.

Qualified source fingerprint:
`3FEED65870EA98787EAC461CCB7AA6BA1CD9E5BC73C21684AB650B873517BAC4`.
Collection used HEAD a92bd7db with the recorded dirty inputs,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges` and MSBuild node reuse disabled. Document and
snapshot Release sizes remain 88/16 bytes. Source capacity remains 65,536 UTF-16
units and ordinary history remains capped at 128 transactions.

Immutable revisions carry one active 32-edit page and at most three sealed numeric
pages, with no source-root or page-parent chain. The available window is 97..128
edits after rollover, guaranteeing the latest 96. An atomic identity allocator
belongs to each lineage; reset creates another lineage. Reads allocate nothing,
mutate no cache and own no worker, window, timer or graphics resource. Staging
failures may leave unused identities but never partially publish source/history.

The OS supplies whole transient-preview text. The model compares existing source
fragments with that borrowed view, visits the suffix in reverse and copies only
the changed scalar/CRLF-safe insertion. Same-content previews update selection
without creating a revision, metadata, undo/redo entry or C++ allocation.

The original complete-source fallback implementation fails the new recent-change
oracle after collecting all fixed edit/read/single-backward trials. Native and
starter receipts both record expected exit 1. A supervisor attached about 97
seconds after launch, verified exact argv, creation time and tool-owned parent,
and enforced a 600-second child deadline measured from creation. The process
finishes in 256.699 seconds without timeout. The attached supervisor cannot
capture its exit code (null); its record is not an independent exit/drain receipt.
The planned 900-second overall budget was not actively enforced for that original
launch. All original records and this limitation are preserved.

The first metadata candidate passes all six initial source-change cases and the
EditorControls/Accessibility suites. Embedded exposes a valid remaining gap:
whole-source transient replacements return tracked but unnecessarily complete
intervals. Its exact failed predicate remains in Raw/FirstCandidate. Localized
preview splicing fixes the implementation, retaining the resource oracle and
adding a seventh behavioral case. The failed candidate is not relabeled as a
successful final run.

FINAL_MATRIX_PLACEHOLDER

| Deterministic C++ observation | Release | Debug / ASan Debug |
| --- | ---: | ---: |
| 512 change queries after 64 retired previews: calls / bytes | 0 / 0 | 0 / 0 |
| 4,096 transient previews of 8,192 units: retained bytes | 22,911 | 22,999 |
| Fixed transient retained-byte limit | 262,144 | 262,144 |
| Fragmented source + capped history retained bytes | 1,400,710 | 1,499,086 |
| Unchanged history retained-byte limit | 4,194,304 | 4,194,304 |
| Tracked byte delta after destruction | 0 | 0 |
| Same-content allocation calls / bytes | 0 / 0 | 0 / 0 |
| Page-rollover allocation failure points swept | 8 | 10 |
| Warm composites / C++ allocations | 1,000 / 0 | 1,000 / 0 |

Metadata adds 188,664 retained C++ bytes to the existing fragmented-history
fixture in each profile compared with the preceding committed implementation.
This cost is explicit; neither retained-byte limit was increased. C++ counters
exclude internal Windows/CRT storage. Failure injection covers every observed
rollover allocation, plus the existing replace/transient/reset/undo/redo sweeps.
Each failure preserves exact source, revision, selection, saved snapshots and
complete undo/redo history.

Seven new behavior cases cover localized edits/previews, scalar/CRLF joins,
selection-only no-ops, undo/redo/sibling branches, copied documents, page expiry,
reset, independent randomized Unicode reconstruction, concurrent immutable reads,
empty sources and moved values. Existing native/embedded input, accessibility,
Unicode corpus, maximum fragmentation and source-allocation tests remain covered.

The three measured operation methods match the actual retained baseline source.
The two unchanged storage-budget methods match the previous committed packet's
source copy; that origin is explicit rather than a reconstructed baseline file.
Five within-process trials give these descriptive Release medians:

| Operation | Original fallback | Final localized candidate |
| --- | ---: | ---: |
| 4,096 fragmented edits | 15.7147 ms | 16.6739 ms |
| 8,192 fragmented boundary queries | 0.5189 ms | 0.5270 ms |
| 32 single-backward queries over 61,440 units | 18.2838 ms | 18.7574 ms |

These are one fresh process per source, with five trials inside it. They show
metadata overhead in this sample; there are no same-binary controls, quiet-host
proof or causal/timing acceptance. All six canonical frame reports are original,
unpaired diagnostics. No fixture, judge, threshold, policy or baseline changed.

Full tooling/validators and pinned formatter results are retained separately.
`Verify.ps1` checks exact hashes, 57 native bindings, six build identities, both
failures, unchanged method bodies and resource/failure-injection observations.
`Verify-Arithmetic.ps1` recomputes the 30 descriptive observations and six medians.
A private one-byte altered copy must fail verification. SHA256SUMS seals all
files except itself; inventory records exact original-to-copy hashes.

The [clear/readback and scheduler study](../2026-10-18-uia-boundaries/README.md)
still locates the dominant measured boundary at blocking Map. Windows policy
refuses the scheduler trace; no trace or causal scheduler/GPU claim is invented.
Performance acceptance remains **HOLD**. Incremental/shared projection and geometry,
the full Markdown engine, physical input/IME/AT and native ARM64 execution remain
separate open gates. Gallery inputs and consumer pins are unchanged.
