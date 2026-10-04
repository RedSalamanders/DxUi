# UI composition improvements for RedSalamander, RedXe and RedPrism

- **Status**: ACTIVE, proposed design and implementation plan, 4 October 2026. No feature in this plan is implemented
  or newly advertised by this documentation change.
- **Scope**: intrinsic sizing and forms; variable-height lists; overlay lifetime and focus; read-only Markdown help;
  reusable motion and theme authoring.
- **Library owner**: DxUi. Product behavior, data, persistence, scheduling and adoption remain in each consumer.
- **Request**: use GPUI Kit as a source of ideas, then design around the actual needs of the three Windows consumers.
  A full code editor is outside this work.
- **Closeout**: complete the library stages and their validation before moving this plan to Done. Separately deferred
  consumer or platform gates stay explicitly on HOLD in their owning repositories.

## 1. Purpose and authority

The largest immediate benefit is reducing repeated measurement, field placement and focus bookkeeping without
changing how an application saves settings, acknowledges an AV command or edits a document. General virtualization
and native help rendering extend what can be composed from retained controls. Motion and theme work should make
existing capabilities easier to reuse rather than introduce a second renderer, scheduler or theme language.

This document owns proposals, sequencing, decisions and acceptance evidence. It does not override current contracts.
Before implementing a stage, reconcile its accepted behavior in the owning domain below; keep enduring requirements
there and record the stage's result here. Public names and signatures below are design sketches, not supported APIs
or compilable usage examples.

