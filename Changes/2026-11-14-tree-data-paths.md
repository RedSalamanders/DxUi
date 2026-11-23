- Stop scanning a `Tree`'s model on every key and copying every row on every expand or collapse. A tree whose model keeps
  `ITreeModel`'s default `FindVisibleItemById` (which reads each row up to the match) now answers id lookups from an id
  index it owns: each distinct visible id with its first row, 12 bytes per visible row, built by one read of every row
  after a change of the visible rows and released with the model. A model that overrides `FindVisibleItemById` is always
  asked and gets no index, so its memory stays independent of the row count. The index follows `NotifyDataChanged`,
  `SetModel`, `SetDelegate`, the visible row count and the tree's own toggle and drop callbacks; every other change of the
  visible rows must be published with `NotifyDataChanged`, as selection and focus reconciliation already required (the
  contract in `Specs/UI/UI_ControlsAndLayout.md`, "Tree item lookup and model notifications", lists them). Up, Down,
  Home, End, Page Up and Page Down now read only the row that takes the focus, and `SetFocusedItemId` looks its item up
  once. An expand or collapse reads every row once before and after the change, keeps ids for the rows that stay and whole
  records only for the rows a collapse removes, read into place and never copied (before: about `3N + 3N'` records);
  its frames are unchanged. Additive diagnostics: `Tree::DebugSetIdLookup` (`Index`, `Verified`, `Model`),
  `DebugGetIdLookupMismatchCount`, `DebugGetIdIndexRowCount` and `DebugGetExpansionRecords`; private work counters
  `lists.treeItemReads`, `treeModelLookups`, `treeIdIndexBuilds`, `treeExpansionRecords` and `treeExpansionRecordCopies`.
  `FX9` gains a last `tree-keys` phase (24 navigation keys on the focused Tree).
  - Validation: the `Tree` suite runs in the verified lookup mode and requires no difference; seeded models (empty, one
    row, deep and wide, all collapsed, alternating, 10,000 rows, duplicate ids, missing ids) give equal focus, selection,
    scroll and delegate events in Index and Model modes; the FX9 and FX11 gates and the collapse frame hashes
    (`DxUi.EmbeddedTests.exe --test-tree-data-paths`, `--tree-motion-frames`) in x64 Debug, Release and ASan Debug, with
    receipts under `Measurements/PerformanceAndMotion_2026-11-13/W2.6`.
