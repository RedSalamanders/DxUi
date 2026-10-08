## Paired benchmark: No regression established

No metric regressed. The contract reads this as no change established, which is not evidence that none exists; metrics whose same-binary controls drifted could not resolve their band on this runner and are counted below.

Resolution: the metrics that held inside their band in every same-binary control were Default 12 of 26, MultilineGrid 8 of 26, MultilineGridDistinct 18 of 26. A shift in the others that is smaller than the runner's own drift could not have been seen.

| | |
|---|---|
| Baseline | `bea676a1f841` first parent of the merge commit (the base as the merge ref was made) |
| Candidate | `cc6690c34939` (this checkout) |
| Library inputs | changed, fingerprint `41423433DDF4` to `F826386B05B7` |
| Method | 3 repetitions of the interleaved pass A, B, B, A, so 6 runs per side; x64 Release; exact two-sided Mann-Whitney U test with the 5% timing and 2% memory investigation bands, exact budgets stay exact |
| Runner | runnervmfi6oq; a shared hosted VM, not a controlled quiet desktop |
| Event | pull request merge ref (refs/pull/69/merge) |

### Scenarios

| Scenario | Check | Set verdict | Regressed | Improved | Metrics held by controls | Controls stable |
|---|---|---|---:|---:|---:|---:|
| Default | No regression established | within-noise-budget | 0 | 0 | 12 of 26 | 0 of 6 |
| MultilineGrid | No regression established | within-noise-budget | 0 | 0 | 8 of 26 | 0 of 6 |
| MultilineGridDistinct | No regression established | within-noise-budget | 0 | 0 | 18 of 26 | 1 of 6 |

### Flagged metrics

None: no metric regressed or improved.

