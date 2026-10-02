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
