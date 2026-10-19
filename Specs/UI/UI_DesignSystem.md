# Design system

Status: normative current contract
Last reviewed: 2026-10-10

[DesignSystem](../DesignSystem/README.md) is the canonical, reviewable design system of DxUi. It restates the
shipped visual language as named tokens, usage rules and one guideline and preview per catalog control, so that
consumers, reviewers and agents can design with DxUi without reading renderer code. The design system lives under
`Specs/DesignSystem` and is committed to Git with the rest of the project. Its rendered control captures live under
[`docs/gallery`](../../docs/gallery/README.md) in the same repository.

## Contents

| Path under `Specs/DesignSystem` | Holds |
| --- | --- |
| `design-system.json` | Legacy index metadata, including historical external asset IDs. It does not define publication; the local source files and gallery captures are authoritative. |
| `tokens.json` | Colors per theme, type styles, spacing, radii, shadows, sizes and strokes. |
| `README.md` | Usage rules that name tokens: themes, color roles, focus, motion, typography, spacing, iconography. |
| `Theming.md` | How an application feeds the palette through `ThemeColors` or role overrides, and which roles keep library defaults. |
| `components/<Control>/README.md` | Guidelines: what the consumer supplies, metrics, palette roles, do and don't. |
| `components/<Control>/preview.html` | Static HTML rendition, line 1 `<!-- @dsCard group="…" height=N -->`. |
| `components/bundle.css` | The stylesheet that renders every preview from the tokens. |
| `components/Cover/preview.html` | The cover. It is not a control. |
| `assets/Gallery/README.md` | Points to the committed gallery captures under `docs/gallery`. |

## Rules

Intrinsic layout adds `FlexPanel`, `FormField` and `FormPanel` to the catalog. Their guidelines and static previews
live in the corresponding component folders; populated harness tiles cover intrinsic wrapping, required/error
presentation, two columns with separate actions, and beside labels in RTL. Forms reuse existing font, ink and chrome
roles. Descriptions/errors use Small text and a 6 DIP vertical field gap; beside labels use a 12 DIP lane gap. Column
and row gaps default to 12 DIP with zero container padding. Error meaning is textual and exposed to UI Automation;
it never depends on a new hard-coded color. Product minimum hit targets and fixed action regions remain explicit
consumer policy. See [layout usage](../../docs/layout.md) and [Controls and layout](UI_ControlsAndLayout.md).

The [final S0-S2 PR evidence](../../Measurements/CiBudget/2026-10-11-cold-build/final-ci.md) retains 18 actual
harness images across x64 Debug/Release/ASan, independently bound to the current-source functional receipts and
root visual review. The committed component previews remain static design guidance with their original provenance.
This documentation closeout changes no tokens, control geometry, rendering or gallery pixels; unfinished
performance, native ARM foreground/resource, physical-input and consumer gates stay explicit.

- Values are exact. Colors come from `ThemePalette` initializers and `MakeDefaultThemePalette`, derived values
  from the same formulas as `MakeThemePalette` and the `Resolve…VisualStyle` resolvers, metrics from `k…Dip`
  constants. Token names are the `ThemePalette` field names. Never round a metric to a scale or re-tint a
  shipped color to pass a check; record a failing contrast pair in the token's usage note instead.
- Themes are Light, Dark and High contrast. High contrast uses the gallery's `ThemeColors` input. Rainbow is a
  mode described in the README, not a palette.
- Previews are static renditions of Direct2D controls and are labelled as such. The harness captures in
  `docs/gallery` stay authoritative; a preview never replaces a gallery tile or a test baseline.
- Every catalog control MUST have `components/<Control>/README.md` and `components/<Control>/preview.html`.
  Adding, renaming or removing a control updates its folder in the same change; no folder may remain for a
  control that is not in `ControlCatalog.h`. `validate-specs.ps1` enforces this.
- A change to palette defaults, derivation, resolver colors, typography, metrics, motion or control states
  updates the affected tokens, README and previews in the same change. Commit those files with the corresponding
  implementation and documentation changes.
- Regenerate changed gallery captures with `gallery.ps1 -PublishDocs`, review them, and commit the resulting
  `docs/gallery` files with the design-system changes. No separate design-system publication is required.

The RichTextEditor M4d preview uses an explicit semantic rendering option. Its
populated gallery tile and static component preview show resolved styles and a
Unicode table. Table/code/image outlines, quote strokes and rules reuse the caret
ink role; selection keeps the existing selected-text/fill roles in every theme.
Cell padding is 6 DIP, block spacing 8 DIP and nesting indentation 16 DIP per level
within the bounded viewport. No palette token changes. Structural commands,
empty/padded source caret sites and semantic style accessibility remain pending;
the static HTML preview is design guidance, never native rendering evidence.
