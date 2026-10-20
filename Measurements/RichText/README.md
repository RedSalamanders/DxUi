# RichTextEditor validation summary

Workload owner: DxUi. Last updated: 2026-10-20.

PR [#89](https://github.com/RedSalamanders/DxUi/pull/89) adds an owned Unicode C++
rich-text core and retained native/embedded editor. This page summarizes the
source-specific results and remaining gates. Generated reports, copied source
trees, binaries, detailed logs and replay packets are kept outside the current
repository tree. They are not required source files or shipped dependencies.

## Implemented scope

- Persistent exact Unicode source, directional selections, transactions and
  undo/redo; cooperative preparation with cancellation and stale-result rejection.
- Lossless CommonMark/GFM analysis, semantic source/display maps, source caret
  sites, native HTML subset and source-exact structural/style commands.
- Native/embedded source TSF/IME and UIA adapters, rich semantic accessibility,
  task alignment, bounded footnote and native flowchart previews.
- Caller-serviced native image rendering and Unicode file-reference paste:
  images use Markdown images, other files use named relative links; unsaved or
  cross-root destinations use file URIs. One mixed paste is one undo transaction.

Full metadata/math/diagram conformance, larger-document capacity and the remaining
engine milestones are not complete. File drag-and-drop is a
[spec and test plan](../../Specs/Plans/WIP/RichTextFileDrop_2026-10-20.md), not an
implemented gesture. Current behavior is described in the
[rich-text guide](../../docs/rich-text.md).

## Functional qualification

The final pre-merge file-reference implementation is commit `19869eb9`; subsequent
`8fe2d5f5` changes documentation only. Its retained qualification records:

| Coverage | Result and limit |
| --- | --- |
| x64 Debug, Release, ASan Debug | 19 ordinary scopes per profile: 57 passes, zero capability skips |
| ARM64 Debug, Release, ASan Debug | Three successful cross-builds; no native execution claimed |
| ASan instrumentation | Fresh intentional heap-use-after-free probe detected |
| Paste transports | Native Ctrl+V/wide CF_HDROP and the application-owned embedded TSF adapter |
| Source behavior | Independent image/link roles and destinations, order, one undo, exact backward-selection restoration, literal contexts and stale-state rejection |
| Failure safety | All 56 observed mixed-transform allocations plus 94 pure path/codec/URI allocation points, with unchanged failed state and exact retry |
| Tooling/docs | Repository validators/tooling, pinned formatting and link checks passed |

The native-image component separately records six builds and 57 ordinary x64
scope passes, known-pixel/alpha/orientation/codec checks, real local-file worker
completion into WARP pixels, UIA alternatives, cancellation, limits and device
replacement. Its Debug failure schedules cover 441 observed allocation points.
These are component results tied to their original source and artifacts, not
qualification of a later merge or a performance waiver.

The parser components retain 1,324 pinned CommonMark/GFM semantic expectations,
2,125 independent entity values, Unicode 17 segmentation vectors and native/
embedded fault/resource checks. Unsupported syntax keeps exact editable source.
Passing a bounded preview does not establish every Markdown extension or general
large-document support.

## Performance investigation

**D4c remains HOLD.** The initial unpaired Default clean-frame p95 flags were
+5.02% and +17.05%; the fixture does not instantiate RichTextEditor. A later
12-block/48-process controlled investigation measured a -1.31% paired effect
(Holm-adjusted p=1), but all 24 same-binary controls drifted somewhere. This did
not establish causal regression or accepted non-regression.

Separate clear/composition/readback probes locate about 96.5% of the same
p95-ranked full-frame interval at the blocking Map boundary. That interval can
include deferred work, waits and preemption; it is not measured GPU duration or
scheduler delay. Coarse thread-state samples took 17–25 ms against sub-ms frames
and could not attribute a wait. Kernel scheduler tracing was refused by Windows
policy. Calling-thread WARP diagnostics remained Map dominated and did not
justify changing the production device policy. Original failures and outliers
remain preserved.

Current `main` also retains an unqualified performance acceptance policy. A PR
candidate cannot approve its own judge or policy. Complete independent policy
review and required matched candidate/CI evidence under the
[performance contract](../../Specs/Core/Core_PerformanceAndResources.md) and
[shared qualification ledger](../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).
No threshold, baseline, exact budget or historical finding is relaxed here.

## PR readiness work (in progress)

- Main `f65e4a4b` integrated at `2ceb1da5`; all five conflicts resolved, retaining
  both documentation indexes and all test registrations (141 unique native files).
- Fresh merged-source native/build/consumer/interactive CI and local targeted
  qualification are pending; earlier component passes do not substitute for them.
- Controlled main/candidate measurements and the required paired gate remain
  pending. Physical IME/AT, remaining resources/extensions and consumer adoption
  retain their own scope; no consumer pin is changed.

## Detailed evidence retention

The previous 19,074 RichText evidence files (396,274,723 uncompressed bytes) and
43 changelog fragments were archived before cleanup. Every one of the archive's
19,117 file entries was verified against its original Git blob identity.

- Source snapshot: `8fe2d5f51a6722bb4d784463783d55159e7715d5`.
- Archive: `richtext-details-8fe2d5f5.zip`, 177,483,690 bytes.
- SHA-256: `832534A2F2B448907C0AD0332E7DE853D488CC0DAC01D009D406B6E51D174B9F`.
- [Immutable component index and original receipts](https://github.com/RedSalamanders/DxUi/tree/8fe2d5f51a6722bb4d784463783d55159e7715d5/Measurements/RichText).

The independent archive and verification inventory are retained by the development
owner outside the repository. Historical links use that immutable snapshot;
current source, tests, usage docs and actual gallery images remain in the PR.
Future runs retain detailed packets privately or as CI artifacts and update this
summary with exact source/run identities, results and limitations.
