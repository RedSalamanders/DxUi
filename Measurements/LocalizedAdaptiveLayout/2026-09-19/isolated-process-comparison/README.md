# Independent-process memory investigation

The original action-flow baseline and all flagged candidates remain in their adjacent folders.
This investigation uses an isolated clean worktree at `f7fe1e4`, built separately with the same
compiler/configuration and common `dxui-complex-ui-v2` fixture. It does not replace the original
baseline or change the 2% process-memory / 5% timing investigation bands.

`baseline-1` through `baseline-5` each start a fresh Release benchmark process, run serially with
no competing build, and retain all five clean/dirty rounds plus fixture, source, executable and
hardware identities. The worktree is `C:/Users/eric/.codex/worktrees/layout-performance-baseline/DxUi`.
`candidate-1` through `candidate-5` use the final Release build including disclosure and the short
viewport fixture. They ran serially after the full Release suites, before the next consumer build.
Every original-baseline comparison is retained and flagged; no favorable sample was selected.
The independent baseline has the same compiled-source fingerprint as the original baseline.
Both implementations have the same compiler and common benchmark fixture fingerprint.

Medians of the five process medians (each process retains five raw rounds):

| Scenario / metric | Baseline | Candidate |
| --- | ---: | ---: |
| Clean completed offscreen FPS | 2620.2704 | 2670.1557 |
| Dirty completed offscreen FPS | 550.8451 | 562.9450 |
| Clean frame p95, ms | 0.4534 | 0.4770 |
| Dirty frame p95, ms | 2.1520 | 2.0047 |
| Dirty preparation p95, ms | 1.3995 | 1.3582 |
| Dirty CPU composition p95, ms | 0.0193 | 0.0220 |
| Clean private bytes | 27,877,376 | 27,451,392 |
| Dirty private bytes | 29,089,792 | 29,278,208 |
| Clean working-set bytes | 41,172,992 | 40,869,888 |
| Dirty working-set bytes | 42,786,816 | 42,491,904 |
| Dirty C++ allocations per 40 frames | 2160 | 2160 |

The original single-process memory flag is not consistently reproduced: unchanged baseline dirty
private-byte medians span 28,921,856–30,588,928; candidate medians span 28,868,608–30,883,840.
The process-median difference is +0.65%. Clean p95 (+5.2%) and dirty CPU composition p95 (+14.0%,
2.7 microseconds) still need investigation; their process ranges overlap, while completed throughput
improves. This is **not** a performance acceptance or threshold waiver. All original failures stay
visible and the performance gate remains open pending a controlled paired timing investigation.

Restore/build logs and per-process logs are retained under
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-isolated-baseline-*`.
