# Embedded D3D11 hosting

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

### Graphics and lifetime

RedXe keeps ownership of the D3D11 device, immediate context, swap chain, presentation, frame pacing and device-loss
sequence. Embedded DxUi receives a borrowed device during setup, derives its D2D/DWrite resources, and owns only its
offscreen textures and cached control resources. No second D3D device, swap chain, rendering HWND or independent
render thread is allowed. RedXe already creates its device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`.

Render D2D content into reusable BGRA textures on that same device, then composite those textures in the plugin's
D3D11 callback. This use of a D3D-backed Direct2D target is supported by the
[device-context model](https://learn.microsoft.com/en-us/windows/win32/direct2d/devices-and-device-contexts) and
[DXGI interoperability](https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-and-direct3d-interoperation-overview).
No CPU readback/upload round trip is permitted in normal UI rendering. Premultiplied alpha, target color format,
text antialiasing on transparent targets and D2D/D3D ordering must be tested; D2D and D3D must not concurrently bind
the same texture for incompatible use. End preparation before host scene composition and rebind the host target
and every required widget pipeline state.

One explicit, module-owned resource pool is keyed by D3D device identity and device generation, and shared by all
AV instances. Per-instance state includes the control tree, focus/capture, draft values and tile/raised surfaces.
Destroy instances before the pool; release all old-generation D2D/D3D references on device loss. Never infer audio
or camera state from graphics lifetime, and never reapply a profile during recovery.

### Preparation must be a supported host phase

The current GPU contract only sanctions resource/raster work on size changes. That is insufficient for changing
device names, focus, text entry and live values at an unchanged size. Do not call `Paint(WindowHost&)` from `Render`
and declare it allocation-free.

Propose a generic `RequestWidgetPreparation(instanceId)` host service plus `IRedXeGpuWidget::PrepareContent(...)`.
These names describe the intended ABI change, not existing methods. Keep declarations in current host/widget headers,
use exact `sizeBytes` and offset assertions, and rebuild all consumers. Non-DxUi GPU implementations provide a cheap
no-work implementation; this mechanism must carry no control types or AV-specific commands.

1. Input, model completion, theme/DPI or relevant geometry changes mark bounded dirty state and request preparation.
   The host coalesces requests per live instance/generation in bounded storage. Unknown/stale instances are rejected
   or discarded without waking a dead owner. Any-thread request copies only bounded identity/flags, never a tree.
2. On its UI thread, before frame construction and outside widget `Render`, the host services dirty visible instances.
   The preparation record supplies borrowed graphics context and at most two final tile/raised raster extents,
   DPI and layout/device generations. No HWND or back buffer is supplied. New requests raised during preparation
   remain pending for a later bounded dispatch; no recursive preparation loop.
3. Apply model changes, allocate/rebuild bounded size resources when needed, measure/shape changed text and paint
   dirty textures here. Stage a coherent visual/input snapshot; never publish new hit rectangles with old visuals.
   The owning contracts must explicitly permit this bounded event-driven work, including content refresh at fixed
   size, while retaining allocation-free frame construction and composition.
4. `Render` selects an already prepared layout variant and composites it. No control-tree traversal for layout,
   text shaping, resource creation, endpoint calls, logging or blocking work is allowed there. Clean textures are
   not repainted when another widget animates. Hidden/occluded surfaces defer preparation until needed again.
5. Allocation/preparation failure retains the last coherent surface or produces an explicit unavailable state; it
   must not leave stale interactive targets active. Capacity failure is bounded and does not start automatic retries.

Keep separate tile and raised density variants when both are drawn in one frame. An identical extent may share one
surface. Raise animation scales the final raised surface for its presentation only; it does not rebuild resources
at every intermediate extent. Hit testing uses the displayed transform and active interaction surface. Page swipes
with negative viewport origins do not rerasterize or reflow content. Host notifications must cover both extents,
even when the existing largest-extent notification alone would not change.

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
AV must additionally admit the combined cost of its tile and raised views and all simultaneous instances.

Composite requires the same device's immediate context and a host-bound render target. It issues one premultiplied
alpha triangle, sets the viewport and every other state it depends on (including disabled scissor/depth/predication
and unused shader stages), then unbinds its SRV. It does not preserve the caller's pipeline state. Negative origins
are allowed for page transitions; non-finite or invalid dimensions/depth ranges fail. It never allocates, shapes
text, rerasterizes, reads back, presents or calls user diagnostics. Consumers bind their state before subsequent work.

ReplaceDevice cancels capture and drops old surface resources while preserving the logical tree/model; a successful
prepare is required before interaction resumes. Recovery never reapplies AV commands. EmbeddedTests checks an actual
new WARP device generation, foreign-device rejection, dirty/hidden/zero/DPI behavior, pixel changes, hostile state,
negative origins, all catalog controls and allocation-free warm composition. Injected physical GPU removal remains
a separate hardware validation case; device-generation replacement is not evidence of a physical fault.


Hit-tested gestures require clean prepared content. Captured continuation can update a live draft before its next
paint, but any intervening bounds, tree or availability revision cancels capture and disables input until prepare
succeeds. This prevents new hit rectangles from being used with an old texture. Keyboard continuation uses the
same prepared interaction revision. A consumer must call Prepare between independent hit-tested gestures.
