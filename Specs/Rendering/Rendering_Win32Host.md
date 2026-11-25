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
modes use flip-model swap chains, for which DXGI_STATUS_OCCLUDED is not a usable occlusion signal. The historical proposal to infer arbitrary window coverage
from Present or DXGI occlusion notifications is not this contract; see
[Microsoft's status documentation](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-status).
This is measured on the Windows 11 baseline: on 25H2, flip-sequential and flip-discard swap chains returned `S_OK`
from `Present` and from `DXGI_PRESENT_TEST` while their window was shown, minimized, cloaked, hidden, or covered by
a child or another window, while a blt-model control returned `DXGI_STATUS_OCCLUDED` when minimized; RedXe records
the status arriving when one of its own native child windows covered its swap chain
([flip-model occlusion record](../../Measurements/FlipModelOcclusion_2026-11-19/README.md)). This holds for every
Direct3D host in the RedSalamanders applications, not only for DxUi.

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
Diagnostic bitmap capture stages the completed frame after EndDraw and does not present it ([frame pacing](#frame-pacing)).
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
thread holds a `Continuous` request and is not paced (a paced host takes its every-frame wakes from its swap chain; see
[frame pacing](#frame-pacing)), otherwise one `SetCoalescableTimer`
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

Embedded hosting owns no timer: the schedule is published as the view's
[animation request](Rendering_EmbeddedD3D11.md) (`None`, `Continuous` or a `Deadline` on the application's clock,
API revision 6), and `AdvanceAnimation` ticks only the due requests. The clock rules of the [embedded contract](Rendering_EmbeddedD3D11.md) are unchanged; a hidden view holds
its requests on a timeline that does not count the hidden time. When the schedule empties outside a wake (a blurred
caret, a cancelled or forgotten request), the next request is a new start: it wakes the application and the next
advance sets the timeline's baseline instead of counting the idle time.

### Frame pacing

Status: implemented (performance and motion plan item W3.6, 23 November 2026) for the native host. The embedded host is
unchanged: the application owns its time and scheduling.

A native host creates its swap chain, in both presentation modes, with `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`
at a maximum frame latency of 1, and passes the same flags to every `ResizeBuffers` (DXGI refuses a waitable chain's
`ResizeBuffers` without its flag with `E_INVALIDARG`). Device flags and the mandatory `DXGI_SCALING_STRETCH` of composition
chains are unchanged. A chain that refuses the flag, or that offers no waitable object, is created and presented unpaced as
before (`frame.pacerUnavailable`).

The waitable object behaves as a semaphore. Observed on WARP and on hardware, for window and composition chains, shown and
hidden (`TestFramePacingWaitableSemanticsOnWarp`): it starts with one unit; every retired present returns one unit;
`ResizeBuffers` adds and removes none, so a unit held across a resize stays valid; a present made without taking a unit does
not block under latency 1 but queues, and later returns a unit of its own, one more than the host waited for. The host
therefore never presents without a unit. A unit the host holds is its credit:

- The credit is taken when a unit is taken: from the chain's first unit when the chain is created, without waiting when no
  wait is armed (`WaitForSingleObject` with a zero timeout), or by the thread-pool wait below. Only a present that returned
  `S_OK` spends it; any other result keeps it, never more than one credit. A failure queued nothing (on WARP, `Present1`
  refused with `DXGI_ERROR_INVALID_CALL` leaves the chain's present count unchanged), so no unit will come back for it. A
  success status such as `DXGI_STATUS_OCCLUDED` may or may not have queued a frame, and the two errors are not equal: a
  credit spent on a frame that returns no unit would leave the window never painting again, exactly where that status
  appears (a minimized window, a locked session), while a credit kept for a frame that did queue leaves one unit too many.
  That unit is absorbed: one credit and one queued frame account for the only unit a latency-1 chain has, so a unit found in
  the object while the credit is held is an excess one; it is taken and dropped before the next present
  (`frame.pacerExcessUnits`), and the queue is back within its bound one frame later at most. The host presents the next requested frame at once in every case (`TestFramePacingFailedPresentKeepsTheCredit`,
  `TestFramePacingSuccessStatusKeepsTheCredit`, through a private seam that makes a present report another result; flip
  chains on the supported baseline never return `DXGI_STATUS_OCCLUDED`). The host has no other handling of that status: as
  for any result other than `S_OK`, a frame that had to be full (after a buffer reset) stays owed until a present returns
  `S_OK`. A device loss releases the chain and the credit with it. Releasing or recreating the chain drops it.
  `ResizeBuffers` keeps it.
- Nothing waits on the object while the credit is held, so nothing can wait for a unit that no present will return.
- Every present is made with a credit; a present without one is counted (`frame.presentsWithoutCredit`) and does not happen
  on any path of the host.
- Only the attachment owner thread reads or writes the credit.
- A deferred paint always has a wake: an armed wait (whose message may already be posted) or the paced frame rendering it
  now. A path that takes the credit while a paint is deferred (a synchronous render, a test seam) or keeps it after a failed
  present hands the deferred region back to the window's update region, where the next `WM_PAINT` renders it with that
  credit. Debug builds check this after each pacer transition (`frame.pacerInvariantViolations` stays 0).

When a frame is wanted (an every-frame request of the [frame schedule](#frame-schedule), or a paint that found no credit) and
no credit is held, the host arms one thread-pool wait on the object (`SetThreadpoolWait`, re-armed per frame) in the process
default pool; DxUi starts no thread and polls nothing. The wait's callback reads the request identity it was armed for,
records that the request was signaled and posts one paced-frame message to the host window through the posted-payload
helper, with the identity in `wParam`. It touches no host, allocates nothing and takes no lock. The message carries no heap
payload, so there is nothing to drain from the payload registry at teardown: a message that outlives its request owns
nothing and is recognized by its identity. Identities come from one process-wide counter and are never reused; a message
whose identity is not the armed request (cancelled, an earlier chain, an earlier attachment of the same HWND) is ignored and
counted (`frame.pacerStaleFrames`). Cancelling a wait sets it to nothing and then waits for its callbacks without cancelling
pending ones, so a unit a satisfied wait took becomes the credit instead of being lost. Teardown always cancels the wait,
drains its callbacks and closes the handle before the chain is released (`frame.pacerTeardownOrderViolations` stays 0); the
pacer is destroyed before the chain on every path, a destructor on a foreign thread included. Every drain (a cancel, a chain
release, a test seam, the pacer's destructor) passes through one function, which at process exit
(`wil::ProcessShutdownInProgress`), when the pool's threads are gone, abandons the wait without waiting for its callbacks.

A paced-frame message with its credit is one frame: while the schedule asks for every frame the host samples the animation
time and runs the scheduled wake, then renders the paints it deferred and whatever the wake invalidated through its own
`WM_PAINT`, which presents. A wake that presents nothing keeps the credit and ticks again one refresh period later from a
one-shot timer on the host window (`wakes.pacerTimer`), never from a wait. While a paced host holds an every-frame request it
does not ask the dispatcher for its periodic timer; deadlines due meanwhile are served by its frames, and the dispatcher's
deadline timer serves them otherwise. A host whose animation clock is a test's keeps the dispatcher.

Every native render waits for a credit first or defers to the paced frame, so none blocks in `Present`:

- `WM_PAINT` (and with it every resize, `UpdateWindow` and `RedrawWindow(RDW_UPDATENOW)`) renders at once when it holds or can
  take a credit without waiting; otherwise it validates the region, keeps its union, arms the wait and returns. The deferred
  region is rendered by the next paced frame, at most one refresh interval later on a composing desktop, together with
  anything invalidated meanwhile, so later invalidations are never presented before earlier ones. Neither `WM_PAINT` nor
  `WM_SIZE` ever waits. A frame presented after a resize has the size the window has when it renders.
- `RenderInitialFrameForShow` (the first frame before a show) waits at most 250 ms for a credit, because its caller shows the
  window next. A new chain already holds its first unit, so a first frame never waits.
- A frame `RenderInitialFrameForShow` presented (`S_OK`) is shown without a second render while it is current: nothing
  invalidated the host since the render began (host invalidations, and a menu popup's own `InvalidateRect`, advance one
  epoch), and the chain, client size and DPI are those it rendered with. The window's next show then raises no visibility
  invalidation, and its first `WM_PAINT` validates the update region without rendering (`frame.showPaintsValidated`), taking
  no credit and deferring nothing; any render, hide or other paint ends that state, so a window hidden and shown again later
  paints as before. A popup opened this way (`MenuPopup`, W2.4) renders once per open: its first frame spends the new
  chain's initial unit, and after the open the pacer holds no deferred region and no armed wait. A change between that render
  and the show (an invalidation, a size or a DPI change) renders exactly once more, at the show's paint or the next paced
  frame. `MenuPopup` no longer invalidates its window after the show (`Controls.Tests.PopupShow.cpp`).
- A diagnostic capture (`DebugCaptureBitmap`) renders and reads the back buffer and does not present: it neither takes nor
  needs a credit. A test render whose caller reads the presented result (`DebugRenderDirtyRectForTest`) waits at most 250 ms.
- A bounded wait that finds no credit does not present (`frame.pacerSynchronousTimeouts`); the frame is deferred.
- Text services and UI Automation learn of prepared content (new text extents) after its pixels were presented. A render
  that presented nothing for want of a credit publishes nothing; its deferred paint publishes when it presents. The one
  exception is a stalled compositor: the first wait timeout that finds the frame still deferred publishes its prepared
  content before its pixels are presented, so a client waits at most one timeout (250 ms) for it rather than until the
  display composes again. That order is acceptable only there, since a stalled compositor displays nothing. A capture,
  which never presents, publishes as a capture did before frame pacing.
- Latency bound: since no path presents without a credit (a synchronous render that finds none defers, a capture never
  presents), at most one frame is ever queued behind the one being shown, and the object never holds a spare unit that
  would let a later frame queue a second one. `TestFramePacingSynchronousRendersBuildNoLatency` presents twenty bounded-wait
  renders and twenty captures back to back and then finds no spare unit while a frame is in flight.
- A deferred region is the paint's `rcPaint`, or the whole client area when `rcPaint` is empty (a window on no monitor
  receives an empty `rcPaint` for a full invalidation, which a render treats as a full frame).

A stalled compositor (display off, a locked or disconnected session, a remote session that stops composing) returns no unit.
The wait then times out after 250 ms: the timeout presents nothing, the frame stays deferred and the wait is armed again, so a
pending frame costs at most one wake per timeout period (`frame.pacerWaitTimeouts`) and nothing pending costs nothing.

Time: a paced frame's animation sample is the compositor's predicted present time, not an averaged delta. The host reads the
statistics of the frame the compositor is composing (`DCompositionGetFrameId(COMPOSITION_FRAME_ID_CONFIRMED)` and
`DCompositionGetStatistics`: `targetTime`, `framePeriod`; a `COMPOSITION_FRAME_ID_CREATED` frame has no statistics yet, so its
call fails) and samples the first target time after now; the phase is not tuned beyond that until hardware evidence asks for
it. On the development workstation (120 Hz, hardware adapter, `FX11`, under load) every paced frame sampled its target
exactly, and DXGI's frame statistics showed the frame displayed a near-constant two refresh periods after the target it was
sampled for (537 of 599 matched presents; [receipt](../../Measurements/PerformanceAndMotion_2026-11-13/W3.6/README.md)). The
offset is constant, and motion needs uniformly spaced samples rather than samples at the displayed time, so the phase stays
the first target after now. The hardware gate (sample within 1 ms of the target in at least 99% of paced frames, at 60 and
144 Hz) is judged by `TestFramePacingSampleTimeIsTargetTimeOnHardware` when `DXUI_W36_HARDWARE_GATE=1` (the hardware slot);
ordinary runs on a shared machine only record the evidence. The sample advances the UI thread's one animation timeline
(`GetAnimationTickMs`), with the existing 50 ms hitch clamp; the first paced frame after idle starts from the real time, and
the timeline never reads less than a value it gave out, so a timeline up to one frame ahead of uptime holds the idle clock
until uptime catches up, and between paced frames the clock runs on at real time. Caret blinks and tooltip delays stay
deadlines on that timeline. When the statistics are unavailable (or report no period) the frame samples the current time
(`frame.pacerStatsFallbacks`). A target is valid only after now and at most three refresh periods ahead (the confirmed frame
targets the next vertical blank, the compositor composes at most one frame ahead of it, and one period absorbs the gap
between the clock read and the statistics read), with a period of at most 100 ms; a frame whose target lies beyond that
samples the current time and is counted (`frame.pacerTargetsOutOfRange`), so the hitch clamp still shortens a real hitch's
step while the timeline's anchor never lies more than three periods ahead of real time. Statistics are read for every frame,
so a change of the compositor clock's refresh rate is followed at the next frame. They are the compositor's global
statistics (`DCompositionGetStatistics`; per-target statistics are not read): a window on a display whose refresh rate
differs from the compositor clock's samples its animation time, and times its skip timer, on the compositor clock's targets
and period, not on its own display's vertical blanks (not verified on a second display). Each paced frame writes a `FramePaced` trace event with its target, its sample, the period and
whether it was clamped ([trace provider](../Core/Core_PerformanceAndResources.md#trace-provider)).

Idle and hidden: the pacer arms only for a deferred paint or while a visible, unsuspended host holds an every-frame request.
Leaving the every-frame request, `SetNativePresentationVisible(false)`, minimizing, detaching and releasing the chain cancel
and drain the wait and kill the skip timer before returning, so a host with nothing armed receives no paced wake. A window
hidden without a schedule change is observed hidden by at most one frame, as the frame schedule says.

Popups: the menu modal loop takes a popup's paced-frame message as early as its `WM_PAINT`, so popup feedback never waits
behind ordinary owner traffic.

Message loops: the paced frame is a message posted to the host's own window and the skip timer a `WM_TIMER` with a timer
procedure, so any loop that dispatches the window's messages keeps a paced host painting, a foreign one included (the
system's size/move, menu and drag loops, a plain `GetMessage`/`DispatchMessage` loop); the host has no idle step a loop must
run (`TestFramePacingForeignMessagePumpKeepsPainting`).

Gate evidence on WARP (WindowHost suite, `Controls.Tests.FramePacing.cpp`): 0 pacer wakes with nothing armed on `FX1`
read-only; ten consecutive ticks that skip `Present` run with at most one wake per refresh period and present the next frame
with the held credit; `FX10`'s window sweep presents only with a credit and creates one chain (0 `ResizeBuffers` failures);
stale messages are rejected after cancel, chain recreation and re-attachment; a stalled compositor presents nothing; a detach
fuzz holds the teardown order under ASan; a failed present keeps the credit; an out-of-range target does not hold the
timeline; a deferred paint keeps its wake through the credit seam; cancelling a just-signaled wait keeps its unit; a foreign
message loop keeps a paced host painting; a render that presented nothing publishes nothing. The hardware clause, the sample equal to the frame's `targetTime` within the
clock's 1 ms in at least 99% of `FX11`'s paced frames at 60 and 144 Hz, is `TestFramePacingSampleTimeIsTargetTimeOnHardware`.

### Pointer-move pacing

Status: implemented (performance and motion plan item W3.7, 25 November 2026) for the native host. The embedded host is
unchanged: the application owns input pacing, and nested input already coalesces to one wake
([embedded contract](Rendering_EmbeddedD3D11.md)). Keyboard input is never coalesced.

A paced native host (see [frame pacing](#frame-pacing)) dispatches a client `WM_MOUSEMOVE` (hover, `OnMouseMove`, the
supplemental tooltip target) at most once per presented frame, without delaying an idle window:

- A move that arrives while the host holds its frame-latency credit, or can take one without waiting, and owes no frame is
  dispatched at once. The frame it asks for renders at the next `WM_PAINT`, as soon as the window retrieves it, and presents
  at the next vertical blank; nothing waits for a paced frame, a timer or an animation tick.
- A move that arrives while a frame is in flight (no credit, or a wait already armed) is kept: the host stores the latest
  position, merges later moves into it and arms the pacer's wait. The next paced frame dispatches it first, before the
  schedule's wake runs any `Tick` and before the frame renders.
- A move that arrives while the credit is held but the previous dispatched move's frame has not rendered yet (`WM_PAINT`
  ranks below input) first renders that owed frame with the credit (`frame.pointerMoveOwedRenders`), then waits for the next
  frame as above. It never renders from inside a handler that pumps: while a render of this host is on the stack (a paint
  walk, the initial root-child batch), during a paced frame or a tick wake, or while another handler of this host runs (the
  host counts its nested `HandleMessage` frames), the move is kept with a wake instead.
- A move that changes nothing visible leaves no frame owed. The next move is then dispatched at once too, so a run of such
  moves makes several dispatches and no frame: each costs one hit test and no paint. Those dispatches are counted apart
  (`frame.pointerMovesNoFrame`). The guarantee is therefore at most one frame-owing pointer-move dispatch per
  presented frame from moves alone, and none waits while the credit is held and no frame is owed. An event of the list
  below that falls between two frames (other input, a scheduled wake) dispatches the pending move early; a move that
  arrives after it, before that frame retires, is dispatched at the next paced frame, so two frame-owing dispatches can
  precede one present. That keeps the order of events and costs one more hit test and hover update.
- A host that is unpaced (`frame.pacerUnavailable`, or no frame-latency object), hidden, zero-sized or not presenting
  dispatches every move at once: it has no frame to wait for.

A pending move always has a wake, the same invariant as a deferred paint (`frame.pacerInvariantViolations` stays 0): the
armed wait, the paced frame dispatching it now, or a posted `WindowHost.PointerMoveFlush` message. That message is posted
when the move cannot wait on the chain: the credit is held while the host may not render, or the chain that owned the wait
was released (a resize, a device loss). It carries an identity from the pacer's process-wide counter, never a pointer; one
that arrives after the move was dispatched, after a detach or a re-attachment of the same window, or for a window already
destroyed dispatches nothing (`frame.pointerFlushesStale`). If the message cannot be posted, the move is dispatched at once.
Bounds: a paced visible host dispatches a pending move at the next retired frame (within one refresh interval on a composing
desktop) or, under a stalled compositor (display off, a locked or disconnected session, a suspended schedule), at the pacer's
first wait timeout, 250 ms, without presenting. An unpaced or hidden host waits 0. A move is never lost and never waits for
unrelated input. Detach drops a pending move rather than dispatching it into a tree being torn down.

Before they are handled, these dispatch a pending move, so every handler sees the latest position and the order of events
is kept: every mouse button down, up and double click, both wheels, `WM_MOUSEHOVER`, `WM_MOUSELEAVE`, `WM_NCMOUSEMOVE`,
`WM_NCMOUSELEAVE`, non-client button downs, `WM_CONTEXTMENU`, `WM_CAPTURECHANGED`, `WM_CANCELMODE`, every key and character
message (`WM_KEYFIRST` to `WM_KEYLAST`), IME composition messages and `WM_IME_CHAR`, `WM_SETFOCUS`, `WM_KILLFOCUS`,
`WM_ACTIVATE`, `WM_ACTIVATEAPP`, `WM_ENTERMENULOOP`, `WM_ENTERSIZEMOVE`, `WM_SHOWWINDOW` hiding, `WM_SIZE` minimizing,
pointer, touch and gesture messages, the paced frame, a pacer wait timeout, every scheduled wake (`RunScheduledTicks`, so a
deadline wake's `Tick` and the tooltip's resolution read the latest position), and every synchronous render whose caller
reads the result (`RenderInitialFrameForShow`, `DebugRenderDirtyRectForTest`, `DebugCaptureBitmap`, and
`DebugWaitForFrameCreditForTest` once it holds the credit).

`WM_SETCURSOR` does not dispatch a pending move. Windows sends it before every uncaptured move, so flushing there would end
coalescing. Its cursor query is answered at its own point (the delivery's `WM_NCHITTEST` point), with its own hit test, and
every control's cursor depends only on that point and on state that changes under capture, when Windows sends no cursor
query. The cursor therefore always follows the latest point, while the hover state may lag it by less than a frame. For the
same reason, reads of hover state (`Control::IsHovered`, `DebugGetHoveredControl`) and programmatic capture
(`CaptureMouse`, `ReleaseMouseCapture`) dispatch nothing: a read runs no control callback, and a move dispatched after a
capture reaches the captured control as a move arriving after it would.

Trace: an `InputDispatched` event ([trace provider](../Core/Core_PerformanceAndResources.md#trace-provider)) names each
pointer-move dispatch, the first and the latest move it absorbed with their arrival times, the number of moves and whether it
left a frame owed. Input to present for a coalesced dispatch is judged from the first move it absorbed, so the wait this
pacing adds while a frame is in flight is counted.

Gate evidence on WARP (WindowHost suite, `Controls.Tests.PointerPacing.cpp`, through the pacer's test-owned frame-latency
object): an idle move is dispatched and presented without any wake; moves posted to a non-activated window while a frame is
in flight make one dispatch per presented frame at the latest position (also read from the trace); a second move renders the
owed frame first; a pending move is dispatched before the paced frame's `Tick`; each flushing message dispatches the pending
move before its own handling; a button down sees the latest position; the cursor follows the latest point of a coalesced
sequence; unpaced and hidden hosts dispatch at once and a stalled one at the first wait timeout; moves delivered from inside a
control callback and from inside a render cause no nested render and are dispatched afterwards; a flush survives a device loss
and is ignored after a detach or a destroy; keys are not coalesced. The hardware clause (input to present within one refresh
interval at p99, at 60 and 144 Hz) is `TestPointerPacingInputToPresentOnHardware`, judged with `DXUI_W37_HARDWARE_GATE=1`.
Menu popups route their pointer moves through the menu controller first; the same host rule applies only to what it leaves to
the popup host, inside the modal menu loop that only the focus-taking suites exercise.

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
again by `RefreshUserSettings` ([user settings](#user-settings)). A changed setting, or a change of reduced
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
tooltip, a tracking `SetTooltip`, `ClearTooltip` (a click, the host's mouse leave, a re-resolved supplemental target),
a capture change, an inspection, detach. Every such change tells the dispatcher, so no stale wake remains; a
wake reaching a time the layer no longer waits for does nothing. The layer's `Tick` returns false: nothing about a
tooltip moves between its deadlines, and the layer is never ticked between them. A native host therefore wakes once to
show a tooltip and once to hide it, and runs no periodic timer for it; an embedded host's request is that deadline while
it is pending, as for any deadline, and `AdvanceAnimation` before it ticks nothing and walks nothing for the tooltip. A
tooltip has no fade
or motion of its own, so it never requests every frame. A Ctrl change over a rich title resolves the supplemental
tooltip at the next wake. The layer is host-owned, not a request of the control schedule: within a wake its deadline is
read after every control's `Tick`, so a `Tick` that dismisses, replaces or re-arms the tooltip is honoured in the same
wake, and detach clears the layer after leaving the dispatcher, so nothing fires after detach. A `TooltipLayer` placed in
a control tree instead requests its next deadline from its host with `RequestAnimationAt` when it is ticked.

A host that is hidden (`SetNativePresentationVisible(false)`, minimized, its window hidden as observed at a wake, or an
embedded view's `SetVisible(false)`) drops a tooltip still waiting to show, since the pointer that asked for it is no
longer known when the host returns; a shown tooltip's hide is held like any deadline and hides it at the first wake after
the show (an embedded view's timeline does not count the hidden time).

A tooltip never outlives its target. While one is pending or shown under a hovered control or a supplemental target
(a control's `SetTooltipText`, or a rich title, found by the host's hit test), every change that can retire or alter
the target marks it stale and tells the dispatcher: `SetVisible` and `SetEnabled` on any control (the target or an
ancestor), a control leaving or joining the host (removal, re-parenting, root replacement, a removed tab page),
`SetTooltipText`, and a model owner's data or geometry change (`Grid` and `Tree` `SetModel`, `NotifyDataChanged`, row
height and bounds). The mark is due at once, so one wake follows and resolves the hover again at the resting pointer,
as a pointer move there would: hover pruning drops a hovered control that is gone, hidden or disabled (clearing its
tooltip), the hovered control's `OnMouseMove` lets a control that sets its own tooltip (`Grid`, `Tree`, `TabControl`)
recompute it for the row or tab now under the pointer, and the supplemental target is resolved again (a changed text
clears the tooltip and schedules the new one with the full show delay; an unchanged target keeps it as it is). Under
mouse capture only the supplemental target is resolved, so nothing is sent to the captured control. On an embedded host
the change makes the schedule's earliest deadline due now (the animation request becomes a deadline already reached, and
the view calls `requestPreparation` because that is sooner than the request it last reported) and the next
`AdvanceAnimation` resolves it. The mark is a flag beside the layer's one deadline, not a second deadline; with no tooltip pending or shown
these setters mark nothing and cause no wake. The target is otherwise resolved again only at the supplemental tooltip's
own deadlines, so a target moved without a data or geometry change of its owner (a plain `SetBounds`, a scroll offset)
is noticed at the next deadline or pointer move. Work counters record the marks (`tooltipTargetMarks`) and the wakes
that resolved the target (`tooltipTargetValidations`).
The show delay is the user's mouse hover time (`SPI_GETMOUSEHOVERTIME`, read and never changed) clamped to 100-2,500 ms,
500 ms when the system reports none. Each host reads it once, when it first schedules a tooltip, and again on
`RefreshUserSettings` ([user settings](#user-settings)). The next tooltip scheduled waits for the refreshed delay.
Tests replace the setting through `DebugSetTooltipHoverTimeForTest`. Work counters record the layer's ticks
(`tooltipDeadlineTicks`) and the shows dropped while hidden (`tooltipShowsDropped`).

### User settings

Status: implemented (performance and motion plan item W3.5, 15 November 2026) for both hosts.

A host keeps three user settings it reads, never changes: the caret blink time and caret timeout
([caret clock](#caret-clock)) and the mouse hover time ([tooltip deadlines](#tooltip-deadlines)). One public call,
`ControlHost::RefreshUserSettings()`, re-reads all three, so a caller cannot refresh one and leave another stale. A
window host calls it itself when its window receives `WM_SETTINGCHANGE`. Windows broadcasts that message to top-level
windows only: a host attached to a child window (`WS_CHILD`) receives it only when the application forwards it, and an
embedded view has no window at all. For both, the application calls `RefreshUserSettings()` (or forwards
`WM_SETTINGCHANGE` to the child host's `HandleMessage`) when its top-level window receives `WM_SETTINGCHANGE`; until then
the host keeps the values it read first. DxUi never polls the settings. Lines per wheel notch and the font-smoothing mode
are process-wide reads with their own rules (a window host refreshes them on `WM_SETTINGCHANGE`; an embedded view reads
the wheel setting at each wheel event and takes ClearType parameters from `SetTextRenderingMonitor`).

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
repeated attachments are idempotent. Device loss retains the selected policy; the final attachment releases it (the pool's
devices may be kept past it, as [Shared device stack](#shared-device-stack) states, but not its policy).
Only an attachment creates or holds the owner thread's graphics pool. A host that is not attached and only measures or
lays out text (`GetWriteFactory`, `GetTextFormat`) borrows the process-shared DirectWrite factory, the same object
attached hosts use, and creates no device or pool, so after it a forced-WARP or default attachment on that thread
succeeds in either order.
This additive option supports deterministic native validation and leaves the default hardware/fallback path intact.

The owner thread's graphics pool creates its device on the default adapter, then WARP; it does not pick the adapter
of each window's monitor, as the RedXe and RedPrism renderers do. This difference is by design. Every host attached
on one thread shares that device with its Direct2D device, factories and native peers and menus, and a window may
move between monitors of different adapters at any time. A monitor-matched device would split the pool into one
device per adapter and move a window's device-dependent resources each time it crosses adapters. A window host
presents only invalidated frames, so the cross-adapter copy Windows makes for a window on another adapter's monitor
costs one copy per presented frame, not per display refresh. A real-time host that owns its device and presents from
the idle side of its loop chooses the monitor's adapter instead.
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

### Shared device stack

Native hosts and popups on one thread share that thread's native device stack: the D3D11 device and immediate context, the
DXGI factory, the Direct2D factory and device, the DirectWrite factory and the DirectComposition device. An attachment
creates it when the thread has none; the last native detach releases it, unless at least one `EmbeddedHost` is attached
on the thread (W2.4, budget `G10`). Then the stack is kept, so the next native popup the application opens (a menu)
creates no device. A kept stack is released when the last such `EmbeddedHost` detaches and no native host is attached, at
thread exit (not during process shutdown, which leaves graphics objects to `ExitProcess`), and by its retained-resource
drop (registry row `SharedDeviceStack.retained`, which counts it only while no native host uses it and releases it only
then), and by `ReleaseRetainedNativeDeviceStack()`, which an application calls on the thread to give the memory back (it does nothing while a native host is attached). The next native attachment finds a kept stack whose device was removed while idle and replaces it, so the session
after a device loss creates exactly one stack; one made under the other device policy (`forceWarp`) is replaced too, as if
there were none, since the final attachment released that policy. Nothing is created early: a thread that never attaches a
native host never has a stack. At most one stack exists per thread. The stack stays native-host state: an `EmbeddedHost`
never draws its surface with it, and its own `ReplaceDevice` does not affect it
([embedded D3D11](Rendering_EmbeddedD3D11.md#graphics-and-lifetime)). What a kept stack costs and saves is in the
[performance contract](../Core/Core_PerformanceAndResources.md#retained-native-device-stack). Tests:
`Controls.Tests.SharedDeviceStack.cpp` (WindowHost suite).
