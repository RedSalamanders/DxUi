# ProgressBar

Linear progress, determinate or indeterminate.

- **Consumer supplies**: minimum, maximum and value; `SetIndeterminate(true)` requires host animation ticks.
- **Metrics**: 2 DIP track, 4 DIP when indeterminate; the indeterminate segment is 40% of the width.
- **Colors** (`ResolveProgressBarVisualStyle`): fill `selectionFill`; track = Blend(`windowBackground`, `inputFill`, 0.14 light · 0.20 dark). In the built-in light theme both are white, so the track is invisible and only the fill reads — keep a caption or percentage beside it.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
