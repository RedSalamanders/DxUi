# Local paired review follow-up benchmark, 30 September 2026

Two serial A1/B1/B2/A2 sets per scene, run back to back at 07:33 and 07:34 UTC: the performance evidence for the
[review follow-ups](../../../../Specs/Plans/Done/ReviewFollowUps_2026-09-29.md) (the grid text-layout tables, the
single-line caption layouts, window-host accessibility, menus and the smaller items).

- A is the [review-fix](../../../../Specs/Plans/Done/ReviewFixes_2026-09-29.md) tree the follow-ups start from, in a
  detached worktree of `40c6c21`; B is the follow-up working tree. Both are uncommitted changes on the same commit, which
  `performance-paired.ps1` refuses, so its steps were run by hand: `performance.ps1 -SkipBuild` on each tree's x64
  Release build in the runner's order, then `Tools/compare_performance.py` for the two crossings and the two
  same-binary controls. Receipts therefore say `sourceDirty: true` and `buildSkipped: true`.
- Every B receipt has library source fingerprint `2F1C6E959C…`, the working tree's at close-out, and every A receipt
  `36E9E4A1CB…`. All 24 share benchmark fingerprint `F9D66318E8…`. Executables: A `D476C2C1A4…`, B `C272E93A71…`.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, WARP 10.0.26100.9278, x64 Release, 96 DPI, Balanced power plan.
  A developer laptop, not a quiet fixture.
- `set1/` and `set2/` each hold the 12 receipts, each receipt's own comparator output (`*.json.comparison.json`,
  status `unpaired`: the receipt passed validation) and the four comparisons per scene (`*-vs-*.comparison.json`);
  `SHA256SUMS` lists all 36 files. The comparator judges only the regression direction, so a control's stored status
  is one-sided; the verdict applies the runner's two-sided rule to the controls (an unchanged binary moving beyond a
  band in either direction makes the set unstable).
- `MultilineGridDistinct` gives every cell its own text and keeps the Tree's short names, so its frame measures the
  grid. `MultilineGrid` gives the Tree long names ending with a color emoji, whose WARP draw costs about 7 ms of its
  10 ms frame ([frame-cost investigation](../frame-cost/README.md)). `Default` shows single-line grid captions.
- Medians of five 40-frame rounds; MB are 10^6 bytes of private memory.

## MultilineGridDistinct (`dxui-complex-ui-multiline-grid-distinct-v1`)

| Metric | Set | A1 | B1 | B2 | A2 | B1 vs A1 | B2 vs A2 | A2 vs A1 | B2 vs B1 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Dirty FPS | 1 | 337.3 | 380.4 | 374.7 | 407.5 | +12.8% | -8.0% | +20.8% | -1.5% |
| Dirty FPS | 2 | 386.7 | 391.2 | 432.1 | 399.9 | +1.2% | +8.0% | +3.4% | +10.4% |
| Dirty p50 (ms) | 1 | 2.83 | 2.58 | 2.55 | 2.41 | -8.7% | +5.7% | -14.7% | -1.3% |
| Dirty p50 (ms) | 2 | 2.54 | 2.47 | 2.23 | 2.46 | -2.4% | -9.2% | -3.0% | -9.7% |
| Dirty private (MB) | 1 | 28.64 | 28.88 | 28.50 | 28.30 | +0.8% | +0.7% | -1.2% | -1.3% |
| Dirty private (MB) | 2 | 28.39 | 29.34 | 28.91 | 28.38 | +3.3% | +1.8% | -0.0% | -1.5% |
| Clean private (MB) | 1 | 27.25 | 27.11 | 27.71 | 27.46 | -0.5% | +0.9% | +0.8% | +2.2% |
| Clean private (MB) | 2 | 27.22 | 26.89 | 27.23 | 27.21 | -1.2% | +0.1% | -0.0% | +1.3% |

Dirty rounds allocate 1,080 C++ allocations every round in B, against 1,093 / 1,080 / 1,111 / 1,080 / 1,080 in A; clean
rounds allocate nothing in both.

## MultilineGrid (`dxui-complex-ui-multiline-grid-v1`)

| Metric | Set | A1 | B1 | B2 | A2 | B1 vs A1 | B2 vs A2 | A2 vs A1 | B2 vs B1 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Dirty FPS | 1 | 108.2 | 105.6 | 93.0 | 106.6 | -2.4% | -12.8% | -1.5% | -11.9% |
| Dirty FPS | 2 | 93.9 | 110.9 | 107.9 | 108.3 | +18.1% | -0.4% | +15.3% | -2.8% |
| Dirty p50 (ms) | 1 | 9.09 | 9.17 | 10.63 | 9.14 | +0.9% | +16.2% | +0.5% | +15.9% |
| Dirty p50 (ms) | 2 | 10.48 | 8.85 | 9.18 | 9.04 | -15.5% | +1.5% | -13.7% | +3.7% |
| Dirty private (MB) | 1 | 31.97 | 32.15 | 32.12 | 32.38 | +0.6% | -0.8% | +1.3% | -0.1% |
| Dirty private (MB) | 2 | 31.84 | 31.81 | 31.80 | 32.00 | -0.1% | -0.6% | +0.5% | -0.0% |
| Clean private (MB) | 1 | 30.69 | 30.73 | 30.47 | 30.67 | +0.1% | -0.7% | -0.1% | -0.9% |
| Clean private (MB) | 2 | 30.10 | 30.89 | 31.19 | 30.32 | +2.6% | +2.9% | +0.7% | +1.0% |

