## Paired benchmark: Inconclusive

The study cannot qualify a pass: policy review, missing identity, drifting controls or insufficient evidence leaves the result unresolved. Re-run or complete independent policy review; do not dismiss flagged metrics as noise.

Policy review required: The base policy is not independently qualified; its verdict cannot establish a candidate pass.

This run was started by hand: the job stays green for a finding, and a pull request would fail on it.

| | |
|---|---|
| Baseline | `0bd0e99c482c` |
| Candidate | `0bd0e99c482c` |
| Library inputs | identical, fingerprint `D1C3380BFDDA`: both sides ran the same library code |
| Method | 12 independent paired blocks; seed 165785271; independently drawn ABBA/BAAB; exact sign-flip with Holm correction; x64 Release; 5% timing and 2% memory bands, exact budgets stay exact |
| Runner | runnervmfi6oq; a shared hosted VM, not a controlled quiet desktop |
| Event | manual dispatch on ej/performance-policy-evidence-gate |

### Scenarios

| Scenario | Check | Set verdict | Regressed | Improved | Metrics held by controls | Controls stable |
|---|---|---|---:|---:|---:|---:|
| Default | No regression established | within-noise-budget | 0 | 0 | 11 of 26 | 0 of 24 |
| MultilineGrid | No regression established | within-noise-budget | 0 | 0 | 8 of 26 | 0 of 24 |
| MultilineGridDistinct | No regression established | within-noise-budget | 0 | 0 | 20 of 26 | 1 of 24 |

### Flagged metrics

None: no metric regressed or improved.

<details><summary>Default: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | Raw p | Holm-adjusted p | Band | Same-binary controls | Baseline raw max | Candidate raw max | Median budget | Peak budget | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|---|---|
| clean | fps | 430.9 | 431.7 | +0.10% | 0.9731 | 1.0000 | 5% | 35.4% max, beyond band | 442.8 | 443.2 | not-applicable | not-applicable | within noise |
| clean | frameP50Ms | 2.2701 | 2.2695 | -0.69% | 0.8926 | 1.0000 | 5% | 22.3% max, beyond band | 2.961 | 2.9469 | not-applicable | not-applicable | within noise |
| clean | frameP95Ms | 2.4982 | 2.401 | +0.69% | 0.9038 | 1.0000 | 5% | 74.1% max, beyond band | 5.2705 | 5.4042 | not-applicable | not-applicable | within noise |
| clean | prepareP95Ms | 0.0006 | 0.0006 | -0.49% | 0.9375 | 1.0000 | 5% | 75.0% max, beyond band | 0.0026 | 0.0027 | not-applicable | not-applicable | within noise |
| clean | composeCpuP95Ms | 0.0188 | 0.0192 | -0.16% | 0.9795 | 1.0000 | 5% | 39.6% max, beyond band | 0.0501 | 0.0694 | not-applicable | not-applicable | within noise |
| clean | privateBytes | 23,390,208 | 23,451,648 | +0.26% | 0.2617 | 1.0000 | 2% | 1.7% max, held | 23,756,800 | 23,908,352 | not-applicable | not-applicable | within noise |
| clean | privatePeakBytes | 23,408,640 | 23,465,984 | +0.24% | 0.2979 | 1.0000 | 2% | 1.8% max, held | 24,371,200 | 24,559,616 | not-applicable | not-applicable | within noise |
| clean | workingSetBytes | 36,874,240 | 36,898,816 | -0.05% | 0.8691 | 1.0000 | 2% | 3.7% max, beyond band | 38,469,632 | 37,236,736 | not-applicable | not-applicable | within noise |
| clean | workingSetPeakBytes | 36,898,816 | 36,921,344 | -0.05% | 0.8789 | 1.0000 | 2% | 3.7% max, beyond band | 39,055,360 | 37,736,448 | not-applicable | not-applicable | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| dirty | fps | 211.6 | 207.7 | +0.48% | 0.8608 | 1.0000 | 5% | 27.6% max, beyond band | 222.9 | 222.5 | not-applicable | not-applicable | within noise |
| dirty | frameP50Ms | 4.6279 | 4.6376 | +0.15% | 0.9360 | 1.0000 | 5% | 28.7% max, beyond band | 7.1916 | 6.5572 | not-applicable | not-applicable | within noise |
| dirty | frameP95Ms | 5.0289 | 5.2067 | -0.16% | 0.9766 | 1.0000 | 5% | 47.8% max, beyond band | 10.4694 | 9.6308 | not-applicable | not-applicable | within noise |
| dirty | prepareP95Ms | 1.8637 | 1.8449 | -1.13% | 0.8081 | 1.0000 | 5% | 33.8% max, beyond band | 3.9364 | 3.2997 | not-applicable | not-applicable | within noise |
| dirty | composeCpuP95Ms | 0.0141 | 0.0136 | +0.04% | 0.9961 | 1.0000 | 5% | 98.6% max, beyond band | 0.0348 | 0.0293 | not-applicable | not-applicable | within noise |
| dirty | privateBytes | 23,666,688 | 23,808,000 | +0.35% | 0.1113 | 1.0000 | 2% | 1.9% max, held | 24,150,016 | 24,199,168 | not-applicable | not-applicable | within noise |
| dirty | privatePeakBytes | 23,730,176 | 23,934,976 | +0.51% | 0.0781 | 1.0000 | 2% | 2.2% max, beyond band | 24,293,376 | 24,227,840 | not-applicable | not-applicable | within noise |
| dirty | workingSetBytes | 37,187,584 | 37,240,832 | +0.06% | 0.7905 | 1.0000 | 2% | 3.8% max, beyond band | 38,600,704 | 37,654,528 | not-applicable | not-applicable | within noise |
| dirty | workingSetPeakBytes | 37,193,728 | 37,249,024 | +0.06% | 0.7671 | 1.0000 | 2% | 3.8% max, beyond band | 38,600,704 | 37,654,528 | not-applicable | not-applicable | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 2,160 | 2,160 | within-budget | within-budget | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |

