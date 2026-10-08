# Production review of the 25 September - 4 October merges

- **Status**: ACTIVE (review complete; no fix started; questions below await the developer)
- **Date**: 2026-10-05
- **Window**: `9f5bc07` (main on 2026-09-25) to `49a9988`: 45 merged pull requests, #21 to #65. About 6,500 changed lines of
  `src`/`include`, 24,000 of tests and 7,700 of tooling and CI.
- **Revalidated**: against `e5ebbb5` (#66, #67) on 2026-10-06; see [Revalidation](#revalidation-at-e5ebbb5).
- **Owning contracts**: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
  [theme](../../UI/UI_ThemeAndTypography.md), [Win32 host](../../Rendering/Rendering_Win32Host.md),
  [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [performance](../../Core/Core_PerformanceAndResources.md),
  [validation](../../Testing/Testing_Validation.md) and [toolchain](../../Build/Build_ToolchainAndConsumption.md).
- **Evidence**: [reviewer findings and verifier verdicts](ProductionReview_2026-10-05/findings.md). The identifiers in
  parentheses below (for example `host-core-1`) are its entries, each with the failure scenario, a fix and a failing test.

## How the review was made

Static review only: nothing was built or run. Nineteen reviewers each took one area: menu loop and lifetime, menu layout,
UIA lifetime and threading, UIA selection semantics, Grid selection, Grid multiline text, Tree, core and editor controls,
text input, the host core, the public API, cross-cutting simplification, test support, two test-quality slices, two
tooling slices, CI and process. Each production area then had a second reviewer looking only for what the first one
missed: side effects in surrounding code, interactions between pull requests, and error and destruction paths. A
completeness critic named six uncovered risk classes (UIA test falsifiability, the accessibility mutex with nested loops,
high-contrast/RTL/DPI visuals, embedded-host parity, the performance evidence pipeline, ARM64/ASan runtime), and each was
reviewed in turn.

Every finding was checked adversarially at `49a9988`. High and critical findings got two verifiers, one tracing the path
from a reachable entry point and one trying to refute it. Medium findings got one verifier, and low findings and
simplifications got a batch verifier per area. Of 274 reported findings, 7 were refuted, 223 confirmed and 44 judged
plausible (real code path, but depending on an unestablished condition). After merging duplicates, about 150 distinct
items remain. The lead reviewer re-read the code for every item marked **[lead]**.

The previous review ([ReviewFixes](../Done/ReviewFixes_2026-09-29.md)) covered 14 to 28 September. Items it deliberately
declined were not reported again. Items marked *pre-existing* predate the window but sit in code the window changed or
depends on.

## Revalidation at e5ebbb5

On 2026-10-06 main moved to `e5ebbb5` with #66 and #67 (scoped testing). Neither touches `src`, `include` or `Samples`.

- **Production findings**: every one still holds, at the same line.
- **Test-file findings**: #66 renamed the test files to the `Scope.Tests.Something` scheme. Only include lines changed,
  one for one, so their lines are unchanged too.
- **Tooling and CI findings**: of those in files #66 modified, nine still hold at moved lines and three still hold with
  changed context (noted in their items). Two are fixed by #66: a plain `test.ps1` no longer runs the focus-taking
  suites.
- **Appendix**: shows every finding at its `e5ebbb5` path and line, with an "At e5ebbb5" status.

#66 and #67 also add about 650 lines of new tooling, so they were reviewed the same way: two reviewers (the scoped-testing
engine; its integration with `test.ps1`, CI and the paired runs), a second look each, and adversarial verification. Of 33
findings, 29 were confirmed, 3 plausible and 1 refuted, none of high severity. They are in
[Scoped testing (#66, #67)](#scoped-testing-66-67) and in the appendix under `scoped-*`.

## Verdict

**Not production-ready as merged.** The main reasons:

- **Use-after-free reachable through documented API.** At least seven paths, from `ControlHost::SetRoot` (#31), the host
  pointer-down path, Grid checkbox/group delegates, TextField's clear button and the process-exit host sweep.
- **Per-call-site lifetime checks.** #57 and #60 put a lifetime check at each call site that may destroy the control. Many
  sites in ComboBox, TextField, TabControl, the Grid and the UIA actions are still unguarded. The structural fix is A1/A5 below.
- **Keyboard regressions.** A focused Tree with no current item selects row 0 on *any* key (Tab, Ctrl, Shift). A Grid
  cannot extend a Shift range upward.
- **Accessibility gaps in the new features.**
  - Range values are never announced in a window host.
  - Embedded hosts raise no focus events for tree items or grid rows.
  - UIA actions run application callbacks under a process-wide lock.
  - The Tree's focus ring is invisible on a selected row.
- **Test infrastructure.** Ctrl+C under `test.ps1 -Interactive` can kill the lease before it restores the desktop.

#66 and #67 change no library code. Their scoped-testing tooling has no high-severity defect, but a plain `test.ps1` can
now pass without running anything, and `-Mode PrePush` can report coverage that no required check enforces.

The design, documentation and evidence culture is strong. Most findings are gaps in otherwise careful work, and the
fixes are local.

## P0: memory safety, hangs and crashes

Each fix lands with an ASan test that fails at `49a9988`.

- [ ] **`ControlHost::SetRoot` frees the old root before `Reparent` reads the moved control's old parent** (#31) **[lead]**.
  `DxUi.WindowHost.cpp:1367` assigns `_root` (destroying the old tree), then `Reparent` walks `_parent` through
  `GetFlowDirection`/`GetDensity` (`DxUi.cpp:919`). This is reached by promoting a child of the current root, which the
  `Panel::GetChildren` comment (`DxUi.h:1671`) documents. Before #31, `SetParent(nullptr)` never read the parent. Fix:
  keep the old root alive until after `Reparent`, as `PageHost::SetPage` does, and see A5.
  (`public-api-architecture-1`, `host-core-1`, `uia-lifetime-threading-1`, `cross-cutting-simplification-1`)
- [ ] **Host pointer-down takes native focus, then reuses the clicked control unchecked** (#57 gap) **[lead]**.
  `DxUi.WindowHost.cpp:2731-2744`: `SetFocus(hwnd)` sends `WM_SETFOCUS`, and `OnSetFocus` may run `SetFocusControl` and
  the application's focus callback, which #57 made a supported place to rebuild the tree. `SetFocusControl(liveControl)`,
  `CaptureMouse` and `RememberPointerButtonDown` then dereference a freed control. `SetFocusControl` itself takes the
  requested control's lifetime only after `PruneStaleInteractionState`, which can run `OnFocusChanged(false)`.
  (`host-core-3`, `cross-cutting-simplification-3`)
- [ ] **Grid checkbox and group delegates** (#60 gap) **[lead]**.
  - `ToggleCheckboxCell` takes its lifetime token *after* `OnGridCheckboxToggled` (`DxUi.Grid.cpp:5210-5213`).
  - The group-header press (`:3610`), the keyboard group toggle (`:3990`) and the `ApplyGroupLayout` loop (`:1363`) keep
    using the grid after `OnGridGroupToggled`.
  - An application that calls `NotifyDataChanged` there (as docs/controls.md tells it to) runs the selection delegate,
    which may rebuild the view. (`grid-selection-lifetime-2`, `process-docs-specs-3`)
- [ ] **Async menu open at process exit** (#23, #41) **[lead]**. `ShutdownAllWindowHostsForProcessExit` detaches the root
  popup's host. `Detach` calls `ReleaseCapture`, whose `WM_CAPTURECHANGED` finalizes the async menu and destroys that same
  host mid-`Detach` (`DxUi.WindowHost.cpp:1288`, `DxUi.Menu.cpp:2271`). The sweep's snapshot also still holds the freed
  submenu hosts. Fix: close open menus before the sweep and re-check the registry for each target.
  (`menu-loop-lifetime-2`)
- [ ] **Callbacks that destroy their control: remaining unguarded sites.**
  - [ ] TextField clear button writes after `onTextChanged` destroyed the field: `SetTextAndNotify` drops
    `NotifyChanged()`'s result (`DxUi.TextInput.cpp:1752`, `:1056`) **[lead]** (`text-input-1`).
  - [ ] `ImportTextInputState(state, true)` always returns true, so the native IMM commit (result plus composition in
    one message) and `WM_PASTE`/`WM_UNDO` keep using a field `onTextChanged` destroyed
    (`DxUi.NativeTextInput.cpp:1395`, `DxUi.TextInput.cpp:2969`) (`text-input-2`).
  - [ ] ComboBox selection and text callbacks; `NotifyTextChanged` calls the stored `std::function` in place
    (`DxUi.ComboBox.cpp:2314`, `:2660`) (`controls-editor-theme-1`, `text-input-10`).
  - [ ] TabControl: a tab switch publishes inside its page-visibility loop. `RemoveTab`/`SelectTab` reuse an index that
    focus callbacks may shift (`DxUi.Controls.cpp:7214`, `:6603`) (`public-api-architecture-5`, `controls-editor-theme-13`).
  - [ ] `ActivateNativeTextInputSession` keeps using the control after its own `SetFocus(_hwnd)`
    (`DxUi.NativeTextInput.cpp:591`) (`text-input-4`).
  - [ ] Tree press/double-click read the model through a hit test made before the focus callbacks
    (`DxUi.Tree.cpp:1128`). Keyboard landing calls raw `SetFocus` and continues (`:1380`, plausible). `RequestExpandedState`
    reports success after the publish destroyed the tree (`:698`) (`tree-2`, `tree-7`, `tree-9`).
  - [ ] UIA actions:
    - `SetFocus` on a tree item focuses the tree through a raw pointer after a publish that may destroy it
      (`DxUi.Accessibility.cpp:8435`).
    - `RemoveFromSelection` on grid rows touches the host with no survived() check (`:8866`).
    - Elements created with control identity 0 can never detect destruction (`:5654`), so `ExecuteSelect` can focus a
      freed Grid or Tree.

    (`tree-5`, `uia-selection-semantics-11`, `public-api-architecture-12`, `grid-selection-lifetime-17`,
    `uia-lifetime-threading-5`, `gap-uia-mutex-nested-loop-matrix-2`)
  - [ ] TextField/ComboBox continue after `SyncTextInput`, whose native accessibility publish raises UIA events. The
    window host dereferences the host after its own raises. Both are plausible: they depend on question Q3
    (`text-input-5`, `public-api-architecture-13`, `gap-uia-test-falsifiability-3`).
- [ ] **UIA actions run application code under the process-wide accessibility mutex** **[lead]**.
  - `ExecuteToggle`, `SetStringValue`, `SetRangeValue`, `Select`, `AddToSelection`, `RemoveFromSelection` and tree
    `Expand` keep the static `std::recursive_mutex` locked while delegates, focus callbacks, nested modal loops (a delegate
    that opens a dialog) and UIA raises run (`DxUi.Accessibility.cpp:8601`, `:8716`).
  - Every provider call on every thread blocks meanwhile, including Narrator's queries of other windows. A delegate that
    synchronously waits on another DxUi UI thread deadlocks.
  - `SetFocus` and `Invoke` already release the lock first. Fix: one resolve, release, act, revalidate helper for all
    eleven actions (A4).

  (`uia-lifetime-threading-4`, `uia-selection-semantics-1`, `host-core-2`, `cross-cutting-simplification-2`,
  `gap-uia-mutex-nested-loop-matrix-1`)

## P1: user-visible defects

Keyboard and pointer:

- [ ] **Tree: any key selects or focuses row 0 and scrolls to the top when no item is current** (#35, #53) **[lead]**.
  `DxUi.Tree.cpp:1389` runs the "first key" step before the switch, so Tab, Shift, Ctrl, Alt, Escape and the F-keys
  (the host offers Tab to the control first) change selection silently, without `OnTreeSelectionChanged`. Since #53 they
  also announce `ElementSelected`. Fix: only for keys the switch handles. (`tree-1`)
- [ ] **Grid: Shift+Up/PgUp/Home cannot extend a range upward** **[lead]**. The current row is
  `GetOrderedSelection().back()`, and `SetRange` stores the range in visible order. So after an upward range the "current"
  row is the anchor. A data refresh also reorders a Ctrl+click selection, which moves the keyboard position, moves UIA
  focus and fires `OnGridSelectionChanged` with nothing changed. Fix: a focused row of its own (A3).
  (`grid-selection-lifetime-1`, `uia-selection-semantics-2`, `grid-selection-lifetime-5`)
- [ ] **Embedded `Cancel` clears the pending double-click** **[lead]**. The reference sample (and docs/hosting.md's
  "Cancel on capture loss") sends Cancel on every `ReleaseCapture`, so embedded double-clicks never register: word
  selection, row activation. The window host does not clear it (`Embedded.cpp:438`). *Pre-existing*, in a function #29
  edited. (`gap-embedded-host-parity-3`)
- [ ] **Menu loop swallows every `WM_TIMER` with id 1 on the thread** **[lead]**. `kSubmenuHoverTimerId` and the
  `AnimationDispatcher` timer are both 1. The loop `continue`s without dispatching (`DxUi.Menu.cpp:5862`), so animations
  in other windows freeze while a modal menu is open. Fix: also require `FindPopupForHwnd(msg.hwnd)`.
  (`menu-loop-lifetime-1`)
- [ ] **Plain menus at 125%/175% reserve a scrollbar lane and scroll by a fraction of a DIP** (#24). The half-pixel
  tolerance was applied to described popups only (`DxUi.Menu.cpp:1183`). Every plain menu with an odd separator count at
  125% is affected. (`menu-layout-ux-1`)
- [ ] **Synchronous `ContextMenu::Show` never dismisses on deactivation or `WM_CANCELMODE`**: its branch is unreachable
  (`DxUi.Menu.cpp:5839`) (`menu-loop-lifetime-3`, question Q7).
- [ ] **Menu scrolling and pointer state.**
  - #59's edge reveal also runs when a submenu opens from hover or click, scrolling the parent under a stationary pointer
    without a repaint or UIA bounds (`DxUi.Menu.cpp:1306`, `:4073`).
  - Wheel and scrollbar scrolling keep the hovered row and its submenu timer on a row that moved (`:4980`).
  - Pointer targeting ignores z-order over the root's hidden scrollbar lane (`:4651`).

  (`menu-layout-ux-2`, `menu-loop-lifetime-5`, `menu-layout-ux-6`, `menu-loop-lifetime-6`)
- [ ] **Menu mnemonics compare the virtual key, not the typed character**, so non-Latin and accented labels are
  unreachable and the wrong command can run on other layouts. The first match invokes immediately even when several
  rows share the key (`DxUi.Menu.cpp:5380`, `:5420`) (`menu-loop-lifetime-4`, `menu-layout-ux-7`, `menu-layout-ux-4`,
  Q8).
- [ ] **Grid navigation**:
  - columns beyond the viewport cannot be reached by keyboard or horizontal wheel (`DxUi.Grid.cpp:4136`);
  - collapsed groups are unreachable by keyboard and UIA, and with every row grouped and collapsed the grid ignores all
    keys (`:3938`);
  - deleting the selected row loses the keyboard position;
  - `SetSelectionMode(Single)` keeps the oldest row, not the current one.

  (`grid-selection-lifetime-12`, `grid-selection-lifetime-3`, `grid-selection-lifetime-10`, `grid-selection-lifetime-18`)
- [ ] **Tree multi-selection drag accepts drops onto selected rows or inside another selected row's subtree**. The
  documented whole-selection move then creates a cycle (`DxUi.Tree.cpp:333`). Related: a pending release-collapse
  overrides selection made while the press is held, and `SetSelectedItemId` in multi-select keeps a non-visible id.
  (`tree-3`, `public-api-architecture-6`, `tree-8`, `public-api-architecture-14`)
- [ ] **Slider and editor controls**:
  - transitions freeze when an ancestor hides the control mid-transition, so a stale halo or pressed thumb shows on
    re-show (#62, `DxUi.Controls.cpp:4907`);
  - a slider removed mid-drag never gets `Cancel`;
  - a same-value `SetValue` during a typed NumericStepper edit ends the edit (`DxUi.EditorControls.cpp:929`);
  - Escape does not revert unparsed text;
  - slider setters are order-dependent (tick marks, `LargeStep`).

  (`controls-editor-theme-5`, `-10`, `-4`, `-12`, `-11`)
- [ ] **Host capture and inheritance**:
  - #29's stale-capture cancel misses controls captured inside a ScrollPanel (`DxUi.Controls.cpp:8948`) and runs only at
    message entry, not on `SetFocusControl`'s prune (`DxUi.WindowHost.cpp:4139`);
  - PageHost pages never inherit flow direction or density (#31, `DxUi.Controls.cpp:1380`).

  (`host-core-6`, `host-core-4`, `host-core-8`)

Text input:

- [ ] **The host window's title messages go to the focused text field** **[lead]**. The native backend is the only one,
  so every window host has this. While a text control has focus, `WM_GETTEXT`, `WM_GETTEXTLENGTH` and `WM_SETTEXT`
  sent to the host HWND are answered by that field (`DxUi.WindowHost.cpp:2977`, `DxUi.NativeTextInput.cpp:1071`,
  `:1096`).
  - An application that calls `SetWindowTextW` on its window while the user types replaces the field's text and fires
    `onTextChanged`; the title does not change.
  - The accessibility publish takes the window name from `GetWindowTextW` (`DxUi.Accessibility.cpp:953`), so UIA
    clients read the field's text, a masked field's plaintext included, as the window's name.

  Fix: stop answering these three messages for the host window (Q17). (`text-input-3`)
- [ ] **Native TSF edits go through `SetText`**: IME commits, the emoji panel and dictation reset a multiline field's
  scroll to the top and clear undo (`DxUi.TextStoreACP.cpp:389`). *Pre-existing*. Apply them through
  `ImportTextInputState` as the IMM and embedded paths do. (`text-input-7`, Q17)
- [ ] **IME placement and composition**:
  - single-line `GetTextExt` ignores horizontal scroll and the masked display text (`DxUi.TextStoreACP.cpp:272`);
  - the native TSF path applies every composition preview as a committed, notifying edit (`:1157`);
  - #55's multiline caret clip half-erases the caret at line start (`DxUi.TextInput.cpp:1605`);
  - the IME caret rect is not clipped.

  (`text-input-13`, `text-input-11`, `text-input-6`, `text-input-16`)
- [ ] `EM_SETSEL` with a reversed range drops the selection, and with start -1 moves the caret to 0. An IMM
  composition continues into another DxUi field of the same HWND. (`text-input-20`, `text-input-12`)

Grid multiline cells (#22, #36):

- [ ] **The full-value tooltip has no size bound**: it grows past the window, cuts at 256 DIP with no marker, and is
  re-laid out on every mouse move (`DxUi.Controls.cpp:9467`) (`grid-multiline-1`).
- [ ] **Single-line cells still break on separators**: a trailing CR/LF shifts and clips the caption, and
  U+2028/U+2029/NEL/VT/FF hide later lines with no tooltip (`DxUi.Grid.cpp:2538`) (`grid-multiline-4`).
- [ ] The Marquee band paints outside its track over the previous column, including under reduced motion (#28,
  `DxUi.Grid.cpp:477`) (`grid-multiline-2`).
- [ ] Copying multiline cells writes raw CR/LF/TAB into the TSV, so a pasted row breaks apart, and the #36 test locks
  this in (`grid-multiline-3`, Q19).
- [ ] Low:
  - no horizontal clip in very narrow multiline cells;
  - hover tooltip ignores viewport-cut cells;
  - justified text loses justification when lines are omitted;
  - Spinner/Marquee captions are measured with the wrong font.

  (`grid-multiline-7`, `-6`, `-11`, `-12`)

## P1: accessibility

Announcements and focus:

- [ ] **Range values never reach UIA clients in a window host** **[lead]**.
  - Slider, Splitter and ProgressBar never republish the snapshot after a keyboard step, a UIA `SetValue` or a
    programmatic value.
  - The window-host diff raises no property events at all (only structure, focus and selection).
  - Narrator is silent, and `RangeValue.Value` stays stale.

  (`controls-editor-theme-2`, `uia-selection-semantics-9` for menu slider rows)
- [ ] **Embedded hosts raise no focus event when focus moves between tree items or grid rows**, or onto a newly added
  or replaced focused control. On gaining focus they announce the Tree, not the item `GetFocus` reports. Fix: one focus
  rule for both hosts (A6). (`gap-embedded-host-parity-1`, `grid-selection-lifetime-8`, `tree-6`,
  `uia-lifetime-threading-6`, `uia-selection-semantics-10`, Q16)
- [ ] **`HasKeyboardFocus` is wrong in three places**:
  - every selected grid row reports it (the Tree fix was not applied to Grid);
  - an embedded view without keyboard focus reports it for the focused item;
  - the window root is announced as focused but answers false.

  (`uia-lifetime-threading-8`, `uia-selection-semantics-7`, `grid-selection-lifetime-20`, `tree-14`,
  `uia-lifetime-threading-9`)
- [ ] **`SetFocusControl` prunes first and announces an intermediate "window" focus before the requested control**. The
  publish's `GetWindowTextW` re-enters `HandleMessage`, and a nested republish is then overwritten by a stale snapshot.
  (`uia-lifetime-threading-7`, `host-core-9`, `uia-lifetime-threading-2`)
- [ ] **Missing UIA events**:
  - expanding or collapsing a tree item raises nothing;
  - wheel scrolling never republishes, so bounds and hit tests go stale;
  - `OnDpiChanged` never republishes;
  - `ApplyGroupLayout` changes selection without publishing.

  (`uia-selection-semantics-8`, `-14`, `host-core-7`, `grid-selection-lifetime-13`)
- [ ] After Ctrl+A on a grid of more than about 280 rows, UIA focus names a row element the snapshot does not hold (no
  name or control type) (`grid-selection-lifetime-7`).

Selection patterns:

- [ ] **`AddToSelection` on an already-selected grid row deselects it**: it is implemented as a Ctrl+click toggle
  (`DxUi.Accessibility.cpp:8803`) **[lead]** (`uia-selection-semantics-3`, `grid-selection-lifetime-4`).
- [ ] **UIA `SetFocus` on a tree item changes a single-select tree's selection without `OnTreeSelectionChanged`**. On a
  grid row it replaces a multi-row selection. `RemoveFromSelection` on a single-select tree clears silently.
  (`tree-4`, `uia-lifetime-threading-10`, `uia-selection-semantics-12`, `grid-selection-lifetime-9`,
  `uia-selection-semantics-4`, Q14)
- [ ] Disabled Tree and Grid accept UIA Select, AddToSelection, RemoveFromSelection and Expand/Collapse
  (`uia-selection-semantics-5`, `tree-15`).
- [ ] A selection that becomes exactly one new item is reported as `Selection_Invalidated` whenever more than 20 were
  selected before (WPF reports `ElementSelected`) (`uia-selection-semantics-6`, Q14).
- [ ] Tree items always report `IsOffscreen=false` (`uia-selection-semantics-16`).

Menus (#56):

- [ ] **Row names leave out the accelerator column**: shortcuts, Info values and a slider's current stop are never
  exposed. Ordinary rows expose no submenu ExpandCollapse state. (`menu-layout-ux-5`, `uia-selection-semantics-15`)
- [ ] **UIA focus on a row leaves the hovered row selected**, and a later pointer move clears the keyboard row without
  repaint or UIA sync, so Enter invokes a row other than the one UIA reports. Closing a submenu or a UIA `SetFocus` on a
  row raises no focus event. A root switch focuses the owner in between. (`menu-layout-ux-3`,
  `uia-selection-semantics-13`, `uia-lifetime-threading-17`, `menu-loop-lifetime-7`)

Text ranges:

- [ ] `Move` reports `moved=1` without moving at the last character and for every Paragraph/Page/Document move, so
  "move until 0" clients never stop. Visual-line moves overflow `int` for large counts. (`uia-lifetime-threading-11`, `-18`)

Visual accessibility (gallery and design-system republish required):

- [ ] **The Tree focus ring has the selection fill's own color**, so with multi-select the current row is invisible once
  selected (1:1 in the default light theme, about 1.2:1 in dark and HC) (#35, `DxUi.Tree.cpp:183`). The Grid draws no
  focus indicator at all. (`gap-visual-modes-hc-rtl-dpi-1`, `grid-selection-lifetime-11`)
- [ ] **Unfocused selected-row text is never contrast-checked**: the Tree fails in every Windows HC theme (about 1.4:1),
  the Grid in the default light theme (about 1.9:1) (`DxUi.Tree.cpp:78`). *Pre-existing*, but multi-select makes whole
  blocks unreadable. `ChooseContrastingTextColor` picks failing colors on mid tones, and #29 added a correct WCAG helper
  beside it instead of fixing it (A7). (`gap-visual-modes-hc-rtl-dpi-2`, `-6`)
- [ ] Described-menu descriptions fall to about 3.5:1 in the light HC theme (Desert) (`gap-visual-modes-hc-rtl-dpi-3`,
  Q11).
- [ ] Tree, described menus, Grid and determinate ProgressBar ignore `FlowDirection`, and no public document records the
  limit (`gap-visual-modes-hc-rtl-dpi-4`, `controls-editor-theme-8`, Q10).

## Security and privacy

- [ ] **A masked TextField answers `WM_GETTEXT` with its plaintext** **[lead]**. Any process in the session can send it,
  unlike a Win32 `ES_PASSWORD` edit. The edit-message shim also answers the host window's own title messages
  (`DxUi.NativeTextInput.cpp:1071`). Masked fields also expose the plaintext to TSF text services and accept IME
  composition (`DxUi.TextStoreACP.cpp:339`). (`text-input-3`, `text-input-9`, Q17)
- [ ] **Registered messages trust their parameters** **[lead]**.
  - `AccessibilityCreateProvider` writes a provider pointer through a raw `lParam` (`DxUi.Accessibility.cpp:9696`), and
    its sender uses an untimed cross-thread `SendMessageW`.
  - Use the existing token-based `SendMessagePayload` with a timeout. That removes both the arbitrary-address write from
    another process and the unbounded wait.

  (`host-core-5`; `menu-loop-lifetime-9` was refuted as a *security* issue on the same-session trust model, Q18)
- [ ] The gallery publish job keeps its `contents: write` token in `.git/config` while it restores, builds and runs
  branch code (`.github/workflows/gallery.yml:94`). Use `persist-credentials: false` and push with the token only in
  the commit step. (`ci-workflows-4`)

## Resources and performance

- [ ] **Accessibility publish cost**:
  - each publish costs O(selected rows) model lookups even after #43 made paint flat;
  - every key press scans all rows, and Ctrl+C scans all groups per selected row;
  - `Tree::SetSelectedItemIds` is quadratic;
  - every Ctrl+click copies every visible `TreeItemData`.

  (`grid-selection-lifetime-6`, `-19`, `cross-cutting-simplification-5`, `public-api-architecture-4`)
- [ ] **Snapshot rebuilt eagerly**: every focus, selection or text change rebuilds the whole window snapshot (every
  expanded tree item, every grid row on screen) even when no UIA client listens. Only the diff is gated on
  `UiaClientsAreListening` (lead observation, Q4, A1).
- [ ] **Text-range providers pin a full window snapshot each**, one per caret move. #29 stopped element providers doing
  this, and embedded event providers still do. Every text-range call copies the control's whole text.
  (`uia-lifetime-threading-3`, `cross-cutting-simplification-4`, `gap-embedded-host-parity-2`,
  `uia-lifetime-threading-19`)
- [ ] **Multiline TextField shapes its text twice per frame**, caret blink included. The multiline `GetTextExt`
  fallback rebuilds a full layout per index. (`text-input-8`, `text-input-14`)
- [ ] **Posted payloads**:
  - the registry serves at most 128 windows (hosts plus open popups) and silently refuses later ones, so UIA actions
    and menu UIA invoke stop working;
  - a full registry is not diagnosed;
  - its static destructor frees entries without the lock.

  (`host-core-10`, `menu-loop-lifetime-11`, `uia-lifetime-threading-20`, `gap-uia-mutex-nested-loop-matrix-3`,
  `menu-loop-lifetime-8`, `gap-arm64-asan-runtime-7`)
- [ ] **Allocation failure terminates**: the window-host publish and the selection diff allocate inside `noexcept`
  functions, and `ParseMenuLabel` is `noexcept` but allocates, so #24's `bad_alloc` handlers cannot catch it.
  `GetSelection` creates one provider per selected row without bound. (`uia-selection-semantics-17`,
  `uia-lifetime-threading-14`, `menu-layout-ux-11`)
- [ ] Low:
  - described rows build a layout per row with no row bound, twice after a DPI move;
  - the omitted-tail layout table forgets shared layouts;
  - zero-extent preparation keeps the Grid's layouts;
  - tree animation does linear id scans per row per frame.

  (`menu-layout-ux-12`, `-9`, `grid-multiline-5`, `gap-embedded-host-parity-6`, `tree-10`)

## Test infrastructure, tests and CI

Interactive lease (#48, #63):

- [ ] **Ctrl+C kills `DxUi.InteractiveLease.exe` through the PowerShell pipeline before it restores the desktop**
  **[lead]**. The lease runs as a redirected native command (`| Out-Host`, and its output is assigned), and pwsh kills
  those when the pipeline stops (`Tools/InteractiveRun.psm1:223`). Start it with `Start-Process -NoNewWindow -PassThru`
  and wait for it in `finally`. (`tooling-runners-1`, `test-support-infra-1`)
- [ ] **Consent and restore edge cases**:
  - a declined, timed-out or interrupted confirmation still moves the pointer and takes focus;
  - the pointer is recorded before the dialog, so every mouse-confirmed run blames a fixture;
  - the Start button's access key works without Alt;
  - the console-close grace is shorter than the lease's worst-case clean-up;
  - the watchdog does not cover a hang in process exit.

  (`test-support-infra-2`, `-3`, `-4`, `-8`, `-6`)
- [x] A plain `test.ps1` ran Menu and NativeTextInput without the lease. Fixed by #66: they left the default list, and a
  local run that names them without `-Interactive` is refused (`test.ps1:29`, `:56`). (`process-docs-specs-1`,
  `tooling-runners-2`)
- [ ] **The non-interactive NewControls lane still touches the desktop**: it moves the physical pointer
  (`TestMenuChoosesTheCursorWhenItOpensAndCloses`), and its activation-dependent description tests silently pass.
  (`tests-menu-a11y-host-1`, `gap-arm64-asan-runtime-4`, `tests-menu-a11y-host-6`, Q20)

UIA test clients:

- [ ] **Clients can hang or miss events**:
  - `UiaTestClient` subscribes desktop-wide without #63's hang protection;
  - `TryAsk` destroys the request functor while the client thread may run it;
  - `FocusEventClient` and four hand-written client threads stop listening after fixed 10-30 s, so late "no event"
    checks pass vacuously (A9).

  (`gap-arm64-asan-runtime-3`, `test-support-infra-7`, `test-support-infra-5`, `gap-arm64-asan-runtime-9`,
  `tests-menu-a11y-host-3`, `gap-uia-test-falsifiability-8`)

Tests that cannot fail:

- [ ] **Event and lifetime assertions**:
  - every selection-event test deduplicates what it hears, so a double raise (double Narrator announcement) cannot fail;
  - "no event" assertions rely on a 500 ms or 1 s quiet window;
  - embedded event tests check presence only and invalidate by hand;
  - callback-destroys-control tests assert things that hold after a use-after-free;
  - #60's replacement tests cannot see a stale `_host` in Release;
  - no test covers a focus callback destroying the control during native `SetFocus`, or a UIA action into a delegate
    that runs a nested loop.

  (`gap-uia-test-falsifiability-1`, `-2`, `-6`, `tree-16`, `tests-grid-tree-render-3`, `gap-uia-test-falsifiability-5`,
  `tests-grid-tree-render-4`, `gap-uia-mutex-nested-loop-matrix-5`)
- [ ] **Visual baselines**: their tolerance let #62's slider redesign through with no baseline update
  (`Tests/Controls/Controls.Tests.DxUiTestHelpers.h:567`) (`tests-grid-tree-render-1`, Q20).
- [ ] **Wall-clock races**: the touch-halo and hover animation tests (the flake #44 fixed for tooltips), the 500 ms
  focus-gain-turn limits, the disclosure client's `changes >= 2`, and the cursor test's early return that leaves an
  async menu open. (`tests-grid-tree-render-2`, `gap-arm64-asan-runtime-5`, `-2`, `tests-menu-a11y-host-2`, `-5`,
  `gap-uia-test-falsifiability-4`, `tests-menu-a11y-host-4`)

CI (#46, #49, #61):

- [ ] **CI counts capability skips as a pass**, so the ARM64 and ASan focus checks can go green without running. This
  matters more since #66: `test.ps1 -Full` on GitHub Actions (`test.ps1:51`) is now the only automatic run of Menu and
  NativeTextInput, and a suite with skips still prints PASS (`test.ps1:199`). Keep a per-lane expected-skip baseline
  and fail on new skips. (`gap-arm64-asan-runtime-1`, Q20)
- [ ] **No CI result can block a merge**. Verified on 2026-10-06: main's only ruleset ("main protect") has deletion,
  non-fast-forward and pull-request rules but no required status checks, there is no classic branch protection, and
  auto-merge is off. Even with protection, no check can require the native matrix: a documentation-only pull request
  reports it under an unexpanded name. Run 37489742358 shows two failing native jobs beside a green `validation`. A
  gallery commit pushed to main leaves main's head without a push run. (`ci-workflows-1`, `ci-workflows-2`, Q21)
- [ ] **Timeouts**: the ASan step budgets exceed the 40-minute job limit (6 minutes of headroom on ARM64), and the
  validation and format jobs have no timeout. (`ci-workflows-3`, `gap-arm64-asan-runtime-6`, `ci-workflows-8`)
- [ ] Low:
  - the native-scope "license" rule names `LICENSE` while the file is `LICENSE.txt`;
  - `Commit-Gallery.ps1` commits everything already staged;
  - nothing tests the docs-only-skip premise.

  (`ci-workflows-6`, `tooling-perf-gate-9`, `ci-workflows-5`, `ci-workflows-7`)

Benchmark gate:

- [ ] **Unmeasured inputs can make a real regression pass**:
  - the identical-library "noise" rule keys on a fingerprint that leaves out build and restore inputs (`DxUi.sln`,
    `build.ps1`, `vcpkg-configuration.json`, toolchain modules, the benchmark project), so a regression caused only by a
    build change can pass the PR gate;
  - the allocation counters live in `EmbeddedTests.cpp`, outside both the fingerprint and the paired overlay.

  (`tooling-perf-gate-1`, `gap-perf-evidence-pipeline-1`, `tooling-perf-gate-4`, `gap-perf-evidence-pipeline-3`, Q22)
- [ ] **Statistics and evidence handling**:
  - the rank test treats the 12 A,B,B,A runs as independent;
  - `Restore-HarnessOverlay` overwrites harness files without checking they are unchanged, and since #66 the overlay
    also writes the renamed fixtures under their old names into old revisions (`Tools/PairedRun.psm1:14`, `:139`);
  - `Publish-BenchmarkVerdict` without `-Reports` judges the newest `summary.json`, possibly a previous run's.

  (`tooling-perf-gate-2`, `-3`, `-5`, `gap-perf-evidence-pipeline-4`)
- [ ] Low:
  - the control-drift rule is implemented twice;
  - the paired run builds the whole solution twice;
  - receipts and paired worktrees are never pruned.

  (`tooling-perf-gate-7`, `-6`, `gap-perf-evidence-pipeline-5`)

Tooling:

- [ ] **Consumer-facing scripts**:
  - a relative `-Root` resolves against the process directory, not the PowerShell location;
  - `Validation.psm1` fails to import in Windows PowerShell 5.1, which breaks `validate-build-matrix.ps1 -Root <consumer>`.

  (`tooling-runners-4`, `tooling-runners-3`, Q23)
- [ ] **API-revision gate**:
  - the consumer-interface gate checks only that files exist;
  - it treats a new mandatory parameter as compatible;
  - the revision 3 migration list omits most of #29's renames (`RunModalLoop`, debug and `Typography` names).

  (`public-api-architecture-7`, `-8`, `-2`, Q24)
- [ ] Low:
  - `-Interactive -Tests` without `-Suites` takes the desktop for a run that must fail;
  - a lease-bound timeout is reported without the test name;
  - `Test-InteractiveMode` fails while other runs write logs;
  - `Test-AsanRuntime` leaves a directory per run;
  - `format.ps1` keeps its own vswhere discovery;
  - `Test-TestFilter` re-runs the Grid suite.

  (`tooling-runners-6`, `-5`, `-7`, `-8`, `-9`, `-10`)

## Scoped testing (#66, #67)

- [ ] **A plain `test.ps1` can run nothing and exit 0** **[lead]**.
  - With only `-Configuration`, `-Platform` or `-SkipBuild`, it hands off to `Test-Changes.ps1` in Affected mode
    (`test.ps1:39`). On a clean checkout of main (the README quick start, a pinned or shallow consumer checkout) that
    selects no scope, prints "No local test execution is required by this plan." and exits 0 (`Test-Changes.ps1:69`).
  - Selection is by changed path only, so a Visual Studio, Windows or driver update on a clean tree also runs nothing.
  - AGENTS.md's code-change requirement and `Core_PerformanceAndResources.md:257` still describe `test.ps1` as the gate
    that always benchmarks and runs tooling.
  - Fix: report NOT_EVALUATED with a non-zero exit when nothing ran, or make Affected an explicit switch, and update
    the documents.

  (`scoped-integration-2`, `scoped-engine-6`, `scoped-integration-3`, Q27)
- [ ] **PrePush can report coverage that nothing enforces** **[lead]**. It delegates (`Tools/ScopedTesting.psm1:224`):
  - native scopes to CI jobs that, by `ci.yml:59-62`'s own rule, no required check may name;
  - from main or a detached HEAD, where no pull request will run;
  - after checking the candidate's `ci.yml` digest, although a pull_request run uses the merge ref's workflow;
  - with only `ci.yml` bound, not the scripts that decide what CI runs (`NativeScope.psm1`, `test.ps1`);
  - InteractiveLease, whose confirmation and warning proofs may skip on hosted runners;
  - after checking the manifest's hard-coded repository, not the remote the push goes to.

  Fix: delegate only from a non-default branch, bind the CI-deciding scripts, compare with `origin/<base>`'s workflow,
  and keep native scopes local until one required aggregate check covers them (`ci-workflows-1`).
  (`scoped-engine-1`, `scoped-integration-4`, `scoped-engine-5`, `-7`, `-10`, `-13`, `-18`, Q21, Q28)
- [ ] **Scope rules miss inputs** **[lead]**:
  - `Tools/tests/**` selects only Tooling (`validate.ps1` and `Test-AsanRuntime.ps1`). But `Test-TestFilter`,
    `Test-TestWatchdog` and `Test-InteractiveLease` run only on `test.ps1`'s native path (`test.ps1:84-91`), so a
    change to them never runs them.
  - Files `validate.ps1` reads (gallery, Measurements, non-Markdown docs, Changes) select no scope.
  - The ComboBox fan-out omits NewControls, which tests TagPicker.
  - Three rules name files that do not exist, and nothing checks that rules match.
  - Edits to Menu or NativeTextInput tests fall back to "full" and print `FULL_NONINTERACTIVE_PASSED` without running
    their suite.

  Fix: a validator that every rule matches a tracked file and every fixture maps to a scope that runs it, and an
  explicit NOT_RUN obligation for the foreground suites. (`scoped-integration-1`, `scoped-engine-2`, `-3`,
  `scoped-integration-8`, `scoped-engine-9`, `scoped-integration-11`, `scoped-engine-14`, `scoped-integration-9`, Q29)
- [ ] **Reused evidence can be stale**:
  - **[lead]** A pass with capability skips becomes a reusable receipt, and the identity ignores desktop and session
    state. A run under a covered or locked desktop is reused later, when the skipped tests would run.
  - The environment identity omits the GPU driver, `d2d1`/UIA/TSF runtime DLLs, the OS build revision, fonts, DPI, the
    `DXUI_*` performance switches, and git/gh and their configuration.
  - Git output is decoded with the console code page, so a non-ASCII tracked path (none today) would silently leave the
    identity.
  - The start/end snapshot cannot see an input that changes and reverts during a run.

  Fix: never reuse a run that skipped, or require an equal skip set; widen the identity; decode git as UTF-8.
  (`scoped-integration-5`, `scoped-engine-4`, `-12`, `-17`, `-8`, `-16`, Q30)
- [ ] **"Full" is not CI's full**. `FULL_NONINTERACTIVE_PASSED` and `test.ps1 -Full` omit `test-consumer.ps1` (both
  variants), the MenuTextLayoutResources fixture and `gallery.ps1`. So a broken `Tests/ConsumerModules` source or
  `Build/DxUi.Consumer.props` is never compiled locally. (`scoped-integration-6`, `scoped-engine-15`)
- [ ] **CI**:
  - The required `validation` check can now be skipped. It needs `windows-tooling` with `if: !cancelled()`, and a run
    cancelled while `windows-tooling` runs reports `validation` as skipped, which GitHub counts as passing (plausible,
    Q31).
  - Nothing guards the CI-only Menu/NativeTextInput addition (`test.ps1:51`).
  - `GITHUB_ACTIONS` alone authorizes the focus suites without the lease, self-hosted runners included.

  (`scoped-integration-7`, `-13`, `-10`)
- [ ] **The renames made every retained pre-rename benchmark baseline unusable**. `benchmarkSha256` hashes paths, so
  `test.ps1 -PerformanceBaseline` with a pre-rename receipt fails "Unmatched fixture" for byte-identical fixtures; only
  `performance-paired.ps1` compares across the rename. The paired overlay's legacy aliases are hard-coded and invisible
  to its SkipBuild check. (`scoped-integration-14`, `-12`)
- [ ] `Test-Changes.ps1` says it does not touch the desktop, but scopes it selects (NewControls) move the physical
  pointer (`scoped-integration-15`; see the NewControls item above).

## API and contract consistency

- [ ] `Grid::RequestSelectRow`, `RequestRemoveRowSelection` and `RequestToggleCheckboxCell` return true for a grid that
  did not survive, unlike Tree's requests. UIA actions whose callbacks destroyed the element report success or
  `NOTSUPPORTED` instead of `UIA_E_ELEMENTNOTAVAILABLE`. (`public-api-architecture-3`, `grid-selection-lifetime-16`,
  `uia-lifetime-threading-15`)
- [ ] `GridColumnDesc::multiline` and `textAlignment` are public but never read. The Grid's text-layout cache internals
  live in `DxUi.h`. The menu-internal `transferNativeFocus` flag is on the public `ControlHost::SetFocusControl`.
  (`grid-multiline-8`, `grid-multiline-9`, `public-api-architecture-11`)
- [ ] **Remaining null-brush draws**: Grid, Scrollbar, Tree, Button and ComboBox still pass possibly-null brushes to
  Direct2D, the class #29 removed from the editor controls. (`controls-editor-theme-14`, `-6`,
  `cross-cutting-simplification-10`)
- [ ] Parent navigation builds a separate, non-canonical root provider, and replaced canonical roots are never
  disconnected (`uia-lifetime-threading-16`).

## Process and documentation

- [ ] **Stale WIP plans and indexes**:
  - [SliderTouchHalo](SliderTouchHalo_2026-10-04.md) still lists the merge as open.
  - [TreeReorder](TreeReorder_2026-09-21.md) and the Tree contract keep a row-straddle limit #38 removed.
  - [GridTextOverflow](GridTextOverflow_2026-09-21.md) describes the replaced V11 cache.
  - [CiRunScope](CiRunScope_2026-10-04.md)'s validation figure does not reproduce.
  - The Done index omits three plans.

  (`process-docs-specs-5`, `-6`, `tree-13`, `grid-multiline-10`, `process-docs-specs-7`, `-4`, Q25)
- [ ] **Stale public docs**:
  - docs/controls.md still calls described menus in progress, pending qualification;
  - `ThemeColors.h` documents the pre-#29 alert fallback.

  (`process-docs-specs-2`, `controls-editor-theme-9`, `public-api-architecture-9`)
- [ ] The reference embedded sample never sets `PointerEvent::device`, so the touch feedback #62 documents never
  appears there (`gap-embedded-host-parity-7`).

## Simplification and architecture proposals

Ordered by payoff. A1, A3 and A5 remove whole defect classes listed above rather than individual sites.

- [ ] **A1. Make accessibility publication non-reentrant.** Today `RefreshAccessibilitySnapshot()`, a `const noexcept`
  method, raises UIA events synchronously. #60 assumes those can dispatch messages that destroy `this`, so every caller
  needs a lifetime check, and many still lack one (ComboBox 9 calls, TextField 7, TabControl, the UIA actions).
  - Queue the publish and raise it from the host once the message turn ends (window host) or in `UpdateAccessibility`
    (embedded). Then no control method can be destroyed by its own publish, and about two dozen guards added by
    #57/#60 become unnecessary.
  - The same queue lets the host skip building snapshots while no client listens and build on the first
    `WM_GETOBJECT`, which removes the per-keystroke whole-window rebuild.
  - Decide with Q3 and Q4.
- [ ] **A2. One selection-commit helper per selector.** Grid repeats the "copy previous, mutate, compare, delegate,
  check lifetime, publish, check lifetime" sequence 9 times and Tree about 6. The copies have drifted: `ApplyGroupLayout`
  never publishes, and `ToggleCheckboxCell` takes its token late. Add `[[nodiscard]] bool PublishSelectionChange(previous)`
  and a `Survives(f)` helper on `Control`: about -150 lines. (`cross-cutting-simplification-6`, `grid-selection-lifetime-14`,
  `tree-11`, `tests-grid-tree-render-5`, `grid-selection-lifetime-15`)
- [ ] **A3. Give Grid a focused row separate from its selection**, as Tree has since #35, using Tree's
  Replace/Range/Toggle/FocusOnly modes. One change fixes:
  - Shift+Up and refresh reordering;
  - `HasKeyboardFocus` on every row and the Ctrl+A focus element;
  - UIA `SetFocus` replacing the selection;
  - the missing focus indicator and the deletion fallback;
  - the documented Ctrl+Up/Down quirk.

  Grid and Tree then share one gesture contract. (`grid-selection-lifetime-21`, Q12, Q13)
- [ ] **A4. One `RunOnWindowThreadAction` helper (resolve under the lock, act unlocked, revalidate, map to
  `UIA_E_ELEMENTNOTAVAILABLE`) for all eleven UIA actions.** Then replace the process-wide `recursive_mutex` with a
  per-target non-recursive lock: snapshots are already atomic per target, and the recursion hides same-thread
  reentrancy. (`gap-uia-mutex-nested-loop-matrix-4`, Q5)
- [ ] **A5. Make tree mutation ownership-safe.**
  - Add `Panel::TakeChild(index)`, which clears `_parent` and `_host`, and stop documenting raw moves out of
    `GetChildren()`.
  - Have the host defer destruction of a replaced root or removed subtree to the end of the message turn. Callbacks
    then cannot free a control while a caller still holds it, which closes the P0 callback class by construction
    instead of site by site.
  - Also have `SetFocusControl` take the requested lifetime before it prunes.

  (lead proposal, Q1, Q2)
- [ ] **A6. One focus rule for both hosts.** Compute `SameFocusedElement` for embedded targets too, and raise the event
  on the snapshot's focused fragment (the item or row `GetFocus` reports), gated on placement focus. Remove the per-record
  focus event from `RaiseEmbeddedAccessibilityChanges`. (`gap-embedded-host-parity-1`)
- [ ] **A7. One contrast resolver in `Theme.cpp`.** Fix `ChooseContrastingTextColor` with the WCAG helper #29 added,
  and resolve the unfocused-selection text and the selected-row focus ring once for Tree and Grid.
  (`gap-visual-modes-hc-rtl-dpi-6`, `-2`, `-1`)
- [ ] **A8. One cross-thread call primitive in `Support/PostedPayload.h`.** #47's `MenuDebugDispatch` re-implements the
  accessibility UI-action Pending/Taken/Abandoned handshake. Use the same primitive for `AccessibilityCreateProvider`,
  and make the registry capacity a diagnosed limit. (`cross-cutting-simplification-9`, `menu-loop-lifetime-10`, about -60
  lines)
- [ ] **A9. One UIA test client.** Move `FocusEventClient` and the four hand-written client threads to
  `UiaTest::Client`. Give it #63's hang protection, and record raw (not deduplicated) event counts.
- [ ] **A10. Smaller deduplications (behavior-preserving)**:
  - `MixAccessibilityHash` duplicates the layout-key hash;
  - `PaintedOver` duplicates `CompositeOverBackground`;
  - a private `ControlBelongsToBranch` and `RevalidateScrollPanelChild` duplicate `IsControlInTree` and
    `RevalidateDispatchedControl`;
  - four id-set membership shapes exist;
  - six copies of the target-release lambda sit beside manual `Release()` calls (AGENTS.md: no manual Release);
  - the two `ComputeItemLayoutMetrics` overloads repeat the row layout;
  - the menu visible-height clamp is written three times, and a dead branch remains;
  - `Show` duplicates `EndAsyncMenuInteraction`;
  - the text store has duplicated tree walkers and a dead LRU branch;
  - the control-drift rule is written twice;
  - `format.ps1` keeps its own vswhere discovery.

  (`cross-cutting-simplification-7`, `-8`, `-11`, `gap-visual-modes-hc-rtl-dpi-7`, `tree-12`, `menu-layout-ux-8`,
  `menu-loop-lifetime-12`, `text-input-19`, `tooling-perf-gate-7`, `tooling-runners-9`)

## Questions for the developer

Lifetime and architecture:

1. **Q1.** Is promoting a descendant of the current root through the same host's `SetRoot` supported? The
   `GetChildren()` comment says so. The fix is needed either way, but the answer decides whether A5's `TakeChild`
   replaces raw moves.
2. **Q2.** Should DxUi controls survive *any* callback destroying them (selection, checkbox, group, text-changed, blur,
   context menu), or should docs/controls.md say every rebuild must be posted? Today only the selection delegate may
   rebuild synchronously. A5's deferred destruction would make the answer "always".
3. **Q3.** Is there measured evidence that `UiaRaiseAutomationEvent` dispatches window messages on the raising thread
   (non-COM-threaded providers, out-of-process clients)? The #60 guards, and several "plausible" items above, rest on it.
   A1 makes it moot.
4. **Q4.** Is the listener-independent, whole-window snapshot rebuild on every keystroke, focus and selection change an
   accepted cost, or should building wait for a client (A1)?
5. **Q5.** Was keeping the accessibility mutex across application callbacks in seven Execute* actions deliberate (unlike
   SetFocus and Invoke)? Are processes with DxUi hosts on several UI threads (RedSalamander viewers or plugins) supported?
6. **Q6.** When do consumers call `ShutdownAllWindowHostsForProcessExit`: only after every DxUi window is gone, or with
   windows and `ShowAsync` menus alive (`WM_ENDSESSION`, a tray Exit)?

Menus:

7. **Q7.** Should a synchronous `ContextMenu::Show` close on deactivation and `WM_CANCELMODE`, as `ShowAsync` and
   `TrackPopupMenu` do?
8. **Q8.** Should mnemonics follow the active layout's character (`WM_CHAR`/`WM_SYSCHAR`, as `NativeMenuBarHost` does),
   and should duplicate mnemonics cycle instead of invoking the first match?
9. **Q9.** Described menus:
   - should a menu with any described row always take the maximum width (456 DIP)?
   - should the description be the UIA `FullDescription`/`HelpText` instead of joined into `Name`?
   - should one row that fails to prepare fall back to plain rather than refusing the menu?
   - what is the largest described row count consumers will show?

Visual design:

10. **Q10.** Are Tree, menus, Grid and ProgressBar meant to mirror under `FlowDirection::RightToLeft`, or should the
    "not mirrored yet" limit be documented?
11. **Q11.** In high contrast, should application-supplied alert colors and the slider halo accent be honored, or should
    system colors be forced? Is the blended description color acceptable in HC?

Grid, Tree and UIA semantics:

12. **Q12.** Should Grid adopt Tree's focused-row model (A3): Ctrl+arrows move focus alone, Ctrl+Space toggles, and the
    current row has a focus indicator? Should deleting the selected rows move to the row that took their place?
13. **Q13.** Should Ctrl+click and Ctrl+Space move the range anchor (Explorer and ListView behavior) in Tree and Grid?
14. **Q14.** UIA selection contract:
    - `AddToSelection`/`RemoveFromSelection` on a single-select control: replace/clear, or `UIA_E_INVALIDOPERATION`?
    - UIA `SetFocus` on a tree item or grid row: notify the delegate? keep a multi-selection?
    - Should `Select` move keyboard focus?
    - Should "became exactly one new item" win over the more-than-20 rule, as in WPF?
15. **Q15.** Since #55, a window root that clients hold becomes "gone" whenever the window flips between one semantic
    control and several (a status label shown next to a single Tree). Intended?
16. **Q16.** For embedded hosts, should DxUi raise item-level focus events (A6), or does the application site own that?

Text input:

17. **Q17.** Text input policy:
    - Should IME commits be undoable?
    - Do real TIPs use the native `ITextStoreACP` path in a WindowHost, and has a real IME or the emoji panel been run
      against a scrolled multiline field?
    - For masked fields: refuse `WM_GETTEXT` from other processes, disable IME as `ES_PASSWORD` does, or keep IME with an
      `IS_PASSWORD` input scope?
    - Should the Win32 edit-message shim answer for a host attached to a top-level window at all?

Security and capacity:

18. **Q18.** What is the threat model for registered messages: are other processes in the session trusted? What is the
    largest number of simultaneously attached hosts plus open popups RedSalamander needs (the payload registry holds
    128)?

Grid multiline:

19. **Q19.** What clipboard format should multiline cells use (raw TSV, locked in by a #36 test; quoted TSV; or an extra
    CSV/HTML format)? How should keyboard-only and touch users reach a clamped cell's full value? Should the tooltip bound
    live in `TooltipLayer` (all controls) or the Grid?

Tests and CI:

20. **Q20.** Test policy:
    - Should the non-interactive NewControls lane keep tests that move the pointer or need activation?
    - Should CI fail on capability skips beyond a per-lane baseline?
    - What is the visual-baseline policy, and is the 2%-of-pixels budget a deliberate cross-GPU allowance?
    - Would you accept a clock seam for the 500 ms focus-gain turn?
21. **Q21.** Which checks does branch protection require today (any native job before #61)? Is auto-merge used? Is the
    gallery workflow meant to push to main directly?
22. **Q22.** Benchmark gate:
    - Should it correct for multiple comparisons?
    - Should it take its decision code from the base branch?
    - Should the "noise" downgrade use the scope step's changed-path categories instead of the library fingerprint?
    - The 2026-09-27 packet calls a +7,327,744-byte dirty multiline peak "within" an envelope the contract records as
      +7,012,352: which number is right?

Tooling, API and process:

23. **Q23.** Which PowerShell do consumers use for `validate-build-matrix.ps1 -Root`? Must the consumer-interface scripts
    stay Windows PowerShell 5.1-compatible?
24. **Q24.** Should the revision 3 migration notes be amended for #29's renames, and should CI enforce the revision rule
    (for example a frozen per-revision consumer compile fixture)?
25. **Q25.** Which WIP plans of the window close now? Should `capabilities.json` list tree multi-select, grid multiline,
    ordinary-menu UIA and the slider touch halo, and move described-menu qualification out of pending?
26. **Q26.** Was the slider touch halo checked on real touch hardware? Did #62 merge before its hosted paired benchmark
    verdict (its plan still lists it)? Is the halo painting over siblings outside the slider's bounds intended?

Scoped testing (#66, #67):

27. **Q27.** Should a plain `test.ps1` that ran nothing exit 0? Does anything outside this repository (consumer
    qualification, pinned or shallow checkouts) call a plain `test.ps1`, and is the README quick start meant to run
    Affected iteration? Should Affected also select scopes when the environment or toolset changed?
28. **Q28.** Is PrePush meant to run once per profile (six times)? How does an x64-only developer satisfy ARM64 PrePush?
    Should delegation work for stacked pull requests (base other than main), and be refused on main or a detached
    HEAD?
29. **Q29.** Scope rules:
    - Are the rules that match no file placeholders for a planned split, or leftovers?
    - Is TagPicker the only control composed in `src` from another control?
    - Should non-Markdown `docs/` and `Measurements/` files select Tooling?
    - Should the foreground suites get an explicit NOT_RUN obligation when their sources change?
30. **Q30.** Should a run that recorded capability skips be reusable? Do developer consoles use UTF-8 output? Should
    `DXUI_GRAPH_PERF` or `DXUI_PERF_JSONL_PATH` runs go through `Test-Changes.ps1`?
31. **Q31.** CI behavior:
    - On the hosted runners, does `DxUi.InteractiveLease.exe --self-test` skip its confirmation and warning proofs?
    - Are self-hosted runners used or planned?
    - After a manual cancel or a `windows-tooling` timeout, does GitHub report `validation` as skipped or as cancelled?
    - Is `windows-tooling` itself required? (See also Q21.)

## Recommended answers

Drafted on 2026-10-06 by seven researchers who read the code, the specs and the platform documentation, then checked by
the lead reviewer. The GitHub facts in Q21, Q26 and Q31 were confirmed against the live repository. These are proposals:
the developer decides. *Kind* is technical (evidence decides), product (a default is proposed) or process.

- **Q1** (technical, small). Yes, it is documented in three places, so it is supported.
  - Fix `SetRoot` now: keep the old root alive until after `Reparent`.
  - Then add `Panel::TakeChild`, which clears `_parent` and `_host` and keeps the inherited flow direction and density.
  - Deprecate the mutable `GetChildren()` span for one API revision.
- **Q2** (product, large). The target contract: any callback may destroy its control (not its host). Implement it
  structurally:
  - A5: removed subtrees are retired to the end of the message turn, and lifetime tokens expire on removal.
  - A1: publishes do not re-enter.
  - Keep checks at delegate calls (A2), because a rebuild can free the borrowed model or delegate.

  Until then, make docs/controls.md list exactly the callbacks that may rebuild synchronously (today: focus, and
  Tree/Grid selection), and fix the P0 sites.
- **Q3** (technical, medium). Unestablished: nothing in the repository measures it, and Microsoft's documentation is
  silent. Measure it with consent: count the messages dispatched on the raising thread during `UiaRaise*` with Narrator,
  NVDA and Accessibility Insights attached, on x64 and ARM64. Implement A1 regardless, because the publish already
  re-enters through its own `GetWindowTextW`.
- **Q4** (product, large). No.
  - Coalesce publishes to one per message turn, raised by the host (A1).
  - Build a window's snapshot only once a client has asked for it, synchronously on the first `WM_GETOBJECT`.
  - Measure today's per-keystroke cost on the complex-UI fixture first.
- **Q5** (technical, medium). Not deliberate: Invoke and SetFocus already release the lock, and their comments state
  the rule.
  - Convert all seven actions, plus text-range Select, through one A4 helper, then use a non-recursive lock per target.
  - Hosts on several UI threads are supported when every call stays on the owner thread; document it.
  - It is P0 if any consumer has hosts on several threads; otherwise the Narrator stall during nested loops still
    justifies it.
- **Q6** (product, medium). Make the sweep callable at any time, with windows and menus alive: `WM_ENDSESSION` does not
  require destroying windows. Before detaching, it:
  - closes each thread's async menus without callbacks;
  - re-checks registry membership per host;
  - leaves alone hosts whose menu loop is on the current stack.
- **Q7** (technical, small). Yes; it is a bug fix, because the header promises `TrackPopupMenu` behavior.
  - Close on deactivation and `WM_CANCELMODE`, handled in the popup window procedure (they are sent, so the loop's peek
    check never sees them).
  - Keep recapturing on bare capture loss.
- **Q8** (technical, medium).
  - (a) Yes: match `WM_CHAR`/`WM_SYSCHAR` characters, as `NativeMenuBarHost` does, and keep virtual keys for navigation.
  - (b) Yes: several matches move to the next match without invoking; only a unique match invokes; an explicit `&`
    mnemonic wins over a first-letter match.
- **Q9** (product, medium).
  - Size described menus to their content within 128-456 DIP. The row layouts already exist, so their metrics are free;
    `minRootWidthDip` stays for a fixed width.
  - Keep the description in Name, but join it with ", " instead of a newline. Expose it as HelpText only when a custom
    accessible name omits it.
  - A row that fails to prepare falls back to a plain row.
  - Bound the described rows per popup by the largest list consumers show (developer input).
- **Q10** (product, small).
  - Document now that Tree, described menus and Grid do not mirror.
  - Mirror ProgressBar now: it is cheap, and it contradicts the Slider beside it.
  - Do not half-mirror.
  - Open an RTL plan (menus, then Tree, then Grid) only when a consumer commits to an RTL UI language.
- **Q11** (technical, medium). Force system colors in high contrast, per Microsoft's contrast-theme guidance:
  - application alert colors become text on window background, with severity carried by an icon or text;
  - the accent and the halo use Highlight;
  - subdued text is WindowText, with no blends.
- **Q12** (product, large). Adopt A3 in full.
  - A focused row in `GridSelectionModel`.
  - Ctrl+arrows move focus alone, Ctrl+Space toggles, and Shift ranges to the focused row.
  - A visible focus indicator through A7.
  - After a deletion, focus moves to the row now at that index; if the selection became empty, that row is selected
    too.

  Check first that no consumer relies on Ctrl+Up/Down toggling the neighbour.
- **Q13** (product, small). Yes, in multi-select, the same in Tree and Grid. Ctrl+click and Ctrl+Space move the anchor to
  the item whether it was selected or deselected (the WPF rule). UIA Add/Remove do not move it.
- **Q14** (technical, medium). Follow the UIA SelectionItem contract, as WPF does:
  - AddToSelection with another item selected in a single-select container returns `UIA_E_INVALIDOPERATION`.
  - Single-select Tree and Grid report `IsSelectionRequired` true, and RemoveFromSelection of the selected item returns
    `UIA_E_INVALIDOPERATION`.
  - SetFocus notifies the delegate exactly as the keyboard would, and never destroys a multi-selection.
  - Select moves the control's current item, not keyboard focus.
  - "Became exactly one new item" wins over the more-than-20 rule.
  - A disabled control returns `UIA_E_ELEMENTNOTENABLED`.
- **Q15** (technical, medium). A defect.
  - Collapse only when the host's root control is itself the semantic control, or has the Status role. A Panel root
    never collapses, so siblings appearing or disappearing cannot kill the root.
  - Keep "gone" for `SetRoot` replacement only, and disconnect the old root then.
- **Q16** (technical, small). DxUi raises item-level focus events (A6). The application reports placement focus
  truthfully, calls `UpdateAccessibility`, and never raises events for elements inside the view. Document the split, and
  confirm RedXe's adapter does not raise them too.
- **Q17** (technical, medium).
  - (a) Yes: each committed composition, emoji insert or dictation phrase is one undo unit. Previews and selection-only
    imports never touch history, as on the embedded path.
  - (b) By code, TSF input methods take the native `ITextStoreACP` path, but no real-IME or emoji-panel run is recorded.
    Do one with consent and record it.
  - (c) Masked fields refuse `WM_GETTEXT`/`WM_GETTEXTLENGTH` for every caller. That is stricter than `ES_PASSWORD`,
    because DxUi's own publish reads the window text in-process. Disable text services while one has focus, as
    `ES_PASSWORD` does.
  - (d) No: stop answering `WM_GETTEXT`, `WM_GETTEXTLENGTH` and `WM_SETTEXT` for the host window (the new P1 item). Keep
    the `EM_*` and clipboard messages.
- **Q18** (technical, small). The trust model and capacity:
  - Same-user, same-integrity processes are not a security boundary (Microsoft's servicing criteria). UIPI already
    blocks lower-integrity senders, and DxUi never relaxes it.
  - Treat registered messages as a robustness matter: no raw pointers in parameters (`SendMessagePayload` with a
    timeout for CreateProvider), validated tokens, bounded waits, and refusal of a foreign-process HWND.
  - Drop the 128-window table, since entries record their window, and keep a diagnosed payload bound (A8).
- **Q19** (product, medium).
  - Copy Excel-style quoted TSV in `CF_UNICODETEXT`: quote fields with TAB, CR, LF or `"`. Add no CSV or HTML until
    asked, and rewrite the #36 test.
  - The full value is reachable through UIA, Ctrl+C and a consumer detail view; document that view as required for
    clamped cells. Add a keyboard tooltip after A3.
  - Bound tooltips in `TooltipLayer`.
- **Q20** (technical, medium).
  - (a) No. Split `RunMenuDescriptionTests` into a non-activating list for NewControls and an activating list for Menu,
    and make the pointer-alignment fixture require activation.
  - (b) Yes. Fail CI on any skip outside a checked-in per-lane baseline keyed by test name.
  - (c) A deliberate visual change regenerates its baselines in the same PR. Replace the 2% ratio with a per-channel
    tolerance plus a small absolute pixel cap, measured on the hosted GPUs and WARP.
  - (d) Yes. A per-host, test-only clock seam, as #44, keeping one real-time test.
- **Q21** (process, small). Today nothing is required, so nothing blocks a merge.
  - Add one always-running aggregate job (for example `ci-gate`) that fails unless `validation` and the native matrix
    succeeded, or native was skipped for a documentation-only PR. Make it the single required check.
  - Do not require matrix names or the path-filtered format check.
  - Make `validation` run with `if: always()`.
  - The gallery workflow refuses `refs/heads/main` and publishes on the PR branch.
- **Q22** (technical, medium).
  - (1) No family-wise correction yet: six runs per side cannot reach Bonferroni thresholds. Fix the A,B,B,A dependence
    first, then calibrate with A/A sets.
  - (2) Keep the gate code from the PR, whose workflow comes from the merge ref anyway. When a PR changes the gate, also
    run the base revision's verdict on the same summary and fail on disagreement.
  - (3) Downgrade to noise only when the fingerprints match *and* every changed measured path is harness, gate or
    workflow. Move the allocation counters into an overlaid, hashed header.
  - (4) Both numbers are right but measure different metrics: +7,012,352 is the median private-bytes envelope, and the
    dirty peak was never enveloped. Correct the packet's "within", and give the contract one figure per metric.
- **Q23** (product, small). Declare the PowerShell version per consumer-interface entry.
  - Keep `vcpkg-install.ps1` and its modules 5.1-compatible; RedSalamander runs them under 5.1.
  - Declare `validate-build-matrix.ps1` PowerShell 7.2+, with `#Requires` or a re-launch under pwsh, rather than porting
    `Validation.psm1`.
  - Fix the relative `-Root`.
- **Q24** (process, medium). Yes to both:
  - amend the revision 3 migration list with #29's full renames, through a new Changes fragment;
  - enforce the rule with a frozen per-revision consumer compile fixture seeded from what consumers use, plus
    mandatory-parameter detection.
- **Q25** (process, small). Close the library side now, and keep consumer pin handoffs in one tracker.
  - Close MenuDescriptions, GridTextOverflow and TreeReorder after fixing their stale text.
  - Close SliderTouchHalo (its hosted paired benchmark passed before the merge) and CiRunScope after recording their
    run ids.
  - Move CodexBranchReview's delegate audit here (A5).
  - In `capabilities.json`, list tree multi-select, grid multiline, ordinary-menu UIA and slider touch feedback, and move
    described menus to supported. If "supported" means safe to pin, wait for the P0 fixes.
- **Q26** (process, small).
  - No real-touch check is recorded. Run a short one on a touch device, or record "synthetic input only".
  - #62 merged at 12:50 on 4 October after `paired-benchmark (pull request)` passed at 12:28.
  - The halo's overlap is intended and documented; keep it.
- **Q27** (product, small). No: a plain `test.ps1` must never exit 0 having run nothing.
  - Hand it to `Test-Changes.ps1 -Mode Full`: reuse keeps it cheap, since unchanged scopes report REUSED.
  - Keep Affected iteration under `Test-Changes.ps1`, and label an empty plan NOTHING_SELECTED, NOT_EVALUATED.
  - No consumer calls DxUi's plain `test.ps1`.
- **Q28** (product, medium).
  - One PrePush call covers every PR profile.
  - Native scopes are delegated only once a required aggregate check (Q21) is confirmed through the rules API.
  - Otherwise they run locally, and ARM64 on an x64 host cross-builds and reports ARM64 runtime NOT_RUN, non-zero.
  - Use the PR's real base for stacked PRs.
  - Refuse to delegate on the default branch, on a detached HEAD, or to another remote.
- **Q29** (technical, small).
  - The dead rules are leftovers: repoint MenuExitLifetime to `DxUi.Tests.Menu.cpp` and delete the Tooltip/TextField
    patterns.
  - Check that every rule matches a tracked file and every fixture maps to a scope that runs it.
  - Add NewControls to the ComboBox rule; TagPicker is the only composition the rules care about.
  - Every non-`.build` change selects Tooling.
  - Menu and NativeTextInput sources yield INTERACTIVE_NOT_RUN, never FULL.
- **Q30** (technical, small).
  - Reuse a run that skipped only when its skip set equals a reviewed per-lane set. NewControls records the same nine
    skips in every x64 profile here, so "never" would never reuse.
  - Consoles here use OEM 850: decode git as UTF-8, and record missing paths instead of dropping them.
  - Performance and mutation runs use `test.ps1` or `performance.ps1`, and `Test-Changes.ps1` hashes every `DXUI_*`
    variable.
- **Q31** (technical, small).
  - (a) The hosted lease self-test does not skip: no SKIPPED line in 18 jobs. Remove the CI tolerance.
  - (b) No repository self-hosted runners exist. Also require `RUNNER_ENVIRONMENT=github-hosted` for the CI-only focus
    suites.
  - (c) A cancelled run reports pending dependants as cancelled (run 37203429227), so the skipped-`validation` case is
    unlikely; `if: always()` removes it anyway.
  - (d) Nothing is required today. Require the single aggregate check (Q21), not `windows-tooling`.

## Sequence

1. **P0 point fixes** that are small and independent of the questions: `SetRoot` order, the pointer-down revalidation,
   the checkbox/group tokens, the clear button and `ImportTextInputState` liveness, closing menus before the exit sweep,
   and the timer-id and Tree first-key fixes. Each comes with an ASan test that fails at `49a9988`.
2. **Decide Q1-Q5**, then implement A1/A4/A5. These replace the remaining P0 callback items and the mutex item
   structurally rather than site by site. Measure the publish change on the complex-UI and grid-selection benchmarks
   against a retained baseline.
3. **Keyboard, Grid (A3), menu and text-input defects (P1)**, then accessibility. Exercise with a real UIA client
   (Narrator or Accessibility Insights) where the tests cannot.
4. **Visual accessibility (A7)**, with the gallery and design system regenerated through `gallery.ps1 -PublishDocs`.
5. **Tooling, tests and CI**: lease Ctrl+C, skip baseline, gate fingerprint, consumer-interface gate, and the scoped-testing
   items (an empty plain run, PrePush delegation, rule coverage, reuse identity) once Q27-Q31 are answered.
6. **Documentation and plan close-out**, then move this plan to Done.

Validation follows AGENTS.md:

- `validate.ps1` and `format.ps1 -Check`;
- `test.ps1` in x64 Debug, Release and ASan Debug, and builds in the three ARM64 configurations;
- the focus suites through `test.ps1 -Interactive` only with the person's agreement;
- paired benchmark evidence for the publish, selection and text-layout changes;
- gallery regeneration for visual changes.

Consumer adoption (RedXe, RedSalamander) stays separate.
