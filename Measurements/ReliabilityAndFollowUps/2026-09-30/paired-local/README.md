# Local paired reliability follow-ups benchmark, 30 September 2026

`performance-paired.ps1 -BaselineRevision 2079eed -Scenario Default,MultilineGrid,MultilineGridDistinct -Repetitions 3`
ran three interleaved A, B, B, A passes per scene on the developer laptop, completed at 16:36 UTC. It is the
performance evidence for the [reliability follow-ups](../../../../Specs/Plans/Done/ReliabilityAndFollowUps_2026-09-30.md),
and the first set judged by the rank-test verdict that plan added.

- A is `main` at `2079eed` (the PowerShell tooling merge) in a detached worktree the runner made; B is the plan's
  integrated tree at `4df0fe0`. Library source fingerprints: A `2F1C6E959C…`, B `BC93C31539…`; executables A
  `537ABE98E5…`, B `C171344251…`. All 36 receipts share benchmark fingerprint `F9D66318E8…`: both trees ran B's
  harness, which differs from A's only in PowerShell files, so A's build is `main`'s. Every receipt says
  `sourceDirty: true`: A's worktree carries the runner's harness overlay, and B's checkout held an uncommitted edit to
  the plan's text during the run. The fingerprints, which cover the library inputs only, identify what was measured.
  The runner builds each tree once before measuring, so every receipt also says `buildSkipped: true`.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, WARP 10.0.26100.9278, compiler 195136257, x64 Release, 96 DPI,
  Balanced power plan. A developer laptop, not a quiet fixture.
- Each scene ran A1, B1, B2, A2, A3, B3, B4, A4, A5, B5, B6, A6: six runs per side of five 40-frame rounds each. A run's
  value is the median of its rounds. The verdict compares A's six run medians with B's by an exact two-sided
  Mann-Whitney U test (six against six can reach p = 0.0022) with the investigation band (5% timing, 2% memory), and
  exact budgets stay exact; spread is each side's range over its median.
- Files: the 36 receipts, each receipt's own comparator output (`*.json.comparison.json`, `unpaired`: it passed
  validation), the 36 pass-by-pass comparisons (`*-vs-*.comparison.json`, kept for continuity) and the runner's
  `summary.json` with every set verdict, stored as `summary.receipt.txt` because every `.json` here is read as a
  receipt. `SHA256SUMS` lists all 109.
- MB are 10^6 bytes. Prepare p95 of a clean frame is about 0.001 ms (0.0008 to 0.0012), so its percentages are
  noise of a tiny number.

## Default (`dxui-complex-ui-v2`)

| Phase | Metric | A median | B median | Change | p | A spread | B spread | Verdict |
|---|---|---:|---:|---:|---:|---:|---:|---|
| clean | FPS | 946.2 | 943.0 | -0.3% | 0.937 | 9% | 21% | within-noise |
| clean | Frame p50 (ms) | 1.02 | 1.02 | +0.3% | 1.000 | 5% | 20% | within-noise |
| clean | Frame p95 (ms) | 1.27 | 1.32 | +3.9% | 0.485 | 17% | 24% | within-noise |
| clean | Prepare p95 (ms) | 0.00 | 0.00 | -12.5% | 0.613 | 42% | 76% | within-noise |
| clean | Private (MB) | 25.09 | 25.62 | +2.1% | 0.485 | 6% | 3% | within-noise |
| clean | Working set (MB) | 41.31 | 41.83 | +1.3% | 0.485 | 3% | 2% | within-noise |
| dirty | FPS | 340.6 | 323.2 | -5.1% | 0.132 | 14% | 16% | within-noise |
| dirty | Frame p50 (ms) | 2.81 | 2.95 | +4.9% | 0.132 | 12% | 20% | within-noise |
| dirty | Frame p95 (ms) | 3.65 | 3.98 | +9.0% | 0.180 | 20% | 21% | within-noise |
| dirty | Prepare p95 (ms) | 2.28 | 2.39 | +5.0% | 0.240 | 30% | 26% | within-noise |
| dirty | Private (MB) | 26.45 | 26.52 | +0.3% | 0.699 | 2% | 3% | within-noise |
| dirty | Working set (MB) | 42.56 | 42.69 | +0.3% | 0.699 | 1% | 2% | within-noise |

Hidden phase: private bytes 26.47 MB in A and 26.83 MB in B (median; +0.36 MB, p 0.143). At the hide, private bytes fell by more than 0.1 MB in 0 of 6 A runs and 0 of 6 B runs (B: +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 MB).

## MultilineGrid (`dxui-complex-ui-multiline-grid-v1`)

