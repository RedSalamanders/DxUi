# Slider

Continuous or stepped value selection along a horizontal or vertical track.

- **Consumer supplies**: range and value; `SetOnChange(SliderChange)` with Preview, Commit and Cancel phases. Keep preview separate from durable commits.
- **Metrics**: 6 DIP track, 20 DIP gray disc, `accent` inner thumb 6 rest / 16 hover / 12 pressed; unpainted 48 DIP hit band — a press within 24 DIP of the thumb drags, otherwise seeks. Interaction 100 ms, value 167 ms; `SetValue` snaps.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
