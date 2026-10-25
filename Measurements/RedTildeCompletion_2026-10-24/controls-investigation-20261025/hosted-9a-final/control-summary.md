# Hosted 9a final control replay: curated projection

## Verdict and identity

Provider verdict: **inconclusive**. Its explicit reason is `policy-review-required`: the base policy is not independently qualified, so the verdict cannot establish a candidate pass. The three scenario set records each say `within-noise-budget` with zero regressed metrics; the top-level verdict has no scenario entries. This distinction matters: the same-side replay below diagnoses control instability and does not replace the provider verdict.

Base commit: `b06f534b36ffc2c0c9671a142d1e51545480be22`. Measured candidate commit for event `refs/pull/100/merge`: `7c8f2d88c8069d31c93f1d831536e279fd69dcdd`. Source fingerprints: baseline `0BA64DA69F28AEC624D9E5A104FA777DC82AC2FE6680CDD52EA7F230E762BE52`; candidate `F423000D1AD799EDBAA3EE17B121480C5AE567CC75E2BCE2AF0D783AC7AB26F1`. The packet does not expose a separate head SHA field.

## Same-side controls

All 144 receipt files matched this packet’s own `summary.json` scenario/run roster. Their SHA-256 values were computed and retained in the private audit, with provenance anchored by the independently verified artifact ZIP digest. Independent replay checked the raw medians, signed changes, and 72 control statuses.

- 71/72 controls flagged at least one metric; 1/72 remained stable.
- 248/1,872 metric observations breached their recorded investigation bands.
- 40/78 metric keys held in all 24 controls; 0/78 breached in all 24.
- The most frequently breached key was `MultilineGrid/clean/composeCpuP95Ms` at 17/24; `MultilineGridDistinct/clean/composeCpuP95Ms` also breached 17/24. Maximum was 17/24, not all controls.

Each scenario/phase/metric row has 24 same-side comparisons (12 blocks × 2 roles). Median and p95 absolute movement are descriptive. Full 78-row table:

