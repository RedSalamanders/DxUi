# Hosted paired CI retry — PR #69, attempt 2

This archive retains all 111 report files from workflow run 37835055279, attempt 2, paired-benchmark job 113526687217, artifact 11577283455 (`paired-benchmark-x64-Release`). `summary.comparison.json` contains the original `summary.json` bytes under the comparison-specific name. Gate metadata is retained byte-for-byte as `verdict.txt`. `manifest.txt` records each report's SHA-256, byte length, archived name and source path; copied report bytes were checked against the extracted artifact. `receipt.txt` records the job and artifact provenance, including the ZIP hash.

## Measurement identity

- Baseline: `bea676a1f8418141f61cab9326b9b910e89a14b4`, source fingerprint `41423433DDF45EFBBBAFA7B35D9A16D2F9D057E4271779D4D8EAC4955C481051`.
- Candidate: PR merge commit `e9806509a952df01a77bcef568ca9d7c40501519`, source fingerprint `F826386B05B760C3E8FFFFEEB857B3F889398C1E10CD57AEBBB048D20AD5E54F`.
- Runner: hosted shared x64 Release runner; three ABBA repetitions, six runs per side, in `A1, B1, B2, A2, A3, B3, B4, A4, A5, B5, B6, A6` order.
- Scenarios: `Default`, `MultilineGrid`, and `MultilineGridDistinct`; each set's minimum attainable p-value was 0.0021645.

## Retry result

The retry's gate verdict is **pass**, with all three scenarios `within-noise-budget` and zero regressed or improved metrics. No metric was flagged. This means the retry established no regression under these measurements; it does not establish an improvement or erase the first run's unresolved Default timing signal in [the original hosted result](../ci-first-37835055279/README.md).

Controls were unstable despite the pass: the gate reports 6/6 unstable same-binary controls in each scenario. Metric-level control drift exceeded its band for 12/26 Default metrics, 17/26 MultilineGrid metrics and 8/26 MultilineGridDistinct metrics. The largest drifts were Default dirty `composeCpuP95Ms` at 53.00%, MultilineGrid dirty `composeCpuP95Ms` at 30.89%, and MultilineGridDistinct clean `composeCpuP95Ms` at 29.75%. Since no metric regressed, these drifts did not turn the retry into an inconclusive gate verdict, but they limit sensitivity to smaller shifts.

The report completed at 2026-10-08T20:42:23.6533266Z. Artifact ZIP SHA-256: `02f06929863e333b4506226364031ab829b354b7e3b329c80f488d1e756bc672`.