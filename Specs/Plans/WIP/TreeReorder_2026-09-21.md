# Tree multi-select and drag-reorder

- **Status**: ACTIVE (drag-reorder and multi-select are both in `Tree` on branch `improve/tree-multi-select`; merge,
  the republished gallery and design system, and RedPrism's pin bump remain)
- **Owner**: DxUi `Tree` (`include/DxUi/DxUi.h`, `src/Controls`). RedPrism consumes the result with a pin bump.
  It must not grow a second tree widget.
- **Why**: RedPrism's layers panel is a `Tree`. `ITreeDelegate` could select, invoke, expand, and show a context
  menu. It could not multi-select or drag a row. `TabControl` and `Grid` already drag-reorder. Group Selected
  Layers and Merge Layers in RedPrism already accept several layer ids; the panel could only select one, and
  reorder stays on the keyboard (`Ctrl+]` / `Ctrl+[`).

## Contract to add

Normative text lives in [Controls and layout](../../UI/UI_ControlsAndLayout.md#tree-multi-select) and
[Input and accessibility](../../UI/UI_InputAndAccessibility.md); this plan records what was built and proved.

- [x] Modifier click extends the selection. The model stays UI-thread only. `Tree::SetMultiSelectEnabled(true)` (off by
  default, silent) makes the selection a set held in `GridSelectionModel`: click selects one and sets the anchor,
  Ctrl+click toggles, Shift+click selects the visible range from the anchor; the keys are Shift+movement (range),
  Ctrl+movement (focus alone), Ctrl+Space (toggle) and Ctrl+A. The focused item (`GetSelectedItemId`,
  `GetFocusedItemId`) and the set (`GetSelectedItemIds`, `IsItemSelected`, silent `SetSelectedItemIds`) are distinct.
  A tree that never enables it is unchanged: every existing Tree, Accessibility and Control test passes untouched.
- [x] A pointer drag previews an insertion line. Release commits once (source id, insert-before id, parent id).
  Escape or capture loss cancels. Already in the library (`SetReorderEnabled`, `OnTreeReorder(TreeDrop)`); with
  multi-select the contract is unchanged (one source, the dragged row): a press with Ctrl or Shift never drags, a press
  on a row of a multi-selection keeps the selection while the pointer is down so the delegate can move all of
  `GetSelectedItemIds` when the source belongs to it, and a click that never became a drag selects that row alone on
  release. No multi-item drop is invented.
- [x] `ITreeDelegate` gains the reorder callback and the multi-select change callback: `OnTreeReorder` (before) and
  `OnTreeSelectionSetChanged(std::span<const uint64_t>)` (default no-op, once per change, after `OnTreeSelectionChanged`
  of the same gesture, from gestures, UI Automation requests and model changes that drop selected rows).
  `OnTreeSelectionChanged` keeps its meaning for the focused item. Do not put a document or layer type in the library:
  none was.
- [x] UI Automation: the Selection pattern reports `CanSelectMultiple` and lists the selected items, each item reports
  `IsSelected`, `Select`/`AddToSelection`/`RemoveFromSelection`/`SetFocus` work, and `ElementSelected`,
  `ElementAddedToSelection`, `ElementRemovedFromSelection`, `Selection_Invalidated` (and the `IsSelected` property
  change) are raised for window hosts and embedded hosts. A single-select tree reports and raises what it did.
- [x] Catalog interaction tests and a gallery tile: Tree suite (14 new tests), Accessibility suite (2 new tests), the
  embedded suite (`TestEmbeddedTreeMultiSelect`), and the gallery's `Tree / Multi-select` tile (several rows selected,
  not adjacent, all four rows visible). `test.ps1` in x64 Debug, Release and ASan Debug, plus the three ARM64 builds:
  see the evidence below.

## Design decisions to review

- Keyboard follows the list-view convention for the two keys where Grid differs (Grid has no separate focus: its current
  row is its last selected row, Ctrl+Up/Down toggle the neighbour, Space toggles a checkbox cell): Ctrl+movement moves
  the focus and Ctrl+Space toggles it. Shift, Ctrl+click, Ctrl+A, the anchor and the right-click rule are Grid's.
- Selected rows that leave the visible rows (removed, or hidden by a collapsed ancestor) leave the selection at the
  next `NotifyDataChanged`/`SetModel`, as the single selection always did; `ITreeModel` exposes only visible rows, so a
  hidden selected row cannot be told from a removed one.
- Events follow WPF's selector rules (one new selected item: `ElementSelected`; otherwise per-item added/removed; over
  20 or a vanished item: `Selection_Invalidated`).
