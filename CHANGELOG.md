# Changelog

New entries start as fragments under [Changes](Changes/README.md) and are folded in here, newest first.

## Unreleased

- A Tree's selection that becomes one new item while a previously selected item leaves the tree (removed, or hidden by a
  collapsed ancestor) now raises `ElementSelected` on the new item, as a Grid already did for a row out of view, instead of
  one `Selection_Invalidated` of the tree: the new item's event says that the others left the selection, so the item that has
  no element needs no event of its own. A change that leaves no such item, or that does not end in one new item, is
  reported as before. One condition of `CollectSelectionChanges` (`DxUi.Accessibility.cpp`) no longer treats trees apart.
  - **Tests.** The Accessibility suite's single-selection tree step that removes the selected item while the application
    selects another now hears `ElementSelected` and `IsSelected` of the new item; the multi-select tree's new step (two selected
    items, one leaving the tree as the application selects a third) hears the third selected and the other's `IsSelected`
    change. Both failed against the library before this change, which raised the tree's invalidation.
  - Specified in `UI_InputAndAccessibility.md` (Selection events) and `Testing_Validation.md`; `docs/controls.md` and the
    Tree plan follow. API revision stays 3. Nothing visual changed.
- A Tree with a single selection and a Grid raise UI Automation's selection events, as a multi-select Tree already did, so a
  screen reader hears a list's selection move whatever kind of list it is (the developer's decision of 2 October).
  - **What a client hears.** Every change of a tree's or a grid's selection, from a click, a key, a UI Automation request or
    the application's setters (silent to the delegate, not to clients), raises the SelectionItem `IsSelected` property change of
    each item or row whose state changed; `ElementSelected` on the one that became the whole selection; otherwise
    `ElementAddedToSelection` and `ElementRemovedFromSelection` on each that joined or left; and, past 20 changes, one
    `Selection_Invalidated` on the control instead (the multi-select tree's threshold). An item that left the selection and
    has no element is also an invalidation: a tree's item that left its rows, a grid's row out of view (a grid's rows are
    virtualized: a snapshot holds those on screen and the selected ones) or removed from the model, except that a row out of
    view needs no event when one row became the whole selection, since that row's `ElementSelected` says the others left.
    The events come from the item's own element and the invalidation from the control's, which is the window's when a
    collapsed root stands for the control; window hosts raise them after the focus announcement of the publish, embedded
    hosts from `UpdateAccessibility`. A multi-select tree raises what it did, and turning its multi-select off now removes
    the items it drops.
  - **How** (`DxUi.Accessibility.cpp`). The diff a publish made of multi-select trees (`CollectSelectionChanges`, was
    `CollectTreeSelectionChanges`) compares the selected ids of every tree and grid: the set, a single selection's one item, a
    grid's selected rows. It stays in publishing and grows no per-publish work: an unchanged selection costs a comparison of
    its ids and no allocation; sizes that differ by more than 20 are an invalidation before an id is compared; otherwise only
    the ids between the two selections' common start and end are compared (scanned up to 16, else one sorted copy a side),
    and collecting stops at the 21st change. It no longer builds hash sets of both selections and of every row of the tree.
    `RaiseSelectionEvents` (was `RaiseTreeSelectionEvents`) raises a tree's items and a grid's rows from their own providers.
    Something can run while it raises (an outgoing call of UI Automation's in a single-threaded apartment dispatches
    messages): it now ends once the host is disconnected or an embedded view's root is gone, as before, and also once the
    control is hidden, removed or replaced, so no event reaches an element that is gone. An embedded host's selection diff is
    made by the publish, as a window host's is.
  - **Tests.** The Accessibility suite drives each change through the window's own messages (no foreground), UI Automation's
    path or the setters, and requires the in-process client to hear exactly the expected selection events of each step, for
    a control that fills its window and for its twin beside a button: a single-selection tree's click, Up, setters, clearing,
    and items leaving the tree; a grid's click, Down, Ctrl+click, Shift+click, clearing, Ctrl+A, exactly 20 and 21 changes,
    rows out of view and a selected row leaving the model (the #51 tests that raised these events themselves now hear the
    library's). The multi-select tree's test now hears turning multi-select off and stays silent for 30 selected rows that
    moved. A diagnostics hook, `DebugSetAccessibilitySelectionEventHookForTest`, stands for what runs while events are raised:
    hiding the grid, replacing the window's root or detaching the host from it ends the raising after that event. The
    Embedded suite hears a view's single-selection tree and grid events (click, keys, setters, Ctrl and Shift clicks,
    clearing, Ctrl+A) and ends the raising when the view is hidden or its root replaced. Every one of these tests failed
    against the library before this change, and each of sixteen single-point mutants of it fails a named step (among them:
    grids or single selections left out of the diff, no `ElementSelected`, grid rows raised as tree items, the threshold at
    20, the common end of the two selections not trimmed, a row out of view named, the lookup of a large selection left
    unsorted, and each check that ends the raising).
  - Specified in `UI_InputAndAccessibility.md` (a Selection events section), `UI_ControlsAndLayout.md` and
    `Testing_Validation.md`; `docs/controls.md`, `docs/hosting.md` and the Tree plan follow. API revision stays 3 (no public
    declaration changed; the hook is an internal diagnostics function). Nothing visual changed, so the gallery and the design
    system are unchanged.
- Every row of a menu without descriptions is now a UI Automation element, as a described menu's rows already were, so
  a screen reader can read, focus and invoke the rows of any native menu (RedSalamander's destination-menu test found
  none in an ordinary menu). Ported from `codex/menu-description-layout` (`15be545`, `d45c361`): three conditions on a
  popup's descriptions are gone from `DxUi.Menu.cpp`. Commands are MenuItem elements with Invoke, radio and toggle rows
  report their checked state, information rows are text and separators stay out of the tree. A row the viewport scrolled
  away reports `IsOffscreen` with no rectangle until UIA focus scrolls it into view (`DxUi.Accessibility.cpp`), and the
  native focus a popup receives selects no row in any menu: it restores only a row the keyboard or UI Automation chose.
  - **Snapshot sizing.** A window host's accessibility snapshot now counts its records first, reserves its vectors once
    and makes each record in place (`65b0257` of the same branch, rebuilt on main), which helps every host and a long
    menu most: a Down at 4,096 rows takes 24% less (7.9 to 6.0 ms) and holds 0.97 MB less.
  - **Cost.** Six interleaved runs per side against main (x64 Release, `Measurements/PlainMenuUia/2026-10-02`): an open
    menu holds about 1.6 KB more live heap per command row and 1.9 to 2.0 KB per radio row (+19,967 bytes for twelve
    commands, +6.39 MB for 4,096), all returned on close. A key or wheel notch that moves the rows takes about 0.3 ms
    plus 1.4 to 2.6 µs per row more: Down takes 340 µs instead of 38 for twelve rows, 635 instead of 38 for 128 and
    6.1 ms instead of 38 µs for 4,096, where opening takes 77 ms instead of 60. Described menus are unchanged. The
    developer accepted this cost on 2 October 2026; `Core_PerformanceAndResources.md` records the envelope.
  - **Tests.** `TestPlainMenuAccessibilityInvokesAndDisconnects`, `TestPlainMenuAccessibilityScrollsFocusedRow` and
    `TestMenuNativeFocusSelectsNoRowAndRestoresTheChosenOne` join the described-menu group, which runs in the Menu and
    the nonactivating NewControls lanes.
  - Specified in `UI_InputAndAccessibility.md`; `docs/controls.md`, `Testing_Validation.md` and the `accessibleName`
    comment in `DxUi.h` follow. No API changes, and nothing visual changed.
- A window-host element can no longer act on a control that replaced its own, in the cases a review of the unmerged
  `codex/fileops-ui-qualified` branch found still open (its fix, rebuilt on main's element identity):
  - **The window's element.** While a single semantic control collapses into the window's element, that element is the
    control's element and is bound to it as every other element is to its control. Once the control is replaced or stops
    being the only one, a retained window element and an action already queued from it report `UIA_E_ELEMENTNOTAVAILABLE`
    instead of invoking the replacement (a queued `Invoke` from another thread ran the replacement's action), while its
    focus and point queries still answer that nothing is there (an embedded view's replaced root still reports every call
    gone); the window gives a newly acquiring client a fresh element.
    A window element acquired while no control collapsed into it is gone in the same way once one does.
  - **Focus callbacks that rebuild the controls.** An element's `SetFocus` and `Invoke` whose focus-changed callback
    rebuilds the controls report the element gone instead of success, and leave the callback's own focus choice;
    `Button::Invoke` with `focusSelf` no longer touches the button that callback destroyed.
  - **Runtime ids.** An element whose control is gone answers `GetRuntimeId` with `UIA_E_ELEMENTNOTAVAILABLE`, as every
    other call.
  - **Tests.** The branch's five native-lifetime tests, ported to the Accessibility suite, and a sixth for a window element
    acquired before its control collapsed into it. All but the queued text-range `Select`, which main already refused,
    failed before this change; each of the nine single-point reversions of the fix fails one of them.
  - Specified in `UI_InputAndAccessibility.md` and `Testing_Validation.md`; the plan is `CodexBranchReview_2026-10-02`.
- A multiline `TextField` draws its caret only inside its text viewport: a caret on a partly visible line was drawn past the
  viewport's edge, over whatever lies below the field. The caret's line is clipped to the viewport (`19ca44d` of the
  `codex/multiline-caret-viewport` branch). The Embedded suite's `TestMultilineCaretViewport`, from that branch, renders the
  caret on a WARP surface at 96, 144 and 192 DPI, editable and read-only, and requires no caret pixel outside the viewport;
  it failed before this change (20 caret pixels below a partly visible bottom line at 96 DPI). Specified in
  `UI_ControlsAndLayout.md`; `docs/controls.md` follows.
- A described native menu row holds one text layout instead of two (plan `MenuDescriptionMemory_2026-09-27`, the
  row-layout item). Its label, an empty spacer paragraph and its description are paragraphs of one DirectWrite layout
  built from the body format.
  - **Layout.** The description's range takes the small format's font (family, size, weight, style, stretch and locale,
    read back from the formats and applied where they differ, and the small format's tab stop for the layout, since only
    the description can hold a tab). The spacer's font size is set per row and width until its line is exactly
    `ceil(label height) - label height + 3` DIP tall, so the description starts 3 DIP below the label's height rounded up
    to whole DIPs, where a layout of its own was drawn. The heights and line counts are sums of the layout's line
    heights, which are bit-identical to what two layouts reported, and a row seeds the next row's spacer with the size it
    settled on, so rows whose labels are as tall are laid out once.
  - **Drawing.** One call draws the row: the label in its color and the description through a drawing effect on its
    range, which is one brush per popup and Direct2D device, recolored for each row just before its draw and recreated
    with the device.
  - **Memory.** A layout holds about 17 KB of shaping storage plus about 30 bytes a character, so the saving is per
    layout, and the popup-local sharing this replaces saved only where labels repeat. Whole-menu live heap, Release x64,
    `MenuResourceScaling` v5, open minus before, median of the last 16 of 32 cycles (the same in three runs of each
    build): rows with distinct labels 596,882 to 395,910 bytes for twelve rows (-33.7%), 1,096,836 to 695,752 for 24
    (-36.6%) and 2,100,287 to 1,298,940 for 48 (-38.2%); rows that all repeat one label, which main shared one layout
    for, 388,728 to 394,952 (+1.6%), 660,862 to 692,886 (+4.8%) and 1,208,673 to 1,292,258 (+6.9%). The developer
    accepted that cost for repeated labels on 2 October 2026 (the row-layout packet's option 1: real menus have distinct
    labels). A label has to repeat in about seven rows of one popup before sharing beats a layout per row, and a plain
    menu holds 16 bytes more (two pointers). In isolation (`MenuTextLayoutResources` v2, twelve rows) the native layouts
    hold 453,754 live bytes as pairs and 253,762 as one formatted layout per row.
  - **Pixels.** The 420 captures of a five-theme, eight-fixture, four-DPI matrix with keyboard, hover, wheel and scrolled
    states (emoji, Hebrew, Arabic, CJK, Thai, stacked marks, tabs, line breaks, an empty label, a long token) are
    byte-identical to main's in pixels and geometry, and so are all six gallery sheets, so `docs/gallery` needs no update.
  - **Tests.** `MenuResourceScaling` v5 adds the distinct-label menus (the earlier cases repeat one label in every row)
    and `MenuTextLayoutResources` v2 the one-layout mode. Six Menu-suite tests hold the row to the two-field contract
    against a separate layout measured in the test (heights, line counts, widths and the gap, through DPI changes), to
    its colors (enabled, disabled and hovered rows, and color glyphs), its tab stops, the layout count, the host's two
    formats agreeing on every property a layout holds for all its text, and the description surviving a lost device; ten
    mutants each fail a test, and the count test fails on main's two layouts.
  - **API.** `DebugSimulateContextMenuPopupDeviceLoss` and `ContextMenuPopupItemLayoutDebugState::descriptionOffsetDip`
    are additive diagnostics, so the API revision does not change; `rowLayouts` now counts one per described row.
- A control that focuses itself from an input handler no longer touches itself once the focus callbacks destroyed it
  (the control that lost the focus, its own focus handler and the host's focus-changed callback run inside
  `SetFocusControl`, and an application may rebuild the controls from any of them). The internal
  `FocusControlAndSurvive` gives a control the host's focus, first the window's when asked, and says whether the control
  outlived it. All 28 places where a library control focused itself now go through it, and the 25 that went on using
  the control stop when it is false: the presses, double clicks, context menus and mnemonics of Button, Toggle,
  RadioButton, PageIndicator, Slider, ColorSwatch, MenuBar, TabControl, ComboBox, Splitter, ColorPicker, TextField, Grid
  and Tree, a TabControl's tab keys, and removing the tab that held the focus. `Button::Invoke` uses it too. `TabControl::SelectTab` (private) now says whether the control
  outlived its focus and selection callbacks, so a tab press no longer captures the mouse for a destroyed control.
  UI Automation's Select and AddToSelection, and the focus of a grid row or cell (which selects it), now check that
  their grid or tree survived the selection's delegate before focusing it, and report `UIA_E_ELEMENTNOTAVAILABLE` where
  they focused a destroyed control before. `Grid::SelectRow` (private), which they share with a grid's press, double
  click and arrow keys, went on refreshing the destroyed grid after its delegate: it now stops and says so, and its
  callers stop too, as `Grid::RequestRemoveRowSelection` (RemoveFromSelection) and Ctrl+A now do. A tree's selection
  already stopped after its delegate. `docs/controls.md` says which callbacks may rebuild the controls, and which of
  Grid's delegate calls should post a rebuild instead.
  - **Tests.** `RequireFocusReplacementLeavesControlAlone` (`DxUiTestHelpers.h`) runs each of those handlers with a
    focus-changed callback that replaces every control, in the Control, EditorControls, ComboBox, TextField, Grid and
    Tree suites. `TestNativeAccessibilitySelectionDelegateReplacementStopsTheFocus` does the same with a grid's and a
    tree's selection delegate for UI Automation's actions, and `TestGridSelectionDelegateReplacementStopsTheInput` with
    a grid's for a click, Down and Ctrl+A. AddressSanitizer catches a handler that touched its destroyed control; each
    test fails so against main's library.
  - Specified in `UI_InputAndAccessibility.md`; `Testing_Validation.md` and the codex review plan follow. No API
    changes, and nothing visual changed.
- The described-menu memory plan moves to Done. Its waiver left the performance contract on 30 September, after paired
  sets within the bands, and one layout per described row merged as #37, after its pull request's hosted paired set found
  no regressed metric. CI's native ARM64 Debug, Release and ASan Debug jobs now run the Grid multiline-cell verification
  suites (workflow run 36951713344, no capability skip), so the Grid text plan, the test contract and the plan index no
  longer list that run as remaining.
- `vcpkg-install.ps1` restores with the Visual Studio installation and default MSVC toolset that MSBuild compiles DxUi
  with, not the newest toolset vcpkg finds. The development machine's VS 18 Insiders has MSVC 14.52.36725, which has no
  x64-hosted ARM64 compiler, beside the default 14.51.36231, so every fresh ARM64 restore failed configuring
  `wil:arm64-windows` (vcpkg chose
  `14.52.36725\bin\Hostx64\x64\cl.exe` for it and the compile test ended in `LNK1104` on `MSVCRTD.lib`), and a consumer's
  compiled vcpkg libraries could be built by a newer toolset than the one that links them.
  - The installation comes from the discovery `build.ps1` uses, now one function (`Get-DxUiVisualStudioInstallation` in
    `Tools/VisualStudio.psm1`, which `test-consumer.ps1` and `Test-AsanRuntime.ps1` call too), and the toolset from that
    installation's `VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`; nothing is hard-coded. A missing or
    malformed version file fails before anything is cloned, naming the file and the repair.
  - Each platform gets an overlay triplet, `<output root>\vcpkg-triplets\<platform>\<triplet>.cmake`
    (`Tools/VcpkgTriplet.psm1`): the pinned vcpkg checkout's triplet copied unchanged in its own line endings, then
    `VCPKG_VISUAL_STUDIO_PATH` and `VCPKG_PLATFORM_TOOLSET_VERSION` (major.minor), passed to `vcpkg install` as
    `--overlay-triplets`. It is rewritten only when its bytes change, so a patch update of the default toolset leaves it
    alone. vcpkg's package ABI hash changes with the triplet, so the first restore after this change rebuilds the
    packages (WIL here, built in 5-6 s). In an ARM64 restore on an x64 host the host triplet's tool ports keep the stock
    triplet; they run during the restore and are not linked.
  - A fresh ARM64 restore and a fresh x64 restore now succeed with vcpkg's own `Compiler found:` line naming
    `14.51.36231\bin\Hostx64\arm64\cl.exe` and `...\Hostx64\x64\cl.exe`. The interface (`-Platform`, `-OutputRoot`) is
    unchanged, so a consumer that runs the pinned checkout's script (RedXe, RedPrism and RedSalamander do) needs no
    change, and the script still parses in Windows PowerShell 5.1, which RedSalamander runs it with. A consumer's own
    installer for compiled libraries needs the same pins. `Tools/tests/Test-VcpkgTriplet.ps1` covers the fixture
    installations, the overlay text, the write-only-on-change rule and the wiring; each of 26 throwaway mutants (dropping
    either pin, losing the stock text, always rewriting, a second discovery, a new installer parameter, and more) fails it
    (`Build_ToolchainAndConsumption`). A nonvisual tooling change: no gallery image or design-system preview changes.
- A `Tree` row that straddles the viewport's top or bottom edge no longer paints past the tree's frame. Before, rows
  were culled only when wholly outside the viewport, so a straddling row was drawn whole. A selected row half scrolled
  away by a thumb drag painted its fill up to a row height over the frame and the controls above, and a reorder marker
  on such a row escaped the same way. Rows, including those an expansion animates, are now clipped to the viewport, and
  the marker to 1 DIP past it, so an insertion line on the edge keeps its width. Wholly visible rows are unchanged.
  The gallery's Tree / Hierarchy tile, whose viewport cuts its last row, loses one pixel row of that row's badge edge
  that used to paint into the frame's inset. `TestTreeRowsStraddlingTheViewportEdgesPaintOnlyInsideIt` fails without
  either clip.
- `Tree` selects several items (plan `TreeReorder_2026-09-21`). `Tree::SetMultiSelectEnabled(true)` is opt-in and off by
  default: a tree that never enables it keeps its single selection, its modifier-blind clicks and keys, its pixels and
  its callbacks, and every existing Tree, Accessibility and Control test passes untouched. On, the selection is a set held
  in `GridSelectionModel` (Grid's class, so its anchor and gestures): click selects one row and is the anchor, Ctrl+click
  toggles, Shift+click selects the visible range from the anchor (across group boundaries), Shift with Up, Down, Home,
  End, Page Up or Page Down extends it, Ctrl with those keys moves the focus alone, Ctrl+Space toggles the focused
  row (plain Space still invokes it, and the space character of Ctrl+Space is not typeahead) and Ctrl+A selects every
  visible row; Left and Right expand or collapse the focused group without touching the selection, and where they move
  to the parent or the first row of a group they follow the same Shift and Ctrl rules; a right-click on a selected row
  keeps the selection, an expander click leaves it alone, and a double-click with Ctrl or Shift only activates. The focused item (`GetSelectedItemId`, new `GetFocusedItemId`, what the focus ring
  and the keys use) and the set (new `GetSelectedItemIds` in visible order, `IsItemSelected`) are distinct; the new
  setters `SetSelectedItemIds` and `SetFocusedItemId` are silent like `SetSelectedItemId`. `ITreeDelegate` gains
  `OnTreeSelectionSetChanged(std::span<const uint64_t>)` (default no-op): once per change of the set, after
  `OnTreeSelectionChanged` of the same gesture, never for an unchanged selection (a click on the one selected row, Ctrl+A
  on a full selection, rows that only moved), with the tree's state already final so a delegate may read, change or
  destroy the tree. `NotifyDataChanged` and `SetModel` keep the selected ids that are still visible rows, in the
  model's order, and report the rest as one change; a collapse that hides selected rows deselects them, as it did a
  single selection. Every selected row paints with `selectionFill`/`selectionText` (`selectionInactiveFill` while
  unfocused) and only the focused row draws the focus ring, selected or not (`TreeDebugRowVisualState::current`).
  Drag-reorder keeps its single source: a press with Ctrl or Shift never drags, a plain press on a row of a multi-selection
  keeps the selection while the pointer is down so the delegate can move `GetSelectedItemIds` when the dragged row
  belongs to it, and a click that never became a drag collapses to that row on release (Escape and capture loss leave
  the selection alone). UI Automation: the Selection pattern reports `CanSelectMultiple` and lists every selected item,
  each item reports `IsSelected`, only the focused item reports `HasKeyboardFocus`, `Select`/`AddToSelection`/
  `RemoveFromSelection` change the set through the same callbacks (`AddToSelection` adds and never toggles, and
  `SetFocus` moves the focus without selecting), and a publish that changes the selection raises `ElementSelected` (one new item),
  per-item `ElementAddedToSelection`/`ElementRemovedFromSelection` with the `IsSelected` property change, or one
  `Selection_Invalidated` (over 20 items, or a selected item left the tree), for window hosts and embedded hosts; a tree
  without multi-select reports and raises what it always did. Fourteen Tree, two Accessibility and one Embedded test
  cover it, and the gallery gains a `Tree / Multi-select` tile. The API revision does not change (additive: `Tree::SetMultiSelectEnabled`,
  `MultiSelectEnabled`, `GetSelectedItemIds`, `IsItemSelected`, `SetSelectedItemIds`, `SetFocusedItemId`,
  `GetFocusedItemId`, `RequestAddVisibleItemToSelection`, `RequestRemoveVisibleItemFromSelection`, `OnSelectAll`,
  `ITreeDelegate::OnTreeSelectionSetChanged` and the diagnostics field `TreeDebugRowVisualState::current`; private
  members changed). Known limit, unchanged: in a window whose only semantic control is the tree, UI Automation does not
  deliver events raised on its items (their parent chain ends at the tree).
- The Tooltip suite's timer fixtures no longer depend on wall-clock timing.
  - **The failure.** `TestTooltipLayerHideDelayExpiresAfterTimerTicks` scheduled the default 100 ms hide, pumped messages
    for 50 ms of wall time, and required the tooltip to be visible. A CI runner that stalled overran that margin and failed
    the check, on a pull request that did not touch tooltips.
  - **The cause.** The deadline is on the animation dispatcher's clock.
    - While the dispatcher is idle, that clock reads the wall clock, and its first tick restarts it from the wall clock. A
      stall between scheduling and that first tick therefore put the clock past the deadline at once. A 150 ms stall
      injected there reproduces the CI failure.
    - Once the dispatcher is running, each tick moves the clock by the time since the previous tick, clamped to 50 ms.
  - **What the hide-delay fixtures do now.** They keep the dispatcher ticking with a subscription of their own, take the
    deadline from its clock, and dispatch one message at a time. Each tick is therefore observed with the state it left.
  - **Hide delay.** With a delay of twice the clamp, at least one tick lands before the deadline. The tooltip must be visible
    after each such tick, and hidden after the first tick at or past the deadline.
  - **Pointer move.** The move-cancels-hide fixture moves the tooltip one tick into the delay, then requires it to stay
    visible once the clock passes the old deadline. Before, a stalled runner let it pass without testing the cancel.
  - **Passive tooltip lifetime.** The old check never tested the five-second display lifetime: with the old check in place, a
    6 s lifetime still passed.
    - It ran after a click that had already hidden the tooltip: mouse-up releases capture, and losing capture clears the
      tooltip.
    - Its window was never shown, and a host ticks its tooltip only while its window is visible, so the tick did nothing.
    - Even on a visible tooltip, its tick at `GetTickCount64()` plus 6 s mixed the wall clock with the dispatcher's. On a
      machine whose mouse hover time is about a second or longer, that tick lands before the lifetime ends.
  - **What the passive fixture does now.** It shows its window without activating it, and checks the lifetime before the
    click. The tooltip is shown with a tick past the longest show delay (2.5 s), and the lifetime is checked exactly from
    that tick: visible at 4,999 ms, hidden at 5,000 ms. The rest of the fixture is unchanged, apart from a message that now
    says the click hid the tooltip, not the lifetime.
  - **No new hang risk.** A ten-second limit ends a run whose ticks never come. Library behavior is unchanged.
- Grid selection membership no longer scans a large selection, and a selection model no longer keeps the room of one it has
  dropped. `GridSelectionModel::IsSelected` was a linear search of the ordered ids and a `Grid` asks it once for every visible
  row on every paint, so after Ctrl+A on a long list each repaint searched the selection 24 times (0.9 to 1.4 us a call at
  20,000 selected rows, 47 to 94 us at 1,000,000); a pending Tree multi-select change reuses the model. The model now keeps
  its ids twice, in selection order (what `GetOrderedSelection` returns) and ascending beside it, and answers a selection of
  up to 1,024 ids by the scan it always used and a larger one by a binary search of the ascending copy, which allocates
  nothing. What a selection holds is unchanged: the order of `GetOrderedSelection`, the anchor rules of `Toggle` (the first id
  of the selection order when the anchor leaves), `SetRange` (the id it started from) and `PreserveOrdered` (the first id
  kept), an id a list gives twice (held twice), and the exception contract (`Clear`, `SetSingle`, `Toggle` and the accessors
  stay `noexcept`; `SetRange` and `PreserveOrdered` leave the selection as it was when an allocation fails).
  - The limit is 1,024, not the 16 to 64 that a first measurement suggested. That measurement asked the same 32 ids on every
    pass, so a processor learned which way each comparison of a search went and the search looked 2 to 5 ns at any size; its
    0.7 to 2 ns slowdown below 64 ids belonged to the two executables (built alike, the sorted copy is never slower than `main`
    in that order). The ids of the rows on screen, asked of a selection of hashed ids, come in no order a processor can learn,
    and a search then costs 5 to 20 ns more than the scan at 8 to 128 selected ids (a miss at 16 ids: 4.4 ns on `main`, 18.6 on
    the sorted copy alone). The two meet at about 500 to 650 ids; 1,024 is the first power of two beyond them, so that no size
    is slower than the scan it replaced. In three builds of ten runs per side (`main`, the sorted copy alone and this; x64
    Release, WARP; run as A, B, C, C, B, A), of 82 per-call comparisons at up to 1,024 ids against `main` 80 are within noise
    and 2 read slower at the 5% rate, and all 16 above 1,024 ids are faster (1,500 ids: -90% in the order a processor
    predicts, -44% to -59% in random order; 1,000,000 ids: -100%, 14 to 16 ns, or 149 ns in random order, against 49 to 94 us).
  - A Grid's paint no longer grows with its selection: Prepare takes 22% less at 200,000 selected rows and 64% less at
    1,000,000 (27% to 30% and 70% to 73% in the first set), flat at 0.5 to 0.9 ms, and what `IsSelected` costs inside a paint,
    isolated by painting alike with a small and a full selection, falls from 0.05, 0.35 and 1.6 ms at 20,000, 200,000 and
    1,000,000 rows to nothing measurable; a paint allocates as often as before, and no paint metric differs from the sorted
    copy alone.
  - A buffer with room for more than 4,096 ids (32 KiB) is given back when the ids that replace its contents need at most half
    of it: `Clear` and `SetSingle` swap it with an empty vector (no allocation in a Release build, so both stay `noexcept`),
    `SetRange` makes both copies beside the old ones and swaps them in, a `PreserveOrdered` that drops ids keeps no room it
    reserved for a selection that shrank, and `Toggle` leaves its room. After Ctrl+A over 200,000 ids a model held 3.2 MB (twice
    what the ordered ids alone held); it holds 0 bytes after `Clear`, 16 after a click on a row and 1,600 after a Shift+click 99
    rows away, and a selection of 4,096 ids keeps its room. Giving the room back costs 0.1 to 0.2 ms and getting it again
    0.3 ms at 200,000 ids (1 to 2 us at 20,000), against 0.4 to 11 ms for the Ctrl+A itself.
  - `PreserveOrdered`, which `NotifyDataChanged` calls on every data change of a Grid with a selection, asks about every row of
    the model. The sorted copy made it cheaper than the hash set it replaced when all or half of the rows are selected (0.70 ms
    instead of 1.45 for 20,000 ascending ids, 8.2 instead of 41.9 for 200,000), but dearer for a modest selection in a long list
    with hashed ids: 200,000 rows with 16 to 5,000 ids clicked took 2.4 to 11 ms where `main` took 1.2 to 2.8. It now asks a
    table of bits (16 to 32 per selected id) first and searches only the rows it lets through: 0.2 to 1.1 ms for 3 to 5,000
    ids, with 2 or 3 heap calls where `main` made 5 to 5,008 and the sorted copy alone 1 or 2.
  - The costs: 8 bytes per selected row (the model grows from 40 to 64 bytes); `SetRange` over ids that do not already ascend
    sorts the second copy (1.0 ms for 20,000 ids, 10.9 ms for 200,000; a probe puts a radix sort at about a seventh of that,
    and it is not done); `Toggle` at 1,000 scattered ids takes 0.26 us where it took 0.17; and an empty selection costs the
    scan's 4.7 ns a call where the sorted copy alone cost 2.2.
  - Tests (Grid suite): a copy of the old linear logic as the reference, and fixed-seed randomized runs over universes of 1
    to 5,200 ids (so that selections pass 1,024 and 4,096 ids) that check count, order, anchor and `IsSelected` after every
    operation and the room rule after every mutator that replaces the selection, and must reach repeated ids, fallbacks, dropped
    ids, unchanged data changes, the anchor's removal, selections on both sides of both limits, and every way of giving room
    back; focused cases (a selection grown past 1,024 ids and shrunk back, answering membership at every size around the
    limit; `Clear` keeping 4,096 ids' room and giving back 4,097's; a range over exactly half of the room giving it back and
    one id more reusing it; a click and a Shift+click after Ctrl+A at the Grid; a long list of 60,000 hashed rows kept in two
    orders; the earlier ones for repeated ids, anchors and orders); and `static_assert`s of the `noexcept` contract. The room is
    asserted through an additive diagnostics accessor, `GridSelectionModel::DebugGetBuffers`, the library's own exact count,
    like `DebugGetContextMenuResources`. Of 40 throwaway mutants, 35 are caught (the first campaign's 15, now also through
    `Toggle` and the large selections, where the scan hides the ascending copy; the scan dropping the newest or oldest id; the
    search reading the wrong copy; each limit and half-rule comparison; releasing one buffer but not the other, for `Clear`,
    `SetSingle` and `SetRange`; the filter turned off or built from the wrong ids), and the other 5 cannot fail a test because
    they give the same answers (a scan limit of 1,023 or 0 ids, the scan and the search swapped, a filter that lets every id
    through, `SetRange` testing the room of one copy only, the two always being equal): the per-call measurement sees those.
  - The opt-in `DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json> [parts]` (`Tests/Embedded/GridSelectionBenchmark.h`,
    fixture `dxui-grid-selection-v2`) measures paint, the isolated cost, `IsSelected` per call in both orders, the heap a model
    holds after each way back from Ctrl+A, the mutators and `PreserveOrdered` over a long list with synthetic data; the
    allocation hook of `EmbeddedTests.cpp` keeps an exact live byte count on request. Its receipts and README are under
    `Measurements/GridSelection/2026-10-01` (two sets of the sorted copy against `main`, and the three-build set).
    API revision stays 2 (an additive diagnostics accessor, and private members of `GridSelectionModel` and so the size of
    `Grid`, changed). The performance contract, the testing contract and `docs/performance.md` describe the budget and the
    measurement; no default or pixel changed (the six sheets of a local Release render are byte-identical to `main`'s and to
    `docs/gallery`).
- DxUi's private window messages are registered by name instead of being `WM_APP` offsets, so an application's own
  messages on the window it shares with DxUi can no longer collide with them: RedSalamander's test message at
  `WM_APP + 0x6A` was DxUi's accessibility action. All sixteen (`src/Support/WindowMessages.h`) now come from
  `RegisterWindowMessageW` as `RedSalamanders.DxUi.<Component>.<Purpose>.v1`, with values in 0xC000–0xFFFF that no
  application message can take: the five that reach the application's window (the accessibility action and provider
  creation, the process-exit detach, the end of a focus gain's turn and a modal menu's menu-bar hover), the ten that
  menu popups receive, and `TextInputServices`' deferred lock, which was registered already and takes the new name.
  Each is registered once, on first use, and every sender and receiver compares that cached value. A failed
  registration is 0 (`WM_NULL`): no sender posts or sends it, each one behaving as when its post fails, and no
  receiver matches it, since a registered message does not compare with a message value and only `Matches` recognizes
  one. `TextInputServices::Attach` reports such a failure as `E_FAIL` rather than a stale `GetLastError`, which could
  read as `S_OK`. A window procedure forwards every message, registered ones included, to `HandleMessage`; DxUi
  reserves no `WM_APP` value, so consumers can drop their reservations of `WM_APP + 0x6A` to `0x6D` and `0x539`. An
  application that drove a modal menu's menu-bar hover by posting `WM_APP + 0x539` itself, as RedSalamander's main
  menu does, calls the new `ContextMenu::PostMenuBarHover` instead. The WindowHost suite checks that every message is
  registered, distinct and never `WM_NULL`, and sends and posts application messages at each former value to show that
  they reach the window procedure while `HandleMessage` neither consumes nor acts on them; the Menu suite shows the
  same for `WM_APP + 0x539` during a modal menu. API revision stays 2 (additive `ContextMenu::PostMenuBarHover`).
  Nothing visual changed, so the gallery is unchanged.
- A failed runtime check in a Debug test run no longer opens a dialog on the desktop.
  - The STL range check a mutant tripped opened the CRT's modal Abort/Retry/Ignore box. Such a box holds an unattended run
    until its watchdog ends it, and interrupts whoever is at the desktop.
  - Every native test executable now first calls `Tests/Support/FailureReports.h`. The report goes to stderr and the run
    ends with exit code 3, without Windows Error Reporting's dialog. The Embedded executable calls it before `main`, so
    the fingerprinted `BenchmarkMain.h` is unchanged.
  - `Test-TestWatchdog.ps1` runs the hidden `--failure-report-self-test` and requires it to end within its bound with
    exit code 3 and the report (`Testing_Validation`).
- A menu test probe of a popup on another thread now waits a bounded time and never touches the popup from the caller's thread.
  - **The failure.** On CI, `TestMenuPopupMaterialsProduceDistinctCaptures` hung until the 300 s watchdog (x64 ASan Debug,
    PR #42; the rerun passed).
    - Every step of its driver is bounded except the capture probe, a cross-thread `SendMessageW` with no timeout.
    - A thread that is still pumping messages answers such a call at once, so the popup thread itself had stopped,
      most likely inside the capture's render. Nothing recorded where.
  - **The fix.**
    - The item rectangle, paint, text and layout probes, the backdrop installer and the bitmap capture now use the state
      probe's design: the popup thread answers into a shared dispatch, and the caller waits at most 3 seconds (the state
      probe stays posted and bounded at 1 second).
    - A popup thread that does not answer fails the probe instead of holding the driver, and with it the open menu.
  - **Why not just add a timeout.** These probes passed pointers into the caller's stack. With a plain
    `SendMessageTimeoutW`, a late answer would have written into a dead frame.
    - Now the payload is registered (`SendMessagePayload` in `PostedPayload.h`). A late answer lands in the shared
      dispatch, and a payload that is never taken is freed with its window.
  - **Two cross-thread reads removed.**
    - The capture and backdrop probes read the popup from the caller's thread before checking threads.
    - The item layout probe had no cross-thread path at all, though the Menu suite's drivers call it.
    - All of them now read the popup only on its own thread. The layout probe has its own registered message,
      `RedSalamanders.DxUi.MenuPopup.DebugGetItemLayout.v1`.
  - **Tests and diagnostics.**
    - `TestContextMenuDebugCaptureBoundsWedgedWindowThread` wedges the popup thread for 4 seconds and requires a capture
      sent meanwhile to fail at its bound (2.9 to 3.9 s), then a capture to succeed once the thread runs.
    - The capture readiness wait prints its longest probe when it times out.
    - The cause of the CI hang itself is not established; a recurrence now names it.
- The Menu suite's modal-loop peek test no longer races its own driver thread.
  - **The failure.** `TestMenuModalLoopPeeksOnlyLivePopupsWhenAPeekClosesASubmenu` failed once on CI's ARM64 Release runner,
    on a pull request that does not touch menus: "the modal loop goes on peeking after a peek closed the submenu". Its
    driver had passed every step, yet the test kept no peek after the close.
  - **The cause.** The test's hook closes the submenu from the loop's peek of the root popup's state probes, and the submenu
    is destroyed before the hook sets the event the driver waits on. The driver then found the submenu gone at once and
    posted its next state probe while the pass that fired the hook still had the root's probe peek to finish. That peek
    could remove the probe: the root answered it and the driver ended the recording before the loop's next pass peeked
    anything.
  - **What the test does now.** After the hook fires, the driver posts only a wake and waits for the first peek kept after the
    close before it posts another state probe. That also makes true what the test assumes: no state probe is pending when
    the hook closes the submenu, so the rest of that pass goes on through the chain the close changed.
  - **Still a test of the loop.** The wait changes only when the driver posts its probes. A chain walk that kept a copy
    taken before the first peek (the shape #42 fixed) would still peek the closed submenu's window on the pass that fired
    the hook, and the test still fails on any kept peek of a window that is gone.
  - Library behavior is unchanged.
- The native menu's modal loop no longer reads a freed popup when its chain changes during a peek.
  - `PeekMessageW` first runs the handlers of messages other threads sent to the menu's thread, and such a handler can
    close or open a submenu. The loops that drain the popups' own paints and state probes walked `controller.popups` with
    a range-for, so a submenu closed during a peek left the loop reading past the shortened chain.
  - An x64 ASan Debug Menu run on main failed once in four with a container-overflow in `PeekMenuDebugStateMessage`.
  - One helper, `PeekMenuPopupMessage`, now indexes the chain afresh for every peek and holds no popup across the call,
    without allocating. The priority order is unchanged: input, then the popups' paints, then the probes.
  - `TestMenuModalLoopPeeksOnlyLivePopupsWhenAPeekClosesASubmenu` closes the open submenu from the loop's peek of the
    root popup's probes, through the new diagnostics hook `DebugSetContextMenuModalLoopPeekHookForTest`. It requires the
    loop to peek only live popups afterwards. It fails on the previous loop, and an index loop that caches the chain's
    size fails it with an STL range check.
- `test.ps1 -Interactive` runs the control suites that need the person's real desktop under a desktop lease of DxUi's own, with a
  warning before it takes anything and the foreground window, keyboard focus and pointer put back afterwards. More than twenty
  control tests (22 in the Menu suite, a few in NativeTextInput) record a capability skip in a run that cannot get focus, and the
  last run of them went through an ad-hoc wrapper rebuilt from a RedSalamander header recovered from an old commit. Nothing about
  the library changed: this is the test harness.
  - Selection: without `-Suites` the run is `Menu` and `NativeTextInput`; `MenuResources` and `MenuResourceScaling`, which also
    reject `--no-activate`, run when named. Any other suite is refused by name before anything is built. The list is
    `Tools/InteractiveRun.psm1`'s, and `Test-InteractiveMode.ps1` keeps it equal to `DxUi.ControlTests.exe`'s own `suiteCanActivate`.
    A run without `-Interactive` is unchanged, including for these four suites, and never reaches the lease.
  - Refusal: before anything is built in a CI job or a process with no interactive window station; after the build the lease checks
    the session natively (`DxUi.InteractiveLease.exe --check`: visible window station, not session 0, active session, an input
    desktop it can open and that is its own, no screen saver, a readable pointer) and checks again when it is about to ask, so a
    locked screen or a disconnected session refuses with its reason. Nothing is shown or taken by a check.
  - The warning is a dialog (`Tests/InteractiveLease`) that says what will happen and for about how long. Its default button is
    Cancel, so a key pressed by accident as it appears cannot start a takeover, and if nobody answers within two minutes the answer
    is Cancel (exit code 21, nothing taken). During the run a banner at the top of the primary monitor, above every window and
    click-through, tells the person to keep hands off the keyboard and mouse.
  - The lease records the foreground window, its keyboard focus and the physical pointer before the dialog appears, takes a
    session-wide lease (a second run refuses instead of queueing), keeps the system awake, runs each suite as a child of its own in
    a kill-on-close job (the foreground is granted to that child alone; only a process the lease started is ever ended) and
    restores the foreground, then the focus, then the pointer, verifying each by reading the desktop and never by a call's result.
    It does so on every exit path: passing suites, a failing suite, the watchdog's exit code 124, a hung child, a child that cannot
    start, a cancelled or unanswered dialog, a warning that cannot be shown, Ctrl+C. The pointer is restored last because activating
    a window can move it. A pointer the fixtures did not put back is reported with where the run left it, and never counted as the
    fixtures' own restoration.
  - An interactive run passes only when every suite exits 0, none records a capability skip (it exists to run what other runs
    skip) and no part of the desktop failed to come back. Logs and receipts take the suffix `.interactive` and a receipt records
    the lease, so they never replace those of the run that records the suite's skips.
  - Tests, none of which takes or asks for a desktop: the new `InteractiveLease` control suite (36 tests, in every `test.ps1` run,
    `--no-activate`) restores a desktop built of fake windows, a fake pointer and a fake foreground and runs the whole lease against
    fake services on every path, requiring the restoration and the release of the lease and the display each time; 28 throwaway
    mutants of the restoration and the orchestration (restoring in the wrong order, trusting a call that reports success, skipping the
    restoration after Cancel or a failed child, never releasing the lease, a timeout taken for a Start, and the rest) each fail it.
    `DxUi.InteractiveLease.exe --self-test` proves the Windows services on a private desktop that is never the input desktop:
    children with their logs, exit codes, bound and stop, the session's lease, and the dialog (Start, Cancel, no answer, a stopped
    run) and the warning. `Test-InteractiveMode.ps1` covers the selection, the refusals (two bounded runs of `test.ps1` under a CI
    environment end before anything is built), the plan and result files and that `test.ps1` reaches the lease only through
    `-Interactive`; 19 throwaway mutants of the PowerShell side and the dialog's source each fail one of the two scripts.
  - Hardening before review:
    - **Failed checks:** the lease routes its own failed checks away from dialogs, as every test process does.
    - **`<cwchar>`:** `Tests/Support/FailureReports.h` now includes `<cwchar>` for `std::fputws`. The lease was the first file
      to include it before anything else, and Debug failed to compile.
    - **Ending a test process:** the two tooling tests that end a test process that outlived its bound now end only that
      process, through their own handle, never its process tree.
    - **Snapshot comparison:** the refusal tests compare their before and after file listings in ordinal order; `Sort-Object`
      is not a total order for those paths, and a Release run failed the comparison on identical listings.
    - **Refusal text:** the refusal tests read the child's output without its color codes. On CI's Ubuntu runner the child colors
      its error text although its output is redirected, and the codes split the reason the test looks for.
  - Docs: the contract is in `Testing_Validation` (Interactive tests), the command in `docs/performance.md` and `Tools/README.md`,
    and `AGENTS.md` and the build and input skills say to run it only with the person's agreement. A nonvisual tooling change: no
    gallery image or design-system preview changes.
- The paired benchmark runs on GitHub for every pull request to `main` that changes something it measures, so a merge waits
  on a hosted runner and not on a quiet developer machine, where the same-binary controls drifted and the set proved
  nothing. The job's first step matches the pull request's changed paths against `Get-BenchmarkScopeRules`: the library
  inputs and harness every receipt hashes (`src`, `include`, `Build`, the build props and vcpkg manifests,
  `performance.ps1` and its comparator and benchmark inputs), the sources of the benchmark executable
  (`Tests/Embedded`, `Tests/Support`) and the fixtures and samples compiled into it, the build and restore scripts, the
  paired measurement and gate tooling and `ci.yml`; Markdown never counts. The steps after it run only when one matched,
  so a pull request that changes none of them ends the job within a couple of minutes, passed, and a step that cannot
  decide fails the job instead of passing a pull request unmeasured. The job always starts rather than being skipped,
  because GitHub reports a skipped job under its unevaluated name expression, which a required check of that name would
  never see.
  - The `paired-benchmark` job compares the pull request's merge ref with that merge commit's first parent, the base as
    the merge ref was made, so the two differ by exactly this pull request (the merge base with the branch would also hold
    what `main` gained since the branch was cut), in the gating scenarios `Default`, `MultilineGrid` and
    `MultilineGridDistinct`, three repetitions each: six runs per side, whose smallest attainable p is 0.0022. A run
    takes 10 to 15 minutes (a pull request's job is bounded at 45, a manual run's at the 90 it always had), a newer push
    to the pull request cancels the older run, the token stays `contents: read`, and the pinned actions are the ones the
    other jobs use. A pull request's check is named `paired-benchmark (pull request)`, apart from the push and manual
    runs of the same job, so that a required check names exactly one check run.
  - The job summary lists each scenario's set verdict and every metric's medians, change, p-value, band, same-binary
    controls and outcome, flagged metrics first; `paired-benchmark-x64-Release` keeps every receipt, comparison,
    `summary.json` and the conclusion as `verdict.json`. The check fails for a confirmed degradation, a regressed metric
    whose own same-binary controls stayed within its band (an exact budget: stayed equal), and for an inconclusive run, a
    regressed metric whose controls drifted beyond it. GitHub has no neutral job conclusion and a green check would read
    as a pass, so both fail; an inconclusive run is re-run, and a flagged metric is listed whatever the conclusion.
    Nothing is rebaselined: the gate measures the base afresh. A run with no regressed metric passes as no regression
    established, and its summary counts the metrics whose controls could not hold their band.
  - The controls of the flagged metric decide, not those of all twenty-six, because a hosted runner drifts beyond some
    band in nearly every control: in the retained A/A set (`Measurements/HostedPairedGate/2026-10-01`, the same library
    code built twice) all 18 controls drifted, in 1 to 13 of 26 metrics, mostly p95 timings (frame, preparation and
    composition), while the eight exact budgets never drifted and nothing was flagged in 78 metric tests. A second
    hosted A/A run (`aa-2` beside it) flagged five clean-phase timings (p 0.004 to 0.026) with their controls drifted,
    which is inconclusive per metric and so can need a re-run. The strict reading, any unstable control making its
    scenario inconclusive, is `-StrictControls` of `Tools/Publish-BenchmarkVerdict.ps1` and was off because it would have
    made every hosted run inconclusive. When both sides have one library fingerprint (a change to the benchmark or its
    tooling only), a timing or memory flag is listed as noise and a rise in a deterministic budget still fails. A set too
    small to reach p < 0.05 cannot pass.
  - A manual dispatch with `benchmark_baseline` keeps its inputs, its default scenarios, its artifact and its three
    repetitions, publishes the same summary and stays green for a finding. Its concurrency group is now its own
    (`github.run_id`), so a push to the dispatched ref, or another dispatch of it, no longer cancels a measurement
    someone asked for (a merge to `main` would have cancelled one dispatched on `main`).
  - The decisions live in `Tools/BenchmarkGate.psm1` with the two step scripts `Tools/Get-BenchmarkScope.ps1` and
    `Tools/Publish-BenchmarkVerdict.ps1` (which also judges a local run: `-Reports <reports directory>`), and
    `Tools/tests/Test-BenchmarkGate.ps1` covers them: the scope rules against the receipt's inputs, the include closure
    of the benchmark executable and every module the scripts import; the pair on fixture merge commits; the conclusion
    on synthetic summaries, on the retained local and hosted sets and on every summary this repository retains, whatever
    its age; the summary, annotations and exit codes; and the workflow's wiring. `Get-LibraryInputPaths` is the
    comparator's one additive export. The performance contract, `docs/performance.md`, `Tools/README.md`,
    `CONTRIBUTING.md`, the testing contract and the performance skill describe it. Nothing visual changed, so the gallery
    is unchanged; the library, its API and every receipt format are unchanged.
- The Grid's bounded multiline cells are verified by 16 new tests in the Grid, Rendering, Accessibility and Embedded suites
  (plan `GridTextOverflow_2026-09-21`, the verification item), each comparing a scenario with a fresh twin painted on the
  same device and never with stored pixels. Ctrl+C and `OnCopy` of trimmed cells holding CR LF, U+2028 and U+2029, a
  zero-width-joiner emoji, a 5,000-unit word, decomposed accents and Arabic are exact across rows, columns and reordered
  columns; UI Automation's Name, Value and ValuePattern carry the same values, a 100,000-unit one included, and the bounding
  rectangle of a cell the viewport cuts is the clipped cell, at five scroll offsets and under a dragged scrollbar thumb.
  Decomposed accents paint the pixels of precomposed ones. A 100,000-unit value whose shaped prefix ends inside a surrogate
  pair, a zero-width-joiner sequence or a letter and its marks paints like a short twin. In a right-to-left flow the marker
  stays on the side its text reads to and each cell's ink in its cell. A cell narrower than a word keeps its ink inside, its
  ellipsis and its tooltip at every clamp. A cell the viewport cuts paints a shifted crop of its whole self at whole-row,
  scrollbar-dragged fractional vertical and fractional horizontal offsets. A new dpi, theme (light, dark, high contrast), font
  or density repaints like a fresh attach. Values that share a start but lay out differently never share a layout. The layout
  tables hold at a lowered ceiling and paint right after eviction, and the real tables stop at 16,384 entries, evict a least
  recently used way and halve when use drops. Device loss, a move between hosts and, on WARP in an embedded view, long French
  cells (marker, hide and show, device replacement, dpi) reproduce a fresh grid. Every test fails under at least one temporary
  mutation of the library (27 mutants, 33 pairs of a mutant and a test; the archived table names the assertion that failed,
  and the one pair that passes is an eviction order the pixel test cannot see, which the Grid test catches). Nothing in the
  library was wrong: the only library change is the diagnostics hook `Grid::DebugSetTextLayoutEntryLimit` (and
  `DebugGetTextLayoutEntryLimit`), which lowers the layout tables' ceiling for the test; the production ceiling is unchanged.
  One contract sentence was imprecise and is corrected: an entry the current or previous paint used is not evicted while a
  table can grow, but at the ceiling a full set gives up its least recently used way, which the Grid test pins on a 64-entry
  table running the same code as at 16,384.
  The fixtures that move a control between hosts moved from `DxUiTests.EditorControls.cpp` to
  `Tests/Controls/DxUiTestMovedControls.h`, and the tests share `Tests/Controls/GridMultilineFixtures.h`. The Grid reads no
  `FlowDirection`, so a right-to-left grid is not mirrored and paints the pixels of a left-to-right one (a finding, logged by
  the right-to-left test, not a change). x64 Debug, Release and ASan Debug runs of the Grid, Rendering, Accessibility,
  Embedded and MultilineText suites are archived under `Measurements/GridTextOverflow/2026-09-30/verification`. The API
  revision does not change (additive diagnostics accessors; a private member added).
- `docs/gallery` is regenerated from `448e924` by the Publish docs gallery workflow, which adds the `Tree / Multi-select`
  tile (#35) to all five theme sheets. The design system is republished as version 17 with those sheets and the Tree
  guideline, preview and styles that #35 changed, and `Specs/DesignSystem/design-system.json` matches its published index
  byte for byte.
  - The merge, toolchain and host-message plan moves to Done: its four library items are merged (#39, #40, #41, #43), the
    batch's hosted paired set establishes no regression, and each consumer tracks its follow-ups in a plan of its own.
  - The Tree multi-select plan records #35's hosted paired result and the regenerated gallery; RedPrism's pin bump remains.
- A window whose only semantic control is a Tree, a Grid or a masked TextField now exposes that control's parts to UI Automation
  (a dialog with one list or one field is such a window), and a client hears the events raised on them.
  - **The failure.** A client could not reach a one-control window's items (the window's element had no children), and UI
    Automation dropped every event raised on them. The multi-select Tree change recorded it as a known limit: "in a window
    whose only semantic control is the tree, UI Automation does not deliver events raised on its items", so the selection
    events a multi-select tree raises never reached a client of a window of that tree alone. A one-field window lost more:
    the text-changed and selection-changed events its field raises never reached a client, and the element a text range
    names as enclosing its text was not the window's.
  - **The cause.** Such a window collapses: its root element stands for the control, which has no element of its own. The
    root's children were nothing, though, and the control's fragments (a tree's items, a grid's headers, rows and cells, a
    masked field's reveal button) had a second element for the control as their parent, which had no parent. They formed a
    subtree nobody reaches from the window, and UI Automation does not deliver an event raised on an element whose parents
    do not lead to the window (with the root childless but the parents connected the events arrive, and with the children
    in place but the parents orphaned they do not). The selection container, containing grid and enclosing element of a
    text range, and the text events, were such second elements too.
  - **The fix** (`DxUi.Accessibility.cpp`). The window's element is the control's one element. Its first and last child are
    the control's first and last fragment, in the order the control's own element had them (the same two functions serve
    both), and the parent of a fragment is that element. One function, `CreateControlElement`, now makes the element of
    the control at a path, and returns the window's canonical element when a collapsed root stands for it. Hit testing, the
    focus answer, every event about a control (disclosure, focus, text, and the invalidation of a multi-select tree's
    selection, which the multi-select change had special-cased), the Selection container of an item or row, the containing
    grid of a cell and the enclosing element of a text range all use it. A status root keeps its other controls as its
    children. Embedded views never collapse, so they needed no change: their root element is the application's child and the
    control's elements follow it.
  - **Tests.** The Accessibility suite walks a UI Automation client from the window's element to the items, headers, rows,
    cells and reveal button and back (first and last child, siblings, parent, a search of the children, the selection
    container and the containing grid, a replaced tree's gone elements), and it listens at the window for the events raised
    on them and the text events a field raises. Each runs for a control that fills its window and for the same control
    beside a button, under the same expectations. The twins passed before the fix; the single-control tests failed, with no
    child below the window's element and no item event heard while an event raised on the window's element itself was
    heard. The multi-select Tree's selection-events test (selected, added, removed, invalidated, and silence) runs again for
    a tree that fills its window, and hears the events the library raises itself; it failed before the fix. The library
    raises no selection event for a single selection or for a Grid, so those are raised by the test on the elements the
    library hands out for the items. Eleven single-point mutants of the fix (each of the places above, the order of a grid's
    last child, the reveal button) each fail a named assertion. The focus change the host announces for the item the keyboard
    reached needs the foreground: the Menu suite tests it for a tree and a grid that fill their window and records a
    capability skip without a desktop.
  - **An embedded harness.** `Tests/Embedded/EmbeddedUiaBridge.h` plays the application: a window with a UI Automation
    provider of its own whose child is the view's root element, with the view's site adapted to it. The embedded suite
    attaches views of a Tree and a Grid to it and walks and listens to them with the same in-process client
    (`Tests/Support/UiaTestClient.h`, which the control suites now share): the walk from the application's element to the
    items and back, the events the view raises when it publishes a change, the events raised on the items, and the selection
    events a multi-select tree's view raises itself from `UpdateAccessibility`, which had no client harness. Mutants that
    orphan an embedded item, drop the view's focus event or drop its selection events fail them.
  - Specified in `UI_InputAndAccessibility.md`, `UI_ControlsAndLayout.md` (which no longer says a window of one tree gets no
    item events) and `Testing_Validation.md`; `docs/hosting.md`, the input-accessibility skill and the Tree plan's known
    limits describe both. API revision stays 3 (no declaration changed). Nothing visual changed, so the gallery is unchanged.
- Changes no longer conflict over the changelog or the plan index. A change records its changelog entry as one file under
  `Changes/` (`<yyyy-mm-dd>-<topic>.md`, one bullet; `Changes/README.md`), and `Tools/Fold-Changelog.ps1` folds the
  fragments into `CHANGELOG.md`, newest first. `validate-specs.ps1` checks each fragment's name and shape, and
  `Test-Docs.ps1` covers the fold. The WIP plan index lists one plan per entry, separated by blank lines, so updates to
  different plans merge cleanly. On 30 September five branches each prepended an entry or edited a neighbouring index
  row, so they merged one at a time and ran CI again after each re-merge.
- API revision 3, with a rule for when the revision changes and a validated list of what consumers may call.
  - **Why.** Three changes since revision 2 could break a pinned consumer, yet the revision stayed at 2. A consumer moving its
    pin to this commit or a later one sets `apiRevision` to 3 in `Dependencies/DxUi.lock.json` and makes these changes:
    - **Renamed interfaces (#29).** `IDxGridModel`, `IDxGridDelegate`, `IDxTreeModel` and `IDxTreeDelegate` are now
      `IGridModel`, `IGridDelegate`, `ITreeModel` and `ITreeDelegate`.
    - **No Python tools (#30).** A consumer that ran the Python build-matrix validator runs
      `validate-build-matrix.ps1 -Root <consumer root>` instead.
    - **Registered window messages (#41).** DxUi's private window messages are registered by name, so a consumer:
      - drops any DxUi `WM_APP` value it copied;
      - forwards every message to `HandleMessage`, including registered ones (0xC000–0xFFFF);
      - posts the menu-bar hover with `ContextMenu::PostMenuBarHover`.
    - **New output fingerprint.** The consumer output fingerprint includes the revision, so the first build after the
      move uses a fresh dependency output directory.
  - **The rule** (`Specs/Build/Build_ToolchainAndConsumption.md`).
    - The revision increments when a consumer built against the previous one could stop compiling, linking or behaving as
      before without changing its own code. Additions do not count.
    - Instead of incrementing, a change may keep the old form working for one revision. The alias has no `[[deprecated]]`
      attribute, because consumers compile with warnings as errors; the changelog names it instead.
  - **The consumer interface.** `capabilities.json` now lists what a consumer may call or import:
    - three scripts, with the parameters consumers pass;
    - five module functions, with their parameters;
    - the four MSBuild entry files;
    - the public header root.
    `validate-dependencies.ps1` fails when a listed entry disappears, a listed function is no longer exported, or a listed
    parameter is renamed. It reads scripts and modules from their parse trees, and new parameters pass.
  - **Single source for the revision.** `Get-DxUiConsumerBuildIdentity` and `test-consumer.ps1` read the revision from
    `capabilities.json` instead of hard-coding 2. The docs, the consumer-integration skill and the README say revision 3.
- The described-menu memory waiver is removed (plan `MenuDescriptionMemory_2026-09-27`). It accepted up to 1,175,552
  clean-round private bytes (+4.6%) that the September 21 pairs measured for described native menus. A local paired
  set of six runs per side (`e47c836` against `6f769ab`, the hosted set's harness) now finds +0.36% (p = 0.70), and dirty
  -0.89% (p = 0.56), within the bands, as the hosted set did (-0.50%, -0.70%). Described menus carry no memory envelope
  anymore; the receipts are under `Measurements/MenuDescriptions/2026-09-30/paired-local`.
- `TestMenuSurvivesAWindowClosingItFromSetCursor` runs instead of skipping everywhere. Since it was added (PR 29) it
  skipped in every run, local and x64 CI alike, as "another window covers the menu-closing window". On a developer
  desktop the cover was the desktop application's own window, which takes the foreground back while the suite runs and
  rises above the test's non-topmost window. The window is now topmost, and the skip names the covering window's
  class and process. The full Menu suite runs with no skip on an interactive desktop.
- Consumers can restore current pins again. #22 added measurement receipts with tracked paths of up to 176 characters.
  A consumer restores its pin under `<root>\.build\dependencies\DxUi\source\<commit>\`, and Git without long paths
  fails there once the full path passes 259 characters. RedXe's `main` CI has failed at restore since it pinned
  `6b34456`, and RedSalamander could not restore `a0b4934` from `D:\RedSalamander`.
  - Every tracked path is now at most 150 characters, so any consumer root of up to 35 characters restores.
    `validate-dependencies.ps1` rejects a longer tracked or untracked-but-unignored path
    (`Build_ToolchainAndConsumption`).
  - 767 evidence files were renamed, with their bytes unchanged: CI artifact directories dropped `native-`,
    `Performance-*` receipts keep 8 of their 32 run-id digits, and `associative-cache-rejected` became
    `assoc-cache-rejected`. Hash lists and manifests name the new paths, and each packet's README records the rename.
- The Menu suite's real-client focus fixtures (the click that activates a window UI Automation has or has not seen, and
  the reactivation of a seen window) no longer fail when another application takes the foreground (plan
  `SeenWindowReactivation_2026-09-30`, item 3). One failed a local Debug run while the desktop application and the Start
  menu took it. They now play each attempt with windows of their own through `RunUntilForegroundHeld`
  (`Tests/Controls/DxUiTestHelpers.h`), the multi-window form of `RunWhileForegroundHeld`, which is now written on top of
  it. A failed expectation records the first failure, with the names the client heard, instead of ending the test, and
  an attempt that another application took the foreground from, from any of its windows, is played again, five times at
  most. The attempt that kept the foreground decides the test, and one that never did records a skip naming the thief.
  Under `--foreground-thief` all three record that skip after five attempts, and never fail. Item 7's first version,
  announcing every move of a gain's turn, and removing the reactivation announcement each still fail their test.
- Activating a window UI Automation has seen announces its focused control (plan `SeenWindowReactivation_2026-09-30`).
  UI Automation answers the first focus event of a window with a call of the fragment root's `GetFocus`, and its later
  ones from whether the window's root element has the keyboard focus, which it does not while the focus is inside a
  control. The host announced nothing for what a gain focuses or restores, so switching back to such a window (Alt+Tab)
  was reported by nothing, and a screen reader did not say which control had the focus. The host now reads, as a gain
  begins and before it publishes anything, how many `GetFocus` calls have begun on the window. When any had, it
  announces the focused element once the gain's message-loop turn ends: the restored control, the one the activation
  focused, or the window when no control has focus. It does not when it already announced a move of that turn (the
  click that activates the window) or when the root element stands for its single focused control, which UI
  Automation reports itself. A window's first activation stays the answer of its first `GetFocus` call, with no
  duplicate. The message the host posts at a gain now names its turn, so the message of an earlier gain, which the
  window lost before the loop turned, no longer ends a later turn. The WindowHost suite tests the decision without a
  desktop, including a stale turn message and a single-control root. The Menu suite reactivates a seen window with an
  in-process UI Automation client, once restoring a control and once focusing the first, and requires the client to
  hear that control once. Five mutants each fail a test. API revision stays 2 (additive diagnostics accessor
  `ControlHost::DebugGetReactivationAnnouncementCount`; private members changed).
- Window-host UI Automation providers resolve their control without scanning the tree (plan
  `ReliabilityAndFollowUps_2026-09-30`, item 6): every provider call searched the published records for its control's
  path (several times per call, so a client walking every element of a window paid for the whole tree on each one), and
  every event searched the live tree for its control. Each snapshot now carries lookup tables built as it is published
  (records by path, hit rectangles by kind, path and item, and control addresses), so a provider call examines a few
  table slots and an event a search of the sorted addresses (about log2 of the records) and a walk down its path. In a
  window of 1,960 buttons a `Navigate` call examined 4,901 records on average (9,796 at most) and now 5.8 (21), a
  property read 2,942 (5,880) and now 3.5 (15), `get_BoundingRectangle` 2,942 (3,921) and now 3.4 (11), a pattern
  query 2,942 (5,880) and now 3.5 (15), and an event's control lookup 1,002 tree nodes (2,001) and now 17 (17); a tree
  of 245 buttons costs the same per call as one of 1,960. What a provider or event reaches is unchanged: the tables
  describe the snapshot's own tree, a control they hold is confirmed against the live tree before it is used, one they
  do not hold (added since the publish, hidden, no element) is searched for there as before, and stale elements still
  answer `UIA_E_ELEMENTNOTAVAILABLE`. What lives inside one control (a tree's items by id, a grid's rows and cells) and
  hit testing a point still scan. Test-only diagnostics count what resolutions examine, resolve an event's path and
  check the tables against a scan of the records.
- A click that activates a window announces the clicked control once (plan `ReliabilityAndFollowUps_2026-09-30`, item
  7): the click sets its control after the window's `WM_SETFOCUS`, in the same turn of its message loop and before UI
  Automation has acted on the system's activation focus event, and the host announced that move besides the event, so a
  client could hear the clicked control twice. What the event reports depends on what UI Automation knows of the window:
  the first focus event of a window it has not reported before is answered with a call of the fragment root's
  `GetFocus`, on whatever thread, which reads the published snapshot when it runs, and its later ones without
  `GetFocus`, from whether the window's root element has the keyboard focus, which reports nothing while the focus is
  inside a control (observed on Windows 11 build 26200). A window that gained focus now leaves a focus move of that turn
  to the event only while no `GetFocus` call has ever begun on it, and announces the move itself otherwise (at worst as
  a duplicate, never not at all): each call counts itself in the window's provider target before it loads the snapshot,
  and the host reads the count after it stores the snapshot of the move, all four operations sequentially consistent, so
  with the count at zero every call that follows loads the snapshot after the store and reports the clicked control. The
  host posts itself a message at the gain whose dispatch ends the turn (as does losing focus, or 500 ms without it,
  should a window procedure never hand it to the host); a click in the window that is already active, or any move in a
  later turn, is announced by the host as before. The WindowHost suite tests the decision without a desktop (a first
  gain's move left to the system and announced once a call has begun, a window asked before, the turn's three ends, and
  a call that a test gate holds on a thread of its own before it counts itself), and the Menu suite plays a click that
  activates a window with an in-process UI Automation client in both cases: a window UI Automation has asked before (the
  client hears the clicked control once, and last) and a first activation whose `GetFocus` call the gate holds until
  after the click (it hears the clicked control as often as it hears the control of a first activation, and never the
  control the activation focused on the way). A window procedure must pass the private message (`WM_APP + 0x06D`) to
  `HandleMessage`, as it does the accessibility ones. API revision stays 2 (additive diagnostics accessors
  `ControlHost::DebugIsInFocusGainTurn` and `DebugGetFocusMovesLeftToSystemCount`; private members changed).
- A hung control test ends the run with its name instead of holding a CI job (plan `ReliabilityAndFollowUps_2026-09-30`,
  item 12). On the pull-request run of PR 30 the x64 ASan Debug job printed nothing for the 35 minutes between starting the
  Menu suite and the job's 40-minute limit, and its log named no test. Every control test a suite runner starts through
  `DXUI_RUN_TEST`, and every fixture suite without named tests, now runs under a watchdog: one thread waits for the deadline
  on a condition variable (it never polls) and, when a test outlives it, writes `TIMEOUT: <TestName> after <N> s` and
  terminates the process with exit code 124, since a stuck test cannot be unwound. The deadline is 300 s, forty times the
  slowest of the 816 non-foreground tests under AddressSanitizer (7.5 s); `DxUi.ControlTests.exe --test-timeout=<seconds>` and
  `test.ps1 -TestTimeout <seconds>` set it and 0 turns it off. Every run prints its deadline, every `[DONE]` marker carries the
  test's duration, and `test.ps1` reports a failing suite's exit code and `TIMEOUT:` line beside the last lines of its log
  (`Tools/SuiteFailure.psm1`) and prints every capability skip under its suite as `skipped <Test>: <reason>`, so a CI log
  shows which tests a missing desktop or foreground left unrun without the uploaded suite log. The three resource fixtures report each sample, so their deadline bounds a cycle. A hidden
  `--watchdog-self-test` switch runs a test that never returns, and `Tools/tests/Test-TestWatchdog.ps1` asserts the exit code,
  the line and the time, and that with the watchdog off the same test still hangs. The audit of the Menu, NativeTextInput and
  resource suites found no unbounded polling loop, `INFINITE` wait or UI Automation wait without a deadline, but two ways a
  test could still hang: a driver thread that gave up before it found its popup left the owner thread in `ContextMenu::Show`'s
  modal loop for good (every driver now starts with `DismissMenusIfDriverFails`, which a new test with a driver that fails
  at once requires to close the menu, and a source scan requires in every driver), and a UI Automation client thread was
  joined after a pumped wait of 3 s although its teardown may need the pumping thread (the Menu and Accessibility tests
  that join one now wait as long as its setup was allowed, 20 s, and fail the test). Tests and tooling only: no library code
  changed, and the five gallery sheets are byte-identical, so `docs/gallery` needs no update.
- A `Grid` that stops painting returns its retained text layouts (plan `ReliabilityAndFollowUps_2026-09-30`, item 9):
  hidden itself or under a hidden ancestor (an unselected tab page, a collapsed panel or page host), in a hidden
  embedded view, detached from its host or given another model, it releases its layouts, their key strings, its tables
  and its ellipsis sign at once, and showing it lays out only the cells it then shows, once. Until now such a grid kept
  its last paint's layouts until a next paint that might never come. A painting grid holds one layout per cell it drew
  last, however far it has scrolled (28 in 32 entries for a 6x4 multiline test grid, 66 in 128 for 10x6, 44 in 64 for
  ten single-line rows) with 6 to 12 KB of key strings and 5 to 14 KB of tables; the layouts are the bulk, about 20 KB
  of process heap each (570 KB for the 6x4 grid, 934 KB for the single-line grid, measured in Release as what a hide
  returns). Painting, the layouts' lifetime while painting and its allocations are unchanged. The hook is a protected
  `Control::OnHidden`, which panels and page hosts forward; a hidden or minimized native window keeps its layouts.
  With this attribution the developer accepted the retention the review follow-ups introduced for their frame rate
  (`Default` dirty rate +11.3%, p = 0.029, over both 2026-09-29 sets): the performance contract's accepted Grid layout
  retention bounds it to one layout per cell of the last paint, and none while the grid is not painting.
- Publishing `docs/gallery` after a merge is no longer a manual copy (plan `ReliabilityAndFollowUps_2026-09-30`, item
  11): the manual `Publish docs gallery` workflow (`.github/workflows/gallery.yml`) regenerates the gallery from a native
  x64 Release build with `gallery.ps1 -PublishDocs`, validates the specifications and commits it to the branch it runs
  on. Like the formatting workflow's apply mode it runs only on `workflow_dispatch` with an explicit boolean input, holds
  `contents: write` for that one job, pushes normally and never forces, and never runs on `pull_request_target`. It
  commits only when a sheet, the HTML index or the README changed (`generation.json` records the source commit and so
  differs after every commit); `Tools/Commit-Gallery.ps1` makes that decision and is tested against fixture repositories.
  `docs/gallery` itself was last rendered on a hosted runner from `ed1dea9`, before PR 29's visual changes; it is
  regenerated here on the developer machine from the final sources (`main` and this tree render byte-identical sheets
  there), and the design system is republished with those sheets and the guidelines PR 29 changed.
- One validation entry point (plan `ReliabilityAndFollowUps_2026-09-30`, item 10): `validate.ps1` runs the five
  validators and the tooling tests, each in its own process, reports every failure before it fails, and is what CI's
  validation job runs; `test.ps1` now also runs the tooling tests. The validators' file scans no longer enter a nested
  git checkout (a directory other than the scanned root holding a `.git` file or directory, such as an agent's worktree
  under `.claude/worktrees`): validating a main checkout counted every worktree's copy of the Markdown (1,069 files
  instead of about 214), so a half-edited worktree could fail it. Every tooling test script now sets strict mode itself,
  which a tooling test requires: `test.ps1` is strict and the scripts it calls inherit it, so the new watchdog test passed
  11 of 11 cases alone and failed 6 under `test.ps1` (`.Count` on a function's output unrolled to one line or none).
- Paired sets can establish a result on a noisy machine (plan `ReliabilityAndFollowUps_2026-09-30`, item 4):
  `performance-paired.ps1 -Repetitions N` (default 3, at most 10) repeats the interleaved A, B, B, A pass, so each side
  has 2N runs, and judges every phase and metric on all runs at once: an exact two-sided Mann-Whitney U test (counted
  over the observed ranks, ties included, no approximation) of the baseline run medians against the candidate's, with
  the median shift and the metric's band (5% timing and FPS, 2% process memory). A metric is `regressed` or `improved`
  only when p < 0.05 and the shift exceeds the band, and any rise in an exact budget (surface bytes, replacement peak,
  allocations) in any candidate run is `regressed`; a set with a regressed metric is `advice-required`. Same-binary
  spread is reported, not a veto. Six runs against six reach p = 0.0022 when completely separated; two against two
  cannot reach 0.05. The first pass keeps its receipt and comparison names, and `Compare-PerformanceSet` in
  `Tools/PerformanceComparison.psm1` is tested on synthetic receipts. The performance contract and guide state the rule.
- `performance-paired.ps1` compares two trees (plan `ReliabilityAndFollowUps_2026-09-30`, item 5): `-BaselinePath` and
  `-CandidatePath` measure existing DxUi working trees as they are, uncommitted work included, with this checkout's
  harness written into them for the run and their files put back afterwards. A pair with a named tree is refused when
  the two library source fingerprints are identical instead of when the commits are; revisions keep the commit rule.
  `summary.json` records each side's revision or path, commit and fingerprint. The receipt's source fingerprint and
  benchmark input list moved unchanged into `Tools/PerformanceComparison.psm1`, and `Tools/PairedRun.psm1` with its
  tests owns tree selection, the refusal rules, named-tree validation and the harness overlay.
- A moved control is told what it now inherits (plan `ReliabilityAndFollowUps_2026-09-30`, item 8). A control's flow
  direction and density are inherited through its parents and a parent's own change is announced to its children, but
  a move (`ControlHost::SetRoot`, `PageHost::SetPage`, `Panel::AddChild`) announced nothing, so a control that keeps an
  arrangement for them kept the one of its old place: a `ColorPicker` (its steppers, hex field and buttons), a
  `NumericStepper`, a `TabControl` (its header, unless its host changed too) or a horizontal `StackPanel` moved out of
  a right-to-left parent stayed mirrored, and a `Tree` or `Grid` moved between densities kept its row metrics.
  `Control::Reparent` (which `SetParent` calls, and `SetRoot` and `SetPage` use in place of `SetParent` plus
  `PropagateHost`) compares what the control inherits before and after and announces `OnFlowDirectionChanged` /
  `OnDensityChanged` once, in its final place, only when a value differs: adding a child that inherits what its parent
  has, replacing a root and clearing a panel announce nothing, while a child added under a right-to-left or compact
  parent, and a root arriving in a compact host, now hear what they inherit. `ColorPicker` also releases its gradient
  brushes and their Direct2D device reference when its host changes instead of holding the old host's device until its
  next paint, and `Panel::ClearChildren` skips the empty slot a child moved out through `GetChildren()` leaves (it
  dereferenced it). The menu bar and the text field key their layouts on these inputs and were never stale. API
  revision stays 2 (additive diagnostics accessor `ColorPicker::DebugHasCachedBrushes` and protected
  `Control::Reparent`).
- The described-menu memory check is deterministic. `DebugGetContextMenuResources` (a test-only diagnostics hook) counts
  the live menu popups, the text layouts their described rows hold and the accessibility records of menu-popup
  snapshots, exactly and whatever the renderer and the allocator keep; the fixture asserts that all three return to
  their value before the menu opened after it closes while a client holds eight row elements. It no longer bounds the
  process heap, which a software renderer's surfaces and caches (WARP, or the Basic Render Driver of a GPU-less
  runner) swing by up to about 3 MB, and it now runs under AddressSanitizer too. A window-host provider that pins a
  snapshot, a target that keeps its last snapshot after the window closes and a popup that is never freed each fail it.
- NativeTextInput's focus fixtures survive another application taking the foreground. Windows then deactivates the
  window and the host releases its native text session, TSF document included, as designed; the desktop application
  hosting a session did so 30-95 ms after each test window activated and failed the TSF document fixture in three of
  six local runs. The four fixtures that pump after taking focus repeat their sequence (`RunWhileForegroundHeld`, five
  runs at most) until no application takes the foreground and then make exactly their former assertions; when one takes
  it every time they record a capability skip naming its executable, and a regression with the foreground held still
  fails. Two deterministic fixtures deliver the takeover as Windows sends it, and `DxUi.ControlTests.exe
  --foreground-thief[=<minMs>,<maxMs>]` reproduces the desktop application on demand.
- The control-test runner runs single tests: `DxUi.ControlTests.exe --suite=<Suite> --test=<Name>[,<Name>...]` and
  `test.ps1 -Suites <Suite> -Tests <Name>[,<Name>]` run only the named test functions of the suite. Every suite runner
  registers its tests as `DXUI_RUN_TEST(TestName);`, which checks the filter and prints the test's `[START]`/`[DONE]`
  markers. A name no selected suite registers, a malformed list or a fixture suite without named tests fails the run
  (exit code 2) instead of passing with nothing run, and a filtered `test.ps1` run keeps its own log and receipt
  (`*.filtered`), so it never replaces the receipt of the whole suite.
- Repository tooling is PowerShell only (plan `PowerShellTooling_2026-09-30`): the spec, skill, dependency,
  inherited-test and build-matrix validators, the performance comparator and their tests are PowerShell modules and
  scripts, so `test.ps1`, the validators and CI need no Python, pip or PyYAML. The comparator reproduces every stored
  paired comparison under `Measurements` exactly. New root entry points `validate-test-port.ps1` and
  `validate-build-matrix.ps1` join the other validators, `Tools/tests/Invoke-ToolingTests.ps1` runs the tooling tests,
  and `Tools/Install-ClangFormat.ps1` installs the checksum-pinned formatter that `format.ps1` now finds first.
- Follow up the review's reported items as decided (plan `ReviewFollowUps_2026-09-29`):
  - Multiline `Grid` cells keep their shaped layouts in 32-way set-associative tables keyed by a library-owned mixed
    hash of the value and layout box, growing (to 16,384) while the values of the current and the previous paint crowd
    a set: a repaint of unchanged cells shapes nothing and a one-row scroll either way shapes only the entering row
    (scrolling up no longer evicts the rows still in view), for any number of distinct visible values. Values whose
    visible text is the same share its omitted-tail layout through a keyed table instead of a scan of every entry. Key
    and display strings reuse power-of-two storage, so dirty frames allocate no more than the direct-mapped cache did.
    A value shapes only what it can show, wrapped or on one line: a 100,000-unit cell lays out a few hundred units
    (243 at `SetLineClamp(1)`, where the old prefix never stopped; 971 at a clamp of 1,000 in a short row, sized by the
    lines that fit), and it is retained by that prefix, so its repaint and hover shape nothing and another value
    starting the same way shares it. Keys round layout boxes to 1/64 DIP, so rows at heights such as compact density's no
    longer miss after a scroll. The omission marker takes the direction of the text it ends, read by character (an
    emoji ending Arabic text no longer sends it right), and trailing lines of other spaces or default-ignorable
    characters add nothing. A `MultilineGridDistinct` benchmark scene gives every cell its own text.
  - Single-line `Grid` cells keep their caption layouts in the same table instead of laying the caption out on every
    paint and every hover, with pixels identical to the previous centred-text draw; a long leading-aligned caption
    shapes only the prefix its cell can show (363 units for a 100,000-unit caption over three paints).
  - Window hosts raise UI Automation focus changes as focus moves inside their focused window, naming the focused
    tree item or grid row as GetFocus does, and the window itself once no control has focus; a window that lost the
    foreground announces nothing, and the window's own activation is left to the system's focus event. An element's
    SetFocus moves logical focus before Win32 focus, and UI Automation's SetFocus no longer holds the accessibility
    mutex while the host runs callbacks and raises events. A window-host element (tree items and grid rows too) whose
    control was removed or replaced at the same path reports `UIA_E_ELEMENTNOTAVAILABLE`, with runtime ids from a
    per-process serial that no later control reuses, and such a republish, a whole-tree `SetRoot` included, raises
    StructureChanged. A child of a collapsed status root reports its own events. A focus callback that removes its
    control leaves no focus behind (a rootless host keeps a live one); a control disabled or removed while focused
    loses focus, published, at the host's next message; a republish never touches a focused control its panel removed.
  - While a menu is open its popups show the arrow cursor and a window of the menu's thread outside them chooses its
    own; closing lets that window choose again (except when the owner's destruction closes it). Closing a described
    menu returns all its memory, even while a client holds its row elements.
  - A disclosure's chevron rotation starts at its first animation tick (so a change made while hidden rotates once
    shown) and a gap in the ticks pauses it; unsupplied alert colors keep distinct, readable default tones outside high
    contrast, and a derived partner is black or white by contrast measured as the pair paints (4.5:1 for any supplied
    fill, translucent ones over the window background included);
    right-to-left tab titles, `NumericStepper` labels and units and the `ColorPicker` hex caption read right to left
    from their start; `NumericStepper::SetStepButtonNames` (default "Increase" / "Decrease") and `ColorPicker::Labels`
    step-button names, caption and swatch widths (bounded to 4,096 DIPs) serve translated captions.
  - Source-breaking renames drop the repeated namespace: `IGridModel`, `IGridDelegate`, `ITreeModel`, `ITreeDelegate`,
    `RunModalLoop`, `IsRenderStageActiveForDebug`, `EmitRenderMutationBlockedForDebug`, and in `DxUi::Typography`
    `Spec`, `GetSpec`, `PerfEmitter`, `GetPerfEmitter`, `SetPerfEmitter`, `EmitPerfCounter`, `FontFamilyCacheEntry`,
    `TextFormatCacheEntry`, `GetMeasurementCacheMutex`, `GetFontFamilyCache`, `GetTextFormatCache`,
    `kFamilyCacheMissMetric` and `kTextFormatCacheMissMetric`. No aliases remain; consumers rename when they move
    their pin. API revision stays 2, as for the earlier modal-loop rename.
- Fix defects found by reviewing the last fifteen days of changes (plan `ReviewFixes_2026-09-29`):
  - Captured drags: the embedded rule resolves the capture through the live tree before touching it (a destroyed
    captured control was read after free) and requires every ancestor to stay visible and enabled. WindowHost now
    cancels a drag whose control or ancestor is disabled or hidden (`OnCaptureLost`) instead of dropping the capture
    silently and leaving the control mid-drag.
  - `Splitter`, `NumericStepper`, `ColorPicker` and `TextField`'s focus bar and button hovers skip a draw when no
    solid brush is available, as the shared helpers do. `Splitter` keeps its separator inside an extent smaller
    than both pane minimums and is exposed to UI Automation as a Thumb with RangeValue.
  - `ColorPicker` keys its gradients by a retained `ID2D1Device` (a recreated device context could reuse the cached
    address and fail every preparation with D2DERR_WRONG_RESOURCE_DOMAIN). The gradients are hue-independent and
    placed by transform, so hue drags and moves create no resources. The field being typed in keeps its text, caret
    and undo history; a canceled component edit is followed; Escape in the hex field cancels; runtime flow changes
    re-arrange the fields; right-to-left flow mirrors the saturation axis the keys already mirrored.
  - `NumericStepper` ends every previewed edit with Commit or Cancel (an unparsable edit reverts with Cancel),
    commits text changed without focus (UI Automation), re-rounds on `SetDecimals`, never sticks on a step finer
    than its decimals, reads full-width and Arabic-Indic digits, and rewrites its field only when the text changed.
  - `Tree` row drags never target the dragged row's own subtree, follow model changes and wheel scrolls, decide the
    drop at the release point, cancel on a second button, `SetModel` or `SetReorderEnabled(false)`, treat id 0 as an
    ordinary row, paint without a per-frame id scan and repaint only when the drop changes. Row `iconText` again
    uses the UI font unless it is a private-use glyph (letter icons had rendered as missing-glyph boxes).
  - Multiline `Grid` cells no longer draw with `D2D1_DRAW_TEXT_OPTIONS_CLIP` on their fractional layout box (it cut
    the last line's descenders and accounted for most of the multiline memory growth: dirty-round private bytes fall
    about 5.5 MB on the MultilineGrid fixture); trailing NEL/VT/FF and blank tail lines add no ellipsis; any
    trimming offers the tooltip; and a scrolled single-line caption offers its full value.
  - Described menus paint Info-row descriptions, render color glyphs, raise UI Automation focus changes as the
    keyboard moves through rows, and a UIA SetFocus on a popup root no longer dismisses the menu. Window-host
    UIA event providers no longer pin a full accessibility snapshot each while a client listens.
  - Right-to-left `Checkbox` and `RadioButton` captions sit beside their indicator. `ProgressBar` rejects non-finite
    values and an empty indeterminate bar requests no frames. `Slider` no longer drops a change as small as its
    smallest step.
  - `performance-paired.ps1` judges same-source controls two-sided (`unstable-control`), fixes `-SkipBuild` and
    annotates findings in CI; `performance.ps1` never re-stamps a stale receipt and keeps one default receipt per
    scenario. API revision stays 2 (additive; private members and one Tree debug field changed).
- Honor reduced motion in `ProgressBar`: an indeterminate bar rests its segment centered, requests no animation
  ticks and paints identically every frame; restoring motion resumes the 2 s sweep. `Tick` re-seeds when the tick
  clock moves backwards and drops whole loops from long gaps, so no elapsed time can corrupt the sweep phase.
  API revision stays 2 (additive diagnostics accessor only).
- Never paint status invisibly: `MakeThemePalette` still copies supplied alert colors, but a zero-alpha alert
  color, including the `ThemeColors` zero default, falls back to `windowBackground` for fills and `text` for
  text. High-contrast themes without alert colors now show toned grid rows, tone badges and throughput limits.
- Add the editor consumer controls `Splitter`, `NumericStepper` and `ColorPicker`: preview/commit/cancel notifications,
  keyboard operation, right-to-left mirroring, disabled and focus-visible states, and the public `HsvFromArgb` /
  `ArgbFromHsv` / `ParseHexColor` / `FormatHexColor` helpers. `WindowHostCursorKind::VerticalResize` maps to the
  vertical resize cursor. Catalog/factory count is 30. API revision stays 2 (additive).
- Paint `Slider` like the Windows volume flyout: a 6 DIP capsule track (fill and remainder the same thickness), a fixed
  20 DIP gray chrome disc, and an accent inner thumb that matches the track at rest and grows to 16 DIP on hover
  (12 DIP pressed). Pointer hit testing stays an unpainted 48 DIP band with a 24 DIP grab radius. Embedded
  `DispatchPointer` synthesizes control double-click (word selection) using the system interval and a 16 DIP slop.

- Release the embedded surface on `SetVisible(false)` and zero-extent `Prepare`: `surfaceBytes` reports 0 while
  hidden or zero-sized, and the next visible sized preparation reallocates exactly one surface with identical pixels.
  `AdvanceAnimation` no longer marks the view dirty unconditionally; every `Tick` that changes visual state
  invalidates (caret blink flips, button/page transitions, tooltip show/hide, grid/tree/menu animation), and
  `ControlHost::RequestAnimation` wakes an embedded view only when animation becomes requested, so paint-time
  requests (indeterminate progress, busy grids) no longer re-dirty the view inside every preparation. Bound the
  per-host solid-brush (256) and configured-text-format (96) caches with a trim at preparation/paint start, report
  `cachedBrushes`/`cachedTextFormats` in `EmbeddedStatistics`, and gate the complex benchmark at zero clean-round and
  64 (Release) / 320 (Debug) dirty per-frame C++ allocations. API revision stays 2 (additive).
- Add `PageIndicator`: a bottom strip of dots for paged surfaces. Fewer than two pages paint nothing and are not
  hittable. Click, Left/Right/Home/End and `SetSelectedIndex` share one selected index; only user input fires
  `SetOnSelected`. Catalog/factory count is 27.
- Deliver API revision 2 through one DxUi.lib: public controls, a 26-control catalog/factory, neutral themes and
  diagnostics, and native plus supplied-device embedded hosting. Foundation is part of the same archive.
- Separate dirty preparation from allocation-free D3D11 composition; support logical capture, DPI, visibility,
  device replacement, shared device pools, and slider preview/commit/cancel notifications.
- Add a public toggle/slider consumer, relocated exact-pin consumption tests, five-theme all-control gallery,
  supplied-device WARP regressions and all 853 reusable inherited runtime cases. Record the 88 exclusions.
- Add revision-checked embedded text snapshots, application-side TSF/clipboard services, and lazy embedded UIA
  attach with synthetic tests. Close library extraction and the first RedXe pin/synthetic adapters (`redxe-adapter`).
  Matched performance and real IME/AT remain RedXe AV gates.

- Make this repository the canonical home of DxUi: editable src/Controls and Tests/Controls, neutral namespace,
  one frame runtime, historical provenance metadata and explicit pending dependencies; remove the duplicate tree.

- Bootstrap independent private repository, guidance, skills, design contracts, validators and build/CI entrypoints.
- Preserve source/test provenance from RedSalamander and extract application-independent frame-runtime foundation.
- Plan RedXe-first control/embedded-host integration; retain RedSalamander migration as a separate HOLD plan.
