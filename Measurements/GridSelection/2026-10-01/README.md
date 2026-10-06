# Grid selection membership, 1 October 2026

`GridSelectionModel::IsSelected` scanned the ordered selection, and a `Grid` asks it once for every visible row on every
paint, so after Ctrl+A on a long list each repaint searched the selection linearly 24 times. The model now also keeps its ids
ascending and answers with a binary search that allocates nothing. This is the paired record of that change: `main`
against the sorted copy, x64 Release, offscreen on WARP, ten runs per side (five interleaved A, B, B, A passes) and a
second set of the same two executables that repeats it.

**Update, later on 1 October 2026.** A second set of three builds, in the
[refinement](#refinement-a-scan-for-small-selections-room-given-back-and-preserveordered) at the end of this record,
measured three of its limits again and found a fourth. The small-selection slowdown below was a property of its two
executables and the measure that showed it flatters a binary search, so the fix is a scan up to 1,024 ids, not 64; what a
large selection leaves behind after `Clear` is now measured and given back; and `PreserveOrdered` was slower than `main`
for a modest selection in a long list, which this record's all-or-half selections could not show. The text from here to
the refinement is left as it was recorded, with a note where it is wrong or superseded.

The opt-in `DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json>` (fixture `dxui-grid-selection-v1`,
[`GridSelectionBenchmark.h`](../../../Tests/Embedded/Embedded.Tests.GridSelectionBenchmark.h)) measured both builds. It is synthetic and
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
branch; it is left to the developer's advice. *Update: the refinement below measures this again with builds made alike and
finds no slowdown, and shows why a scan limit of 64 would have been the wrong one.*

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
  budgets allow no growth); this is the accepted cost of the design. *Update: the refinement measures what a selection
  leaves behind (3.2 MB after Ctrl+A over 200,000 ids on B) and gives it back.*
- **Ctrl+A over scattered ids** is the one operation that gets slower: the ascending copy is sorted, 1.0 ms for 20,000
  ids and 10.9 ms for 200,000 where A copied them in 0.04 and 0.16 ms. Ids that already ascend (a model whose stable
  ids grow with its rows) skip the sort and cost the second copy: 0.004 ms more at 20,000 ids and 0.06 ms more (0.2 ms
  the first time, with its allocation) at 200,000. A radix sort of the ids would be several times faster at the price of a
  scratch buffer; it is not done.
- **`PreserveOrdered`** is the call `NotifyDataChanged` makes on every data change of a Grid that has a selection. It no
  longer builds a hash set (one allocation per selected id: 20,010 calls and 1.52 MB for 20,000 ids, 200,013 and 13.2 MB for
  200,000): it asks the ascending copy, builds the result in one vector and, when a data change leaves the selection as it was,
  stops there. It takes 0.65 ms instead of 1.57 for 20,000 ascending ids (1.37 against 1.72 for scattered ones) and 7.3 ms
  instead of 29.3 for 200,000, with 1 heap call instead of 20,010. *Update: this is for selections of all or half of the
  rows, the only ones measured here. A modest selection in a long list (hashed ids, 200,000 rows, 16 to 5,000 ids clicked)
  took B 2 to 4 times as long as A, which the refinement shows and fixes.*
- **`Toggle`** is unchanged within noise (3.5 us against 3.7 us for an add and a remove at 20,000 ids): the ascending copy
  is a binary search and one `memmove` of the vector's tail.

## What this establishes and what it does not

- Establishes: `IsSelected` costs 2 to 17 ns from 0 to 1,000,000 selected ids where it cost up to 83 us, every B run below
  every A run from 256 selected ids up in both sets; a Grid's paint no longer grows with its selection (0.37 to 0.49 ms of
  Prepare at every size, against 0.40 to 1.59 ms), 27% to 30% less at 200,000 selected rows and 70% to 73% less at
  1,000,000; the saving inside a paint is 0.03 ms at 20,000 rows, 0.22 to 0.28 ms at 200,000 and 1.3 to 1.4 ms at 1,000,000
  (all p < 0.001 in both sets); paint allocations and `Toggle` are unchanged; `PreserveOrdered` is cheaper in time and far
  cheaper in heap (for the selections measured here; see the update under `PreserveOrdered` above).
- Does not establish: the 5.6% and 7.5% faster paints at 20,000 rows (they do not repeat, and the controls show a few
  percent between the binaries alone); anything about the Tree, which does not use the model yet.
- Limits: WARP offscreen only, no hardware GPU and no presentation; one developer laptop shared with other work, so
  run-to-run spread of a paint metric is 10% to 100% and only a shift well beyond that separates; one compiler; 24 visible
  rows (the saving per paint scales with the visible rows, the paint with them as well); the 32-bit scrambled ids are
  synthetic, and made by one multiplication whose successive values a branch predictor learns in part, as it learns the
  fixed batch of questions of the per-call measure, so this record's search times are the optimistic ones (the refinement
  measures the others); x64 only, with no native ARM64 execution; the default complex-UI scene was not timed here (its paired set is
  the acceptance for that fixture) and only shown to hold no selection. An earlier complete set measured with a previous
  version of the harness, which lacked the isolated cost, agreed (200,000 rows -32.6%, 1,000,000 rows -68.5%, the 20,000-row
  rows within noise); its receipts name a commit that was rewritten and are not retained.

## Refinement: a scan for small selections, room given back, and `PreserveOrdered`

The first record left three costs flagged: `IsSelected` read 0.7 to 2 ns slower than `main` for selections of 8 to 64 ids,
both vectors kept their capacity through `Clear` (not measured), and `SetRange` sorts ids that do not already ascend. A review
asked for the first two to be refined and the third to be left to the developer. Measuring them showed that this record's
per-call measure flatters a binary search, and that `PreserveOrdered` had a cost it had not seen. Three commits answer that,
on top of two that extend the harness, and a fourth changes comments only:

