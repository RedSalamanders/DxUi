# Embedded D3D11 hosting

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

### Graphics and lifetime

The embedding application owns the BGRA-capable D3D11 device, immediate context, swap chain, presentation, frame
pacing and device-loss sequence. Embedded DxUi derives its D2D/DWrite resources from that supplied device and owns
only offscreen textures and cached control resources. It creates no second D3D device, swap chain, rendering HWND
or independent render thread.

Render D2D content into reusable BGRA textures on that device, then composite into an application-bound render target.
This follows the [device-context model](https://learn.microsoft.com/en-us/windows/win32/direct2d/devices-and-device-contexts)
and [DXGI interoperability](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-direct3d-interoperation-overview).
No CPU readback/upload round trip is permitted in production rendering. Test premultiplied alpha, target format,
text antialiasing and D2D/D3D ordering. End preparation before composition and rebind the application's pipeline state.

Share an explicit GraphicsDevice pool among views using the same device generation. Each view owns its control tree,
focus/capture, drafts and surface. Release old-generation graphics references on device loss while retaining logical
state. Graphics recovery must not execute unrelated application operations.

### Preparation and composition

1. Input, model, theme, DPI and geometry changes mark bounded dirty state. The supplied callback asks the application
   to schedule preparation. Coalesce work, reject stale instance requests and avoid recursive preparation dispatch.
2. On the owning UI thread, prepare changed visible content before scene composition. Allocation, layout, shaping
   and raster work belong here, including content changes at an unchanged size. No application ABI is prescribed.
3. Composite the prepared surface without layout, allocation, rasterization, application calls, I/O or blocking work.
   Clean views do not repaint merely because other content animates. Hidden/occluded work waits for visibility.
4. A preparation failure must suppress stale interaction and follow the documented unavailable/retry contract;
   it must not start an automatic retry loop.

Different simultaneous layouts use independent views with a shared pool. Presentation transforms do not require
rerasterizing unchanged content. Consumers map input to the displayed view and account for every raster extent.
Their widget, overlay, plugin and host-service contracts belong in their own repositories, not this library spec.

### Implemented public contract

`GraphicsDevice::Create` retains an application-created BGRA-capable D3D11 device and creates D2D/DWrite resources,
embedded shader objects and composition state. It never creates a D3D device. Each EmbeddedHost retains the shared
pool, its own ControlHost and one offscreen BGRA surface. Two density variants use two EmbeddedHosts with the same
pool and application model; their trees and hit rectangles remain independent. The caller selects which view is
interactive and supplies view-local physical coordinates. This replaces the earlier single-tree/two-surface sketch.

Attach, prepare, input, animation, replace-device and destruction are UI-thread affine. The borrowed preparation
callback is synchronous and must not re-enter the host. Dirty requests coalesce; requests raised during painting
remain pending after preparation. The caller advances animation only when NeedsAnimation is true. Hidden views
request no periodic work; zero-sized targets disable interaction. Prepare returns S_FALSE for a clean or suspended
view. Failed preparation suppresses composition/input until a successful prepare. No automatic retry loop exists.

Prepare receives width/height in pixels and DPI (48..768), checks D3D dimension limits and a 64 MiB surface ceiling,
then performs changed layout/raster work. Surface replacement peaks at at most 128 MiB per view. Graphics resources
are shared at the pool; surface bytes, replacement peak, allocation/preparation/composition counts are queryable.
Consumers must additionally admit the combined cost of every simultaneous view.

Composite requires the same device's immediate context and a host-bound render target. It issues one premultiplied
alpha triangle, sets the viewport and every other state it depends on (including disabled scissor/depth/predication
and unused shader stages), then unbinds its SRV. It does not preserve the caller's pipeline state. Negative origins
are allowed for page transitions; non-finite or invalid dimensions/depth ranges fail. It never allocates, shapes
text, rerasterizes, reads back, presents or calls user diagnostics. Consumers bind their state before subsequent work.

ReplaceDevice cancels capture and drops old surface resources while preserving the logical tree/model; a successful
prepare is required before interaction resumes. Recovery never reapplies application commands. EmbeddedTests checks an actual
new WARP device generation, foreign-device rejection, dirty/hidden/zero/DPI behavior, pixel changes, hostile state,
negative origins, all catalog controls and allocation-free warm composition. Injected physical GPU removal remains
a separate hardware validation case; device-generation replacement is not evidence of a physical fault.


Hit-tested gestures require clean prepared content. Captured continuation can update a live draft before its next
paint, but any intervening bounds, tree or availability revision cancels capture and disables input until prepare
succeeds. This prevents new hit rectangles from being used with an old texture. Keyboard continuation uses the
same prepared interaction revision. A consumer must call Prepare between independent hit-tested gestures.

A zero-sized prepared target suspends both preparation requests and animation ticks. Dirty state and pending motion
remain retained; the caller resumes them by preparing a nonzero target. Hidden and zero-sized views never request
background frames for those deferred changes.
