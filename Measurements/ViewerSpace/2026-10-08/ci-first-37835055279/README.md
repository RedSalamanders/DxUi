# First hosted paired CI result — PR #69

This archive retains the 111 report files from workflow run 37835055279, job 113509851836, artifact 11576126309 (`paired-benchmark-x64-Release`). `summary.comparison.json` contains the original `summary.json` bytes under the comparison-specific name required for retained measurements. The gate metadata is retained byte-for-byte as `verdict.txt` so it is not mistaken for a library benchmark receipt. `manifest.txt` records SHA-256, byte length, archived filename and original source path; every copied file was hash-checked against the raw report directory. `receipt.txt` records the artifact ZIP SHA-256 and CI identifiers.

## Measurement identity

- Baseline: `bea676a1f8418141f61cab9326b9b910e89a14b4`, source fingerprint `41423433DDF45EFBBBAFA7B35D9A16D2F9D057E4271779D4D8EAC4955C481051`.
- Candidate: PR merge commit `e9806509a952df01a77bcef568ca9d7c40501519`, source fingerprint `F826386B05B760C3E8FFFFEEB857B3F889398C1E10CD57AEBBB048D20AD5E54F`.
- Runner: hosted shared x64 Release runner `runnervmfi6oq`; three ABBA repetitions, six runs per side, in `A1, B1, B2, A2, A3, B3, B4, A4, A5, B5, B6, A6` order.
- Scenarios: `Default`, `MultilineGrid`, and `MultilineGridDistinct`; each set's minimum attainable p-value was 0.0021645.

## Reviewed finding

**Overall verdict: inconclusive. Preserve the Default finding as unresolved pending a repeat.** The `Default` set reports one regressed metric: dirty `composeCpuP95Ms` rose from 0.01275 ms to 0.01425 ms (+11.76%, p=0.0238095), beyond the 5% timing band. But the metric's same-binary control drift was 32.79%, so the hosted runner did not hold steady for this measurement. The gate therefore marks the flag unconfirmed and the scenario inconclusive; this is not evidence of a confirmed code regression or a pass.

`MultilineGrid` and `MultilineGridDistinct` each report `within-noise-budget`, with zero regressed metrics. This means no regression was established in those sets; it does not prove no change. The 32.79% drift is specific to the flagged Default metric. Other metrics and scenarios also contain unstable controls (the verdict reports 6/6 unstable controls for Default, 5/6 for MultilineGrid and 6/6 for MultilineGridDistinct), so their quiet-run sensitivity is limited.

Each scenario judges 26 metrics, making chance flags possible. A repeat on a quiet runner is needed to determine whether the Default timing signal persists. Keep this finding and its raw comparisons visible until then; do not rebaseline from this run. This library-only benchmark also does not establish consumer presentation latency or native ARM64 runtime behavior.

The run completed at 2026-10-08 20:06:23 UTC. Artifact ZIP SHA-256: `9c56ad66f6399a711343e1d3d5d82a5a8c0fced2f19e269333d5c0b542f12873`.