| Commit | What it does |
|---|---|
| `840f394`, `3f54e48` | The harness: finer sizes, random-order questions, retained heap, `PreserveOrdered` over a long list, a `parts` argument |
| `1b62b58` | `IsSelected` scans a selection of up to 1,024 ids, as `main` does, and searches a larger one |
| `ec6f519` | A buffer with room for more than 4,096 ids is given back when the next selection needs half of it or less |
| `739a526` | `PreserveOrdered` asks a table of bits before it searches |
| `0e4ed69` | Comments only: the figures in the comments of the two limits are those of this record |

`Tests/Embedded/GridSelectionBenchmark.h` is now fixture `dxui-grid-selection-v2`. It still uses only the public
`Grid` and `GridSelectionModel` interfaces, so one source measures all three builds. What it gained:

- **membership** asks every question twice over. *In order* is this record's measure: a batch of 32 ids is asked again and
  again, so the processor learns which way each comparison of a search goes. *Random order* is 65,536 questions in a random
  order, half about selected ids and half about ids that are not, which no processor can learn: it is closer to the ids of the
  rows on a screen asked about a selection of hashed ids. Sizes 384, 512, 768 and 1,500 were added where the two algorithms
  trade places.
- **retention** counts the C++ heap bytes a model holds after Ctrl+A over 1,000 to 200,000 ids and after each way back (`Clear`,
  a click, a Shift+click 99 rows away, a data change that drops every other row). The allocation hook of `EmbeddedTests.cpp`
  keeps a live byte count on request: the sizes of the blocks one thread has allocated less those it has freed, read as a
  difference, so the figure is exact and does not depend on how the operating system's heap returns memory (the process's
  private bytes are recorded beside it and are noisier).
- **preserve** times `PreserveOrdered` over a list of 200,000 rows whose ids are hashes of the rows, with 3 to 5,000 of them
  clicked in an order unlike the rows': the usual data change, which asks one question for every row.
- **mutators** gained `SetRange` over a selection that already is every row, and `Clear` and `SetSingle` on a model that has
  just selected every row. `parts` names the parts to measure, so one question can be asked in seconds.

### Source, machine and method

- **main** is the `src` and `include` of `726b43d` in the tree of `739a526`, a `git archive` built in an output root of its
  own: library source fingerprint `E707BF59AE…` (the first record's A), executable `C3773A4D6A…`. **current** is the same
  with the `src` and `include` of `c8cc6e2`, the sorted copy alone and the first record's B: fingerprint `95F759F3F7…`,
  executable `8262372122…`. **refined** is a `git archive` of `739a526`, a clean tree (`sourceDirty: false`): fingerprint
  `896812D5FD…`, executable `09C269CCBD…`. One script built all three from the harness of `739a526` (`refine-New-Tree.ps1.txt`,
  which builds `Tests/Embedded/DxUi.EmbeddedTests.vcxproj` with the properties `build.ps1` passes: `refine-Build-Project.ps1.txt`),
  so they differ in the library and nothing else. None is an executable of the first record: its A came from another archive and
  its B from the working tree. The commit after `739a526` changes comments in `DxUi.Grid.cpp` and nothing else (library
  fingerprint `67876230F4…`); the receipts name `739a526`, the commit that was measured. Every receipt shares harness
  fingerprint `6CC0420652…`.
- Machine: the first record's, an AMD Ryzen AI 7 PRO 350 laptop (Windows 10.0.26200, WARP 10.0.26100.9278, MSVC 195136257, x64
  Release, Balanced power plan), shared with other agent sessions and desktop applications. Total processor utility after a run
  was 19% to 85%, median 40%; every run started after three consecutive samples under the 60% threshold, at above-normal
  priority. A run takes 20 to 25 s, the set 15 minutes.
- Thirty runs went `main-1`, `current-1`, `refined-1`, `refined-2`, `current-2`, `main-2`, `main-3` and so on to `main-10`:
  forward, then backward, five times, so each side has ten runs and a drift lands on all three alike. A run's value is as in
  the first record (the median of its rounds, passes or repeats). A pair of sides is judged by the repository's exact two-sided
  Mann-Whitney test with the 5% band, ten against ten (p can reach 0.000011), and exact budgets (heap calls and bytes) allow no
  growth. The summary holds three pairs: refined against main, refined against current, and current against main.
- How often identical code is flagged: of the 82 per-call comparisons at up to 1,024 selected ids, where refined runs main's own
  code (`std::ranges::find`), 80 are within noise and two read slower: 4 ids, a hit in order (3.6 to 4.3 ns, +17%, p = 0.011) and
  1,000 ids, a miss in random order (79.4 to 89.8 ns, +13%, p = 0.015). Two in 82 is what a 5% test gives; none reads faster.
- Files: `refine-main-1.json` to `refine-refined-10.json` (30 receipts), `refine-summary.receipt.txt` (every run value and
  verdict of the three pairs; every `.json` here is read as a receipt), `refine-Run-Paired3.ps1.txt` (the driver),
  `refine-Format-Packet3.ps1.txt` (the tables), `refine-New-Tree.ps1.txt` and `refine-Build-Project.ps1.txt` (how the three
  executables were built), `refine-RadixProbe.cpp.txt` and `refine-RadixProbe.output.txt` (the sort probe below) and
  `refine-SHA256SUMS`. The files above are untouched.

### Why the scan limit is 1,024, and what the first record's slowdown was

