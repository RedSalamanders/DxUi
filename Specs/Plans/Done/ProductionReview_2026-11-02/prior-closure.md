# Closure of earlier review findings: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: whether the fixes claimed for the earlier
reviews (`Specs/Plans/Done/CodeReview_2026-10-31.md`, `DeliveryReview_2026-10-31.md`, `CodeReview_2026-10-28.md`,
`RichTextAudit_2026-10-21.md`, `CodeReview_2026-10-10.md`, `ProductionReview_2026-10-05.md`) hold in code and tests
after PR #159 (`93c77750..6df17007`).
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity. `origin/main` (`5fc4d4c3`)
differs only in documentation; `src/` and `include/` are identical. 20 findings survive (2 P1, 7 P2, 11 P3), 7 are
duplicates carried by other area files, 1 was refuted.

## Verdict table of earlier findings

Sources: 10-31 = `Specs/Plans/Done/CodeReview_2026-10-31.md`, DR = `Specs/Plans/Done/DeliveryReview_2026-10-31.md`,
10-28 = `Specs/Plans/Done/CodeReview_2026-10-28.md`, 10-10 = `Specs/Plans/Done/CodeReview_2026-10-10.md`,
PR = `Specs/Plans/Done/ProductionReview_2026-10-05.md`, RTA = `Specs/Plans/Done/RichTextAudit_2026-10-21.md`.
Test strength: STRONG (fails if the fix is reverted), WEAK (exists but cannot fail on the defect), NONE, (I) = runs only
in `test.ps1 -Interactive`. Paths are under `src/Controls/` unless they say otherwise.

| ID | Source | Claim | Verdict | Evidence at `6df17007` | Test | Note |
| --- | --- | --- | --- | --- | --- | --- |
| DX-3 | 10-31, DR | Quoted CF_HDROP fallback injects live Markdown | PARTIAL | `DxUi.RichTextEditor.cpp:1048` rejects only quote, CR, LF; raw path in opaque contexts `:1139` | STRONG (reported vector) | Residual is CMD-08 (PRIOR-05 merged) |
| R28-44 | 10-31, 10-28 | `Grid::SetModel` returns before `_model = model` on `bad_alloc` | FIXED | `DxUi.Grid.cpp:1208` assigns first | WEAK | Throwing getter, no allocation fault; PRIOR-08, PRIOR-16 |
| R28-41 | 10-31, 10-28 | Device loss in ResizeBuffers or creation never resets the device | FIXED | `DxUi.WindowHost.cpp:4469-4478` | WEAK | Test renders the recovery frame itself; PRIOR-06 |
| DX-4 / R28-05 | 10-31, DR, 10-28 | Native TSF has no capacity ceiling | FIXED | `DxUi.TextStoreACP.cpp:204-207`, checks 1386, 1730, 1813 | STRONG | Composition path not exercised |
| DX-2 | 10-31, DR | UIA focus does not cancel hover timers | FIXED on `main` | `DxUi.Menu.cpp:3525-3526`, revision refusal 4473-4477 | STRONG (I) | Not on the wall-compat remotes (PRIOR-02); PRIOR-20 |
| Wave 1 port | 10-31, DR | Port both menu cancels onto `origin/ej/wall-compat-*` | PARTIAL | Only local `a5b3ac81`, `a8cb8e1d`; remotes still `a059e07c`, `616104f3` | n/a | PRIOR-02 |
| R28-45 | 10-31, 10-28 | Every typed or unmatched `WM_CHAR` cancels pointer timers | FIXED | `DxUi.Menu.cpp:5514-5735` | STRONG (I) | |
| R28-01 | 10-31, 10-28 | Grid and Tree do not re-clamp scroll when they grow | FIXED | `DxUi.Grid.cpp:6490`, `DxUi.Tree.cpp:2961` `OnBoundsChanged` | NONE | PRIOR-08 |
| R28-02 | 10-31, 10-28 | Any pending `PrepareContent` cancels an unrelated capture | FIXED | `DxUi.Controls.cpp:2530-2560` gesture-owner path | NONE | Only the self-capture cancel is tested; PRIOR-08 |
| R28-03 | 10-31, 10-28 | `lineHeightMultiplier` skipped after the first chunk | FIXED (correctness) | `DxUi.RichDocumentLayout.cpp:307-313` forces whole-document reference layout | WEAK (pins mechanism) | Unmeasured cost regression: RENDER-02 (PRIOR-04 merged) |
| R28-04 | 10-31, 10-28 | RTL Subtle table width uses `second - first` | FIXED | `DxUi.RichSemanticGeometry.cpp:1404` | NONE | RTL case exists but cannot fail; PRIOR-08 |
| Enter EOL | 10-31, DR | Source, Shift+Enter and refused paragraph insert CRLF | FIXED | `DxUi.RichTextEditor.cpp:5349-5353` | STRONG | Helper duplicated at 1062, 5351, 5448 |
| R28-06 | 10-31, 10-28 | `GetStatus` never reports read-only or multiline | FIXED | `DxUi.TextStoreACP.cpp:1355-1362` | STRONG | New failure return: CTRL-03 (PRIOR-15 merged) |
| R28-40 | 10-31, 10-28 | Read-only editor swallows printable key-downs | FIXED | `DxUi.WindowHost.cpp:3890` `AcceptsTypedText()` | NONE | PRIOR-08 |
| R28-07 / R28-08 | 10-31, 10-28 | Double backslash and escaped bar in math | FIXED | `DxUi.NativeMath.cpp:1245-1252`, 636-643 | STRONG | Disposition documents stale (PRIOR-14) |
| R28-09 | 10-31, 10-28 | `:::`, `accTitle`, `accDescr` become diagram content | FIXED | `DxUi.NativeFlowchart.cpp:784-788`, 1010-1038 | STRONG | Implemented as metadata, beyond the Q9 "reject" decision |
| R28-12 | 10-31, 10-28 | Tree selection and Grid busy/sort setters skip invalidation | FIXED | `DxUi.Grid.cpp:1496-1540`, `DxUi.Tree.cpp:836-988` | NONE | PRIOR-08 |
| Q1 (R28-14) | 10-31 | `requestPreparation` re-entry vs members read after a wake | PARTIAL; residual use-after-free confirmed | `include/DxUi/Embedded.h:29`; `DxUi.TextInput.cpp:3105`, 3179, 3662 | NONE (residual) | PRIOR-03 (P1) |
| Q2 / DX-10 | 10-31, DR | Allocation-failure policy split | FIXED | `DxUi.RichTextEditor.cpp:5282-5288` `InputFailureHandled` | none located | Differs from DX-10's wording, meets its intent |
| DX-11 | 10-31, DR | `Separator` inserted mid-enum at apiRevision 4 | NOT FIXED (optional) | `include/DxUi/ControlCatalog.h` unchanged in PR #159 | n/a | HOST-04 (PRIOR-18 merged) |
| Qualification HOLDs | 10-31 | Paired performance, A/A, ARM64, physical and consumer gates | NOT FIXED (deferred) | `Measurements/WipDelivery_2026-11-02/README.md:29` | n/a | Tracked by DOCS-05 |
| math-parser-12 | 10-31 | Double backslash outside rows | FIXED (code) | as R28-07 | STRONG | Disposition says "refuted" (PRIOR-14) |
| MM-33 / MM-34 | 10-31 | State/class `:::` and acc metadata | FIXED | as R28-09 | STRONG | Revalidation document says "Remains" (PRIOR-14) |
| CR-16 / R28-17 | 10-31, 10-28 | `E_ABORT` never retries | FIXED (by contract) | `src/Rendering/Embedded.cpp:564-566` | n/a | Native counterpart still abandons frames: PRIOR-07 |
| CR-25 | 10-31, 10-10 | Paint-time layout and per-move tree walks | PARTIAL | `DxUi.WindowHost.cpp:5722` uncached prune per move | NONE | PRIOR-19 |
| DX-1 | DR | Tree release invoke skipped after collapse refresh | FIXED | `DxUi.Tree.cpp` `OnMouseUp`, `50062ee5` | STRONG | |
| DX-5 | DR | `Separator()` `noexcept` terminates on `bad_alloc` | FIXED | `Separator::Separator()` not `noexcept`, `dfedb096` | present, not re-read | |
| DX-6 | DR | `TagPicker::ClearChildren` leaves null slots | FIXED | `erase_if` in `TagPicker::ClearChildren` | STRONG | |
| DX-7 | DR | Receipt `DispatchCharacter` returns false after consuming | NOT FIXED (unscheduled) | `src/Rendering/Embedded.cpp:1154`, 1163-1168 | NONE | Narrowed: PRIOR-17 |
| DX-8 | DR | Unfocused Tree selection text | FIXED | `DxUi.Tree.cpp:83` | STRONG | |
| DX-9 | DR | Wrapped Label with typography override | FIXED | `DxUi.Layout.cpp` | STRONG | Unwrapped multiline label drops trailing spaces (nit) |
| R28-10 | 10-28 | Source ink for bare autolinks and `\(` | FIXED | `Tests/Controls/Controls.Tests.RichTextLayout.cpp:1797-1813` | STRONG | `https://` case untested |
| R28-11 | 10-28 | UIA `ScrollIntoView` leaves the snapshot stale | FIXED | `DxUi.Accessibility.cpp:7809-7810` | WEAK | Test republishes after every scroll; PRIOR-08 |
| R28-13 | 10-28 | Tree refresh and typeahead fight the user | FIXED | `TestTreeRefreshTypeaheadAndWheelBoundaries` | STRONG | |
| R28-16 | 10-28 | Large range geometry terminates | FIXED (no terminate) | `GetRangeScreenRect` maps `bad_alloc`, `f7ed0578` | present | Work still proportional to range length |
| R28-24 | 10-28 | Read-only Source mirror relays out and scrolls to top | FIXED (receipt) | `SynchronizeReadOnlySourceSnapshot` | present | Not independently re-traced |
| R28-46 | 10-28 | Menu tests do not assert pending-open before the timer | FIXED | `openPendingBeforeKeyboard` asserts | STRONG (I) | |
| R28-51 | 10-28 | Zero-warning gate passes an empty log | FIXED | `Build/BuildConsole.psm1:19-52`, `build.ps1:156-179` | n/a | |
| R28-37 / R28-38 | 10-28 | CODEOWNERS covers CI scripts | FIXED | `.github/CODEOWNERS` | n/a | R28-52/53 not checked |
| Native frame abandon | 10-28 known item | One failing `PrepareContent` blanks the native window | NOT FIXED (scope) | `DxUi.Controls.cpp:2433-2447`, `DxUi.WindowHost.cpp:4942-4949` | WEAK | No retry is the contract; window-wide abort is the defect: PRIOR-07 |
| CR-24 | 10-10, 10-28 | 13 Tree delegate calls unwrapped leave capture stuck | CONTAINED at the dispatch boundary | `DxUi.Tree.cpp:2010`, 2066-2070 release capture first | n/a | Stuck-capture claim refuted (PRIOR-09) |
| Wave 6 tests | 10-31 | Seven directed regression tests | PARTIAL | 3 of 7 present (UIA then timer, unmatched char, HDROP) | n/a | PRIOR-08 |
| Done status | all | Plans closed when waves pass | NOT FIXED | 311 open boxes in six Done plans | n/a | DOCS-05 (PRIOR-01 merged) |
| Consumer pins (Wave 4) | 10-31 | RedTilde and RedSalamander pin Wave 1+2 `main` | CANNOT VERIFY | Lock files live in consumer repositories | n/a | Question 4 |
| RTA set (24 items) | RTA | commands-02..07, blocks-01..03, inlines-02..04, editor-01..03, semantic-02, geometry-01, math-parser-01/03, math-layout-01, accessibility-01, status-truth-06, critic-01 | FIXED | `f5a2cea9`, `4c46e96b`, `b5869cbc`, `c0a4f583` and others | mixed | Weak oracles: PRIOR-13 |
| commands-01 | RTA | Out-of-bounds read deleting a table column | FIXED for the read; new defect | `DxUi.RichTextCommands.cpp:3939-3940` writes a blank row | none | PRIOR-10 |
| commands-09 | RTA | Enter in a quoted non-leaf block drops the prefix | PARTIAL | `DxUi.RichTextCommands.cpp:1436-1458` | WEAK (pins residual) | PRIOR-11 |
| inlines-01 | RTA | Quadratic autolink scanning | PARTIAL | `DxUi.MarkdownInlines.h:1260-1266` | WEAK | PRIOR-12 |
| status-truth-09/12 | RTA | Gallery tile claim; math boxes open under Done | NOT FIXED | `docs/rich-text.md:386-388` | n/a | PRIOR-14 |
| tests-engine-02, tests-host-02 | RTA | `DxUiMath1-target.json` never loaded; duplicated host tests | NOT FIXED | not re-traced | n/a | Not covered |
| P0 set | 10-10, PR | CR-1..CR-4, host-core-1 and the other P0s | FIXED | delegated audit | mixed | |
| CR-21 | 10-10 | Opening key's `WM_CHAR` taken as a mnemonic | NOT FIXED (deferred) | `DxUi.Menu.cpp:6283-6299` | NONE | PRIOR-24 |
| CR-34 | 10-10 | Evidence bloat and personal data in the public tree | NOT FIXED (owner decisions pending) | `Measurements/` | n/a | DOCS-12, DOCS-14 (PRIOR-22 merged) |
| CR-37 | 10-10 | Test hooks ship; `#else` arms never compile | NOT FIXED (deferred to next revision) | `include/DxUi/Configuration.h:4` | n/a | ARCH-04, HOST-14, ARCH-07 (PRIOR-23 merged) |
| text-input-9 | PR | Masked field keeps the IME active | PARTIAL | `DxUi.NativeTextInput.cpp:1713-1727` | NONE | PRIOR-21 |
| grid-selection-lifetime-3 | PR | All-collapsed grouped Grid ignores every key | NOT FIXED | `DxUi.Grid.cpp:5887-5890` | NONE | PRIOR-25 |
| host-core-6 | PR | Disabled control inside a ScrollPanel keeps dragging | NOT FIXED | `DxUi.WindowHost.cpp:5448-5470`, `DxUi.Controls.cpp:10903-10910` | NONE | PRIOR-26 |
| controls-editor-theme-4 | PR | `NumericStepper::SetValue` resets `_editing` | NOT FIXED | `DxUi.EditorControls.cpp:973-980` | NONE | PRIOR-27 |
| About 20 more | PR, 10-10 | P1/P2 sub-items still open | NOT FIXED (spot-checked) | see PRIOR-28 | NONE | PRIOR-28 |
| N-16..N-21 | 10-10 | Partial or deferred items | PARTIAL | not re-traced | n/a | Not covered |

