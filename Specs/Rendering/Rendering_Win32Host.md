# Window hosting

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The WindowHost adapter owns HWND integration and its own swap-chain presentation only in window-host mode.
It uses the same retained control implementation as embedded mode, with a separate scheduling/presentation adapter.
Device loss, resize, zero-size suspension, DPI, focus and teardown have explicit contracts and regression tests.
Its message/timer/animation resources stop or quiesce when hidden and are destroyed by their owning runtime.

Native ControlHost/WindowHost is supported inside the same DxUi.lib; its messages, animation dispatcher and
resource helpers are library-owned. RedSalamander's application migration remains a later independent plan.

Debug builds request the optional D3D11 SDK layer. If device creation returns DXGI_ERROR_SDK_COMPONENT_MISSING,
retry once with only D3D11_CREATE_DEVICE_DEBUG removed, retaining BGRA support and hardware/WARP policy. Other
errors keep their normal failure behavior. This handles the [documented optional debug-layer failure](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)
without preventing ordinary control rendering and text layout. Native tests report the raw WARP debug-layer probe
and require the host to initialize with or without that SDK component.

The native menu modal loop processes pointer/keyboard input and pending paints for its own popup windows before
ordinary posted owner-window traffic. Input feedback cannot depend on the entire owner queue becoming empty.
An idle menu still blocks on messages; this policy adds no timer, polling or synchronous repaint during dispatch.
The existing owner-message-flood test retains its hover/invocation deadline and verifies visible feedback.
