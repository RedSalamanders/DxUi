# W3.3 tooltip deadlines

Plan item `W3.3` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contract: [tooltip deadlines](../../../Specs/Rendering/Rendering_Win32Host.md#tooltip-deadlines). Gate: on `FX14`, wakes
from the tooltip at most 2 per show (one show, one hide) and 0 `Tick` calls between the deadlines, on the embedded host
through the fixture and on a native window in real time.

- **Source**: the W3.3 change on base `7d39fef72` (receipts and the Release, Debug and ASan Debug test runs; first measured on
  bases `67f4761aa` and `a03782610` with identical tooltip counters), uncommitted at measurement time; the commit that carries this file.
- **Build**: x64 Release (receipts, real time), x64 Debug and x64 ASan Debug (same tests).
- **Adapter**: WARP (`FX14` receipts; the native test window forces WARP and sits off every monitor, shown without activation).

## FX14 through the fixture clock

Receipts: [FX14](../FX14/README.md), regenerated with `performance-fixtures.ps1 -Fixture FX14` (embedded with
`-CountDrawCalls`, window plain). `FX14` gained a `hide` phase (the pointer rests until the 5 s display lifetime ends).
`frame.tooltipDeadlineTicks` counts the host tooltip layer's ticks, each at a reached deadline; before this item the
layer was ticked at every wake while a show was pending or a tooltip shown, and that was not counted.

| Phase | Tooltip ticks before (inferred from code) | Tooltip ticks after (embedded, window) | Control `Tick` calls after | Embedded advances after |
| --- | --- | --- | --- | --- |
| `enter`, `pending` | 0 (the fixture advances no time) | 0, 0 | 0 | 0 |
| `show` | 2 (the +1 ms baseline advance and the +10 s advance) | 1, 1 | 0 | 2 (the first only sets the view's timeline baseline) |
| `shown` | 0 (no time advanced) | 0, 0 | 0 | 0 |
| `hide` (new) | not in the old fixture; every wake for 5 s in real time | 1, 1 | 0 | 1 |
| `leave` | 0 | 0, 0 | 0 | 0 |

Every counter of the phases the old receipt had is unchanged for `warm` to `shown`. `leave` now paints and renders
nothing, because the tooltip it used to clear was already hidden by `hide`; that repaint (2 paints, one render, one
implicit text layout) moved to `hide`. Memory lines are recorded as the receipts report them and are not attributed to this item.

No tooltip fade or motion exists (the layer paints at full opacity, `TooltipLayer::Paint`), so there are no animation
frames to separate from the two waiting wakes.

## Native window in real time

`TestTooltipDeadlinesWakeOnlyAtShowAndHideInRealTime` (Tooltip suite): `FX14`'s scene, hover time 400 ms through the test
seam, real dispatcher wakes (`wakes.deadlineTimer`, `wakes.animationTimer`), the 50 pending and 50 shown moves of the
hover path sent as `WM_MOUSEMOVE`.

| Interval | Deadline wakes | Periodic wakes | Tooltip ticks | Control `Tick` calls |
| --- | --- | --- | --- | --- |
| waiting for the show | 1 | 0 | 1 | 0 |
| shown, until the hide | 1 | 0 | 1 | 0 |
| 1 s after the hide | 0 | 0 | 0 | 0 |

Measured identically in x64 Debug, Release and ASan Debug. Before this item the tooltip held a `Continuous` request while
pending and while shown, so the periodic timer (8 ms, raised by Windows to its 10 ms minimum) ran for the hover delay
plus the 5 s display lifetime: between about 345 (the system's 15.6 ms timer resolution, the plan's estimate) and 540
(10 ms) periodic wakes per tooltip at a 400 ms hover time, inferred from the code, not measured on the old build.
