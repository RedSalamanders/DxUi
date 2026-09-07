# DxUi complex-UI measurement, 2026-09-07 (embedded surface lifetime round)

Independent library evidence for the `dxui-complex-ui-v2` workload (83 controls, 1,000 synthetic Grid/Tree records,
1280x720 at 96 DPI, reduced motion, 20 warm-up frames, five 40-frame rounds per clean/dirty scenario) measured
around the [embedded surface lifetime round](../../../Specs/Plans/Done/EmbeddedSurfaceLifetime_2026-09-07.md).
The harness gained allocation ceilings in this round, so its fixture hash
(`9DCFBC39C2F57B76F9106D268FD5B9F382E5A7950E85EE33BC4069DF9C64D461`) differs from the
[2026-09-05 evidence](../2026-09-05/README.md); the two folders are not comparable to each other.
No RedXe or RedSalamander code, settings, plugin or device service is loaded.

## Files

All receipts: x64, WARP 10.0.26100.9278, Ryzen 9 9950X3D, Windows 10.0.26200, Balanced power, `workloadOwner: DxUi`.

| File | What it is |
| --- | --- |
| `baseline-Release.json`, `baseline-Debug.json` | Previous implementation (commit `7b571b73`) built with the final harness and measured before the implementation change (library fingerprint `42087213…`, executables `285B6B1D…` / `456D7B45…`). |
| `rebaseline-Debug.json` | The same previous implementation rebuilt in an isolated copy and measured again immediately before a candidate run, under the same desktop conditions (executable `058A8749…`). |
| `environment-probe-Debug.json` | An earlier candidate build measured while the desktop was busy: a 3x slowdown of clean frames that never touch changed code; the `advice-required` comparison is kept as the record of that environment. The same slowdown reproduced with the baseline binary. |
| `candidate-Release.json`, `candidate-Debug.json` | Final candidate binaries (library fingerprint `0840340D…`, executables `1DA3736E…` / `783862B7…`), measured at the start of the Menu receipt runs. |
| `scene.png` | Rendered workload from the candidate, captured outside timing after warm-up. |

## Five-round medians of the retained receipts

| Receipt | Scenario | FPS | Frame p50 | Frame p95 | Prepare p95 | C++ allocations | Surface / peak bytes | Private bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline Release | clean | 2,288.8 | 0.421 ms | 0.481 ms | 0.0003 ms | 0 | 3,686,400 / 3,686,400 | 26,316,800 |
| Baseline Release | dirty | 487.0 | 2.019 ms | 2.257 ms | 1.617 ms | 2,160 | 3,686,400 / 3,686,400 | 27,930,624 |
| Candidate Release | clean | 2,075.5 | 0.468 ms | 0.524 ms | 0.0003 ms | 0 | 3,686,400 / 3,686,400 | 26,685,440 |
| Candidate Release | dirty | 465.2 | 2.099 ms | 2.471 ms | 1.673 ms | 2,160 | 3,686,400 / 3,686,400 | 27,230,208 |
| Baseline Debug | clean | 2,302.0 | 0.416 ms | 0.539 ms | 0.001 ms | 0 | 3,686,400 / 3,686,400 | 29,908,992 |
| Baseline Debug | dirty | 407.1 | 2.386 ms | 2.896 ms | 2.037 ms | 10,280 | 3,686,400 / 3,686,400 | 31,522,816 |
| Rebaseline Debug | clean | 2,087.4 | 0.450 ms | 0.539 ms | 0.0011 ms | 0 | 3,686,400 / 3,686,400 | 28,962,816 |
| Rebaseline Debug | dirty | 407.7 | 2.386 ms | 2.750 ms | 2.022 ms | 10,280 | 3,686,400 / 3,686,400 | 30,388,224 |
| Candidate Debug | clean | 2,134.6 | 0.455 ms | 0.564 ms | 0.001 ms | 0 | 3,686,400 / 3,686,400 | 29,487,104 |
| Candidate Debug | dirty | 411.1 | 2.417 ms | 2.689 ms | 2.010 ms | 10,280 | 3,686,400 / 3,686,400 | 30,490,624 |

The full `test.ps1` suite runs (embedded in `.build/reports/<Suite>-x64-<Configuration>.json`, executables
`1DA3736E…` / `783862B7…`) measured the candidate at Release 2,230.7 clean / 498.6 dirty FPS (prepare p95 1.602 ms)
and Debug 2,173.4 / 403.8 FPS (prepare p95 2.074 ms). Allocation counts, surface bytes and the replacement peak are
identical to the baseline in every run, and hidden preparation/composition stayed zero.

