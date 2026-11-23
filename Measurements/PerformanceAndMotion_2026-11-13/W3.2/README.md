# W3.2 Caret clock

Plan item `W3.2` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contract: [window hosting, "Caret clock"](../../../Specs/Rendering/Rendering_Win32Host.md#caret-clock).

Source: the `W3.2` commit on the integration branch (receipts regenerated from its working tree, so they record
`uncommitted changes: True`), x64 Release, adapter WARP. `FX1` now runs with motion on (`Definition::reducedMotion =
false`) and pins the caret settings to Windows' defaults through the host's test seam (530 ms blink, 5 s caret timeout);
before this item its carets ignored both.

## FX1 counters, before and after

Sums over each phase; "before" is the receipt this item replaced (the `W3.1` source), "after" the regenerated one in
[FX1](../FX1/README.md). A flip is one caret `Tick` (`frame.tickCalls`).

| Host | Phase | Counter | Before | After |
| --- | --- | --- | --- | --- |
| embedded | editable | `frame.tickCalls` (caret flips) | 18 | 10 (the last at the 5 s caret timeout; 0 after it) |
| embedded | editable | `wakes.embeddedAdvance` (the fixture's polling application) | 100 | 54 (0 after the timeout: `NeedsAnimation` is false) |
| embedded | editable | `frame.preparations` / `frame.paintExecutions` / `frame.pixelsPainted` | 18 / 72 / 8,640,000 | 10 / 40 / 4,800,000 |
| embedded | readonly | `frame.tickCalls` / `wakes.embeddedAdvance` | 18 / 100 | 0 / 0 |
| embedded | readonly | `frame.preparations` / `frame.paintExecutions` | 19 / 76 | 1 / 4 (the focus change only) |
| embedded | multiline | `frame.tickCalls` / `wakes.embeddedAdvance` | 18 / 20 | 9 / 11 |
| embedded | multiline | `frame.preparations` / `frame.paintExecutions` | 19 / 76 | 9 / 36 |
| embedded | editable, readonly, multiline | `cppAllocations` | 55, 57, 70 | 31, 3, 35 |
| window | editable | `frame.tickCalls` / `frame.renders` / `wakes.paintMessage` | 18 / 18 / 18 | 10 / 10 / 10 |
| window | readonly | `frame.tickCalls` / `frame.renders` / `wakes.paintMessage` | 18 / 19 / 19 | 0 / 1 / 1 |
| window | multiline | `frame.tickCalls` / `frame.renders` / `wakes.paintMessage` | 18 / 19 / 19 | 9 / 9 / 9 |

Paints per flip, the baseline for `W3.7` and `W4`: every flip is one whole-surface preparation (embedded) or render
(window) of 4 paint executions and 480,000 pixels (800 x 600): the host cannot yet invalidate only the caret.
Resource creation, churn and the other counters move with the number of flips only.

The window fixture runner never dispatches a timer, so its wake counters stay 0; the native wakes are measured in real
time by `TestFrameScheduleNativeFx1CaretWakesInRealTime` (Animation suite): FX1's scene on a WARP `WindowHost` shown
without activation off every monitor, the editable field focused without native focus transfer. In x64 Debug: 10
deadline wakes, 0 periodic, 10 `Tick` calls and 11 paints in the first 6 s, then 0 wakes in the 4 s after the caret
timeout (before this item: 18 deadline wakes in 10 s). The same test asserts 0 wakes for a focused read-only field
(2 s), 0 while minimized, and on restore the caret shown at once with its first flip one period later (one wake, one
`Tick`, no held flip fired).

## FX13

[FX13](../FX13/README.md) runs with reduced motion (its definition), so its focused search field's caret is now steady:
`idle-focused-caret` `frame.tickCalls` 59 to 0, `wakes.embeddedAdvance` 60 to 0, preparations 8 to 1 (the focus change);
`caret-timer-period` `frame.tickCalls` 2 to 0, `wakes.embeddedAdvance` 100 to 0. The hardware receipt
(`fx13-minimized-hardware-…`) was not regenerated (WARP-only machine run) and still shows the old caret.

## Gallery

`TextField / Focused` and `TextField / Focused read-only` are new tiles, last on the sheet so that no other tile moves:
each is a focused `TextField` captured in a host of its own (the sheet's one focus belongs to the rich editor), with
the caret phase frozen by a steady blink time (`INFINITE` through the test seam), so the caret is drawn in its shown
phase whenever the capture runs. Captured with:

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json `
    -CompareAfter .build/gallery/x64/Release/tiles -Scale 100,150,200
```

Parity against the [G1 receipt](../G1/README.md), every tile in the five themes at 100, 150 and 200%: 1,260 of 1,290
tiles compared, all 1,260 that existed before identical (420 at each scale); the 30 new tiles have no "before". The
retained tiles are the two new ones in the five themes at the three scales, under [gallery-tiles](gallery-tiles)
(`<percent>/<theme>/<tile>.png`), with the capture's `receipt.json` (`-Tiles 'textfield-focused,textfield-focused-read-only'`;
source = this change's working tree on base `c030d60c`, forced WARP) and `tiles.tsv`.
