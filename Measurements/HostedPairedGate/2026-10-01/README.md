# Hosted paired gate calibration, 1 October 2026

An A/A set on a hosted runner: the same library code measured twice. It calibrates the
[hosted paired gate](../../../Specs/Core/Core_PerformanceAndResources.md#hosted-paired-gate), and answers two questions
the gate's design depends on: how still a hosted runner holds a binary between runs, and whether the same code, built
twice, is ever flagged.

- Run: the `paired-benchmark` job of workflow run
  [36881550868](https://github.com/RedSalamanders/DxUi/actions/runs/36881550868), dispatched on a temporary branch with
  baseline `e72cf62` (`main` after #43) and candidate `26281ec`, the default scenarios `Default`, `MultilineGrid` and
  `MultilineGridDistinct`, three repetitions each (six runs per side). The job took 12 min 40 s: the restore, two Release
  builds and 36 measured runs.
- A and B differ only in `.github/workflows/ci.yml` (the candidate skips the validation and native jobs on a dispatch, so
  the set did not queue seven other jobs on the shared runner pool). Both have library source fingerprint
  `3313B8F721…`. Their Release executables differ (A `03B8D4AE87…`, B `0CA7030881…`, built in two worktrees), so this is
  two builds of one source, not one binary run twice. The harness is the checkout's and is the commit's own, so no
  overlay was written: every receipt has benchmark fingerprint `F9D66318E8…`, fixture `dxui-complex-ui-v2` for `Default`.
- Runner `runnervmfi6oq`: Intel Xeon Platinum 8573C, Windows 10.0.26100, WARP 10.0.26100.33438, compiler 195136260,
  High performance power plan, x64 Release, 1280x720 at 96 DPI.
- Files: the 36 receipts, each with its own comparator output (`*.json.comparison.json`, unpaired), the 36 pass-by-pass
  comparisons and the runner's summary as `summary.receipt.txt`. `SHA256SUMS` covers every file here except itself and
  this README. The set is retained as the run wrote it; nothing was edited.

## Set verdicts

| Scenario | Set verdict | Regressed | Improved | Controls drifting (of 6) | Metrics every control held (of 26) |
|---|---|---:|---:|---:|---:|
| `Default` | `within-noise-budget` | 0 | 0 | 6 | 10 |
| `MultilineGrid` | `within-noise-budget` | 0 | 0 | 6 | 10 |
| `MultilineGridDistinct` | `within-noise-budget` | 0 | 0 | 6 | 17 |

No metric of identical library code is flagged in 78 metric tests. Shifts beyond a band occur, as the rank test
expects of noise, but not with p < 0.05: `Default` clean 95th-percentile composition CPU time is 28.2% higher
(0.0201 to 0.0257 ms, p = 0.70), clean frame p95 12.9% higher (p = 0.39). Two memory shifts are significant and inside
the 2% band: `Default` private bytes are 0.77% lower clean (p = 0.011) and 1.21% lower dirty (p = 0.015). That is what
the band is for, and the set calls neither a verdict. All exact budgets (surface bytes, replacement peak, C++
allocations, composition allocations) are equal in all 12 runs of every scenario.

## Same-binary controls

All 18 controls (A2 against A1 and B2 against B1 of three passes, in three scenarios) drift beyond a band in some metric,
in 1 to 13 of 26 (median 6), so every scenario's controls read `unstable-control` and a gate that required stable
controls overall would never pass a hosted run. The drift is not spread evenly:

- The first pass drifts about twice as much as the later two: its six controls move 12, 10, 13, 8, 8 and 1 metrics
  (mean 8.7), the other twelve 1 to 8 (mean 4.4).
- The metrics that drift in most controls are timings of a few milliseconds or microseconds: dirty preparation p95
  (13 of 18 controls), dirty frame p95 (12), composition CPU p95 (10 clean, 10 dirty), clean preparation p95 (9),
  dirty FPS (8) and dirty median frame time (7). Working set drifts in 3 to 4 controls and private bytes in 3 to 4.
- The eight exact metrics (surface bytes, replacement peak, C++ allocations and composition allocations, clean and
  dirty) never drift in any control. A deterministic budget is the one signal the runner gives without noise.

The gate therefore judges the controls of the metric a set flagged, not of all twenty-six together, and treats an exact
budget as confirmed when its controls are equal.

## Gate conclusion

`Tools/Publish-BenchmarkVerdict.ps1` reads this summary and its control comparisons and concludes `pass`: nothing
regressed, the library inputs of both sides are identical, and each scenario's summary counts the metrics whose
controls held their band. Under its strict reading, any unstable control making a scenario inconclusive, the same set is
inconclusive in all three scenarios. `Tools/tests/Test-BenchmarkGate.ps1` runs the conclusion over this set and over a
retained local one.

## A second set

The same two commits dispatched again ([`aa-2`](aa-2/README.md), on an AMD EPYC runner) flagged five clean-phase timings
of identical code (p 0.004 to 0.026), every one with a control beyond its band: the candidate was slow in five of its six
runs and the baseline in none. Per metric the gate reads that as inconclusive, not as a degradation, so a hosted run can
need a re-run. One of two A/A sets flagged; the pair cannot say how often.

## Limits

One set on one shared virtual machine, run while other jobs of the repository used the same runner pool. An A/A of 36
runs cannot give a false-positive rate for the gate; it shows only that this set raised no flag. The benchmark measures
offscreen WARP frames: presentation, hardware graphics and long-run retention remain outside it.
