# Theme and typography

Tree inactive selection MUST prefer the theme's `selectionText` token on the
composited `selectionInactiveFill`, consistent with Grid. The shared contrast
fallback still applies; high contrast preserves the supplied HighlightText pair.
Selected row icons and expanders follow that resolved text color.

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
Configured `ControlHost::GetTextFormat(role, size, weight, alignment, paragraph, wrap, reading)` requests share the
configured role-format cache key when the size, after 1/64-DIP normalization, and weight equal `Typography::GetSpec(role)`.
Alignment, paragraph alignment, wrapping and reading direction remain in the key. Other normalized sizes and weights
remain separate. Icon, HeroIcon and IconLarge use the role-default key only when the Fluent icon family is available;
without it, the default Small fallback and explicit custom-format creation/retry paths remain intact. This alias does not
change the bounded cache capacity or its owner-paint/preparation eviction timing.
For `Label` typography overrides, `characterSpacingDip == 0` uses DirectWrite's default zero character spacing.
Measurement and painting leave a new or retained text layout at that default instead of querying
`IDWriteTextLayout1` and applying an explicit zero mutation. Nonzero spacing keeps the existing query, mutation and
failure handling. This does not replace or invalidate retained layouts, and zero spacing does not alter layout-cache
identity.
Do not add a dependency on either
application's theme record or icon assets. Preserve the original notice for any later imported assets.

Prepare changed Unicode text/layout and glyph resources outside composition. Use real font metrics and clip to
control bounds without cutting essential ink. DPI changes rebuild appropriate resources; position-only movement
does not. Transparent-target antialiasing, alpha blending, mixed scripts and narrow bounds require visual tests.

RichTextEditor may override inherited body/code family and size, font-relative
line height and heading scale through its public appearance value. These values
are part of layout reuse identity and UIA font attributes. Opt-in accent links
use the supplied theme accent, including the high-contrast palette; focus dimming
uses subdued text and is disabled in high contrast. Consumer policy chooses
whether normal-theme accent/link-to-text contrast calls for persistent cues.

Optional `paragraphSpacingAfterLines` (finite 0..4) and `listItemSpacingDip`
(finite 0..256) configure semantic minimum spacing. Both absent preserve the
legacy 8-DIP gap after every box, including source-only empty caret boxes. Either
override engages collapsed minimum-gap placement: an unset paragraph option is
an 8-DIP minimum, while actual intervening blank-source line-box heights contribute
to the requested gap. One separator must not stack another full paragraph
margin; additional source blank lines keep their full, distinct geometry. A
nominal body line uses body font/line metrics, excluding larger headings and
inline objects. List minima apply at new-item transitions, including nested
entry and following outer siblings; continuation paragraphs retain paragraph
spacing. Table row/cell padding is unchanged. Values are validated and included
in preparation/reuse identity; Source/v0 placement, exact ACPs, source selection,
empty/container/trailing caret sites and undo/redo remain unchanged. No consumer
may implement these gaps by rewriting source or displacing local glyphs.

Spacing is measured between complete prepared content frames. A DirectWrite
hit-test caret can be inset and shorter than its line box; its bottom is not
the paragraph bottom. Geometry oracles use complete frames or equal-font
caret-top advances against an independently specified line advance. Public
semantic bounds are viewport-clipped, so frame-gap assertions must also prove
that the queried frames are fully visible.

The extracted MotionPolicy resolves immediately to the target when reduced motion is enabled and otherwise keeps
the caller's animated progress. Shared controls paint with consumer-supplied theme tokens; text shaping and control
rendering are implemented in DxUi.lib. Pending work is consumer integration, not another library extraction.
