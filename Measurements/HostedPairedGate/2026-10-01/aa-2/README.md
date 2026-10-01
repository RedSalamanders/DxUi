# Second hosted A/A set: a flagged set of identical code, 1 October 2026

A second A/A set of the [hosted paired gate](../../../../Specs/Core/Core_PerformanceAndResources.md#hosted-paired-gate)'s
calibration (the [first](../README.md) flagged nothing): the same two commits, dispatched again on a hosted runner. It
shows how a flag appears on identical library code, and what the gate's per-metric controls make of it.

- Run: the `paired-benchmark` job of workflow run
  [36887285573](https://github.com/RedSalamanders/DxUi/actions/runs/36887285573) (9.9 minutes), baseline `e72cf62`,
  candidate `26281ec`, the three default scenarios, three repetitions. Both sides have library source fingerprint
  `3313B8F721…` and different Release executables (A `C04BCD8AF6…`, B `0195A3497E…`), as in the first set. Every receipt
  has benchmark fingerprint `F9D66318E8…` (fixture `dxui-complex-ui-v2` for `Default`).
- Runner `runnervmfi6oq`: AMD EPYC 9V45 (the first set ran on an Intel Xeon Platinum 8573C), Windows 10.0.26100, WARP
  10.0.26100.33438, compiler 195136260, High performance power plan, x64 Release.
- Files: `reports.zip` holds the 109 files the run wrote, byte for byte (the 36 receipts, their comparator outputs, the 36
  pass-by-pass comparisons and the runner's summary as `summary.receipt.txt`), as in the first set. `SHA256SUMS` has the
  archive's hash. Nothing was edited.

## Set verdicts

| Scenario | Set verdict | Regressed | Controls drifting (of 6) | Metrics every control held (of 26) |
|---|---|---:|---:|---:|
| `Default` | `advice-required` | 4 | 6 | 11 |
| `MultilineGrid` | `advice-required` | 1 | 6 | 10 |
| `MultilineGridDistinct` | `within-noise-budget` | 0 | 6 | 18 |

Five metrics of identical library code are `regressed`, all clean-phase timings, all with p < 0.05 and beyond their band:

| Scenario | Metric | A median | B median | Change | p | Same-binary controls, largest drift |
|---|---|---:|---:|---:|---:|---:|
| `Default` | clean FPS | 478.2 | 410.8 | -14.1% | 0.026 | 23.2% |
| `Default` | clean frame p50 | 2.045 ms | 2.340 ms | +14.4% | 0.026 | 16.1% |
| `Default` | clean frame p95 | 2.227 ms | 2.983 ms | +33.9% | 0.0043 | 35.6% |
| `Default` | clean composition CPU p95 | 0.0149 ms | 0.0213 ms | +42.6% | 0.015 | 52.0% |
| `MultilineGrid` | clean frame p50 | 2.057 ms | 2.235 ms | +8.7% | 0.026 | 19.0% |

The run medians in schedule order show where they come from. `Default` clean FPS: A1 476.5, B1 387.4, B2 388.2, A2 479.8,
A3 488.1, B3 389.6, B4 480.1, A4 492.5, A5 465.9, B5 447.1, B6 432.0, A6 476.1. The candidate is slow in five of its six
runs (B1, B2, B3, B5, B6) and fast in the sixth, while the baseline stays fast in all six, which separates the ranks as a change
would. Whether the slow runs are bursts that fell into the candidate's slots or a difference between the two builds, the set
cannot say. The exact budgets are equal in all 36 runs.

## Same-binary controls

All 18 controls drift beyond a band in some metric (1 to 11 of 26, median 5.5). The five flagged metrics each had a
control beyond its band: in `Default` clean FPS, B4 against B3 moved +23.2%, because the candidate's own runs differ. The gate
therefore reads each of the five as *inconclusive* (a flag the controls cannot tell from the runner); on its own
identical-library-inputs rule it lists them as noise and passes the run. `Tools/tests/Test-BenchmarkGate.ps1` runs both
readings over this set.

## Limits

One set on one shared machine. It shows that a hosted run of unchanged code can be flagged, and that the same-binary
controls drifted for every flagged metric here, not that they always will: a slowdown that lasted a whole side, with every
control inside its band, would read as a confirmed degradation. A re-run on another runner, or a confirmation repeat inside the job, is the
contract's repeat.
