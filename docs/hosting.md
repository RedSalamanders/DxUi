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
5. Present through the application. DxUi embedded mode owns no HWND, swap chain, timer, worker or presentation loop.

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

Use WIL for owned COM/Windows resources and `unique_ptr` for the control tree. Do not pass DxUi objects or STL
ownership through a plugin ABI: source, archive, toolchain and runtime must match within the consuming module.
The [sample host](../Samples/EmbeddedControls/Main.cpp) demonstrates application-owned device/window lifetime.

### Embedded text-state transport

`ReadTextInput` returns an owned snapshot of the available focused control, a view revision and caret/viewport
bounds in view-local DIPs. It clears its output on failure. No snapshot is available while hidden, suspended,
detached or incoherently prepared. `ApplyTextInput` accepts only that view's current revision; any intervening
invalidation, focus/tree replacement or device/attachment lifetime change invalidates the token. Read again after
an accepted edit. These C++ records stay within a module; a consumer defines its own bounded plugin transport.

Preview imports displayed text, selection and composition markers without a model notification. Commit notifies
once relative to the pre-composition text, including when the committed text already matches the preview. Cancel,
focus loss, hiding, zero-size suspension and device replacement discard composition without committing it.
Externally replaced text survives cancellation. Preview/cancel/selection preserve TextField undo/redo history;
a committed composition is one undoable edit. Escape cancels composition before ordinary application handling. Read-only policy belongs to the control and cannot be changed by
an imported snapshot; selection is still available. Validate text/range/clauses before mutation, with a 65,536
UTF-16-unit text ceiling and 256 clause boundaries. A callback may destroy the edited control; no access follows
without lifetime/focus revalidation. All APIs are UI-thread operations outside composition/rendering.

The state API itself owns no OS services. For the application-side TSF and clipboard adapter, include
DxUi/TextInputServices.h. Create TextInputServices on the application COM STA, Attach the existing HWND,
and supply a TextInputClient for the current embedded snapshot's focusId. Each client is specific to one
focus session; reject Read/Apply/Cancel after that identity changes. Forward PreTranslate before Windows
message translation and HandleMessage from the window procedure; refresh client selection after input, call
NotifyChanged after external edits, and clear/detach before removing the view or HWND. Screen geometry is
application-owned and must reflect the actual viewport transform. One service uses the existing message loop
and no additional rendering resources.

Clipboard(Copy/Cut/Paste) provides bounded Unicode editing and optionally accepts an application clipboard.
Normal keyboard shortcuts route through PreTranslate. The automated control tests use an in-memory clipboard.
The [normative contract](../Specs/UI/UI_InputAndAccessibility.md) defines composition ordering, cancellation,
deferred locks and lifetime. RedXe transport adoption, UIA attachment and real IME/assistive-technology acceptance
are still pending; library service tests do not establish them.

The public [EmbeddedTextClient example](../Samples/EmbeddedControls/EmbeddedTextClient.h) binds an
EmbeddedHost to the application's TextInputServices using an immutable focusId. Run the independent
EmbeddedControls executable with --text-input to edit a profile name alongside the toggle and slider.
The application supplies the device, HWND, presentation, physical screen origin and DPI conversion.
The sample forwards deferred messages and TSF keys, cancels on focus/DPI loss, and detaches before destruction.
With --text-input --output image.png it uses a hidden application window and private clipboard to check
public consumption and render a Unicode result. That automated path does not simulate an actual IME.

Text snapshots remain editable while ordinary drawing is dirty, but omit unavailable caret/viewport geometry.
HitTestTextInput and GetTextInputRangeBounds validate the current revision and require prepared layout;
they return view-local DIPs. The application converts exactly once into physical screen pixels. Call
NotifyLayoutChanged after changed layout has been prepared, including after a TS_E_NOLAYOUT response.
This is separate from NotifyChanged, because an IME-originated edit must not echo its own text notification.
