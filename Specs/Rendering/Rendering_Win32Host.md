# Window hosting

Status: normative intended contract
Last reviewed: 2026-11-07

The [C++ failure policy](../Core/Core_FailureHandling.md) governs allocation, callback and paint failures.
API 5 implements that protocol. Source-specific qualification is recorded with the
[failure policy](../Core/Core_FailureHandling.md); earlier functional receipts do not prove the new host fatal/dump contract.

The application may report known occlusion through
`ControlHost::SetNativePresentationVisible(false)` on the attachment owner thread.
The signal defaults to true for each attachment. False immediately stops the host's
animation wakes, holds its frame schedule (later requests join it) and suppresses native
invalidation, preparation and rendering, including explicit initial frames.
Restoring true requests a full frame and resumes the held schedule without
activating the window. It changes presentation only, preserving the logical tree,
input, source and accessibility state. The method returns S_FALSE for an unchanged
signal, E_UNEXPECTED for an unattached or embedded host and RPC_E_WRONG_THREAD for
a different caller thread. Embedded applications retain `EmbeddedHost::SetVisible`.

The application owns the visibility decision and its event source; DxUi installs
no global window hooks and does not poll other windows. Both native presentation
modes use flip-model swap chains, for which Windows does not return
DXGI_STATUS_OCCLUDED. The historical proposal to infer arbitrary window coverage
from Present or DXGI occlusion notifications is not this contract; see
[Microsoft's status documentation](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-status).

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

The WindowHost adapter owns HWND integration and its own swap-chain presentation only in window-host mode.
It uses the same retained control implementation as embedded mode, with a separate scheduling/presentation adapter.
Device loss, resize, zero-size suspension, DPI, focus and teardown have explicit contracts and regression tests.
For a child-HWND attachment, the application MUST forward ancestor position
changes after movement through `NotifyTextInputLayoutChanged(GetFocusControl())`
on the attachment owner thread. A parent's move does not deliver the child's
`WM_MOVE`, but changes the screen coordinates exposed to text services. DxUi
does not hook ancestors; the public notification ignores a control that is not
the active text-input owner. Documented forwarding does not by itself qualify
physical IME candidate-window placement.
Its message/timer/animation resources stop or quiesce when hidden and are destroyed by their owning runtime.
Diagnostic bitmap capture stages the completed frame after EndDraw and before swap-chain presentation.
The one-argument capture keeps refusing hidden hosts; an explicit `allowHidden` overload supports owned,
deterministic harness capture without showing, activating or reading the desktop. Consecutive captures must
reflect current control state on their first frame, including caption hover at simulated DPI.
Changing non-text logical focus on a hidden or zero-size host never activates its HWND; visible, positively sized
hosts retain the requested native-transfer policy. See the [input contract](../UI/UI_InputAndAccessibility.md).

The window host chooses its text antialias mode from the user's font-smoothing setting and binds its monitor's DirectWrite
rendering parameters. `WM_SETTINGCHANGE` re-reads the setting (one existing case also refreshes wheel lines) and repaints
when the text state changed; `WM_DPICHANGED`, `WM_DPICHANGED_AFTERPARENT` and a `WM_MOVE` onto another monitor read the
parameters again. A changed text state applies to a full frame. Rules and measurements are in
[theme and typography](../UI/UI_ThemeAndTypography.md#text-antialiasing-and-rendering-parameters).

A control Tick result requests continued scheduling, independently of pixel invalidation. The native animation
dispatcher does not invalidate the window merely because a control needs another tick. Controls explicitly
invalidate visual changes, including completion; focused editors repaint when the caret blink phase changes.

### Frame schedule

Status: implemented (performance and motion plan item W3.1, 14 November 2026) for both hosts.

Each `ControlHost` keeps one schedule of per-control requests and ticks only the controls holding a due request; no
wake walks the tree. A request is `Continuous` (`RequestAnimation(control)`: the next frame, and every frame while the
control's `Tick` returns true), `Deadline` (`RequestAnimationAt(control, dueTickMs)`: one `Tick` when the animation clock
reaches the time, nothing before) or `Once` (one tick on the next frame; the rich editor restarts its caret blink with it; a deadline already passed is due at the next wake). A control holds at most one
request: every frame wins over the next frame, which wins over a deadline, and a new deadline replaces the previous one; `CancelAnimation(control)` withdraws it. Whatever a scheduled `Tick` returns, true asks
for the next frame. The tooltip layer waits for deadlines of its own ([tooltip deadlines](#tooltip-deadlines)). Every library control asks for its own ticks:
transitions, phase loops and scrolling (`Button`, `ProgressBar`, `ThroughputGraph`, `Slider`, `PageHost`, `Tree`,
`Grid`, scrollbars, the menu popup's slider, the rich editor's middle-button scrolling) request `Continuous`; the carets
of `TextField`, editable `ComboBox` and `RichTextEditor` hold one `Deadline` per phase flip, armed by the host's caret
clock (below), and none on blur.

Compatibility: `RequestAnimation()` without a control, and an embedded root installation (one discovery tick), tick
every visible control once (`Panel` and `PageHost` leave their children to that walk while the host dispatches); each
control whose `Tick` returns true then holds a `Continuous` request of its own. A derived control should call
`RequestAnimation(*this)` or `RequestAnimationAt(*this, due)` instead. `Panel::Tick` and `PageHost::Tick` called directly
still walk their children. Work counters separate the two walks (`tickLegacyTreeWalks`, `tickDiscoveryWalks`); the
library's own controls never cause a legacy walk.

Native wakes: one dispatcher per UI thread runs one periodic timer at the existing cadence only while some host of the
thread holds a `Continuous` request (pacing to the display is a separate item), otherwise one `SetCoalescableTimer`
one-shot for the earliest deadline across the thread's hosts, armed one system tick late so its wake sees the deadline
due, and nothing when nothing waits. A deadline wake ticks the due controls and renders only if one invalidated. The
first periodic tick after idle starts the timeline at the real time it runs; no phantom frame is added. Every wake and
every deadline uses one monotonic clock (`GetAnimationTickMs`): the periodic timeline while the periodic timer runs,
otherwise uptime less what the timeline lost to its hitch clamp, so starting or stopping the periodic timer never moves
the clock and a caret keeps its phase. A minimized or `SetNativePresentationVisible(false)` host holds its schedule,
`Continuous` requests included, and receives no wakes; a host whose own window is hidden (or in a `WM_SETREDRAW` FALSE
block) stops after at most one wake that observes it. Any sign that it is visible again resumes it: `WM_SHOWWINDOW`,
`WM_SIZE` restored, `WM_WINDOWPOSCHANGED` with `SWP_SHOWWINDOW`, `WM_SETREDRAW` TRUE, a `WM_PAINT`, or any change to its
schedule made while it is visible. A host hidden only through an ancestor (a minimized top-level, a hidden parent, a
parent's redraw block) receives none of these, so while it holds requests it looks once a second whether it is visible
again, ticking nothing meanwhile. On show, deadlines that came due meanwhile fire once at the first wake. A host hidden
by one of its own `Tick`s holds what that wake had not yet done.

A hidden control, or a control below a hidden one, keeps its request: it is neither ticked nor counted in the next wake
(it costs no wake) until it is shown, and from its show it is ticked again, so a transition left part-way lands or
resumes with no further input. A control that leaves a host while holding a request is ticked once by the host it joins.
One wake ticks a control at most once (a control the compatibility walk ticked is not ticked again for its request); a
control cancelled, forgotten or moved earlier in the same wake is not ticked, and a control cancelled or moved during
its own `Tick` does not re-arm by returning true. A `Tick` that runs a modal loop gets the dispatcher's nested wakes
refused (about one per periodic interval) until it returns. `Tick` called on a control that is in no host, or in another
host, requests nothing. Schedule calls (`RequestAnimation`, `RequestAnimationAt`, `CancelAnimation`) belong to the
attachment's owner thread: from another thread they change nothing and set `ERROR_INVALID_THREAD_ID`.

Lifetime: a request keeps the control's address beside its lifetime token and is used only while the token is alive
and the control still names the host. `ForgetControl` removes a control's request when it leaves the host (removal,
move to another host, root replacement), including from the snapshot of a wake in progress; detaching or closing a host
(a popup) drops its schedule. A wake works on a snapshot, so a `Tick` may re-arm, remove itself, a
sibling or the root, hide or destroy its host; a host destroyed or detached during its wake is not touched again.
Hover, capture and double-click observers remain pruned by the existing interaction-state validation before dispatch,
which addresses them only by comparison and lifetime token.

Embedded hosting owns no timer: `AdvanceAnimation` ticks only the due requests, and `NeedsAnimation` stays true while
any request, a pending deadline included, is held, so an application that polls it every frame keeps the caret
blinking. The clock rules of the [embedded contract](Rendering_EmbeddedD3D11.md) are unchanged; a hidden view holds
its requests on a timeline that does not count the hidden time. When the schedule empties outside a wake (a blurred
caret, a cancelled or forgotten request), the next request is a new start: it wakes the application and the next
advance sets the timeline's baseline instead of counting the idle time.

### Caret clock

Status: implemented (performance and motion plan item W3.2, 15 November 2026) for both hosts.

Each `ControlHost` has one caret clock. It owns the blink phase of the host's one focused caret owner (`TextField`,
editable `ComboBox`, `RichTextEditor`): the owner tells it when it gains focus or input happens (restart), when it
loses focus (stop), forwards its scheduled `Tick`, and asks it at paint whether the caret is shown. A restart shows the
caret at once and, while the caret blinks, arms the owner's `Deadline` one period later; each flip is one `Tick` that
invalidates once and arms the next flip, counted from the restart, so a late wake never shifts the phase. Input is a
keystroke, a selection move, a click, a text-input import (IME composition update or commit) and a text or selection the
application sets while the owner is focused. Focus that moves between two caret owners in one frame leaves only the new
owner's deadline; a host with no focused caret owner schedules nothing for a caret.

The period is the user's caret blink time (`GetCaretBlinkTime`; `INFINITE` is a steady caret; 530 ms when it cannot be
read). Blinking stops `SPI_GETCARETTIMEOUT` after the latest restart (5 s when it cannot be read; 0 or `INFINITE` never
stops) with the caret shown: the flip that shows it is the last, and nothing wakes until input, which restarts blinking,
as Windows' own caret does. A steady caret is shown and schedules nothing: a read-only `TextField` or `RichTextEditor`
(the [controls contract](../UI/UI_ControlsAndLayout.md) draws its caret), `INFINITE`, and reduced motion (the theme's
`reducedMotion`, which `SetTheme` applies at once). The settings are read, never changed: at a host's first caret and
again by `RefreshCaretBlinkSettings`, which a window host calls on `WM_SETTINGCHANGE`; an embedded host owns no window,
so its application calls it when its own window receives `WM_SETTINGCHANGE`. A changed setting, or a change of reduced
motion, shows a focused caret and restarts it in the new mode. `DebugSetCaretBlinkSettingsForTest` replaces both settings
for one host, so tests never touch the user's.

A hidden or minimized host wakes nothing (above); when it is shown, a focused caret is shown at once and restarts its
phase, replacing the flip it held. A host hidden only through an ancestor (a hidden or minimized parent, a parent's redraw
block) receives no message when the ancestor shows again: the first wake that finds its window visible (at most one
look later) resumes the same way before it ticks anything. A window that loses focus blurs its focused control, which stops the caret; focus that
returns restarts it. A popup that does not take focus (the `ComboBox` list) leaves the caret blinking. A caret owner
hidden while focused keeps its deadline without waking (above) and, shown again, is ticked once and continues its
phase from the latest restart. A focused owner that draws no caret holds no deadline either: a disabled `TextField`, editable
`ComboBox` or `RichTextEditor` (enabling it again restarts the blink) and a `RichTextEditor` navigating semantic elements
with the keyboard, and an owner with empty bounds (given an area while focused, its caret shows and blinks from then
on). An owner below a disabled ancestor keeps its own enabled state and still draws and blinks its caret until the host
drops its focus at its next message. A device loss leaves the phase and its
deadline alone. `GetFrameScheduleWake` therefore reports `None` for a steady caret, a read-only field and a timed-out
caret, and exactly one `Deadline` per phase otherwise. Every flip still repaints the whole window or surface: the host
cannot invalidate only the caret yet (`FX1` records one preparation per flip). The Win32 system caret
(`CreateCaret`, used for IME and accessibility positioning) and UI Automation are unaffected: they follow the caret's
position, not its blink.

### Tooltip deadlines

Status: implemented (performance and motion plan item W3.3, 15 November 2026) for both hosts.

The host's tooltip layer waits for at most one deadline: the show of a pending tooltip (`SetTooltipDelayed`, after the
show delay) or the hide of a shown one (5,000 ms after it shows, or the delay a pointer leave passes to
`BeginTooltipHideDelay`). The deadline is read from the layer whenever the schedule is computed, so it is armed by
entering a waiting mode and gone with leaving it: a pointer leave during the delay, a replacement by another control's
tooltip, a tracking `SetTooltip`, `ClearTooltip` (a click, the host's mouse leave, a removed or re-resolved supplemental
target), a capture change, an inspection, detach. Every such change tells the dispatcher, so no stale wake remains; a
wake reaching a time the layer no longer waits for does nothing. The layer's `Tick` returns false: nothing about a
tooltip moves between its deadlines, and the layer is never ticked between them. A native host therefore wakes once to
show a tooltip and once to hide it, and runs no periodic timer for it; an embedded host's `NeedsAnimation` stays true
while a deadline is pending, as for any deadline, and `AdvanceAnimation` before it ticks nothing. A tooltip has no fade
or motion of its own, so it never requests every frame. A Ctrl change over a rich title resolves the supplemental
tooltip at the next wake. The layer is host-owned, not a request of the control schedule: within a wake its deadline is
read after every control's `Tick`, so a `Tick` that dismisses, replaces or re-arms the tooltip is honoured in the same
wake, and detach clears the layer after leaving the dispatcher, so nothing fires after detach. A `TooltipLayer` placed in
a control tree instead requests its next deadline from its host with `RequestAnimationAt` when it is ticked.

A host that is hidden (`SetNativePresentationVisible(false)`, minimized, its window hidden as observed at a wake, or an
embedded view's `SetVisible(false)`) drops a tooltip still waiting to show, since the pointer that asked for it is no
longer known when the host returns; a shown tooltip's hide is held like any deadline and hides it at the first wake after
the show (an embedded view's timeline does not count the hidden time).

The show delay is the user's mouse hover time (`SPI_GETMOUSEHOVERTIME`, read and never changed) clamped to 100-2,500 ms,
500 ms when the system reports none. Each host reads it once, when it first schedules a tooltip, and again on
`RefreshTooltipHoverTime()`: a window host calls it on `WM_SETTINGCHANGE`; an embedded host has no window, so its
application calls `Controls().RefreshTooltipHoverTime()` when its own window receives `WM_SETTINGCHANGE`. The next
tooltip scheduled waits for the refreshed delay. Tests replace the setting through `DebugSetTooltipHoverTimeForTest`.
Work counters record the layer's ticks (`tooltipDeadlineTicks`) and the shows dropped while hidden
(`tooltipShowsDropped`).

Hit queries revalidate root lifetime and current ownership before dispatch. Root replacement or retirement during
painting aborts that frame before capture or presentation; the next update applies the current root's bounds before
painting it. A checked native `PrepareContent` failure is isolated to that control: healthy siblings can still
prepare and present. The failed control keeps unavailable content, preserves source, paints only safe prior
geometry/chrome, and is excluded from the pending-work aggregate. Report once for that failed episode; unrelated
invalidation does not retry it. An explicit content/bounds request or resource/typography epoch change permits
another preparation. Unrelated intrinsic-layout continuations do not retry the failure. Native resource and
typography retirement rearms controls through the canonical content-cancellation pass; initial resource creation
precedes native content preparation. A reentrant explicit request remains newer pending work. Captured gestures crossing failed
or pending content are cancelled before matching Up dispatch. The embedded host retains its checked HRESULT
return contract and does not compose a failed preparation. An unexpected paint/input exception, including bad_alloc, requires the host fatal diagnostic
path; it must not be turned into a skipped frame or a consumed message followed by continued operation.
Failure-site context must survive the handoff. These boundaries do not qualify whole-host destruction inside a callback.
After application/TSF pretranslation, native pumps call `DispatchKeyboardMessage` before Windows translation.
Its exact attached-HWND Return/Space path dispatches once through the real window procedure, then translates only
an explicitly permitted character still owned by the original live text control. Commands, failed callbacks and
retired owners cannot produce text for a successor. Other HWNDs, child HWNDs, system keys and IME-owned keys retain
their existing path. The detailed permission and reentrancy rules belong to
[input and accessibility](../UI/UI_InputAndAccessibility.md); forwarding `HandleMessage` alone does not establish
this pretranslation boundary.
The same frame-abort rule applies to page and model geometry replacement. Incoming page bounds deferred during
paint are synchronized before rendering in the next Update, even if the containing root bounds did not change.
Supported child extraction, clearing, reparenting, visibility and enabled-state changes also invalidate an in-flight
frame under an unchanged root. Attachment changes schedule a corrective native paint; the next capture or update
renders the current tree. Delegate replacement from a model getter cannot publish a partially painted frame.

Attach runs on a same-process HWND's owner thread. One host owns an HWND, and one host has at most one attachment.
Repeating Attach is idempotent only for the same HWND, presentation mode and batch-initial-children option; a repeated force-WARP request also requires the existing shared pool to be WARP. A default force-WARP option inherits the current pool policy. Changing HWND, mode or batch option requires Detach first; an incompatible explicit WARP request fails.
Attach attempted synchronously from a teardown callback fails with `ERROR_BUSY`; a later attachment after Detach
returns remains supported. Teardown cannot acquire a new HWND or resurrect focus or a native text-input session.
A rejected attachment preserves the existing host, accessibility peers, payloads and shared-resource accounting.
`AttachOptions::forceWarp` chooses software rendering before the owner thread's native graphics pool is in use.
Default attachments inherit that pool's policy, including native peers/menus. A forced request incompatible with
an attached/default-created pool fails with `ERROR_INVALID_STATE`; it never replaces peers' resources. Matching
repeated attachments are idempotent. Device loss retains the selected policy; the final attachment releases it.
Only an attachment creates or holds the owner thread's graphics pool. A host that is not attached and only measures or
lays out text (`GetWriteFactory`, `GetTextFormat`) borrows the process-shared DirectWrite factory, the same object
attached hosts use, and creates no device or pool, so after it a forced-WARP or default attachment on that thread
succeeds in either order.
This additive option supports deterministic native validation and leaves the default hardware/fallback path intact.
`CreateD3D11DeviceWithWarpFallback` places each `D3D11CreateDevice` attempt inside the
audited synchronous native-call boundary. A C++ exception handled within the native
implementation may return its original HRESULT; an escaping exception remains fatal.
Hardware/WARP ordering, empty outputs on failure and the optional missing-debug-layer
retry are unchanged. The allowance covers neither application callbacks nor the whole
graphics setup. Deterministic factory injection is a private test seam, not a public API.
The host registry is bounded to 128 concurrent attachments. Exhaustion fails with a diagnostic and recovers when
an attachment is released; it never replaces an existing owner.

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

Private heap payloads use the existing bounded registry. A receiver must match the token, destination HWND,
message protocol and payload type before taking ownership; mismatches leave the genuine request available.
Payload type identity is the address of private writable per-type storage, independent of the deleter function address:
link-time identical-code folding may give distinct deleter instantiations the same executable address. The deleter
remains solely for destruction. These tokens and type identities are private to one linked DxUi module and do not
establish ownership or type identity across independently linked DLL boundaries.
Registry exhaustion is a reported failure and freeing an entry permits a retry. Accessibility menu Invoke
returns the failed post's HRESULT instead of claiming that an unqueued action succeeded. A dispatch timeout
cancels work that is still pending; an action already taken by the owner can finish after the caller times out.
Late results use owned request state and cannot write into the departed caller's stack.

The native menu and animation-dispatcher window classes use the instance of the module containing their
window procedure. An executable and independently linked DLLs may each use DxUi.lib in one process;
they must not route another module's native messages or silently lose its animation subscriptions.
The external consumer fixture verifies actual animation ticks, both class/procedure module identities,
menu invocation and repeated use across one executable and two DLLs using only a POD result ABI.
The fixture joins its UI thread before unloading its DLLs; it does not qualify unload with live native UI.

The module's UI-thread animation dispatcher keeps its message-only HWND warm while any native host remains
attached. Detaching one host must preserve sibling subscriptions and the shared idle cache. The final attachment
releases that cache after retiring its controls and subscriptions, while the process failure reporter and module
are still available. If final detachment occurs inside a timer callback, window destruction waits until that
callback and pending subscription bookkeeping finish. An idle dispatcher must not retain a window until TLS
destruction after host uninstallation. Embedded hosting does not acquire this native timer or window.
A native host is subscribed to the dispatcher exactly while its schedule waits for something, through one function that
joins, reschedules and leaves, and `Detach` leaves unconditionally, so no subscription outlives its host or its need.
Requests made before `Attach` reach the dispatcher when it succeeds. A host detached after its thread's dispatcher was
destroyed (a static host destroyed after the thread's thread-local objects at process exit) finds no dispatcher and has
nothing to leave; it never touches the destroyed one. If a window nevertheless survives until thread-local destruction (an application that
leaves a host attached when its thread ends), the dispatcher drops its subscribers without calling them and stops its
timers first; its window procedure handles its own teardown messages, and any timer message that arrives without an
active failure registration, by allocation-free, callback-free cleanup without entering DxUi work (a timer it stops
that way is recorded as stopped, so a later subscriber arms it again), so an ordinary thread or process exit never
reports a missing failure host.
An animation request from a native detachment callback is ignored; focus/capture retirement cannot create
a replacement subscription after that host has relinquished its original subscription.

Debug builds request the optional D3D11 SDK layer. If device creation returns DXGI_ERROR_SDK_COMPONENT_MISSING,
retry once with only D3D11_CREATE_DEVICE_DEBUG removed, retaining BGRA support and hardware/WARP policy. Other
errors keep their normal failure behavior. This handles the [documented optional debug-layer failure](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice)
without preventing ordinary control rendering and text layout. Native tests report the raw WARP debug-layer probe
and require the host to initialize with or without that SDK component.

For a native flip-model swap chain, the first back-buffer contents are undefined. WindowHost creates or resizes the
chain with a pending full-frame presentation, so an initial partial WM_PAINT is rendered and presented as a full frame.
The pending state clears only when every enabled initial root-child Flush, EndDraw, and Present return S_OK; an
occluded status or any failure keeps the next render full. EmbeddedHost does not own or present a swap chain and
is unaffected by this rule.
A render that cannot recreate what it presents with (the device, the swap chain, its back buffer or target bitmap, or a
`ResizeBuffers`) while its window is visible and sized, and a render that loses the device, leave no update region
behind, so nothing else would paint the window again. WindowHost therefore arms one one-shot retry on its own HWND
(a timer whose procedure invalidates the window; never a periodic timer) and backs off: the first retry waits one
frame (16 ms), each further failure or loss before a successful present doubles the wait, up to 1 s (16, 32, 64, 128,
256, 512, 1,000, 1,000 ms ...). A successful `Present` disarms a pending retry and returns the next wait to one frame;
detachment disarms it. A hidden window's retry invalidates nothing, and its show paints. EmbeddedHost keeps no timer:
`Prepare` returns the failure, the view stays dirty and requests no preparation of its own, and the application
decides when to prepare again ([embedded contract](Rendering_EmbeddedD3D11.md)). `DebugSetRecoveryRetryClockForTest`
drives the schedule from a test clock.
Solid-color brushes are device-context resources: a swap-chain resize or a new target bitmap keeps the host's brush
cache, and only a device change (`DiscardDeviceResources`, a shared-device generation change) clears it.
A partial frame redraws only its dirty rectangle and presents it with `Present1`; the presented frame stays correct
because the runtime copies the previous frame's updated areas into the buffer being presented, and only a reader of the
back buffer before `Present` (a debug capture) can see older content outside the rectangle.
This is observed, not assumed. `TestWindowHostPresentedFrameAfterPartialPresentsIsCurrentOnWarp` and
`...OnHardware` (WindowHost suite) present a full frame, a partial frame over rectangle A and one over an overlapping
rectangle B through `DebugRenderDirtyRectForTest`, and read the image the compositor shows with the non-activating
presented-image capture (`Tests/Support/Support.Tests.PresentedWindowCapture.h`). In both presentation modes, on WARP
and on the default hardware adapter, the presented image after B has 0 BGRA differences from a full render of the same
state, outside B included, while the back buffer read before B's `Present`
(`DebugRenderDirtyRectForTest(rect, capture)`) still holds frame 0's A-only pixels, two frames old. A negative control
whose dirty rectangle misses a change shows exactly the missed pixels, so the capture reads presented content, not the
model. A machine without a hardware adapter reports the hardware case as a capability skip. The capture is a test
harness, not a library API: `DwmFlush`, then `PrintWindow(PW_CLIENTONLY | PW_RENDERFULLCONTENT)` of a per-monitor-aware
window placed off-screen at the bottom of the z-order, answering PrintWindow's own `WM_PAINT` so the host neither renders
nor presents during the read; the [receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W0.6/README.md) records the
adapters, driver and captures.
The incremental-versus-fresh oracle therefore reads only full native frames (`DebugCaptureBitmap`) until the
presented-frame truth test supplies a presented-image reader; `DebugDropRetainedStateForTest` drops every registered
retained cache of the host and its controls and invalidates the window ([validation](../Testing/Testing_Validation.md#incremental-versus-fresh-oracle)).
The [trace provider](../Core/Core_PerformanceAndResources.md#trace-provider) observes a `WindowHost` on its own thread:
each mouse, wheel, key and character message the window procedure receives writes one input-received event, the first
invalidation after each render pass one frame-requested event with its reason (invalidate, visibility, recovery or
continuation), each render pass prepare begin and end events, each `Present` or `Present1` one present event, and each
`WM_PAINT` a wake. Nothing is written, and no host state changes, while no session or test sink listens.
`AttachOptions::batchInitialRootChildren` is an opt-in native swap-chain rendering policy, defaulting to false. While
the initial draw remains pending after swap-chain creation or resize, a full frame may flush after each visible
direct child of a root `Panel`, with at most eight intermediate Flush boundaries in addition to the final EndDraw
submission. Remaining children,
overlays, and tooltips keep their existing order. Nested panels do not submit.
Successful Flush and EndDraw consume this initial-draw policy even when Present reports occlusion, so hidden warm
frames never repeat the intermediate submissions while full presentation remains pending. Partial frames do not
submit. The option does not apply to embedded hosts. A failed Flush stops further submissions, still pairs BeginDraw
with EndDraw, suppresses capture and presentation, and leaves the initial draw and full-frame state pending; a
device-loss result follows the existing resource reset and recovery path. An EndDraw failure also keeps the initial
draw pending.
Root replacement, supported retained-tree/interaction mutation, and reentrant rendering end the current batch sequence.
The default-off behavior remains the ordinary single EndDraw path.

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

A native host's animation clock, `GetAnimationTickMs`, is the current UI-thread dispatcher timeline, not the last tick
of an idle individual host. Transitions and tooltip show/hide deadlines ([tooltip deadlines](#tooltip-deadlines)) stamp it, so a resumed host neither shows nor
hides a newly scheduled tooltip at once because its previous tick is stale, nor holds a transition still because the
dispatcher lost a stall beyond its hitch clamp. An embedded host's clock is its own timeline
([embedded D3D11](Rendering_EmbeddedD3D11.md)).
