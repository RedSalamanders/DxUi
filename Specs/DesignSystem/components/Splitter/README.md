# Splitter

Resizable divider between two panes.

- **Consumer supplies**: orientation, pane minimums, `SetPosition`; place panes from `GetFirstPaneBounds` / `GetSecondPaneBounds` inside `SetOnChange(SplitterChange)` (preview while dragging, commit on release or keyboard, cancel on Escape or capture loss). The persisted position is consumer state.
- **Metrics**: thickness 6, minimum pane 48, keyboard step 8 (32 large), hit slop 2. Grip in `borderStrong`.

The preview is a static HTML rendition styled by `bundle.css` from the tokens; the real control is painted by DxUi.lib with Direct2D. The gallery captures under Assets › Gallery are the authoritative rendering.
