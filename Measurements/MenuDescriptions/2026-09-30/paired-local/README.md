# Local paired described-menu benchmark, 30 September 2026

The quiet local sets the [memory optimization plan](../../../../Specs/Plans/WIP/MenuDescriptionMemory_2026-09-27.md)
asks for, comparing `e47c836` (main after #27, before #24) with `6f769ab` (#24's described menus merged) in the
Default scenario. `performance-paired.ps1 -BaselineRevision e47c836 -CandidateRevision 6f769ab -Scenario Default
-Repetitions 3` ran three interleaved A, B, B, A passes, so six runs per side. The runner judges each metric with an
exact two-sided Mann-Whitney U test (six against six can reach p = 0.0022) and the investigation band (5% timing, 2%
memory); exact budgets stay exact.

- Harness: the runner ran from a worktree of main `a0b4934` whose five C++ benchmark inputs were checked out from
  `cb4cdde`, the harness of the [hosted set](../../2026-09-27/paired-hosted/README.md). The current harness uses the
  interfaces PR 29 renamed and does not compile against these revisions. Every receipt has benchmark fingerprint
  `2DB9197762…`, the hosted set's, so the two sets measured the same fixture (`dxui-complex-ui-v2`). The harness
  worktree is dirty only through those three checked-out files, and both trees are dirty only through the copied
  harness.
- Source fingerprints: A `97DA120629…`, B `48DB8E2C7F…`; executables A `93420B951E…`, B `EB09A6B6AF…`.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, x64 Release, 96 DPI, Balanced power plan. A developer laptop.
- Files: 12 receipts, their own comparator outputs (`*.json.comparison.json`), the 12 pass-by-pass comparisons and the
  runner's summary as `summary.receipt.txt`; `SHA256SUMS` lists all 37.

## Default (`dxui-complex-ui-v2`)

| Phase | Metric | A median | B median | Change | p | Verdict |
|---|---|---:|---:|---:|---:|---|
| clean | fps | 1179.1 | 1126.2 | -4.49% | 0.485 | within-noise |
| clean | frameP95Ms | 1.021 | 1.096 | +7.27% | 0.084 | within-noise |
| clean | privateBytes | 24.77 MB | 24.86 MB | +0.36% | 0.699 | within-noise |
| clean | privatePeakBytes | 24.82 MB | 24.88 MB | +0.26% | 0.699 | within-noise |
| clean | workingSetBytes | 41.40 MB | 41.31 MB | -0.21% | 1.000 | within-noise |
| dirty | fps | 399.3 | 391.5 | -1.93% | 0.699 | within-noise |
| dirty | frameP95Ms | 2.974 | 2.966 | -0.26% | 0.818 | within-noise |
| dirty | privateBytes | 26.49 MB | 26.25 MB | -0.89% | 0.556 | within-noise |
| dirty | privatePeakBytes | 26.52 MB | 26.26 MB | -0.97% | 0.485 | within-noise |
| dirty | workingSetBytes | 42.80 MB | 42.60 MB | -0.47% | 0.069 | within-noise |

Every run keeps no clean-round and 2,160 dirty-round C++ allocations, no composition allocation, and 3,686,400
surface bytes; the set is `within-noise-budget`, with nothing regressed or improved.

## Verdict

The waived increase does not reproduce. Clean private memory differs by +0.36% (24.77 against 24.86 MB, p = 0.70),
against the waiver's +4.6% (1,175,552 bytes) from the September 21 pairs, and dirty private memory by -0.89%
(p = 0.56), against pair three's unaccepted +8.67%. The hosted set had measured clean private memory at -0.50% and
-0.70%. With repeated paired runs within the bands, the plan removes the waiver. A placebo control and a heap
attribution were planned to explain a reproducible increase, and there is none to explain. No timing change is
established either: clean frame p95 is 7.3% higher, beyond its band but with p = 0.084. Clean FPS is 4.5% lower
(p = 0.485). These are directions, not results, and the September 27 correction already found the earlier
clean-frame timing flag not to be a cost of described menus.