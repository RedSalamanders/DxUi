# Delivery review 2026-10-31

Status: NOT STARTED. The sequence below is ready to implement. No library, test, or tooling code has changed for it.
Date: 2026-10-31.
Source: `include/` and `src/` are byte-identical among this worktree `d98d7605` (`ej/code-review-20261030`), the delivery tip `f2940cc265db79458b6615492779b839c5c1b94a` (31 Oct 2026, merge of #152), and current `origin/main` `07ff5741cc0cbcd10f7aa5e9e120d0c4b0c16d92` (merge of #154). The commits after the delivery tip are CI evidence roots and a changelog fold. Line numbers below are that shared source.
Owning contracts: [input and accessibility](../../UI/UI_InputAndAccessibility.md), [controls and layout](../../UI/UI_ControlsAndLayout.md), [toolchain and consumption](../../Build/Build_ToolchainAndConsumption.md), and [embedded hosting](../../Rendering/Rendering_EmbeddedD3D11.md).
Related records, which this plan does not re-implement: menu timer behavior already required by the input contract; [R28-05](CodeReview_2026-10-28.md) (the same native IME capacity hole); audit [editor-12](RichTextAudit_2026-10-21/findings.md) and checklist [W2.10](RichTextAudit_2026-10-21.md) (the same raw-Enter ending). This plan owns the library repairs. Consumer WIPs own product bugs and re-pins.

`apiRevision` stays 4. Additions and these repairs do not increment it. Do not publish an API revision 5 for this work.

## Consumer locks

A fix that exists only on DxUi `main` does not ship. Each lock is `Dependencies/DxUi.lock.json` in that product. The keyboard-cancel commit `762983a021c448e4c7334d347ba46f79e20ac817` (2026-10-28, "Prioritize keyboard navigation over pending menu hover timers") is on `origin/main` only. It is not an ancestor of any of the four locks, and none of those trees contain the "pending ancestor timer" cancel in `RouteMenuKeyboardEvent`.

| Product | Lock | API | Where it sits | What a later pin can take |
| --- | --- | --- | --- | --- |
| RedPrism | `a059e07c39c58cbf5218c34f7b9f1a9ac29baf3b` | 3 | Not an ancestor of DxUi `main`. Port target: `origin/ej/wall-compat-redprism-20261028`. | Cannot take `f2940cc2` until the product moves to revision 4. The menu fix is ported onto that branch. RedPrism's own WIP tracks the re-pin. |
| RedXe | `616104f3b2b693a824a3a76b18b851055accf777` | 4 | Not an ancestor of DxUi `main` (diverged). Port target: `origin/ej/wall-compat-redxe-20261028`. | Same port. RedXe's WIP tracks the re-pin of that branch, not of `main`. |
| RedSalamander | `4cdf1ce52c7785446698e346b8aec058ed6a9283` | 4 | Ancestor of DxUi `main`. | A later pin bump can take a `main` commit. The wall-compat tip `1f385ee0` is not this lock and is not an ancestor of `main`. |
| RedTilde | `6ed0512c68743c57ac209e22010679d211c4a42b` | 4 | Ancestor of DxUi `main`. | Same as RedSalamander. RedTilde uses the native rich editor, file paste, separators, and a multiline file-count label. Its WIP owns the product bugs; this plan owns the library repairs those pins need. |

RedPrism and RedXe have no `src/Controls/DxUi.RichTextEditor.cpp` at their locks. RedSalamander and RedTilde do. On all four locks, arrow keys and UIA focus both lose a pending hover timer, because the keyboard cancel is absent there. On `f2940cc2` the keyboard cancel is present and the UIA hole is still open. Pinning that tip is not a complete menu fix.

## Sequence

1. On `main`, land DX-2's UIA half, source-view Enter, DX-4, and DX-3.
2. In that same effort, port both the keyboard cancel (`762983a0`) and the UIA cancel onto the two wall-compat branches. Those branches do not merge `main`.
3. Then land DX-1 and the low items on `main`.
4. RedTilde and RedSalamander pin bumps happen after that `main` commit. DX-1 does not block the bump: no current consumer hits multi-select plus reorder plus single-click invoke. Land it on `main` before the bump anyway, so the combination is not left behind the pin.
5. RedPrism and RedXe re-pin their wall-compat branches rather than `main`. RedPrism's lock stays on API revision 3 until that product's own revision move.

The implementation change records its own `Changes/` fragment. This plan-only document does not. `CHANGELOG.md` stays untouched until the fold of that fragment.

## Wave 1, on main and in the locks that ship

These are already inside trees that ship, or they are missing from the wall-compat ports. The next pins and those ports need them correct.

### DX-2 Menu hover timers and UIA focus

- [ ] Severity: Medium. Effort: urgent.
- **Current behavior.** `RouteMenuKeyboardEvent` cancels every popup hover timer before it changes selection ([DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp) 5454-5457). `CancelSubmenuHoverTimer` (4298-4311) kills the timer and clears `hoverTimerId`, `hoverTimerKind`, and `hoverTimerItemIndex`, which makes an already queued `WM_TIMER` a no-op. `MenuAccessibilityItem::OnFocusChanged` (3512-3527) writes `keyboardIndex`, clears `hoveredIndex`, scrolls the row, and posts `MenuPopupAccessibleFocus`. It does not cancel. `HandleSubmenuHoverTimer` (4446-4469) still opens the stored hover row or closes the chain, and it does not look at `keyboardIndex`. The timer id is `kSubmenuHoverTimerId` (1) at 3990, and `WM_TIMER` reaches the handler at 5826-5830. `MenuPopup::controller` (1144) is the chain the UIA path can walk.
- **Contract.** [UI_InputAndAccessibility.md](../../UI/UI_InputAndAccessibility.md) 386-388: explicit UIA focus of a focusable row follows the session rule for root menus and submenus. Lines 430-432: keyboard navigation cancels pending pointer cascade timers throughout the chain, and a previously queued timer must not replace or close a keyboard-focused child. The code matches that sentence for keys and misses it for UIA focus. The contract already says this; the repair makes the code match. No new requirement.
- **Repair on main.** When UIA focus sets `keyboardIndex`, cancel every popup in the chain the same way `RouteMenuKeyboardEvent` does. Do it in `OnFocusChanged`, synchronously: the posted focus message runs later, so a `WM_TIMER` already queued can open or close first. Also make `HandleSubmenuHoverTimer` refuse an open or a close that would replace or close a popup that already has `keyboardIndex`. Clearing the timer and refusing the stale fire are both required. `f2940cc2` still has the UIA hole; consumers do not bump to that tip as a complete fix.
- **Repair on the wall-compat branches.** Port both the keyboard cancel and the UIA cancel. RedPrism `a059e07c` and RedXe `616104f3` have neither.
- **Acceptance.** Hover until `hoverTimerPendingOpen`, focus another row by keyboard and, separately, by UIA `SetFocus`, then `SendMessageW(hwnd, WM_TIMER, 1, 0)`. One popup remains, and `keyboardIndex` is the focused row. Mirror that for a pending close: the child stays open and keeps `keyboardIndex`. Existing menu tests cancel through `WM_KEYDOWN` and then deliver `WM_TIMER` ([DxUi.Tests.Menu.cpp](../../../Tests/Controls/DxUi.Tests.Menu.cpp) 5764-5780). The UIA focus case (9998-10001) does not deliver `WM_TIMER` after focus, so those tests stay green today. Add the UIA-then-timer case, and the pending-close mirror, beside them.

### Source-view Enter

- [ ] Severity: Medium for RedTilde. Effort: with wave 1.
- **Current behavior.** Rich view `InsertParagraph` copies the line ending: `Eol` in [DxUi.RichTextCommands.cpp](../../../src/Controls/DxUi.RichTextCommands.cpp) 138-145 returns that line's ending, else the first other line's ending, else `L"\r\n"`. Source view, Shift+Enter, and a refused paragraph command assign `L"\r\n"` in [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) 2329-2349. An LF document gains CRLF. The paste path already selects the ending at 381-387: the first `\r` or `\n` in the source, two units when that `\r` starts a CRLF, otherwise `L"\r\n"`.
- **Where it ships.** Present on the RedTilde and RedSalamander pins. Absent on RedPrism and RedXe (no `RichTextEditor.cpp`). RedTilde's product plan tracks the user-facing contract. The code change is here.
- **Repair.** Use that same ending selection for the raw Enter insertion (source view, Shift+Enter, and `ExecuteMarkdownCommand` returning `Unavailable`). One helper shared with paste is enough. This closes the raw-insertion half of editor-12. It does not start the audit's wider EOL-scanner work; W2.10 stays the audit's item and is not a second implementation of this fix.
- **Contract edit in the same change.** [UI_InputAndAccessibility.md](../../UI/UI_InputAndAccessibility.md) 1156-1157 currently requires raw CRLF when no structural plan is available, and the same raw replacement for Shift+Enter. Change that sentence to the document's first ending, with CRLF only when the document has none. [docs/rich-text.md](../../../docs/rich-text.md) 256 says "Enter as CRLF" and follows the same edit.
- **Acceptance.** Enter in an LF Source document inserts one LF. A CRLF document still inserts CRLF. A document with no ending still inserts CRLF. Shift+Enter and a refused paragraph command use that same ending.

### DX-4 Native IME past 64Ki

- [ ] Severity: Medium. Effort: with wave 1. Same defect as R28-05; this item is the repair.
- **Current behavior.** `RichText::Document::MaximumUnits` is `64 * 1024` ([RichDocument.h](../../../include/DxUi/RichDocument.h) 94). Over-capacity text throws `std::length_error` ([DxUi.RichDocument.cpp](../../../src/Controls/DxUi.RichDocument.cpp) 23-24 and 710-711). `ClientTextStoreTarget::GetMaximumDocumentTextUnits` returns `kMaximumTextUnits` (65536) ([TextInputServices.cpp](../../../src/Controls/TextInputServices.cpp) 17 and 75-77). `NativeTextStoreTarget` ([DxUi.TextStoreACP.cpp](../../../src/Controls/DxUi.TextStoreACP.cpp) 201) does not override the base, so the ceiling stays `size_t` max ([TextStoreTarget.h](../../../src/Controls/TextStoreTarget.h) 36-38). `QueryInsert` (1347-1366) and `InsertTextAtSelection` (1709-1710) call `ReplacementFitsCapacity` (1764-1770), which reads that ceiling, so the edit is allowed. `FinishComposition` (888-920) restores the pre-composition base and then `ApplyControlState` (834-837) calls `ImportTextInputState`. That function catches `const std::logic_error&` (which includes `length_error`) and returns false ([DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) 1843-1851). The preview is gone, and the text service was told the insert fit.
- **Where it ships.** RedTilde uses this editor. RedPrism and RedXe have no `RichTextEditor.cpp`.
- **Repair.** Override `GetMaximumDocumentTextUnits` on `NativeTextStoreTarget` with the focused control's ceiling. `RichTextEditor`'s ceiling is `RichText::Document::MaximumUnits`. Controls without a tighter ceiling keep the current `size_t` maximum. `QueryInsert` and `InsertTextAtSelection` already fail with `HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW)` when the override says the replacement does not fit, so the oversized final composition never becomes the import that rolls the preview back. The base virtual on `Control` is not required if the native target can read the live control's ceiling; a small query on `Control`, defaulting to the current maximum and overridden by `RichTextEditor`, is the direct way to do that. The client target's 65536 ceiling stays as it is.
- **Acceptance.** Native store (not the client adapter). Document length 65530. A final composition whose text length is 65540. `QueryInsert` fails. The last successful preview remains. A composition that stays at or under 65536 still commits.

### DX-3 Quoted file-paste fallback

- [ ] Severity: Medium. Effort: with wave 1.
- **Current behavior.** [TryPrepareClipboardPaste](../../../src/Controls/DxUi.RichTextEditor.cpp) 411-448: if any `CF_HDROP` path fails `BuildMarkdownFileReference`, the fallback wraps every path in raw quotes (427-447). `ValidLocalPathText` rejects quotes and newlines ([DxUi.RichImagePaths.h](../../../src/Controls/DxUi.RichImagePaths.h) 41-47), so those paths take the fallback, and the quotes are not escaped. A path containing image markup becomes live Markdown. Paste does not activate or open a file. `BuildMarkdownFileReference` (443-444) only builds source; the load is later and caller-serviced. Native paste calls this function at 2463. Embedded paste calls it at [Embedded.cpp](../../../src/Rendering/Embedded.cpp) 1188. Percent-decoding already rejects an encoded slash, backslash, or colon (`DecodeLocalImageUriPath`, 270-271). There is no second traversal project.
- **Repair.** Inside `TryPrepareClipboardPaste`, so native and embedded paste share it: reject a path that fails `ValidLocalPathText`, or escape quotes and line breaks in the quoted fallback. Either repair satisfies the test. Keep the quoted fallback for a raw or code context whose paths are otherwise representable. A path that fails `ValidLocalPathText` must not be appended as an unescaped quoted string.
- **Acceptance.** One `CF_HDROP` path containing a quote, a newline, and image markup does not produce an image node and does not ask `ResolveLocalImagePath` for the injected destination. A normal `C:\docs\a.png` paste still becomes one image or link. Extend the existing file-reference paste checks in [Support.Tests.RichImagePaths.h](../../../Tests/Support/Support.Tests.RichImagePaths.h) rather than a new suite.

## Before the next ancestor pin

Lower urgency. Land these on `main` as well, before RedTilde and RedSalamander move their pins. They are not a reason to delay the wall-compat menu port.

### DX-1 Tree release invoke after a collapsing refresh

- [ ] Severity: Medium. Not reached by current call sites. Effort: after wave 1. Does not block the pin bump. Land it on `main` before the bump so it is not forgotten.
- **Current behavior.** [OnMouseUp](../../../src/Controls/DxUi.Tree.cpp) 1899-1947: multi-select plus reorder plus single-click invoke. A release that is not a drag collapses the selection (`CollapseSelectionToItem`, 3046-3072). The selection callback may `NotifyDataChanged` and bump the revision. The mouse-up guard's `IsCurrent()` at 1934 and 1939 then skips `OnTreeItemInvoked`. `ResumeAfterCallback` exists. The inner selection guard uses it (3064, 3072, and the selection-change path at 736). The outer mouse-up guard does not: the check at 1919 runs before the collapse, and nothing resumes that guard after a successful collapse.
- **Contract.** [UI_ControlsAndLayout.md](../../UI/UI_ControlsAndLayout.md) 752-756: a callback that refreshes the model does not end the gesture. The same holds for Tree. Rows are re-resolved by stable id.
- **Repair.** After a successful collapse, `ResumeAfterCallback` on the mouse-up guard, then keep the release hit-test and the id compare (1938-1944). A callback that removes the row still fails that compare and does not invoke.
- **Acceptance.** Those three flags. The selection callback calls `NotifyDataChanged` and leaves the row. Release invokes once. A callback that removes the row does not invoke.
- **Consumers.** RedTilde calls `SetInvokeOnSingleClick` and does not enable reorder. No consumer hits the combination. Note only: RedXe's lock `616104f3` uses `IsCurrent()` inside `CollapseSelectionToItem` rather than `ResumeAfterCallback`. Mention that if that product enables reorder later. It is not part of this repair.

### DX-8 Unfocused Tree selection text

- [ ] Severity: Low. Effort: with the pre-pin items.
- **Current behavior.** An unfocused Tree passes `theme.selectionText` into `ResolveInactiveSelectionTextColor` ([DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp) 80-83). The resolver returns `theme.selectionText` immediately in high contrast, and otherwise returns the preferred color when it reaches 4.5:1 ([DxUi.Theme.cpp](../../../src/Controls/DxUi.Theme.cpp) 252-265). Grid correctly passes `theme.selectionText` ([DxUi.Grid.cpp](../../../src/Controls/DxUi.Grid.cpp) 231 and 282). Leave Grid alone.
- **Contract.** [UI_ControlsAndLayout.md](../../UI/UI_ControlsAndLayout.md) 708-712: inactive Tree text prefers `text`, and Grid text prefers `selectionText`, when the ink reaches 4.5:1. High contrast retains the HighlightText pair.
- **Repair.** Pass `theme.text`. High contrast still returns `theme.selectionText` because the resolver does that before it looks at the preferred color.
- **Tests.** `TestTreeInactiveSelectionPrefersSelectionTextWhenReadable` ([DxUi.Tests.Tree.cpp](../../../Tests/Controls/DxUi.Tests.Tree.cpp) 1415-1441) locks `theme.selectionText` at 1431. Update it to expect `theme.text` when that ink is readable, and keep the unreadable-contrast and high-contrast assertions. `TestTreeMultiSelectPaintsEverySelectedRowWithTheSelectionColors` (2923-2957) still says "normal text" in the comment at 2923, while 2930 and 2940 pass `theme.selectionText` into the resolver. Point those comparisons at `theme.text`. The high-contrast checks at 2957 already expect `palette.selectionText` and stay as they are.
- **Pins.** RedXe's lock already passes `theme.text` (its Tree.cpp line 81). RedPrism assigns `theme.text` directly for the unfocused selected row (its Tree.cpp inactive branch). This repair makes current `main` match that preference through the shared resolver.

### DX-9 Wrapped label with a typography override

- [ ] Severity: Low. Effort: with the pre-pin items. RedTilde's file-count label is multiline with an override.
- **Current behavior.** `MeasureText` uses `metrics.width` when wrapping and `widthIncludingTrailingWhitespace` for one line, and it sets `baselineDip` ([DxUi.Layout.cpp](../../../src/Controls/DxUi.Layout.cpp) 101-105). `Label::MeasureOverride` with a typography override always uses `widthIncludingTrailingWhitespace` (240-285, the assignment at 281) and leaves `baselineDip` unset. A wrapped multiline label can report overflow.
- **Repair.** When the label is multiline and a maximum width is set, use `metrics.width`, matching `MeasureText`. Set `baselineDip` from the first line the same way `MeasureText` does, so the override path and the plain path agree for the same string.
- **Acceptance.** The same wrapping string, with and without an override, at the same maximum width. Overflow is false when the ink fits.

### DX-10 OnChar swallows bad_alloc after publish

- [ ] Severity: Low. Effort: with the pre-pin items.
- **Current behavior.** `ReplaceSelection` (910-916) replaces the document and then `Changed`, which publishes. `OnChar` is `noexcept` on the override ([DxUi.h](../../../include/DxUi/DxUi.h) 3298; definition 2302) and catches `const std::exception&`, including `bad_alloc`, and returns false (2373-2376). That return happens after a published replacement. The base virtual is not `noexcept` (1645). `OnKeyDown` terminates on `bad_alloc` first (2539-2542) and only then catches `std::exception`. `RouteFocusedCharInput` starts `controlHandled` at true and treats a throw as taken ([DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) 2187-2189). `TryControlCallback` catches `std::exception` and leaves that flag true when the assignment never completes ([DxUi.Internal.h](../../../src/Controls/DxUi.Internal.h) 176-186). A `noexcept` function that lets `bad_alloc` escape terminates inside `OnChar`, so the host never sees it.
- **Repair.** Let `bad_alloc` leave `OnChar` so the native host keeps the character taken. That means the override cannot stay `noexcept`: an escaping `bad_alloc` has to reach `TryControlCallback`. Keep a `std::exception` catch for length and argument failures that happen before `ReplaceSelection` publishes, and return false only then. Once the replacement has published, do not turn a later exception into an unhandled character. The embedded bare `DispatchCharacter` ([Embedded.cpp](../../../src/Rendering/Embedded.cpp) 1002-1017) catches `std::exception` and returns false. That catch must not turn this `bad_alloc` into an unhandled character; the character stays taken, matching the native route. Leave the receipt overload alone (DX-7, below). `OnKeyDown` keeps its terminate-on-`bad_alloc` behavior.

### DX-5 Separator construction and bad_alloc

- [ ] Severity: Low. Effort: with the pre-pin items. This is an out-of-memory path. RedTilde constructs separators.
- **Current behavior.** `Separator()` is `noexcept` ([DxUi.h](../../../include/DxUi/DxUi.h) 2072; [DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) 2637). `Control` default-initializes `_lifetimeToken` with `std::make_shared<int>(0)` (1798) before the `Separator` body runs. A throw from that `noexcept` constructor terminates. `CreateControl` already catches `bad_alloc` and returns `E_OUTOFMEMORY` ([ControlCatalog.cpp](../../../src/Controls/ControlCatalog.cpp) 45-92), and its comment says failure leaves the caller's result untouched ([ControlCatalog.h](../../../include/DxUi/ControlCatalog.h) 49). The handler never runs for `Separator`.
- **Repair.** Drop `noexcept` from the declaration and the definition.
- **Acceptance.** Fault that lifetime-token allocation inside `CreateControl(ControlKind::Separator, result)` and require `E_OUTOFMEMORY` with `result` unchanged. Use the smallest test-only failure point beside the existing `DebugFailNext*` hooks. A process-wide allocator that fails unrelated allocations is the wrong seam. The existing catalog check in [DxUi.Tests.EditorControls.cpp](../../../Tests/Controls/DxUi.Tests.EditorControls.cpp) 1910 still creates a separator on the success path.

### DX-6 TagPicker::ClearChildren leaves null slots

- [ ] Severity: Low. Effort: with the pre-pin items. No consumer constructs a `TagPicker`.
- **Current behavior.** `Panel::TakeChild` moves the child out and leaves a null `unique_ptr` slot ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) 1328-1333). `TagPicker::ClearChildren` (7201-7209) takes every child except the combo and does not compact. `FormField::ClearChildren` erases those holes ([DxUi.Forms.cpp](../../../src/Controls/DxUi.Forms.cpp) 618).
- **Repair.** After the removal loop, erase null slots the same way `FormField` does.
- **Acceptance.** A derived picker adds a button, calls `ClearChildren`, and `GetChildren` contains only the combo. Put the case next to the existing TagPicker tests in [DxUi.Tests.Controls.cpp](../../../Tests/Controls/DxUi.Tests.Controls.cpp).

