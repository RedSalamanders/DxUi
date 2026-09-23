# StatusStrip

Bottom status bar with one value or several aligned sections.

- **Consumer supplies**: `SetText` for one value, or `SetSections` and `SetSectionText` for aligned values.
- **Type**: `ListItem` (12 DIP) in `text` on `surfaceBackground`; sections separated by `gridLine`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
