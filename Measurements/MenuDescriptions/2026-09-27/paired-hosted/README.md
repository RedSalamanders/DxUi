# Hosted paired described-menu benchmark, 27 September 2026

`performance-paired.ps1 -BaselineRevision e47c836 -CandidateRevision 6f769ab -Scenario Default` ran
one serial A1/B1/B2/A2 set on a single hosted x64 runner: the `paired-benchmark` job of workflow run
[36345251201](https://github.com/RedSalamanders/DxUi/actions/runs/36345251201). This is the hosted
measurement in step one of the
[memory optimization plan](../../../../Specs/Plans/Done/MenuDescriptionMemory_2026-09-27.md).

- A is `e47c836`, main after #27 and before #24. B is `6f769ab`, #24 merged. Their library sources
  differ only by #24's change.
- Both worktrees use the harness from clean `cb4cdde`: `performance.ps1` and its five benchmark
  inputs. Every receipt has benchmark fingerprint `2DB9197762…`, and both sides are marked dirty
  only because of those copies.
- Runner `runnervm99s1a`: AMD EPYC 9V74, Windows 10.0.26100, WARP 10.0.26100.33438, compiler
  195136257, x64 Release.

## Default medians

| Metric | A1 | B1 | B2 | A2 | B1 − A1 | B2 − A2 |
|---|---:|---:|---:|---:|---:|---:|
| Clean private bytes | 23,003,136 | 22,888,448 | 22,691,840 | 22,851,584 | -114,688 | -159,744 |
| Clean working set | 37,752,832 | 37,384,192 | 36,306,944 | 36,651,008 | -368,640 | -344,064 |
| Dirty private bytes | 23,498,752 | 23,355,392 | 23,220,224 | 23,326,720 | -143,360 | -106,496 |
| Dirty FPS | 206.38 | 207.43 | 203.22 | 206.75 | +0.5% | -1.7% |

All runs keep 0 clean and 2,160 dirty-round C++ allocations and 3,686,400 surface bytes.

No memory metric flags in either crossing. Clean private memory moves by -0.50% and -0.70%, which
does not reproduce the waived September 21 increase of 3.2% to 4.6%. The crossings flag clean frame
p95 (+20.76%) and dirty timing items, but both same-source controls flag timing too (A2/A1 dirty
composition CPU p95 +15.45%; B2/B1 dirty frame p95 +7.66%), so they are runner noise.

## Limitations

This is one set on a shared hosted VM, not a quiet desktop. The September 21 pairs used a different
machine, baseline (`78b3de3`) and pre-review candidate. The waiver therefore stays until the
optimization plan's quiet local sets are measured. `summary.receipt.txt` is the runner's
`summary.json`. `SHA256SUMS` covers every file here except itself and this README.
