# ColorPicker

Color selection sheet: saturation/value field, hue strip, new/current swatches, RGB and hex entry.

- **Consumer supplies**: size `kDefaultWidthDip` × `kDefaultHeightDip` (316 × 236), captions via `SetLabels`, `SetColor` to open, `SetOnChange(ColorPickerChange)`; feed eyedropper results through `SampleColor`. OK or Enter commit; Cancel or Escape restore.
- **Metrics**: field 160, hue strip 20, gap 8, swatch 44×28, rows 28, buttons 72. The field and strip paint literal color ramps; all chrome uses palette roles.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
