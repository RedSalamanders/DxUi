- A paced `WindowHost` now dispatches at most one frame-owing pointer move per presented frame without delaying an idle
  window (performance and motion plan item W3.7, [pointer-move pacing](../Specs/Rendering/Rendering_Win32Host.md#pointer-move-pacing)).
  A move that arrives while the host holds its frame-latency credit and owes no frame is dispatched at once and its frame
  renders at the next `WM_PAINT`. A move that arrives while a frame is in flight waits, merged with later moves, and the
  next paced frame dispatches the latest position before any `Tick`. A second move that arrives before the previous move's
  frame rendered renders that frame first, never from inside a render, a paced frame, a tick wake or another of the
  host's handlers. Every button, wheel, key, capture, focus, activation, menu or size-move loop, hide and minimize message,
  every scheduled wake and every synchronous render whose caller reads the result dispatches a pending move first, so the
  order of events is kept. `WM_SETCURSOR` does not, and the cursor follows the latest point. A pending move always has a
  wake: the pacer's wait, or a posted `WindowHost.PointerMoveFlush` message whose identity makes it inert after a detach
  or destroy. Unpaced and hidden hosts dispatch at once; a stalled compositor dispatches at the first 250 ms wait timeout.
  Keyboard input is not coalesced and the embedded host is unchanged. New work counters (`pointerMoveDispatches`,
  `pointerMovesNoFrame`, `pointerMovesCoalesced`, `pointerMoveOwedRenders`, `pointerFlushesStale`) and a
  trace event, `InputDispatched`, which carries the first and the latest move a dispatch absorbed with their arrival times,
  so input to present is measured from the first. The window performance fixtures now take a frame credit before each
  step's input, so a step's moves are dispatched the same way in every run. Validated on WARP by the WindowHost suite's
  `TestPointerPacing*` tests. The hardware clause (input to present within one refresh interval at p99, at 60 and 144 Hz)
  remains open: run `TestPointerPacingInputToPresentOnHardware` with `DXUI_W37_HARDWARE_GATE=1` in a hardware slot.