## Findings

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| PRIOR-02 | P1 | Confirmed | The menu stop-ship port for RedPrism and RedXe exists only on unpushed local branches | `Measurements/WipDelivery_2026-11-02/README.md:9` | S |
| PRIOR-03 | P0 | Confirmed | Use-after-free: TextField touches `this` after a synchronous `requestPreparation` that may retire it | `src/Controls/DxUi.TextInput.cpp:3105` | S (guards), M (deferral) |
| PRIOR-07 | P2 | Confirmed | One failing `PrepareContent` aborts every native frame of the whole window, silently | `src/Controls/DxUi.WindowHost.cpp:4942` | M |
| PRIOR-08 | P2 | Confirmed | Seven P1 fixes have no test that fails when the fix is reverted | `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:524` | M |
| PRIOR-21 | P2 | Confirmed | A masked TextField leaves the IME active and swallows its commits | `src/Controls/DxUi.NativeTextInput.cpp:1713` | M |
| PRIOR-25 | P2 | Confirmed | Keyboard trap: an all-collapsed grouped Grid ignores every key | `src/Controls/DxUi.Grid.cpp:5887` | S |
| PRIOR-26 | P2 | Confirmed | A control disabled or hidden mid-drag inside a ScrollPanel keeps dragging | `src/Controls/DxUi.WindowHost.cpp:5448` | S |
| PRIOR-27 | P2 | Confirmed | `NumericStepper::SetValue` during a typed edit loses the terminal Commit or Cancel | `src/Controls/DxUi.EditorControls.cpp:976` | S |
| PRIOR-28 | P2 | Plausible | About 20 further ProductionReview and CodeReview_2026-10-10 items are open in code with no tracking | `src/Controls/DxUi.Accessibility.cpp:8986` | L |
| PRIOR-06 | P3 | Confirmed | The R28-41 test renders the recovery frame itself; non-device-loss resize failure schedules no repaint | `Tests/Controls/Controls.Tests.NativeDevicePolicy.h:38` | S |
| PRIOR-10 | P3 | Confirmed | DeleteTableColumn turns a pipe-less table row into a blank line | `src/Controls/DxUi.RichTextCommands.cpp:3940` | S |
| PRIOR-11 | P3 | Plausible | Enter after a quoted thematic break drops the quote prefix | `src/Controls/DxUi.RichTextCommands.cpp:1436` | S |
| PRIOR-12 | P3 | Plausible | Email autolink candidates revalidate the domain outside the work counter | `src/Controls/DxUi.MarkdownInlines.h:1265` | S |
| PRIOR-13 | P3 | Plausible | RichText tests use self-maintained or mirrored oracles | `Tests/Support/Support.Tests.SemanticSourceBlock.h:300` | S each |
| PRIOR-14 | P3 | Confirmed | Disposition and status documents contradict the code | `Specs/Plans/Done/RichTextAudit_2026-10-21/MathFindingsDisposition_2026-10-24.md:24` | S |
| PRIOR-16 | P3 | Confirmed | `Grid::SetModel` failure clears the selection without notifying | `src/Controls/DxUi.Grid.cpp:1314` | S |
| PRIOR-17 | P3 | Plausible | Receipt character dispatch reports a consumed, possibly committed character as unhandled on `std::exception` | `src/Rendering/Embedded.cpp:1163` | S |
| PRIOR-19 | P3 | Confirmed | `UpdateHover` runs an uncached whole-tree prune on every mouse move | `src/Controls/DxUi.WindowHost.cpp:5722` | S |
| PRIOR-20 | P3 | Confirmed | The menu stop-ship regressions run only in the interactive suite | `Tests/test-scopes.json:570` | M |
| PRIOR-24 | P3 | Confirmed | The opening key's queued `WM_CHAR` is taken as a menu mnemonic | `src/Controls/DxUi.Menu.cpp:6283` | S |

