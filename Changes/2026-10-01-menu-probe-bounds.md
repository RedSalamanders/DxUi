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
