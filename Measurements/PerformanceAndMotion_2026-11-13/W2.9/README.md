# W2.9 pointer-move cost: before and after

Plan item `W2.9` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
rule: [pointer hit result](../../../Specs/UI/UI_InputAndAccessibility.md#pointer-hit-result).

## Counters on the same fixtures

Both sides are x64 Release on WARP, from the integration tip `c51f35df` (before: that tip with only the four new work
counters added; after: the `W2.9` change). The after receipts are the fixture receipts in [FX14](../FX14/README.md),
[FX2](../FX2/README.md), [FX10](../FX10/README.md) and [FX5](../FX5/README.md); the before receipts were taken with
`DxUi.EmbeddedTests.exe --fixture-receipt` and `DxUi.ControlTests.exe --fixture-receipt` from the before build and are
summarized here. Per measured step:

| Fixture, host, phase | Counter | Before | After |
| --- | --- | --- | --- |
| `FX14` embedded, every move (`enter`, `pending`, `shown`, `leave`) | `frame.hitTests` | 2 | 1 |
| | `frame.hitTestReuses` | 0 | 1 |
| | `frame.treeVisitsHitTest` | 4 | 2 |
| `FX14` window, every move | `frame.hitTests` | 2 (cursor query and move; see below) | 1 |
| | `frame.hitTestReuses` | 0 | 1 |
| `FX2` embedded, `hover` and `fit-hover` | `frame.hitTests` | 2 | 1 |
| | `resources.textLayout.created` | 11,000 / 20,600 | 10,500 / 20,500 |
| `FX10` embedded, `sweep` | `frame.preparationMembershipSearches` | 11 | 0 |
| | `frame.plainLabelPreparations` | 10 | 0 |
| `FX10` window, `sweep` (a step that prepared) | `frame.preparationMembershipSearches` | 11 | 0 |
| | `frame.plainLabelPreparations` | 10 | 0 |
| `FX5` embedded, `warm` (total) | `frame.preparationMembershipSearches`, `frame.plainLabelPreparations` | 16, 16 | 0, 0 |

No other counter moved on these fixtures, except that `FX2`'s saved hit test no longer re-runs the `ComboBox` popup layout
inside `HitTestOverlay` (500 and 100 width-measurement layouts per hover step, `created` and `churn` with them; `W2.2`
removes the rest). `FX5` clean and dirty frames are unchanged, `cppAllocations` included.

`FX14`'s window steps now make the cursor query Windows' `WM_SETCURSOR` makes before each move (the window runner sends
only the move). Before `W2.9` that query and the move each walked the tree: the before receipt, taken without the query,
shows 1 per move for the move alone; `TestPointerHitWindowUnreportedControlIsNeverReused` shows the 2 walks of a query and
a move without reuse.

## Label tiles

`Label` is the control whose preparation changed (plain labels are no longer prepared). Its 10 gallery tiles (`label-body`
and `label-typography-override` in five themes) at 100, 150 and 200% are retained here: `<percent>/<theme>/<tile>.png`,
`receipt.json` (provenance: base `c51f35df` with the change applied, `sourceDirty: true`; x64 Release; WARP forced) and
`tiles.tsv`. They are pixel-identical to the all-tiles receipt `G1`:

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'Label' -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json -CompareAfter .build/gallery/x64/Release/tiles -Scale 100,150,200 -Tiles 'Label'
```

reports 10 of 10 identical at each scale.
