# S0-S2 intrinsic sizing and reusable forms: completed library implementation

- **Status**: DONE on 11 October 2026 for S0-S2 library implementation, normative/usage/design-system publication
  and recorded local x64 validation at the retained source identities. This is the completed portion of the original
  4 October UI composition plan; it does not claim validation of a later integration or merge tree.
- **Scope**: accepted measurement/ownership decisions, intrinsic measurement, opt-in Flex layout and stable forms.
- **Library owner**: DxUi. No consumer pin, adapter or product behavior was changed by this library work.
- **Remaining acceptance**: [UI composition qualification](../WIP/ConsumerUiQualification_2026-10-11.md) owns
  paired-performance/policy acceptance, native ARM64 execution, physical device/input qualification and consumer pilots.
  Those gates remain open; moving this implementation record to Done neither passes nor waives them.
- **Deferred implementation**: [S3-S7 proposals](../WIP/ConsumerUiImprovements_S3_S7_2026-10-11.md) retain overlay
  sessions, variable-height lists, read-only Markdown help, motion and theme authoring. They are unimplemented.

## 1. Completion scope and authority

The user accepted S0-S2 on 10 October 2026 and requested this implementation/adoption split on 11 October.
The original plan combined library delivery with later proposals and broader acceptance. This Done record closes
only the implemented library scope and its recorded local validation. The separate WIP qualification plan retains
all unfinished performance, platform, physical-input and adoption obligations from that plan.

S0-S2 reduce repeated measurement, field placement and focus bookkeeping while applications continue to own drafts,
validation, settings persistence, AV acknowledgements and document edits. They retain manual layout compatibility,
stable editor ownership and the existing native/embedded host model. Their supported APIs are in
`include/DxUi/DxUi.h` and [the layout guide](../../../docs/layout.md).

This document is a historical implementation/evidence record. Domain contracts remain authoritative.

