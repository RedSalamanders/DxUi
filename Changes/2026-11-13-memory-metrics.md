- Add memory metrics (performance and motion plan W0.3). `EmbeddedStatistics` gains `retainedTextLayouts`,
  `retainedTextLayoutUnits`, `retainedBitmaps` and `retainedBitmapBytes`, counted by `GetStatistics` through a
  read-only, allocation-free walk (never in Composite), with the `DXUI_HAS_RETAINED_RESOURCE_STATISTICS` header
  capability. A separate FX5 memory receipt (`dxui-complex-ui-memory-v1`), written by every Embedded run and on demand by
  `--memory-receipt` or `--memory-receipt-hardware`, records live tracked C++ bytes after scene build, warm-up and
  scrolling, retained resources per phase, `QueryVideoMemoryInfo` local and non-local usage on hardware (not applicable
  on WARP), hidden residency deltas and control object sizes; the timed benchmark and its paired inputs are unchanged.
  Compile-time probes hold `Control`, `Label`, `Button`, `Toggle`, `Checkbox` and `RadioButton` at their measured
  sizes. An Embedded suite test proves that a second FX5 lifetime returns every tracked byte and, in Debug on a D3D11
  debug-layer device, every DxUi-created device object. RichTextEditor text layouts are not yet counted; per-object
  DirectWrite and Direct2D bytes are not observable.
