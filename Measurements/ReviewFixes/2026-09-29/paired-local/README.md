# Local paired review-fix benchmark, 29 September 2026

One serial A1/B1/B2/A2 set per scenario (the `performance-paired.ps1` order), run by hand because the candidate was an
uncommitted working tree on `40c6c21`, which the runner refuses as the same commit. It is the performance evidence for
the [review fixes](../../../../Specs/Plans/Done/ReviewFixes_2026-09-29.md).

- A is `40c6c21` in a detached worktree whose library is unchanged; only test files differ (the new regression tests,
  with assertions made non-fatal for the falsification run). B is the review-fix working tree. Both receipts say
  `sourceDirty: true` for those reasons, and `buildSkipped: true` because each tree was built immediately before.
- Both trees use byte-identical benchmark inputs: every receipt has benchmark fingerprint `2DB9197762…`, the same as
  the retained hosted sets. Executables: A `F1B08333491E…`, B `8905FA70AAA3…`.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, WARP 10.0.26100.9278, compiler 195136257, x64 Release, 96 DPI.
  A developer laptop on AC power, not a controlled quiet fixture.

## MultilineGrid medians (`dxui-complex-ui-multiline-grid-v1`)

| Metric | A1 | B1 | B2 | A2 | B1 − A1 | B2 − A2 |
|---|---:|---:|---:|---:|---:|---:|
| Clean private bytes (MB) | 35.73 | 30.73 | 28.96 | 35.88 | −5.00 | −6.92 |
| Clean private peak (MB) | 38.59 | 30.76 | 29.02 | 36.91 | −7.83 | −7.89 |
| Dirty private bytes (MB) | 37.87 | 31.93 | 32.80 | 36.75 | −5.94 | −3.95 |
| Dirty private peak (MB) | 41.71 | 31.95 | 32.83 | 42.43 | −9.76 | −9.60 |
| Dirty FPS | 95.8 | 108.2 | 93.5 | 84.5 | +12.9% | +10.7% |

All runs keep 0 clean and 1,080 dirty-round C++ allocations. The memory reduction repeats in both crossings and in
clean and dirty rounds, and it is larger than either same-binary control's drift (A2/A1 −2.96% dirty private, B2/B1
+2.75%). It comes from drawing multiline cells without `D2D1_DRAW_TEXT_OPTIONS_CLIP` (see the slot-hash experiment:
restoring only the old slot hash leaves the same 32.06 MB dirty median).

## Default medians (`dxui-complex-ui-v2`)

Every run keeps 0 clean and 2,160 dirty-round C++ allocations. Private bytes stay within the 2% band (clean A 24.72 /
24.94 MB, B 25.11 / 25.17 MB; dirty A 25.83 / 26.63 MB, B 26.15 / 26.27 MB).

## Timing verdict

Both same-source controls are unstable in both scenarios (for example Default B2/B1 dirty FPS +47.7%, MultilineGrid
A2/A1 dirty p95 +29%), so no timing crossing here is evidence, favorable or not. The review's changes do not touch the
clean-frame path (preparation returns early; composition is unchanged). A quiet-fixture repeat decides timing.

`SHA256SUMS` covers every file here except itself and this README.
