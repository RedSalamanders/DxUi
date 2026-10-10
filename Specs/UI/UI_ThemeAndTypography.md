# Theme and typography

Status: normative intended contract
Last reviewed: 2026-09-29

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Consumers provide colors and style tokens. Shared controls support light, dark, high contrast and reduced motion.
Outside high contrast, `MakeThemePalette` uses an alert pair the consumer supplies as given. In high contrast it uses
the user's `text` on `windowBackground` for every severity, even when application alert colors were supplied. Severity
remains available through labels and icons. Slider touch feedback uses the system Highlight (`selectionFill`), and
menu descriptions use the same system foreground as their primary label, including HighlightText on selected rows.
An alert color with zero alpha, including the
zero default, was not supplied. An unsupplied color takes its tone's default from `MakeDefaultThemePalette` (for the
theme's `darkMode`), so info, warning and error stay distinct and readable; with `highContrast`, whose system palettes
have no tones, it takes `text` on `windowBackground` instead. Contrast is measured on what paints: a fill over
`windowBackground`, its text over that fill, so a translucent supplied color is judged with the ground showing through.
A text derived for a supplied fill is the tone's text, else `text`, else pure black or white, whichever measures the
higher contrast, and always keeps the pair at 4.5:1 or better: one of pure black and white reaches 4.58:1 against any
painted color, which a lightness threshold (or near-black and near-white, down to 4.42:1 against a mid tone) does not
guarantee. A ground derived for a supplied text is the tone's fill, else `windowBackground`, else black or white,
whichever shows the text best; only a supplied text too translucent to stand out on any ground stays below 4.5:1.
`ChooseContrastingTextColor` compares linear WCAG luminance and returns opaque pure black or white, whichever has
the higher contrast. Callers composite translucent fills over their actual ground before choosing text.
`Typography::IsFontFamilyAvailable` caches answers per DirectWrite factory and family. Cache misses request an
updated system font collection. `InvalidateFontFamilyAvailability(factory)` drops that factory's answers;
null drops all answers. Hosts serialize invalidation with their font-selection queries and call it when the
selection changes, never per frame. This refreshes availability without changing retained formats or layout
automatically; the host recreates affected formats through its existing font-change path.
Retain Segoe UI typography and Fluent/MDL2/Unicode icon fallback where supported. `FontRole::Icon` is 12 DIP,
`FontRole::IconLarge` is 32 DIP, and `FontRole::HeroIcon` is 64 DIP, all on Segoe Fluent Icons with MDL2 fallback.
Do not add a dependency on either
application's theme record or icon assets. Preserve the original notice for any later imported assets.

Prepare changed Unicode text/layout and glyph resources outside composition. Use real font metrics and clip to
control bounds without cutting essential ink. DPI changes rebuild appropriate resources; position-only movement
does not. Transparent-target antialiasing, alpha blending, mixed scripts and narrow bounds require visual tests.

The extracted MotionPolicy resolves immediately to the target when reduced motion is enabled and otherwise keeps
the caller's animated progress. Shared controls paint with consumer-supplied theme tokens; text shaping and control
rendering are implemented in DxUi.lib. Pending work is consumer integration, not another library extraction.
