# Window hosting

Status: normative intended contract
Last reviewed: 2026-10-01

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The WindowHost adapter owns HWND integration and its own swap-chain presentation only in window-host mode.
It uses the same retained control implementation as embedded mode, with a separate scheduling/presentation adapter.
Device loss, resize, zero-size suspension, DPI, focus and teardown have explicit contracts and regression tests.
Its message/timer/animation resources stop or quiesce when hidden and are destroyed by their owning runtime.

Native ControlHost/WindowHost is supported inside the same DxUi.lib; its messages, animation dispatcher and
resource helpers are library-owned. RedSalamander's application migration remains a later independent plan.

DxUi's private window messages are registered by name with `RegisterWindowMessageW`, as
`RedSalamanders.DxUi.<Component>.<Purpose>.v<N>` (`src/Support/WindowMessages.h`), never as `WM_USER` or `WM_APP`
offsets. Their values come from 0xC000–0xFFFF, which no application message on a shared window can take: DxUi reserves
no `WM_APP` value, and an application never needs one of DxUi's values. A host window procedure forwards every message,
registered ones included, to `HandleMessage`, which consumes only DxUi's own. Each message is registered once, when it
is first needed, and senders and receivers compare that cached value; a message whose parameters change takes the next
version. A failed registration yields 0, which is `WM_NULL`: it is never posted or sent, no receiver matches it, and
each sender then behaves as it does when its post or send fails. An application that drives a modal menu's menu-bar
hover calls `ContextMenu::PostMenuBarHover`. The WindowHost and Menu suites check that every message is registered,
distinct and nonzero, and that application messages at the former `WM_APP` values reach the window procedure while
DxUi neither consumes nor acts on them.

The native menu and animation-dispatcher window classes use the instance of the module containing their
window procedure. An executable and independently linked DLLs may each use DxUi.lib in one process;
they must not route another module's native messages or silently lose its animation subscriptions.
The external consumer fixture verifies actual animation ticks, both class/procedure module identities,
menu invocation and repeated use across one executable and two DLLs using only a POD result ABI.
The fixture joins its UI thread before unloading its DLLs; it does not qualify unload with live native UI.

Debug builds request the optional D3D11 SDK layer. If device creation returns DXGI_ERROR_SDK_COMPONENT_MISSING,
retry once with only D3D11_CREATE_DEVICE_DEBUG removed, retaining BGRA support and hardware/WARP policy. Other
errors keep their normal failure behavior. This handles the [documented optional debug-layer failure](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)
without preventing ordinary control rendering and text layout. Native tests report the raw WARP debug-layer probe
and require the host to initialize with or without that SDK component.

For a native flip-model swap chain, the first back-buffer contents are undefined. WindowHost creates or resizes the
chain with a pending full-frame presentation, so an initial partial WM_PAINT is rendered and presented as a full frame.
The pending state clears only when both EndDraw and Present return S_OK; an occluded status or any failure keeps the
next render full. EmbeddedHost does not own or present a swap chain and is unaffected by this rule.

The native menu modal loop processes pointer/keyboard input and pending paints for its own popup windows before
ordinary posted owner-window traffic. Input feedback cannot depend on the entire owner queue becoming empty.
An idle menu still blocks on messages; this policy adds no timer, polling or synchronous repaint during dispatch.
Each `PeekMessageW` first runs the handlers of messages other threads sent to the menu's thread. Such a handler can open
or close a submenu while the loop walks the popup chain, so the loop indexes the chain afresh for every peek and holds no
popup across the call. A popup that closed is not peeked, and none is read after it was freed.
The existing owner-message-flood test retains its hover/invocation deadline and verifies visible feedback.
Because capture suppresses `WM_SETCURSOR`, the menu sets the cursor from each delivered pointer move (arrow over its
popups, the same-thread window under the pointer choosing its own outside them) and reads the pointer only once when
it takes or moves capture and once when it closes; see the [input and accessibility contract](../UI/UI_InputAndAccessibility.md).

An asynchronous menu controller must also close its interaction and native popup chain during
thread-local/CRT destruction when its caller leaves a menu open. Mark finalization before releasing
capture or destroying windows: their synchronous messages must not recursively delete a host whose
destructor is active. This fallback does not invoke an application completion callback. The separate
`MenuExitLifetime` process suite exits with a live captured menu to exercise this boundary under ASan;
ordinary owner-window scope cleanup does not cover it.

WM_DPICHANGED can arrive synchronously inside a window operation, for example while a new popup moves
to a monitor with another DPI. A failed described-menu reflow there only dismisses the session, so no
command can run. Asynchronous finalization, including the completion callback, is posted and runs after
that operation unwinds; a running session ignores a stale request. Popup creation that observes the
dismissed session fails without showing or activating the popup. For the root created by `ShowAsync`,
that call reports failure and never fires its callback; a dismissed submenu or root switch finalizes
through the posted request or the modal loop.

Native tooltip show/hide deadlines use the current UI-thread dispatcher clock, not the last tick of an idle
individual host. A resumed host must not show or hide a newly scheduled tooltip immediately because its
previous tick is stale. Embedded tooltip scheduling retains the application-provided animation epoch.
