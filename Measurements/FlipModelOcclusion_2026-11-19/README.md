# Flip-model occlusion status, 19 November 2026

Question (RedHome register row H-20, convergence step CV-6-08): does `IDXGISwapChain::Present` return
`DXGI_STATUS_OCCLUDED` for a flip-model swap chain on the supported Windows 11 baseline? The
[window-hosting contract](../../Specs/Rendering/Rendering_Win32Host.md) says it does not; the RedXe and RedPrism
renderers act on that status from their flip-model `Present`.

## Result

On Windows 11 25H2 (build 26200.9911), with an AMD Radeon 860M (driver 32.0.22024.19001), a flip-model swap chain
returned `S_OK` from `Present(1, 0)` and from `Present(0, DXGI_PRESENT_TEST)` in every window state: shown, minimized,
restored, cloaked and hidden, for both `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` and `DXGI_SWAP_EFFECT_FLIP_DISCARD`. It never
returned `DXGI_STATUS_OCCLUDED`, in 30 presents per state. The contract's statement holds on this baseline.

The control, a blt-model `DXGI_SWAP_EFFECT_DISCARD` swap chain on the same kind of window, returned
`DXGI_STATUS_OCCLUDED` from `Present(1, 0)` in every shown state and from `Present(0, DXGI_PRESENT_TEST)` while
minimized, so the probe observes the status whenever DXGI returns it. (That the blt-model `Present(1, 0)` reports
occlusion while the window is shown is not explained here; it does not affect the flip-model result.)

So a flip-model host cannot learn occlusion from `Present`. It needs an application-owned signal: minimize and
restore through `WM_SIZE`, visibility, cloaking, display power or the session state, as this contract requires
(`ControlHost::SetNativePresentationVisible`).

## Method

[`raw/OcclusionProbe.cpp.txt`](raw/OcclusionProbe.cpp.txt) (SHA-256
`d8e2cd75ef2501dd231f08375ac856eaa8dc4afad26667a0b733b4db30aef511`) is a standalone console probe built with MSVC
14.53 (`cl /EHsc /O2 /W4`); the executable's SHA-256 was
`65b50884048720283ef8b5fc5472530ba38b2ab7e6b3d822d9f645b905f92300`. For each swap effect it creates a 64 × 64
`WS_POPUP` window with `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW` in the corner of the work area, a hardware Direct3D 11
device and a two-buffer swap chain (one buffer for the blt control) for that window. It then moves the window through
five states: `SW_SHOWNOACTIVATE`, `SW_SHOWMINNOACTIVE`, `SW_SHOWNOACTIVATE` again, `DWMWA_CLOAK` set, and `SW_HIDE`.
In each state it clears and presents 30 frames with a 16 ms message pump between them, then presents once with
`DXGI_PRESENT_TEST`. Each line of [`raw/probe-output.txt`](raw/probe-output.txt) records `IsIconic`,
`IsWindowVisible` and `DWMWA_CLOAKED` at the time of the measurement, so every state is confirmed rather than
assumed. Two runs gave the same output.

No window was activated or focused, no input was sent, and the run took a few seconds. The probe is not part of
the build or of any suite.

## Limits

One machine and one adapter. A window covered by another application's window was not measured; the minimized,
cloaked and hidden states, where occlusion is certain, already return `S_OK`. Windows releases before 25H2 were not
measured.
