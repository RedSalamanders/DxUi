- Grid selection membership no longer scans a large selection, and a selection model no longer keeps the room of one it has
  dropped. `GridSelectionModel::IsSelected` was a linear search of the ordered ids and a `Grid` asks it once for every visible
  row on every paint, so after Ctrl+A on a long list each repaint searched the selection 24 times (0.9 to 1.4 us a call at
  20,000 selected rows, 47 to 94 us at 1,000,000); a pending Tree multi-select change reuses the model. The model now keeps
  its ids twice, in selection order (what `GetOrderedSelection` returns) and ascending beside it, and answers a selection of
  up to 1,024 ids by the scan it always used and a larger one by a binary search of the ascending copy, which allocates
  nothing. What a selection holds is unchanged: the order of `GetOrderedSelection`, the anchor rules of `Toggle` (the first id
  of the selection order when the anchor leaves), `SetRange` (the id it started from) and `PreserveOrdered` (the first id
  kept), an id a list gives twice (held twice), and the exception contract (`Clear`, `SetSingle`, `Toggle` and the accessors
  stay `noexcept`; `SetRange` and `PreserveOrdered` leave the selection as it was when an allocation fails).
  - The limit is 1,024, not the 16 to 64 that a first measurement suggested. That measurement asked the same 32 ids on every
    pass, so a processor learned which way each comparison of a search went and the search looked 2 to 5 ns at any size; its
    0.7 to 2 ns slowdown below 64 ids belonged to the two executables (built alike, the sorted copy is never slower than `main`
    in that order). The ids of the rows on screen, asked of a selection of hashed ids, come in no order a processor can learn,
    and a search then costs 5 to 20 ns more than the scan at 8 to 128 selected ids (a miss at 16 ids: 4.4 ns on `main`, 18.6 on
    the sorted copy alone). The two meet at about 500 to 650 ids; 1,024 is the first power of two beyond them, so that no size
    is slower than the scan it replaced. In three builds of ten runs per side (`main`, the sorted copy alone and this; x64
    Release, WARP; run as A, B, C, C, B, A), of 82 per-call comparisons at up to 1,024 ids against `main` 80 are within noise
    and 2 read slower at the 5% rate, and all 16 above 1,024 ids are faster (1,500 ids: -90% in the order a processor
    predicts, -44% to -59% in random order; 1,000,000 ids: -100%, 14 to 16 ns, or 149 ns in random order, against 49 to 94 us).
  - A Grid's paint no longer grows with its selection: Prepare takes 22% less at 200,000 selected rows and 64% less at
    1,000,000 (27% to 30% and 70% to 73% in the first set), flat at 0.5 to 0.9 ms, and what `IsSelected` costs inside a paint,
    isolated by painting alike with a small and a full selection, falls from 0.05, 0.35 and 1.6 ms at 20,000, 200,000 and
    1,000,000 rows to nothing measurable; a paint allocates as often as before, and no paint metric differs from the sorted
    copy alone.
  - A buffer with room for more than 4,096 ids (32 KiB) is given back when the ids that replace its contents need at most half
    of it: `Clear` and `SetSingle` swap it with an empty vector (no allocation in a Release build, so both stay `noexcept`),
    `SetRange` makes both copies beside the old ones and swaps them in, a `PreserveOrdered` that drops ids keeps no room it
    reserved for a selection that shrank, and `Toggle` leaves its room. After Ctrl+A over 200,000 ids a model held 3.2 MB (twice
    what the ordered ids alone held); it holds 0 bytes after `Clear`, 16 after a click on a row and 1,600 after a Shift+click 99
    rows away, and a selection of 4,096 ids keeps its room. Giving the room back costs 0.1 to 0.2 ms and getting it again
    0.3 ms at 200,000 ids (1 to 2 us at 20,000), against 0.4 to 11 ms for the Ctrl+A itself.
  - `PreserveOrdered`, which `NotifyDataChanged` calls on every data change of a Grid with a selection, asks about every row of
    the model. The sorted copy made it cheaper than the hash set it replaced when all or half of the rows are selected (0.70 ms
    instead of 1.45 for 20,000 ascending ids, 8.2 instead of 41.9 for 200,000), but dearer for a modest selection in a long list
    with hashed ids: 200,000 rows with 16 to 5,000 ids clicked took 2.4 to 11 ms where `main` took 1.2 to 2.8. It now asks a
    table of bits (16 to 32 per selected id) first and searches only the rows it lets through: 0.2 to 1.1 ms for 3 to 5,000
    ids, with 2 or 3 heap calls where `main` made 5 to 5,008 and the sorted copy alone 1 or 2.
  - The costs: 8 bytes per selected row (the model grows from 40 to 64 bytes); `SetRange` over ids that do not already ascend
    sorts the second copy (1.0 ms for 20,000 ids, 10.9 ms for 200,000; a probe puts a radix sort at about a seventh of that,
    and it is not done); `Toggle` at 1,000 scattered ids takes 0.26 us where it took 0.17; and an empty selection costs the
    scan's 4.7 ns a call where the sorted copy alone cost 2.2.
  - Tests (Grid suite): a copy of the old linear logic as the reference, and fixed-seed randomized runs over universes of 1
    to 5,200 ids (so that selections pass 1,024 and 4,096 ids) that check count, order, anchor and `IsSelected` after every
    operation and the room rule after every mutator that replaces the selection, and must reach repeated ids, fallbacks, dropped
    ids, unchanged data changes, the anchor's removal, selections on both sides of both limits, and every way of giving room
    back; focused cases (a selection grown past 1,024 ids and shrunk back, answering membership at every size around the
    limit; `Clear` keeping 4,096 ids' room and giving back 4,097's; a range over exactly half of the room giving it back and
    one id more reusing it; a click and a Shift+click after Ctrl+A at the Grid; a long list of 60,000 hashed rows kept in two
    orders; the earlier ones for repeated ids, anchors and orders); and `static_assert`s of the `noexcept` contract. The room is
    asserted through an additive diagnostics accessor, `GridSelectionModel::DebugGetBuffers`, the library's own exact count,
    like `DebugGetContextMenuResources`. Of 40 throwaway mutants, 35 are caught (the first campaign's 15, now also through
    `Toggle` and the large selections, where the scan hides the ascending copy; the scan dropping the newest or oldest id; the
    search reading the wrong copy; each limit and half-rule comparison; releasing one buffer but not the other, for `Clear`,
    `SetSingle` and `SetRange`; the filter turned off or built from the wrong ids), and the other 5 cannot fail a test because
    they give the same answers (a scan limit of 1,023 or 0 ids, the scan and the search swapped, a filter that lets every id
    through, `SetRange` testing the room of one copy only, the two always being equal): the per-call measurement sees those.
  - The opt-in `DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json> [parts]` (`Tests/Embedded/GridSelectionBenchmark.h`,
    fixture `dxui-grid-selection-v2`) measures paint, the isolated cost, `IsSelected` per call in both orders, the heap a model
    holds after each way back from Ctrl+A, the mutators and `PreserveOrdered` over a long list with synthetic data; the
    allocation hook of `EmbeddedTests.cpp` keeps an exact live byte count on request. Its receipts and README are under
    `Measurements/GridSelection/2026-10-01` (two sets of the sorted copy against `main`, and the three-build set).
    API revision stays 2 (an additive diagnostics accessor, and private members of `GridSelectionModel` and so the size of
    `Grid`, changed). The performance contract, the testing contract and `docs/performance.md` describe the budget and the
    measurement; no default or pixel changed (the six sheets of a local Release render are byte-identical to `main`'s and to
    `docs/gallery`).