| Phase | Metric | A median | B median | Change | p | A spread | B spread | Verdict |
|---|---|---:|---:|---:|---:|---:|---:|---|
| clean | FPS | 1020.9 | 1022.6 | +0.2% | 1.000 | 15% | 18% | within-noise |
| clean | Frame p50 (ms) | 0.94 | 0.94 | -0.6% | 0.970 | 14% | 19% | within-noise |
| clean | Frame p95 (ms) | 1.18 | 1.23 | +3.9% | 0.699 | 24% | 14% | within-noise |
| clean | Prepare p95 (ms) | 0.00 | 0.00 | -10.0% | 0.448 | 80% | 78% | within-noise |
| clean | Private (MB) | 30.24 | 30.11 | -0.4% | 0.937 | 5% | 7% | within-noise |
| clean | Working set (MB) | 46.69 | 46.40 | -0.6% | 0.589 | 2% | 4% | within-noise |
| dirty | FPS | 87.9 | 79.1 | -10.0% | 0.818 | 40% | 29% | within-noise |
| dirty | Frame p50 (ms) | 11.30 | 12.55 | +11.0% | 0.818 | 58% | 26% | within-noise |
| dirty | Frame p95 (ms) | 12.37 | 13.67 | +10.5% | 0.699 | 59% | 34% | within-noise |
| dirty | Prepare p95 (ms) | 11.22 | 12.63 | +12.6% | 0.699 | 64% | 37% | within-noise |
| dirty | Private (MB) | 32.02 | 32.03 | 0.0% | 0.937 | 6% | 4% | within-noise |
| dirty | Working set (MB) | 48.08 | 47.97 | -0.2% | 0.937 | 4% | 3% | within-noise |

Hidden phase: private bytes 32.39 MB in A and 31.75 MB in B (median; -0.63 MB, p 0.394). At the hide, private bytes fell by more than 0.1 MB in 0 of 6 A runs and 0 of 6 B runs (B: +0.00, +0.00, +0.00, +0.00, +0.00, +0.00 MB).

## MultilineGridDistinct (`dxui-complex-ui-multiline-grid-distinct-v1`)

| Phase | Metric | A median | B median | Change | p | A spread | B spread | Verdict |
|---|---|---:|---:|---:|---:|---:|---:|---|
| clean | FPS | 1006.9 | 1054.5 | +4.7% | 0.310 | 37% | 34% | within-noise |
| clean | Frame p50 (ms) | 0.94 | 0.89 | -4.9% | 0.240 | 49% | 49% | within-noise |
| clean | Frame p95 (ms) | 1.23 | 1.19 | -3.7% | 0.394 | 67% | 70% | within-noise |
| clean | Prepare p95 (ms) | 0.00 | 0.00 | -5.9% | 0.364 | 82% | 150% | within-noise |
| clean | Private (MB) | 27.29 | 27.43 | +0.5% | 0.699 | 3% | 4% | within-noise |
| clean | Working set (MB) | 43.46 | 43.62 | +0.4% | 0.589 | 2% | 3% | within-noise |
| dirty | FPS | 362.7 | 364.5 | +0.5% | 0.699 | 48% | 42% | within-noise |
| dirty | Frame p50 (ms) | 2.67 | 2.64 | -1.3% | 0.699 | 71% | 62% | within-noise |
| dirty | Frame p95 (ms) | 3.44 | 3.52 | +2.3% | 0.818 | 108% | 58% | within-noise |
| dirty | Prepare p95 (ms) | 2.38 | 2.43 | +2.2% | 0.937 | 91% | 68% | within-noise |
| dirty | Private (MB) | 28.47 | 28.61 | +0.5% | 0.699 | 3% | 4% | within-noise |
| dirty | Working set (MB) | 44.88 | 44.97 | +0.2% | 0.937 | 2% | 2% | within-noise |

Hidden phase: private bytes 28.44 MB in A and 27.93 MB in B (median; -0.52 MB, p 0.026). At the hide, private bytes fell by more than 0.1 MB in 0 of 6 A runs and 4 of 6 B runs (B: +0.01, -0.90, -0.87, -0.95, -1.22, +0.01 MB).

## Verdict

- **Deterministic budgets.** Every run of both sides keeps no clean-round C++ allocation, 2,160 (`Default`, one round
  2,145 in every run) or 1,080 (multiline scenes) per dirty round, no composition allocation, and 3,686,400 surface
  bytes and replacement peak.
- **Sets.** All three scenes are `within-noise-budget`: no timing or memory metric regressed or improved, in either
  phase. The pass-by-pass comparator flags all 18 crossings and every one of the 18 same-binary controls drifted
  beyond a band, so no single pass could separate a change from this laptop's noise; the six-run verdict is the result.
- **Timing.** The largest shifts, `Default` dirty FPS -5.1% (p = 0.132) and `MultilineGrid` dirty FPS -10.0%
  (p = 0.818, with 40% and 29% spreads), are not established, and nothing the plan changed runs in these frames: a
  dirty frame sets each card's slider and progress bar and scrolls the grid, no control is hidden, moved or given a
  model, and the benchmark connects no UI Automation target, so window-host accessibility never runs.
- **Hidden memory (item 9).** A hidden grid now releases its text layouts. In `MultilineGridDistinct`, whose cells
  each hold their own layout, private bytes fell by 0.87 to 1.22 MB at the hide in four of B's six runs and in none of
  A's, and B's hidden private bytes are 0.52 MB below A's (p = 0.026). In `Default` and `MultilineGrid` the heap keeps
  the freed blocks committed: at the hide, private bytes stayed the same in every run of either side but two B runs
  of `MultilineGrid`, which grew by one 4 KB page, so process memory does not show the release there. The Grid
  suite's layout counters establish the release itself.