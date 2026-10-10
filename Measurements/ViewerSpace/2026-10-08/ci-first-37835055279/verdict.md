## Paired benchmark: Inconclusive

The run cannot say. A metric was flagged but its same-binary controls drifted beyond its band, or the set is too small to judge a timing. It does not pass: re-run the job for a new runner, or repeat the set on a quiet machine, and treat the flagged metrics as unresolved findings, not as noise.

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
| Default | Inconclusive | advice-required | 1 | 0 | 10 of 26 | 0 of 6 |
| MultilineGrid | No regression established | within-noise-budget | 0 | 0 | 8 of 26 | 1 of 6 |
| MultilineGridDistinct | No regression established | within-noise-budget | 0 | 0 | 20 of 26 | 0 of 6 |

- **Default**: 1 regressed metric has same-binary controls that drifted beyond their band on this runner, so the flag cannot be told from machine noise. Re-run the job; the flagged metrics are listed below, not dismissed.

### Flagged metrics

| Scenario | Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---:|---|---|
| Default | dirty | composeCpuP95Ms | 0.0128 | 0.0143 | +11.76% | 0.0238 | 5% | 32.8% max, beyond band | regressed, controls drifted |

<details><summary>Default: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 323.2 | 314.9 | -2.57% | 0.3939 | 5% | 22.1% max, beyond band | within noise |
| clean | frameP50Ms | 3.0557 | 3.077 | +0.70% | 0.1797 | 5% | 15.6% max, beyond band | within noise |
| clean | frameP95Ms | 3.353 | 3.7466 | +11.74% | 0.3939 | 5% | 26.0% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0004 | 0.0004 | 0.00% | 1.0000 | 5% | 33.3% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0253 | 0.0283 | +11.88% | 0.1320 | 5% | 26.2% max, beyond band | within noise |
| clean | privateBytes | 23,416,832 | 23,334,912 | -0.35% | 0.1948 | 2% | 2.0% max, beyond band | within noise |
| clean | privatePeakBytes | 23,449,600 | 23,377,920 | -0.31% | 0.1797 | 2% | 2.3% max, beyond band | within noise |
| clean | workingSetBytes | 36,691,968 | 36,794,368 | +0.28% | 0.6234 | 2% | 4.0% max, beyond band | within noise |
| clean | workingSetPeakBytes | 36,722,688 | 36,816,896 | +0.26% | 0.6991 | 2% | 4.0% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 143.8 | 148.7 | +3.39% | 0.8182 | 5% | 87.6% max, beyond band | within noise |
| dirty | frameP50Ms | 6.2864 | 6.3943 | +1.72% | 0.3939 | 5% | 37.6% max, beyond band | within noise |
| dirty | frameP95Ms | 7.5302 | 8.5742 | +13.86% | 0.3939 | 5% | 72.4% max, beyond band | within noise |
| dirty | prepareP95Ms | 2.9041 | 2.7148 | -6.52% | 0.9372 | 5% | 43.9% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0128 | 0.0143 | +11.76% | 0.0238 | 5% | 32.8% max, beyond band | regressed, controls drifted |
| dirty | privateBytes | 23,791,616 | 23,601,152 | -0.80% | 0.0108 | 2% | 1.4% max, held | within noise |
| dirty | privatePeakBytes | 23,871,488 | 23,611,392 | -1.09% | 0.0087 | 2% | 1.4% max, held | within noise |
| dirty | workingSetBytes | 37,025,792 | 37,048,320 | +0.06% | 1.0000 | 2% | 4.0% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 37,036,032 | 37,064,704 | +0.08% | 0.9372 | 2% | 4.0% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 2,160 | 2,160 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGrid: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 351.0 | 350.9 | -0.04% | 0.9372 | 5% | 19.2% max, beyond band | within noise |
| clean | frameP50Ms | 2.8234 | 2.826 | +0.09% | 0.8182 | 5% | 17.2% max, beyond band | within noise |
| clean | frameP95Ms | 2.965 | 2.9675 | +0.08% | 0.8182 | 5% | 23.9% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0006 | 0.0005 | -9.09% | 0.3463 | 5% | 28.6% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0267 | 0.0266 | -0.19% | 0.7835 | 5% | 19.5% max, beyond band | within noise |
| clean | privateBytes | 28,688,384 | 28,536,832 | -0.53% | 0.0931 | 2% | 3.5% max, beyond band | within noise |
| clean | privatePeakBytes | 28,743,680 | 28,565,504 | -0.62% | 0.0411 | 2% | 3.6% max, beyond band | within noise |
| clean | workingSetBytes | 41,932,800 | 41,482,240 | -1.07% | 0.0931 | 2% | 4.2% max, beyond band | within noise |
| clean | workingSetPeakBytes | 41,963,520 | 41,504,768 | -1.09% | 0.0649 | 2% | 4.2% max, beyond band | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 63.2 | 65.8 | +4.08% | 0.2403 | 5% | 8.4% max, beyond band | within noise |
| dirty | frameP50Ms | 14.8688 | 14.9665 | +0.66% | 0.9372 | 5% | 6.4% max, beyond band | within noise |
| dirty | frameP95Ms | 16.0843 | 16.869 | +4.88% | 0.5887 | 5% | 13.0% max, beyond band | within noise |
| dirty | prepareP95Ms | 12.5758 | 12.6099 | +0.27% | 0.9372 | 5% | 14.2% max, beyond band | within noise |
| dirty | composeCpuP95Ms | 0.0199 | 0.0229 | +15.11% | 0.1277 | 5% | 33.3% max, beyond band | within noise |
| dirty | privateBytes | 30,027,776 | 29,904,896 | -0.41% | 0.9372 | 2% | 3.4% max, beyond band | within noise |
| dirty | privatePeakBytes | 30,898,176 | 30,521,344 | -1.22% | 0.2403 | 2% | 3.0% max, beyond band | within noise |
| dirty | workingSetBytes | 42,848,256 | 42,297,344 | -1.29% | 0.6991 | 2% | 3.8% max, beyond band | within noise |
| dirty | workingSetPeakBytes | 43,059,200 | 42,928,128 | -0.30% | 0.4848 | 2% | 3.4% max, beyond band | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

