# W2.6 `Tree` data paths — model reads per key, records per expand or collapse, collapse frames

Plan item W2.6 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior
in [controls and layout](../../../Specs/UI/UI_ControlsAndLayout.md) ("Tree item lookup and model notifications", "Tree
expansion animation capacity") and the counters in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md) ("Work counters", Lists).

- **Scenarios**: the fixtures' own definitions. `FX9` (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx9.h`) gained a
  last phase, `tree-keys`: step 0 focuses the Tree on its first visible row (`SetFocusedItemId`, no key), then 24 steps
  press one navigation key each (Down, Up, Page Down, Page Up, End, Home). Its 10,000-row model answers
  `FindVisibleItemById` itself. `FX11` (`Embedded.Tests.PerformanceFixtureFx11.h`, unchanged) toggles a folder of six rows
  through `RequestExpandedState` in its `tree-expand-<rate>hz` and `tree-collapse-<rate>hz` phases, sampled at 30, 60,
  144 and 240 Hz on the frozen fixture clock, on a 15/21-row model that keeps `ITreeModel`'s default lookup. Five more
  cases (`Embedded.Tests.TreeDataPaths.h`, `RunMotionCase`) toggle the first folder of a 108-row tree (folders of 40, 5
  and 60 rows, the same default lookup) in a 192 DIP viewport at 60 Hz: a wheel notch at step 3 of a collapse
  (`collapse-scrolled`), the viewport growing to 344 DIP at step 4, showing removed rows that were below it
  (`collapse-resized`), an expansion at step 3 before the collapse ends (`collapse-reopened`), an expansion while scrolled
  one notch (`expand-scrolled`) and a collapse whose removed rows straddle the viewport (`collapse-mid-list`).
- **Counts**: work counters `lists.treeItemReads` (model rows a Tree reads outside `Paint`; `ITreeModel`'s default
  lookup counts its reads), `lists.treeModelLookups`, `lists.treeIdIndexBuilds`, `lists.treeExpansionRecords` (whole row
  records an expand or collapse keeps) and `lists.treeExpansionRecordCopies`. Frames: FNV-1a over the BGRA pixels of the
  Tree's bounds of every sampled frame, 513 frames in all (FX11's eight tree phases and the five cases).
- **Source**: "before" is base `2f9001a3` plus the counters, the `tree-keys` phase and the measuring code only (the
  library's lookups and records as they were); "after" is the W2.6 change on the same base, in Debug, Release and ASan
  Debug. x64, MSVC 19.52.37025, WARP, Windows 11 build 26300. No timing is recorded.
- **Commands**: `DxUi.EmbeddedTests.exe --fixture-receipt FX9@default Warp <file>` and `FX11@default`;
  `--tree-motion-frames <file>`; the gate `--test-tree-data-paths` (also run by the Embedded suite); the gallery
  `./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'Tree' -Warp` then `-CompareBefore
  Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json`.
- **Files**: Debug fixture receipts, [FX9 before](fx9-default-embedded-warp-x64-debug-before.json),
  [FX9 after](fx9-default-embedded-warp-x64-debug-after.json), [FX11 before](fx11-default-embedded-warp-x64-debug-before.json),
  [FX11 after](fx11-default-embedded-warp-x64-debug-after.json); frame hashes,
  [before, Debug](tree-motion-frames-x64-debug-before.json), after [Debug](tree-motion-frames-x64-debug-after.json),
  [Release](tree-motion-frames-x64-release-after.json) and [ASan Debug](tree-motion-frames-x64-asan-debug-after.json).
  Each file is stamped with the SHA-256 of its inputs as committed (`benchmarkInputs`) and `baseCommit`; the "before"
  files were produced by the earlier library revision described above. The gallery comparison reported 30 of 30 tiles
  identical (`.build/gallery/parity/parity.json`, not retained). The Release receipts of both fixtures are in
  [FX9](../FX9/README.md) and [FX11](../FX11/README.md).

## Gate

| Clause | Before | After |
| --- | --- | --- |
| `FX9` (10,000 rows): model reads per navigation key beyond the focused row | 1 (the row being left and the row taking the focus: 2 reads per key) | **0** (1 read per key: the row taking the focus) |
| `FX11`: record copies per expand or collapse, at most the viewport's 12 rows | 72 copies; 36 records kept (every row before and after) | **0** copies; 0 records for an expand, 6 (the removed rows) for a collapse |
| Collapse (and expand) frames with a frozen clock, WARP | — | **513 of 513 frame hashes identical** to before, in Debug, Release and ASan Debug |
| `Tree` gallery tiles at 100, 150 and 200 % against `G1` | — | **30 of 30 identical** |

`FX9` per step of `tree-keys` (24 keys after step 0), Debug receipts: `lists.treeItemReads` 2 per key before, 1 after;
`lists.treeModelLookups` 1 per key both (the model's own O(1) lookup of the focused item) and 2 → 1 at step 0
(`SetFocusedItemId` looks its item up once); `lists.treeIdIndexBuilds` 0; C++ allocations 2 fewer per key (729-731 → 727-729,
frame included). `FX11` per transition (step 0 of each tree phase): reads 39 → 38 and lookups 1 → 0 after the first
toggle (the toggled item is found in the rows read after the change), records kept 36 → 0 (expand) or 6 (collapse),
record copies 72 → 0, C++ allocations about 370 fewer (60 Hz: 1,045 → 673 expand, 1,042 → 672 collapse).

Every pre-existing phase of both fixtures records the same series before and after, counter by counter, except the
counters this change moves in the eight `FX11` tree phases (`cppAllocations` and the `lists` counters above); `FX9`'s
`warm`, `hover`, `grid` and `tree` phases are unchanged in every counter. The ASan Debug receipts equal the Debug ones.
The regenerated Release receipts (rebased on `79b20d43`, after W2.5 and W3.1) differ in the `FX11` receipts only by the eight
tree phases; the `FX9` receipts were older than W2.5 and W3.1, whose Grid and allocation changes they now include, and gain the
new `tree-keys`
phase, the new `lists` counters (the `FX9` `tree` phase reads 2 rows per wheel step, as before: the pointer's hit test
and hover after each notch, newly counted) and, in the eight `FX11` tree phases, about 74 fewer C++ allocations per
transition in Release; the `FX11` receipts also gain counters other items added since they were taken
(`frame.shadowPasses`, `frame.d2dDevices`, `frame.swapChains`). On that tip the 513 frame hashes are still identical to before (Release).

## Memory

The id index exists only for a model that keeps `ITreeModel`'s default lookup: 12 bytes per distinct visible id (an
8-byte id and a 4-byte row in two exactly sized arrays). Measured (`TestTreeIdIndexBytesPerVisibleRow`, 10,000 rows):
10,000 entries, 120,000 resident bytes; the key that built it left 120,078 live C++ bytes in Release and 120,094 in
Debug (the rest is host input state of that key, not the index). Building it holds 24 more bytes per row transiently
(the ordered ids and the sorted pairs). `FX9`'s model answers lookups itself, so its Tree holds no index: the registry row
`Tree._visibleIdIndexCache` reads 0 entries and 0 bytes, and its tracked live bytes differ from before by the same 96 bytes
at every sample from the build on (the larger `Tree` object and, in Debug, its new vectors' iterator proxies), independent
of the row count (`G15`). Against `G13` (256 B per simple control all-in), the index is per row, not per control; it is
O(visible rows) only where a model has no lookup of its own, the case `W10.1`'s single virtualizer replaces.
