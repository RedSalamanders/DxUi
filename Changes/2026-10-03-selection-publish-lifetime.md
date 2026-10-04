- Tree and Grid stop using themselves when the accessibility publish that follows a selection destroys them: UI
  Automation event delivery dispatches messages, and a message may rebuild the controls before the publish returns.
  `Tree::SelectVisibleIndex` and `Grid::SelectRow` (private) now report a control that did not survive its publish as
  well as its delegate, so a tree's press, expander, double click, context menu, arrow keys and type-ahead, and a grid's
  press, double click and arrow keys, no longer expand, arm a drag, capture the mouse, invoke, activate a row or
  invalidate for a destroyed control. Ctrl+A in both, a tree's multi-select change and a grid's keyboard group collapse
  stop after their publish too, and `Tree::NotifyDataChanged` no longer calls `OnTreeSelectionSetChanged` for a tree its
  publish destroyed. `Tree::RequestSelectVisibleItem`, `RequestAddVisibleItemToSelection` and
  `RequestRemoveVisibleItemFromSelection` return false for a tree that did not survive them, so UI Automation reports
  the element gone.
  - **Grid selection delegate.** A grid no longer touches itself after `OnGridSelectionChanged` destroyed it during a
    group collapse (the Left key or a press on the group's header), `NotifyDataChanged`, `SetModel`, `SetSelectionMode`
    or a checkbox toggle whose model change moved the selection. `NotifyDataChanged`, `SetModel` and `SetSelectionMode`
    read the destroyed grid when they published after the delegate, and so did the key's group collapse.
  - **Tests.** With a client listening, the Accessibility suite replaces the window's root at the first selection event
    each publish path raises (`UiaTest::SelectionEventInterruption`), for a tree and a grid that fill their window and
    beside a button. In the Grid suite, `TestGridGroupCollapseSelectionDelegateReplacementStopsTheInput` and
    `TestGridSelectionDelegateReplacementDuringModelChangesStopsTheGrid` have the selection delegate replace the controls.
    Against main's library they fail (AddressSanitizer, or their result, capture or invalidation check), except the
    release of a click on a multi-selection, which main already stopped; each of the fourteen guards removed on its own
    fails a named test.
  - Specified in `UI_InputAndAccessibility.md`, with `Testing_Validation.md`, `docs/controls.md` and the request and
    helper comments of `DxUi.h`. No signature or visual changes.
