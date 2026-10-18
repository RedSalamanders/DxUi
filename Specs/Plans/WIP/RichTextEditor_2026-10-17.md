# Owned Unicode rich-text editor

Status: ACTIVE — bounded retained preview, D2d source adapters, source UIA range commands, indexed snapshot reads and shared Unicode 17 segmentation functionally qualified. Clear/readback profiling locates a blocking-Map boundary; Windows policy refuses scheduler tracing. Unstable controls and the unqualified policy keep performance acceptance on HOLD. Remaining D2a, complete engine and physical input/AT gates stay open.
Owner request: 2026-10-17. Starting revision:
`9f848b6464f166aa89229b80cd0e2ac22dbed42b`.

## Scope and current evidence

Build an owned Unicode C++ rich-text/Markdown editor informed by actual Office
RichEditD2D decompilation through Ghidra MCP. The standalone prototype lives at
`Z:\src\richedit`; its source, tests, specification and timestamped receipts are
external prototype evidence, not an existing DxUi capability. Its bounded v0
projection renders/edits headings and simple inline styles and retains unsupported
syntax. It does not qualify complete CommonMark/GFM/math/Mermaid or input/AT.

The production scope includes source/formatted/read-only views, CommonMark/GFM,
math, Mermaid, footnotes and front matter. Standards, helper/dependency policy and
conformance corpora must be pinned before declaring feature completeness.
Accepted future profiles remain CommonMark 0.31.2, a separate strict GFM
0.29-gfm profile, and RichMarkdown (CommonMark 0.31.2 plus the specified GFM
extensions/math/Mermaid/footnotes/front matter). The v0 grammar is none of those
conformance profiles. Normative library requirements are in
[controls/layout](../../UI/UI_ControlsAndLayout.md),
[embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[input/accessibility](../../UI/UI_InputAndAccessibility.md) and
[performance/resources](../../Core/Core_PerformanceAndResources.md).

The original checkout has concurrent unrelated work. This feature uses a
separate worktree/branch; do not switch, clean, stage or overwrite that work.
D2b added the reusable model/projection/layout to the canonical archive; that
slice left the retained control/catalog/gallery unchanged. D2c now adds the
retained preview while preserving public API revision 4.
The additive core has its own explicit bounded support/qualification status.

## Execution gates

- [x] P0: Own Unicode C++ prototype, actual Ghidra MCP exports, meaningful model/
  projection/layout/worker/file/native/app tests and real offscreen capture.
- [x] P1: Local x64 Debug/Release reference+owned tests, owned ASan tests and a
  deliberate heap-use-after-free runtime diagnosis. These qualify the bounded
  prototype only, not the library or complete engine scope.
- [x] D0: Inspect current library contracts and define migration ownership,
  preparation/composition, input/accessibility and acceptance sequence.
- [x] D1a: Isolate a feature branch and index this active plan.
- [x] D1b: Measure and retain exact Release baseline with unchanged fixture before
  modifying library code. Current migrated performance-policy approval and
  paired acceptance remain governed by their existing separate qualification.
- [ ] D2a: Review API/source boundaries. Replace prototype whole-source edit
  copies/single full layout and per-control worker with suitable document,
  incremental/shared scheduling and prepared-resource contracts.
- [x] D2b-core: Canonical bounded model/projection/layout and migrated behavior
  tests pass all 19 normal suites in x64 Debug/Release/ASan and all ARM64 builds.
- [x] D2b: Move reusable model/projection/layout into include/DxUi and src/Controls
  once. Replace WRL with WIL and follow stdcpplatest/SDL/WX/configuration rules.
  The standalone app then consumes the single pinned/development DxUi.lib;
  no sibling cpp lists or synchronized second source implementation.
- [x] D2c: Implement retained RichTextEditor using ControlHost's existing shared
  DWrite/D2D/theme/DPI/input/clipboard/invalidation services. Embedded mode owns no
  HWND/device/swap chain/thread/timer; native mode uses existing host services.
- [x] D2d-v0: Integrate native/embedded source TSF/IME, mapped accessible ranges,
  lifetime/rebinding and callback-retirement semantics. This bounded adapter gate
  now includes bounded source range commands. Rich structural/style semantics,
  semantic edit anchors and physical IME/AT acceptance remain open.
- [x] D3: Add public API/revision rationale, catalog/factory, populated gallery,
  docs/controls.md usage, design-system guideline/preview, current capability
  status, change fragment and scoped/native test inventory entries together.
- [ ] D4a: Complete strict pinned CommonMark and GFM profiles, source-preserving
  rich transactions and full independent corpora. Add accepted extension display,
  editing, preservation, diagnostic and worker-staleness tests.
- [x] D4b-v0: Bounded preview native + embedded control tests in x64 Debug/Release/ASan Debug and all
  ARM64 builds; distinguish native ARM64 execution. Validate/format/tooling and
  exact local/CI obligations under the scoped-testing contract.
- [ ] D4c: WARP/hostile state/two-view/shared resources, device-loss, clean/dirty/
  hidden/zero-size and failed-preparation evidence. Measure baseline/candidate
  serially on the same fixture with complete provenance and no silent rebaseline.
- [ ] D4d: Actual IME/Narrator/NVDA/physical input and focus-taking suites at an
  agreed desktop time through the existing lease; synthetic QI is insufficient.
- [x] D4e: Generate/review all affected themed gallery sheets with the harness and
  gallery.ps1 -PublishDocs; do not stage unrelated gallery edits.
- [ ] D5: Reconcile implemented support and remaining gaps in domain contracts/
  capabilities/docs. Close and move this plan to Done only after its gates.
  Consumer pins/adapters/product qualification remain separately owned.

## Proposed source and test map

Public core headers: `include/DxUi/RichDocument.h`, `RichMarkdown.h` and
`RichTextLayout.h`; retained controls remain on the existing DxUi control surface. Owned implementation: `src/Controls/DxUi.RichDocument.cpp`,
`DxUi.RichMarkdownProjection.cpp`, `DxUi.RichTextLayout.cpp`, and
`DxUi.RichTextEditor.cpp`. Projection/layout are additive public supporting APIs
because the standalone app must consume them without private-header access. Final naming is
reviewed against the existing source before publication.

Migrate behavioral tests into Scope.Tests.RichDocument, RichMarkdown,
RichTextLayout, RichTextEditor, RichTextInput and RichTextAccessibility. Add
Embedded.Tests.RichTextResources for preparation/composition, WARP, lifetime,
shared-device, two-view and resource scenarios. Register exact native test names
and affected-scope rules. Existing TextInputServices/TextStoreTarget and provider
helpers are reused/extended rather than copied.

Application persistence/dialogs/assets and file workers remain app-owned. The
prototype NativeControl HWND/device host never becomes an embedded control.
Composition must do no layout/raster/allocation/I/O/wait. Geometry/pixels are
published together; stale results and callbacks after retirement/rebinding are
rejected. Unsupported syntax always preserves visible editable source.

## Relationship to other work

The read-only Markdown help proposal under UI composition S3-S7 remains a
separate consumer/UX adoption item; this explicit owner request authorizes the
new rich-text workstream without closing those unrelated proposals. Existing
performance-policy/physical-input/consumer qualification ledgers retain their
scope. No old receipt is restamped as current integration evidence.

## Retained pre-change evidence

The [DxUi-only Release baseline](../../../Measurements/RichText/2026-10-17-prechange/README.md)
was collected successfully through the existing v2 build/measurement path before
any compiled library source changed. It is unpaired and exercises the existing
Default complex-UI workload; it is not a new-editor benchmark or non-regression
acceptance. D2-D5 remain pending.

Active worktree: `Z:\src\DxUi-worktrees\rich-text-core-d2`, branch
`codex/rich-text-prototype`. The first Z-volume build failed for disk space. A
hash-verified copy preserved all 33,894 files before Git worktree registration
was repaired. Old caches are archived outside the active worktree; the Z copy is
retained because automatic approval review rejected its removal. No unrelated
checkout files were reset, staged or overwritten.

## D2 core execution, 2026-10-17

The E volume later fell to about 1 GB free. The source was copied to the current
Z worktree and all 10,556 copied files (332,716,744 bytes) were SHA256-verified
before registration repair. The E copy and earlier Z recovery copy are retained;
no source recovery tree was deleted. Build outputs/dependencies are fresh on Z,
not relocated absolute-path tlogs. The relocation receipt is under .build.

The current slice migrates the bounded reusable core and its existing behavior
suite, improves edits to avoid complete old/new source copies, adds piece-aware
snapshot slicing/boundary inspection, WIL ownership, caller font settings,
projection validation and explicit prepared-layout retirement. Scope.Tests.Rich*
are registered under EditorControls; the supplied-device Embedded suite remains
required alongside the WARP borrowed-target core scenarios.

D2a's whole-document layout, indexed large-document storage and shared retained
preparation service remain unfinished; a pure core owns no scheduler and the
standalone application keeps its bounded worker. D2c/D2d are not closed by moving
the source: retained host, TSF/IME/UIA, callback/rebinding and composition need
separate implementation. This slice must not publish a RichTextEditor catalog
entry or claim full D2/Markdown/AT acceptance.

The prototype build is being changed to exact-pin normal consumer imports with
isolated dependency fingerprints, matching toolchain/CRT/architecture and no
sibling cpp compilation. The source moves once; old reusable implementation and
copied behavior tests are retired after canonical tests pass. Required remaining
receipts include x64 Debug/Release/ASan tests, ARM64 builds, validator/format,
prototype linked-archive smoke and the unchanged-fixture performance accounting.

## Reconstruction provenance

The independent prototype source identity before migration was
`a688833bc1081d2f84038c82be4d601a9f1ff0fa1cbcf031647e87d3ecfed633`.
Its source manifest and eight successful Ghidra MCP exports remain at
Z:\src\richedit\validation and reference/prototype. The Office reference was
RICHED20.DLL 16.0.20430.20032, SHA256
`7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41`.
Comments name observed range/history/D2D contracts and RVAs. This is an owned
behavioral implementation using public platform APIs; it calls no private Office,
MSO/PTS/LineServices or hidden Microsoft editing control. Markdown projection is
our source-preserving v0 grammar, not parsing code recovered from Rich Edit.

| Original prototype file | SHA256 before migration |
| --- | --- |
| include/RichText/Document.h | `9e6f3a7bec3eb0c0105d393d47c8cb0411fbc7083d067502c24083b34fd56b06` |
| include/RichText/Layout.h | `be9fe9456397f93c505f9c0f5e501aedee9494bb810619b7dd0c56ada9449842` |
| include/RichText/Projection.h | `7982da77f4db6d85d2833db99f56dfcf1f9bf4deb9e098c1b55ac0c64210af0f` |
| src/Document.cpp | `681e03bab8440e8619a9add1b569e9351f1dff4bf5bcaea4d4ef25854bfdae89` |
| src/Layout.cpp | `13f3cbb51d6ba35aab4d4914cc3595272bd2c190fc63ee6eb2c8793cbcfb210f` |
| src/Projection.cpp | `cea38a0ce83ea2d0ff2fa1be78ef79a9f5d189c3dbacd1ca5c38ee3a3a7a4ea1` |

## Core qualification checkpoint

[Reviewed core receipts](../../../Measurements/RichText/2026-10-17-core-d2/README.md)
retain exact source/artifact identities. All 57 normal native suite executions
passed with zero capability skips; all six builds and the intentional ASan
heap-use-after-free diagnosis passed. Ten named core scenarios are registered
and executed in EditorControls. All validators/tooling and pinned format checks
passed. The first two failed builds are retained and are not qualification.

Both existing-control performance comparisons returned advice-required. The
quiet repeat's warning set differed from the initial run; no causal conclusion,
non-regression acceptance, baseline replacement or additional pass-seeking repeat
was made. The existing fixture does not instantiate rich text. D4c retains the
paired study/attribution/owner-advice gate, plus full preparation/platform failure
injection and retained resource/device-loss qualification. This does not prevent
preparing the concrete source/core consumer for review; it prevents claiming
performance or full integration acceptance.

D2b's exact-pin prototype/public consumer gate follows the first source commit.
D2a's incremental model/layout/shared scheduler and D2c/D2d's retained/input/AT
work remain open. Ordinary focus-taking suites are explicitly unrun.

## D2b consumer closeout

Code commit 302f3783a5d2ef60f201ca3abf508c1158fde039 is the prototype's exact
qualified dependency pin. Its clean immutable checkout is consumer-owned under
Dependencies/DxUi/source/<commit>; later documentation/evidence commits on this
feature branch do not move the pin or break its next build. Normal consumer
props/targets build one archive with isolated toolchain/CRT/architecture outputs.
The old reusable cpp files and copied behavior tests are removed from the app;
only forwarding headers and application-owned worker/file/native host remain.

The relocated public consumer passes its frozen revision-4 API, standalone new
public headers, actual rendering, EXE/two-DLL ownership fixture and ten rejected
pin/build mismatches. Prototype x64 Debug/Release each pass nine tests, including
reference/parity and generated-project ownership; ASan passes four native app
suites and the specific deliberate heap-use-after-free diagnosis. All three
ARM64 consumer profiles cross-build. Cross-host runtime and wrong ASan Release
configuration are rejected. Direct canonical cpp enumeration is rejected even
when the pinned source lives inside the consumer tree. Restoring an existing pin
validates it without consulting or modifying another repository.

The application probe initially produced a different ASan access-violation after
/sdl was added; that failure is retained. It now uses the canonical DxUi
ownership-reset/borrowed-alias pattern and still rejects an arbitrary crash. The
actual linked-core renderer capture is byte-identical to the prior prototype.
Core fixture source/project/C++ bytes match the pinned checkout; the two JSON
test manifests are semantically identical after Git's LF normalization.

These consumer receipts remain distinct from the canonical 57-suite/code matrix.
At D2b closeout the bounded v0 core was complete, while D2a/D2c/D2d and D3-D5
remained active. Those receipts claim no retained RichTextEditor, complete
Markdown/extensions, TSF/UIA, native ARM64 runtime or performance acceptance. The performance advice-required
reports and original baseline remain unchanged.

## D2c retained preview implementation

RichTextEditor now uses the existing ControlHost services in both hosting modes.
Its implementation is DxUi.RichTextEditor.cpp and its public surface is DxUi.h;
it adds no private device/HWND/worker/timer or composition work. It uses the
bounded canonical model/projection/layout, shared host typography/colors, logical
focus/capture, clipboard and invalidation/animation. Unicode scalar buffering,
source selection, v0 formatting/history, bounded scrolling, cancellation and
notification retirement have named EditorControls and supplied-device tests.

Catalog/factory, populated gallery tile, design-system guideline/preview, usage
docs, capability and scoped/native inventories are included with this new concrete
control rather than waiting for a separate publication change. API revision stays
4: existing declarations and revision-4 consumer contracts remain compatible.
The advertised addition is retained-rich-text-editor-v0-preview; rich-text-editor
remains pending. SupportsTextInput is deliberately false for D2c; TSF/IME and rich
UIA ranges remain D2d. Incremental layout and full syntax/extensions stay D2a/D4a.

Before source changes, a fresh Release Default measurement at 1c6a660a was retained
under Measurements/RichText/2026-10-17-retained-d2c. The first attempt refused stale
SkipBuild attestation; a fresh build then hit MSB4166 worker exits. Its diagnostic
retry completed. These failures are retained separately, not counted as passes.
This existing-library workload does not instantiate the new control; dedicated
WARP resource tests qualify its preparation/composition boundaries separately.
Functional/build/gallery/performance closeout is recorded below after execution.

## D2c closeout

[The retained qualification](../../../Measurements/RichText/2026-10-17-retained-d2c/README.md)
records six final successful builds, 57 ordinary x64 suite passes with zero skips,
20 named rich-text scenarios per x64 configuration, 5,082 embedded checks and the
real ASan heap-use-after-free runtime probe. The final library source fingerprint
is C3C445AC41AF49947FA1B5144D214D8E3A6C9D7D07B44399CFFF54F7CD11219F.
Native ARM64 execution is not established by its three cross-builds. Full
validate.ps1/tooling and pinned formatting pass. All five themed 34-control gallery
sheets and supplied-device example were regenerated and visually reviewed.

Native/embedded hidden-root preparation now preserves layout retirement and hides
text; showing restores identical WARP pixels without replacing the view surface.
The host's empty default DirectWrite locale is accepted and regression-tested,
including RTL placement and invalid-direction failure atomicity. Clipboard and
notification callbacks recheck identity/revision/binding and may retire the owner.
No ANSI path or per-control HWND/device/swap chain/thread/timer is added.

The first serial existing-library comparison flags three timings; its one quiet
repeat is within-noise-budget against the same retained baseline. Both are kept;
this is not controlled paired or new-editor stress acceptance and does not close
D4c or the historic D2b warnings. D4b-v0 qualifies this bounded source only and
must be rerun when later engine/input behavior changes. D2a, D2d, D4a/D4c/D4d
and D5 remain open. The plan stays in WIP; consumer adoption is separately owned.

## D2d source input and accessibility execution

Use the existing shared native/embedded text store and immutable UIA publication
paths. Source ACPs include Markdown delimiters and original CRLF; rendered-only
text and structural/style accessibility are separate future scope. Preserve the
bounded preview and public API revision 4. Existing prototype Ghidra exports
remain design evidence; D2d integrates DxUi host contracts rather than importing
private Microsoft text-service implementations or introducing another engine.

- [x] Retain a fresh pre-change Release Default measurement at clean 0c939579.
- [x] Add atomic history-preserving Document transient replacements. Preview and
  rollback preserve undo/redo; hosts restore entry text before one committed edit.
- [x] Opt RichTextEditor into shared native TSF/IMM and embedded revision/focusId
  text input, with exact source state and mapped caret/range/hit geometry.
- [x] Draw clipped composition and conversion-target underlines during preparation.
- [x] Extend shared native/embedded UIA to Document/Text/Text2/TextEdit, source
  selections, actual backward-selection caret, mapped/visible ranges and read-only.
- [x] Add tests for transient/history and invalid services, production IMM and ACP
  protocols, embedded revisions/cancel/commit/retirement and application-bridged
  client navigation/text events. Tests use non-activating windows or no HWND.
- [x] Resolve exploratory build/test failures and freeze all compiled inputs.
- [x] Fresh x64 Debug/Release/ASan normal suites plus actual ASan runtime diagnosis;
  all ARM64 builds, with cross-build-only status explicit.
- [x] Validate/format/tooling, review generated composition/gallery captures and
  retain exact source/artifact/protocol/resource/performance receipts.
- [x] Close D2d's bounded source-adapter gate after evidence and reconcile docs.

Real IME/candidate-window/input-language, Narrator/NVDA, physical/focus-taking,
native ARM64 and controlled rich-workload performance acceptance remain D4c/D4d.
Unknown UIA text attributes and annotations remain unsupported. D2a incremental
layout and D4a complete Markdown/extensions are not closed by this integration.
The initial D2d gate did not qualify the legacy range scrolling and selection
add/remove operations. The source-command follow-through below now qualifies
those operations; rich structural semantics and real AT remain separate gates.

## D2d bounded source-adapter closeout

The [D2d receipts](../../../Measurements/RichText/2026-10-17-input-d2d/README.md)
bind six final builds and 57 ordinary x64 suite passes, with zero capability skips,
to library fingerprint FC6F53842E58EA9EB63181CC005E51913617C80DA6ACD194CE96649B7F6F6941.
There are 31 named rich-text control scenarios per x64 configuration; the embedded
suite passes 5,155 checks. Warm caret/visible-range metadata requests stay at
432 bytes (Debug/ASan) or 400 bytes (Release) for both 32 and 65,536 source units,
within the 4,096-byte budget. The actual ASan heap-use-after-free probe, pinned
formatting and reviewed gallery/composition captures are retained. All five
validators and tooling pass after an affected specification retry; the original
full command's documentation-link failure is retained. ARM64 receipts establish
three cross-builds only.

Both serial Default candidate comparisons remain advice-required. The first
flags private memory; its one quiet repeat flags dirty private bytes +7.6% and
working set +2.5%. Preserve both reports and the original baseline. The unchanged
83-control/1,000-row fixture does not instantiate RichTextEditor, and the baseline
and candidate differ in their temporary-directory toolchain identity. These
unpaired diagnostics establish neither source causality nor performance
acceptance. D4c remains open; no regression is silently accepted or rebaselined.

Native TSF/IMM protocol and real in-process UIA client tests qualify the source
adapters, not physical input-language/candidate-window or assistive-technology
behavior. D2a, D4a, D4c, D4d and D5 remain open; the plan stays in WIP and consumer
pins remain unchanged.

## D2d UIA range-command follow-through

- [x] Retain a pre-change Release Default baseline at 71059fb9. The first run's
  output changed Git dirty state and was refused; preserve it separately.
- [x] Implement owner-dispatched vertical range scrolling without selection or
  document/history changes, including hidden markers, clamping and retirement.
- [x] Implement contiguous selection union/subtraction and explicit rejection of
  disjoint results. Preserve backward selection and read-only selection access.
- [x] Replace text/attribute search stubs; exercise remaining range movement,
  cloning/comparison, visible bounds and empty-child contracts in both hosts.
- [x] Test native dispatch, embedded owner-thread actions, disabled/hidden/stale
  controls, callback retirement and real-client selection notifications.
- [x] Freeze inputs, run affected/full required profiles, validators and format;
  retain performance/resource receipts and reconcile specs/docs.

This slice completes the bounded source range-command contract, not semantic
anchors, rich style/structure, full Unicode grapheme/bidi navigation or real AT.
Gallery pixels need no regeneration: these nonvisual commands reuse the existing
scroll/selection rendering; the gallery's fixture and appearance are unchanged.

### Source-command qualification and performance hold

The [source-command receipts](../../../Measurements/RichText/2026-10-17-uia-ranges/README.md)
bind final library fingerprint
E3437165FCDB1374DE2AA8A2AE0AA791CB4581AB9528180ACA91D90508ECFDCA
to all six builds and 57 ordinary x64 suite passes with zero capability skips.
Each x64 profile covers 41 named rich-text scenarios, including 14 accessibility
cases. Ten new cases cover native/embedded selection, search, scrolling, remaining
range commands and thread ownership. Real UIA clients also invoke add/remove/
scroll and observe selection text/events. Embedded tests retain 5,155 checks,
zero-allocation warm composition and constant bounded range-query metadata.
ASan's actual heap-use-after-free probe, all validators/tooling and pinned format
checks pass. ARM64 remains cross-build-only; focus-taking/physical AT is unrun.

The serial Default comparison and its one repeat both flag clean-frame p95:
approximately 0.430 ms before, then 0.452 ms (+5.02%) and 0.504 ms (+17.05%).
Retain both failures and the original baseline. Fixture/toolchain/environment
identities match, but these are unpaired diagnostics and the unchanged fixture
contains no RichTextEditor; source causality is unestablished. Functional command
qualification does not accept this timing change. D4c and acceptance/merge stay
on HOLD pending developer advice under the owning performance contract.

The owner selected controlled baseline/candidate profiling. No performance
budget/tradeoff has been approved, and no threshold, baseline, consumer pin or
historical warning was changed.

### Controlled Default investigation

This is a focused diagnostic of the flagged Default workload, not full three-scene
qualification or approval of the unqualified paired policy. Freeze the existing
benchmark, judge, assignment generator and library sources throughout the study.
Baseline is 71059fb94c12c8966056e3afe626d1548ab1030b; candidate is
8c21a634c5abd55f14e384673e7d710de572bdcb. Reuse the candidate's attested Release/x64
build and let the existing paired runner create/restore/build its isolated baseline.
Use matching evaluated compiler profiles on the Z drive. The initial attempt
reused the original candidate-local TEMP/TMP path. Its first A/B pair exposed
different normalized TEMP/TMP properties across checkouts, so that partial attempt
is ineligible and retained. Correct the setup with one TEMP/TMP path outside both
trees, rebuild both Release/x64 outputs, and preflight full cross-side identities
before starting the replacement fixed study. This changes measurement setup,
not library source, harness, comparator, thresholds or the historical reports.

- [x] Inspect the owning performance/profiling contracts and preserve all original
  reports, including both p95 warnings. Confirm the Default fixture is unchanged.
- [x] Execute one predeclared 12-block Default study (48 serial fresh processes),
  independently randomized ABBA/BAAB with the generated seed and literal schedule
  retained. Keep both within-binary controls per block and every original report.
- [x] Follow with eight fixed diagnostic processes: heap-off ABBA, then heap-on
  BAAB, using the same two built executables. Keep raw CPU/thread/cache/heap
  telemetry, sampling costs, query failures and captures; no retries for outliers.
- [x] Compare run/block and per-frame tails, preparation/composition intervals,
  deterministic resource invariants and same-binary drift. Profile consumption
  without inferring actual scheduling delay, thread placement or heap ownership.
- [x] Retain the full evidence inventory and exact hashes, reconcile docs/plan,
  run affected documentation/tooling validation and record remaining decisions.

The first canonical attempt had a 2,400-second owned-runner budget including
isolated restore/build and was stopped on the cross-side identity mismatch.
Corrective Release builds have a separate 1,200-second setup budget. The fresh
canonical study has a 2,400-second owned-runner budget after setup/preflight;
the subsequent eight-process pilot has a 900-second overall
budget and the existing 120-second per-native-child limit. Retain partial failures
and unattempted slots if a budget or provenance check stops either sequence.
Do not run builds/tests concurrently with measured processes, change machine
power/priority/affinity policy, kill independent applications or silently replace
any scheduled process. These bounds and the two-scene qualification omission are
explicit diagnostic scope; acceptance/merge and D4c remain on HOLD.

The [controlled investigation](../../../Measurements/RichText/2026-10-17-uia-profiling/README.md)
completed all 48 canonical and eight diagnostic reports. Corrected preflight and
every measured identity agree. The paired clean-frame p95 effect is -1.31%
(Holm-adjusted p=1); both frozen judges agree on all 26 within-noise verdicts.
All 24 controls flag some drift, including 13 clean-p95 band crossings from
-14.33% to +14.41%. The original slowdown is not established consistently;
non-regression acceptance is also unestablished. About 98% of the same-frame tail
interval remains outside preparation/CPU composition in the combined bind/clear,
copy/readback/other-overhead remainder. Extra warm-up and heap profiles do not
isolate its cause. Exact resource budgets hold and all eight captures match.
The refused TEMP/TMP setup, original warnings and raw timing outliers remain
retained. Library/harness/policy and consumer pins are unchanged. Further causal
attribution needs a separate bounded boundary/stack/scheduler investigation;
this diagnostic closes no D4c, full-scene or trusted-policy gate.

### Clear/readback and scheduler follow-through, 2026-10-18

The next slice uses a separate opt-in `dxui-complex-ui-boundaries-v1` diagnostic.
The canonical benchmark, baseline, judges, bands and policy remain frozen. After
200 fixed dirty warm frames and a populated capture, eight 40-frame clean probes
run full, clear-only, composite-only, readback-only, then reverse order. All retain
no-op preparation, target binding and blocking one-pixel completion. Interior QPC
edges can be disabled to examine timer observer effects. Absolute QPC and main
thread creation identity permit external trace alignment; CPU intervals and
omitted-command controls are not independently measured GPU costs.

- [x] Retain a fresh pre-change canonical Release Default baseline.
- [x] Attempt a uniquely named bounded scheduler preflight without changing
  machine policy or touching other recordings. Corrected profile selection is
  refused with `0xc5585011`; preserve the initial selector error as well. No
  scheduler trace or scheduler-delay conclusion is available.
- [x] Implement bounded diagnostic schema/runner, validation and negative tests;
  qualify native commands and unchanged resource invariants.
- [x] Overlay the exact final diagnostic closure onto the pre-UIA-range baseline,
  build both sides under common external TEMP/TMP and verify full identities.
- [x] Predeclare eight fresh processes: timers-off ABBA, timers-on BAAB. Limit
  the overall pilot to 900 seconds and each native child to 120 seconds. Preserve
  all failures, outliers and unattempted slots; no builds/tests during probes.
- [x] Analyze per-frame boundaries and omitted-command controls, independently
  verify intervals/raw hashes, archive all evidence and reconcile these docs.
- [ ] Continue D2a/D4a and remaining feature gates after this diagnostic slice;
  no passing diagnostic closes the engine, trusted-policy or physical AT gates.

The [boundary packet](../../../Measurements/RichText/2026-10-18-uia-boundaries/README.md)
retains all eight processes (234.312 seconds), 2,560 native probe frames and
byte-identical populated captures. Full-frame same-p95 blocking-Map shares center
at 96.586% / 96.520%. Omitted-command and timers-off/on controls remain variable,
including B Off composite-only tails of 2.0695 / 1.1337 ms. This locates an API
wait boundary without identifying scheduler or GPU duration. Exact clean/surface/
allocation/hidden invariants pass; both overlays restore the baseline clean.
No production rendering change, regression waiver or acceptance verdict follows.

### D2a indexed snapshot lookup follow-through

- [x] Add independent fragmented-source, cross-piece UTF-16/CRLF, coalescing,
  snapshot/history-retirement and concurrent-read oracles. Capture a Release
  pre-change filtered run with fixed lookup trials under the root test runner.
- [x] Replace per-offset piece scans with binary-search cumulative piece ends,
  reusing the existing descriptor fields. Start slices and equal-range checks at
  their first relevant piece. Preserve atomic edits, snapshots/history, capacity,
  scalar/CRLF boundaries and no added descriptor storage.
- [x] Run affected native suites across x64 Debug/Release/ASan Debug, all ARM64
  builds, validators/tooling/formatting and retain before/after diagnostic trials.
- [x] Reconcile normative contracts, guide, capabilities and measurements.

This is a bounded D2a substep. Descriptor copying on edit, whole-document
projection/layout, indexed large-document trees, shared preparation scheduling,
full Markdown/extensions and physical input/AT qualification remain active work.
No visual/control/gallery state changes are intended by lookup indexing.

The [indexed-source packet](../../../Measurements/RichText/2026-10-18-indexed-source/README.md)
retains 57 passing ordinary x64 suite/log pairs with zero skips, all six builds,
the specific sanitizer heap-use-after-free probe, four unpaired canonical reports,
six post-change timer/profile branch diagnostics, the complete tooling/validator
run and formatting. The library fingerprint is 4EDDEF355659895319D81501089066D93E8F6885C2701A0C19C149C17C2EC014.
The unchanged 4,096-fragment oracle's Release lookup-trial median changes from
6.5310 to 0.7234 ms; this one-process-per-revision observation is descriptive,
without same-binary controls or acceptance. All 1,920 post-change diagnostic
probe frames pass clean/allocation/surface/hidden checks and six populated captures
match the earlier study. ARM64 remains cross-build-only. Existing bounded-v0
capability flags and gallery content are unchanged; consumer pins are untouched.

### Remaining Unicode and bounded-admission implementation

The Unicode step replaces the former approximate shared helper in
DxUi.SingleLineTextEditing.cpp with one pure DxUi.TextElements service. The rich
layout, existing text controls and UIA use the same pinned rule set; no parallel
rich-only segmentation implementation or OS/font-dependent navigation is added.

- [x] Collect and hash-pin Unicode 17.0.0 GraphemeBreakProperty, emoji-data,
  DerivedCoreProperties/InCB, the complete GraphemeBreakTest and license. An
  [reference PowerShell prototype](../../../Measurements/RichText/2026-10-18-unicode-preparation/README.md)
  generates 833 disjoint property intervals and
  all 766 original conformance vectors; its rule state machine matches all 766.
  This is reference/prototype evidence, not implemented native capability.
- [x] Integrate deterministic generated data and source/license provenance with
  generator verification tests. No floating upstream inputs or corpus exclusions.
- [x] Upgrade the shared allocation-free boundary walker and rich layout's
  navigation/hit stops; retain explicit source ACP/IME geometry semantics.
- [x] Add complete native conformance, malformed/bounds, shared editor/UIA,
  native/embedded rich navigation/deletion and prepared-resource tests.
- [x] Qualify code, tests, specs/docs and affected configuration/platform gates;
  distinguish logical grapheme support from full visual bidi/word navigation.
- [x] Bound DecodeUtf8 before result allocation. Reject impossible byte lengths
  and decoded sizes first; test admission and exact limits with an Embedded C++
  allocation-byte budget. This does not raise capacity.

Before changing the implementation, native Release tests failed on the original
corpus vector 1, offset 0 (old helper incorrectly joins an extender after a
control), and rejection of 65,537 ASCII bytes requested 131,127 C++ heap bytes.
Both failures are retained. After the fix all 766 vectors and both rich UIA host
cases pass; oversized rejection requests zero C++ bytes in both tested cases.
The [Unicode qualification](../../../Measurements/RichText/2026-10-18-unicode-graphemes/README.md)
retains 57 passing ordinary x64 suites with zero skips, six attested builds,
47 named rich-text cases per profile, complete native corpus checks and the
specific ASan diagnosis. Each embedded profile passes 5,161 checks and 1,000 warm
composites with zero C++ allocation. The forward 49,152-unit mixed-script walk
has 16,384 elements and zero C++ allocation calls/bytes; both oversized UTF-8
rejections request zero C++ bytes. The library fingerprint is
55DE3A71D32382F2DE7DC23D3A7E4EED6A3CDE40F93684C45FF83880F47E9C85.
The original ARM64 Release compilation succeeded but its attestation refused a
generator-message input change. Exact bytes were restored and the full compiled
input identity matches the successful native runs; two separate recovery builds
pass. Original attempts/refusal/unattempted slot and recovery are retained.
Validators/full tooling/formatting pass. ARM64 remains cross-build only.

The docs/gallery review found no changed gallery source, style, glyph shaping,
theme token or populated example. Only logical navigation, hit stops and mask
source indexing change; the existing gallery inputs retain their displayed
elements. No gallery pixel regeneration or consumer repin is part of this slice.

The Markdown parser/helper dependency policy remains an explicit active decision:
the owned control/rendering/editing services stay C++, while a pinned Unicode
grammar helper inside the single archive is being clarified with the owner.
Foreground Menu/NativeTextInput timing agreement is also pending. Neither pending
question blocks the independent Unicode/storage work or authorizes desktop use.

### Bulk logical character movement follow-through

Inspection of MoveTextRangePositionByUnit found that a multi-character command
calls the arbitrary-caret prefix-scanning helper for every step. This repeats
the same prefixes for both forward and backward movement. Fix this independently
of the Default readback hold; it does not identify that hold's cause.

- [ ] Retain bounded, fixed-count Release baseline trials with an independent
  boundary/count oracle before changing the bulk command implementation.
- [ ] Preserve nearest-boundary snapping, zero-count behavior, endpoint crossing,
  actual signed moved counts and INT_MIN/INT_MAX safety. Traverse established
  boundaries forward; backward movement may count a prefix then visit its target
  in a second pass, without a new allocation/index/cache.
- [ ] Test the original Unicode corpus at every offset/count boundary, large
  native/embedded provider commands, source/selection/history preservation and
  a deterministic C++ allocation budget. Retain descriptive before/after trials.
- [ ] Reproduce collapsed keyboard deletion from a valid explicit ACP inside a
  grapheme. Preserve interior ACP geometry/explicit source selections, while
  collapsed Backspace/Delete remove the containing element. Cover both hosts,
  source/selection restoration and existing shared text-control callers.
- [ ] Update contracts/docs and qualify affected suites and final platform matrix.

### Remaining D2a storage sequence

After bulk command qualification, retain the fragmented-edit memory baseline
before replacing the descriptor vector. Immutable snapshots must keep sharing
unchanged tree paths and source buffers; no cache, worker or view-owned resource
is added. The 64-Ki/128-history bounds stay fixed until incremental preparation is
qualified separately.

- [ ] Measure retained C++ bytes for fragmented source plus capped history,
  no-op replacement allocation and edit/read trials with independent text/history
  oracles. Keep raw failures and descriptive timings.
- [ ] Move private snapshot representation behind its existing shared ownership
  boundary. Use a balanced persistent piece tree with bounded fanout, path copying,
  exact subtree lengths and boundary-local rebalance/coalescing. Reuse subtrees
  outside the edit; retain transactional failure behavior and concurrent reads.
- [ ] Reject invalid selections and acknowledge same-content edits before
  cloning storage/history. Cover empty, cross-node UTF-16/CRLF, split/merge,
  deletion/undo/redo/reset, retained snapshots and hostile fragmentation.
- [ ] Qualify deterministic retained-byte bounds, native/embedded behavior,
  all required configurations and specs/docs; keep incremental layout/shared
  scheduling and full large-document support explicitly open until implemented.