The first record asked `IsSelected` for the same 32 ids on every pass, so a processor learned which way each comparison of a
binary search went. It found the search cheap everywhere except 0.7 to 2 ns dearer than the scan at 8 to 64 ids. Neither
holds up.

- **The slowdown belonged to the executables.** Built alike (above), `current` is never slower than `main` in that same order:
  of 49 comparisons 31 are faster and 18 within noise, and 8 and 16 ids are among the within-noise ones. A rerun of the first
  record's two executables on a quiet machine (six runs each; not retained, because their receipts name an earlier harness)
  gave +0.3 and +0.45 ns for a miss at 8 and 16 ids and nothing at 32 ids or more. Half a nanosecond between two executables
  is what the placement of code moves.
- **The search is slow when it cannot be predicted.** The ids of the rows on screen, asked about a selection made in row order
  whose ids are hashes, come in no order a processor can learn, and a binary search then mispredicts about every other
  comparison. In random order, of 41 comparisons at up to 1,024 ids, `current` is slower than `main` in 33 (a miss at 16 ids:
  4.4 ns on `main`, 18.6 on `current`; at 64 ids 8.7 and 28.2 ns) and faster in 5; above 1,024 ids it is faster in all 8. That
  is the real small-selection cost of the sorted copy: 5 to 20 ns a call at 8 to 128 ids, where the first record flagged 0.7
  to 2 ns.
- **So the scan stays up to the size where the search catches up.** In random order a scan costs about 0.1 ns an id and a
  search 39 to 50 ns from 256 to 1,500 ids, so they meet near 500 to 650 ids: at 512 ids the scan costs 36.1 ns for a hit and
  39.7 for a miss against 44.2 and 42.9 for the search, at 768 it costs 49.6 and 69.5 against 43.9 and 44.7, and at 1,000
  61.1 and 79.4 against 49.6 and 48.2. The limit is 1,024, the first power of two beyond the meeting point. `refined` scans
  at up to 1,024 ids and searches above, so it is `main`'s code in the first range and `current`'s in the second.

Per call, in nanoseconds, the median of ten runs per side, for an id that is selected (hit) and one that is not (miss). In a
verdict cell `=` is within noise, and `faster` and `slower` are the exact test's verdicts beyond the 5% band (a percentage is
the change of the median). In order, as this record measured it:

| Selected ids | main (hit / miss) | current (hit / miss) | refined (hit / miss) | refined vs main (hit / miss) | refined vs current (hit / miss) |
|---:|---:|---:|---:|---|---|
| 0 | - / 5.2 | - / 2.2 | - / 4.7 | - / -9.5% = | - / +113.2% slower |
| 1 | 4.1 / 4.3 | 2.2 / 2.6 | 5.0 / 4.4 | +20.9% = / +2.2% = | +123.6% slower / +65.5% slower |
| 2 | 4.5 / 4.7 | 3.0 / 2.7 | 3.6 / 4.3 | -21.4% = / -9.5% = | +20.1% = / +60.6% slower |
| 4 | 3.6 / 3.9 | 2.8 / 3.6 | 4.3 / 3.5 | +17.3% slower / -10.5% = | +51.9% slower / -2.9% = |
| 8 | 3.7 / 4.5 | 4.2 / 4.5 | 4.3 / 4.5 | +15.2% = / +1.3% = | +2.3% = / +0.4% = |
| 16 | 4.6 / 5.4 | 4.7 / 5.2 | 3.9 / 4.8 | -14.7% = / -11.8% = | -17.4% = / -7.7% = |
| 32 | 5.1 / 6.8 | 4.7 / 6.0 | 5.8 / 6.6 | +14.9% = / -3.0% = | +23.2% slower / +10.2% = |
| 64 | 6.5 / 8.6 | 6.5 / 7.2 | 6.7 / 8.2 | +2.9% = / -4.7% = | +3.2% = / +14.0% = |
| 128 | 10.4 / 14.4 | 6.9 / 8.2 | 8.0 / 13.7 | -23.2% = / -4.5% = | +16.4% = / +67.0% slower |
| 256 | 16.8 / 22.4 | 7.2 / 8.9 | 17.0 / 22.0 | +0.9% = / -1.9% = | +136.7% slower / +147.7% slower |
| 512 | 32.3 / 40.7 | 9.4 / 10.0 | 33.8 / 40.7 | +4.8% = / -0.1% = | +261.4% slower / +306.8% slower |
| 768 | 45.7 / 66.5 | 9.3 / 10.6 | 45.9 / 63.5 | +0.4% = / -4.5% = | +391.7% slower / +501.5% slower |
| 1,000 | 55.9 / 86.6 | 9.8 / 9.7 | 58.6 / 87.8 | +5.0% = / +1.4% = | +495.6% slower / +809.5% slower |
| 1,500 | 89.8 / 118.3 | 9.8 / 10.1 | 9.2 / 9.2 | -89.7% faster / -92.3% faster | -5.9% = / -9.6% = |
| 20,000 | 918.8 / 1465.1 | 13.3 / 13.1 | 10.7 / 11.9 | -98.8% faster / -99.2% faster | -20.0% = / -9.1% = |
| 200,000 | 9378.4 / 16904.7 | 16.0 / 16.6 | 14.6 / 14.3 | -99.8% faster / -99.9% faster | -8.8% = / -14.0% = |
| 1,000,000 | 49488.5 / 88757.9 | 18.2 / 18.4 | 13.8 / 15.6 | -100.0% faster / -100.0% faster | -24.5% faster / -15.1% faster |

In random order:

