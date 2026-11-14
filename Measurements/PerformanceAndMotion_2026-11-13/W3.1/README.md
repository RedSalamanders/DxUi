# W3.1 One frame scheduler per host

Plan item `W3.1` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contract: [window hosting, "Frame schedule"](../../../Specs/Rendering/Rendering_Win32Host.md#frame-schedule).

## G1 on the native host in real time

Scenario: `TestFrameScheduleNativeFx1CaretWakesInRealTime` (Animation suite of `DxUi.ControlTests.exe`) builds FX1's own
scene (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx1.h`) on a WARP `WindowHost` shown without activation off every
monitor, focuses the editable field with FX1's own focus step (no native focus transfer) and pumps its messages for
10 s of real time, counting the dispatcher's real timer wakes (`wakes.deadlineTimer` + `wakes.animationTimer`). Build:
x64 Debug, Release and ASan Debug (18 deadline wakes in each), source = the `W3.1` commit. Adapter: WARP (`forceWarp`).

| Measurement (10 s, focused editable `TextField`) | Before W3.1 | After W3.1 |
| --- | --- | --- |
| Dispatcher timer wakes | about 1,000 (8 ms `SetTimer`, raised to the 10 ms minimum; inferred, `FX1` README) | 18 one-shot deadline wakes, 0 periodic |
| `Tick` calls | one tree walk of 4 controls per wake | 18, one per caret phase flip |
| `WM_PAINT` | 18-19 (one per flip) | 19 |

The same test asserts 0 timer wakes with nothing armed (2 s), while minimized, with the native presentation hidden and
after blur (1.5 s each), at most one wake for a window hidden by `SW_HIDE` (the wake that observes it), and one wake
carrying one `Tick` for the deadline that expired while minimized.

## Fixture counters

The window fixture runner never dispatches a timer, so its wake counters stay 0 before and after; the schedule's work
shows in `frame.tickCalls` and `frame.tickRequesters` (new) and in `frame.treeVisitsTick`, which a scheduled wake no
longer increments. Receipts regenerated with `performance-fixtures.ps1` (x64 Release, WARP) where a counter moved;
sums over each phase, before (receipt at the base commit) and after:

| Fixture, host | Phase | Counter | Before | After | Why |
| --- | --- | --- | --- | --- | --- |
| FX1 embedded | editable | `frame.treeVisitsTick` | 400 | 4 | one tree walk of 4 controls per tick before; now only the one discovery walk of the new root, which the first advance runs |
| FX1 embedded | editable / readonly / multiline | `frame.tickCalls` = `frame.tickRequesters` | (new) | 18 / 18 / 18 | one `Tick` per caret phase flip (530 ms) |
| FX1 embedded | readonly / multiline | `frame.treeVisitsTick` | 400 / 80 | 0 / 0 | no tree walk |
| FX1 embedded | warm, editable | `cppAllocations` | 19, 54 | 20, 55 | the schedule's request and snapshot vectors allocate once on first use and are reused |
| FX1 window | editable / readonly / multiline | `frame.treeVisitsTick` | 400 / 400 / 80 | 0 | as above; `tickCalls` 18 each |
| FX2 embedded | open | `cppAllocations` | 9 | 10 | first schedule vector allocation (caret request) |
| FX7 embedded | warm | `cppAllocations` | 207 | 208 | first schedule vector allocation |
| FX11 embedded / window | every motion phase | `frame.treeVisitsTick` | 84-1,392 per phase | 0 | the tree walk per tick is replaced by ticks of the animating control only: `tickCalls` = `tickRequesters` (6-79 per phase); preparations, renders and pixels unchanged |
| FX11 window | warm, toggle, checkbox, popup phases | `frame.treeVisitsTick` | 96-1,152 | 0 | nothing requested in those phases, so no wake ticks anything |
| FX11 embedded | warm | `frame.tickDiscoveryWalks` | (new) | 1 | the root's discovery walk |
| FX11 embedded | page-slide-30hz, progress-slide-60hz | `cppAllocations` | 153, 590 | 155, 592 | the schedule vectors' first allocation in the host that phase builds |
| FX13 embedded | idle-nothing-armed | `frame.tickDiscoveryWalks` | (new) | 1 | as above |
| FX13 embedded | idle-focused-caret | `frame.treeVisitsTick`; `tickCalls` | 2,100; - | 0; 59 | 35-control walk per tick replaced by one caret `Tick` per due flip (1 s steps) |
| FX13 embedded | idle-focused-caret | `cppAllocations` | 1 | 3 | the two schedule vectors' first allocation |
| FX13 embedded | caret-timer-period | `frame.treeVisitsTick`; `tickCalls` | 3,500; - | 0; 2 | 100 ticks of the 10 ms period, 2 of them at a due flip |
| FX14 embedded / window | show | `frame.treeVisitsTick` | 4 / 4 | 2 / 0 | the tooltip's own request replaces the tree walk; the embedded 2 is the root's discovery walk |

Pixels, preparations, renders, presents, resources and every other counter are unchanged on every fixture. The
regenerated receipts also carry counters that later items added after the previous receipts were written (`frame.shadowPasses`,
`frame.d2dDevices`, `frame.swapChains`, `rich.chunksLaid`, `rich.preparationTurns`); they are not this item's. Other
receipt differences observed when regenerating (FX7 `switch`/`richtext` allocations and the newer `rich.*` counters, FX10
and FX12 window `renderingParams`, `documentFlattens`, and FX12 window renders, presents and paints halved) were
reproduced by the base commit's own Release build and belong to earlier items (W6.4, W7.4); those receipts are left
as they were, except FX7, which is regenerated whole for its `warm` allocation. FX9's only change is the new
`frame.tickDiscoveryWalks` = 1 in its `grid` phase (the discovery walk it already counted in `treeVisitsTick`); its
receipts are left as they were.

Not reproduced by this change: `TestNativeIncrementalParityInteraction` (Rendering) and
`TestNativeAccessibilityFocusCallbackReplacementStopsOriginalAction` (Accessibility) fail identically on the base
commit's build on this desktop.
