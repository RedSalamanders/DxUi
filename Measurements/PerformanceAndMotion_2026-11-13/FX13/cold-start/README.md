# FX13 cold start: launch to first presented frame

Receipt-only real-time record of plan item `W0.4` (`FX13`, whole application) of the
[performance and motion plan](../../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md). It is not part of the
fixture definition and not part of the determinism gate; its numbers are wall-clock time of fresh processes on a shared
machine and are **not a timing verdict**.

- **Scenario**: the `FX13` application (`BuildApplication` in
  [the FX13 fixture](../../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx13.h): 34 controls) built in a fresh
  `DxUi.ControlTests.exe` child process on a WARP `WindowHost` (`HwndSwapChain`) with a 1920x1080 client area, shown
  without activation, off every monitor, at the bottom of the z-order; reduced motion.
- **Method**
  ([source](../../../../Tests/Controls/Controls.Tests.PerformanceFixtureColdStart.cpp)): the parent reads
  `QueryPerformanceCounter` just before `CreateProcess`; the child reads it when its command starts (after the test
  runner installs its failure host) and takes the timestamp of its first successful `Present` from the DxUi trace
  provider's test sink. The child then reads the presented image through the compositor (`PrintWindow`,
  `Support.Tests.PresentedWindowCapture.h`) and fails unless it shows a drawn frame. Nothing takes focus or the pointer.
- **Command**: `./.build/x64/Release/DxUi.ControlTests.exe --fx13-cold-start Measurements/PerformanceAndMotion_2026-11-13/FX13/cold-start/fx13-cold-start-window-warp-x64-release.json 5`
- **Source**: commit `b2f3a218c2498b6c83caf8b1c274f42ebda3137c` (the W0.4 FX6/FX7/FX13 change before its receipts were
  added), x64 Release, compiler 195237025, WARP, Windows build 26300, taken on 14 November 2026 while other builds ran on
  the machine (the FX13 receipt's host-idle record for the same session: 28.8% busy).

## Receipt

[fx13-cold-start-window-warp-x64-release.json](fx13-cold-start-window-warp-x64-release.json): five runs, launch to
first present 141.5-203.0 ms (median 152.7 ms), of which launch to the child's command 17.9-72.8 ms (median 19.1 ms). Under load; repeat in a
quiet window before quoting it.

The hardware variant and the minimized video-memory residency (`QueryVideoMemoryInfo`) are declared, not run: they need
the hardware adapter (`./performance-fixtures.ps1 -Fixture FX13 -Variant minimized-hardware -Adapter Hardware`).