| Selected ids | main (hit / miss) | current (hit / miss) | refined (hit / miss) | refined vs main (hit / miss) | refined vs current (hit / miss) |
|---:|---:|---:|---:|---|---|
| 0 | - / 4.5 | - / 1.8 | - / 4.1 | - / -9.0% = | - / +128.9% slower |
| 1 | 4.6 / 4.2 | 1.8 / 2.5 | 3.9 / 4.1 | -14.8% = / -2.7% = | +124.3% slower / +63.4% slower |
| 2 | 3.9 / 4.4 | 6.1 / 5.8 | 4.1 / 4.3 | +6.6% = / -1.4% = | -32.6% faster / -25.5% faster |
| 4 | 3.6 / 3.5 | 8.9 / 9.7 | 3.3 / 3.8 | -7.8% = / +7.3% = | -63.0% faster / -61.1% faster |
| 8 | 8.5 / 4.1 | 13.5 / 14.6 | 8.8 / 4.1 | +3.0% = / +1.5% = | -35.0% faster / -71.9% faster |
| 16 | 10.9 / 4.4 | 18.0 / 18.6 | 11.0 / 4.7 | +1.6% = / +6.2% = | -38.7% faster / -74.7% faster |
| 32 | 13.6 / 5.2 | 23.6 / 23.4 | 13.4 / 5.9 | -1.7% = / +12.5% = | -43.4% faster / -74.9% faster |
| 64 | 15.2 / 8.7 | 29.1 / 28.2 | 15.2 / 8.5 | -0.3% = / -2.6% = | -47.9% faster / -69.9% faster |
| 128 | 18.9 / 13.1 | 32.8 / 31.0 | 18.2 / 12.9 | -3.7% = / -1.2% = | -44.5% faster / -58.3% faster |
| 256 | 24.4 / 22.9 | 38.6 / 40.4 | 24.1 / 22.1 | -1.0% = / -3.8% = | -37.5% faster / -45.4% faster |
| 512 | 36.1 / 39.7 | 44.2 / 42.9 | 36.1 / 39.0 | +0.1% = / -1.7% = | -18.3% faster / -9.1% = |
| 768 | 49.6 / 69.5 | 43.9 / 44.7 | 47.1 / 72.8 | -5.1% = / +4.7% = | +7.3% = / +62.8% slower |
| 1,000 | 61.1 / 79.4 | 49.6 / 48.2 | 61.4 / 89.8 | +0.4% = / +13.0% slower | +23.6% slower / +86.4% slower |
| 1,500 | 85.9 / 118.5 | 50.4 / 49.9 | 48.0 / 48.3 | -44.2% faster / -59.2% faster | -4.8% = / -3.1% = |
| 20,000 | 849.3 / 1419.9 | 72.7 / 72.7 | 65.6 / 66.5 | -92.3% faster / -95.3% faster | -9.7% faster / -8.6% faster |
| 200,000 | 9571.5 / 16825.2 | 107.1 / 107.0 | 97.8 / 95.4 | -99.0% faster / -99.4% faster | -8.6% faster / -10.8% = |
| 1,000,000 | 50050.4 / 94173.0 | 166.3 / 164.2 | 148.6 / 149.3 | -99.7% faster / -99.8% faster | -10.7% = / -9.0% = |

- **Against `main`, up to 1,024 ids: no slower** (the same code). The small sizes asked for, the empty selection and 8, 16,
  32, 64 and 128 ids, read between -23% and +15% in order and between -9% and +13% in random order, never above +0.8 ns (3.7 to
  4.3 ns for a hit at 8 ids, 5.2 to 5.9 ns for a miss at 32 ids in random order), and none is significant.
- **Against `main`, above 1,024 ids: faster**, at 1,500 ids (-90% in order, -44% to -59% in random order) and up to 1,000,000
  (-100%): all 16 comparisons say so. `refined` costs 9 to 16 ns in order and 48 to 149 ns in random order from 1,500 to
  1,000,000 ids, where `main`'s scan costs up to 89 us in order and 94 us in random order.
- **Against `current`:** in random order `refined` is faster at 2 to 512 ids (33 of 41 comparisons at up to 1,024 ids; a miss at
  16 ids 18.6 to 4.7 ns) and slower at 0 and 1 ids (a search of nothing or of one id costs about 2 ns less than a scan: 1.8
  against 4.1 ns for the empty selection) and at 768 to 1,000 ids (a miss at 1,000 ids: 48.2 against 89.8 ns). In order it is
  slower at 0 to 6 ids (2 to 3 ns) and from 96 to 1,000 ids, 23 comparisons in all, where a predicted search costs 7 to 10 ns and
  the scan 10 to 88. That is the price of the margin.
- **The default complex-UI scene** holds no selected row (`selectedRows: 0` of 1,000 in every receipt), so each of its paints
  asks `IsSelected` 12 times of an empty selection: 5.2 ns on `main`, 4.7 on `refined` and 2.2 on `current` in order, 4.5, 4.1
  and 1.8 in random order, which is 62, 56 and 26 ns of a Prepare of hundreds of microseconds. `refined` is no worse than `main`
  and does not keep the 2 to 3 ns that `current` saved; an early return for an empty selection would, and was not added, because
  no band can see 30 ns of a paint.

What each limit would cost, from the medians above (derived from `main`'s and `current`'s columns, not from builds of each): a
model that scans up to N ids costs what `main` costs at those sizes and what `current` costs above them. The sizes between 512
and 768 ids were not measured, so a limit of 512 shows no slowdown against `main` here; but the search is dearer than the scan
just above 512 (a hit at 512 ids: 44.2 against 36.1 ns), so a limit of 512 would put a small slowdown against `main` at about
513 to 650 ids that this set cannot show.

