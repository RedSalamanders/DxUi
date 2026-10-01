# Grid selection membership, 1 October 2026

`GridSelectionModel::IsSelected` scanned the ordered selection, and a `Grid` asks it once for every visible row on every
paint, so after Ctrl+A on a long list each repaint searched the selection linearly 24 times. The model now also keeps its ids
ascending and answers with a binary search that allocates nothing. This is the paired record of that change: `main`
against the sorted copy, x64 Release, offscreen on WARP, ten runs per side (five interleaved A, B, B, A passes) and a
second set of the same two executables that repeats it.

The opt-in `DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json>` (fixture `dxui-grid-selection-v1`,
[`GridSelectionBenchmark.h`](../../../Tests/Embedded/GridSelectionBenchmark.h)) measured both builds. It is synthetic and
library-owned: no consumer repository, plugin, settings or service, and only the public `Grid` and `GridSelectionModel`
interfaces, so one source measures any revision. Its report has five parts:

- **paint**: a Grid of 1,000 to 1,000,000 rows, selected as Ctrl+A selects it (also none, and three rows), scrolled to the
  top, middle or end and painted in five rounds of 60 dirty frames: the time and the UI thread's cycles of `Prepare`, the
  time of the whole frame (clear, composite and a blocking one-pixel readback) and the C++ allocations of each round. A
  `ScrambledRowsModel` gives row *i* the stable id `i` times an odd 32-bit constant, so the selection's order is not
  its ascending order. 24 rows are visible (672 DIP of 28 DIP rows).
- **selectionCost**: what the longer selection costs inside a paint, with no other run in between. One Grid, scrolled
  to the middle, is painted in blocks of 30 frames that alternate between two selections that draw alike (every
  visible row is selected in both) and differ only in size: the 81 rows around the view, and every row. The difference of
  the two medians (the first five frames after each change are dropped) is the cost, in ten rounds per run.
- **membership**: `IsSelected` per call over selections of 0 to 1,000,000 ascending ids, for a batch of 32 ids from the
  middle that are selected (hit) and 32 above them that are not (miss).
- **mutators**: `SetRange` over every row (the first call on a new model, cold, and later calls, warm), `PreserveOrdered`
  over every row and over half of them, and a `Toggle` on and off, at 1,000, 20,000 and 200,000 rows with ascending
  and scattered ids, with the C++ heap calls and bytes of each, which a counting `operator new` records.
- **complexUiScene**: how many rows the default complex-UI scene's Grid holds selected after the benchmark's warm-up and
  a round of dirty frames.

## Source, machine and method

- A is `main` (`726b43d`) with the harness: a `git archive` of `c8cc6e2` whose `src` and `include` are those of
  `726b43d`, built in its own output root. Library source fingerprint `E707BF59AE…` (identical to a checkout of main), executable
  `4B76987FFD…`. The harness is the files of `1db3127`, which the change that follows it leaves as they are.
- B is `c8cc6e2` (the sorted copy), library source fingerprint `95F759F3F7…`, executable `50C6985C61…`, built by `build.ps1
  -Configuration Release -Platform x64` after a clean rebuild of the library. Its tree held uncommitted documentation and
  this packet, so the receipts say `sourceDirty: true` and list the paths; the fingerprints, which cover the library inputs
  only, identify what was measured. Every receipt shares harness fingerprint `B7C3325248…` (`benchmarkInputs`: the harness, `EmbeddedTests.cpp`,
  `ComplexUiScene.h` and `GraphicsFixture.h`). The harness is not one of `performance.ps1`'s benchmark inputs and its entry
  is dispatched from `RunFunctionalTests`, so the complex-UI fixture's hash is unchanged.
- Machine: AMD Ryzen AI 7 PRO 350 (8 cores, 16 threads), 55.6 GB, Windows 10.0.26200, WARP 10.0.26100.9278, MSVC
  compiler 195136257, x64 Release (`/O2`), 96 DPI, Balanced power plan, on AC. It is a developer laptop shared with other
  agent sessions building and testing and with desktop applications, not a quiet fixture: total processor utility was 8%
  to 50% before each run, 23% to 48% in the first set (receipts: `quietBeforeStart`, `processorUtilityPercentAfterRun`), and
  each run was started at above-normal priority so that compiles beside it preempt its UI thread less. Both sides were
  run the same way.
