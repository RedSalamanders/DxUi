- Built performance fixtures FX9 (large-list scroll), FX10 (live resize) and FX12 (dense screen) on the fixture framework
  (performance and motion plan W0.4), with WARP baseline receipts under `Measurements/PerformanceAndMotion_2026-11-13`.
  FX9 scrolls a Grid over a synthetic 100,000-row or 1,000,000-row model and a 10,000-row Tree for 10 s of the fixture
  clock at 120 Hz; FX10 sweeps a FlexPanel, FormPanel, Grid and RichTextEditor screen from 800x600 to 3840x2160 in 30
  steps; FX12 records full frames, one hover change, one label change and scrolling on 2,000 controls (also at 3840x2160
  and 192 DPI), a 10,000-control form and 120 distinct text-format keys. A fixture variant can now be declared
  receipt-only: receipts and timing run it, the ordinary suites' determinism gate leaves it out. A new private work
  counter, `text.formatCacheTrims`, counts each clear of the host's text-format cache above its key bound.