<details><summary>MultilineGridDistinct: all 26 metrics</summary>

| Phase | Metric | Baseline median | Candidate median | Change | p | Band | Same-binary controls | Outcome |
|---|---|---|---:|---:|---:|---:|---|---|
| clean | fps | 350.0 | 351.8 | +0.53% | 0.3095 | 5% | 4.9% max, held | within noise |
| clean | frameP50Ms | 2.8417 | 2.8178 | -0.84% | 0.0931 | 5% | 1.3% max, held | within noise |
| clean | frameP95Ms | 2.9497 | 2.9514 | +0.06% | 0.9372 | 5% | 17.2% max, beyond band | within noise |
| clean | prepareP95Ms | 0.0007 | 0.0006 | -14.29% | 0.1234 | 5% | 28.6% max, beyond band | within noise |
| clean | composeCpuP95Ms | 0.0264 | 0.0268 | +1.71% | 1.0000 | 5% | 21.9% max, beyond band | within noise |
| clean | privateBytes | 25,315,328 | 25,233,408 | -0.32% | 0.1797 | 2% | 0.9% max, held | within noise |
| clean | privatePeakBytes | 25,374,720 | 25,286,656 | -0.35% | 0.3723 | 2% | 0.7% max, held | within noise |
| clean | workingSetBytes | 38,592,512 | 38,557,696 | -0.09% | 0.4199 | 2% | 0.4% max, held | within noise |
| clean | workingSetPeakBytes | 38,617,088 | 38,580,224 | -0.10% | 0.4177 | 2% | 0.4% max, held | within noise |
| clean | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | cppAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| clean | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | fps | 156.5 | 156.8 | +0.21% | 0.6991 | 5% | 11.5% max, beyond band | within noise |
| dirty | frameP50Ms | 6.2724 | 6.2531 | -0.31% | 0.3939 | 5% | 1.2% max, held | within noise |
| dirty | frameP95Ms | 6.7354 | 6.9197 | +2.74% | 0.3939 | 5% | 7.0% max, beyond band | within noise |
| dirty | prepareP95Ms | 3.0687 | 3.0422 | -0.86% | 0.5887 | 5% | 4.2% max, held | within noise |
| dirty | composeCpuP95Ms | 0.0172 | 0.0175 | +2.04% | 0.7987 | 5% | 15.0% max, beyond band | within noise |
| dirty | privateBytes | 25,737,216 | 25,733,120 | -0.02% | 0.7251 | 2% | 1.4% max, held | within noise |
| dirty | privatePeakBytes | 25,778,176 | 25,800,704 | +0.09% | 0.8182 | 2% | 1.2% max, held | within noise |
| dirty | workingSetBytes | 39,155,712 | 39,272,448 | +0.30% | 0.2229 | 2% | 1.3% max, held | within noise |
| dirty | workingSetPeakBytes | 39,172,096 | 39,274,496 | +0.26% | 0.2403 | 2% | 1.2% max, held | within noise |
| dirty | surfaceBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | replacementPeakBytes | 3,686,400 | 3,686,400 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | cppAllocations | 1,080 | 1,080 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |
| dirty | composeAllocations | 0 | 0 | 0.00% | 1.0000 | exact | 0.0% max, held | within noise |

</details>

The retained receipts, comparisons and `summary.json` are in the `paired-benchmark-x64-Release` artifact. A metric regresses only when p < 0.05 and its median shift is beyond the band (a rise in an exact budget always regresses); `Specs/Core/Core_PerformanceAndResources.md` says what a verdict establishes and what a finding requires.
