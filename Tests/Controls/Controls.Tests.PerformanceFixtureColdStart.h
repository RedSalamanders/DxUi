#pragma once
// FX13 cold start (plan W0.4): the receipt-only real-time measurement of a fresh process from launch to its first presented
// frame. Not part of the fixture definition and never part of the determinism gate: it is wall-clock time on a shared
// machine. Tests/README.md, "Performance fixtures".

// DxUi.ControlTests.exe --fx13-cold-start <output.json> [runs]: launches `runs` (default 5) fresh child processes of this
// executable, each building the FX13 application on a non-activating, off-screen WindowHost (WARP), and writes their
// launch-to-first-present times. Returns 0 on success, 2 on bad arguments, 1 when a child fails.
int RunFx13ColdStartCommand(int argc, wchar_t** argv);
// The child side: -1 when the arguments are not its own (--fx13-cold-start-child <output>), otherwise its exit code.
int RunFx13ColdStartChild(int argc, wchar_t** argv);
