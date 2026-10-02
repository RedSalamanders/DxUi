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
