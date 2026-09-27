# MenuBar

Application menu bar and its drop-down / context menus.

- **Consumer supplies**: `MenuBarItem` records via `SetItems`; `SetOnOpenItem` and hover changes. Native menu operations need the HWND integration.
- **MenuBar metrics**: height 30 (24 compact), inset 2, item padding 10 (6 compact), gap 2.
- **Menu metrics**: padding 4 top/bottom, width 128–456, icon area 28, text starts at 36, accelerator gap and right padding 16, chevron area 24, separator 9 (1 DIP line, 4 above and below), submenu offset 4, cascade hover delay 400 ms. Check U+E73E, radio bullet U+F137, submenu U+E76C.
- **Described entries**: optional `secondaryText` under the label for Standard/Toggle/Radio/Info rows, in Small type and the accelerator color. Both fields wrap at the menu width and the row grows; `accessibleName` overrides the spoken identity. Qualification is pending.
- **Colors**: `overlayBackground`, `overlayBorder`, `shadow-menu` (Solid) or `shadow-menu-acrylic`; accelerators in `subduedText`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