| Area | Owning contracts |
| --- | --- |
| Ownership, compatibility, consumer pins | [Architecture](../../Core/Core_Architecture.md), [Consumption](../../Build/Build_ToolchainAndConsumption.md) |
| Measurement, forms, list realization | [Controls and layout](../../UI/UI_ControlsAndLayout.md) |
| Overlay routing, selection, Unicode, IME, UIA | [Input and accessibility](../../UI/UI_InputAndAccessibility.md) |
| Typography, theme resolution, motion policy | [Theme and typography](../../UI/UI_ThemeAndTypography.md) |
| Preparation, composition, visibility, device generations | [Embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [Win32 hosting](../../Rendering/Rendering_Win32Host.md) |
| Budgets and paired evidence | [Performance and resources](../../Core/Core_PerformanceAndResources.md) |
| Suites, platform and native qualification | [Validation](../../Testing/Testing_Validation.md) |
| Catalog, usage, gallery and publication | [Documentation](../../Core/Core_Documentation.md), [Design system](../../UI/UI_DesignSystem.md) |

The useful GPUI Kit ideas were intrinsic sizing, reusable fields and independent form/action layout, as described by
[Style](https://gpui-kit.com/docs/style/) and [Form](https://gpui-kit.com/component/form/). DxUi uses typed opt-in
primitives and keeps breakpoints, drafts and validation in application code. It does not import a CSS layout API,
GPUI entities or the Rust runtime. A full editor remains outside this work.

## 2. Historical consumer evidence and priorities

The inventory and priority table below retain the original planning context, including opportunities deferred to
S3-S7. They are source inspections, not current consumer pins or product acceptance.

### 2.1 Inspection provenance

Source and contracts were inspected on 4 October 2026. These are planning observations, not runtime qualification
or benchmark results. Consumer working trees were clean at the final provenance check. DxUi's shared checkout had
an unrelated menu-test change, so this plan was prepared in an isolated worktree from the main commit below.

| Repository | Inspected HEAD | Consumer DxUi pin / API revision |
| --- | --- | --- |
| DxUi | `27326cf2a7dbf844978747159154f99c04066eb3` | Library API revision 3 |
| RedSalamander | `fbe113a0f747f89af32d6fb337e4e67b9f5bdd6b` | `19ca44d95a0cd493fc7fb1a57f4d4ad227bdefc1` / 2 |
| RedXe | `71107b053be59af16c34272e82a7990386f84ca0` | `271bd54be24eadf3f03ba5221d1be7be069860f2` / 3 |
| RedPrism | `024380a1d00092a5a17462a963169b11e7073ba8` | `271bd54be24eadf3f03ba5221d1be7be069860f2` / 3 |

Every pin comes from that consumer's `Dependencies/DxUi.lock.json`. Recheck these identities and the consumer's
active plan before adoption. A source inspection at a newer library revision does not establish that an older
consumer pin supports a proposed API.

The S0 recheck on 10 October found DxUi base `60e1cbff8f7a75757e9447073965d09101d9833c` at API revision 4;
RedSalamander HEAD `6442f882da827be59f62f36759c5aff49c2091c8` pins `6c436c213ac0fac181b2faafd1daf6085bdedc23`
at revision 3; RedXe HEAD `7b38d4bb3d64a55e24ec3b601cd79a0007651204` and RedPrism HEAD
`97dd25b70e83f81c339a4b4e7e7b7589268f2510` both still pin `271bd54be24eadf3f03ba5221d1be7be069860f2` at revision 3.
RedXe has unrelated active runtime changes and RedPrism has unrelated plan changes; this library work does not
modify either tree. RedSalamander's General cards still measure text through `PrefsUi` and explicitly place
label/description/toggle or combo bounds. RedPrism's size card still has explicit title, row, note and action extents.
Those observations confirm the proposed pilots' need, but are source inspection rather than product qualification.

### 2.2 What the consumers actually need

Paths in this table are relative to the named repository under `Z:/src`.

| Consumer and inspected sources | Existing behavior | Opportunity and boundary |
| --- | --- | --- |
| RedSalamander: `Specs/UI/UI_PreferencesDialog.md`, `RedSalamander/Preferences.General.cpp` | Retained DxUi preference cards; explicit measurement and bounds; one scrolling page body and fixed OK/Cancel/Apply | First form pilot: a small General-page section. Preserve draft/Apply/Cancel, category-tree focus, live density and per-page scroll |
| RedSalamander: `Specs/UI/UI_FileOperationsPopup.md`, `RedSalamander/FolderWindow.FileOperations.Popup.cpp` | Retained task/conflict content, measured action wrapping, a hand-placed speed-limit form | Reuse intrinsic fields in the speed-limit prompt; evaluate variable-height task cards only when a large-task fixture demonstrates a benefit |
| RedSalamander: `FolderWindow.FileOperations.IssuesPane.cpp`, `FolderWindow.FileOperations.Popup.PathDetail.cpp` under `RedSalamander` | Issues already use DxUi Grid; same-window path inspection preserves exact paths and returns to a live row | Keep the issues table on Grid. Use path-detail behavior as an overlay lifecycle/focus test, including background non-activation |
| RedSalamander: `RedSalamander/Ui/AlertOverlayWindow.cpp` | A separate native alert window has its own lifetime, activation and focus restoration | Share logical session rules through an adapter; preserve its HWND and OS routing until a separate migration is justified |
| RedSalamander: `docs/Themes.md`, `RedSalamander/Preferences.Themes.cpp`, `RedConfigure/Themes`, `Common/ThemeExpression.h` | Theme v2 files, authored expressions, live preview/cancel and an advanced editing workbench already exist | Supply library role metadata to the existing editor. Do not add a second theme DSL, file watcher or persistence layer |
| RedSalamander: `Plugins/ViewerWeb/ViewerWeb.cpp`, `ViewerWebSecurity.h` in that directory | Markdown is already rendered by the WebView2 viewer with bundled markdown-it | A native help control serves small embedded descriptions. It does not replace the Markdown file viewer |
| RedXe: `Specs/Plugins/Plugins_AVControl.md`, `Plugins/AVControl/ProfileControls.cpp` | At most four AV profiles, a manually arranged draft editor, device selectors, saved levels and a six-step camera guide | Form composition and optional help rendering are useful. Four profiles do not justify virtualization; preserve drafts across device/view reconstruction |
| RedXe: `AVControlLayout.cpp`, `AVControlView.cpp` under `Plugins/AVControl`; `Specs/UI/UI_Dashboard.md` | Product-specific density tiers, 48 DIP live action targets, confirmed backend state; AV view sets reduced motion | Preserve those layout tiers and immediate acknowledgements. Keep AV/device logic, page gestures and scheduling in the application |
| RedPrism: `Specs/UI/UI_ApplicationChrome.md`, `RedPrism/ChromeHost.cpp`, `ChromeHostSize.cpp`, `ChromeHostAdjust.cpp` | Manual size/adjustment forms; an ordered card stack already remembers keyboard return targets | First embedded form pilot: one size card. Extract common overlay bookkeeping while preserving each card's keyboard policy and app-owned preview/commit/cancel |
| RedPrism: `Common/Document.h`, `DocumentHistory.cpp`, `JobActivity.h` under `Common` | History and job models exist; default history entry limit is 100 | A history/activity browser is a future candidate, not an existing UI requirement. Validate demand before adding a product panel |
| RedPrism: `RedPrism/ThemePalette.h`, `Specs/Core/Core_Settings.md` | Existing semantic palette adapter and live system/light/dark settings | Improve adapter clarity and role coverage. Keep canvas rendering, tool-specific controls and document settings outside DxUi |

The current RedPrism card policy is particularly important: notice, Command List, Rename and Picker take keyboard
focus on opening; size and adjustment cards do not. On close, a card restores focus only if it held the keyboard.
An unhandled Enter/Escape goes to the card holding focus, otherwise the top card. A universal modal focus trap would
change this behavior and is not the proposed default.

### 2.3 Priority and first adopters

| Priority | Improvement | First library proof | First consumer candidate |
| --- | --- | --- | --- |
| P1 | Intrinsic sizing and forms | Localized settings form with native and embedded hosts | RedSalamander General section; RedPrism size card |
| P1 | Overlay sessions and focus policy | Nested/modeless overlays, stale return target and teardown | RedPrism card bookkeeping; RedSalamander path-detail adapter |
| P2 | Variable-height list | Mixed log/card fixture with append, removal and height changes | RedSalamander large task-card scenario, subject to measured need |
| P2 | Theme role authoring | Metadata completeness and adapter parity | RedSalamander's existing theme workbench |
| P2 | Shared motion primitives | Retargeting and idle/hidden/reduced-motion tests | Existing DxUi transitions, then RedSalamander theme-cycle feedback |
| P3 | Native Markdown help | Localized, selectable help with links and long text | RedXe camera-guide experiment; other small help surfaces only if useful |

These priorities concern reuse and engineering risk, not a promise to redesign all three applications. Consumer
pilots are separate changes with their own product contracts, pinned dependencies and qualification.

## 3. Shared design rules

1. Keep manual `SetBounds` layout valid. New layout containers are opt-in islands inside an existing retained tree.
   No global auto-layout pass may silently move existing controls.
2. Keep one owner per control. Models and delegates are explicitly borrowed for a documented lifetime; detach
   callbacks before their owners die. Generation-aware handles replace long-lived raw return-focus pointers.
3. Measure text, parse documents, update realization and arrange controls during preparation or explicit model
   updates. Clean composition performs none of those operations and retains its allocation-free contract.
4. Embedded hosts borrow the device/context and use application scheduling. No new swap chain, HWND, worker,
   periodic timer or network/file service is introduced.
5. Separate presentation from data mutation. The application decides what validates, commits, cancels, opens a URI,
   follows the tail of a log or restores external focus.
6. Cache by actual dependencies: text/model revision, available width, font identity, DPI, density, flow direction
   and relevant theme roles. Color-only changes should not reparse text or recompute unrelated geometry.
7. Preserve pointer capture, IME, Unicode, clipping, accessibility, high contrast and reduced motion. Bounds are
   applied once, only when changed, before input/UIA geometry is republished; do not reflow from inside a callback.
8. Reuse existing DxUi measurement, scrolling, transition, UIA and host helpers before adding another utility.
   Establish finite budgets and observable overflow/failure behavior for each new retained structure.

The compatibility rules in the Consumption contract apply. Additive APIs that preserve old usage do not by
themselves increment `apiRevision`. Removing/renaming interfaces, changing existing semantics or requiring a new
call does. Rebuild every adopting consumer with matching headers, library, toolchain and CRT; C++ ownership never
crosses a plugin ABI. All three current revision-3 pins need an explicit revision-4 compatibility/adoption review.

## 4. Workstream A: intrinsic sizing and reusable form layout

### 4.1 Current support and problem

DxUi already has `Panel`, `StackPanel`, `ScrollPanel`, typography helpers and `ArrangeMeasuredActions`.
`StackPanel` needs supplied child extents; `Toolbar` is a plain panel. Before S1/S2 there was no common intrinsic
measurement contract or reusable label/editor/description/validation field layout, so consumers calculate the
same text heights, label widths, margins and footer positions in multiple places. This branch implements the
new opt-in primitives; its implementation and local evidence are recorded here, with broader acceptance retained in the qualification WIP.

Retain the existing measured-action helper for atomic action wrapping. Do not replace it with a layout system that
splits buttons, obscures commands or introduces an unreviewed density policy.

### 4.2 Accepted S0 public concepts

| Concept | Accepted responsibility |
| --- | --- |
| `LayoutConstraints` | Finite minimum/maximum DIP sizes, an explicit unbounded axis, and validation |
| `DesiredSize` | Measured size and optional baseline; independent of the control's current position |
| `MeasureContext` | Borrowed host typography/environment and preparation-scoped cache access |
| Intrinsic measurement hook | Optional default implementation for existing/custom controls; explicit extent fallback |
| `FlexPanel` | Row/column layout, gap, padding, min/max extent, grow and alignment; wrapping is explicit |
| `FormPanel` | Uniform field layout, shared label column, app-selected column count and independent actions |
| `FormField` | Owns label, editor, optional description and validation message; stable field identity |

Accepted usage:

```cpp
// Implemented API; check HRESULTs and nullable additions in production code. See docs/layout.md.
LayoutConstraints limits = LayoutConstraints::AtMost(bodyWidthDip, bodyHeightDip);
DesiredSize desired;
HRESULT result = MeasureControl(control, context, limits, desired);

auto* form = body->AddChild<FormPanel>();
form->SetColumns(1);                 // application decides the responsive tier
form->SetLabelPlacement(LabelPlacement::Above);
auto* width = form->AddField(L"canvas.width", localizedWidthLabel);
auto* editor = width->AddEditor<NumericStepper>();
width->SetDescription(localizedWidthHint);
width->SetValidation(ValidationState::Error, localizedError);
// Product code owns the value, unit conversion, validation and Save/Cancel.
```

S0 chose a non-pure `Control::MeasureOverride` with a checked `Control::Measure` boundary and `MeasureControl`
convenience call. Existing custom controls return `E_NOTIMPL` and keep explicit extents. Manual Panel/StackPanel/
Toolbar semantics remain intact. New containers are opt-in; additive API does not increment the existing revision
4 again, but C++ object-layout changes require every adopting consumer to rebuild with the same pin.

Reuse inventory: host DirectWrite formats and reading-direction resolution supply real shaping; existing lifetime
tokens, `Panel::TakeChild`, interaction retirement, text-service snapshots, immutable UIA publication and
`ArrangeMeasuredActions` retain their responsibilities. Numeric measurement caches are new container-owned state;
no parallel text renderer, focus manager, schema validator or action-wrapper is added. Native and embedded hosts
share one pending intrinsic traversal during preparation, with no composition work or new scheduling service.

Accepted limits are 1,024 logical Flex slots, 256 logical form fields and 65,536 UTF-16 shaping units per pass across
nested/cache-miss work. Optional maxima represent unbounded axes; invalid constraints and arithmetic fail before
geometry commits. Measured natural extents can exceed a constrained width for an indivisible glyph and report
overflow. Cache keys bind actual content/revision, constraints and host typography; color/hover do not dirty shape.
Custom callbacks revalidate owner, child, host and interaction identity before continuation. The retained baseline
is the original same-binary study at the exact base above, linked from [Measurements](../../../Measurements/README.md);
its drift is diagnostic and cannot be relabeled a policy qualification. Form-specific manual/intrinsic synthetic
cost and visual evidence are retained below; they do not establish paired-performance or product acceptance.

### 4.3 Measurement and arrangement behavior

- Measure leaf text using existing DirectWrite formats, actual shaping and wrapping, including trailing lines,
  surrogate pairs, combining sequences and RTL. No character-count width estimates.
- A minimum interactive extent is distinct from a preferred text extent. The application supplies product-specific
  minima, including RedXe's live 48 DIP actions. A constrained container reports overflow or scrolls; it does not
  silently shrink a critical target.
- Validate non-finite values, negative extents and contradictory constraints before mutating geometry. Unbounded
  axes are explicit rather than represented by arbitrary huge floats.
- Measure only invalidated dependencies. Arrangement writes changed bounds during preparation; repeated identical
  preparation does not invalidate capture, reshuffle focus or shape text again.
- Start with row/column flow and simple wrapping. Exclude arbitrary CSS selectors, percentages, absolute-position
  cascades and a general grid track-sizing engine. Add another primitive only for a demonstrated consumer need.
- Manual children keep explicit extents until their owner opts into measurement. `Panel`, `StackPanel` and
  `Toolbar` retain their present meaning and existing examples remain valid.

### 4.4 Form behavior

- Support labels above or beside editors, one or two application-selected field columns, shared label widths,
  descriptions and validation messages. Calculate label widths from the current visible field group.
- Keep editor ownership stable while changing columns or label placement. Do not reconstruct the tree on resize,
  update a draft by rebinding controls, or alter tab order according to incidental geometric sort.
- Associate the field label and description with the editor's UIA provider. Required/error state needs accurate
  semantics and a textual explanation; color alone is insufficient.
- Keep footer actions separate from the scrollable body. Reuse measured action wrapping; invalid forms remain
  inspectable and their error text can be copied/read.
- Expose validation presentation only. No schema language, implicit validation, auto-submit, dirty-state manager,
  save transaction or app settings dependency is included.
- When a field is removed or becomes unavailable, cancel its gesture/composition through existing host rules and
  ask the owner for a safe successor. Merely reflowing a field retains its focus, selection and draft.

### 4.5 Implementation and evidence

- [x] A1: inventory and reuse measurement helpers; settle the public hook, cache keys and failure semantics (S0).
- [x] A2: implement leaf measurement for the controls required by the pilots, then opt-in `FlexPanel`.
- [x] A3: implement form/field composition and label/description/validation UIA relationships.
- [x] A4: add localized form samples, gallery states and design guidance; publish visual changes.

A4's publication is complete: the control guide, layout usage, normative contracts, capability inventory, three
component guidelines/previews and published 33-control gallery are updated. All six harness images were reviewed
for clipping, overlap and missing content. A5/A6 consumer pilots remain in the qualification WIP.

## 5. Completed stages and sequencing decisions

| Stage | Implemented deliverable | Recorded local evidence |
| --- | --- | --- |
| S0 | Consumer recheck, public measurement/ownership/API decisions, reuse inventory, fixture and budget decisions | Historical source identities above; accepted domain/API decisions; original baseline retained without a policy-acceptance claim |
| S1 | Intrinsic measurement and opt-in row/column Flex layout | Leaf/cache/layout behavior in both hosts, manual-layout compatibility, finite-budget/failure tests and local native results |
| S2 | Stable form/field composition and label/description/validation UIA relationships | Localized synthetic forms, stable editors during reflow, published docs/gallery/design-system previews and local native results |

S1 depends on S0 and S2 on S1. The original wider exit evidence also called for paired non-regression, real IME,
platform qualification and separately owned native/embedded consumer pilots. Those uncompleted acceptance
obligations are carried by the qualification WIP, not marked passed in this Done record.

- [x] Choose measurement adapter versus a default non-pure control hook; define cache/invalidation ownership.
- [x] Settle form ownership, stable field IDs and UIA label/description relationships.
- [x] Classify accepted S0-S2 public changes under current compatibility rules; record explicit consumer migration needs.

Later list-revision/budget, overlay-focus/teardown, Markdown-parser and theme-metadata decisions are retained with
S3-S7. The original S8 closeout/handoff step is represented by this library record plus its separately owned
qualification plan; it does not close the deferred stages or product gates.

## 6. Recorded local validation and bounded diagnostics

The original [native/form/Grid packet](../../../Measurements/Layout/2026-10-10/README.md) retains the following
results at its recorded source/build identities. These are historical evidence, not a claim that the split itself
reran native suites or qualified subsequent integration fixes.

- [x] Run `validate.ps1` and `format.ps1 -Check` at the frozen native source; later docs/evidence require renewed tooling validation.
- [x] Run all nineteen noninteractive control/Embedded/foundation/host scopes in x64 Debug, Release and ASan Debug:
  57 passing suite/log pairs, zero skips and all 38 new tests completed in every profile. Their complex-UI receipts are unpaired.
- [x] Build ARM64 Debug, Release and ASan Debug.
- [x] Record WARP preparation/composition and synthetic graphics-generation replacement plus clean/dirty/hidden
  lifetime evidence. Physical hardware/device-removal acceptance remains separate.
- [x] Exercise 96/144/192 DPI, Light/Dark/RainbowLight/RainbowDark/high-contrast themes, standard/compact density,
  RTL/LTR and Unicode content where relevant.
- [x] Verify UIA navigation/events with the existing client helpers and provider disconnection on detach; synthetic
  assertions alone do not qualify real assistive-technology, IME or touch behavior.
- [x] Run Menu, NativeTextInput, MenuResources and MenuResourceScaling only through `test.ps1 -Interactive`, at a
  time the person at the desktop has agreed to. Debug, Release and ASan Debug each pass all four with zero skips
  and successful restoration. Earlier failed ASan confirmations/restoration and the unavailable-input-desktop check
  (error 5) remain separately retained.

That packet retains all local results,
initial Embedded and ASan Accessibility failures, isolated/repeated retests and two initial Debug lease timeouts.
The later Debug/Release focus runs pass; three earlier ASan confirmations time out before Start, with no child
executed. The third ASan attempt reports failed foreground/pointer restoration and the following native desktop
check reports Windows error 5. A fourth locally confirmed ASan run passes all four suites with zero skips and
successful restoration after desktop access is restored. The earlier failed attempts and refusal remain separate.
The eight-field Manual/Form diagnostic has matching pre-timing
pixels/geometry/metadata, zero unchanged/composition work, 7 KiB of extra warm retained plain C++ Form memory and
zero tracked plain-live bytes after each fixture's destruction. These are bounded diagnostics, with no speedup,
consumer, hardware or paired-policy acceptance claim.

The later [approved automatic-Start qualification packet](../../../Measurements/Performance/2026-10-10-profiling/validation/auto-start/README.md)
retains six builds and 69 successful x64 suite executions: 57 noninteractive and all four focus-taking suites in each
of Debug, Release and ASan Debug, with zero native skips and verified restoration. Its frozen native source is
`02961a88321c75ccd39090873f48977206d6dfdb`; subsequent documentation/evidence commits do not change those executable
bytes. Actual confirmations were clicked Start; private confirmation tests separately verify no-click countdown
expiry. This later evidence supersedes older open local-focus obligations for its exact source/artifact identity.
Original UIA extra-event and native-input setup cause questions remain in
[ProductionReview](../WIP/ProductionReview_2026-10-05.md); a passing fresh run is not a proved cause or fix attribution.

Local synthetic WARP preparation/composition, graphics-generation replacement and clean/dirty/hidden lifetime
checks do not qualify physical hardware/device removal. ARM64 cross-builds do not qualify native runtime. Synthetic
UIA/Unicode/reflow checks and native x64 focus suites do not replace real IME, assistive technology or touch checks.
Paired non-regression and trusted-policy acceptance remain open, including the later timing/FPS flags.

## 7. Implementation closeout

- [x] Inspect the three consumers and separate supported APIs from future proposals and product adoption.
- [x] Complete S0-S2 code and the recorded local functional/resource validation.
- [x] Update owning domain contracts, affected usage/hosting/performance documentation and truthful capabilities.
- [x] Add catalog/factory entries, populated gallery tiles, interaction tests, control usage and committed design-system
  guidelines/previews; publish the gallery through `gallery.ps1 -PublishDocs`.
- [x] Retain reviewed library-only receipts under Measurements; leave generated build products under `.build`.
- [x] Split the deferred S3-S7 work and all uncompleted S0-S2 acceptance into their WIP records.

This closeout does not close existing menu, Grid, Tree, slider, production-review, consumer or platform plans.

On 11 October the developer requested remote-main integration of this completed implementation, with S3-S7 and
unfinished qualification retained in WIP. The [integration packet](../../../Measurements/UiComposition/2026-10-11-integration/README.md)
retains final native source `bf11ba44fbd661fd5e1e6015f95d5719a4bfba83`: each of three x64 profiles passes 20 selected
form/quiet/embedded client cases and all four full foreground suites, with zero skips and verified restoration.
Both original and forced IME undo cases complete. The discarded-edit reset has a controlled pre-fix failure;
original composition, two-undo and late-result assertions remain. Embedded notification ownership/coalescing and
the form client's message-aware owner wait have reviewed real-client evidence. Actual compiler/SDK, dependency,
artifact, harness and environment inputs are compared across archival; `284a9ec6` and `061748aa` remain distinct
historical runs. The earlier budget repair assigned 60 minutes only to ARM64 ASan while preserving the stage caps
and existing checks. Exact-head/base/merge-tree PR CI requires its own reviewed results. The dated
[validation decision](../../Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage) retains unfinished
performance, native-platform, physical-input and consumer gates without recording a successful `ci-gate`.

The first PR #73 run at head `cdb3be2cf68d6444de9e66559db44fcfd75c0781` failed Linux tooling validation:
four evaluated-path fixture cases and one cross-process creation-timestamp assertion. The subsequent focused
fixture repair preserves the production policy/runner, rejection cases and bounded process assertions. Its
[portability packet](../../../Measurements/Tooling/2026-10-11-linux-portability/README.md) retains the failed run,
actual synthetic merge, reproducible Linux negatives and independently reviewed Windows/Linux positives.
That superseded run was cancelled; it supplies no native, consumer or paired acceptance. Fresh exact-head CI
remains required before the requested remote integration. The fixtures change no control rendering or gallery
content; the committed design-system previews and their harness provenance remain applicable to their own source.

The subsequent run at head `1cdbfde4c93bafa6a0c794fd80939387467fefa4` completes all x64 Full and synthetic-consumer
work, but each ARM64 Full step reaches its 25-minute cap before ordinary suite completion. The
[cold-build packet](../../../Measurements/CiBudget/2026-10-11-cold-build/README.md) retains those failures and the
successful independent consumers. ARM64 cold build/provenance now receives a finite 45-minute Full window;
every enclosing row fits its Full and consumer windows plus setup/artifact allowance. Native per-test watchdogs,
consumer limits and failure rules stay required. This repair changes no control rendering, gallery or design-system
content and requires fresh exact-source CI; the truncated run does not qualify ARM64 or paired performance.
