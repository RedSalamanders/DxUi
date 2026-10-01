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