| Area | Owning contracts to reconcile before implementation |
| --- | --- |
| Ownership, compatibility, consumer pins | [Architecture](../../Core/Core_Architecture.md), [Consumption](../../Build/Build_ToolchainAndConsumption.md) |
| Measurement, forms, list realization | [Controls and layout](../../UI/UI_ControlsAndLayout.md) |
| Overlay routing, selection, Unicode, IME, UIA | [Input and accessibility](../../UI/UI_InputAndAccessibility.md) |
| Typography, theme resolution, motion policy | [Theme and typography](../../UI/UI_ThemeAndTypography.md) |
| Preparation, composition, visibility, device generations | [Embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [Win32 hosting](../../Rendering/Rendering_Win32Host.md) |
| Budgets and paired evidence | [Performance and resources](../../Core/Core_PerformanceAndResources.md) |
| Suites, platform and native qualification | [Validation](../../Testing/Testing_Validation.md) |
| Catalog, usage, gallery and publication | [Documentation](../../Core/Core_Documentation.md), [Design system](../../UI/UI_DesignSystem.md) |

### 1.1 Borrowed ideas and their limits

| Reference | Useful idea | DxUi interpretation |
| --- | --- | --- |
| [GPUI Style](https://gpui-kit.com/docs/style/), [Form](https://gpui-kit.com/component/form/) | Intrinsic sizes, reusable fields, label placement, independent form columns and action layout | Small typed layout primitives and an opt-in form container; application owns breakpoints, drafts and validation |
| [VirtualList](https://gpui-kit.com/component/virtual-list/) | Visible-range realization with estimated item sizes | Vertical heterogeneous retained rows with stable identity, bounded recycling and measured variable heights |
| [Focus Trap](https://gpui-kit.com/component/focus-trap/) | A reusable scope for keyboard navigation | Overlay sessions with explicit focus policies, including preservation of external/canvas focus |
| [TextView](https://gpui-kit.com/component/text-view/) | Rich, selectable help content | A bounded, read-only native Markdown view; no browser or editor subsystem |
| [Editor](https://gpui-kit.com/component/editor/) | Shows the much larger scope of editing infrastructure | Explicitly excluded: code editing, syntax engines, completion, LSP, diagnostics and editor plugins |
| [Motion](https://gpui-kit.com/base/motion/) | Retargetable transitions and presence | Reusable value samplers driven by existing host clocks, with reduced-motion and hidden-state behavior |
| [Theme](https://gpui-kit.com/component/theme/) | Discoverable semantic roles and easier authoring | Typed role metadata, override validation and better adapter guidance; consumer theme formats remain consumer-owned |

Do not translate a CSS-style API wholesale, replace retained ownership with GPUI entities, or import GPUI's Rust
runtime. The useful behavior must work with DxUi's single library target, Win32 and embedded D3D11 hosts.

## 2. Consumer evidence and priorities

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
crosses a plugin ABI. RedSalamander's revision-2 adapter needs an explicit compatibility/adoption review.

## 4. Workstream A: intrinsic sizing and reusable form layout

### 4.1 Current support and problem

DxUi already has `Panel`, `StackPanel`, `ScrollPanel`, typography helpers and `ArrangeMeasuredActions`.
`StackPanel` needs supplied child extents; `Toolbar` is a plain panel. There is no common intrinsic measurement
contract or reusable label/editor/description/validation field layout. Consumers therefore calculate the same
text heights, label widths, margins and footer positions in multiple places.

Retain the existing measured-action helper for atomic action wrapping. Do not replace it with a layout system that
splits buttons, obscures commands or introduces an unreviewed density policy.

### 4.2 Proposed public concepts

| Concept | Proposed responsibility |
| --- | --- |
| `LayoutConstraints` | Finite minimum/maximum DIP sizes, an explicit unbounded axis, and validation |
| `DesiredSize` | Measured size and optional baseline; independent of the control's current position |
| `MeasureContext` | Borrowed host typography/environment and preparation-scoped cache access |
| Intrinsic measurement hook | Optional default implementation for existing/custom controls; explicit extent fallback |
| `FlexPanel` | Row/column layout, gap, padding, min/max extent, grow and alignment; wrapping is explicit |
| `FormPanel` | Uniform field layout, shared label column, app-selected column count and independent actions |
| `FormField` | Owns label, editor, optional description and validation message; stable field identity |

Design sketch:

```cpp
// Proposed concepts only; exact signatures are resolved in stage S0.
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

Prefer a measurement adapter/default hook that keeps existing custom control subclasses source-compatible.
Decide during S0 whether a virtual hook or a separate measurer is the smaller supported contract. Do not introduce
a pure virtual method that every consumer control must implement.

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

- [ ] A1: inventory and reuse measurement helpers; settle the public hook, cache keys and failure semantics.
- [ ] A2: implement leaf measurement for the controls required by the pilots, then opt-in `FlexPanel`.
- [ ] A3: implement form/field composition and label/description/validation UIA relationships.
- [ ] A4: add localized form samples, gallery states and design guidance; publish visual changes.
- [ ] A5: adopt one RedSalamander General section and one RedPrism size card in separate consumer changes.
- [ ] A6: consider the RedSalamander speed-limit prompt and RedXe profile editor after those pilots pass.

Acceptance: long labels and error messages at 96/144/192 DPI, narrow/wide widths, standard/compact density and both
flow directions remain readable; tab order and IME survive reflow; fixed actions remain available; unchanged forms
perform no new measurement. Compare old/manual and proposed layout on the same synthetic form and consumer scenario.
Record reduced duplicated measurement/bounds logic alongside latency, allocations and memory; a shorter API is not
evidence of a faster UI.

## 5. Workstream B: a general variable-height virtual list

### 5.1 Current support and intended addition

Grid and Tree already limit visible-row work. Their models and cell/node behavior are valuable and remain in use.
The missing primitive is a vertical list of arbitrary retained row compositions: a log entry, a task card,
a progress summary or a history entry with wrapped secondary text.

RedSalamander's task-card scenario is the first candidate, subject to a large-task workload proving that ordinary
retained children cause a meaningful cost. Its issues table remains Grid. RedPrism's history limit is bounded and
no history browser was found in the inspected chrome; a new browser requires an application decision. RedXe's four
profiles stay ordinary controls.

### 5.2 Proposed model and delegate contract

```cpp
// Proposed, UI-thread contracts; models/delegates are borrowed until DetachModel.
struct ListItemKey { uint64_t value; };
struct ListChange { uint64_t revision; /* inserted/removed/updated stable keys */ };

class IVirtualListModel
{
    // Count, index-to-key, key-to-index, item kind and model revision.
    // No control allocation and no storage of application service objects.
};
class IVirtualListDelegate
{
    // Create an owned row for a kind; Bind(key, bindingGeneration); Unbind.
    // Supply a cheap initial height estimate and update the row's presentation.
};

auto* list = parent->AddChild<VirtualList>();
list->AttachModel(model, delegate);
list->SetOverscanRows(2);
list->SetFollowTail(false);          // application enables and controls this policy
list->ApplyChanges(changes);         // prepared before new input/UIA geometry
list->EnsureVisible(itemKey);
```

The real API must define lifetime, failure, reentrancy and revision rules, not only a draw callback. Creation returns
one owned control tree; a bound row never shares ownership with another container. Data mutations are marshalled by
the application to the UI thread. Removed/reused keys cannot make an old callback address a new item.

### 5.3 Realization, size index and scroll anchoring

- Begin with vertical orientation. Horizontal lists and masonry are deferred because the current embedded input
  contract exposes a vertical wheel and the inspected candidates do not require them.
- Use estimated heights and a prefix-size index to locate the viewport, then measure realized rows at the actual
  width. Establish target complexity of O(log N + realized rows) for ordinary scrolling, not a promise that every
  model replacement/filtering operation is O(1).
- Metadata may scale with N; control trees and text layouts scale with the realized range. Publish both costs.
  Use bounded per-kind pools and release unused kinds instead of retaining every historical row.
- Keep large accumulated offsets in a representation that avoids float precision loss; use local DIP bounds for
  visible rows. Test long lists, very tall items and overflow without allocating hostile-size fixtures.
- Preserve a stable key plus offset as the scroll anchor when text wraps, items update or preceding rows disappear.
  Specify fallback to the next/previous live key when the anchor itself is removed.
- Follow-tail is optional. A user scroll away stops following; append does not drag the reader to the bottom.
  The list has no refresh timer; the model's actual changes request preparation.
- Reflow after DPI/width/font changes retains the anchor and remeasures only the necessary visible work before
  progressively correcting estimates. Do not pre-shape all offscreen content to achieve an exact scrollbar.

Initial budget proposal for S0, to validate against fixtures before making it normative: up to 100,000 indexed items,
4 MiB size/identity metadata, 256 realized row trees including overscan and focus/capture pins, and two overscan rows
per side. These are independent caps, not promises that every combination fits. Saturation returns a diagnostic
and keeps the last valid model/view; explicit consumer overrides need their own evidence. Existing surface/cache
budgets are unchanged.

### 5.4 Input, editing and accessibility

- Rebinding a row disconnects obsolete callbacks, tooltip/link targets and UIA providers before assigning a new
  key/generation. A late event on a detached provider reports unavailability, never another item's data.
- Pin a focused editor or captured row within the finite realization budget. If the item is removed, cancel its
  gesture and IME composition before reuse and restore focus to an owner-selected live target.
- Do not enumerate 100,000 realized UIA children. Provide accurate visible providers and a bounded path to
  `VirtualizedItem`/item lookup and `Realize` where supported; explicitly document any unsupported offscreen pattern.
  Verify navigation and events from an actual UIA client.
- Scrolled/clipped rows do not receive pointer hits outside the viewport. Nested child scrolling handles the wheel
  first, then the list when appropriate. Selection policy is explicit and does not override a child TextField.
- Detach/hide/device loss release obsolete row resources through the existing host lifetime rules. Reopening uses
  application model state rather than stale retained providers.

### 5.5 Implementation and evidence

- [ ] B1: stable-key/revision contract and synthetic variable-height size index, with insertion/removal tests.
- [ ] B2: row realization/recycling, anchoring, bounded diagnostics and focus/capture pinning.
- [ ] B3: scrolling, keyboard, UIA lookup/realization and editor cancellation coverage in both hosts.
- [ ] B4: catalog, docs, gallery and a synthetic log/card sample; publish measured scaling receipts.
- [ ] B5: qualify the RedSalamander task-card pilot, or defer adoption if its workload does not benefit.
- [ ] B6: evaluate a RedPrism history/activity view only in a separately accepted product plan.

Acceptance fixture sizes: 0, 1, 100, 1,000, 10,000 and 100,000 items; uniform and mixed estimated/measured heights;
append bursts; removals before/inside/after the viewport; filtering/reordering; long localized text; focus, capture
and IME during recycling; repeated hide/show. Holding viewport size constant must not make realized control/text
counts grow with total N. Report index cost, bind/measure counts, scroll p95, frame p95, allocations and memory.

## 6. Workstream C: overlay sessions, dismissal and focus restoration

### 6.1 Current support and missing shared contract

DxUi has control-specific overlay painting/hit testing, menus, ComboBox popups, smoke overlays and host focus routing.
Consumers still implement card/session ordering, dismissal and return-focus bookkeeping. Share that logical work
without replacing native menu sessions, IME candidate ownership or application-specific windows.

The manager is per `ControlHost`/embedded view. It does not create an HWND, invoke activation, or own product state.
Use the existing overlay traversal rather than a second render tree with duplicated control ownership.
The first adapter may register an already-owned descendant through a host-validated lifetime token. Registration
does not acquire a second owner or require moving every existing card. A manager-owned content path is separate;
both paths disconnect when the host detects removal. Do not retain an unchecked raw panel pointer.

### 6.2 Proposed session API and policies

```cpp
// Proposed concepts, not current API.
OverlayOptions options;
options.focusMode = OverlayFocusMode::Preserve;    // explicit choice for Prism Size/Adjust
options.tabPolicy = OverlayTabPolicy::Unchanged;
options.dismissOnOutsidePointer = false;
options.restoreOnlyWhenFocusOwned = true;

OverlayHandle handle = host.Overlays().Open(ownedContent, options, callbacks);
// Dismissal requests are delivered to product code; it decides cancel/commit/veto.
host.Overlays().Close(handle, OverlayCloseReason::Cancelled);
```

| Policy | Required choice |
| --- | --- |
| Initial keyboard focus | Preserve current focus, focus an explicit live child, or owner-managed |
| Tab containment | Trap within this scope, ordinary host navigation, or owner-managed |
| Background availability | Modal exclusion or modeless interaction |
| Outside pointer / Escape | Request dismissal, ignore, or delegate; no implicit destructive acceptance |
| Return target | Generation-aware retained target plus an optional external/canvas host callback |
| Dismissal callback | Reason and session generation; close once even during nested/reentrant requests |

The manager separates z-order from keyboard ownership. It must represent a visible modeless card that has never
taken keyboard focus, and route to a focused card before the top card when configured for RedPrism. Tab trapping is
opt-in for genuinely modal dialogs. A named shared policy may reduce repeated configuration, but cannot hide these
differences.

### 6.3 Lifetime and input routing

- Opening retains content once, captures a valid return token and publishes input/UIA geometry on preparation.
  Reopening/reordering a session preserves its original return intent; stale handles are rejected.
- Close removes input eligibility and UIA navigation immediately, cancels owned capture/composition through host
  rules and notifies once. Optional exit painting cannot leave dismissed content interactive.
- A close requested inside a control callback marks the session closed but defers tree destruction until the
  dispatch/preparation boundary is safe. No callback can delete the control whose method is still executing.
- Restore keyboard focus only when the session still owns it. If the user focused the canvas, another card,
  another HWND or another application, closing a card cannot steal it back.
- Validate the return control's generation, attachment, visibility and enabled state. On failure, use the owner-
  supplied successor, another eligible scope, or the application's canvas policy.
- Native menus, ComboBox popups and active text composition get the existing first refusal. One Escape cannot both
  cancel a dropdown/IME interaction and close its parent card. No embedded default/cancel button is introduced.
- Modal background controls leave pointer routing, keyboard navigation and UIA navigation consistently; modeless
  overlays preserve their configured background access.
- Host hide/detach/device reconstruction resolves sessions and disconnects providers without callbacks into dead
  product owners. Define explicit teardown notification separately from user cancellation.

RedSalamander's background path-detail popup must remain non-activating. Its native alert adapter continues to own
HWND activation/restoration, and the existing foreground rules remain authoritative. Do not infer a focus defect
merely because separate code currently performs similar bookkeeping.

### 6.4 Implementation and evidence

- [ ] C1: map existing overlay traversal, native popup priority and focus-token lifetime; settle policies.
- [ ] C2: implement session handles, logical ordering, dismissal reasons and transactional teardown.
- [ ] C3: implement opt-in containment and validated return tokens for retained and external targets.
- [ ] C4: qualify a RedPrism adapter against the current card contract before removing app bookkeeping.
- [ ] C5: qualify RedSalamander path-detail/background and alert adapter scenarios in its own harness.

Acceptance matrix includes nested modal scopes, multiple modeless cards, reopen/reorder, outside clicks, child popup
Escape, IME cancellation, removed return targets, focus moved to canvas/external HWND, inactive background popup,
close from a callback, hide, detach and device reconstruction. UIA client navigation/events must agree with actual
keyboard/pointer eligibility. Existing native Menu tests remain required when menu routing changes.

## 7. Workstream D: bounded, read-only Markdown help

### 7.1 Purpose and scope

Add a native `MarkdownView` for short help, installation steps, explanatory notes and readable errors inside a
retained surface. RedXe's existing six-step camera setup guide is a concrete small-content experiment, not evidence
that the current guide is Markdown. RedSalamander already has a Markdown file viewer; preserve it. No native
Markdown help implementation was found in RedPrism's inspected chrome.

Initial supported subset: headings, paragraphs, emphasis, strong text, inline code, plain fenced code blocks,
ordered/unordered lists, block quotes, links and thematic breaks. Specify behavior for unsupported constructs
and malformed input. Tables are a later extension only if an accepted help document needs them.

Excluded: editing, syntax highlighting, code completion, LSP, executable/raw HTML, scripts, browser integration,
remote images, automatic downloads, document plugins and arbitrary filesystem/network access.

### 7.2 Parsing and public behavior

```cpp
// Proposed read-only view.
auto* help = parent->AddChild<MarkdownView>();
HRESULT result = help->SetDocument(localizedMarkdown); // owns a bounded source snapshot
help->SetSelectable(true);
help->SetOnLinkRequested([owner](const HelpLink& link) {
    owner->HandleHelpLink(link);     // product validates and opens allowed commands/URIs
});
```

Use a declared [CommonMark](https://spec.commonmark.org/0.31.2/) subset and executable parser examples. Evaluate a
pinned parser such as [MD4C](https://github.com/mity/md4c) against dependency, license, malformed-input and resource
requirements before selection. Do not claim full CommonMark conformance for a partial renderer, or implement the
grammar using ad hoc regular expressions. This stage adds no dependency until that decision is recorded.

- Parse to an immutable source/block/run snapshot on document change, separate from width-dependent DirectWrite
  layout. Color changes repaint; width/font/DPI changes relayout; neither reparses unchanged content.
- Keep the existing document on invalid input, failed allocation or budget overflow and return a diagnostic.
  Malformed syntax with defined literal fallback is ordinary content, not a failure.
- Define a recoverable empty-document state, parser ownership and snapshot release explicitly. Dependency choice
  includes license/notice handling, pinned version, build triplets and malformed-input/resource-limit tests.
- Proposed initial limits: 65,536 UTF-16 code units, 4,096 blocks, 1,024 links and nesting depth 16. Validate these
  against actual localized help before promoting them to the owning contracts; do not silently truncate.
- Render selection/copy using Unicode text boundaries and bidi-aware hit testing. Preserve a mapping between
  source offsets and rendered plain text; explicitly distinguish copying rendered text from copying source.
- Support Ctrl+C/Ctrl+A, keyboard link navigation/activation, visible focus and appropriate read-only UIA text/
  hyperlink semantics. It is not an editable TextField and does not manufacture an IME editing session.
- Never execute a link from the control. Give the application the target and activation context; it owns allowed
  schemes, local help commands and any confirmation/persistence policy.
- Treat runtime filenames, error strings and paths as literal runs or correctly escaped content. An application
  error containing Markdown punctuation cannot become an unintended command link.
- Large preparation remains bounded and synchronous for this scope. If future product documents need background
  parsing, the application schedules immutable results; the embedded control still owns no worker.

### 7.3 Implementation and evidence

- [ ] D1: inventory real help text and localization, choose the declared subset and parser dependency strategy.
- [ ] D2: implement immutable document preparation, typography, wrapping and bounded failure behavior.
- [ ] D3: add selection, copy, keyboard links, UIA and nested scroll behavior.
- [ ] D4: add a synthetic help sample, usage guidance, gallery and design-system publication.
- [ ] D5: compare a RedXe camera-guide pilot with the current label presentation; retain the simpler form if rich
  text does not improve readability or navigation.

Acceptance: subset parser examples, malformed/unsupported constructs, embedded markup in filenames, CRLF/LF,
emoji/combining/RTL text, selection across blocks, exact plain-text copy, refused link activation, long words/code,
budget limits, scrolling, font/theme/DPI changes and hide/show. Measure parse, first layout and reflow separately;
the settled view produces no animation deadlines and no background requests.

## 8. Workstream E: reusable motion and easier theme authoring

### 8.1 Motion: build on existing animation infrastructure

DxUi already has `FrameClock`, `FrameBudget`, `MotionPolicy`, control `Tick` behavior, `PageHost` transitions and
connected animation, plus `EmbeddedHost::NeedsAnimation/AdvanceAnimation`. RedSalamander also has an application
animation dispatcher. Inventory and reuse these before extracting primitives; do not add another scheduler.

Proposed first primitives:

| Primitive | Responsibility | Initial use |
| --- | --- | --- |
| `ScalarTransition` | Finite-duration easing, current value, target, retargeting and settled state | Existing hover/focus/disclosure behavior, preserving its current timing |
| `PresenceState` | Enter/visible/exit paint lifecycle separate from input eligibility | Optional overlay feedback; logical close stays immediate |
| `SpringTransition` | Optional bounded value/velocity sampler with explicit settle limits | Deferred until a real interaction needs it and paired evidence justifies it |

Keep sampling allocation-free after initialization and use existing clock/hitch rules. Retarget from the current
sample without jumps. Reduced motion snaps to the final value and removes animation demand immediately, including
when the setting changes during a transition. Hidden/detached content has no animation wake-up; reappearance uses
an explicit finish/restart policy rather than accumulating hidden elapsed work.

Only changed values request preparation; settled transitions clear `NeedsAnimation`. Embedded composition merely
uses prepared pixels. No presence animation delays cancellation, focus restoration or provider disconnection.
Do not spring direct pointer manipulation or animate an unconfirmed backend value.

RedXe's AV view explicitly disables animation-only frames for live acknowledgements; retain that policy. A future
decorative guide transition cannot enable animations on its mute/volume/camera controls. RedSalamander's theme-cycle
overlay is a possible application pilot; preserve its existing timing and dismissal contract. RedPrism remains
driven by its application loop, without a new chrome timer or implicit new user setting.

- [ ] E1: inventory easing/transition implementations and extract the smallest reusable sampler.
- [ ] E2: migrate one existing control without a visual/timing change, then test retargeting and reduced motion.
- [ ] E3: add presence integration only after overlay teardown semantics pass.
- [ ] E4: evaluate a spring only with a demonstrated need; keep it deferred otherwise.

Acceptance: deterministic supplied clock, retarget/reverse, zero duration, hitch clamp, invalid inputs, settled
state, policy change mid-flight, hide/detach, reappearance and many repeated transitions. Record wake-up counts and
verify zero animation-only work when settled/hidden or reduced motion is active.

### 8.2 Theme authoring: discover roles and validate adapters

DxUi already accepts `ThemeColors` through `MakeThemePalette`, or direct overrides of a default `ThemePalette`.
The [current adapter guidance](../../DesignSystem/Theming.md) and
[design tokens](../../DesignSystem/tokens.json) explain those paths. Extend them rather than maintaining a parallel
theme schema. Fonts remain typography roles, not theme-file colors.

Proposed additions:

- Typed `ThemeRole` and `ThemeRoleInfo` metadata: stable semantic name, value type, default/derived status, affected
  control families and relevant contrast pair. Include all supported role overrides without claiming unused
  consumer-only diff colors affect library controls.
- A single maintained mapping between public palette fields, metadata and design-system tokens. If code generation
  is needed, use checked, deterministic PowerShell tooling; do not hand-maintain competing role registries.
- A transactional override builder/validator over an existing resolved palette. Reject unknown roles and non-finite
  values; describe missing related roles and alpha-aware contrast problems. Return diagnostics for authoring tools
  without silently rewriting the consumer's authored expressions.
- A resolved palette revision/hash for cache invalidation based on effective values. An identical application theme
  reload does not recreate brushes or invalidate text layout; changed color roles repaint only affected work.
- A synthetic role preview and documentation of the two adapter paths, including which defaults remain after direct
  overrides. Expose metadata in a form the existing RedSalamander workbench can consume.

Keep high-contrast system precedence, alpha-aware painted contrast, alert fallback semantics, derived accent
variants and stable Rainbow seeds. Different status meanings retain words/glyphs when their colors coincide.
Do not add library-owned theme files, directories, watchers, expressions, application presets, persistence or
random-per-frame hue generation.

RedSalamander resolves theme v2 expressions and owns temporary preview/Apply/Cancel and atomic export. RedXe passes
resolved appearance through its existing plugin boundary. RedPrism owns system/light/dark settings and canvas
colors. They can all use role metadata without changing those contracts.

- [ ] E5: audit palette fields, token documentation and consumer adapters; settle metadata ownership.
- [ ] E6: add typed descriptors and override diagnostics with default-palette parity tests.
- [ ] E7: add role previews and updated authoring/adapter documentation; republish the design system.
- [ ] E8: integrate metadata into RedSalamander's existing theme workbench in a separate product change.

Acceptance: metadata coverage of public roles, exact unchanged default palettes, partial override diagnostics,
invalid/unknown input, transparent alert pairs, accent refresh, stable hue seeds, live apply/cancel and system
high-contrast override. Measure palette/brush churn across repeated identical and changing themes.

## 9. Implementation stages and dependencies

Keep one WIP record and execute focused stages. Each code stage gets its own reviewed change and changelog fragment;
do not combine five new subsystems into one implementation PR.

| Stage | Deliverable | Depends on | Exit evidence |
| --- | --- | --- | --- |
| S0 | Recheck consumers; API/ownership decisions; reusable helper inventory; synthetic fixtures and retained baselines | This proposal | Accepted domain updates, exact fixture/source identities, compatibility and budget decisions |
| S1 | Intrinsic measurement and opt-in flow layout | S0 | Leaf/cache/layout tests, both hosts, manual-layout non-regression, paired resource evidence |
| S2 | Form composition and accessibility | S1 | Localized forms, stable editors/IME, gallery/docs; separate native and embedded consumer pilots |
| S3 | Overlay sessions and explicit focus policies | S0 | Lifecycle/input/UIA matrix; Prism policy parity and non-activating Salamander adapter gate |
| S4 | Vertical variable-height VirtualList | S1; S3 where rows launch overlays | Realization/anchoring/recycling/UIA tests, scaling budgets, conditional task-card pilot |
| S5 | Read-only MarkdownView | S1; S0 parser decision | Declared-subset, selection/link/UIA tests; parse/reflow evidence and bounded help pilot |
| S6 | Reusable motion and optional presence | S0; S3 for presence | Existing animation parity, deterministic clock, reduced-motion/idle/hidden evidence |
| S7 | Theme metadata and authoring diagnostics | S0 | Adapter/default parity, token completeness, gallery/publication, workbench handoff |
| S8 | Library closeout and consumer handoffs | Completed accepted library stages | Honest capabilities/docs, reviewed receipts, Done record; each deferred gate explicitly owned |

S3 and S7 can be designed independently of layout implementation. S6 starts by extracting existing behavior;
spring motion is not on the critical path. S4/S5 do not require general editor support or a CSS layout engine.

### 9.1 Decisions required during S0

- [ ] Choose measurement adapter versus a default non-pure control hook; define cache/invalidation ownership.
- [ ] Settle form ownership, stable field IDs and UIA label/description relationships.
- [ ] Settle list model revisions, batch mutation semantics, key reuse rules and concrete finite budgets.
- [ ] Define external focus return callbacks and teardown/dismissal reentrancy rules.
- [ ] Choose the Markdown parser and explicit subset after checking real localized documents and dependency policy.
- [ ] Choose one theme metadata source of truth and decide whether deterministic generation is necessary.
- [ ] Classify every public change under current compatibility rules; record explicit consumer migration needs.
- [ ] Establish consumer pilot value criteria and leave optional product additions deferred if no need is shown.

## 10. Validation, benchmarks and resource evidence

### 10.1 Library scenarios

Use library-owned synthetic models and content. No fixture requires consumer settings, devices, documents or
services. The proposed scenario names below are additions to design, not existing script switches.

| Proposed fixture | Baseline comparison | Principal evidence |
| --- | --- | --- |
| IntrinsicForms | Current manual layout vs new container on identical labels/editors/actions | Measure/arrange calls, cache hits, dirty p95, allocations, stable geometry/focus |
| VariableHeightList | Current retained ScrollPanel rows vs new list; increasing N at fixed viewport | Realized trees/layouts, index bytes, bind counts, scroll/append/reflow p95 |
| OverlayLifecycle | Current equivalent synthetic session behavior vs manager | Close count, invalid focus returns, stale UIA/callbacks, resources after cycles |
| MarkdownHelp | Current equivalent composed plain help; parser/layout stages reported separately | Parse/first-layout/reflow time, selection correctness, bounded retained source/layout bytes |
| MotionTheme | Current control transitions and palette adapters vs extracted primitives/metadata | Value/timing parity, wake-ups, brush churn, clean/hidden resources |

Capture current Default/MultilineGrid/MultilineGridDistinct benchmarks before implementation and run them on every
candidate as regression guards. For a new fixture, run the same fixture against both designs under the same harness;
do not invent a zero-cost baseline for a new feature with no equivalent. If only candidate feature costs can be
measured, report them as budgets, separately from regression evidence.

Follow [the paired procedure](../../../docs/performance.md): matched machine/configuration/DPI/renderer/fixture,
interleaved A/B/B/A passes, three repetitions by default (six runs per side), same-binary noise controls and reviewed
receipts. Current investigation bands are 5% for timing/FPS and 2% for process memory, with statistical and noise
qualification. Existing deterministic composition, surface, cache and hidden-work budgets remain hard limits.
Confirmed degradation requires measured optimization, scope reduction or explicit deferral advice; never silently
rebaseline. Run the hosted paired gate when a PR changes what its benchmark measures.

Report preparation, composition, GPU completion/presentation methodology, private bytes, working set, retained
resources and allocation peaks separately. WARP throughput with a blocking readback is not display cadence.
No source found in this research establishes an apples-to-apples GPUI Kit versus DxUi benchmark; published numbers
from different runtimes/hardware/scenes must not become a performance target or a superiority claim.

### 10.2 Coverage and platform gates

For each implemented stage:

- [ ] Run `validate.ps1` and `format.ps1 -Check`.
- [ ] Run the affected control and Embedded suites, then required x64 Debug, Release and ASan Debug coverage through
  `test.ps1`; include its complex-UI receipts and paired comparison evidence.
- [ ] Build ARM64 Debug, Release and ASan Debug. Native ARM64 execution is separately required for runtime claims.
- [ ] For rendering/resource changes, record WARP, device loss/recreation and clean/dirty/hidden lifetime evidence.
- [ ] Exercise 96/144/192 DPI, Light/Dark/RainbowLight/RainbowDark/high-contrast themes, standard/compact density,
  RTL/LTR and Unicode content where relevant.
- [ ] Verify UIA navigation/events with the existing client helpers and provider disconnection on detach; synthetic
  assertions alone do not qualify real assistive-technology, IME or touch behavior.
- [ ] Run Menu, NativeTextInput, MenuResources and MenuResourceScaling only through `test.ps1 -Interactive`, at a
  time the person at the desktop has agreed to. Without that agreement, omit them from `-Suites` and record the gate
  as outstanding. Other suites continue through the non-activating harness.

These are implementation gates. This planning-only change requires documentation/tooling validation and formatting;
it changes no runtime behavior and does not claim that native implementation tests or consumer pilots have run.

### 10.3 Consumer adoption gates

| Consumer | Separate acceptance before its pin is moved |
| --- | --- |
| RedSalamander | Explicit revision-2 migration review; Preferences draft/Apply/Cancel and page-focus/scroll parity; exact path inspection and background non-activation; conflict action wrapping; native alert lifetime; theme-expression/preview parity |
| RedXe | Profile draft/device revision preservation; at most four profiles; density tiers and 48 DIP targets; confirmed backend state; immediate live AV controls; camera guide commands/content; hidden views keep no services/timers/providers |
| RedPrism | Size/adjustment preview/commit/cancel; focus/card policy including canvas; child dropdown/IME priority; capture and document teardown; live theme settings; app-owned canvas and history/job limits |

Generate before/after application captures only through each consumer's deterministic test harness, retaining
scenario, source, build, DPI and theme provenance. Do not take desktop/manual screenshots. Any proposal mockup is
labelled separately from an actual application capture. No captures were generated by this planning change.

Preserve existing RedSalamander ARM64/ASan handoff and RedXe hardware/real-IME/assistive-technology HOLD gates.
Library validation cannot complete those product gates. Keep adoption measurements in the application repository.

## 11. Documentation and completion checklist

- [x] Inspect all three consumers and distinguish current support from candidate additions.
- [x] Record the five workstreams, proposed APIs, sequencing, ownership and measurable acceptance.
- [ ] Accept S0 decisions and update the owning domain contracts before changing behavior.
- [ ] Complete accepted library stages and required platform, interaction and paired performance evidence.
- [ ] For every new concrete control, add catalog/factory support, a populated gallery tile, interaction tests,
  `docs/controls.md` usage and a design-system guideline/preview; republish with `gallery.ps1 -PublishDocs`.
- [ ] Update affected usage/hosting/performance documentation and link reviewed DxUi-only receipts under
  `Measurements`. Generated temporary reports stay under tooling-owned `.build`.
- [ ] Update `capabilities.json` only for actually implemented/qualified support, with pending limits explicit.
- [ ] Record explicit consumer pins/adapters and independent adoption outcomes; retain deferred product gates.
- [ ] Reconcile final normative contracts, move this completed library plan to Done and update the index.

Documentation/gallery review for this proposal: current published controls and images remain accurate because no
implementation or rendered appearance changes here. Gallery regeneration and design-system publication become
required in the visual implementation stages above. This plan does not close existing menu, Grid, Tree, slider,
consumer or platform plans.