- Runs went A1, B1, B2, A2, A3, B3, B4, A4 and so on to A10, serially (`Run-Paired.ps1.txt`, which calls the repository's
  `Get-PairedRunSchedule` and `Get-MetricVerdict`). A run's value is the median of its five rounds' medians for a paint
  metric, of ten rounds' differences for the cost, of seven timed passes for a per-call time and of nine repeats for a
  mutator. The verdict compares A's ten run values with B's by the exact two-sided Mann-Whitney U test with the 5%
  investigation band, as in the [performance contract](../../../Specs/Core/Core_PerformanceAndResources.md#paired-sets);
  exact budgets (heap calls and bytes) allow no growth. Ten against ten reaches p = 0.000011. The repeat set (`repeat-*`)
  was run about two minutes after the first set ended, with the same two executables.
- Files: 40 receipts (`A1`..`A10`, `B1`..`B10`, and the same with the prefix `repeat-`), the two summaries as
  `summary.receipt.txt` and `repeat-summary.receipt.txt` (every `.json` here is read as a receipt), the driver and the two
  table generators (every table of a summary, and the compact ones below) as `Run-Paired.ps1.txt`, `Format-Packet.ps1.txt`
  and `Readme-Tables.ps1.txt`, and `SHA256SUMS`. The summaries hold every run value and verdict, including the rows below
  that are left out.

## What a paint costs

Prepare p50 is the median Prepare time of a dirty paint; Mcycles are the UI thread's own cycles in it (millions), to which
the time other processes hold the processor does not add (their use of shared caches and cores still does). Frame p50
includes the clear, the composite and the readback. First set:

| Scenario | Rows | Selected | Prepare p50 A (ms) | Prepare p50 B (ms) | Change | p | Verdict | Mcycles A | Mcycles B | Frame p50 A (ms) | Frame p50 B (ms) | Frame verdict |
|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---:|---|
| all-1k-middle | 1,000 | 1,000 | 0.409 | 0.408 | -0.2% | 1.000 | within-noise | 0.814 | 0.815 | 1.150 | 1.171 | within-noise |
| all-20k-top | 20,000 | 20,000 | 0.400 | 0.366 | -8.7% | 0.256 | within-noise | 0.800 | 0.730 | 1.179 | 1.150 | within-noise |
| all-20k-middle | 20,000 | 20,000 | 0.424 | 0.400 | -5.6% | 0.035 | improved | 0.845 | 0.799 | 1.146 | 1.131 | within-noise |
| all-20k-end | 20,000 | 20,000 | 0.439 | 0.406 | -7.5% | 0.011 | improved | 0.874 | 0.811 | 1.151 | 1.125 | within-noise |
| all-200k-middle | 200,000 | 200,000 | 0.583 | 0.423 | -27.5% | <0.001 | improved | 1.165 | 0.845 | 1.295 | 1.153 | improved |
| all-1m-middle | 1,000,000 | 1,000,000 | 1.590 | 0.485 | -69.5% | <0.001 | improved | 3.176 | 0.970 | 2.427 | 1.227 | improved |
| none-20k-middle | 20,000 | 0 | 0.591 | 0.568 | -3.9% | 0.005 | within-noise | 1.181 | 1.135 | 1.314 | 1.279 | within-noise |
| few-1k-top | 1,000 | 3 | 0.567 | 0.552 | -2.7% | 0.035 | within-noise | 1.130 | 1.101 | 1.347 | 1.335 | within-noise |

The repeat set agrees where the change is large and not where it is small: 200,000 rows -29.9% (p = 0.001) and 1,000,000
rows -72.9% (p < 0.001), but 20,000 rows at the top, middle and end +1.9%, -1.0% and +7.4% (p = 0.42 to 0.85), and the controls
(`none-20k-middle` +7.3%, `few-1k-top` +40.5% with a spread of 65% / 43%) within noise. A paint costs B the same at every size,
0.37 to 0.49 ms of Prepare from 1,000 to 1,000,000 selected rows. Where nothing differs between the builds
(`all-1k-middle`, `none-20k-middle`, `few-1k-top`) B reads 0.2% to 3.9% faster in the first set, so the two binaries differ
by a few percent on their own, and the 20,000-row rows, 5.6% and 7.5%, are within that floor's reach. What a Prepare spends on
`IsSelected` is measured by the cost below. C++ allocations per round of 60 paints are 11,820 for 438,720 bytes in every
scenario on both sides: the selection adds no allocation to a paint.

## What the longer selection costs inside a paint

| Rows | Selected in the full state | A cost (ms) | B cost (ms) | A cost (Mcycles) | B cost (Mcycles) | A share of its Prepare | p | Verdict |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 20,000 | 20,000 | 0.0270 | 0.0006 | 0.050 | -0.001 | 4.1% | <0.001 | improved |
| 200,000 | 200,000 | 0.2817 | -0.0046 | 0.568 | -0.014 | 31.8% | <0.001 | improved |
| 1,000,000 | 1,000,000 | 1.4007 | 0.0077 | 2.770 | 0.015 | 69.1% | <0.001 | improved |

The repeat gives A 0.0299, 0.2222 and 1.2578 ms and B 0.0027, -0.0062 and -0.0123 ms (all p < 0.001). A's share is its cost
over the median Prepare of the full-selection state in the same runs. The cost is the saving that the paint rows above
show at 200,000 and 1,000,000 rows and cannot resolve at 20,000 rows, where it is 4% to 6% of a Prepare: it matches 24
calls of 0.93 to 1.44 us (22 to 35 us), the per-call times below. At 200,000 and 1,000,000 rows every A run costs more than
every B run; at 20,000 rows the two sets give p < 0.001 without separating every run. A negative cost is B painting the
full state a little faster than the small one, noise of the order of 0.01 ms.

## IsSelected per call

| Selected ids | A hit (ns) | B hit (ns) | A miss (ns) | B miss (ns) | Miss: change | p | Verdict |
|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | - | - | 4.7 | 2.3 | -51.1% | <0.001 | improved |
| 1 | 5.1 | 2.3 | 4.2 | 2.8 | -34.3% | <0.001 | improved |
| 2 | 4.4 | 3.3 | 4.6 | 2.6 | -43.6% | <0.001 | improved |
| 3 | 4.7 | 2.9 | 4.4 | 3.3 | -24.1% | <0.001 | improved |
| 4 | 3.8 | 3.0 | 3.8 | 3.4 | -10.7% | 0.089 | within-noise |
| 8 | 4.1 | 4.1 | 4.0 | 4.7 | +18.0% | 0.029 | regressed |
| 16 | 4.6 | 4.9 | 4.9 | 5.8 | +18.8% | 0.023 | regressed |
| 32 | 5.0 | 5.6 | 6.2 | 6.3 | +2.4% | 0.912 | within-noise |
| 64 | 6.7 | 5.2 | 8.4 | 7.1 | -15.5% | 0.029 | improved |
| 256 | 17.5 | 7.4 | 21.7 | 8.8 | -59.4% | <0.001 | improved |
| 1,000 | 57.0 | 8.7 | 84.8 | 8.9 | -89.6% | <0.001 | improved |
| 20,000 | 928.7 | 11.1 | 1,441.1 | 11.7 | -99.2% | <0.001 | improved |
| 200,000 | 9,170.9 | 12.9 | 15,922.8 | 13.7 | -99.9% | <0.001 | improved |
| 1,000,000 | 47,495.7 | 16.0 | 83,209.6 | 16.6 | -100.0% | <0.001 | improved |

A scan costs a hit about half the selection and a miss all of it; the search costs 2 to 17 ns at every size. The repeat
gives 617 ns and 1,154 ns at 20,000 ids and 6.6 and 13.8 us at 200,000 for A, 9.9 and 11.5 ns and 12.7 and 13.5 ns for B.

**Small selections.** The default complex-UI scene's Grid (1,000 rows, 12 visible) holds no row selected (`complexUiScene`
in every receipt: 0 of 1,000), so each paint of it asks `IsSelected` 12 times of an empty selection: 4.7 ns before and 2.3 ns
after. Nothing in that scene can regress, and no timing or memory band can see the 24 bytes (the model grows from 40 to 64
bytes) of its one Grid. For selections of 1 to 64 ids B is faster at 1 to 3 and at 64, equal at 4 and 32, and slower at 8 and 16
by 0.7 and 0.9 ns for a miss (p = 0.029 and 0.023): the verdict rule flags those two rows. In the repeat B is 1.1 to 2.2 ns slower
per hit at 8 to 64 ids (A 2.3 to 3.7 ns, B 4.1 to 4.9 ns). A paint asks 24 times, so that is at most about 50 ns of a Prepare of
400,000 ns, which no frame-level band can see. A short linear scan below about 64 ids would remove most of it at the price of a
branch; it is left to the developer's advice.

