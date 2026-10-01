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
