# Owned Unicode rich-text editor

Status: ACTIVE — bounded retained preview, source TSF/UIA, Unicode 17 segmentation, persistent storage, source-change/projection services, guarded retained geometry and cooperative preparation are functionally qualified. Owned lossless Unicode block/inline grammar, specified GFM metadata and document-scoped node reconciliation are qualified as opt-in bounded analysis against all 1,324 pinned semantic expectations. Clear/readback profiling locates a blocking-Map boundary; external state queries are too coarse and Windows policy refuses scheduler tracing. Performance acceptance stays on HOLD. The complete engine and physical/platform gates remain open.
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

Current parser sequence (D4a; each stage requires code, independent native/embedded
tests and owning contract/docs updates before completion):

- [x] M0: Pin reference APIs and separate CommonMark 0.31.2, strict GFM 0.29-gfm
  and RichMarkdown grammar targets. Preserve original corpora/licenses; use own
  Unicode C++ implementation, with no foreign parser runtime or copied parser.
- [x] M1: Lossless source/block/lexeme partitions, explicit transformed-value
  affinity, bounded resumable root/lexical scans and opt-in host analysis.
  Code/tests/docs pass all 57 ordinary x64 suites, six profile builds and full
  tooling/format checks. See the [sealed parser-stage evidence](../../../Measurements/RichText/2026-10-18-markdown-parser/README.md).
  This qualifies root/lexical analysis, not complete CommonMark/GFM rendering.
- [x] M2: Complete dependency-aware base container/leaf blocks, exact content-line
  spans and conservative document/node reconciliation across structural edits.
  All 568 pinned CommonMark/GFM block-section structure/code/list-start oracles,
  source preservation, native/embedded scenarios, duplicates and Unicode edge
  identities pass. Capped lookahead/marker metadata and deferred container ends
  have deterministic scaling tests. Fallible node/container construction passes
  all observed Debug/ASan allocation schedules; the original noexcept failure
  and interrupted/failed attempts remain preserved. All 57 ordinary x64 scope
  obligations (55 fresh plus two exact reused), six root builds, eight failure
  probes, 324 exact raster pairs and full tooling/format checks pass. See the
  [sealed M2 evidence](../../../Measurements/RichText/2026-10-18-markdown-blocks/README.md).
  Source stays capped at 65,536 UTF-16 units; line/reference/compaction work is
  indivisible, and full inline/extension/semantic/platform/performance gates remain.
- [x] M3: Complete inline precedence and delimiters, code spans, links/images,
  autolinks, opaque raw HTML, line breaks and profile-specific GFM extensions.
  Lexical values are candidates until their context is resolved, never ready-made
  formatted text. Run independent semantic/HTML expectations for every original
  pinned example, retaining every failure and version difference. All 1,324
  original semantic outputs, 42 directed three-profile fixtures, unique padded
  cell IDs, parser NUL labels and native/embedded edit/history checks pass.
  All 57 fresh ordinary x64 scopes, six root builds, 12 probes, 324 exact raster
  pairs and full validation/format pass. See the [sealed M3 evidence](../../../Measurements/RichText/2026-10-18-markdown-inlines/README.md).
  Semantic native rendering/editing and the accepted extensions remain M4/M5.
  Resolved inline forests, global Unicode reference folding, extension options
  and opaque HTML tagfilter metadata are implemented. Native/embedded reference
  edit/undo/redo tests prove that unchanged block IDs cannot validate old inlines.
  Failed, interrupted and superseded attempts remain preserved separately from
  the final frozen qualification; see the detailed history below.
  The expanded standalone sample is built, captured and launched;
  its nine application/reference tests pass using its unchanged exact DxUi pin.
- [ ] M4: Semantic projection with source-backed/transformed/generated/object and
  repeated/reordered ranges, explicit primary editing affinity, atomic source
  transactions, structural commands and structured accessibility. Integrate the
  parser tree into the renderer only after independent coordinate/editing oracles.
- [ ] M5: Explicit front matter, footnote, math and Mermaid profiles, native
  editable source-bearing objects/alternatives, isolated offline diagram helper
  deployment and pending/error source presentation. Helper choice remains an
  active owner recommendation; unknown syntax remains exact source throughout.
- [ ] M6: Reconcile raw NUL admission with Document/codecs/clipboard/TSF/UIA;
  qualify cancellation/storage/pathological work and real capacity targets before
  raising the current 65,536-unit limit. Bound generated table padding separately:
  body-row count times header-column count can amplify a short exact source.
  The all-newline fixture does not establish a bound for such trees. Worker out-of-order tests are required if
  caller-shared execution is introduced. Physical/platform/performance gates stay
  independently open.

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

### HTML strategy

HTML recognition, standard conformance output and product rendering are separate
contracts. M2 recognizes the pinned dialect's seven HTML-block boundary rules,
including container termination and literal code contexts, and retains every raw
source unit. M3 resolves inline tags/precedence and dialect-specific filtering.
Independent expected HTML is a test oracle; the control does not use a browser DOM
or HTML as its native rendering engine. M4 will project an explicitly supported
safe subset through the owned native renderer. Scripts, event attributes and
remote resource requests do not execute; unsupported/active HTML remains visible,
editable source. This is an intended rendering boundary, not a claim that the
current v0 preview already renders HTML tags semantically.

The owned parser stage adds public `MarkdownSyntax.h` and canonical
`DxUi.MarkdownSyntax.cpp`, with no second projection implementation. The existing
RichMarkdown, RichTextEditor, RichTextAccessibility and supplied-device resource
tests own its core/native/embedded behavior; the generated original-corpus header
is in the native inventory. Standard entity data and original corpus bytes/notices
live under ThirdParty/Markdown and regenerate offline through the two owning
PowerShell generators. The ordinary preview remains the v0 renderer by default.

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

- [x] Retain bounded, fixed-count Release baseline trials with an independent
  boundary/count oracle before changing the bulk command implementation.
- [x] Preserve nearest-boundary snapping, zero-count behavior, endpoint crossing,
  actual signed moved counts and INT_MIN/INT_MAX safety. Traverse established
  boundaries forward; backward movement may count a prefix then visit its target
  in a second pass, without a new allocation/index/cache.
- [x] Test the original Unicode corpus at every offset/count boundary, large
  native/embedded provider commands, source/selection/history preservation and
  a deterministic C++ allocation budget. Retain descriptive before/after trials.
- [x] Reproduce collapsed keyboard deletion from a valid explicit ACP inside a
  grapheme. Preserve interior ACP geometry/explicit source selections, while
  collapsed Backspace/Delete remove the containing element. Cover both hosts,
  source/selection restoration and existing shared text-control callers.
- [x] Update contracts/docs and qualify affected suites and final platform matrix.

The fixed before schedule completed three fresh root-runner slots in 342.901
seconds. The unchanged bulk oracle passes; five 6,144-unit / 3,072-element trials
take roughly 82-90 ms per direction. These are descriptive timings, with no
same-binary controls or non-regression claim. A legacy surrogate-interior caret
reproduces invalid UTF-16 after Backspace. Both native and embedded rich fixtures
also fail complete-grapheme deletion from an interior scalar ACP. Every original
filtered failure receipt, native/root log, benchmark binding and slot outcome is
copied before the next invocation reuses its output path.

The implementation uses shared deletion-range policy, existing prepared stops for
rich queries, overflow-safe signed movement and at most three forward passes for
bulk movement. Official-corpus deletion/command oracles, maximal provider actions,
legacy/rich undo and C++ allocation budgets pass. The
[command qualification](../../../Measurements/RichText/2026-10-18-grapheme-commands/README.md)
retains three original failing runs, 57 passing ordinary x64 suite runs with zero
native skips, six attested builds, full tooling/validators and formatting.
Embedded records 5,167 checks and 1,000 allocation-free warm composites in each
profile. Maximal movement/deletion queries request zero C++ bytes. Five fixed
within-process trials have median forward/backward times 83.0029/82.7482 ms before
and 0.0579/0.0553 ms after. These are descriptive one-process-per-revision
observations, without same-binary controls or Default performance acceptance.
Gallery sample inputs, shaping and styles are unchanged; no gallery regeneration
or consumer pin update follows. The remaining engine and physical gates stay open.

### Remaining D2a storage sequence

After bulk command qualification, retain the fragmented-edit memory baseline
before replacing the descriptor vector. Immutable snapshots must keep sharing
unchanged tree paths and source buffers; no cache, worker or view-owned resource
is added. The 64-Ki/128-history bounds stay fixed until incremental preparation is
qualified separately.

- [x] Measure retained C++ bytes for fragmented source plus capped history,
  no-op replacement allocation and edit/read trials with independent text/history
  oracles. Keep raw failures and descriptive timings.
- [x] Move private snapshot representation behind its existing shared ownership
  boundary. Use a balanced persistent piece tree with bounded fanout, path copying,
  exact subtree lengths and boundary-local rebalance/coalescing. Reuse subtrees
  outside the edit; retain transactional failure behavior and concurrent reads.
- [x] Reject invalid selections and acknowledge same-content edits before
  cloning storage/history. Cover empty, cross-node UTF-16/CRLF, split/merge,
  deletion/undo/redo/reset, retained snapshots and hostile fragmentation.
- [x] Qualify deterministic retained-byte bounds, native/embedded behavior,
  all required configurations and specs/docs; keep incremental layout/shared
  scheduling and full large-document support explicitly open until implemented.

The next source slice also preserves the common single-backward-character path:
bulk negative movement currently counts a prefix and visits its target. A single
step can use the existing predecessor walk without that second traversal. Retain
the corpus/count oracle and a large-source single-step observation when changing
this path; the qualified bulk packet makes no single-step speedup claim.

The fixed storage-before schedule completed three slots: a root Release run of
the unchanged fragmented-read oracle and new edit/single-step trials, followed
by two isolated resource failures against the same attested library build.
The 12,288-unit fragmented source plus capped history retains 40,235,301 tracked
C++ bytes and releases its delta to zero. Same-content replacement requests
24 C++ allocations / 518,630 bytes. The predeclared candidate bounds are 4 MiB
retained and zero no-op allocations. Original failure logs, process hashes,
build identities and the root canonical report remain preserved. These focused
resource runs are separate from full native-suite qualification.

Persistent storage, virtual future-selection validation, early no-op admission
and the single-backward shortcut pass the first three affected Release suites
with zero skips. Tracked retained storage is 1,212,046 bytes; no-op replacement
requests zero allocations/bytes; all five allocation-failure sweeps pass.
Split/merge stress keeps eight independent
snapshots across resets and compares every source/boundary with a flat oracle.
Fault injection sweeps all allocation points of five source/history operations.
The private node change preserves SourceSnapshot/Document object layouts and API
revision 4; it does not establish binary compatibility with differently built
archives. Source-coordinated consumers must rebuild. No existing gallery inputs,
shaping, styles or consumer pins change in this slice. A final test adds 65,536
separate source pieces, middle cuts and 128 repeated growth/root-collapse cycles.
All six profiles are being qualified with these final frozen test inputs.

