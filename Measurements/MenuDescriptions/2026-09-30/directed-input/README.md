# Directed input restoration on main, 30 September 2026

The directed-input qualification the [described-menu plan](../../../../Specs/Plans/WIP/MenuDescriptions_2026-09-21.md)
requires. Menu and NativeTextInput run in x64 Debug, Release and ASan Debug, each under the retained V4 wrapper. The
wrapper holds RedSalamander's desktop lease and shows its input warning. It activates only its own bootstrap window,
grants the foreground only to the test process it starts, and then checks that the person's foreground window,
focus and cursor come back. All six runs pass with zero skips and full restoration:

| Configuration | Menu | NativeTextInput |
|---|---|---|
| Debug | exit 0, 0 skips, restored | exit 0, 0 skips, restored |
| Release | exit 0, 0 skips, restored | exit 0, 0 skips, restored |
| ASan Debug | exit 0, 0 skips, restored | exit 0, 0 skips, restored |

"Restored" is the wrapper's line
`Restoration: childFocus=1 priorTargetPresent=1 priorTargetValid=1 foreground=1 cursorSaved=1 cursorRestored=1`, so
every run returned the focused child window, the foreground window and the exact cursor position. The Menu logs also
show the suite's own pointer restoration (`Menu pointer restoration: restored=1 exact=1`).

- Source: `improve/menu-descriptions-closeout` at `e6cda48`, which is main `a0b4934` plus the menu-closing test fix
  below. The executables' SHA-256 are in `receipts.txt`: Debug `B19D987284…`, Release `47E036ED8F…`, ASan Debug
  `3A050D97AB…`.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, the developer's interactive desktop.
- Runner: `run-directed-input-v4.ps1.txt` is the retained V4 runner of the
  [grid's interactive packet](../../../GridTextOverflow/2026-09-21/qualification/interactive/README.md) with this
  machine's paths. Like V4, it stops at the first run that fails, skips or does not restore.
- Wrapper: `dxui-interactive-wrapper.cpp.txt` is V4's retained source with its one include pointed at
  `DirectedSelfTestInputWarning.h.txt`. That is RedSalamander's header as of `8ab1d4781`, the one version that has the
  `TryActivateOwnedWindow` helper the wrapper calls; the header on RedSalamander's master no longer has it. The
  wrapper was compiled as V4 compiled it (`cl /std:c++latest /EHsc /W4 /WX /utf-8`, wil from the DxUi vcpkg tree).
  `receipts.txt` records the wrapper, source and header hashes, and these copies are byte-exact.

## The first attempt

`attempt1-closing-window-covered` keeps the first run, which the runner stopped after Debug Menu. That run passed
and restored everything, but recorded one skip: `TestMenuSurvivesAWindowClosingItFromSetCursor`, "another window
covers the menu-closing window". That test had skipped in every run since PR 29 added it, local and x64 CI alike.
Named, the cover was the desktop application's own window (`claude.exe`). It takes the foreground back while the
suite runs, which puts it above the test's non-topmost window. `e6cda48` makes that window topmost and names any cover
in the skip. The second attempt, above, ran everything.

## Limits

These runs establish restoration of foreground, focus and cursor on one interactive desktop. They do not establish
native ARM64, a real screen reader or IME session, physical mixed-DPI, or performance acceptance. The consumer's
assistive-technology qualification stays with RedSalamander's File Operations plan.
