# ComboBox

Drop-down selection in three variants: Window, Modern and Edit (editable text with filtering).

- **Consumer supplies**: `Item` records via `SetItems`; `SetOnSelectionChanged`, text/submission callbacks and popup requests. For touch call `SetMinimumPopupItemHeight(48.0f)`.
- **Metrics**: `radius-sm` for Window/Edit, `radius-md` for Modern; drop button 22 DIP (20 compact); popup padding 4, up to 8 visible items.
- **Colors**: field as TextField; popup `overlayBackground` with `overlayBorder` and `shadow-popup`; selected item `selectionFill`/`selectionText`, hover `hoverFill`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
