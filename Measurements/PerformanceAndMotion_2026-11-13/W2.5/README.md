# W2.5 Grid data paths: before and after on FX3

Plan item `W2.5` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); contract
"Grid data caches" in [controls and layout](../../../Specs/UI/UI_ControlsAndLayout.md#grid-data-caches).

- **Scenario**: fixture `FX3` ([definition](../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx3.h)): a 2,048-row Grid
  (wheel, touchpad, column resize, 200 arrow keys), then the grouped model with a select-all and ten arrows; variant
  `default` (20,000 rows in 1,000 groups) and the receipt-only `grouped-full` (200,000 rows in 10,000 groups).
- **Before (counts)**: the retained [FX3 receipts](../FX3/README.md) of integration tip `89eba619b`, x64 Release, WARP, draw
  calls counted.
- **After (counts)**: [FX3/](FX3/README.md), the same tip with this item, written by
  `performance-fixtures.ps1 -Fixture FX3 -FixtureHost Embedded -CountDrawCalls -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W2.5`
  (x64 Release, MSVC 19.52.37025, WARP, embedded host). Counts only; timing not measured.
- **Pixels**: `DxUi.EmbeddedTests.exe --fx3-frame-hashes <variant> <file>`, an FNV-1a hash of the prepared WARP surface after
  each of the 423 steps. `fx3-<variant>-frames-before.txt` comes from base `c51f35df0` without the item;
  `fx3-<variant>-frames-after.txt` from the item. The item rebased on `89eba619b` reproduces the after hashes exactly.

## Pixels

All 423 frame hashes are equal before and after in both variants: the change draws exactly what the base drew.

## Counts per step (C++ allocations, Release; each step includes its frame)

| Phase | default before | default after | grouped-full before | grouped-full after |
| --- | --- | --- | --- | --- |
| wheel (max) | 299 | 98 | 299 | 98 |
| touchpad (max) | 298 | 97 | 298 | 97 |
| resize (max) | 299 | 97 | 299 | 97 |
| arrows (max) | 303 | 102 | 303 | 102 |
| grouped-wheel step 0 (new model) | 24,212 | 2,116 | 240,212 | 20,116 |
| grouped-wheel steady (last) | 36,142 | 93 | 360,142 | 93 |
| select-all | 16,225 | 96 | 160,225 | 96 |
| selected-arrow first (collapse) | 30,245 | 179 | 300,245 | 179 |
| selected-arrow steady (last) | 30,155 | 89 | 300,155 | 89 |

After: `lists.gridCollectOrderedGroups` and `lists.gridGetColumnCalls` are 0 on every measured step except grouped-wheel step
0, where the new model fills each cache once (1 collection; 4 GetColumn for the column fill and 4 for the column widths).
The remaining allocations are the frame's (the paint's visible-item list and text work, and the models' own formatting of
cell text), which `W2.1`, `W4.2` and `W9.4` own. Text layouts created per step are unchanged (wheel 12, touchpad 4).

## Tracked live C++ bytes (after each phase)

| Sample | default before | default after | grouped-full before | grouped-full after |
| --- | --- | --- | --- | --- |
| arrows | 31,837 | 32,613 | 31,837 | 32,613 |
| grouped-wheel | 127,652 | 136,467 | 991,652 | 1,072,467 |
| select-all | 447,714 | 456,529 | 4,191,714 | 4,272,529 |
| selected-arrow | 130,468 | 139,283 | 995,876 | 1,076,691 |
| hidden (after the W9.1 cell probe) | 105,085 | 112,780 | 969,085 | 1,048,780 |

The groups were already held between paints before; the added bytes are the collapsed-row prefix (8 bytes per group) and the
column descriptions, O(groups) and O(columns). `FX9` (its retained receipts against the same native receipt command on the
item) tracks 44,832 and 45,248 bytes after its grid phase before and 45,408 and 45,824 after, at 100,000 and 1,000,000 rows:
the same 416-byte difference between the two row counts, so nothing the change adds grows with the rows. Its grid scroll
step drops from 129 to 103 C++ allocations.

On `FX5` (ComplexUi) a dirty frame drops from 54 C++ allocations to 2 in Release: the 52 were the scene model's "Column N"
titles, one per header and visible cell. The [work-counter receipt](WorkCounters/README.md) records it in all three
configurations.

## Gate

`TestFx3GridDataPathGate` ([gate](../../../Tests/Embedded/Embedded.Tests.PerformanceGridDataPaths.h), Embedded suite) asserts on
both variants: no collection and no GetColumn per steady frame, no selection copy, after the select-all 0 DxUi allocations
per Down key and per click or Ctrl+click (the models' own formatting subtracted) and 2 (the selection's one-id buffers the
give-back rule requires) for the Down that replaces the select-all, held cell layouts within the drawn cells after every
step, and every frame equal to a fresh view's.

## Gallery

`./gallery.ps1 -Configuration Release -SkipBuild -Scale 100,150,200 -Tiles 'Grid' -Warp` then `-CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json`: the five Grid tiles are identical to the G1 receipt at 100%, 150% and 200% (15 of 15). No tile is retained here: none changed.
