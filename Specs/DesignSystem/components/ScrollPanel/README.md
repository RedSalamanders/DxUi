# ScrollPanel

Clipping container with an internal scrollbar.

- **Consumer supplies**: a content tree, the content extent, and optionally `SetOnScrollChanged`. Pointer coordinates are translated into content space.
- **Scrollbar**: thickness 12, minimum thumb 20, inset 2, thumb radius 4; `scrollbarTrack`, `scrollbarThumb`, `scrollbarThumbHot`; 140 ms fades.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