Dirty rounds allocate 1,080 every round in B, against 1,116 / 1,080 / 1,080 / 1,080 / 1,080 in A; clean rounds
allocate nothing in both.

## Default (`dxui-complex-ui-v2`)

| Metric | Set | A1 | B1 | B2 | A2 | B1 vs A1 | B2 vs A2 | A2 vs A1 | B2 vs B1 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Dirty FPS | 1 | 426.1 | 447.7 | 441.5 | 404.6 | +5.1% | +9.1% | -5.0% | -1.4% |
| Dirty FPS | 2 | 406.5 | 463.2 | 471.2 | 411.6 | +13.9% | +14.5% | +1.2% | +1.7% |
| Dirty p50 (ms) | 1 | 2.28 | 2.21 | 2.17 | 2.45 | -3.0% | -11.2% | +7.3% | -1.7% |
| Dirty p50 (ms) | 2 | 2.43 | 2.11 | 2.03 | 2.34 | -13.2% | -13.2% | -4.0% | -3.9% |
| Dirty private (MB) | 1 | 26.28 | 26.60 | 27.21 | 26.26 | +1.2% | +3.6% | -0.1% | +2.3% |
| Dirty private (MB) | 2 | 26.89 | 26.49 | 27.18 | 25.98 | -1.5% | +4.6% | -3.4% | +2.6% |
| Clean private (MB) | 1 | 24.61 | 26.03 | 25.81 | 25.24 | +5.8% | +2.3% | +2.5% | -0.8% |
| Clean private (MB) | 2 | 25.21 | 25.11 | 24.99 | 24.92 | -0.4% | +0.3% | -1.2% | -0.5% |

Every run allocates 2,160 C++ allocations per dirty round (one round 2,145), identically in A and B; clean rounds
allocate nothing.

## Verdict

- **Deterministic budgets.** Dirty rounds make the same or fewer C++ allocations in every scene and set (B 1,080 per
  round in both multiline scenes, where A reaches 1,111 and 1,116), clean rounds make none, and surface bytes,
  replacement peaks, composition allocations and hidden work are identical in all 24 receipts.
- **Controls.** By the runner's two-sided rule all 12 same-binary controls are unstable: an unchanged binary moved by
  more than 5% in a timing metric or 2% in memory between its two runs. The comparator's one-sided status is
  `advice-required` for 11 of the 12 crossings and for 10 of the 12 controls. These sets therefore establish neither
  a timing change nor a memory change; what follows is direction, not proof.
- **Timing.** `Default`: all four crossings raise the dirty rate (+5.1%, +9.1%, +13.9%, +14.5%). In set 2, whose
  dirty-rate controls stay within the band (+1.2%, +1.7%), dirty p50 is 13.2% lower in both crossings. That is what
  the change predicts: an unchanged single-line caption is no longer laid out again on each paint (a test asserts a
  repaint creates no layout). `MultilineGrid`: one run per set is the outlier (B2 in set 1, 11.9% below B1; A1 in
  set 2, which A2 exceeds by 15.3%), and the crossings without it are -2.4% and -0.4%, within the band.
  `MultilineGridDistinct`: a dirty-rate control drifts 20.8% in set 1 and 10.4% in set 2, and the crossings range
  from -8.0% to +12.8%, three of four higher. A quiet-fixture repeat decides the multiline scenes.
- **Memory.** Averaged over its four crossings, B's median private bytes differ from A's by +0.48 MB
  (`MultilineGridDistinct` dirty), +0.52 and +0.49 MB (`Default` dirty and clean), +0.37 MB (`MultilineGrid` clean),
  and -0.05 and -0.08 MB (`MultilineGridDistinct` clean, `MultilineGrid` dirty). Single crossings range from -0.40 to
  +1.42 MB, while an unchanged binary drifts by up to 0.91 MB (-3.4%, `Default` set 2, A2 against A1). About half a
  megabyte is what the design retains: the tables keep the layouts of the cells in view and of the previous paint (so
  scrolling never evicts a row still in view), and single-line captions now keep theirs. The
  [performance contract](../../../../Specs/Core/Core_PerformanceAndResources.md#i26-accepted-multiline-grid-memory-tradeoff)
  records the developer's priority and what remains to decide.
