# Local I19 follow-up, 13 September 2026

No hosted CI was used. These receipts belong to the library; product adoption
measurements remain in RedSalamander and RedXe.

The quiet x64 Release run completed all 18 suites with exit code zero. Menu records
nine interactive-desktop capability skips; the other 17 suites record none. Each
suite report and log is retained in the
[qualification evidence](../../../Specs/Plans/Done/SharedLibraryReadiness/local-2026-09-13/README.md).
Successful exit does not qualify skipped paths
or erase the earlier Release Menu failure. The tested control executable hash is
`51AC2C57629289AD65772E9D37A56F92569ACD47CDC08858C88619C0BA1058C0`.

## Longer matched complex-UI measurements

`long-pairs/` retains 16 raw receipts, all eight candidate/baseline comparisons,
six repeated-baseline comparisons and a descriptive summary. Collection metadata
is retained with the qualification evidence linked above.
Baseline `d192e474e540adc2656e69b8e8150b0f7a05d63f` and candidate
`13788e95f1c9a16210a87988d742d9dfc6f66eef` received the same recorded fixture
patch in isolated worktrees. No canonical library implementation was edited for
measurement. The qualification evidence's `dxui-long-fixture-preparation.json` records the earlier build stage,
four executable hashes, exact three-file patch and comparator validation.

The named `dxui-complex-ui-v3-long` fixture keeps the same 83 controls, 1,000 rows,
1280 by 720 surface, 96 DPI and WARP renderer. It uses 240 warmup frames followed
by five rounds of 600 frames per scenario. Both processes inherit processor mask
255. Debug then Release each ran A1/B1/B2/A2/A3/B3/B4/A4, serially, with no builds
or other test workloads. The patch changes only fixture duration/identity,
validation of that specific duration, and equal build scheduling. Timing, memory,
surface and allocation thresholds are unchanged. Do not compare these receipts
to the earlier 40-frame fixture as if the workloads matched.

| Median across four runs | Debug baseline → candidate | Release baseline → candidate |
|---|---|---|
| Clean completed FPS | 1166.01 → 1160.95 (-0.43%) | 1167.18 → 1170.95 (+0.32%) |
| Dirty completed FPS | 366.04 → 359.99 (-1.65%) | 449.67 → 449.78 (+0.02%) |
| Dirty preparation p95 | 2.0067 → 2.0275 ms (+1.04%) | 1.4217 → 1.4288 ms (+0.50%) |
| Dirty composition CPU p95 | 0.00825 → 0.00840 ms (+1.82%) | 0.00680 → 0.00685 ms (+0.74%) |
| Dirty private bytes | 23,658,496 → 23,961,600 (+1.28%) | 20,959,232 → 21,456,896 (+2.37%) |

All receipts retain zero composition allocations and zero hidden work; per-round
allocation counts and 3,686,400 surface bytes are unchanged between revisions.
These deterministic results do not establish process-memory or timing acceptance.

Acceptance remains **open**: four of four Debug and three of four Release paired
comparisons exceed at least one unchanged band. All three repeated-baseline
comparisons in each profile also exceed a band. The Debug clean preparation
median is only 0.35 versus 0.40 microseconds; individual relative changes are large
at that scale. Release clean peak private bytes rise a median 731,136 bytes
(3.25%), exceeding the band in three pairs. Preserve that signal along with the
baseline variation; do not silently dismiss it or rebaseline. The summary medians
are descriptive and do not override individual comparator results.

Measurements cover completed offscreen WARP frames with readback. They do not
measure presentation, hardware-GPU performance or application-level behavior.
