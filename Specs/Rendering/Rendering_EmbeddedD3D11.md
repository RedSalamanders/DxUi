# Embedded D3D11 hosting

The [C++ failure policy](../Core/Core_FailureHandling.md) distinguishes explicit HRESULT/null refusal from
unexpected exceptions. API 5 implements the host fatal/context handoff and example dump owner; final
qualification is tracked by the linked failure contract. Existing exception-recovery receipts are historical
evidence and do not establish conformance to that policy.

Opt-in RichTextEditor Markdown analysis uses this host's existing preparation
traversal and continuation coalescing. Pending parsing returns S_FALSE before
geometry/raster publication and never advances during Composite, source reads
or UIA queries. Hiding, either zero dimension, detach/root retirement or resource
replacement cancels the pending owner job. Immutable published source-only
analysis may survive geometry changes and retained readers may outlive a view.
Analysis adds no HWND, worker, timer, device or swap chain. Analysis alone keeps
v0 formatted pixels; the separate explicit semantic preview uses the same owner
turns for CPU lowering, no-wrap marker measurement, body/cell/caret geometry and
raster preparation. Its bounded native HTML policy uses owned text/role metadata
and existing brushes, with no DOM or resource loading. Composition consumes only
the acknowledged cached surface and adds no layout, allocation or periodic work.
Block-tree preparation uses the same owner traversal. Snapshot ancestry and node
reconciliation belong to source preparation, never Composite. Equal source in a
second view has a separate document identity; geometry-only changes may retain a
completed pure syntax. Dependent fence/container edits must not reuse stale
line-local block classifications.

M3 reference/inline/extension preparation and dense forest publication share the
same traversal. Analysis still contributes no rendered primitives or host/device
resources. Native and embedded fixtures compare literal semantic expectations for
each grammar family and re-resolve global references through edit/undo/redo while
holding immutable older trees. A second embedded view retains independent syntax.
The v0 pixel comparison remains required; M4 owns semantic renderer integration.

