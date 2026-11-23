# W0.1 work counters — ComplexUi (FX5) baseline receipt

Plan item W0.1 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); counters
defined in [performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#work-counters).

- **Scenario**: the existing ComplexUi fixture (`Samples/ComplexUi/ComplexUiScene.h`): 83 controls in an `EmbeddedHost`,
  1280×720, 96 DPI, offscreen, reduced motion, 1,000-row model. 20 warm-up frames, then 40 clean frames (prepare and
  composite with nothing changed) and 40 dirty frames (`ComplexUiScene::Update(20 + n)`, the canonical benchmark's first
  dirty round). Per frame: the C++ allocation sink of `Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h` around prepare
  and composite, and the `Detail::WorkCounterScope` delta after a `ResourceLifetimeTracker` sweep (destructions observed,
  overflow 0). No timing is recorded; these are counts.
- **Command**: `DxUi.EmbeddedTests.exe --test-work-counters <receipt>` (also run, without a receipt, by the Embedded suite).
- **Source**: base `8e3dc0f0` plus the W0.1 working tree (uncommitted at capture; the commit carrying these files is the
  source). **Adapter**: WARP. **Compiler**: MSVC 19.52.37025 (`_MSC_FULL_VER` 195237025). **OS**: Windows 11 build 26300.
- **Files**: [Release](work-counters-complex-ui-release.json), [Debug](work-counters-complex-ui-debug.json),
  [ASan Debug](work-counters-complex-ui-asan-debug.json), all x64. Each counter reports per-frame min, max and total.
  `workloadOwner`, `baseCommit` and the SHA-256 `benchmarkInputs` (scene, allocation instrument, fixture and counter
  headers) were added to each executable-written receipt at capture; the counts are unchanged.

| Per frame (identical min and max in all 40 frames) | Clean | Dirty |
| --- | --- | --- |
| C++ allocations, Release | 0 | 54 |
| C++ allocations, Debug and ASan Debug (debug iterators) | 0 | 257 |
| `Paint` executions / paint-walk visits | 0 / 0 | 83 / 83 (every visible control) |
| Preparations / compositions | 0 / 1 | 1 / 1 |
| Pixels repainted | 0 | 921,600 (whole surface) |
| Resources created / churn | 0 / 0 | 56 / 56 |
| of which implicit `DrawText` layouts | 0 | 49 (created and released in the call) |
| of which `IDWriteTextLayout` | 0 | 7 (all released in the same frame) |

The Release dirty value matches the retained benchmark sink (2,160 allocations per 40 dirty frames in
`Measurements/EmbeddedInteractionLayout/Default-B1.json`). Debug and ASan Debug count one container proxy per standard
container, as the benchmark's separate Debug ceiling records. Counts are identical in all three configurations except the
allocation sink.

This receipt is the plan's baseline and is not rewritten. `W2.5` (the Grid column-description cache) lowers the Release
dirty value to 2; its [receipt](../W2.5/WorkCounters/README.md) is the value the Embedded gates now pin.

The second part of W0.1, the counting COM wrappers and the Direct2D draw-call count on the same fixture, has its own
[receipt](CountingWrappers/README.md).
