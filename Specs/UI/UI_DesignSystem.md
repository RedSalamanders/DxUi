# Design system

Status: normative current contract
Last reviewed: 2026-09-23

[DesignSystem](../DesignSystem/README.md) is the canonical, reviewable design system of DxUi. It restates the
shipped visual language as named tokens, usage rules and one guideline and preview per catalog control, so that
consumers, reviewers and agents can design with DxUi without reading renderer code. It is published as a private
Design System artifact (https://claude.ai/artifact/Wqp8iF8K9GNF7VehTSGjLF); the files in this repository are the
source of truth and the artifact is republished from them.

## Contents

| Path under `Specs/DesignSystem` | Holds |
| --- | --- |
| `design-system.json` | The artifact index: title, namespace, asset records. Keep its `createdOnFiles` marker. |
| `tokens.json` | Colors per theme, type styles, spacing, radii, shadows, sizes and strokes. |
| `README.md` | Usage rules that name tokens: themes, color roles, focus, motion, typography, spacing, iconography. |
| `Theming.md` | How an application feeds the palette through `ThemeColors` or role overrides, and which roles keep library defaults. |
| `components/<Control>/README.md` | Guidelines: what the consumer supplies, metrics, palette roles, do and don't. |
| `components/<Control>/preview.html` | Static HTML rendition, line 1 `<!-- @dsCard group="…" height=N -->`. |
| `components/bundle.css` | The stylesheet that renders every preview from the tokens. |
| `components/Cover/preview.html` | The cover. It is not a control. |
| `assets/Gallery/README.md` | Describes the uploaded gallery captures, which are copies of `docs/gallery` PNGs. |

## Rules

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
  updates the affected tokens, README and previews in the same change, then republishes the artifact.
- Republishing copies these files to the artifact's `project/` folder: all changed files first, then the index
  last. The gallery PNGs are artifact uploads recorded in the index; re-upload them after `gallery.ps1 -PublishDocs`
  changes them and update their records.