## Mutators and memory

| Operation | Rows | Ids | A (ms) | B (ms) | Change | p | A heap (calls, bytes) | B heap (calls, bytes) |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| SetRange all, cold | 20,000 | ascending | 0.0197 | 0.0165 | -16.5% | 0.315 | 1, 160,039 | 2, 320,078 |
| SetRange all, cold | 20,000 | scattered | 0.0387 | 1.0215 | +2542.8% | <0.001 | 1, 160,039 | 2, 320,078 |
| SetRange all, warm | 20,000 | ascending | 0.0037 | 0.0077 | +109.6% | <0.001 | 0, 0 | 0, 0 |
| SetRange all, warm | 20,000 | scattered | 0.0034 | 0.9177 | +26889.7% | <0.001 | 0, 0 | 0, 0 |
| PreserveOrdered all kept | 20,000 | ascending | 1.5709 | 0.6543 | -58.4% | <0.001 | 20,010, 1,521,833 | 1, 160,039 |
| PreserveOrdered all kept | 20,000 | scattered | 1.7168 | 1.3684 | -20.3% | <0.001 | 20,010, 1,521,833 | 1, 160,039 |
| PreserveOrdered half kept | 20,000 | ascending | 1.3833 | 0.3486 | -74.8% | <0.001 | 20,010, 1,521,833 | 2, 160,078 |
| PreserveOrdered half kept | 20,000 | scattered | 1.6711 | 1.1064 | -33.8% | <0.001 | 20,010, 1,521,833 | 2, 160,078 |
| Toggle one row on and off | 20,000 | ascending | 0.00373 | 0.00352 | -5.6% | 0.739 | - | - |
| Toggle one row on and off | 20,000 | scattered | 0.00378 | 0.00400 | +5.9% | 0.247 | - | - |
| SetRange all, cold | 200,000 | ascending | 0.1797 | 0.3911 | +117.7% | <0.001 | 1, 1,600,039 | 2, 3,200,078 |
| SetRange all, cold | 200,000 | scattered | 0.1647 | 10.9454 | +6545.7% | <0.001 | 1, 1,600,039 | 2, 3,200,078 |
| SetRange all, warm | 200,000 | ascending | 0.0446 | 0.1062 | +138.3% | <0.001 | 0, 0 | 0, 0 |
| SetRange all, warm | 200,000 | scattered | 0.0451 | 10.7279 | +23713.3% | <0.001 | 0, 0 | 0, 0 |
| PreserveOrdered all kept | 200,000 | ascending | 29.2855 | 7.3258 | -75.0% | <0.001 | 200,013, 13,181,982 | 1, 1,600,039 |
| PreserveOrdered all kept | 200,000 | scattered | 31.3384 | 20.2367 | -35.4% | <0.001 | 200,013, 13,181,982 | 1, 1,600,039 |
| PreserveOrdered half kept | 200,000 | ascending | 26.9062 | 4.1533 | -84.6% | <0.001 | 200,013, 13,181,982 | 2, 1,600,078 |
| PreserveOrdered half kept | 200,000 | scattered | 29.7387 | 15.5859 | -47.6% | <0.001 | 200,013, 13,181,982 | 2, 1,600,078 |
| Toggle one row on and off | 200,000 | ascending | 0.03888 | 0.02804 | -27.9% | 0.043 | - | - |
| Toggle one row on and off | 200,000 | scattered | 0.03910 | 0.03729 | -4.6% | 0.481 | - | - |