| Scan limit | Worst against main (ratio, ns, where) | Worst against the better of scan and search (ratio, ns, where) |
|---:|---|---|
| 0 (always search) | 4.5x, +18 ns, 32 ids, miss, random order | 4.5x, +18 ns, 32 ids, miss, random order |
| 16 | 4.5x, +18 ns, 32 ids, miss, random order | 4.5x, +18 ns, 32 ids, miss, random order |
| 64 | 2.5x, +17 ns, 96 ids, miss, random order | 2.6x, +3 ns, 1 ids, hit, random order |
| 128 | 1.8x, +15 ns, 192 ids, miss, random order | 2.6x, +3 ns, 1 ids, hit, random order |
| 256 | 1.3x, +10 ns, 384 ids, hit, random order | 2.6x, +3 ns, 1 ids, hit, random order |
| 512 | none (it is main) | 4.1x, +31 ns, 512 ids, miss, predictable |
| 1,024 | none (it is main) | 9.0x, +77 ns, 1,000 ids, miss, predictable |
| 2,048 | none (it is main) | 11.7x, +108 ns, 1,500 ids, miss, predictable |

A limit of 16 or 64, as the review suggested from the first record's 8 to 64 ids, would leave `current`'s random-order cost
above it: up to 18 and 17 ns a call slower than `main`, at 32 and 96 ids. 1,024 is chosen over 512 for its margin over the
meeting point, which moves with the machine (the scan leans on vector instructions and the search on the branch predictor):
500 to 650 ids here, and 600 to 850 in an exploratory run on a quieter machine that is not retained. The cost of the margin is
the difference between the 512 and 1,024 rows above.

### What a selection leaves behind

C++ heap bytes held by one model, counted exactly, after Ctrl+A over a list of the ids' size (selected as the Grid selects
all rows) and then after each way back:

| Ids | After | main | current | refined |
|---:|---|---:|---:|---:|
| 1,000 | Ctrl+A | 8,039 | 16,078 | 16,078 |
| 1,000 | then Clear | 8,039 | 16,078 | 16,078 |
| 1,000 | then a click on a row (SetSingle) | 8,039 | 16,078 | 16,078 |
| 1,000 | then a Shift+click 99 rows away (SetRange) | 8,039 | 16,078 | 16,078 |
| 1,000 | then a data change that drops every other row (PreserveOrdered) | 8,039 | 8,000 | 8,000 |
| 4,096 | Ctrl+A | 32,807 | 65,614 | 65,614 |
| 4,096 | then Clear | 32,807 | 65,614 | 65,614 |
| 4,096 | then a click on a row (SetSingle) | 32,807 | 65,614 | 65,614 |
| 4,096 | then a Shift+click 99 rows away (SetRange) | 32,807 | 65,614 | 65,614 |
| 4,096 | then a data change that drops every other row (PreserveOrdered) | 32,807 | 32,846 | 32,846 |
| 4,097 | Ctrl+A | 32,815 | 65,630 | 65,630 |
| 4,097 | then Clear | 32,815 | 65,630 | 0 |
| 4,097 | then a click on a row (SetSingle) | 32,815 | 65,630 | 16 |
| 4,097 | then a Shift+click 99 rows away (SetRange) | 32,815 | 65,630 | 1,600 |
| 4,097 | then a data change that drops every other row (PreserveOrdered) | 32,815 | 32,862 | 32,862 |
| 20,000 | Ctrl+A | 160,039 | 320,078 | 320,078 |
| 20,000 | then Clear | 160,039 | 320,078 | 0 |
| 20,000 | then a click on a row (SetSingle) | 160,039 | 320,078 | 16 |
| 20,000 | then a Shift+click 99 rows away (SetRange) | 160,039 | 320,078 | 1,600 |
| 20,000 | then a data change that drops every other row (PreserveOrdered) | 160,039 | 160,078 | 160,078 |
| 200,000 | Ctrl+A | 1,600,039 | 3,200,078 | 3,200,078 |
| 200,000 | then Clear | 1,600,039 | 3,200,078 | 0 |
| 200,000 | then a click on a row (SetSingle) | 1,600,039 | 3,200,078 | 16 |
| 200,000 | then a Shift+click 99 rows away (SetRange) | 1,600,039 | 3,200,078 | 1,600 |
| 200,000 | then a data change that drops every other row (PreserveOrdered) | 1,600,039 | 1,600,078 | 1,600,078 |

- **The rule** (`kReleaseIds` in `DxUi.Grid.cpp`): a buffer with room for more than 4,096 ids (32 KiB) is given back when the
  ids that replace its contents need at most half of that room. `Clear` and `SetSingle` swap a wasted buffer with an empty
  vector, which allocates nothing in a Release build (a Debug build's iterator checking gives any new vector a small proxy, as
  it already gave `SetSingle` and `Toggle` their buffers), so both stay `noexcept`. `SetRange` makes both copies for the new
  selection beside the old
  ones and swaps them in, so a failed allocation still leaves the selection as it was. `PreserveOrdered` no longer keeps the
  room it reserved for a selection that shrank to a few ids. A range that needs more than half of the room reuses it, so
  stepping a large range by a row allocates no more than before, and `Toggle`, which adds or removes one id, leaves the room alone.
- **What it leaves:** after Ctrl+A over 20,000 and 200,000 ids and `Clear`, 0 bytes where `current` held 320,078 and 3,200,078 and
  `main` 160,039 and 1,600,039; after a click on a row 16 bytes; after a Shift+click on a row 99 away 1,600. A selection of
  exactly 4,096 ids keeps its room and one of 4,097 gives it back. The process's private bytes (`privateAfterClearBytes` in
  the receipts) show the same at 200,000 ids: 3.2 MB on `current`, 1.6 MB on `main`, 0 on `refined`; at 20,000 ids the heap had
  room to spare and none shows.
