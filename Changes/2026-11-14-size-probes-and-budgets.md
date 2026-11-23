- Add size probes and G13 memory budget lines (performance and motion plan W9.1). Compile-time probes now also hold
  `ThroughputGraphSample` (408 bytes) and the UI Automation record `AccessibilityControlNavigationSnapshot` (936 bytes in
  Release, 1,088 with Debug STL) at their measured sizes, which x64 and ARM64 share. The retained-cache registry gives
  each cache a resident byte count (container storage by capacity, heap strings, bitmap pixels; never DirectWrite or
  Direct2D object storage), and `EmbeddedStatistics` gains `retainedCacheEntries`, `retainedCacheBytes` and
  `largestTextHistoryBytes`, counted by `GetStatistics` in its existing allocation-free walk, with the
  `DXUI_HAS_RETAINED_CACHE_STATISTICS` header capability. Every performance fixture receipt lists the registered caches
  and reports each G13 line (idle private bytes, minimized video memory, bytes per control, bytes per Grid cell, text
  history per editor) as a counter beside its limit; a Grid cell probe hides and shows each Grid and measures, from the
  process heaps, what rebuilding its cell layouts adds (about 21 KB a cell on a hardware adapter; WARP's own heap
  traffic leaves that line unmeasured there) and what hiding returns (285 bytes a cell). FX5 records two lines over budget as named exceptions: about 640 bytes per control (W9.6) and the
  bytes each single-line Grid cell keeps (W9.4); minimized video memory is measured on hardware only (W9.2). Private
  bytes stay advisory.