The initial matrix passes all 19 Release suites, including maximum fragmentation,
then Debug exposes two C++ iterator-proxy allocations / 32 bytes in the exact
same no-op method. Retain that failure and every initial scheduled/unattempted
profile separately. MSVC's owning string moves allocate Debug container proxies;
fault injection must also avoid their noexcept move/default-construction paths.
The repair borrows Unicode views synchronously, copies changed payload into owned
buffers, and constructs optional history staging directly from existing history.
It never retains caller views. Add borrowed/temporary input lifetime coverage,
retain the original measured method bodies, then rerun all six profiles. Header/
archive signatures remain source-coordinated; object sizes and API revision 4
remain unchanged, without claiming compatibility with an older built archive.

The [persistent-storage qualification](../../../Measurements/RichText/2026-10-18-piece-tree/README.md)
seals library fingerprint 8EE9BD21B0CE810E1B6B39EBE30D00EF62945764CDF52D8C9AB8D65D8B8EFD99.
All 57 final ordinary x64 suites pass with zero native skips, including 57 named
rich cases in each profile. Six successful build attestations, full tooling/
validators and formatting are retained. Embedded passes 5,222 Release checks and
5,234 Debug/ASan checks, with 1,000 allocation-free warm composites in each.
Retained C++ storage is 1,212,046 bytes in Release and 1,310,422 in Debug/ASan;
all profiles release the tracked delta to zero and request zero no-op allocations.
Every observed failure point in all five operations preserves complete history.
The original three-slot schedule completes in 254.052 seconds; the initial
six-profile matrix attempts two slots in 906.632 seconds and retains its Debug
failure and four unattempted slots. The separate six-profile recovery completes
all six in 2,039.092 seconds. ARM64 remains cross-build only.

Five unchanged within-process methods retain descriptive Release medians:
fragmented edits 202.858 -> 16.7215 ms, lookup 0.7445 -> 0.5166 ms, and 32 backward
single-step queries 32.5871 -> 17.6228 ms. The dormant allocation-failure hook is
added after the before run; there are no same-binary controls or quiet-host proof.
These observations do not clear Default frame acceptance. A current read-only
token check still lists no system-profiling privilege; no machine policy or
privileges change and no scheduler trace is claimed.

### Next D2a incremental-preparation sequence

- [x] Add immutable bounded change metadata between source snapshots, including
  transient bursts, undo/redo/branches, reset and expired history. Preserve the
  existing object sizes, no-op admission and 4 MiB fixture budget. Return safe
  conservative scalar/CRLF source intervals without allocation or mutable caches.
- [x] Add the bounded v0 line-block projection service with exact mapping, reuse,
  retirement, failure and retention qualification.
- [x] Integrate bounded retained paragraph geometry with the qualified projection
  service. Measure initial preparation and edits independently; preserve atomic
  source/geometry publication and caret/range/scroll behavior. Cancellation and
  stale scheduled-result checks belong to the following shared-preparation step.
- [ ] Integrate bounded host/shared preparation scheduling and resource ownership
  for native/embedded views; qualify clean/dirty/hidden/device-loss and two-view
  behavior before changing capacity or claiming large-document support.
- [ ] Complete the pinned Markdown/extension engine and rich transactions under
  D4a, with original corpora and explicit source/edit/diagnostic semantics.

Private preparation has collected the 652 CommonMark 0.31.2 examples, all 672
GFM source examples from the pinned 0.29.0.gfm.13 tag and their original licenses.
Two upstream task-list examples are marked disabled; preserve them for coverage.
This collection is not a parser implementation or conformance claim. The helper
decision and desktop timing question remain pending; independent preparation
and source services continue without either answer.

The next source-change tests reproduce the missing tracked interval against a
correct whole-source fallback. Original failing receipt, native/root logs,
canonical report and unchanged operation trials are preserved before metadata
implementation. The owned root child has a verified 600-second deadline from
creation; supervision attaches after launch, and its exit-code field is null.
The starting runner and original native receipt both retain the actual exit 1;
the guard records no timeout and 256.699 seconds. Do not represent the supervisor
as having captured an exit code or a drained output stream.

The implementation keeps immutable numeric pages beside the tree root, with a
shared lineage counter to distinguish branches and identity gaps after failed
staging. Pages retain no source roots or parent-page chains. The latest 96 edits
are guaranteed within a 97..128-entry window after rollover. A bounded common-
ancestor search handles undo/redo and copied documents; missing history/reset
uses whole-source fallback. Conservative intervals expand newly joined CRLF
edges so both snapshots can be sliced safely. Initial affected qualification is
pending. New resource budgets are zero C++ query allocations, at most 256 KiB
after 4,096 transient previews, and the unchanged 4 MiB history fixture limit.

The first metadata candidate passes all six initial source-change cases and
the ordinary EditorControls/Accessibility suites. Embedded confirms zero query
allocation, but fails the expected localized preview interval: ReplaceTransient
still replaces complete source. Preserve that original candidate's source,
receipts, logs and canonical report. Refine whole-text previews by comparing
common prefix/suffix across shared fragments, expanding scalar/CRLF boundaries
and replacing only the changed interval. Add explicit same-content, supplementary
scalar, CRLF, empty/whole replacement tests before rerunning affected suites.

### Bounded source-change qualification

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
The six-slot schedule completes in 1696.316 seconds within its declared 3,600-second overall / 900-second child limits.

### D2a line-block projection in progress

Implement PreparedProjection with one canonical v0 grammar, immutable local block
payloads and source/display prefix indexes. The first complete-source draft fails
the reuse oracle as expected after five fixed trials. Review removes an avoidable
whole-Projection copy before enabling reuse; a refined baseline adds actual flat
Project initial/edit comparisons. Both original expected failures, exact source,
compiler/build bindings and supervised process outcomes remain preserved. The
refined baseline completes in 259.765 seconds under its 600-second child bound.

The candidate widens tracked intervals to physical lines plus neighbors, reuses
outer payloads, shares adjacent equal source lines and publishes a staged state.
Source identity is checked independently of external revision. New coverage
compares exact display/source units/styles/ACP affinities, randomized Unicode
edits, branches, modes/reset/expiry, lifetime/retirement, zero-allocation mapping,
all preparation failures and fixed retained-memory limits. Initial affected
Release qualification is pending. The retained editor, DirectWrite paragraph
geometry and shared/cooperative host scheduling are unchanged; D2a stays open.
Gallery inputs and consumer pins remain unchanged by this supporting-core slice.
All three initial affected Release suites pass. Embedded observes 47 changed-
preparation allocation points, zero mapping/no-op allocations, 1,582,261 retained
C++ bytes for the 1,024-line editing fixture, and 3,083,454 / 9,571,134 for maximum
newline/unique-line inputs. All released deltas are zero; these precede the
snapshot slicing fix and are retained as preliminary evidence.

An isolated Debug probe then reproduces noexcept termination at snapshot slicing
allocation point 0. The original root build succeeds and the diagnostic child
exits its explicit 197 handler without a CRT dialog; both operations finish in
189.544 seconds. Preserve its exact source, streams, build binding and executable
hash. Replace the empty string's noexcept default constructor with its throwing
size constructor. A direct slice failure sweep and the complete projection sweep
must now pass before ordinary Debug/ASan suites are launched. Final qualification
uses all19 scopes in each x64 profile and all ARM64 configurations; no preliminary
Release receipt is relabeled as current after this source change.
### Fresh read-only Ghidra recalculation evidence

The [Office recalculation packet](../../../Measurements/RichText/2026-10-18-recalc-reference/README.md)
contains six actual MCP decompilations from the saved Office 16.0.20430.20032
reference, SHA-256 7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41.
An owned stdio MCP client sees the registered tools without raw plugin HTTP or
policy changes. The saved program opens read-only; its changed flag stays false
and the active /windows-edit/user32.dll program is unchanged. The Java script's
successful completion and actual nonempty pseudocode are independently checked.

InvalidateRecalc, background stepping, required recalculation, line recalculation
and viewport paths inform the next geometry/scheduling component. Treat private
fields/indirect calls as uncertain. The owned design distinguishes reusable dirty
preparation from full retirement and guards revision/binding/reentrancy. Do not
copy Office's per-control timer into DxUi; reuse existing host scheduling. These
new exports are evidence for the next implementation, not proof that it exists.
### Qualified v0 projection blocks

The [projection-block packet](../../../Measurements/RichText/2026-10-18-projection-blocks/README.md)
seals library fingerprint 574862352141C18EDBC27BEB54037E53B2566E8D8663891511F087872965B9F0.
All 57 ordinary x64 suites pass with zero native skips, all six build profiles
pass, and all four final isolated Debug/ASan allocation diagnostics return zero.
Full tooling, five validators and pinned formatting pass. Eight new behavior
cases preserve exact text, source units, styles and every ACP/affinity across
edits, line joins/splits, equal-revision branches, reset/expiry and retirement.

Snapshot slicing now propagates Debug allocation failure instead of terminating
inside its empty string default constructor. The original point-zero exit 197
is retained independently; the final slice sweeps cover one Release / two Debug
and ASan allocations. Complete projection sweeps cover 47 / 95 / 95 points and
prove prior-state preservation followed by successful retry. Four mapping/no-op
sweeps allocate zero C++ bytes. Each editing/maximum-source fixture releases its
tracked delta to zero and remains under its unchanged 4 MiB / 16 MiB limits.

Both refined-before/final operation methods remain byte-identical. Descriptive
block initial/edit medians are 1.1589/114.419 ms before and 1.2675/1.9308 after;
initial preparation is slower in this sample. Rebuilt source units across 128
edits fall from 3,921,152 to 11,520, with 130,688 outer slots reused. Flat Project
comparisons and the original unrefined draft are also retained. There are no
same-binary controls, quiet-host proof or frame/timing acceptance claims.

This completes only the supporting projection component. Retained geometry,
shared/cooperative host preparation, the full pinned Markdown/extension engine,
physical input/AT and native ARM64 execution remain open. Keep this plan WIP.
Review the current shared GraphicsDevice and owner preparation callback before
adding host services. DirectWrite formats may not be thread-safe; no assumption
of arbitrary-thread geometry follows merely from a shared factory. The control
still prepares full layout and no consumer pin or gallery input changes here.
The six-profile/twelve-operation schedule completes in 2088.375 seconds under its declared limits.

### Retained geometry qualification sequence

