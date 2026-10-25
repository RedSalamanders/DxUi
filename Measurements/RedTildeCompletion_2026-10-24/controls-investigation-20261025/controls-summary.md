# Curated same-side control replay summary

**Public packet:** `Measurements/RedTildeCompletion_2026-10-24/sdk-594d67fb`  
**Cohort:** `594d67fb-same-side-controls`; seed `811064005`; 3 scenarios, 12 blocks per scenario, 2 roles per block.

The measured cohort contains 144 native reports. All 144 receipt hashes were verified against the public manifest; an independent replay recomputed and matched all 1,872 stored metric observations and all 72 control statuses.

## Aggregate results

- 69/72 control comparisons flagged at least one metric; 3/72 did not.
- 307/1,872 metric observations exceeded their unchanged investigation bands (16.4%).
- 30/78 metric keys held in all 24 controls; 0/78 breached in all 24.
- Maximum drift count for one metric key: 15/24.

The control-level count means at least one metric breached in a paired comparison. It does not imply every metric was noisy.

## Per-metric control results

Signed movement is the later-versus-earlier delta within each same-side control; p95 is the 95th percentile of absolute signed movement. Null values mean the metric is not expressed as a percentage for that comparison.

| Scenario / phase / metric | N | Breached | Held | Median signed % | P95 absolute % | Min to max signed % | + / - / 0 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Default/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/clean/composeCpuP95Ms | 24 | 10 | 14 | 2.455 | 15.385 | -16.735 to 15.385 | 14 / 8 / 2 |
| Default/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/clean/fps | 24 | 9 | 15 | -1.700 | 11.326 | -15.325 to 8.879 | 8 / 16 / 0 |
| Default/clean/frameP50Ms | 24 | 9 | 15 | 0.471 | 14.280 | -10.758 to 18.936 | 13 / 11 / 0 |
| Default/clean/frameP95Ms | 24 | 7 | 17 | 0.630 | 10.559 | -10.955 to 10.559 | 12 / 12 / 0 |
| Default/clean/prepareP95Ms | 24 | 14 | 10 | 0.000 | 12.500 | -20.000 to 12.500 | 10 / 4 / 10 |
| Default/clean/privateBytes | 24 | 0 | 24 | 0.114 | 1.045 | -1.134 to 0.984 | 12 / 12 / 0 |
| Default/clean/privatePeakBytes | 24 | 0 | 24 | 0.017 | 1.037 | -1.045 to 1.037 | 12 / 11 / 1 |
| Default/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/clean/workingSetBytes | 24 | 1 | 23 | -0.050 | 1.992 | -4.002 to 0.452 | 9 / 14 / 1 |
| Default/clean/workingSetPeakBytes | 24 | 1 | 23 | -0.055 | 1.991 | -4.020 to 0.441 | 9 / 14 / 1 |
| Default/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| Default/dirty/composeCpuP95Ms | 24 | 15 | 9 | 1.615 | 13.605 | -8.982 to 17.391 | 15 / 8 / 1 |
| Default/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/fps | 24 | 10 | 14 | -2.146 | 11.160 | -11.233 to 8.958 | 6 / 18 / 0 |
| Default/dirty/frameP50Ms | 24 | 8 | 16 | 1.279 | 12.141 | -9.457 to 18.002 | 16 / 8 / 0 |
| Default/dirty/frameP95Ms | 24 | 13 | 11 | -0.505 | 14.496 | -14.496 to 20.987 | 12 / 12 / 0 |
| Default/dirty/prepareP95Ms | 24 | 8 | 16 | 1.819 | 10.303 | -10.303 to 13.296 | 15 / 9 / 0 |
| Default/dirty/privateBytes | 24 | 2 | 22 | 0.374 | 2.386 | -2.605 to 2.386 | 14 / 9 / 1 |
| Default/dirty/privatePeakBytes | 24 | 2 | 22 | 0.136 | 2.316 | -1.825 to 2.453 | 13 / 11 / 0 |
| Default/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| Default/dirty/workingSetBytes | 24 | 2 | 22 | -0.087 | 2.482 | -4.351 to 0.972 | 9 / 15 / 0 |
| Default/dirty/workingSetPeakBytes | 24 | 2 | 22 | -0.098 | 2.440 | -4.401 to 0.939 | 10 / 14 / 0 |
| MultilineGrid/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/clean/composeCpuP95Ms | 24 | 8 | 16 | -0.480 | 17.455 | -17.455 to 18.615 | 11 / 13 / 0 |
| MultilineGrid/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/clean/fps | 24 | 9 | 15 | -1.593 | 15.154 | -15.154 to 20.794 | 10 / 14 / 0 |
| MultilineGrid/clean/frameP50Ms | 24 | 7 | 17 | -0.122 | 12.677 | -12.677 to 13.789 | 11 / 13 / 0 |
| MultilineGrid/clean/frameP95Ms | 24 | 6 | 18 | -1.107 | 23.039 | -35.999 to 23.039 | 9 / 15 / 0 |
| MultilineGrid/clean/prepareP95Ms | 24 | 4 | 20 | 0.000 | 12.500 | -11.111 to 12.500 | 2 / 2 / 20 |
| MultilineGrid/clean/privateBytes | 24 | 1 | 23 | -0.150 | 1.426 | -2.929 to 1.320 | 10 / 14 / 0 |
| MultilineGrid/clean/privatePeakBytes | 24 | 1 | 23 | -0.134 | 1.410 | -3.005 to 1.302 | 10 / 14 / 0 |
| MultilineGrid/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/clean/workingSetBytes | 24 | 1 | 23 | -0.238 | 1.427 | -3.258 to 0.849 | 8 / 15 / 1 |
| MultilineGrid/clean/workingSetPeakBytes | 24 | 1 | 23 | -0.238 | 1.378 | -3.365 to 0.839 | 9 / 15 / 0 |
| MultilineGrid/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGrid/dirty/composeCpuP95Ms | 24 | 9 | 15 | 0.331 | 8.667 | -8.667 to 10.625 | 12 / 10 / 2 |
| MultilineGrid/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/fps | 24 | 8 | 16 | -2.267 | 11.227 | -24.795 to 4.534 | 6 / 18 / 0 |
| MultilineGrid/dirty/frameP50Ms | 24 | 4 | 20 | 1.457 | 8.104 | -8.104 to 19.230 | 14 / 10 / 0 |
| MultilineGrid/dirty/frameP95Ms | 24 | 14 | 10 | -0.989 | 26.263 | -15.903 to 45.227 | 9 / 15 / 0 |
| MultilineGrid/dirty/prepareP95Ms | 24 | 13 | 11 | -0.376 | 38.151 | -17.562 to 49.112 | 11 / 13 / 0 |
| MultilineGrid/dirty/privateBytes | 24 | 12 | 12 | 0.124 | 11.367 | -11.745 to 11.367 | 13 / 11 / 0 |
| MultilineGrid/dirty/privatePeakBytes | 24 | 5 | 19 | 0.167 | 2.637 | -3.421 to 2.520 | 13 / 11 / 0 |
| MultilineGrid/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGrid/dirty/workingSetBytes | 24 | 11 | 13 | -0.377 | 6.972 | -8.881 to 6.972 | 9 / 14 / 1 |
| MultilineGrid/dirty/workingSetPeakBytes | 24 | 1 | 23 | -0.033 | 1.813 | -4.036 to 1.813 | 11 / 13 / 0 |
| MultilineGridDistinct/clean/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/clean/composeCpuP95Ms | 24 | 7 | 17 | 0.473 | 10.748 | -6.667 to 16.337 | 13 / 11 / 0 |
| MultilineGridDistinct/clean/cppAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/clean/fps | 24 | 4 | 20 | -1.034 | 7.227 | -7.711 to 5.165 | 9 / 15 / 0 |
| MultilineGridDistinct/clean/frameP50Ms | 24 | 5 | 19 | -0.159 | 7.836 | -5.828 to 8.777 | 11 / 13 / 0 |
| MultilineGridDistinct/clean/frameP95Ms | 24 | 4 | 20 | -0.253 | 12.456 | -10.382 to 23.705 | 10 / 14 / 0 |
| MultilineGridDistinct/clean/prepareP95Ms | 24 | 15 | 9 | 0.000 | 12.500 | -12.500 to 12.500 | 7 / 8 / 9 |
| MultilineGridDistinct/clean/privateBytes | 24 | 0 | 24 | -0.057 | 1.429 | -1.429 to 1.516 | 11 / 12 / 1 |
| MultilineGridDistinct/clean/privatePeakBytes | 24 | 0 | 24 | -0.024 | 1.488 | -1.488 to 1.643 | 11 / 12 / 1 |
| MultilineGridDistinct/clean/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/clean/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/clean/workingSetBytes | 24 | 0 | 24 | -0.115 | 0.690 | -1.774 to 0.673 | 10 / 14 / 0 |
| MultilineGridDistinct/clean/workingSetPeakBytes | 24 | 0 | 24 | -0.115 | 0.669 | -1.793 to 0.652 | 10 / 14 / 0 |
| MultilineGridDistinct/dirty/composeAllocations | 24 | 0 | 24 | n/a | n/a | n/a to n/a | 0 / 0 / 0 |
| MultilineGridDistinct/dirty/composeCpuP95Ms | 24 | 10 | 14 | 1.774 | 28.351 | -28.351 to 263.309 | 14 / 10 / 0 |
| MultilineGridDistinct/dirty/cppAllocations | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/fps | 24 | 6 | 18 | -1.948 | 8.864 | -12.132 to 5.238 | 6 / 18 / 0 |
| MultilineGridDistinct/dirty/frameP50Ms | 24 | 2 | 22 | 0.989 | 6.775 | -8.968 to 6.775 | 16 / 8 / 0 |
| MultilineGridDistinct/dirty/frameP95Ms | 24 | 10 | 14 | 0.072 | 11.558 | -13.453 to 9.054 | 13 / 11 / 0 |
| MultilineGridDistinct/dirty/prepareP95Ms | 24 | 6 | 18 | 0.994 | 11.045 | -10.725 to 16.217 | 15 / 9 / 0 |
| MultilineGridDistinct/dirty/privateBytes | 24 | 3 | 21 | 0.056 | 3.783 | -3.783 to 4.682 | 13 / 11 / 0 |
| MultilineGridDistinct/dirty/privatePeakBytes | 24 | 3 | 21 | 0.063 | 3.867 | -3.867 to 5.048 | 14 / 10 / 0 |
| MultilineGridDistinct/dirty/replacementPeakBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/surfaceBytes | 24 | 0 | 24 | 0.000 | 0.000 | 0.000 to 0.000 | 0 / 0 / 24 |
| MultilineGridDistinct/dirty/workingSetBytes | 24 | 2 | 22 | -0.083 | 2.164 | -2.164 to 2.461 | 7 / 16 / 1 |
| MultilineGridDistinct/dirty/workingSetPeakBytes | 24 | 2 | 22 | -0.098 | 2.233 | -2.233 to 2.606 | 7 / 17 / 0 |

