# Hosted paired grid benchmark, 27 September 2026

`performance-paired.ps1` ran one serial A1/B1/B2/A2 set for each of the Default and MultilineGrid
scenarios on a single hosted x64 runner: the `paired-benchmark` job of workflow run
[36344067481](https://github.com/RedSalamanders/DxUi/actions/runs/36344067481), dispatched with
baseline `6f769ab`.

- A is `6f769ab`, main before #22. B is `217e602`, whose library inputs equal main `6b34456` after
  #22 merged. A and B differ only by #22's `DxUi.h` and `DxUi.Grid.cpp` changes.
- Both builds use B's six harness files: `performance.ps1` and its five benchmark inputs. Every
  receipt therefore has benchmark fingerprint `2DB9197762…`. A's receipts are marked dirty only
  because of those copied files.
- Runner `runnervm99s1a`: AMD EPYC 9V45, Windows 10.0.26100, WARP 10.0.26100.33438, compiler
  195136257, x64 Release.

## MultilineGrid medians

| Metric | A1 | B1 | B2 | A2 | B1 − A1 | B2 − A2 |
|---|---:|---:|---:|---:|---:|---:|
| Clean private bytes | 28,393,472 | 30,392,320 | 30,183,424 | 27,447,296 | +1,998,848 | +2,736,128 |
| Dirty private bytes | 29,581,312 | 33,783,808 | 31,723,520 | 28,651,520 | +4,202,496 | +3,072,000 |
| Dirty private peak bytes | 29,753,344 | 37,081,088 | 34,643,968 | 28,889,088 | +7,327,744 | +5,754,880 |
| Dirty FPS | 58.78 | 102.80 | 107.23 | 62.98 | +74.9% | +70.3% |

All runs keep 1,080 dirty-round C++ allocations and 3,686,400 surface bytes. Composition allocates
nothing and hidden work is zero.

The accepted V11 envelope in the
[performance contract](../../../../Specs/Core/Core_PerformanceAndResources.md) records dirty
multiline private memory rising by 7,012,352 bytes, described as roughly 6–7 MiB. Both median
crossings stay below it. The larger dirty peak increase, 7,327,744 bytes (6.99 MiB), is within that
range. Dirty throughput improves by about 70–75%, the same direction as V11. The comparator flags
the B/A memory increases, which are the accepted cost, and some clean timing items. The same-source
A2/A1 control is within its bands; the B2/B1 control flags only dirty composition CPU p95 (+7.43%).

## Default

No memory flag crosses the change: clean private bytes move by -483,328 and +98,304. Timing is
dominated by runner noise. The same-binary A2/A1 control moves clean FPS by -29.8% and clean frame
p95 by +52%, so this set supports no timing conclusion. All four Default comparisons are
`advice-required`; nothing is rebaselined.

## Limitations

This is one set on a shared hosted VM, not a quiet desktop. It measures offscreen WARP frames, not
presentation. The retention and heap-walk fixtures were not run. The result supports applying the
accepted V11 envelope to the current grid revision; it does not set a new budget.

The job step reported exit code 1 after writing every result. GitHub's `pwsh` wrapper re-exited
with the comparator's code for a flagged comparison; `c391ab7` fixes that. `summary.receipt.txt`
is the runner's `summary.json`. `SHA256SUMS` covers every file here except itself and this README.
