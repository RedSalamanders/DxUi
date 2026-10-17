# Owned Unicode rich-text editor

Status: ACTIVE — D2c/D3 bounded retained preview qualified. D2a incremental, D2d OS input/accessibility and complete engine gates remain pending.
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
- [ ] D2d: Integrate native/embedded TSF/IME, accessible source/display ranges,
  lifetime/rebinding and callback-retirement semantics before claiming support.
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