## Paired comparisons and why they are not accepted as a regression

Every `compare_performance.py` run of a candidate against a retained baseline returned `advice-required`, each time
on different sub-band metrics while the desktop was actively in use (browser, capture overlays, Task Manager):

| Candidate run | Baseline | Flagged (median before -> after) |
| --- | --- | ---: |
| Debug, busy desktop (`environment-probe-Debug.json`) | baseline-Debug | clean FPS 2,302 -> 712; reproduced with the baseline binary, so environmental |
| Debug | baseline-Debug | clean p50 0.416 -> 0.448 ms; compose CPU p95 13 -> 15 µs; dirty prepare p95 2.04 -> 2.26 ms |
| Debug | rebaseline-Debug | timings within band; private bytes 28.96 -> 29.76 MB (+2.7%) |
| Debug (repeat) | baseline-Debug | clean p50 0.416 -> 0.442 ms; compose CPU p95 13 -> 14 µs and 12 -> 13 µs |
| Release | baseline-Release | clean p95 0.481 -> 0.511 ms; private bytes 26.32 -> 27.52 MB (+4.6%) |

FPS medians were within the 5% band in every paired run. Compose-CPU p95 is a 12-15 µs quantity, so a 1 µs shift is
a 8% "regression"; private bytes vary by up to 4.5% between rounds of one run on this desktop, above the 2% band.

To separate the environment from the change, both binaries were run alternately in the same state
(`DxUi.EmbeddedTests.exe --benchmark`, raw rounds under `.build/reports/abab`; those raw files carry no receipt
identity and are therefore not retained here). Five-round medians per run:

| Configuration | Order | Baseline clean FPS | Candidate clean FPS | Baseline dirty FPS | Candidate dirty FPS | Baseline dirty prepare p95 | Candidate dirty prepare p95 |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Release | B,C x4 | 2,105.8 / 2,289.6 / 2,195.7 / 2,169.3 | 2,372.8 / 2,186.7 / 2,254.1 / 2,097.9 | 497.0 / 480.9 / 495.1 / 470.3 | 501.7 / 503.2 / 484.7 / 499.5 | 1.64 / 1.69 / 1.63 / 1.74 ms | 1.62 / 1.53 / 1.70 / 1.55 ms |
| Debug | B,C x4 | 2,406.9 / 2,143.9 / 2,111.0 / 2,138.8 | 2,217.4 / 2,354.7 / 2,119.7 / 2,317.9 | 412.5 / 414.5 / 416.8 / 405.4 | 392.7 / 400.4 / 407.2 / 395.3 | 2.02 / 1.99 / 2.01 / 2.25 ms | 2.16 / 2.08 / 2.07 / 2.10 ms |
| Debug | C,B x3 | 2,145.1 / 2,225.9 / 2,306.2 | 2,173.5 / 2,067.9 / 2,158.0 | 392.3 / 408.2 / 403.1 | 403.9 / 397.0 / 404.5 | 2.17 / 2.00 / 2.03 ms | 2.14 / 2.10 / 2.03 ms |

Means: Release clean 2,190 vs 2,228 FPS and dirty 486 vs 497 FPS; Debug clean 2,200 vs 2,253 FPS; Debug dirty 412
vs 399 FPS when the candidate ran second and 401 vs 401 FPS when it ran first. The direction flips with the run
order and the differences stay inside round-to-round variance, so there is no measurable regression. This is not an
authorization to relax the bands or replace the baseline: the `test.ps1` suite receipts of this round are `unpaired`,
and a `within-noise-budget` receipt needs `test.ps1 -PerformanceBaseline` against `baseline-<Configuration>.json`
on a quiet interactive desktop.

## Limits

Completed offscreen WARP frames including target clear and a blocking one-pixel readback; no presentation, vsync,
hardware GPU, input latency or long-run retention claim. The desktop was in use during all candidate measurements;
the receipts record that state through their round-to-round variance rather than hiding it. Do not compare these
numbers to the 2026-09-05 fixture hash or use them as RedXe AV acceptance.

Reproduce from the repository root with `performance.ps1 -Configuration <Configuration> -Platform x64 -OutputPath
<new-file> -Baseline Measurements/ComplexUi/2026-09-07/baseline-<Configuration>.json`. See
[the protocol](../../../docs/performance.md).
