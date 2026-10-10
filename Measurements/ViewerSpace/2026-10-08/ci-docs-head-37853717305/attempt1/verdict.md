## Paired benchmark: Inconclusive

The run cannot say. A metric was flagged but its same-binary controls drifted beyond its band, or the set is too small to judge a timing. It does not pass: re-run the job for a new runner, or repeat the set on a quiet machine, and treat the flagged metrics as unresolved findings, not as noise.

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
| Default | No regression established | within-noise-budget | 0 | 0 | 11 of 26 | 0 of 6 |
| MultilineGrid | Inconclusive | advice-required | 1 | 0 | 12 of 26 | 0 of 6 |
| MultilineGridDistinct | No regression established | within-noise-budget | 0 | 0 | 16 of 26 | 0 of 6 |

- **MultilineGrid**: 1 regressed metric has same-binary controls that drifted beyond their band on this runner, so the flag cannot be told from machine noise. Re-run the job; the flagged metrics are listed below, not dismissed.

### Flagged metrics

| Scenario | Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---|
| MultilineGrid | dirty | composeCpuP95Ms | 0.0176 | 0.0193 | +9.97% | 0.0433 | 5% | 44.2% max, beyond band | regressed, controls drifted |

<details><summary>Default: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 357.5 | 355.9 | -0.45% | 0.5887 | 5% | 17.5% max, beyond band | within noise |
| clean | frameP50Ms | 2.7815 | 2.7811 | -0.01% | 0.8182 | 5% | 16.1% max, beyond band | within noise |
| clean | frameP95Ms | 2.8708 | 2.9219 | +1.78% | 0.4848 | 5% | 21.4% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0006 | -7.69% | 0.5368 | 5% | 42.9% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0249 | 0.0258 | +3.62% | 0.9372 | 5% | 26.4% max, beyond band | within noise |
| clean | privateBytes | 23,492,608 | 23,406,592 | -0.37% | 0.3095 | 2% | 1.2% max, held | within noise |
| clean | privatePeakBytes | 23,492,608 | 23,425,024 | -0.29% | 0.4177 | 2% | 1.3% max, held | within noise |
| clean | workingSetBytes | 36,708,352 | 36,786,176 | +0.21% | 0.3939 | 2% | 4.4% max, beyond band | within noise |
| clean | workingSetPeakBytes | 36,730,880 | 36,808,704 | +0.21% | 0.4199 | 2% | 4.4% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 168.5 | 168.1 | -0.23% | 0.9372 | 5% | 18.8% max, beyond band | within noise |
| dirty | frameP50Ms | 5.8571 | 5.8338 | -0.40% | 0.3095 | 5% | 18.1% max, beyond band | within noise |
| dirty | frameP95Ms | 6.139 | 6.3853 | +4.01% | 0.9372 | 5% | 39.1% max, beyond band | within noise |
| dirty | prepareP95Ms | 2.2957 | 2.2421 | -2.34% | 0.1797 | 5% | 52.6% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0155 | 0.0141 | -9.06% | 0.0649 | 5% | 29.0% max, beyond band | within noise |
| dirty | privateBytes | 23,797,760 | 23,781,376 | -0.07% | 0.8182 | 2% | 2.3% max, beyond band | within noise |
| dirty | privatePeakBytes | 23,922,688 | 23,797,760 | -0.52% | 0.4848 | 2% | 1.7% max, held | within noise |
| dirty | workingSetBytes | 37,029,888 | 37,062,656 | +0.09% | 0.5887 | 2% | 4.0% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 37,036,032 | 37,081,088 | +0.12% | 0.4848 | 2% | 4.0% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGrid: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 295.0 | 299.3 | +1.48% | 0.2403 | 5% | 36.1% max, beyond band | within noise |
| clean | frameP50Ms | 3.3494 | 3.3196 | -0.89% | 0.3939 | 5% | 20.9% max, beyond band | within noise |
| clean | frameP95Ms | 3.4904 | 3.4516 | -1.11% | 0.3939 | 5% | 35.6% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0006 | -14.29% | 0.3160 | 5% | 75.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0308 | 0.0297 | -3.57% | 0.4848 | 5% | 19.2% max, beyond band | within noise |
| clean | privateBytes | 28,884,992 | 28,655,616 | -0.79% | 0.3095 | 2% | 1.8% max, held | within noise |
| clean | privatePeakBytes | 28,930,048 | 28,690,432 | -0.83% | 0.4610 | 2% | 1.7% max, held | within noise |
| clean | workingSetBytes | 41,900,032 | 41,758,720 | -0.34% | 0.4848 | 2% | 2.8% max, beyond band | within noise |
| clean | workingSetPeakBytes | 41,932,800 | 41,785,344 | -0.35% | 0.4848 | 2% | 2.8% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 60.5 | 60.0 | -0.71% | 0.8182 | 5% | 4.0% max, held | within noise |
| dirty | frameP50Ms | 15.6442 | 16.0309 | +2.47% | 0.3939 | 5% | 3.7% max, held | within noise |
| dirty | frameP95Ms | 17.8614 | 19.3877 | +8.55% | 0.4848 | 5% | 16.6% max, beyond band | within noise |
| dirty | prepareP95Ms | 13.5226 | 14.1371 | +4.54% | 0.3095 | 5% | 11.3% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0176 | 0.0193 | +9.97% | 0.0433 | 5% | 44.2% max, beyond band | regressed, controls drifted |
| dirty | privateBytes | 29,175,808 | 29,130,752 | -0.15% | 0.8182 | 2% | 4.2% max, beyond band | within noise |
| dirty | privatePeakBytes | 30,703,616 | 30,339,072 | -1.19% | 0.4848 | 2% | 3.3% max, beyond band | within noise |
| dirty | workingSetBytes | 41,699,328 | 41,879,552 | +0.43% | 0.4848 | 2% | 5.3% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 42,860,544 | 42,975,232 | +0.27% | 0.9372 | 2% | 2.9% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGridDistinct: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 292.0 | 291.8 | -0.05% | 1.0000 | 5% | 40.3% max, beyond band | within noise |
| clean | frameP50Ms | 3.3906 | 3.3779 | -0.37% | 0.6991 | 5% | 21.5% max, beyond band | within noise |
| clean | frameP95Ms | 3.8647 | 3.5154 | -9.04% | 0.9372 | 5% | 39.3% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0007 | +7.69% | 1.0000 | 5% | 60.0% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.031 | 0.0323 | +4.36% | 0.4848 | 5% | 38.7% max, beyond band | within noise |
| clean | privateBytes | 25,307,136 | 25,311,232 | +0.02% | 0.9740 | 2% | 1.1% max, held | within noise |
| clean | privatePeakBytes | 25,370,624 | 25,372,672 | +0.01% | 0.8593 | 2% | 0.9% max, held | within noise |
| clean | workingSetBytes | 38,565,888 | 38,551,552 | -0.04% | 0.8182 | 2% | 1.0% max, held | within noise |
| clean | workingSetPeakBytes | 38,586,368 | 38,572,032 | -0.04% | 0.8182 | 2% | 1.0% max, held | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 130.7 | 129.9 | -0.63% | 0.8182 | 5% | 18.6% max, beyond band | within noise |
| dirty | frameP50Ms | 7.017 | 7.1639 | +2.09% | 0.2403 | 5% | 15.7% max, beyond band | within noise |
| dirty | frameP95Ms | 9.445 | 9.9725 | +5.58% | 0.2403 | 5% | 37.1% max, beyond band | within noise |
| dirty | prepareP95Ms | 3.5508 | 3.5776 | +0.75% | 0.2403 | 5% | 24.8% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0153 | 0.0166 | +8.52% | 0.4177 | 5% | 27.4% max, beyond band | within noise |
| dirty | privateBytes | 25,608,192 | 25,651,200 | +0.17% | 0.4545 | 2% | 0.5% max, held | within noise |
| dirty | privatePeakBytes | 25,706,496 | 25,780,224 | +0.29% | 0.4567 | 2% | 0.6% max, held | within noise |
| dirty | workingSetBytes | 39,114,752 | 39,008,256 | -0.27% | 0.6991 | 2% | 1.0% max, held | within noise |
| dirty | workingSetPeakBytes | 39,114,752 | 39,047,168 | -0.17% | 0.6991 | 2% | 1.0% max, held | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. A metric regresses only when p < 0.05 and its median shift is beyond the band (a rise in an exact budget always regresses); `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.