### DX-11 ControlKind ordinals

- [ ] Severity: Low. Optional. This is not a revision bump. Skip it unless the static assertions below are added in the same change.
- **Current behavior.** [ControlCatalog.h](../../../include/DxUi/ControlCatalog.h) 6-42 gives `ControlKind` implicit values. `e4f1790d` inserted `Separator` after `CardPanel` and appended `RichTextEditor`. Before that commit the enumerators were `Panel` = 0 through `FormPanel` = 32, with `Label` = 3. After it, `Separator` = 3, `Label` = 4, `FormPanel` = 33, and `RichTextEditor` = 34. [Build_ToolchainAndConsumption.md](../../Build/Build_ToolchainAndConsumption.md) 218-235: an equal `apiRevision` does not allow mixing old and new objects, and additions do not increment the revision. No consumer stores the raw `uint8_t` or calls `CreateControl` with a numeric `ControlKind`. In-tree `CreateControl` switches on the enumerator names. Named rebuilds still construct the same controls.
- **Repair, only together with the assertions.** Give the pre-`e4f1790d` enumerators their old values: `Panel` 0, `PageHost` 1, `CardPanel` 2, `Label` 3, `Button` 4, `Toggle` 5, `Checkbox` 6, `RadioButton` 7, `RadioButtons` 8, `ProgressBar` 9, `ThroughputGraph` 10, `Slider` 11, `Toolbar` 12, `MenuBar` 13, `TabControl` 14, `ColorSwatch` 15, `TextField` 16, `ComboBox` 17, `TagPicker` 18, `StatusStrip` 19, `PopupLayer` 20, `StackPanel` 21, `ScrollPanel` 22, `TooltipLayer` 23, `Tree` 24, `Grid` 25, `PageIndicator` 26, `Splitter` 27, `NumericStepper` 28, `ColorPicker` 29, `FlexPanel` 30, `FormField` 31, `FormPanel` 32. Place `Separator` = 33 and `RichTextEditor` = 34 after `FormPanel`. `RichTextEditor` keeps the value it has had since it was added. Add `static_assert` of those historical ordinals, including `Label` == 3 and `FormPanel` == 32. `apiRevision` stays 4. The catalog size stays 35.

