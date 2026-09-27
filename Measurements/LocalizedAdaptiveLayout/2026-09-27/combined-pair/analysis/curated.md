# Paired DxUi benchmark curation

All eight run receipts and twelve comparison outputs passed identity, input, source/binary freeze, workload, hidden-work, retention-bound, and default-threshold checks.

## Comparison flags

| Comparison | Status | Flags |
|---|---|---|
| Default-A1-B1 | advice-required | clean/frameP95Ms, clean/composeCpuP95Ms, dirty/composeCpuP95Ms, dirty/privateBytes, dirty/privatePeakBytes |
| Default-A1-B2 | advice-required | clean/fps, clean/frameP50Ms, clean/frameP95Ms, clean/composeCpuP95Ms, dirty/composeCpuP95Ms |
| Default-A2-B1 | advice-required | dirty/composeCpuP95Ms, dirty/privateBytes, dirty/privatePeakBytes, dirty/workingSetBytes, dirty/workingSetPeakBytes |
| Default-A2-B2 | advice-required | clean/fps, clean/frameP50Ms, clean/frameP95Ms, clean/privateBytes, clean/privatePeakBytes |
| Default-A1-A2 | advice-required | clean/frameP95Ms, clean/composeCpuP95Ms, dirty/composeCpuP95Ms |
| Default-B1-B2 | advice-required | clean/fps, clean/frameP50Ms, clean/frameP95Ms, clean/privateBytes, clean/privatePeakBytes, clean/workingSetBytes, clean/workingSetPeakBytes |
| MultilineGridRetention-A1-B1 | advice-required | clean/privateBytes, clean/privatePeakBytes, clean/workingSetPeakBytes, dirty/privateBytes, dirty/privatePeakBytes, dirty/workingSetBytes, dirty/workingSetPeakBytes |
| MultilineGridRetention-A1-B2 | advice-required | clean/frameP95Ms, clean/prepareP95Ms, clean/privateBytes, clean/privatePeakBytes, dirty/privateBytes, dirty/privatePeakBytes, dirty/workingSetBytes, dirty/workingSetPeakBytes |
| MultilineGridRetention-A2-B1 | advice-required | clean/privateBytes, clean/privatePeakBytes, clean/workingSetBytes, clean/workingSetPeakBytes, dirty/privateBytes, dirty/privatePeakBytes, dirty/workingSetBytes, dirty/workingSetPeakBytes |
| MultilineGridRetention-A2-B2 | advice-required | clean/prepareP95Ms, clean/composeCpuP95Ms, clean/privateBytes, clean/privatePeakBytes, clean/workingSetBytes, clean/workingSetPeakBytes, dirty/composeCpuP95Ms, dirty/privateBytes, dirty/privatePeakBytes, dirty/workingSetBytes, dirty/workingSetPeakBytes |
| MultilineGridRetention-A1-A2 | advice-required | clean/frameP95Ms |
| MultilineGridRetention-B1-B2 | advice-required | clean/fps, clean/frameP95Ms, clean/prepareP95Ms, dirty/fps, dirty/frameP50Ms, dirty/frameP95Ms, dirty/prepareP95Ms, dirty/composeCpuP95Ms |

## Four-cross-pair observed medians

These are observed B-versus-A deltas across the four cross-variant comparisons. They do not identify cause.

