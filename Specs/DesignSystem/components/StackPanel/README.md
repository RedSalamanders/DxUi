# StackPanel

Panel that lays children out along one axis.

- **Consumer supplies**: `SetOrientation`, `SetGap`, `SetPadding`, a `SetChildExtent` for every child, and `ApplyLayout()` after content or bounds change.
- **Chrome**: none; the dashed guide in the preview is not painted. Use spacing tokens for gap and padding.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