- **What it costs** is in the mutators below: `Clear` after Ctrl+A takes 0.3 and 1.3 us at 20,000 ids (ascending and scattered)
  and 0.10 and 0.17 ms at 200,000, a click 0.4 and 2.2 us and 0.11 and 0.18 ms, and the next Ctrl+A allocates again: over 200,000
  ascending ids it takes 0.39 ms where `current`'s reused buffers took 0.12 (over scattered ids 11.0 ms against 10.9, the sort
  dominating; over 20,000 ids no difference the runs can separate).
- **Why it applies to `SetSingle` and `SetRange`:** the way back from Ctrl+A is a click (`SetSingle`) or a Shift+click
  (`SetRange`) far more often than Escape (`Clear`); a rule for `Clear` alone would leave the model that a click produced
  holding 3.2 MB for one id. `PreserveOrdered` reserved `min(rows, selected)` ids, which after a data change that keeps few
  of a large selection is the old size.

### `PreserveOrdered` over a long list

`PreserveOrdered` is the call `NotifyDataChanged` makes on every data change of a Grid that has a selection, and it asks one
question for every row of the model. The first record measured it only with all or half of the rows selected, where the
sorted copy wins. With a modest selection in a long list and ids that are hashes (the usual case), the binary search
mispredicts and `current` took 1.6 to 3.9 times as long as `main`'s hash set from 16 selected ids up. `refined` asks a table of
bits first (16 to 32 bits per selected id, one load per row, letting through one row in 16 to 32 that is not selected for the
search to answer):

| Selected | main | current | refined | refined vs main | refined vs current | heap calls main / current / refined |
|---:|---:|---:|---:|---|---|---|
| 3 | 1.651 | 0.877 | 0.197 | -88.1% better | -77.6% better | 5 / 1 / 2 |
| 16 | 1.198 | 2.380 | 0.326 | -72.8% better | -86.3% better | 19 / 1 / 2 |
| 64 | 2.618 | 4.160 | 0.464 | -82.3% better | -88.9% better | 67 / 2 / 3 |
| 256 | 1.944 | 5.975 | 0.572 | -70.6% better | -90.4% better | 260 / 2 / 3 |
| 512 | 2.442 | 7.129 | 0.679 | -72.2% better | -90.5% better | 516 / 2 / 3 |
| 1,000 | 2.617 | 8.020 | 0.845 | -67.7% better | -89.5% better | 1,005 / 2 / 3 |
| 5,000 | 2.844 | 11.065 | 1.137 | -60.0% better | -89.7% better | 5,008 / 2 / 3 |

Heap: `main` makes one call per selected id (5 to 5,008), `current` 1 or 2 and `refined` 2 or 3: the table is the one call more.

### What else moved

Paint (Prepare p50, ms) and the cost of `IsSelected` inside a paint, which the limit and the filter must leave alone:

| Scenario | main | current | refined | refined vs main | refined vs current | Mcycles main / current / refined |
|---|---:|---:|---:|---|---|---|
| all-1k-middle | 0.544 | 0.558 | 0.594 | +9.1% noise | +6.5% noise | 1.089 / 1.115 / 1.187 |
| all-20k-top | 0.526 | 0.543 | 0.533 | +1.2% noise | -2.0% noise | 1.049 / 1.087 / 1.063 |
| all-20k-middle | 0.604 | 0.579 | 0.571 | -5.5% noise | -1.3% noise | 1.205 / 1.151 / 1.141 |
| all-20k-end | 0.626 | 0.586 | 0.612 | -2.3% noise | +4.3% noise | 1.246 / 1.173 / 1.223 |
| all-200k-middle | 1.048 | 0.741 | 0.812 | -22.5% better | +9.7% noise | 2.089 / 1.470 / 1.624 |
| all-1m-middle | 2.365 | 0.852 | 0.862 | -63.6% better | +1.2% noise | 4.655 / 1.701 / 1.700 |
| none-20k-middle | 0.746 | 0.854 | 0.786 | +5.4% noise | -8.0% noise | 1.480 / 1.704 / 1.572 |
| few-1k-top | 0.732 | 0.752 | 0.767 | +4.8% noise | +2.0% noise | 1.459 / 1.499 / 1.514 |

| Rows | main | current | refined | refined vs main | refined vs current |
|---:|---:|---:|---:|---|---|
| 20,000 | 0.0485 | 0.0054 | 0.0132 | -72.8% better | +143.3% noise |
| 200,000 | 0.3474 | -0.0253 | -0.0071 | -102.0% better | -72.0% noise |
| 1,000,000 | 1.5798 | -0.0211 | -0.0305 | -101.9% better | +44.7% noise |

The 20,000-row paint differences (-5.5% to +1.2%) are inside the noise of the first record's controls; the gains at 200,000 and
1,000,000 selected rows are those of the first record (-22.5% and -63.6% of Prepare against -27.5% to -30% and -70% there), and
`refined` against `current` shows no paint difference at all (64 metrics: 64 within noise). C++ allocations per round of 60
paints are the same on all three sides.

