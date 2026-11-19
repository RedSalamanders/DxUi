# Flip-model occlusion status, 19 November 2026

Question (RedHome register row H-20, convergence step CV-6-08): does `IDXGISwapChain::Present` return
`DXGI_STATUS_OCCLUDED` for a flip-model swap chain on the supported Windows 11 baseline, so that a host can learn
occlusion from it? The [window-hosting contract](../../Specs/Rendering/Rendering_Win32Host.md) takes occlusion from
the application instead; the RedXe and RedPrism renderers act on that status from their flip-model `Present`.

## Result

On Windows 11 25H2 (build 26200.9911), with an AMD Radeon 860M (driver 32.0.22024.19001), a flip-model swap chain
returned `S_OK` from `Present(1, 0)` and from `Present(0, DXGI_PRESENT_TEST)` in every window state the probe set:
shown, minimized, restored, covered by a visible child window, covered by another topmost window, cloaked and
hidden, for both `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` and `DXGI_SWAP_EFFECT_FLIP_DISCARD`, in 30 presents per state.

The control, a blt-model `DXGI_SWAP_EFFECT_DISCARD` swap chain on the same kind of window, returned
`DXGI_STATUS_OCCLUDED` from both calls while minimized in every run, so the probe observes the status whenever DXGI
returns it. In the first run it also returned it from `Present(1, 0)` while shown; in the second it did not. The blt
model's own variability does not affect the flip-model result.

RedXe's renderer records the opposite case in its source: a host-owned native child window that covers its swap chain
made DXGI report `DXGI_STATUS_OCCLUDED` while the dashboard was in the foreground, so it ignores the status on such
pages. A plain visible child window did not reproduce that here.

So for a flip-model swap chain `DXGI_STATUS_OCCLUDED` is not a usable occlusion signal: it did not arrive when the
window was minimized, cloaked, hidden or covered, and where it has been seen it came from the application's own
child window. A host takes occlusion from signals it owns: minimize and restore through `WM_SIZE`, visibility,
cloaking, display power or the session state, as this contract requires (`ControlHost::SetNativePresentationVisible`).

## Method

[`raw/OcclusionProbe.cpp.txt`](raw/OcclusionProbe.cpp.txt) is a standalone console probe built with MSVC 14.53
(`cl /EHsc /O2 /W4`). For each swap effect it creates a 64 × 64 `WS_POPUP` window with
`WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW` in the corner of the work area, a hardware Direct3D 11 device and a two-buffer
swap chain (one buffer for the blt control) for that window. It then moves the window through its states:
`SW_SHOWNOACTIVATE`, `SW_SHOWMINNOACTIVE`, `SW_SHOWNOACTIVATE` again, a visible `STATIC` child over the whole client
area, a second non-activating topmost `WS_POPUP` exactly over the window, `DWMWA_CLOAK` set, and `SW_HIDE`. In each
state it clears and presents 30 frames with a 16 ms message pump between them, then presents once with
`DXGI_PRESENT_TEST`. Each output line records `IsIconic`, `IsWindowVisible` and `DWMWA_CLOAKED` at the time of the
measurement, so every state is confirmed rather than assumed.

- [`raw/probe-output.txt`](raw/probe-output.txt): the first run, with an earlier version of the probe that had no
  covered states (its source is in this folder's history); repeated once with the same output.
- [`raw/probe-output-covered.txt`](raw/probe-output-covered.txt): the probe as committed here, with the two covered
  states.

No window was activated or focused, no input was sent, and each run took a few seconds. The probe is not part of the
build or of any suite.

## Limits

One machine and one adapter. RedXe's covering native child, a window widget's own swap chain or redirection surface,
was not reproduced. Windows releases before 25H2 were not measured.
