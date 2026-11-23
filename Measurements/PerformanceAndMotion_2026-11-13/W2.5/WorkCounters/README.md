# W2.5 work counters — ComplexUi (FX5) after the Grid data paths

The work-counter receipt of [W0.1](../../W0.1/README.md) taken again on the same fixture after plan item `W2.5`, because the
item changes a count that the W0.1 receipt, `TestFx5MatchesWorkCounterReceipt` and the trace-provider gate pin. The W0.1
receipt stays as the baseline; this one is the current value of `WorkCountersComplexUi::kReleaseDirtyFrameAllocations`
(`Tests/Embedded/Embedded.Tests.WorkCounters.h`), which the three gates share.

- **Scenario**: unchanged from W0.1: `Samples/ComplexUi/ComplexUiScene.h`, 83 controls in an `EmbeddedHost`, 1280×720,
  96 DPI, WARP, offscreen, reduced motion; 20 warm-up frames, 40 clean frames, 40 dirty frames
  (`ComplexUiScene::Update(20 + n)`).
- **Command**: `DxUi.EmbeddedTests.exe --test-work-counters <receipt>`, x64 Release, Debug and ASan Debug.
- **Source**: integration tip `a03782610` (which contains W2.5, `W2.2` and `W2.6`) plus the commit carrying these files. **Compiler**: MSVC 19.52.37025.
  **OS**: Windows 11 build 26300. `workloadOwner`, `baseCommit` and the SHA-256 `benchmarkInputs` were added to each
  executable-written receipt at capture, as in W0.1; the counts are unchanged.
- **Files**: [Release](work-counters-complex-ui-release.json), [Debug](work-counters-complex-ui-debug.json),
  [ASan Debug](work-counters-complex-ui-asan-debug.json).

| Per dirty frame (identical in all 40 frames) | W0.1 baseline | After W2.5 |
| --- | --- | --- |
| C++ allocations, Release | 54 | 2 |
| C++ allocations, Debug and ASan Debug (debug iterators) | 257 | 23 |
| `Paint` executions / paint-walk visits | 83 / 83 | 83 / 83 |
| Resources created / churn | 56 / 56 | 56 / 56 |
| Pixels repainted | 921,600 | 921,600 |

Clean frames are unchanged: 0 allocations, 0 created resources, 0 churn, one composite.

## The 52 allocations removed

The scene's Grid model answers `GetColumn` with `{id, L"Column " + id, 170}`; an eight-character title does not fit the
seven-character small-string buffer, so each call allocated. Before W2.5 the Grid asked `GetColumn` once for each of the 4
headers and once for each of the 4 columns of the 12 visible rows: 4 + 48 = 52 allocations per dirty frame. The W2.5
column-description cache reads the columns once per data revision, so a dirty frame asks none. A stack per allocation of a
dirty frame (a temporary tracer on the allocation sink, not retained) shows the remaining 2 in Release: the vector of
visible body items the Grid's paint builds, and the model's own formatting of the changed cell text (`GetCellData`).
`W2.1`, `W4.2` and `W9.4` own those.

## What the pinned number is

One dirty frame: after 20 warm-up and 40 clean frames, the allocation sink's count from `ComplexUiScene::Update(20 + n)`
through `Prepare` and `Composite`, the counter scope built before counting starts. `TestFx5MatchesWorkCounterReceipt`
counts the same window per FX5 dirty step (the step, then the fixture frame, which also flushes), so a fixture receipt's
dirty series and this receipt's dirty frame are the same number, and the gates share one constant. The pin is Release
only: Debug and ASan Debug count the debug iterators' container proxies (23 per dirty frame here, 257 at W0.1).
On `a03782610` with this change, five consecutive Release runs of `--test-work-counters`, `--test-performance-fixtures`
and `--fixture-receipt FX5 Warp` each gave 2 per dirty frame in every frame (80 per 40, warm-up 75 in total; clean 0),
in the work-counter, counting-wrapper, FX5 framework and trace-provider runs alike. A reported 23 for the FX5 test is the
Debug and ASan Debug value, not a Release one.

The parent `c030d60ce` measured 54 in both `--test-work-counters` and the `FX5` fixture run; W2.5 on top of it (`79b20d439`) measured 2 in both.
The retained [FX5 fixture receipts](../../FX5/README.md) (hardware and WARP, `W0.4`) record the baseline 54 and are left as
history; no test reads them.
