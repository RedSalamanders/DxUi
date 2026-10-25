# Hosted 2fed paired benchmark — final packet (25 October 2026)

This is the completed PR #102 paired study for DxUi head `2fed82465dcee9f213753754e0e8e54f353cf38f`. The benchmark measured synthetic merge `9ac8cc0bb857a0a71f0ea0e94ef3a51c69be7371`, whose first parent/base is `4faf16ed647df71cd7d5623c1682738592771c3e`; the merge's second parent is the PR head. The six hosted native jobs passed, but paired verdict publication and the overall CI gate failed because the base policy is not independently qualified. The provider's stated outcome is **inconclusive**, not a candidate pass or performance acceptance.

## Paired result and controls

The provider reported all three scenario sets within their noise budgets and zero regressed metrics. Independent replay matched 144 receipt hashes, 936 stored paired effects, all 78 metric rows and their p-values/Holm adjustments, and the CSV projection. Same-side control replay found 71/72 controls flagged on at least one metric; 260 of 1,872 per-metric observations crossed recorded investigation bands and 1,612 held. Thirty-seven of 78 keys held across all 24 same-side comparisons; none drifted across all 24. These control counts diagnose this packet only and are not a policy qualification.

The baseline and candidate measured EmbeddedTests executables match their archived receipt identities: baseline SHA-256 `8306E4416B733B45966D6622FA462AF7DBF96843ABC0F7F92928472AC291DFF7`; candidate SHA-256 `2DFD6F8FBD50BAD75F753E443830F42170936D5FF96EADFC553917F5BEC6C02E`. The provider archive SHA-256 is `0BE9B0CF5837D64831988B247E45343E067307D1E2C125E829DE1AA2BB4DC545`.

## Measurement limits

The legacy `composeCpuP95Ms` metric is elapsed QPC wall time around `view.Composite`, not thread CPU time. QPC was 10 MHz (0.1 microseconds per tick); the timing arrays lie on this clock grid, so clean Prepare values near a few ticks are floor-sensitive. This interpretation is source-backed by `Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h` at the measured merge: lines 122–125 select and check the clock, lines 166–168 time `view.Composite`, and lines 195–196 serialize the elapsed duration under the legacy field name.

The observer recorded 6,152 samples, 39 gaps longer than two seconds, 6.34 ms collection-bracket P95 and 4.82 seconds maximum. Its 34.65% weighted whole-host busy value comes from aggregate `GetSystemTimes` counters and includes the benchmark, observer and any other processes. It does not establish a quiet host or prove external contention or preemption caused drift. Replay found no control-pair matching defect and established no cause for the timing variation.

The archived snapshot `hosted-2fed-pr102-native` correctly records that the paired job was pending when captured. This final packet adds the later result without rewriting that snapshot. The older CD35 and SDK1714 studies remain separate and must not be pooled with this cohort. No thresholds, fixtures, investigation bands or acceptance policy changed.

## Files

- [Derived public comparison](public-projection.comparison.json): exact staged privacy-safe structured projection.
- [Per-metric CSV](metric-rows.csv): exact staged projection; `.gitattributes` preserves its bytes.
- [Independent audit](independent-audit.md): curated replay scope, identities and evidence limits.

The separate [root lead replay](root-lead-replay.md) rehashes all 144 receipts and both measured executables, replays the stored-effect p/Holm arithmetic and verifies all 1,170 CSV cells. The independent audit derives the effects from raw rounds.