</details>

<details><summary>MultilineGrid: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | Raw p | Holm-adjusted p | Band | Same-binary controls | Baseline raw max | Candidate raw max | Median budget | Peak budget | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|---|---|
| clean | fps | 405.5 | 365.4 | -1.12% | 0.3477 | 1.0000 | 5% | 24.0% max, beyond band | 443.6 | 443.7 | not-applicable | not-applicable | within noise |
| clean | frameP50Ms | 2.3346 | 2.5535 | +1.09% | 0.4268 | 1.0000 | 5% | 23.3% max, beyond band | 4.6517 | 4.0024 | not-applicable | not-applicable | within noise |
| clean | frameP95Ms | 2.6314 | 2.8181 | +1.85% | 0.6309 | 1.0000 | 5% | 70.1% max, beyond band | 5.998 | 8.1113 | not-applicable | not-applicable | within noise |
| clean | prepareP95Ms | 0.0006 | 0.0006 | -2.07% | 0.8330 | 1.0000 | 5% | 68.8% max, beyond band | 0.0017 | 0.0019 | not-applicable | not-applicable | within noise |
| clean | composeCpuP95Ms | 0.0192 | 0.0195 | +0.94% | 0.7861 | 1.0000 | 5% | 45.5% max, beyond band | 0.0401 | 0.0375 | not-applicable | not-applicable | within noise |
| clean | privateBytes | 28,731,392 | 28,727,296 | +0.07% | 0.7554 | 1.0000 | 2% | 3.4% max, beyond band | 29,700,096 | 29,274,112 | not-applicable | not-applicable | within noise |
| clean | privatePeakBytes | 28,774,400 | 28,786,688 | +0.05% | 0.8330 | 1.0000 | 2% | 3.3% max, beyond band | 32,653,312 | 32,112,640 | not-applicable | not-applicable | within noise |
| clean | workingSetBytes | 41,926,656 | 41,961,472 | -0.16% | 0.6069 | 1.0000 | 2% | 4.7% max, beyond band | 44,113,920 | 42,401,792 | not-applicable | not-applicable | within noise |
| clean | workingSetPeakBytes | 41,957,376 | 41,979,904 | -0.17% | 0.5923 | 1.0000 | 2% | 4.7% max, beyond band | 44,654,592 | 43,438,080 | not-applicable | not-applicable | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| dirty | fps | 73.0 | 74.6 | +0.41% | 0.7773 | 1.0000 | 5% | 22.5% max, beyond band | 86.4 | 83.9 | not-applicable | not-applicable | within noise |
| dirty | frameP50Ms | 13.0535 | 12.7674 | -0.50% | 0.7012 | 1.0000 | 5% | 23.9% max, beyond band | 16.6377 | 16.1082 | not-applicable | not-applicable | within noise |
| dirty | frameP95Ms | 14.7537 | 15.1258 | +1.13% | 0.6582 | 1.0000 | 5% | 53.5% max, beyond band | 27.6168 | 28.4122 | not-applicable | not-applicable | within noise |
| dirty | prepareP95Ms | 11.197 | 11.4577 | +0.45% | 0.8457 | 1.0000 | 5% | 33.4% max, beyond band | 17.2725 | 21.458 | not-applicable | not-applicable | within noise |
| dirty | composeCpuP95Ms | 0.0162 | 0.016 | +0.05% | 0.9907 | 1.0000 | 5% | 37.2% max, beyond band | 0.0403 | 0.0405 | not-applicable | not-applicable | within noise |
| dirty | privateBytes | 29,859,840 | 30,019,584 | +0.42% | 0.2891 | 1.0000 | 2% | 4.6% max, beyond band | 31,154,176 | 32,075,776 | not-applicable | not-applicable | within noise |
| dirty | privatePeakBytes | 30,892,032 | 30,828,544 | -0.14% | 0.6904 | 1.0000 | 2% | 3.1% max, beyond band | 31,506,432 | 32,776,192 | not-applicable | not-applicable | within noise |
| dirty | workingSetBytes | 42,727,424 | 42,870,784 | +0.18% | 0.5386 | 1.0000 | 2% | 4.1% max, beyond band | 45,072,384 | 44,769,280 | not-applicable | not-applicable | within noise |
| dirty | workingSetPeakBytes | 43,216,896 | 43,171,840 | -0.17% | 0.5527 | 1.0000 | 2% | 4.2% max, beyond band | 45,318,144 | 45,834,240 | not-applicable | not-applicable | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 1,080 | 1,080 | within-budget | within-budget | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |

