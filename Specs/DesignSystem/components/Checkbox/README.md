# Checkbox

Two- or three-state check with a caption; shares Toggle's checked-state and event semantics.

- **Consumer supplies**: caption, checked state, `SetOnToggled`. `SetMultiline(true)` wraps long captions — reserve 36 DIP horizontally and 6 DIP vertically around measured Body text.
- **Metrics**: 16 DIP box, `radius-sm`, inset 6, 8 DIP gap to text; indeterminate dash 1.5 DIP stroke inset 4.
- **Colors**: checked and indeterminate fill `selectionFill` with `selectionText` glyph U+E73E (fallback ✓).

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
