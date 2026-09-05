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