</details>

<details><summary>MultilineGridDistinct: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | Raw p | Holm-adjusted p | Band | Same-binary controls | Baseline raw max | Candidate raw max | Median budget | Peak budget | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---:|---:|---|---|---|
| clean | fps | 439.7 | 438.3 | +0.02% | 0.9595 | 1.0000 | 5% | 4.8% max, held | 444.2 | 443.2 | not-applicable | not-applicable | within noise |
| clean | frameP50Ms | 2.2597 | 2.259 | -0.22% | 0.4751 | 1.0000 | 5% | 4.8% max, held | 2.6934 | 2.3051 | not-applicable | not-applicable | within noise |
| clean | frameP95Ms | 2.3255 | 2.3264 | -0.17% | 0.6152 | 1.0000 | 5% | 4.5% max, held | 3.5376 | 3.1513 | not-applicable | not-applicable | within noise |
| clean | prepareP95Ms | 0.0005 | 0.0005 | -5.35% | 0.3809 | 1.0000 | 5% | 100.0% max, beyond band | 0.0011 | 0.0011 | not-applicable | not-applicable | within noise |
| clean | composeCpuP95Ms | 0.0169 | 0.017 | -1.45% | 0.4614 | 1.0000 | 5% | 24.5% max, beyond band | 0.0412 | 0.0269 | not-applicable | not-applicable | within noise |
| clean | privateBytes | 25,282,560 | 25,303,040 | -0.11% | 0.3037 | 1.0000 | 2% | 1.4% max, held | 25,669,632 | 25,550,848 | not-applicable | not-applicable | within noise |
| clean | privatePeakBytes | 25,317,376 | 25,358,336 | -0.04% | 0.6665 | 1.0000 | 2% | 1.3% max, held | 26,300,416 | 26,243,072 | not-applicable | not-applicable | within noise |
| clean | workingSetBytes | 38,735,872 | 38,707,200 | -0.10% | 0.3198 | 1.0000 | 2% | 1.2% max, held | 39,124,992 | 39,014,400 | not-applicable | not-applicable | within noise |
| clean | workingSetPeakBytes | 38,754,304 | 38,727,680 | -0.10% | 0.3379 | 1.0000 | 2% | 1.1% max, held | 39,776,256 | 39,636,992 | not-applicable | not-applicable | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |
| dirty | fps | 210.8 | 210.8 | -0.01% | 0.9766 | 1.0000 | 5% | 4.7% max, held | 216.3 | 216.7 | not-applicable | not-applicable | within noise |
| dirty | frameP50Ms | 4.690 | 4.7033 | -0.26% | 0.5420 | 1.0000 | 5% | 5.8% max, beyond band | 5.0249 | 5.0253 | not-applicable | not-applicable | within noise |
| dirty | frameP95Ms | 4.9545 | 4.9674 | +1.00% | 0.3970 | 1.0000 | 5% | 18.9% max, beyond band | 9.459 | 6.5347 | not-applicable | not-applicable | within noise |
| dirty | prepareP95Ms | 2.0394 | 1.9899 | -1.14% | 0.5654 | 1.0000 | 5% | 28.4% max, beyond band | 3.4556 | 2.9056 | not-applicable | not-applicable | within noise |
| dirty | composeCpuP95Ms | 0.0125 | 0.0128 | -0.44% | 0.9063 | 1.0000 | 5% | 34.1% max, beyond band | 0.0385 | 0.0348 | not-applicable | not-applicable | within noise |
| dirty | privateBytes | 25,649,152 | 25,581,568 | -0.24% | 0.2104 | 1.0000 | 2% | 1.8% max, held | 26,157,056 | 26,042,368 | not-applicable | not-applicable | within noise |
| dirty | privatePeakBytes | 25,686,016 | 25,661,440 | -0.14% | 0.4434 | 1.0000 | 2% | 1.8% max, held | 26,333,184 | 26,157,056 | not-applicable | not-applicable | within noise |
| dirty | workingSetBytes | 39,272,448 | 39,223,296 | -0.17% | 0.1885 | 1.0000 | 2% | 1.1% max, held | 39,780,352 | 39,772,160 | not-applicable | not-applicable | within noise |
| dirty | workingSetPeakBytes | 39,272,448 | 39,239,680 | -0.15% | 0.2266 | 1.0000 | 2% | 1.2% max, held | 39,784,448 | 39,849,984 | not-applicable | not-applicable | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 3,686,400 | 3,686,400 | within-budget | within-budget | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 1,080 | 1,080 | within-budget | within-budget | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | 1.0000 | exact | 0.0% max, held | 0 | 0 | within-budget | within-budget | within noise |

</details>

The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. Migrated timing and memory metrics use the Holm-adjusted p-value and declared band; legacy rows retain their original raw p-value and show no adjusted p-value. Exact-budget rows report median and raw-peak findings separately. `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.
