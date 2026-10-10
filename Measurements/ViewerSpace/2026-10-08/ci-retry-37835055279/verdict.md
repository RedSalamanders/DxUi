## Paired benchmark: No regression established

No metric regressed. The contract reads this as no change established, which is not evidence that none exists; metrics whose same-binary controls drifted could not resolve their band on this runner and are counted below.

Resolution: the metrics that held inside their band in every same-binary control were Default 14 of 26, MultilineGrid 9 of 26, MultilineGridDistinct 18 of 26. A shift in the others that is smaller than the runner's own drift could not have been seen.

| | |
|---|---|
| Baseline | `bea676a1f841` first parent of the merge commit (the base as the merge ref was made) |
| Candidate | `e9806509a952` (this checkout) |
| Library inputs | changed, fingerprint `41423433DDF4` to `F826386B05B7` |
| Method | 3 repetitions of the interleaved pass A, B, B, A, so 6 runs per side; x64 Release; exact two-sided Mann-Whitney U test with the 5% timing and 2% memory investigation bands, exact budgets stay exact |
| Runner | runnervmfi6oq; a shared hosted VM, not a controlled quiet desktop |
| Event | pull request merge ref (refs/pull/69/merge) |

### Scenarios

| Scenario | Check | Set verdict | Regressed | Improved | Metrics held by controls | Controls stable |
|---|---|---|---:|---:|---:|---:|
| Default | No regression established | within-noise-budget | 0 | 0 | 14 of 26 | 0 of 6 |
| MultilineGrid | No regression established | within-noise-budget | 0 | 0 | 9 of 26 | 0 of 6 |
| MultilineGridDistinct | No regression established | within-noise-budget | 0 | 1 | 18 of 26 | 0 of 6 |

### Flagged metrics

| Scenario | Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---|
| MultilineGridDistinct | clean | composeCpuP95Ms | 0.0182 | 0.0166 | -9.07% | 0.0303 | 5% | 29.7% max, beyond band | improved |