- [x] Preserve a fresh real native-control baseline with the original full layout:
  five fixed 26,624-unit / 128-edit preparation/raster trials, original executable,
  root build attestation, native receipt, complete logs and unpaired canonical
  performance report. Owned supervision completes in 517.991 seconds, exit zero,
  without timeouts or desktop activation. Initial times are 521–657 ms and edit
  times 50,898–51,271 ms. These are descriptive observations, not gate acceptance.
- [x] Qualify PreparedDocumentLayout against whole-document DirectWrite geometry
  and WARP pixels, then finish native/embedded publication/lifetime/resource tests.
- [x] Qualify zero-allocation layout retirement, clone isolation, all observed
  geometry allocation failures and maximum-source chunk/storage limits in x64
  Debug/Release/ASan, followed by all three ARM64 cross-builds and tooling checks.

The candidate connects the existing immutable source-line projections to retained
geometry in both ControlHost modes. Geometry chunks target 1,024 source units,
ending at physical lines; no published document may exceed 128 chunks. Source
changes invalidate public geometry immediately while retaining the previous
bounded graph for reuse. Host/font/mode/visibility retirement releases it.
Staged projection clones share immutable payloads, and all fallible geometry work
precedes joint publication. A nonfinal chunk excludes its phantom terminal empty
DirectWrite line; the final chunk keeps the real final line. Drawing visits only
viewport-intersecting chunks during host preparation. Prefix work remains linear,
and an unusually long source line still uses a whole-line layout.

Clear must not default-construct STL containers inside noexcept. Fallibly prepared
empty retirement buffers allow nonallocating swaps while retaining Content's
projection member identity. End on a trailing empty display line must remain at
its caret, including hidden-only final source. No new worker/timer/device or
canonical benchmark policy change is introduced. The complete shared scheduler,
full Markdown/extensions and physical IME/AT/native ARM64 gates remain open.

The first candidate failed compilation on an unavailable WIL helper; its complete
source and build failure are retained. The second candidate exposed a collapsed
visible-range error, while Embedded passed 7,828 checks and Accessibility passed.
After that correction, all 32 short geometry variants and edit/undo comparisons
passed, but maximum-newline height differed by roughly 1,100 DIPs and fractional
raster scaling differed in 2,656 bytes. Original raw images, artifacts, source,
logs and independent diagnostic outcomes remain under the current geometry work.
Four independent probes passed (edit publication, compaction/branches, native
reuse/retirement and fixed trials); height and raster probes failed independently.
The diagnostic driver initially searched stdout for trace markers; stderr proves
the selected cases started/completed, so retain that receipt limitation explicitly.

The next candidate keeps a conservative whole-layout compatibility path. Separate
chunk accumulation must match sequential DirectWrite float positions/baselines
exactly, without absolute-placement rounding; otherwise stage canonical whole
geometry and discard the incompatible chunk graph. Raster origin association or
non-axis transforms can request one whole-layout raster cache during owner
preparation. Whole-layout work/reuse/source units and fallback state are explicit
statistics, not credited as bounded incremental work. Reserve one of 128 resource
slots for this compatibility cache (127 chunks); old/staged graphs remain bounded.
Source-only changes may share a compatible immutable whole layout if display text,
styles, factory and typography match. Exact incremental rendering for every
rounding-sensitive document/transform remains an unchecked D2a requirement; this
fallback preserves behavior and does not close performance acceptance.

The fourth affected Release run passes EditorControls and Embedded (8,797 checks)
under source fingerprint 2DB200E3. All observed copy/chunk/whole-geometry failure
sweeps pass (9/265/359 allocations); clear and unchanged paths allocate zero C++
bytes. The 1,024-line/512-edit retained and replacement peaks are 2,844,161 and
2,942,318 tracked C++ bytes, and release returns to zero. Eighteen raster pairs
match. Its fixed 128-edit trials are about 53 seconds, so the fast unguarded draft
is not accepted as the implementation. This evidence is still affected Release,
not the final six-profile qualification.

The fifth affected Release run adds 54 exact WARP pairs spanning heading/rich,
source and styled no-heading profiles, LTR/RTL, DPI/scale and skew, and three
viewports. Source/styled 16-DIP cases explicitly assert actual chunk geometry and
first-view chunk drawing. It passes all selected cases and preserves 108 PNGs.
The real native diagnostic confirms 13-DIP body typography at 96 DPI uses whole
geometry/raster for the 26,624-unit fixture; its full source work is reported.
The original fixed timing method remains unchanged. Final Debug/Release/ASan,
ARM64 cross-build and tooling qualification now follows with inputs frozen.

The first final matrix passes all 19 Release scopes, then stops during the
isolated Debug geometry sweep at allocation point 173 (exit 197). Clear and
22 copy-assignment allocation points pass first. A separate local-symbol stack
probe identifies the named JoinBlocks return moving a Projection through a
noexcept Debug string move constructor. Preserve both failed operations, their
exact executables/PDBs, source and streams. The stack-probe wrapper contains an
obsolete snapshot-slice predicate; its child stack is diagnostic evidence, not
a successful wrapper or qualification result. Fill a fallibly constructed
Projection in place and prepare through the existing reference-taking helper.
Both chunk and forced-whole allocation sweeps must pass before ordinary Debug
and ASan qualification resumes. Remove the temporary DbgHelp import from the
test executable; canonical measurement inputs gain no symbol-reporting work.

### Qualified guarded geometry and observer-resolution closeout

The [geometry packet](../../../Measurements/RichText/2026-10-18-retained-geometry/README.md)
binds source fingerprint 4927C4EE251F2845B3BBD9E9534CF0B1C2C50A790A7C68345EAC3C38F9271B9C.
All 57 ordinary x64 suite executions pass with zero skips; all six build profiles
and four isolated Debug/ASan diagnostics pass. The twelve-operation matrix finishes
in 3,344.366 seconds with all 22 changed inputs byte-identical at start/end. Full
validation/tooling and pinned formatting pass. Exact WARP qualification preserves
54 pairs per profile, including actual chunk geometry/drawing assertions. Original
compile, visible-range, height, pixel and point-173 Debug failures remain retained;
the stack-probe wrapper's obsolete predicate is explicitly not a passing operation.

Release copy/normal/forced-whole sweeps cover 9/265/359 allocation points;
Debug/ASan cover 22/349/464. Clear, unchanged geometry and cached composition
allocate zero C++ storage. The 1,024-line/512-edit retained/replacement peaks are
2,844,161/2,942,318 bytes in Release and 2,980,281/3,078,982 in Debug/ASan. Maximum
distinct-line storage reaches 13,940,034/17,872,762 bytes under its fixed 24 MiB
limit; all tracked deltas release to zero. SDK-private heaps remain outside these
C++ counters. Native ARM64 execution and physical input/AT are not established.

The fixed native trial body is unchanged. Before/final initial medians are
522.474/521.053 ms; 128-edit medians are 51,102.6/53,381.5 ms. The default 13-DIP
fixture explicitly reports complete chunk and whole-layout work. This is a
behavior-preserving compatibility component, not an accepted speed improvement.
Keep that cost visible while implementing exact retained line placement/drawing;
do not accept the unsafe fast draft, loosen an oracle or silently rebaseline.

The [thread-state packet](../../../Measurements/RichText/2026-10-18-thread-state-resolution/README.md)
preserves Off/On/On/Off same-binary controls after build/tooling activity stopped.
All 1,280 frames and four populated captures match their invariants; all native
children exit zero. The native main-thread identity and common QPC clock are
verified. All 35 acquisition brackets cross or fall outside frame boundaries:
query p95 17–25 ms is too coarse for full-frame p95 0.45–0.57 ms. No Map state,
wait-reason, scheduler-delay or stable observer-overhead conclusion follows.
The raw outliers, empty aligned histograms, original helper-development errors
and unavailable source lookup remain retained. Kernel tracing is still refused
by Windows policy; the existing performance HOLD stays open.

Continue implementation in this order:

- [x] Investigate legacy flat Projection/default/move/copy failure safety in
  isolated native children before claiming a defect. Preserve an original passing
  sweep if the compiler's NRVO avoids the suspected flat-helper failure. A private
  constructor/probe draft is uncompiled and is not implemented support.
- [x] Move the five repeated native timing trials behind explicit diagnostic
  selection, preserving their body and final Release artifact/sample bindings.
  Independent behavior/resource tests remain ordinary. This removes roughly
  280/530 seconds of descriptive repetition from Debug/ASan iteration without
  reducing behavior coverage or changing the canonical acceptance fixture.
- [x] Implement a separate bounded WARP default/calling-thread-rasterization
  diagnostic using the documented SDK flag; test both policies and exact captures.
  Preserve default library/canonical device creation. Work placement/runtime
  changes are not a measurement of scheduler latency by themselves.
- [x] Measure independent whole-DirectWrite callback baselines, caret/range line
  placement and accumulation at 13/18/18.1 DIP, maximum newlines, wrapped/mixed
  fonts, RTL and fractional origins before selecting a retained-line adapter.
  All three bounded native probes are preserved with their original observations.
- [x] Finish exact retained line placement/drawing for qualified default,
  fractional and large-origin cases; retain independent geometry and pixel
  oracles. Inexact tiny-font/empty-suffix cases retain explicit whole fallback.
- [ ] Integrate shared/cooperative preparation under existing host ownership,
  cancellation, revision/binding and stale-publication contracts. Embedded mode
  does not acquire its own worker/timer; shared DWrite factory identity alone
  does not prove arbitrary-thread format/layout safety.
- [ ] Complete pinned CommonMark/GFM, lossless semantic source/display anchors,
  rich transactions and the accepted math/Mermaid/footnote/front-matter profiles.
  The original full specification's 1 MiB typing, 10 MiB loading and 100 MiB
  source-preservation targets exceed the current 65,536-unit v0 bound; they are
  not qualified large-document support. Reconcile expanded/generated/reordered
  display spans and NUL admission before claiming full conformance.

This closes only the guarded geometry/resource component and the coarse-observer
investigation. D2a, D4a/D4c/D4d and D5 remain active; keep the plan under WIP.
Desktop-time agreement is still absent, so focus-taking suites remain pending
under AGENTS.md. No consumer pin or gallery input is changed by this closeout.

Both packet verifiers and their one-byte negative integrity tests pass. The first
post-archive dependency validation reports 32 paths over the 150-character
consumer restore limit. Compact aliases retain original path mappings and every
byte hash; all five post-archive validators then pass. This packaging repair does
not change native source, results or performance acceptance.

### WARP execution-policy follow-through in progress

The diagnostic source now supports explicit Default/CallingThread raster modes
under a v2 boundary report, while absent mode preserves v1. CallingThread uses
the documented SDK profiling flag; GraphicsFixture, the canonical benchmark
closure, judges, thresholds and library device defaults remain unchanged.
Native requested/actual flags and exact capture equality must be validated before
interpreting the intervention. Schema/runner tests reject missing/contradictory
policies and preserve existing output on invalid option combinations. Alternate
PowerShell option casing is normalized before native dispatch. All 141 portable
profiling fixtures pass; this does not substitute for native execution.

