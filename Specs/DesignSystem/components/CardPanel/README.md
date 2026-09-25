# CardPanel

A Panel with themed card chrome.

- **Consumer supplies**: children with bounds in host DIPs, including the card's origin.
- **Chrome**: `cardBackground` with 1 DIP `borderDefault`; 4 DIP radius inline, 8 DIP as an overlay card with `shadow-menu`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
