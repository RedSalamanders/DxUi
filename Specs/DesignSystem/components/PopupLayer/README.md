# PopupLayer

Panel that paints and hit-tests its children above the rest of the tree.

- **Consumer supplies**: real overlay children, visibility and lifetime through the owning host.
- **Chrome**: none of its own; modal content usually sits on `smokeOverlay` inside an overlay **CardPanel** (`radius-lg`, `shadow-menu`).

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
