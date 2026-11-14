- Build performance fixtures FX1 (idle carets), FX2 (ComboBox popup and typing), FX3 (Grid scrolling, resizing, keys,
  grouping and select-all) and FX14 (tooltip path) on the fixture framework (performance and motion plan W0.4), with
  their WARP x64 Release baseline receipts under `Measurements/PerformanceAndMotion_2026-11-13/`. FX1 and FX14 run on the
  embedded and window hosts; FX2 (whose popup is a native window on a WindowHost) and FX3 (too slow on a window for the
  suites) on the embedded host. Time is the frozen fixture clock: FX1 simulates 10 s per field at one tick per 100 ms,
  which the receipts say. FX4 (menus) stays declared: a MenuPopup session needs the interactive desktop. Variants gain `receiptOnly` for full-size scenes
  the suites skip (FX3's 200,000-row, 10,000-group grouping); the tooltip hover path and `LargeGridModel` moved into
  shared headers so the fixtures reuse them. No library code changed.