# TagPicker

Multi-select of named choices shown as removable tags.

- **Consumer supplies**: `SetOptions(allLabel, values)`, `SetSelectedValues`, `SetOnSelectionChanged`.
- **Metrics**: minimum height 32, padding 4, tag gap 6, row gap 4, tag row 24, remove button 18, tag width 48–172, tag radius `radius-tab` (5).

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
