#pragma once

// Presentation-mode comparison (performance and motion plan item W0.9; Tests/README.md, "Presentation-mode comparison"): FX5 and
// a text-heavy page (FX1's multiline page) on a WindowHost in HwndSwapChain and in CompositionSwapChain, on WARP and on the default
// hardware adapter. One process runs one fixture on one adapter in both modes and writes one case document; the script
// performance-presentation.ps1 runs every case and merges them into Measurements/.../W0.9/presentation-mode.json.
// Pixels are read from the presented image (Support.Tests.PresentedWindowCapture.h), never from the back buffer alone.
// Counters and read-back decide; timing is recorded and advises.

// DxUi.ControlTests.exe --presentation-modes <output.json> <FX5|FX1> Warp|Hardware hwnd-first|composition-first <timingFrames>
// <judged 0|1>: one case document. Exit code 0 on success, 3 when the adapter is missing (a hardware case on a machine without a
// hardware adapter; nothing is written), 2 on a malformed command line.
int RunPresentationModeCommand(int argc, wchar_t** argv);

// Window-free tests of the analysis (alpha recovery from two backdrops, image comparison classes) and of the scope anchors.
void TestPresentationModeAlphaRecoveryIsExact();
void TestPresentationModeImageComparisonClassifiesPixels();
void TestPresentationModeScopeAnchorsResolve();
// FX5 and the text page on WARP in both modes: the pixel, cost and behaviour parts are present, the capture calibrates against
// the back buffer, counters agree between two runs in opposite orders. Capability skip without an interactive desktop.
void TestPresentationModeComparisonOnWarp();
// The same on the default hardware adapter; a capability skip without one.
void TestPresentationModeComparisonOnHardware();
