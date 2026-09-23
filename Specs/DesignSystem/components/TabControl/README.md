# TabControl

Tabbed pages with optional close and reorder.

- **Consumer supplies**: populated pages via `AddTab`; selection, close-request, closed and reorder callbacks as needed.
- **Metrics**: strip 32, header padding 12, gap 4, minimum tab width 72, close button 16, overflow button 24, tab radius `radius-tab` (5). Hover `headerHovered`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