- Membership tests in the set are linear in its size (Grid's too); Ctrl+A on a very large tree costs an O(n) walk of the
  visible rows for every gesture that changes membership, as `NotifyDataChanged` already does per call.

## Known limits (not caused by this change)

- In a window whose only semantic control is the tree, the UI Automation parent chain of a tree item ends at a
  `Control` provider with no parent (`ResolveSnapshotNavigationTarget` returns the collapsed root's path for an item's
  parent, and nothing for that provider's parent), so a client scoped to the window did not receive the events raised
  on the items there (observed for the selection events). Every window with a second
  semantic control (a label, a button) delivers them. The Accessibility test therefore adds a Label to its window.
  Making an item's parent the canonical root in a collapsed-root window, and the root's first child that control's
  first item, is a navigation change of its own.
- An in-process UI Automation client hears each event twice, the second a moment later; the test waits the stream out.
- The embedded selection events (`EmbeddedHost::UpdateAccessibility` raising the same `RaiseTreeSelectionEvents`) have no
  client harness: the repository has no UI Automation client that reaches an embedded host's provider, so the embedded
  test proves the patterns, the state and the callbacks, and the events are proved on the window-host path only.
- `Tree::Paint` does not clip a row that straddles the content rectangle, so a selected (or hovered) row scrolled half
  out of view paints its fill past the tree's frame. It is older than this change and applies to a single selection
  too; the gallery tile is tall enough to show every row instead.
- `test.ps1` skips the real-client focus fixtures (among them `TestWindowHostTreeArrowAnnouncesTheFocusedItem`) when it
  cannot take the foreground, as it could not here (8 to 9 capability skips in NewControls). Those fixtures did not run
  against this change. The tree's focus fragment is still its focused item, so a multi-select tree announces the row the
  keys reach, but no test here shows a client hearing it.

## Evidence

Branch `improve/tree-multi-select`, tested on the final tree (every command PowerShell, no benchmarks).

| Test | It proves | A mutant it fails |
| --- | --- | --- |
| Tree `TestTreeMultiSelectIsOptInAndSingleSelectIsUnchanged` | Off by default: modifiers, Ctrl+A, Ctrl+Space, the setters and typeahead keep their single meaning and no set callback is made | default on; Ctrl+Space toggling; Ctrl chord dropped from typeahead |
| `TestTreeCtrlAndShiftClickBuildTheSelectionInVisibleOrder` (and `...ThroughTheHostMessages`, through `WM_LBUTTONDOWN` flags) | Click, Ctrl+click, Shift+click across a group boundary and upward, the anchor, the model order, one callback per change | Ctrl or Shift replacing; set unsorted; anchor taken from the focus; duplicate set reports |
| `TestTreeKeyboardExtendsTogglesAndSelectsAll` (and `...ThroughTheHostMessages`, through key messages) | Shift+Up/Down/Home/End/Page Down extend, Ctrl+movement moves the focus, Ctrl+Space toggles, Ctrl+A selects all, the Ctrl+Space character is not typeahead | Shift not extending; Ctrl+movement toggling; Ctrl+Space or Ctrl+A unhandled; character guard removed |
| `TestTreeKeyboardGesturesStartFromTheFirstRowWhenNothingIsFocused` | A first key focuses row 0 silently and its own gesture then decides the selection | first-key step selecting |
| `TestTreeGroupKeysFollowTheModifierRulesAndLeaveTheSelectionWhenTheyExpand` | Left/Right move to the parent or first row by the movement rules, and expanding or collapsing never touches the selection | Left or Right ignoring Shift and Ctrl; a collapse selecting the group |
| `TestTreeSelectionCallbacksFireOncePerChangeAndSeeTheFinishedSelection` | Call order (`OnTreeSelectionChanged` then the set), no call for an unchanged set, state final inside both, UI Automation requests and the silent setters | set before row; callbacks before the state; unchanged set reported |
| `TestTreeSelectionSetCallbackMayDestroyTheTree` | A delegate that destroys the tree in the set callback, from each gesture and from a collapsing release, leaves nothing touched afterwards | lifetime check after the call removed (heap-use-after-free under ASan) |
| `TestTreeMultiSelectionSurvivesModelChangesExpandCollapseAndScrolling` | Hidden or removed rows leave the selection once, the rest follow the model's order, a moved row is no change, `SetModel`, scrolling | no reconcile; no notification; order ignored; order-sensitive comparison |
| `TestTreeMultiSelectPaintsEverySelectedRowWithTheSelectionColors` | Every selected row takes the selection colors (inactive fill when unfocused), only the focused row owns the ring; sampled from a real capture | painting the focused row only; ring on selected rows |
| `TestTreeDragReorderKeepsItsSourceAndTheMultiSelection` | One source, one `OnTreeReorder`, the selection kept while pressed and on Escape or capture loss, a click collapses on release | immediate collapse; Ctrl press dragging; no collapse; Escape or capture loss collapsing |
| `TestTreeMultiSelectContextMenuExpanderAndDoubleClickKeepTheSelectionRules` | Right-click on a selected row keeps the set, the expander and a modified double-click leave it alone | right-click replacing; expander selecting; Ctrl double-click toggling |
| `TestTreeSelectionSettersAndModeSwitchKeepTheSelectionCoherent` | `SetSelectedItemIds`, `SetFocusedItemId`, switching the mode both ways | setter selecting; focus lost on disabling; invisible ids kept |
| Accessibility `TestAccessibilityTreeMultiSelectExposesSelectionPatternsAndItemState` | `CanSelectMultiple`, `GetSelection` in order, per-item `IsSelected`, only the focused item has keyboard focus, Select, AddToSelection (not a toggle), RemoveFromSelection, SetFocus | `CanSelectMultiple` false; single `GetSelection`; Select adding; focus equal to selection |
| `TestAccessibilityTreeMultiSelectRaisesSelectionEvents` | An in-process client hears `ElementSelected`, per-item added and removed with the `IsSelected` change, one `Selection_Invalidated` over 20 items or for a vanished item, nothing for moved rows or a single-select tree | no events; one new item not reported as selected; no threshold; removal events for a replacement |
| Embedded `TestEmbeddedTreeMultiSelect` | Pointer and key modifiers, `CanSelectMultiple`, the selection and its actions through an embedded view | Ctrl or Shift click replacing; Ctrl+A; `CanSelectMultiple`; AddToSelection |

- Falsification: 52 single-point mutants (37 in `Tree`, 15 in UI Automation), each a run-time switch in a throwaway
  build that was never committed and each removing one behavior of the list above (Ctrl+click replacing the selection,
  the set callback before the row callback, no lifetime check after the set callback, `CanSelectMultiple` false, no
  events). Every test in the table failed at least one of them in x64 Debug; the destroy test's mutant is a
  heap-use-after-free under ASan Debug. The matrix exposed two gaps, both closed: the fixture always enabled the mode, so
  a tree that defaulted to multi-select passed the opt-in test (the fixture now leaves a default tree alone), and no row
  of the tests began with a space, so the Ctrl+Space character reaching typeahead went unseen (one does now).
- x64 `test.ps1 -Suites Tree,Accessibility,NewControls,Control,Embedded`: Debug, Release and ASan Debug pass (the Menu and
  NativeTextInput suites were not run). ARM64 Debug, Release and ASan Debug build.
- `format.ps1 -Check` and `validate.ps1` pass.
- `test.ps1` reports unpaired complex-UI FPS and memory. Two runs of this tree's x64 Release build gave clean 488 to
  1130 FPS, dirty 123 to 387 FPS and 24.5 to 26.6 MB private: the FPS moved with what else ran on the machine. No
  paired baseline was measured, so this change's effect on them is not established by those numbers. The multi-select
  code is behind `_multiSelect`: a tree that never enables it adds one branch on the flag per painted row, per key or
  pointer message and per published snapshot, and allocates nothing more.
- Gallery: `gallery.ps1 -Configuration Release -SkipBuild -OutputDirectory <scratch>` renders the `Tree / Multi-select`
  tile in all five themes (Controls, Inputs and Themes selected, Buttons not, unfocused so the inactive fill, per-row
  hues in the rainbow themes). `docs/gallery` is not regenerated here.

## STOP

Editing `LocalizedAdaptiveLayout` as a side effect. Resetting a consumer checkout. Shipping the gesture only
inside RedPrism.
