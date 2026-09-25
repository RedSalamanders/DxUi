DxUi is a Windows C++ retained-control library drawn with Direct2D and DirectWrite. Its look is quiet, Fluent-adjacent Windows chrome: neutral surfaces, one blue accent, 4 DIP corners, 1 DIP hairlines and a two-stroke keyboard focus ring. Every value here is a DIP (device-independent pixel) and maps 1:1 to a field of `ThemePalette` or a `k…Dip` constant.

## Themes and where colors come from

- Paint only with the palette roles. Controls never hard-code a color; they read `ThemePalette` fields, named here exactly as in `DxUi.h` (`windowBackground`, `selectionFill`, …).
- **Light** and **Dark** are the built-in palettes (`MakeDefaultThemePalette(dark)`). **High contrast** and every app theme are *derived* by `MakeThemePalette(ThemeColors)` from 11 consumer colors: background, text, selection fill/text, accent and three alert pairs. The high-contrast values here are the library's own gallery theme (black ground, white text, `#003b80` selection, yellow accent).
- **Rainbow** is a mode, not a palette: with `rainbowMode` on, Grid/Tree rows and menu selection take a hue from a stable hash of the item (saturation 0.24/0.32, value 0.97/0.34 for row tints). Never use rainbow tints to carry meaning.
- DxUi owns no theme files or keys. Applications map their own theme onto these roles, either through `ThemeColors` or by overriding palette roles; the Theming section lists which roles each path feeds and which keep library defaults. Set `dark` from the resolved colors, not the theme's base.
- Treat `accentHover` and `accentPressed` as derived: the library shifts `accent` in Oklab lightness (+8%, then −12% on light bases / +16% on dark bases). Override `accent` and recompute; don't set the variants by hand.

## Color usage rules

- Grounds: `windowBackground` for the page, `surfaceBackground` / `cardBackground` for tiles and panels, `overlayBackground` for menus and popups, `headerBackground` for grid headers, menu bars and tab strips.
- Text: `text` on every ground; `subduedText` for captions, units and accelerators; `disabledText` only for disabled controls (it is deliberately below 4.5:1).
- Selection: `selectionFill` carries `selectionText`. When a list loses focus, switch to `selectionInactiveFill`. Hover and press are translucent washes, `hoverFill` and `pressedFill`, laid over whatever is below.
- Controls: Buttons use `buttonFill` / `buttonBorder` → `buttonHotFill` → `buttonPressedFill`. Inputs use `inputFill` / `inputBorder`, and paint `focusStroke` as the border when focused.
- Primary actions: `buttonPrimaryFill` is a blend of `buttonFill` and `selectionFill` (a pale blue in light). Its caption is `buttonPrimaryText`, which is `selectionText` only when that reaches 4.5:1 — otherwise `text`. Keep this rule when adding accents.
- Status: pair `infoFill`/`infoText`, `warningFill`/`warningText`, `errorFill`/`errorText`, and always include a word ("Info", "Warn") or glyph — never color alone.
- Tooltips invert the theme: `tooltipBackground` is near-black in light and near-white in dark.
- Known gaps, kept exact from the source: the 1 DIP control outlines `border`, `buttonBorder` and `inputBorder` sit around 1.6–2:1 against their grounds. The focus ring, not the outline, is the accessible boundary. In app-derived themes, including high contrast, alert colors come from the consumer; the gallery supplies none, so this system shows them as `text` on `windowBackground`.

## Focus, states and motion

- **Keyboard focus** draws two strokes: an outer 2 DIP stroke in `focusStrokeOuter`, 2 DIP outside the control, and an inner 1 DIP stroke in `focusStrokeInner`, 1 DIP outside it. The corner radius grows by the offset. Focus chrome appears only for keyboard-originated focus, and always in high contrast.
- **Pressed** buttons shift their caption 1,1 DIP. **Disabled** controls use `buttonDisabledFill` and `disabledText` and drop the outline.
- **Durations**: hover and focus 140 ms; disclosure 240 ms; page transition 250 ms; slider interaction 100 ms and value 167 ms; tree expander 240 ms and expansion 320 ms; menu cascade delay 400 ms; tooltip delay 500 ms (display 5 s); caret blink 530 ms.
- **Easing**: FastDecelerate `cubic-bezier(0, 0, 0, 1)` for entrances, PointToPoint `cubic-bezier(0.55, 0.55, 0, 1)` for moves. Pages enter +24 DIP and leave −12 DIP.
- **Reduced motion** follows `SPI_GETCLIENTAREAANIMATION`. When it is on, every animation resolves immediately to its target.