- **Memory.** Each selected row costs 8 more bytes: the first Ctrl+A over 20,000 ids asks for 160,039 bytes of the C++ heap
  on A and 320,078 on B (two vectors; the 39 is the standard library's alignment padding of an allocation of 4 KB or
  more), and over 200,000 ids 1,600,039 and 3,200,078. The model itself grows from 40 to 64 bytes (`selectionModelBytes`).
  Later calls reuse the capacity and allocate nothing on either side. Like the ordered ids, both vectors keep their
  capacity after `Clear` or a smaller selection, so what a large selection leaves behind doubles from 8 to 16 bytes per
  row of its size; that is not measured here. The verdict rule marks the heap bytes of `SetRange` regressed (exact
  budgets allow no growth); this is the accepted cost of the design.
- **Ctrl+A over scattered ids** is the one operation that gets slower: the ascending copy is sorted, 1.0 ms for 20,000
  ids and 10.9 ms for 200,000 where A copied them in 0.04 and 0.16 ms. Ids that already ascend (a model whose stable
  ids grow with its rows) skip the sort and cost the second copy: 0.004 ms more at 20,000 ids and 0.06 ms more (0.2 ms
  the first time, with its allocation) at 200,000. A radix sort of the ids would be several times faster at the price of a
  scratch buffer; it is not done.
