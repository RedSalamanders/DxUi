# Hosting, input and lifetime

## Embedded view

The application creates a D3D11 device with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`. Call
`GraphicsDevice::Create(device, pool)` and `view.Attach(pool, callbacks)`, checking both HRESULTs.
Share one pool per device generation; each `EmbeddedHost` owns an independent control tree and cached BGRA surface.
All view/pool operations and destruction occur on the UI thread. The pool retains COM references to the supplied
device; the application owns immediate-context scheduling and presentation.

1. Build the tree and apply `view.Controls().SetTheme(...)`. `MakeDefaultThemePalette(true)` selects a dark palette;
   use `false` for light. Apply the application's reduced-motion and high-contrast policy explicitly.
2. Model, input, theme or geometry changes mark the view dirty. A borrowed `requestPreparation` callback requests
   host work; it is synchronous, coalesced, must not re-enter the view, and its context must outlive attachment.
3. Before scene drawing, call `view.Prepare(widthPixels, heightPixels, dpi)`. It performs changed layout and raster
   work. `S_FALSE` means already clean, hidden or zero-sized. A failed HRESULT suppresses input/composition until
   preparation succeeds; handle the failure instead of repeatedly retrying in a tight loop.
4. Bind the application's render target, then call `view.Composite(context, viewport)`. It binds its required
   pipeline state and draws the prepared texture. It changes D3D state: rebind your state for subsequent drawing.
5. Present through the application. DxUI embedded mode owns no HWND, swap chain, timer, worker or presentation loop.

Clean `Composite` does no allocation, layout, shaping or readback. DPI must be 48..768; the per-view surface limit
is 64 MiB, with a 128 MiB replacement ceiling. Check summed memory before creating many views. If two simultaneous
layouts need distinct hit rectangles or sizes, create two views sharing the pool and bind both to the same model.

## Input, DPI and animation

Pass view-local physical pixel coordinates to `DispatchPointer`. Convert application screen/client coordinates
through the actual displayed viewport transform, including animation offsets. Pass Move/Down/Up/Wheel/Leave/Cancel
and the appropriate modifiers. Prepare changed content before a new hit-tested gesture. Cancel on capture loss.
Captured drag continuation may update a draft between paints; tree, bounds or availability changes cancel capture.

Use `DispatchKey` and `DispatchCharacter` for basic keyboard/character input. Those methods are not a complete
embedded IME, text-store or UI Automation bridge: the consumer must supply the integrations described in
[input and accessibility](../Specs/UI/UI_InputAndAccessibility.md). Native ControlHost has separate HWND services.

Call `AdvanceAnimation(nowTickMs)` only while `NeedsAnimation()` requests host ticks, then prepare when dirty.
Stop scheduling idle and hidden views. Call `SetVisible(false)` when unavailable, and prepare a nonzero target
when resuming. Change layout in DIPs and pass the new physical extent/DPI to Prepare; do not rasterize at each
intermediate animation scale. Theme and layout changes need refreshed docs/gallery when changing the library.

## Native HWND host

Use `ControlHost` (`WindowHost` is its compatibility alias) with a caller-owned HWND. After the HWND exists,
call `Attach(hwnd)` and check its boolean result, then install the tree and theme. Forward appropriate window
messages through `HandleMessage(hwnd, message, wParam, lParam, handled)`; return that result when handled and
otherwise continue normal window dispatch. This mode owns native graphics, text/accessibility and presentation
services. The application still owns the top-level window and event-blocked message loop.

Call `Detach()` before the caller-owned HWND and borrowed application state are destroyed. Do not call native
`Attach(HWND)` on `EmbeddedHost::Controls()`. The
[native fixtures](../Tests/Controls/DxUiTestHelpers.h) show real window creation, forwarding and teardown.

## Device recovery and ownership

Create a new shared pool for the replacement application device, then call `ReplaceDevice(pool)` on every view.
This retains the logical tree/model and cancels capture. Prepare successfully before resuming input and composition.
Release all references to the old generation. Check every HRESULT at these boundaries.

Use WIL for owned COM/Windows resources and `unique_ptr` for the control tree. Do not pass DxUI objects or STL
ownership through a plugin ABI: source, archive, toolchain and runtime must match within the consuming module.
The [sample host](../Samples/EmbeddedControls/Main.cpp) demonstrates application-owned device/window lifetime.