| Workload | Case | Metric | Median delta | Median change |
|---|---|---:|---:|---:|
| Default | clean | composeCpuP95Ms | 5E-05 | 0.632% |
| Default | clean | frameP95Ms | 0.0327 | 6.967% |
| Default | clean | composeAllocations | 0 | n/a |
| Default | clean | replacementPeakBytes | 0 | 0% |
| Default | clean | privatePeakBytes | -333824 | -1.234% |
| Default | clean | cppAllocations | 0 | n/a |
| Default | clean | workingSetBytes | -665600 | -1.616% |
| Default | clean | workingSetPeakBytes | -638976 | -1.55% |
| Default | clean | fps | -105.192102 | -4.146% |
| Default | clean | surfaceBytes | 0 | 0% |
| Default | clean | privateBytes | -296960 | -1.103% |
| Default | clean | prepareP95Ms | 0 | 0% |
| Default | clean | frameP50Ms | 0.01885 | 4.93% |
| Default | dirty | composeCpuP95Ms | 0.0056 | 31.5% |
| Default | dirty | frameP95Ms | 0.04215 | 2.132% |
| Default | dirty | composeAllocations | 0 | n/a |
| Default | dirty | replacementPeakBytes | 0 | 0% |
| Default | dirty | privatePeakBytes | 710656 | 2.461% |
| Default | dirty | cppAllocations | 0 | 0% |
| Default | dirty | workingSetBytes | 546816 | 1.286% |
| Default | dirty | workingSetPeakBytes | 548864 | 1.291% |
| Default | dirty | fps | -0.973969 | -0.183% |
| Default | dirty | surfaceBytes | 0 | 0% |
| Default | dirty | privateBytes | 722944 | 2.506% |
| Default | dirty | prepareP95Ms | 0.0114 | 0.879% |
| Default | dirty | frameP50Ms | 0.00335 | 0.207% |
| MultilineGridRetention | clean | composeCpuP95Ms | 0.0001 | 1.064% |
| MultilineGridRetention | clean | frameP95Ms | 0.00955 | 1.965% |
| MultilineGridRetention | clean | composeAllocations | 0 | n/a |
| MultilineGridRetention | clean | replacementPeakBytes | 0 | 0% |
| MultilineGridRetention | clean | privatePeakBytes | 4036608 | 12.507% |
| MultilineGridRetention | clean | cppAllocations | 0 | n/a |
| MultilineGridRetention | clean | workingSetBytes | 1601536 | 3.582% |
| MultilineGridRetention | clean | workingSetPeakBytes | 1570816 | 3.495% |
| MultilineGridRetention | clean | fps | -9.485613 | -0.381% |
| MultilineGridRetention | clean | surfaceBytes | 0 | 0% |
| MultilineGridRetention | clean | privateBytes | 3057664 | 9.702% |
| MultilineGridRetention | clean | prepareP95Ms | 5E-05 | 16.667% |
| MultilineGridRetention | clean | frameP50Ms | 0.00485 | 1.296% |
| MultilineGridRetention | dirty | composeCpuP95Ms | 0 | 0.17% |
| MultilineGridRetention | dirty | frameP95Ms | -9.48585 | -52.445% |
| MultilineGridRetention | dirty | composeAllocations | 0 | n/a |
| MultilineGridRetention | dirty | replacementPeakBytes | 0 | 0% |
| MultilineGridRetention | dirty | privatePeakBytes | 8448000 | 25.363% |
| MultilineGridRetention | dirty | cppAllocations | 0 | 0% |
| MultilineGridRetention | dirty | workingSetBytes | 1900544 | 4.057% |
| MultilineGridRetention | dirty | workingSetPeakBytes | 2095104 | 4.459% |
| MultilineGridRetention | dirty | fps | 65.605937 | 113.58% |
| MultilineGridRetention | dirty | surfaceBytes | 0 | 0% |
| MultilineGridRetention | dirty | privateBytes | 6934528 | 20.98% |
| MultilineGridRetention | dirty | prepareP95Ms | -9.40815 | -53.701% |
| MultilineGridRetention | dirty | frameP50Ms | -9.25255 | -53.093% |

## Cost interpretation

All four cross-variant comparisons in each workload and all four same-variant controls have advice-required flags. The controls show measurement variability; they do not prove that a new effect is absent. This pair establishes neither a stable combined-candidate effect nor its absence, so possible effects outside accepted scope remain open.

The Default clean frame median (+0.01885 ms) and P95 (+0.0327 ms) observations overlap the previously accepted grid median (+0.025 ms) and menu clean P95 (+0.049 ms), respectively. Retention dirty private memory (+6.61 MiB) overlaps the accepted grid +6–7 MiB scope. Do not add these overlapping observations as new costs or attribute them to a component.

Outside that stated scope, Default dirty compose CPU P95 is +0.0056 ms (+31.5%, 5.6 microseconds), Default dirty private bytes +0.69 MiB, and retention working set +1.53 MiB clean / +1.81 MiB dirty. These are observed potential effects, not qualified stable costs; keep them open.

Four-cross-pair medians for Default dirty frame P95 (+2.13%) and retention clean frame P50/P95 (+1.30%/+1.97%) are below the 5% timing guardrail. Retention dirty frame P50/P95 show roughly 53% lower times, but the B1/B2 control also flags dirty timing variation. Treat those timing observations as unstable, not a cost or benefit.

## Previously accepted cost context

Previously accepted reference costs (context only): grid +6–7 MiB and +0.025 ms grid frame median; menu +0.049 ms clean frame P95 and +0.165 ms dirty frame P95. These historical figures are not subtracted from this pair and are not causal estimates.

Same-variant controls remain listed among the comparison flags above. Advice-required results are retained as findings; no thresholds or baselines are changed.