Status: normative intended contract
Last reviewed: 2026-10-17

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
Embedded text is grayscale on the premultiplied surface whatever the font-smoothing setting. The view binds no DirectWrite
rendering parameters unless the application supplies its monitor through `SetTextRenderingMonitor`; then the next
preparation binds that monitor's parameters and repaints, and steady preparations bind nothing
([theme and typography](../UI/UI_ThemeAndTypography.md#text-antialiasing-and-rendering-parameters)).
Preparing a changed surface mutates the borrowed immediate context, including clearing PS shader-resource slot 0
and output-merger render targets before D2D work. Preparation does not preserve application bindings; pipeline
mutation is not limited to Composite. The application rebinds its state before its next draw.

Share an explicit GraphicsDevice pool among views using the same device generation. Each view owns its control tree,
focus/capture, drafts and surface. Release old-generation graphics references on device loss while retaining logical
state. Graphics recovery must not execute unrelated application operations. Surface loss, hiding and zero-size suspension cancel composition and retire old text-session and dispatch tokens while retaining a still-owned logical focus. A successful visible sized preparation creates a fresh input session for that focus; explicit CancelTextInput on OS focus loss still clears it.

Device replacement and detach release the view's owned targets, surfaces and graphics references without calling
ClearState or Flush on the application's supplied immediate context. Native swap-chain retirement owns that
cleanup separately. This preserves application bindings during retirement; Composite still binds its documented
pipeline state and does not preserve the caller's state while drawing.

Semantic script/color admission borrows the supplied target for one capability query per new script-bearing
geometry preparation. Staging retains a scalar capability after its raster pointer retires; the query's
temporary interface reference is released immediately. This adds no device, worker, HWND or timer. Compatible
geometry reuse and ordinary paragraphs need no admission query. Composition still consumes the cached surface
without semantic preparation. Native script drawing retains its existing target-interface use and resource
contract. Unsupported targets keep exact-leaf fallback; other query failures preserve the prior publication
and return their native HRESULT through the checked preparation result.

### Preparation and composition

1. Input, model, theme, DPI and geometry changes mark bounded dirty state. The supplied callback asks the application
   to schedule preparation. Coalesce work and reject stale instance requests. Keep the active-Prepare guard set while
   invoking the scheduling callback so a synchronous callback cannot recursively drain another preparation turn;
   clear it after the callback only if the same attachment still owns the state.
2. On the owning UI thread, prepare changed visible content before scene composition. Allocation, layout, shaping
   and raster work belong here, including content changes at an unchanged size. No application ABI is prescribed.
3. Composite the prepared surface without layout, allocation, rasterization, application calls, I/O or blocking work.
   Clean views do not repaint merely because other content animates. Hidden/occluded work waits for visibility.
4. A preparation failure must suppress stale interaction and follow the documented unavailable/retry contract;
   it must not start an automatic retry loop.
5. The prepared surface is a retained cache of the view. `DebugDropRetainedStateForTest` (diagnostics, tests only) drops it
   with every registered cache of the view and its controls, keeps the view's size, marks it dirty and suspends input until
   the next `Prepare`; `DebugReadSurfaceForTest` copies the surface's premultiplied BGRA pixels through a staging texture for
   the incremental-versus-fresh oracle ([validation](../Testing/Testing_Validation.md#incremental-versus-fresh-oracle)).
   Neither is used by preparation or composition.
6. The [trace provider](../Core/Core_PerformanceAndResources.md#trace-provider) writes its events on the calling
   thread inside these calls and adds no thread, timer, window or callback: one input-received event per
   `DispatchPointer`, `DispatchKey` or `DispatchCharacter` call, a frame-requested event (invalidate, visibility,
   recovery, continuation or input wake) before the first `requestPreparation` call after each preparation begins,
   prepare begin and end around a painting `Prepare`, one composite event per drawing `Composite`, and a wake per
   `AdvanceAnimation` the view asked for. Composite's event is a fixed record with no heap work.

Different simultaneous layouts use independent views with a shared pool. Presentation transforms do not require
rerasterizing unchanged content. Consumers map input to the displayed view and account for every raster extent.
Their widget, overlay, plugin and host-service contracts belong in their own repositories, not this library spec.

### Implemented public contract

RichTextEditor v0 uses this same ControlHost preparation/cache/composition path.
It borrows host fonts, brushes, DWrite/D2D and owner scheduling, creating no
embedded HWND/device/worker/timer. Unicode source, selection and history survive
surface suspension and device-generation replacement. Its layout retires when
hidden and rebuilds on demand; clean composition does no rich-text work.
DispatchCharacter and the D2d source text-service/UIA adapters are available.
DispatchKey offers Tab/Shift+Tab to the focused control first, allowing semantic
list/cell editing before ordinary focus traversal. A declined callback that
retires/unbinds the control or changes focus consumes the old input frame;
it cannot traverse a successor. Character input uses the same control route as
native WM_CHAR. No physical keyboard/focus takeover is needed for these tests.
Applications retain HWND/TSF client, focusId/revision and screen-placement ownership;
prepare changed composition/scroll/DPI before publishing geometry or accessibility.
Real IME/AT/physical qualification remains pending. See [the rich-text guide](../../docs/rich-text.md).

For embedded Return/Space keydown, `DispatchKey` can issue a move-only
`EmbeddedKeyDispatchReceipt` that binds the original view attachment, control
tree and focused owner before callbacks. A raw Win32 pump uses
`ShouldTranslateCharacter(receipt)` after dispatch and calls the OS
`TranslateMessage` only when the same live text owner has permission; the later
`WM_CHAR` uses ordinary `DispatchCharacter`. A transport that can preserve the
exact key/character association may instead pass the receipt to the correlated
`DispatchCharacter` overload. The receipt is one-use for a matching character;
foreign and mismatched attempts are rejected without consuming it, while a
matching character whose original owner or attachment has retired is consumed
without retargeting. Standalone committed characters remain independent of
pending receipts. Handled command activation does not authorize text; an
explicitly accepted editor Return or an unhandled text-input key can translate
once for that same owner. The API does not infer provenance from character
value or time. Nested key dispatches use distinct receipt objects. If overlapping
calls reuse one receipt object, the outer call invalidates that shared association
on completion, including when unwinding a callback; neither call may authorize a
delayed character through the reused object.

Cooperative content preparation: Prepare returns
S_FALSE with NeedsPreparation=true while owner work remains; the application
callback requests the next event-driven turn. No partial surface is composed.
One outstanding notification coalesces further changes even while pixels remain
dirty. Failure schedules no automatic retry. Geometry/pointer input stays
unavailable; an explicitly opted-in focused rich editor may export/edit current
source and run revision-checked IME transactions. Tree/bounds/visibility/focus/
resource changes revoke pending text permission. Replacement, hiding, zero extent
and detach cancel obsolete staging.

`GraphicsDevice::Create` retains an application-created BGRA-capable D3D11 device and creates D2D/DWrite resources,
embedded shader objects and composition state. It never creates a D3D device. Each EmbeddedHost retains the shared
pool, its own ControlHost and one offscreen BGRA surface. Two density variants use two EmbeddedHosts with the same
pool and application model; their trees and hit rectangles remain independent. The caller selects which view is
interactive and supplies view-local physical coordinates. This replaces the earlier single-tree/two-surface sketch.

Attach, prepare, input, animation, replace-device and destruction are UI-thread affine. The borrowed preparation
callback may retire or replace retained controls. Mutating input entry points defer preparation notifications
until the outermost synchronous entry has completed control callbacks, ownership checks and failure cleanup.
This covers key/character/pointer dispatch, text application/cancellation, clipboard and drop entry points.
Nested input on the same host coalesces to one wake before the outer call returns. Another embedded view's
direct setters retain immediate notification. Detach discards obsolete attachment requests; a replacement
attachment's requests still wait for any older input on that owning host to finish. Direct control setters
outside these input entries retain synchronous notification and must stop after a lifetime token expires.
Dirty requests coalesce; requests raised during painting
remain pending after preparation. The caller advances animation when the view's animation request asks for it. Hidden views
request no periodic work; zero-sized targets disable interaction. Prepare returns S_FALSE for a clean or suspended
view. Failed preparation suppresses composition/input until a successful prepare. An unchanged content failure returns its HRESULT (E_ABORT is normalized to E_FAIL), and schedules no automatic retry; a caller may retry under its own bounded policy. E_ABORT remains reserved for superseded work.
The callback must not re-enter EmbeddedHost preparation, composition, input, device replacement or lifecycle methods.
It may retire or replace controls through a `ControlHost` reference borrowed before the callback; this limited
retirement is tolerated. Library continuations that may run after synchronous scheduling or measurement callbacks
MUST keep a weak control lifetime and stop before later member access if it expired. Host-specific continuation also
revalidates current ownership. This contract does not promise safety when the callback destroys the owning host.

Optional control visual preparation and cancellation are separate from content-geometry preparation. They may
retain bounded resources but MUST NOT publish text-input/UIA geometry or delay input readiness. Cancellation runs
after the retiring surface state has been cleared. A control hook may retire its tree or detach the embedded view
while the owning host object remains alive; every continuation revalidates attachment lifetime and the current
device/visibility operation before accessing borrowed state. Ordinary dirty revisions remain independent of those
identities. An in-progress draw retains its device context until EndDraw even if a callback retires host resources.
Unfinished optional preparation keeps `NeedsPreparation()` true and coalesces one owner scheduling notification
after the preparation call. A coherent frame remains compositable while that work continues. Hosts consume that
notification with another preparation turn; content readiness alone does not imply that optional work is idle.
Tests which arm a distinct scheduling callback first drain initial preparation to idle. Text/input/UIA readiness
tests retain the earlier content-ready boundary and never wait for cosmetic work merely to inspect source.
A device replacement or hide/show transition during preparation aborts that stale frame. When the same attachment
survives, preparation clears its active flags and delivers one deferred owner wake for the new visible, sized
operation; a hidden or zero-sized view sends no wake. This is a new operation request, not an automatic retry of a
failed frame. Initial Attach still leaves the first preparation to its caller. Ordinary device replacement never
marks a notification as sent without delivering one, so a later real invalidation can request preparation.

An embedded view keeps one animation clock, `ControlHost::GetAnimationTickMs`. AdvanceAnimation adds the difference
between its application milliseconds and the previous tick's; the first tick after a pause in ticking (no animation
requested, hidden, zero-sized, a new attachment) only sets the baseline, a tick that goes back adds nothing, and the
clock saturates rather than wrapping. Between advances the clock follows the application's: a stamp taken outside an
advance reads `EmbeddedCallbacks::nowTickMs` (required at `Attach`, on the clock passed to `AdvanceAnimation`) and moves
the clock forward as an advance at that tick would, without ticking; it never moves it back and never crosses a baseline
a pause left for the next advance, and inside an advance every `Tick` sees the advance's own time. Every control stamps its transitions, caret phase, sort glyph, Tree expansion and
tooltip deadlines with that clock, and Tick receives it, so no control mixes system uptime with application time and no
application epoch changes a transition. A new attachment continues from the clock the host read before it, so the clock
never goes back. Native hosting uses the UI thread's dispatcher timeline as the same clock. Hiding/zero-sizing still
suspends ticks under the existing retained-motion contract.

Preparation rechecks the root after DPI notification and between paint and overlay calls. Root retirement or
interaction-geometry replacement during painting returns `E_ABORT`, retains pending preparation and leaves the
surface incoherent. A later explicit successful preparation restores composition. Checked nonthrowing preparation
failure leaves the partial surface unpublished. An unexpected paint exception enters host fatal diagnostics rather
than returning an ordinary retryable failure; context capture must precede loss of the originating stack.
Model notifications invalidate the interaction revision as well as pixels. A page installed during paint defers its
bounds to the next explicit Prepare, including transition pages; no resize or animation tick is required. Preparation
executes this layout before rendering, and a reentrant pending replacement keeps the frame unpublished.
Flow, density and Tree/Grid metric changes revoke prepared hit geometry. Pointer dispatch rechecks coherence after
pruning, uncaptured hover callbacks and hit testing, so a reentrant change cannot retarget the current Down against an
old surface. Captured controls receive each event once directly; a completed drag handler may invalidate sibling pane
layout and retains its draft through the following preparation while its captured bounds and effective state survive.
Color-only theme changes outside painting keep dirty-pixel pointer input available; in-paint palette replacement
rejects the mixed frame.

AdvanceAnimation marks the view dirty only through control invalidation. Every `Tick` that changes visual state
invalidates its host: an indeterminate progress bar on every tick, a caret only when its blink phase flips, a
transition on every tick including the one that settles it, tooltip show/hide, and grid/tree/menu animation. An
idle tick therefore leaves a clean prepared view clean and causes no raster work. Hosts check NeedsPreparation after
ticking; an unconditional Prepare of a clean view still returns S_FALSE.

**Animation request.** API revision 6 publishes the host's [frame schedule](Rendering_Win32Host.md#frame-schedule) as
`EmbeddedAnimationRequest GetAnimationRequest() const noexcept` and `EmbeddedAnimationRequest AdvanceAnimation(uint64_t
nowTickMs) noexcept`. A request is `None`, `Continuous` (advance every presented frame) or `Deadline` with `dueTickMs`.
The view owns no timer or worker; the application decides when to call. Rules:

1. `GetAnimationRequest` reads the application's clock, the schedule's held requests and the tooltip layer's one
   deadline; it ticks nothing, walks no tree and allocates nothing. It is `None` while the view is detached, hidden or
   zero-sized, nothing is scheduled, or the only deadline lies beyond the end of the application's clock (a clock at
   the end of its range must not make a correct loop advance with a zero sleep forever).
2. `dueTickMs` is on the application's clock, the one it passes to `AdvanceAnimation`: the last supplied tick plus what
   the view's timeline still lacks of the earliest deadline. A deadline the timeline has
   reached is reported at the last supplied tick (due at once). While the next tick would only take the baseline (the
   first request after a pause: nothing requested, hidden, zero-sized, a new attachment) the request is a `Deadline`
   of 0, due at once, since the application's clock is not known until that tick.
3. The clock rules above are unchanged: the baseline after a pause, no going back, saturation. A deadline armed before a
   pause keeps its place on the view's timeline, so after the pause it is due the time it still lacked after the
   baseline advance; a clock that jumps forward ticks each due request once and re-arms it from the new time, so a caret
   ticks once and keeps its phase and a tooltip shows once with its hide 5 s later.
4. `AdvanceAnimation` with nothing requested does nothing and returns `None`. Otherwise it advances the timeline, ticks
   only the due requests (an advance before the earliest deadline ticks no control and returns the same deadline) and
   returns the next request, read after the ticks.
5. The view records the request it reports (from `GetAnimationRequest` and `AdvanceAnimation`) and tells a sleeping
   application when the schedule needs it sooner than that record (something
   after nothing, `Continuous` after a deadline, an earlier deadline) through the existing `requestPreparation`
   callback, the one owner wake of the view (`WakeSchedule` of the
   [explicit state machines](../Plans/WIP/ExplicitStateMachines_2026-11-10.md) catalog `1.1`): the change marks the view
   dirty, and that wake coalesces like any other until the next `Prepare`. A request raised inside a wake is returned by
   that `AdvanceAnimation` instead; a request raised while painting (an indeterminate progress bar, a busy grid) does not
   re-dirty the view inside the preparation that painted it, because it is no sooner than the one already reported.
   An application that sleeps until the last request it read is therefore never left asleep; it reads the request after
   the callback. A callback raised inside an advance sees the due requests being ticked and uses the request that
   advance returns.

A new root gets one `Continuous` request for its discovery tick of the whole visible tree. A hidden view holds its
requests and its timeline does not count the hidden time; a held deadline therefore keeps its remaining time across a
hidden period, where a native host, whose dispatcher timeline keeps running, fires a deadline that passed meanwhile once
on show. Both drop a tooltip still waiting to show and restart a focused caret on show.
The caret follows the host's [caret clock](Rendering_Win32Host.md#caret-clock): a blinking caret holds one deadline per
phase flip, a steady one (read-only, `INFINITE` blink time, reduced motion, after the caret timeout) holds none, so the
request turns `None`; showing the view shows a focused caret at once and restarts its phase. The view owns no
window and receives no `WM_SETTINGCHANGE`: the application calls `Controls().RefreshUserSettings()` when its own
window receives one ([user settings](Rendering_Win32Host.md#user-settings)), as it calls `SetTheme` for reduced motion
and `SetTextRenderingMonitor` for ClearType changes. DxUi never polls the settings.

The tooltip shows and hides at two [deadlines](Rendering_Win32Host.md#tooltip-deadlines) and is ticked at nothing
else; `SetVisible(false)` drops a tooltip still waiting to show, and a change to its target makes the schedule due at once. Its
show delay is the user's mouse hover time, read once and again at each `RefreshUserSettings`. DxUi never polls the
setting.

Prepare receives width/height in pixels and DPI (48..768), checks D3D dimension limits and a 64 MiB surface ceiling,
then performs changed layout/raster work. A resize allocates the replacement surface before releasing the previous
one, so the single-surface cap bounds the transactional replacement peak to 128 MiB per view by construction;
`replacementPeakBytes` reports the observed peak. A surface that fails to be created returns its HRESULT from Prepare:
the view stays dirty, requests no preparation of its own and owns no timer, and the application's next Prepare creates
it again (the native host's own retry schedule is in the [Win32 host contract](Rendering_Win32Host.md)). Graphics resources are shared at the pool; surface bytes,
replacement peak, allocation/preparation/composition counts and the per-view cache sizes (`cachedBrushes`,
`cachedTextFormats`) are queryable. `GetStatistics` also counts, by a read-only walk of the tree that allocates
nothing and never runs inside Composite, the text layouts the view's controls and tooltip keep between preparations
(`retainedTextLayouts`, with the UTF-16 units they shape in `retainedTextLayoutUnits`) and their retained bitmaps and
decoded images other than the surface (`retainedBitmaps`, `retainedBitmapBytes`). It covers Label, Button and its
derived controls, TextField, ComboBox, Grid (an omitted tail shared by several cells counts once), RichTextEditor
images and the tooltip; RichTextEditor text layouts are not yet counted. In the same walk, which visits each control
once, it sums every registered retained cache of the view, its controls and its tooltip (`retainedCacheEntries`, and
`retainedCacheBytes`: the heap storage DxUi holds for them, never DirectWrite or Direct2D object storage and nothing for
a RichTextEditor's prepared layout) and reports the undo and redo storage of the TextField or ComboBox that holds the
most (`largestTextHistoryBytes`); `DXUI_HAS_RETAINED_CACHE_STATISTICS` marks headers with these fields. Solid brushes are bounded to 256 entries and configured text formats to 96
(`ControlHost::kSolidBrushCacheLimit`, `kConfiguredTextFormatCacheLimit`); a cache beyond its bound is cleared at the
start of the next preparation, never mid-paint. Consumers must additionally admit the combined cost of every
simultaneous view.

Cache trimming happens only after a dirty view enters owner preparation and before the control preparation pass.
Evicting configured text formats forces visual-only preparation in that same pass, so retained control resources
rebuild against current format identities before Paint. Trimming never occurs on a clean Composite and does not
change geometry or input-readiness state.

The view's host retains a text layout for each string of plain text its last preparation drew
([retained text layouts](../Core/Core_PerformanceAndResources.md#retained-text-layouts)). A string new to that table is
laid out in preparation, never in Composite, which creates no layout and makes no allocation. Releasing the surface
(hidden, zero extent, device replacement, detach) releases the table too. Its entries and bytes are reported by
`retainedCacheEntries` and `retainedCacheBytes` (registry row `ControlHost._textLayoutCache`), not by
`retainedTextLayouts`, which counts what controls and the tooltip keep.

Composite requires the same device's immediate context and a host-bound render target. It issues one premultiplied
alpha triangle, sets the viewport and every other state it depends on (including disabled scissor/depth/predication
and unused shader stages), then unbinds its SRV. It does not preserve the caller's pipeline state. Negative origins
are allowed for page transitions; non-finite or invalid dimensions/depth ranges fail. It never allocates, shapes
text, rerasterizes, reads back, presents or calls user diagnostics. Consumers bind their state before subsequent work.
The application submits the immediate context at least once per frame it composites or prepares (its Present does,
or `Flush` when it composes without presenting); the view never flushes the borrowed context. Until it is submitted, a
driver keeps the queued work and resources the view released (a replaced surface, retired targets); a software adapter
such as WARP keeps them in process memory, so a frame loop that never submits grows private bytes with every frame.

ReplaceDevice cancels capture and drops old surface resources while preserving the logical tree/model; a successful
prepare is required before interaction resumes. Recovery never reapplies application commands. EmbeddedTests checks an actual
new WARP device generation, foreign-device rejection, dirty/hidden/zero/DPI behavior, pixel changes, hostile state,
negative origins, all catalog controls, allocation-free warm composition, surface release on hide and zero extent
with exactly one reallocation and pixel-identical restoration, device replacement while hidden, tick-driven dirtying
and cache bounds. Injected physical GPU removal remains a separate hardware validation case; device-generation
replacement is not evidence of a physical fault.

Hit-tested gestures require a prepared interaction revision. Captured continuation can update a live draft before its next
paint. A bounds revision of some other control keeps that capture when the captured control is still in the tree,
it and every ancestor are enabled and visible, and it keeps the same bounds, so a splitter drag can reflow the panes
on the next preparation. The check resolves the capture through the live tree before touching it, so a destroyed
control is only released. Moving the captured control itself, hiding or disabling it or an ancestor, removing it, and
any change of the view's size or DPI, cancels capture and disables input until prepare succeeds. A second Down on the
same control within the system double-click interval and a 16 DIP slop is
`OnMouseDoubleClick`. A capture that a handled Down began follows a button mask: a MiddleDown that the captured control
accepts joins the gesture, an Up while that middle button is still down keeps the capture, and the MiddleUp that leaves
no button down releases it (unless a callback already chose a newer capture). A declined MiddleDown does not join, and
a middle gesture the control captured by itself (rich-text auto-scroll) is not released by its MiddleUp. MiddleUp reaches
the captured control, or else the control under the pointer, through `Control::OnMouseMiddleUp`; it reports handled
when that control does or a capture is held. The view has no secondary-button action. Ordinary dispatch refuses an unacknowledged bounds revision. Keyboard continuation uses the
same prepared interaction revision. After interaction geometry changes, a consumer calls Prepare before the next
independent hit-tested gesture, or finishes arranging every live control bound and calls PrepareInteraction at the
last successfully painted width, height and DPI. PrepareInteraction only acknowledges that layout; it does no
painting, resource creation, cache trimming or accessibility/text-geometry publication, and changes none of the
surface/preparation/composition counters. Dirty pixels remain scheduled for the next full Prepare before composition.
The caller must not acknowledge a partial arrangement or rely on painting to finish its hit geometry.

This optional boundary cannot enable initial, failed, hidden, zero-sized or replaced-device preparation. A changed
extent/DPI requires full Prepare; PrepareInteraction returns S_FALSE and disables dispatch until then. At the accepted
extent it returns S_OK after a changed interaction revision, or S_FALSE when already current. Wrong-thread calls fail
without mutation. The ordinary capture rules apply: a still-live captured control with unchanged bounds and enabled,
visible ancestors survives sibling layout; moved, removed, hidden or disabled capture is canceled. A cancellation
callback changing interaction geometry prevents acknowledgement (ERROR_RETRY); the caller must arrange that new
state explicitly. A failed paint or a text-import exception cannot be rehabilitated through this boundary. Text-input bounds and
UIA snapshot publication still require clean full preparation. Qualification is tracked in the
[interaction layout qualification](../Plans/Done/EmbeddedInteractionLayout_2026-10-06.md).

A hidden or zero-extent view holds no surface. SetVisible(false) and a zero-sized Prepare release the texture, shader
view and D2D target, report `surfaceBytes` 0, cancel pointer/text input and disconnect accessibility. Both states
suspend preparation requests and animation ticks. Dirty state and pending motion remain retained; the caller resumes
them by showing the view or preparing a nonzero target, which allocates exactly one surface (one
`surfaceAllocations` increment) and re-rasterizes the content. Hidden and zero-sized views never request background
frames for those deferred changes.

The supplied-device sample refreshes its text-service client after accepted input/focus changes and completed
preparation, not after every unhandled window message or cooperative pending turn. Its HWND owns TME_LEAVE
tracking and forwards PointerAction::Leave; hide and teardown cancel the registration. A changed window origin
notifies TextInputServices::NotifyLayoutChanged without a text refresh. Native mouse capture mirrors the
still-current logical capture after dispatch, preserving a new capture acquired by a release callback. The
headless text-input proof sends messages only to its hidden owned HWND and verifies leave routing, unhandled-message
refresh counts, origin notification and unchanged foreground/focus; native text-service suites own sink-delivery
assertions. The proof does not post WM_QUIT into its caller's message loop.
