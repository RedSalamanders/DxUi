# W0.1 counting wrappers — ComplexUi (FX5) cross-check, draw calls, pixel identity and cost

Second part of plan item W0.1 of the [performance and motion plan](../../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
wrappers defined in [performance and resources](../../../../Specs/Core/Core_PerformanceAndResources.md#work-counters). The
first part's receipt, without wrappers, is [one level up](../README.md).

- **Scenario**: the same ComplexUi fixture and frame schedule as the first receipt (83 controls, 1280×720, 96 DPI,
  offscreen WARP, reduced motion; 20 warm-up frames, 40 clean frames, 40 dirty frames `ComplexUiScene::Update(20 + n)`),
  with `NativeWorkTests::CountingWrappers` installed before the scene creates its `GraphicsDevice`. Each measured frame is
  read back and hashed after its counters are taken (FNV-1a over all 3,686,400 bytes of the composited target); the frame
  time is QPC around `Prepare` and `Composite` only, readback excluded.
- **Command**: `DxUi.EmbeddedTests.exe --test-counting-wrappers <receipt> <comparison>`. The receipt is the wrapped run;
  the comparison runs the fixture unwrapped, wrapped, wrapped, unwrapped in one process and requires every measured frame's
  pixels to equal the first unwrapped run's. The Embedded suite runs the same checks (one unwrapped and one wrapped run)
  and the seeded-mismatch test.
- **Source**: integration tip `c128bdb1` plus the W0.1 wrapper working tree (uncommitted at capture; the commit carrying these files
  is the source). **Adapter**: WARP. **Compiler**: MSVC 19.52.37025 (`_MSC_FULL_VER` 195237025). **OS**: Windows 11
  build 26300. All x64.
- **Files**: Release: [wrapped receipt, run 1](counting-wrappers-complex-ui-release-run1.json) and
  [run 2](counting-wrappers-complex-ui-release-run2.json), byte-identical; [comparison, run 1](wrapper-comparison-release-run1.json)
  and [run 2](wrapper-comparison-release-run2.json). Debug: [receipt](counting-wrappers-complex-ui-debug.json),
  [comparison](wrapper-comparison-debug.json). ASan Debug: [receipt](counting-wrappers-complex-ui-asan-debug.json),
  [comparison](wrapper-comparison-asan-debug.json). Each receipt holds every work counter per frame (min, max, total), the
  wrappers' creations by kind in the `wrappers` group, and under `countingWrappers` what was wrapped and setup's creations
  by kind, helpers beside wrappers. `workloadOwner`, `baseCommit` and the SHA-256 `benchmarkInputs` (scene, allocation
  instrument, fixture, wrapper and counter headers, as committed) were added to each executable-written file at capture;
  the counts are unchanged.

## Wrappers against helpers (identical in every configuration and both Release runs)

| Created per frame (min = max in all 40 frames) | Clean: helpers / wrappers | Dirty: helpers / wrappers |
| --- | --- | --- |
| implicit `DrawText` layouts | 0 / 0 | 49 / 49 |
| `IDWriteTextLayout` | 0 / 0 | 7 / 7 |
| text formats, brushes, gradient stops, bitmaps, effects, command lists, layers, geometries, stroke styles, device contexts | 0 / 0 | 0 / 0 |
| other kinds, escaped interfaces, unwrapped render targets (wrappers only) | 0 | 0 |

Setup (scene initialization through the 20 warm-up frames), helpers / wrappers: 1 / 1 bitmap (the surface, from a DXGI
surface), 22 / 22 brushes, 1 / 1 device context, 980 / 980 implicit `DrawText` layouts, 16 / 16 text formats,
134 / 134 text layouts. The wrappers also saw one object of another kind, the Direct2D device, which the helpers do not
count. One factory, one device, one device context and one DirectWrite factory were wrapped; none escaped by
`QueryInterface`, and all were released when the scene was (`liveAfterTeardown` 0).

The Embedded suite's seeded test paints a control that creates a brush directly on the host's device context. Its frame
reports exactly one mismatch, `brush`: helpers 0, wrappers 1, with 2 draw calls (the host's `Clear` and the probe's
`FillRectangle`).

## Draw calls

A dirty frame makes **399** Direct2D draw calls (min = max = 399, 15,960 over 40 frames), a clean frame **0**, in every
configuration and in both consecutive Release runs. Without the wrappers the counter reads 0 on every frame.

## Pixels and cost

Every measured frame's composited pixels are identical with and without the wrappers (80 frames per run, four runs per
comparison; the last dirty frame hashes to 2808861114653577386 in Release, Debug and ASan Debug alike).

Median Prepare plus Composite time per run, Release, in run order unwrapped, wrapped, wrapped, unwrapped:

| Comparison | Dirty unwrapped (ms) | Dirty wrapped (ms) | Clean unwrapped (ms) | Clean wrapped (ms) |
| --- | --- | --- | --- | --- |
| Run 1 | 1.303, 1.314 | 1.377, 1.300 | 0.0299, 0.0277 | 0.0257, 0.0238 |
| Run 2 | 1.279, 1.358 | 1.204, 1.226 | 0.0283, 0.0263 | 0.0404, 0.0263 |

Here the wrapped dirty medians average 3% below the unwrapped ones. Two earlier pairs of comparisons on the same machine,
before the final rebase, averaged 4% below and 3% above (the sign changes from set to set). The
wrapped and unwrapped medians overlap within run-to-run spread: no cost is measurable at this resolution (each call
through a wrapper adds one virtual forward and, for a draw or a creation, one increment). Cost is therefore not what
decides where they run. They run only in dedicated tests so that ordinary suites keep exercising the shipping path with
real Direct2D and DirectWrite objects; one unwrapped and one wrapped hashed run and the seeded test add about one second
to the Embedded suite in Release (the four-run comparison takes 2.0 s).
