- Grid data paths (performance and motion plan W2.5): a Grid reads its model's groups and column descriptions once per
  data revision (`NotifyDataChanged`, `SetModel`, `SetDelegate`, a group toggle it asked for, `ApplyColumnLayout`, or a
  changed row, group or column count) and paints, hit-tests and moves the keys from those copies, so a steady frame no
  longer collects the groups about ten times nor asks `GetColumn` once per header and cell. Row tops are a binary search
  of a collapsed-row prefix instead of a walk over the groups (O(G^2) per grouped frame before); arrow keys, Page keys,
  Home, End and clicks list no rows; Shift and Ctrl+Shift ranges list only the rows they select. `GridSelectionModel`
  gains `GetEpoch`, which moves exactly when the ordered selection changes and replaces the copy a gesture took to
  compare it (1.6 MB after a 200,000-row select-all); `AssignInOrder` and `SetSelectedRowIds` answer membership through
  an ascending copy behind a table of bits instead of a hash set. A change to groups or columns that is not published
  with `NotifyDataChanged` now shows only at the next notification, as the contract always required (see "Grid data
  caches" in the controls contract); diagnostics builds verify it with `Grid::DebugSetDataPathVerification`, which the
  Grid suite runs fatal. On `FX3` (Release, WARP, each step with its frame) a steady grouped wheel step drops from 36,142
  C++ allocations to 93 and an arrow after the select-all from 30,155 to 89, with every frame's pixels unchanged; the gate is in
  `Embedded.Tests.PerformanceGridDataPaths.h` and the equality tests in `Controls.Tests.GridDataPaths.cpp`. A dirty
  ComplexUi (`FX5`) frame drops from 54 C++ allocations to 2 in Release, and from 257 to 23 in Debug and ASan Debug (the
  scene's Grid formatted a "Column N" title for each header and visible cell); the work-counter, FX5 and trace-provider
  gates pin the new Release count from one constant, with
  the receipt under `Measurements/PerformanceAndMotion_2026-11-13/W2.5/WorkCounters`.
  `performance-fixtures.ps1 -OutputRoot` now writes the generated README's links from the directory it writes them in (they
  were one level too shallow under a nested output root).