The mutators, in milliseconds with the C++ heap calls and bytes each makes (`better`, `worse` and `noise` are the same
verdicts; `n/a` is a baseline below the table's resolution, 0.00005 ms, which gives no percentage). `Toggle` is unchanged within noise at 20,000 and
200,000 ids (ascending: 3.5 against 3.4 and 2.6 us at 20,000 ids, 37.6 against 39.7 and 37.3 us at 200,000; the rows are below),
and takes 0.26 us against 0.17 at 1,000 scattered ids (+49%, p = 0.002, as on `current`: the second copy's insert and erase):

| Operation | Rows | Ids | main | current | refined | refined vs main | refined vs current | heap main / current / refined (calls, bytes) |
|---|---:|---|---:|---:|---:|---|---|---|
| SetRange all, cold | 20,000 | ascending | 0.0388 | 0.0988 | 0.0785 | +102.5% worse | -20.6% noise | 1, 160,039 / 2, 320,078 / 2, 320,078 |
| SetRange all, warm | 20,000 | ascending | 0.0030 | 0.0083 | 0.0078 | +162.7% worse | -6.6% noise | 0, 0 / 0, 0 / 2, 320,078 |
| SetRange all, over the same selection | 20,000 | ascending | 0.0029 | 0.0083 | 0.0074 | +153.4% worse | -10.9% noise | 0, 0 / 0, 0 / 0, 0 |
| PreserveOrdered all kept | 20,000 | ascending | 1.4547 | 0.6728 | 0.6985 | -52.0% better | +3.8% noise | 20,010, 1,521,833 / 1, 160,039 / 2, 225,614 |
| PreserveOrdered half kept | 20,000 | ascending | 1.3923 | 0.3613 | 0.3789 | -72.8% better | +4.9% noise | 20,010, 1,521,833 / 2, 160,078 / 3, 225,653 |
| Clear after select all | 20,000 | ascending | 0.0000 | 0.0000 | 0.0003 | n/a worse | n/a worse | 0, 0 / 0, 0 / 0, 0 |
| SetSingle after select all | 20,000 | ascending | 0.0000 | 0.0000 | 0.0004 | n/a worse | n/a worse | 0, 0 / 0, 0 / 2, 16 |
| Toggle one row on and off | 20,000 | ascending | 0.00339 | 0.00255 | 0.00352 | +4.1% noise | +38.1% noise | - |
| SetRange all, cold | 20,000 | scattered | 0.0384 | 0.9339 | 0.8286 | +2060.5% worse | -11.3% noise | 1, 160,039 / 2, 320,078 / 2, 320,078 |
| SetRange all, warm | 20,000 | scattered | 0.0038 | 0.8019 | 0.8318 | +22080.0% worse | +3.7% noise | 0, 0 / 0, 0 / 2, 320,078 |
| SetRange all, over the same selection | 20,000 | scattered | 0.0038 | 0.8443 | 0.8245 | +21886.7% worse | -2.3% noise | 0, 0 / 0, 0 / 0, 0 |
| PreserveOrdered all kept | 20,000 | scattered | 1.6137 | 1.3571 | 1.3816 | -14.4% noise | +1.8% noise | 20,010, 1,521,833 / 1, 160,039 / 2, 225,614 |
| PreserveOrdered half kept | 20,000 | scattered | 1.6559 | 1.1197 | 1.0502 | -36.6% better | -6.2% noise | 20,010, 1,521,833 / 2, 160,078 / 3, 225,653 |
| Clear after select all | 20,000 | scattered | 0.0000 | 0.0001 | 0.0013 | n/a worse | +1150.0% worse | 0, 0 / 0, 0 / 0, 0 |
| SetSingle after select all | 20,000 | scattered | 0.0000 | 0.0002 | 0.0022 | n/a worse | +975.0% worse | 0, 0 / 0, 0 / 2, 16 |
| Toggle one row on and off | 20,000 | scattered | 0.00349 | 0.00400 | 0.00380 | +8.8% noise | -5.1% noise | - |
| SetRange all, cold | 200,000 | ascending | 0.1889 | 0.3742 | 0.4116 | +117.9% worse | +10.0% noise | 1, 1,600,039 / 2, 3,200,078 / 2, 3,200,078 |
| SetRange all, warm | 200,000 | ascending | 0.0486 | 0.1170 | 0.3940 | +710.7% worse | +236.8% worse | 0, 0 / 0, 0 / 2, 3,200,078 |
| SetRange all, over the same selection | 200,000 | ascending | 0.0513 | 0.1174 | 0.1264 | +146.5% worse | +7.7% worse | 0, 0 / 0, 0 / 0, 0 |
| PreserveOrdered all kept | 200,000 | ascending | 41.8977 | 7.5382 | 8.1692 | -80.5% better | +8.4% noise | 200,013, 13,181,982 / 1, 1,600,039 / 2, 2,124,366 |
| PreserveOrdered half kept | 200,000 | ascending | 39.1252 | 4.2899 | 4.5722 | -88.3% better | +6.6% noise | 200,013, 13,181,982 / 2, 1,600,078 / 3, 2,124,405 |
| Clear after select all | 200,000 | ascending | 0.0000 | 0.0000 | 0.0985 | n/a worse | n/a worse | 0, 0 / 0, 0 / 0, 0 |
| SetSingle after select all | 200,000 | ascending | 0.0001 | 0.0000 | 0.1075 | +214900.0% worse | n/a worse | 0, 0 / 0, 0 / 2, 16 |
| Toggle one row on and off | 200,000 | ascending | 0.03974 | 0.03732 | 0.03762 | -5.3% noise | +0.8% noise | - |
| SetRange all, cold | 200,000 | scattered | 0.1895 | 11.5258 | 11.1111 | +5764.9% worse | -3.6% noise | 1, 1,600,039 / 2, 3,200,078 / 2, 3,200,078 |
| SetRange all, warm | 200,000 | scattered | 0.0519 | 10.8659 | 11.0115 | +21116.7% worse | +1.3% noise | 0, 0 / 0, 0 / 2, 3,200,078 |
| SetRange all, over the same selection | 200,000 | scattered | 0.0517 | 10.8043 | 10.7813 | +20773.8% worse | -0.2% noise | 0, 0 / 0, 0 / 0, 0 |
| PreserveOrdered all kept | 200,000 | scattered | 52.3280 | 21.0890 | 21.6620 | -58.6% better | +2.7% noise | 200,013, 13,181,982 / 1, 1,600,039 / 2, 2,124,366 |
| PreserveOrdered half kept | 200,000 | scattered | 44.2590 | 15.4712 | 15.2940 | -65.4% better | -1.1% noise | 200,013, 13,181,982 / 2, 1,600,078 / 3, 2,124,405 |
| Clear after select all | 200,000 | scattered | 0.0000 | 0.0005 | 0.1668 | n/a worse | +36966.7% worse | 0, 0 / 0, 0 / 0, 0 |
| SetSingle after select all | 200,000 | scattered | 0.0001 | 0.0013 | 0.1763 | +352500.0% worse | +14004.0% worse | 0, 0 / 0, 0 / 2, 16 |
| Toggle one row on and off | 200,000 | scattered | 0.04212 | 0.04008 | 0.04080 | -3.1% noise | +1.8% noise | - |

- `SetRange` over scattered ids sorts them, as before (11.1 ms at 200,000 ids, 0.83 ms at 20,000); the sort is left as it is.
- "Warm" is a `SetRange` after a `Clear`: on `refined` it allocates, because `Clear` gave the room back (2 calls, 3.2 MB at
  200,000 ids), where `current` reused it. "Over the same selection" is Ctrl+A twice, which reuses the room on all three.
- `PreserveOrdered` makes one allocation more than `current` for its table (and about 0.5 MB more at 200,000 ids), and still one
  hundred-thousandth of `main`'s heap calls (2 against 200,013); the verdict rule's exact budgets mark that growth against
  `current`.
- Of the 132 mutator comparisons against `main` (the counts include the 1,000-row cases that the table leaves out), 35 are faster
  (all `PreserveOrdered`: time, calls and bytes), 42 are within noise and 55 are slower: the second copy of `SetRange` (cold:
  time, calls and bytes, 18, as in the first record), its sort for scattered ids, `Clear` and `SetSingle` after Ctrl+A (the
  release, 16), `SetRange` after a `Clear` (the allocation, 14) and the `Toggle` at 1,000 scattered ids named above.
  Against `current` none is faster, 81 are within noise and 51 are slower, and all but one are consequences of this change: the
  release (`Clear` and `SetSingle`, 16), the allocation after it (`SetRange` warm, 9) and the table of `PreserveOrdered` (heap
  calls and bytes, 24, and its time once). The one is a `SetRange` over the same selection at 200,000 ascending ids (+7.7%, 9 us),
  which no change of the code explains and is inside what the placement of code moves.

### `SetRange`'s sort

Left as it is, as the review asked: 1.0 ms for 20,000 ids and 10.9 ms for 200,000 (this set: 0.83 and 11.1 ms) are the price of
the ascending copy for ids that do not already ascend. A least-significant-digit radix sort of the same ids is the measured
option. `refine-RadixProbe.cpp.txt` times it beside `std::sort` on the ids of the harness's scattered model (one multiplication,
in row order; one scratch buffer of 8 bytes an id; a pass whose byte is the same in every id is skipped; median of 21 repeats on
this laptop, one run):

| Ids | `std::sort` (ms) | radix sort (ms) | Ratio |
|---:|---:|---:|---:|
| 1,000 | 0.0053 | 0.0060 | 0.9 |
| 20,000 | 0.7431 | 0.1098 | 6.8 |
| 200,000 | 10.2367 | 1.5451 | 6.6 |
| 1,000,000 | 62.8341 | 8.5804 | 7.3 |

That is about a seventh of the sort at 20,000 ids and more, for a transient scratch buffer the size of the ids; at 1,000 ids
there is nothing to win. It is a probe outside the library, not a measurement of a change, and no radix sort is in the code.

### What the refinement establishes and what it does not

- Establishes: `IsSelected` is never slower than `main` at any measured selection size, in either order of questions (82
  comparisons at up to 1,024 ids, 80 within noise and 2 flagged at the 5% rate; all 16 at larger sizes faster), where `current`
  was up to 4.5 times slower for 2 to 512 ids in random order; the gains of a large selection are unchanged (paint, the cost
  inside a paint and the per-call time at 1,500 ids and more, with no paint metric differing from `current`); a model holds
  nothing after Ctrl+A and `Clear` or a click (3.2 MB before for 200,000 ids) at a cost of 0.1 to 0.2 ms per release at 200,000
  ids; `PreserveOrdered` takes 0.2 to 1.1 ms for 3 to 5,000 selected ids in 200,000 rows, where `main` took 1.2 to 2.8 ms and
  `current` 0.9 to 11 ms.
- Does not establish: a limit anywhere between 512 and 1,024 ids is better than 1,024 (the sizes between 512 and 768 were not
  measured); that `current`'s slowdown below 64 ids is absent for ids in other orders than the two measured; anything about a
  processor without the vector instructions of this one, where the scan is slower and the meeting point lower (the limit leaves
  a margin of about 1.6 to 2 times); the Tree, which does not use the model yet.
- Limits: as the first record's (WARP offscreen only, one laptop shared with other work with run-to-run spread of 10% to 100%
  for a paint metric, one compiler, 24 visible rows, x64 only), plus: a per-call time here is a hot loop of one call site, so
  it holds for what the processor caches and predicts in such a loop, and 65,536 random questions are as unpredictable as hashed
  ids and no more; the heap bytes are those of the standard library's allocations through the executable's `operator new`, not
  of the operating system's heap, which `privateAfter...Bytes` shows only coarsely; no repeat set was run for this refinement.
