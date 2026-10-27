# ProgressBar

Linear progress, determinate or indeterminate.

- **Consumer supplies**: minimum, maximum and value; `SetIndeterminate(true)` requires host animation ticks unless reduced motion or explicit animation pause is on.
- **Metrics**: 2 DIP track, 4 DIP when indeterminate; the indeterminate segment is 40% of the width.
- **Motion**: the indeterminate segment sweeps across the track every 2 s. Reduced motion rests it centered, from 30% to 70% of the track, and requests no ticks; the gallery sheets show this resting state.
- **Colors** (`ResolveProgressBarVisualStyle`): fill `selectionFill`; track = Blend(`windowBackground`, `inputFill`, 0.14 light · 0.20 dark). In the built-in light theme both are white, so the track is invisible and only the fill reads — keep a caption or percentage beside it.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.

`SetAnimationPaused(true)` holds the indeterminate segment at its current phase without requesting ticks. The control stays enabled and indeterminate. Resume retains that phase and starts a new animation clock. Keep a localized status caption beside a paused bar. Reduced motion still uses the centered resting segment. The existing gallery uses reduced motion, so this adds no change to its static pixels or default appearance.
