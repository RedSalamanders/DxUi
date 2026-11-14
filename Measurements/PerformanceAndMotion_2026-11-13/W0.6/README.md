# W0.6 presented-frame truth

Plan item `W0.6` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
the observation is recorded in [the Win32 host contract](../../../Specs/Rendering/Rendering_Win32Host.md).

## Scenario

`TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnWarp` and
`TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnHardware` (WindowHost suite,
`Tests/Controls/Controls.Tests.PresentedFrame.h`). Each case runs on a fresh thread (a clean native graphics pool), once
per presentation mode, in a 192 x 128 device-pixel client area of a per-monitor-aware window shown without activation,
off-screen and at the bottom of the z-order. The painter fills solid, aliased, pixel-aligned rectangles, one color per
region and step: rectangle A = (16,16)-(112,80), rectangle B = (80,48)-(176,112) overlapping it, a "neither" region, and
an unpainted strip x >= 184 (window background in `HwndSwapChain`, transparent over the capture's backdrop
`RGB(240,128,0)` in `CompositionSwapChain`).

1. Full frame (A0, B0); its presented image must equal the full render exactly (capture calibration).
2. A changes to A1; `DebugRenderDirtyRectForTest(A)` presents only A with `Present1`; presented image current everywhere.
3. B changes to B2; `DebugRenderDirtyRectForTest(B, prePresentBackBuffer)` presents only B and reads the back buffer
   before `Present`. Gate: the presented image after B has 0 BGRA differences from a full render of the same state.
4. Negative control: A returns to A0 and B changes to B3, but only B is presented; the presented image must differ from
   the model in exactly the 5,120 A-only pixels (it shows what was presented, not the model).

The capture never renders the window (the host's render count is unchanged across every read).

## Result (x64, source: this item's commit on base `ac6db736`)

| Adapter | Driver | Mode | Presented after B vs full render | Back buffer before B's Present vs full render | Negative control |
| --- | --- | --- | --- | --- | --- |
| WARP (Microsoft Basic Render Driver, 0x1414:0x008C) | 10.0.26100.9711 | HwndSwapChain | 0 | 5,120 (all A-only: A0) | 5,120 (all A-only) |
| WARP | 10.0.26100.9711 | CompositionSwapChain | 0 | 5,120 (all A-only: A0) | 5,120 (all A-only) |
| NVIDIA GeForce RTX 5080 (0x10DE:0x2C02) | 32.0.16.1714 | HwndSwapChain | 0 | 5,120 (all A-only: A0) | 5,120 (all A-only) |
| NVIDIA GeForce RTX 5080 | 32.0.16.1714 | CompositionSwapChain | 0 | 5,120 (all A-only: A0) | 5,120 (all A-only) |

Identical in x64 Debug, Release and ASan Debug (logs below). Windows 11 Pro 10.0.26300, display 3840 x 2160 at 120 Hz,
window DPI 144. The hardware case uses the library's default adapter, which was the RTX 5080; the machine's AMD
integrated GPU was not exercised because `ControlHost` exposes no adapter choice. The plan's claim holds: `Present1`
keeps the presented frame current outside the dirty rectangle, and only a pre-present reader sees the older frame.

## Capture method

`Tests/Support/Support.Tests.PresentedWindowCapture.h`: `DwmFlush`, then `PrintWindow(PW_CLIENTONLY |
PW_RENDERFULLCONTENT)` into a 32-bpp DIB, in the window's own DPI context, waiting one compositor frame per attempt
until a pixel only the expected frame paints appears (30 frames at most; no sleeps). PrintWindow also sends the window
`WM_PAINT`; a host would render and present a new full frame in response, replacing the frame being read, so the
capture answers `WM_PAINT`/`WM_ERASEBKGND` itself for the duration of the call (painting the backdrop color into the
print) and refuses a window with a pending update region. Windows.Graphics.Capture was not chosen: it needs a WinRT
frame pool, a borderless-capture permission and frame-arrival handling for the same result, and offers no deterministic
"this present is shown" signal either.

## Files

- `x64-<Configuration>/<adapter>-<mode>-presented-0-full.png`, `-presented-1-after-a.png`, `-presented-2-after-b.png`,
  `-presented-3-negative-control.png`: compositor images.
- `-backbuffer-before-present-b.png`: the back buffer read before B's `Present` (composed over the backdrop).
- `-full-render-final.png`: the full render of the final state.
- `run-x64-<Configuration>.log`: the test output with adapter identity and difference counts (teardown trace lines
  removed).

Captures and logs come from `DxUi.ControlTests.exe --suite=WindowHost --test=<both tests>` built by MSBuild from the
working tree of this item (Debug, Release and ASan Debug, x64), before the commit; the `test.ps1` runs reported with
the item reproduce the same lines.