<details><summary>Default: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 431.4 | 434.3 | +0.67% | 1.0000 | 5% | 15.9% max, beyond band | within noise |
| clean | frameP50Ms | 2.306 | 2.2804 | -1.11% | 0.9372 | 5% | 16.5% max, beyond band | within noise |
| clean | frameP95Ms | 2.382 | 2.4152 | +1.39% | 0.8182 | 5% | 16.7% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0005 | 0.0004 | -20.00% | 0.2056 | 5% | 25.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0157 | 0.0155 | -1.28% | 0.9784 | 5% | 22.6% max, beyond band | within noise |
| clean | privateBytes | 23,414,784 | 23,386,112 | -0.12% | 0.4156 | 2% | 2.4% max, beyond band | within noise |
| clean | privatePeakBytes | 23,431,168 | 23,392,256 | -0.17% | 0.3290 | 2% | 2.1% max, beyond band | within noise |
| clean | workingSetBytes | 36,667,392 | 36,751,360 | +0.23% | 1.0000 | 2% | 3.9% max, beyond band | within noise |
| clean | workingSetPeakBytes | 36,691,968 | 36,771,840 | +0.22% | 1.0000 | 2% | 3.8% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 218.7 | 219.4 | +0.31% | 0.8182 | 5% | 2.7% max, held | within noise |
| dirty | frameP50Ms | 4.5215 | 4.5089 | -0.28% | 0.3939 | 5% | 2.8% max, held | within noise |
| dirty | frameP95Ms | 4.6765 | 4.7116 | +0.75% | 1.0000 | 5% | 3.6% max, held | within noise |
| dirty | prepareP95Ms | 1.6082 | 1.5841 | -1.50% | 0.0931 | 5% | 2.3% max, held | within noise |
| dirty | composeCpuP95Ms | 0.0105 | 0.0107 | +1.91% | 0.5887 | 5% | 53.0% max, beyond band | within noise |
| dirty | privateBytes | 23,697,408 | 23,697,408 | 0.00% | 0.7316 | 2% | 0.9% max, held | within noise |
| dirty | privatePeakBytes | 23,736,320 | 23,730,176 | -0.03% | 0.6991 | 2% | 0.8% max, held | within noise |
| dirty | workingSetBytes | 37,003,264 | 37,101,568 | +0.27% | 0.6991 | 2% | 2.6% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 37,021,696 | 37,105,664 | +0.23% | 0.8182 | 2% | 2.6% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGrid: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 432.7 | 436.6 | +0.91% | 0.1320 | 5% | 6.3% max, beyond band | within noise |
| clean | frameP50Ms | 2.2955 | 2.2743 | -0.93% | 0.2403 | 5% | 6.3% max, beyond band | within noise |
| clean | frameP95Ms | 2.5039 | 2.3553 | -5.93% | 0.1797 | 5% | 10.2% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0005 | 0.0005 | 0.00% | 0.1515 | 5% | 16.7% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0191 | 0.0182 | -4.71% | 0.3939 | 5% | 16.0% max, beyond band | within noise |
| clean | privateBytes | 28,571,648 | 28,678,144 | +0.37% | 0.9004 | 2% | 2.2% max, beyond band | within noise |
| clean | privatePeakBytes | 28,649,472 | 28,717,056 | +0.24% | 0.9372 | 2% | 2.2% max, beyond band | within noise |
| clean | workingSetBytes | 41,709,568 | 41,814,016 | +0.25% | 0.3939 | 2% | 4.1% max, beyond band | within noise |
| clean | workingSetPeakBytes | 41,734,144 | 41,842,688 | +0.26% | 0.3939 | 2% | 4.1% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 79.6 | 81.3 | +2.18% | 0.1797 | 5% | 14.0% max, beyond band | within noise |
| dirty | frameP50Ms | 12.119 | 11.9604 | -1.31% | 0.4848 | 5% | 15.3% max, beyond band | within noise |
| dirty | frameP95Ms | 13.4798 | 13.3521 | -0.95% | 0.8182 | 5% | 7.3% max, beyond band | within noise |
| dirty | prepareP95Ms | 10.4089 | 10.4301 | +0.20% | 0.5887 | 5% | 6.7% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0145 | 0.0126 | -13.15% | 0.1061 | 5% | 30.9% max, beyond band | within noise |
| dirty | privateBytes | 30,289,920 | 30,128,128 | -0.53% | 0.0411 | 2% | 3.4% max, beyond band | within noise |
| dirty | privatePeakBytes | 30,922,752 | 30,685,184 | -0.77% | 0.1320 | 2% | 1.1% max, held | within noise |
| dirty | workingSetBytes | 42,924,032 | 42,659,840 | -0.62% | 0.1320 | 2% | 3.0% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 43,012,096 | 42,844,160 | -0.39% | 0.3095 | 2% | 3.2% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGridDistinct: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 432.9 | 437.7 | +1.12% | 0.0931 | 5% | 16.8% max, beyond band | within noise |
| clean | frameP50Ms | 2.2772 | 2.2722 | -0.22% | 0.3939 | 5% | 16.4% max, beyond band | within noise |
| clean | frameP95Ms | 2.4126 | 2.3457 | -2.78% | 0.0411 | 5% | 18.5% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0005 | 0.0004 | -20.00% | 0.2424 | 5% | 25.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0182 | 0.0166 | -9.07% | 0.0303 | 5% | 29.7% max, beyond band | improved |
| clean | privateBytes | 25,260,032 | 25,300,992 | +0.16% | 0.3939 | 2% | 0.7% max, held | within noise |
| clean | privatePeakBytes | 25,286,656 | 25,344,000 | +0.23% | 0.6991 | 2% | 0.5% max, held | within noise |
| clean | workingSetBytes | 38,553,600 | 38,629,376 | +0.20% | 0.2208 | 2% | 0.3% max, held | within noise |
| clean | workingSetPeakBytes | 38,584,320 | 38,651,904 | +0.18% | 0.3095 | 2% | 0.3% max, held | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 210.1 | 210.8 | +0.30% | 0.9372 | 5% | 2.5% max, held | within noise |
| dirty | frameP50Ms | 4.6709 | 4.7043 | +0.72% | 0.1320 | 5% | 1.3% max, held | within noise |
| dirty | frameP95Ms | 4.9485 | 4.9235 | -0.50% | 0.3939 | 5% | 11.4% max, beyond band | within noise |
| dirty | prepareP95Ms | 2.0529 | 1.9924 | -2.95% | 0.6991 | 5% | 27.4% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0106 | 0.0107 | +0.94% | 0.6623 | 5% | 14.4% max, beyond band | within noise |
| dirty | privateBytes | 25,640,960 | 25,628,672 | -0.05% | 1.0000 | 2% | 1.4% max, held | within noise |
| dirty | privatePeakBytes | 25,724,928 | 25,743,360 | +0.07% | 0.8182 | 2% | 1.3% max, held | within noise |
| dirty | workingSetBytes | 39,098,368 | 39,184,384 | +0.22% | 0.2403 | 2% | 0.4% max, held | within noise |
| dirty | workingSetPeakBytes | 39,122,944 | 39,202,816 | +0.20% | 0.2403 | 2% | 0.4% max, held | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. A metric regresses only when p < 0.05 and its median shift is beyond the band (a rise in an exact budget always regresses); `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.
