# Immutable v0 projection blocks and recoverable snapshot reads

`PreparedProjection` reuses immutable physical-line projections outside a tracked
source change widened to complete lines and neighboring context. The same v0
builder produces both flat `Project` output and block payloads. Source/display
prefixes index local offsets; adjacent equal physical lines share their payload.
Mapping preserves every UTF-16 ACP, display source unit, style and both delimiter
affinities. Source identity is independent of the caller's revision, including
sibling documents with equal numeric revisions.

Qualified library fingerprint:
`574862352141C18EDBC27BEB54037E53B2566E8D8663891511F087872965B9F0`.
Collection uses the feature checkout at HEAD c3117eb4 with recorded dirty inputs,
TEMP=TMP=`Z:\DxUiPerfTemp\uia-ranges` and MSBuild node reuse disabled. Source and
history limits stay at 65,536 UTF-16 units / 128 transactions. This is v0 syntax,
not CommonMark/GFM conformance. The retained editor still uses a full layout.

Preparation slices only widened changed context, then stages replacement payloads
and the bounded prefix index before one nonthrowing publication. Index rebuilding
is linear in physical line count. Missing ancestry, reset and mode changes safely
rebuild; semantically empty sources can remain unchanged across reset. Local block
revisions are zero; the service's revision/mode identify its published state.
Statistics count rebuilt/reused block slots and widened source units, not parser
calls. Clear releases the prepared state; moved/default/retired services reject
content/mapping queries. DisplayText explicitly materializes a flat copy.

The original complete-source draft records five fixed initial/edit trials, then
expected-fails the new reuse oracle. Review finds an avoidable complete-Projection
copy, removes it before reuse activation, and recollects a refined baseline with
flat Project comparisons. Both original root/native receipts, streams, source
snapshots, canonical reports and compiler bindings remain preserved. Supervision
starts at each owned launch: 259.911 and 259.765 seconds, respectively, within a
600-second child / 900-second overall schedule. These are expected exit 1
capability failures, separate from final suite success.

The first candidate passes EditorControls, Embedded and Accessibility in Release.
Its original receipts/logs/canonical report remain in Raw/FirstCandidate. Original
Release binaries are hash-verified and retained in the private build archive named
by its artifact receipt. The later diagnostic source snapshots include tracing;
they are not represented as exact copies of that preliminary Release test closure.
The critical core and measured source snapshots are retained separately.

An isolated Debug diagnostic then exposes an existing snapshot-read gap: failure
at allocation point zero terminates inside the empty string's noexcept default
constructor. The successful root build and exact native artifact bind the negative
child, whose explicit terminate handler exits 197 without a CRT dialog. Both
operations finish in 189.544 seconds. Slice now uses a throwing size constructor;
its observed failures propagate normally without changing source/history. The
direct slice and full projection sweeps remain ordinary suite tests. Only their
explicit diagnostic child flags install the terminate handler.

All **57 final ordinary x64 suite runs** pass with zero native skips across Release, Debug and ASan Debug. All six x64/ARM64 build profiles pass. The fixed schedule completes six profiles / twelve owned operations in 2088.375 seconds within 3,600 overall / 900 per profile; Debug/ASan each pass both isolated sweeps before their ordinary suites. ARM64 is cross-build only. The isolated ASan runtime probe actually diagnoses heap-use-after-free; its expected nonzero result is separate from suite success.

| Tracked C++ observation | Release | Debug / ASan Debug |
| --- | ---: | ---: |
| Four mapping/no-op sweeps over 512 lines: allocation calls / bytes | 0 / 0 | 0 / 0 |
| 1,024-line / 512-edit source + projection + history retained bytes | 1,582,261 | 1,708,341 |
| Editing fixture retained-byte limit | 4,194,304 | 4,194,304 |
| 65,536-newline source retained bytes | 3,083,454 | 3,083,670 |
| 32,768 unique-line source retained bytes | 9,571,134 | 13,503,390 |
| Each maximum-source retained-byte limit | 16,777,216 | 16,777,216 |
| Tracked byte delta after each fixture's destruction | 0 | 0 |
| Snapshot slice allocation points swept | 1 | 2 |
| Changed projection allocation points swept | 47 | 95 |
| Warm composites / C++ allocations | 1,000 / 0 | 1,000 / 0 |

The counters measure C++ storage, not internal Windows/CRT/DirectWrite allocations.
Eight new behavior cases cover exact full/block equivalence, all ACP affinities,
random Unicode edits and line joins/splits, undo/redo, modes/reset/expiry, equal-
revision branches, externally held payloads, moved/retired state and maximum source
and physical-line count. Failure injection checks every observed point, complete
prior-state and block identity preservation, then successful retry. The existing
source/history, Unicode corpus, native/embedded input/UIA and rendering suites remain
covered. No source capacity, resource limit, fixture, judge or policy is rebaselined.

Five within-process Release trials retain these descriptive medians:

| Operation | Refined complete-source baseline | Final candidate |
| --- | ---: | ---: |
| Flat Project initial preparation | 0.4196 ms | 0.3893 ms |
| Flat Project: 128 edits and preparations | 43.8677 ms | 35.7581 ms |
| Block service initial preparation | 1.1589 ms | 1.2675 ms |
| Block service: 128 edits and preparations | 114.419 ms | 1.9308 ms |

Block initial preparation is slower in this sample. Across 128 edits, widened
source units fall from 3,921,152 to 11,520, with 130,688 outer block slots reused.
The first unrefined draft's block initial/edit medians are 1.4038 / 158.617 ms;
all its observations remain available. The two refined-before/final measured
method bodies are byte-identical. The candidate also changes adjacent payload
sharing and snapshot string construction; there is one fresh process per source
and five trials within it, with no same-binary controls or quiet-host proof.
These measurements do not establish sole-cause attribution or frame acceptance.

All six canonical frame reports are original unpaired diagnostics. Full tooling,
five validators and pinned clang-format results are retained separately. The
verifier checks hashes, 57 native/performance/build bindings, twelve owned
qualification operations, four successful final isolated probes, all original
failures, unchanged measured methods and resource bounds. The arithmetic verifier
recomputes 50 observations, ten medians and rebuilt/reused work counts. SHA256SUMS
seals every file except itself; inventory records exact original-to-copy hashes.

The [clear/readback and scheduler study](../2026-10-18-uia-boundaries/README.md)
still locates the dominant boundary at blocking Map. Windows policy refuses the
scheduler trace; no trace or causal scheduler/GPU conclusion is invented.
Performance acceptance remains **HOLD**. Retained paragraph geometry, shared host
preparation, full Markdown/extensions, physical input/IME/AT and native ARM64
execution remain open in the [active plan](../../../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).
Gallery inputs and consumer pins are unchanged by this supporting-core slice.