Repeated timing is selected explicitly by its existing named test; the measured
body remains unchanged and independent ordinary behavior cases remain enabled.
An isolated flat Project fault sweep has been added to test the existing helper
before selecting a fix. Do not assume optional NRVO caused a failure until the
owned original diagnostic proves it. The constructor draft remains private and
uncompiled; no supporting API/source compatibility change has been adopted.
Next: native Debug build/original probe, then source-frozen scoped qualification,
bounded CLI/refusal cases and fresh WARP execution-policy controls.

The original flat helper passes all 29 observed Debug allocation points. Its root
build and isolated child both exit zero in 275.213 seconds; complete source,
streams, build identity and exact executable hash are retained. The library
fingerprint remains 4927C4EE with no src/include/build/dependency diff. Do not call
the named-return hypothesis a reproduced defect. Keep the sweep as ordinary
coverage and retain aggregate/value API compatibility; the private constructor
draft is not adopted. Native device-policy/CLI/profile qualification follows.

Preliminary owned Debug diagnostics validate both native device policies against
the original-probe artifact: requested/actual flags are 32/32 and 40/40, and both
populated captures exactly match SHA-256 6DF197E22101D1A71DAE8DFF1354B5B357D16E7E58D6EED74EA85BD14F9000BD.
Both children exit zero with complete drained streams. This is Debug smoke
coverage, not a root suite result or Release timing/acceptance. Full qualification
now freezes all source/test/spec/doc inputs across the declared profile matrix.

### Qualified WARP-policy diagnostic and explicit timing selection

The [policy packet](../../../Measurements/RichText/2026-10-18-warp-raster-policy/README.md)
binds the unchanged library fingerprint 4927C4EE and canonical fixture E9DE4125.
All six build profiles and 57 ordinary x64 suite runs pass, with zero skips.
Six isolated Debug/ASan children pass clear, copy/normal/forced-whole geometry
and flat projection sweeps. Flat Project covers 21 allocation points in Release
and 29 in Debug/ASan; no flat-helper defect is reproduced under this compiler.
The fourteen-operation matrix takes 2,828.066 seconds. All eleven changed inputs
match at start/end, and twenty-eight exact binary/runtime/symbol files remain
private. Full five-validator/tooling qualification and formatter 22.1.3 pass.
The value API and the unadopted private constructor draft remain unchanged.

Ordinary EditorControls logs in all profiles exclude the five timing trials;
independent native/embedded behavior/resource tests remain enabled. The explicit
Release selection passes its original five-trial body (SHA-256 38F50730), with
initial/edit medians 683.7/66,413.6 ms. Its first collector incorrectly read the
ordinary receipt name and fails after the root/native child exits zero. Preserve
that source, exception and child result. The filtered receipt/log and all five
samples are recovered without a native rerun; this evidence-only recovery does
not turn the original collector into a passing operation. These shared-host
times do not establish a speed change or regression.

Four malformed native requests return the expected failure before report/capture
output. Legacy v1 with an absent policy still binds the original three arguments,
zero-allocation/no-op/hidden invariants and identical populated capture. The
separate fixed Default/CallingThread/CallingThread/Default study completes four
fresh children in 129.766 seconds, preserving all 1,280 frames and identical PNGs.
Requested/device-reported flags match 32/32 and 40/40. The first full-work-window
p95 values are 0.7756/4.5538/4.6896/0.4622 ms. Both CallingThread runs remain about
99.6% Map at that p95 frame; they do not isolate application-main-thread execution
or scheduler delay. Mode names describe the requested SDK policy, not measured
thread ownership. Preserve every window/outlier; do not subtract window costs.

Selected-image snapshots outside the complete study record compiler activity
(one matching compiler gains 152.109 CPU seconds), without paths, command lines
or interventions. They cannot establish a quiet host, ownership or frame-level
attribution. Kernel tracing remains unavailable and the original HOLD stays open.
No library/canonical device, judge, threshold, baseline or consumer pin changes.
Continue the independent retained-line probe, exact retained drawing, shared
preparation and full syntax/semantic/editing implementation. D2a/D4a/D4c/D4d/D5
remain active; this is a supporting diagnostic component, not overall completion.

### Exact retained-line investigation started

An explicit `TestRichDocumentLayoutRawLineOriginDiagnostics` candidate now lives
in the existing document-layout test module. It creates independent whole
DirectWrite layouts rather than reading the owned chunk implementation. Bounded
preallocated callback storage records real glyph origins; metrics/ACP samples
compare actual positions with single-precision and double-prefix accumulation.
Cases cover 13/18/18.1 DIP, maximum newlines, plain/wrapped mixed fonts and RTL,
fractional/far origins and pixel-snapping options. The candidate is uncompiled;
the production retained renderer and geometry/pixel oracles are unchanged.
Next: compile/run the owned bounded Release probe and retain all observations
before choosing a line-placement/drawing change. No new performance claim follows.

The first root Release build/probe pass in 447.163 seconds. The native selected
case takes 0.337 seconds; exact source, executable/PDB, build attestation, filtered
receipt/log and canonical unpaired diagnostic are retained. Single-precision
sequential metrics match total height bits in all nine layouts. Simple prefix
baselines still differ in mixed-font glyph callbacks, and caret top differs at
some ACPs even in plain text. Do not adopt a prefix-only correction as exact.
The next probe records font metrics, bounded mismatch details, range top/height,
alternative operation associations and actual color-glyph interface availability.
Round-trip float output also preserves the far fractional origin explicitly.

The second root Release probe passes in 361.631 seconds; its actual ranges match
single-precision top/height in all nine cases, while caret and mixed-font glyph
arithmetic differs. The third independent experiment places a zero-width,
invisible first-line object at the intended document origin. All nine total-height/
caret comparisons and all 54 glyph-origin comparisons match exactly, including
snapping and far origins. Original data/source/artifacts for all three probes stay
retained. These are focused diagnostic observations, not full renderer qualification.

A production candidate now seeds each retained DirectWrite chunk with that private
positioning line. Its two synthetic UTF-16 units stay outside every source/display
map. DirectWrite retains its own caret/font/color drawing and origin association.
Reuse also requires identical document origin; height-changing edits may rebuild
the dependent suffix and must report that source work. Prefix-height mismatch
retains canonical whole fallback. Existing geometry/pixel tolerances remain
unchanged; tiny-font fallback, source boundaries, clear/fault/resource, actual
native/embedded use and exact WARP tests must pass before accepting the candidate.

The first positioned build fails because WRL prevents ordinary `new` on runtime
classes. Its full source/build/error are retained. Use `Make` and translate a null
allocation result to `std::bad_alloc` during staging; do not change the public
failure contract or treat that compile failure as native qualification.

The second candidate builds without warnings and passes the affected Release
Embedded/EditorControls suites in 322.233 seconds overall. The 54 actual/reference
WARP pixel pairs and unchanged geometry tolerances pass. The actual native body
font is 13 DIP: 26 retained chunks, 1,040 newly prepared source units and no whole
geometry/raster/preparation. Ordinary fault schedules cover 249 chunk and 361
whole-fallback C++ allocations; clear/unchanged queries remain allocation-free.
These affected checks do not substitute for the final configuration matrix.
Exact source, binaries/PDBs, root receipts, logs and all 108 PNGs remain private
under rich-line-origins-20261018/SecondPositioned pending the sealed public packet.

Next tests exercise inserted newlines, heading height and wrapped first-line
changes with explicit dependent suffix work, undo, hidden-only terminal blocks,
real user FFFC preservation, and the actual 13-DIP embedded editor at three DPIs.
Only after they pass will the unchanged five-trial diagnostic be measured again.
Shared/cooperative preparation, full grammar/semantic editing and physical/platform
acceptance remain active; this candidate does not close those gates.

The third affected Release candidate passes in 328.947 seconds, including all
new height/empty-suffix tests and the default embedded editor at 96/144/192 DPI.
Each embedded edit reuses 25 of 26 chunks and prepares 1,040 source units, with no
whole work. Source text-service snapshots contain exact original units. The
final test delta retains the original 54 WARP pairs and adds 13-DIP, fractional
18.1-DIP and large-origin profiles (108 pairs total), with nonempty final-viewport
checks and explicit no-fallback assertions. Native/embedded UIA scroll fixtures
also assert positioned geometry and exact source text. Qualification is pending.

The fourth affected Release candidate passes in 331.413 seconds: all 108 exact
pixel pairs execute, all final viewports contain actual reference ink, and every
profile uses chunk geometry/drawing. Native/embedded UIA scrolling passes on
default-font positioned layouts and exposes exact source. The fixed next schedule
freezes all changed code/tests/prose, executes the unchanged explicit five-trial
method once, then x64 Debug/ASan/Release ordinary suites with isolated Debug/ASan
clear/chunk/whole/flat failure sweeps and all three ARM64 builds. Focus-taking
suites still require desktop-time agreement. All original attempts/artifacts stay
retained; no accepted frame-performance or universal incremental-work claim follows.

### Qualified positioned lines and next shared-preparation component

The frozen final matrix passes all 14 owned operations in 2,338.031 seconds:
57 ordinary x64 suites (19 each in Debug, Release and ASan Debug), six isolated
Debug/ASan probes, and all six root build profiles. There are zero native skips,
build warnings or build errors. ARM64 results are cross-builds, not execution.
All 14 changed source/test/spec/doc inputs match their start/end hashes. Seventy-
four exact final build artifact files remain private with hashes; the earlier
failed WRL build and all three probe/four candidate attempts remain preserved.

Each x64 profile passes 108 exact WARP pairs (324 total), with original geometry
tolerances unchanged. The six font profiles include actual 13-DIP controls,
fractional 18.1-DIP text and large origins, both bidi directions, three transforms
and first/middle/end viewports. Every final viewport contains actual reference
ink. Native/embedded UIA scrolling exposes exact source text; embedded edits at
96/144/192 DPI reuse 25 of 26 chunks and prepare 1,040 source units without whole
geometry, preparation or drawing. Height-changing edits, hidden-only suffixes,
real FFFC source, undo, failed staging and retirement have explicit tests.

Clear/unchanged/cached composition retain zero tracked C++ allocations. Complete
Debug/ASan schedules exercise 22 copy, 335 chunk, 469 whole and 29 flat-projection
allocation points each. Release maximum-source C++ storage is 7,771,189/5,690,558/
13,290,725 bytes; the repeated-edit fixture retains 2,845,969 bytes with a 2,944,126-
byte replacement peak. All tracked release deltas are zero. Existing 24 MiB
maximum-source and 8/16 MiB retained/replacement limits remain unchanged and
exclude SDK-private heaps.

