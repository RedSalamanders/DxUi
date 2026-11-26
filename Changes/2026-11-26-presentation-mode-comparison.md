- Added the presentation-mode comparison measurement (performance and motion plan item W0.9, [tests guide](../Tests/README.md)): `FX5`
  and `FX1`'s text-heavy page run on a `WindowHost` in `HwndSwapChain` and in `CompositionSwapChain`, on WARP and on the default hardware
  adapter, through `DxUi.ControlTests.exe --presentation-modes` and `./performance-presentation.ps1`, which writes
  `Measurements/PerformanceAndMotion_2026-11-13/W0.9/presentation-mode.json` and checks it for all four parts (pixels read from the
  presented image, cost, behaviour, scope) on both adapters. It records differences and recommends no default. The measurement adds
  no product behaviour: `ComplexUiScene` gained `BuildRoot` and `Apply` (the embedded scene is unchanged), the window fixture runner
  gained a public session over its one `WindowInstance` in a chosen mode, and `FX5` has a window form that only the comparison uses.
  A monitor move that changes DPI and a remote session cannot be driven from the test program and are listed as not run with manual
  steps; the DPI case is the host's `WM_DPICHANGED` seam, labelled a simulation. Timing is recorded as not judged on a busy machine; the
  receipt names the command that judges it on a quiet one. Validated by `Tools/tests/Test-PresentationModes.ps1` and the WindowHost
  suite's `TestPresentationMode*` tests on WARP (the hardware case is a capability skip without a hardware adapter).
