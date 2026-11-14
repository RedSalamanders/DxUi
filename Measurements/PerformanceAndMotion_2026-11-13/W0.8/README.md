# W0.8 trace provider receipt

Item W0.8 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); contract in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#trace-provider), schema in
[performance](../../../docs/performance.md#trace-provider).

- **Source**: base `fda11e04` plus the W0.8 change (the commit whose subject ends in `(W0.8)`), x64, Visual Studio 18
  Insiders, MSVC 14.52.37025, Windows 11 build 26300.
- **Adapter**: WARP for the ComplexUi fixture; the `WindowHost` scenario uses the host's default device on an offscreen,
  non-activating window.
- **Counts, not timing.** The latencies below include the swap chain's wait for the next vertical blank of an offscreen
  window and are recorded only to show input-to-present is computable per input; they are no `G8` verdict.

## ComplexUi dirty frames (FX5), `TraceProviderComplexUi` in the Embedded suite

40 dirty frames with nothing listening, then 40 with a test sink listening, after 20 warm-up frames.

| Configuration | C++ allocations per dirty frame, nothing listening | with a sink listening | Records for 40 traced frames |
| --- | --- | --- | --- |
| Release | 54 | 54 | 120 (prepare begin, prepare end, composite per frame) |
| Debug | 257 | 257 | 120 |
| ASan Debug | 257 | 257 | 120 |

The W0.1 receipt records the same 54 (Release) and 257 (Debug, ASan Debug): the provider adds no allocation.

## Grid scrolling in a `WindowHost`, `TestTraceProviderPairsEveryGridScrollInputWithItsPresent` (Rendering suite)

A 2,048-row, 4-column `Grid` (the scenario later fixture FX3 extends): 100 wheel notches, 20 pointer moves and 10 key
presses (down and up), each followed by a synchronous paint.

| Configuration | Input events | Inputs sent | Present events | Present counter | Wheel input-to-present p50 / max |
| --- | --- | --- | --- | --- | --- |
| Debug | 140 | 140 | 120 | 120 | 8,257 / 12,310 µs |
| Release | 140 | 140 | 120 | 120 | 8,273 / 8,456 µs |
| ASan Debug | 140 | 140 | 120 | 120 | 8,274 / 8,457 µs |

## ETW read-back, `TestTraceProviderEventsReadBackThroughEtw`

This desktop session (medium integrity, not elevated) could start a user-mode real-time ETW session. Ten wheel notches
and one key press wrote 62 records to the sink; the session delivered the same 62 events, equal field for field (name,
host, frame, input, detail, timestamp, thread), in Debug, Release and ASan Debug.
