# Slider appearance and acknowledgement measurements

The original implementation is `d192e474e540adc2656e69b8e8150b0f7a05d63f`; the candidate source is
`3f50bbcc531e3057571a29d2718d2147ac6bad25`. Initial receipts predate that local commit and therefore identify
the original HEAD plus `sourceDirty` and the candidate source fingerprint. Original Debug was measured after
the WIP document was created but before compiled source changed; its compiled-source fingerprint matches the
original Release receipt.

All measurements use the unchanged `dxui-complex-ui-v2` synthetic scene: 1280×720, 96 DPI, 83 controls, 1,000
model rows, five rounds of 40 completed offscreen WARP frames, on the same x64 machine/toolchain/power policy.
The receipts contain full environment, fixture, source and executable identities. These are offscreen frame
rates with readback, not displayed FPS or touchscreen latency.

## Evidence retained

`baseline-release.json` and `baseline-debug.json` are the pre-change receipts. The initial candidate Release
runs and comparisons are retained as `candidate-release-1/2/3/4*`; they flagged varying memory/timing metrics.
The original Release implementation was rebuilt in an isolated detached checkout and repeated as
`baseline-release-repeat*`. This clone has one unchanged compiled-input file differing only in newline bytes,
which explains the different raw source fingerprint; normalized contents otherwise match the original source.

`paired-before-1..5.json` and `paired-after-1..5.json` are five alternating original/candidate Release runs,
executed serially. All raw samples remain available; the original baseline was not replaced or tuned to pass.
The following values are medians of the five run medians, used to assess repeatability, without changing the
automated comparator or its investigation bands.

| Scenario / metric | Original | Candidate | Change |
| --- | ---: | ---: | ---: |
| Clean completed offscreen FPS | 2,575.01 | 2,649.69 | +2.90% |
| Dirty completed offscreen FPS | 569.82 | 571.56 | +0.30% |
| Clean p95 frame, ms | 0.4508 | 0.4469 | −0.86% |
| Dirty p95 frame, ms | 2.0047 | 1.9418 | −3.14% |
| Dirty p95 preparation, ms | 1.3235 | 1.2746 | −3.70% |
| Clean private bytes | 27,009,024 | 27,512,832 | +503,808 / +1.865% |
| Dirty private bytes | 28,286,976 | 28,557,312 | +270,336 / +0.956% |
| Cached surface bytes, both scenarios | 3,686,400 | 3,686,400 | unchanged |
| Clean / composition C++ allocations | 0 | 0 | unchanged |
| Dirty C++ allocations per 40-frame round | 2,160 | 2,160 | unchanged |

There is no consistent Release timing regression in these alternating runs. This does not establish a speedup.
Clean private-byte run medians range from 25,423,872 to 27,299,840 before and 26,726,400 to 27,881,472 after.
The process-level metric includes Windows graphics/runtime heaps, not just DxUi's C++ allocations and surface.
Changing the thumb geometry could alter graphics-cache residency, and allocator/process variation is also visible;
the precise contributor was not isolated. There is no added surface, drawing operation, timer, worker, or measured
C++ allocation. These short runs cannot exclude a leak or replace a long retention test.

The user explicitly chose to retain the improved appearance with the approximately 0.5 MiB clean-memory caveat.
That narrow accepted tradeoff is durable in [the performance contract](../../../Specs/Core/Core_PerformanceAndResources.md).
Other investigation bands and deterministic budgets are unchanged.

Initial Debug comparisons and five candidate repeats are also retained. The original was then rebuilt in Debug
and alternated with the candidate in `paired-debug-before/after-1..5.json`. Both implementations were substantially
slower than the early baseline on this desktop; the candidate was not consistently slower than the contemporaneous
original. Per-run variation was large, so these results establish neither a speedup nor displayed-frame acceptance.
The aggregate medians were clean FPS 1,944.14 → 2,340.50 and dirty FPS 399.58 → 417.87; clean private bytes
29,048,832 → 29,392,896 (+1.18%), dirty private bytes 30,810,112 → 31,526,912 (+716,800 bytes / +2.33%).
The Debug memory medians also varied in both directions across pairs. No C++ allocation count or surface budget
changed. The original timing flags do not establish a code-caused slowdown; all flags and per-pair comparisons
are retained, with no threshold changes. The last pair finished as the next RedXe build command started; compilation
began after the benchmark, and no concurrent compiler workload was used for these pairs.

Functional test runs were executed separately without a comparison argument
so that an investigation flag did not prevent the suites from running. Their `unpaired` label is not used as
performance acceptance. Both configurations passed all 18 suites; Debug/Release Menu recorded 8/9 checks requiring
an interactive desktop. ARM64 Debug and Release cross-builds passed; no native ARM64 runtime result is claimed.

All five themed galleries and the embedded example were regenerated and visually inspected. The existing
consumer hardware/IME/assistive-technology and presented-frame performance gates remain separate.

The Release relocated-consumer check passed with the exact candidate pin, public-only sample headers, independent
outputs, three rendered samples, and five rejected invalid pins. The candidate compiled-source fingerprint remained
`D45F118C41B79357EC64D9502690BAEB2FE1927301C655D44FBE4CC6534E2AFE` throughout validation and documentation closeout.
