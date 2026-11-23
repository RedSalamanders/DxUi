- End each embedded performance-fixture frame with a flush of the immediate context, as an application's present does
  (performance and motion plan W0.4). Without it WARP kept every frame's queued work and released surfaces in process
  heap memory, so receipts showed private bytes growing about 80-90 KB per composited frame without bound (FX6, FX13)
  and up to about 950 MB in the FX10 resize sweep; with the flush they plateau, and a hardware adapter and the native
  `WindowHost` (which presents every frame and flushes on resize) stay flat either way. The embedded contract and the
  hosting guide now say that the application submits its immediate context every frame. Every embedded fixture receipt
  is taken again; their work counters are unchanged. The IME copies scenario and the FX7 fixture now share one set of
  TSF doubles (`Embedded.Tests.TsfDoubles.h`).
