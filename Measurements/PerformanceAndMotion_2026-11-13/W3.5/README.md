# W3.5 embedded animation request (API revision 6)

Plan item `W3.5` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contract: [embedded D3D11 hosting](../../../Specs/Rendering/Rendering_EmbeddedD3D11.md) ("Animation request"). Gate: a
scheduler simulation on embedded `FX1` with at most 2 application wakes per blink period, 0 when idle, 0 while hidden;
`FX14` at most 2 application wakes per tooltip show; `FX13` idle and hidden 0 application wakes.

- **Source**: the W3.5 change on base `9574105be`, uncommitted at measurement time; the commit that carries this file.
- **Build**: x64 Release ([application-wakes-x64-release.txt](application-wakes-x64-release.txt), the output of
  `DxUi.EmbeddedTests.exe --test-animation-request`); the same tests pass in x64 Debug and ASan Debug.
- **Adapter**: WARP. Counters and the fixture clock only; no timing.

## Scenario

Each fixture runs twice on its own definition through `RunEmbedded`, with one of two simulated applications
(`ApplicationLoop` in `Tests/Embedded/Embedded.Tests.PerformanceFixtures.h`):

- **Before: the polling application.** At each fixture step it advances once whenever the view requests anything. That
  is the best revision 5 allowed (`NeedsAnimation()` is a `bool`), and it is the application of every fixture receipt, so
  `wakes.embeddedAdvance` keeps its meaning. A real polling application wakes every frame (16 ms at 60 Hz) while anything
  waits, which the fixture's 100 ms, 500 ms and 1 s steps understate.
- **After: the application that sleeps until the request.** It knows only the request it last read: the one its own last
  `AdvanceAnimation` returned, or one it read again because the view's `requestPreparation` callback woke it (or because
  it hid the view itself). It sleeps on the fixture clock until that `Deadline` (exactly at `dueTickMs`), advances once
  per step while `Continuous`, and never while `None`; input from the fixture's steps arrives at the fixture time between
  deadlines, and the view's clock (`EmbeddedCallbacks::nowTickMs`) is the fixture clock. A *wake* is an advance it slept
  until; an advance made at once in a turn it was already awake for (the baseline after a pause) is counted as an
  advance only. Never a real timer. Measured again after the W3.5 review fixes (the required application clock, the
  record of the reported request, the strict simulation): the same numbers.

## Application wakes

| Fixture, phase | Before: polling advances | After: wakes (advances) | Tick calls before / after |
| --- | --- | --- | --- |
| `FX1` editable (10 s, caret 530 ms, 5 s timeout) | 54 | 11 (11): 1 discovery frame, 10 caret flips; at most 2 in any 1,060 ms blink period; 0 after the timeout | 10 / 10 |
| `FX1` read-only | 0 | 0 (0) | 0 / 0 |
| `FX1` multiline (500 ms steps) | 11 | 10 (11): 10 flips plus one baseline advance | 9 / 10 |
| `FX1` editable refocused, then hidden 10 s | 0 while hidden | 0 while hidden; request `None` | 0 / 0 |
| `FX13` idle, nothing armed (60 s) | 1 (the new root's discovery) | 1 (1), then 0 | 0 / 0 |
| `FX13` idle, focused caret (reduced motion: steady) | 0 | 0 | 0 / 0 |
| `FX13` hidden 60 s after the phases | 0 | 0 | 0 / 0 |
| `FX14` show (+1 ms, +10 s) | 2: discovery, show | 3: discovery, show at +400 ms, hide 5 s later | 1 tooltip tick / 2 |
| `FX14` hide (+5 s) | 1: hide | 1: the second show (the shown phase's moves scheduled it) | 1 / 1 |

`FX14` differs only because the sleeping application wakes exactly at each deadline: its first tooltip hides inside the
show step, so the 50 moves of the shown phase schedule a second show. Every tooltip deadline is one wake (3 wakes for 3
deadlines, 2 shows), a show's cycle is two wakes 5 s apart, and nothing wakes between them.

## What an advance costs

Per advance (`ApplicationAdvance` records the `WorkCounterScope` delta of each `AdvanceAnimation`):

- every advance ticks exactly the due requests (`tickCalls == tickRequesters`), and each deadline wake of the sleeping
  application ticks something (a control or the tooltip);
- an advance before the earliest deadline returns that deadline without running a wake: 0 `Tick` calls, 0 tick tree
  visits and 0 tooltip target walks (`tooltipTargetValidations`, `W3.3`'s counter of the hit-test walk and title lookup
  that resolves a tooltip's target, which is no `Tick`). The polling application made 46 such advances on `FX1`; before
  this item each ran a wake (a snapshot of the schedule); now each does none. A tooltip target marked stale (`W3.3`)
  makes the schedule due at once, so such an advance is never skipped, and the view wakes a sleeping application for it;
- tick tree visits happen only in the discovery tick of a new root (4 on `FX1`, 35 on `FX13`, 2 on `FX14`), as before.

## Fixture receipts

The fixture receipts were not regenerated. On the base `9574105be`, a Release `--fixture-receipt` run of `FX1` and
`FX14` (with draw calls) and `FX13` with this change matched every counter series of the committed embedded receipts
([FX1](../FX1/README.md), [FX13](../FX13/README.md), [FX14](../FX14/README.md)) step for step, `cppAllocations` included.
Rebased onto `42e6e34ce`, `FX13` and `FX14` still match; `FX1` differs only in paint allocations, created text layouts
and its two new multiline phases, which `W2.7` (text field paint) changed and which this item does not touch: its wake,
advance and tick counters (`wakes.embeddedAdvance`, `frame.tickCalls`, `frame.tickRequesters`, `frame.treeVisitsTick`)
are unchanged. The only other new values are counters those receipts predate (`W3.3`'s `frame.tooltipTargetMarks` and
`frame.tooltipTargetValidations`, 1 in `FX14` show and 1 in hide).
