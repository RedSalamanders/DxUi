- Published UI Automation snapshots incrementally (plan W8.2). A generation shares every record, chunk and table its
  changes did not touch: a name, value or focus change re-reads that control's record, a geometry change its subtree's
  records and the bounding rectangles, and everything else is shared with the generation before. On `FX6` with one
  client, 100 `Slider` steps build no whole window and re-read one record each, and the 200 px `Splitter` drag builds
  once, at the release. Every incremental publication of the tests is compared field by field with a whole build, and
  the event diff skips what two generations share, so clients hear the events a whole build raises.
  - `Slider` steps, a `Splitter` preview, `TextField::SetText`, `ProgressBar` values, keyboard focus moves, every
    `SetBounds` and every hit-test change of a control no longer build the whole window; a structure, state or model
    change, or any change the host cannot account for (a hover or animation of another control), still does.
  - A layout that moves more controls than a host keeps marks for (32) collapses their geometry marks onto their common
    parent instead of building the whole window, so a `Splitter` drag that lays out a pane stays incremental. With no
    client interested, a native window is dirtied once per publication and later marks skip the refresh; a client that
    arrives reads the current values and bounds. Marks are cleared when a target is registered, unregistered,
    disconnected or dropped, so a flush owed before a detach no longer silences the next attachment.
  - Geometry-only publications are coalesced: at most one per 100 ms of the host's clock plus one when the layout
    settles, through the host's existing frame schedule (an embedded view reports an animation request due at once). A
    frame moving every control of a 2,000-control form cost about 11 ms to publish with one client; a deferred frame costs
    under 0.01 ms. Name, value, focus, state and structure changes still publish at once. A window host that presents no
    frame (hidden, minimized, presentation turned off, hidden through an ancestor) never holds a deferral, and a UI
    Automation client's query during one asks the window's thread to publish, so its next read is current.
  - A resized `Splitter` publishes its clamped position and range; a moved pane of sliders stays incremental; and a
    `Grid`, `Tree`, `TabControl`, `FlexPanel`, `FormField` or `FormPanel` resize no longer builds the whole window.
  - A name or value change of a control that lays out its own elements (a `TabControl` title, a `Grid`, a `Tree`, a
    `ComboBox`, a masked field) rebuilds the bounding rectangles, which those elements' rectangles follow.
  - Fixed: on a window host, `Control::SetBounds`, scroll offsets and a `ComboBox` popup's placement did not republish UI
    Automation bounding rectangles until another change did; the incremental-versus-fresh oracle no longer exempts it.
  - A window host's publication that only unheard changes led to (made while no client was requested or listening, and
    published by a later query) now becomes the event baseline. This closes the path a review of the client gate
    described; the in-process scenario test passes with and without the rule.