The byte-identical explicit five-trial body passes. Median initial preparation
is 145.909 ms and 128 accepted edits take 2,166.98 ms, versus the preserved original
522.474/51,102.6 ms. Keep every sample and earlier 53,381.5/66,413.6-ms observations
separate. These are descriptive shared-host results without same-binary drift
controls, quiet-host proof, a causal speed claim, 1 MiB p95 input-to-paint evidence
or frame acceptance. The original clear/readback/scheduler HOLD remains open.
Canonical benchmark/judges/thresholds/baseline and consumer pins are unchanged.

The [positioned-line packet](../../../Measurements/RichText/2026-10-18-positioned-lines/README.md)
records source, original failures, receipts, exact images and replay. Documentation
and gallery review finds no intended visual change: the independent whole-layout
pixel oracle is unchanged, and every new retained image matches it exactly. No
gallery fixture, typography token, theme, catalog entry or preview is changed.

Next implementation order, still active under D2a/D4a/D4c/D4d/D5:

1. Add cancellable owner-thread staged geometry advances with independent finite
   SDK-layout and reusable-block work budgets, preserving the last published pair
   on failure. Budgeted work units do not bound an indivisible SDK/parser call.
2. Integrate host wakeups/coalescing, document identity/revision/binding/typography/
   device keys and stale publication rejection. Audit every synchronous
   EnsureLayout caller, including input, TSF and UIA. Pending geometry must not
   silently drain the entire request. Embedded controls acquire no worker/timer.
3. Implement lossless semantic anchors, pinned CommonMark/GFM and accepted math,
   Mermaid, footnotes/front matter, then source-preserving rich transactions.
4. Qualify long paragraphs, capacity/latency, two views, clean/dirty/hidden/device
   loss, physical IME/AT and native ARM64. Focus-taking tests still require agreed
   desktop time under AGENTS.md; silence supplies no agreement.

Fresh actual Ghidra MCP reads separately verify eight recalculation routines from
the saved Office reference without changing its program or active user32 view.
StartBackgroundRecalc requests a host timer and has a synchronous failure path;
StepBackgroundRecalc's wrapper supplies no hard CPU budget. The owned design must
separate invalidation, pending work, advances, completion and retirement rather
than copying private flags, ABI, timer ownership or presumed thread safety. Those
reads are next-component evidence, not an implemented scheduler or HOLD closure.

Full validation (all five validators and tooling tests) and pinned formatter
22.1.3 pass in 352.743 seconds after native qualification. Packet replay and two
one-byte negative integrity tests pass. The original verifier's incorrect UIA
suite-log lookup is preserved; correction reads the existing passing Accessibility
log and changes no native source/test/result or oracle. Final document checks and
staged Git-only replay precede the local component commit; the full feature and
original frame-performance HOLD remain open.

### Cooperative preparation implementation, 2026-10-18

- [x] Stage cancellable document geometry with separate SDK-layout and block/
  placement budgets; preserve atomic source/geometry publication on failure.
- [x] Borrow ControlHost's live-tree traversal and native/application wakeups;
  coalesce one pending request without a control-owned worker/timer/queue.
- [x] Keep queries non-draining, retain source/IME transactions while pending
  and reject obsolete source/mode/binding/width/typography/resource results.
- [x] Cover budgets, unchanged reuse, compaction/whole fallback, cancellation,
  failure/retry, rapid typing, stale IME tokens, callback retirement, two views,
  native/embedded UIA and hide/zero extent/device/typography replacement.
- [x] Complete required native/build/resource qualification, validators and
  pinned formatter; preserve original attempts and reconcile specs/docs.

The core exposes BeginPreparation/AdvancePreparation/CancelPreparation. Direct
core Prepare remains a compatibility drain of those same phases. The control
uses owner turns (default four SDK layouts and 4,096 block/placement steps per
visible owner), never input/paint/geometry query drains. Aggregate host work
depends on owner count. Begin's v0 projection/index pass, joins, one physical
paragraph and whole compatibility SDK calls remain indivisible. Pending logical
commands may project source without DirectWrite. No hard millisecond, complete
parser or large-document guarantee follows.

Each owner holds one current pending request; immutable geometry can share the
published graph. Complete key checks precede acknowledgement, and successful
pixels precede text-service/UIA geometry publication. One outstanding application
notification is distinct from dirty pixels; failure has no self-retry loop.
Source typing, selection/history and revision-checked IME preview/cancel remain
available during pending geometry. Visual movement/scroll/hit/bounds wait for
current layout. Native WM_PAINT and embedded application continuations are both
tested. Embedded views acquire no HWND/device/swap chain/worker/timer.

Native window hide and either zero-size dimension explicitly retire content,
including unchanged-width/zero-height transitions. WM_SETREDRAW's asymmetric hide
retains the swap chain. Source/selection/history survive restoration. The initial
embedded first-Prepare notification convention is preserved. Reentrant Prepare
is rejected; scheduling callbacks remain enqueue signals under the embedded
ownership/reentrancy contract.

Qualification uses source fingerprint
4FF99EDD4EF7A76C5C346D5BBA3DD99A167C049987256265C548D8778C322A57.
All 57 ordinary x64 Debug/Release/ASan scopes pass with zero capability skips;
six isolated Debug/ASan failure probes and all six root build profiles pass.
ARM64 is cross-build evidence, not native execution. All 324 actual/reference
WARP PNG pairs are byte-identical, with unchanged 0.125-DIP geometry tolerance.
Copy/chunk/whole failure sweeps cover 22/338/471 observed points in Debug/ASan;
existing zero-allocation, 24-MiB source-fixture, 8/16-MiB retained/peak and zero-
released-delta oracles pass. SDK-private heaps are excluded from C++ counts.
The fixed 14-operation matrix finishes in 3176.330 seconds; full five-
validator/tooling validation and formatter 22.1.3 finish in 340.817
seconds. Evidence sealing/replay and final document checks are separate closeout
records in the [cooperative packet](../../../Measurements/RichText/2026-10-18-cooperative/README.md).

| Attempt | Observed result retained |
| --- | --- |
| CoreFirst | Core-only affected Release passes in 349.381 s; no later host qualification implied. |
| HostFirst | Protected lifetime-access compile failure, 375.262 s overall; original source/full logs retained. |
| HostSecond | Provenance refuses shortened MSBuild failure markers, 20.325 s; intermediate trees/hash mappings preserved, no policy changes. |
| HostThird | Build passes; old visual-navigation drain assumption and extra initial notification fail, 510.074 s. Original predicates retained. |
| HostFourth | Three affected Release suites pass, 557.366 s; subsequent native lifecycle gap remains unqualified by it. |
| FirstMatrix | Debug 19/19 and 108 exact pairs pass; owned ASan build intentionally stopped after hide/zero-height audit, exit -1, 854.246 s. Not a compiler failure/timeout or final-source qualification. |
| HostFifth | Four affected Release suites pass, 545.276 s, including the native lifecycle fix and WindowHost. |
| Final | Fresh frozen lifecycle-fixed source passes all native/build/resource obligations; earlier source results are not substituted. |

All failed/interrupted receipts, snapshots and private artifact hashes remain
preserved. The fixed timing method body is unchanged but its test-only preparation
helper closure changes; earlier trial observations cannot be pooled as an
unchanged-fixture causal comparator. Canonical five benchmark inputs, judges,
thresholds, tolerances, baselines, device policy and consumer pin remain unchanged.
The original frame-performance HOLD remains open.

Actual read-only Ghidra MCP calls retain eight recalculation routines from the
saved Office reference SHA 7f810b35cd2f818b70475d865ef743860b40cefe463e60d2f1c2de5bd2c6aa41.
Active/open user32 state and the saved program remain unchanged. Pending/step/
completion/retirement inform the owned design; private ABI/flags/timer/thread
assumptions are not copied. Raw warnings and exact exports remain in the packet.

