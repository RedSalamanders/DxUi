# Tree multi-select and drag-reorder

- **Status**: HOLD (drag-reorder is in the library; multi-select is not)
- **Owner**: DxUi `Tree` (`include/DxUi/DxUi.h`, `src/Controls`). RedPrism consumes the result with a pin bump.
  It must not grow a second tree widget.
- **Why**: RedPrism's layers panel is a `Tree`. `IDxTreeDelegate` can select, invoke, expand, and show a context
  menu. It cannot multi-select or drag a row. `TabControl` and `Grid` already drag-reorder. Group Selected
  Layers and Merge Layers in RedPrism already accept several layer ids; the panel can only select one, and
  reorder stays on the keyboard (`Ctrl+]` / `Ctrl+[`).

## Contract to add

- Modifier click extends the selection. The model stays UI-thread only.
- A pointer drag previews an insertion line. Release commits once (source id, insert-before id, parent id).
  Escape or capture loss cancels.
- `IDxTreeDelegate` gains the reorder callback and the multi-select change callback. Match the existing
  `TabControl` / `Grid` drag-reorder gestures. Do not put a document or layer type in the library.
- Catalog interaction tests and a gallery tile. `test.ps1` in x64 Debug, Release, and ASan Debug, plus the
  ARM64 build.

## STOP

Editing `LocalizedAdaptiveLayout` as a side effect. Resetting a consumer checkout. Shipping the gesture only
inside RedPrism.