## Not scheduled

### DX-7 Character receipt

Dropped as a ship item. The receipt overload of `DispatchCharacter` resets the receipt ([Embedded.cpp](../../../src/Rendering/Embedded.cpp) 1057) and then returns false when `OnChar` returns false (1084) or throws a non-allocation exception (1090-1094). The returns after the reset and before `OnChar` already return true. All four products call the bare two-argument overload. RedPrism does so from `ChromeHost::DispatchCharacter` into `_view.DispatchCharacter(character, modifiers)`. RedXe does so from `AVControlView`. The current RedTilde and RedSalamander trees have no `DispatchCharacter` call and do not pass a receipt. Record the repair, and do it before any transport passes receipts: after the reset, return true when `OnChar` returns false or throws, because the receipt is already consumed. Do not schedule that now.

## Explicitly not a finding

- Clean Composite allocation. Composition does not allocate.
- Posted-payload type identity.
- File paste opening a file inside `TryPrepareClipboardPaste`. That function only builds source.
- An API revision 5. This work does not create one.

## Validation when implementation starts

Extend the existing menu, rich-text, tree, layout, catalog, and TagPicker tests named above. A new native test file uses `<Component>.Tests.Something.h/.cpp` and `Tests/native-test-files.json`. Run the affected `test.ps1` configurations for the files actually changed. The wall-compat ports are verified on those branches, not by a consumer pin of `f2940cc2`. Move this plan to Done only after the chosen items are implemented, their tests pass, and the Enter contract sentences are updated. DX-11 may stay unchecked if the ordinal assertions are not added. DX-7 stays in the note above.
