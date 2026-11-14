# FX4: Menus

**FX4 has never run.** No receipt exists in this directory and no count below is measured. The fixture is built
(definition `Tests/Embedded/Embedded.Tests.PerformanceFixtureFx4.h`, runner
`Tests/Controls/Controls.Tests.PerformanceFixtureFx4.cpp`) and compiles in x64 Debug, Release and ASan Debug, but it takes
the foreground, so only an interactive run executes it (`Definition::interactiveOnly`, the developer's decision of
14 November 2026). Every ordinary run prints `FX4: interactive, not run`. Fixture framework:
[performance fixtures](../../../Specs/Testing/Testing_Validation.md#performance-fixtures); plan item `W0.4` of the
[performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).

- **Scenario**: an embedded-only application (one `EmbeddedHost` on the application's WARP device, a plain Win32 owner
  window, no native `WindowHost` on the thread) opens a 4,096-row `MenuPopup` (360 DIP viewport, first row selected),
  steps it 100 times with Down, closes it, opens and closes it again, then runs ten whole sessions of a 12-row menu.
- **Budgets proved**: `G10` (devices created per menu session: `resources.d3d11Device.created` per `sessions` step,
  with `resources.deviceContext.created`, `frame.d2dDevices`, `frame.compositionDevices` and `frame.swapChains`) and
  `G3`'s per-step work. The gates of `W2.4`, `W4.2`, `W4.3`, `W4.4` and `W5.12` read `open` (`popups.menuPopupPaints`
  per open, `popups.backdropCaptures`) and `keyboard` (effect, command-list, layer, geometry, gradient-stop and bitmap
  creations, `popups.backdropBlurPasses`, `frame.shadowPasses`, `popups.menuRowsPainted`, text layouts, captures);
  `W10.1`'s FX4 clause is structural.
- **Phases**: `warm` (2 steps, warm-up: the application's first session), `open` (1), `keyboard` (100), `close` (1),
  `cycle` (2: open, close), `sessions` (10). Popups take the DPI of the primary monitor, which the run prints.
- **Adapter**: the popups render on the thread's shared native device stack (hardware first, WARP fallback); the receipt
  names the default adapter.

## Interactive run sheet

Nothing here may run unless the person at the desktop has agreed to the time. Both commands take the foreground window,
keyboard focus and mouse capture through the interactive desktop lease (`DxUi.InteractiveLease.exe`), which asks first
(default Cancel; `-InteractiveAutoStart` only after explicit agreement to automatic start), shows a warning while it
runs, and restores the person's window, focus and pointer however the run ends. They refuse in a CI job, without an
interactive window station, without the lease executable and without an unlocked desktop.

1. **Determinism gate** (two consecutive runs, equal counters in every measured phase):

   ```powershell
   ./test.ps1 -Configuration Release -Platform x64 -Interactive -Suites Menu -Tests TestPerformanceFixtureFx4IsDeterministic
   ```

   Estimated duration: the usual `test.ps1` preamble (tooling tests, build, harness and lease self-tests, the benchmark:
   several minutes, nothing taken), then about one to two minutes under the lease for two FX4 runs. These are estimates:
   FX4 has no measured duration yet. In Debug or ASan Debug expect longer, since each open lays out 4,096 rows.

2. **Receipt** (one run, written here only when valid):

   ```powershell
   ./performance-fixtures.ps1 -Fixture FX4 -Interactive
   ```

   Estimated duration: the build, a 3-second host-idle observation, then about one minute under the lease (the lease is
   told 120 s). The receipt is `fx4-default-window-interactive-x64-release.json`; regenerating this README keeps this run
   sheet below the receipt summary.

**What the person sees.** The lease's confirmation dialog and its warning; a small borderless window titled "DxUi FX4
embedded-only application" near the top of the primary monitor, in the half the pointer is not in; a menu of rows
"Row 0001" onwards opening below it, its selection moving down 100 rows and scrolling, closing, opening and closing again;
then ten short menus ("Session item 0001" onwards) opening and closing. Screen pixels behind each menu are captured for
its blurred backdrop.

**What must not be touched meanwhile.** Neither the mouse nor the keyboard, from Start until the lease reports that it
has restored the desktop. The menus hold mouse capture and keyboard focus: any pointer movement over a menu or any key
press reaches it. Before starting, leave the pointer away from the top of the primary monitor on either side (the
fixture opens its menus in the half of the work area that does not hold the pointer and never moves the pointer). Do not
lock the screen or switch desktops.

**A valid run versus an invalidated one.** A valid determinism run ends with the test's `[DONE]` line, a line
`FX4 (interactive): popup DPI ...` summarizing renders per open, rows per keyboard step and devices per session, and a
passing `test.ps1`. A valid receipt run prints the same summary line and `Performance fixture receipt: ...fx4-...json`.
An invalidated run fails and writes no receipt, naming the reason:

- `blocked activation: the application window could not take the foreground (stage ...)`, or `blocked activation: the
  popup window is 1 by 1 pixels`, `... is not its layout rectangle`, `... is not the foreground window`, `... does not
  hold keyboard focus`, `... does not hold mouse capture`: the foreground hand-off was blocked (no lease, another
  application holding the foreground, `--no-activate`). The testing contract warns that a blocked activation path can
  leave a temporary 1-by-1 popup despite correct internal layout; such counts are never recorded.
- `the pointer reached the menu`, `input other than the fixture's reached the menu`, `the menu was dismissed ...`,
  `the menu lost the foreground ...`: the person's input or another application interfered. Run again without touching
  anything.
- `the pointer rests where the menu opened`: move the pointer to the other half of the primary monitor and run again.

A nondeterministic counter between the two runs fails the gate with the phase, step and counter; it is a defect of the
fixture or the library, never a reason to drop the counter.
