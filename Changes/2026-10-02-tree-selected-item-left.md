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