## Temporal trends

For each scenario, phase, metric, and role, the JSON retains first-six and last-six chronological medians, their relative change, linear slope per sequence position, Pearson position correlation, and full-run p95 range. Run names, timestamps, and block identifiers are omitted.

The observed pattern is scenario-dependent: in MultilineGridDistinct, FPS fell on both roles (clean: 6.23% baseline and 5.27% candidate; dirty: 6.72% and 6.43%), while clean and dirty frame/compose timing rose on both roles in the listed measures. Default FPS moved upward instead. This common movement is compatible with temporal/order sensitivity, but does not identify a cause or justify host attribution.

## Timer resolution

All 172,800 samples across frame, prepare, and compose timing arrays fell on the recorded QPC grid (0.0001 ms per tick). Across the 1,440 p95 values per sample type, distinct tick counts were 1,391 frame, 706 prepare, and 177 compose. Clean Prepare p95 values include 323 samples at 8 ticks and 244 at 9 ticks; one tick is 12.5% or 11.1% at those values. Clean Prepare flags occurred in 33/72 controls (Default 14, MultilineGrid 4, MultilineGridDistinct 15), with median signed movement 0% in each scenario. This supports a quantization sensitivity explanation for these flags, but is not a full performance qualification.

## Limits

- This projection contains aggregates only; it omits raw receipt names and hashes, local paths, timestamps, per-run identities, host/device details, and observer measurements.
- A/A calibration is separate and not pooled here.
- The control replay does not establish quiet-host or observer-overhead qualification, overall acceptance, or causal attribution for temporal patterns.
- No acceptance threshold or policy was changed.

Machine-readable projection: `controls-summary.json`.
