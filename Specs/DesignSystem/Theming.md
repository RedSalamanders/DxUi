# Feeding the palette from an application theme

DxUi owns no theme files, theme keys or settings. An application resolves its own theme and hands DxUi colors through one of two adapters. Everything in this system is expressed in `ThemePalette` roles so that either path can be checked against it.

## Path A: `ThemeColors` → `MakeThemePalette`

Supply 11 colors and 4 flags; DxUi derives every other role.

| `ThemeColors` field | Becomes |
| --- | --- |
| `backgroundArgb` | `windowBackground`; the source of `surfaceBackground`, `headerBackground`, `border`, `gridLine`, `inputFill`, `buttonFill` blends |
| `textArgb` | `text`; the source of `subduedText`, `disabledText`, scrollbar and border blends |
| `selectionBackgroundArgb` / `selectionTextArgb` | `selectionFill` / `selectionText`; the source of `focusStroke`, `hoverFill`, `pressedFill`, header and button state blends |
| `accentArgb` | `accent`, then `accentHover` / `accentPressed` by Oklab lightness |
| `alertInfo*`, `alertWarning*`, `alertError*` | `infoFill`/`infoText`, `warningFill`/`warningText`, `errorFill`/`errorText`, copied as given |
| `darkMode` | selects the Light or Dark defaults for roles the derivation does not touch |
| `darkBase` | chooses every light-versus-dark blend weight in the derivation |
| `highContrast` | stronger hover/press washes, opaque inactive selection, focus strokes from `text` / `windowBackground` |
| `rainbowMode` | per-item hue tints in lists and menus |

The seven `diff*Argb` fields are accepted but **not read** by `MakeThemePalette`; no DxUi control paints a diff color. Diff views stay application-owned.

## Path B: start from `MakeDefaultThemePalette(dark)` and override roles

An application may write roles directly, as RedSalamander's main window does. Then the roles it does not write keep the library defaults of this system, whatever the application's colors are:

- `cardBackground`, `smokeOverlay`, `borderDefault`, `borderStrong`, `overlayBorder` (unless written)
- `toggleKnobFill`, `toggleKnobCheckedFill`, `pressedFill`
- `focusStrokeOuter`, `focusStrokeInner`
- `tooltipBackground`, `tooltipText`

When overriding, write every role in the same family (all grounds, all borders, all text) or derive the missing ones with the same blends as Path A. Call `RefreshAccentVariants` after changing `accent`.

## Rules for either path

- **Set `dark` and `darkBase` from the resolved colors, not from the theme's name or base.** A custom theme with dark grounds built on a light base otherwise gets light-theme knobs, tooltips, borders and blend weights. Decide by the luminance of the resolved `windowBackground`.
- **Accent**: applications normally pass the Windows accent (`DwmGetColorizationColor`). `#0078d6` is only the library default.
- **Windows high contrast**: take grounds, text and selection from `GetSysColor` (`COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_HIGHLIGHT`, `COLOR_HIGHLIGHTTEXT`, `COLOR_GRAYTEXT`) and set `highContrast`. System palettes have no alert colors, so applications commonly map all three severities to the highlight pair — which makes them identical. Status in DxUi therefore MUST always carry a word or glyph.
- **App high contrast** is a normal theme with `highContrast` set, for example the gallery's black / white / `#003b80` / yellow input used for this system's High contrast values. Its alert pairs are whatever the application supplies.
- **Rainbow** is a flag, never a palette. Keep a fixed per-application seed so hues are stable across sessions.
- **Fonts** are not theme data. Themes change colors only; type roles and the Segoe UI Variable / Fluent Icons / Consolas families stay fixed.