- **`PreserveOrdered`** is the call `NotifyDataChanged` makes on every data change of a Grid that has a selection. It no
  longer builds a hash set (one allocation per selected id: 20,010 calls and 1.52 MB for 20,000 ids, 200,013 and 13.2 MB for
  200,000): it asks the ascending copy, builds the result in one vector and, when a data change leaves the selection as it was,
  stops there. It takes 0.65 ms instead of 1.57 for 20,000 ascending ids (1.37 against 1.72 for scattered ones) and 7.3 ms
  instead of 29.3 for 200,000, with 1 heap call instead of 20,010.
- **`Toggle`** is unchanged within noise (3.5 us against 3.7 us for an add and a remove at 20,000 ids): the ascending copy
  is a binary search and one `memmove` of the vector's tail.

## What this establishes and what it does not

- Establishes: `IsSelected` costs 2 to 17 ns from 0 to 1,000,000 selected ids where it cost up to 83 us, every B run below
  every A run from 256 selected ids up in both sets; a Grid's paint no longer grows with its selection (0.37 to 0.49 ms of
  Prepare at every size, against 0.40 to 1.59 ms), 27% to 30% less at 200,000 selected rows and 70% to 73% less at
  1,000,000; the saving inside a paint is 0.03 ms at 20,000 rows, 0.22 to 0.28 ms at 200,000 and 1.3 to 1.4 ms at 1,000,000
  (all p < 0.001 in both sets); paint allocations and `Toggle` are unchanged; `PreserveOrdered` is cheaper in time and far
  cheaper in heap.
- Does not establish: the 5.6% and 7.5% faster paints at 20,000 rows (they do not repeat, and the controls show a few
  percent between the binaries alone); anything about the Tree, which does not use the model yet.
- Limits: WARP offscreen only, no hardware GPU and no presentation; one developer laptop shared with other work, so
  run-to-run spread of a paint metric is 10% to 100% and only a shift well beyond that separates; one compiler; 24 visible
  rows (the saving per paint scales with the visible rows, the paint with them as well); the 32-bit scrambled ids are
  synthetic; x64 only, with no native ARM64 execution; the default complex-UI scene was not timed here (its paired set is
  the acceptance for that fixture) and only shown to hold no selection. An earlier complete set measured with a previous
  version of the harness, which lacked the isolated cost, agreed (200,000 rows -32.6%, 1,000,000 rows -68.5%, the 20,000-row
  rows within noise); its receipts name a commit that was rewritten and are not retained.
