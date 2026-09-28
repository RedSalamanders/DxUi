# Hosted paired gallery follow-ups benchmark, 28 September 2026

`performance-paired.ps1 -BaselineRevision 3c882b2 -Scenario Default` ran one serial A1/B1/B2/A2 set on a
single hosted x64 runner: the `paired-benchmark` job of workflow run
[36382031032](https://github.com/RedSalamanders/DxUi/actions/runs/36382031032). It is the performance evidence
for the [gallery follow-ups](../../../../Specs/Plans/WIP/GalleryFollowUps_2026-09-28.md).

- A is `3c882b2`, before the follow-ups. B is `ed1dea9`, with the reduced-motion `ProgressBar` and the
  alert-color fallback. Their library sources differ only by those two changes.
- Both worktrees use `ed1dea9`'s harness, identical to `3c882b2`'s, so no receipt is dirty. Every receipt has
  benchmark fingerprint `2DB9197762…` on the `dxui-complex-ui-v2` fixture.
- Runner `runnervm99s1a`: AMD EPYC 9V74, Windows 10.0.26100, WARP 10.0.26100.33438, compiler 195136257,
  x64 Release.

The fixture uses the built-in dark palette with reduced motion and only determinate progress bars, so neither
change is on its executed paths. The set checks that nothing else moved.

## Default medians

| Metric | A1 | B1 | B2 | A2 | B1 − A1 | B2 − A2 |
|---|---:|---:|---:|---:|---:|---:|
| Clean private bytes | 23,027,712 | 22,745,088 | 22,769,664 | 22,712,320 | -282,624 | +57,344 |
| Clean working set | 37,859,328 | 37,199,872 | 36,462,592 | 36,261,888 | -659,456 | +200,704 |
| Dirty private bytes | 23,646,208 | 23,441,408 | 23,343,104 | 23,162,880 | -204,800 | +180,224 |
| Clean frame p95 (ms) | 3.289 | 2.824 | 2.834 | 2.317 | -14.1% | +22.3% |
| Dirty FPS | 155.14 | 206.95 | 207.70 | 207.02 | +33.4% | +0.3% |

All runs keep 0 clean and 2,160 dirty-round C++ allocations and 3,686,400 surface bytes.

No memory, allocation or surface metric flags in any comparison. B1/A1 and both same-source controls are
within the noise budget. B2/A2 is `advice-required` for two timing items: clean frame p95 +22.3% and dirty
composition CPU p95 +9.26% (0.0108 to 0.0118 ms). The other crossing moves both the other way (-14.1% and
-4.24%), and the same-binary A2/A1 control moves them by -29.6% and -8.47%, with clean FPS +28.6%: A1 was a
slow first run. These flags are runner timing noise, not a regression. Nothing is rebaselined.

## Limitations

This is one set on a shared hosted VM, not a quiet desktop, and it measures offscreen WARP frames rather than
presentation. `summary.receipt.txt` is the runner's `summary.json`. `SHA256SUMS` covers every file here except
itself and this README.
