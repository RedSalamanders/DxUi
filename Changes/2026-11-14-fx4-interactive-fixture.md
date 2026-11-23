- Built performance fixture `FX4` (menus, plan W0.4) as the first interactive-only fixture: a 4,096-row `MenuPopup`
  opened, stepped 100 times with the keyboard and closed, an open/close cycle and ten menu sessions in an embedded-only
  application, with its own runner in `DxUi.ControlTests.exe`. `Definition::interactiveOnly` marks a fixture that takes
  the foreground: both determinism gates, both receipt commands, `performance-fixtures.ps1` without `-Interactive` and
  the timing harness skip it with `FX4: interactive, not run`, and the catalog JSON exposes the flag. Its determinism
  gate is `TestPerformanceFixtureFx4IsDeterministic` in the interactive `Menu` suite and its receipt is
  `performance-fixtures.ps1 -Fixture FX4 -Interactive`, a child of the interactive desktop lease that refuses without
  it; a blocked activation or input reaching the menu fails the run without a receipt. New work counters
  `frame.shadowPasses`, `frame.d2dDevices`, `frame.compositionDevices`, `frame.swapChains`,
  `popups.backdropBlurPasses`, `popups.menuPopupPaints` and `popups.menuRowsPainted`; no behaviour changes. FX4 has
  not run yet: its run sheet is in `Measurements/PerformanceAndMotion_2026-11-13/FX4/README.md`.
