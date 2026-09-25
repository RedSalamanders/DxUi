# ThroughputGraph

Live history chart of timestamped samples, with an optional band, secondary series, limit and current-value marker.

- **Consumer supplies**: `SetSamples`, optional `SetSecondarySamples` / `SetSecondarySeriesColor`, `SetLimit`, `SetCurrentValueMarker`, `SetOverlayText`, `SetRainbowMode`, `SetPerStreamBands`, `SetTransitionDuration` (default 160 ms). Keep history within the configured limits.
- **Colors**: ground `surfaceBackground`; three `gridLine` rules at 0.75 DIP; 2 DIP series in `accent` (rainbow hues per stream outside high contrast); band `accent` at 18% light / 22% dark / 32% high contrast; secondary 1.5 DIP `warningText` by default; limit 1 DIP `disabledText`; labels `text`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
