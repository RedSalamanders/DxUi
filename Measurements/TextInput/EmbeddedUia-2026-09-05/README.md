# Embedded UIA: retained short comparison

The predeclared A1/B1/B2/A2 comparison used retained ad88833 versus 26459b4, Debug then Release,
identical v2 inputs, 83 controls / 1,000 model rows and five 40-frame rounds. Only the measurement
process and its children used CPU affinity 0xFFFF. Every raw, matched and same-source report is retained.

Acceptance remains OPEN. All four nearest-pair comparisons flagged different metrics. Same-source
controls also vary: unchanged Debug baseline flags eight bands; Debug candidate flags clean p95;
Release baseline is within bands, while Release candidate flags clean FPS, p95 and CPU composition.
These controls do not establish a repeatable causal library regression and do not waive any failure.
The same source and fixture hashes are recorded in the raw files. No threshold or baseline was replaced.

| Comparison | Result | Flagged changes |
| --- | --- | --- |
| EmbeddedUia-20260905-01-Debug-B1.json.matched.json | advice-required | clean/composeCpuP95Ms +12.77%; dirty/composeCpuP95Ms +8.00%; dirty/privateBytes +3.11% |
| EmbeddedUia-20260905-01-Debug-B2.json.matched.json | advice-required | clean/frameP95Ms +13.27% |
| EmbeddedUia-20260905-01-Release-B1.json.matched.json | advice-required | dirty/privateBytes +2.61%; dirty/privatePeakBytes +2.52% |
| EmbeddedUia-20260905-01-Release-B2.json.matched.json | advice-required | clean/fps -7.66%; clean/frameP95Ms +24.77%; clean/composeCpuP95Ms +7.87% |
| EmbeddedUia-20260905-01-Debug-A2.json.same-source.json | advice-required | clean/frameP95Ms +5.69%; clean/composeCpuP95Ms +5.32%; dirty/fps -6.04%; dirty/frameP50Ms +5.79%; dirty/frameP95Ms +7.32%; dirty/prepareP95Ms +5.85%; dirty/composeCpuP95Ms +8.00%; dirty/privateBytes +2.23% |
| EmbeddedUia-20260905-01-Debug-B2.json.same-source.json | advice-required | clean/frameP95Ms +15.77% |
| EmbeddedUia-20260905-01-Release-A2.json.same-source.json | within-noise-budget | None |
| EmbeddedUia-20260905-01-Release-B2.json.same-source.json | advice-required | clean/fps -6.24%; clean/frameP95Ms +22.26%; clean/composeCpuP95Ms +5.49% |

The component adds no gallery pixels. Local library suites and ARM64 cross-builds pass; RedXe has
passed full x64 Debug/Release integration and an actual OS UIA discovery/property-event fixture using
synthetic devices. This benchmark keeps accessibility inactive, and proves neither active-provider resource
cost nor application/hardware acceptance. A separately declared longer fixture is being considered to
increase warmup and round duration symmetrically for both retained sources. It must keep the same thresholds
and cannot compare long runs with these short runs. Canonical RedXe adoption remains gated.
