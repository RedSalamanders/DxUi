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
is 64 MiB, and because a resize allocates the replacement before releasing the old surface the replacement peak is
at most 128 MiB (`GetStatistics().replacementPeakBytes`). A hidden or zero-sized view holds no surface. Check summed
memory before creating many views. If two simultaneous layouts need distinct hit rectangles or sizes, create two
views sharing the pool and bind both to the same model.

## Input, DPI and animation

Pass view-local physical pixel coordinates to `DispatchPointer`. Convert application screen/client coordinates
through the actual displayed viewport transform, including animation offsets. Pass Move/Down/Up/Wheel/Leave/Cancel
and the appropriate modifiers. Set `device` to the contact's device so a touch drag gets touch feedback (a slider's touch
halo): for a mouse message your window received, `PointerDeviceFromMessageExtraInfo(GetMessageExtraInfo())` while you
handle it. An event that names no device is the mouse. A paint-dirty view still accepts hit-tested pointer gestures while its geometry is
coherent; Prepare before composition, and after bounds, tree, visibility or enabled changes that bump the interaction
revision. Cancel on capture loss.
Captured drag continuation may update a draft between paints; tree, bounds or availability changes cancel capture.

Use `DispatchKey` and `DispatchCharacter` for basic keyboard/character input. Those methods are not a complete
embedded IME, text-store or UI Automation bridge: the consumer must supply the integrations described in
[input and accessibility](../Specs/UI/UI_InputAndAccessibility.md). Native ControlHost has separate HWND services.

Call `AdvanceAnimation(nowTickMs)` only while `NeedsAnimation()` requests host ticks. A tick marks the view dirty
only when a control changes visual state (an indeterminate progress bar on every tick, a caret only when its blink
phase flips), so check `NeedsPreparation()` after ticking instead of preparing unconditionally; a clean `Prepare`
returns `S_FALSE`. The `requestPreparation` callback also fires when animation first becomes requested, so a host
that reacts to it observes `NeedsAnimation()` after preparing; a repeated request changes nothing. Stop scheduling
idle and hidden views. Call `SetVisible(false)` when a view is unavailable:
hiding, like preparing a zero extent, releases the cached surface (`GetStatistics().surfaceBytes` becomes 0), and
the next visible sized `Prepare` allocates one surface and re-rasterizes the content. `cachedBrushes` and
`cachedTextFormats` report the bounded per-view caches (256 solid brushes, 96 configured text formats, trimmed at
preparation start). Change layout in DIPs and pass the new physical extent/DPI to Prepare; do not rasterize at each
intermediate animation scale. Theme and layout changes need refreshed docs/gallery when changing the library.

## Native HWND host

