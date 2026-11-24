- An embedded view now repaints only what changed. Invalidating a control (`Assign` with `Affects::Paint`, `Invalidate`,
  `RequestInvalidate`, `InvalidateMeasure`), moving it or changing hover adds the control's painted extent, its bounds plus
  how far it paints outside them, to a paint region of at most eight device-pixel rectangles, and `EmbeddedHost::Prepare`
  clears and repaints only those rectangles. Host-level changes, tree changes and anything it cannot attribute still
  repaint the whole surface, as does text that newly escapes its control. Panels skip children whose extent misses the
  area being painted, on both hosts, and a `ScrollPanel` skips a viewport clip that would change nothing. On FX5 a hover
  change paints the two toggles, the sliders just below them, their cards and the root, not 83 controls; a scrolled 10,000-control form paints only the
  rows in view. The window host still paints whole frames (presenting the region is plan item `W4.9`). The prepare-end
  trace event and the `frame.pixelsPainted` counter report the repainted area. No raster tile is built.
  - New public members, additive in API 6: `struct PaintOverhang`, `Control::GetPaintOverhang` (a protected virtual: the
    farthest a control paints outside its bounds in any state: 3 DIP for library controls, and what a
    `PageIndicator` with more pages than fit, a short `Slider` and a `ColorPicker` smaller than its parts draw past their
    bounds) and `Control::DeclarePaintOverhang` (a class outside the library declares that its `Paint` stays within it).
    Controls derived outside the library repaint the whole view and are never skipped until their own constructor
    declares, and nothing below an undeclared control is skipped, so they paint exactly as before, even when they draw their
    children into a command list or another target. Plan item `W4.10`; the receipt is `Measurements/PerformanceAndMotion_2026-11-13/W4.10/`.
