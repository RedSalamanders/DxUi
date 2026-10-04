# Slider

Continuous or stepped value selection along a horizontal or vertical track.

- **Consumer supplies**: range and value; `SetOnChange(SliderChange)` with Preview, Commit and Cancel phases. Keep preview separate from durable commits. An embedded host passes each contact's `PointerEvent::device`.
- **Metrics**: 6 DIP track, 24 DIP gray disc, `accent` inner thumb 14 rest / 20 hover / 16 pressed; unpainted 48 DIP hit band — a press within 24 DIP of the thumb drags, otherwise seeks. A touch drag adds a 48 DIP halo, `accent` at 24% (an opaque 2 DIP ring in high contrast); the mouse and pen never show it. Interaction 100 ms, value 167 ms; `SetValue` snaps.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
