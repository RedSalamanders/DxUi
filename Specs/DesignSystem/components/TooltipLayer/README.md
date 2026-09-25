# TooltipLayer

Host-managed tooltip overlay.

- **Consumer supplies**: nothing directly — use `ControlHost::SetTooltip`, `SetTooltipDelayed`, hide delay and `ClearTooltip`. Delayed behavior needs host ticks.
- **Timing**: show delay 500 ms (100–2500 allowed), display 5000 ms.
- **Metrics**: offset 14,18 from the pointer, screen margin 8, padding 10×6, width 96–280, minimum height 28, `radius-sm`. Colors invert the theme: `tooltipBackground` / `tooltipText`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
