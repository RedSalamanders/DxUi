- The Tooltip suite's timer fixtures no longer depend on wall-clock timing.
  - **The failure.** `TestTooltipLayerHideDelayExpiresAfterTimerTicks` scheduled the default 100 ms hide, pumped messages
    for 50 ms of wall time, and required the tooltip to be visible. A CI runner that stalled overran that margin and failed
    the check, on a pull request that did not touch tooltips.
  - **The cause.** The deadline is on the animation dispatcher's clock.
    - While the dispatcher is idle, that clock reads the wall clock, and its first tick restarts it from the wall clock. A
      stall between scheduling and that first tick therefore put the clock past the deadline at once. A 150 ms stall
      injected there reproduces the CI failure.
    - Once the dispatcher is running, each tick moves the clock by the time since the previous tick, clamped to 50 ms.
  - **What the hide-delay fixtures do now.** They keep the dispatcher ticking with a subscription of their own, take the
    deadline from its clock, and dispatch one message at a time. Each tick is therefore observed with the state it left.
  - **Hide delay.** With a delay of twice the clamp, at least one tick lands before the deadline. The tooltip must be visible
    after each such tick, and hidden after the first tick at or past the deadline.
  - **Pointer move.** The move-cancels-hide fixture moves the tooltip one tick into the delay, then requires it to stay
    visible once the clock passes the old deadline. Before, a stalled runner let it pass without testing the cancel.
  - **Passive tooltip lifetime.** The old check never tested the five-second display lifetime: with the old check in place, a
    6 s lifetime still passed.
    - It ran after a click that had already hidden the tooltip: mouse-up releases capture, and losing capture clears the
      tooltip.
    - Its window was never shown, and a host ticks its tooltip only while its window is visible, so the tick did nothing.
    - Even on a visible tooltip, its tick at `GetTickCount64()` plus 6 s mixed the wall clock with the dispatcher's. On a
      machine whose mouse hover time is about a second or longer, that tick lands before the lifetime ends.
  - **What the passive fixture does now.** It shows its window without activating it, and checks the lifetime before the
    click. The tooltip is shown with a tick past the longest show delay (2.5 s), and the lifetime is checked exactly from
    that tick: visible at 4,999 ms, hidden at 5,000 ms. The rest of the fixture is unchanged, apart from a message that now
    says the click hid the tooltip, not the lifetime.
  - **No new hang risk.** A ten-second limit ends a run whose ticks never come. Library behavior is unchanged.