The requested [MD4C](https://github.com/mity/md4c),
[Discount](https://github.com/Orc/discount) and
[cmark-gfm](https://github.com/github/cmark-gfm) public README/API/license inputs
are pinned by commit/URL/hash in the packet and discussed in the rich-text guide.
Callbacks, compile/output separation, AST/source positions and extension APIs
inform later work; abort/feed/finish do not prove resumable parsing. No parser
dependency or copied implementation is adopted.

This component is complete; the overall WIP plan stays active. Full lossless
CommonMark/GFM and semantic transactions, math/Mermaid/footnotes/front matter,
source-visible pending/error presentation, resumable parsing/long paragraphs,
capacity/latency, physical IME/AT, agreed focus-taking tests, native ARM64 execution,
consumer adoption and final performance acceptance remain explicit later gates.
Docs/gallery review retains the existing assets after review of the unchanged
catalog/example/styles and small one-turn gallery fixture. Independent exact
raster/geometry oracles validate the completed control; no baseline is rebaselined.

## M2 block-tree closeout, 2026-10-18

The owning code/test/contracts/docs are synchronized for the bounded syntax slice.
The source manifest and actual native artifacts are bound in the sealed M2 packet.
Its benchmark-source fingerprint is
`1238CEE85A031EA32CE6810F5F2FDF6066BCA33677E67AE22C1DB7097ED9B768`.
The packet includes original standards/notices, raw failures, independent block
oracles, exact raster pairs and offline integrity replay. Private binaries and
symbols remain under the owning .build report folder. Final prose closeout follows
qualification without changing compiled code, fixtures, judges or thresholds.

Snapshot ancestry distinguishes equal paragraphs; exact surviving nodes take
priority over enclosing-node fallback at deletion boundaries. Changed kinds,
reparenting and lost ancestry remain conservative. Block IDs are not geometry or
semantic-cache validity keys. The renderer remains v0 and analysis remains opt-in.
At the M2 boundary, M3 was next: inline precedence, global reference resolution and dialect extensions,
then independent full semantic/HTML expectations. D2a/M4-M6/D4c/D4d/D5 stay open;
there is no native ARM64, physical IME/AT, large-file or frame-performance acceptance.

## M3 implementation and preserved qualification history (2026-10-18 to 2026-10-19)

These attempt notes preserve their state at the time. The final M3 closeout below
supersedes pending actions recorded in the earlier attempts.

The owned parser resolves a dense inline forest and global references, plus
configurable GFM tables/task markers/two-tilde strike/extended autolinks/tagfilter.
Private staging contains no published delimiter tombstones. Original source and
the earlier lexical partition remain exact, and native pixels still use v0.
Extended links are resolved after bracket ownership and before remaining
emphasis; their own entity/backslash spelling stays literal. Supplemental pinned
cmark-gfm reference observations are retained with their implementation/spec
differences. No foreign parser code is linked into the library.

The completed fifth Release diagnostic has byte-verified frozen inputs and
passes all 1,324 original semantic outputs, directed cases across three profiles,
full EditorControls (27.088 s) and Embedded (17,576 checks). The parser failure
probe sweeps all 209 observed requests and 11 Begin requests, retaining the prior
publication and allowing retry. Maximum all-newline retained C++ storage is
11,929,389 bytes, replacement peak 26,317,605, released delta zero and unchanged/
retire allocations zero. This remains the same narrow fixture as M2, not an
all-input capacity claim. These diagnostics do not replace root qualification.

M3 closeout requires all 57 ordinary x64 obligations, six root profile builds,
Debug/ASan failure probes, exact existing raster comparisons, complete tooling/
format checks and sealed offline-replay evidence. D2a/M4-M6/D4c/D4d/D5 remain open.

The first root matrix attempt was interrupted during the Debug build after review
found missing VT tagfilter whitespace and over-broad slash boundaries. Its frozen
inputs, partial build log and owned-process interruption receipt remain under
`Interrupted1`; it is incomplete evidence. A shared core/native/embedded fixture
now requires filtering `<SCRIPT` plus VT and `<script/>`, while preserving
`<script/unknown>`. Restart the root matrix with the corrected frozen source.

Interrupted2 preserves a complete ordinary Debug pass and a partial ASan run on
the next source. Independent review then reproduced a bounds assertion from
`[r]: /url` followed immediately by `| - |`, using a private diagnostic linked
to a copied exact old ASan archive. Table-start reference extraction retired the
entire paragraph but left it active. Clear that retired leaf before the delimiter
line is processed; shared core/native/embedded cases cover definition-only,
real-header and quote variants. The fault fixture now also exercises this
retirement path and multiple-reference sorting. The fixed source needs a fresh
root matrix; these interrupted successes are not reused as final qualification.

Superseded3 retains the next complete six-profile/57-scope matrix and tooling
pass. A final independent probe then found duplicate identities when reparsing
two generated missing table cells with the same empty source range (exit 23,
duplicate identity 9 on revision 2). Reconciliation now keeps equivalent keys
in source traversal order and consumes each old identity once. Core repeated
revisions and unrelated edits, plus native/embedded edit/undo/redo and held
snapshots, require distinct stable cell identities. Preserve the completed
earlier matrix and private exact artifacts; qualify the changed source afresh.

Interrupted4 preserves the subsequent successful Debug build and four isolated
probes, plus the partial ordinary run and exact artifacts. A separate old-archive
probe exposed raw NUL reference labels not matching normalized inline labels
(exit 24). Normalize raw NUL before full case folding, retain exact source and
test this parser-only behavior under all three profiles. Document/text-service
NUL admission stays deferred to M6. No interrupted result closes qualification.

## M3 final qualification (2026-10-19)

The final frozen source passes every original CommonMark/GFM semantic output
(652 + 672), 568 independent block expectations, 42 directed three-profile cases,
parser-only NUL reference-label normalization and stable distinct padded table
cells. Native and embedded hosts cover exact source/history, reference edits,
undo/redo, independent views and held immutable trees. The native formatted view
still uses v0; this closes the bounded analysis slice, not semantic rendering.

All 57 complete ordinary x64 scope/profile runs were freshly executed without
capability skips; all six root builds, 12 isolated probes, 324 exact raster pairs,
five validators, complete tooling and pinned formatting pass. ARM64 is compile-only.
The qualified compiled library fingerprint is
`C00CA41A8251D688A8A29CE081E26A4E036214553EA0F028A0EC437F3F6605ED`.
The final source snapshot binds all 29 changed implementation/test/data/docs
inputs; prose/evidence closeout preserves the compiled and library identities.

Debug and ASan each sweep all 771 observed parser requests and 32 admission
points; Release sweeps 273 and 11. The unchanged maximum all-newline fixture
retains 11,929,557 bytes (Debug/ASan) or 11,929,389 (Release), with replacement
peaks 26,318,285 or 26,317,605. Released delta and unchanged/retirement allocations
are zero. These measurements do not bound every generated tree; table-padding
amplification and general capacity/pathological latency remain M6 obligations.

The expanded standalone default feature sample passes nine application/reference
tests and has a reviewed harness capture and verified launch. Its exact consumer
pin stays `302f3783a5d2ef60f201ca3abf508c1158fde039`; it remains separate v0 sample
evidence. Reviewed library pixels, catalog and design-system tokens did not change,
so the gallery is unchanged; the 324 exact raster comparisons cover this slice.
D2a, M4-M6, D4c, D4d and D5 remain open, including semantic rendering/structural
editing, accepted extensions, physical IME/AT/native ARM64 and the original
frame-performance HOLD. Foreground suites have not been run without agreed time.

The reviewed [M3 evidence packet](../../../Measurements/RichText/2026-10-18-markdown-inlines/README.md)
is sealed as `654233044FADADA33E8DB3A524161B511591A226CF73993EB61CB2BF4BEF4276`.
An independent exact copy and a Git-only staged archive both verify. Deliberately
modified native receipt and PNG bytes are rejected; restored/original packets
verify again. The [replay receipt](../../../Measurements/RichText/2026-10-18-markdown-inlines-verification.receipt.txt)
records the external seal, packet file count and extracted Git tree/archive hash.
Compiled-source identity is unchanged by documentation/evidence closeout.

## M4 resume checkpoint (2026-10-19)

Paused at the user's explicit request to save everything needed for later.
M4 is authorized but remains **not implemented**. Only initial inspection/design
analysis was performed in this turn: no M4 production/test code, baseline,
build, native/embedded run or qualification result exists yet. The next session
must not promote M3 results to M4 acceptance. The checkpoint is documentation only.

### Checkout and immutable starting evidence

- Work only in `Z:\src\DxUi-worktrees\rich-text-core-d2`, branch
  `codex/rich-text-prototype`. The source baseline is commit
  `e2956b94c5cfd587f3abd9c4087d937e298e64d9` (M3 complete). A later checkpoint-only
  commit does not represent additional implementation.
- `Z:\src\DxUi` has concurrent unrelated work. Do not switch, reset, clean, stage
  or overwrite that checkout. The conversation's default cwd is
  `D:\utils\ghidra-master`; use an explicit working directory for every repo command.
- M3 compiled library fingerprint:
  `C00CA41A8251D688A8A29CE081E26A4E036214553EA0F028A0EC437F3F6605ED`.
  Compiled-source identity:
  `4e26e1abf5fa51f80cd81612de176fd506b2b22820d99a3f4f845f1597cd4fcc`.
- The sealed M3 packet and external replay receipt linked immediately above are
  immutable. Its seal is
  `654233044FADADA33E8DB3A524161B511591A226CF73993EB61CB2BF4BEF4276`.
  M3 passed 1,324 original semantic expectations, 568 block expectations, 42
  directed three-profile scenarios, 57 fresh ordinary x64 scope/profile runs,
  six root builds, 12 isolated probes and 324 exact raster comparisons.
- Private matching artifacts/receipts remain under
  `.build/reports/rich-markdown-inline-20261018/Final`, `FinalArtifacts` and
  `FinalArtifacts.receipt.txt`; final commit proof is
  `.build/reports/m3-final-commit.receipt.txt`. Post-closeout full validation and
  pinned-format results are in
  `.build/reports/markdown-inlines-postcloseout-20261019`. Read timestamps/hashes
  before using any live `.build/logs` or ordinary scope receipt; old logs persist.
- The standalone sample at `Z:\src\richedit` is not a Git repository. Its expanded
  default source is `samples/FeatureShowcase.md`; executable is
  `build-dxui-x64-Release/Release/RichEditRebuild.exe`. Nine app/reference tests,
  a reviewed harness capture and a launch were verified; it is not claimed to
  be currently open. Exact consumer pin remains
  `302f3783a5d2ef60f201ca3abf508c1158fde039`, with the v0 renderer. Do not silently
  change any consumer pin or keep reopening a demo the user may have closed.

### Inspected boundaries and next file reads

Read root `AGENTS.md`, `Specs/README.md` and the owning architecture, performance,
controls/layout, embedded-rendering and input/accessibility contracts before
editing. The external `plan-execute-with-spec-update` skill was selected and read.
The local control-development, embedded-rendering, input-accessibility,
performance-resources, modern-cpp-wil, spec-workflow and build-dxui skills were read.
No instruction authorizes proactive agent delegation; none was used.

The following are inspected entry points, not completed M4 designs:

- `include/DxUi/MarkdownSyntax.h`: immutable M3 blocks/inlines, table/task metadata,
  original source and revision-local inline indices. Semantic values can be
  transformed even for Text. Enclosing `source` ranges do not map interior ACPs.
  Stable block IDs do not validate reference-dependent semantic content.
- `include/DxUi/RichMarkdown.h` and `src/Controls/DxUi.RichMarkdownProjection.cpp`:
  v0 `Projection`, `SourceUnit`, `StyleRun`, `PreparedProjection`, `ToggleMarker`.
  Inspect the actual `Projection::ToDisplay`/`ToSource` bodies and every mapping
  caller next; the current header offers only one scalar mapping in each direction.
- `include/DxUi/RichDocument.h`: persistent snapshots, source selection and bounded
  history. `Document::Replace` is an atomic source transaction; inspect its
  implementation before adding multiple source edits. Do not bypass its history,
  scalar/CRLF validation or failure guarantees.
- `src/Controls/DxUi.RichTextLayout.cpp`: `PreparedLayout::PrepareOwned` explicitly
  validates monotonically increasing `sourceToDisplay` and nonoverlapping ordered
  `units`, rejecting zero-length units. It currently applies bold/italic/code/
  strike/heading styles to one DirectWrite layout. Grapheme navigation and exact
  ACP geometry have distinct contracts. Clear uses preallocated retirement buffers
  to avoid throwing MSVC Debug proxy construction.
- `src/Controls/DxUi.RichDocumentLayout.cpp`: `State` owns v0 `PreparedProjection`,
  chunks and optional whole-layout reference. `SourceBoundary` binary-searches a
  monotonic source-to-display mapping; `SourceMatches` scans physical v0 blocks.
  Read the complete preparation and geometry/navigation methods before extending
  them. A naive nonmonotonic map substitution would invalidate these assumptions.
- `src/Controls/DxUi.RichTextEditor.cpp`: State owns document, parser, layout and
  current profile. PrepareContent first advances optional analysis, then stages
  geometry. `LayoutIsCurrent`, invalidation, cancellation, binding/revision checks,
  source TSF/IME, scrolling, navigation, selection and painting must remain coherent.
  Analysis alone currently preserves v0 pixels. Inspect range-rectangle and deletion
  paths carefully before integrating multiple display fragments per source range.
- `src/Controls/DxUi.Accessibility.cpp`: locate `AccessibilityTextRangeProvider`
  `FindAttribute` (around 5803), `GetAttributeValue` (5887), `GetChildren` (6649),
  and `TextUnit_Format` (11158). These locations may move. Bodies still need full
  review. Current normative rich attributes support IsReadOnly; structured/style
  semantics and rendered-only text are pending. Inspect prepared accessibility
  snapshots/part ownership before choosing a structured child-provider mechanism.
- Existing tests: `Tests/Controls/Scope.Tests.RichMarkdown.cpp`, RichTextEditor,
  RichTextLayout, RichDocumentLayout, RichProjectionBlocks, RichTextInput and
  RichTextAccessibility; `Tests/Embedded/Scope.Tests.RichTextResources.h`,
  RichGeometryResources and RichProjectionResources; shared Markdown fixtures
  under `Tests/Support`. UIA client tests must use the actual in-process client
  and embedded bridge rather than asserting provider internals alone.

### Open design decisions: resolve before implementation

1. Define an explicit semantic projection API and independent mapping oracles
   before renderer integration. It must represent source-backed, transformed,
   generated, object, repeated and reordered display spans, leading/trailing
   affinity and a documented primary editing occurrence. A single monotonic
   `sourceToDisplay` vector is insufficient. Source-range geometry needs a list
   of display fragments; taking only mapped endpoints can include unrelated text.
2. Define replacement/deletion behavior for entity expansion, escape decoding,
   code normalization, generated markers, hidden delimiters, image alternatives,
   repeated occurrences and reorderings. Reject ambiguity or use the declared
   primary affinity; never invent an implicit destructive source interval.
3. Decide how semantic geometry fits the existing staged layout service. One
   candidate is a separate semantic projection plus an explicitly counted whole
   DirectWrite layout path inside PreparedDocumentLayout; another is a dedicated
   semantic layout adapter. Neither was chosen or implemented. Preserve v0 reuse
   contracts and report any whole-source/layout fallback and resource cost.
4. Decide explicit public opt-in/default/profile behavior. An enabling API was
   considered so existing analysis-only callers retain their published contract;
   candidate names/options are not approved APIs. Keep profile/mode/revision/
   typography/device/binding request keys and cancellation/stale-result rejection.
5. Define native rendering for CommonMark/GFM blocks and inlines, including list
   markers/tasks, code, quotes, links, image alternatives and tables. Preserve
   unsupported HTML as editable opaque source or a documented safe native subset;
   no browser/script execution or automatic remote loading follows from M3 HTML.
   Math/Mermaid/footnotes/front matter remain M5, not invented M4 placeholders
   presented as completed features.
6. Specify atomic structural commands and history/selection behavior before
   implementing Enter/list continuation/exit, indent/outdent, task toggles,
   heading/style/link operations and table edits. Stage all source edits before
   one committed notification/history entry. Cover failures, read-only/disabled,
   reentrancy and stale semantic revisions. Retain exact unedited source/EOLs.
7. Preserve exact source ACPs for TSF/IME and existing source UIA ranges. Define
   structured UIA semantics, style attributes, Format/range behavior, children,
   bounds/actions and detach/disconnect using the same acknowledged revision and
   mapping as pixels. Generated text does not silently replace text-store ACPs.
8. Bound generated rendering output before allocation. M3 table padding can
   amplify header columns times body rows; the all-newline storage fixture does
   not establish an all-input bound. General capacity/pathological latency remains
   M6, but a semantic renderer still needs fail-safe admission and failure tests.

### Resume sequence and acceptance

- Check status/branch/current contracts, then retain a pre-M4 Release baseline on
  unchanged source with exact fixture/tool/dependency/artifact identity. No M4
  baseline measurement was made during this paused inspection.
- Add a detailed M4 checklist here; keep the single indexed plan. Implement map
  primitives and independent coordinate/edit oracles first, then native rendering,
  structural transactions, retained native/embedded integration and accessibility.
  Complete each component's code, meaningful tests and owning specs/docs together.
- Test decoded/multiscalar/supplementary text, scalar/CRLF/grapheme boundaries,
  hidden-only selections, backward selections, generated/padded content, objects,
  repeated/reordered spans and primary affinity. Cover cancellation/coalescing,
  stale parser/layout results, profile/mode changes, reference edits, undo/redo,
  allocation failures, callback retirement, two views, DPI/flow/resize/device loss,
  clean/dirty/hidden preparation and allocation-free composition.
- Use `Test-Changes.ps1 -Explain` and affected scopes during iteration. Required
  closeout includes x64 Debug/Release/ASan ordinary tests, three ARM64 builds,
  WARP/device-loss and measured clean/dirty/hidden/resource evidence for renderer
  changes, full `validate.ps1` and pinned `format.ps1 -Check`. Do not turn compile
  success or a focused run into whole-M4 qualification. Preserve failed attempts.
- Visual changes require deterministic harness captures, gallery regeneration
  through `gallery.ps1 -PublishDocs`, review of themed/embedded sheets, affected
  design-system previews and docs. Never substitute a desktop screenshot.
- Native/embedded synthetic and in-process UIA tests do not qualify physical IME,
  Narrator/NVDA, touch or native ARM64. Real-focus Menu, NativeTextInput,
  MenuResources and MenuResourceScaling require the person's agreement to the
  time. None was granted for this M4 turn; do not run them automatically.
- Existing benchmark observations are unpaired diagnostics. D4c performance HOLD
  stays open; no threshold/judge/rebaseline changes or causal speedup claim. Use
  same-machine controlled baseline/candidate evidence for new rendering costs.
- Environment used by prior root qualification: TEMP/TMP
  `Z:\DxUiPerfTemp\uia-ranges`, `MSBUILDDISABLENODEREUSE=1`. Discover Visual Studio
  through repository tooling. The observed pinned formatter is
  `Z:\src\DxUi\.build\format\clang_format\data\bin\clang-format.exe` (22.1.3);
  verify availability/version rather than assuming an installation.
- Keep M4 unchecked until its actual acceptance completes. D2a/M5/M6/D4c/D4d/D5
  and consumer adoption remain separately open. Keep naming/comments clear,
  update capabilities/changelog/usage docs truthfully, and do not modify sealed
  M1/M2/M3 evidence or silently adopt a new consumer pin.

## M4 resumed implementation (2026-10-19)

The checkpoint above is historical. Work resumed on its exact owned branch.
The first pre-edit SkipBuild attempt was refused for stale build attestation;
its inputs/refusal remain in `.build/reports/rich-semantic-m4-20261019/BaselineAttempt1Refused`.
A fresh root Release rebuild and complete Embedded/EditorControls/Accessibility
baseline passed on unchanged M3 source; logs, artifact hashes and receipts are
retained separately under `Baseline`. Its unpaired performance diagnostics
cannot close D4c.

### Chosen boundaries and ordered checklist

- [x] M4a: Add an independent immutable `SourceDisplayMap` with declared primary
  occurrences, leading/trailing affinity, source-backed/transformed/generated/
  object spans, fragmented range queries and atomic display-boundary snapping.
  Reordered and repeated spans are explicit; v0 Projection keeps its existing
  monotonic invariants. Validate UTF-16/CRLF boundaries, exact source-backed
  spelling, primary ownership, complete display coverage and bounded admission.
- [x] M4b: Add revision/snapshot-guarded multi-range source transactions. Validate
  ordered disjoint edits and future selection before a single Document.Replace;
  preserve unedited spelling/EOLs, one undo entry and strong failure guarantees.
  Test stale/equal-revision foreign input, no-op, undo/redo, backward selections,
  invalid joins, capacities and every observed allocation failure.
- [x] M4c: Build a cooperative immutable semantic document from M3 syntax with
  precise value-token maps (including entities, code normalization and stripped
  prefixes), block/inline style/role metadata and bounded generated output.
  Compare constructed values against parser semantics before publication.
- [ ] M4d: Stage native DirectWrite geometry for semantic blocks and table cells,
  using the same semantic map for drawing, caret/hit/selection/navigation/scroll.
  Keep all allocation/layout/raster work in preparation, and preserve existing
  v0 callers through explicit control opt-in. No worker/HWND/timer is introduced.
- [ ] M4e: Add structural list/heading/style/link/task/table commands through M4b,
  with source-exact history and read-only/disabled/stale/reentrant coverage.
- [ ] M4f: Publish acknowledged semantic structure/style through native/embedded
  UIA, preserving source ACPs for TSF/TextPattern. Test real in-process clients,
  format/range children, actions, bounds and provider retirement.
- [ ] M4g: Qualify native/embedded interaction, cancellation/staleness, allocation
  failures, WARP/device loss, resources, all required build/runtime profiles,
  validators/format and deterministic themed/embedded gallery captures. Update
  capabilities, architecture/input/layout/design-system/usage docs and fragment.

M4a and the core M4b transaction are pure preparation primitives; the retained
M4b adapter uses existing ControlHost lifecycle/notification services. This is not
a claim that the retained control renders CommonMark/GFM. Generated spans have empty source ownership and never
contribute source-selection glyphs. Transformed/object spans are indivisible
edit targets; repeated spans name one primary source occurrence. Display-range
queries return source fragments and deduplicate repetitions instead of replacing
an enclosing interval across reordered content. Source boundaries use explicit
affinity and do not assume global display order. Default semantic rendering
remains disabled until M4d/f can publish one coherent acknowledged revision.

### M4a/b functional evidence

`RichSemantic.h` / `DxUi.RichSemantic.cpp` implement the independent map and
transaction, with exact immutable snapshot identity in RichDocument. The retained
ApplySourceEdits adapter uses Changed/SelectionChanged and introduces no rendering
change. Native/embedded shared oracles cover hidden/generated/decoded/object/
repeated/reordered spans, affinities, scalar/CRLF admission, mixed EOLs, backward
selection, one undo/redo, stale/foreign plans, no-op and rejection atomicity.
Retained tests additionally cover parser/geometry invalidation, read-only/disabled,
notification exceptions, reentrant undo and callback owner destruction. Two real
UIA client tests observe complete committed source, selection, caret and TextChanged
through native and application-bridged embedded parents.

- Release pure-primitives candidate: complete EditorControls and Embedded passed;
  28 observed allocation failures injected individually. Maximum simultaneous
  65,536 source units / 131,072 display units / 65,536 spans retained 4,063,635 C++
  bytes and released zero net bytes; scalar coordinate queries allocated zero.
  Receipts/logs/hashes: `.build/reports/rich-semantic-m4-20261019/PrimitivesRelease`.
- Debug including retained adapter/client tests: complete EditorControls,
  Embedded and Accessibility passed, zero capability skips. Forty-four observed
  allocation failures passed; maximum map retained 4,063,795 bytes, released zero
  net bytes and scalar queries allocated zero. Receipts/logs/hashes: `RetainedDebug`
  in the same private report directory. Debug UIA source-plan client cases passed
  in both hosts. These observations exclude parser/SDK/driver/GPU storage.
- Attempt1 code-3 failures came from a new test fixture hard-coding revision zero;
  Document construction starts at revision one. Use the observed revision.
  Preserve `Attempt1Failed`, early probes and driver logs; corrected cases pass.
- Full validators/tooling and pinned format passed for the pure-primitives source
  snapshot; refresh after later adapter/builder changes. Remaining profile/platform
  qualification belongs to M4g. M4 stays unchecked, and the performance HOLD stays.
- Docs/gallery review: this component adds preparation/source transaction APIs,
  without changing active pixels/defaults; no new gallery capture is needed for
  M4a/b. M4d/f visual integration still requires complete regenerated/reviewed sheets.

M4a/b qualification completed against frozen code commit
`e160883bcb37e4bd22e767077a5475a81aa6c63b`: complete EditorControls, Embedded and
Accessibility passed in x64 Debug, Release and ASan Debug (nine ordinary scope/
profile runs, zero capability skips). ASan's isolated detection probe passed.
All three ARM64 builds passed; no ARM64 runtime result is claimed. Full
validate.ps1 (five validators plus tooling) and pinned 22.1.3 format passed.
The sequential Release/ASan/ARM64 driver enforced unchanged commit/source before
and after every job, retained raw logs, native receipts and artifact hashes in
`.build/reports/rich-semantic-m4-20261019/Qualification`; Debug is in RetainedDebug.
These results qualify this component, not the unfinished M4c/g integration or
physical IME/AT/foreground gates. Canonical unpaired diagnostics remain unpaired;
D4c stays HOLD. Do not reuse these artifacts as later M4c/d/f qualification.

M4c is the next active component. Its bounded semantic document must reconstruct
resolved values from exact source tokens, retain immutable role/style/table/task/
image metadata and reject unprovable interior maps. Native caret scaffolds for
empty/trailing/hidden source lines and grammar-aware deletion of normalized
line joins need explicit M4d/e tests; a scalar display endpoint alone cannot
represent all such source editing sites. Keep these as active implementation
requirements, rather than silently claiming that pure token mapping finishes
structural editing.

### M4c active implementation

RichSemanticDocument/Builder and the private RichSemanticValues helper are now
implemented in the owned source tree and under first-pass testing. They do not
activate retained semantic pixels. The planned 32,768 combined syntax-node and
4,096 geometry-paragraph limits fail before SDK allocation. Added tests include
independent directed display/source examples, dense graph/style/mapping checks,
cooperative scheduling/coalescing/cancellation/retirement and sparse-table/leaf
admission. A separate pinned-source test covers all 1,324 parser inputs, explicitly
rejecting inputs outside existing Document admission. Embedded tests add complete
and pending-Begin allocation sweeps, idle/retire checks and a maximum plain-source
storage fixture. No result is claimed until these new tests actually pass.

Keep the following M4d/e decisions active when integrating this data:
- Native source caret sites must cover empty/trailing/container-only lines and
  exact document Home/End independently of hidden glyph intervals. An empty
  Markdown semantic paragraph is not automatically an editable source line.
- Visual deletion of a normalized code/paragraph line join may require removing
  stripped continuation prefixes in one source transaction. Deleting only a
  decoded newline's raw units can expose a previously hidden quote/list prefix.
- A legal source ACP inside a transformed/object token needs an explicit atomic
  keyboard-delete rule. Programmatic source replacement and source TSF snapshots
  retain their existing exact-source contract.
- Generated marker/spacing hits use content insertion anchors; they must not
  insert a character between a task's closing bracket and required whitespace.
- Semantic admission/work failure must leave source editing usable, preserve
  publication pairing and avoid automatic retry loops; define and test source
  fallback/error status before enabling the retained renderer.

M4c first-pass Release evidence: complete EditorControls and Embedded passed,
alongside the separate three-case focused run. All 1,324 pinned source cases
constructed legal semantic maps (zero failures or source-admission exclusions).
The embedded builder passed all 198 observed complete-preparation allocation
failures and five pending-Begin failures. Coalescing/publication/retirement made
zero C++ allocations. The maximum plain-source publication retained 1,442,955
additional tracked bytes and released zero net bytes. This fixture excludes its
already held syntax and all SDK/GPU/process storage. Logs, native receipts,
executables, symbols, library and hashes are retained under
`.build/reports/rich-semantic-m4-20261019/M4cRelease`. Debug/ASan and later native
integration qualification are still pending; M4c stays unchecked for now.

M4c CPU preparation subsequently passed complete EditorControls/Embedded in x64
Debug and ASan Debug against frozen commit
`a045836a2549b6c76fecaee9e70296e0f42f5991`. Both instrumented profiles passed 514
complete-preparation and 15 pending-Begin allocation failure points, with zero
idle/retire allocations and full release of the maximum plain fixture. The ASan
detection probe passed. Raw logs, scope receipts, executables, PDBs, library and
runtime DLLs are retained in M4cQualification. M4c's checkbox denotes this pure
CPU component; all-current native rendering/ARM64/full validation still belongs
to M4g after integration. Earlier M4c Release counts remain in M4cRelease.

### M4d active native integration, 2026-10-19

The owned worktree now contains PreparedSemanticGeometry and a private retained
layout adapter. An explicit SetMarkdownRenderingEnabled option selects native
semantic preparation after the completed current parser/CPU document; defaults
and analysis alone retain v0. Source mode stays raw. Admission overflow records
the exact rejected syntax owner and prepares SourceFallback once, preserving
requested mode, source, selection and history. OOM/SDK/internal failure remains
a failed owner turn without automatic rescheduling.

Implemented geometry covers paragraph styles/headings, quote/list indentation,
equal-width table cells with physical alignment/RTL columns, rules and cell/code/
image outlines. Row placement, source line indexing and element bounds are
cooperative metadata phases; each turn limits SDK calls separately. Selection,
composition, text-service range rectangles and scrolling use actual source
fragments. Collapsed token deletion expands interior transformed/object ACPs to
one atomic source transaction. Source line units cover table cells and coalesce
wrapping atomic objects without reversed/overlapping ACP intervals.

First focused geometry-only Release tests passed (two named cases, zero skips),
with raw logs, exact source patch/new files, binaries, symbols and library hashes
in `.build/reports/rich-semantic-m4-20261019/M4dGeometryRelease`. This is backend
evidence only. Retained attempt1 failed compilation in the adapter; retain its
driver/build logs. Corrected code and complete EditorControls/Embedded/Accessibility
are under attempt2 testing; no pass is claimed in advance.

Added shared native/embedded retained tests cover opt-in/profile rejection,
current CPU/native publication, source view, hidden/object/table range rectangles,
RTL reflow, hiding/showing, raw document End, atomic entity deletion/undo and
admission fallback without idle retry. Embedded coverage also runs the shared-view
WARP clean/dirty/hidden/device-replacement fixture with semantics enabled, plus
owned allocation failure/idle/retirement sweeps. Two actual UIA client cases keep
raw source/selection/caret/events coherent with the new renderer. The populated
gallery tile and static design preview now describe the opt-in semantic path;
regeneration and visual review are still required after functional checks pass.

M4d stays unchecked. Remaining native work includes distinct empty/trailing/
container/padded source caret sites, hanging marker indentation, link styling and
the explicitly validated HTML subset. M4e still owns structural joins/format
cleanup, list/heading/link/task/table commands; M4f owns semantic UIA children and
style/format units. All-current x64/ARM64/resource/gallery qualification remains
M4g. This component does not close the D4c performance HOLD or other platform gates.

Retained attempt2 subsequently passed complete Release EditorControls, Embedded
and Accessibility (zero capability skips), including 119 individually injected
native-geometry allocation failures and zero idle/retire C++ allocations. Its
source patch/new files, executables, PDBs, library, raw logs and receipts are kept
in M4dRetainedRelease2. Full validate.ps1 (five validators and all tooling) and
pinned 22.1.3 format passed. These are the pre-navigation-correction artifacts.
Review found a Right-arrow affinity problem at generated paragraph separators;
the next candidate uses the next content's leading affinity, adds shared
native/embedded paragraph-crossing regressions and prevents retired layout-engine
publications from colliding in the retained generation counter. Fresh candidate
qualification is required after those corrections.

The frozen `0758311a65aeae6971b430f75c4020df94f90fdd` candidate passed all three
Release scopes, and Debug EditorControls/Accessibility. Debug Embedded exited 3
at the new geometry allocation sweep. Inspection identified an implicitly default
constructed source-line vector: MSVC Debug's noexcept constructor allocates its
iterator proxy, terminating rather than unwinding an injected allocation failure.
The correction uses the fallible empty-size constructor, including the affected
retained line-query/edit-plan locals. The existing complete geometry failure sweep
is the regression test. Preserve M4dQualification, its failed Debug binary/PDB/
library/logs/receipt and the passing Release archive; restart current qualification
against the corrected commit without treating this failure as a passing result.

The `d692329e41eb7b90e64f37b62d0f3329b7155b6c` constructor repair was incomplete:
an early Debug embedded probe and the ordinary Debug Embedded scope still exited
3. Keep M4dQualificationFixed and the early probe log. Debug EditorControls and
Accessibility passed. The semantic inline-bound pass additionally calls the
existing PreparedLayout::Range, whose named-vector return could perform a
noexcept Debug vector move when NRVO was omitted. The next repair uses an explicit
fallible copy/prvalue return and adds an isolated semantic-geometry fault probe
with bounded stack reporting and per-failure-point tracing. Run that focused probe
before restarting the profile matrix; do not promote either failed candidate.

The `84df13e8c1a04d3e77c6f5cb749ee760849faf59` range repair passed its isolated
Debug geometry probe: all 293 observed allocation failures preserve publication
and allow explicit retry; coalescing/scalar queries/retirement allocate zero.
The exact executable, PDB, library, trace and hashes are retained in M4dRangeProbe.
This is focused evidence, not full qualification. Inspection then extended the
same rule to retained optional-vector line/rectangle results, constructed with
in_place to avoid another nested noexcept move. Added v0 and semantic query fault
sweeps verify unavailable geometry, unchanged source/history/publication and
successful retry for each observed allocation, with an isolated trace option.

The new query probe on `b067114331c8548fefcad13d34e018cf6de1038e` caught the
same legacy defect in the v0 PreparedDocumentLayout::Range return at line 557:
engine 0, rectangle operation, allocation point 8 of 11 exited 197. LLVM
symbolization of its retained executable/PDB resolves the stack through the
vector move, PreparedDocumentLayout, retained adapter and text-service getter.
Keep M4dQueryProbeFailed (including the symbolized stack). The next fix returns
an explicit empty prvalue or fallible copy there too. The semantic geometry
probe still passes all 293 points on this candidate; the query failure remains
separate evidence. Rerun both isolated probes before the profile matrix.
