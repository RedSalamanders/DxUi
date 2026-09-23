# PageIndicator

A strip of dots showing the current page of a paged surface.

- **Consumer supplies**: `SetPageCount`, `SetSelectedIndex`, `SetOnSelected` (clicks and Left/Right/Home/End). Hidden and not hittable with fewer than two pages.
- **Metrics**: strip 20 DIP (`kStripHeightDip`), dot radius 3 (4 selected), gap 14.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