## P0 findings

### PRIOR-03 — Use-after-free: TextField touches `this` after a synchronous `requestPreparation` that may retire it

- **Severity** P0 (reviewer P2, verifier P1, lead P0) · **Category** bug (memory safety) · **Verdict** Confirmed ·
  **Confidence** high (static; chain proven by the project's own retiring fixture) · **Effort** S (guards), M (deferral)
  · **Introduced** `d04ab124` (contract widened; TextField sites not guarded)
- **Location**: `src/Controls/DxUi.TextInput.cpp:3105-3106`, `:3127`, `:3162-3165` (`ImportTextInputState`),
  `:3179-3200` (`RestoreEditHistoryState`), `:3661-3667` (`NotifyChanged`), `:2310-2316` and the redo twin (Ctrl+Z,
  Ctrl+Y callers), `:1902-1903` (clear button); `include/DxUi/Embedded.h:29`; `src/Rendering/Embedded.cpp:263-281`
- **What**: the wake is synchronous end to end:
  - `Control::InvalidateMeasure` (`src/Controls/DxUi.cpp:1116-1137`) calls `_host->Invalidate()` when an ancestor has
    intrinsic layout (FlexPanel, FormPanel);
  - `ControlHost::Invalidate` (`src/Controls/DxUi.WindowHost.cpp:1814-1820`) calls `_embeddedInvalidate` for an
    embedded host;
  - `InvalidateThunk` → `MarkDirty` → `RequestPreparation` (`src/Rendering/Embedded.cpp:263-281`) calls the
    application's `requestPreparation` at 280 unless a request is already outstanding; `Prepare` clears that flag
    (`:442`), so the first invalidation after a Prepare always calls out;
  - `RequestAnimation` (`DxUi.WindowHost.cpp:1890-1901`) and `Control::Invalidate(host)` reach the same callback.

  The header now says the callback "may retire or replace retained controls" (`Embedded.h:29`, also
  `Specs/Rendering/Rendering_EmbeddedD3D11.md:129-134`, `docs/hosting.md:39`), and
  `Tests/Controls/Controls.Tests.IntrinsicLayout.cpp:1231-1355` retires the root from the callback and expects setters
  to return safely. `d04ab124` guarded about 20 setters (and TextField's own setters at 962-1440 are guarded), but these
  paths still write or read members after a wake: `ImportTextInputState` writes `_caretIndex` and the rest at 3106-3161
  after `InvalidateMeasure` (3105), reads `_multiline` after `ResetCaretBlink` (3127), reads `_text` at 3163 after
  `Invalidate(host)` (3162) and then calls `NotifyChanged`; `RestoreEditHistoryState` writes `_text` and the rest at
  3180-3200 after `InvalidateMeasure` (3179), and its Ctrl+Z caller then calls `NotifyChanged()` (2312);
  `NotifyChanged` writes caches at 3663-3665 after `InvalidateMeasure` (3662). Plausible siblings: 1902-1903
  (`SetText({})` then `NotifyChanged()` with no lifetime check) and `ComboBox::ImportTextInputState`
  (`src/Controls/DxUi.ComboBox.cpp:2073-2074`). The native host is unaffected (`Invalidate` is `InvalidateRect`).
- **Failure scenario**: an embedded view with a FlexPanel root holding a multiline TextField. The application's
  `requestPreparation` replaces the page (`SetRoot`) as the header allows. After a Prepare the user presses Ctrl+Z:
  `DispatchKeyImpl` (`src/Rendering/Embedded.cpp:948-1061`) reaches `TextField::OnKeyDown` with no earlier wake,
  `RestoreEditHistoryState` calls `InvalidateMeasure`, the callback destroys the field, and 3180 writes freed memory
  (ASan: heap-use-after-free). The same happens through `EmbeddedHost::ApplyTextInput` Commit
  (`Embedded.cpp:1441`), which writes at 3106.
- **Fix**: keep the owner's contract (HOST-10 records it as an owner decision) and make it safe:
  1. Guards (S). In `ImportTextInputState` compute `const bool changed = previousText != state.text;` before any wake,
     take `const auto lifetime = GetLifetimeToken();` at entry, and return `false` when `lifetime.expired()` after
     `InvalidateMeasure`, after `ResetCaretBlink` and after `Invalidate(host)` (use `changed`, never `_text`, after the
     last wake). The embedded caller already maps `false` plus a dead control to `S_OK` (`Embedded.cpp:1426-1442`).
     In `RestoreEditHistoryState` compute `remeasure = _multiline && _text != state.text`, do every member write first,
     and call `InvalidateMeasure()` last; return a `bool` alive flag and have `TryUndoDirectEdit`, `TryRedoDirectEdit`
     and `OnKeyDown` (2310-2330) check a lifetime token before `NotifyChanged()`. In `NotifyChanged` invalidate the
     caches, finish the metric and copy `_onTextChanged`, `_text` and `_masked` before `InvalidateMeasure()`, then
     return `false` if expired. At 1902 check a lifetime token between `SetText({})` and `NotifyChanged()`. Apply the
     same audit to `ComboBox::ImportTextInputState`.
  2. Deferral (M, removes the class of bug during input). Add a dispatch depth to `EmbeddedHost::State`; every input
     entry point (`DispatchPointer`, `DispatchKeyImpl`, both `DispatchCharacter` overloads, `ApplyTextInput`, the
     clipboard entry points, animation advance) increments it with a `wil::scope_exit` that decrements and, at depth
     zero, calls `RequestPreparation()` if a wake was deferred. `RequestPreparation` records the wake instead of
     calling out while the depth is non-zero. The callback is still delivered synchronously before the entry point
     returns, and those entry points already revalidate their target after control callbacks. Setters called by the
     application outside a dispatch keep the step-1 guards.
  Pitfall: do not restore "must not re-enter" in the header without also removing the retiring fixtures; the spec and
  tests now depend on the wider contract.
- **Regression test**: `Tests/Controls/Controls.Tests.IntrinsicLayout.cpp`, next to `runRetiringSetter` (same WARP
  fixture and arming callback that calls `SetRoot(nullptr)`):
  (a) multiline TextField "a" in a FlexPanel, focused; `Prepare`; `ReadTextInput`; arm; `ApplyTextInput(revision,
  text "ab", Commit)` returns `S_OK` and the root is null. (b) same field, `DispatchCharacter(L'b', 0)` to create undo
  history, `Prepare`, arm, `DispatchKey('Z', true, MK_CONTROL)`; the root is null. (c) the same with Ctrl+Y after an
  undo. Under ASan Debug, (a) reports heap-use-after-free at `DxUi.TextInput.cpp:3106` and (b) at `:3180` today; after
  the fix all are clean. With step 2, also assert a debug counter of callbacks delivered while dispatch depth was
  non-zero stays 0.
- **Verifier notes**: reachable only in embedded hosts whose callback retires controls synchronously; no in-repo
  callback does so in production code and consumer callbacks are unknown, hence P1, not P0. Coalescing hides the bug
  when an earlier invalidation in the same dispatch already called out.
- **Lead note**: `hosts-api.md` states that no use-after-free was found in the embedded paths; this one sits in
  `DxUi.TextInput.cpp`, outside that area's scope, so the two statements do not conflict.
- **Lead note**: raised to P0. The public contract (`include/DxUi/Embedded.h:29`: "invoked synchronously on the UI
  thread. It may retire or replace retained controls") and the project's own retiring fixture allow exactly the
  callback behaviour that frees the `TextField`, and the lead re-traced the chain at `6df17007`:
  `ImportTextInputState` → `InvalidateMeasure` (`src/Controls/DxUi.cpp:1116-1137`) → `_host->Invalidate()` →
  `EmbeddedHost::MarkDirty` → `RequestPreparation` → `callbacks.requestPreparation` (`src/Rendering/Embedded.cpp:263-281`),
  then member writes at `DxUi.TextInput.cpp:3106` onward. A memory-safety defect permitted by the documented contract is
  a stop-ship regardless of how many consumers retire controls in the callback today. Choose one of two fixes in Wave 0:
  narrow the contract back to "must not retire or re-enter the view synchronously" (and change the fixture), or keep it
  and defer the wake while any input entry point is on the stack, plus lifetime guards at every listed site.

## P1 findings

### PRIOR-02 — The menu stop-ship port for RedPrism and RedXe exists only on unpushed local branches

- **Decision (2 November, D10)**: the three branches were pushed: `ej/dxui-compat-redprism-menu-20261101` at
  `a5b3ac81`, `ej/dxui-compat-redxe-menu-20261101` at `a8cb8e1d` and `ej/dxui-delivery-sdk-20261102` at `68f6b15e`. The
  publication half of the fix is done; consumer adoption (pinning those branches or fast-forwarding `wall-compat`) stays
  open in the plan.
- **Severity** P1 (reviewer P1, verifier P1) · **Category** delivery · **Verdict** Confirmed · **Confidence** high (git
  facts), consumer state unknown · **Effort** S · **Overlaps** DOCS-01 (same unpublished commits, evidence facet)
- **Location**: remote refs `origin/ej/wall-compat-redprism-20261028` = `a059e07c`,
  `origin/ej/wall-compat-redxe-20261028` = `616104f3`; local `ej/dxui-compat-redprism-menu-20261101` = `a5b3ac81`,
  `ej/dxui-compat-redxe-menu-20261101` = `a8cb8e1d`; `Measurements/WipDelivery_2026-11-02/README.md:9`;
  `Specs/Plans/Done/CodeReview_2026-10-31.md:127, 155-156, 186-187, 216-218`
- **What**: DX-2 and R28-45 (keyboard and UIA focus must cancel pending submenu hover timers) are fixed on `main`, but
  RedPrism and RedXe "do not merge `main`" and the plan tells them to re-pin the wall-compat branches. The ports exist
  only locally: both port branches have no upstream, `git branch -r --contains` prints nothing for either SHA, and
  `git for-each-ref --contains` lists only the local head. The remote wall-compat refs (fetched 2026-11-02 15:44) still
  hold the 28 October locks. The ports are strict descendants of those refs (12 and 7 commits ahead), so a fast-forward
  publishes them. The delivery packet cites both local SHAs as qualified.
- **Failure scenario**: RedXe follows Wave 4 step 4 and re-pins `origin/ej/wall-compat-redxe-20261028`. It gets
  `616104f3`, with neither cancel: arrow keys and UIA focus on an open submenu still lose to a pending hover timer that
  replaces or closes the keyboard-owned child.
- **Fix**: with the owner's authorization (AGENTS.md: Git actions use the user's authorization, no force push),
  fast-forward the published branches: `git push origin a5b3ac81:refs/heads/ej/wall-compat-redprism-20261028` and
  `git push origin a8cb8e1d:refs/heads/ej/wall-compat-redxe-20261028` (no force needed), or publish under new names and
  update the re-pin instruction in the consumer WIPs. Record the full SHAs and the observed `git branch -r --contains`
  output in `Measurements/WipDelivery_2026-11-02/README.md`. DOCS-01 keeps its own parts (packet "Subject branches"
  line, the PrePush SHA-reachability rule, SDK source `68f6b15e`).
- **Regression test**: `git merge-base --is-ancestor a8cb8e1d origin/ej/wall-compat-redxe-20261028` exits 0 (1 today);
  same for `a5b3ac81` against the redprism ref. DOCS-01's PrePush rule prevents recurrence.
- **Verifier notes**: limitation: consumer lock files are outside this repository, so whether RedPrism or RedXe already
  pinned something else cannot be seen. DOCS-01 rated the evidence facet P2 with "escalate to P1 if RedPrism or RedXe
  are to adopt"; Wave 4 says they adopt these branches, so the delivery facet is P1 and primary here.

## P2 findings

### PRIOR-07 — One failing `PrepareContent` aborts every native frame of the whole window, silently

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug (availability) · **Verdict** Confirmed (fix corrected)
  · **Confidence** high · **Effort** M · **Introduced** pre-existing; CR-16 closed only the embedded half
- **Location**: `src/Controls/DxUi.Controls.cpp:2433-2447`, `:2522-2526`, `:2563-2575`;
  `src/Controls/DxUi.WindowHost.cpp:4942-4949` (dead copy at 4642-4649), `:3336-3346`, `:1876-1887`
- **What**: a FAILED `PrepareContent` keeps the control pending, records `_contentPreparationFailure` and sets
  `complete = false`; `PreparePendingPageLayout` returns false with `_contentPreparationContinues` false. `Render`
  returns before drawing any control and does not invalidate; `BeginPaint` already validated the region;
  `RenderInitialFrameForShow` still returns true. The native host never reads `_contentPreparationFailure` (only
  `src/Rendering/Embedded.cpp:566` does) and logs nothing. There is no self-scheduled retry, which is the contract
  (`Specs/UI/UI_ControlsAndLayout.md:1816`, "must not self-reschedule", pinned by
  `Tests/Controls/Controls.Tests.RichTextEditor.cpp:1162-1180`); the failed preparation re-runs on the next unrelated
  invalidation because `_pendingContentPreparation` stays true. So a transient failure loses frames until input, and a
  deterministic one aborts every later frame of every control in the window. Intrinsic-layout failure, by contrast,
  paints the previous geometry (`DxUi.Controls.cpp:2403-2421`).
- **Failure scenario**: a RichTextEditor's preparation fails deterministically, for example the tokenizer
  `bad_alloc` of EDIT-04 (`editor.md`), a `wil::ResultException` from DirectWrite, or the `std::length_error` branch at
  `DxUi.RichTextEditor.cpp:4063-4077`. A freshly shown window never presents a DxUi frame; an existing one keeps its
  last frame, and no Label, Button or caret ever updates while that document is shown. Nothing is reported.
- **Fix**: scope the failure to the control.
  - In `PreparePendingPageLayout`, record a failed content preparation per control (for example
    `_contentPreparationFailed` plus the HRESULT) instead of `complete = false`; exclude failed controls from the
    `contentPending` aggregate that decides the return value, so the frame is painted when only failed (not pending)
    controls remain.
  - A failed control stays failed until its next `RequestContentPreparation` (content, bounds, resource or typography
    change), mirroring the intrinsic `_intrinsicAllocationRetryUsed` pattern; one transient `E_OUTOFMEMORY` allowance
    may re-arm the pending flag, never `Invalidate()`.
  - Report once per control and HRESULT with `Debug::Warning`, deduplicated like `_reportedLayoutFailureControl`.
  - Pitfall: `RichTextEditor::Paint` throws through `RequireCurrentLayout` (`DxUi.RichTextEditor.cpp:4605`,
    `:4143-4148`) when its layout is not current; it must paint its chrome and return when its content failed.
  - Embedded keeps its documented contract (Prepare returns the HRESULT); no change there.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextEditor.cpp` next to the failing-editor fixture at 1139:
  a visible non-activated `AttachedHostWindow` whose root Panel holds a Label and a `FailingEditor` returning
  `E_FAIL`. After `RenderInitialFrameForShow()`, `DebugGetRenderCount()` increased and `DebugCaptureBitmap(capture)`
  succeeds with Label ink present (both fail today); the editor's call count stays stable over a 40 ms idle pump (keeps
  the existing no-loop assertion).
- **Verifier notes**: the reviewer's "one bounded `Invalidate()` retry" contradicts the contract and the pinned test; it
  is replaced above. The window is not blank after its first frame; it is frozen.

### PRIOR-08 — Seven P1 fixes have no test that fails when the fix is reverted

- **Severity** P2 (reviewer P2, verifier P2) · **Category** test · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M
- **Location**: fixes at `src/Controls/DxUi.Grid.cpp:6490`, `src/Controls/DxUi.Tree.cpp:2961` (R28-01);
  `src/Controls/DxUi.Controls.cpp:2530-2560` (R28-02); `src/Controls/DxUi.RichSemanticGeometry.cpp:1404` (R28-04);
  `DxUi.Grid.cpp:1496-1540`, `DxUi.Tree.cpp:836-988` (R28-12); `src/Controls/DxUi.WindowHost.cpp:3890` (R28-40);
  `DxUi.Grid.cpp:1208-1240` (R28-44); `src/Controls/DxUi.Accessibility.cpp:7809-7810` (R28-11)
- **What** (search of `Tests/` per item):
  - R28-01: none. No Grid or Tree test grows bounds after scrolling; `TestGridKeyboardViewportAndColumnIdentity`
    (`Tests/Controls/DxUi.Tests.Grid.cpp:5176-5200`) only shrinks.
  - R28-02: none. `Tests/Embedded/Embedded.Tests.RichTextResources.h:4008-4011` checks that the pending editor's own
    capture is cancelled, which also passes without the fix; `Tests/Embedded/Embedded.Tests.Embedded.cpp:2250-2269`
    keeps a Splitter drag across layout with no pending content.
  - R28-04: none effective. `Tests/Support/Support.Tests.RichSemanticGeometry.h:1849-1861` has an RTL Subtle case but
    asserts only caret hit affinity and width within the available width, which a collapsed table also satisfies.
  - R28-12: none. No invalidation-count assertion on any of the setters.
  - R28-40: none. `TestNativeUnhandledKeysReachApplicationFallback`
    (`Tests/Controls/Controls.Tests.HostControlRegressions.cpp:607-656`) has no read-only case.
  - R28-44: weak. `TestGridNoexceptGeometryContainsThrowingGroupGetter` (`DxUi.Tests.Grid.cpp:237-250`) uses a throwing
    getter after assignment, not the selection-copy allocation failure that was the use-after-free.
  - R28-11: weak. `ScrollingCommands` (`Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:507-596`) calls
    `fixture.Publish()`, which runs `RefreshAccessibilitySnapshot` (94-107), after every `ScrollIntoView`; R28-11 asked
    to drop exactly that refresh.
  - The project's receipt agrees (`Measurements/WipDelivery_2026-11-02/raw/historical/historical-per-id.md.txt:91-132`:
    "not yet located", "untraced").
- **Failure scenario**: a refactor reverts any of these fixes and every suite stays green.
- **Fix and regression tests** (each fails if its fix is reverted):
  - R28-01: `DxUi.Tests.Tree.cpp`: 100 rows, height 120, scroll to the end, `SetBounds` to 4× the height, assert
    `DebugGetVerticalScrollDip() == 0`. `DxUi.Tests.Grid.cpp`: same growth, then assert a press at the first body row
    selects row 0 (as at `DxUi.Tests.Grid.cpp:2632-2634`; Grid has no scroll accessor). Run native and embedded.
  - R28-02: embedded fixture with a Splitter and the `OneDeferredPreparation` editor
    (`Embedded.Tests.RichTextResources.h:4097-4108`, returns `S_FALSE` once): press and move the Splitter, request
    editor content, `Prepare`; assert `GetCapturedControl() == splitter` and no Cancel phase.
  - R28-04: RTL Subtle table with columns "a" and "a much longer cell"; assert the second cell's `ElementBounds` width
    exceeds the first by more than 40 DIP.
  - R28-12: for each setter, assert `DebugGetInvalidateCount()` grew by at least 1 (it counts even for a hidden host,
    `DxUi.WindowHost.cpp:1822-1824`).
  - R28-40: add a focused read-only RichTextEditor to the fallback test; `SendMessageW(WM_KEYDOWN, 'N')` returns the
    application fallback value.
  - R28-44: add a diagnostics seam that fails the next selection snapshot allocation in `Grid::SetModel`; assert
    `GetModel() == &newModel` after the old model is destroyed and that paint and hit-test are ASan-clean.
  - R28-11: between `ScrollIntoView` and the `RectangleValues` read, run only content preparation (native
    `PrepareNativeContentForTests`, embedded `Prepare`), not `RefreshAccessibilitySnapshot()` or `bridge->Update()`.
  - R28-41: see PRIOR-06.
- **Verifier notes**: every listed search was repeated at HEAD; the only directed tests found are the ones named.

### PRIOR-21 — A masked TextField leaves the IME active and swallows its commits

- **Severity** P2 (reviewer P2, verifier P2) · **Category** privacy / input · **Verdict** Confirmed (code facts;
  scenario corrected) · **Confidence** medium · **Effort** M
- **Location**: `src/Controls/DxUi.NativeTextInput.cpp:1713-1727`, `:1331-1375`; `src/Controls/DxUi.WindowHost.cpp:3958-3960`
- **What**: `src` contains no `ImmAssociateContext*`, `IS_PASSWORD`, input scope or keyboard-disabled compartment. For a
  masked field the host deactivates TSF and drops its own preview, but `CancelNativeTextInputImeComposition` only resets
  DxUi state; it never tells the IME to stop. `WM_IME_STARTCOMPOSITION`, `WM_IME_COMPOSITION` and
  `WM_IME_ENDCOMPOSITION` are swallowed; `WM_IME_SETCONTEXT` and `WM_IME_NOTIFY` still reach `DefWindowProc`. The code
  comment at 1716 promises "password text is never an input-method candidate or composition buffer"; the IME still
  receives every keystroke.
- **Failure scenario**: a Japanese user in kana mode focuses a password field. The IME composes each keystroke (and may
  learn it, predict it or send it to cloud candidates), can show its candidate window on conversion, and the committed
  result is swallowed, so the password cannot be typed until the IME is switched off. The Win32 `ES_PASSWORD` edit
  disables the IME instead.
- **Fix**: on masked activation call `ImmAssociateContextEx(hwnd, nullptr, 0)` and restore with `IACE_DEFAULT` on
  deactivation or unmask; for TSF set `IS_PASSWORD` through `ITfInputScope` or set
  `GUID_COMPARTMENT_KEYBOARD_DISABLED` on the context. Report `TS_SD_READONLY` from `GetStatus` for masked fields
  (CTRL-03). Embedded: document that the application's text service must apply the same policy for
  `EmbeddedTextInputSnapshot` states with `masked` set.
- **Regression test**: `Tests/Controls/DxUi.Tests.NativeTextInput.cpp` (interactive suite): focus a masked TextField,
  require `ImmGetContext(hwnd) == nullptr`; unmask and require a context again. Fails today.
- **Verifier notes**: the reviewer's "reading shown in clear" is unlikely because `DefWindowProc` never sees the
  composition messages; the candidate window and IME learning remain. Visual behaviour is not verified at runtime.

### PRIOR-25 — Keyboard trap: an all-collapsed grouped Grid ignores every key

- **Severity** P2 (reviewer P2, verifier P2) · **Category** accessibility · **Verdict** Confirmed · **Confidence** high
  · **Effort** S
- **Location**: `src/Controls/DxUi.Grid.cpp:5884-5890`, `:5917-5945`
- **What**: `Grid::OnKeyDown` returns false when `visibleRows` is empty. Keyboard group toggling
  (`toggleGroupFromKeyboard`) starts from a visible row, so when every group is collapsed no key expands anything;
  only pointer or UIA can.
- **Failure scenario**: a grouped Grid with two collapsed groups and no ungrouped rows. A keyboard-only user tabs in;
  Right, Enter, Space, Home and Down all return false.
- **Fix**: when no rows are visible but groups exist, treat the first group header as the keyboard position: Right,
  Enter and Space expand it through `OnGridGroupToggled` (with the existing `TryControlCallback` and
  `guard.ResumeAfterCallback()` pattern), Home and Down focus it.
- **Regression test**: `Tests/Controls/DxUi.Tests.Grid.cpp`: `GroupedGridModel` with two collapsed groups covering all
  rows and a recording delegate; `OnKeyDown(VK_RIGHT)` returns true and records one toggle to expanded. Fails today.

### PRIOR-26 — A control disabled or hidden mid-drag inside a ScrollPanel keeps dragging

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.WindowHost.cpp:5448-5470` (`CancelStaleCapture`);
  `src/Controls/DxUi.Controls.cpp:10903-10910` (`ScrollPanel::LiveInnerCaptured`), `:11505-11510`, `:7000-7008`
- **What**: inside a ScrollPanel the host capture is the panel; the dragged child is `_innerCapturedChild`.
  `CancelStaleCapture` judges only the panel, `LiveInnerCaptured` checks lifetime and membership only, and the panel
  forwards moves to the child. `Slider::OnMouseMove` has no enabled check. The comment at `DxUi.WindowHost.cpp:5450-5451`
  says the purpose is to stop a disabled or hidden slider, splitter, picker or tree mid-drag.
- **Failure scenario**: a Slider in a ScrollPanel is being dragged; the application disables it (for example while a
  job runs). The value keeps following the pointer and a Commit fires on release.
- **Fix**: in `CancelStaleCapture` resolve `ControlCaptureAccess::GestureOwner(*this)` (`src/Controls/DxUi.Internal.h:34`)
  and judge its interactivity; when the gesture owner is a forwarded child that is no longer effectively interactive,
  call `CancelCapturedControlInteraction()`, which reaches `ScrollPanel::OnCaptureLost` and the child's
  `OnCaptureLost` (restores the draft). Also return null from `LiveInnerCaptured` for a child that is not effectively
  enabled and visible.
- **Regression test**: `Tests/Embedded/Embedded.Tests.Embedded.cpp` beside the direct case at 2236-2245: Slider inside a
  ScrollPanel, press and move, `SetEnabled(false)`, `Prepare`, move; expect one Cancel, the value restored and no
  capture. Repeat with `SetVisible(false)`. Fails today.

### PRIOR-27 — `NumericStepper::SetValue` during a typed edit loses the terminal Commit or Cancel

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug · **Verdict** Confirmed (extended) · **Confidence** high
  · **Effort** S
- **Location**: `src/Controls/DxUi.EditorControls.cpp:973-980`, `:782-821`, `:1173-1213`
- **What**: `SetValue` clears `_editing` and `_editPreviewed` unconditionally. `CommitEdit` returns early without a
  Commit when `! _editing` and the parsed value equals `_value`. The class promises (1206-1207) that every preview ends
  with a Commit or Cancel; any `SetValue` during a previewed edit breaks that.
- **Failure scenario**: value 10; a Preview handler echoes `SetValue(GetValue())` (two-way binding). The user types "15"
  and presses Enter. "1" previews and is echoed (edit ended), "15" starts a new edit from 1 and is echoed again, Enter
  finds no edit in progress: no Commit with 15.
- **Fix**: if `_editing` and the clamped value equals `_value`, return without touching the edit state. If a different
  value arrives during an edit, end the edit explicitly: emit Cancel when `_editPreviewed` (consumers restore their
  preview), then apply the new value and `SyncText()`.
- **Regression test**: `Tests/Controls/DxUi.Tests.EditorControls.cpp`: the echoing handler above; type "1", "5", Enter;
  expect exactly one Commit with 15. Second case: `SetValue(3)` after a preview; expect one Cancel then value 3. Both
  fail today.

### PRIOR-28 — About 20 further ProductionReview and CodeReview_2026-10-10 items are open in code with no tracking

- **Severity** P2 in aggregate (reviewer P2, verifier P2) · **Category** process / bugs · **Verdict** Plausible
  (two spot-checked) · **Confidence** medium · **Effort** L in aggregate
- **Location**: P2 items: menu-layout-ux-2 (an existing test asserts the bug), menu-loop-lifetime-6,
  menu-loop-lifetime-7 (`src/Controls/DxUi.Menu.cpp:4742` destroys before 4755 creates, focus flickers to the owner),
  tree-8, controls-editor-theme-5 (Slider and Button transitions freeze under a hidden ancestor), grid-multiline-4
  (`DxUi.Grid.cpp:3771`, 3878), grid-multiline-2 (491-492), grid-multiline-1 (tooltip re-layout per move,
  `DxUi.Controls.cpp:11827-11839`), grid-selection-lifetime-7, uia-selection-semantics-9/15 (menu row names omit the
  accelerator and value, `DxUi.Menu.cpp:3560-3567`), uia-lifetime-threading-9 (`DxUi.Accessibility.cpp:8986`, the root
  answers `HasKeyboardFocus` false unconditionally), uia-selection-semantics-13. P3 items: text-input-6
  (`DxUi.TextInput.cpp:1738-1741`), text-input-12, text-input-16, text-input-20b (`DxUi.NativeTextInput.cpp:1592-1594`,
  test locks the bug), high-contrast `subduedText` (`DxUi.Theme.cpp:1143`), controls-editor-theme-10/11/12,
  host-core-4, grid-selection-lifetime-5/12.
- **What**: these rows are unchecked or partially fixed in Done plans and absent from the closeout's open list.
- **Fix**: reopen them as tracked rows (the DOCS-05 reconciliation pass) and fix the P2 rows before declaring delivery.
- **Regression test**: per item, as in the source plans.
- **Verifier notes**: uia-lifetime-threading-9 and menu-loop-lifetime-7 were confirmed in code; the rest are from the
  delegated audit and not re-traced.

## P3 findings

### PRIOR-06 — The R28-41 test renders the recovery frame itself; non-device-loss resize failure schedules no repaint

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test / bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Related** HOST-06 (siblings on a reset pool, masked by the same test)
- **Location**: `Tests/Controls/Controls.Tests.NativeDevicePolicy.h:38-56`, `:79-101`;
  `src/Controls/DxUi.WindowHost.cpp:4463-4479`, `:1814-1829`
- **What**: `isSoftware` calls `RenderInitialFrameForShow()`, so the post-loss frame is test-driven; deleting the
  `Invalidate()` at 4477 leaves the test green. The window is hidden, and `ControlHost::Invalidate` is a no-op for a
  hidden window (1825), so the test could not observe the scheduled repaint anyway. A non-device-loss `ResizeBuffers`
  failure discards size resources and returns false with no `Invalidate()`, although the warning says recreation is
  deferred "to a later visible render".
- **Failure scenario**: `ResizeBuffers` fails with `E_OUTOFMEMORY` during a maximize; the window keeps its stale frame
  until unrelated input.
- **Fix**: in the non-device-loss branch call `Invalidate()` once, guarded by a per-size flag so a repeated failure at
  the same size does not loop (consistent with the no-self-reschedule rule for repeated failures).
- **Regression test**: in the stage-4 case record `DebugGetInvalidateCount()` (it increments before the visibility
  check, 1822-1824) before the injected failure and assert it grew before any test-driven render; add a stage-4
  injection with `E_OUTOFMEMORY` and assert the same. The second fails today.

### PRIOR-10 — DeleteTableColumn turns a pipe-less table row into a blank line

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** commands-01 fix
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:3933-3940`
- **What**: for a single-slot row the deletion writes `L" "`; a whitespace-only line ends the table.
- **Failure scenario**: source "| a | b |", "| - | - |", "x", "| y | z |" (one per line), caret 2, DeleteTableColumn:
  row "x" becomes a blank line and "| z |" becomes a paragraph. [A] also reports a header-row variant that turns into a
  setext heading.
- **Fix**: when the edited row would contain no unescaped pipe, emit edge pipes: "| |" for a single-slot body row,
  "| b |" and "| - |" for header and delimiter rows.
- **Regression test**: `Tests/Support/Support.Tests.RichSemantic.h` command table: the input above with
  DeleteTableColumn expects "| b |", "| - |", "| |", "| z |" and exactly one Table node. Fails today.

### PRIOR-11 — Enter after a quoted thematic break drops the quote prefix

- **Severity** P3 · **Category** bug · **Verdict** Plausible (delegated audit, not re-traced) · **Effort** S
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:1436-1458`; test pinning the residual near
  `Tests/Support/Support.Tests.RichSemantic.h:744`
- **What / failure scenario**: source `L"> ***\n> b"`, caret 5, InsertParagraph yields `L"> ***\n\n> b"`: the quote
  splits in two.
- **Fix**: in the non-leaf quote case set `prefix = Slice(line.first, at)`.
- **Regression test**: expect `L"> ***\n> \n> b"`; update the pinned expectation.

### PRIOR-12 — Email autolink candidates revalidate the domain outside the work counter

- **Severity** P3 · **Category** perf · **Verdict** Plausible · **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1260-1266`, `:1321-1322`
- **What**: `Email(...)` is called without the work counter; the comment claims one domain validation per '@' but no
  per-'@' cache is visible at the call. The delegated audit estimates about 2×10⁸ character checks for 10,000 repeats
  of "._a", one '@', then 10,000 repeats of "b.".
- **Fix**: pass the autolink work counter and cache domain validity per '@' position.
- **Regression test**: add that input to `Tests/Support/Support.Tests.MarkdownCapacity.h:103-126` with `Email` counted
  and require `autolinkInspectionUnits ≤ 8n`.

### PRIOR-13 — RichText tests use self-maintained or mirrored oracles

- **Severity** P3 · **Category** test · **Verdict** Plausible (blocks-02 confirmed) · **Effort** S each
- **Location**: `Tests/Support/Support.Tests.SemanticSourceBlock.h:296-308` (blocks-02, only `lines.size() >= 2u`);
  inlines-02 (asserts its own counter); inlines-04 (no test); `Tests/Support/Support.Tests.MathCorpus.h:362`, `:388-414`
  (math-layout-05/06 recompute the implementation's constants)
- **Fix and tests**: exact `EmptyLines()` for "    code" followed by two newlines; inlines-04: 8,000 repeats of "*a* " in
  the rejected list; math layout: absolute ratios from TeX/KaTeX, for example inline `\frac12` total height at most
  about 1.7 em.

### PRIOR-14 — Disposition and status documents contradict the code

- **Severity** P3 · **Category** doc · **Verdict** Confirmed (math-parser-12 and MM-34 re-read) · **Effort** S
- **Location**: `Specs/Plans/Done/RichTextAudit_2026-10-21/MathFindingsDisposition_2026-10-24.md:24` (math-parser-12
  "refuted"; the code now rejects the double backslash outside rows, `b5869cbc`);
  `Specs/Plans/Done/RichTextAudit_2026-10-21/MermaidRevalidation_2026-10-24.md:68` (MM-34 "Remains"; accTitle and
  accDescr are now metadata); `docs/rich-text.md:386-388` (gallery claim, from the delegated audit);
  `Specs/Plans/Done/RichTextEditor_2026-10-17.md:3022`, `:3107` (open boxes under DONE, also in DOCS-05)
- **Fix**: update both dispositions with the fixing commit; correct or remove the gallery sentence.
- **Regression test**: none (documentation); DOCS-05's Done-plan rule covers the boxes.

### PRIOR-16 — `Grid::SetModel` failure clears the selection without notifying

- **Severity** P3 · **Category** bug · **Verdict** Confirmed · **Effort** S · **Introduced** `b5869cbc`
- **Location**: `src/Controls/DxUi.Grid.cpp:1214-1229`, `:1233-1244`, `:1314`
- **What**: on a snapshot `bad_alloc` the selection, focus and current row are cleared and
  `selectionSnapshotAvailable` is false, which skips `NotifySelectionAndFocusChanges`; `clearFailedModelView` paths
  return before any notification. The application's selection mirror goes stale.
- **Fix**: after clearing, report the empty selection once under `guard.IsCurrent()`.
- **Regression test**: `Tests/Controls/DxUi.Tests.Grid.cpp`: a delegate plus the throwing-getter fixture (237-250)
  expects one `OnGridSelectionChanged` with an empty set. Fails today.

### PRIOR-17 — Receipt character dispatch reports a consumed, possibly committed character as unhandled on `std::exception`

- **Severity** P3 · **Category** bug · **Verdict** Plausible (narrowed) · **Effort** S
- **Location**: `src/Rendering/Embedded.cpp:1154-1168`; `include/DxUi/Embedded.h:176-181`
- **What**: returning `OnChar`'s false for a declined character is the ordinary "not handled" meaning and is fine. The
  inconsistency is the exception path: `bad_alloc` returns true (1156-1162) but any other `std::exception` returns
  false (1163-1168) after the receipt was consumed and the control may have committed.
- **Fix**: return true from the `std::exception` handler, as the `bad_alloc` handler does.
- **Regression test**: `Tests/Embedded/Embedded.Tests.Embedded.cpp`: a focused control whose `OnChar` commits then
  throws `std::runtime_error`; the receipt dispatch returns true. Fails today.

### PRIOR-19 — `UpdateHover` runs an uncached whole-tree prune on every mouse move

- **Severity** P3 · **Category** perf · **Verdict** Confirmed · **Effort** S
- **Location**: `src/Controls/DxUi.WindowHost.cpp:5716-5723`, `:1072-1140`, `:3225-3240`; also reached from
  `src/Rendering/Embedded.cpp:872`, `:879`
- **What**: after the cached message-entry prune, `UpdateHover` calls `PruneStaleInteractionState()` with no pass;
  `ResolveControlInteractionState` is a depth-first search from the root for each live observer (capture, hover,
  focus, text input), so each `WM_MOUSEMOVE` costs up to four tree walks.
- **Fix**: pass the message's `MessagePrunePass`, or skip the prune when `_interactionRevision` is unchanged since the
  entry prune.
- **Regression test**: with the resolver node-visit diagnostics counter, one `WM_MOUSEMOVE` over a 1,000-control tree
  visits at most one tree's worth of nodes; plus a paired measurement.

### PRIOR-20 — The menu stop-ship regressions run only in the interactive suite

- **Severity** P3 (reviewer P2, verifier P3) · **Category** test coverage · **Verdict** Confirmed · **Effort** M
- **Location**: `Tests/test-scopes.json:570-577`; `Tests/Controls/DxUi.Tests.Menu.cpp:5689`
- **What**: `TestMenuKeyboardInputCancelsPendingSubmenuHoverTimers` is strong but runs only under `-Interactive`, so
  default lanes never catch a regression of DX-2 or R28-45.
- **Fix**: add a non-activating test that drives `RouteMenuKeyboardEvent`, the UIA focus path and the hover-timer
  handler on an owned, non-activated popup (the stationary-hover fixtures create such popups) and asserts the cleared
  timer state and the revision refusal.
- **Verifier notes**: lowered to P3: the test exists and is strong, focus-taking suites are gated by policy, and a
  non-activating variant's feasibility is unproven.

### PRIOR-24 — The opening key's queued `WM_CHAR` is taken as a menu mnemonic

- **Severity** P3 · **Category** bug · **Verdict** Confirmed (acknowledged deferred in
  `Specs/Plans/Done/CodeReview_2026-10-10.md:1138-1139`) · **Effort** S
- **Location**: `src/Controls/DxUi.Menu.cpp:6283-6299`
- **What / failure scenario**: a context menu opened from the `WM_KEYDOWN` of 'R' receives the queued `WM_CHAR` 'r' in
  its modal loop, which maps every `WM_CHAR` to a mnemonic, and invokes "&Rename". No opening-key filter exists.
- **Fix**: in the loop's first turn drop one `WM_CHAR` whose message time and scan code match the opening key-down.
- **Regression test**: interactive Menu suite: post `WM_CHAR` 'r', `Show` a menu with "&Rename"; require no invoke.

## Duplicates carried by other area files

| ID | Merged into | Extra facts kept here |
| --- | --- | --- |
| PRIOR-01 | DOCS-05 (Done plans keep unchecked boxes); DOCS-21 (closeout says "covered") | Six plans: 42/0, 11/0, 54/0, 54/0, 118/22, 32/58 open/checked (311 open). `CodeReview_2026-10-31.md:274-276` gates its own move to Done on implemented waves and passing tests; the per-ID receipt says PARTIAL TRACE for every R28 row checked here. |
| PRIOR-04 | RENDER-02 | `docs/rich-text.md:950-962` lists whole-geometry exceptions but not `lineHeightMultiplier > 0`. The R28-03 test (`Tests/Controls/Controls.Tests.RichDocumentLayout.cpp:497-521`) asserts `UsesWholeGeometry()` on four lines; the Wave 6 three-chunk pitch oracle is absent. R28-03 is fixed for correctness, not "fix wrong". |
| PRIOR-05 | CMD-08 | DX-3 rejects only quote, CR, LF (`DxUi.RichTextEditor.cpp:1048`), which NTFS forbids anyway; `Tests/Support/Support.Tests.RichImagePaths.h:278-289` uses a benign path, so it cannot fail on delimiters. An injected image token reaches local image resolution. |
| PRIOR-15 | CTRL-03 | Masked fields interact with PRIOR-21: report `TS_SD_READONLY` with `S_OK`. |
| PRIOR-18 | HOST-04 | Correction: `include/DxUi/ControlCatalog.h` is unchanged in `93c77750..6df17007`; the `ControlKind` insertion predates PR #159 and DX-11 is optional (`CodeReview_2026-10-31.md:276`). The four new `Control` virtuals are in HOST-04. |
| PRIOR-22 | DOCS-12 (privacy), DOCS-14 (size) | CR-34 was never claimed fixed: `CodeReview_2026-10-10.md:737-760` leaves it waiting on owner decisions Q11, Q18, Q19, Q20. |
| PRIOR-23 | ARCH-04, HOST-14 (dead `#else` arms); ARCH-07 (test seams in the shipping library) | `Debug…(` occurrences in `include/DxUi/DxUi.h` grew from 164 at `93c77750` to 177. CR-37 was deferred to the next API revision (`CodeReview_2026-10-10.md:1141-1143`). |

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| PRIOR-09 | CR-24 residual: a throwing Tree delegate (13 unwrapped calls) leaves native capture stuck, so every later click goes to the Tree | `Tree::OnMouseUp` releases capture before each delegate call (`src/Controls/DxUi.Tree.cpp:2010` before 2027/2042; 2066-2070 before 2083). The host captures only after a handled down returns (`src/Controls/DxUi.WindowHost.cpp:3634-3637`), and Tree's own `CaptureMouse` (1873, 1886) follows the selection delegates (1834). None of the 13 enclosing functions is `noexcept`; the dispatch boundary is the documented containment point (`Specs/UI/UI_InputAndAccessibility.md:816-821`, `DxUi.WindowHost.cpp:4032-4037`). Residual (P3, optional): that catch does not cancel capture, which matters only for a custom control that throws in `OnMouseUp` before releasing its own capture; a throw at `DxUi.Tree.cpp:3120` skips the focus notification at 3134. |

## Patterns

1. **Closure by narrative, not by checkbox.** Six plans moved to Done with 311 open boxes while their own receipt says
   PARTIAL TRACE (DOCS-05).
2. **One mega-commit, few directed tests.** `b5869cbc` fixes about 16 stop-ship items; R28-01, -02, -04, -12 and -40
   have no test that fails on revert, and four of seven named Wave 6 tests are absent (PRIOR-08).
3. **Tests that supply the recovery themselves.** R28-41 renders the frame, R28-11 republishes the snapshot, and the
   device-policy test renders the sibling too (PRIOR-06, PRIOR-08, HOST-06).
4. **Tests that assert the mechanism or the implementation's own value.** R28-03 asserts `UsesWholeGeometry()`,
   inlines counters assert themselves, math oracles recompute the constants, commands-09 pins the residual
   (RENDER-02, PRIOR-11, PRIOR-12, PRIOR-13).
5. **Contract widened, call sites not audited.** `requestPreparation` may now retire controls; setters were guarded but
   the TextField input paths were not (PRIOR-03). CR-16 was closed by re-specifying the embedded half while the native
   host still abandons whole frames (PRIOR-07).
6. **Fixes scoped to the named example.** DX-3 rejects the characters NTFS already forbids (CMD-08); commands-01 removes
   the out-of-bounds read but writes a blank row (PRIOR-10).
7. **Delivery across branches not closed.** The consumer half of the menu stop-ship exists only on unpushed branches
   (PRIOR-02).
8. **Interactive-only regression coverage** for the top stop-ship (PRIOR-20).

## Area architecture and simplification proposals

1. **Defer embedded wakes while an input entry point is on the stack** (PRIOR-03). Rationale: one change in
   `src/Rendering/Embedded.cpp` removes the use-after-free class for every control's input path instead of per-site
   guards. Files: `Embedded.cpp` (`RequestPreparation`, entry points), `include/DxUi/Embedded.h` (sentence: delivered
   before the entry point returns). Risk: low; delivery stays synchronous with respect to the caller. Effort M.
   Before production.
2. **Per-control content failure in the native host** (PRIOR-07). Rationale: align native with intrinsic-layout
   failure (paint what is ready, keep the failed control pending, report once) and with the embedded HRESULT report.
   Files: `src/Controls/DxUi.Controls.cpp` (`PreparePendingPageLayout`), `src/Controls/DxUi.WindowHost.cpp` (`Render`),
   `src/Controls/DxUi.RichTextEditor.cpp` (`Paint`). Risk: medium (frame gating). Effort M. Before production.
3. **Post-wake lint** (PRIOR-03). A tooling test that scans `src/Controls/*.cpp` for `InvalidateMeasure()`,
   `RequestInvalidate()`, `Invalidate(host)` or `RequestAnimation()` followed by a member statement without an
   `expired()` check in between, with an allow marker. Files: `Tools/tests/`. Risk: false positives. Effort S.
   Opportunistic.

## Test-quality observations

- `Tests/Controls/Controls.Tests.NativeDevicePolicy.h:38-56`: `isSoftware` renders the frame it then counts.
- `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:507-596` (`ScrollingCommands`): `Publish()` after every
  scroll hides R28-11.
- `Tests/Controls/Controls.Tests.RichDocumentLayout.cpp:497-521`: asserts the whole-geometry mechanism, not pitch.
- `Tests/Controls/DxUi.Tests.Grid.cpp:237-250`: throwing getter stands in for the selection-copy `bad_alloc`.
- `Tests/Support/Support.Tests.RichSemanticGeometry.h:1849-1861`: the RTL Subtle case passes with collapsed columns.
- `Tests/Embedded/Embedded.Tests.RichTextResources.h:4008-4011`: proves the self-capture cancel only, not R28-02.
- `Tests/Controls/Controls.Tests.RichTextEditor.cpp:1162-1180`: pins "no repaint loop" but never checks that other
  controls paint while one control's preparation fails (PRIOR-07).
- `Tests/Support/Support.Tests.RichImagePaths.h:278-289`: benign path only; cannot fail on delimiter characters.
- `Tests/Controls/Controls.Tests.IntrinsicLayout.cpp:1231-1355`: covers retiring setters but no input path
  (`ApplyTextInput`, Ctrl+Z), which is where PRIOR-03 lives.

## Questions for the developer

1. Q1: the header now allows `requestPreparation` to retire controls (`d04ab124`). Keep that contract and add the
   dispatch-depth deferral plus guards? Recommended: yes; reverting would contradict the spec and the retiring
   fixtures.
2. May the compat ports `a5b3ac81` and `a8cb8e1d` be fast-forwarded onto the published wall-compat branches, or should
   they be published under new names? Recommended: fast-forward; both are strict descendants.
3. Native content failure: should one control's failed preparation still block the whole window's frame?
   Recommended: no; paint the frame and report once, with no self-scheduled retry.
4. Have RedTilde and RedSalamander moved their `Dependencies/DxUi.lock.json` to a commit at or after `6df17007`? This
   repository cannot show it.
5. Masked fields: disable the IME (IMM association and TSF password input scope) as the Win32 password edit does?
   Recommended: yes.
6. `NumericStepper::SetValue` during an edit: ignore an unchanged value and end a changed one with Cancel?
   Recommended: yes.

## Not covered

- CodeReview_2026-10-28 P2/P3 items beyond the table: R28-15, R28-18/19, R28-20 to R28-23, R28-25 to R28-27, R28-29 to
  R28-36, R28-42, R28-43, R28-47 to R28-50, R28-52, R28-53 and `Test-CiGate` `EXPECTED_*`.
- R28-23 and R28-24 accepted from the receipt; DX-5 (`dfedb096`) and R28-16 (`f7ed0578`) tests exist but were not read.
- RichText audit: remaining MermaidRevalidation and MathFindingsDisposition rows, completeness.md missing-feature rows
  (MD-01..MD-21, MA-02/09-19, MM-01..MM-56), tests-engine-02, tests-host-02, minor items. PRIOR-11 and PRIOR-12 were not
  re-traced.
- CodeReview_2026-10-10: CR-19, -23, -26, -28, -33, -35, -36; N-1 to N-21 (N-16..N-21 partial or deferred), R-1 to
  R-12; hosted CI state (CR-15) and the external formatting engine (CR-30). ProductionReview sections other than P0,
  P1 and Security. PRIOR-28 items other than the two spot-checked.
- Consumer repositories (lock files, adoption state) and the live remote beyond the local `origin/*` refs fetched
  2026-11-02 15:44.
- No runtime confirmation: the use-after-free (PRIOR-03), IME behaviour (PRIOR-21) and performance claims (PRIOR-12,
  PRIOR-19) are static traces.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.TextInput.cpp` | touched functions in full: `SetText`, setters 962-1440, `OnMouseDown` clear button, `OnKeyDown` undo/redo, `ImportTextInputState`, `RestoreEditHistoryState`, undo/redo helpers, `NotifyChanged` |
| `src/Rendering/Embedded.cpp` | `Attach`, `Detach`, `ReplaceDevice`, `InvalidateThunk`, `MarkDirty`, `RequestPreparation`, `Prepare` entry, `DispatchKeyImpl`, `DispatchCharacter` (receipt), `ApplyTextInput` |
| `src/Controls/DxUi.cpp` | `InvalidateMeasure`, `RequestInvalidate`, `SetEnabled`, `d04ab124` diff |
| `src/Controls/DxUi.Controls.cpp` | `PreparePendingPageLayout` in full; Button, Slider `OnMouseUp`/`OnMouseMove`; ScrollPanel capture helpers |
| `src/Controls/DxUi.WindowHost.cpp` | `Invalidate`, `RequestAnimation`, `RenderInitialFrameForShow`, `CaptureMouse`, `ReleaseMouseCapture`, `WM_PAINT`, button down/up dispatch, `HandleMessage` catch, `Render` preparation gate, `ResizeBuffers` failure, `CancelStaleCapture`, `PruneStaleInteractionState`, `ResolveControlInteractionState`, `UpdateHover` |
| `src/Controls/DxUi.RichTextEditor.cpp` | `PrepareContent` return paths and catch blocks, `Paint` head, `RequireCurrentLayout`, file-paste staging |
| `src/Controls/DxUi.Tree.cpp` | 13 cited delegate sites with enclosing functions; `OnMouseDown`, `OnMouseDoubleClick`, `OnMouseUp` |
| `src/Controls/DxUi.Grid.cpp` | `SetModel` (`b5869cbc` diff), `OnKeyDown` head and keyboard group toggle |
| `src/Controls/DxUi.EditorControls.cpp` | NumericStepper text callback, `SetValue`, `CommitEdit`, `CancelEdit` |
| `src/Controls/DxUi.NativeTextInput.cpp` | masked IME path, `CancelNativeTextInputImeComposition`, `ClearNativeTextInputCompositionState`; grep for IME-disable APIs |
| `src/Controls/DxUi.RichTextCommands.cpp`, `DxUi.MarkdownInlines.h`, `DxUi.Menu.cpp`, `DxUi.Accessibility.cpp` | cited lines only |
| `include/DxUi/Embedded.h`, `Configuration.h`, `ControlCatalog.h` | callback contract; macro; PR-range diff (empty) |
| Tests | every test named above read at the cited lines; `Tests/` searched per PRIOR-08 item |
| Plans and receipts | six Done plans (box counts, cited lines), `historical-per-id.md.txt`, delivery README, other area files for overlap |
| Git | refs, ancestry and containment for the compat ports; `93c77750..6df17007` diffs of `ControlCatalog.h`, `b5869cbc`, `d04ab124` |
