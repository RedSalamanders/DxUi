# Production review: reviewer findings and verifier verdicts

Appendix to [the review plan](../ProductionReview_2026-10-05.md). Generated from the review workflow of 2026-10-05 over
`9f5bc07..49a9988` (#21-#65), revalidated against `e5ebbb5` on 2026-10-06, when #66 and #67 were reviewed too (last section,
`scoped-*`). Each entry is a reviewer's finding as reported, followed by the independent verifiers' verdicts (high and
critical: a *trace* and a *refute* verifier; medium: one verifier; low and simplifications: one batch verifier per area).
Severity is the lowest severity a non-refuting verifier gave. Paths and lines are at `e5ebbb5`; **At e5ebbb5** says whether
the finding still holds there and, where #66 renamed a file or moved a line, what changed. The prose quotes the code as
reviewed, so it may name a test file by its old name. The plan, not this appendix, is the curated and deduplicated list;
several entries describe the same defect.

## Critical (1)

### public-api-architecture-1

**ControlHost::SetRoot destroys the old root before Reparent reads the moved child's old parent (use-after-free)**  
`src/Controls/DxUi.WindowHost.cpp:1367` · #31, #29 · lifetime · confirmed (trace: confirmed/critical; refute: confirmed/critical)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #31 replaced `_root-&gt;SetParent(nullptr); _root-&gt;PropagateHost(this);` with `_root-&gt;Reparent(nullptr, this)`. Reparent first reads `GetFlowDirection()` and `GetDensity()`, and both walk `_parent`. At this point `_parent` still points at the panel the child was moved out of. SetRoot runs `_root = std::move(root);` before calling Reparent. That move-assignment destroys the previous root, together with every panel under it. The header (DxUi.h:1671) now tells consumers to move a child out through `Panel::GetChildren()` and pass it to `ControlHost::SetRoot`. If that child came from the host's own current root tree (for example promoting a content panel to root), its old parent is freed before Reparent dereferences it. PageHost::SetPage d …
- **Failure**: The host's root is Panel A with child B. The consumer calls `auto b = std::move(A-&gt;GetChildren()[0]); host.SetRoot(std::move(b));`. SetRoot calls PublishEmpty, ResetRootInteractionState and `_root-&gt;PropagateHost(nullptr)`, then `_root = std::move(root)` deletes A. `_root-&gt;Reparent(nullptr, this)` then calls `GetFlowDirection()`, which runs `_parent-&gt;GetFlowDirection()` on the deleted A: heap use-after-free (an ASan report or a crash, or a wrong flow direction and density). Only a child with both an explicit flow direction and an explicit density avoids it. The tests (EditorControls …
- **Fix**: In ControlHost::SetRoot, keep the old root alive until the new root has been reparented, and release it at the end: ``` std::unique_ptr&lt;Control&gt; oldRoot = std::move(_root); if (oldRoot) oldRoot-&gt;PropagateHost(nullptr); _root = std::move(root); _defaultButton = nullptr; _cancelButton = nullptr; if (_root) { _root-&gt;Reparent(nullptr, this); _root-&gt;SetBounds(...); } oldRoot.reset();   // after Reparent; before RefreshWindowHostAccessibilitySnapshot RefreshWindowHostAccessibilitySnapsh …
- **Test**: Add a Controls test (EditorControls, next to "Moving controls") that promotes a descendant of the host's own current root: 1. Create a WindowHost. Build root Panel A with an inherited flow direction and no explicit density, holding child B, an AnnouncementProbe or a Panel with a grandchild. 2. Call …

## High (20)

### controls-editor-theme-2

**Window host never republishes or announces Slider/Splitter/ProgressBar value changes: Narrator stays silent and RangeValue.Value goes stale**  
`src/Controls/DxUi.Accessibility.cpp:8701` · #29, #53, #25 · accessibility · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: In a window host, UIA reads values from the last published snapshot. Two things are missing. (1) Nothing republishes that snapshot after a keyboard step, a UIA RangeValue.SetValue or a programmatic SetValue on Slider, Splitter or ProgressBar. (2) Even when something else republishes, the window-host publish path raises only structure, focus and selection events (selection events were added in #53). The value-change event is raised only in RaiseEmbeddedAccessibilityChanges, which the embedded host calls. #29 added Splitter's Thumb/RangeValue element on the stated promise that screen readers 'speak its position and can move it', and #25/#29 guarded ProgressBar against NaN 'reaching UI Automation'. Yet in a window host neither value change eve …
- **Failure**: In a WindowHost, Narrator focuses a Slider (value 42) and the user presses Right Arrow: the value becomes 43 and the thumb moves, but no snapshot is published and no UIA_RangeValueValuePropertyId event is raised, so Narrator says nothing; reading the value again returns 42 until some unrelated focus or mouse-up republishes. Other cases: a Narrator or Voice Access RangeValue.SetValue(68) on the slider or splitter returns S_OK while get_Value still reports the old value; a ProgressBar updated by the application never announces progress.
- **Fix**: 1. Republish after state-changing input in the window host. In ControlHost's WM_KEYDOWN/WM_SYSKEYDOWN (and WM_KEYUP) branch, when `controlHandled && liveKeyTarget`, call RefreshWindowHostAccessibilitySnapshot(_hwnd, this). This mirrors WM_LBUTTONUP and also covers other key-driven state such as NumericStepper and Splitter arrows. In ExecuteSetRangeValueOnWindowThread, after a successful RequestValue or RequestPosition, recheck lifetime, then call RefreshWindowHostAccessibilitySnapshot(_hwnd, hos …
- **Test**: Add a WindowHost accessibility test with a focused Slider (min 10, max 90, value 42, step 2) and get its IRangeValueProvider. (a) Send WM_KEYDOWN VK_RIGHT to the host window. Assert rangeValuePattern-&gt;get_Value returns 44.0. Today it returns 42.0. (b) Call rangeValuePattern-&gt;SetValue(68.0). As …

### cross-cutting-simplification-1

**ControlHost::SetRoot frees the old root before Reparent reads the new root's old parent (use-after-free when a child of the current root becomes the root)**  
`src/Controls/DxUi.WindowHost.cpp:1367` · #31 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #31 replaced `_root-&gt;SetParent(nullptr); _root-&gt;PropagateHost(this);` with `_root-&gt;Reparent(nullptr, this)`. Reparent's first step reads the control's inherited flow direction and density through its current `_parent` (by design: "The old parent is read to tell what differs, so it must still exist"). The same PR documented moving a child out of `Panel::GetChildren()` and handing it to `ControlHost::SetRoot`. Moving a child out of the span does not clear its `_parent`. SetRoot then destroys the old root with `_root = std::move(root)` before it calls `Reparent`. If the new root was the old root's child, or any descendant of it, Reparent dereferences freed memory. PageHost::SetPage reparents before it releases anything, so only SetRoo …
- **Failure**: An app unwraps its root: `auto root = std::make_unique&lt;Panel&gt;(); Panel* p = root.get(); p-&gt;AddChild&lt;Grid&gt;(); host.SetRoot(std::move(root)); ... host.SetRoot(std::move(p-&gt;GetChildren()[0]));`. In SetRoot, `_root = std::move(root)` destroys `p`, then `_root-&gt;Reparent(nullptr, this)` calls `GetFlowDirection()`, which runs `_parent-&gt;GetFlowDirection()` on the freed panel, and `GetDensity()`, which runs `_parent-&gt;GetDensity()`. ASan reports a heap-use-after-free. A release build reads garbage, either announcing a wrong flow direction or density or crashing on the parent c …
- **Fix**: In SetRoot, reparent the new root before the old one is released: ``` std::unique_ptr&lt;Control&gt; previous = std::exchange(_root, std::move(root)); _defaultButton = nullptr; _cancelButton = nullptr; if (previous) previous-&gt;PropagateHost(nullptr);   // the moved child's slot is null, so this cannot reach it if (_root) { _root-&gt;Reparent(nullptr, this); _root-&gt;SetBounds(...); } previous.reset(); ``` `previous-&gt;PropagateHost(nullptr)` and the new root's `Reparent` can run in either or …
- **Test**: Add a control test that runs under ASan Debug: 1. Create a ControlHost and a root Panel. Add a child whose inherited flow direction and density both come from the panel, then set the panel's explicit flow direction to RightToLeft and its density to Compact. 2. Call `host.SetRoot(std::move(panel))`. …

### gap-embedded-host-parity-1

**Embedded host never announces focus moves between tree items or grid rows, or onto a newly added or replaced focused control**  
`src/Controls/DxUi.Accessibility.cpp:973` · #53, #32, #35, #26 · accessibility · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The window host decides whether to raise a focus change by comparing the focused element: a control, a tree item, a grid row or none (`SameFocusedElement`). It then raises the event on that element (`AnnounceWindowHostFocus`). In `PublishWindowHostAccessibilitySnapshot` that comparison is skipped for embedded targets. `RaiseEmbeddedAccessibilityChanges` raises `UIA_AutomationFocusChangedEventId` only when a control record's `controlHasFocus` flips, and only on the control's element. It also skips any record whose control identity changed. The result is three gaps. (a) Moving focus inside an already-focused Tree or Grid raises no focus event. A multi-select tree's Ctrl+Up/Down, which moves focus without selecting, is completely silent, and a …
- **Failure**: RedXe hosts a multi-select Tree in an EmbeddedHost and Narrator is running. The user presses Ctrl+Down: Tree::SetFocusedItemId moves `_selectedItemId`, the app prepares and calls UpdateAccessibility, and the record diff finds no changed field, so nothing is announced. In a second case the app rebuilds a settings pane and focuses the new TextField at the same tree path. The record has a new controlIdentity, the loop marks only a structure change, and Narrator is never told that focus moved into the field.
- **Fix**: 1. In PublishWindowHostAccessibilitySnapshot, compute `changes.focusMoved = ! SameFocusedElement(*previous, *snapshot)` for embedded targets too. The embedded branch already clears focusedFragment when `! placement.hasKeyboardFocus`, so losing the app's focus is covered. 2. In RaiseEmbeddedAccessibilityChanges, pass the changes struct (or focusMoved). When focusMoved is set and `current-&gt;focusedFragment` has a value, build the provider for that fragment and raise UIA_AutomationFocusChangedEve …
- **Test**: Add three tests in Tests/Embedded/EmbeddedUiaTests.h, using the EmbeddedSingleControlView + Bridge + EmbeddedClientWalk pattern with `subscription.focus = true`. Each fails at HEAD and passes after the fix. (1) Multi-select Tree with item A focused and the host holding keyboard focus. Send Ctrl+Down …

### gap-embedded-host-parity-3

**Embedded Cancel clears the pending double-click, so consumers that follow 'Cancel on capture loss' never get a double-click**  
`src/Rendering/Embedded.cpp:438` · #29 · bug · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: `DispatchPointer(Cancel)` calls `CancelPointer`, which always calls `ClearPendingPointerDoubleClick()`, even when no control is captured. docs/hosting.md tells consumers to 'Cancel on capture loss', and the reference sample releases Win32 capture on every button-up. Windows sends `WM_CAPTURECHANGED` on that `ReleaseCapture`, the sample forwards it as Cancel, and the press just remembered for double-click detection is erased. The window host's `WM_CAPTURECHANGED` handler does not clear the pending double-click. This is a parity break in the function #29 edited; the clearing itself predates the review window. The embedded double-tap test dispatches Down/Up/Down without the Cancel that real consumers send, so it cannot catch this.
- **Failure**: Run Samples/EmbeddedControls --text-input and double-click a word in the text field, or double-click a Tree row. The sequence is Down, Up, then ReleaseCapture, which sends WM_CAPTURECHANGED and therefore DispatchPointer(Cancel), clearing `_pendingPointerDoubleClick`. The second Down then calls `ShouldTreatPointerDownAsDoubleClick`, which returns false, so the word is not selected and the row is not activated or expanded.
- **Fix**: In EmbeddedHost::CancelPointer, clear the pending double-click only when a press was actually cancelled. Use `if (captured) { if (IsControlInTree(...)) captured-&gt;OnCaptureLost(_host); _host.ClearPendingPointerDoubleClick(); }`, so that an interrupted press cannot later pair into a double-click. A Cancel arriving after a completed Up, when nothing is captured, then keeps the candidate, which matches the window host's WM_CAPTURECHANGED. The InputIsCoherent revision-change path and the detach/re …
- **Test**: Extend the embedded double-tap test in Tests/Embedded/EmbeddedTextInputTests.h to dispatch Down, Up, Cancel, Down, Up on the word "alpha", matching the sample's ReleaseCapture -&gt; WM_CAPTURECHANGED sequence. Assert that the selection is exactly [8,13). This fails at HEAD, where the caret is only p …

### gap-visual-modes-hc-rtl-dpi-1

**Tree focus ring has the selection fill's own color, so with multi-select (#35) a keyboard user cannot see which selected row has focus**  
`src/Controls/DxUi.Tree.cpp:183` · #35 · accessibility · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: DrawTreeRow draws the focus ring 1 DIP wide, inset 1.5 DIP inside rowRect, in visuals.focus = theme.focusStroke. focusStroke is the selection fill, or nearly so. In the default light palette (include/DxUi/DxUi.h:879/886) focusStroke and selectionFill are both (0, 0.47, 0.84), which is 1:1 contrast. In the dark default and in MakeThemePalette (Theme.cpp:1033), focusStroke = BlendColor(selectionFill, selectionText, 0.10 or 0.04), about 1.2:1 against selectionFill. High-contrast palettes get the same blend, so in HC the ring is Highlight drawn on Highlight. Before #35 the ring only ever drew on the selected row (showFocus = selected && ...), where the fill already showed the position. Multi-select makes the ring the only cue telling the curren …
- **Failure**: Light theme, SetMultiSelectEnabled(true), select rows 1-4 with Shift+Down, then press Ctrl+Up twice. Focus is now on row 2, which is selected. Its focus ring is (0,0.47,0.84) on (0,0.47,0.84) fill, so nothing on screen shows the focus moved. Ctrl+Space then toggles a row the user cannot locate. In an HC theme with the tree focused by mouse, the ring that showFocus forces on is also Highlight-on-Highlight.
- **Fix**: In ResolveTreeRowVisuals, pick the ring color from the fill underneath it. When the row is selected (or has the rainbow fill), use a color that contrasts with visuals.fill: for example visuals.text (selectionText, which is already chosen to contrast with the fill), or theme.focusStrokeInner/focusStrokeOuter, whichever has the higher contrast. Keep theme.focusStroke for unselected rows, where it sits on the surface background. A more robust option is to draw the existing two-tone ring inset insid …
- **Test**: In DxUiTests.Tree.cpp, extend the multi-select visuals test (around line 2175). For the light default palette, the dark default palette, and a high-contrast ThemeColors passed through MakeThemePalette (with Highlight differing from the gallery's), enable multi-select, select {1,3,4}, focus the tree …

### gap-visual-modes-hc-rtl-dpi-2

**Text on unfocused selected rows is never contrast-checked: Tree fails in every Windows HC theme, Grid fails in the default light theme**  
`src/Controls/DxUi.Tree.cpp:78` · #35, #43 · accessibility · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The two lists paint unfocused selected rows with different fixed text tokens, and each fails in one mode. Tree uses theme.text on selectionInactiveFill. In high contrast, MakeThemePalette makes selectionInactiveFill an opaque selectionFill (Theme.cpp:1046-1047), so the row becomes WindowText on Highlight. In Windows 11 HC themes that is about 1.4:1 (Aquatic: #FFFFFF on #8EE3F0; Desert: #3D3D3D on #903909). Grid uses selectionText on the same token. In the default light palette, selectionInactiveFill is 45% of (0,0.47,0.84) over a near-white surface, so the row is white on roughly (0.55,0.76,0.93), about 1.9:1. #35 paints every selected Tree row this way whenever the tree loses focus, and #43 targets large Grid multi-selections, so whole blo …
- **Failure**: An HC Aquatic consumer passes WindowText/Highlight/HighlightText as ThemeColors. The user multi-selects tree items and clicks into another control. Every selected label is white on light cyan (about 1.4:1). Separately, a light-theme app with a Grid selection whose focus moves to a toolbar shows white text on light blue (about 1.9:1).
- **Fix**: Add one shared helper in Theme.cpp, ResolveInactiveSelectionText(theme, rowGround): 1. In high contrast, return theme.selectionText, since the inactive fill there is opaque Highlight and HighlightText is its designated pair. 2. Otherwise, composite selectionInactiveFill over rowGround (CompositeOverBackground already exists), then return selectionText if ContrastRatio &gt;= kMinimumNormalTextContrast, else theme.text if that passes, else ChooseContrastingTextColor(composite). Use the helper at T …
- **Test**: 1. Tree test: build a palette with MakeThemePalette from an HC ThemeColors set to the Aquatic values (background 0xFF202020, text 0xFFFFFFFF, selectionBackground 0xFF8EE3F0, selectionText 0xFF263B50, highContrast TRUE). Select two items, call DebugGetRowVisualState(theme, i, true, state) with the tr …

### grid-selection-lifetime-1

**Keyboard Shift+Up / Shift+PgUp / Shift+Home cannot extend a range upward: the grid's current row is the bottom of the range, not the row reached**  
`src/Controls/DxUi.Grid.cpp:3958` · #43, #53 · bug · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Grid has no current (focused) row of its own. OnKeyDown, the keyboard context menu and GetPrimarySelectedRow (which UIA uses for the focused row) all treat GetOrderedSelection().back() as the current row. The spec states this rule: "Grid, whose current row is simply the last selected one". SetRange, however, always stores the slice in visible order, whichever direction the range was made in, and the model tests require this ("a range made backwards keeps the list's order"). So after any upward range, back() is the anchor (the bottom row), not the row the user just reached. This was not introduced in the window, but #43 rewrote SetRange and kept the conflict, and #53 now announces focus from back().
- **Failure**: Rows 0-20. Click row 10, then press Shift+Up: rows 9-10 are selected and back() is 10. Press Shift+Up again: currentRow is 10, so the next row is 9 again and the selection stays 9-10. The range never grows past one row upward. Shift+PgUp from row 50 gives 40-50 and then stays there. After Shift+Home (0-10), Shift+Down gives 10-11 instead of 1-10. After a Shift+click on row 5 with the anchor at row 10, Down moves from row 10, Shift+F10 opens the menu on row 10, and Narrator's focus stays on row 10 (the anchor), not row 5. With Ctrl held, the documented Ctrl+Up toggle of the neighbouring row als …
- **Fix**: Add `std::optional&lt;uint64_t&gt; _currentRowId` to GridSelectionModel. SetSingle(row), Toggle(row) and SetRange(…, current) set it to that row. Clear() clears it. PreserveOrdered moves it to back() (or the anchor) when its row is dropped, the same way the anchor is reconciled. Expose GetCurrent(). Replace the three or four `GetOrderedSelection().back()` uses with GetCurrent(), falling back to back(): Grid::OnKeyDown :3958, OnContextMenu :4181, the preferred row at :4533, and GetPrimarySelected …
- **Test**: Add Grid control tests in Extended mode with 30 rows: (a) Select row 10, send OnKeyDown(VK_UP, MK_SHIFT) twice. Require the selection to be rows 8-10 and GetPrimarySelectedRow()==8. This fails today with 9-10 and 10. (b) Select row 10, press Shift+Home, then Shift+Down. Require rows 1-10. Today it g …

### grid-selection-lifetime-2

**A checkbox or group delegate that calls NotifyDataChanged, followed by the documented rebuild from the selection delegate, frees the grid while it is still in use**  
`src/Controls/DxUi.Grid.cpp:5213` · #60, #57 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The #60 guards cover the selection delegate only where the Grid itself calls it. Two kinds of delegate call still continue to use `this` afterwards with no lifetime check: checkbox toggles and group toggles (the group-header press, the keyboard Left/Right lambda, and the ApplyGroupLayout loop). docs/controls.md:36 tells applications to "Call `NotifyDataChanged` after model changes". docs/controls.md:154-157 also allows rebuilding the controls from the selection delegate after "a model change (`NotifyDataChanged`, ...)". So an application that follows both rules frees the grid from inside OnGridCheckboxToggled or OnGridGroupToggled: the change publishes the selection, the selection delegate rebuilds the controls, and the delegate call return …
- **Failure**: A to-do grid. OnGridCheckboxToggled marks the row done, removes it from the model and calls grid-&gt;NotifyDataChanged(). The toggled row was the selected one, so NotifyDataChanged changes the selection and calls OnGridSelectionChanged, which rebuilds the view (host.SetRoot). Control returns into ToggleCheckboxCell, where GetLifetimeToken() copies `_lifetimeToken` out of the freed Grid (a reference-count write into freed heap), and then NotifyDataChanged() and Invalidate run on freed memory. Collapsing a group that holds the selected row has the same effect, by Left or by a header press, when …
- **Fix**: Apply the same pattern in all four places: 1. Take `const std::weak_ptr&lt;int&gt; lifetime = GetLifetimeToken();` before calling the delegate. 2. Return as soon as `lifetime.expired()` is true after it returns. 3. Take the "previous selection" copy after the delegate returns, so a change the application already reported through its own NotifyDataChanged is not reported again. Per site: - ToggleCheckboxCell (Grid.cpp:5208): move the token above OnGridCheckboxToggled and return true when it has e …
- **Test**: Add three Controls tests and run them under ASan Debug. Each should fail with a heap-use-after-free before the fix and pass after it. 1. **Checkbox toggle.** Build a grid of checkbox rows inside a host and select row 0. - The delegate's OnGridCheckboxToggled removes row 0 from the model and calls `g …

### host-core-1

**SetRoot frees the old root before Reparent reads the new root's parent chain: use-after-free when promoting a descendant of the current root**  
`src/Controls/DxUi.WindowHost.cpp:1367` · #31 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: PR #31 replaced `_root-&gt;SetParent(nullptr); _root-&gt;PropagateHost(this);` with `_root-&gt;Reparent(nullptr, this)`. Reparent now reads the inherited flow direction and density through the control's current `_parent` chain before it resets `_parent` (DxUi.cpp:919-921). SetRoot calls it after `_root = std::move(root)` has already deleted the old root. A control moved out of the current root's tree through `Panel::GetChildren()` keeps `_parent` pointing into that tree, because nothing clears it when a child is moved out. Its `_host` also stays set, since Panel::PropagateHost skips the null slot. So the reads land in freed memory. The public header recommends this exact move ("A child moved out of the span (to give it to ControlHost::SetRo …
- **Failure**: `auto* root = host.SetRoot(std::make_unique&lt;StackPanel&gt;())`-style tree {header, content}; later `host.SetRoot(std::move(static_cast&lt;Panel*&gt;(host.GetRoot())-&gt;GetChildren()[1]))` to drop the header. In SetRoot, `_root = std::move(root)` destroys the StackPanel. `Reparent` then runs `GetFlowDirection()`, which reads `_parent-&gt;_explicitFlowDirection`/`_parent-&gt;_parent`, and `GetDensity()`, which also reads `_parent-&gt;_host`, all on the freed StackPanel. ASan Debug reports a heap-use-after-free. In Release the reads return garbage; with a grandchild (`innerPanel-&gt;GetChildr …
- **Fix**: In ControlHost::SetRoot, keep the previous root alive until the new root has been reparented: ``` std::unique_ptr&lt;Control&gt; previous = std::move(_root); if (previous) previous-&gt;PropagateHost(nullptr); _root = std::move(root); _defaultButton = nullptr; _cancelButton = nullptr; if (_root) { _root-&gt;Reparent(nullptr, this); _root-&gt;SetBounds(...); } previous.reset();   // destroy only after the new root no longer reaches into it RefreshWindowHostAccessibilitySnapshot(_hwnd, this); ... ` …
- **Test**: Add a Controls test to run under ASan Debug, in two variants: a WindowHost host and `EmbeddedHost::Controls()`. - Build a root StackPanel A containing [Button header, StackPanel B containing [TextBox t]]. Give A an explicit RightToLeft flow direction and leave t without an explicit value. - Child ca …

### host-core-3

**Host pointer-down path takes native focus before re-validating the clicked control: use-after-free when a focus callback rebuilds the tree (gap in the #57 fix)**  
`src/Controls/DxUi.WindowHost.cpp:2733` · #57, #29 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #57 made controls that focus themselves stop touching themselves once focus callbacks destroy them (FocusControlAndSurvive). The host's own WM_LBUTTONDOWN/WM_RBUTTONDOWN path has the same pattern and was not fixed. For a focusable control that handled the press without focusing itself, it calls SetFocus(hwnd) and then SetFocusControl(liveControl), CaptureMouse(liveControl) and RememberPointerButtonDown(liveControl), and it never re-validates liveControl after SetFocus. When the host window lacks native focus (a child-window host, or a top-level window whose focus is in a native child), SetFocus sends WM_SETFOCUS synchronously. OnSetFocus then restores the retained control, or, when there is none, runs SetFocusControl(first focusable), which …
- **Failure**: A consumer control (Control subclass, SetFocusable(true), OnMouseDown returns true; consumers cannot call the internal FocusControlAndSurvive) sits in a ControlHost attached to a child HWND. The host has never had focus (_focusedControl == nullptr), and the app's SetOnFocusChanged callback rebuilds the panel or calls SetRoot. The user clicks the control: OnMouseDown returns true, SetFocus(hwnd) runs, then WM_SETFOCUS, OnSetFocus, SetFocusControl(first focusable), _onFocusChanged(first) and the rebuild, which destroys the clicked control. SetFocusControl(liveControl) then dereferences freed mem …
- **Fix**: In HandleMessage's handled branch (DxUi.WindowHost.cpp:2723), revalidate after every reentrant step, the same way FocusControlAndSurvive does: ``` Control* focusTarget = liveControl; if (focusTarget-&gt;IsFocusable()) { if (! focusTarget-&gt;SupportsTextInput()) { SetFocus(hwnd); focusTarget = RevalidateInteractiveDispatchedControl(targetLifetime, _root.get(), focusTarget); } if (focusTarget) { SetFocusControl(focusTarget); focusTarget = RevalidateInteractiveDispatchedControl(targetLifetime, _ro …
- **Test**: Add a WindowHost test that goes through HandleMessage instead of calling OnMouseDown directly: - Create a real host HWND that does not have native focus: a child of a parent window, with focus left on the parent or on a sibling native control. - Make the root a Panel holding one consumer Control sub …

### menu-layout-ux-1

**Plain menus still reserve a scrollbar lane and scroll by a fraction of a DIP at 125%/175% because of whole-pixel rounding; the #24 fix only covered described menus**  
`src/Controls/DxUi.Menu.cpp:1183` · #24 · bug · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #24 found that rounding the popup height to whole device pixels can leave the viewport up to half a pixel shorter than content that actually fits. It added a half-pixel tolerance, but only for popups with described rows. Plain popups keep the strict `contentHeightDip &gt; viewportHeightDip` test. The surface height is `DipExtentToPixels(heightDip, dpi)` (lround), and `menuHeightDip` is that pixel height converted back to DIPs. At 120 DPI (125%), any content height of 4n+1 DIPs rounds down and NeedsScrollbar() becomes true. Rows are 32/24 DIP, headers 24/20, sliders 72 and padding 8, all multiples of 4, but a separator is 9 DIP. So every plain menu with 1 or 5 separators at 125%, or 3 or 7 separators at 175%, gets this. The test added in #24 …
- **Failure**: On a 125% monitor (dpi 120), open a plain context menu with four commands and one separator. Content is 8 + 4*32 + 9 = 145 DIP, which is 181.25 px and rounds to 181 px, so menuHeightDip = 144.8. NeedsScrollbar() returns true: a 12-DIP scrollbar track is painted, every row's text and highlight area loses 12 DIP, and the mouse wheel scrolls the content by 0.2 DIP. Gallery and most tests run at 96 DPI, so nothing catches it.
- **Fix**: Make the half-pixel tolerance apply to every popup, then delete the described-only special case: ``` [[nodiscard]] bool ContentOverflowsViewport(float viewportHeightDip) const noexcept { if (! (viewportHeightDip &gt; 0.0f)) return false; return contentHeightDip - viewportHeightDip &gt; PixelToDip(0.5f); } [[nodiscard]] float GetScrollExtent() const noexcept { if (! NeedsScrollbar()) return 0.0f; return (std::max)(0.0f, contentHeightDip - menuHeightDip); } ``` Also update the comment at lines 117 …
- **Test**: Add a test that opens a plain (non-described) context menu with 4 commands and 1 separator at standard density. That is 137 DIP of content; at Compact density use 2 commands + 1 separator, which is 65 DIP. - Pick the DPI with the same roundsBelowContent search used in TestDescribedMenuFractionalDpiK …

### menu-loop-lifetime-2

**Process-exit host sweep frees an open async menu's ControlHost while its own Detach is still running (use-after-free)**  
`src/Controls/DxUi.WindowHost.cpp:1288` · #23, #41 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Nothing in ShutdownAllWindowHostsForProcessExit handles open menus. Menu popup ControlHosts register in the attached-host registry like any other host. The sweep copies raw ControlHost* pointers and calls DetachForProcessExit on each one. For the root popup of a ShowAsync session, which holds capture (BeginAsyncMenuInteraction calls SetCapture(root-&gt;hwnd)), Detach(true) calls ReleaseCapture() synchronously. That sends WM_CAPTURECHANGED to MenuWndProc, which takes the async capture-loss dismissal and calls FinalizeAsyncMenuController. That runs DestroyPopupChain and `controller.popups.clear()`, which destroys every MenuPopup, including the ControlHost that is mid-Detach. ~ControlHost calls Detach(), which returns at once because `_detachI …
- **Failure**: An async context menu is open. The application exits without first destroying its windows, for example from a WM_ENDSESSION or quit-command handler that calls ShutdownAllWindowHostsForProcessExit and then exits, so the sweep reaches the root popup's host. (1) Detach continues on freed `this`: DeactivateTextInput(false), UnregisterWindowHostAccessibilityTarget(attachedHwnd, this), `_root.reset()` and the writes to `_attachmentOwnerThreadId` and `_detachInProgress`, causing a heap use-after-free and a crash during exit. (2) The sweep's `attachedHosts` snapshot also holds the hosts of open submen …
- **Fix**: 1. Close async menus before detaching any host. Add an internal `ShutdownAsyncMenusForCurrentThread()` to Menu.cpp. It should repeatedly take the last entry of `ActiveAsyncMenuControllers()` and call `Dismiss()` + `FinalizeAsyncMenuController()`; whether `onClosed` runs at process exit is a product decision. In `ShutdownAllWindowHostsForProcessExit`: - On the current thread, call it before taking the snapshot. - For each foreign owner thread, marshal it once through the existing `WindowHostProce …
- **Test**: Add a ControlTests WindowHost/Menu test, and run it under ASan Debug: 1. Create an owner AttachedHostWindow. 2. Call `ContextMenu::ShowAsync` with a submenu item, and open the submenu so two popup hosts are registered. 3. Require that `DebugGetAttachedWindowHostCount() == 3` and that `GetCapture()` …

### test-support-infra-1

**Ctrl+C under test.ps1 -Interactive can kill the lease before it restores the desktop**  
`Tools/InteractiveRun.psm1:223` · #48, #63 · bug · confirmed (trace: plausible/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The lease is started as a PowerShell native command whose stdout is redirected: it is piped to Out-Host, and before #63 it was captured as function output. On Ctrl+C, PowerShell's NativeCommandProcessor.StopProcessing kills a non-standalone native process with Process.Kill, so it stops abruptly. The same Ctrl+C also reaches the lease's ConsoleHandler (Main.cpp:49-67), which only sets an event. The main thread then has to end the job, wait for the child, run DesktopLease::Restore (up to several seconds) and write the result. The kill from PowerShell arrives within milliseconds, so the restore normally never finishes. The kill-on-close job ends the child, but the foreground window, keyboard focus and pointer are not given back, and no result …
- **Failure**: The person confirms and the Menu suite is running with a test window in the foreground and the pointer moved. They press Ctrl+C in the terminal. pwsh kills DxUi.InteractiveLease.exe while its handler thread has only set g_interruptEvent. The job kills the child. The test window disappears, Windows activates whatever window is next in z-order, and the pointer stays where the test left it. interactive-result.txt is missing, and the next run finds an abandoned mutex.
- **Fix**: PowerShell must not own the lease process's lifetime. 1. In Invoke-DxUiInteractiveLease, start the lease with `$p = Start-Process -FilePath $Executable -ArgumentList &lt;quoted args&gt; -NoNewWindow -PassThru`, then read `$null = $p.Handle` right away so the exit code stays available afterwards. 2. Its output goes straight to the shared console, with no redirection. The lease still gets Ctrl+C itself through that console. 3. Wait in a loop PowerShell can stop: `try { while (-not $p.WaitForExit(2 …
- **Test**: Add a case to Tools/tests/Test-InteractiveMode.ps1. 1. Build a fake lease that waits about 300 ms after start, then sleeps another 1.5 s, then writes a marker and a result file. A small pwsh-hosted script launched through a native shim is enough, matching the existing $fake pattern. 2. Run Invoke-Dx …

### text-input-1

**TextField clear button writes to the field after its text-changed callback destroyed it**  
`src/Controls/DxUi.TextInput.cpp:1752` · #57, #60 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The clear-button branch of TextField::OnMouseDown calls SetTextAndNotify({}). That function ignores the liveness result of NotifyChanged(). The branch then calls ResetCaretBlink(host), which writes _caretBlinkAnchorTickMs and _caretVisible, and then Invalidate(host). Every keystroke path in the same file checks `if (! NotifyChanged()) return true;`, and #57/#60 audited the same kind of defect for focus and selection callbacks. This site was missed. It belongs to the 'wider delegate audit' item that is still open in CodexBranchReview_2026-10-02.md, but it is a concrete heap write after free, not a hypothetical one.
- **Failure**: A search box rebuilds its results panel, including the box itself, from SetOnTextChanged (for example panel-&gt;ClearChildren() followed by re-adding controls, or host.SetRoot). The user clicks the clear (X) button. onTextChanged destroys the TextField, then ResetCaretBlink writes two members of the freed object. That is heap corruption, which ASan reports as heap-use-after-free.
- **Fix**: Do not change the public `void SetTextAndNotify`. In the clear branch of TextField::OnMouseDown, call the private `[[nodiscard]] bool NotifyChanged()` directly and stop if the field was destroyed: ResetSingleLineSelectionClickSequence(_selectionClickSequence); _passwordRevealKeyboardFocused = false; SetText({}); if (! NotifyChanged()) return true;   // onTextChanged destroyed this field; host revalidates via its lifetime token ResetCaretBlink(host); Invalidate(host); return true; An alternative …
- **Test**: Add a Controls test to the TextInput or EditorControls suite. It should run under the ASan Debug configuration and must not take real focus; use a hidden WindowHost the way the existing lifetime tests do. 1. Create a WindowHost whose root is a Panel containing a TextField with SetClearButtonEnabled( …

### text-input-2

**Native IMM commit and WM_PASTE/WM_UNDO paths keep using a TextField that onTextChanged destroyed**  
`src/Controls/DxUi.NativeTextInput.cpp:1395` · #57, #60 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: TextField::ImportTextInputState(state, true) calls NotifyChanged() but discards the result and always returns true (TextInput.cpp:2969-2973), so callers cannot tell that the field died. Embedded.cpp guards this with stillLive(), and the native TSF target guards it with GetLiveControl(). The native IMM path does not. applyResultPayload calls SyncNativeTextInputSession(editTarget), and a WM_IME_COMPOSITION that carries both GCS_RESULTSTR and GCS_COMPSTR then calls startCompositionFromCurrentRange(), which dereferences editTarget-&gt;ExportTextInputState, and applyCompositionPayload, which dereferences editTarget-&gt;ImportTextInputState. The host prunes _focusedControl and _nativeTextInputControl lazily: Panel::ClearChildren only clears the c …
- **Failure**: In a native WindowHost a Japanese IME commits a clause while the next clause is still composing (one WM_IME_COMPOSITION with both RESULTSTR and COMPSTR). The field's onTextChanged rebuilds its section with parent-&gt;ClearChildren() and AddChild. applyResultPayload returns, then startCompositionFromCurrentRange makes a virtual call through the freed TextField: use-after-free or crash. The same happens for Shift+Insert, which arrives as WM_PASTE, when the paste's onTextChanged removes the field.
- **Fix**: 1. Make TextField::ImportTextInputState return NotifyChanged()'s result (`return NotifyChanged();` at TextInput.cpp:2971). Make ComboBox::ImportTextInputState do the same. 2. In HandleNativeTextInputImeMessage and HandleNativeTextInputEditMessage, take `const std::weak_ptr&lt;int&gt; editTargetLifetime = editTarget-&gt;GetLifetimeToken();` at the start. Add a `live()` check, `! editTargetLifetime.expired() && editTarget == _focusedControl && IsControlInTree(_root.get(), editTarget)`. Run it afte …
- **Test**: Add a native WindowHost test for the NativeTextInput or Controls suite, run under x64 ASan Debug. Set a TextField as root's child, focus it, and activate the native session. Its SetOnTextChanged callback calls parent-&gt;ClearChildren() and adds a fresh TextField. Then: (a) Send WM_IME_STARTCOMPOSIT …

### text-input-7

**Native TSF writes go through SetText, which resets the multiline scroll to the top (and clears undo)**  
`src/Controls/DxUi.TextStoreACP.cpp:389` · pre-existing · bug · confirmed (single: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: NativeTextStoreTarget::ApplyState is reached from ITextStoreACP::SetText and InsertTextAtSelection on every TIP edit: IME composition, emoji panel, dictation, touch keyboard. It applies the edit with TextField::SetTextAndNotify or SetText. SetText is the programmatic-reset API: it sets _multilineFirstVisibleLine = 0 and _horizontalScrollDip = 0, clears _undoHistory and _redoHistory, and regenerates the concealed mask epoch. ApplyState then only calls SetSelectionRange, and neither it nor multiline Paint calls EnsureMultilineCaretVisible. The IMM and embedded paths use ImportTextInputState instead, which keeps firstVisibleLine and re-ensures the caret is visible. This is pre-existing code, outside the window.
- **Failure**: In a WindowHost multiline TextField scrolled to line 40, the user commits Chinese or Japanese text with a TSF IME, or inserts an emoji with Win+. . The view jumps to line 0 and the caret is off-screen. Ctrl+Z can no longer undo anything typed before the insertion.
- **Fix**: In NativeTextStoreTarget::ApplyState, replace SetText/SetTextAndNotify plus SetSelectionRange with one call for the TextField branch: `textField-&gt;ImportTextInputState(*_host, state, notifyChange)`. The incoming state already carries firstVisibleLine and multiline from ReadState. Keep the existing liveness, focus and text-equality guard and the `_host-&gt;SyncTextInput(control); _host-&gt;Invalidate();` that follow. Add the equivalent for ComboBox: call ComboBox::ImportTextInputState (DxUi.Com …
- **Test**: Add a test to Tests/Controls/DxUiTests.NativeTextInput.cpp that drives the TSF store, following the existing store tests near lines 5700-5871: 1. Create a WindowHost with a multiline TextField of about 100 lines and focus it. 2. Place the caret on line 60 by keyboard (Ctrl+End or arrow keys) so firs …

### tooling-runners-1

**Ctrl+C kills DxUi.InteractiveLease.exe through the PowerShell pipeline, so the desktop is not restored**  
`Tools/InteractiveRun.psm1:223` · #48, #63 · reentrancy · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Invoke-DxUiInteractiveLease starts the lease as a native command whose stdout is piped (`| Out-Host`), and its caller assigns the result (`$lease = Invoke-DxUiInteractiveLease ...`). A native command whose output is redirected is not 'standalone' to PowerShell. When the pipeline stops (which is what Ctrl+C does), NativeCommandProcessor.StopProcessing kills that native process. The lease does install a console handler (Main.cpp:49-66) that sets g_interruptEvent and restores the desktop on its main thread. That handler races pwsh's TerminateProcess, and a killed lease restores nothing: Testing_Validation.md:492 itself says restoration 'cannot happen if the lease process itself is killed'. The test.ps1 help (lines 15-16) and Testing_Validation …
- **Failure**: The person confirms `test.ps1 -Interactive`. The Menu suite has moved the pointer and the test window holds the foreground, and the person presses Ctrl+C to stop the run. pwsh stops the pipeline and kills DxUi.InteractiveLease.exe (the job's kill-on-close also ends the child) before or while the lease's handler runs. The person's window stays deactivated, keyboard focus is lost and the pointer stays where the suite left it. The warning banner disappears with the process, and no result file is written, so test.ps1 reports only 'the lease wrote no result'. I reproduced the mechanism with a piped …
- **Fix**: Start the lease outside PowerShell's native-command pipeline, so that stopping the pipeline never terminates it. The lease shares the console, so it still receives CTRL_C itself and restores the desktop. In Invoke-DxUiInteractiveLease: ``` $p = Start-Process -FilePath $Executable -ArgumentList @('--run', "`"--plan=$plan`"", "`"--result=$result`"", "`"--label=$Label`"", "--estimate=$EstimateSeconds", "--confirm-timeout=$ConfirmSeconds", "--child-timeout=$ChildTimeoutSeconds") -NoNewWindow -PassTh …
- **Test**: Add a case to Tools/tests/Test-InteractiveLease.ps1 that uses a fake lease executable or script. On start it writes a 'started' marker, then sleeps about 2 s, and on exit it writes the result file and a 'cleaned' marker. Plain `pwsh -File` works as the fake if Invoke-DxUiInteractiveLease accepts it, …

### tree-1

**Every key, including Tab, Shift, Ctrl, Alt and F-keys, silently selects or focuses row 0 and scrolls the tree to the top**  
`src/Controls/DxUi.Tree.cpp:1389` · #35, #53 · ux · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: When the tree has no focused item, OnKeyDown runs its "first key" step before it checks which key was pressed. It calls SelectVisibleIndex(0, ...) with notifyDelegate=false and only then reaches the switch, whose default returns false. WindowHost sends every WM_KEYDOWN/WM_SYSKEYDOWN to the focused control's OnKeyDown first: Tab (WindowHost.cpp:2859, before HandleTabNavigation), the bare Shift, Ctrl and Alt keys, Escape and the F-keys. So a key the tree does not handle still changes its state: - Single-select: row 0 becomes the selection without OnTreeSelectionChanged. Since #53 the publish also raises UIA ElementSelected for row 0. - Multi-select (#35): the focus moves to row 0. - Both modes: SelectVisibleIndex calls EnsureVisibleIndex(0), …
- **Failure**: (1) A multi-select tree is focused with no focused item, for example after the user deleted the focused rows. The user wheel-scrolls to row 200 and presses Ctrl to Ctrl+click a row. The Ctrl keydown moves the focus to row 0 and scrolls to the top (SetInputModality(Keyboard) repaints it), so the click lands on a different row than the one the user aimed at. (2) Single-select tree with nothing selected: the user presses Ctrl to fire a Ctrl+D "delete selected" accelerator. The Ctrl keydown reaches the tree first and silently selects row 0, the accelerator reads GetSelectedItemId() == row 0 and de …
- **Fix**: In Tree::OnKeyDown, decide whether the tree handles the key before changing any state. Do this by checking the key first: VK_UP, VK_DOWN, VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_LEFT, VK_RIGHT, VK_RETURN and VK_SPACE are the only keys the switch acts on. For any other key, return false without touching `_selectedItemId`, `_selection` or `_verticalScrollDip`. For example, insert this just before the FindSelectedVisibleIndex block: switch (virtualKey) { case VK_UP: case VK_DOWN: case VK_HOME: case …
- **Test**: Add TestTreeUnhandledKeysLeaveAnUnfocusedTreeUntouched to Tests/Controls/DxUiTests.Tree.cpp. Build a TreeSelectionFixture with FlatTreeItems(200) in both single-select and multi-select modes, with no focused item. Scroll the view down: SetVerticalScrollOffset, or a wheel message through the host, so …

### uia-lifetime-threading-1

**SetRoot destroys the old tree before Reparent reads the new root's old parent chain (use-after-free)**  
`src/Controls/DxUi.WindowHost.cpp:1367` · #31 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #31 replaced `SetParent(nullptr)` with `Control::Reparent`. Reparent reads the inherited flow direction and density through `_parent` before it changes anything, and its own comment says the old parent "must still exist". `ControlHost::SetRoot` move-assigns `_root` first, which deletes the old root and its whole subtree, and only then calls `_root-&gt;Reparent(nullptr, this)`. The public header documents moving a child out of `Panel::GetChildren()` to hand it to SetRoot. When that child came from the current root's own tree, its `_parent` points into the tree SetRoot just freed.
- **Failure**: An app promotes part of its current UI to be the root: `host.SetRoot(std::move(rootPanel-&gt;GetChildren()[0]))`, for example to make a page the whole window. `_root = std::move(root)` deletes rootPanel. `Reparent` then calls `GetFlowDirection()`, which runs `_parent-&gt;GetFlowDirection()` on the freed panel, followed by `GetDensity()`. The result is a heap-use-after-free under ASan, or garbage reads or a crash in Release. Before #31 this path never read the old parent. The moved-control tests only move between two different hosts, where the old panel stays alive, so they never hit this.
- **Fix**: In SetRoot, keep the old tree alive until after the new root has been reparented: ``` std::unique_ptr&lt;Control&gt; oldRoot = std::exchange(_root, std::move(root)); if (oldRoot) oldRoot-&gt;PropagateHost(nullptr); _defaultButton = nullptr; _cancelButton = nullptr; if (_root) { _root-&gt;Reparent(nullptr, this); _root-&gt;SetBounds(...); } oldRoot.reset(); RefreshWindowHostAccessibilitySnapshot(_hwnd, this); ... ``` This resets oldRoot before the accessibility refresh, so the UIA snapshot never …
- **Test**: Add a same-host moved-control test to Tests/Controls (alongside the existing cross-host test in the moved-controls header). Create a WindowHost whose root is a Panel holding a child (a Button, say, with no explicit flow direction or density), then call `host.Host().SetRoot(TakeChild(*rootPanel, 0u)) …

### uia-selection-semantics-2

**Grid's keyboard current row and UIA focus row come from the last id of the ordered selection, which is model order after a range or a data change: Shift+Up sticks, and a data refresh moves focus and announces it**  
`src/Controls/DxUi.Grid.cpp:3955` · #53 · bug · confirmed (trace: confirmed/high; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Grid has no focus row of its own. OnKeyDown takes the row that keys move from as the last id of GetOrderedSelection(), and GetPrimarySelectedRow, which the accessibility snapshot publishes as the focused GridRow (DxUi.Accessibility.cpp:939-943), uses the same id. GridSelectionModel::SetRange stores the range in model order whichever way it was extended (std::minmax of anchor and current). PreserveOrdered, which every NotifyDataChanged runs through ReconcileSelectionForVisibleRows, reorders a Ctrl+click selection (stored in insertion order) into model order. The last id is therefore often neither the row the user moved to nor the row they last clicked. The core is older than this window, but #53 and the focus-from-snapshot-diff work now turn …
- **Failure**: (1) Click row 10, then press Shift+Up: SetRange(anchor 10, current 9) stores [9,10] and back() is 10. The next Shift+Up starts again from row 10 and produces [9,10] again, so upward extension stops after one row. Shift+Home followed by Shift+Up shrinks [0..10] to [9,10]. Throughout, the UIA focus stays on row 10, so Narrator never announces the rows the user is extending over. (2) Ctrl+click row 5, then Ctrl+click row 2, so the selection is [5,2] with focus on row 2. Any NotifyDataChanged, such as a live model refresh, reorders it to [2,5]. The focus fragment jumps to row 5, focusMoved raises …
- **Fix**: 1. Add a `std::optional&lt;uint64_t&gt; _currentRowId` to Grid, or a current id to GridSelectionModel. 2. Set it in SelectRow on every gesture to the row id passed in: the single row, the moving end of a range, or the toggled row. When a Ctrl toggle deselects that row, it can stay as the focus without being selected, as in a list view. 3. Clear or repair it in Clear/SetSingle callers. In ReconcileSelectionForVisibleRows, keep it when its row is still visible; otherwise use the nearest visible ro …
- **Test**: Add these Grid keyboard tests to Tests/Controls (a Grid with 20 rows, multi-select mode): 1. Click (SelectRow) row 10, then press VK_UP with MK_SHIFT three times. Require the selection to be rows 7..10, i.e. 4 rows. This fails today because the selection stays [9,10]. 2. Click row 10, then press Shi …

## Medium (102)

### controls-editor-theme-1

**ComboBox selection and text-changed callbacks that destroy the combo leave it running on freed memory**  
`src/Controls/DxUi.ComboBox.cpp:2660` · #57, #60 · lifetime · confirmed (trace: confirmed/high; refute: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: CommitSelection calls the consumer's selection-changed callback and then keeps using `this` (EnsurePopupSelectionVisible, host.SyncTextInput(this), Invalidate) without checking the control's lifetime. NotifyTextChanged has the same problem: it calls `_onTextChanged(_text)` on the member std::function, passing a view of the member string, then calls RefreshAccessibilitySnapshot(), which reads _host. Its callers (OnChar at 1734, the edit paths at 1995 and 2084, CommitSelection at 2646) then touch the caret, popup and layout state. In this window #57 made self-focusing controls stop once a focus callback destroyed them, #60 did the same for Tree and Grid selection delegates, and #29 made TextField::NotifyChanged report whether the field surviv …
- **Failure**: A settings page has a 'Mode' ComboBox whose SetOnSelectionChanged replaces the page (PageHost::SetPage, or ClearChildren and rebuild), which destroys the ComboBox. The user picks an item by click (line 989), Enter (1636) or arrow keys (1534/1687/1701). CommitSelection runs the callback, then calls EnsurePopupSelectionVisible -&gt; UpdatePopupLayout on freed memory and host.SyncTextInput(this) with a dangling pointer: a use-after-free that crashes or trips ASan. An editable combo used as a search box whose text-changed handler rebuilds the pane that contains it fails the same way on every keyst …
- **Fix**: 1. Rewrite NotifyTextChanged as `[[nodiscard]] bool NotifyTextChanged()`, following TextField::NotifyChanged: - take `const std::weak_ptr&lt;int&gt; life = GetLifetimeToken();` first; - copy `_onTextChanged` and a `std::wstring` snapshot of `_text`, then invoke the copy; - `if (life.expired()) return false;`, then RefreshAccessibilitySnapshot(); - `return ! life.expired();`. 2. Have every caller (OnChar 1734, the edit paths at 1995 and 2084) return true (handled) immediately when it returns fals …
- **Test**: Add TestComboBoxSelectionAndTextCallbacksThatDestroyTheComboStopTheInput to Tests/Controls/DxUiTests.ComboBox.cpp. Mirror the RequireFocusReplacementLeavesControlAlone pattern: put a ComboBox in a WindowHost root Panel, and install SetOnSelectionChanged and SetOnTextChanged callbacks that call `root …

### controls-editor-theme-4

**A same-value SetValue during a typed NumericStepper edit silently ends the edit: no Commit, and Escape can no longer revert**  
`src/Controls/DxUi.EditorControls.cpp:929` · #29 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 introduced `_editPreviewed` and the rule that every Preview ends with a Commit or a Cancel. SetValue unconditionally sets `_editing = false; _editPreviewed = false;`, even when the value equals the open edit's current value. A consumer that acknowledges the model from its Preview handler, the 'refresh controls from the model' pattern (Slider supports this explicitly since #25), therefore closes the edit without any terminal notification. CommitEdit and CancelEdit then see `! _editing` and do nothing. When the formatted value differs from the typed text (decimals, clamping), SyncText also rewrites the field under the caret while the user is still typing.
- **Failure**: A NumericStepper is bound to a model, and the consumer's handler does `model.width = c.value; RefreshAll();`, where RefreshAll calls stepper-&gt;SetValue(model.width). The user types '15' into a field showing 10. Preview(15) runs, then SetValue(15) closes the edit. Enter calls CommitEdit, which finds `! _editing` and sends no Commit, so a consumer that persists or records undo on Commit loses the change. Escape no longer restores 10 (`if (! _editing) return false`). With decimals=2, typing '1.5' previews 1.5, the acknowledgement rewrites the text to '1.50' and resets caret and undo history mid …
- **Fix**: In NumericStepper::SetValue, while an edit is open, treat a value equal to the current preview as an acknowledgement and leave the edit alone: ``` void NumericStepper::SetValue(double value) noexcept { const double clamped = ClampValue(value); if (_editing && std::fabs(clamped - _value) &lt;= kValueEpsilon) { return; // model acknowledgement of the open edit's preview: keep the edit, caret and text } ...existing body... } ``` For a different value while an edit is open, choose one of two behavio …
- **Test**: Add a test to Tests/Controls (the EditorControls suite): 1. Create a focused NumericStepper with range 0-100, decimals 0, value 10. 2. In SetOnChange, record each phase, and on Preview call `stepper-&gt;SetValue(change.value)`. 3. Type "15" into the field (focused, so `typing` is true), then submit …

### controls-editor-theme-5

**Slider (and Button hover/focus) transitions freeze when an ancestor hides the control mid-transition; a stale touch halo, pressed thumb or old thumb position shows on re-show**  
`src/Controls/DxUi.Controls.cpp:4907` · #62, #29, #31 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Panel::Tick skips hidden children, and the host stops ticking once no visible control asks for frames. Slider transitions are only advanced from Tick, and Slider::Paint never asks for frames again while a transition is still active. So a transition that is running when an ancestor is hidden stays frozen with active=true until some unrelated control happens to start animating. #29 fixed exactly this for the Button disclosure: Button::Paint re-requests animation while the disclosure is active. The fix was not applied to Slider's new touch halo (#62), its hover and press transitions, its value easing, or Button's hover and focus transitions. Two things make it likely. Slider::OnMouseUp starts the halo-shrink transition before it fires Commit, …
- **Failure**: (a) A user touch-drags a volume slider in a flyout whose Commit callback closes it. The next time the flyout opens, the 48 DIP translucent halo and the pressed thumb are still painted around a slider nobody is touching. (b) A user presses Right Arrow on a slider whose Commit hides the page, or switches TabControl pages within 167 ms. On return the thumb sits at the old position while GetValue and UIA report the new one. (c) The pointer rests on a slider or button while the keyboard switches tabs. The prune starts the hover-out transition on the hidden page, and on return the control shows hove …
- **Fix**: 1. Add `void Slider::OnHidden() noexcept override { Control::OnHidden(); SnapVisualTransitions(); SnapDisplayedValue(); }`. Panel and PageHost already propagate OnHidden, so this covers ancestor hides. 2. In Slider::Paint, mirror Button: `if (_hoverTransition.active || _pressTransition.active || _touchTransition.active || _valueAnimationActive) host.RequestAnimation();`. This covers transitions started after the hide, such as the prune-driven OnHoverChanged(false). 3. In Button::Paint, extend th …
- **Test**: In the Controls suite, put a Slider inside a Panel under a test host and set the touch pointer device. - Touch case: simulate mouse down and up on the thumb. In the Commit callback call panel-&gt;SetVisible(false). Run the host animation ticks until idle (they will skip the slider). Call panel-&gt;S …

### cross-cutting-simplification-2

**UIA Select/AddToSelection/RemoveFromSelection/Toggle/SetValue/Expand run app delegates and raise UIA events under the process-wide accessibility mutex (cross-thread deadlock)**  
`src/Controls/DxUi.Accessibility.cpp:8716` · #29, #35, #53 · threading · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 narrowed `ExecuteSetFocusOnWindowThread` to "the mutex guards resolving the host only": app callbacks and UIA events no longer run under the global `GetAccessibilityTargetMutex()`. The sibling Execute* paths were not narrowed and still hold that recursive mutex for their whole body. These are Toggle (8601), SetStringValue (8643), SetRangeValue (8691), Select (8716), AddToSelection (8771), RemoveFromSelection (8822) and Tree Expand (8913). Under the lock they run application delegates (OnGridSelectionChanged, OnTreeSelectionChanged/SetChanged, OnGridCheckboxToggled, text/slider change callbacks, OnTreeToggleExpanded) and RefreshWindowHostAccessibilitySnapshot. Since #53 that function raises the selection, focus and structure events, whic …
- **Failure**: An app runs two UI threads: a grid window on thread A and a main window on thread B. Narrator, Voice Access or an automation client issues SelectionItem.Select on a grid row. Thread A runs ExecuteSelectOnWindowThread and acquires the global mutex. Grid::RequestSelectRow calls the app's OnGridSelectionChanged, which does `SendMessageW(hwndB, ...)` to update the main window. At the same moment thread B is inside WM_GETOBJECT (AcquireCanonicalRootProvider) or a republish (RefreshWindowHostAccessibilitySnapshot) and blocks on the mutex in a non-pumping wait. A waits for B and B waits for A. Both U …
- **Fix**: Apply the SetFocus/Invoke pattern to Toggle, SetStringValue, SetRangeValue, Select, AddToSelection, RemoveFromSelection and Tree Expand: 1. Resolve host and control under the lock in a nested block, and capture the plain values needed (indices, item id, multi-select flag). 2. Release the lock. 3. Call the Request*/SetTextAndNotify/RequestValue/OnMnemonic/RequestExpandedState method. 4. After every application-reaching call (delegate, SetFocusControl), revalidate with `ResolveHost() == host` and, …
- **Test**: Add a test to the Accessibility control suite. Create WindowHost B on a second thread that pumps messages, and a grid in WindowHost A on the test thread. Give A's grid an IGridDelegate whose OnGridSelectionChanged calls `DxUi::CreateWindowHostAccessibilityProvider(hwndB)`, or `SendMessageTimeoutW(hw …

### cross-cutting-simplification-3

**SetFocusControl's entry prune now publishes and announces before the target's lifetime is taken: UAF window and an extra 'window' focus announcement**  
`src/Controls/DxUi.WindowHost.cpp:1615` · #29, #57 · reentrancy · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 made PruneStaleInteractionState publish when it drops a stale focus (`if (prunedFocus) RefreshWindowHostAccessibilitySnapshot(...)`). A publish raises a focus event via AnnounceWindowHostFocus, and per #60's own premise an STA UI Automation call can dispatch messages there. SetFocusControl calls the prune first, but takes `requestedLifetime = control-&gt;GetLifetimeToken()` only afterwards. If the publish dispatches a message (for example the posted rebuild the docs tell delegates to use) that destroys `control`, SetFocusControl dereferences it. This defeats #57's FocusControlAndSurvive, whose token is checked only after SetFocusControl returns. Even without reentrancy, the prune announces focus on the window root ('nothing focused'). T …
- **Failure**: In a click handler: `okButton-&gt;SetEnabled(false); host.SetFocusControl(nameField);` (or FocusControlAndSurvive from a control's handler) with Narrator running. SetEnabled republishes and the focused record is still okButton. SetFocusControl → PruneStaleInteractionState sees okButton is no longer interactive and clears `_focusedControl`. It publishes, and focusMoved=true makes AnnounceWindowHostFocus raise FocusChanged on the canonical root, so the window is announced. The final publish then announces nameField. If UiaRaiseAutomationEvent pumps a posted rebuild that destroys nameField, line …
- **Fix**: 1. In SetFocusControl, capture `requestedLifetime` (and `requestedControl`) before calling the prune. 2. Make PruneStaleInteractionState stop publishing itself; have it return `bool prunedFocus` instead. Call sites that need a publish do it when the result is true: HandleMessage's entry prune and the other prune calls at 1792, 2438, 2863-2961, 3976, 4058 and 4302. Embedded hosts already publish in UpdateAccessibility. 3. In SetFocusControl, keep the result and publish only once. The `_focusedCon …
- **Test**: Add an Accessibility test using the existing focus-event client (Controls.Tests.DxUiFocusEventClient.h) or the host's `_debugFocusAnnouncementCount`. Steps: create a window host with two buttons, focus button A, and register a UI Automation focus listener so UiaClientsAreListening is true. Within on …

### cross-cutting-simplification-4

**Text range providers still pin a full window-host snapshot each; element providers stopped doing so for exactly this retention reason**  
`src/Controls/DxUi.Accessibility.cpp:4218` · #29 · resource-leak · plausible (single: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 introduced CaptureProviderCreationSnapshot so that window-host element providers no longer pin a snapshot, stating that UI Automation keeps event providers alive while a client listens and that pinning O(controls) snapshots would accumulate until the window closes. All three AccessibilityTextRangeProvider constructors still use `CaptureAccessibilitySnapshot(target, hwnd)` for window hosts. The range reads only one record's text, the bounds scale and the enclosing element from that snapshot (ResolveText 5730, GetEnclosingElement 4980, bounds 4910). Every caret or text change republishes a new snapshot (NativeTextInput.cpp:864) and then raises ActiveTextPositionChanged with a newly created range (9437). Each retained range therefore keeps …
- **Failure**: A user types in a TextField of a window that also holds a large Grid or Tree, with Narrator listening. Each keystroke publishes a new snapshot (several MB for a large grid) and creates an ActiveTextPositionChanged range holding `_snapshot`. Under the premise of the element fix (UIA retains event providers and args while a client listens), memory grows by one whole snapshot per caret move until the window closes. With any client that caches ranges, each cached range pins a stale full snapshot.
- **Fix**: 1. In all three AccessibilityTextRangeProvider constructors, initialize `_snapshot(CaptureProviderCreationSnapshot(target, hwnd))`. Move that helper above the range class, or forward-declare it there. 2. Change the readers to use the guarded current snapshot instead of the pinned one. In ResolveText, GetEnclosingElement and the GetBoundingRectangles scale: ``` const auto snapshot = CaptureSnapshot(); const auto* record = (snapshot && snapshot-&gt;alive && snapshot-&gt;hasRetainedRoot) ? FindCont …
- **Test**: Add a test-only live counter for AccessibilitySnapshot instances, alongside DebugGetContextMenuResources/LiveResourceCount. For example, a Detail::LiveResourceCount member on AccessibilitySnapshot, reported by a debug getter. Accessibility suite test, window host: - A window holds a TextField plus a …

### cross-cutting-simplification-5

**Tree::SetSelectedItemIds builds the selection with one GridSelectionModel::Toggle per id: quadratic in the number of ids**  
`src/Controls/DxUi.Tree.cpp:631` · #35, #43 · performance · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The silent restore API added in #35 calls `_selection.Toggle(itemId)` for every listed visible id. Since #43, Toggle keeps the ascending copy with `_sortedRowIds.insert(sortedIt, rowId)`, which is O(n) per call, so k ids cost O(k²) element moves. The model's own header says its mutators stay within O(n log n) because they run once per gesture; the bulk loop breaks that assumption. The final order is then rebuilt anyway with `PreserveOrdered(visibleIds)`, so the per-id insertion work is wasted.
- **Failure**: A multi-select tree (for example a folder tree with hashed node ids, so not ascending) restores 100,000 selected ids after a refresh via SetSelectedItemIds. The inserts into `_sortedRowIds` at random positions cost about k²/4 = 2.5e9 element moves (about 20 GB of memmove), freezing the UI thread for seconds. 20,000 ids still cost about 0.1 s. The same restore through Grid's SetRange is O(n log n).
- **Fix**: Add one bulk mutator to GridSelectionModel, for example: `void Assign(std::vector&lt;uint64_t&gt; orderedRowIds, std::optional&lt;uint64_t&gt; anchorRowId);` It should build the sorted copy next to the current one (copy, then SortRowIds), apply the same IsRoomWasted / give-back rule SetRange uses, swap both vectors in only after every allocation has succeeded, and set the anchor to `anchorRowId` if that id is present, otherwise to the front. In Tree::SetSelectedItemIds, replace the SetSingle + T …
- **Test**: Add a Controls test, Tree multi-select SetSelectedItemIds at scale: - Build a flat Tree of 100,000 visible items whose ids are a fixed pseudo-random permutation, for example `id = (i * 0x9E3779B97F4A7C15) | 1`, so visible order is not ascending id order. - Call SetSelectedItemIds with all ids in vis …

### gap-arm64-asan-runtime-1

**CI counts capability skips as a pass, so the focus checks on ARM64 and ASan can go green without running**  
`test.ps1:199` · #31, #32, #48, #63 · ci · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid; more important since #66: CI (test.ps1 -Full on GitHub Actions, test.ps1:51) is now the only automatic run of Menu and NativeTextInput, and it still reports a suite with skips as PASS (test.ps1:199).
- **What**: With no -Suites, CI runs test.ps1 with the default suite list. That list includes Menu and NativeTextInput, which run without --no-activate on the hosted runners. Most of their focus-dependent tests skip when activation fails or another app keeps the foreground: TryActivateDxUiTestWindow gives up after 800 ms (wall clock), and RunUntilForegroundHeld skips after five lost attempts. test.ps1 still prints PASS with only a skip count, and nothing in ci.yml or the tooling fails a lane on skips or compares them with an expected set. The only proof that ARM64 and ASan Debug run these checks is a single historical run (36951713344) cited in Testing_Validation.md. AGENTS.md requires native ARM64 runtime evidence, and on these lanes that evidence can …
- **Failure**: An ARM64 runner image change, or a slower ARM64 ASan Debug foreground switch, makes TryActivateDxUiTestWindow exceed 800 ms (or another process takes the foreground in all five attempts). Every focus test in Menu/NativeTextInput then logs 'SKIPPED: ... requires an interactive desktop' and the job reports 'PASS Menu (N capability skips recorded)'. A PR that breaks focus restoration, UIA focus announcements or the wedged-probe bound on ARM64/ASan still merges green, and consumers adopt that main commit as validated.
- **Fix**: Add an `-AllowedSkips`/expected-skip baseline. The smallest version is a checked-in JSON file that maps suite to an allowed skip count or test-name set, per runner class. Have test.ps1 fail when `$env:GITHUB_ACTIONS` is set and a suite records a skip outside that baseline, using zero for Menu and NativeTextInput on the hosted runners if that is the intended policy. Write unexpected skips to `$env:GITHUB_STEP_SUMMARY`. Then change Testing_Validation.md:236 so it states the enforced rule instead o …
- **Test**: Add a tooling test under Tools/tests that gives the skip-evaluation function a fake suite log containing a `SKIPPED: DxUi menu debug-state timeout requires an interactive desktop` line for Menu, with GITHUB_ACTIONS set and an empty baseline, and asserts that the suite is reported as a failure. With …

### gap-arm64-asan-runtime-3

**UiaTestClient subscribes to desktop-wide focus through CUIAutomation with none of the hang protection #63 added to FocusEventClient**  
`Tests/Support/Support.Tests.UiaTestClient.h:684` · #63, #53, #51 · test-quality · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: PR #63 found that focus subscriptions inspect unrelated providers across the desktop. It switched FocusEventClient to CUIAutomation8 with a 2 s connection timeout and a 3 s transaction timeout, and Testing_Validation.md now states that a desktop-wide subscription must not give an unrelated provider an unbounded wait. The shared UiaTestClient (Tests/Support) still creates CLSID_CUIAutomation, which exposes no timeouts. EmbeddedUiaTests turns on its desktop-wide focus subscription, and the Embedded suite has no per-test watchdog. The fix for this defect class was applied to one of the two UIA client helpers and not the other.
- **Failure**: A hosted runner or developer desktop has a slow or hung UIA provider (another application). In TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow, AddFocusChangedEventHandler, focus event delivery or RemoveAllEventHandlers waits on that provider. Setup exceeds kSetupAllowanceMs (20 s) and Fail("the UI Automation client starts") ends the Embedded suite, or the destructor fails with "the UI Automation client ends". This is the flake #63 removed from the Menu lane, now in Embedded.
- **Fix**: 1. Move `CreateFocusAutomationClient` from `Tests/Controls/DxUiFocusEventClient.h` into `Tests/Support/UiaTestClient.h`, in the `UiaTest` namespace, using the same `CUIAutomation8` with a 2000 ms connection timeout and a 3000 ms transaction timeout. 2. Have `DxUiFocusEventClient.h` call the moved helper. 3. In `Client::RunSession`, replace the `CoCreateInstance(CLSID_CUIAutomation, ...)` at line 684 with `hr = CreateFocusAutomationClient(session.automation.put());`. Applying it to every session …
- **Test**: Add a source-policy or tooling check under `Tools/tests` (or in the validator that owns test-source policy). It should fail when any file under `Tests/` creates `CLSID_CUIAutomation` directly rather than going through the shared timeout helper, and fail when `UiaTestClient.h` does not set `put_Trans …

### gap-arm64-asan-runtime-4

**Default non-activating NewControls lane moves the user's physical pointer with no lease to restore it**  
`Tests/Controls/DxUi.Tests.Menu.cpp:7993` · #29, #63 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: RunMenuDescriptionTests runs in both the Menu lane and the NewControls lane. Every plain test.ps1 run includes NewControls with --no-activate, documented as never reaching for the desktop. TestMenuChoosesTheCursorWhenItOpensAndCloses is in that group and calls SetCursorPos on the physical pointer of whoever is at the desktop. It restores the pointer only from a destructor, and only when the pointer has not moved since. When the process ends through the watchdog's TerminateProcess (exit code 124) or a Require/std::exit inside the attempt, no destructor runs, and outside -Interactive no lease restores the pointer. Every other pointer-moving fixture is confined to the Menu lane.
- **Failure**: A developer runs plain test.ps1 while working. In NewControls the pointer jumps to the test's text window at (100,520)+. If the menu under test hangs in that attempt, for example the Escape never closes it so the loop keeps waiting, the watchdog terminates the process after 300 s and the pointer is never put back. The same happens if the Require in ClientScreenPointForTest, which runs after AlignCursor, ever fails.
- **Fix**: At the top of TestMenuChoosesTheCursorWhenItOpensAndCloses (Menu.cpp:7947), add: ``` if (! DxUiTestWindowsCanActivateFlag()) { SkipDxUiTest("the menu open-and-close cursor test moves the physical pointer; it runs in the Menu lane under -Interactive"); return; } ``` (or a bare `return` like the sibling at 8148). The test then runs only in the Menu lane, where the -Interactive lease restores the pointer on every exit path, including Require/std::exit and the watchdog. Optionally, for resilience in …
- **Test**: Add a tooling or native check that runs the NewControls suite with `--no-activate --test=TestMenuChoosesTheCursorWhenItOpensAndCloses` and asserts the test either reports SKIPPED or never moves the pointer. A simple way is to read GetPhysicalCursorPos before and after and confirm the pointer was not …

### gap-embedded-host-parity-2

**Embedded event providers still each pin a full accessibility snapshot, the retention #29 fixed for window hosts and #53 now multiplies**  
`src/Controls/DxUi.Accessibility.cpp:4323` · #29, #53 · resource-leak · confirmed (trace: confirmed/high; refute: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: In #29 the window host stopped event providers from pinning a snapshot, because UIA keeps event providers alive while a client listens. That plan measured tens to hundreds of KB per toggle, kept until the window closed. Embedded providers still pin: `CaptureProviderCreationSnapshot` returns the current snapshot for any embedded target. `RaiseEmbeddedAccessibilityChanges` creates one such provider per changed record on every publish. Since #53, `RaiseSelectionEvents` also creates one per changed tree item or grid row, each holding the O(controls + visible items + selected rows) snapshot of that publish. Each publish stores a new snapshot, so the retained snapshots are distinct and accumulate.
- **Failure**: An embedded Grid with a few hundred visible rows has a screen reader listening. Each arrow key, or each slider drag step that is published (RangeValue property event), creates new event providers. Each one pins that publish's snapshot, which includes point-hit records, navigation records and up to 256 off-screen selected rows. Memory grows with every published change for as long as the client listens, the same pattern the window host was fixed for.
- **Fix**: Remove the pin and give embedded elements the identity that window-host elements already use. 1. In `CaptureWindowHostControlIdentity`, drop the `target-&gt;embedded` exclusion so embedded elements also record `{controlLifetime, controlIdentity}` from the current snapshot when they are created. 2. Make `CaptureProviderCreationSnapshot` return null in every case. Change the constructors of AccessibilityTextRangeProvider (4218-4245) to stop pinning as well. 3. In `GuardedElementSnapshot`, use the …
- **Test**: Add an embedded-suite test with a live UIA client. - Setup: register an in-process `IUIAutomation` property-changed handler and a focus/SelectionItem event handler on an embedded host that contains a Grid with about 200 rows. - Instrumentation: add a test-only `LiveResource::AccessibilitySnapshot` c …

### gap-perf-evidence-pipeline-1

**Gate tests pin the library-only fingerprint; no case would fail when a measured build or executable change is judged as 'library unchanged'**  
`Tools/tests/Test-PairedRun.ps1:259` · #46, #31 · test-quality · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged, line moved.
- **What**: Question (1), fingerprint part. Get-BenchmarkScopeRules counts Tests/Embedded, Tests/Support, Samples, DxUi.sln, build.ps1, vcpkg-install.ps1, vcpkg-configuration.json and the VcpkgTriplet/VisualStudio modules as measured. Get-BenchmarkConclusion, however, derives `LibraryUnchanged` from sourceFingerprint, which covers only src, include, Build, the Directory.Build files and the vcpkg manifests, and it turns every non-exact regression into 'noise' when the fingerprints match. The tests cover each half separately and never together. Test-PairedRun asserts that the fingerprint equals the historic library-only computation. Test-BenchmarkGate:34-54 asserts that build, restore and executable paths are 'measured'. Test-BenchmarkGate:408-422 assert …
- **Failure**: A PR changes only Tests/Embedded/DxUi.EmbeddedTests.vcxproj, EmbeddedTests.cpp, Tests/Support/FailureReports.h or Tools/VcpkgTriplet.psm1. Get-BenchmarkScope marks it relevant and the hosted run measures it. Both fingerprints are equal, so a confirmed dirty fps or privateBytes regression is listed as 'regressed, unchanged library (noise)', and the conclusion is pass with exit 0. The whole tooling suite stays green.
- **Fix**: Keep sourceFingerprint as the historic library-only receipt value. Give the "unchanged" decision its own identity: 1. Add `compiledFingerprint` to Get-SideSummary. Compute it after the harness overlay (performance-paired.ps1:172), with the same per-file SHA-256 scheme, over every path of the scope rules 'library input', 'benchmark executable', 'fixture or sample compiled into it' and 'build or restore'. Export these paths from one function (for example Get-CompiledInputPaths in BenchmarkGate or …
- **Test**: Add these to Test-BenchmarkGate.ps1 or Test-PairedRun.ps1: 1. A fixture case that builds two git trees with identical src/include/vcpkg.json and a differing Tests/Embedded/EmbeddedTests.cpp. Run a second variant that differs only in build.ps1. Compute each side's identity with the function the gate …

### gap-perf-evidence-pipeline-3

**The benchmark's allocation counters live in a file that neither benchmarkSha256 nor the paired overlay covers, so the two sides can be measured with different instruments**  
`Tools/PerformanceComparison.psm1:19` · #43, #46, #31 · architecture · confirmed (batch: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: `cppAllocations` and `composeAllocations` come from the replaced `operator new`/`operator delete` and the `allocations`/`countAllocations` thread_locals in Tests/Embedded/EmbeddedTests.cpp. `Check`/`Hr`, which end the process, and the static initializer `g_failureReportsRouted` are in that file too. performance.ps1 hashes only `$script:BenchmarkInputs` into `benchmarkSha256`, and Get-PairedHarness copies only those headers plus the scripts onto both trees. EmbeddedTests.cpp and DxUi.EmbeddedTests.vcxproj are in neither list. In a paired run each side therefore compiles its own instrument, yet `Assert-MatchedFixture` accepts the pair because benchmarkSha256 matches. PR #43 already changed this hook: it added `allocationBytes` and a `countLiv …
- **Failure**: A PR changes the hook in EmbeddedTests.cpp (for example it starts counting the aligned or nothrow forms, or stops counting one) together with src. The baseline worktree builds its own older EmbeddedTests.cpp and the candidate builds the new one. The exact budget dirty/cppAllocations then rises or falls by the instrument difference, and the gate reports a confirmed degradation, or an improvement that hides a real allocation rise, blamed on the library. If only EmbeddedTests.cpp or the vcxproj changes, the library fingerprints match and timing flags become 'noise', but an exact budget moved by t …
- **Fix**: Move the replacement operator new/new[]/delete/delete[], the countAllocations/allocations/allocationBytes/countLiveBytes/liveBytes TLS and the FreeBlock helper into a header that only EmbeddedTests.cpp includes, for example Tests/Embedded/BenchmarkInstrument.h. The replacement operators must still be defined in exactly one translation unit. Add the header to `$script:BenchmarkInputs` so it is hashed, overlaid and checked by Get-OverlayCompiledChanges. Note that the overlay only takes effect for …

### gap-uia-mutex-nested-loop-matrix-1

**UIA Toggle/Select/SetValue/Expand keep the process-wide accessibility mutex locked while app delegates and nested modal loops run**  
`src/Controls/DxUi.Accessibility.cpp:8601` · #53, #54, #56, #57, #60 · threading · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: These functions lock GetAccessibilityTargetMutex() (one static std::recursive_mutex for the whole process, line 791) for their whole body: ExecuteToggleOnWindowThread (8601), ExecuteSetStringValueOnWindowThread (8643), ExecuteSetRangeValueOnWindowThread (8691), ExecuteSelectOnWindowThread (8716), ExecuteAddToSelectionOnWindowThread (8771), ExecuteRemoveFromSelectionOnWindowThread (8822), the Tree branch of ExecuteExpandOnWindowThread (8913) and AccessibilityTextRangeProvider::ExecuteSelectOnWindowThread (5274). While locked, each one calls application code: Toggle::OnMnemonic (focus callbacks, then onToggled), Grid::RequestToggleCheckboxCell, TextField/ComboBox::SetTextAndNotify, Slider::RequestValue, Splitter::RequestPosition, the Tree/Gri …
- **Failure**: (a) Narrator toggles a checkbox through the Toggle pattern. The RPC thread posts the action. The window thread runs ExecuteToggleOnWindowThread with the mutex locked, and the app's onToggled opens a DxUi ContextMenu::Show, a MessageBox or a confirmation dialog. RunMenuModalLoop pumps every message, so the menu works with the keyboard. But every UIA call into any DxUi provider in the process, including QueryInterface on the new menu-row elements, blocks on the mutex until the menu or dialog closes. Narrator cannot read or invoke the menu it just caused to open. It hangs for its call timeouts. T …
- **Fix**: Apply the existing Invoke/SetFocus pattern to every Execute* path that reaches app code: ExecuteToggleOnWindowThread, ExecuteSetStringValueOnWindowThread, ExecuteSetRangeValueOnWindowThread, ExecuteSelect/AddToSelection/RemoveFromSelectionOnWindowThread and the Tree branch of ExecuteExpandOnWindowThread. - In a scoped block that holds GetAccessibilityTargetMutex(), resolve the host, the control (Toggle*, Grid*, Tree*, TextField*/ComboBox*, Slider*/Splitter*), the visible or row index and the cel …
- **Test**: Add a WindowHost accessibility test with a Checkbox whose onToggled runs a nested message loop on the window thread. It should set an event "entered", then pump messages until a second event is set. Also add a second plain control. 1. From an MTA test thread, call IToggleProvider::Toggle on the chec …

### gap-uia-test-falsifiability-1

**Every selection-event test deduplicates what it hears, so a library-level double raise (double Narrator announcement) cannot fail any test**  
`Tests/Support/Support.Tests.UiaTestClient.h:325` · #51, #53, #54, #60 · test-quality · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: UiaTest::Client::DistinctSince sorts and removes duplicate events. HearSelectionEvents then deduplicates the described strings a second time, and the older SelectionEventObserver::Take/DistinctCount in the multi-select tree test does the same. ExpectHeard therefore compares sets of distinct events. If the library raises the same IsSelected change or the same ElementSelected/Added/Removed event twice for one change, every test still passes. That can happen through two publishes that each diff against the same `before`, through RaiseSelectionEvents being reached twice, or through a raise loop that repeats an id. The reason given for deduplicating ("an in-process client hears each event twice ... about 60 ms apart") has not been shown to be a …
- **Failure**: A regression makes a Tree click publish twice against the same previous snapshot, or makes raiseItem run twice per id. Each change then raises 'IsSelected:true Afficheurs' and 'Selected Afficheurs' twice, and Narrator announces the selection twice. DistinctSince and HearSelectionEvents collapse the duplicates, ExpectHeard sees {Selected, IsSelected:true, IsSelected:false}, and TestSingleTreeWindowItemEventsReachAClientSubscribedToTheWindow, the grid twin and the embedded twins all pass.
- **Fix**: Count library raises as well as comparing the client's deduplicated set. 1) In the window-host and embedded `hear` lambdas (DxUiTests.Accessibility.cpp:7246 and :7441, EmbeddedUiaTests.h:369 and :418), construct `UiaTest::SelectionEventInterruption counter({});` around `action()`. A null action only counts. 2) Return or require `counter.Events() == expected`, where `expected` is the exact number of described events for the step. The current call sites already pass the list size, for example hear …
- **Test**: Show that the strengthened test fails for a double raise: 1) Temporarily duplicate the raise in RaiseSelectionEvents. For example, call raiseItem twice for change.added[index], or call RaiseWindowHostSelectionChanges(hwnd, changes.selections) twice at DxUi.Accessibility.cpp:9587. 2) With the current …

### gap-visual-modes-hc-rtl-dpi-3

**Described-menu descriptions use subduedText, a blend that falls to about 3.5:1 in the light Windows HC theme (Desert)**  
`src/Controls/DxUi.Menu.cpp:3276` · #24, #37 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #24/#37 draw a menu row's description (Small font role) in accelColor = style.accelText = theme.subduedText. MakeThemePalette makes subduedText BlendColor(text, windowBackground, 0.38) for a light base, and this also applies in high contrast. HC is supposed to use system colors only, and when HC text is not pure black the blend drops below 4.5:1. Desert (#3D3D3D on #FFFAEF) blends to about #878582, about 3.5:1 against the popup background, for small text. Descriptions carry real information (the gallery tile disambiguates two identical labels purely by parent location), so an HC user must read them.
- **Failure**: Windows HC Desert. A consumer opens a described menu whose two rows share a label and differ only in their description paths. The paths render at about 3.5:1 in the small font, below the 4.5:1 required for small text, so the user cannot reliably tell the rows apart.
- **Fix**: In MakeThemePalette, after the subduedText assignment, add `if (palette.highContrast) palette.subduedText = palette.text;`. Windows high-contrast menus draw accelerators and secondary text in the menu text color, and GrayText is reserved for disabled state. This also fixes accelerators, group headers and chevrons. Disabled rows still use alpha 0.4, a separate existing choice that could later map to a supplied GrayText. If keeping visual hierarchy matters more, a second option is to raise the ble …
- **Test**: Add a Theme test that builds ThemeColors like Desert: backgroundArgb 0xFFFFFAEF, textArgb 0xFF3D3D3D, selection 0xFF903909/0xFFFFF5E3, highContrast=TRUE, darkBase=FALSE. It should assert that the WCAG contrast of palette.subduedText against both palette.windowBackground and palette.surfaceBackground …

### gap-visual-modes-hc-rtl-dpi-4

**Tree, described menus, Grid and determinate ProgressBar ignore FlowDirection, and no public doc records the limit**  
`src/Controls/DxUi.Tree.cpp:221` · #24, #35, #37, #38, #28, #29 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Neither DxUi.Tree.cpp nor DxUi.Menu.cpp reads FlowDirection. Tree rows are laid out left to right: indent and expander on the left, badge on the right, and text drawn through DrawCenteredText with its default LTR reading direction. VK_LEFT/VK_RIGHT collapse and expand without mirroring. The new multi-select, expander hit zone and reorder marker inherit this. Described menu rows (#24/#37) build their layout from the body format with LEADING alignment, an LTR reading direction, and fixed left lane, right shortcut and right chevron positions. Grid does not mirror, which only a WIP plan records (GridTextOverflow_2026-09-21.md:65-68). The determinate ProgressBar fills from bounds.left in RTL, although the Slider beside it mirrors (Controls.cpp:4 …
- **Failure**: A Hebrew or Arabic consumer sets FlowDirection::RightToLeft on its root. Checkbox, TabControl and Slider mirror, but the Tree still indents from the left, puts chevrons on the left, and collapses on Left. Neutral punctuation and numbers in labels are ordered as in an LTR paragraph. Context-menu descriptions read LTR, and a progress bar fills from the left toward the reading start. Nothing in docs/controls.md or the Tree/Grid/ProgressBar design-system pages warns of this.
- **Fix**: 1. **ProgressBar.** In the determinate branch, anchor the fill at the right edge when `IsRightToLeft()`: `const float fillLeft = IsRightToLeft() ? bounds.right - fillWidth : bounds.left;` then use `RectF(fillLeft, trackTop, fillLeft + fillWidth, ...)`. Mirror the indeterminate and secondary segments the same way. 2. **Tree text.** Pass `GetFlowDirection()` (through the paint context) as the last argument of the `Tree.cpp:221` call and of the badge call. 3. **Tree layout and keys.** Mirror the ro …
- **Test**: 1. **ProgressBar.** Render a determinate ProgressBar at value 25 of 100 with `FlowDirection::RightToLeft` into an offscreen target. Assert that a pixel in the rightmost quarter of the track has the progress-fill color and a pixel in the leftmost quarter does not. This fails today because the fill st …

### gap-visual-modes-hc-rtl-dpi-6

**ChooseContrastingTextColor picks a failing text color on mid tones; #29 added a correct WCAG helper next to it instead of fixing it**  
`src/Controls/DxUi.Theme.cpp:249` · #29, #24, #37, #35 · architecture · confirmed (batch: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ChooseContrastingTextColor computes a weighted sum of gamma-encoded channels and returns white below 0.55. WCAG relative luminance is computed in linear light, and the black/white crossover sits near an sRGB gray of 0.46. For backgrounds in roughly the 0.46-0.55 band it returns white below 4.5:1 while black would pass. #29 added MostContrastingNeutral, which uses the proper linear ContrastRatio, but only ResolveAlertTone calls it. About 15 call sites still use the old helper, including ones the window added or kept: the described-menu hover text, accelerator and description colors (Menu.cpp:450-455, used by DrawMenuDescription), Tree and Grid rainbow row text, and Grid FolderView rainbow selection. Rainbow tints and the menu hover composite …
- **Failure**: Rainbow mode, a hovered menu row whose tint composites to about (0.5,0.5,0.5). The gamma luminance is 0.50, below 0.55, so the label and description render in white (0.98) at 3.8:1. Black (0.06) would reach 4.8:1. A Tree or Grid rainbow-selected row with the same tint has the same problem.
- **Fix**: Reimplement ChooseContrastingTextColor in Theme.cpp as `const D2D1_COLOR_F dark = ColorF(0.06,0.06,0.06,1), light = ColorF(0.98,0.98,0.98,1); return ContrastRatio(light, background) &gt;= ContrastRatio(dark, background) ? light : dark;`. Make MostContrastingNeutral the same comparison with pure 0/1 neutrals, sharing one helper parameterized by the neutral pair, or remove it if pure neutrals are not required. Add a Theme test that sweeps gray 0.40-0.60 and every rainbow hue at the shipped HSV par …

### grid-multiline-1

**Full-value tooltip of a clamped cell has no size bound: it grows past the window, cuts the text at 256 DIP with no marker, and is re-laid out on every mouse move**  
`src/Controls/DxUi.Controls.cpp:9467` · #22, #29, #36 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: When a multiline cell omits lines, Grid::OnMouseMove passes the whole model value to host.SetTooltip. TooltipLayer::EnsureLayoutCache lays out the whole string at 260 DIP wide. It sizes the tooltip box from the full DWRITE_TEXT_METRICS.height. Paint then draws the layout with kTextDrawOptions, which includes D2D1_DRAW_TEXT_OPTIONS_CLIP, so only the 256-DIP layout box (kTooltipPreferredTextHeightDip) is visible. That is about 15 lines, with no trimming or ellipsis. SetTooltip also treats any change of origin as a change and calls InvalidateLayoutCache. Because the Grid passes the current pointer position, every mouse move over a truncated cell re-shapes the whole value in the tooltip and repaints the whole grid. The Grid itself shapes only a …
- **Failure**: A multiline log or description column holds a 2,000-character value: about 40 tooltip lines, roughly 650 DIP. Hovering shows a box about 650 DIP tall. Its top is clamped to the client top, it runs past the bottom of a 600-DIP window, and below 256 DIP it is blank, so the rest of the value is unreachable by hover. With the 100,000-unit values the tests hover (TestGridMultilineDecomposedAccentsPaintLikePrecomposed), each pixel of pointer travel re-shapes 100,000 units in CreateTooltipTextLayout and repaints the grid, so hover lags badly.
- **Fix**: 1) Split the TooltipLayer cache in two. Keep the IDWriteTextLayout and its metrics keyed on (text, client width, DPI). When only the origin changes, recompute `bounds` without touching the layout: SetTooltip and SetTooltipDelayed should mark only the bounds stale, and InvalidateLayoutCache should run only when the text, the DPI or the client size changes. 2) Bound the box. Compute heightDip as min(textHeight, kTooltipPreferredTextHeightDip, clientHeight - 2*margin) + 2*padding. Create the layout …
- **Test**: Add a Rendering/Grid test. Use a 600x400 DIP host with a multiline Grid cell holding RepeatToUnits(words, 100000u). Hover the cell, then send 10 more mouse moves inside it, 1 DIP apart, painting after each. (a) Assert that DebugGetBoundsDip(host) lies fully inside GetClientBoundsDip() and that its h …

### grid-multiline-2

**Grid Marquee band is painted outside its track, over the previous column, and stays there under reduced motion**  
`src/Controls/DxUi.Grid.cpp:477` · #28 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ComputeMarqueeFillRect starts the band one band-width left of the track and clamps only the right edge, never the left. Paint fills that rectangle as is. For the first band/travel fraction of every 1.4 s cycle, the band covers the cell's 8-DIP padding and runs into the column painted before it. Under reduced motion, Paint passes animationTickMs = 0, so the band rests at cyclePosition 0, entirely outside the track. The track then looks empty (no sign of activity) and a static bar sits on the previous column. #28 made the ProgressBar's indeterminate segment rest centred and clamped under reduced motion. The Grid's indeterminate Marquee cell is the same class of defect, and it was not covered. The gallery uses a determinate Marquee (progress 0 …
- **Failure**: Turn on reduced motion and show a 170-DIP column of indeterminate Marquee cells (progress &lt;= 0). The track is 154 DIP, so the band is max(12, 0.32*154) = 49 DIP. Each Marquee cell shows an empty track, plus a 49-DIP accent bar from 41 DIP inside the previous column to the track's left edge, painted over that column's text. Without reduced motion the bar sweeps through the previous column for about 24% of every cycle.
- **Fix**: In ComputeMarqueeFillRect, clamp both edges: `const float bandLeft = std::max(trackRect.left, left); const float bandRight = std::min(trackRect.right, left + bandWidthDip); return D2D1::RectF(bandLeft, trackRect.top, std::max(bandLeft, bandRight), trackRect.bottom);`. For reduced motion, pass a resting phase that centres the band (for example tick = kMarqueeCycleDurationMs / 2) instead of 0. Do this only for the Marquee path, since the spinner and sort-glyph code also read animationTickMs. Alter …
- **Test**: Add a rendering test with reduced motion enabled and a two-column Grid. Column 0 is a Text cell with empty text on a known background. Column 1 is an indeterminate Marquee cell (progress 0), 170 DIP wide. Render to a WIC bitmap and assert two things: 1. No pixel of the progress fill colour appears l …

### grid-multiline-3

**Copying multiline cells writes raw CR/LF and TAB into the TSV, so a pasted row breaks apart; the #36 test locks this in**  
`src/Controls/DxUi.Grid.cpp:4491` · #36, #22 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: BuildSelectionTsv joins cells with '\t' and rows with "\r\n", and appends each cell's value unquoted. Multiline cells (the feature #22 hardened) normally contain CR LF, LF, U+2028 or tabs. Spreadsheets and other TSV readers then cannot tell a line break inside a value from the row separator. TestGridCopyOfTrimmedMultilineCellsIsExact (#36) asserts this unquoted form unit for unit. So the test guards the ambiguous output, and the copy contract in the plan reads as satisfied while a paste into Excel or Sheets is wrong.
- **Failure**: Select the row {"Première ligne\r\nDeuxième ligne", "Un Deux", "x"}, press Ctrl+C and paste into Excel. The single row becomes two rows: "Deuxième ligne" lands in column A of a new row, and every later column shifts. A value holding a TAB likewise moves the following cells one column right.
- **Fix**: In BuildSelectionTsv, write each cell through a small quoting helper instead of appending BuildGridCellCopyText directly. If the cell text contains '\t', '\r', '\n' or '"', emit '"' + the value with every '"' doubled + '"'. Otherwise emit the value unchanged. U+2028 and U+2029 do not need quoting, because spreadsheet readers do not treat them as row separators. Keep the CR LF row separator and the TAB column separator. If the full value must also be available without quoting, also publish an HTM …
- **Test**: Add TestGridCopyQuotesFieldsWithSeparators. Use a TextTableModel with one row: {L"a\r\nb", L"x\ty", L"say \"hi\"", L"plain", L"u v"}. Select the row, press Ctrl+C, and require the clipboard to equal `"a\r\nb"` TAB `"x\ty"` TAB `"say ""hi"""` TAB `plain` TAB `u v`, with no trailing separator. Also pa …

### grid-multiline-4

**Single-line cells still break on line-separator characters: a trailing CR/LF shifts and clips the caption, and U+2028/U+2029/NEL/VT/FF hide later lines with no tooltip**  
`src/Controls/DxUi.Grid.cpp:2538` · #29, #22 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The 26 September review fixes made multiline cells drop trailing CR/LF, U+2028 and U+2029 (FindCellTextContentEnd), because those caused a false ellipsis or a half-line offset. The single-line path that #29 rewrote is still exposed to the same defect. PrepareSingleLineCellLayout shapes the whole caption with a NO_WRAP, vertically CENTRED format, and NO_WRAP still breaks at every paragraph separator. DrawCellText then draws the result clipped to the text rectangle. A value ending in CR LF becomes a two-line block centred on the cell: the real line moves up half a line and its ascenders are clipped. The hover check only looks for '\r' and '\n', although the same function family lists all seven break characters (:2477, IsCellTextLineBreak). A …
- **Failure**: A single-line cell (multiline=false) is fed "Ready\r\n" from a log or file, in a 32-DIP row (26-DIP text rectangle, Body line about 18.6 DIP). The two-line block is centred at an offset of -5.6 DIP, so "Ready" sits about 9 DIP above its normal baseline with its top clipped. Separately, the value "Un Deux" shows only a vertically displaced "Un". IsSingleLineCellTextClipped finds no '\r'/'\n' and the widest line fits, so hovering offers nothing and "Deux" can only be reached through copy or UIA.
- **Fix**: In PrepareSingleLineCellLayout, shape and key `const std::wstring_view text = std::wstring_view(cellData.text).substr(0, FindCellTextContentEnd(cellData.text));` and return nullptr when the result is empty, so trailing breaks and blank trailing lines never add an empty centred line. The cache comparison `candidate.text == text` and the hash then use the trimmed view consistently. In IsSingleLineCellTextClipped, run the same trim and replace the CR/LF test with `std::ranges::any_of(content, IsCel …
- **Test**: Add a single-line Grid test in DxUiTests.Grid.cpp with multiline=false, a 32-DIP row and a wide column. (a) Value L"Un Deux". Hover the cell and assert that a tooltip equal to the full value is shown. This fails at HEAD, because :2538 misses U+2028 and the widest line fits. Repeat for U+2029, U+0085 …

### grid-selection-lifetime-11

**Grid draws no focus indicator: a focused grid with no selection looks unfocused, and the current row of a multi-selection cannot be told apart**  
`src/Controls/DxUi.Grid.cpp:2823` · #35 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Grid paints focus only as a change of selection fill (selectionFill versus selectionInactiveFill). DxUi.Grid.cpp never calls PaintFocusRing, while every other focusable control does, and Tree (#35) gives the current item its own focus ring. As a result, a focused grid with an empty selection (after a reload, or after the deletion described above) shows no focus at all. With several rows selected, the user cannot see the row where Up, Down and Shift will start, which matters more given that Ctrl+A moves the current row to the end. This fails WCAG 2.4.7 (Focus Visible), and high-contrast themes depend on the ring.
- **Failure**: The user tabs into a grid whose selection is empty: no row and no border change. The user Shift+clicks rows 3-10, then presses Shift+Up or Down. Which row moves is invisible, so the result looks random.
- **Fix**: Add `std::optional&lt;uint64_t&gt; _currentRowId` to Grid (or to GridSelectionModel). Set it in SelectRow, in Ctrl+Space and Ctrl+arrow handling, in Ctrl+A and on clicks. Clear or remap it in PreserveOrdered or on reload when the row disappears. Use it in place of `GetOrderedSelection().back()` at Grid.cpp:3955 and in OnContextMenu. Add `bool current` and `bool keyboardFocused` parameters to ResolveGridRowVisuals, with Tree's rule `showFocus = current && (keyboardFocused || (theme.highContrast & …
- **Test**: Add a Controls test that builds a Grid with 5 rows and no selection, gives it keyboard focus through the test host with keyboard focus visible, and paints it. 1. Through the resolved-visuals hook (as Tree's `out.showFocus` / `focusArgb` at Tree.cpp:753-754) or a pixel probe, assert that a focus stro …

### grid-selection-lifetime-12

**Columns beyond the viewport cannot be scrolled into view by keyboard, horizontal wheel or Shift+wheel**  
`src/Controls/DxUi.Grid.cpp:4136` · pre-existing · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Left and Right are used only for group collapse and expand. Every other horizontal key falls through to `default: return false`. OnMouseWheel scrolls only vertically, and the window host does not route WM_MOUSEHWHEEL to the grid. The horizontal offset changes only through a press on the horizontal scrollbar's track or thumb, so off-screen columns are reachable only with a mouse. This predates the window.
- **Failure**: A grid whose columns are wider than its viewport, used with a keyboard or with a touchpad's two-finger horizontal swipe. Nothing brings the right-hand columns into view. A sighted keyboard user cannot read those cells.
- **Fix**: 1. In `Grid::OnKeyDown`, when Left or Right was not consumed as a group gesture, scroll `_horizontalScrollDip` by one column boundary, or by about 3 rows' worth of DIP. Then call `ClampScrollOffsets()` and Invalidate, and return true only if the offset changed so the host can still move focus. Optionally map Ctrl+Home and Ctrl+End to the first and last column. 2. In `Grid::OnMouseWheel`, when `modifiers` contains Shift, or when there is no vertical extent but there is horizontal extent, apply th …
- **Test**: Add a test in the Grid control suite with 10 columns of 200 DIP each in a 400-DIP-wide grid, with no groups. 1. Call `OnKeyDown(VK_RIGHT, 0)` and assert that the horizontal offset (read through a debug accessor or the layout metrics at Grid.cpp:1532 `metrics.horizontalScrollDip`) is greater than 0. …

### grid-selection-lifetime-19

**Every key press scans all rows, Right scans all rows once per collapsed group, and Ctrl+C scans all groups for each selected row**  
`src/Controls/DxUi.Grid.cpp:4083` · #43 · performance · confirmed (batch: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #43 made paint independent of the selection size, but the keyboard and copy paths still grow with the list. OnKeyDown builds CollectVisibleRowIndices, an O(rows) allocation, for every key, including keys it does not handle. For each collapsed group, findAssociatedCollapsedGroup calls FindNearestVisibleRow, which allocates and fills the whole visible-row list again, so one Right press costs O(collapsed groups × rows). BuildSelectionTsv calls IsRowVisibleByGroupLayout (O(groups)) and EnsureColumnWidths for every selected row. All of this runs inside the noexcept HandleMessage.
- **Failure**: 1,000,000 rows in 500 collapsed groups: each Right press allocates about 500 × 8 MB and pushes about 5×10^8 indices, hanging the UI for seconds. With 1,000,000 rows in 5,000 groups, Ctrl+A then Ctrl+C makes about 2.5×10^9 group comparisons before the clipboard is written.
- **Fix**: Compute the visible-row list once per key, and only for keys that need it (handle Ctrl+A and Ctrl+C before building it). Change FindNearestVisibleRow to work from group bounds: the first row at or after an index that is not inside a collapsed group can be found by walking or binary-searching the sorted groups without materializing rows. In findAssociatedCollapsedGroup, derive the associated row from the next group boundary. Replace the `ranges::find` with an ordinal computed from the group layou …

### grid-selection-lifetime-3

**Collapsed grid groups cannot be reached by keyboard or UI Automation; with every row grouped and collapsed, the grid ignores all keys**  
`src/Controls/DxUi.Grid.cpp:3938` · #60 · accessibility · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: OnKeyDown returns false before the group gestures whenever no row is visible. When every row belongs to a collapsed group, no key reaches toggleGroupFromKeyboard, so Right cannot expand anything. Where some rows are visible, Right expands a collapsed group only when the current row is the first visible row after that group (findAssociatedCollapsedGroup), and nothing tells the user that. UI Automation gets no element for group headers: the snapshot lists only `VisibleBodyItem::Kind::Row` items, and DxUi.Accessibility.cpp has no group or ExpandCollapse support for grids. Meanwhile GridRowCount and GridItem.Row count the hidden rows. This predates the window, but #57 and #60 changed these same group-collapse paths.
- **Failure**: A Favorites/Folders/Drives grid in which every row is grouped. The user collapses all groups with the mouse, or the application restores a saved layout with ApplyGroupLayout where all groups are collapsed. A keyboard-only user who tabs into the grid gets nothing from any key, because OnKeyDown returns false. Narrator announces a table whose RowCount is the number of model rows but which contains no row and no group element, and it has no way to expand a group.
- **Fix**: Minimal fix: in Grid::OnKeyDown, when `visibleRows.empty()` and `! groups.empty()`, handle VK_RIGHT, VK_RETURN and VK_SPACE (and optionally VK_HOME) by calling the existing `toggleGroupFromKeyboard(firstCollapsedGroupIndex, false)`. To do that, move the empty check after the lambdas are declared, or factor the toggle into a member function. Proper fix: give the grid a keyboard current item that can be a group header (VisibleBodyItem index). On a header, Left/Right collapse/expand it, Up/Down ste …
- **Test**: Add a Controls test with a grouped model of 2 groups x 3 rows, all rows grouped, and a delegate that applies OnGridGroupToggled to the model. Collapse both groups with ApplyGroupLayout({{g1,true},{g2,true}}), focus the grid, and send VK_RIGHT through OnKeyDown. Assert that the call returns true and …

### grid-selection-lifetime-4

**UIA AddToSelection on an already-selected grid row deselects it (implemented as Ctrl+click toggle)**  
`src/Controls/DxUi.Accessibility.cpp:8803` · #57, #60 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ExecuteAddToSelectionOnWindowThread maps a grid row's AddToSelection to RequestSelectRow(rowIndex, MK_CONTROL). In Extended mode, SelectRow treats Ctrl as GridSelectionModel::Toggle. Toggle removes a row that is already selected. UIA requires AddToSelection on a selected item to do nothing. In this same window the Tree was fixed and tested for exactly this ("AddToSelection is not a toggle"), and its contract says "adding a selected item changes nothing". The Grid path still has the same defect.
- **Failure**: An Extended grid has rows A and B selected. A UIA client (an assistive technology or a test or automation script that 'ensures' an item is selected) calls SelectionItem.AddToSelection on B. B is removed from the selection, OnGridSelectionChanged fires, and ElementRemovedFromSelection is raised, all reported as S_OK. A second call adds B back.
- **Fix**: Add `bool Grid::RequestAddRowToSelection(size_t rowIndex)` next to `RequestRemoveRowSelection` (DxUi.Grid.cpp:2112): - Validate exactly as `RequestSelectRow` does: model present, index in range, `FindVisibleRowOrdinal`. - If `_selectionModel.IsSelected(_model-&gt;GetStableRowId(rowIndex))`, return true without touching the model or calling the delegate. - Otherwise, call `SelectRow(rowIndex, _selectionMode == GridSelectionMode::Single ? 0u : MK_CONTROL)`. The row is not selected at this point, s …
- **Test**: Extend the Grid AddToSelection test in Tests/Controls/DxUiTests.Accessibility.cpp (about line 4678), using the Tree test at 4132-4134 as the model: 1. After rows 0 and 1 are selected through AddToSelection, record the delegate's selection-changed count. 2. Call `secondSelectionPattern-&gt;AddToSelec …

### grid-selection-lifetime-5

**Any data refresh reorders a Ctrl+click selection into row order, moving the keyboard position and UIA focus and firing OnGridSelectionChanged with nothing changed**  
`src/Controls/DxUi.Grid.cpp:4537` · #43, #53 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: NotifyDataChanged and SetModel always call ReconcileSelectionForVisibleRows. That calls PreserveOrdered with the visible row ids, which rebuilds the selection in row order. Because the current row is back() of that order (see the Shift+Up finding), a refresh that changes no rows still moves the current row to the bottom-most selected row. EqualRowSelection compares the ordered vectors, so the delegate is told the selection changed. #53's event code already recognizes a reorder-only change as 'the same items in another order' and raises no selection event, but the focus change is still announced.
- **Failure**: Ctrl+click row 10, then Ctrl+click row 3: the ordered selection is [10,3] and the current row is 3. The application refreshes with no row change, or sorts the column (NotifyDataChanged). The selection becomes [3,10]: OnGridSelectionChanged fires although no row joined or left, the UIA focused row moves from 3 to 10 (Narrator announces row 10), and the next Down key selects row 11 instead of row 4. After a sort, the current row becomes whichever selected row now sorts last.
- **Fix**: Choose one of these two fixes. 1. **Smallest change.** In ReconcileSelectionForVisibleRows, capture `const std::optional&lt;uint64_t&gt; currentId = selection.back()` before calling PreserveOrdered. If that id is still selected afterwards, rotate it back to the end of `_selectedRowIds`; `_sortedRowIds` does not change, because the membership is the same. Expose this as a GridSelectionModel method, for example `PreserveOrdered(orderedRowIds, std::optional&lt;uint64_t&gt; keepLast)`, so that both …
- **Test**: Add a Grid interaction test in Tests/Controls/DxUiTests.Grid.cpp with a 20-row flat model and a counting delegate. The steps and checks: 1. Ctrl+click row 10, then Ctrl+click row 3. 2. Assert GetPrimarySelectedRow() == 3. Reset the delegate's selection-change count. 3. Call NotifyDataChanged() with …

### grid-selection-lifetime-6

**Every accessibility publish costs O(selected rows) model lookups: #43 made paint flat, but a 1M-row Ctrl+A still makes each resize step or focus change scan the whole selection**  
`src/Controls/DxUi.Accessibility.cpp:1911` · #43 · performance · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The window-host snapshot is built on every publish, whether or not an assistive technology is listening: the target is registered when the host attaches, and OnSize, SetFocusControl and a handled mouse-up all publish. For each selected id it calls IGridModel::FindRowByStableId and copies the id into selectedGridRowIds. The #43 contract says what a Grid asks per paint must not grow with the selection, but the publish after every gesture, and on every WM_SIZE, still grows with it. IGridModel documents no complexity for FindRowByStableId, so a model with a linear lookup makes this quadratic. SnapshotGridRowIsSelected then answers each row's IsSelected by a linear scan of that vector.
- **Failure**: Press Ctrl+A on a 1,000,000-row grid, then resize the window. Each WM_SIZE publish calls FindRowByStableId 1,000,000 times and allocates an 8 MB vector. With a hashed lookup that is tens of ms per resize step, and with a linear lookup the UI hangs. Every focus change and thumb-drag release pays the same cost. Narrator reading rows pays an O(n) scan per IsSelected query.
- **Fix**: 1. Stop calling FindRowByStableId for every selected id on each publish. Look up indexes only for ids the snapshot materializes: the visible rows already have them, and the offscreen rows are capped by AccessibilityOffscreenSelectedRowMaterializationLimit. Copy the selection ids without validating them, or validate them lazily on the window thread when GetSelection is called and drop ids that have left the model at that point. 2. Store an ascending copy of the selected ids in the record. You can …
- **Test**: Add an Accessibility (or grid-selection benchmark) test with a counting IGridModel: 1,000,000 rows, with FindRowByStableId incrementing a counter. Attach the Grid to a WindowHost, select all rows (SetRange over every id), reset the counter, then send one WM_SIZE (or call RefreshWindowHostAccessibili …

### grid-selection-lifetime-7

**After Ctrl+A on a grid of more than about 280 rows, UI Automation focus moves to a row element the snapshot does not hold, so it has no name or control type**  
`src/Controls/DxUi.Accessibility.cpp:1917` · #53, #43 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Ctrl+A calls SetRange(allRows, front, back). That makes the last visible row the primary row, and the grid does not scroll to it. The window host's focused fragment for a grid is GetPrimarySelectedRow(), i.e. that last row. The snapshot holds rows on screen plus at most 256 selected rows off screen, taken in selection (row) order. The last row of a long list is therefore never among them. The focus move is announced (focusMoved compares gridRowId) on a GridRow provider whose GetPropertyValue finds no row record and returns S_OK with an empty value. So Narrator lands on a nameless element that has no control type. The keyboard position moves to the end of the list as well: the next Down collapses the selection to the last row and scrolls the …
- **Failure**: A 1,000-row grid scrolled to the top, rows 0-24 visible, current row 10. The user presses Ctrl+A. The publish materializes rows 25-280, the focused fragment becomes row 999, and AnnounceWindowHostFocus raises FocusChanged on GridRow(999). Its Name and ControlType come back empty, so Narrator says nothing useful, and Inspect shows a focused element with no name.
- **Fix**: Two changes. (a) Always put the focused row in the snapshot. In the Grid branch of the snapshot builder (around Accessibility.cpp:1908), resolve GetPrimarySelectedRow() first. If that row is neither visible nor already materialized, materialize it before the capped loop, or exempt it from the 256 cap, and skip it inside the loop. The focused fragment must always resolve to a record. (b) Keep the user's row as the current row on Ctrl+A. In OnSelectAll, pass the current primary row instead of allR …
- **Test**: Add a Controls accessibility test. Build a WindowHost grid with Multiple selection and 1,000 rows (or keep 300 rows and lower the limit with DebugSetAccessibilityOffscreenSelectedRowMaterializationLimitForTest(8)), scrolled to the top. Select row 10, focus the grid, send Ctrl+A, then publish the sna …

### grid-selection-lifetime-8

**In an embedded host, moving between grid rows (or tree items) raises no UI Automation focus event, and gaining focus announces the grid rather than its row**  
`src/Controls/DxUi.Accessibility.cpp:9867` · #53 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: For window hosts, the host compares the focused fragment, including the grid row or tree item, between snapshots and announces the item. For embedded hosts, PublishWindowHostAccessibilitySnapshot skips focusMoved, and RaiseEmbeddedAccessibilityChanges raises FocusChanged only when a control's own `controlHasFocus` flips. It raises that event on the control's element. GetFocus, however, returns the GridRow or TreeItem fragment. As a result, row-to-row keyboard movement inside a focused grid or tree raises no focus event in an embedded view, and the focus event raised when the grid gains focus names a different element from the one GetFocus reports. For Grid, only #53's selection events reach the client. For a multi-select tree, Ctrl+Up/Down …
- **Failure**: An embedded view (RedXe) containing a focused Grid, with Narrator running. The user presses Down. The current row changes, `controlHasFocus` stays true, and RaiseEmbeddedAccessibilityChanges raises no FocusChanged, so Narrator's focus stays on the previous row and only an ElementSelected arrives. In a multi-select Tree, Ctrl+Down raises no event at all, so the screen reader is silent while the focus ring moves.
- **Fix**: In RaiseEmbeddedAccessibilityChanges, after the per-record loop: 1. Compute `const bool focusMoved = current-&gt;focusedFragment.has_value() && ! SameFocusedElement(*previous, *current);`. This is safe for an embedded view: when placement.hasKeyboardFocus is false, focusedFragment is already reset. 2. When `focusMoved` is true and `connected()` holds, build the provider the way AnnounceWindowHostFocus does: TreeItemTag for TreeItem, GridRow for GridRow, path-only for Control, each with hwnd = nu …
- **Test**: Write an embedded-host UIA test. Attach accessibility with hasKeyboardFocus = true and register a FocusChanged handler, either in-process or by counting through the existing test notification hooks. Run three cases: (a) A focused Grid with rows 0..3 and row 0 selected. Send Down, prepare, then call …

### grid-selection-lifetime-9

**UI Automation SetFocus on a grid row or cell replaces a multi-row selection with that one row**  
`src/Controls/DxUi.Accessibility.cpp:8468` · #57, #60, #35 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ExecuteSetFocusOnWindowThread handles a GridRow or GridCell by calling RequestSelectRow(rowIndex, 0u). With no modifiers, SelectRow calls SetSingle. A client that only moves focus to a row therefore throws away the user's multi-selection and fires OnGridSelectionChanged. Tree (#35) deliberately moves only the focus in this case (`tree-&gt;SetFocusedItemId`). The accessibility spec (line 119) documents that grid focus selects, but it does not say this destroys a selection of several rows.
- **Failure**: The user selects 50 rows (Shift+click or Ctrl+A) to delete them. A screen reader whose cursor moves system focus (Narrator with cursor/focus sync, or any client calling IUIAutomationElement::SetFocus) moves onto one row to read it. The grid now selects only that row and OnGridSelectionChanged fires, so the application's preview and status update. When the user presses Delete, it applies to one row.
- **Fix**: This is the minimal fix that preserves behavior; it needs no new focused-row state. In ExecuteSetFocusOnWindowThread, for GridRow and GridCell, after resolving rowIndex: - If `grid-&gt;IsRowSelected(rowIndex)` (public API at include/DxUi/DxUi.h:3649), skip RequestSelectRow. Under the grid's current model that row already reports HasKeyboardFocus, so only do host-&gt;SetFocusControl(grid), the survived() checks, the snapshot refresh, ::SetFocus and Invalidate. - Otherwise keep the current Request …
- **Test**: Add an Accessibility test with a Multiple-selection Grid of 5 rows in a WindowHost and a counting delegate: 1. Select rows 1, 2 and 3 with RequestSelectRow(1,0), then RequestSelectRow(2,MK_CONTROL) and RequestSelectRow(3,MK_CONTROL). Reset the delegate's change counter. 2. Get the IRawElementProvide …

### host-core-2

**UIA Toggle/Value/RangeValue/Select actions run focus changes and app callbacks while holding the process-wide accessibility mutex**  
`src/Controls/DxUi.Accessibility.cpp:8601` · #29, #57 · threading · confirmed (trace: confirmed/medium; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: PR #29 changed ExecuteSetFocusOnWindowThread so the global recursive `GetAccessibilityTargetMutex()` only guards host resolution. ExecuteInvokeOnWindowThread already worked that way ("Validate the target while locked, then run the application callback after releasing the accessibility mutex"). ExecuteToggleOnWindowThread still holds the lock for the whole action: `toggle-&gt;OnMnemonic(*host)` now runs FocusControlAndSurvive(host, *this, true) from #57, which calls ::SetFocus(hwnd), SetFocusControl, the app's focus-changed callback, and then ApplyCheckedState, which runs the app's toggle callback. ExecuteSetStringValueOnWindowThread (SetTextAndNotify), ExecuteSetRangeValueOnWindowThread (Slider::RequestValue) and ExecuteSelectOnWindowThread …
- **Failure**: A Narrator user toggles a DxUi checkbox (Toggle pattern), and the app's onToggled callback opens a DxUi confirmation dialog with a modal loop. The UI thread now holds the global accessibility mutex for as long as the dialog is open. Every UIA call from Narrator's worker threads into any DxUi provider blocks on the mutex (get_ProviderOptions, AcquireWindowHostAccessibilityTarget), so Narrator cannot read or operate the dialog until it is closed blind. If the app has a second UI thread, any RefreshWindowHostAccessibilitySnapshot on that thread blocks on the mutex, freezing that window. If the ca …
- **Fix**: Give every Execute*OnWindowThread that runs control or application code the Invoke/SetFocus shape: Toggle, SetStringValue, SetRangeValue, Select, AddToSelection, RemoveFromSelection and Expand/Collapse. 1. In a scoped block holding GetAccessibilityTargetMutex(), resolve host and control (or grid/tree and row/column indexes), validate the pattern and the enabled/read-only state, and capture GetControlLifetimeToken(*control). 2. Release the lock, then call OnMnemonic, RequestToggleCheckboxCell, Se …
- **Test**: Add a Controls accessibility test. 1. Build a WindowHost with a Checkbox whose onToggled callback signals event A, then waits (bounded, for example 5 s) on event B. 2. Obtain the checkbox's IToggleProvider and call Toggle() from a worker thread, so it dispatches to the UI thread. The UI thread pumps …

### host-core-4

**Stale-capture cancel only runs at HandleMessage entry; SetFocusControl and other prune paths still drop a disabled/hidden capture without OnCaptureLost**  
`src/Controls/DxUi.WindowHost.cpp:4139` · #29 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The prior review's fix (ReviewFixes_2026-09-29) added CancelStaleCapture, but it runs only at the top of HandleMessage. PruneStaleInteractionState is also called directly from the public SetFocusControl (line 1615), OnSetFocus/OnKillFocus, UpdateHover and the key/char re-validation paths. For a capture that is still in the tree but no longer effectively interactive, it still clears `_capturedControl` and calls ReleaseCapture silently. The WM_CAPTURECHANGED this sends finds no captured control, so OnCaptureLost never runs. The fix is therefore incomplete: whether a drag is canceled depends on whether the app's disable happens just before a message boundary or just before a SetFocusControl call.
- **Failure**: A user mouse-drags a Slider (Slider::OnMouseDown sets `_dragging = true` and captures). An app accelerator (WM_COMMAND), timer or the slider's own Preview callback hides its page (`tabControl-&gt;SelectTab(...)` → `SetVisible(false)`) or disables its panel and calls `host.SetFocusControl(other)`. SetFocusControl → PruneStaleInteractionState sees the captured slider is not effectively interactive, nulls the capture and calls ReleaseCapture. Slider::OnCaptureLost never runs, so `_dragging` and `_touchDragging` stay true and the value is not reverted. When the page is shown or re-enabled again, m …
- **Fix**: Fold `CancelStaleCapture` into the capture branch of `PruneStaleInteractionState` and delete the separate function: - Resolve `ControlInteractionState` for `_capturedControl`. - If it is not effectively interactive, take the lifetime token only when `state.inTree`, null `_capturedControl`, and call `ReleaseCapture` if the window holds capture. - Then, only if the control was in the tree and `RevalidateDispatchedControl(lifetime, _root.get(), captured)` still returns a live control, call `live-&g …
- **Test**: Add a WindowHost test next to `TestWindowHostDisabledOrHiddenCaptureCancelsTheDrag`: 1. Start a Splitter (or Slider) drag through `OnMouseDown`/`OnMouseMove` with the host capturing it. 2. Disable the splitter, or hide its parent pane. 3. With no `HandleMessage` call in between, call `window.Host(). …

### host-core-6

**#29 stale-capture cancel never reaches controls dragged inside a ScrollPanel: a disabled or hidden slider/splitter keeps dragging and commits**  
`src/Controls/DxUi.Controls.cpp:8948` · #29 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: CancelStaleCapture (and EmbeddedHost::CapturedDragContinues) judges only the host's _capturedControl. Inside a ScrollPanel that is the ScrollPanel itself: its HitTest returns `this`, and after a child handles the press it calls host.CaptureMouse(this) and keeps the real target in _innerCapturedChild. ScrollPanel forwards OnMouseMove/OnMouseUp to _innerCapturedChild and re-validates it only for lifetime and branch membership, never for enabled/visible. So a child (or an ancestor between it and the ScrollPanel) that becomes disabled or hidden mid-drag is never cancelled. Slider::OnMouseMove/OnMouseUp do not check IsEnabled, so the inert control keeps changing its value and commits on release. Scrollable forms and settings pages are the usual …
- **Failure**: A settings page lays a Slider inside a ScrollPanel. During a drag, the app disables the slider (from its Preview callback, or because a device disappeared). On the next WM_MOUSEMOVE, CancelStaleCapture finds the captured ScrollPanel interactive and does nothing. ScrollPanel::OnMouseMove forwards to the disabled slider, which keeps emitting Preview values. On WM_LBUTTONUP, ScrollPanel::OnMouseUp forwards to Slider::OnMouseUp, which commits the new value on a disabled control. The same control directly under a Panel is cancelled and restored (TestWindowHostDisabledOrHiddenCaptureCancelsTheDrag c …
- **Fix**: Fix it in ScrollPanel, the narrowest place. 1. Add a helper that resolves the inner capture: if `_innerCapturedChild` is no longer in the branch, or `! IsControlEffectivelyInteractive(this, _innerCapturedChild)`, clear it and call `child-&gt;OnCaptureLost(host)` with a lifetime guard. If the panel has no thumb drag of its own, also call `host.ReleaseMouseCapture()`. 2. Call this helper at the top of OnMouseMove, OnMouseUp and OnMouseWheel (the 9180 path) before forwarding. If the child was cance …
- **Test**: Copy TestWindowHostDisabledOrHiddenCaptureCancelsTheDrag and put a ScrollPanel between the root and the splitter (or a slider). - Start the drag through the panel: `scrollPanel-&gt;OnMouseDown(host, press, ...)`. Check that `host.GetCapturedControl() == scrollPanel` and that the splitter is dragging …

### host-core-7

**OnDpiChanged never republishes the UIA snapshot, so a child-window host keeps stale pixelsToDipScale and bounds after a per-monitor DPI change**  
`src/Controls/DxUi.WindowHost.cpp:2368` · pre-existing · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: A window-host snapshot stores pixelsToDipScale = 96 / host.GetDpi() and the control bounds in DIPs at publish time. UIA BoundingRectangle and ElementProviderFromPoint convert with that stored scale. OnDpiChanged updates _dpi, re-lays out the root (OnHostDpiChanged + SetBounds) and invalidates, but never calls RefreshWindowHostAccessibilitySnapshot. For a child HWND host, the parent resizes the child inside its own WM_DPICHANGED, before WM_DPICHANGED_AFTERPARENT reaches the child. OnSize therefore runs with the old _dpi, lays out and publishes with the old scale. Then WM_DPICHANGED_AFTERPARENT reaches OnDpiChanged, which corrects _dpi and the layout without republishing. Top-level hosts are fixed only because SetWindowPos usually triggers an …
- **Failure**: A DxUi pane hosted in a child HWND is moved from a 100% to a 150% monitor. The parent's WM_DPICHANGED relayout sends WM_SIZE (new pixel size, old _dpi): the root is laid out 1.5x too wide in DIPs and published with scale 1.0. WM_DPICHANGED_AFTERPARENT then fixes _dpi and the layout but publishes nothing. Until some unrelated publish (a focus move or state change), Narrator's focus rectangle and BoundingRectangle are about 2/3 size and misplaced, and touch exploration (ElementProviderFromPoint maps pixels with the old scale) reports the wrong control.
- **Fix**: In ControlHost::OnDpiChanged, after the root and tooltip relayout, add `if (_root) RefreshWindowHostAccessibilitySnapshot(_hwnd, this);`. Put it after the SetWindowPos block, so that a top-level host whose WM_SIZE has already republished just publishes the same content again. Check that this does not raise a duplicate focus event: SameSemanticControls and focus-diff logic should suppress events when the semantic set is unchanged. Optionally, in OnSize, sync `_dpi` from GetDpiForWindow(_hwnd) whe …
- **Test**: Add a WindowHost test with a WS_CHILD host at 96 DPI that has a root with one focusable Button at known DIP bounds. Publish once and get the button's UIA BoundingRectangle (width W). Then: 1. Send WM_SIZE with a 1.5x pixel size, which mimics the parent's relayout at the old DPI. 2. Make WM_DPICHANGE …

### host-core-8

**PageHost pages are reparented with no parent, so they never inherit the PageHost's flow direction or density although the PageHost forwards those changes to them**  
`src/Controls/DxUi.Controls.cpp:1380` · #31 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #31 rewrote PageHost::SetPage to `page-&gt;Reparent(nullptr, GetHost())`. Reparent's own comment says 'The flow direction and density a control inherits resolve through its parent chain'. A page's _parent is nullptr, and GetFlowDirection/GetDensity are non-virtual walks of _parent: a page resolves LeftToRight and the host theme's density, whatever the PageHost and its ancestors use. PageHost::OnFlowDirectionChanged and OnDensityChanged forward to the pages, so inheritance is clearly intended, but the forwarded pages re-read the same parentless values. Reparent therefore also never announces a difference when a page enters an RTL or compact PageHost. This predates the window, but #31 rewrote exactly this line while documenting parent-chain i …
- **Failure**: An RTL application sets root-&gt;SetFlowDirection(FlowDirection::RightToLeft) and navigates with a PageHost child: pageHost-&gt;SetPage(std::make_unique&lt;StackPanel&gt;(...)). The page and its controls report IsRightToLeft() == false. They lay out left-to-right, sliders map Left/Right keys and drags LTR (Slider::OnKeyDown/UpdateValueFromPoint use IsRightToLeft), and text input syncs as LTR, while the controls outside the PageHost are mirrored. Likewise pageHost-&gt;SetDensity(Density::Compact) leaves page content at the theme's Standard density.
- **Fix**: Give controls a non-owning inheritance link that is separate from the `Panel*` parent. 1. Add `const Control* _inheritanceParent = nullptr;` to `Control`. 2. In `GetFlowDirection`, when there is no explicit value: `if (_parent) return _parent-&gt;GetFlowDirection(); if (_inheritanceParent) return _inheritanceParent-&gt;GetFlowDirection(); return LeftToRight;`. Do the same in `GetDensity`, falling back to the host theme only after both links are null. 3. Add an overload `Reparent(Panel* parent, C …
- **Test**: Add a control test that fails at HEAD: 1. Build a root Panel with `SetFlowDirection(FlowDirection::RightToLeft)` containing a PageHost. 2. Call `SetPage` with a StackPanel holding a probe control that overrides `OnFlowDirectionChanged` and counts calls. 3. Assert that `probe-&gt;IsRightToLeft()` is …

### menu-layout-ux-2

**#59 edge reveal also runs when a submenu opens from hover or click, scrolling the parent under a stationary pointer without repainting it**  
`src/Controls/DxUi.Menu.cpp:1306` · #59 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #59 changed EnsureItemVisible so that reaching the first or last navigable row also scrolls the padding, headers, separators and Info rows before or after it into view. EnsureItemVisible is not only called for Home/End and arrow wrap. OpenSubmenu also calls it unconditionally, including from the hover timer (HandleSubmenuHoverTimer) and pointer clicks. Before #59 that call did nothing for a fully visible row. Now, when the hovered submenu row is the first or last navigable row, it scrolls the parent whenever the leading or trailing region is off screen. OpenSubmenu then never calls InvalidatePopup on the parent, so the parent keeps showing the old scroll position and its UIA bounds are not resynchronized. The submenu is anchored to the row' …
- **Failure**: An oversized menu starts with a Header and a Separator, and its first command is "Recent &gt;" at content y 37..69. The user wheels down one notch (scrollOffset 32), so "Recent &gt;" is fully visible at the top, and hovers it. 400 ms later OpenSubmenu calls EnsureItemVisible: revealTop = 0 &lt; 32, so scrollOffset becomes 0. The submenu opens 32 DIP lower than the row the user still sees, because the parent is not invalidated. On the next mouse move the pointer hits a different row (or the header). Hover changes and ScheduleSubmenuCloseTimer closes the submenu the user was moving toward. A thu …
- **Fix**: Make the edge reveal opt-in for keyboard navigation only. Change the signature to `void EnsureItemVisible(size_t index, bool revealMenuEdges = false) noexcept` and compute revealTop/revealBottom from FindFirst/LastNavigableItem only when revealMenuEdges is true. Pass true only from RouteMenuKeyboardEvent's Up/Down wrap and Home/End calls (lines 5238, 5251, 5267, 5289). OpenSubmenu, OnFocusChanged (3354) and the DPI relayout (3787) keep the row-only reveal. Also close the pre-existing gap in Open …
- **Test**: In Tests/Controls (the NewControls menu tests that #59 extended), build an oversized menu: a Header, a Separator, then a first command "Recent" with children, followed by enough commands to need a scrollbar. Wheel down one notch so scrollOffsetDip equals one item height and "Recent" is fully visible …

### menu-layout-ux-3

**UIA focus on a menu row leaves the hovered row selected, and a later pointer move clears the keyboard row without repaint or UIA sync, so Enter invokes a row other than the one UIA reports focused**  
`src/Controls/DxUi.Menu.cpp:3351` · #24, #56 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: MenuAccessibilityItem::OnFocusChanged (#24, applied to every menu since #56) sets keyboardIndex but does not reset hoveredIndex, which every keyboard path does. Paint highlights `hoveredIndex == i || keyboardIndex == i`, so two rows look selected. In HandleMenuMouseMoveAtPointDip, a pointer move that stays inside the hovered row resets keyboardIndex but invalidates only when the hit row changed. Nothing repaints, and SynchronizeMenuAccessibility does not run. The host's UIA focus stays on the UIA-focused row while Enter/Space now use hoveredIndex.
- **Failure**: The pointer rests over row 5 of an open menu, and Narrator (or another UIA client) sets focus to row 3. Rows 3 and 5 are both painted highlighted. The mouse moves 1 px within row 5. HandleMenuMouseMoveAtPointDip: hit == hoveredIndex, so changed = false, but keyboardIndex.reset() runs and no InvalidatePopup follows. Row 3 still looks selected and is still UIA focus, but Enter goes through `topmost-&gt;keyboardIndex.has_value() ? ... : topmost-&gt;hoveredIndex` and invokes row 5.
- **Fix**: 1. In MenuAccessibilityItem::OnFocusChanged, add `_popup.hoveredIndex.reset();` next to `_popup.keyboardIndex = _index;`, so the UIA path matches the keyboard paths. 2. In HandleMenuMouseMoveAtPointDip, capture `const bool keyboardCleared = pointerTakesKeyboardFocus && popup.keyboardIndex.has_value();` before the reset. - Repaint and re-sync on that as well: `if (changed || keyboardCleared) InvalidatePopup(popup);`. - Keep the submenu hover-timer block gated on `changed` alone, so a sub-row wigg …
- **Test**: Add a non-interactive Menu test in Tests/Controls/DxUiTests.Menu.cpp, following the existing UIA-focus fixtures around lines 6790-6811. 1. Open a context menu with at least 6 plain rows. 2. Deliver a WM_MOUSEMOVE to the popup at the centre of row 5. Assert via DebugGetContextMenuPopupState that `hov …

### menu-layout-ux-4

**Mnemonic and first-letter keys invoke the first matching row immediately, even when several rows share the key (described menus with repeated labels)**  
`src/Controls/DxUi.Menu.cpp:5380` · #24 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: FindMnemonicItem returns the first navigable row whose explicit '&' mnemonic, or else first non-space character, matches. RouteMenuKeyboardEvent then invokes that row at once. Native Win32 menus move the selection among rows that share a mnemonic and invoke only when the match is unique. The logic predates the window. #24 now supports described menus with repeated primary labels told apart only by their descriptions (the documented destination-menu example). In those menus, the first-letter fallback turns a single key press into a command against an arbitrary entry.
- **Failure**: A destination menu has "Archives familiales" with description D:\Photos and "Archives familiales" with description E:\Backup, both without '&'. The user types 'A' (for example, starting to type ahead, or meaning the second entry). FindMnemonicItem returns the first row and InvokeItem(commandId) runs: the consumer copies or moves to D:\Photos without the user choosing that destination.
- **Fix**: Change FindMnemonicItem so that it searches starting after the current keyboardIndex (or hoveredIndex), wrapping around, and also reports whether more than one row matches. For example, return {first-after-current index, matchCount}. In RouteMenuKeyboardEvent: - If matchCount == 1, keep today's behavior: open the submenu, or invoke the row. - If matchCount &gt; 1, set topmost-&gt;keyboardIndex to the next match and clear hoveredIndex, so the pointer state does not override the selection. Then ca …
- **Test**: Add a Menu test along the lines of TestMenuMnemonicHonorsExplicitAmpersandLabels: 1. Open a popup with three rows: "Archives familiales" (secondaryText D:\Photos, id 4001), "Archives familiales" (secondaryText E:\Backup, id 4002), and "Copier" (id 4003). 2. Send 'A'. Assert that the menu is still op …

### menu-layout-ux-5

**Menu rows' UIA names leave out the accelerator column: Info values, a Slider's current stop and command shortcuts are never exposed**  
`src/Controls/DxUi.Menu.cpp:3379` · #24, #56 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: PopulateMenuAccessibility builds each row's UIA name from the decoded label, plus the description when there is one. DecodeMenuItemText(item).acceleratorText is never used. The row elements do not set an AcceleratorKey, a value or a range value either. For an Info row the accelerator column holds the information itself: the gallery and tests use 'Espace utilisé :' with the value '561 Go'. For a Slider row it holds the current stop's text (UpdateSliderInteractionAtPoint writes item.acceleratorText = sliderStops[stopIndex].text). For a command row it holds the shortcut. Before #24 and #56 these rows had no UIA element at all. Every row now has one, but its name drops what the row shows next to the label.
- **Failure**: A menu has an Info row {text='Espace utilisé :', acceleratorText='561 Go'} and a Slider row 'Taille des miniatures' whose current stop is 'Grande'. Narrator in scan mode reads 'Espace utilisé :' with no value. When keyboard focus lands on the slider row, Narrator says 'Taille des miniatures, text', so the user cannot tell the current size. Command rows are announced without their shortcut, which native Win32 menus announce through AcceleratorKey.
- **Fix**: In PopulateMenuAccessibility, compute `const auto decoded = DecodeMenuItemText(item);` once. - **Info rows:** when accessibleName is empty and decoded.acceleratorText is not empty, add the accelerator text to the default name after the label and before the description, for example "Espace utilisé : 561 Go". Update the accessibleName comment in DxUi.h to match. - **Slider rows:** expose the current stop text as a read-only Value. Use a small proxy control that reports the value through GetControl …
- **Test**: In the menu accessibility tests: 1. Open a popup that has the fixture Info row {text="Espace utilisé :", acceleratorText="561 Go"}. Assert that the row's UIA Name (or Value) contains "561 Go". This fails today. 2. Add a Slider row with stops {"Petite","Grande"}, move keyboard focus to it and press R …

### menu-layout-ux-6

**Wheel and scrollbar scrolling keep the hovered row and its pending submenu timer on a row that has moved away from the pointer**  
`src/Controls/DxUi.Menu.cpp:4980` · #24, #56, #59 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The wheel branch moves scrollOffsetDip and invalidates. It neither hit-tests again at the stationary pointer nor cancels a pending PendingOpen hover timer. Thumb drags (4276) and track paging (5035) behave the same way. hoveredIndex keeps naming a row that is no longer under the cursor. focusedIndex(), Enter/Space and Right all fall back to hoveredIndex when there is no keyboardIndex, and HandleSubmenuHoverTimer still opens the old row's submenu. OpenSubmenu then calls parent.EnsureItemVisible(itemIndex), which scrolls the parent back to the row the user just scrolled away from. The code predates the window, but the window makes it common: described rows (#24) make scrolling ordinary, and #59 widened EnsureItemVisible's reveal. This differs …
- **Failure**: A described menu with maxRootHeightDip scrolls. The pointer rests on submenu parent row 2, then the user turns the wheel 3 notches within 400 ms without moving the mouse. Row 2 scrolls out of view. The hover timer fires, opens row 2's submenu at row 2's new (offscreen) position and scrolls the parent back. Alternatively, after scrolling the user presses Enter, and the row that left the view is invoked instead of the one under the cursor.
- **Fix**: Add a small helper, RefreshPopupHoverAfterScroll(controller, popup). Call it after every change to scrollOffsetDip that comes from the wheel (4980), track paging (5036) or thumb drag (4276). The helper should: - Read the current cursor position with GetCursorPos. Do not use lastPointerScreenPoint, because the wheel point is not always recorded there. - Map the position to popup DIPs with the existing screen-to-popup helper. - If the point is inside the popup, call HandleMenuMouseMoveAtPointDip(c …
- **Test**: Add a test in Tests/Controls/DxUiTests.Menu.cpp next to the scrolled described-menu test. It sends messages only and takes no focus. Setup and actions: - Open an async menu with maxRootHeightDip = 170 and described rows. Include a submenu parent at index 1 or 2 and enough rows to scroll. - SendMessa …

### menu-layout-ux-7

**Mnemonic matching only sees virtual keys 'A'-'Z', so accented and non-Latin labels can never be reached by keyboard**  
`src/Controls/DxUi.Menu.cpp:5420` · #24 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: A key becomes a Mnemonic only when its virtual-key code is 'A'..'Z'. The character compared is static_cast&lt;wchar_t&gt;(virtualKey) (5589), the physical key's VK value, not the character the keyboard layout produces. FindMnemonicItem compares that against the label's mnemonic, or the first letter by ResolveMenuLabelMnemonic. A label whose mnemonic or first letter is É, Ç, a Cyrillic or Greek letter, or a digit never matches. On a Cyrillic layout, pressing the key for 'Ф' produces VK 'A', which can match an unrelated Latin label. WM_CHAR and WM_SYSCHAR, which carry the translated character, are never consulted. The code predates the window, but #24 targets localized (French) descriptions and states that 'primary text keeps existing mnemoni …
- **Failure**: A French menu has rows '&Édition' and 'Été 2026'. On AZERTY, the É key reports VK_2, which is not mapped, so nothing happens. A Russian menu with 'Файл' gets no response to Ф. Instead VK 'A' matches any Latin row whose mnemonic is A.
- **Fix**: Match mnemonics on the translated character instead of the virtual-key code: 1. Remove the 'A'-'Z' to Mnemonic mapping from MenuKeyboardKindFromVirtualKey. Letter key-downs will then fall through to TranslateMessage in the modal loop (line 5964). 2. In ProcessMenuPopupMessage, and in the modal loop when the WM_CHAR is posted to a non-popup window that still has focus, handle WM_CHAR and WM_SYSCHAR as a MenuKeyboardEvent with kind Mnemonic and `mnemonic = static_cast&lt;wchar_t&gt;(wp)`. 3. Ignor …
- **Test**: Add these cases to the Menu interaction suite, using its existing synthetic message-injection path: 1. Build a menu with the rows '&Édition', '&1 Recent' and 'Файл'. Post WM_CHAR L'é' to the popup and assert that '&Édition' becomes the keyboard index (or is invoked). Repeat with WM_CHAR L'1' and wit …

### menu-loop-lifetime-1

**Modal menu loop swallows every WM_TIMER with id 1 on the thread, including the AnimationDispatcher tick**  
`src/Controls/DxUi.Menu.cpp:5862` · #42 · bug · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: RunMenuModalLoop intercepts any WM_TIMER whose wParam is kSubmenuHoverTimerId (1), whatever window it is for, and always `continue`s without dispatching it. The thread's AnimationDispatcher drives every window-mode ControlHost animation from a WM_TIMER with the same id (kTimerId = 1) on its message-only window. Its handler therefore never runs while a synchronous ContextMenu::Show is open. That includes NativeMenuBarHost menus, which use Show. This is pre-existing code in the loop that #42 reworked; nothing in the window fixed it, and the slider tests only use ShowAsync.
- **Failure**: (1) Show a menu that has a Slider item, such as a menu-bar menu with the 'Thumbnail size' slider, and drag it. UpdateSliderInteractionAtPoint changes the label and calls SetSliderAnimationTarget, which calls popup.host.RequestAnimation (Menu.cpp:1620). MenuContentControl::Tick never runs, so sliderAnimatedPosition stays put and the thumb does not move until the menu closes. (2) Any application timer created with SetTimer(hwnd, 1, ...) on the menu's thread, or a TIMERPROC timer with id 1, does not fire while the menu is open. (3) If any host on the thread holds an animation subscription when th …
- **Fix**: Delete the WM_TIMER special case at Menu.cpp:5860-5869. A popup's hover timer is a `popupMessage`, so the existing branch at :5871 translates and dispatches it to MenuWndProc, and ProcessMenuPopupMessage (:5507) runs HandleSubmenuHoverTimer. That is the same route ShowAsync uses. Every other WM_TIMER (the dispatcher's message-only window, application timers, TIMERPROC timers) then reaches the generic TranslateMessage/DispatchMessageW at :5964. If the special case has to stay, narrow it to `msg.m …
- **Test**: Add synchronous-Show tests in Tests/Controls/DxUiTests.Menu.cpp, using the existing pattern of posting probes from a helper thread or the owner while `ContextMenu::Show` blocks: (a) Show a root menu with a Slider item (several sliderStops, reducedMotion=false). Drive a key or pointer change that cal …

### menu-loop-lifetime-3

**Synchronous Show never dismisses on deactivation or WM_CANCELMODE; its dismissal branch is unreachable**  
`src/Controls/DxUi.Menu.cpp:5839` · #42, #29 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The modal loop tries to dismiss on WM_ACTIVATEAPP(FALSE), WM_ACTIVATE(WA_INACTIVE), WM_NCACTIVATE(FALSE) and the owner's WM_DESTROY. It only checks messages that PeekMessageW returned, and those messages are always sent, never posted, so this branch never runs. WM_CANCELMODE and WM_CAPTURECHANGED (also sent) are only traced. MenuWndProc dismisses for these messages only when `controller-&gt;asyncSession`. Show and ShowAsync therefore behave differently, and the idle branch actively re-grabs capture after the system cancelled it. Separately, the idle branch blocks in MsgWaitForMultipleObjectsEx without re-checking `controller.running`. A handler that runs inside PeekMessageW and dismisses (for example a failed DPI reflow from a sent WM_DPICH …
- **Failure**: Open a menu with ContextMenu::Show (for example a NativeMenuBarHost menu), then press the Windows key, or let another application or a toast take the foreground. The root popup receives WM_ACTIVATE/WM_ACTIVATEAPP and WM_CANCELMODE as sent messages. None of them dismisses, DefWindowProc releases capture, and at the next idle pass `if (GetCapture() != currentRoot-&gt;hwnd) SetCapture(currentRoot-&gt;hwnd);` takes capture back. The menu stays open behind the other application, and Show keeps blocking the caller. Clicks in other applications do not reach it. Similarly, if the application shows a m …
- **Fix**: In MenuWndProc, handle activation loss for both session modes. Run `dismissForActivation || dismissForCancel` on any controller with `running` and `! destroyingPopupWindow`, and call `controller-&gt;Dismiss()`. Call FinalizeAsyncMenuController only when `asyncSession && asyncInteractionActive && ! asyncFinalizing`. The synchronous loop exits at its `while (controller.running)` check. Keep the capture-loss dismissal async-only, or gate it on the app having lost activation: the synchronous loop in …
- **Test**: Add a Menu test that calls synchronous ContextMenu::Show on a test owner window. Once the root popup appears (existing popup-appearance probe), use a helper thread to move the foreground to another thread's window. The existing Tests/Support/ForegroundThief.h, or TryActivateDxUiTestWindow on a secon …

### menu-loop-lifetime-4

**Popup mnemonics compare the virtual-key code, not the typed character: non-Latin labels never match and the wrong command can run**  
`src/Controls/DxUi.Menu.cpp:5420` · pre-existing · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Both keyboard routes, the modal loop (5931) and ProcessMenuPopupMessage (5589), build `mnemonic = static_cast&lt;wchar_t&gt;(virtualKey)`, and only VKs 'A'..'Z' are treated as mnemonics. Under Cyrillic, Greek, Hebrew or Arabic layouts, letter keys report the US-position VK ('A'..'Z'), not the character typed. Letters behind OEM keys (é, ü, ß) are never mnemonics. ResolveMenuLabelMnemonic falls back to a label's first character, so Latin items in a localized menu are still reachable through the wrong physical key, and FindMnemonicItem invokes a match immediately. The popup never looks at the translated WM_CHAR. NativeMenuBarHost::ActivateMnemonic, by contrast, uses the translated WM_SYSCHAR character. This is pre-existing behaviour on the mo …
- **Failure**: A Russian user opens a menu containing '&Открыть' and an English item 'Git…' (no '&', so its fallback mnemonic is 'G'). Pressing the key labelled 'П' sends VK_G (0x47): RouteMenuKeyboardEvent's Mnemonic case finds 'Git…' and calls controller.InvokeItem, which runs an unintended command. Pressing 'О' for '&Открыть' sends VK_J, which matches nothing, so the mnemonic shown in the menu does nothing.
- **Fix**: Use the VK route only to classify navigation keys, and resolve the mnemonic from the typed character. Minimal change, inside the existing loop and WndProc: in the default branch, a key qualifies as a mnemonic when it has no Ctrl and no Win modifier and ToUnicodeEx gives exactly one printable character. Call it with the VK, the scan code from (lParam &gt;&gt; 16) & 0xFF, a GetKeyboardState snapshot, and GetKeyboardLayout(GetWindowThreadProcessId(msg.hwnd, nullptr)). Pass flag bit 2 (0x4) on Windo …
- **Test**: Add a test in Tests/Controls/DxUiTests.Menu.cpp that drives the popup without taking focus, using its existing popup-message or RouteMenuKeyboardEvent seams: 1. Popup items '&Открыть' (cmd 1) and 'Git…' (cmd 2, no '&'). Deliver the key 'О' the way the layout would (WM_CHAR U+041E, or a WM_KEYDOWN th …

### menu-loop-lifetime-5

**OpenSubmenu scrolls the parent popup without repainting it or republishing UIA bounds; #59's edge reveal makes this happen for fully visible rows**  
`src/Controls/DxUi.Menu.cpp:4073` · #59 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: OpenSubmenu calls `parent.EnsureItemVisible(itemIndex)`, which can change `parent.scrollOffsetDip`. It then positions the submenu from the new offset, but never calls InvalidatePopup(parent), so neither the repaint nor SynchronizeMenuAccessibility runs. None of its callers invalidate the parent afterwards: the hover-timer path (HandleSubmenuHoverTimer), the pointer LeftUp path, keyboard Enter/Space on a hovered row, and the UIA AccessibleInvoke handler, which also sets `popup-&gt;keyboardIndex = index` without invalidating. Before #59 this only happened for a partially visible row. #59 made EnsureItemVisible reveal everything down to `contentHeightDip` for the last navigable row, and up to 0 for the first. Opening a fully visible first or l …
- **Failure**: A long root menu scrolls (maxRootHeightDip, or content taller than the work area). Its last navigable row is "More ▸", followed by a disabled/Info footer row and the 4 DIP bottom padding. The user scrolls with the wheel until "More" is fully visible but the footer is not, then rests the pointer on "More". After 400 ms the submenu opens. The root silently scrolls by the footer height (about 30 DIP), but its swap chain still shows the old frame. The submenu is placed beside where "More" now is, so it sits that far above the painted row. Hit testing already uses the new offset, so the pointer is …
- **Fix**: In OpenSubmenu, record the parent's scroll offset before revealing the row, and invalidate the parent if it moved: ```cpp const float previousOffset = parent.scrollOffsetDip; parent.EnsureItemVisible(itemIndex); if (parent.scrollOffsetDip != previousOffset) InvalidatePopup(parent); ``` Then compute the rects as today. InvalidatePopup queues the repaint and republishes the UIA bounds. In the AccessibleInvoke handler (2125), call InvalidatePopup(*popup) after setting keyboardIndex, or before dispa …
- **Test**: Add a Menu control test with a root menu capped by maxRootHeightDip so it scrolls. The last navigable row is a submenu parent ("More"), followed by a disabled or Info footer row. 1. Scroll by wheel, or set the scroll through the debug state probe, so "More" is fully visible but the footer is out of …

### menu-loop-lifetime-6

**Pointer targeting trusts the root's delivered point and ignores z-order, so the submenu's left 13 DIP act on the root's hidden scrollbar**  
`src/Controls/DxUi.Menu.cpp:4651` · pre-existing · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: This is pre-existing surrounding code on the modal-loop and async input path. Capture always stays on the root popup, so every pointer message reaches MenuWndProc with hwnd == root and client coordinates relative to the root. FindPointerTargetPopup(controller, event) returns the delivered popup whenever the point lies inside that popup's surface rect, before it consults the topmost-first screen search. A first-level submenu is anchored at `parentItemRect.right`, the row's right edge = surface left + GetContentWidthDip() - kMenuBorderDip. When the root shows a scrollbar, GetContentWidthDip() excludes the 12 DIP lane, so the submenu overlaps the root's last 13 DIP (scrollbar lane plus border). In that strip, which the user sees as the submenu …
- **Failure**: A scrollable root (any menu with maxRootHeightDip or one taller than the work area) has a submenu open. The pointer is in the leftmost 13 DIP of a submenu row: the hover backplate starts at 5 DIP and the check/icon slot at 7 DIP. (a) A move there routes to the root: HitTestMenuItem misses (outside the root's viewport), so the root's hover drops, the submenu's highlighted row is cleared (RouteMenuPointerHover resets `hoveredIndex` and `keyboardIndex` on later popups), and ScheduleSubmenuCloseTimer closes the visible submenu after 400 ms if the pointer rests there. (b) A click on a Toggle row's …
- **Fix**: Smallest fix: in `FindPointerTargetPopup(MenuController&, const MenuPointerEvent&)`, take the delivered-point shortcut only when no popup above the delivered popup contains `event.screenPoint`. For example: 1. Find the delivered popup's index. 2. Walk `controller.popups` from the back down to just after that index, testing `GetInteractiveScreenRect()` against `event.screenPoint`, and return the first hit. 3. Only then use `IsDeliveredPopupSurfacePoint`, and fall back to the screen search, which …
- **Test**: Add a Menu interaction test in Tests/Controls with a root that has `maxRootHeightDip` set small enough to force `NeedsScrollbar()`, and a submenu item in view. 1. Open the submenu. 2. Compute a screen point inside the submenu's first navigable row, 3-6 DIP from the submenu surface's left edge. This …

### process-docs-specs-1

**Default test.ps1 suite list includes the focus-taking Menu and NativeTextInput suites, with no desktop lease**  
`test.ps1:29` · #48, #63, #65 · tooling · confirmed (trace: confirmed/medium; refute: confirmed/medium)

- **At e5ebbb5**: Fixed by #66: the default suite list no longer has Menu or NativeTextInput, and a local run that names them without -Interactive is refused (test.ps1:29, :56).
- **What**: PR #48 added `test.ps1 -Interactive`, a desktop lease that asks first, warns, and restores foreground window, focus and pointer. AGENTS.md and the build-dxui skill say these suites run deliberately through it and are otherwise left out of -Suites. But the default $Suites still lists Menu and NativeTextInput. Get-SuiteRun deliberately passes them no --no-activate, because the runner rejects that flag for them. The README quick start and the AGENTS.md rule 'Code changes require test.ps1 in x64 Debug, Release and ASan Debug' are the plain command, so the documented path takes the person's desktop with none of the lease protections. Testing_Validation.md concedes the hazard in prose ('leave them out of -Suites there') instead of fixing the defa …
- **Failure**: A developer or agent runs `.\test.ps1 -Configuration Debug -Platform x64` as README.md and AGENTS.md instruct, on a desktop they are using. Menu and NativeTextInput take the foreground, keyboard focus and pointer with no warning or confirmation. Nothing restores them if the run is killed, hits the watchdog or fails, and the person's keystrokes can land in the test windows.
- **Fix**: Leave the interactive suites out of a plain local run unless the caller asks for them. - In test.ps1, after parsing the parameters: when `-Interactive` is not set, `-Suites` was not passed explicitly, and no CI variable (CI, GITHUB_ACTIONS, TF_BUILD) is set, drop `Get-DxUiInteractiveSuiteNames` from `$Suites`. Print one line saying that Menu and NativeTextInput were skipped and that `-Interactive` runs them. - Alternatively, add an `-IncludeDesktopSuites` switch and pass it from ci.yml:92. Eithe …
- **Test**: Add a tooling test under Tools/tests (run by Invoke-ToolingTests.ps1) for the suite-resolution logic, factored into a function in InteractiveRun.psm1, for example `Resolve-DxUiDefaultSuites -Requested:$false -Interactive:$false -IsCI:$false`. It asserts that the result contains no name from `Get-DxU …

### process-docs-specs-3

**Grid group-header press calls OnGridGroupToggled and then keeps using the grid with no lifetime guard; the plan item is unchecked**  
`src/Controls/DxUi.Grid.cpp:3610` · #60, #57 · lifetime · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #60 and #57 guarded the selection delegate and the focus callbacks. The same defect class remains in Grid's group toggle on a header press (and, per the docs, checkbox, sort, row-activation and context-menu delegate calls). In the press path the lifetime token is taken only after OnGridGroupToggled returns, so a delegate that rebuilds the controls leaves ReconcileSelectionForVisibleRows, ClampScrollOffsets, `_hoveredRow` and `host.ClearTooltip` running on a destroyed grid. ToggleCheckboxCell also takes its token after OnGridCheckboxToggled. docs/controls.md admits 'the grid does not yet stop using itself after them' and tells applications to post the rebuild, and CodexBranchReview's last audit item is still unchecked. This is the pattern AG …
- **Failure**: An application's OnGridGroupToggled (or OnGridCheckboxToggled) rebuilds its page synchronously, which is common for 'expand group -&gt; reload model'. The grid is destroyed inside the delegate. Control returns to Grid::OnMouseDown, which calls ReconcileSelectionForVisibleRows(CollectOrderedGroups(_model)) on freed memory. This is a heap use-after-free that ASan reports and a release build turns into a crash.
- **Fix**: 1. In the group-header press path, the group key path and ToggleCheckboxCell, take `const std::weak_ptr&lt;int&gt; lifetime = GetLifetimeToken();` before the delegate call. Return true (the handled result) immediately if `lifetime.expired()` afterwards. Reuse that same token for the later selection-changed check instead of taking a second one. 2. In ApplyGroupLayout, take the token before the loop, check it after each OnGridGroupToggled (break and return if expired), and check it again after OnG …
- **Test**: Follow the replacement-in-delegate pattern from #60/#57. A test GridDelegate's OnGridGroupToggled (and separately OnGridCheckboxToggled) destroys and replaces the grid synchronously, for example by resetting the owning unique_ptr / the host's control tree. Drive each path: - a left press on a group …

### public-api-architecture-3

**Grid::RequestSelectRow reports success after its delegate destroyed the grid, unlike Tree's Request* APIs**  
`src/Controls/DxUi.Grid.cpp:2108` · #60 · api-design · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #60 made Tree's public RequestSelectVisibleItem/RequestAdd/RequestRemove return false when the tree did not survive, and docs/controls.md documents that. Grid's public RequestSelectRow ignores SelectRow's survival result and always returns true. RequestRemoveRowSelection also returns true on its `lifetime.expired()` path. The internal UIA callers revalidate through the element identity, but an application has no such tool. The docs also say a selection delegate may rebuild the controls. Applications cannot tell 'selected, grid alive' from 'grid destroyed by my delegate', and the two sibling controls have opposite contracts for the same kind of call.
- **Failure**: An application calls `if (grid-&gt;RequestSelectRow(i, 0)) grid-&gt;...` to select programmatically with notification. Its OnGridSelectionChanged rebuilds the page (allowed by docs/controls.md: 'You may rebuild or replace the controls from ... a grid's ... selection delegate'). RequestSelectRow returns true and the application dereferences the destroyed grid (use-after-free). The same code against Tree::RequestSelectVisibleItem is safe.
- **Fix**: 1. Grid.cpp:2108-2109: replace the two lines with `return SelectRow(rowIndex, modifiers);`. 2. Grid.cpp:2142: return false on the expired path. 3. In RequestRemoveRowSelection, after `RefreshAccessibilitySnapshot();`, return `! lifetime.expired()`. Take a second token as SelectRow does, because UIA delivery can destroy the grid there too. 4. Document the contract next to DxUi.h:3713, matching Tree: false for a row that is not visible, and false when the grid did not survive. Extend the docs/cont …
- **Test**: Add a Grid test (Tests/Controls/DxUiTests.Grid.cpp) using a heap-owned grid and a delegate whose OnGridSelectionChanged resets the owning unique_ptr: - `Require(! grid-&gt;RequestSelectRow(1u, 0u), "RequestSelectRow is false when the selection delegate destroyed the grid")`. This fails today because …

### public-api-architecture-4

**Tree::SetSelectedItemIds is quadratic for ids not in ascending order; every Ctrl+click copies every visible TreeItemData**  
`src/Controls/DxUi.Tree.cpp:627` · #35, #43 · performance · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: SetSelectedItemIds builds the selection with one GridSelectionModel::Toggle per id. Toggle inserts into the sorted copy with `_sortedRowIds.insert(sortedIt, rowId)`, which is O(n) per insert. When item ids do not ascend in the order given (hashed or path-derived ids, or a selection restored in a different order), the bulk restore is O(n²) memmove, and it runs on the UI thread in a noexcept API. The header contract for GridSelectionModel says mutators are 'O(n log n) at most'; that holds per call, not for this loop. Separately, every Ctrl+click toggle, Shift range and Ctrl+A calls CollectVisibleItemIds(). That calls ITreeModel::GetVisibleItem for every visible row and copies four std::wstrings each (text, iconText, badgeText, tooltipText) on …
- **Failure**: An application restores a 100k-item selection with `tree-&gt;SetSelectedItemIds(savedIds)` where the ids are hashes. About 100k inserts each move half of a vector of up to 100k uint64 values, roughly 40 GB of memmove: a multi-second UI freeze. In a 200k-row tree, each Ctrl+click copies about 800k strings before the toggle shows.
- **Fix**: Add one bulk mutator to GridSelectionModel, for example `void Assign(std::vector&lt;uint64_t&gt; orderedRowIds, std::optional&lt;uint64_t&gt; anchorRowId)`. It builds the sorted copy beside the current one, sorts it once, and swaps both copies in, so a failed allocation leaves the selection unchanged (the same pattern SetRange uses). It applies the same room give-back rule, and the anchor falls back to front() when the given anchor is not in the list. In Tree::SetSelectedItemIds: - Build `std::u …
- **Test**: Add a Tree control test with a synthetic model of 200,000 visible items and multi-select on. Call SetSelectedItemIds with all 200k ids in descending order (worst case for the sorted insert). Assert that the call completes within a generous bound, for example under 250 ms in Release, or use an operat …

### public-api-architecture-5

**TabControl tab switch publishes UI Automation inside its page-visibility loop and keeps using itself (same UAF class #60 fixed in Tree/Grid)**  
`src/Controls/DxUi.Controls.cpp:7214` · #60, #57, #29 · lifetime · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Control::SetVisible publishes the window's accessibility snapshot on every call. When a hidden page drops out of the UIA tree, RefreshWindowHostAccessibilitySnapshot raises StructureChanged synchronously. TabControl::UpdateVisiblePageBounds calls SetVisible on each page inside a loop over its own `children` vector. SyncLayout calls UpdateVisiblePageBounds, and SelectTab runs it after the focus step that #57 guarded. So one tab switch publishes twice, while TabControl is still iterating its own members. #60 accepted the premise that an in-process UIA event raise can dispatch a message that replaces the root, and it guarded Tree/Grid only. Its plan says the rest of the audit stays open, but it does not name this path. Hiding a page also now c …
- **Failure**: A UIA client is listening (Narrator, or the suite's SelectionEventInterruption harness). The user clicks tab 2 or presses Right on a TabControl. children[0]-&gt;SetVisible(false) raises StructureChanged, and a message dispatched during the raise calls ControlHost::SetRoot or rebuilds the page set, which destroys the TabControl. The loop then reads children.size() and children[1] from the freed vector. SyncLayout goes on to EnsureSelectedTabVisible() and RequestInvalidate(), and SelectTab copies _onSelectionChanged and calls GetLifetimeToken() on the freed object. ASan would report a heap-use-a …
- **Fix**: 1. Make `UpdateVisiblePageBounds` set every page's visibility flag without publishing. Either add an internal `Control::SetVisible(bool, bool publishAccessibility)` (or a ControlAccess helper) that keeps the `_interactionRevision` bump, `OnHidden()` and `RequestInvalidate()` but skips the refresh, or set the flags in one pass. 2. After the loop, take `const std::weak_ptr&lt;int&gt; lifetime = GetLifetimeToken();`, publish once with `RefreshWindowHostAccessibilitySnapshot(host-&gt;GetHwnd(), host …
- **Test**: 1. Add a diagnostics hook next to `NotifySelectionEventRaisedForTest` that runs after `UiaRaiseStructureChangedEvent` (for example `DebugSetAccessibilityStructureEventHookForTest`), plus a StructureEventInterruption helper like SelectionEventInterruption. 2. In the Accessibility suite, keep a `UiaTe …

### public-api-architecture-6

**Multi-selection row drag accepts drop targets inside the carried selection; the documented OnTreeReorder pattern then creates a cycle**  
`src/Controls/DxUi.Tree.cpp:335` · #35 · api-design · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: With multi-select, a press on a row of a multi-selection keeps the selection so that the drag can carry it, and docs/controls.md tells consumers to move every id in GetSelectedItemIds when the source belongs to the selection. ResolveReorderDrop only rejects the dragged row itself and the run of its own visible descendants. It still offers a Before/After/Inside drop on another selected row, or on a descendant of another selected row. The painted indicator and the committed OnTreeReorder both say the drop is valid. A consumer following the documented example then moves a node relative to itself or into its own subtree.
- **Failure**: Multi-select and reorder are on. The user selects folder P (expanded, child C) and an unrelated row X with Ctrl+click, presses on X and drags onto C's middle band. ResolveReorderDrop returns {sourceId=X, targetId=C, place=Inside}, because C is outside X's subtree. The documented OnTreeReorder sees X in the selection and calls MoveLayers({P, X}, C, Inside), which asks the model to put P inside its own child: a cycle, or an orphaned subtree. Dropping 'After' another selected sibling similarly asks to move a row relative to itself.
- **Fix**: Minimal fix in the library: when the drag carries the selection (`_reorderCollapsesSelection` / keepsSelectionForDrag is true), also reject a target that is selected, or that lies inside any selected row's visible subtree. Do this in ResolveReorderDrop or in a precomputation in ResolveReorderSource. Visible rows are in pre-order, so one linear pass at arm time (and again in the re-resolve in NotifyDataChanged) can build the excluded ranges. For each selected visible row, take the run after it wi …
- **Test**: Add a Tree interaction test with multi-select and reorder enabled. The model has P (expanded, children C1, C2), X and Y at root: - Ctrl+click P and X to select both. - Press on X with no modifiers and move more than 4 DIP so it becomes a drag. - Move to C1's middle band (C1 needs children, or use Be …

### test-support-infra-2

**A declined, timed-out or interrupted confirmation still moves the pointer and takes focus from the person**  
`Tests/Support/Support.Tests.InteractiveLease.h:203` · #48 · ux · confirmed (trace: confirmed/medium; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: RunWithDesktop records the desktop before the confirmation and runs a full DesktopLease::Restore on every path, including Declined, TimedOut, Interrupted and Unavailable, where nothing was taken. Restore treats any difference from the recorded state as something to undo. On these paths, any difference is the person's own activity while the dialog was up (up to 120 s). With no anchor set, BringToForeground attaches this thread's input to the current foreground thread (the person's application) and calls SetForegroundWindow, which steals focus. RestoreCursor calls SetPhysicalCursorPos unconditionally. The Menu fixture's ScopedMenuPointerFixture explicitly preserves external pointer movement; the lease does the opposite. The fake-desktop test …
- **Failure**: The person starts test.ps1 -Interactive and goes back to typing in their editor during the multi-minute build and performance run. The topmost dialog appears but is not foreground, and the person ignores it. After 120 s it cancels itself (TimedOut). Restore sees foreground = editor and saved = terminal, attaches to the editor's thread and moves the foreground to the terminal. It also warps the pointer to where it was two minutes earlier. The next keystrokes go to the PowerShell console still running test.ps1, and are queued as typeahead that runs when the prompt returns. With Cancel clicked by …
- **Fix**: Keep the full DesktopLease restore for the paths that took the desktop, meaning Started and after the warning was shown. For the paths that end before that (Declined, TimedOut, Interrupted, None/Unavailable), only undo what the dialog itself did. 1. Move the scope_exit that calls `lease.Restore()` so it is installed only after `Confirm` returns `Started`. The ShowWarning-refused path also needs it, because ActivateAnchor may already have moved the foreground. 2. For the non-start returns, add a …
- **Test**: Add two fake-desktop tests to Tests/Controls/DxUiTests.InteractiveLease.cpp. Both fail at HEAD and pass after the fix. (a) TestLeaseLeavesAPersonsOwnChangesAloneWhenTheAnswerIsCancel: set `onConfirm = [&] { lease.desktop.TakeForeground(lease.person.otherApp); lease.desktop.cursor = {900, 700}; }` (t …

### test-support-infra-3

**The pointer is recorded before the dialog, so every mouse-confirmed run reports the pointer as moved by the run and blames a fixture**  
`Tests/Support/Support.Tests.InteractiveLease.h:199` · #48, #63 · test-quality · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: lease.Capture() runs before Confirm, so the saved cursor is wherever the pointer was before the person moved it to click 'Start the tests'. Menu fixtures (ScopedMenuPointerFixture) put the pointer back to where they found it, which is the Start button position, not the pre-dialog position. At the end, atExit.cursor differs from saved.cursor, so restoration.cursor=restored and runMovedSomething=1. test.ps1 then prints a warning that a fixture left the pointer behind. This happens on every normal run confirmed with the mouse, so the leaked-pointer signal is always on. A real fixture that leaves the pointer behind is indistinguishable from the person's own click, which defeats the evidence Testing_Validation.md says the lease provides (cursor. …
- **Failure**: The person clicks Start with the mouse. The Menu and NativeTextInput suites pass and every fixture restores the pointer correctly. The result still shows `restoration.cursor: restored`, `restoration.runMovedSomething: 1`, and test.ps1 warns: 'The suites left the pointer at &lt;Start button&gt;... a pointer left behind is a fixture to look at.' Receipts record runMovedSomething=true for a clean run.
- **Fix**: Judge the pointer against its position at hand-over, not its position before the dialog. - Add `void DesktopLease::RecordHandOverCursor()` to DesktopLease. It re-reads only `_saved.cursor` and `_saved.cursorKnown`. Keep the foreground and focus from before the dialog, because those are what the person gets back. - In `RunWithDesktop`, call `lease.RecordHandOverCursor()` right after `services.Confirm(request)` returns `Confirmation::Started`, before `ShowWarning`. - `RestoreCursor` and `atExit` t …
- **Test**: Add a test to the lease self-test, with the fake `LeaseServices`/`DesktopBackend` that the self-tests already use, which run without a desktop: - The fake desktop starts with the pointer at (10,10). - The fake `Confirm()` moves it to (500,400), as the person's click does, and returns `Started`. - `S …

### test-support-infra-4

**The Start button's access key works without Alt, so a stray 's' keystroke can start the desktop takeover**  
`Tests/InteractiveLease/InteractiveLease.Tests.Confirmation.h:133` · #48 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: The consent dialog relies on 'the default button is Cancel, so a key pressed by accident ... never starts a takeover; Start needs a click or its access key'. In Windows dialogs, including TaskDialog push buttons, a mnemonic is triggered by the bare letter when focus is on a button; Alt is not needed (as with 'Y' and 'N' in MessageBox). The dialog calls SetForegroundWindow on TDN_CREATED and puts focus on Cancel. If the person is typing in the console that launched the run (which has foreground rights), any word containing 's' answers Start. The tooling test only checks the default button, so it cannot catch this.
- **Failure**: The person keeps typing notes or the next command in the terminal while the build runs. The dialog takes the foreground and focus is on Cancel. The next 's' typed activates '&Start the tests', and the run takes over the desktop without a deliberate choice.
- **Fix**: The smallest fix is to drop the mnemonic: `L"Start the tests"`. Start can then only be chosen by a click or by a deliberate Tab to it followed by Space or Enter. Update the comment at Confirmation.h:113-114 and Testing_Validation.md:479-480 so they no longer mention an access key or Alt+S. If keyboard users should keep a direct shortcut, keep the mnemonic but arm the button late instead: - On TDN_CREATED, send TDM_ENABLE_BUTTON(kStartButton, FALSE). - On TDN_TIMER, re-enable it once elapsed reac …
- **Test**: Extend the lease self-test's private-desktop scenario in Tests/InteractiveLease/Main.cpp (around lines 469-491). Add a DialogScript mode that, from TDN_TIMER shortly after creation, posts WM_KEYDOWN/WM_CHAR/WM_KEYUP for 'S' (no Alt) to the window that has focus (GetFocus on the dialog thread). Check …

### tests-grid-tree-render-1

**Visual baseline tolerance let PR #62's slider redesign through with no baseline update**  
`Tests/Controls/Controls.Tests.DxUiTestHelpers.h:567` · #62, #22, #31 · test-quality · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: VerifyOrUpdateBaselineForTest passes a capture when up to 2% of its pixels differ by more than 8/255. On the small baseline scenes, that budget is bigger than a whole control's visual change. PR #62 enlarged every Slider's thumb (6 to 14 DIP at rest, 16 to 20 hovered) and chrome disc (20 to 24 DIP). TestDxUiAdvancedControlsVisualBaseline draws three sliders in advanced_controls_dark.png, but no file under Tests/Controls/Baselines changed after 2026-09-08. The test kept passing against the old slider design, so these baselines cannot detect a control-level visual regression.
- **Failure**: advanced_controls_dark.png is 804x241 = 193,764 px, so 2% allows 3,875 differing pixels. Per slider, the disc ring growth is about pi*(12^2-10^2) = 138 px and the inner thumb growth about pi*(7^2-3^2) = 126 px, so roughly 260 px each and about 800 px for all three. That is about 0.4%, well under budget. The same holds for core_controls_*.png (304x161, 979 px budget). A future regression of similar size, such as a thumb losing its accent fill, a 4 DIP offset or a missing tick row, would also pass.
- **Fix**: 1. Add an absolute differing-pixel cap next to the ratio: an optional `size_t maxDifferingPixels` parameter that defaults to something like `max(64, 0.1% of total)`, failing when either limit is exceeded. Keep the per-channel tolerance at 8 so anti-aliasing and GPU rounding noise still pass. 2. Review the current captures and regenerate all eight Tests/Controls/Baselines PNGs with the baseline-write mode, recording why in the provenance or changelog. This covers #62 and any other visual drift si …
- **Test**: Add a self-test of the comparator: copy a known baseline capture and repaint a 14x14 DIP disc (about 150 px at 96 DPI) in a contrasting colour at the slider thumb position. Then assert that VerifyOrUpdateBaselineForTest, or a non-exiting variant that returns the verdict, rejects it. This fails today …

### tests-menu-a11y-host-1

**A default (non-interactive) suite moves the user's mouse pointer: NewControls runs the menu cursor test that calls SetCursorPos**  
`Tests/Controls/DxUi.Tests.NewControls.cpp:2331` · #24, #29, #63 · flakiness · confirmed (trace: confirmed/medium; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.
- **What**: Since #24, `RunNewControlTests` calls `RunMenuDescriptionTests()`. That list includes `TestMenuChoosesTheCursorWhenItOpensAndCloses`, which #29 added and #63 reworked. The test moves the physical pointer with `ScopedMenuPointerFixture::AlignCursor` (SetCursorPos) and puts a topmost window on screen at (100,520). Nothing checks `DxUiTestWindowsCanActivateFlag()` first. test.ps1 runs NewControls with `--no-activate` and promises that a run without -Interactive never reaches for the desktop. The activation blocker it starts is only a WH_CBT hook for create, activate and focus, so it does not block SetCursorPos. AGENTS.md says that only `test.ps1 -Interactive` suites, run with the person's agreement, may take the pointer.
- **Failure**: A developer runs the default `test.ps1` or a scoped `Test-Changes.ps1` run that includes NewControls while still using the mouse. Up to five times (RunUntilForegroundHeld), the pointer jumps to the middle of the test window, about (220,580). If the user moves the mouse during an attempt, `GetCursor()==textCursor` no longer holds and the default suite fails spuriously. A `Require` that calls std::exit after AlignCursor skips the fixture destructor, so the pointer is not put back. That Require is `ClientScreenPointForTest`, line 8028 of DxUiTests.Menu.cpp.
- **Fix**: Keep the physical pointer move inside the activating (interactive) lanes. At the top of TestMenuChoosesTheCursorWhenItOpensAndCloses, add: `if (! DxUiTestWindowsCanActivateFlag()) { SkipDxUiTest("the menu open-and-close cursor test places the physical pointer, which only the interactive Menu lane may do"); return; }` The Menu lane still runs it through RunMenuTests -&gt; RunMenuDescriptionTests. Then update Testing_Validation.md:297-300, which says "(NewControls and Menu)", and the NewControls s …
- **Test**: Add a native control test in a nonactivating suite. 1. Set `SetDxUiTestWindowsCanActivate(false)` (restore it with ScopedNonActivatingTestWindows). 2. Construct a ScopedMenuPointerFixture on a test window. 3. Read GetPhysicalCursorPos before and after calling `AlignCursor` with a point at least 50 p …

### text-input-10

**ComboBox::NotifyTextChanged calls the stored std::function in place with a view of `_text`, then touches `this`**  
`src/Controls/DxUi.ComboBox.cpp:2314` · #57 · lifetime · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ComboBox::SetTextAndNotify was hardened: "both the callable and its argument must survive that callback". TextField::NotifyChanged copies the callable and the text and checks lifetime. The ComboBox::NotifyTextChanged helper, used by OnChar, RefreshEditableTextAfterMutation (delete, cut, paste, undo), ImportTextInputState (IMM preview and commit) and CommitSelection, invokes the member `_onTextChanged` directly. It passes a std::wstring_view into the member `_text`, then calls RefreshAccessibilitySnapshot() (reading _host), and its callers go on to ResetEditableCaretBlink, EnsureEditableCaretVisible and Invalidate. This is the same class as first-review finding 2, here in the ComboBox, which that review did not audit.
- **Failure**: An editable ComboBox with an autocomplete or normalising handler, for example `[cb] (std::wstring_view t){ cb-&gt;SetText(Normalize(t)); Log(t); }` or one that calls SetOnTextChanged to swap handlers. When the user types a character, OnChar calls NotifyTextChanged and SetText reassigns `_text`, so `t` dangles and Log(t) reads freed memory. A handler that replaces itself destroys the std::function while it is running. A handler that rebuilds the form destroys the ComboBox, and RefreshAccessibilitySnapshot and OnChar's continuation then read the freed control.
- **Fix**: Change NotifyTextChanged to `[[nodiscard]] bool NotifyTextChanged()`, written as: const auto callback = _onTextChanged; const std::weak_ptr&lt;int&gt; alive = GetLifetimeToken(); if (callback) { const std::wstring snapshot = _text; callback(snapshot); } if (alive.expired()) return false; RefreshAccessibilitySnapshot(); return true; Every caller should return early (`return true;` for input handlers) when it gets false: - OnChar: skip the caret blink, caret-visible and Invalidate calls. - Refresh …
- **Test**: Add Tests/Controls ComboBox tests under ASan Debug: (a) Handler re-enters SetText. Build an editable ComboBox whose OnTextChanged handler does `std::wstring before(t); cb-&gt;SetText(L"X"); seen = std::wstring(t);` and dispatch OnChar(L'a'). Before the fix, ASan reports heap-use-after-free on `t`. A …

### text-input-11

**The native TSF path does not track compositions: every IME preview is applied as a committed, notifying edit**  
`src/Controls/DxUi.TextStoreACP.cpp:1157` · pre-existing · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ActivateNativeTextInputTsf associates a document manager with the native store on the host HWND. TSF IMEs (the default Microsoft IMEs) therefore edit through ITextStoreACP, not WM_IME_*. NativeTextStoreTarget does not override StartComposition, UpdateComposition or EndComposition; the TextStoreTarget defaults just accept. ReplaceTextRange applies every TIP write with ApplyState(state, true), which calls SetTextAndNotify. Composition fields in the host cache are set only by the IMM handler. As a result, with a TSF IME the native TextField has no composition range: no underline or conversion-target paint, no UIA TextEdit composition or conversion events, and DeactivateNativeTextInputSession's composition cancel never applies. The application' …
- **Failure**: In a native-host TextField with the Microsoft Japanese IME, typing 'k','a' calls onTextChanged with "k" and then "か" before the user converts. A search-as-you-type or validation handler runs on preview text, and a handler that rewrites the text makes ApplyState return false, which aborts the TIP edit with E_FAIL in mid-composition. Narrator gets plain text-changed events instead of composition events. Escape fires onTextChanged again when the TIP restores the text.
- **Fix**: Minimal fix: give NativeTextStoreTarget a composition model that matches the embedded one, reusing what the IMM path already has rather than adding a third model. 1. Add `std::optional&lt;TextInputState&gt; _compositionBase` and a nesting depth to NativeTextStoreTarget. - StartComposition: on the first start, capture the base with ExportTextInputState through _host, set `*accepted = TRUE`, and set the host's composition fields. Do this through a new ControlHost helper that does what startComposi …
- **Test**: Add a native-host test, TestNativeTextInputTsfCompositionPreviewDoesNotNotify: - Set up AttachedHostWindow with the Native backend and a focused TextField(L"ab"). Install an onTextChanged counter that records each value. - Create the store via DebugCreateNativeTextInputTextStoreForTest and advise a …

### text-input-13

**Single-line GetTextExt ignores the field's horizontal scroll and masked display text, so IME candidate windows land away from the caret**  
`src/Controls/DxUi.TextStoreACP.cpp:272` · pre-existing · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: For a single-line field, NativeTextStoreTarget::GetRangeScreenRect measures its own layout of `state.text` (the plaintext control text) with no scroll offset, then clamps to the viewport. TextField and ComboBox already implement GetTextInputRangeRects and GetTextInputCaretRect, which use the display text (masks, concealed dot count) and `_horizontalScrollDip`/`_editableHorizontalScrollDip`. The multiline branch and UI Automation use those, so this branch is a parallel geometry utility that has diverged from them. It also always reports pfClipped=FALSE, even when it clamps.
- **Failure**: A single-line path field holds text longer than its width and is scrolled to the end. The user clicks near the visible left edge and starts a TSF IME composition there. The caret is drawn at left+10dip, but GetTextExt returns startOffset ≈ textWidth − visibleWidth + 10, clamped to bounds.right. The IME's candidate and reading windows appear at the far right edge of the field instead of under the composition. In a masked field, offsets are computed from the plaintext glyph widths, while the field draws bullets or a concealed dot count.
- **Fix**: Remove the single-line special case and let both modes use the control's geometry. TryResolveMultilineTextStoreRangeRect is not multiline-specific: for a non-empty range it unions TryGetTextInputRangeRects, and otherwise it unions TryGetTextInputCaretRect over the range. It then clips to bounds. Steps: 1. Rename it, e.g. TryResolveTextStoreRangeRect, and call it unconditionally. 2. Return TS_E_NOLAYOUT when it yields nullopt, as now. 3. Make ClipTextStoreRectToBounds report whether it clamped, a …
- **Test**: Add a test to Tests/Controls/DxUiTests.NativeTextInput.cpp, next to the existing GetTextExt cases (around line 5064): 1. Create a single-line TextField about 80 DIP wide and set text much longer than that (e.g. 200 'W' characters). 2. Focus it, put the caret at the end so `_horizontalScrollDip` &gt; …

### text-input-14

**The multiline GetTextExt fallback rebuilds a full-text DirectWrite layout for every index of an off-screen range**  
`src/Controls/DxUi.TextStoreACP.cpp:131` · pre-existing · performance · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: When TryGetTextInputRangeRects returns nothing for a non-empty range, TryResolveMultilineTextStoreRangeRect loops over every UTF-16 index from start to end and calls TryGetTextInputCaretRect for each. That happens whenever the range is entirely outside the viewport (ClipTextInputRectToBounds collapses each rect to zero height and they are all filtered out) or consists only of line breaks. Each call copies the display text and runs GetOrCreateMultilineLayout plus GetMultilineLineMetrics. It then calls MeasureMultilineCaretRectDip, which creates a new, uncached CreateMultilineTextLayout of the whole text. The cost is O(range × text) shaping, synchronously inside a TSF lock on the UI thread. First-review finding 4 makes this worse: every TSF w …
- **Failure**: A multiline TextField holds 100,000 characters, scrolled down, and the user types with a TSF IME. After the first write resets the scroll, each GetTextExt of the 3–5 character composition costs 4–6 full-text layouts, and TIPs query it several times per keystroke, so input visibly lags. A text service or touch-keyboard query for an off-screen selection of a few thousand characters freezes the window for seconds or more.
- **Fix**: 1. Remove the per-index loop from TryResolveMultilineTextStoreRangeRect. For a non-empty range, add a TextField helper that runs one HitTestTextRange on the cached GetOrCreateMultilineLayout without clipping. Union the hit rects, then compare the union with the viewport once. If they do not intersect, return S_OK with `*pfClipped = TRUE` and the nearest viewport-edge rect, matching the TSF contract and the embedded path in Embedded.cpp:809-819. Otherwise clip the union and set pfClipped to wheth …
- **Test**: Add a Controls test for the native TextStore. Create a multiline TextField with about 2,000 short lines, focus it, and scroll it so firstVisibleLine is about 1,000. Take a read lock and call GetTextExt for a range of a few hundred characters near line 10, which is entirely above the viewport. - Asse …

### text-input-19

**Duplicated tree-membership walkers and a redundant Detach/Disconnect pair in the text store**  
`src/Controls/DxUi.TextStoreACP.cpp:162` · #29 · simplification · confirmed (batch: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: TextStoreControlBelongsToTree (TextStoreACP.cpp:162), NativeTextInputControlBelongsToTree (NativeTextInput.cpp:30), ControlBelongsToTree (WindowHost.cpp:981) and ControlBelongsToBranch (Controls.cpp:235) are four copies of the same recursive Panel walk. AGENTS.md says to search for and reuse an existing helper. DetachNativeTextInputTextStore does a QueryInterface plus static_cast and calls DetachHost(), which only calls Disconnect(). It is always called right after DisconnectNativeTextInputTextStore (NativeTextInput.cpp:743-744 and 785-786), so every teardown disconnects twice through two different casts. #29 renamed the class without folding these.
- **Failure**: A fix to tree membership (for example the null-slot rule noted in Panel::ClearChildren) has to be made in four places. One missed copy makes the TSF store and the host disagree about whether a control is live.
- **Fix**: Expose one `ControlBelongsToTree(const Control* root, const Control* target)` in DxUi.Internal.h, using GetLogicalChildCount/GetLogicalChild, and delete TextStoreControlBelongsToTree, NativeTextInputControlBelongsToTree and ControlBelongsToBranch (or make ControlBelongsToBranch an alias). Add a regression test: a TextField inside a PageHost page, focused, with a TSF store whose ReadState or GetText must succeed and composition must commit. Remove DetachNativeTextInputTextStore and TextStoreACP:: …

### text-input-3

**The edit-message shim gives a masked field's plaintext to any process through WM_GETTEXT, and takes over the host window's own title messages**  
`src/Controls/DxUi.NativeTextInput.cpp:1071` · pre-existing · security · confirmed (trace: confirmed/medium; refute: confirmed/high)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: HandleNativeTextInputEditMessage answers WM_GETTEXT and WM_GETTEXTLENGTH from ExportTextInputState, which returns `_text` unmasked for a masked TextField. Nothing checks `masked`. The shim acts whenever `_focusedControl` is a text-input control, and OnKillFocus(false) keeps `_focusedControl` after the window is deactivated. UI Automation takes care to hide masked text (GetControlAccessibleValue and SupportsTextPattern both return nothing for a masked field), so this path is inconsistent with the library's own password policy. The shim also answers WM_SETTEXT and WM_GETTEXT on the host HWND itself. A top-level host is a supported setup (the test host is a WS_OVERLAPPED top-level window), so while a field has focus, the window's caption messa …
- **Failure**: (1) A login form with a focused masked TextField. Any other process at the same integrity level calls SendMessageW(hostHwnd, WM_GETTEXT, n, buf), as 'asterisk revealer' tools do, and receives the password in clear. This also works after the user switches to another app, because the focus is retained. A Win32 ES_PASSWORD edit refuses this. (2) An app whose ControlHost is attached to its main window calls SetWindowTextW(main, L"Doc - App") while a TextField has focus. The field's text is replaced by the title (with an undo-history reset and an onTextChanged call), and the caption does not change …
- **Fix**: In HandleNativeTextInputEditMessage, export the state once. When `state.masked` is set: - WM_GETTEXTLENGTH returns 0 and WM_GETTEXT writes an empty string and returns 0, both handled. This matches the masked UIA policy and the ES_PASSWORD behavior. An in-process caller that needs the value uses TextField::GetText. - WM_COPY and WM_CUT are already refused by the control. Separately, stop the shim from shadowing a top-level window's caption. Apply WM_GETTEXT, WM_GETTEXTLENGTH and WM_SETTEXT only w …
- **Test**: Add two tests to Tests/Controls/DxUiTests.NativeTextInput.cpp. (a) Masked field: - Set up an AttachedHostWindow with the Native backend, a TextField L"secret" with SetMasked(true), and focus on that field. - Require SendMessageW(hwnd, WM_GETTEXTLENGTH) == 0. - Require WM_GETTEXT into a 64-char buffe …

### text-input-4

**ActivateNativeTextInputSession keeps using the control after its own SetFocus(_hwnd), which can destroy that control and also reactivates the session reentrantly**  
`src/Controls/DxUi.NativeTextInput.cpp:591` · #57 · lifetime · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #57 added a lifetime check after SetFocus(hwnd) in FocusControlAndSurvive because native focus can run code that destroys controls. The same synchronous SetFocus(_hwnd) also runs inside ActivateNativeTextInputSession, which ControlHost::SetFocusControl reaches through ActivateTextInput, but nothing is revalidated there. SetFocus sends WM_KILLFOCUS to the previously focused HWND (application code) and WM_SETFOCUS to the host. OnSetFocus then calls RefreshWindowHostAccessibilitySnapshot, which the project documents as able to dispatch messages that replace controls, and calls ActivateTextInput(_focusedControl) again. After SetFocus returns, the outer call goes on with the raw `control`: ActivateNativeTextInputTsf(control) makes the virtual ca …
- **Failure**: A host child HWND sits next to an application-owned native edit, for example an in-place rename box whose WM_KILLFOCUS commits and rebuilds the DxUi panel. The user clicks a DxUi TextField. TextField::OnMouseDown calls FocusControlAndSurvive, which calls SetFocusControl, then ActivateNativeTextInputSession, then SetFocus(_hwnd). The rename box's WM_KILLFOCUS destroys the controls. ActivateNativeTextInputTsf(control) then calls control-&gt;SupportsTextInput() on freed memory, a use-after-free crash. FocusControlAndSurvive's check after SetFocusControl comes too late. The same happens when the W …
- **Fix**: In ActivateNativeTextInputSession, take `const std::weak_ptr&lt;int&gt; lifetime = control-&gt;GetLifetimeToken();` before calling SetFocus(_hwnd). After SetFocus returns, revalidate with `RevalidateInteractiveDispatchedControl(lifetime, _root.get(), control)` or an equivalent, and require both `control == _nativeTextInputControl` and `control == _focusedControl`. If any check fails, return. Call DeactivateNativeTextInputSession(false) first if `_nativeTextInputControl` still names the dead cont …
- **Test**: Add a WindowHost control test with a host child HWND and a same-thread sibling native child window. Subclass the sibling so its WM_KILLFOCUS calls host.SetRoot(new root), or removes the TextField from its parent. Put keyboard focus on the sibling with SetFocus. Then send WM_LBUTTONDOWN/WM_LBUTTONUP …

### text-input-5

**TextField and ComboBox keep using `this` after SyncTextInput, whose native accessibility publish can dispatch messages that destroy them (the #60 defect class)**  
`src/Controls/DxUi.TextInput.cpp:984` · #60, #53 · lifetime · plausible (trace: plausible/medium; refute: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: SyncTextInput calls SyncNativeTextInputSession, which calls RaiseNativeTextInputAccessibilityEvents. That runs RefreshWindowHostAccessibilitySnapshot and up to five synchronous UiaRaise* calls. The library itself says (Internal.h, Accessibility.cpp:9274, SelectionPublishLifetime plan) that an outgoing UI Automation call in an STA dispatches messages that can hide, remove or replace the control, and #60 guarded the Tree and Grid continuations for this reason. TextField and ComboBox call SyncTextInput from about 29 sites and keep reading members afterwards. TextField::SetText calls RefreshAccessibilitySnapshot() (reading _host) and RequestInvalidate() (reading _host). SetSelectionRange does the same. ReplaceSelectionAndNotify then also runs N …
- **Failure**: Narrator is running, so text events have a listener. The user types or a TSF IME inserts text into a TextField. NativeTextStoreTarget::ApplyState calls textField-&gt;SetTextAndNotify, then SetText, then host-&gt;SyncTextInput(this), and the UiaRaiseAutomationEvent(TextChanged) dispatches a posted application message that rebuilds the page. SetText then calls RefreshAccessibilitySnapshot and RequestInvalidate on the freed field, and SetTextAndNotify calls NotifyChanged, which reads _onTextChanged and _text from freed memory. This is a heap use-after-free of the kind the #60 regressions catch fo …
- **Fix**: Apply the #60 pattern. 1. Make the text sync report survival. Either give ControlHost::SyncTextInput a `[[nodiscard]] bool` result, or keep it void and have each caller take `const std::weak_ptr&lt;int&gt; life = GetLifetimeToken();` before calling SyncTextInput. 2. In TextField::SetText, SetSelectionRange, ReplaceSelectionAndNotify, SetMasked and the input handlers, return immediately after SyncTextInput when `life.expired()`. 3. Make SetText return bool. SetTextAndNotify then calls NotifyChang …
- **Test**: 1. Extend the diagnostics hook so it also fires from RaiseWindowHostTextInputAutomationEvent. One way is to call NotifySelectionEventRaisedForTest, or a text-event equivalent, after each UiaRaise* there. 2. In the Accessibility suite, with a listening UiaTest::Client and a focused TextField that fil …

### text-input-6

**#55 multiline caret clip half-erases the caret at line start and hides it past the right edge**  
`src/Controls/DxUi.TextInput.cpp:1605` · #55 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The caret is a 1-DIP DrawLine centred on the snapped caret x. The new clip is the snapped text rectangle. A caret at the start of a line has caretRect.left == textRect.left (HitTestTextPosition returns x = 0), so half of the stroke lies left of the clip and is removed. The default per-primitive antialiasing (no SetAntialiasMode anywhere in src) then leaves a single half-coverage pixel column instead of the previous two. The caret also disappears when DirectWrite places it at or past the layout width, for example trailing spaces that hang past the wrap width, or the end of a line that exactly fills it. The single-line path clamps caretX to [left, right-1] and has no such problem. The #55 test steps one character right ('place visible caret i …
- **Failure**: Focus an empty multiline TextField, or press Enter or Home in one. The caret sits at column 0 and is drawn at roughly half its former intensity, which is very faint in High Contrast or with light caret colours. Typing spaces at the end of a full-width line makes the caret vanish entirely.
- **Fix**: Limit the clip to the vertical range only. Keep the snapped text rect's top and bottom and widen it horizontally to the control bounds: const D2D1_RECT_F snappedText = SnapRectToPixel(host, textRect); const D2D1_RECT_F snappedBounds = SnapRectToPixel(host, GetBounds()); dc-&gt;PushAxisAlignedClip(D2D1::RectF(snappedBounds.left, snappedText.top, snappedBounds.right, snappedText.bottom), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE); Also clamp the multiline caret x to [textRect.left, max(textRect.left, text …
- **Test**: Extend TestMultilineCaretViewport (Tests/Embedded/EmbeddedTextInputTests.h) at 96/144/192 DPI with these cases: (a) After Ctrl+Home, do not press VK_RIGHT. Count the magenta caret columns and the peak caret-channel intensity, and require the same column count and peak as a caret at column 1. Before …

### text-input-8

**Multiline Paint shapes the whole text twice per frame (caret blink included) despite the layout cache**  
`src/Controls/DxUi.TextInput.cpp:1581` · #55 · performance · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: TextField::Paint gets the cached multiline layout (GetOrCreateMultilineLayout) twice but uses it only for line metrics. DrawMultilineSelection then creates a new IDWriteTextLayout for the full text (line 810), and MeasureMultilineCaretRectDip creates another (line 765). Each caret blink flip (530 ms) invalidates and repaints, so a focused 65K-unit multiline field re-shapes its whole text twice per blink and per keystroke. The two fresh layouts are also sized from the snapped rectangle in one case and the unsnapped one in the other. At fractional DPI that can wrap a boundary line differently from the cached layout used for hit-testing and the IME rect, so the drawn caret can disagree with the drawn text. The #55 change sits in this block.
- **Failure**: A focused multiline log viewer field holds about 50 KB of text. Every blink and every keystroke costs two full DirectWrite shapings, visible as CPU use and frame-time spikes while idle-blinking. At 125% DPI a line that just fits the unsnapped width wraps in the snapped draw layout, so the caret is drawn on the wrong visual line.
- **Fix**: 1. Add layout-taking variants: `DrawMultilineSelectionWithLayout(host, IDWriteTextLayout* layout, origin, clipRect, ...)` and `MeasureMultilineCaretRectDipWithLayout(IDWriteTextLayout*, textRect, scrollDip, caretIndex)`, following the existing DrawSingleLineSelectionWithLayout pattern. 2. In TextField::Paint, call GetOrCreateMultilineLayout once (hoist it above both branches) and pass that layout to the draw, caret and BuildNativeCompositionUnderlineRects/TryGetTextInputRangeRects paths. Keep th …
- **Test**: 1. Add a debug counter to TextField, like Grid's DebugGetTextLayoutStatistics().layoutCreations used in Tests/Controls/DxUiTests.Rendering.cpp, that counts multiline CreateTextLayout calls. Put a large multiline text in a focused TextField, paint once to warm it, then advance Tick past two 530 ms bl …

### text-input-9

**Masked fields expose the plaintext to TSF text services and accept IME composition**  
`src/Controls/DxUi.TextStoreACP.cpp:339` · pre-existing · security · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: NativeTextStoreTarget::ReadState returns the real text of a masked TextField, from either the host cache or the TextField fallback, and GetText, GetSelection and GetTextExt serve it to every TIP: IMEs, the touch keyboard's prediction, dictation. The store reports no input scope (no ITfInputScope or IS_PASSWORD) and GetStatus reports no flags, so text services cannot tell it is a password. The IMM path checks only readOnly, never masked, and neither path disables the IME the way a Win32 ES_PASSWORD edit does. The embedded ClientTextStoreTarget does the same with the client's snapshot. UI Automation, in contrast, hides masked text.
- **Failure**: A Japanese or Chinese user with the IME in kana or pinyin mode types a password into a masked TextField. The IME composes kana or hanzi into the field, so the stored password is not what the user typed on the keys. Its candidate window shows the reading in clear. The IME or touch-keyboard prediction may learn the password into the user dictionary on disk. Any TIP can read the field's plaintext through ITextStoreACP::GetText.
- **Fix**: Do what `ES_PASSWORD` does: while a masked control has focus, keep IME text services out of it. Native host: - In `ActivateNativeTextInputSession` / `ActivateNativeTextInputTsf`, when `controlState.masked` is set, do not push a context. Call `threadMgr-&gt;AssociateFocus(_hwnd, nullptr, ...)` and `SetFocus(nullptr)` (or use an empty document manager) so TSF IMEs are disabled. Keep plain `WM_CHAR` keyboard input working. - In the IMM handler, treat masked like readOnly and return early, and call …
- **Test**: Add native text-input backend tests in the TextField/NativeTextInput suite: 1. `TestNativeTextInputMaskedFieldDoesNotActivateTsfDocument`: focus a masked TextField holding "secret". Assert that `_nativeTextInputEventCounters.tsfActivationSuccessCount` did not increase (or that the TSF context is nul …

### tooling-perf-gate-1

**Identical-library 'noise' rule is keyed on a fingerprint that leaves out the build and restore inputs, so a regression caused only by a build change can pass the PR gate**  
`Tools/BenchmarkGate.psm1:258` · #46 · bug · confirmed (trace: confirmed/medium; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Get-BenchmarkConclusion treats both sides as identical library code when their summary sourceFingerprint values match. In that case every regressed timing or memory metric becomes outcome 'noise', and the scenario can conclude 'pass'. The fingerprint (PerformanceComparison.psm1:22) only covers src, include, Build, Directory.Build.props/.targets, vcpkg.json and vcpkg-tool.json. The scope rules (BenchmarkGate.psm1:40) list other files that change the compiled library or the binary that measures it: DxUi.sln (solution-to-project configuration mapping), build.ps1 (MSBuild /p: properties), vcpkg-install.ps1, vcpkg-configuration.json (overlay ports and registries that pick the WIL headers compiled into the library), Tools/VisualStudio.psm1, Tools …
- **Failure**: A PR adds an overlay port in vcpkg-configuration.json (or changes the /p: properties in build.ps1, or the solution configuration mapping) that makes the library slower. The scope step reports 'build or restore' and measures it. Both sourceFingerprint values are equal, so $unchanged is true. Clean frame p95 regresses +12% with p=0.002 and its controls hold, but line 198 maps 'regressed' to 'noise'. The conclusion is 'pass', Publish-BenchmarkVerdict exits 0 and the PR check is green, with only a notice annotation.
- **Fix**: Tie the noise downgrade to the changed-path scope, not only to the fingerprint. Get-BenchmarkScope already returns Matches with a Reason per path. Have performance-paired.ps1, or the scope step via a workflow output or a field in summary.json, record whether every measured path's Reason is in {'benchmark harness', 'benchmark executable', 'fixture or sample compiled into it', 'paired measurement or gate', 'hosted workflow'}. Pass that to Get-BenchmarkConclusion, and set $unchanged only when the f …
- **Test**: Add a case to Tools/tests/Test-BenchmarkGate.ps1 with a summary where BaselineFingerprint = CandidateFingerprint = 'CAFE' and a held, non-exact regressed metric (for example clean/fps regressed 1100 -&gt; 900 with two held controls). Mark the run's scope as build or restore, either by a summary fiel …

### tooling-perf-gate-2

**The rank test treats the 12 runs as independent, but the A,B,B,A schedule runs same-side runs back to back, and the adjacent B2/B1 control is the one least able to see a burst**  
`Tools/PairedRun.psm1:164` · #31, #46 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged, line moved.
- **What**: Get-PairedRunSchedule produces A1 B1 B2 A2 A3 B3 B4 A4 A5 B5 B6 A6. B1/B2, B3/B4 and B5/B6 run back to back, and so do A2/A3 and A4/A5. Runner noise that lasts about two runs, which the hosted A/A record documents, therefore moves same-side pairs together. The exact Mann-Whitney null (PerformanceComparison.psm1:197-232) assumes exchangeable, independent runs and reports p down to 0.0022, but the effective sample is closer to three clusters per side. With clusters the p-value is anti-conservative: the minimum attainable p for 3 against 3 is 0.10. The confirmation step makes this worse: a B-side control compares two adjacent runs (B2 vs B1), so a burst covering both reads as 'held'. The second hosted A/A shows the pattern (B1 387, B2 388 slow …
- **Failure**: A PR that changes src is measured while the runner has three short slowdowns that land on B1-B2, B3-B4 and B5-B6. All six candidate runs rank above all six baseline runs (p=0.0022, beyond the 5% band). Every control (A2/A1, B2/B1, A4/A3, B4/B3, A6/A5, B6/B5) compares two runs from the same episode and stays inside its band, so the metric is 'confirmed'. The PR is failed as a 'Confirmed degradation' caused by runner noise. With only two such episodes (5 of 6 slow, p=0.026) the result is 'inconclusive' and the PR still fails.
- **Fix**: The minimal fix leaves the A,B,B,A crossings and the rank test alone and moves the confirmation controls off adjacent runs. In Get-PairedRunSchedule, define the same-binary controls across passes: for pass p &gt; 1, compare B(2p-1) with B(2p-3) and A(2p-1) with A(2p-3), or compare each side's first run of one pass with its first run of the next. A burst that spans two consecutive slots can then never sit on both runs of a control. Also stop running same-side runs back to back. Either alternate t …
- **Test**: 1. Add a unit test in Tools/tests for Get-PairedRunSchedule -Repetitions 3. For every comparison with Control=$true, find the step indices of its Candidate and Baseline and assert they differ by more than 1. This fails today: B2 and B1 are at indices 2 and 1. 2. Add a gate test in Test-BenchmarkGate …

### tooling-perf-gate-3

**Restore-HarnessOverlay overwrites or deletes harness files in a named tree without checking whether they changed during the run**  
`Tools/PairedRun.psm1:139` · #31 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid; #66 widened it: the overlay now also writes legacy fixture names into old revisions (PairedRun.psm1:14-18).
- **What**: With -BaselinePath or -CandidatePath, performance-paired.ps1 writes this checkout's harness files (performance.ps1, the comparator module and the benchmark headers) into the developer's own tree and restores them in a finally block. The restore copies the backup over each 'replaced' file and removes each 'created' file unconditionally. It never checks that the file still holds the overlay content it wrote. A paired run takes 10 to 60 minutes, and the named-tree mode exists so that a developer's in-progress tree can be measured as it is. That tree is often the one the developer is editing, and the benchmark headers are exactly the files being worked on.
- **Failure**: A developer runs performance-paired.ps1 -BaselineRevision main -CandidatePath ..\feature. While the 40-minute run is going, they edit ..\feature\Tests\Embedded\ComplexUiBenchmark.h, which the overlay had replaced. When the run ends, the finally block copies the pre-run backup over it, and those uncommitted edits are lost without a warning. A harness file the overlay 'created' and the developer then edited is deleted.
- **Fix**: 1. In Copy-HarnessOverlay, add `writtenHash = (Get-FileHash -LiteralPath $from -Algorithm SHA256).Hash` to each record whose action is not 'unchanged'. 2. In Restore-HarnessOverlay, before restoring or removing a file, compare its current SHA-256 with `writtenHash`. If the file exists and the hash differs, do not touch it. Write a warning that names the file and the backup path, and treat any directory that holds it as not empty. 3. Optionally, have performance-paired.ps1 print the list of overl …
- **Test**: Add two cases to Tools/tests/Test-PairedRun.ps1. Replaced file: 1. Make a target tree whose harness file differs from the source, and run Copy-HarnessOverlay with a backup directory. 2. Write new content 'developer edit' to the overlaid file in the target. 3. Call Restore-HarnessOverlay. 4. Assert t …

### tooling-perf-gate-4

**Allocation counting lives in EmbeddedTests.cpp, outside the shared harness, so each side measures cppAllocations/composeAllocations with its own instrumentation**  
`Tools/PerformanceComparison.psm1:19` · #31, #46 · architecture · confirmed (batch: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The paired design relies on one harness measuring both trees: the benchmark inputs are overlaid onto both sides and hashed into benchmarkSha256, and Assert-MatchedFixture rejects pairs whose hashes differ. However, the global operator new/delete overrides and the countAllocations/allocations thread_locals are defined in Tests/Embedded/EmbeddedTests.cpp, which is not a benchmark input and is not overlaid. ComplexUiBenchmark.h only toggles them. The baseline is therefore compiled with the base's counting code and the candidate with the PR's. benchmarkSha256 still matches, so the set is judged as one fixture. EmbeddedTests.cpp is the file that collects embedded functional tests, so PRs edit it often.
- **Failure**: A PR adds a library allocation in dirty preparation. The same PR edits EmbeddedTests.cpp so the plain operator new forwards to an aligned or other path that does not increment 'allocations' (or simply restructures the counters). The candidate's cppAllocations stays flat or drops while the baseline counts with the old code. benchmarkSha256 is equal, Compare-PerformanceSet accepts the set, the exact budget shows no rise or reads 'improved', and the gate passes a real allocation regression. The reverse case, where a counting fix raises the candidate only, fails the PR as a 'confirmed degradation' …
- **Fix**: Move the counting operator new/new[]/delete/delete[] and their thread_locals (countAllocations, allocations, allocationBytes, countLiveBytes, liveBytes) into a dedicated header such as Tests/Support/AllocationCounter.h. EmbeddedTests.cpp should include it, not ComplexUiBenchmark.h: if ComplexUiBenchmark.h included it, the overlay would put it into a pre-move baseline whose EmbeddedTests.cpp still defines operator new, giving duplicate definitions. Add the header to $script:BenchmarkInputs. This …

### tree-2

**Press/double-click read the model through a hit test made before focus callbacks that may change or clear the model**  
`src/Controls/DxUi.Tree.cpp:1128` · #57, #35 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #57 changed OnMouseDown and OnMouseDoubleClick to call FocusControlAndSurvive. Its contract says the focus callbacks "may rebuild the controls", but the fix only handles the tree being destroyed. The hit test (`hit`, computed at line 1080 / 1203) is made before focus moves, and is then used against `_model` without a re-check. A focus-changed callback that leaves the tree alive but changes its data (it calls `tree-&gt;SetModel(nullptr)`, or filters the rows and calls `NotifyDataChanged`) leaves `hit.visibleIndex` stale or `_model` null.
- **Failure**: An app's SetOnFocusChanged handler re-filters the layers panel when it gains focus: 10 rows become 3, then NotifyDataChanged. The user clicks row 8 of the unfocused tree. FocusControlAndSurvive returns true and `_model-&gt;GetVisibleItem(8, hitItem)` reads out of range. That is UB with operator[], and `std::out_of_range` inside a WndProc with the test helper's `.at()`. If the handler calls SetModel(nullptr), it dereferences null. OnMouseDoubleClick has the same problem at line 1214.
- **Fix**: Use the same order as Grid. In both Tree handlers, call FocusControlAndSurvive first, before the hit test, then hit-test against the model as it is after focus moved. OnMouseDown: ``` if (! _model || _model-&gt;GetVisibleItemCount() == 0u || ! PointInRect(GetHitBounds(), point)) return false; // cheap pre-check keeps 'not handled' semantics if (! FocusControlAndSurvive(host, *this)) return true; const HitInfo hit = HitTestPoint(MakePointDip(point)); if (hit.zone == HitZone::None) { Invalidate(ho …
- **Test**: Add a variant of RequireFocusReplacementLeavesControlAlone, used by TestTreeInputLeavesATreeTheFocusCallbackDestroyed in Tests/Controls/DxUiTests.Tree.cpp. In it, the host's SetOnFocusChanged callback leaves the tree alive and changes its data. (a) Call `tree-&gt;SetModel(nullptr)`, then OnMouseDown …

### tree-3

**Multi-selection drag accepts drops onto selected rows or into another selected row's subtree, which the documented whole-selection move turns into a cycle**  
`src/Controls/DxUi.Tree.cpp:333` · #35 · api-design · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #35 documents that when the dragged row belongs to the selection, OnTreeReorder 'may move every id in GetSelectedItemIds'. The docs/controls.md sample does exactly that: `MoveLayers(wholeSelection ? selection : ..., drop.targetId, drop.place)`. ResolveReorderDrop only rejects the source row and the source's own visible subtree. It still shows the insertion marker for, and commits, a target that is itself selected or lies inside the subtree of another selected row. For a whole-selection move that drop means moving a row relative to itself, or into its own descendant.
- **Failure**: Selection = {P (expanded parent), X}. Press X (a row of the multi-selection, so the selection is kept) and drag onto P's child C, middle zone -&gt; TreeDropPlace::Inside on C. The marker shows a valid drop and OnTreeReorder{source=X,target=C,Inside} is committed. The documented delegate moves {P, X} into C, so P becomes a child of its own descendant: a corrupted or cyclic consumer model, or a silent refusal after a drop the UI showed as valid. Dropping Before/After another selected row (e.g. 11 when {11,13} is dragged by 13) is likewise ill-defined.
- **Fix**: When the drag is armed with `_reorderCollapsesSelection == true` (the source belongs to a multi-selection), compute the forbidden visible-index ranges once in ResolveReorderSource. Each range is [selectedIndex, subtreeEnd) for every visible selected row; the selection is pre-order, so this is a single linear pass. Store them in a reused member vector of index pairs with reserved capacity, not reallocated per move. NotifyDataChanged already calls ResolveReorderSource after a model change, so the …
- **Test**: Add a test in Tests/Controls/DxUiTests.Tree.cpp using a nested fixture: P (id 10, expanded) with children C1 (11, hasChildren) and C2 (12), then sibling X (13). Enable multi-select and reorder. Click P, then Ctrl+click X to get selection {10,13}. Press X, move to the vertical middle of C1's row and …

### tree-4

**UI Automation SetFocus moves a tree's focused item (and, single-select, its selection) without telling the delegate, contrary to the OnTreeSelectionChanged contract**  
`src/Controls/DxUi.Accessibility.cpp:8435` · #35 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The header says OnTreeSelectionChanged fires when 'A user gesture (pointer, keyboard, typeahead or UI Automation)' lands on the focused item. The docs sample tracks `_activeLayer` from it ('The focused row moved (a click, a key, typeahead or UI Automation)'). The UIA SetFocus path calls the silent setter SetFocusedItemId. With multi-select (new in #35) the tree's focused item moves while the app's active item stays put. In single-select (pre-existing) the selected item changes with no callback. The provider comment there ('The selection's delegate may rebuild the controls') assumes a delegate call that never happens.
- **Failure**: A Narrator user calls SetFocus on 'Layer 5' (e.g. via the item's Focus action). The tree focuses (single-select: selects) Layer 5 and UIA announces it, but the app's property pane still shows Layer 2. A later Delete/Ctrl+Space command or keyboard gesture acts from the tree's focus, while app commands keyed on OnTreeSelectionChanged act on Layer 2.
- **Fix**: Recommended fix: make tree UIA SetFocus notify, consistent with Grid. 1. Add a private notifying request to Tree, for example `bool RequestFocusVisibleItem(size_t visibleIndex) noexcept`. It returns `SelectVisibleIndex(visibleIndex, _multiSelect ? SelectMode::FocusOnly : SelectMode::Replace, true)` behind the same bounds check as RequestSelectVisibleItem. 2. In ExecuteSetFocus, resolve the item's visible index (as the Select path does with ResolveTreeVisibleIndex) and call this request. If it re …
- **Test**: Add a test in Tests/Controls/DxUiTests.Accessibility.cpp. Setup: - Build a Tree with a counting ITreeDelegate that records OnTreeSelectionChanged ids and OnTreeSelectionSetChanged calls. - Give it items 1..4 and select item 1 through a gesture or RequestSelectVisibleItem. Reset the counters. - Get t …

### tree-5

**UIA SetFocus on a tree item focuses the tree through a raw pointer after a publish that #60 says can destroy it**  
`src/Controls/DxUi.Accessibility.cpp:8435` · #35, #53, #60 · lifetime · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #60's premise is that raising UI Automation events during a publish can dispatch a message that destroys the tree, so it added lifetime checks after RefreshAccessibilitySnapshot in Tree. ExecuteSetFocusOnWindowThread for a TreeItem calls tree-&gt;SetFocusedItemId(item.id), which returns void and ends with RefreshAccessibilitySnapshot. It then checks only survived(), which compares ResolveHost() with the old host, and calls host-&gt;SetFocusControl(tree) with the raw pointer. SetFocusControl immediately calls control-&gt;GetLifetimeToken() on it. The publish raises events in both modes. Single-select: SetFocusedItemId forwards to SetSelectedItemId, and #53 raises selection events for that change. Multi-select with the tree focused: the focus …
- **Failure**: Narrator calls SetFocus on an item of a single-select tree. SetSelectedItemId publishes, and RaiseWindowHostSelectionChanges raises ElementSelected. While UIA's outgoing call is in progress, an STA dispatches a posted message whose handler replaces the root (the SelectionEventInterruption scenario #60 tests for Grid). The host is unchanged, so survived() is true, and host-&gt;SetFocusControl(tree) calls GetLifetimeToken() on the freed Tree: a heap use-after-free.
- **Fix**: In ExecuteSetFocusOnWindowThread, take the token before the call and check it after: const std::weak_ptr&lt;int&gt; treeLifetime = tree-&gt;GetLifetimeToken(); tree-&gt;SetFocusedItemId(item.id); if (treeLifetime.expired() || ! survived()) return UIA_E_ELEMENTNOTAVAILABLE; host-&gt;SetFocusControl(tree); Another option is to re-resolve with ResolveMutableTreeControl() after the call and return UIA_E_ELEMENTNOTAVAILABLE when it is null. The single-select RemoveFromSelection branch needs no lifeti …
- **Test**: Extend the Accessibility suite, next to TestTreeSelectionPublishReplacementStopsRequests: 1. Build a single-select SelectionPublishTreeWindow (fills its window, and beside a button) with a listening SingleControlClient. 2. Get the IRawElementProviderFragment of a tree item that is not selected. 3. U …

### tree-6

**Embedded views raise no focus event when the focused tree item moves, so focus-only moves in a multi-select tree are silent**  
`src/Controls/DxUi.Accessibility.cpp:9867` · #35, #53 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The window host diffs focusedFragment, including the tree item id, through SameFocusedElement and announces any change. The embedded diff in RaiseEmbeddedAccessibilityChanges looks only at control-level state: UIA_HasKeyboardFocus and UIA_AutomationFocusChanged are raised when controlHasFocus flips, and on the control's own provider. Yet the embedded GetFocus returns the focused TreeItem fragment. #35 separates the focused item from the selection. Ctrl+Up/Down/Home/End, Ctrl+click toggling an item off, and an expander click move only the focus, so the selection diff (#53) raises nothing. For a client of an embedded view these moves produce no event at all, even though the spec says only the focused item reports HasKeyboardFocus and that emb …
- **Failure**: In RedXe's embedded view, a Narrator user in a multi-select tree presses Ctrl+Down three times to reach an item and then Ctrl+Space. The first three keys produce no UIA event, so Narrator says nothing and the user cannot tell which item Ctrl+Space will toggle. The same keys in a WindowHost announce each item.
- **Fix**: 1. In `RaiseEmbeddedAccessibilityChanges`, after the record loop and before `RaiseSelectionEvents`, compare the two snapshots with `SameFocusedElement(*previous, *current)`. Reuse that helper. 2. If the focus changed, `current-&gt;focusedFragment` has a value, and `connected()` holds: - Build the provider for that fragment, as the window host's announce does (around 9250-9262). Use the `TreeItemTag` constructor for a TreeItem, the GridRow constructor for a GridRow, or the control constructor oth …
- **Test**: Extend Tests/Embedded/EmbeddedUiaTests.h with a multi-select variant of TestEmbeddedSingleTreeEventsReachAClientSubscribedToTheApplicationsWindow. Steps: 1. Call `tree-&gt;SetMultiSelectEnabled(true)`, focus the tree and click item 2. 2. Dispatch Ctrl+Down (VK_DOWN with Ctrl held) and publish. 3. Ex …

### uia-lifetime-threading-10

**UIA SetFocus on a single-select tree item silently replaces the selection without calling the tree delegate (Grid rows do call theirs)**  
`src/Controls/DxUi.Accessibility.cpp:8435` · #35, #57 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ExecuteSetFocusOnWindowThread handles a TreeItem by calling Tree::SetFocusedItemId. Without multi-select that calls SetSelectedItemId, a silent setter that never calls ITreeDelegate::OnTreeSelectionChanged. The publish that follows still diffs the selection and raises ElementSelected and the IsSelected changes, so screen readers are told the item is selected. The application is never told. The GridRow and GridCell SetFocus branches of the same function use Grid::RequestSelectRow, which does notify the grid delegate, so the two controls behave differently. #57 added the comment 'The selection's delegate may rebuild the controls' and a survived() check after SetFocusedItemId, but no delegate runs on that path. Its own test skips the tree SetF …
- **Failure**: A user runs Narrator with 'system focus follows the Narrator cursor', or another client that calls IUIAutomationElement::SetFocus, and moves onto item B of a single-select Tree while the application shows details for item A. The tree paints B as selected and Narrator says 'B, selected', but the application's detail pane still shows A. If the user then presses Enter, Tree::OnKeyDown calls OnTreeItemInvoked(B) while the application's state is A, which can open or act on the wrong item.
- **Fix**: In the TreeItem branch of the UIA SetFocus handler, resolve the visible index and select through the notifying path instead of calling the silent setter. For a single-select tree: ``` size_t visibleIndex = 0u; if (! tree || ! ResolveTreeVisibleIndex(visibleIndex)) return UIA_E_ELEMENTNOTAVAILABLE; if (! tree-&gt;RequestSelectVisibleItem(visibleIndex)) return UIA_E_ELEMENTNOTAVAILABLE; // false also covers a tree its delegate destroyed if (! survived()) return UIA_E_ELEMENTNOTAVAILABLE; ``` Then …
- **Test**: 1. In TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus, remove the `if (tree && action == Action::SetFocus) continue;` skip. Before the fix, the replacing tree delegate never runs on tree SetFocus, so the case's expectations fail: the element should report itself gone and nothing sho …

### uia-lifetime-threading-11

**Text range Move reports moved=1 without moving at the last character, and for every Paragraph/Page/Document move, so 'move until 0' clients never stop**  
`src/Controls/DxUi.Accessibility.cpp:5125` · pre-existing · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: For a non-degenerate range, Move(Character|Document) moves range.start by count and then expands to the enclosing unit at the new position. At the last character the start moves to text.size() and the call reports moved=1. GetEnclosingTextRangeCharacterSpan(text, size) then steps back to the last character, so the range is unchanged. For Document the start becomes size with moved=1, and the enclosing unit is again [0,size). NormalizeAccessibilityTextUnit maps Paragraph and Page to Document, so a paragraph range moved by Paragraph always reports one unit moved and stays [0,size). UIA requires moved to count units actually moved, which is 0 at the boundary. This code predates the window but is in the text-range methods the first pass did not …
- **Failure**: In a DxUi TextField, a screen reader keeps an expanded character range and asks for the next character at the end of the text. Every request returns moved=1 with the same range, so the last character is spoken again instead of the reader saying 'end'. A client that reads by paragraph by calling Move(Paragraph, 1) until it returns 0 never terminates: each call returns 1 and the whole text again. Move(Word, 1) from the last word returns moved=1 with an empty range at the end, which adds one blank stop.
- **Fix**: This fix applies only to the non-degenerate branch of Move. The endpoint helpers stay unchanged, because MoveEndpointByUnit needs them to reach text.size(). 1. Character and Document: when `!collapsed`, step through the units one at a time. Count a forward step only if the new start is below text.size() (a unit starts there). Count a backward step only if the position actually changed. For Document this always gives moved=0, since there is one unit, and the range stays [0, n]. If the count reach …
- **Test**: Add a provider-level test that builds a TextField with L"ab" and gets its ITextRangeProvider. Each case below fails today. 1. Expand the range to Character at the last character [1, 2]. Move(TextUnit_Character, 1) must return moved==0 with the range still [1, 2]. It returns 1 today. 2. Use the docum …

### uia-lifetime-threading-12

**Host focus/hover/capture pointers are validated by address only; a new control allocated at a destroyed focused control's address silently inherits keyboard focus**  
`src/Controls/DxUi.WindowHost.cpp:4167` · #29, #57 · lifetime · plausible (single: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Removing a subtree (Panel::ClearChildren, an erase of children) does not tell the host. The host keeps _focusedControl (and _hoveredControl, _capturedControl, _pendingPointerDoubleClick.target) as raw pointers and decides at the next message, in PruneStaleInteractionState, whether they are still alive. It does this only by comparing addresses during a tree walk (ResolveControlInteractionState). If the application destroys the focused control and allocates a replacement in the same callback, the heap may return the same block. The stale pointer then compares equal to a live control in the tree, and the prune keeps it. #57 added lifetime-token checks around the focus callbacks (IsFocusedControlStillHeld), but the prune, which every message ru …
- **Failure**: A click handler rebuilds a button list with ClearChildren() followed by AddChild&lt;Button&gt;(...). The focused Button is freed and the next Button allocation reuses its block, which LFH may hand back. At the next message the prune finds that address in the tree, so _focusedControl now names the new button. That button never received OnFocusChanged(true), so it draws no focus ring and HasFocus() is false. The next Space or Enter goes to it and runs its onClick, a command the user never focused, while a screen reader is told it has focus. AddressSanitizer's quarantine prevents the reuse, so th …
- **Fix**: 1. Pair each retained interaction pointer with a `std::weak_ptr&lt;int&gt;` taken from `GetLifetimeToken()`. This means `_focusedControl`, `_hoveredControl`, `_capturedControl` and `_pendingPointerDoubleClick.target`. Simplest is a small `{Control* ptr; std::weak_ptr&lt;int&gt; life;}` struct, or a private setter, so that every assignment sets both. 2. In `PruneStaleInteractionState`'s `classifyInteractionState`, return `{}` (not in the tree) when the lifetime has expired, before the address wal …
- **Test**: Add a WindowHost control test that forces the address reuse: 1. Build a root Panel with a Button A whose onClick sets a flag, and focus A through the host (SetFocusControl). 2. Take the child slot from `Panel::GetChildren()` and call `Control* p = slot.release(); p-&gt;~Control(); new (p) Button(... …

### uia-lifetime-threading-2

**A publish's GetWindowTextW re-enters HandleMessage; the prune's new nested republish is overwritten by a stale snapshot that still names the disabled control as focused**  
`src/Controls/DxUi.Accessibility.cpp:953` · #29 · reentrancy · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: `PublishWindowHostAccessibilitySnapshot` works out the focused fragment and every record's `controlHasFocus`, then calls `GetWindowTextW` on the host window. For a window owned by this process that sends WM_GETTEXT to its own procedure synchronously. Every DxUi window procedure forwards all messages to `HandleMessage`, which runs `CancelStaleCapture()` and `PruneStaleInteractionState()` before anything else. Since #29, a prune that removes the focus calls `RefreshWindowHostAccessibilitySnapshot`. The recursive global mutex lets that call run a nested publish, store it and raise its events. The outer publish then compares its own earlier-built snapshot against the nested one, stores it on top, and announces the result.
- **Failure**: In a click handler the app disables the focused button while it saves: `saveButton-&gt;SetEnabled(false)`. Control::SetEnabled refreshes, and the outer publish records the button as focused, with HasKeyboardFocus true. `GetWindowTextW` then sends WM_GETTEXT, and the prune drops `_focusedControl` because the button is disabled. The nested publish stores 'no focus' and announces the window. The outer publish then stores its stale snapshot (focus = the disabled button), sees a focus move from none to the button, and announces the disabled button. Narrator hears the window and then the disabled bu …
- **Fix**: Smallest fix: in PublishWindowHostAccessibilitySnapshot, read the window title before reading any host state, so any re-entrant prune and republish finishes first. The outer snapshot then sees the pruned focus and compares itself with the nested one correctly. Better still, read the title with InternalGetWindowText(target.hwnd, ...), which sends no message, so a publish never re-enters the window procedure while it holds the accessibility mutex. The menu popup, test and NativeMenuInterop procedu …
- **Test**: Add a test next to TestWindowHostPublishSkipsAFocusedControlItsPanelRemoved. Create an AttachedHostWindow with a Panel root holding two Buttons, call SetFocusControl(first), then first-&gt;SetEnabled(false) without pumping messages. Then: - Require that window.Host().GetFocusControl() == nullptr. Th …

### uia-lifetime-threading-3

**Window-host text-range providers still pin a full snapshot; one is raised per caret move (incomplete #29 fix)**  
`src/Controls/DxUi.Accessibility.cpp:4218` · #29 · resource-leak · plausible (trace: plausible/medium; refute: plausible/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 found that UI Automation keeps event providers alive while a client listens. Each window-host provider pinned a full snapshot, measured at tens to hundreds of KB per event that stayed until the window closed. #29 stopped element providers from pinning (`CaptureProviderCreationSnapshot` returns null for window hosts), but all three `AccessibilityTextRangeProvider` constructors still pin `CaptureAccessibilitySnapshot(target, hwnd)`. For window hosts that snapshot is used only by ResolveText, GetEnclosingElement and the DPI scale. `RaiseWindowHostTextInputAutomationEvent` creates a new range for every ActiveTextPositionChanged event, and the text-input path republishes a new snapshot right before each event.
- **Failure**: With Narrator running, the user types or moves the caret in a TextField inside a window that also has a Grid or a large Tree. Each keystroke publishes a new O(controls) snapshot and raises ActiveTextPositionChanged with a range that pins it. If UIA retains the event arguments as #29 measured for event providers, memory grows by roughly one snapshot per keystroke until the window closes. The same applies to ranges clients keep from GetSelection or DocumentRange.
- **Fix**: Make text ranges follow the element-provider rule: pin the snapshot only for embedded ranges. Replace the three `_snapshot(CaptureAccessibilitySnapshot(target, hwnd))` initializers with `_snapshot(CaptureProviderCreationSnapshot(target, hwnd))` (move that helper above the class). For window hosts, add small members filled at construction from the current snapshot's record for `_path`: `std::optional&lt;std::wstring&gt; _creationText` (set when `controlSupportsText`), `bool _supportsText`, and `f …
- **Test**: Add a retention test in the Accessibility suite. Build a window host with a TextField and a large Grid or Tree (a snapshot of several hundred KB). Keep N=50 `ITextRangeProvider`s from the TextField's `ITextProvider::get_DocumentRange`, typing one character between acquisitions so a new snapshot is p …

### uia-lifetime-threading-4

**Process-wide accessibility mutex is held across application callbacks and UIA event raising in most Execute* actions**  
`src/Controls/DxUi.Accessibility.cpp:8716` · #29, #57 · threading · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 made UIA SetFocus "resolve under the accessibility mutex and act without it", and the comments say events are "Raised outside the publish lock: clients may call back into these providers from other threads". The other actions still take the global `std::recursive_mutex` for their whole body. They then call into application code (selection delegate, focus-changed callback, onChanged/value callbacks, tree expand delegate) and `RefreshWindowHostAccessibilitySnapshot`, which raises UIA events while the outer lock is held. #57 added survival checks here precisely because these callbacks may rebuild the UI, so they run long and arbitrary application code under a lock that every window's publish and every provider call in the process needs, in …
- **Failure**: (1) Stall: Narrator expands a Tree node through ExpandCollapse. `tree-&gt;RequestExpandedState` runs the application's delegate, which for example enumerates a directory, under the mutex. Every Narrator call into any DxUi window in the process blocks on the mutex for that whole time, even though those calls only read snapshots. (2) Deadlock in a multi-UI-thread app: a grid's selection delegate, run by UIA Select on thread A, calls `RaiseWindowHostAccessibilityNotification(hwndB, ...)` or SendMessage to a window on thread B. Thread B's handler, or any publish it is running, blocks on the mutex …
- **Fix**: Apply the ExecuteSetFocusOnWindowThread structure to each Execute* action. In a scoped block under the lock: resolve the host, control, tree or grid, and the index or item data, and capture GetControlLifetimeToken or the tree's lifetime token. Release the lock. Then call the application-facing operation (RequestSelectVisibleItem / RequestSelectRow / RequestExpandedState / OnMnemonic / SetTextAndNotify / RequestValue / RequestPosition / SetFocusControl). Between steps, check survival with the lif …
- **Test**: Add a Controls/Accessibility test with a Tree whose ITreeDelegate::OnTreeToggleExpanded (and, as a second case, a Grid selection delegate) starts a std::thread. That thread calls a read-only provider method on another element of the same or a second window (e.g. GetPropertyValue(UIA_NamePropertyId) …

### uia-lifetime-threading-5

**Elements created with identity 0 cannot detect their control's destruction; ExecuteSelect can then focus a freed Grid or Tree**  
`src/Controls/DxUi.Accessibility.cpp:5654` · #29, #55, #57 · lifetime · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #55 and #57 rely on `survived() == (ResolveHost() == host)`, which works only if CaptureSnapshot's identity check can fail. A window-host element's identity is read in its constructor from whatever snapshot is current at that moment. The caller (Navigate, GetFocus, ElementProviderFromPoint, GetSelection), however, chose the path from a snapshot it loaded earlier. If a publish happens between those two loads, `FindControlNavigationRecord` finds no record and the identity is `{}`. That can be the empty stand-in SetRoot publishes while the old tree is destroyed, or a removal. With identity 0, GuardedElementSnapshot always returns the snapshot and ResolveIdentifiedControlAtPath matches by path alone, permanently. `Grid::RequestSelectRow` return …
- **Failure**: A Narrator thread calls Navigate while the UI thread is inside SetRoot, between PublishEmpty and the final refresh, and gets a GridRow element with identity 0. That element now resolves to whatever Grid later occupies the same path. When Select is invoked on it, `ExecuteSelectOnWindowThread` → `grid-&gt;RequestSelectRow` runs the selection delegate, which rebuilds the UI. `survived()` stays true because identity 0 is never checked, so `host-&gt;SetFocusControl(grid)` dereferences the freed grid (`control-&gt;GetLifetimeToken()`). Even without the crash, such an element acts on a replacement co …
- **Fix**: The minimal fix is local to the Execute paths, and it also simplifies them. In ExecuteSelectOnWindowThread and ExecuteAddToSelectionOnWindowThread, and in any other Execute* path that calls a delegate-bearing request on a Tree or Grid, capture `const std::weak_ptr&lt;int&gt; lifetime = GetControlLifetimeToken(*grid)` (or `*tree`) before the call. After the call, and again after SetFocusControl, return UIA_E_ELEMENTNOTAVAILABLE if `lifetime.expired()`. That replaces `survived()`'s snapshot round …
- **Test**: Add an ASan-run Controls test that creates a GridRow provider with identity 0 deterministically. Use a test hook that makes the element-creation path see an empty published snapshot: call PublishEmptyWindowHostAccessibilitySnapshot (as SetRoot does) and then create the provider through GetFocus or N …

### uia-lifetime-threading-6

**Embedded hosts announce focus only for whole controls, never for the tree item or grid row that GetFocus reports**  
`src/Controls/DxUi.Accessibility.cpp:9867` · #29, #32 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 and #32 made window hosts raise the focus-changed event for the element GetFocus reports: a tree's focused item, a grid's focused row, a control, or the window. For embedded hosts, `PublishWindowHostAccessibilitySnapshot` skips the focus diff (`if (! target.embedded)`). `RaiseEmbeddedAccessibilityChanges` raises focus only when a control record's `controlHasFocus` flips, and it raises it on the control element. The embedded root's GetFocus, meanwhile, returns the focusedFragment (the TreeItem or GridRow). The spec says "embedded hosts raise theirs from the snapshot diff", but that diff never looks at the focused fragment.
- **Failure**: In an embedded view (RedXe), the user tabs into a Tree. The event names the Tree control, while GetFocus answers the selected item. Arrowing between items raises no focus-changed event at all. Ctrl+Arrow in a multi-select tree moves focus without changing the selection, so nothing at all is announced. A grid in Extended mode behaves the same way. Screen-reader users cannot follow keyboard focus inside embedded trees and grids.
- **Fix**: 1. In PublishWindowHostAccessibilitySnapshot, compute `changes.focusMoved = ! SameFocusedElement(*previous, *snapshot)` for embedded targets as well. Keep structureChanged window-only if that is intended. 2. Pass `changes.focusMoved` into RaiseEmbeddedAccessibilityChanges. 3. In RaiseEmbeddedAccessibilityChanges, keep the HasKeyboardFocus property change on the control record, but remove the per-record UIA_AutomationFocusChangedEventId raise. Instead, after the record loop, when `focusMoved && t …
- **Test**: Add an embedded-host UIA test that uses the existing in-process UiaTestClient focus handler (IUIAutomationFocusChangedEventHandler). Attach accessibility to an EmbeddedHost whose root holds a Tree with 3 items, with placement.hasKeyboardFocus = true. 1. Focus the Tree, prepare, and call UpdateAccess …

### uia-lifetime-threading-7

**SetFocusControl prunes first and announces the window before the requested focus, giving a double announcement on rebuild-and-focus**  
`src/Controls/DxUi.WindowHost.cpp:4205` · #29 · ux · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: `SetFocusControl` starts with `PruneStaleInteractionState()`. Since #29, a prune that drops the focus republishes, and that publish announces focus right away. When the focused control was removed in the same turn (ClearChildren publishes nothing), the prune publishes 'no focus', and AnnounceWindowHostFocus raises the focus event on the window root. SetFocusControl then focuses the requested control and announces that too. The intermediate 'window has focus' state is internal to one call and should never reach clients.
- **Failure**: A list rebuild followed by focusing the first new item in the same handler: `list-&gt;ClearChildren(); list-&gt;AddChild&lt;...&gt;(...); host.SetFocusControl(first);`. The same happens with #57's case of a focus-changed callback that rebuilds and refocuses. Narrator interrupts itself to read the window title and then the new control on every such rebuild. The removed control's StructureChanged and focus events are also raised from inside SetFocusControl, before the new focus exists.
- **Fix**: Let the caller decide whether a prune that drops focus publishes. Change PruneStaleInteractionState to `bool PruneStaleInteractionState(bool publishFocusLoss = true) noexcept`, returning prunedFocus and calling RefreshWindowHostAccessibilitySnapshot only when publishFocusLoss is true. In SetFocusControl, call `const bool prunedFocus = PruneStaleInteractionState(false);`. Every path after that must still publish once: - The `_focusedControl == control` path already publishes. - The main path alre …
- **Test**: Add an interactive accessibility test (it needs foreground keyboard focus, so it belongs with the -Interactive suites): 1. Create an AttachedHostWindow that holds keyboard focus, and register a UIA focus-changed handler (for example with the Controls.Tests.DxUiFocusEventClient helper) so UiaClientsA …

### uia-lifetime-threading-8

**Every selected grid row reports HasKeyboardFocus, not just the focused row (the Tree fix was not applied to Grid)**  
`src/Controls/DxUi.Accessibility.cpp:6090` · #29, #32 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: The focused grid element that GetFocus and the host's focus announcement report is the primary selected row (`GetPrimarySelectedRow`). A GridRow's UIA_HasKeyboardFocus, however, is true for every selected row while the grid has focus. #35 changed TreeItem to `SnapshotTreeItemIsFocused`, but GridRow still uses selection membership. That contradicts the window's other focus reporting, which #32 made authoritative.
- **Failure**: In an Extended-selection grid with five rows selected, all five rows answer HasKeyboardFocus = TRUE. Clients that find the focused element by property, such as UIA FindFirst with HasKeyboardFocus, Narrator's focus verification or Accessibility Insights checks, get the wrong row, or several focused elements.
- **Fix**: Add `std::optional&lt;uint64_t&gt; gridFocusedRowId` to AccessibilityControlNavigationSnapshot (near line 516). Fill it in the Grid snapshot branch (around line 1827) from the same source as focusedFragment: `if (auto primary = grid-&gt;GetPrimarySelectedRow()) record.gridFocusedRowId = model-&gt;GetStableRowId(*primary);`. Add `SnapshotGridRowIsFocused(record, rowId)` that returns `record.gridFocusedRowId == rowId`. Change line 6090 to `record-&gt;gridHasFocus && SnapshotGridRowIsFocused(*recor …
- **Test**: In Tests/Controls (next to the Tree check that uses ReadProviderBoolProperty(..., UIA_HasKeyboardFocusPropertyId, ...) around DxUiTests.Accessibility.cpp:4117), add a Grid test. Create a GridSelectionMode::Extended grid with 5 rows, focus it, select rows 1, 2 and 3 (row 3 last, so it is primary), an …

### uia-lifetime-threading-9

**The window's root element is announced as focused but answers HasKeyboardFocus=false, so clients that check it drop the event**  
`src/Controls/DxUi.Accessibility.cpp:6266` · #29, #32 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: #29 and #32 made the host raise UIA_AutomationFocusChangedEventId on the window's canonical root whenever no control has focus. This covers a pruned (disabled, hidden or removed) focused control, the end of a gain turn in a window with no focused control, and a collapsed root whose control lost focus. The root provider's GetPropertyValue still answers UIA_HasKeyboardFocusPropertyId with an explicit VARIANT_FALSE when it stands for no control. For a collapsed root it answers record-&gt;controlHasFocus, which is false in exactly this case. Because the native provider returns false explicitly, the HWND proxy's 'Win32 focus is here' value never applies. The element the host announces therefore says it is not focused. GetFocus returns null at th …
- **Failure**: A user with NVDA has focus on a Button that the application disables, or on a control the application removes. At the next message PruneStaleInteractionState republishes, focusMoved is true, and AnnounceWindowHostFocus raises FocusChanged on the canonical root. NVDA reads HasKeyboardFocus from the sender before it accepts a UIA focus event (shouldAllowUIAFocusEvent), gets false, and ignores the event. NVDA keeps the removed or disabled control as its focus object and says nothing about the window. The same happens when a seen window that has no focusable control is re-activated, because EndWin …
- **Fix**: Record the window host's Win32 focus in the snapshot at publish, or read `GetGUIThreadInfo(GetWindowThreadProcessId(hwnd), ...).hwndFocus == hwnd`. Then, for a window-host Root: - With no record (6266), answer HasKeyboardFocus = `!snapshot-&gt;focusedFragment.has_value() && windowHasFocus`. - With a collapsed root (6295), answer true also when `!snapshot-&gt;focusedFragment.has_value() && windowHasFocus`. Make RootElementHasKeyboardFocus use the same rule, so EndWindowHostFocusGainTurn does not …
- **Test**: Add a desktop-free WindowHost test that subscribes a recording hook to AnnounceWindowHostFocus's provider, or use the in-process UIA client in the Menu suite: 1) A window with a focused Button; disable the Button while the window holds Win32 focus; pump one message so PruneStaleInteractionState repu …

### uia-selection-semantics-1

**UIA selection and tree-expand actions run app delegates, focus callbacks and UIA event raising while holding the process-wide accessibility mutex**  
`src/Controls/DxUi.Accessibility.cpp:8716` · #35, #53, #60 · threading · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ExecuteSelectOnWindowThread, ExecuteAddToSelectionOnWindowThread, ExecuteRemoveFromSelectionOnWindowThread, the tree-item branch of ExecuteExpandOnWindowThread and the grid-cell branch of ExecuteToggleOnWindowThread all take the single process-wide std::recursive_mutex for their whole body. Inside that lock they call Tree/Grid selection requests, which call ITreeDelegate/IGridDelegate. They also call host-&gt;SetFocusControl, which runs focus callbacks, and RefreshWindowHostAccessibilitySnapshot. Since #53 that refresh also raises the focus and selection UIA events (RaiseWindowHostSelectionChanges, AnnounceWindowHostFocus). The rest of the file deliberately avoids this: Invoke and button Expand release the lock before the callback, and the …
- **Failure**: An app runs two UI threads, each with a DxUi window. Narrator calls SelectionItem.Select on a tree item in window A, and the call runs on A's thread with the mutex held. A's OnTreeSelectionChanged does SendMessage to window B on thread C, for example to update a details pane. At the same moment thread C handles a click in its own Tree and calls RefreshWindowHostAccessibilitySnapshot, which blocks on the global mutex. A waits for C and C waits for A: both UI threads deadlock. In a single-threaded app, a selection delegate that opens a modal confirmation dialog holds the mutex for as long as the …
- **Fix**: Apply the Invoke/SetFocus pattern to `ExecuteSelectOnWindowThread`, `ExecuteAddToSelectionOnWindowThread`, `ExecuteRemoveFromSelectionOnWindowThread`, the tree branch of `ExecuteExpandOnWindowThread` and the grid-cell branch of `ExecuteToggleOnWindowThread`: 1. In a short `{ scoped_lock }` block, resolve `host`, the Tree or Grid pointer, the visible/row index, the item data and the MultiSelectEnabled flag, and return early when validation fails. 2. Release the lock, then call the `Request*` meth …
- **Test**: Add a Controls/Accessibility test: a Tree with an `ITreeDelegate` whose `OnTreeSelectionChanged` (and, in a second case, `OnTreeToggleExpanded`) signals an event, then waits up to about 2 s for a worker thread to finish. The worker waits for that signal and calls `GetPropertyValue(UIA_NamePropertyId …

### uia-selection-semantics-10

**Embedded views raise no focus event when focus moves between a tree's items or a grid's rows**  
`src/Controls/DxUi.Accessibility.cpp:9867` · #35, #51, #53 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: An embedded publish computes focusedFragment (a tree item or grid row) and GetFocus reports it. RaiseEmbeddedAccessibilityChanges only raises UIA_AutomationFocusChangedEventId when a control-level record's controlHasFocus flips. PublishWindowHostAccessibilitySnapshot computes focusMoved (SameFocusedElement) only for window hosts (`if (! target.embedded)`). Moving the keyboard between items of the same focused tree or grid therefore raises no focus event in an embedded view. For single-select trees and grids #53's ElementSelected partly hides this; multi-select focus-only moves (Ctrl+arrows) are completely silent.
- **Failure**: An application embeds a DxUi multi-select Tree in its own window. A Narrator user presses Ctrl+Down to move the focus cursor without changing the selection. The view republishes from UpdateAccessibility, but no focus event is raised, so Narrator keeps reading the previous item. The same tree in a WindowHost announces every move.
- **Fix**: 1. In `PublishWindowHostAccessibilitySnapshot`, compute `changes.focusMoved = ! SameFocusedElement(*previous, *snapshot)` for embedded targets too. Keep `structureChanged` limited to window hosts, since the embedded side computes its own. 2. Pass `changes.focusMoved` (or the whole `changes`) into `RaiseEmbeddedAccessibilityChanges`. 3. In that function, stop raising FocusChanged from the control provider at line 9871; keep the HasKeyboardFocus property event there. 4. After the record loop, if ` …
- **Test**: Add an embedded UIA test next to the existing embedded receipts tests. 1. Set up an EmbeddedHost holding a multi-select Tree with at least three visible items, attach accessibility with `placement.hasKeyboardFocus = true`, and subscribe a UIA FocusChanged handler. 2. Focus the tree and select item 0 …

### uia-selection-semantics-12

**UIA SetFocus on a single-select tree item changes the selection without calling OnTreeSelectionChanged**  
`src/Controls/DxUi.Accessibility.cpp:8435` · #35, #60 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: ExecuteSetFocusOnWindowThread for a TreeItem calls Tree::SetFocusedItemId. Without multi-select that calls SetSelectedItemId, which is documented as silent. The tree's selection therefore changes, its paint and the UIA ElementSelected event follow, but the application's delegate is never told. This contradicts the delegate contract in DxUi.h:1405 ('A user gesture (pointer, keyboard, typeahead or UI Automation) made itemId the selected item'). It is also inconsistent with Grid, whose row and cell SetFocus go through RequestSelectRow, which notifies. The comment that #35/#60 added here ('The selection's delegate may rebuild the controls') assumes a delegate call that never happens. With multi-select, SetFocus moves the focused item silently t …
- **Failure**: A file-browser tree drives a details pane from OnTreeSelectionChanged. An NVDA user moves focus to a tree item (object navigation, then 'move focus to navigator object'), or a UIA test calls SetFocus on it. The tree highlights the item and Narrator or NVDA hears it selected, but the details pane still shows the previous item. Pressing Enter then invokes the item the app does not consider selected.
- **Fix**: In the TreeItem SetFocus branch, resolve the item's visible index (ResolveTreeVisibleIndex, as the Select path does) and route the change through the notifying path: - Without multi-select, call `tree-&gt;RequestSelectVisibleItem(visibleIndex)`. - With multi-select, call a new or existing Request entry that calls `SelectVisibleIndex(visibleIndex, SelectMode::FocusOnly, true)`. Leave it out if that index is already the focused item, so a redundant SetFocus does not re-notify. Keep the existing su …
- **Test**: Add three tests: 1. Single-select tree with a counting ITreeDelegate, items 1 and 2, item 1 selected. Get the UIA fragment for item 2, call SetFocus, and require S_OK, GetSelectedItemId()==2, and OnTreeSelectionChanged called exactly once with 2. This fails today because the count is 0. 2. Multi-sel …

### uia-selection-semantics-13

**Menu row focus is announced only when SynchronizeMenuAccessibility sees a host-focus change: closing a submenu, or a UIA SetFocus on a row, raises no focus event, and GetFocus names the parent row while a submenu is in use**  
`src/Controls/DxUi.Menu.cpp:3446` · #31, #56 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: A native menu popup's rows never get Win32 focus. AnnounceWindowHostFocus returns early for IsNativeMenuPopupWindow, so the only UIA focus event for a row is the RaiseWindowHostFocusChanged in SynchronizeMenuAccessibility, and only when that popup's host focus control changes there. Each popup keeps its own host focus. When a keyboard-opened submenu closes (Left or Escape go to CloseTopmostSubmenu), the parent popup's keyboardIndex and host focus are unchanged, so InvalidatePopup(parent) raises nothing. A UIA SetFocus on a row (ExecuteSetFocusOnWindowThread, nativeMenuRow) moves the host focus through SetFocusControl(control, false). That publish is skipped by AnnounceWindowHostFocus, and the posted MenuPopupAccessibleFocus then finds the f …
- **Failure**: With Narrator, a user opens a context menu with Shift+F10, arrows to 'Open with' and presses Right: the submenu's first row is announced. They press Left or Escape: the submenu window is destroyed, the element Narrator last focused is gone, and nothing is announced. The user does not know they are back on 'Open with'. Pressing Caps+Tab (read focus) while inside the submenu reads 'Open with' instead of the submenu row. Native Win32 menus announce the parent item after the submenu closes.
- **Fix**: Make the menu controller, not the per-popup host-focus diff, own row-focus announcements. 1. In CloseTopmostSubmenu and CloseSubmenuChainFrom (when closedAny is true), after InvalidatePopup(parent), check the parent. If it has a keyboardIndex that is a navigable row and its host focus equals that row, call RaiseWindowHostFocusChanged(parent.hwnd, children[*parent.keyboardIndex]). One option is a helper, AnnounceMenuPopupFocus(MenuPopup&), shared with SynchronizeMenuAccessibility. The mouse-drive …
- **Test**: Add a UIA event test in the Menu suite, alongside the existing Home/Down focus-event test (Tests/Controls DxUiTests.Menu.cpp, around line 7250). 1. Open a context menu that has a submenu, then subscribe to UIA_AutomationFocusChangedEventId. 2. Press Down until the submenu parent row is selected, the …

### uia-selection-semantics-14

**Wheel scrolling a Tree or Grid never republishes the accessibility snapshot: item bounds, hit testing and the Grid's row set stay at the old viewport**  
`src/Controls/DxUi.WindowHost.cpp:2818` · #51 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Tree item and Grid row/cell bounds, ElementProviderFromPoint and IsOffscreen are answered from point-hit records captured when the snapshot is published. Those records cover only the rows in the viewport (AppendTreeAccessibilityPointHits walks from GetFirstVisibleItemIndex; Grid's GetVisibleRowCount and GetVisibleRowAt iterate BuildVisibleBodyItems, which is clipped to the viewport). Grid's UIA row set (gridVisibleRowIds, used for navigation) is also viewport-limited. WM_MOUSEWHEEL dispatches Tree::OnMouseWheel or Grid::OnMouseWheel, which change the scroll offset and only invalidate, and the host does not republish after a wheel event (unlike mouse-up, size and focus). This window fixed the same class for menu popups (a row that moved repu …
- **Failure**: Scroll a 200-row tree with the mouse wheel or a touchpad, without clicking. Narrator's focus rectangle and BoundingRectangle for the focused item still point at the old position. Items now on screen report no rectangle, and Narrator's mouse mode, or any ElementProviderFromPoint, names the item that used to be under the pointer. In a Grid, a screen reader that walks rows with item navigation reads the rows of the previous viewport and cannot reach the rows now shown until some key, click or focus change republishes.
- **Fix**: In WindowHost's WM_MOUSEWHEEL case, follow the WM_LBUTTONUP pattern: 1. Take `target-&gt;GetLifetimeToken()` before dispatching. 2. After `OnMouseWheel` returns true, revalidate the target with `RevalidateDispatchedControl(lifetime, _root.get(), target)`. 3. If it is still alive, call `RefreshWindowHostAccessibilitySnapshot(_hwnd, this)`. Make Grid::OnMouseWheel keep returning false when the offset does not change, so wheel input at the scroll limits does not trigger a republish. Make Tree::OnMo …
- **Test**: Add a WindowHost accessibility test with a Tree of about 200 items in a short viewport: 1. Publish, then read the BoundingRectangle and IsOffscreen of item 0 and of an item about 30 rows down. 2. Send WM_MOUSEWHEEL with delta -WHEEL_DELTA*5 at a client point inside the tree. 3. Assert that item 0's …

### uia-selection-semantics-3

**SelectionItem.AddToSelection on an already-selected Grid row deselects it**  
`src/Controls/DxUi.Accessibility.cpp:8803` · #53, #60 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: AddToSelection for a grid row calls Grid::RequestSelectRow(rowIndex, MK_CONTROL). Grid::SelectRow treats Ctrl as a toggle (`_selectionModel.Toggle(rowId)`), so adding a row that is already selected removes it. The Tree path guards against exactly this: RequestAddVisibleItemToSelection uses FocusOnly when the item is already selected, with the comment 'Adding a selected item changes nothing: this is not a toggle'. Since #53 the wrong removal is also announced, with ElementRemovedFromSelection and IsSelected=false.
- **Failure**: A multi-select grid has rows 3 and 5 selected. A UIA client (Voice Access, an automation script, or Narrator's add-to-selection command) calls AddToSelection on row 5, which is valid and should be idempotent. Row 5 is toggled off, the app's OnGridSelectionChanged fires with only row 3, and the user hears that row 5 was removed from the selection.
- **Fix**: Add `bool Grid::RequestAddRowToSelection(size_t rowIndex)` next to `RequestRemoveRowSelection`: - Apply the same model, range and visibility checks. - If `_selectionMode == GridSelectionMode::Single`, call `SelectRow(rowIndex, 0u)`. - Otherwise, if `_selectionModel.IsSelected(_model-&gt;GetStableRowId(rowIndex))`, return true without calling the delegate. Optionally move focus to the row only, matching Tree's FocusOnly behaviour. - Otherwise call `SelectRow(rowIndex, MK_CONTROL)`. Use it at Acce …
- **Test**: In the grid selection accessibility test near Tests/Controls/DxUiTests.Accessibility.cpp:4678, use a multi-select grid: 1. Call AddToSelection on row 0, then on row 1. 2. Record the delegate's selection-changed count. 3. Call AddToSelection on row 1 again. 4. Require that it returns S_OK, that `grid …

### uia-selection-semantics-4

**RemoveFromSelection on a single-select Tree clears the selection silently, without telling the app's delegate**  
`src/Controls/DxUi.Accessibility.cpp:8854` · #35, #53 · bug · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: For a tree without multi-select, RemoveFromSelection calls tree-&gt;SetSelectedItemId(std::nullopt). That is the application's own setter, which by design does not notify the delegate ('the application's own setters (which are silent to the delegate, not to clients)'). Every other UIA selection request goes through the delegate: Select uses RequestSelectVisibleItem with notifyDelegate=true, the multi-select removal uses NotifySelectionSetChanged, and the Grid removal calls OnGridSelectionChanged. Since #53, clients are also told that the item left the selection, so the UI reports a state the application never received.
- **Failure**: In a single-select folder tree whose delegate drives a details pane, a UIA client calls RemoveFromSelection on the selected item. The tree drops its selection and clients hear ElementRemovedFromSelection, but OnTreeSelectionChanged is never called. The details pane keeps showing the old item, and the next keyboard or delegate-driven action runs against a selection the app believes still exists.
- **Fix**: Minimal and API-preserving: in the single-select branch, return UIA_E_INVALIDOPERATION when the item is the selected one, and S_OK as a no-op when it is not. WPF's single-select ListBoxItem behaves this way. Also have get_IsSelectionRequired report TRUE for a single-select Tree that has a selection, so clients are not invited to make the call. Update the spec line at UI_InputAndAccessibility.md:382 and the test at Accessibility.cpp:3961. Alternative, if the product wants deselection supported: a …
- **Test**: In Tests/Controls/DxUiTests.Accessibility.cpp, around line 3961: on a single-select tree whose item 10 is selected and whose recording delegate counts calls, call selectionPattern-&gt;RemoveFromSelection(). For the INVALIDOPERATION fix, assert that it returns UIA_E_INVALIDOPERATION, that tree-&gt;Ge …

### uia-selection-semantics-5

**Disabled Tree and Grid still accept UIA Select, AddToSelection, RemoveFromSelection and tree-item Expand/Collapse**  
`src/Controls/DxUi.Accessibility.cpp:8726` · #35, #53 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Tree items and grid rows report UIA_IsEnabledPropertyId from treeIsEnabled or gridIsEnabled, so a disabled tree's items read as unavailable. None of the selection or tree-expand actions check IsEnabled, and Tree::RequestSelectVisibleItem, Grid::RequestSelectRow, RequestRemoveRowSelection and RequestExpandedState do not either. A UIA client can therefore change the selection of a disabled control, call its delegate, and SetFocusControl it. The button Expand path already returns UIA_E_ELEMENTNOTENABLED, and the Value and RangeValue setters check IsEnabled.
- **Failure**: An app disables its file tree during a background operation so its selection cannot change. Voice Access or Narrator calls Select on an item, which reports IsEnabled=false. The selection changes anyway, OnTreeSelectionChanged runs in the middle of the operation, and since #53 clients hear ElementSelected for a control that claims to be disabled.
- **Fix**: Add an enabled guard once, right after the tree or grid is resolved in each executor, matching the button path: - In ExecuteSelectOnWindowThread: `if (! tree-&gt;IsEnabled()) return UIA_E_ELEMENTNOTENABLED;` and the same for `grid`. - In ExecuteAddToSelectionOnWindowThread: guard both the tree and the grid branch. Put the tree check before the single-select fallback so it covers that case too. - In ExecuteRemoveFromSelectionOnWindowThread: guard both the tree branches and the grid branch. - In E …
- **Test**: Add a test in Tests/Controls/DxUiTests.Accessibility.cpp: 1. Build a Tree with a recording ITreeDelegate and two items, one of them with children. Call SetEnabled(false) on the tree and refresh the snapshot. 2. Resolve the second tree-item provider. Require ISelectionItemProvider::Select, AddToSelec …

### uia-selection-semantics-6

**A selection that becomes exactly one new item is reported as Selection_Invalidated, not ElementSelected, whenever more than 20 items were selected before**  
`src/Controls/DxUi.Accessibility.cpp:2229` · #53, #54 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: CollectSelectionChanges checks the size-difference shortcut (`|was|-|now| &gt; 20 -&gt; invalidated`) before it detects the 'replaced' case, and its tooMany() count includes the removed items even when replaced is set. In the replaced case the removed items raise no event of their own, only an IsSelected property change, so they should not count toward the bulk threshold. The spec says 'The rules are WPF's' and puts 'exactly one item that was not selected raises ElementSelected' first. WPF's SelectorAutomationPeer checks that single-selection case before it applies the invalidate limit.
- **Failure**: In a multi-select grid or tree the user presses Ctrl+A on 500 rows, then clicks or arrows to a single row. now.size()==1 and the row was not selected before, but the size difference is 499 &gt; 20, so only Selection_Invalidated is raised on the container. The same happens with 21 to 40 previously selected items that are on screen: replaced is true, but added (1) plus removed (&gt;20) trips tooMany(). Clients that track the selection through ElementSelected (NVDA's selection announcements, magnifiers following the selected item) get no item event.
- **Fix**: In CollectSelectionChanges, decide the replaced case before the size shortcut, with a linear scan and no allocation: `if (now.size() == 1u && std::ranges::find(was, now[0]) == was.end()) { change.replaced = true; change.added.push_back(now[0]); /* optionally: push up to kAccessibilityMaxSelectionEvents on-screen ids of `was` into change.removed for their IsSelected=false property change; never set invalidated */ changes.push_back(std::move(change)); continue; }` Then remove the replaced handling …
- **Test**: Extend the TestAccessibilityTreeMultiSelectRaisesSelectionEvents sequence near Tests/Controls/DxUiTests.Accessibility.cpp:5996 and add a Grid counterpart: 1. With 30 visible items and nothing selected, select items 1-25 through the model or AddToSelection, then select item 30 alone (Select or a plai …

### uia-selection-semantics-7

**Every selected Grid row reports HasKeyboardFocus=true in multi-select**  
`src/Controls/DxUi.Accessibility.cpp:6090` · #35, #53 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: A grid row's HasKeyboardFocus is computed as gridHasFocus && row-is-selected. With GridSelectionMode multi, several rows claim keyboard focus at once, while GetFocus and the published focusedFragment name only the primary selected row. #35 fixed this defect class for Tree, whose HasKeyboardFocus now uses SnapshotTreeItemIsFocused (the focused item, not the selected set). The same correction was not applied to Grid, and #53 now raises per-row selection events that clients reconcile with focus.
- **Failure**: In a focused grid with rows 2, 5 and 9 selected, a UIA client looks for the element with HasKeyboardFocus (Narrator's focus recovery, an Inspect or Accessibility Insights check) and finds three. Accessibility Insights flags the duplicate keyboard focus, and Narrator can read the wrong row when it recovers focus after an announcement.
- **Fix**: 1. Add `uint64_t gridFocusedRowId = 0u;` and `bool hasGridFocusedRow = false;` to `AccessibilityControlNavigationSnapshot`. In the grid branch of the snapshot builder, fill them from the same source `GetFocus` uses: `grid-&gt;GetPrimarySelectedRow()` mapped through `model-&gt;GetStableRowId`, or `selection.back()` when it still resolves. 2. Add `SnapshotGridRowIsFocused(record, rowId)` and use it at line 6090: `record-&gt;gridHasFocus && SnapshotGridRowIsFocused(*record, _gridRowId)`. This makes …
- **Test**: Add a Grid UIA test with these steps: 1. Create a focused Grid with `GridSelectionMode::Extended` and 10 rows. 2. Select rows 2, 5 and 9 (for example click, then Ctrl+click twice, or through the selection model). 3. Get the row providers through the Grid fragment navigation or `Selection.GetSelectio …

### uia-selection-semantics-8

**Expanding or collapsing a tree item raises no UIA event: no ExpandCollapseState change and no StructureChanged for its children**  
`src/Controls/DxUi.Tree.cpp:698` · #35, #51 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: Tree items expose the ExpandCollapse pattern and the state property. Expanding or collapsing one, by keyboard Right/Left, a click on the chevron, or UIA Expand/Collapse, only republishes the snapshot. Nothing compares item expanded states between snapshots: the window-host diff only computes focus, semantic-control structure and selection, and the embedded diff only looks at control-level records. Buttons with disclosure get RaiseWindowHostDisclosureChanged. Tree items get no equivalent, and the children that appear or disappear raise no StructureChanged on the tree element.
- **Failure**: A Narrator user focuses a collapsed folder and presses Right. The folder expands visually and its children become elements, but Narrator stays silent because no ExpandCollapseState change arrives. A client that cached the tree's children keeps the old list until something else invalidates it. Collapsing is equally silent. Win32 TreeView and WPF TreeViewItem both raise the property change.
- **Fix**: 1. In PublishWindowHostAccessibilitySnapshot, record the expanded state of the focused tree item in AccessibilityFocusedFragmentSnapshot (for example `std::optional&lt;bool&gt; treeItemExpanded`, set when `focusedFragment.kind == TreeItem` and the item has children). Also store each tree's visible item count, or a cheap generation/hash, in its navigation record. 2. Add two fields to WindowHostSnapshotChanges: - `std::optional&lt;std::pair&lt;bool,bool&gt;&gt; focusedItemExpansion`, filled when S …
- **Test**: Add a Controls test that hosts a single Tree with a parent item that has children, and register UIA handlers on the window for: - AutomationPropertyChanged(UIA_ExpandCollapseExpandCollapseStatePropertyId) - StructureChanged Test steps: 1. Focus the parent item, send VK_RIGHT, and pump until idle. As …

### uia-selection-semantics-9

**Menu slider rows are plain Text elements with no value, so screen-reader users get no feedback while adjusting them**  
`src/Controls/DxUi.Menu.cpp:3378` · #56 · accessibility · confirmed (single: confirmed/medium)

- **At e5ebbb5**: Still valid: code unchanged.
- **What**: PopulateMenuAccessibility creates a MenuAccessibilityItem&lt;Label&gt; for Slider rows and leaves them out of the MenuItem role (the command predicate excludes Slider). The element therefore gets Label's Text control type and Name = label text only, with no RangeValue or Value pattern. Moving the slider updates item.sliderValue and item.acceleratorText, but SynchronizeMenuAccessibility only syncs bounds and focus, so the name never changes. #56 extended these elements to every ordinary menu, and the spec explicitly makes slider rows focusable through UIA.
- **Failure**: A context menu has a 'Zoom' slider row with stops 50% to 200%. A Narrator user arrows to it and hears 'Zoom, text', then presses Right to change the stop. Nothing is announced and no element exposes the current stop, so the user cannot tell the value without sight.
- **Fix**: Minimal fix: in PopulateMenuAccessibility, for `item.kind == MenuItemKind::Slider` with non-empty `sliderStops`, include the current stop text in what UIA reads. The simplest form appends it to the default name, e.g. `name = label.displayText + L", " + item.sliderStops[ClampSliderValue(item)].text`, or puts it in help text via SetAccessibleHelpText. Only apply this when `accessibleName` is empty, so consumer names win. In UpdateSliderInteractionAtPoint, after setting `acceleratorText`, update th …
- **Test**: Add a Menu UIA test that opens a native menu with one Slider row: label "Zoom", stops {"50%","100%","200%"}, sliderValue = 1. 1. Get the UIA element by AutomationId "menu.item.&lt;i&gt;". 2. Assert that its Name or ValueValue contains "100%". Today it fails: the Name is just "Zoom" and no Value/Rang …

## Low (144)

### ci-workflows-1

**No check can require the native matrix: a pull request with failing native jobs passes every required check** `.github/workflows/ci.yml:62` · #61 · ci · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
#61 makes the six native jobs conditional on `needs.native-scope.outputs.native`. A matrix job skipped by a job-level `if` reports one check under its unexpanded name, so the workflow comment (and Testing_Validation.md:198) say no required check may name a native job. As a result, nothing in branch protection can block a merge on a native failure: an x64/ARM64/ASan build break or a failing control/embedded suite. The … **Fix**: Add a fixed-name aggregate job and document it as the required native check. For example: ```yaml native-result: needs: [native-scope, native] if: ${{ always() }} runs-on: ubuntu-24.04 timeout-minutes: 2 steps: - shell: pwsh env: RESULT: ${{ needs.native.result }} SCOPE: ${{ needs.native-scope.outputs.native }} run: if …

### ci-workflows-2

**A gallery commit pushed to main leaves main's head without a push run, so consumer update checks report main unvalidated** `Tools/Commit-Gallery.ps1:27` · #49, #61 · ci · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The docs (docs/performance.md:355-358, CONTRIBUTING.md:21-23, Core_Documentation.md:38) tell maintainers to run the Publish docs gallery workflow "after a merge" on the branch. After a merge, that branch is naturally main. Commit-Gallery pushes with the workflow's GITHUB_TOKEN, which starts no workflow. Since #61, the only runs that validate main are `push` runs. Tools/ConsumerUpdate.psm1 reports an update only when … **Fix**: Simplest option: refuse main in gallery.yml with `if: ... && github.ref != 'refs/heads/main'`. Then update performance.md, CONTRIBUTING.md and Core_Documentation.md to say the gallery is published on the PR branch before merge. That is how #49 actually used it. After the token push, the maintainer re-runs validation or …

### ci-workflows-3

**The ARM64 ASan Debug job's 40-minute limit leaves about 6 minutes of headroom, and the per-step limits cannot take effect before it** `.github/workflows/ci.yml:102` · #61, #31 · flakiness · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
The CiRunScope plan measured ARM64 ASan Debug at 34 minutes against a job limit of 40, roughly 18% headroom on a shared hosted ARM runner. The step limits added in the window are commented as "about twice the slowest runs" so that a hang ends "while the job still has time to upload its logs". For ARM64 ASan they add up to 25 + 15 + 15 = 55 minutes, beyond the job limit. A slow day or a stall in one step therefore hit … **Fix**: Make the job limit at least the sum of the step limits plus restore, gallery and upload time, per platform, so the step limits fire first as the comment intends. For example `timeout-minutes: ${{ matrix.platform == 'ARM64' && 70 || 45 }}`. Alternatively, add a per-entry `timeout` field to the matrix include list and us …

### ci-workflows-4

**The gallery publish job leaves its contents:write token in .git/config while it restores, builds and runs branch code** `.github/workflows/gallery.yml:94` · #49 · security · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The publish job is granted `contents: write` and checks out without `persist-credentials: false`. actions/checkout therefore writes the token into the workspace git config as an extraheader for the whole job. The job then bootstraps vcpkg and builds ports from source (vcpkg-install.ps1), builds the solution, and runs DxUi.ControlTests.exe, DxUi.EmbeddedControls.exe and validate-specs.ps1. Any of these can read the wr … **Fix**: Add `persist-credentials: false` to the checkouts in gallery.yml and in format.yml's reformat job. In the commit/push step only, pass `env: GH_TOKEN: ${{ github.token }}` and give push a step-local credential, e.g. `git -c http.https://github.com/.extraheader="AUTHORIZATION: basic $([Convert]::ToBase64String([Text.Enco …

### ci-workflows-5

**Commit-Gallery commits everything already staged, not only docs/gallery, when run locally as documented** `Tools/Commit-Gallery.ps1:24` · #49 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The script stages docs/gallery and decides whether anything changed only under docs/gallery. It then runs a plain `git commit`, which commits the whole index. Its help text says "Only docs/gallery is staged, so nothing else a build touched is committed" and invites local use ("run it locally with -NoPush to see what a publish would commit"). Without -NoPush it also pushes. The fixture test covers an unstaged source e … **Fix**: Commit with a pathspec: `git -C $Root @identity commit -q -m 'docs: regenerate the control gallery' -- docs/gallery`. A pathspec commit uses --only semantics and leaves other staged entries alone. In the no-change branch, run `git -C $Root restore --staged -- docs/gallery` (or `reset -q -- docs/gallery`) before `exit 0 …

### ci-workflows-6

**The 'license' documentation rule matches nothing: the repository's license file is LICENSE.txt** `Tools/NativeScope.psm1:22` · #61 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Get-DocumentationScopeRules lists `LICENSE` (an exact file name or directory prefix). The tracked file is `LICENSE.txt`, which this rule does not match, and Test-NativeScope even asserts that `LICENSE.txt` needs the native jobs. The rule is dead code, and the step summary's description of documentation ("... the license ...") is untrue. The error is on the safe side, so the cost is runner time only: about 130 runner … **Fix**: Change the rule to `Paths = @('LICENSE.txt')`. In Test-NativeScope.ps1, move 'LICENSE.txt' out of the 'every other path needs the native jobs' list and assert `Get-DocumentationReason 'LICENSE.txt'` returns 'license'. Keep a lookalike such as 'LICENSE.txt.bak' or 'LICENSE' in the native list to keep checking exact-matc …

### ci-workflows-7

**No test guards the premise of the docs-only skip: nothing checks that native jobs do not read the documentation roots** `Tools/tests/Test-NativeScope.ps1:102` · #61 · test-quality · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The skip is safe only because no native job compiles, runs or reads anything under Specs, Changes, Measurements, docs, .agents or any *.md (NativeScope.psm1's comment: "Nothing here is compiled, run or read by a native job"). The tests only check that the classifier puts those roots in the documentation class, which restates the rule table. No test scans the native inputs (vcxproj/filters/props, test.ps1, test-consum … **Fix**: If the developer wants the guard, add a tooling test that scans only structural references. Check `#include "..."` lines in src/include/Tests/Samples, `Include="..."` attributes in *.vcxproj/*.filters/*.props/*.targets, and quoted path literals in the native entry scripts and the Tools modules they import. Fail on any …

### ci-workflows-8

**The validation and formatting jobs have no timeout, so a hang holds the check for the 6-hour default** `.github/workflows/ci.yml:32` · #30, #61 · ci · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The window adds step and job limits to the native, native-scope, paired-benchmark and gallery jobs so that a hang ends promptly. The `validation` job (validate.ps1: five validators and the tooling tests, which create git fixture repositories and run child pwsh/git processes) has no `timeout-minutes`. Neither do format.yml's `check` and `reformat` jobs on Windows runners, which download and run clang-format. These are … **Fix**: Add `timeout-minutes: 15` to the `validation` job in ci.yml, using about twice its usual runtime, and `timeout-minutes: 15` to format.yml's `check` and `reformat` jobs. Optionally set `GIT_TERMINAL_PROMPT: 0` and `GIT_EDITOR: 'true'` in the validation job's env so a fixture git command fails fast instead of waiting on …

### controls-editor-theme-10

**A slider removed from the tree mid-drag (PageHost navigation, a child moved out) never reports Cancel and keeps painting its drag and touch halo** `src/Controls/DxUi.WindowHost.cpp:4089` · #29, #62 · lifetime · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The spec says Slider 'Capture loss, Escape, hiding or detach reports Cancel and restores the initial value'. #29's CancelStaleCapture deliberately skips controls that are no longer in the tree ('A removed control is left to that prune and never dereferenced here'). PruneStaleInteractionState then drops the capture silently, and EmbeddedHost::CancelPointer also notifies only controls still in the tree. A control can b … **Fix**: Add a host-side 'cancel capture in subtree' step for controls that leave the tree while still alive. Before PageHost moves `_currentPage` to `_outgoingPage`, and before ClearChildren or RemoveChild-style detaches, check whether the host's captured control belongs to the departing branch (ControlBelongsToBranch already …

### controls-editor-theme-11

**Slider setters are order-dependent: tick marks are never re-clamped after a range change and paint outside the track; SetStep can leave LargeStep smaller than Step** `src/Controls/DxUi.Controls.cpp:4942` · #25 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#25 hardened every range and step setter against non-finite input, but it left dependent state that a later setter can break. SetTickMarks clamps the ticks to the range in force when it is called. SetMinimum and SetMaximum never re-clamp them, and Paint maps each tick with (tick-min)/(max-min) without skipping values outside 0..1. SetLargeStep enforces largeStep &gt;= step, but SetStep does not raise _largeStep, so t … **Fix**: Keep `_tickMarks` as supplied: sort them, but do not clamp. In Paint, skip a tick whose normalized position is outside [0,1], or re-clamp and de-duplicate in SetMinimum/SetMaximum. In SetStep, add `_largeStep = (std::max)(_largeStep, _step);`, or resolve the effective large step as max(_largeStep, _step) for keyboard a …

### controls-editor-theme-12

**NumericStepper Escape does not revert text that never parsed, and the key falls through to the dialog's cancel button** `src/Controls/DxUi.EditorControls.cpp:799` · #29 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 reworked the stepper's edit state. _editing is set only after a parse succeeds, and the Escape handler returns false when _editing is false. When the field holds text that never parsed, Escape leaves it in place and the unhandled key reaches the host's cancel-button or OnEscape handling. Only a later blur, through CommitEdit, reverts the text. **Fix**: In the VK_ESCAPE preview handler, when `! _editing` and `_field-&gt;GetText() != FormatValue(_value)`, call SyncText(), invalidate and return true. Return false only when there is nothing to revert, so Escape still reaches the dialog's cancel button. Add a test: type '-' and press Escape; require the text to revert and …

### controls-editor-theme-13

**TabControl RemoveTab/SelectTab reuse an index that focus callbacks may have shifted, so the wrong tab is removed or reported** `src/Controls/DxUi.Controls.cpp:6603` · #57 · lifetime · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#57 made both functions survive destruction during focus callbacks and range-check the index again. A callback that inserts or removes another tab keeps the index in range but makes it refer to a different tab. RemoveTab then erases whatever is now at that index. SelectTab sets _selectedIndex before the callbacks and passes the stale index to onSelectionChanged, although RemoveTab may have already adjusted _selectedI … **Fix**: In RemoveTab, capture `Control* target = children[index].get()` and its lifetime token before the focus move. Afterwards, re-find target's current index in AccessChildren() and return if it is gone. Have RemoveTab return whether it removed the tab, and fire onTabClosed only on success, using the resolved index. In Sele …

### controls-editor-theme-14

**The null-brush defect class #29 removed from editor controls remains in Grid, Scrollbar, Tree and Button paint** `src/Controls/DxUi.Grid.cpp:2672` · #29 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 added ResolveBrush to EditorControls because ControlHost::GetSolidBrush returns null when both brush creation and the fallback brush fail, and ColorPicker paint now skips draws in that case. The first reviewer reported ComboBox. The same pattern is unguarded in Grid (surface, header, cell and row fills, progress cells), in Scrollbar (track and thumb), in Tree (the focus rectangle) and in Button (DrawButtonChrome … **Fix**: Combine with item 0. Share the null-skipping Fill/Stroke/Line helpers from EditorControls.cpp in an internal header, and convert the Grid, Scrollbar, Tree and ComboBox sites. Button needs no change. Extend the forced-null-brush WindowHost test to paint a Grid (header, group, selected row, progress cell), a Tree with a …

### controls-editor-theme-6

**ComboBox paint still hands Direct2D possibly-null solid brushes, a pattern #29 removed from the editor controls** `src/Controls/DxUi.ComboBox.cpp:903` · #29 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 added ResolveBrush to DxUi.EditorControls.cpp because 'GetSolidBrush returns null after a brush failure or device loss; skip the draw instead of handing Direct2D a null brush'. ControlHost::GetSolidBrush can return nullptr when there is no context, when brush and fallback creation fail, or under the forced-null-brush diagnostic. ComboBox::Paint still passes host.GetSolidBrush(...) straight into FillRectangle, Fil … **Fix**: Fix this together with item 8. Move the EditorControls ResolveBrush/FillRect/StrokeRect helpers (EditorControls.cpp:42-60) into a shared internal header, or reuse the existing FillEllipseWithColor-style helpers in Controls.cpp. Replace the four ComboBox sites (759, 879, 903, 905) with the null-skipping helpers or `if ( …

### controls-editor-theme-7

**New slider geometry lets the hovered thumb outgrow its bounds-clamped chrome disc in short sliders** `src/Controls/DxUi.Controls.cpp:4774` · #62 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#62 enlarged the inner thumb to 14/20/16 DIP and the chrome disc to 24 DIP. The chrome is clamped to the control's smaller extent minus 2 (ResolveHaloDiameter), but the inner thumb is not clamped at all. Before #62 the hover thumb (16) fit inside the clamped chrome down to an 18 DIP tall slider. Now any horizontal slider shorter than 22 DIP (or vertical slider narrower than 22) paints a hovered accent thumb larger th … **Fix**: Scale the inner thumb diameter by ResolveHaloDiameter(bounds)/kSliderChromeDiameterDip, or cap it at the chrome diameter minus 4 DIP, so the rim survives in short sliders. Alternatively, document a 26 DIP minimum cross-axis size for Slider in UI_ControlsAndLayout.md and docs/controls.md. Add a geometry test (DebugGetIn …

### controls-editor-theme-8

**ProgressBar ignores right-to-left flow: determinate fill, segments and the indeterminate sweep always grow from the left** `src/Controls/DxUi.Controls.cpp:3497` · #28, #29 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#28 and #29 reworked ProgressBar painting (ComputeProgressTrack, ComputeIndeterminateSegmentRect, reduced-motion rest, non-finite guards) but none of it consults IsRightToLeft()/GetFlowDirection(). Slider, NumericStepper, ColorPicker and TabControl all mirror in right-to-left flow, and #29 fixed several RTL alignments. In an Arabic or Hebrew UI the progress fill starts at the left (the trailing edge), segmented value … **Fix**: When IsRightToLeft() is true, mirror the determinate fill, the segmented primary and secondary rectangles (swap the halves, mirror the hatch direction) and the indeterminate segment within the track. A MirrorRect equivalent to the one in EditorControls.cpp:35 works if it is shared. Add the RTL rule to the ProgressBar s …

### controls-editor-theme-9

**ThemeColors.h documents the pre-#29 alert fallback; consumers are told unsupplied tones become text on window background** `include/DxUi/ThemeColors.h:18` · #28, #29 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#28 added the public comment describing #28's ColorFromSuppliedArgb fallback. #29 then replaced that fallback with ResolveAlertTone: outside high contrast, an unsupplied fill or text now takes the library's distinct info, warning or error tone, and a supplied fill gets a derived text that reaches 4.5:1. Text on window background is used only in high contrast. The public header still states the #28 behaviour, so a con … **Fix**: Rewrite the ThemeColors.h comment to match ResolveAlertTone. Zero alpha means unsupplied, and an unsupplied fill or text takes the library's info, warning or error tone (high contrast: windowBackground and text). A supplied fill without a text gets a derived text with at least 4.5:1 contrast. A supplied text that is un …

### cross-cutting-simplification-10

**#29 fixed null-brush draws only in editor controls and TextField; Grid, ComboBox and Tree still pass GetSolidBrush() straight to Direct2D** `src/Controls/DxUi.Grid.cpp:2672` · #29 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 added EditorControls' ResolveBrush and TextField guards with the stated rule: "GetSolidBrush returns null after a brush failure or device loss; ... skip the draw instead of handing Direct2D a null brush". It also added TestWindowHostEditorControlsSurviveForcedNullSolidBrushes. The same class remains elsewhere: 14 raw `dc-&gt;...(…, host.GetSolidBrush(...))` calls in Grid::Paint (surface, header, rows, groups, sep … **Fix**: Declare the existing null-safe FillRectangleWithColor, DrawLineWithColor and related helpers (Controls.cpp:509 and following) in Internal.h. Route the raw Grid, ComboBox and Tree draws through them, or through `if (auto* b = host.GetSolidBrush(c))` guards, and delete EditorControls' private FillRect, StrokeRect and rel …

### cross-cutting-simplification-11

**FindTextLayoutEntry's 'least recently used older layout' victim branch is dead: EndTextLayoutPaint already released every layout the last paint did not use** `src/Controls/DxUi.Internal.h:118` · #29, #36 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The generation is incremented only at the start of Grid::Paint, and a scope_exit always calls EndTextLayoutPaint. That call resets the layout of every entry whose lastUse is not the current generation. Hover lookups between paints stamp the same generation. When the next paint looks up entries, every entry with a layout therefore has lastUse == generation-1, so `entry.lastUse + 1u &lt; generation` is never true. Vict … **Fix**: Remove the `entry.lastUse + 1u &lt; generation` victim branch. Rewrite the FindTextLayoutEntry comment to state the invariant: between paints only the last paint's layouts are alive, so a lookup reuses a free way or grows the table and evicts a live layout only at the bound. Optionally add a debug assertion in EndTextL …

### cross-cutting-simplification-6

**Selection-change publication is hand-copied 9 times in Grid and about 6 times in Tree, and the copies have drifted** `src/Controls/DxUi.Grid.cpp:1376` · #60, #53, #35 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#60 added the lifetime guards site by site. Grid now repeats the same sequence at SetModel 1037, SetSelectionMode 1093, ApplyGroupLayout 1376, NotifyDataChanged 1441, RequestRemoveRowSelection 2135, the group-header press 3616, the keyboard group toggle 4041, OnSelectAll 4231 and SelectRow 4443: snapshot previous, compare, take a token, call OnGridSelectionChanged, check the token, take a token, RefreshAccessibilityS … **Fix**: Add `[[nodiscard]] bool Grid::PublishSelectionChange(std::span&lt;const uint64_t&gt; previous)`: notify the delegate if the selection differs, stop if the grid was destroyed, then RefreshAccessibilitySnapshot and stop again if destroyed. Add a Tree equivalent, and route every site through it. Make ApplyGroupLayout and …

### cross-cutting-simplification-7

**Parallel utilities added beside existing helpers (AGENTS.md: search existing helpers first)** `src/Controls/DxUi.Accessibility.cpp:1500` · #29, #31, #35, #53 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The window added several helpers that duplicate ones already in the tree: (1) MixAccessibilityHash (#31) is MurmurHash fmix64 written out again; its own comment says it is "as the retained text layouts' keys use it", and the same code is inline in Internal.h HashTextLayoutKey. (2) Theme.cpp PaintedOver (#29) equals the existing CompositeOverBackground in the same file whenever the ground is opaque, which it is at eve … **Fix**: Move fmix64 into Internal.h as `MixHash64` and use it in HashTextLayoutKey and HashControlPath. Delete PaintedOver and call CompositeOverBackground. Delete the Controls.cpp copies and use IsControlInTree plus an exported RevalidateDispatchedControl. Use SameControlLifetime at 4208 and 5657. For the tree snapshot, itera …

### cross-cutting-simplification-8

**Six local copies of the WindowHostAccessibilityTarget release lambda, with manual Release() still beside them** `src/Controls/DxUi.Accessibility.cpp:9204` · #29, #31, #32, #53 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Each new event raiser (CreateWindowHostEventProvider, AnnounceWindowHostFocus, RaiseWindowHostSelectionChanges, RaiseWindowHostStructureInvalidated, and two at 10161/10178) re-declares the same `constexpr auto releaseTarget` lambda and a `wil::unique_any&lt;WindowHostAccessibilityTarget*, ...&gt;`. Older functions in the same area still pair AcquireWindowHostAccessibilityTarget with hand-written Release calls: RaiseW … **Fix**: Make AcquireWindowHostAccessibilityTarget return `wil::com_ptr_nothrow&lt;WindowHostAccessibilityTarget&gt;` (the type has AddRef/Release, so com_ptr works without IUnknown) by attaching the AddRef'd pointer. Delete the six local lambdas and the manual Release calls at 9410/9418/9421, 9661/9667 and 10020/10026, and use …

### cross-cutting-simplification-9

**MenuDebugDispatch/MenuDebugPayload (#47) re-implement the accessibility UI-action cross-thread handshake** `src/Controls/DxUi.Menu.cpp:1943` · #47 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#47 generalized the menu probes into templates: a Pending/Taken/Abandoned atomic state, a manual-reset completed event, a payload whose destructor abandons the call and signals, caller-side waiting with abandon-or-accept-if-complete, and handler-side take-then-CAS. This is the same state machine AccessibilityUiActionDispatch and AccessibilityUiActionPayload implement at Accessibility.cpp 164-214, with the caller at 5 … **Fix**: If the developer accepts touching the production UIA handshake: add `template &lt;class Request&gt; struct CrossThreadCall` to Support/PostedPayload.h. It would hold the state, event and request, take an abandon hook (accessibility sets ERROR_CANCELLED) and a post/send choice, and expose caller-side wait/abandon/accept …

### gap-arm64-asan-runtime-2

**Wall-clock 500 ms focus-gain turn is asserted after waits with no upper bound, so slow lanes fail instead of skipping** `Tests/Controls/DxUi.Tests.Menu.cpp:7649` · #32 · flakiness · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
ControlHost::IsInFocusGainTurn ends a turn 500 ms after WM_SETFOCUS, measured with GetTickCount64. The activating-click tests assert that the turn is still running after TryActivateDxUiTestWindow. That helper can poll the foreground for up to 800 ms with Sleep(10) after SetActiveWindow has already started the turn. The WindowHost tests make the same assertion after several SetFocusControl calls, which publish snapsho … **Fix**: Optional hardening. Add a diagnostics-only override to ControlHost, for example DebugSetFocusGainTurnClockForTest(ULONGLONG (*now)() noexcept) or DebugSetFocusGainTurnLimitForTest(ULONGLONG). IsInFocusGainTurn reads it in place of GetTickCount64 or the constant. Fixtures that are not about the limit freeze the clock, o …

### gap-arm64-asan-runtime-5

**Slider touch-halo test (and the hover test it copies) reads the wall clock before the call that starts the transition** `Tests/Controls/DxUi.Tests.Animation.cpp:447` · #62 · flakiness · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
Slider::BeginVisualTransition stamps the transition with its own ::GetTickCount64() inside OnMouseDown/OnMouseUp. The test reads startTickMs before that call, then requires in-flight progress at startTickMs+40 and a settled value at startTickMs+200 (duration 140 ms). GetTickCount64 advances in steps of about 15.6 ms. If more than about 31 ms passes between the test's read and BeginVisualTransition (preemption on a lo … **Fix**: Make the slider's transition clock injectable for tests: let BeginVisualTransition take `now` from a host/test clock as #44 did for tooltips, or add a DebugGetTouchHaloStartTickMs() getter. Then tick relative to the recorded start (start+40, start+200) instead of a time read before the call. Apply the same change to th …

### gap-arm64-asan-runtime-6

**ASan lanes' step budgets add up to more than the 40-minute job limit, so a slow lane is cancelled by the job rather than by a named step** `.github/workflows/ci.yml:102` · #31, #61 · ci · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
The workflow comment says the step limits end a hang 'while the job still has time to upload its logs'. On both ASan Debug lanes, test.ps1 (25 min, including the build and performance.ps1), test-consumer (15) and test-consumer -DisableStlAnnotations (15) add up to 55 minutes under a 40-minute job limit. vcpkg-install and gallery.ps1 have no step limit at all, and gallery runs its whole Gallery fixture as a single 300 … **Fix**: Give each matrix row a job timeout that covers its step budgets plus upload time, for example a `jobTimeout` matrix field (about 60 for the ASan rows) used as `timeout-minutes: ${{ matrix.jobTimeout }}`. Alternatively, move the -DisableStlAnnotations consumer build into its own job. Add step timeouts to vcpkg-install a …

### gap-arm64-asan-runtime-7

**PayloadRegistry's static destructor frees entries without the lock and leaves them registered, so a later drain frees them again** `src/Support/PostedPayload.h:22` · #47 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Payloads() is a function-local static created on the first InitPostedPayloadWindow, the first WindowHost attach or menu popup. Its destructor calls destroy on every pending entry without taking the mutex and without clearing the entry. Any window drained afterwards finds the same token/window/value and destroys it a second time. Static objects constructed before the first attach, such as a consumer's global or single … **Fix**: Make the registry immortal (`static PayloadRegistry& r = *new PayloadRegistry;`) and drop the destructor, so pending payloads are reclaimed by the OS at exit and later drains still see a valid registry. If destruction is required, take the mutex in the destructor, move the entries out, reset them, and destroy them outs …

### gap-arm64-asan-runtime-8

**Peek-hook setter can still pair an already-loaded hook with a cleared or new context** `src/Controls/DxUi.Menu.cpp:1531` · #42 · threading · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The setter publishes hook=null, then context, then hook, and its comment promises the loop 'never calls a new hook with the previous context'. That direction holds with acquire/release. The reverse does not: the loop loads the hook and then loads the context separately, so a hook loaded before a clear or replacement is called with the context published afterwards (null, or another test's). Clearing also does not wait … **Fix**: Document in DxUi.h and the setter that the hook must be set and cleared on the menu's thread while no context menu is open, and debug-assert that no menu loop is running in the setter. If cross-thread use is ever needed, publish one atomic pointer to an immutable {hook, context} pair and load it once per call.

### gap-arm64-asan-runtime-9

**FocusEventClient copies UiaTestClient and differs from it, including a 30 s idle exit that silently stops listening** `Tests/Controls/Controls.Tests.DxUiFocusEventClient.h:179` · #32, #63 · architecture · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
Tests/Support/UiaTestClient.h already implements an in-process UIA client on its own MTA thread with a focus subscription (Subscription::focus), pumped waits and request dispatch. FocusEventClient (#32) and the hand-rolled client in TestDescribedMenuRaisesFocusChangesForKeyboardRows duplicate it, and the copies have drifted. Only FocusEventClient got the CUIAutomation8 timeouts (#63). Its run loop also exits after 30 … **Fix**: Short term: wait on stop/request with INFINITE in both clients, as UiaTestClient does; the owners' destructors already bound teardown. Then rebuild FocusEventClient and the described-menu client as thin wrappers over UiaTest::Client with Subscription{.focus = true}, with FocusElementNamed as an Ask lambda. Move CreateF …

### gap-embedded-host-parity-5

**Embedded event raising keeps going after the root is replaced or a newer publish supersedes it** `src/Controls/DxUi.Accessibility.cpp:9797` · #53, #26 · reentrancy · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The spec premises that an outgoing UIA call in an STA can dispatch messages during raising. `RaiseSelectionEvents` therefore stops when `rootLifetime` expires, but the property, focus and text loop in `RaiseEmbeddedAccessibilityChanges` checks only `host != nullptr`. When the root is replaced mid-raise, it keeps raising property and focus events on elements of the replaced tree. When the application's message handler … **Fix**: Use one predicate for both raise paths: host connected, `! target.rootLifetime.expired()` for an embedded target, and `target.snapshot.load() == current`, so a superseded publish stops raising. Pass `current`, or a 'still latest' lambda, into RaiseSelectionEvents. Alternatively, add a reentrancy depth or generation cou …

### gap-embedded-host-parity-6

**Zero-extent preparation releases the surface but keeps the Grid's text layouts that hiding returns** `src/Rendering/Embedded.cpp:261` · #43 · resource-leak · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#43 made `SetVisible(false)` call `root-&gt;OnHidden()` so a Grid returns its retained layouts, key strings and tables. The comment there says this is because 'Nothing paints while the view is hidden'. A zero-extent `Prepare` also paints nothing, and hosting.md describes hiding and preparing a zero extent as equivalent suspensions. That path calls only `ReleaseSurface()`, so the Grid keeps every layout while collapse … **Fix**: In Prepare's zero-extent branch, call `_host._root-&gt;OnHidden()` only on the transition into zero size (when `! wasZeroSized && s.zeroSized` and the view is visible), so repeated Prepare(w, 0) calls do not rerun it. Better, factor a private Suspend() helper (ReleaseSurface, then root-&gt;OnHidden) used by SetVisible( …

### gap-embedded-host-parity-7

**The reference embedded sample never sets PointerEvent::device, so the touch feedback #62 documents never appears there** `Samples/EmbeddedControls/Main.cpp:155` · #62 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
docs/hosting.md now tells consumers to set `device` from `PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())` so that a touch drag shows the slider's touch halo. docs/hosting.md also names the sample as the host that demonstrates application lifetime and input forwarding, but it constructs every PointerEvent without the device. A touch drag on the sample's slider is therefore reported as a mouse drag, and anyon … **Fix**: In the WM_LBUTTONDOWN, WM_MOUSEMOVE and WM_LBUTTONUP cases, compute `const auto device = DxUi::PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo());` and pass it as the sixth initializer, with wheelDelta 0. Optionally do the same for WM_MOUSEWHEEL. The function is declared in include/DxUi/DxUi.h:534, not PointerInp …

### gap-perf-evidence-pipeline-4

**Default summary discovery in Publish-BenchmarkVerdict, the path CI uses, has no test, so a stale summary.json is untested** `Tools/BenchmarkGate.psm1:482` · #46 · test-quality · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Question (1), stale-summary part. The CI verdict step calls Publish-BenchmarkVerdict.ps1 without -Reports, and it runs after a failed measurement (`!cancelled()`). The script then falls back to Find-PairedSummary, which returns the newest summary.json under .build/paired by LastWriteTime, with no link to the run that just failed. Every Publish test passes -Reports explicitly, and Find-PairedSummary is never called in … **Fix**: Have performance-paired.ps1 print or emit its run directory: write it to $env:GITHUB_OUTPUT when present, and always to stdout. Pass it to Publish-BenchmarkVerdict.ps1 -Reports in ci.yml, so the CI verdict never relies on discovery. For the discovery fallback, either require -Reports when the newest run directory under …

### gap-perf-evidence-pipeline-5

**Per-run performance receipts and paired worktrees are never pruned** `test.ps1:97` · #31, #46 · resource-leak · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
Question (4). Every test.ps1 run still measures and prints complex-UI FPS and memory, as AGENTS.md requires. Each run also writes a new GUID-named receipt and its .comparison.json under .build/reports, and nothing ever removes them. performance-paired.ps1 leaves every run's worktree under .build/paired/&lt;run&gt;/: a detached checkout, its vcpkg_installed restore and a full Release build of DxUi and the test executa … **Fix**: Ask the developer for a retention policy. Then either keep the newest N GUID Performance receipts per platform/configuration in test.ps1, deleting only files that match `Performance-&lt;Platform&gt;-&lt;Configuration&gt;-&lt;32 hex&gt;.json(.comparison.json)` inside the validated .build/reports path, or document a clea …

### gap-uia-mutex-nested-loop-matrix-2

**Execute* paths use `host` after a publish or ::SetFocus that can destroy it, without the survived() check added elsewhere** `src/Controls/DxUi.Accessibility.cpp:8741` · #57, #60 · lifetime · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#57 and #60 added survived() (`ResolveHost() == host`) after the selection delegate and after SetFocusControl. But the next lines still dereference `host` after a call that can run arbitrary messages. RefreshWindowHostAccessibilitySnapshot raises focus and selection events. The repository's own contract says "an outgoing call of UI Automation's in a single-threaded apartment dispatches messages, and a message can ... … **Fix**: 1) In the Execute* paths, check the host before touching it again after a publish. Use `if (! survived()) return UIA_E_ELEMENTNOTAVAILABLE;` (or `ResolveHost() != host`) between each RefreshWindowHostAccessibilitySnapshot or ::SetFocus and the next `host-&gt;` use. Do the same after Grid::RequestRemoveRowSelection, whi …

### gap-uia-mutex-nested-loop-matrix-3

**A dropped menu-row invoke still reports success, and a full payload registry makes windows refuse every UIA action with no diagnostic** `src/Controls/DxUi.Menu.cpp:1056` · #56, #47 · bug · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
In #56, menu-row Invoke goes through SetAccessibleInvoke -&gt; PostMenuAccessibilityRequest, and that function discards PostMessagePayload's result. Control::InvokeAccessible returns true, so ExecuteInvokeOnWindowThread returns S_OK whether or not the post was queued. PostMessagePayload fails when the process-wide registry has no free entry (128 payloads) or when the popup HWND was never registered. InitPostedPayload … **Fix**: 1. Make InitPostedPayloadWindow return bool. In ControlHost::Attach and MenuWndProc's WM_NCCREATE, log a failed registration through the existing diagnostics or trace path, so a full registry is visible. 2. Let menu accessible invoke report failure: give the invoke callback a bool or HRESULT result (or a menu-specific …

### gap-uia-mutex-nested-loop-matrix-4

**Process-wide recursive mutex couples every window and thread; a per-target lock plus one resolve/act/revalidate helper would remove the problem class** `src/Controls/DxUi.Accessibility.cpp:791` · #53, #56, #57, #60 · architecture · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Snapshots are already published through an atomic shared_ptr per target (target.snapshot), and each target's mutations happen only on its own window thread. Even so, one static recursive_mutex serializes every provider call (down to QueryInterface) and every publish across all windows, threads and menu popups. Its recursion hides same-thread reentrancy, so nested Execute* calls look safe while running under the outer … **Fix**: Step 1 (low risk): add one helper that resolves host and control under GetAccessibilityTargetMutex, releases the lock, runs the action/delegate, then re-acquires and checks `ResolveHost() == host` (and the control's lifetime token) before any publish, Invalidate or SetFocusControl. Return UIA_E_ELEMENTNOTAVAILABLE when …

### gap-uia-mutex-nested-loop-matrix-5

**No test drives a UIA action into a delegate that runs a nested message loop or races another thread's UIA call** `Tests/Controls/DxUi.Tests.Accessibility.cpp:2937` · #57, #60 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus replaces the root synchronously inside the delegate (`window.Host().SetRoot(std::make_unique&lt;Panel&gt;())`). The posted-action tests stall the handler with test events. No test makes a delegate pump a nested loop (RunModalLoop, ContextMenu::Show or a PeekMessage loop) while a second thread issues a UIA call or another posted action. No test destroys … **Fix**: Add a native Accessibility test with two parts: 1. A Toggle (or Tree) selection delegate signals an event and runs a bounded PeekMessage loop of about 1 s. Meanwhile a worker thread calls get_ProviderOptions or GetPropertyValue on a provider of a second window owned by another thread, and asserts it returns within, say …

### gap-uia-test-falsifiability-2

**'No event' assertions rely on a 500 ms or 1 s wall-clock quiet window; a late spurious event is swallowed by the next step and never reported** `Tests/Support/Support.Tests.UiaTestClient.h:816` · #51, #53, #54 · test-quality · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
With expected == 0, HearSelectionEvents runs the action and then WaitOutTheStream(500 ms quiet). The library raises synchronously inside the publish, but UIA delivers to the MTA client asynchronously on its own threads. The client logs arrivals of 1 s or more as a known slow-runner condition (kSlowArrivalMs), and setup has exceeded 3 s on hosted runners. An event that arrives after the quiet window is not attributed … **Fix**: In UiaTest::HearSelectionEvents, wrap the action in a deterministic raise counter and assert on it when expected == 0. Keep the client wait as a delivery check: ```cpp action();  // becomes: size_t raised = 0u; { UiaTest::SelectionEventInterruption counter({}); action(); raised = counter.Events(); } if (expected == 0u …

### gap-uia-test-falsifiability-3

**Publish dereferences the ControlHost after UIA structure and focus raises: the same reentrancy class #60 fixed for controls, and the test seam covers selection raises only** `src/Controls/DxUi.Accessibility.cpp:9567` · #60, #53 · lifetime · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#60 and RaiseSelectionEvents rest on the premise that an outgoing UIA call in an STA can dispatch messages that remove controls or detach the host. SelectionEventInterruption tests that premise only after selection raises. In RefreshWindowHostAccessibilitySnapshot, RaiseWindowHostStructureInvalidated(hwnd) runs UiaRaiseStructureChangedEvent. The code then calls EmbeddedAccessibilityAccess::ReporterOfFocusMove(*host, … **Fix**: First decide the contract. Option 1 (recommended): document in include/DxUi/DxUi.h and the accessibility spec that a ControlHost must not be destroyed from code that runs inside its own calls (UIA raises, delegates, focus callbacks), and that detaching it or replacing its root is the supported way to tear it down there …

### gap-uia-test-falsifiability-4

**Disclosure client test waits for 'changes &gt;= 2', which a repeat of the expand event satisfies before the collapse arrives** `Tests/Controls/DxUi.Tests.Accessibility.cpp:129` · #27 · flakiness · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
TestDisclosureNotifiesNativeAutomationClient counts ExpandCollapseState changes. After the collapse it waits only for `changes &gt;= 2` and then reads `state == Collapsed` once. This file documents elsewhere that an in-process client hears each event again a moment later. If the expand event is repeated, `changes` is already 2 before the collapse is raised, the wait returns at once, and the asynchronous delivery of t … **Fix**: Make each step wait on the value it asserts: `waitUntil(kNotificationDeadlineMs, [&]{ return observer-&gt;state.load() == ExpandCollapseState_Expanded; })`, then the same for Collapsed. Better, port the test to UiaTest::Client with Subscription{.properties={UIA_ExpandCollapseExpandCollapseStatePropertyId}}, wait for th …

### gap-uia-test-falsifiability-5

**#60 replacement tests cannot detect a stale continuation through the control's _host in Release: SetRoot nulls _host before destroying the old root** `Tests/Controls/DxUi.Tests.Accessibility.cpp:7797` · #60 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
ExpectRootReplacementDuringSelectionPublish asserts only host-observable effects: the return value, focus and capture are null, and the invalidate count is unchanged. That catches continuations that use the `host` parameter (Invalidate(host), SetFocusControl). ControlHost::SetRoot calls `_root-&gt;PropagateHost(nullptr)` before releasing the old root, so the freed Tree's `_host` is null. A reverted Tree::SetMultiSele … **Fix**: State in the comment above ExpectRootReplacementDuringSelectionPublish that a stale continuation through the freed control's `_host` or members is caught only by the Debug fill or ASan. Make sure Test-Changes.ps1 always maps Tree, Grid and Accessibility source changes to an ASan Debug run of these suites. Optionally ad …

### gap-uia-test-falsifiability-6

**Embedded multi-select tree event test checks only presence (WaitForEvent over the whole history), so wrong extra events pass; 'reports it once' is not asserted** `Tests/Embedded/Embedded.Tests.EmbeddedUia.h:597` · #53, #35 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
The window-host twins and the other embedded tests compare the exact set of selection events per step (HearSelectionEvents + ExpectHeard). TestEmbeddedMultiSelectTreeRaisesItsSelectionEventsToAClientOfTheApplicationsWindow instead calls WaitForEvent, which counts matches across all events recorded since the client started. It therefore cannot fail if the view raises additional wrong events, such as Invalidated or Ele … **Fix**: Rewrite each step with the local `hear` helper (UiaTest::HearSelectionEvents with the expected count) and walk.ExpectHeard on the exact distinct set, as in the single-tree embedded test. Replace the hand-rolled waitOutTheStream with walk.client.WaitOutTheStream. In EmbeddedAccessibilityTests.h, capture `const size_t se …

### gap-uia-test-falsifiability-7

**Embedded selection raising does not stop when a non-root tree or grid is hidden or removed mid-raise; the embedded interruption test covers only view hide and root replacement** `src/Controls/DxUi.Accessibility.cpp:9281` · #60, #53 · test-quality · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
RaiseSelectionEvents documents that raising ends once the control 'is no longer the one published at its path (hidden, removed or replaced, which republishes)'. `published()` reads target.snapshot. In an embedded view nothing republishes except UpdateAccessibility: RefreshWindowHostAccessibilitySnapshot returns at once when hwnd is null. `connected()` checks only that the host is attached and the root's lifetime. So … **Fix**: Developer decision: if embedded raising should stop when the control leaves, carry the control's lifetime token in SelectionChange and have `published()` also require it unexpired, which cheaply covers removal. Live visibility would need a control read, which conflicts with 'providers never touch a control'. Then add a …

### gap-uia-test-falsifiability-8

**Four hand-rolled in-process UIA client threads duplicate UiaTest::Client and silently unsubscribe after fixed 10/15/30 s lifetimes** `Tests/Controls/DxUi.Tests.Accessibility.cpp:5858` · #51, #53 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
#51 added UiaTest::Client (an MTA client thread, a Recorder, pumped waits, bounded teardown) and #53 added SelectionEventsSubscription/HearSelectionEvents. Four tests in the same file still carry their own copies: TestDisclosureNotifiesNativeAutomationClient, TestWindowHostRebuildRaisesStructureInvalidation, TestCollapsedStatusRootChildEventComesFromTheChild, and RunTreeMultiSelectSelectionEventsTest with its HeardSe … **Fix**: Port the four tests to UiaTest::Client. The disclosure test uses Subscription{.properties={ExpandCollapseState}}. The rebuild and collapsed-root tests use {.structure=true} or {.properties=...}. RunTreeMultiSelectSelectionEventsTest uses UiaTest::SelectionEventsSubscription() + HearSelectionEvents + ExpectHeard. Then d …

### gap-visual-modes-hc-rtl-dpi-5

**Gallery and Theme tests use an HC palette whose text equals its selection text, cover no RTL or fractional-DPI variant, and cannot catch the defects above** `Tests/Controls/DxUi.Tests.Gallery.cpp:74` · #49, #35, #62, #24 · test-quality · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
The only high-contrast gallery sheet uses white text and white selection text on a dark-blue (#003B80) selection. With WindowText == HighlightText, the Tree's unfocused 'text on Highlight' rows look correct. Because the selection fill is dark, a focus ring blended from it is hard to judge anyway. Real Windows HC themes all use a light Highlight with dark HighlightText, or the reverse, against a WindowText that differ … **Fix**: 1. In DxUiTests.Theme.cpp, add a table-driven contrast test over the default light, dark and rainbow palettes plus two real Windows HC palettes: Aquatic-like (Window #202020, WindowText #FFFFFF, Highlight #8EE3F0, HighlightText #263B50) and a light one like Desert. Require ratio(selectionText, selectionFill) &gt;= 4.5 …

### gap-visual-modes-hc-rtl-dpi-7

**The two Tree::ComputeItemLayoutMetrics overloads duplicate the whole row layout** `src/Controls/DxUi.Tree.cpp:1657` · #35, #38 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
ComputeItemLayoutMetrics(host, visibleIndex, item) and ComputeItemLayoutMetrics(host, rowTopDip, item) contain the same 40 lines (row, expander, icon and badge rects). The only difference is that the index overload computes rowTop from contentRect.top + index*rowHeight - scroll. Any visual change, such as the RTL mirroring above or a DPI snap, must be made twice. #35 hit-testing (expander zone) uses one copy and pain … **Fix**: Replace the body of the size_t overload with `return ComputeItemLayoutMetrics(host, GetContentRect().top + (static_cast&lt;float&gt;(visibleIndex) * _rowHeightDip) - _verticalScrollDip, item);` and keep the empty-content guard in the float overload. Both are private overloads declared in include/DxUi/DxUi.h:3480-3481. …

### grid-multiline-10

**GridTextOverflow WIP plan still describes the replaced V11 direct-mapped cache and says single-line cells are unchanged** `Specs/Plans/WIP/GridTextOverflow_2026-09-21.md:323` · #29, #36 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The active plan's Checkpoint section says production is V11 with a 32-slot cache, says single-line cells keep their previous rendering path, and (line 210) says "Current source is V11". At HEAD the Grid uses 32-way set-associative tables of up to 16,384 entries, a separate omitted-tail table, and retained layouts for single-line captions (PrepareSingleLineCellLayout replaced DrawCenteredText). The September 30 sectio … **Fix**: Mark the 'Checkpoint' and 'September 23' present-tense statements as historical, or rewrite them to describe the HEAD cache: set-associative tables, omitted-tail table and retained single-line layouts. Link them to Core_PerformanceAndResources.md's accepted Grid layout retention as the current state.

### grid-multiline-11

**Clamped JUSTIFIED cells lose justification only when lines are omitted, because the omitted-tail layout turns every visible line into its own paragraph** `src/Controls/DxUi.Grid.cpp:2367` · #22 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
When content is omitted, PrepareCellTextLayout rebuilds the visible text by joining the measured lines with '\n' and lays it out NO_WRAP in the cell's own format, which keeps cellData.textAlignment. DirectWrite never justifies the last line of a paragraph. Every rebuilt line is now a paragraph end, so a DWRITE_TEXT_ALIGNMENT_JUSTIFIED cell that omits text paints ragged-left lines. The same column's cells that fit kee … **Fix**: Simplest fix: for multiline cells, map DWRITE_TEXT_ALIGNMENT_JUSTIFIED to LEADING (or justify only in the fitting path) and document it in docs/controls.md. If justification of omitted cells is wanted, verify by test whether joining soft-wrapped lines with U+2028 under a WRAP layout with SetMaxHeight keeps justificatio …

### grid-multiline-12

**Spinner and Marquee captions are checked for clipping against text their paint never draws: wrong font and no spinner-frame prefix** `src/Controls/DxUi.Grid.cpp:3501` · #29 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 made the single-line hover check read Paint's own retained layout ("the tooltip reads that same layout"). Spinner and Marquee cells do not paint through that layout. Spinner draws "&lt;frame&gt; " + text in FontRole::Body over contentRect. Marquee draws its caption in FontRole::Small, centred. OnMouseMove still sends them to IsSingleLineCellTextClipped. That function measures cellData.text alone, in _cellTextFont … **Fix**: In OnMouseMove, handle Spinner and Marquee before the generic branch. For Spinner, measure the frame-prefixed caption in FontRole::Body against the paint contentRect (use a fixed widest frame or the current frame) with MeasureSingleLineTextWidthDip. For Marquee, measure the caption in FontRole::Small, centred. Neither …

### grid-multiline-5

**Omitted-tail layout table forgets shared layouts after any all-hit paint, so identical tails are re-shaped and duplicated** `src/Controls/DxUi.Grid.cpp:2198` · #29 · performance · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
A cell's main-table entry that hits returns at once and refreshes only its own lastUse. The `_cellDisplayLayouts` entry whose layout it shares is never touched. At the end of that paint, EndTextLayoutPaint releases every display entry whose lastUse is not the current generation. The shared IDWriteTextLayout stays alive only through the main entries' com_ptr copies, and the display table no longer knows about it. The … **Fix**: Store the display key hash in CellTextLayoutCache when the entry takes `display.layout`. On a main-table hit with a stored display hash, refresh the display entry's lastUse through a non-mutating lookup of its set that matches by keyHash and layout pointer. A plain lookup is needed because FindTextLayoutEntry may evict …

### grid-multiline-6

**Hover tooltip ignores a cell cut by the top or bottom of the viewport** `src/Controls/DxUi.Grid.cpp:2575` · #22, #29 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#22 and #29 made paint lay text out against the full cell and made the tooltip read the same layout. The viewport test in both clip checks only compares horizontal extents. The bottom visible row is usually cut by the body rectangle, and during a thumb drag so is the row under the header. Its hidden lines are clipped by the body clip, yet hovering offers no tooltip whenever the layout itself is not truncated. The con … **Fix**: In both clip checks, also return true when the painted text's vertical span crosses viewportRect.top or bottom by more than 0.5 DIP. For multiline that span is origin y = textRect.top + (height - paintHeight)/2 to + paintHeight. For single-line it is the centred line box from the layout metrics' top and height. Add a t …

### grid-multiline-7

**Multiline cell text is drawn without a horizontal clip, so in very narrow text rectangles it can paint over the badge or the next column** `src/Controls/DxUi.Grid.cpp:2613` · #29 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#29 removed D2D1_DRAW_TEXT_OPTIONS_CLIP from multiline drawing because of descender shaving and per-draw allocation. Now a clip is pushed only when the first line is taller than the rectangle. DirectWrite wrapping still puts at least one grapheme cluster on each line, and that cluster can be wider than the layout box. The no-wrap omitted-tail layout can likewise end up wider than the box when the box is narrower than … **Fix**: Record in the entry whether the prepared layout is wider than its box (metrics.width or overhang &gt; width, which is already computed when setting `truncated`). Only then push an axis-aligned clip spanning the text rectangle's horizontal extent and the full cell height, so descenders keep their room and the common pat …

### grid-multiline-8

**Public GridColumnDesc::multiline and textAlignment are never read by the Grid** `include/DxUi/DxUi.h:1282` · #22 · api-design · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Only per-cell fields drive the multiline clamp contract (`GridCellData::multiline` and `GridCellData::textAlignment`). The column descriptor also exposes `multiline = true` (on by default) and `textAlignment`, and nothing in src reads either field. These fields predate the window, but #22 built the documented multiline feature next to them. They now suggest a per-column switch that does nothing, against the rule to k … **Fix**: Deprecate GridColumnDesc::multiline and ::textAlignment ([[deprecated]] with a message pointing to GridCellData) and remove them in the next API revision. Add a changelog fragment and a docs/controls.md note that multiline and alignment are per cell. Do not try to make them defaults without first adding an optional/uns …

### grid-multiline-9

**Grid text-layout cache internals live in the public header, so every cache change edits DxUi.h and the Grid object layout** `include/DxUi/DxUi.h:3823` · #22, #29, #31 · architecture · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#22, #29 and #31 each changed private cache structures declared in the public Grid class: CellTextLayoutCache, CellDisplayLayoutCache, the scratch string, the generation counter, the ellipsis objects, the Prepare*/Release* helpers and the debug limit. The table algorithms already live in src (DxUi.Internal.h: FindTextLayoutEntry, EndTextLayoutPaint, ResizeTextLayoutTable). Because the data stays in the public header, … **Fix**: Ask the developer whether control internals should move behind src-private implementation types as a general policy. If yes, start with a src-private GridCellTextCache owned through a forward-declared std::unique_ptr, allocated once at construction, which gives the cache a direct unit-test surface. If no, leave the lay …

### grid-selection-lifetime-10

**Deleting the selected row loses the keyboard position: the selection empties and the next arrow key jumps to the top of the list** `src/Controls/DxUi.Grid.cpp:4533` · #43 · ux · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
ReconcileSelectionForVisibleRows looks up the index of the primary row after the model has already changed. When that row was removed, FindRowByStableId returns nullopt, so the nearest-visible-row fallback (which collapse uses) never runs and the selection becomes empty. With an empty selection, OnKeyDown starts from visibleRows.front(). UI Automation focus moves from the row to the grid itself. In a file list, which … **Fix**: Only if the developer decides the library should own this. In `ReconcileSelectionForVisibleRows`, the old row index is not available once the model has changed, so cache it. Store the primary row's last resolved index (`_lastPrimaryRowIndex`) whenever the selection is published, painted or navigated. When `FindRowBySta …

### grid-selection-lifetime-13

**ApplyGroupLayout collapses rows and changes the selection but never publishes to UI Automation or invalidates** `src/Controls/DxUi.Grid.cpp:1376` · #60 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Every other path that changes the grid's visible rows or selection ends with RefreshAccessibilitySnapshot. #60 made this uniform for SetModel, NotifyDataChanged and SetSelectionMode, and ApplyColumnLayout also publishes. ApplyGroupLayout toggles groups through the delegate, reconciles the selection and calls OnGridSelectionChanged, but neither publishes nor invalidates. UIA keeps describing the old rows and selection … **Fix**: At the end of ApplyGroupLayout, take `GetLifetimeToken()` before OnGridSelectionChanged and return if it expired. Then call RefreshAccessibilitySnapshot(), and call RequestInvalidate() if the grid is still alive. Add RefreshAccessibilitySnapshot to the group-header press path before Invalidate(host), guarded the same w …

### grid-selection-lifetime-14

**Nine hand-copied 'copy previous selection, mutate, compare, delegate, lifetime, publish' sequences; two already skip the publish, and each copies the whole selection** `src/Controls/DxUi.Grid.cpp:4443` · #60, #43 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
SetModel, SetSelectionMode, ApplyGroupLayout, NotifyDataChanged, RequestRemoveRowSelection, the group-header press, the keyboard group toggle, OnSelectAll and SelectRow each repeat the same sequence. Each copies the full ordered selection into a vector beforehand, compares in order, takes a lifetime token around the delegate call, then takes another around RefreshAccessibilitySnapshot. #60 had to patch nine sites one … **Fix**: First, extract `[[nodiscard]] bool Grid::NotifySelectionChanged(std::span&lt;const uint64_t&gt; previous)` (compare, delegate under a lifetime token, RefreshAccessibilitySnapshot under a token, return survival) and use it at all nine sites. That fixes the ApplyGroupLayout and header-press drift. Replacing the copy with …

### grid-selection-lifetime-15

**Keyboard group collapse reimplements selection reconciliation: it drops the group's rows even if the delegate did not collapse it, and has a dead branch** `src/Controls/DxUi.Grid.cpp:3994` · #60 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The pointer path for a group toggle calls ReconcileSelectionForVisibleRows against the groups the model now reports. The keyboard path instead builds an unordered_set of every row id in the toggled group (allocated per keypress, potentially 100k entries) and removes those rows from the selection. It does this whenever `collapsed` was requested, whether or not the model actually collapsed the group. Its first fallback … **Fix**: After OnGridGroupToggled, compute visibility from `updatedGroups`. Keep the previous ids, in selection order, whose row is still visible: `FindRowByStableId` + `IsRowVisibleByGroupLayout(row, updatedGroups)`, or a sorted-visible check. Fall back to FindNearestVisibleRow(updatedGroups, primaryRow) only when nothing is l …

### grid-selection-lifetime-16

**Grid::RequestSelectRow / RequestRemoveRowSelection / RequestToggleCheckboxCell return true for a grid that did not survive, unlike Tree's requests** `src/Controls/DxUi.Grid.cpp:2108` · #60 · api-design · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#60 changed Tree's public Request* calls to return false when the delegate or the publish destroyed the tree, and documented that ("Do not reuse a borrowed control pointer after such a publish"). Grid's public Request* calls discard SelectRow's survival result and return true. A consumer that calls them (they are public and undocumented) cannot tell whether its Grid* is still valid. The only safe caller is the UIA co … **Fix**: Return SelectRow's result from RequestSelectRow, and return false after an expired lifetime in RequestRemoveRowSelection and RequestToggleCheckboxCell, matching Tree. The UIA callers at Accessibility.cpp:8468, 8490, 8619, 8749, 8803 and 8866 then report UIA_E_ELEMENTNOTAVAILABLE instead of NOTSUPPORTED when the request …

### grid-selection-lifetime-17

**UIA RemoveFromSelection on a grid row (and a multi-select tree item) touches the host after the selection delegate without checking that it survived** `src/Controls/DxUi.Accessibility.cpp:8871` · #60 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The Select and AddToSelection actions check `ResolveHost() != host` (or survived()) after the request before using `host`. The RemoveFromSelection branches for a grid row and a multi-select tree item call RefreshWindowHostAccessibilitySnapshot and then `host-&gt;Invalidate()` unconditionally. Grid::RequestRemoveRowSelection's comment says 'the caller revalidates its element', but this caller does not. **Fix**: In the GridRow branch of ExecuteRemoveFromSelectionOnWindowThread, add `if (ResolveHost() != host) return UIA_E_ELEMENTNOTAVAILABLE;` after RequestRemoveRowSelection and before the refresh and Invalidate, as in Select and AddToSelection. Combined with item 3, a false result for a destroyed grid also suffices. The tree …

### grid-selection-lifetime-18

**SetSelectionMode(Single) keeps the oldest selected row instead of the current (primary) one** `src/Controls/DxUi.Grid.cpp:1090` · #60 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The model documents that the primary row is last in selection order (GetPrimarySelectedRow, keyboard and UIA focus all use back()). Narrowing to Single mode keeps front() instead, so the row the user last acted on is dropped and the focus jumps. **Fix**: Use `selection.back()` so the primary or current row is kept, matching GetPrimarySelectedRow. Add a test: Ctrl+click rows 2, 7 and 9, call SetSelectionMode(Single), and expect row 9 to stay selected and focused.

### grid-selection-lifetime-20

**Every selected grid row reports HasKeyboardFocus = true; Tree fixed this for its items, Grid still has it** `src/Controls/DxUi.Accessibility.cpp:6090` · #35, #53 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
A GridRow's HasKeyboardFocus is `gridHasFocus && SnapshotGridRowIsSelected`. With a multi-row selection, several elements therefore claim keyboard focus, although the focused fragment (and GetFocus) is the primary row only. For tree items, #35 changed this to SnapshotTreeItemIsFocused, so the same defect remains in Grid. Each query also scans selectedGridRowIds linearly, which costs O(selected) per property read afte … **Fix**: Record the focused grid row id on the control's navigation record when the snapshot is built (the same GetPrimarySelectedRow value used for focusedFragment). Add `SnapshotGridRowIsFocused(record, rowId)` and use it for UIA_HasKeyboardFocusPropertyId. Add a UIA test with three selected rows where exactly one reports Has …

### grid-selection-lifetime-21

**Grid has no focused row separate from its selection; adopting Tree's focused-item model from #35 would remove a whole class of defects** `src/Controls/DxUi.Grid.cpp:1940` · #35, #43, #53 · architecture · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Grid derives its keyboard position, UIA focus, context-menu anchor and accessibility focus from `selection.back()`, which is GetPrimarySelectedRow. That value changes with every Ctrl+click, Ctrl+A, refresh reorder and row deletion, and it disappears with the selection. Tree (#35) uses the same GridSelectionModel but keeps `_selectedItemId` as a focused item distinct from the set, with the modes Replace, Range, Toggle … **Fix**: Treat this as a design proposal for the developer: whether Grid should adopt Tree's focused-item gesture contract (FocusOnly, Toggle, Range, Replace), share the gesture logic next to GridSelectionModel, and retire the documented 'Keyboard differences from Grid'. If accepted, add `std::optional&lt;uint64_t&gt; _focusedR …

### host-core-10

**Posted-payload window registry silently stops registering windows after 128, disabling cross-thread UIA actions for later windows** `src/Support/PostedPayload.h:50` · #47 · bug · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
InitPostedPayloadWindow keeps a fixed array of 128 HWNDs shared by every attached ControlHost and every live menu popup in the process. When the array is full it returns without logging. PostMessagePayload and SendMessagePayload refuse unregistered windows, and DispatchAccessibilityUiActionToWindowThread maps that refusal to UIA_E_ELEMENTNOTAVAILABLE because the last error is ERROR_SUCCESS. Slots can also leak: `Cont … **Fix**: Make InitPostedPayloadWindow return bool. When the window table is full, log through Debug::Error, and make ControlHost::Attach fail (return false) so the consumer sees the failure instead of later UIA errors. In ControlHost::Attach, refuse a different HWND while `_hwnd` is non-null and log it, or detach first. That cl …

### host-core-5

**Registered DxUi messages accept parameters from any sender: CreateProvider writes through a raw lParam pointer, and the detach and UI-action handlers are unauthenticated** `src/Controls/DxUi.Accessibility.cpp:9696` · #41 · security · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
After #41 every DxUi message has a stable, documented name (`RedSalamanders.DxUi.&lt;Component&gt;.&lt;Purpose&gt;.v1`), so any process in the session gets the same value from RegisterWindowMessageW. AccessibilityCreateProvider is still answered by writing a COM pointer to whatever address lParam holds, with no check that the message came from this process. It is the only handler that does not go through the posted-p … **Fix**: Minimal fix (closes the non-hostile scenario 2): - In CreateWindowHostAccessibilityProvider, read the pid as well: `DWORD pid = 0; const DWORD tid = GetWindowThreadProcessId(hwnd, &pid);`. - Return nullptr when `tid == 0 || pid != GetCurrentProcessId()`. - Apply the same check wherever else a raw stack address is sent …

### host-core-9

**SetFocusControl after disabling the focused control publishes and announces an intermediate 'no focus' (the window) before the new control** `src/Controls/DxUi.WindowHost.cpp:4205` · #29 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
PR #29 made PruneStaleInteractionState republish the snapshot whenever it prunes the focus. SetFocusControl runs that prune before applying the requested focus and then publishes again at the end. When the old focus is stale, the first publish moves the snapshot focus from the old control to none. Outside a focus-gain turn, ReporterOfFocusMove returns Host, and AnnounceWindowHostFocus raises UIA focus on the root pro … **Fix**: Make PruneStaleInteractionState report whether it pruned focus, and let callers that publish anyway defer the publish. For example, add an overload `bool PruneStaleInteractionState(bool publishFocusPrune)`. SetFocusControl, OnSetFocus and OnKillFocus pass false. SetFocusControl must still publish on its early-return pa …

### menu-layout-ux-10

**The submenu chevron on a described row is centered on the whole row, while icon, check and shortcut align to the label** `src/Controls/DxUi.Menu.cpp:2848` · #24 · ux · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
For described rows, GetMenuItemLayoutRects moves the accelerator and icon rects onto the primary (label) field: 'Shortcut and selection indicators align to the primary field'. The chevron rect is computed earlier from the full rowHeightDip and is never adjusted. Rows are now 54 DIP or taller, so the chevron of a described submenu parent sits in the vertical middle of the row, next to the description, while the check, … **Fix**: Ask the developer which placement is intended. If the chevron should align with the label, add `if (! item.children.empty()) { layout.chevronRectDip.top = layout.textRectDip.top; layout.chevronRectDip.bottom = layout.textRectDip.bottom; }` to the described branch. Either way, state the rule in docs/controls.md and the …

### menu-layout-ux-11

**The new bad_alloc handlers cannot catch failures in ParseMenuLabel, which is noexcept but allocates; the process terminates instead of refusing the popup** `src/Controls/DxUi.Menu.cpp:362` · #24, #29 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#24's review fixes added catch(const std::bad_alloc&) to PrepareMenuDescriptionLayouts and PopulateMenuAccessibility, so that 'a failed allocation ... terminated the process' becomes a rejected popup. Both call ParseMenuLabel inside those try blocks. ParseMenuLabel is declared noexcept but does displayText.reserve() and push_back(). A bad_alloc thrown there reaches the noexcept boundary and calls std::terminate befor … **Fix**: Remove noexcept from ParseMenuLabel. Every caller that must stay noexcept (ComputeMenuSize, Paint, the accessibility text getters at 2093/6205/6387, the mnemonic lookups) then needs to catch bad_alloc locally with a documented fallback, such as the raw label view or an empty name. The cheaper option is to make ParseMen …

### menu-layout-ux-12

**Described rows build a DirectWrite layout for every row on open, DPI change and root switch, with no bound on row count** `src/Controls/DxUi.Menu.cpp:2689` · #24, #37 · performance · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
PrepareMenuDescriptionLayouts creates and measures a layout for every described row before the popup is shown, and again on each DPI reflow and SwitchRootPopup. The cost is about 17 KB of shaping storage per layout (UI_ControlsAndLayout.md), up to four measure passes per row, plus a 1.6 KB UIA proxy per row (#56). Nothing caps the row count or defers rows outside the viewport, although paint only touches visible rows … **Fix**: Ask the developer for the supported row count of described popups. If large menus are in scope, measure row heights with one reusable layout and keep drawing layouts only for rows near the viewport, bounded and reused like the Grid's last-drawn layout cache, with a paired measurement. Otherwise document the supported u …

### menu-layout-ux-8

**The visible-height clamp that the scrollbar-lane decision must match is written out three times; the no-description branch of PrepareMenuDescriptionLayouts is dead** `src/Controls/DxUi.Menu.cpp:2737` · #24, #37 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The lane is correct only if PrepareMenuDescriptionSize computes the same requested height that CreateMenuPopupWindow and RelayoutMenuPopupForDpi later size the surface with ("NeedsScrollbar applies the same comparison to the final surface ... so the two agree"). That clamp (`maxRootHeightDip &gt; 0 ? min(content, maxRootHeightDip) : content`, root only) is copied three times with slightly different guards (`popup.con … **Fix**: Add `[[nodiscard]] float ResolveRequestedVisibleHeightDip(const MenuPopup& popup, float contentHeightDip) noexcept`, which applies the root-only maxRootHeightDip clamp from popup.isSubmenu and popup.controller. Use it in PrepareMenuDescriptionSize, RelayoutMenuPopupForDpi and CreateMenuPopupWindow (popup-&gt;controller …

### menu-layout-ux-9

**A popup created on the primary monitor and then moved to another DPI prepares every described row layout, and captures the backdrop, twice per open** `src/Controls/DxUi.Menu.cpp:3875` · #24, #37 · performance · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
CreateMenuPopupWindow creates the popup at (0,0), on the primary monitor, so the host attaches at the primary DPI. It then sizes everything for popup-&gt;dpi, the target monitor's DPI, and calls PrepareMenuDescriptionSize, which builds one DirectWrite layout per described row. The later SetWindowPos onto a monitor with another DPI delivers WM_DPICHANGED synchronously; the comments at 3752 and 3989 and the plan descri … **Fix**: Create the popup at its target monitor, for example `CreateWindowExW(..., screenPoint.x, screenPoint.y, 1, 1, ...)`, so the host attaches at popup-&gt;dpi and no DPI change happens during the first SetWindowPos. Keep RelayoutMenuPopupForDpi for real monitor moves while the menu is open. Optionally also short-circuit it …

### menu-loop-lifetime-10

**#47 added a second copy of the cross-thread Pending/Taken/Abandoned dispatch protocol** `src/Controls/DxUi.Menu.cpp:1943` · #47 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
MenuDebugDispatch, MenuDebugPayload, CallMenuPopupDebugProbe and AnswerMenuPopupDebugProbe re-implement the protocol that AccessibilityUiActionDispatch and AccessibilityUiActionPayload already implement in DxUi.Accessibility.cpp. Both use a shared_ptr dispatch, a manual-reset event, a three-state atomic CAS, a payload destructor that marks Abandoned and signals, and a caller that times out, CASes to Abandoned, and ac … **Fix**: If adopted, add a `Detail::CrossThreadCall&lt;Result&gt;` in src/Support/PostedPayload.h. It owns the state atomic, the manual-reset event and the abandon-on-destroy payload, and offers Call(hwnd, msg, post|send, timeout) and Answer(lp, fn). Port the menu diagnostics probes first. Move AccessibilityUiThreadAction onto …

### menu-loop-lifetime-11

**PostedPayload accepts only 128 windows and silently ignores later ones, which then lose UIA actions and menu UIA invoke/focus** `src/Support/PostedPayload.h:50` · #47 · bug · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
InitPostedPayloadWindow stores windows in a fixed `std::array&lt;HWND, 128&gt;` that the whole process shares. When it is full it returns without a diagnostic, and every later PostMessagePayload or SendMessagePayload to that window fails because the window is not registered. Every attached ControlHost HWND and every open menu popup (registered at WM_NCCREATE) takes a slot, and slots are freed only by DrainPostedPaylo … **Fix**: Return bool from InitPostedPayloadWindow and log or assert the failure in ControlHost::Attach and the menu WM_NCCREATE handler, so the limit is never silent. Better: drop the window table and make a payload valid when its entry's hwnd matches and IsWindow(hwnd). The entry already records its window, and the drain on WM …

### menu-loop-lifetime-12

**Show's loop exit duplicates EndAsyncMenuInteraction with a local previousFocus, so the #23 destructor never restores focus for Show** `src/Controls/DxUi.Menu.cpp:5633` · #23 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
RunMenuModalLoop keeps `previousFocus` in a local and reimplements, at its tail, the same release-capture-if-a-popup-holds-it and restore-focus-if-a-popup-has-it logic that EndAsyncMenuInteraction applies with `controller.previousFocus`. #23 made ~MenuController call EndAsyncMenuInteraction for every controller. For Show's stack controller, previousFocus is always null, so that call can never restore focus. Storing p … **Fix**: In RunMenuModalLoop set `controller.previousCapture = GetCapture(); controller.previousFocus = GetFocus();` instead of the local. Replace the tail with a shared EndMenuInteraction(controller) that takes a trace-prefix or mode argument so the existing event names survive, or update the tests that match them. Separately, …

### menu-loop-lifetime-7

**Root switch destroys the active, focused root before activating the new one, so the owner gains and loses focus on every switch** `src/Controls/DxUi.Menu.cpp:4488` · #32 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
SwitchRootPopup calls DestroyPopupChain first. Destroying the active, focused root popup makes Windows activate and focus its owner. Only after that does CreateMenuPopupWindow and ActivatePopupForKeyboard move activation and focus back to the new root. Each menu-bar Left/Right or hover switch therefore gives the owner WM_ACTIVATE, WM_SETFOCUS and WM_KILLFOCUS. ControlHost::OnSetFocus restores its focused control, sta … **Fix**: Create the new root before tearing down the old one. Build it with CreateMenuPopupWindow into a temporary, or insert it ahead of the old root. Then SetCapture and ActivatePopupForKeyboard on it, and only then destroy the old popups, with destroyingPopupWindow set. GetRootPopup() must keep resolving to popups[0], so mov …

### menu-loop-lifetime-8

**PayloadRegistry destructor frees queued payloads without the lock or clearing them, which races with threads still alive at exit** `src/Support/PostedPayload.h:22` · #47 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The function-local static registry's destructor runs during static destruction and calls every entry's destroy function. It does not take the mutex, does not clear entries, and runs payload destructors (AccessibilityUiActionPayload releases COM providers, MenuDebugPayload signals an event) at an arbitrary point in static teardown. Other UI threads keep running until ExitProcess. A UI thread that is still alive can Ta … **Fix**: Make the registry immortal: `static PayloadRegistry& r = *new PayloadRegistry;` with no destructor, and let the OS reclaim outstanding payloads at exit. That removes both the race and the static-teardown ordering hazard. Do not lock-and-drain in the destructor, because running payload destructors (COM releases) during …

### process-docs-specs-2

**docs/controls.md still calls described menus in-progress and pending qualification with an unresolved memory comparison** `docs/controls.md:87` · #24, #33, #37, #52 · process-docs · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The 'Described native menu entries' section says 'The in-progress menu description qualification adds secondaryText' and ends 'the unresolved common-scene memory comparison. The capability remains pending qualification until those gates are resolved.' That is stale. The waiver was removed on 30 September, the one-layout-per-row optimization merged (#37), and the MenuDescriptions plan checks every library gate except … **Fix**: Rewrite only the closing sentence of docs/controls.md (lines 85-87). Suggested text: "The retained measurements separate open-menu layout/accessibility cost and screenshot buffers. The common-scene memory comparison was resolved on 2026-09-30 and the waiver removed (see docs/performance.md). Library qualification is co …

### process-docs-specs-4

**Done plans index omits three plans completed in the window** `Specs/Plans/Done/README.md:3` · #52, #65, #43 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
AGENTS.md says completed plans move to Done and the Done README indexes them. MenuDescriptionMemory_2026-09-27.md (DONE 2 Oct, #52), MergeAndHostHygiene_2026-10-01.md (DONE 1 Oct) and InteractiveMenuValidation_2026-10-04.md (moved in #65) are in Specs/Plans/Done but absent from the index. No validator checks the Done index, so the omission is silent. The WIP README's closing paragraph links Done records by name, so t … **Fix**: Add one-line entries, newest first, for InteractiveMenuValidation_2026-10-04, MenuDescriptionMemory_2026-09-27 and MergeAndHostHygiene_2026-10-01 to Specs/Plans/Done/README.md. Then add a check to the spec validator in Tools/Validation.psm1 that fails when a *.md file under Specs/Plans/Done (other than README.md) is no …

### process-docs-specs-5

**SliderTouchHalo plan still lists Merge as open and says the merge remains, though #62 merged** `Specs/Plans/WIP/SliderTouchHalo_2026-10-04.md:31` · #62, #64 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The plan's status says 'the hosted paired benchmark, the merge and the consumers' PointerEvent::device remain', the checklist has `- [ ] Merge.`, and the WIP README entry repeats it. #62 is merged (27326cf), and #64's own provenance table inspects main at that commit. The unchecked 'hosted paired benchmark verdict' item has no recorded outcome either, though the hosted gate runs on every PR that touches src (which #6 … **Fix**: Check `Merge.` and reference #62/27326cf. Record the hosted paired benchmark's verdict and workflow run id from PR #62, or, if no verdict was recorded, say so and why. Cut the Status line and the WIP README entry down to the consumer `PointerEvent::device` adoption that really remains. Consider moving the plan to Done …

### process-docs-specs-6

**TreeReorder plan still lists the row-straddle paint clip as a known limit although #38 fixed it** `Specs/Plans/WIP/TreeReorder_2026-09-21.md:76` · #38, #35 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Under 'Known limits (not caused by this change)' the plan says 'Tree::Paint does not clip a row that straddles the content rectangle ... the gallery tile is tall enough to show every row instead.' #38 added the clip, and the changelog and UI_ControlsAndLayout.md (line 327) document it. The plan, which still sits in WIP and is read as the record of what Tree does, now states the opposite of the code. **Fix**: Mark the TreeReorder known-limit bullet (lines 76-78) as resolved by #38 and point to the Tree clip test, or delete the bullet. Drop the 'gallery tile is tall enough' rationale.

### process-docs-specs-7

**CiRunScope plan's validation figure does not reproduce (it lists #34), and its hosted-observation items stay open after the merge** `Specs/Plans/WIP/CiRunScope_2026-10-04.md:41` · #61, #34 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The plan says that applied to the last 40 PRs the rules skip the native jobs for '#26, #34, #49, #52 and #58'. Applying the plan's own rules (Tools/NativeScope.psm1) to the merge diffs in the window, the documentation-only PRs are #26, #49, #52, #58 and #64. #34 changes Tools/Validation.psm1 and Tools/tests/Test-Validation.ps1, which are not documentation, so it would run the native jobs. The recorded evidence for th … **Fix**: Correct the Validation figure: drop #34, which is skipped by no rule, and give the true count for the PRs examined. Record the run ids of #61's own PR run, of a documentation-only PR such as #64 or #65 showing the skipped native jobs and the scope summary, and of the push to main that ran all six. Then check the item a …

### public-api-architecture-11

**Menu-internal `transferNativeFocus` flag exposed on public ControlHost::SetFocusControl** `include/DxUi/DxUi.h:4473` · #29 · architecture · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The only caller that passes false is the native menu popup (Menu.cpp:3441). The parameter exists so a menu row can take logical focus while the session's owner keeps Win32 focus. Exposing it publicly adds a consumer-facing mode with a non-obvious failure. For a non-text control in an ordinary WindowHost, passing false leaves Win32 focus on another window, so the 'focused' control never receives WM_KEYDOWN. The docume … **Fix**: Decide whether the parameter is part of the supported API. If not, at the next apiRevision increment, keep the public `SetFocusControl(Control*)` and move the no-transfer variant to a private member. Reach it through the existing friend access (EmbeddedAccessibilityAccess-style, or a ControlHost friend declared in DxUi …

### public-api-architecture-12

**UIA RemoveFromSelection on a grid row reports success and keeps using the host after the selection delegate destroyed the grid** `src/Controls/DxUi.Accessibility.cpp:8866` · #60, #53 · lifetime · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#60 made the UIA Select and AddToSelection paths re-resolve the host (`survived()` / `ResolveHost() != host`) after the control's selection request. The RemoveFromSelection path was left out. For a grid row it calls RequestRemoveRowSelection, which returns true even when OnGridSelectionChanged destroyed the grid ('the caller revalidates its element'). The caller does not revalidate: it publishes and calls host-&gt;In … **Fix**: After grid-&gt;RequestRemoveRowSelection, add `if (ResolveHost() != host) return UIA_E_ELEMENTNOTAVAILABLE;` before RefreshWindowHostAccessibilitySnapshot/Invalidate, matching ExecuteAddToSelectionOnWindowThread. Also make Grid::RequestRemoveRowSelection (and RequestSelectRow, if it shares the pattern) return false whe …

### public-api-architecture-13

**RefreshWindowHostAccessibilitySnapshot dereferences the host after raising events that, by #60's own premise, can dispatch messages** `src/Controls/DxUi.Accessibility.cpp:9575` · #31, #32, #35, #60 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Inside RefreshWindowHostAccessibilitySnapshot, the focus-announcement bookkeeping added with the reactivation/focus-gain-turn work runs after RaiseWindowHostStructureInvalidated. It calls ReporterOfFocusMove(*host), NoteFocusMoveAnnounced(*host) and, after AnnounceWindowHostFocus raised another event, CountFocusAnnouncement(*host). It uses the raw `host` captured before those raises and checks nothing in between. The … **Fix**: Remove the post-raise host access. Decide `ReporterOfFocusMove(*host, focusResolutions)` and call `NoteFocusMoveAnnounced(*host)` (or record the decision) before RaiseWindowHostStructureInvalidated. These read and write only host state that the raise cannot legitimately change. Apply CountFocusAnnouncement, a debug cou …

### public-api-architecture-14

**Tree::SetSelectedItemId in multi-select keeps a non-visible id that GetSelectedItemIds then reports, unlike SetSelectedItemIds** `src/Controls/DxUi.Tree.cpp:543` · #35 · api-design · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
In multi-select mode the two silent setters disagree. SetSelectedItemIds keeps only ids that are visible rows. SetSelectedItemId calls _selection.SetSingle(id) without that check. GetSelectedItemIds, documented as 'the selected items in visible order', then returns an id that is not a visible row, and IsItemSelected reports it. The UIA snapshot filters to visible rows, so UIA and the API disagree. The next NotifyData … **Fix**: Apply one visibility rule to both setters. Implement SetSelectedItemId(id) as SetSelectedItemIds of a one-element span, or as nothing when id is nullopt, so a non-visible id clears the selection in both modes. Alternatively, accept hidden ids in both setters and have ReconcileSelectionWithModel skip the delegate for si …

### public-api-architecture-2

**Revision 3 migration list omits most of #29's source-breaking renames** `Specs/Build/Build_ToolchainAndConsumption.md:209` · #45, #29 · api-design · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#45 introduced the revision rule and says 'Revision 3 covers three changes made at revision 2'. For #29 it lists only the four interface renames. #29's own changelog entry lists many more public renames with no aliases: RunDxUiModalLoop→RunModalLoop, IsDxUiRenderStageActiveForDebug→IsRenderStageActiveForDebug, EmitDxUiRenderMutationBlockedForDebug→EmitRenderMutationBlockedForDebug, and in DxUi::Typography TypographyS … **Fix**: Extend the revision 3 section in Build_ToolchainAndConsumption.md and the matching CHANGELOG.md revision-3 entry (the fragment is already folded). The "Renamed interfaces (#29)" bullet should become "Renamed declarations (#29)" and list, or explicitly point to, the full #29 rename set, with old and new names: - `RunDxU …

### public-api-architecture-7

**The consumer-interface gate checks only that include/DxUi and the MSBuild files exist; renamed C++ declarations and MSBuild properties pass every gate** `Tools/Validation.psm1:643` · #45, #29 · tooling · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#45 states that the API revision must increment when a public header declaration or MSBuild property is removed or renamed. Test-DxUiConsumerInterface checks parameter names only for PowerShell scripts and modules. For the C++ surface and the MSBuild contract it only tests that a path exists. test-consumer.ps1 compiles the repository's own samples, which are edited in the same change as the library. Neither validate- … **Fix**: Add a frozen consumer-API fixture for each revision, for example Tests/ConsumerApi/Revision3.cpp. It names the public types, uses `override` on each delegate and model virtual, and calls each free function such as Typography::GetSpec. Add a matching .vcxproj that references DxUiConsumerOutputRoot and the other document …

### public-api-architecture-8

**Consumer-interface rule and validator treat a new mandatory parameter as a compatible addition** `Tools/Validation.psm1:627` · #45 · tooling · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The revision rule says it increments when a consumer 'could stop ... without a change of its own'. Yet the spec says 'a new parameter is an addition', and Test-DxUiConsumerInterface only checks that each listed parameter still exists. Adding a `[Parameter(Mandatory)]` parameter to Get-DxUiConsumerBuildIdentity, validate_consumer.ps1 or another listed entry breaks every pinned consumer's call (PowerShell prompts or fa … **Fix**: In Get-ScriptParameterNames and Get-ModuleFunctionParameters, also collect the names of mandatory parameters: a ParameterAst whose ParameterAttribute has Mandatory set (named argument with no value, or $true). Fail when a listed script or function has a mandatory parameter that is not in its capabilities.json list. Opt …

### public-api-architecture-9

**Public ThemeColors.h comment contradicts unsupplied-alert behavior shipped in #29** `include/DxUi/ThemeColors.h:18` · #29 · api-design · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The public header comment added in #28 says unsupplied (zero-alpha) alert colors make MakeThemePalette use windowBackground and text. #29 changed ResolveAlertTone: outside high contrast an unsupplied alert keeps the library's default tones, and only high contrast falls back to windowBackground/text. The changelog and the design-system README describe the new behavior; the header, which consumers read first, still des … **Fix**: Reword ThemeColors.h:18 to match ResolveAlertTone. Zero alpha means none supplied: MakeThemePalette keeps DxUi's default alert tone, or uses windowBackground/text in high contrast. When only one of a fill/text pair is supplied, the other is adjusted for 4.5:1 contrast.

### test-support-infra-5

**FocusEventClient duplicates UiaTest::Client, and its worker silently stops after 30 s idle and signals finished before releasing COM** `Tests/Controls/Controls.Tests.DxUiFocusEventClient.h:179` · #31, #63 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
FocusEventClient (#31) is a parallel UIA client: its own MTA thread, request event, pump-while-waiting loop and recorder. UiaTest::Client (#51/#53) already offers all of this: Subscription::focus records focus events with the cached name, plus a generic request mechanism. The duplicate has drifted. (a) Its worker waits on its events with a 30-s timeout, and on timeout it exits the loop, removes the focus handler and … **Fix**: Replace FocusEventClient with UiaTest::Client(target, Subscription{.focus = true}, [&]{ window.PumpMessages(); }). Add a FocusElementNamed request built on TryAsk (FindFirst by name under ElementFromHandle(target), then SetFocus). Express Count(name), LastName and PrintNames through Count(matcher), Events() and PrintEv …

### test-support-infra-6

**The watchdog does not cover a hang in process exit after main returns, although its comment says it does** `Tests/Support/Support.Tests.TestWatchdog.h:44` · #31 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
The singleton is leaked so that 'a hang inside exit() ends as well', but Watch() only expires an armed unit. After main returns, every ScopedTestDeadline has been disarmed, so the watchdog thread waits forever on `_name != nullptr`. A hang in CRT teardown (static or thread_local destructors, DLL detach, UIA or COM teardown, the class of problem #23 fixed for menus) is unbounded again. Only MenuExitLifetime, a fixture … **Fix**: Just before each `return` from wmain that follows suite execution, arm a deliberately never-disarmed unit (e.g. `TestWatchdog::Instance().Arm("process exit")`). Use a short limit or the normal one, so a CRT/DLL-detach/COM teardown hang reports `TIMEOUT: process exit` with exit code 124. If exit hangs are meant to be le …

### test-support-infra-7

**UiaTest::Client::TryAsk destroys the request functor while the client thread may still be running it** `Tests/Support/Support.Tests.UiaTestClient.h:598` · #51 · threading · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
On a request timeout, TryAsk assigns `_task = nullptr` before calling Fail. The client thread may at that moment be inside `_task(session)` (a provider call that is slow or blocked on the owner thread). Destroying a std::function while another thread invokes it is a data race and undefined behavior. Instead of the intended 'FAILED: &lt;what&gt;' diagnostic, the run can crash with an access violation, or the late answ … **Fix**: On timeout, call Fail before touching `_task`: `if (! done) Fail(what); _task = nullptr; return answered;`. Since Fail is [[noreturn]], `_task` is never destroyed while the client thread may still be running it.

### test-support-infra-8

**The console-close grace period is shorter than the lease's own worst-case clean-up** `Tests/InteractiveLease/InteractiveLease.Tests.Runner.cpp:63` · #48 · bug · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
On CTRL_CLOSE_EVENT the handler waits 4.5 s for g_finished, and Windows ends the process about 5 s after the event. The interrupted path on the main thread can wait up to 10 s for the terminated child (ChildProcess.h:202). Then RestoreForeground can take 3 × (ActivateAnchor up to 0.5 s + Settles up to 1 s), RestoreFocus up to 1 s more, and then the result write. When a child is slow to die or the foreground is contes … **Fix**: Restore the cursor first: it is instantaneous and independent of the foreground. On an interrupt that came from CTRL_CLOSE/LOGOFF/SHUTDOWN, cap restoration at one foreground attempt (record the event type in an atomic that DesktopLease consults, or pass a reduced attempt count), so the whole cleanup fits inside about 3 …

### tests-grid-tree-render-2

**Touch-halo animation test races the wall clock, the flake #44 fixed for tooltips** `Tests/Controls/DxUi.Tests.Animation.cpp:447` · #62 · flakiness · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
Slider transitions take their start time from ::GetTickCount64() inside BeginVisualTransition. The new test reads its own GetTickCount64() before OnMouseDown/OnMouseUp, then ticks at fixed offsets from that reading and requires progress strictly in flight or settled. If the runner stalls between the test's clock read and the control's, the control's start time is later than the test assumes. AdvanceVisualTransition t … **Fix**: Follow the PageHost pattern: make the test's tick times come from the transition's own start, not from a second wall-clock read. 1. Add a test-only accessor to Slider (DxUi.h, next to DebugGetTouchHaloProgress), for example `[[nodiscard]] uint64_t DebugGetTouchHaloStartTickMs() const noexcept { return _touchTransition. …

### tests-grid-tree-render-3

**Callback-destroys-control tests assert things that hold even after a use-after-free** `Tests/Controls/DxUi.Tests.Tree.cpp:588` · #35, #57 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
TestTreeSelectionSetCallbackMayDestroyTheTree checks `host.GetRoot() != nullptr`, and RequireFocusReplacementLeavesControlAlone checks `replaced` and `GetFocusControl() == nullptr`. Both hold whether or not the destroyed control touched itself afterwards: the callback always installs a fresh Panel and clears focus. So in x64/ARM64 Debug and Release (4 of 6 CI configurations), these tests cannot fail for the bug their … **Fix**: In TestTreeSelectionSetCallbackMayDestroyTheTree, have RootReplacingDelegate record `_host.DebugGetInvalidateCount()` right after SetRoot. After each gesture, require the count to be unchanged. In RequireFocusReplacementLeavesControlAlone, capture `host.DebugGetInvalidateCount()` inside the focus callback after `host.S …

### tests-grid-tree-render-4

**No test covers a focus callback that destroys the control during native SetFocus** `Tests/Controls/Controls.Tests.DxUiTestHelpers.h:2404` · #57 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
FocusControlAndSurvive(host, control, takeNativeFocus=true) calls SetFocus(hwnd) and returns false if that destroyed the control. Button::Invoke(focusSelf), Button right-press, Control::OnMnemonic, Toggle::OnMnemonic and RadioButton::OnMnemonic all use this branch. RequireFocusReplacementLeavesControlAlone always builds a WindowHost without an HWND, so host.GetHwnd() is null and the branch never runs. Its replacing c … **Fix**: Add an AttachedHostWindow variant of the helper for the takeNativeFocus=true callers: Button::Invoke(host, true), Button right press, Toggle, RadioButton and Control mnemonics. Start with no host focus control and native focus elsewhere, so WM_SETFOCUS takes the `SetFocusControl(FindAdjacentFocusable(...))` path. The f …

### tests-grid-tree-render-5

**Grid repeats the selection-changed delegate pattern in 8 places; one helper would remove the ordering mistakes** `src/Controls/DxUi.Grid.cpp:1037` · #57, #60 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Grid publishes a selection change with the same hand-written block in 8 places: SetModel, SetSelectionMode, NotifyDataChanged, RequestRemoveRowSelection, the group-header press, the keyboard group toggle, OnSelectAll and SelectRow. Each block copies the previous selection, compares, takes a lifetime token, calls the delegate and checks expiry. #57 and #60 had to fix these one by one, and the #60 plan records one site … **Fix**: Add a private `[[nodiscard]] bool Grid::PublishSelectionChangeAndSurvive(std::span&lt;const uint64_t&gt; previousSelection)`. It returns true when `_delegate` is null or the selection is unchanged. Otherwise it takes GetLifetimeToken(), calls OnGridSelectionChanged(*this) and returns `!lifetime.expired()`. Route all ni …

### tests-menu-a11y-host-2

**Focus-gain turn tests depend on a 500 ms wall-clock limit (the timing class #44 removed from the tooltip tests)** `Tests/Controls/DxUi.Tests.WindowHost.cpp:3212` · #32 · flakiness · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
`ControlHost::IsInFocusGainTurn` ends the turn after `GetTickCount64() - _focusGainTurnStartedMs &gt;= 500`. Many tests start a turn with WM_SETFOCUS and then assert in-turn decisions several statements later, with no control over that clock. The decisions they read are DebugIsInFocusGainTurn and DebugGetFocusMovesLeftToSystemCount. Some of those statements run UIA provider calls (ReadFocusedElementName) or std::form … **Fix**: Give the turn limit a diagnostics seam instead of reading GetTickCount64 directly. Two options: - An injectable clock: a per-host `ULONGLONG (*)() noexcept` or a `DebugSetFocusGainClockForTest`, defaulting to GetTickCount64. - A `DebugSetFocusGainTurnLimitForTest(ULONGLONG ms)` override, where 0 or max means no limit. …

### tests-menu-a11y-host-3

**Hand-written UIA client threads stop listening after fixed timeouts, so late "no event" checks can pass vacuously; they also duplicate UiaTest::Client** `Tests/Controls/DxUi.Tests.Accessibility.cpp:5891` · #51, #53, #54, #63 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
Five tests each start their own MTA UIA client thread, subscribe, then stop listening after a fixed timeout: StructureInvalidation, DisclosureSender, the multi-select SelectionEventObserver, MenuRowFocusObserver, and FocusEventClient's request loop. Each copies the thread setup, the waitUntil pump and the stopClient scope_exit, and each uses a plain CLSID_CUIAutomation. After the timeout the client removes its handle … **Fix**: Port RunTreeMultiSelectSelectionEventsTest to UiaTest::Client with SelectionEventsSubscription() and UiaTest::HearSelectionEvents, and compare with SortedEvents/DescribeEventList. Expected strings then include the control type ("Selected TreeItem 'Élément 2'"). This removes SelectionEventObserver, HeardSelectionEvent a …

### tests-menu-a11y-host-4

**Early-return path of the cursor test leaves the async menu open while its callback holds a reference to a dead `closed` local** `Tests/Controls/DxUi.Tests.Menu.cpp:8029` · #63 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
In the per-attempt lambda, `owner` is declared first and `closed` later, at line 8029. The ShowAsync callback captures `&closed`. When `WaitForOwnedContextMenuPopupWindowByFirstItemText` returns null, `finish()` is returned. The `dismiss` scope_exit then calls DismissOwnedContextMenuPopupChain, which only finds visible owned popups. A popup that is not found therefore stays open. `closed` goes out of scope before `ow … **Fix**: Declare `bool closed = false;` before `AttachedHostWindow owner;` in the attempt lambda, or capture a std::shared_ptr&lt;bool&gt;. Optionally, have the dismiss scope_exit also destroy any remaining DxUi_ContextMenu window owned by `owner`, visible or not, before the locals unwind.

### tests-menu-a11y-host-5

**The "lost foreground announces nothing" test uses only a 500 ms client observation, not the host's deterministic announcement counter** `Tests/Controls/DxUi.Tests.Menu.cpp:7872` · #32 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
TestWindowHostThatLostTheForegroundAnnouncesNoFocusChange claims that a window that lost the foreground raises no focus event. It checks this only by counting what an in-process UIA client heard during `client.Settle()` (500 ms). The neighbouring tests in this file and in WindowHost assert `host.DebugGetFocusAnnouncementCount()`, which reads the decision directly. Event delivery to the client is asynchronous: an anno … **Fix**: Before `window.Host().SetFocusControl(second)`, record `const uint64_t announcedBefore = window.Host().DebugGetFocusAnnouncementCount();`. After the move, add `Require(window.Host().DebugGetFocusAnnouncementCount() == announcedBefore, "the host announces nothing for a window that lost the foreground");`. Keep the clien …

### tests-menu-a11y-host-6

**Description tests that need activation silently pass in the NewControls lane, and one test conditionally skips its key checks without reporting it** `Tests/Controls/DxUi.Tests.Menu.cpp:6372` · #24, #56 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
RunMenuDescriptionTests runs in both the NewControls lane (non-activating; CI and default runs) and the Menu lane (interactive only). Three tests simply `return` when windows cannot activate, without calling SkipDxUiTest: TestDescribedAsyncMenuRestoresFocusedOwnerChild, TestDescribedSubmenuSliderFocusKeepsSessionFocus and TestMenuItemRoleOutsideMenuPopupTransfersNativeFocus. The log records `[DONE]` as if they had pa … **Fix**: Replace the bare returns with `SkipDxUiTest("... needs the activating Menu lane"); return;`. Alternatively, split RunMenuDescriptionTests into a non-activating list, run in NewControls, and an activating list, run only in RunMenuTests, so that NewControls does not list tests it cannot run. In TestDescribedPointerMenuAc …

### text-input-12

**An IMM composition is never cancelled in the IME when focus moves between DxUi fields of one HWND, so it continues in the new field** `src/Controls/DxUi.NativeTextInput.cpp:618` · pre-existing · ux · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Every DxUi text field shares the host HWND and therefore one HIMC. When focus moves to another control, DeactivateNativeTextInputSession restores the old field to its composition base and clears DxUi's composition state, but it never tells the IME: there is no ImmNotifyIME(NI_COMPOSITIONSTR, CPS_CANCEL or CPS_COMPLETE) anywhere in the file. The IME still holds its composition string. Its next WM_IME_COMPOSITION is ha … **Fix**: This follows the spec's "discard on focus loss" policy. In DeactivateNativeTextInputSession, when `_nativeTextInputImeComposing` is set and `_hwnd` is valid, do these steps in order: 1. Capture the edit target and its base state. 2. Call `ClearNativeTextInputCompositionState()` first. Any WM_IME_COMPOSITION or WM_IME_E …

### text-input-15

**An IME message that reaches the host while it lacks focus re-activates the session, and the session's SetFocus takes the keyboard focus back** `src/Controls/DxUi.NativeTextInput.cpp:1230` · #32 · ux · plausible (single: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
HandleNativeTextInputImeMessage requires only that the retained `_focusedControl` support text input. OnKillFocus(false) keeps `_focusedControl` but deactivates the session, so on a later WM_IME_STARTCOMPOSITION, WM_IME_COMPOSITION or WM_IME_ENDCOMPOSITION, `editTarget != _nativeTextInputControl`. ActivateNativeTextInputSession(editTarget) then runs `if (_hwnd && GetFocus() != _hwnd) SetFocus(_hwnd);`. No check confi … **Fix**: Make IME-message handling refuse to (re)activate a session unless the host actually owns focus. In HandleNativeTextInputImeMessage, after the backend/control guard, add: ``` if (_hwnd && GetFocus() != _hwnd) { return false; } ``` DefWindowProc then handles the stray message. Alternatively, activate only when `_nativeTe …

### text-input-16

**IME/system caret rect for multiline is not clipped to the viewport, unlike the painted caret and range rects** `src/Controls/DxUi.TextInput.cpp:2695` · #55 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#55 hides a caret that the wheel scrolled out of the viewport, but only in Paint. GetTextInputCaretRect for multiline still returns the unclipped rect. UpdateNativeTextInputCaret feeds that rect to CreateCaret/SetCaretPos (the system caret that Magnifier and Narrator follow) and to the IMM composition and candidate windows. GetTextInputRangeRects already clips with ClipTextInputRectToBounds. The behaviours are now in … **Fix**: Clamp the multiline result in GetTextInputCaretRect to textRect, matching the range rects. Clamp the top and bottom with ClipTextInputRectToBounds and, when the caret is fully outside, return a 1-px edge rect at the nearest viewport edge rather than nullopt, so TSF and IMM keep a stable anchor. Keep Paint's clip. Also …

### text-input-17

**Password-reveal UIA Invoke reports success after a focus callback destroyed the field** `src/Controls/DxUi.TextInput.cpp:1240` · #57, #55 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
InvokePasswordRevealButton returns true when FocusControlAndSurvive reports that the field died, and the UIA provider maps true to S_OK. #55/#57 made element SetFocus and Invoke report UIA_E_ELEMENTNOTAVAILABLE when their control did not survive the focus change. This path breaks that contract and is not covered by the #57 tests, which cover press, double click and context menu only. The reveal state is also toggled … **Fix**: Make InvokePasswordRevealButton report death separately. Use a tri-state return, or in the provider capture GetControlLifetimeToken(*revealTextField) before the call and return UIA_E_ELEMENTNOTAVAILABLE when the token expired. Optionally focus first and toggle only after the field survives, matching Button::Invoke. Add …

### text-input-18

**Deferred TSF lock re-posts itself without waiting while a lock is held across a nested message loop** `src/Controls/TextInputServices.cpp:739` · #41 · performance · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
When the registered deferred-lock message is dispatched while a lock is still held, DispatchPendingLock returns TS_E_NOLOCK. HandleMessage then immediately PostMessage()s the same message again. If the lock is held across a nested message loop, the loop dequeues it and HandleMessage re-posts it again, spinning the UI thread at 100% CPU until the outer lock ends. A nested loop can come from a TIP or a TextInputClient: … **Fix**: Do not re-post on TS_E_NOLOCK. In HandleMessage, leave _pendingLockFlags set and return. At the end of TextStoreACP::RequestLock, after `_lockFlags = 0u;`, call `_target-&gt;ScheduleLock()` when `_pendingLockFlags != 0`. lockPosted is false after HandleMessage, so this posts once, after the outer lock is released. Also …

### text-input-20

**EM_SETSEL with a reversed range drops the selection, and a start of -1 moves the caret to 0** `src/Controls/DxUi.NativeTextInput.cpp:1158` · pre-existing · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The Win32 EM_SETSEL contract says the start may be greater than the end (the lower value starts the selection) and that a start of -1 removes the selection. The mapper turns any negative value into text.size(), and SetTextInputSelectionRange collapses every start &gt;= end to a caret at `end`. **Fix**: In the EM_SETSEL case, handle `static_cast&lt;int&gt;(wp) == -1` first: clear selectionAnchorIndex and keep caretIndex. Keep the end sentinel mapping only for lParam. Add a raw-assign path that sets anchor = start and caret = end whenever start != end, so a reversed range keeps the active end at the lower index. Add EM …

### tooling-perf-gate-5

**Publish-BenchmarkVerdict without -Reports judges the newest summary.json on disk, which can be a previous run's after the current run failed** `Tools/Publish-BenchmarkVerdict.ps1:34` · #46 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
When -Reports is not given, the script uses Find-PairedSummary, which returns the summary.json under .build/paired/* with the newest LastWriteTime. It does not check that the summary belongs to the run that just executed. On a fresh hosted runner there is only one run. On a local machine or a reused or self-hosted runner, .build/paired keeps earlier runs, because the worktrees are deliberately left in place. A paired … **Fix**: Have the workflow pass -Reports explicitly. performance-paired.ps1 can emit the reports path as a step output, or write it to .build/paired/latest-run.txt when the run directory is created. Without -Reports, fail with 'no verdict' when more than one run directory exists, or when the newest run directory (by name or cre …

### tooling-perf-gate-6

**The paired run builds the whole solution (control, foundation and lease tests, sample) twice when only DxUi.EmbeddedTests is measured** `performance-paired.ps1:192` · #46 · performance · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Each side is built with build.ps1, which builds DxUi.sln with target Build. That compiles every project, including the large Tests/Controls suite, the foundation tests, the interactive lease executable and the EmbeddedControls sample, for both the baseline worktree and the candidate. The paired measurement only runs .build/x64/Release/DxUi.EmbeddedTests.exe. On the hosted gate this is most of the build time inside a … **Fix**: Optional. Have performance-paired.ps1 invoke MSBuild directly for Tests/Embedded/DxUi.EmbeddedTests.vcxproj, resolving MSBuild through Tools/VisualStudio.psm1 and passing the same DxUiOutputRoot, Configuration and Platform properties. Its ProjectReference builds DxUi.lib. This avoids depending on the baseline's build.p …

### tooling-perf-gate-7

**The control-drift rule is implemented twice and must stay in sync (performance-paired Compare-Measurement and BenchmarkGate Get-ControlDrift)** `performance-paired.ps1:144` · #31, #46 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
performance-paired.ps1 classifies each same-binary control as stable-control or unstable-control with its own predicate. Get-ControlDrift re-reads the same comparison files and re-implements the predicate with its own null and exact handling (Test-SameNumber vs '-ne'). The gate uses the per-metric result for 'held' and the summary's status for the Unstable count and the Strict reading. If one copy changes, for exampl … **Fix**: Add and export Test-ControlChangeDrifted ($change) from Tools/PerformanceComparison.psm1, which both scripts already import or can import. Use it in Compare-Measurement and in Get-ControlDrift. In Get-ControlDrift, count Unstable from its own per-control results (a control with any drifted change) instead of the summar …

### tooling-perf-gate-9

**The native-scope 'license' rule names a file that does not exist; the repository's license is LICENSE.txt, which the test pins as non-documentation** `Tools/NativeScope.psm1:22` · #61 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Get-DocumentationScopeRules declares 'LICENSE' as documentation, but the only tracked license file is LICENSE.txt. The rule matches nothing, and Test-NativeScope.ps1:69 asserts that 'LICENSE.txt' is not documentation, so the dead rule is locked in. The effect is conservative (the native jobs run), but the rule and its test describe a skip that never happens. **Fix**: Change the rule's path to 'LICENSE.txt'. In Test-NativeScope.ps1, replace 'LICENSE' with 'LICENSE.txt' in the documentation list at line 47. At line 69, replace 'LICENSE.txt' with another look-alike name that must stay native, such as 'LICENSE.txt.bak' or 'LICENSES/x.txt', so the prefix-match test is kept.

### tooling-runners-10

**Test-TestFilter re-runs the whole unfiltered Grid suite in every test.ps1 run** `Tools/tests/Test-TestFilter.ps1:66` · #31 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid for every test.ps1 -Full profile; Test-Changes runs it only when its scope is affected.  
test.ps1 runs Test-TestFilter.ps1 before the suites whenever a control suite is selected, which is every default run in all three required configurations. Its last case runs DxUi.ControlTests.exe --suite=Grid with no filter, which executes the whole Grid suite only to compare the started names with the DXUI_RUN_TEST registrations. test.ps1 then runs the Grid suite again, and its log, test-Grid-&lt;platform&gt;-&lt;co … **Fix**: Decide where the guarantee should live. Option A: keep the case in Test-TestFilter but skip it when 'Grid' is in -Suites, and have test.ps1, after its own Grid run, compare the '  [START]' lines of test-Grid-*.log (unfiltered, non-interactive) with the DXUI_RUN_TEST registrations. Option B: add a cheap `--list-tests` t …

### tooling-runners-2

**A default test.ps1 run takes real focus and the pointer through Menu/NativeTextInput with no lease, consent or restoration** `test.ps1:29` · #48 · ux · confirmed (single: confirmed/low)  
**At e5ebbb5**: Fixed by #66: the default suite list no longer has Menu or NativeTextInput, and a local run that names them without -Interactive is refused (test.ps1:29, :56).  
#48 added the lease so that the four suites that take the real desktop run only with the person's agreement and are restored afterwards. The default -Suites list still contains Menu and NativeTextInput, and Get-SuiteRun deliberately gives them no --no-activate. So `test.ps1` with no options, which AGENTS.md requires for every code change in three configurations, runs them outside the lease. They take the foreground a … **Fix**: In test.ps1, before the build: - If `-not $Interactive` and `-not $PSBoundParameters.ContainsKey('Suites')` and `$null -eq (Get-DxUiInteractiveRefusal)` (a person's desktop), remove the names from `Get-DxUiInteractiveSuiteNames` from $Suites. - Print one line, for example: "Menu, NativeTextInput left out: they take the …

### tooling-runners-3

**validate-build-matrix.ps1, a consumer-interface script, cannot run in Windows PowerShell 5.1: Validation.psm1 fails to import** `Tools/Validation.psm1:7` · #30, #45 · api-design · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
capabilities.json lists validate-build-matrix.ps1 (-Root) as consumer interface. The revision-3 changelog tells consumers to replace the Python build-matrix validator with `validate-build-matrix.ps1 -Root &lt;consumer root&gt;`. That script imports Tools/Validation.psm1, which turns on strict mode and then reads `$IsWindows` at module scope. That automatic variable does not exist in Windows PowerShell 5.1, so the imp … **Fix**: Make the contract explicit at the consumer boundary without porting anything: (a) Add `#Requires -Version 7.2` as the first statement of validate-build-matrix.ps1, so a 5.1 run fails with "requires PowerShell 7.2" before the module import. (b) In Build_ToolchainAndConsumption.md's consumer-interface list, state the run …

### tooling-runners-4

**Relative -Root is resolved against the process directory, not the PowerShell location, so the wrong checkout is validated or folded** `Tools/Validation.psm1:781` · #30, #58 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
Test-DxUiBuildMatrix (behind the consumer-facing `validate-build-matrix.ps1 -Root`) and Fold-Changelog.ps1 resolve -Root with [IO.Path]::GetFullPath. That uses the process working directory, which Set-Location does not change in PowerShell 7. Tools/VcpkgTriplet.psm1:87-89 avoids this deliberately ('Relative paths mean the caller's PowerShell location, which .NET file calls do not know'), but the same defect class rem … **Fix**: At every script boundary that takes a user path, resolve it with `$PSCmdlet.GetUnresolvedProviderPathFromPSPath($Root)` (or `$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath`) before any .NET call: validate-build-matrix.ps1 (or Test-DxUiBuildMatrix with [CmdletBinding()]), Fold-Changelog.ps1, bui …

### tooling-runners-5

**An interactive suite ended by the lease's bound is reported as 'the runner reported a timeout without naming the test'** `test.ps1:165` · #48 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
In an interactive run test.ps1 takes only `$child.ExitCode` from the lease result and ignores `$child.TimedOut` and `$child.Interrupted`, which Read-DxUiLeaseResult parses. When the lease ends a child at its bound (15 min, or 3 x -TestTimeout), it terminates the job with code 124. The runner wrote no TIMEOUT line, so Get-SuiteFailureReport attributes the 124 to the runner's watchdog and says it did not name the test. … **Fix**: In the interactive branch, when `$child.TimedOut` is true, build the summary as "$suite was ended by the interactive lease after its $bound s bound" and add the last '  [START]' test in the log that has no matching '[DONE]', or say 'before any test started'. Record `timedOut` and `interrupted` in the receipt's lease bl …

### tooling-runners-6

**`-Interactive -Tests` without -Suites takes the person's desktop for a run that is guaranteed to fail** `test.ps1:65` · #48 · ux · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged, line moved.  
Without -Suites, -Interactive runs Menu and NativeTextInput, and -Tests passes every name to every control suite. A name must exist in every selected suite, so the runner exits 2 for a suite that lacks it. test.ps1 checks only that some control suite is selected and validates names nowhere before the lease. The person is asked, the desktop is taken, Menu runs the named test, and NativeTextInput then fails with 'Unkno … **Fix**: In the `if ($Interactive)` block at test.ps1:37-41, throw before any build when `$Tests.Count -and $Suites.Count -gt 1`, for example "-Interactive -Tests needs one suite: add -Suites Menu or -Suites NativeTextInput". Optionally, validate the names against each suite with a runner listing before the lease starts.

### tooling-runners-7

**Test-InteractiveMode's 'no log or receipt was written' checks fail spuriously while any build or test writes to .build/logs** `Tools/tests/Test-InteractiveMode.ps1:74` · #48 · flakiness · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Two cases snapshot every file in .build/logs and .build/reports, including each LastWriteTimeUtc, before and after a refused child `test.ps1 -Interactive`, and require identical snapshots. These directories are shared with every other repository tool: build.ps1 writes its MSBuild file log to .build/logs, and test.ps1 writes suite logs and receipts there. validate.ps1 (which runs these tooling tests) is routinely run … **Fix**: Limit the snapshot to what a refused interactive run could write: `*.interactive*.log`/`*.interactive*.json`, interactive-plan.txt, interactive-result.txt and Performance-*.json in .build/reports. Or compare only files whose LastWriteTimeUtc is after the case start and whose names match test.ps1's patterns, so unrelate …

### tooling-runners-8

**Test-AsanRuntime.ps1 leaves a new .build/ToolTests/AsanRuntime-&lt;guid&gt; directory behind on every test.ps1 run** `Tools/tests/Test-AsanRuntime.ps1:9` · #30 · resource-leak · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Every test.ps1 run (every configuration) calls Test-AsanRuntime.ps1. It creates a GUID-named fixture with a project, two MSBuild logs and a staged copy of a synthetic runtime, and never removes it, on success or failure. #30 touched the script (discovery and strict mode) but kept the leak. The neighbouring tooling tests remove their fixtures after validating the path (TestSupport.psm1 Remove-FixtureRoot; Test-Consume … **Fix**: Wrap lines 9-35 in try/finally. In finally, check that `[IO.Path]::GetDirectoryName($fixture)` equals the full path of `$repo/.build/ToolTests` and that the leaf matches '^AsanRuntime-[0-9a-f]{32}$', then call `Remove-Item -LiteralPath $fixture -Recurse -Force`. Or move the test onto TestSupport's fixture-root helpers, …

### tooling-runners-9

**format.ps1 keeps its own vswhere discovery, which the shared-discovery rule and its test miss** `format.ps1:11` · #30, #40 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#40 moved Visual Studio discovery into Tools/VisualStudio.psm1 ('The one discovery'), and Test-VcpkgTriplet enforces it for build.ps1, vcpkg-install.ps1, test-consumer.ps1 and Test-AsanRuntime.ps1, but not for format.ps1. format.ps1's fallback still calls vswhere directly. It does not check that vswhere exists (on a machine without the VS Installer, `& $vswhere` throws a 'not recognized' error instead of the script's … **Fix**: Either drop the Visual Studio fallback entirely, since the 22.1.3 pin makes the pinned install or PATH the real sources, and fall straight to the friendly throw. Or replace lines 11-13 with `try { Import-Module (Join-Path $PSScriptRoot 'Tools/VisualStudio.psm1') -Force; $FormatterPath = Join-Path (Get-DxUiVisualStudioI …

### tree-10

**Expansion/collapse animation does linear id scans per painted row and per hidden descendant on every frame** `src/Controls/DxUi.Tree.cpp:867` · #38 · performance · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Paint (restructured by #38 around the clip) recomputes loop-invariant parent indices with linear scans for every visible row. In the collapse pass it also scans `afterItems` linearly for every descendant of the collapsed parent before testing whether that row is on screen. That is O(descendants x rows) per frame for the 320 ms animation. The cost predates the window, but it sits inside the clipped paint #38 touched. **Fix**: Hoist `beforeParentIndex`/`afterParentIndex` out of the row loop, or compute them once in BeginTreeExpansionAnimation and store them in the animation state. In the collapse loop, drop the afterItems scan: descendants of a collapsed parent are absent from afterItems by construction, and NotifyDataChanged already clears …

### tree-11

**Selection-change publication sequence is hand-copied at five sites with diverging lifetime handling** `src/Controls/DxUi.Tree.cpp:2064` · #35, #60 · simplification · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Five sites each repeat the same sequence: copy the previous ordered selection, mutate, optionally call OnTreeSelectionChanged with a lifetime check, call NotifySelectionSetChanged, then take a token, RefreshAccessibilitySnapshot and check the token. The sites are SelectVisibleIndex (1982-2034), OnSelectAll (2151-2183), CollapseSelectionToItem (2088-2097), RequestRemoveVisibleItemFromSelection (2131-2141) and NotifyDa … **Fix**: First decide one canonical order: focus callback, then set callback, then UIA publish, or publish first as in NotifyDataChanged. Then add a private `[[nodiscard]] bool PublishSelectionChange(const std::vector&lt;uint64_t&gt;& previous, std::optional&lt;uint64_t&gt; announcedFocus)` that checks lifetime after each step …

### tree-12

**Two copies of the row layout computation (and a third copy of the expander rectangle in the hit test)** `src/Controls/DxUi.Tree.cpp:1657` · #38 · simplification · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
ComputeItemLayoutMetrics(host, size_t visibleIndex, item) and ComputeItemLayoutMetrics(host, float rowTopDip, item) are line-for-line identical after the first statement. HitTestPoint re-derives the expander rectangle with the same constants (8 DIP + depth*indent, 12 DIP wide, 6 DIP vertical inset). A layout change (RTL mirroring, density, the #38 clip inset) must be applied in three places, or hit testing drifts fro … **Fix**: Make `ComputeItemLayoutMetrics(host, size_t visibleIndex, item)` compute rowTop and return the rowTop overload's result. Extract a private `ComputeExpanderRect(float rowTopDip, uint32_t depth) const noexcept` used by both the layout function and HitTestPoint, instead of having HitTestPoint call the host-dependent overl …

### tree-13

**Tree contract and WIP plan still state limits that #38 and #43 removed** `Specs/Plans/WIP/TreeReorder_2026-09-21.md:76` · #35, #38, #43 · process-docs · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The WIP plan's 'Known limits' still says Tree::Paint does not clip straddling rows, which #38 fixed. The plan (line 56) and the normative spec (UI_ControlsAndLayout.md line 403) still say membership tests are 'linear in its size, as in Grid'. Since #43, GridSelectionModel binary-searches selections above 1,024 ids. AGENTS.md requires public capability status to stay truthful. **Fix**: In the WIP plan, mark the clip limit resolved by #38 (4cc3562) and drop the 'gallery tile is tall enough' workaround note. Update plan line 56 and UI_ControlsAndLayout.md:403 to: scan up to 1,024 ids, binary search above, per GridSelectionModel. Re-check the gallery/docs text that cites the clip workaround.

### tree-14

**An embedded view without keyboard focus still reports HasKeyboardFocus=true for the focused tree item and the primary grid row** `src/Controls/DxUi.Accessibility.cpp:961` · #35, #53 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
When an embedded placement lacks keyboard focus, the publish clears focusedFragment and every record's controlHasFocus, but not treeHasFocus or gridHasFocus. The item and row providers answer HasKeyboardFocus from those fields, so an embedded view the app has moved focus away from still reports an item with keyboard focus. With #35's focused-item/selection split, clients rely on this property to find the current item … **Fix**: In the same loop at 958-963, also set `record.treeHasFocus = false; record.gridHasFocus = false;`. Better, have the item and row providers derive HasKeyboardFocus from `record-&gt;controlHasFocus` so one field carries the truth. Add an embedded test that publishes with hasKeyboardFocus=false and reads HasKeyboardFocus …

### tree-15

**UIA Select, AddToSelection and RemoveFromSelection change a disabled Tree and call its delegate** `src/Controls/DxUi.Accessibility.cpp:8774` · #35 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Tree items report IsEnabled from record-&gt;treeIsEnabled, but none of the tree-item action paths check the control's IsEnabled before acting: Select, AddToSelection, RemoveFromSelection, SetFocus and Expand. The disclosure Button path returns UIA_E_ELEMENTNOTENABLED (8900-8903). #35 added two more unchecked entry points that mutate the selection and invoke the delegate: RequestAddVisibleItemToSelection and RequestRe … **Fix**: In the TreeItem branches of Select, AddToSelection, RemoveFromSelection, Expand/Collapse and SetFocus (and the GridRow equivalents), return UIA_E_ELEMENTNOTENABLED when `! tree-&gt;IsEnabled()`, as the Value/RangeValue/Button paths do. Add a disabled-tree case to the multi-select pattern test asserting the HRESULT and …

### tree-16

**Embedded selection-event tests invalidate the view by hand before every publish, so they cannot catch a gesture or UIA path that forgets to invalidate** `Tests/Embedded/Embedded.Tests.EmbeddedUia.h:241` · #35, #53 · test-quality · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: file renamed by #66, content and line unchanged.  
An embedded view publishes accessibility changes only after a preparation, and UpdateAccessibility returns S_FALSE when the preparation counter is unchanged (Accessibility.cpp:9938-9940). Every step of the embedded tree and grid selection tests calls test.view.Controls().Invalidate() before Prepare and Update. If a click, key, UIA action or Tree method stopped calling Invalidate, the app would never republish and cli … **Fix**: For gesture and UIA steps (click, DispatchKey, provider actions), publish without the manual Invalidate and assert `Prepare(...) == S_OK` (not S_FALSE), which proves the input dirtied the view. Keep the manual invalidation only for the documented silent setters (SetSelectedItemId(s), RequestAdd/Remove called directly).

### tree-7

**Keyboard landing calls raw SetFocus(hwnd) and then touches the tree without a lifetime check (defect class #57 fixed elsewhere)** `src/Controls/DxUi.Tree.cpp:1380` · #57, #35 · lifetime · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#57 introduced FocusControlAndSurvive(host, control, takeNativeFocus) because native focus transfer runs WM_KILLFOCUS/WM_SETFOCUS handlers and the host's focus callbacks synchronously, and they may destroy the control. Tree's key landing lambda (reused by every movement key and Ctrl+Space since #35) still calls ::SetFocus directly and then calls the member Invalidate. It is now the only raw SetFocus in a control. **Fix**: Replace the raw call with `if (! FocusControlAndSurvive(host, *this, true)) return true;`, or take `GetLifetimeToken()` before `SetFocus` and return before `Invalidate(host)` if it expired. Add a key-forwarding test whose focus callback destroys the tree.

### tree-8

**Pending release-collapse overrides selection/focus changes made while the press is held, and moves focus back without OnTreeSelectionChanged** `src/Controls/DxUi.Tree.cpp:1270` · #35 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
A plain press on a row of a multi-selection (with reorder enabled) arms `_reorderCollapsesSelection`, and the release unconditionally collapses to the pressed row. Nothing clears that pending collapse when the selection or focus changes during the press: keys (only Escape is handled while armed), Ctrl+A, UIA Select/Add/Remove, SetSelectedItemIds/SetFocusedItemId. CollapseSelectionToItem then overwrites the newer stat … **Fix**: Clear `_reorderCollapsesSelection` in SelectVisibleIndex (on any call not made by the press itself), OnSelectAll, RequestRemoveVisibleItemFromSelection, SetSelectedItemIds and SetSelectedItemId. Simpler: in OnMouseUp, collapse only if `_selectedItemId == collapseId` and the selection is unchanged since the press (recor …

### tree-9

**RequestExpandedState reports success after the accessibility publish destroyed the tree (#60 lifetime fix not applied here)** `src/Controls/DxUi.Tree.cpp:698` · #60 · lifetime · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#60 made SelectVisibleIndex, RequestRemoveVisibleItemFromSelection, OnSelectAll, NotifyDataChanged and SetMultiSelectEnabled check the lifetime token after RefreshAccessibilitySnapshot, because UIA event delivery can dispatch a message that destroys the tree. The public RequestExpandedState, which UIA ExpandCollapse calls directly, still returns true after that publish with no check. It also returns false (mapped to … **Fix**: Take the token once and end with `RefreshAccessibilitySnapshot(); return ! selfLifetime.expired();`. In ExecuteExpandOnWindowThread, return UIA_E_ELEMENTNOTAVAILABLE when the tree did not survive (re-resolve, or use the survived() pattern used for grid rows). Extend the header comment to name RequestExpandedState.

### uia-lifetime-threading-13

**The 'seen window' heuristic uses one per-target GetFocus counter shared by all clients, while UIA's knowledge is per client and per HWND** `src/Controls/DxUi.Accessibility.cpp:773` · #32 · accessibility · plausible (batch: plausible/low)  
**At e5ebbb5**: Still valid: code unchanged.  
ReporterOfFocusMove and EndWindowHostFocusGainTurn decide whether UIA will call GetFocus for a gain from `focusResolutions`, a count kept on the WindowHostAccessibilityTarget. That count covers calls from every client process since this target was created. The behavior it models, "UI Automation asks a window for its focus only for the first focus event it answers", belongs to each client's UIA instance, and to the HW … **Fix**: Ask the developer whether Detach then Attach on the same HWND is supported. If it is, add a Narrator/UIA test for it. If the system's GetFocus is not re-issued after re-attach, keep the count per HWND across target recreation (for example, carry it in a property that outlives the target) or treat a freshly registered t …

### uia-lifetime-threading-14

**GetSelection creates one provider per selected grid row without bound, and window-host publishes let bad_alloc escape noexcept functions** `src/Controls/DxUi.Accessibility.cpp:7526` · #53, #29 · performance · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
`selectedGridRowIds` holds every selected row, and GetSelection allocates an AccessibilityProvider (plus vector slots and a SAFEARRAY entry) for each, inside a noexcept COM method that calls `reserve` and `push_back`. #53 now raises Selection_Invalidated for any change of more than 20 rows, such as Ctrl+A, which is exactly when clients re-query GetSelection. Separately, `RefreshWindowHostAccessibilitySnapshot` is noe … **Fix**: Wrap the publish in RefreshWindowHostAccessibilitySnapshot and both publish calls in RegisterWindowHostAccessibilityTarget in try/catch(const std::bad_alloc&). On failure, keep the previous snapshot, mirroring EmbeddedHost::UpdateAccessibility. Wrap the GetSelection body in try/catch(std::bad_alloc) and return E_OUTOFM …

### uia-lifetime-threading-15

**Actions whose callbacks destroyed the element still report success or NOTSUPPORTED instead of UIA_E_ELEMENTNOTAVAILABLE** `src/Controls/DxUi.Accessibility.cpp:8634` · #55, #57 · api-design · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
#55 and #57 specify that an element whose focus or selection callback rebuilt the controls reports itself gone. Select, AddToSelection and SetFocus do this, but Toggle, RemoveFromSelection and Expand do not. `Toggle::OnMnemonic` returns true even when FocusControlAndSurvive failed and the toggle never happened, so ExecuteToggle returns S_OK. `Grid::RequestRemoveRowSelection` returns true after its delegate destroyed … **Fix**: In ExecuteToggleOnWindowThread, return UIA_E_ELEMENTNOTENABLED when the toggle is disabled or hidden. Capture the toggle's lifetime token (or compare ResolveHost()/ResolveMutableControl() afterwards) and return UIA_E_ELEMENTNOTAVAILABLE when the toggle did not survive. Consider having Toggle::OnMnemonic return the surv …

### uia-lifetime-threading-16

**Parent navigation builds a separate, non-canonical root provider, and replaced canonical roots are never disconnected** `src/Controls/DxUi.Accessibility.cpp:9010` · #55 · architecture · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
The canonical-root rule ("Every root path QIs/AddRefs this object instead of constructing an equivalent provider") is bypassed by `CreateProviderFromNavigationTarget(Root)`, which builds a new AccessibilityProvider. This is what a control's NavigateDirection_Parent returns. Also, #55's AcquireCanonicalRootProvider drops a 'gone' cached root with `reset()` without UiaDisconnectProvider. Unregister disconnects only the … **Fix**: Change the Root case of CreateProviderFromNavigationTarget to `return CreateRootFragmentProvider();`. In AcquireCanonicalRootProvider, move the gone root into a local com_ptr under the lock and call UiaDisconnectProvider on it after the lock is released, as UnregisterWindowHostAccessibilityTarget does. Add a test that …

### uia-lifetime-threading-17

**UIA SetFocus on a native menu row moves the row focus but raises no focus-changed event** `src/Controls/DxUi.Accessibility.cpp:8528` · #56 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
For a menu-popup row, ExecuteSetFocusOnWindowThread calls SetFocusControl(control, false). The row's OnFocusChanged (MenuAccessibilityItem) first sets keyboardIndex to its own index. SetFocusControl's publish reports focusMoved, but AnnounceWindowHostFocus returns early for every native menu popup. The posted MenuPopupAccessibleFocus then runs SynchronizeMenuAccessibility, which raises RaiseWindowHostFocusChanged onl … **Fix**: In the nativeMenuRow branch of ExecuteSetFocusOnWindowThread, after a successful SetFocusControl and a survival check, call RaiseWindowHostFocusChanged(_hwnd, control) when host-&gt;GetFocusControl() == control and UIA clients are listening. Alternatively, have SynchronizeMenuAccessibility raise the event when the publ …

### uia-lifetime-threading-18

**Visual-line Move/MoveEndpointByUnit overflows int for large counts and moves in the wrong direction** `src/Controls/DxUi.Accessibility.cpp:3514` · pre-existing · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
TryMoveTextRangeSpanByVisualLine computes `static_cast&lt;int&gt;(currentIndex) + count` before clamping. When a client passes a large count, for example INT_MAX to mean 'to the end' (a common idiom for MoveEndpointByUnit), the addition overflows for any currentIndex &gt; 0. That is undefined behaviour, and in practice it wraps negative, so clamping selects line 0 and moved becomes -currentIndex. The logical-line and … **Fix**: Compute in 64 bits: `const int64_t requested = static_cast&lt;int64_t&gt;(currentIndex) + count; const int64_t target = std::clamp&lt;int64_t&gt;(requested, 0, static_cast&lt;int64_t&gt;(spans-&gt;size() - 1u)); const int moved = static_cast&lt;int&gt;(target - static_cast&lt;int64_t&gt;(currentIndex));`. Add INT_MAX a …

### uia-lifetime-threading-19

**Every text-range call copies the control's entire text inside noexcept COM methods** `src/Controls/DxUi.Accessibility.cpp:5722` · pre-existing · performance · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
AccessibilityTextRangeProvider::ResolveText returns std::wstring by value. GetText, Move, MoveEndpointByUnit, CompareEndpoints, ExpandToEnclosingUnit and GetBoundingRectangles each call it, usually under the process-wide accessibility mutex, and a Move(count==0) copies the text only to emit a perf value. The text lives in the snapshot or _textOverride, which the range already holds, so a std::wstring_view would cost … **Fix**: Change ResolveText to return std::wstring_view (an empty view when there is no record), backed by `_textOverride` or the pinned `_snapshot` record. Update the locals at the call sites from `const std::wstring text` to `const std::wstring_view text`; the helpers already take wstring_view. For the live-text branches at 5 …

### uia-lifetime-threading-20

**The posted-payload registry silently stops serving UIA actions for the 129th live DxUi window** `src/Support/PostedPayload.h:20` · #47 · resource-leak · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Posted and sent payloads (UIA actions marshalled to the window thread, menu accessible invoke and focus, and #47's cross-thread probes) are accepted only for windows in a fixed table of 128 per process. InitPostedPayloadWindow silently does nothing when the table is full, and ControlHost::Attach does not check the result. For any later window, PostMessagePayload returns false. DispatchAccessibilityUiActionToWindowThr … **Fix**: Replace the fixed `windows` array with a growable container under the existing mutex. Have InitPostedPayloadWindow return bool and catch bad_alloc to report failure. Have Attach and the menu popup creation trace a diagnostic on failure. Keep the 128 bound on in-flight entries. Add a test that registers more than 128 HW …

### uia-selection-semantics-11

**Grid RemoveFromSelection skips the 'element survived' checks that Select and AddToSelection perform, then touches the host** `src/Controls/DxUi.Accessibility.cpp:8866` · #57, #60 · lifetime · confirmed (single: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Grid::RequestRemoveRowSelection deliberately returns true after its delegate destroys the grid, with the comment 'the caller revalidates its element'. The RemoveFromSelection caller does not revalidate: it calls RefreshWindowHostAccessibilitySnapshot(_hwnd, host), then host-&gt;Invalidate(), and returns S_OK. Select and AddToSelection check `ResolveHost() == host` after the request. This is the defect class #57 and # … **Fix**: In ExecuteRemoveFromSelectionOnWindowThread's GridRow branch, add the revalidation after the request: ``` if (! grid || ! ResolveGridRowIndex(rowIndex) || ! grid-&gt;RequestRemoveRowSelection(rowIndex)) return UIA_E_NOTSUPPORTED; if (ResolveHost() != host) return UIA_E_ELEMENTNOTAVAILABLE; RefreshWindowHostAccessibilit …

### uia-selection-semantics-15

**Ordinary menu rows expose no submenu ExpandCollapse state and no accelerator key** `src/Controls/DxUi.Menu.cpp:3396` · #56 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Since #56 every row of every menu is a UIA element. A row with children (a submenu) gets only the MenuItem role and Invoke, which posts VK_RETURN and opens the submenu. It has no ExpandCollapse pattern or state, so clients cannot tell it opens a submenu or whether it is open. The row's acceleratorText (for example 'Ctrl+C') is never exposed: the name is only the decoded label (plus description) and the provider has n … **Fix**: Add an accessible accelerator string to Control (or reuse the MenuAccessibilityItem wrapper) and fill it from DecodeMenuItemText(item).acceleratorText. Snapshot it and return it for UIA_AcceleratorKeyPropertyId. For rows with `! item.children.empty()`, publish a disclosure state (for example through the same `controlDi …

### uia-selection-semantics-16

**Tree items always report IsOffscreen=false, including items scrolled out of the viewport that have no bounding rectangle** `src/Controls/DxUi.Accessibility.cpp:6119` · #35, #51 · accessibility · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
Tree point hits, and therefore BoundingRectangle, exist only for on-screen items, but IsOffscreen is hard-coded to false for every visible tree item. Grid rows compute gridRowOffscreen, and #56 added a scrolled-away check for menu rows. Tree items are left with an inconsistent pair: an empty BoundingRectangle together with IsOffscreen=false. **Fix**: In the TreeItem branch, return `VariantFromBool(! FindSnapshotFragmentBounds(*snapshot, AccessibilityFragmentKind::TreeItem, _path, item-&gt;visibleIndex, 0u, 0u).has_value())`, matching the menu-row branch. The alternative is a `treeItemOffscreen` flag computed during snapshot build, as grid rows do, which avoids the …

### uia-selection-semantics-17

**An allocation failure while publishing a window-host snapshot terminates the process: Refresh/Register are noexcept but the publish and the new selection diff allocate without a catch** `src/Controls/DxUi.Accessibility.cpp:9559` · #53, #56 · bug · confirmed (batch: confirmed/low)  
**At e5ebbb5**: Still valid: code unchanged.  
PublishWindowHostAccessibilitySnapshot is not noexcept. It runs make_shared, reserves and fills records, copies every tree item's text and every grid cell's text, and since #53 runs CollectSelectionChanges, which allocates vectors and sorted copies. RefreshWindowHostAccessibilitySnapshot and RegisterWindowHostAccessibilityTarget, both noexcept, call it with no try/catch, so std::bad_alloc becomes std::terminate. The … **Fix**: Wrap the publish calls in RefreshWindowHostAccessibilitySnapshot and RegisterWindowHostAccessibilityTarget in `try { ... } catch (const std::bad_alloc&) { /* keep the previously published snapshot; raise no events */ }`, and document the fallback next to the embedded one. Inside PublishWindowHostAccessibilitySnapshot, …

## Scoped testing review (#66, #67) (32)

Reviewed at `e5ebbb5` by two reviewers with a second look each; the same verification rules apply.

### scoped-engine-1

**PrePush counts native CI jobs that cannot be required checks as delegated coverage**  
`Tools/ScopedTesting.psm1:224` · #66, #67 · pr-coverage · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Get-ScopedPrCoverage hands every native scope of a profile to the forthcoming PR. The CI jobs that would run those scopes (the `native` matrix) are deliberately not required checks. The required `validation` job depends only on `windows-tooling`, and no aggregate job reports the matrix result. The contract says the PR 'must pass its required checks', but none of those checks covers native coverage. Delegated native obligations are therefore enforced by nothing.
- **Failure**: A clean branch changes src/Controls/DxUi.Grid.cpp. The developer runs `Test-Changes.ps1 -Mode PrePush -Configuration 'ASan Debug'`: all 20 scopes print DEFERRED_CI, nothing runs locally and the script exits 0. In the PR, the ASan native job fails, hits its 40-minute timeout or is cancelled. `validation` and `paired-benchmark (pull request)` are green, so the merge button or auto-merge proceeds, and the use-after-free reaches main with no passing ASan run anywhere.
- **Fix**: 1. **Add an aggregate job to ci.yml** with a fixed name, for example `native-result`, defined as `needs: [native-scope, native]` with `if: always()`. - It fails unless `needs.native.result == 'success'`, or `needs.native.result == 'skipped' && needs.native-scope.outputs.native == 'false'` (a docs-only PR). - It also fails when `native` was cancelled. - Developer action needed: make `native-result` a required check. 2. **Make delegation depend on that check.** In test-scopes.json, add a `prRequir …
- **Test**: Two tests in Tools/tests/Test-ScopedTesting.ps1 would fail before the fix: 1. **Workflow test.** Parse ci.yml and assert three things: - A job exists whose `needs` includes `native` and whose `if` is `always()`. - Its step fails for native results `failure`, `cancelled`, and `skipped` together with …

### scoped-engine-10

**PR delegation binds only ci.yml bytes; scripts that decide what CI actually runs are unbound**  
`Tools/ScopedTesting.psm1:233` · #66, #67 · pr-coverage · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Delegation is gated on `(Get-ScopedDigest $local) -cne $Manifest.prWorkflowDigest` over .github/workflows/ci.yml only. CI coverage is actually decided by scripts that ci.yml invokes: test.ps1 (`-Full -SkipTooling` body, line 39-56 suite handling), Tools/Get-NativeScope.ps1 and BenchmarkGate's Resolve-PullRequestPair/Get-PullRequestChangedPaths (whether the native matrix runs at all), and Tools/InteractiveRun.psm1. Test-ScopedTesting.ps1:193-199 only checks that the workflow text contains `test.ps1 -Full -SkipTooling` and that test.ps1's `$Suites` *default value text* names each scope. The WIP plan says "a ... modified runner invalidates equivalence", but no runner bytes are part of the delegation identity. The local side imports only Native …
- **Failure**: A PR adds a CI-only quarantine to test.ps1's body, e.g. `if ($env:GITHUB_ACTIONS -eq 'true') { $Suites = $Suites | Where-Object { $_ -ne 'Animation' } }`. Alternatively, Get-NativeScope.ps1 starts writing native=false for some event. ci.yml is unchanged, so the digest matches and Test-ScopedTesting still passes (the param default is intact). `Test-Changes -Mode PrePush` delegates Animation for every profile and runs nothing locally. CI never runs Animation, the PR goes green, and full coverage was claimed.
- **Fix**: Extend the reviewed identity from ci.yml alone to the whole CI coverage contract: 1. Add a `prContractFiles` list to Tests/test-scopes.json. It should cover .github/workflows/ci.yml, test.ps1, Tools/Get-NativeScope.ps1, Tools/NativeScope.psm1, Tools/BenchmarkGate.psm1, Tools/InteractiveRun.psm1, Tools/SuiteFailure.psm1, test-consumer.ps1 and vcpkg-install.ps1, plus anything else that test.ps1 dot-sources or imports. 2. In Get-ScopedPrCoverage, compute `prWorkflowDigest` over the sorted path + CR …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 that runs in a temporary committed clone/fixture repository: 1. Set up a clean tree where ci.yml matches the digest and `gh` is stubbed to return `state=active`. 2. Commit a body-only change to test.ps1, such as an inert comment or a GITHUB_ACTIONS-on …

### scoped-engine-12

**Environment identity omits the DXUI_* switches that native tests and the documented mutation workflow read**  
`Tools/ScopedTesting.psm1:149` · #66, #67 · reuse-identity · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Get-ScopedEnvironmentIdentity hashes MachineName, OS, PSVersion, ASAN_OPTIONS, CI, GITHUB_ACTIONS, PROCESSOR_IDENTIFIER, PATH, VCToolsVersion and WindowsSDKVersion. Current native tests change behavior on other variables. Tests/Controls/DxUi.Tests.Rendering.cpp:1276 runs `TestThroughputGraphHueChurnPerformanceScenario` (with Require assertions) only when DXUI_GRAPH_PERF=1. DxUi.Tests.Animation.cpp:15 and DxUi.Tests.WindowHost.cpp:92 write perf JSONL to DXUI_PERF_JSONL_PATH. The repository's documented falsification procedure (Measurements/GridTextOverflow/2026-09-30/verification/README.md:71,156) selects compiled-in mutants at runtime with DXUI_MUTANT / DXUI_ONLY_FRENCH. Because these variables are not in the identity, a receipt from a run …
- **Failure**: A developer runs `Test-Changes -Scopes Rendering` once (pass, receipt written), then sets DXUI_GRAPH_PERF=1 to exercise the opt-in graph scenario and runs it again. The output is `REUSED Rendering (identical successful local evidence)`: the gated scenario never executes, and a regression it would catch is reported as passed. Likewise, in a mutation campaign that loops DXUI_MUTANT over switch-compiled mutants, every mutant after the first clean run is REUSED as PASSED, so the campaign wrongly concludes that the suite kills no mutant. DXUI_PERF_JSONL_PATH runs produce no data while reporting suc …
- **Fix**: In Get-ScopedEnvironmentIdentity, add every environment variable whose name starts with DXUI_, sorted by name, as `name=value`. For example: `$values += (Get-ChildItem Env: | Where-Object Name -like 'DXUI_*' | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join "`n"`. A conservative alternative is to refuse reuse (treat it like -Force) whenever any DXUI_* variable is set. Also add a tooling check that greps Tests/ and src/ for GetEnvironmentVariableW/getenv/_wgetenv literals an …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1: 1. Save `$env:DXUI_GRAPH_PERF`. 2. Clear it and compute `$a = Get-ScopedEnvironmentIdentity`. 3. Set `$env:DXUI_GRAPH_PERF = '1'` and compute `$b`. 4. Assert `$a -cne $b`, then restore the variable in `finally`. 5. Do the same for DXUI_PERF_JSONL_PAT …

### scoped-engine-13

**PrePush delegates InteractiveLease to CI, where its confirmation/warning proofs are allowed to skip**  
`Tools/tests/Test-InteractiveLease.ps1:63` · #66, #67 · pr-coverage · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: InteractiveLease is a native scope listed in every prCoverage profile (test-scopes.json:348 etc.), so PrePush treats CI as equivalent coverage. Locally, test.ps1 runs Test-InteractiveLease.ps1 for that scope, and a `SKIPPED:` from the lease self-test fails unless `$onCi`. On CI the skip is accepted: `Assert-True $onCi "a private desktop could be made here..."`. Per its own comment, a hosted runner's session cannot make a private desktop. The CI execution therefore omits the confirmation-dialog and warning-banner proofs (lines 65-70) that the local execution requires. The contract says to delegate only "the identical scopes/configuration that the enabled PR workflow actually runs" and that environment-reduced coverage must not count as full.
- **Failure**: A PR changes Tests/InteractiveLease/InteractiveLease.Tests.Confirmation.h or InteractiveLease.Tests.WarningBanner.h (or the lease logic behind them). The tree is clean and the change is native, so `Test-Changes -Mode PrePush` delegates InteractiveLease for all six profiles and runs nothing locally. On the hosted runner the self-test prints SKIPPED for the dialog and warning, and the CI accepts it. The PR goes green with a broken confirmation dialog that no execution exercised. That dialog guards the person's desktop for -Interactive runs.
- **Fix**: Minimal fix: stop counting a skip-tolerant CI run as equivalent coverage. Add a manifest flag on the scope, for example `"ciReduced": true` (or `"delegable": false`), to InteractiveLease in Tests/test-scopes.json. Then make Get-ScopedPrCandidateScopes drop flagged scopes, for example `$covered = @($covered | Where-Object { $_ -notin @($Manifest.scopes | Where-Object { $_.PSObject.Properties['ciReduced'] -and $_.ciReduced } | ForEach-Object name) })`, so PrePush keeps the lease proofs local. Upda …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1. Load the real manifest and call Get-ScopedPrCandidateScopes with ChangedPaths @('Tests/InteractiveLease/InteractiveLease.Tests.Confirmation.h') for x64 Debug. Assert that 'InteractiveLease' is not in the result. At HEAD it is returned, so the case fa …

### scoped-engine-2

**Tools/tests/** selects only Tooling, but three changed fixtures run only on test.ps1's native path**  
`Tests/test-scopes.json:242` · #66, #67 · selection · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Every path under Tools/tests selects only the Tooling scope. Tooling runs `validate.ps1`, which runs Invoke-ToolingTests, plus `Test-AsanRuntime.ps1`. Test-TestFilter.ps1, Test-TestWatchdog.ps1 and Test-InteractiveLease.ps1 are in neither. test.ps1 invokes them only when control suites or InteractiveLease are among its -Suites. Their shared TestSupport.psm1 is routed the same way. Editing any of these selects a scope that never executes the edited file.
- **Failure**: An edit to Tools/tests/Test-TestWatchdog.ps1 (or TestSupport.psm1) breaks the watchdog check. `./Test-Changes.ps1` (or a plain `./test.ps1`) explains 'Tools/tests/Test-TestWatchdog.ps1: tooling regression tests =&gt; Tooling'. It runs validate.ps1 and Test-AsanRuntime.ps1, prints 'SELECTED_PASSED' and exits 0. The changed fixture never ran. Under PrePush with a clean tree it is delegated too, so only CI ever runs it.
- **Fix**: In test-scopes.json, add rules for these files. Because matching is a union, the existing Tools/tests/** → Tooling rule still applies, and rule order does not matter: - `Tools/tests/Test-TestFilter.ps1` and `Tools/tests/Test-TestWatchdog.ps1` → `["Control"]`. Any non-Foundation/Embedded native scope makes test.ps1 run both. - `Tools/tests/Test-InteractiveLease.ps1` → `["InteractiveLease"]`. - `Tools/tests/TestSupport.psm1` → `["Control", "InteractiveLease"]`. Alternatively, have Test-Changes cal …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 that runs over every `Tools/tests/*.ps1` and `*.psm1` file: 1. Call Get-ScopedTestPlan with that single path. 2. Assert that the file is executed by the selected scopes. Either its name appears in Invoke-ToolingTests.ps1's list or among the toolingCom …

### scoped-engine-3

**Files that validate.ps1 reads never select Tooling (gallery, Measurements, test sources, src rules)**  
`Tools/ScopedTesting.psm1:101` · #66, #67 · selection · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Non-Markdown paths under docs/, Measurements/ and Changes/ select no scope at all. Every explicit rule (suite test sources, Tests/Foundation/**, Tests/Embedded/**, src/Controls/DxUi.{Grid,Tree,ComboBox}.*) omits Tooling. The validators and tooling tests nevertheless read exactly these files: gallery PNG hashes against generation.json, Measurements receipts and their READMEs, retained test-case definitions in Tests/Controls/*.cpp, include and namespace scans of src/Tests, and BenchmarkGate's include walk from Tests/Embedded. The plan says 'Documentation edits select only independent validators', but the code selects nothing.
- **Failure**: (a) A Grid visual fix edits src/Controls/DxUi.Grid.cpp and runs `gallery.ps1 -PublishDocs`, as AGENTS requires, changing docs/gallery/*.png and generation.json. Affected selects the 11 Grid-rule scopes but not Tooling, so a stale or mismatched gallery hash or control count goes unreported. (b) Renaming one of the 62 retained `void Test...()` cases bound to Tests/Controls/DxUi.Tests.Grid.cpp selects only Grid, so validate-test-port's 'Retained case is missing' never runs. (c) Editing Measurements/X/run.json (wrong workloadOwner, no README) prints 'No local test execution is required by this pla …
- **Fix**: In Get-ScopedTestPlan, add the non-native (validator) scopes for every changed path outside .build/, not only for `.md` paths. Tooling's identity already hashes every tracked file (`Get-ScopedSourceIdentity` without -CompiledOnly), so an unchanged tree would still reuse its receipt. The minimal change: after the `\.md$` branch, add `$tooling = @($Manifest.scopes | Where-Object { -not $_.native } | ForEach-Object name)` to the selected set for every path. In particular, add it in the docs|Measure …
- **Test**: Add a Run-Case to Tools/tests/Test-ScopedTesting.ps1 that loops over 'docs/gallery/x.png', 'docs/gallery/generation.json', 'Measurements/X/run.json', 'Tests/Controls/DxUi.Tests.Grid.cpp', 'Tests/Foundation/x.cpp' and 'src/Controls/DxUi.Grid.cpp', calls `Get-ScopedTestPlan $manifest @($path)`, and as …

### scoped-engine-4

**Receipt environment identity omits GPU/driver, D2D/UIA/TSF runtime DLLs, OS build revision, fonts and DPI**  
`Tools/ScopedTesting.psm1:147` · #66, #67 · reuse-identity · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Native receipts are reused when the environment digest matches. That digest hashes only d3d10warp.dll, d3d11.dll and dwrite.dll, plus RuntimeInformation.OSDescription, which carries major.minor.build but no update revision (UBR). WindowHost creates a hardware D3D11 device first and requests the Debug SDK layer. The control suites exercise Direct2D, UI Automation and TSF. None of the adapter, driver, d2d1/dxgi/UIAutomationCore/msctf/user32, installed fonts, display scale, D3D SDK layers or DXUI_* environment variables are part of the identity. The contract claims reuse 'only for equal ... environment'.
- **Failure**: A Patch Tuesday cumulative update changes UIAutomationCore.dll and d2d1.dll but not the three hashed DLLs (OSDescription stays 'Microsoft Windows 10.0.26200'), or the GPU driver updates, or the developer docks to a 150% monitor. `Test-Changes.ps1 -Mode Full` then prints REUSED for Accessibility, Rendering, TextField and the rest, followed by FULL_NONINTERACTIVE_PASSED, without running anything on the new stack. Installing or removing the Graphics Tools optional feature also toggles Debug-layer validation unnoticed.
- **Fix**: Extend Get-ScopedEnvironmentIdentity with these inputs: - UBR and DisplayVersion from HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion - hashes of d2d1.dll, dxgi.dll, d3d11_3SDKLayers.dll (or a present/absent marker), UIAutomationCore.dll, msctf.dll, user32.dll and windowscodecs.dll - the description, vendor ID, device ID and driver version of each display adapter, taken from Win32_VideoController DriverVersion/PNPDeviceID or from the display class registry keys - the system DPI (HKCU:\Control …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1: 1. Compute Get-ScopedEnvironmentIdentity. 2. Set $env:DXUI_GRAPH_PERF='1' and compute it again. Assert the two digests differ. Restore the variable in a finally block. This case fails today because DXUI_* variables are not hashed. Add a second case: …

### scoped-engine-5

**PrePush delegates to a 'forthcoming PR' from main or a detached HEAD, where no PR will run**  
`Tools/ScopedTesting.psm1:231` · #66, #67 · pr-coverage · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Delegation checks HEAD stability, a clean tree, the workflow digest and that the workflow is active. It never checks that the candidate is on a non-default branch that will become a pull request. On local main, or on a detached HEAD, every PR-covered scope is still deferred and nothing runs locally. For a direct push to main, CI's push run validates only after the code has landed.
- **Failure**: A developer has two commits on local `main`, ahead of origin/main, touching src/. `Test-Changes.ps1 -Mode PrePush -Configuration Release` prints 'DEFERRED_CI (forthcoming PR, pending success): Foundation, ..., Tooling' and 'No local test execution is required by this plan.' The developer then runs `git push origin main`, which admin bypass or absent protection allows. The commits land on main tested by nothing, and the push CI reports the breakage only afterwards.
- **Fix**: In Get-ScopedPrCoverage, right after `$candidate` is captured, keep obligations local when HEAD cannot become a PR head: $branch = (Invoke-ScopedGit $Root @('symbolic-ref','-q','--short','HEAD')).Trim()   # catch/empty =&gt; detached if (-not $branch -or $branch -ceq $Manifest.defaultBranch) { return @() } if ((Invoke-ScopedGit $Root @('rev-list','--count',('origin/'+$Manifest.defaultBranch+'..HEAD'))).Trim() -eq '0') { return @() } Invoke-ScopedGit may throw on a non-zero exit. The existing Run …
- **Test**: In Test-ScopedTesting.ps1's delegation case, keep the gh stub returning `{"state":"active"}` and add three cases: (a) After `git init` plus the baseline commit, run `git checkout -q -B main`, make one extra commit, leave origin/main at the baseline, and assert `@(Get-ScopedPrCoverage $delegate $cove …

### scoped-engine-6

**Plain test.ps1 now runs affected-only (possibly nothing), contradicting AGENTS.md and validate.ps1 claims**  
`test.ps1:39` · #66, #67 · docs-contract · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: A plain `test.ps1` call (only -Platform/-Configuration/-SkipBuild) now delegates to Test-Changes in Affected mode. That mode selects only from repository path changes, and exits 0 with no execution when nothing changed. Toolchain, OS or driver changes never select anything. AGENTS.md (authoritative) still says every test.ps1 run reports complex-UI FPS/memory, that test.ps1 runs the tooling tests, and that code changes require test.ps1 in x64 Debug, Release and ASan Debug. validate.ps1's header repeats the tooling claim.
- **Failure**: An agent follows AGENTS.md after a Visual Studio toolset update. On a clean branch whose only change is in docs/, or after the work was already committed and merged locally, it runs `./test.ps1 -Configuration 'ASan Debug'`. The output is 'No local test execution is required by this plan.' with exit 0: no build, no ASan probe, no FPS/memory report, no tooling tests. The agent records the 'test.ps1 in ASan Debug' requirement as satisfied.
- **Fix**: Docs: - Change AGENTS.md:44-45, AGENTS.md:59-61, validate.ps1:8, Specs/Testing/Testing_Validation.md:46-48 and README.md:15-16 to say `test.ps1 -Full` (or `Test-Changes.ps1 -Mode Full|PrePush`) wherever they mean a complete qualification, the benchmark or the tooling run. - State that a plain `test.ps1` call is affected-only iteration. Code: - When test.ps1 delegates, print a line that cannot be missed, for example 'AFFECTED ITERATION ONLY - not a configuration qualification; use -Full'. - Chang …
- **Test**: Add a docs-contract check to Tools/tests (or validate-specs.ps1). It should fail when AGENTS.md, Testing_Validation.md, README.md or validate.ps1 say plain `test.ps1` "runs the tooling tests", "every test.ps1 run/invocation" or qualifies Debug/Release/ASan without `-Full` (the check fails today on A …

### scoped-engine-7

**Workflow digest checks the candidate's ci.yml, but pull_request runs use the merge ref's workflow**  
`Tools/ScopedTesting.psm1:233` · #66, #67 · pr-coverage · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Delegation compares the candidate's own ci.yml with the candidate's own manifest digest. For pull_request events, GitHub runs the workflow file of the merge commit, so a ci.yml change on main after the branch point is what actually runs. That change is never seen locally (no fetch, no comparison with origin/main). The plan states that a base/merge-tree change invalidates equivalence, and Testing_Validation.md:582 states that the PR 'executes its candidate workflow'. Neither holds when the base changed ci.yml.
- **Failure**: A long-lived branch forked before main moves ASan PR coverage to a scheduled job (main updates its own test-scopes.json in the same commit). On the branch, the local ci.yml and manifest digest still match, so `-Mode PrePush -Configuration 'ASan Debug'` defers all 20 scopes. The PR's merge-ref workflow is main's new one, which has no ASan PR job, so ASan never runs for that change before merge.
- **Fix**: In Get-ScopedPrCoverage, after the local digest check and before returning, also confirm that the remote default branch's ci.yml is the same as the candidate's. One way is `git show origin/&lt;defaultBranch&gt;:.github/workflows/ci.yml`, normalised to LF and hashed with Get-ScopedDigest. A better way, since gh is already required and origin/main is never fetched, is to read the live default branch with `gh api repos/&lt;repo&gt;/contents/.github/workflows/ci.yml?ref=&lt;defaultBranch&gt;`. If th …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 next to lines 211-213, using the same sandbox repository fixture. Commit the reviewed ci.yml and manifest on a feature branch. Then create an `origin/main` ref (a local ref, or stub the gh contents call) whose ci.yml has different bytes, for example w …

### scoped-engine-8

**Git output is decoded with the console code page; non-ASCII tracked files silently leave the evidence identity**  
`Tools/ScopedTesting.psm1:7` · #66, #67 · reuse-identity · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Invoke-ScopedGit redirects stdout without setting StandardOutputEncoding while passing `core.quotepath=false`, so git emits raw UTF-8 paths. On Windows, .NET decodes redirected output with the console output code page, typically OEM 437/850 unless the session is configured for UTF-8. The resulting mojibake path fails Test-Path, and Get-ScopedSourceIdentity treats it as deleted ('Removed files simply leave the closure'). Assert-ScopedTestNames also skips it. This is latent: the repository has no non-ASCII paths today, but it is a Unicode-focused library whose test data could add some.
- **Failure**: A future fixture Tests/Controls/Data/Größe.txt, read by a TextField test, is listed as 'Gr├Âße.txt' in a default-code-page console. It is excluded from both the CompiledOnly and full identities. Editing it leaves every receipt valid, so Test-Changes prints REUSED and never runs the test on the new data. A declared test source with such a name would also escape the naming/inventory check.
- **Fix**: In Invoke-ScopedGit, add `$start.StandardOutputEncoding = [Text.UTF8Encoding]::new($false)` and `$start.StandardErrorEncoding = [Text.UTF8Encoding]::new($false)`. This matches Validation.psm1's Get-GitPathList. You could also share one helper between the two modules. To be safe even if decoding goes wrong again, Get-ScopedSourceIdentity should stop dropping listed paths that don't exist. Add an explicit row instead, such as `"$path`0&lt;missing&gt;"`, so an unresolvable name changes the identity …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1: 1. In the fixture repository, create and `git add` a file with a non-ASCII name, for example `Tests/Data/Größe.txt`. 2. Run Get-ScopedSourceIdentity with and without -CompiledOnly. 3. Change the file's content and assert that both identities change. …

### scoped-engine-9

**ComboBox fan-out omits NewControls, which tests TagPicker built on an editable ComboBox**  
`Tests/test-scopes.json:280` · #66, #67 · selection · confirmed (trace: confirmed/medium; refute: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: The `src/Controls/DxUi.ComboBox.*` rule (lines 279-293) selects ComboBox, Control, Rendering, Accessibility, Embedded, WindowHost, Theme, TextField and Animation. It leaves out NewControls. NewControls exercises a ComboBox indirectly: TagPicker's constructor (src/Controls/DxUi.Controls.cpp:5307-5335) calls `_combo = AddChild&lt;ComboBox&gt;(); _combo-&gt;SetEditable(true); _combo-&gt;SetAutoOpenOnTextInput(true); ...`. `TagPicker::SetInputText` (5381) forwards to `_combo-&gt;SetText`, `GetInputText` (5393) returns `_combo-&gt;GetText()`, and `CommitInput` (5448) reads that text. `TestTagPickerAddsRemovesAndDedupesOptions` (Tests/Controls/DxUi.Tests.NewControls.cpp:2108-2134) depends on that path: `picker.SetInputText(L"file"); Require(picke …
- **Failure**: A developer changes ComboBox::SetText/GetText for editable combos, or how SetAutoOpenOnTextInput consumes text. Test-Changes (or plain test.ps1, which now delegates to it) selects the nine ComboBox consumers, they pass, and it prints SELECTED_PASSED. The NewControls TagPicker test that the change breaks never runs locally. Only the later Full/CI run reveals it, after the developer has been told the affected scopes passed.
- **Fix**: Add "NewControls" to the scopes of the `src/Controls/DxUi.ComboBox.*` rule in Tests/test-scopes.json. Extend the rule's reason to name the TagPicker composition, so a later reviewer can see the edge is deliberate. Optionally, have the manifest audit look for `AddChild&lt;T&gt;` / `make_unique&lt;T&gt;` of a rule-owned control class in other src files and require the owning test scopes, so future compositions are not missed.
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 that calls `Get-ScopedTestPlan -Manifest (Read-ScopedTestManifest $root) -ChangedPaths @('src/Controls/DxUi.ComboBox.cpp')` and asserts that `$plan.scopes -contains 'NewControls'`. It fails against the HEAD manifest and passes once NewControls is adde …

### scoped-integration-1

**Tools/tests/** maps the build-dependent runner checks to a Tooling scope that never runs them**  
`Tests/test-scopes.json:242` · #66, #67 · mis-scoped-selection · confirmed (trace: confirmed/high; refute: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: The rule `"pattern": "Tools/tests/**", "scopes": ["Tooling"]` sends every change under Tools/tests to the Tooling scope only. Tooling runs `toolingCommands` = `validate.ps1` and `Tools/tests/Test-AsanRuntime.ps1` (test-scopes.json:322-325; Test-Changes.ps1:110-114). validate.ps1 runs the five validators and Invoke-ToolingTests.ps1, whose list (Invoke-ToolingTests.ps1:6) does not include Test-TestFilter.ps1, Test-TestWatchdog.ps1 or Test-InteractiveLease.ps1. Those three need a built executable, so only test.ps1's native path runs them, and only when a control or InteractiveLease suite is in `-Suites` (test.ps1:84-92). They and the TestSupport.psm1 they import are covered by the same Tools/tests/** rule. Before #66 a plain local test.ps1 alw …
- **Failure**: A developer edits Tools/tests/Test-TestWatchdog.ps1 (or TestSupport.psm1) and breaks it, then runs `./test.ps1` (which hands off to Test-Changes Affected). The plan is `Tools/tests/Test-TestWatchdog.ps1: tooling regression tests =&gt; Tooling`. validate.ps1 and Test-AsanRuntime pass, and a Tooling receipt is written (Test-Changes.ps1:127-129). The output is `SELECTED_PASSED` and the exit code is 0. The changed script never ran. Later runs print `REUSED Tooling` until some src change happens to select a control scope. Only the native CI jobs run it, and those are not required checks.
- **Fix**: In Tests/test-scopes.json, add specific rules next to the existing `Tools/tests/**` rule. Rules combine as a union, so these add native scopes on top of Tooling: - `Tools/tests/Test-TestFilter.ps1` and `Tools/tests/Test-TestWatchdog.ps1` -&gt; ["Tooling", "Grid"]. Any control scope triggers test.ps1:84-87. - `Tools/tests/Test-InteractiveLease.ps1` -&gt; ["Tooling", "InteractiveLease"]. This also triggers Filter and Watchdog. - `Tools/tests/TestSupport.psm1` -&gt; ["Tooling", "Grid", "Interactive …
- **Test**: In Tools/tests/Test-ScopedTesting.ps1, add plan assertions against the real manifest. For each of `Tools/tests/Test-TestFilter.ps1`, `Tools/tests/Test-TestWatchdog.ps1`, `Tools/tests/Test-InteractiveLease.ps1` and `Tools/tests/TestSupport.psm1`, call `Get-ScopedTestPlan -Manifest (Read-ScopedTestMan …

### scoped-integration-2

**Plain test.ps1 in the README quick start builds and runs nothing on a clean default-branch checkout, and exits 0**  
`test.ps1:39` · #66, #67 · behavior-regression · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: When only Platform, Configuration or SkipBuild are bound, test.ps1 now runs `Test-Changes.ps1` in Affected mode and then `exit $LASTEXITCODE` (test.ps1:39-43). Test-Changes compares against `merge-base origin/main HEAD`. With no committed, staged, unstaged or untracked change (vcpkg-install and builds write only under the ignored .build/), the plan is empty. It prints `No local test execution is required by this plan.` and `exit 0` (Test-Changes.ps1:69) before the build step. README.md:14-15 and docs/getting-started.md:15 still present `.\test.ps1 -Configuration Debug -Platform x64` as the build-and-test step and follow it with `.\gallery.ps1 -SkipBuild` and `.\.build\x64\Debug\DxUi.EmbeddedControls.exe`.
- **Failure**: Fresh clone, following README.md:13-20: `vcpkg-install.ps1 -Platform All` writes only under .build. `test.ps1 -Configuration Debug -Platform x64` then sees zero changed paths, builds nothing, runs no suite or benchmark and exits 0. The ARM64 builds run. `gallery.ps1 -SkipBuild` then fails because `.build\x64\Debug\DxUi.ControlTests.exe` does not exist. The same happens after `git pull` on main: an agent following AGENTS.md:61 ("Code changes require test.ps1 in x64 Debug, Release and ASan Debug") gets three exit-0 runs that executed nothing.
- **Fix**: Minimal docs fix: in README.md:15-16, replace the two plain test.ps1 calls with `.\build.ps1 -Configuration Debug -Platform x64` plus `.\test.ps1 -Full -Configuration Debug -Platform x64`, and the same for Release. Update Specs/Testing/Testing_Validation.md:46 to say this behavior requires -Full or -Suites. Code hardening, in Test-Changes.ps1:69: when the plan is empty, print an explicit `NOTHING_EXECUTED; repository NOT_EVALUATED` label instead of a plain success line. When the profile has no b …
- **Test**: Add a tooling test to Tools/tests/Test-ScopedTesting.ps1. In a temporary Git fixture repository, create a clean commit equal to origin/main with no `.build` profile. Invoke the Test-Changes.ps1 entry logic, with build.ps1 stubbed, in Affected mode. Assert that it either invokes the build stub or exi …

### scoped-integration-4

**PrePush hands all native obligations to native jobs that no required check can enforce**  
`Tools/ScopedTesting.psm1:224` · #66, #67 · ci-coverage · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: This is a new interaction with the plan's existing item "No check can require the native matrix". With a clean committed candidate and an active workflow, Get-ScopedPrCoverage returns every prCoverage scope for the profile, all 19 native scopes plus Tooling, whenever NativeScope says native. Test-Changes then removes them from local work (Test-Changes.ps1:65-69). For native scopes the only CI executor is the conditional `native` matrix. ci.yml:59-63 states that no required check may name it, and the windows-tooling fold-in reaches only `validation`. Testing_Validation.md:576 rests the delegation on "the PR must pass its required checks", which cannot include a native job. When everything is handed off, Test-Changes also exits at line 69 wit …
- **Failure**: A developer on a clean branch runs `./Test-Changes.ps1 -Mode PrePush -Configuration Debug`. Nothing runs locally. The last line is `No local test execution is required by this plan.` and the exit code is 0. They push and open the PR with auto-merge enabled (plan Q21). The x64 Debug native job fails a Grid test. `validation`, which now includes windows-tooling, and `paired-benchmark (pull request)` are green, so the PR merges with no passing native run anywhere, local or CI.
- **Fix**: Smallest safe fix: in Get-ScopedPrCandidateScopes (ScopedTesting.psm1:213-221), stop returning native scopes until a fixed-name aggregate check exists. For example, add a manifest field such as `prRequiredNativeCheck` and drop every scope with `native: true` from `$covered` unless that field is set, so only Tooling, which `validation` enforces, is delegated. Full fix: add a `native-result` job to ci.yml with `needs: [native-scope, native]` and `if: always()`. It fails unless `needs.native.result …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 that calls Get-ScopedPrCandidateScopes with a code path (`src/Controls/DxUi.Grid.cpp`) and a manifest that has no required native aggregate check declared. Assert that no returned scope has `native: true` and that only `Tooling` is returned. Today it …

### scoped-integration-5

**A pass with capability skips becomes reusable evidence: the identity ignores desktop and session state**  
`Test-Changes.ps1:117` · #66, #67 · stale-reuse · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: This is a new interaction with the plan's item "CI counts capability skips as a pass". Test-Changes judges the native run only by test.ps1's exit code. test.ps1 prints `PASS &lt;suite&gt; (N capability skips recorded)` and exits 0 when tests skipped (test.ps1:199). Test-Changes then writes a reusable PASSED receipt per scope (Test-Changes.ps1:127-129). The identity it reuses on (Get-ScopedRunIdentity plus Get-ScopedEnvironmentIdentity, ScopedTesting.psm1:147-182) covers source, binaries, machine, OS, PATH and graphics DLLs. It does not cover desktop or session state or the skip set, and noninteractive suites skip according to that state. Example: NewControls runs RunMenuDescriptionTests (DxUi.Tests.NewControls.cpp:10), whose cursor test doe …
- **Failure**: A developer runs `./test.ps1` while another window covers the test windows, or while connected over a disconnected or locked RDP session. NewControls and Accessibility pass with several capability skips and get PASSED receipts. With an unlocked desktop and identical code, the next run prints `REUSED NewControls` and `REUSED Accessibility`. The skipped tests, which would now execute and might fail, are never run. The reuse contract ("equal ... environment") is not met.
- **Fix**: In Test-Changes.ps1, after test.ps1 returns 0, read each runtime suite's report: Join-Path $root ".build/reports/$name-$Platform-$Configuration.json" (the same name test.ps1 writes when it is not filtered or interactive). Call Write-ScopedReceipt only when that report exists, its exitCode is 0 and @($r.skips).Count -eq 0. Otherwise print "PASSED_WITH_SKIPS $name (not reusable)" and leave no receipt, so the next run executes the suite again. Rejecting reuse after any skip is simpler and safer tha …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1 that runs Test-Changes against a fixture root. Stub test.ps1 so it exits 0 and writes `.build/reports/&lt;Scope&gt;-x64-Debug.json` with `skips=@('SKIPPED: another window covers ...')`. Assert that no scoped receipt exists under `.build/reports/scoped …

### scoped-integration-6

**The FULL_NONINTERACTIVE_PASSED label and `test.ps1 -Full` omit CI-only noninteractive work, so a change to ConsumerModules is never compiled**  
`Test-Changes.ps1:131` · #66, #67 · coverage-overclaim · confirmed (single: confirmed/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: An unmapped path widens the plan to every manifest scope (ScopedTesting.psm1:103-106). With nothing handed off, the result is labeled `FULL_NONINTERACTIVE_PASSED`. test.ps1's `-Full` is documented as "Runs the complete noninteractive gate" (test.ps1:20-21). Neither runs the noninteractive work that CI treats as part of the gate: `test-consumer.ps1` (both variants, ci.yml:113-119), the `MenuTextLayoutResources` fixture (ci.yml:120-124; it does not take focus) and `gallery.ps1`, which runs the Gallery suite and the EmbeddedControls sample (ci.yml:127-128). `Tests/ConsumerModules/ConsumerModules.Tests.*.cpp` is in the active native test inventory (native-test-files.json:2-3) but is compiled only by test-consumer.ps1. The fallback marked conser …
- **Failure**: A developer edits Tests/ConsumerModules/ConsumerModules.Tests.NativeModule.cpp, or Build/DxUi.Consumer.props, or test-consumer.ps1, and introduces a compile error. `./test.ps1` (handed off) prints `unmapped input; conservative full fallback`, runs the 19 suites and validate.ps1, and prints `FULL_NONINTERACTIVE_PASSED` in green. The broken consumer module is only discovered by the native CI jobs, which are not required checks.
- **Fix**: Smallest honest fix: add the missing coverage to the manifest. Add a non-reusing native "Consumer" obligation with rules for `Tests/ConsumerModules/**`, `Build/DxUi.Consumer.*`, `test-consumer.ps1` and `include/DxUi/**`. Its runner would call test-consumer.ps1 (plus -DisableStlAnnotations under ASan Debug). Add the same kind of obligation for gallery.ps1 (`gallery.ps1`, `Tests/Controls/DxUi.Tests.Gallery.cpp`, samples) and for the non-focus MenuTextLayoutResources fixture. Make the fallback and …
- **Test**: Add a case to Tools/tests/Test-ScopedTesting.ps1. Call Get-ScopedTestPlan with ChangedPaths @('Tests/ConsumerModules/ConsumerModules.Tests.NativeModule.cpp'), then with @('test-consumer.ps1'), and assert that the plan selects the Consumer obligation (or that the plan's coverage label is not FULL_NON …

### scoped-integration-7

**The required `validation` check can now be skipped, and GitHub treats a skipped required check as passing**  
`.github/workflows/ci.yml:33` · #66, #67 · ci-gate · plausible (single: plausible/medium)

- **At e5ebbb5**: New in #66/#67.
- **What**: Before this window, `validation` had no `needs` and no `if`, so it started right away and could only end as success, failure or cancelled. It now has `needs: windows-tooling` and `if: ${{ !cancelled() }}`. It therefore waits for the Windows job (queue time plus up to 15 minutes), and if the run is cancelled during that wait its `if` is false and the job is reported as skipped. GitHub's docs say a job skipped by a condition reports "Success" and does not block a merge, even when it is a required check. The final step that turns a non-success Windows result into a failure never runs, because the whole job is skipped. Test-NativeScope.ps1:184 now locks in this `if:` as intended behavior.
- **Failure**: A pull request whose paired-benchmark scope is not relevant (docs-only, or a test-only change) finishes `paired-benchmark (pull request)` in about 2 minutes. Someone cancels the run from the Actions UI while `windows-tooling` is still running. `validation` becomes skipped and counts as success, so every required check is green. The PR merges without validate.ps1 having run on Linux or Windows. That means no validators and no tooling tests: broken docs/spec/gallery validation, a stale ci.yml digest or a failing Test-ScopedTesting can all reach main. The old workflow left `validation` as cancell …
- **Fix**: Make the gate job run even in a cancelled run, but skip the expensive work there: ``` validation: needs: windows-tooling if: ${{ always() }} runs-on: ubuntu-24.04 steps: - uses: actions/checkout@... if: ${{ !cancelled() }} - if: ${{ !cancelled() }} shell: pwsh run: ./validate.ps1 - if: ${{ always() }} ...guard unchanged (fails on 'cancelled'/'skipped'/'failure') ``` In a cancelled run the job now ends as failure, through the guard's throw on 'cancelled', not as skipped, so a required `validation …
- **Test**: In Tools/tests/Test-NativeScope.ps1, replace the line-184 assertion with checks on the `validation` job: - `Assert-True $validation.Contains('if: ${{ always() }}')`, placed at job level with the regex `(?m)^    if: \$\{\{ always\(\) \}\}\s*$`. - `Assert-True (-not ($validation -cmatch '(?m)^    if: …

### scoped-engine-14

**Scope rules are unverified: dead patterns and suite bodies living in other files** `Tests/test-scopes.json:130` · #66, #67 · selection · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Several rules match no tracked file, and nothing validates the manifest against the tree. `Tests/Controls/DxUi.Tests.MenuExitLifetime.cpp` does not exist: RunMenuExitLifetimeTests is in DxUi.Tests.Menu.cpp. `src/Controls/DxUi.Tooltip.*` and `src/Controls/DxUi.TextField.*` match nothing, because TextField and Tooltip live in shared sources. NewControls runs RunMenuDescriptionTests from DxUi.Tests.Menu.cpp, which build … **Fix**: In Read-ScopedTestManifest or Test-ScopedTesting.ps1, fail when a rule pattern matches no tracked file. Remove the MenuExitLifetime pattern, or point it at DxUi.Tests.Menu.cpp. Remove the dead Tooltip/TextField source patterns, or point them at the shared files that actually hold that code. Add NewControls to the Grid …

### scoped-engine-15

**FULL_NONINTERACTIVE_PASSED label excludes consumer builds and gallery that CI native jobs run** `Test-Changes.ps1:131` · #66, #67 · docs-contract · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Full and PrePush-without-delegation print FULL_NONINTERACTIVE_PASSED after the 20 scopes. The CI native job additionally runs test-consumer.ps1 (with -DisableStlAnnotations for ASan), the MenuResourceScaling and MenuTextLayoutResources characterization, and gallery.ps1. None of these are scopes. The plan records a consumer-only failure that the local qualification missed. **Fix**: Either add a Consumer scope that runs test-consumer.ps1 (for native profiles, including the ASan -DisableStlAnnotations variant), put it in prCoverage and in rules for Build/, include/, src/ and Tests/ConsumerModules/, or rename the label to something like FULL_SCOPES_PASSED and print which CI-only steps (consumer, gal …

### scoped-engine-16

**Start/end snapshot cannot detect inputs that change and revert during a run** `Test-Changes.ps1:121` · #66, #67 · toctou · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Reuse safety compares one identity taken before execution with one taken after. A change made and undone during the run (A→B→A) is invisible. The Tooling scope reads hundreds of files over minutes. In addition, the native identity at line 92 re-hashes source after the build-time check at line 85, instead of reusing $sourceBefore. **Fix**: Build $commonNative from $sourceBefore plus the $artifact identity from line 86 (or 78) instead of re-hashing. For tooling, either run the validators against an immutable snapshot (a temporary worktree of the hashed tree), or record per-file LastWriteTimeUtc/size with the identity and reject any input touched after the …

### scoped-engine-17

**Tooling receipt identity omits git/gh binaries and git configuration that the tooling fixtures execute** `Tools/ScopedTesting.psm1:147` · #66, #67 · reuse-identity · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
The shared Tooling receipt (`independent/Tooling.json`) is keyed on full source identity plus Get-ScopedEnvironmentIdentity. validate.ps1 runs Invoke-ToolingTests, whose fixtures shell out to real git (init/commit/update-ref/worktree in Test-ScopedTesting, Test-BenchmarkGate, Test-Docs, Test-NativeScope, Test-PairedRun, Test-ConsumerUpdate). Their outcome depends on the git version and the user's global/system git co … **Fix**: Add `git --version`, the resolved git/gh executable hashes and a hash of the global/system git config (or of the `git config --list --show-origin` lines for behavior-affecting keys) to the environment identity. Also make Test-ScopedTesting.ps1 and Test-Docs.ps1 fixtures pass `-c commit.gpgsign=false -c core.autocrlf=fa …

### scoped-engine-18

**Delegation checks the manifest's hard-coded repository, not the remote that will receive the push** `Tools/ScopedTesting.psm1:235` · #66, #67 · pr-coverage · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
`gh api "repos/$($Manifest.repository)/actions/workflows/ci.yml"` verifies that RedSalamanders/DxUi's workflow is active. It never verifies that HEAD will be pushed to, or a PR opened against, that repository. The checkout's `origin` (or the branch's push remote) can be a fork or mirror. GitHub disables Actions on forks by default, so a PR opened within the fork runs nothing. **Fix**: Before delegating, resolve the push remote for the current branch (branch.&lt;name&gt;.pushRemote, then remote.pushDefault, then origin), normalize its https/ssh URL to owner/repo, and return @() unless it equals $Manifest.repository (case-insensitive).

### scoped-integration-10

**GITHUB_ACTIONS alone authorizes focus-taking suites without the lease, including on self-hosted runners** `test.ps1:51` · #66, #67 · desktop-safety · plausible (batch: plausible/low)  
**At e5ebbb5**: New in #66/#67.  
The comment says "Hosted jobs own their desktop", but the check is `$env:GITHUB_ACTIONS -eq 'true'`, which every self-hosted runner also sets. Under `-Full`, Menu and NativeTextInput are added, and the local refusal at :56 is skipped. Both run directly, without `--no-activate` (test.ps1:123), so they take real focus with no lease, warning or consent. **Fix**: Require `$env:RUNNER_ENVIRONMENT -eq 'github-hosted'` in addition to `GITHUB_ACTIONS` at test.ps1:51 and :56, after confirming that every native matrix runner (including windows-11-vs2026-arm) reports github-hosted. Otherwise use an explicit opt-in such as `DXUI_CI_OWNS_DESKTOP=1`, set in ci.yml. Add a tooling assertio …

### scoped-integration-11

**Three scope rules name files that do not exist, and nothing checks that rules match** `Tests/test-scopes.json:130` · #66, #67 · rename-completeness · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
`Tests/Controls/DxUi.Tests.MenuExitLifetime.cpp` does not exist: RunMenuExitLifetimeTests is in DxUi.Tests.Menu.cpp:8810. `src/Controls/DxUi.Tooltip.*` (line 295) and `src/Controls/DxUi.TextField.*` (line 307) match no file: src/Controls has no Tooltip or TextField sources, so that code lives in DxUi.Controls.cpp and the text-input files. These paths fall back to the full plan, so selection stays conservative. Howeve … **Fix**: Remove the three dead rules, or point them at real files (MenuExitLifetime belongs with the Menu test source). Correct the plan's TextField claim. Add a Test-ScopedTesting case requiring each rule pattern to match at least one tracked path, using Get-ScopedTrackedPaths and Test-ScopedPattern.

### scoped-integration-12

**The paired overlay's legacy aliases are hard-coded and invisible to the SkipBuild check** `Tools/PairedRun.psm1:14` · #66, #67 · benchmark-fixture-integrity · plausible (batch: plausible/low)  
**At e5ebbb5**: New in #66/#67.  
Core_PerformanceAndResources.md:333 now requires that "Renamed benchmark headers MUST also overlay any existing legacy include paths". That is met only by a three-entry literal map (PairedRun.psm1:14-18), and nothing checks it against Get-BenchmarkInputPaths or git rename history. Get-OverlayCompiledChanges (PairedRun.psm1:175-181) counts only records whose path is in Get-BenchmarkInputPaths, which are the new names, … **Fix**: Add a Test-PairedRun or Test-BenchmarkGate assertion that every name in `git log --follow --name-only` history for each BenchmarkInput is either the current name or a key-mapped alias. Alternatively, derive the aliases from git rename history. Including alias records in Get-OverlayCompiledChanges is cheap belt-and-brac …

### scoped-integration-13

**Nothing guards the CI-only Menu/NativeTextInput addition; deleting it would leave CI green with no Menu coverage** `test.ps1:51` · #66, #67 · test-coverage · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Menu and NativeTextInput were taken out of the Suites default. The only thing that still runs them automatically is the GITHUB_ACTIONS branch at test.ps1:51-53. Test-ScopedTesting checks that the default list contains every manifest scope ('CI native default misses'), but Menu and NativeTextInput are not manifest scopes. No tooling test checks the GITHUB_ACTIONS addition, and none checks the new local refusal at test … **Fix**: Extend the 'reviewed CI preserves full native coverage' case. Parse test.ps1's AST and assert that an `if` whose condition references `$Full` and `GITHUB_ACTIONS` appends exactly Menu and NativeTextInput. Better, list the CI-only foreground suites in test-scopes.json (for example a `ciForegroundSuites` field), have tes …

### scoped-integration-14

**The renames changed benchmarkSha256 for byte-identical fixtures, so every retained pre-rename baseline is now 'Unmatched fixture'** `performance.ps1:77` · #66, #67 · evidence-continuity · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
benchmarkSha256 hashes `"&lt;path&gt; &lt;sha&gt;"` lines, so it includes the input paths. The rename changed three of those paths (BenchmarkMain.h, ComplexUiBenchmark.h and HeapDiagnostic.h are R100, identical bytes). Every receipt made before the rename now differs from every receipt made after it, and Assert-MatchedFixture rejects the pair. A pre-rename revision's own performance.ps1 still hashes the old paths, so … **Fix**: Add a note to the ScopedTesting Changes fragment or docs/performance.md: retained receipts made before f840b50 do not match post-rename receipts, so use performance-paired.ps1 with -BaselineRevision against a pre-rename commit. Optionally key benchmarkInputs by a stable role name to avoid future pure-rename breaks. Tha …

### scoped-integration-15

**Test-Changes says it does not touch the desktop, but the scopes it auto-selects move the physical pointer** `Test-Changes.ps1:32` · #66, #67 · documentation-overclaim · confirmed (batch: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Test-Changes is now the default target of a plain `./test.ps1`. Its help says 'Does not activate the person's desktop', and the scoped-testing plan says 'No test may seize the person's foreground or pointer without agreement to the time.' Any unmapped change selects the NewControls scope, which runs RunMenuDescriptionTests (DxUi.Tests.NewControls.cpp:2331). That includes TestMenuChoosesTheCursorWhenItOpensAndCloses, … **Fix**: Make Test-Changes.ps1 help and Tests/README say that the noninteractive NewControls lane still moves the physical pointer, until the menu cursor tests change. The structural fix is to skip the AlignCursor-based menu-description tests when DxUiTestWindowsCanActivateFlag() is false, recording a capability skip, or to mov …

### scoped-integration-3

**Normative docs and AGENTS.md still describe test.ps1 as the full gate that always benchmarks and runs tooling** `Specs/Core/Core_PerformanceAndResources.md:257` · #66, #67 · docs-overclaim · confirmed (single: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
The handoff to Test-Changes, `-SkipTooling` in CI and the local refusal of foreground suites make several normative or instructional statements false. (1) Core_PerformanceAndResources.md:257 says "Every `test.ps1` invocation MUST report complex-UI FPS and memory", and AGENTS.md:44-45 says "Every test.ps1 run reports complex-UI FPS/memory". A handed-off run whose native scopes are all REUSED, or which selects only Too … **Fix**: Edit the docs only; no code change is needed, because CI's benchmark and tooling coverage is intact. (1) Core_PerformanceAndResources.md:257 and AGENTS.md:44-45: limit the MUST to runs that execute native suites (`test.ps1 -Full` or explicit `-Suites`). State that a scoped run whose native scopes are all REUSED, or whi …

### scoped-integration-8

**Affected iteration selects no validation for non-Markdown docs/, Measurements/ and Changes/ files, though validate.ps1 checks them** `Tools/ScopedTesting.psm1:101` · #66, #67 · mis-scoped-selection · confirmed (single: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
Get-ScopedTestPlan checks `.md` first and sends it to Tooling. Any other path that no rule matches and that is under docs/, Measurements/ or Changes/ is recorded with `scopes=@()`, meaning nothing runs. The Tooling scope (validate.ps1) does read those files. Test-DxUiMeasurements parses every Measurements/**/*.json receipt (owner, fixture, 64-hex benchmarkInputs, a README beside it). Test-DxUiDesignSystem reads docs/ … **Fix**: At ScopedTesting.psm1:101-102, select the non-native scopes for these paths, as the `.md` branch already does, instead of `scopes=@()`: `$targets=@($Manifest.scopes | Where-Object { -not $_.native } | ForEach-Object name); foreach($n in $targets){[void]$selected.Add($n)}; $reasons.Add([pscustomobject]@{path=$path; scop …

### scoped-integration-9

**Edits to Menu/NativeTextInput test sources fall back to 'full' and report FULL_NONINTERACTIVE_PASSED without running the suite that owns them** `Tests/test-scopes.json:107` · #66, #67 · misleading-verdict · confirmed (single: confirmed/low)  
**At e5ebbb5**: New in #66/#67.  
The rules give every control suite's source file an owning scope except DxUi.Tests.Menu.cpp, DxUi.Tests.NativeTextInput.cpp and DxUi.Tests.Gallery.cpp. An edit to one of those falls through to 'unmapped input; conservative full fallback' (ScopedTesting.psm1:105). That selects all 20 manifest scopes, so `plan.full` is true and the run ends with FULL_NONINTERACTIVE_PASSED. Menu and NativeTextInput are not manifest scop … **Fix**: 1. Add explicit rules to Tests/test-scopes.json for the files owned by foreground suites: - `Tests/Controls/DxUi.Tests.Menu.cpp`, `Tests/Controls/DxUi.Tests.MenuResources.h`, `Tests/Controls/DxUi.Tests.NativeTextInput.cpp`, `src/Controls/DxUi.Menu*` and `src/Controls/DxUi.NativeTextInput*`. - Each rule maps to the noni …

## Refuted (8)

- **menu-loop-lifetime-9**: Name-registered messages from any process can make a DxUi window write through an arbitrary pointer or detach its host (`src/Controls/DxUi.Accessibility.cpp:9694`). Refuted: The handlers exist as quoted (Accessibility.cpp:9694-9700 writes through `reinterpret_cast&lt;IRawElementProviderFragmentRoot**&gt;(lp)`; WindowHost.cpp:2418-2422 calls DetachForProcessExit). However, only same-desktop processes at the same or higher integrity can send these messages, because UIPI b …
- **controls-editor-theme-3**: Hiding or zero-sizing an embedded view never cancels a captured drag; a Cancel sent while hidden is dropped and the drag (and touch halo) resumes after re-show (`src/Rendering/Embedded.cpp:208`). Refuted: The finding's premise is that SetVisible(false) and a zero-extent Prepare never cancel the pointer capture. That is wrong. Both paths call ReleaseSurface(), and ReleaseSurface() calls CancelPointer() unconditionally as its second statement. CancelPointer() clears _host._capturedControl. When the cap …
- **public-api-architecture-10**: Slider touch halo is not bounded by the control, unlike the chrome disc it grows from (`src/Controls/DxUi.Controls.cpp:4846`). Refuted: The code does let the halo grow unclipped to 48 DIP. Controls.cpp:4846 has `std::lerp(ResolveHaloDiameter(GetBounds()), kSliderTouchHaloDiameterDip, _touchTransition.progress)`, and the paint at 4965-4979 pushes no clip. The owning contract makes this deliberate. Specs/UI/UI_ControlsAndLayout.md:225 …
- **tooling-perf-gate-8**: -StrictControls downgrades a confirmed degradation to 'inconclusive' (`Tools/BenchmarkGate.psm1:226`). Refuted: The behavior is intended and documented. Get-BenchmarkConclusion's doc comment (BenchmarkGate.psm1, above Get-BenchmarkConclusion) says: 'StrictControls restores the all-metrics reading: any unstable control makes the scenario inconclusive, whatever it found.' Test-BenchmarkGate.ps1:332-334 asserts …
- **process-docs-specs-8**: MenuDescriptions plan still says the described-menu tradeoff branch is not in main (`Specs/Plans/WIP/MenuDescriptions_2026-09-21.md:41`). Refuted: The central claim is false. Line 41 says the timing-tradeoff acceptance is on `codex/menu-description-layout` (`0f35bab`), 'which main does not yet contain', and that is literally true at HEAD: `git merge-base --is-ancestor 0f35bab HEAD` fails. `git grep -i 'timing tradeoff' HEAD -- Specs docs` matc …
- **gap-embedded-host-parity-4**: A consumer that drops registered messages silently loses TSF deferred locks and UIA actions, and no gate or diagnostic detects it (`src/Controls/TextInputServices.cpp:453`). Refuted: The finding's main claim is wrong. The TSF deferred lock did not move to a registered message in #41. At the window base 9f5bc07, and back to 2ca8cc0 (2026-09-05), TextInputServices.cpp already used `RegisterWindowMessageW(L"DxUi.TextInputServices.DeferredLock.v1")`. #41 only moved the registration …
- **gap-perf-evidence-pipeline-2**: The strict-controls test asserts that a confirmed degradation becomes 'inconclusive' (`Tools/tests/Test-BenchmarkGate.ps1:332`). Refuted: The code the reviewer quotes does exist. Tools/BenchmarkGate.psm1:226-228 turns 'degraded' into 'inconclusive' under -Strict, and Tools/tests/Test-BenchmarkGate.ps1:332-333 asserts that result. This is not a defect, for three reasons. (1) The behavior is the documented contract of an opt-in mode: - …
- **scoped-engine-11**: Local NativeScope decision uses an unfetched origin/main merge-base; CI uses the merge ref's first parent and the PR's real base (`Tools/ScopedTesting.psm1:237`). Refuted: The code the finding quotes is still at HEAD e5ebbb5, so the review text is current. The two bases really are computed differently. The local side diffs from merge-base(origin/main, HEAD) with no fetch, always against main. CI diffs the merge ref against its first parent, using GITHUB_BASE_REF. The …