<details><summary>Default: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 435.6 | 434.8 | -0.18% | 0.8182 | 5% | 19.3% max, beyond band | within noise |
| clean | frameP50Ms | 2.2846 | 2.277 | -0.33% | 0.9372 | 5% | 6.7% max, beyond band | within noise |
| clean | frameP95Ms | 2.3513 | 2.3968 | +1.93% | 1.0000 | 5% | 30.8% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0009 | 0.0008 | -11.76% | 0.9199 | 5% | 33.3% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0192 | 0.0187 | -2.86% | 1.0000 | 5% | 36.0% max, beyond band | within noise |
| clean | privateBytes | 23,427,072 | 23,408,640 | -0.08% | 0.8182 | 2% | 1.4% max, held | within noise |
| clean | privatePeakBytes | 23,437,312 | 23,439,360 | +0.01% | 0.6991 | 2% | 1.2% max, held | within noise |
| clean | workingSetBytes | 36,827,136 | 36,804,608 | -0.06% | 0.9372 | 2% | 3.7% max, beyond band | within noise |
| clean | workingSetPeakBytes | 36,849,664 | 36,825,088 | -0.07% | 0.9372 | 2% | 3.7% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 209.1 | 207.8 | -0.61% | 1.0000 | 5% | 7.2% max, beyond band | within noise |
| dirty | frameP50Ms | 4.7616 | 4.7317 | -0.63% | 0.9372 | 5% | 6.8% max, beyond band | within noise |
| dirty | frameP95Ms | 4.9807 | 5.0306 | +1.00% | 0.9372 | 5% | 17.2% max, beyond band | within noise |
| dirty | prepareP95Ms | 1.9093 | 1.9456 | +1.90% | 0.9372 | 5% | 9.6% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0161 | 0.0164 | +2.18% | 0.6190 | 5% | 12.1% max, beyond band | within noise |
| dirty | privateBytes | 23,691,264 | 23,640,064 | -0.22% | 0.3095 | 2% | 1.7% max, held | within noise |
| dirty | privatePeakBytes | 23,898,112 | 23,820,288 | -0.33% | 0.5887 | 2% | 1.1% max, held | within noise |
| dirty | workingSetBytes | 37,072,896 | 37,062,656 | -0.03% | 0.6688 | 2% | 3.5% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 37,085,184 | 37,064,704 | -0.06% | 0.5887 | 2% | 3.5% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGrid: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 428.4 | 426.7 | -0.39% | 0.6991 | 5% | 20.1% max, beyond band | within noise |
| clean | frameP50Ms | 2.2813 | 2.2794 | -0.09% | 0.8182 | 5% | 24.1% max, beyond band | within noise |
| clean | frameP95Ms | 2.5532 | 2.6713 | +4.63% | 0.5887 | 5% | 25.8% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0007 | -7.14% | 0.8420 | 5% | 25.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0188 | 0.0203 | +7.98% | 0.8182 | 5% | 68.2% max, beyond band | within noise |
| clean | privateBytes | 28,772,352 | 28,571,648 | -0.70% | 0.3939 | 2% | 3.3% max, beyond band | within noise |
| clean | privatePeakBytes | 28,809,216 | 28,628,992 | -0.63% | 0.4848 | 2% | 3.5% max, beyond band | within noise |
| clean | workingSetBytes | 41,836,544 | 41,644,032 | -0.46% | 0.3939 | 2% | 3.1% max, beyond band | within noise |
| clean | workingSetPeakBytes | 41,865,216 | 41,676,800 | -0.45% | 0.3939 | 2% | 3.1% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 75.3 | 76.8 | +2.05% | 0.5887 | 5% | 9.1% max, beyond band | within noise |
| dirty | frameP50Ms | 12.7993 | 12.9013 | +0.80% | 0.9372 | 5% | 9.8% max, beyond band | within noise |
| dirty | frameP95Ms | 13.6507 | 13.9248 | +2.01% | 0.3939 | 5% | 14.6% max, beyond band | within noise |
| dirty | prepareP95Ms | 10.7026 | 10.734 | +0.29% | 1.0000 | 5% | 18.5% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0166 | 0.0165 | -0.90% | 1.0000 | 5% | 31.8% max, beyond band | within noise |
| dirty | privateBytes | 30,038,016 | 29,978,624 | -0.20% | 0.8182 | 2% | 3.8% max, beyond band | within noise |
| dirty | privatePeakBytes | 31,004,672 | 30,910,464 | -0.30% | 0.3939 | 2% | 3.6% max, beyond band | within noise |
| dirty | workingSetBytes | 43,124,736 | 42,592,256 | -1.23% | 0.2403 | 2% | 4.9% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 43,409,408 | 42,819,584 | -1.36% | 0.3095 | 2% | 2.4% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGridDistinct: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 435.1 | 435.4 | +0.08% | 0.5887 | 5% | 19.3% max, beyond band | within noise |
| clean | frameP50Ms | 2.2869 | 2.2796 | -0.32% | 0.6991 | 5% | 18.6% max, beyond band | within noise |
| clean | frameP95Ms | 2.3578 | 2.3666 | +0.37% | 0.3939 | 5% | 19.2% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0006 | -21.43% | 0.1061 | 5% | 40.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0199 | 0.0185 | -7.05% | 0.2900 | 5% | 28.5% max, beyond band | within noise |
| clean | privateBytes | 25,190,400 | 25,325,568 | +0.54% | 0.2338 | 2% | 0.8% max, held | within noise |
| clean | privatePeakBytes | 25,286,656 | 25,346,048 | +0.23% | 0.2900 | 2% | 0.8% max, held | within noise |
| clean | workingSetBytes | 38,576,128 | 38,606,848 | +0.08% | 0.6688 | 2% | 0.7% max, held | within noise |
| clean | workingSetPeakBytes | 38,594,560 | 38,629,376 | +0.09% | 0.6190 | 2% | 0.6% max, held | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 203.7 | 203.9 | +0.12% | 0.9372 | 5% | 4.1% max, held | within noise |
| dirty | frameP50Ms | 4.8804 | 4.8129 | -1.38% | 0.6991 | 5% | 1.1% max, held | within noise |
| dirty | frameP95Ms | 5.1002 | 5.1413 | +0.81% | 0.3095 | 5% | 15.0% max, beyond band | within noise |
| dirty | prepareP95Ms | 2.2914 | 2.2486 | -1.87% | 0.6991 | 5% | 12.5% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0152 | 0.0147 | -3.29% | 0.5628 | 5% | 8.6% max, beyond band | within noise |
| dirty | privateBytes | 25,606,144 | 25,597,952 | -0.03% | 0.5736 | 2% | 0.8% max, held | within noise |
| dirty | privatePeakBytes | 25,620,480 | 25,649,152 | +0.11% | 1.0000 | 2% | 1.8% max, held | within noise |
| dirty | workingSetBytes | 39,215,104 | 39,129,088 | -0.22% | 0.1797 | 2% | 1.7% max, held | within noise |
| dirty | workingSetPeakBytes | 39,249,920 | 39,133,184 | -0.30% | 0.1623 | 2% | 1.8% max, held | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. A metric regresses only when p < 0.05 and its median shift is beyond the band (a rise in an exact budget always regresses); `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.