| Scenario / phase / metric | N | Breached | Held | Median signed % | P95 abs % | Min to max signed % | + / - / 0 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Default/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/clean/composeCpuP95Ms | 24 | 14 | 10 | -0.251 | 15.179 | -21.992 to 13.472 | 10 / 12 / 2 |
| Default/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/clean/fps | 24 | 0 | 24 | 0.054 | 2.120 | -3.339 to 2.120 | 15 / 9 / 0 |
| Default/clean/frameP50Ms | 24 | 0 | 24 | 0.033 | 0.313 | -0.209 to 0.313 | 14 / 10 / 0 |
| Default/clean/frameP95Ms | 24 | 1 | 23 | -0.027 | 2.434 | -2.434 to 17.405 | 11 / 13 / 0 |
| Default/clean/prepareP95Ms | 24 | 5 | 19 | 0.000 | 12.500 | -22.222 to 0.000 | 0 / 5 / 19 |
| Default/clean/privateBytes | 24 | 0 | 24 | 0.113 | 1.212 | -1.368 to 1.212 | 13 / 11 / 0 |
| Default/clean/privatePeakBytes | 24 | 0 | 24 | 0.122 | 1.212 | -1.230 to 1.212 | 13 / 11 / 0 |
| Default/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/clean/workingSetBytes | 24 | 2 | 22 | -0.050 | 2.591 | -3.994 to 0.552 | 11 / 13 / 0 |
| Default/clean/workingSetPeakBytes | 24 | 2 | 22 | -0.028 | 2.601 | -4.012 to 0.540 | 11 / 12 / 1 |
| Default/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/dirty/composeCpuP95Ms | 24 | 6 | 18 | 0.735 | 10.949 | -10.667 to 17.647 | 12 / 11 / 1 |
| Default/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/fps | 24 | 5 | 19 | 0.320 | 12.926 | -13.270 to 7.311 | 15 / 9 / 0 |
| Default/dirty/frameP50Ms | 24 | 0 | 24 | -0.045 | 1.170 | -3.428 to 0.598 | 10 / 14 / 0 |
| Default/dirty/frameP95Ms | 24 | 9 | 15 | -0.844 | 17.516 | -24.220 to 12.953 | 8 / 16 / 0 |
| Default/dirty/prepareP95Ms | 24 | 7 | 17 | -0.323 | 23.320 | -23.881 to 8.106 | 11 / 13 / 0 |
| Default/dirty/privateBytes | 24 | 1 | 23 | 0.113 | 1.912 | -2.744 to 1.912 | 14 / 10 / 0 |
| Default/dirty/privatePeakBytes | 24 | 2 | 22 | 0.301 | 2.310 | -2.710 to 2.310 | 16 / 7 / 1 |
| Default/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/workingSetBytes | 24 | 1 | 23 | 0.082 | 1.282 | -4.975 to 1.054 | 17 / 7 / 0 |
| Default/dirty/workingSetPeakBytes | 24 | 1 | 23 | 0.120 | 1.272 | -4.965 to 1.020 | 14 / 10 / 0 |
| MultilineGrid/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/clean/composeCpuP95Ms | 24 | 17 | 7 | 1.439 | 20.000 | -87.206 to 20.000 | 12 / 12 / 0 |
| MultilineGrid/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/clean/fps | 24 | 3 | 21 | 0.145 | 22.705 | -4.503 to 28.304 | 13 / 11 / 0 |
| MultilineGrid/clean/frameP50Ms | 24 | 3 | 21 | 0.002 | 18.448 | -18.517 to 4.459 | 12 / 11 / 1 |
| MultilineGrid/clean/frameP95Ms | 24 | 4 | 20 | -0.562 | 19.766 | -43.565 to 19.766 | 8 / 16 / 0 |
| MultilineGrid/clean/prepareP95Ms | 24 | 11 | 13 | 0.000 | 22.222 | -22.222 to 28.571 | 5 / 6 / 13 |
| MultilineGrid/clean/privateBytes | 24 | 4 | 20 | -0.028 | 2.719 | -3.307 to 2.248 | 11 / 13 / 0 |
| MultilineGrid/clean/privatePeakBytes | 24 | 4 | 20 | -0.021 | 2.798 | -3.275 to 2.260 | 10 / 12 / 2 |
| MultilineGrid/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/clean/workingSetBytes | 24 | 2 | 22 | 0.171 | 2.114 | -3.397 to 1.961 | 14 / 10 / 0 |
| MultilineGrid/clean/workingSetPeakBytes | 24 | 2 | 22 | 0.151 | 2.122 | -3.423 to 1.960 | 14 / 10 / 0 |
| MultilineGrid/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/dirty/composeCpuP95Ms | 24 | 10 | 14 | -2.172 | 10.870 | -10.870 to 12.658 | 9 / 15 / 0 |
| MultilineGrid/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/fps | 24 | 11 | 13 | -3.063 | 13.113 | -8.441 to 15.354 | 6 / 18 / 0 |
| MultilineGrid/dirty/frameP50Ms | 24 | 3 | 21 | 0.313 | 9.724 | -11.829 to 4.499 | 13 / 11 / 0 |
| MultilineGrid/dirty/frameP95Ms | 24 | 9 | 15 | -1.773 | 16.530 | -29.273 to 10.138 | 9 / 15 / 0 |
| MultilineGrid/dirty/prepareP95Ms | 24 | 9 | 15 | -0.445 | 19.968 | -19.968 to 21.182 | 10 / 14 / 0 |
| MultilineGrid/dirty/privateBytes | 24 | 16 | 8 | -1.134 | 12.498 | -5.902 to 13.543 | 11 / 13 / 0 |
| MultilineGrid/dirty/privatePeakBytes | 24 | 8 | 16 | -0.320 | 3.360 | -2.352 to 3.387 | 11 / 13 / 0 |
| MultilineGrid/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/workingSetBytes | 24 | 12 | 12 | 0.395 | 5.333 | -4.590 to 10.481 | 13 / 11 / 0 |
| MultilineGrid/dirty/workingSetPeakBytes | 24 | 1 | 23 | 0.209 | 1.569 | -3.533 to 1.072 | 16 / 8 / 0 |
| MultilineGridDistinct/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/clean/composeCpuP95Ms | 24 | 17 | 7 | -4.807 | 18.026 | -20.565 to 17.098 | 10 / 14 / 0 |
| MultilineGridDistinct/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/clean/fps | 24 | 0 | 24 | -0.094 | 2.784 | -2.784 to 4.470 | 11 / 13 / 0 |
| MultilineGridDistinct/clean/frameP50Ms | 24 | 0 | 24 | -0.014 | 0.206 | -4.460 to 0.206 | 9 / 15 / 0 |
| MultilineGridDistinct/clean/frameP95Ms | 24 | 1 | 23 | 0.142 | 4.462 | -4.462 to 5.524 | 13 / 11 / 0 |
| MultilineGridDistinct/clean/prepareP95Ms | 24 | 15 | 9 | 0.000 | 14.286 | -12.500 to 28.571 | 6 / 9 / 9 |
| MultilineGridDistinct/clean/privateBytes | 24 | 0 | 24 | 0.357 | 1.042 | -0.820 to 1.356 | 16 / 7 / 1 |
| MultilineGridDistinct/clean/privatePeakBytes | 24 | 0 | 24 | 0.373 | 1.007 | -0.897 to 1.369 | 17 / 6 / 1 |
| MultilineGridDistinct/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/clean/workingSetBytes | 24 | 0 | 24 | 0.011 | 0.790 | -0.532 to 0.856 | 12 / 12 / 0 |
| MultilineGridDistinct/clean/workingSetPeakBytes | 24 | 0 | 24 | -0.016 | 0.843 | -0.469 to 0.866 | 12 / 12 / 0 |
| MultilineGridDistinct/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/dirty/composeCpuP95Ms | 24 | 4 | 20 | 0.000 | 11.039 | -14.194 to 5.556 | 11 / 11 / 2 |
| MultilineGridDistinct/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/fps | 24 | 4 | 20 | 0.152 | 12.050 | -12.673 to 6.158 | 13 / 11 / 0 |
| MultilineGridDistinct/dirty/frameP50Ms | 24 | 0 | 24 | -0.137 | 0.907 | -2.567 to 0.342 | 9 / 15 / 0 |
| MultilineGridDistinct/dirty/frameP95Ms | 24 | 15 | 9 | -2.866 | 18.295 | -16.776 to 19.459 | 8 / 16 / 0 |
| MultilineGridDistinct/dirty/prepareP95Ms | 24 | 7 | 17 | 0.288 | 10.084 | -12.760 to 9.211 | 13 / 11 / 0 |
| MultilineGridDistinct/dirty/privateBytes | 24 | 0 | 24 | 0.056 | 1.004 | -1.423 to 0.947 | 14 / 10 / 0 |
| MultilineGridDistinct/dirty/privatePeakBytes | 24 | 0 | 24 | 0.118 | 1.669 | -1.755 to 1.109 | 14 / 10 / 0 |
| MultilineGridDistinct/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/workingSetBytes | 24 | 0 | 24 | -0.073 | 0.831 | -0.965 to 0.600 | 11 / 13 / 0 |
| MultilineGridDistinct/dirty/workingSetPeakBytes | 24 | 0 | 24 | -0.077 | 0.831 | -0.965 to 0.593 | 11 / 13 / 0 |

## Temporal trends and evidence boundaries

The JSON keeps 108 first-six/last-six role trend rows with run identities and timestamps removed. These patterns are descriptive of this cohort; they do not establish host causation. The full private replay contains per-control chronology and receipt hashes.

Baseline source was marked dirty and the packet names three harness overlays (`Tools/PairedRun.psm1`, `Tools/PerformanceAcceptancePolicy.v1.json`, `Tools/ScopedTesting.psm1`). The extracted evidence did not include a complete dirty-source path inventory. Candidate source was marked clean. This limits how specifically the baseline build state can be reconstructed.

Observer status: **not established**. The collection completion record says sample gaps and collection costs still require inspection and explicitly limits claims about dedicated-host status, scheduler delay, and GPU duration. No quiet-host or observer-overhead qualification is claimed.

This is the latest 9a packet only; it is not pooled with the earlier 594d67fb cohort. Public projection omits raw receipt names and hashes, absolute paths, timestamps, machine names, process identifiers, and per-process records.

Machine-readable aggregate: `control-summary.json`. The detailed private replay remains in repository-local audit evidence.
