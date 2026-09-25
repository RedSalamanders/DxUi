# Toolbar

Command strip of icon-only buttons, toggle buttons and separators.

- **Consumer supplies**: `AddButton(tooltip, glyph)`, `AddToggleButton(tooltip, glyph)`, `AddSeparator()`, bounds and actions. Every icon button MUST carry tooltip text — it is the accessible name.
- **Colors** (`ResolveToolbarVisualStyle`): `cardBackground` with a 1 DIP `borderDefault` bottom border; separators 1 DIP, Blend(`windowBackground`, `borderDefault`, 0.36 light · 0.50 dark), inset 6 DIP (4 compact).

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
