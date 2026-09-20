# Final alternating baseline/candidate investigation

Ten pairs were declared before collection, alternating baseline/candidate then candidate/baseline.
Every process, raw five-round receipt, comparison and flag is retained. Collection checks for other
active builds/tests before each process; the independently running RedXe build had finished first.
The original baseline and earlier flagged runs are preserved in adjacent folders. No threshold,
workload, baseline or allocation budget was replaced. Collection and analysis scripts are retained
as text alongside their output.

Both executables use the identical common fixture, compiler, WARP runtime, power policy and machine.
The unchanged implementation lives in the isolated `layout-performance-baseline` worktree at
`f7fe1e4`; only test adapters were added there. Its compiled-source fingerprint matches the original
pre-implementation baseline. Candidate and baseline hashes are in [analysis.txt](analysis.txt).

| Metric (median of ten process medians) | Baseline | Candidate |
| --- | ---: | ---: |
| Clean completed offscreen FPS | 2537.79 | 2589.39 |
| Dirty completed offscreen FPS | 581.71 | 580.05 |
| Clean frame p95, ms | 0.46185 | 0.45805 |
| Dirty frame p95, ms | 1.95180 | 1.91550 |
| Dirty preparation p95, ms | 1.24770 | 1.26085 |
| Dirty CPU composition p95, ms | 0.01720 | 0.01730 |
| Clean private bytes | 26,761,216 | 27,267,072 |
| Dirty private bytes | 28,999,680 | 28,917,760 |
| Clean working-set bytes | 40,220,672 | 40,923,136 |
| Dirty working-set bytes | 42,588,160 | 42,442,752 |

The earlier dirty composition p95 increase does not reproduce consistently (median paired delta
-1.11%; three of ten pairs flag). Clean frame p95 flags in five pairs but improves in the other five.
Clean private memory deserves continued monitoring: six pairs flag, median paired delta +3.34%,
while the ratio of process medians is +1.89%. These are different aggregations, both reported.
Exploratory paired bootstrap intervals include zero for every timing/process-memory metric; these
wide intervals do **not** prove equivalence or turn the individual flags into passes. This controlled
repeat does not confirm a systematic regression. The investigation is retained with its uncertainty;
consumer hardware/presentation and long-run measurements remain required before product closeout.

Deterministic resource budgets pass unchanged in every round: one 3,686,400-byte surface, identical
replacement peak, zero clean C++ allocations, zero composition allocations and 2,160 dirty C++
allocations per 40 frames. Hidden preparation/composition is zero in every receipt. The independent
localized action fixture improves 847 to 426 microseconds per 24,000 layouts (adjacent records).

This is paired offscreen library evidence, not displayed application FPS, native mixed-DPI or an
acceptance of additional memory cost. If later paired product evidence confirms degradation,
the existing advice requirement still applies; no trade-off or waiver is introduced here.