## Typography

- Use Segoe UI Variable, with the optical family chosen by size: **Small** at 12 DIP and below, **Text** from 12 to 32 DIP, **Display** at 32 DIP and above. If Segoe UI Variable is missing, fall back to Segoe UI. Monospace is Consolas.
- Styles map 1:1 to `FontRole`. `Body` (13 DIP) is the default for every caption. Rows use `ListItem` (12), accelerators and badges use `Small` (11), group captions use `BodyStrong` (14/20), and headings use `Subtitle` (20/28), `Title` (24), `TitleLarge` (40/52) and `Display` (68/92). `Header` is deprecated.
- Line height comes from real DirectWrite metrics, not a fixed leading. Never clip or ellipsize a label to make it fit; size the control to the measured text. The gallery exercises French captions for this reason.
- Write captions in sentence case with no terminal period ("Include hidden", "Show hidden files"). Menu accelerators read like "Ctrl+Enter".

## Spacing, size and shape

- DxUi has no global spacing ramp. The recurring steps are `space-2`, `space-4`, `space-6`, `space-8`, `space-10`, `space-12`, `space-16` and `space-24`, and each token lists the control metrics it covers. Copy a control's own metric rather than rounding it to a step (5 stays 5).
- Rows are `row-height` (28). Headers, tab strips and steppers are `header-height` (32), and the MenuBar is `menubar-height` (30). Compact density scales grid rows by 0.82. For touch, raise popup rows to `touch-row` (48). Never shrink a hit target for density.
- Corners: `radius-sm` (4) is the default for buttons, fields, menus, popups and tooltips. Tabs and tags use `radius-tab` (5), Selector buttons and Modern combos use `radius-md` (6), overlay cards and toggle rows use `radius-lg` (8), and the toggle track uses `radius-pill`.
- Every outline is `stroke-hairline` (1 DIP).
- Elevation is reserved for overlays. Menus use `shadow-menu` and ComboBox popups use `shadow-popup`, with acrylic variants for the Acrylic material. Panels and cards stay flat with a `borderDefault` hairline.
- Overlay materials are Solid (default), Mica, MicaAlt and Acrylic, with backdrop blur 28, 34 and 40 respectively.

## Iconography

- Draw glyphs from **Segoe Fluent Icons**, falling back to Segoe MDL2 Assets and then to Unicode. Sizes are `Icon` (12), `IconLarge` (32) and `HeroIcon` (64).
- Glyphs in use: ChevronDown U+E70D (fallback ▾), ChevronUp U+E70E (▴), ChevronLeft U+E76B (‹), ChevronRight U+E76C (›), CheckMark U+E73E (✓), RadioBullet U+F137, Cancel/clear U+E711, RevealPassword U+E7B3. Grid treats any U+E000–U+F8FF character as an icon glyph.
- DxUi distributes **no font, icon-font, logo or image files**. The fonts are the system's own. There is no logo, so the name is set in plain type.

## Rendering and captures

- Every screenshot in **Assets › Gallery** was produced by the test harness (`gallery.ps1 -PublishDocs`), never by desktop capture. Each sheet shows all 30 controls in one theme, on a 1600 DIP canvas with 7 columns, a 24 DIP margin and a 10 DIP gap.
- The component previews in this system are static HTML renditions of the Direct2D controls. Where they differ, the gallery captures are authoritative.
- Every one of the 30 catalog controls has a guideline and a preview. A new control is not complete until it has both, alongside its `docs/controls.md` entry and gallery tile.