Use `ControlHost` (`WindowHost` is its compatibility alias) with a caller-owned HWND. After the HWND exists,
call `Attach(hwnd)` and check its boolean result, then install the tree and theme. Forward appropriate window
messages through `HandleMessage(hwnd, message, wParam, lParam, handled)`; return that result when handled and
otherwise continue normal window dispatch. This mode owns native graphics, text/accessibility and presentation
services. The application still owns the top-level window and event-blocked message loop.
An explicit focus request made from a control or host callback supersedes its outer request. Focus callbacks
should complete without throwing and overrides must call their base handler; standard exceptions are contained
with a safe base-state fallback. Reset and root replacement retire the old interaction state without taking HWND
keyboard focus from another window, and a callback may choose a successor in the newly installed tree.
Removing an editor from its host focus notification immediately retires its native text session and cache.
While the window holds the foreground's keyboard focus, moving focus between its controls (or a tree's items and a
grid's rows) raises the UI Automation focus change a screen reader follows. The window's own activation is reported by
the system's focus event, which UI Automation answers from the fragment root's `GetFocus` the first time it sees the
window and afterwards from whether the window's root element has the keyboard focus, reporting nothing while the focus
is inside a control. The host therefore announces the control a click that activated the window focused (it is set
after the window's `WM_SETFOCUS`, in the same message-loop turn), except in a window `GetFocus` has never been called
on, where it leaves that move to the call that answers the first event, which reports the clicked control. For the same
reason, when a window UI Automation has seen is activated again (Alt+Tab back to it), the host announces the control
that activation restored or focused once that turn ends, unless it announced a click of the turn already. Forward
every message, not only input, to `HandleMessage`, including registered messages (0xC000–0xFFFF): the host posts and
sends its own messages to its window (accessibility actions, root-provider creation for another thread, the
process-exit detach, and the message that ends that turn). DxUi registers each of them by name with
`RegisterWindowMessageW` (`RedSalamanders.DxUi.<Component>.<Purpose>.v1`) and reserves no `WM_USER` or `WM_APP`
value: the application's own messages in those ranges reach its window procedure, and `HandleMessage` neither
consumes nor acts on them. A window procedure that never passes the turn's message on leaves a click to the system's
event for up to 500 ms after the window gains focus, and its activations are never announced when UI Automation has
seen the window.

Native accessibility state changes normally mark the snapshot dirty and coalesce into a posted publication. A UIA
query on the window thread flushes dirty state before answering; a worker-thread query reads the last complete immutable
snapshot. Event comparison uses a separate baseline that advances when the queued diff drains, so a fresh query does not
consume pending events. An interested host publishes an actual focus transition synchronously before returning. That
keeps the first worker-thread `GetFocus` answer current and records who reports the transition at publication time,
while text, selection and other ordinary state changes remain coalesced. Detaching retires the snapshot and attachment
cookie; an old queued flush cannot publish into a later host attached to the same HWND. Embedded accessibility follows
its explicit `UpdateAccessibility` timing after coherent preparation, placement or focus changes.

An element whose control was
removed, or replaced at the same place in the tree, reports `UIA_E_ELEMENTNOTAVAILABLE`, and the replacement's
elements get new runtime ids; after adding or removing children, call `RefreshAccessibilitySnapshot` (or let the next
focus, size, pointer or state change do it) so clients see the new tree, which also tells them to navigate again
(StructureChanged).
A window whose only semantic control is a tree, a grid, a text field, a button or another single control has one
element, the window's, which stands for that control. What the control exposes of its own (a tree's items, a grid's
headers, rows and cells, a masked field's reveal button) is that element's children, and the element is their parent,
their fragment root and the container their patterns name, so a client walks to them from the window and the events
raised on them reach a client subscribed to the window, as they do for the same control beside another control.
When the archive is linked into several modules, each module owns its native menu and animation window classes.
Keep each module loaded while its hosts, windows, callbacks or UI-thread resources remain alive.

Call `Detach()` before the caller-owned HWND and borrowed application state are destroyed. Do not call native
`Attach(HWND)` on `EmbeddedHost::Controls()`. The
[native fixtures](../Tests/Controls/Controls.Tests.DxUiTestHelpers.h) show real window creation, forwarding and teardown.

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

Client and TSF callbacks can replace the active client synchronously. The latest explicit client request wins;
retiring a previous client cannot cancel its successor or deliver stale deferred locks to it. Detach rejects
reattachment from teardown callbacks; attach again after Detach returns when reusing the service.

Clipboard(Copy/Cut/Paste) provides bounded Unicode editing and optionally accepts an application clipboard.
The embedded document ceiling is 65,536 UTF-16 units; oversized paste fails without changing the document.
Native Grid/TextField clipboard transport independently supports larger selections, including 100,000 units.
It validates allocation bounds and UTF-16 on reads, checks allocation arithmetic on writes, and attempts clipboard
ownership once. A failed copy leaves cut text intact.
Normal keyboard shortcuts route through PreTranslate. The automated control tests use an in-memory clipboard.
The [normative contract](../Specs/UI/UI_InputAndAccessibility.md) defines composition ordering, cancellation,
deferred locks and lifetime. Library attach APIs are supported. RedXe tree/event routing, real IME/assistive-technology
acceptance and matched text/UIA performance remain pending; library service tests do not establish them.

The public [EmbeddedTextClient example](../Samples/EmbeddedControls/EmbeddedTextClient.h) binds an
EmbeddedHost to the application's TextInputServices using an immutable focusId. Run the independent
EmbeddedControls executable with --text-input to edit a profile name alongside the toggle and slider.
The application supplies the device, HWND, presentation, physical screen origin and DPI conversion.
The sample forwards deferred messages and TSF keys, cancels on focus/DPI loss, and detaches before destruction.
Deferred TSF lock notifications consumed by a nested message loop wait for the active lock to unwind; they do not
repost into that loop. Pending flags remain coalesced if posting fails and retry on the next real lock request.
The native host leaves `WM_GETTEXT`, `WM_GETTEXTLENGTH` and `WM_SETTEXT` to its window caption. Read or change an
editor through its control API, TextInputServices or UI Automation instead.
The native edit shim retains reversed `EM_SETSEL` anchor/caret orientation and maps native CRLF offsets consistently.
With --text-input --output image.png it uses a hidden application window and private clipboard to check
public consumption and render a Unicode result. That automated path does not simulate an actual IME.

Text snapshots remain editable while ordinary drawing is dirty, but omit unavailable caret/viewport geometry.
HitTestTextInput and GetTextInputRangeBounds validate the current revision and require prepared layout;
they return view-local DIPs. The application converts exactly once into physical screen pixels. Call
NotifyLayoutChanged after changed layout has been prepared, including after a TS_E_NOLAYOUT response.
This is separate from NotifyChanged, because an IME-originated edit must not echo its own text notification.


## Embedded accessibility

The public EmbeddedAccessibility.h adapter connects an existing prepared control tree to an application's UIA
fragment root. Use the same one archive. Implement EmbeddedAccessibilitySite in the caller's module, forwarding
Navigate, FragmentRoot and RequestFocus to the application's accessible window tree. ActionCompleted posts
coalesced application work; it must not synchronously reenter controls. A plugin uses its own COM/POD adapter.

AttachAccessibility takes a process-unique nonzero attachment ID and a physical-screen placement. The viewport
size matches Prepare's pixel size; GetAccessibilityProvider returns an owned COM reference. Publish changed
preparation/placement/focus with UpdateAccessibility; unchanged updates allocate nothing. Hide/detach/device loss
disconnect surviving providers. Keep provider code mapped while external COM references may survive.

Tests/Embedded/Embedded.Tests.EmbeddedAccessibility.h is an executable example using only public control/hosting interfaces:
toggle, slider and Unicode field patterns; negative-origin 144-DPI geometry; COM cross-apartment marshaling; parent
and focus callbacks; distinct identities after replacement; and cleanup. It is a synthetic component example,
not a screen-reader acceptance claim or completed RedXe adapter.

Tests/Embedded/Embedded.Tests.EmbeddedUiaBridge.h is the application's side as a test plays it: a window whose UIA provider (it hosts the
window, and uses COM threading as the view's providers do) has the view's root element for its only child, with the view's
site adapted to it. Tests/Embedded/Embedded.Tests.EmbeddedUia.h attaches views of one tree or one grid to it and subscribes an
in-process UIA client (Tests/Support/Support.Tests.UiaTestClient.h, also used by the control suites) to the window: the client walks from
the application's element to the control's parts and back, and hears the events the view raises on publishing a change,
among them the selection events of a tree (one selected item or several) and of a grid's rows, which UpdateAccessibility
raises for every change of a selection since the last update. An embedded view's root is never collapsed into its only
control: the root element is the application element's child, and the control's element is the root's.

Native consumers may include `DxUi/NativeMenuInterop.h` to adapt borrowed HMENU resources,
`DxUi/FocusRestore.h` for owned-window focus transitions, and `DxUi/PointerInput.h` for pointer
message decoding. `DxUi/Typography.h` and `DxUi/AccessibilityTextUnits.h` expose the shared font
and Unicode-unit policies. Keep application resources and command policy in your adapter.
After a font-selection change, call `DxUi::Typography::InvalidateFontFamilyAvailability(factory)` before
checking available families and rebuilding affected formats. Passing null clears every factory's availability
answers. Serialize this with the host's font queries; cached checks remain cheap and misses refresh installed
fonts. Do not invalidate per frame.

Native consumers that know an attached HWND can acquire its canonical root with
`DxUi::CreateWindowHostAccessibilityProvider(hwnd)` from `<DxUi/DxUi.h>`. Adopt the returned owned COM
reference with `wil::com_ptr::attach`; null means the window or attachment is unavailable. Call only for a
window in the same process. A foreign-thread call synchronously dispatches to the owner, which must pump
messages. Repeated acquisitions share identity during one attachment. Detach invalidates access to the retired
tree; reattachment creates a distinct identity. Keep the owning module loaded while any provider is retained.
This API requires neither private implementation headers nor a consumer diagnostics build define.
