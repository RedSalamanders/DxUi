- Grid selection membership no longer scans the selection. `GridSelectionModel::IsSelected` was a linear search of the
  ordered ids and a `Grid` asks it once for every visible row on every paint, so after Ctrl+A on a long list each
  repaint searched the selection linearly 24 times (0.9 to 1.4 us a call at 20,000 selected rows, 47 to 83 us at
  1,000,000), and a pending Tree multi-select change reuses the model. The model now keeps its ids twice, in selection
  order (what `GetOrderedSelection` returns) and ascending beside it, and answers with a binary search that allocates
  nothing: 2 to 17 ns a call from 0 to 1,000,000 selected ids. In two interleaved sets of ten runs per side (x64
  Release, WARP, `main` against this) a paint of a Grid with 200,000 selected rows takes 27% to 30% less Prepare time
  and one with 1,000,000 rows 70% to 73% less, flat at 0.37 to 0.49 ms at every size; the cost of the selection inside
  a paint, isolated by painting alike with a small and a full selection, falls from 0.03 ms at 20,000 rows, 0.22 to
  0.28 ms at 200,000 and 1.3 to 1.4 ms at 1,000,000 to nothing measurable (the 20,000-row paint itself differs by less
  than the runs separate), and a paint allocates the same 11,820 times per 60 paints as before. What a selection holds
  is unchanged: the order of `GetOrderedSelection`, the anchor rules of `Toggle` (the first id of the selection order
  when the anchor leaves), `SetRange` (the id it started from) and `PreserveOrdered` (the first id kept), an id a list
  gives twice (held twice), and the exception contract (`Clear`, `SetSingle`, `Toggle` and the accessors stay
  `noexcept`; `SetRange` and `PreserveOrdered` leave the selection as it was when an allocation fails). The costs: 8
  bytes per selected row (Ctrl+A over 20,000 ids asks the heap for 320 KB where it asked for 160 KB; the model grows
  from 40 to 64 bytes), `SetRange` over ids that do not already ascend sorts them (1.0 ms for 20,000 ids, 10.9 ms for
  200,000; ascending ids cost 0.004 and 0.06 ms more), and `IsSelected` is 0.7 to 2 ns slower for selections of 8 to
  64 ids (flagged by the verdict rule at 8 and 16 ids, p = 0.03 and 0.02, and negligible against a paint).
  `PreserveOrdered`, which `NotifyDataChanged` calls on every data change of a Grid with a selection, no longer builds
  a hash set: 0.65 ms instead of 1.57 for 20,000 ascending ids and 7.3 instead of 29.3 for 200,000, with one heap call
  instead of 20,010 and 200,013, and it leaves everything alone when the selection is unchanged. Tests (Grid suite): a
  copy of the old linear logic as the reference and four fixed-seed runs of 11,500 mixed operations checked after
  every one (count, order, anchor, and `IsSelected` for every id against both the reference and the model's own
  `GetOrderedSelection`), which must reach repeated ids, fallbacks, dropped ids, unchanged data changes, the anchor's
  removal and half-full selections; focused cases (a range over repeated ids, backward ranges and fallbacks, an anchor
  removed by `Toggle`, `PreserveOrdered` dropping and reordering ids, membership after every mutator, a scattered
  20,000-id selection, copies); a Grid-level test of Ctrl+A, Ctrl+click, Shift+click and a data change over 4,000
  rows; and `static_assert`s of the `noexcept` contract. They pass unchanged on the old model, and each of 15
  throwaway mutants (`Clear`, `SetSingle`, `Toggle` (adding and removing) and `PreserveOrdered` each forgetting the
  ascending copy, `Toggle` inserting out of order, `SetRange` leaving it unsorted or deduplicated, `Toggle` keeping
  the anchor it removes or removing the last occurrence of a repeated id, `SetRange` anchoring at the current id,
  `PreserveOrdered` keeping a dropped anchor, returning the ascending order or treating any result of the same size as
  unchanged, and `IsSelected` answering true for any id not above the largest) fails a named assertion. The opt-in
  `DxUi.EmbeddedTests.exe --benchmark-grid-selection` (`Tests/Embedded/GridSelectionBenchmark.h`) measures paint, the
  isolated cost, `IsSelected` per call and the mutators with synthetic data; its receipts are under
  `Measurements/GridSelection/2026-10-01`. API revision stays 2 (a private member of `GridSelectionModel` and so the
  size of `Grid` changed). Docs and the gallery need no update: no public API, default or pixel changed (the six
  sheets of a local Release render are byte-identical to `main`'s and to `docs/gallery`), and `docs/performance.md`
  and the performance contract now describe the measurement and the budget.
