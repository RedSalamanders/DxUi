# Input and accessibility

Status: normative intended contract
Last reviewed: 2026-10-01

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Localized adaptive layout must preserve the focused control's stable identity through wrapping,
disclosure and DPI changes, and publish matching visible/hit/UIA rectangles. If a focused child is
collapsed, return focus to the disclosure header; if an application removes an action, the caller
chooses its safe focus successor. Layout never chooses a destructive default. ExpandCollapse,
Toggle, full text and selection semantics must match the confirmed displayed state. Color, focus
chrome and checked state are distinct; status meaning also has text and an icon. Qualify pointer,
Tab/Shift+Tab, Space, Enter/Escape dispatch and UIA bounds in both hosting modes under the
[localized adaptive layout plan](../Plans/Done/LocalizedAdaptiveLayout_2026-09-19.md).

Qualification exercises complete transitions: initial state, input, callback effect, acknowledged
state, removal/failure, focus recovery and teardown. A screenshot of hover or a successful Invoke
return does not prove the requested state change. Semantic headings/labels follow visual reading
order; full-value access must not require duplicate hidden announcements. Repeated unchanged status
does not produce repeated notifications. Application navigation supplies its own stable target and
stale-completion policy; the library does not choose the destination or steal focus on completion.

Described native menus expose full per-entry names and MenuItem roles, exact command invocation
and acknowledged checked state. Modal and asynchronous menu tracking both activate the root popup
for native keyboard dispatch; submenus never activate. Dismissal restores the previously focused
owner control while the menu still owns focus. Logical entry navigation does not change that
session's native focus target. Native activation of a popup never selects an entry: it restores only a row
that keyboard or UIA navigation already chose, so a pointer-opened menu has no keyboard target and
publishes no transient focus. Explicit UIA focus of any focusable row in a native menu popup,
including non-command rows such as sliders, follows the session rule for both root menus and
submenus, independently of their activation style. The rule belongs to the popup host, not to the
MenuItem role: other controls with that role keep ordinary UIA focus transfer, and embedded host
focus stays with its bridge. Bounds follow the visible scrolled row geometry and DPI reflow; a row
whose bounds change republishes the provider snapshot even without a focus change, so hit testing
and BoundingRectangle match the scrolled rows. Per-entry elements, and therefore `accessibleName`,
exist only in a popup with at least one described entry; a popup without described entries keeps its
existing tree and exposes no per-entry elements. Posted UIA
actions carry popup-instance identity so HWND reuse cannot dispatch an old action into a new menu;
retained providers disconnect on teardown. Implementation and validation are tracked in the
[menu description plan](../Plans/WIP/MenuDescriptions_2026-09-21.md).

While a popup holds mouse capture Windows sends no `WM_SETCURSOR`, so the menu chooses the cursor on every delivered
pointer move: the standard arrow over its popups (and while one drags a slider or scrollbar), and outside them the
cursor the window under the pointer chooses when that window belongs to the menu's thread (it receives
`WM_NCHITTEST` then `WM_SETCURSOR` as without capture); another thread's window is never messaged and shows the
arrow. That window is messaged from a posted message, outside the menu's own handlers, so its `WM_SETCURSOR`
handling may close the menu. The cursor is chosen once more when the menu takes or moves capture, and the window under
the pointer chooses again as soon as the menu closes. These are one-shot reads of the pointer; an idle menu never polls
it. A described menu's layouts and accessibility proxies exist only while it is open: closing it returns them, even
while a client still holds its row elements. `DebugGetContextMenuResources` counts them exactly (popups, row text
layouts and the accessibility records of menu-popup snapshots, wherever held), so a test asserts that a closed menu
holds none, whatever the renderer and allocator keep.

Custom controls overriding `OnFocusChanged` MUST invoke their base implementation so `HasFocus`,
focus chrome and UIA keyboard-focus properties acknowledge the host transition. A stored host
focus pointer alone is insufficient. Consumers qualify both visible focus and raw provider state.

Win32 focus stays on a window host's HWND while focus moves between its controls (Tab, arrows, pointer,
`SetFocusControl`), so the host raises the UIA focus-changed event itself, from the difference between two published
snapshots, for the element GetFocus reports: a tree's focused item, a grid's focused row, a control, or the window
itself once no control has focus. It raises it only while its window holds the foreground's keyboard focus and a client
listens: a window that has just lost the foreground (a queued message moving focus after the user switched away)
announces nothing. When the window itself gains focus, Windows raises the focus event and UI Automation answers it. The
first focus event of a window it has not reported before is answered with a call of the fragment root's GetFocus, on
whatever thread, which reads the published snapshot when it runs (window-host providers are not marshaled to the
window's thread). Its later focus events for the window are answered without GetFocus: it asks the window's root element
whether it has the keyboard focus, and reports that element when it does and nothing when the focus is inside a control
(observed with an in-process client on Windows 11 build 26200: the activation of a seen window whose focus is in a
control was reported by nothing, and a click in that turn by nothing but the host). Whatever the activation itself
focuses or restores is published but not announced on the way, so a window's first activation is reported by that call.
A later one, which UI Automation answers without asking, the host reports itself: when the turn of the gain ends (see
below) it announces the focused element, unless it announced a move of that turn already or the root element itself
has the keyboard focus (a root that stands for its single focused control, which UI Automation reports). Which of the two
a gain is, the host reads from the count of GetFocus calls as the gain begins, before it publishes anything: UI
Automation asks only for the first focus event of a window it answers, so a call counted by then answered an earlier
event, and this gain's comes later and reads what the gain published. A click that activates the
window sets its control after the window's WM_SETFOCUS, in the same turn of the loop and before UI Automation has acted
on the event, so the host decides who reports that move. In a window no GetFocus call has ever begun on, the call that
answers the first focus event comes after the move and reports the clicked control, so the host leaves the move to it
and a client hears the control once, as it hears the control an activation focuses. Each call counts itself in the
window's provider target before it loads the snapshot, and the host reads the count right after it stores the snapshot
of the move, all four operations sequentially consistent, so a call the count does not include loads the snapshot after
the store, and one it does include may have answered the event with the control the activation focused, which the host
follows with its own announcement (at worst a duplicate). In a window UI Automation has asked before nothing else
reports the clicked control, so the host announces it itself and a client hears it once, and the end of the turn adds
nothing. The turn ends when the registered message the host posts to its window at the gain is dispatched (the message
of an earlier gain, which the window lost before the loop turned, ends nothing), or when the window loses focus; should a
window procedure never hand the host that message, it ends after 500 ms, so a move is never left to the system's event
for long, but the activation of a window UI Automation answers without asking is then not announced. A move in a
later turn, such as a click in the window that is already active, is announced by the host. An element's SetFocus moves
the host's logical focus before it takes Win32 focus, so the activation never first focuses (and reports) the window's
first control; a UIA client's SetFocus first has UI Automation focus the hosting window, which reports the window's
current focus the way a dialog's activation does, and the requested element is the last one reported. Native menu popups
raise theirs once a keyboard transition completes, and embedded hosts raise theirs from the snapshot diff. A
focus-changed callback that removes the control it was told about leaves no control focused. A control disabled, hidden
or removed while it has focus loses it at the host's next message, which publishes the change, so a client hears the
window. Replacing the whole tree (`SetRoot`) is compared with the tree before it, not with the empty snapshot standing
in during the swap.

A window-host element keeps the identity of the control it was created for, as an embedded element does. Once that
control is removed, or another control takes its tree path (a rebuilt list or tree), every call on the old element,
including a tree item's or grid row's, returns `UIA_E_ELEMENTNOTAVAILABLE` and never reaches the new control, and the
new control's elements (its items and rows too) have different runtime ids: they carry a per-process serial the
control receives when first published, which no later control reuses. A republish that adds, removes or replaces
a semantic control raises StructureChanged (ChildrenInvalidated) on the window's element, so a client navigates again.
Structural changes alone (`AddChild`, `ClearChildren`) republish the tree at the next focus, size, pointer or state
change, or through `RefreshAccessibilitySnapshot`. An event about a control comes from that control's element; only
the control a collapsed semantic root stands for reports through the window's element.

A window whose only semantic control is a Tree, a Grid, a Button, a Checkbox or Toggle, a TextField, a ComboBox, a Slider or a
ColorSwatch, or whose root has the Status role, has a collapsed semantic root: the window's element stands for that control, with
its control type, name, value and patterns, and the control has no element of its own. What the control exposes of its own is
that element's children: a tree's items, a grid's headers and then its rows (a row's children are its cells, and a cell's parent
is its row), a masked field's reveal button. The window's element is their parent and their fragment root. It is also the
selection container of a tree item or grid row, the containing grid of a grid cell, the enclosing element of a text range of the
field, and the element every event about the control comes from, so a client walks from the window's element to the parts and
back, and sees the control as it sees the same control in a window with a second control, apart from which element stands for
it. That includes its events: UI Automation drops an event raised on an element it cannot reach from the window through the
parents, so an item whose parent chain ended at a second element for its tree (as a collapsed root's items once did, with the
tree's element parentless) never reached a client subscribed to the window. A multi-select tree's selection events reach it the
same way (the invalidation of the selection is raised on the window's element). A status root keeps its other semantic controls as
children of its own. An embedded view is never collapsed: its root element is the child of the application's element (the site's
`Navigate`), the control's element is the root's child and the control's items are the control's children, the same chain from
the application's element to an item, so the application's element is where an embedded client subscribes.

An element resolves its control without scanning the tree. Each published snapshot carries lookup tables built with
it: a control's record by its path (which also gives its place among its siblings), a fragment's hit rectangle by kind,
path and item, and a control's record by its address. A provider call examines a few table slots, however many
controls the window holds, where a scan of the records examined half of them for every call and a client walking every
element paid that for each one; an event finds the path of its control by a search of the sorted addresses, about log2
of the records, and a walk down that path to confirm it. The tables describe the tree as published, so an event about a
control they do not hold (added since the publish, hidden, or no element) searches the live tree, as it always did, and
a control they do hold is confirmed against the live tree before it is used. What lives inside one control still scans
that control's own items (a tree's items by id, a grid's rows and cells), and a hit test by point scans the hit
rectangles in paint order. A diagnostics counter of what resolutions examine, and a check of the tables against a scan
of the records, back the tests.

Buttons with acknowledged disclosure state expose ExpandCollapse, consistent state properties and
state-change notifications. Expand/Collapse requests are idempotent against current acknowledged state,
reject disabled controls and invoke caller behavior only when a change is requested. Caller callbacks
run outside the accessibility snapshot mutex and may replace the tree. They own content visibility
and focus recovery; the provider does not invent a body subtree or directly change application state.
Clearing disclosure removes the pattern; removed/hidden controls cannot be activated through old
providers. Native and embedded lifetime/acknowledgement tests qualify this implementation, with
[final native receipts](../../Measurements/LocalizedAdaptiveLayout/2026-09-20/native-ci-main-78b3/README.md)
for all six x64/ARM64 configurations. ARM64 Menu foreground capability skips remain explicit.
This is not a claim of consumer screen-reader acceptance.



RedXe's adapter supplies widget-local physical coordinates; EmbeddedHost converts them to DIPs exactly once. It extends the generic input
contract to distinguish capture by a control from page-pan arbitration, and forwards Move/Up/Cancel for the captured
pointer. One accepted slider Down owns the gesture; edge navigation retains its reserved region. Cancel on hide,
capture loss, detach and invalidating geometry changes; no volume setter on cancellation.

Keyboard, focus, text and UIA are implementation gates, not optional polish. DxUi.lib supplies embedded text
snapshots, application-side TSF/clipboard services and lazy embedded UIA attach; those library APIs are supported
in `capabilities.json`. The host still owns OS focus, message routing and HWND association. Bounded COM/POD
transport to a plugin tree is a consumer adapter. No top-level HWND, `DxUi::Control*`, `std::function` or
STL string crosses the RedXe ABI. Clipboard and text-service behavior use explicit host requests.

An optional accessibility mechanism exposes virtual-child providers to the application's UIA root through
`EmbeddedAccessibilitySite`. Record interfaces, ownership, UI-thread marshalling, generation invalidation,
screen-coordinate transforms, and provider behavior after detach. A surviving assistive-technology reference must
never dereference a destroyed control or unloaded owner. Hidden controls and background modal contents leave
navigation; labels, Toggle, RangeValue, selection, text/value and live-state patterns must agree with visible
confirmed state. Library synthetic tests cover the attach API. End-to-end RedXe routing, real IME/touch/screen-reader
acceptance and matched text/UIA performance remain RedXe AV release gates (`embedded-host-text-uia-bridge`).

DxUi must support preview-versus-commit slider events, keyboard steps, cancellation, and externally acknowledged
values. The old `SetOnValueChanged` alone does not establish AV's commit-on-release behavior. AV commands remain in
the plugin; visual changes do not themselves mutate Windows devices. At least one real touch/IME/screen-reader
verification supplements synthetic tests.

### Current library input surface

EmbeddedHost dispatches pointer Down/Move/Up/Wheel/Leave/Cancel, keyboard down/up and character events to its retained
tree, with stale-focus/capture pruning. A second Down on the same control within the system double-click interval and a
16 DIP slop calls `OnMouseDoubleClick` (word selection, row activation) instead of `OnMouseDown`. Pointer Down/Move/Up/Wheel switch the host to pointer modality so keyboard-only
focus chrome does not appear on a touch. Hit-testing stays valid while the cached surface is paint-dirty. A changed interaction revision makes a new hit
incoherent until the next preparation. A drag already captured continues when some other control's bounds change and
the captured control stays in the tree and unmoved, with it and every ancestor enabled and visible. Moving, hiding,
disabling or removing the captured control, hiding or disabling an ancestor, or resizing the view, cancels that drag.
The next preparation applies that rule; input dispatched while the bounds revision is still unprepared cancels the
drag, so a consumer applies pane layout on its next preparation rather than inside the change callback.
WindowHost applies the same rule at message entry: a captured control that is still in the tree but no longer
interactive receives `OnCaptureLost` (its drag reports Cancel) instead of losing the capture silently.
ControlHost retains native Win32 TSF/IME and UIA behavior, exercised by the
ported suites. EmbeddedHost owns no OS-focus HWND. Application-side `TextInputServices` borrows the caller's HWND;
`AttachAccessibility` publishes providers without creating one. Cross-plugin COM/POD transport, composition/IME
routing through a consumer host, and real assistive-technology attachment remain RedXe AV release gates
(`embedded-host-text-uia-bridge`). Native popup windows are a Win32-host capability; embedded consumers use
ComboBox/PopupLayer overlays or host-owned menu services.

Slider::SetOnChange reports Preview while dragging and exactly one Commit on accepted release, including an
unchanged final value. Capture loss, Escape, hiding or detach reports Cancel and restores the initial value. Keyboard
steps report Commit. Hover and press ease painted chrome only: a 6 DIP track, inner thumb 6→16 DIP hover and 16→12 DIP
pressed, inside a fixed 20 DIP gray chrome disc. The 48 DIP hit band and 24 DIP grab radius do not change with hover or press. Keyboard steps and
RequestValue ease the painted thumb to the committed value, then stop requesting ticks. Pointer drags and SetValue snap
the painted position so live acknowledgement cannot lag. Reduced motion snaps every visual and requests no slider ticks.
SetValue updates from externally acknowledged state without firing an input callback, including snapping a pending
animation when the acknowledged value equals its accepted target. Existing
SetOnValueChanged remains the legacy live-value observer; AV uses SetOnChange and calls the OS setter only on Commit.
Callback-driven root replacement is supported and covered by a regression test.

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

This API does not by itself attach an OS text store. `TextInputServices` is the application-side TSF/clipboard
adapter in the same archive. Real IME/touch/screen-reader acceptance and RedXe plugin transport remain consumer
gates. Native and embedded controls share the existing text model and rendering.

Native text-store notification callbacks may destroy or replace their control, change focus, or replace text.
The store revalidates control lifetime, focus and text before applying selection or notifying TSF. Callback
exceptions return failure across the COM boundary; they never unwind through it. A failed continuation preserves
the callback's newer state and balances the sink edit transaction. Editable ComboBox callbacks receive an owned
text snapshot so their argument and callable survive destruction of the control.

### Application-side text services

TextInputServices in the same DxUi.lib attaches to an application-owned HWND on its COM STA. It creates no
renderer, swap chain, worker or timer. SetClient lazily creates a TSF document and associates focus; clear the
client on focus loss/view removal and detach before destroying the HWND. One client represents one immutable
focusId. The embedded snapshot exposes that identity separately from its frequently changing revision;
leaving and returning to a field changes focus identity even when no intermediate snapshot was read.

The application implements TextInputClient in its own module. Its Read/Apply/Cancel and physical-screen geometry
operations adapt application-defined transport; neither the HWND nor a DxUi C++ object crosses a plugin ABI.
Read returns no state after its focus identity is replaced. Apply validates the original revision; Cancel verifies
the focus identity and cancels preview only. Geometry is transformed once by the application using the actual
displayed viewport. Retained TSF objects become disconnected before their client or control lifetime ends.

The COM text store shares native control adaptation with the application client adapter. Application edits are
staged for one TSF lock because initial insertion may precede composition-start notification. Preview changes
displayed text without committing; completion commits once, including unchanged preview text. Failed/stale/focus-
replaced transactions do not retry against a new revision. An owned callback argument survives disconnection.
Application-side TSF edits are not echoed back through sink change notifications; separately observed external
changes are notified. Native-host notification behavior remains covered by its existing compatibility tests.

Forward PreTranslate before TranslateMessage/DispatchMessage and HandleMessage from the application window
procedure. Nested synchronous locks fail; asynchronous requests coalesce into one pending lock with the strongest
requested access. A generation-tagged posted message, registered like DxUi's other private messages (see the
[window hosting contract](../Rendering/Rendering_Win32Host.md)), grants it after the active lock, without recursion or
a timer.
Clear/detach invalidates queued messages. NotifyChanged publishes external edits outside an active TSF lock.
Escape clears a composition before ordinary editor handling; the application refreshes its focused client afterward.

The shared clipboard backend opens the clipboard once, without retry sleeps. Reads honor the allocation extent,
require a terminator and reject malformed UTF-16. Native transport has no embedded-editor size ceiling: valid
selections of 100,000 UTF-16 units and larger are supported subject to representable allocation size and available
memory. Writes check terminator/byte arithmetic before allocation and reject embedded NUL; allocation and ownership
failures are explicit. The application-side embedded edit/snapshot ceiling remains 65,536 units: an oversized paste
fails without truncation or document mutation. Copy requires a selection; masked copy/cut
and read-only cut/paste fail before clipboard access. A failed copy cannot delete selected text. Paste normalizes
line endings and commits once against the captured revision. Embedded controls have no clipboard HWND: commands
go through the application service. Automated control suites inject a private clipboard, including their setup/read
helpers, and never use a desktop clipboard as a test data channel.

This implements a TSF/clipboard service, not a real IME/AT product pass. The generic consumer connection, full
display-attribute/IME acceptance, real touch/IME/screen-reader checks and matched text/UIA performance remain open
on RedXe AV (`embedded-host-text-uia-bridge`). Library `AttachAccessibility` is a separate supported API; it does
not close those gates.
Reference: Microsoft [text stores](https://learn.microsoft.com/en-us/windows/win32/tsf/text-stores) and
[composition ordering](https://learn.microsoft.com/en-us/windows/win32/tsf/compositions).

### Prepared text geometry and public consumer

EmbeddedHost exposes revision-checked HitTestTextInput and GetTextInputRangeBounds in view-local DIPs.
Dirty/unprepared geometry returns S_FALSE with cleared outputs; state remains editable when its tree is coherent.
ReadTextInput omits optional bounds until preparation completes. A successful fully clipped range has empty bounds
and clipped=true. The app performs the one DIP-to-physical-screen conversion. Geometry for staged TSF text returns
TS_E_NOLAYOUT, and the app calls TextInputServices::NotifyLayoutChanged after preparation. Notifications retain
the store through callbacks that release the application's last reference. Clear/disconnect prevents stale focus
sessions from receiving edits, cancellation, or geometry.

The shared text store supports the documented InsertTextAtSelection flags: NOQUERY permits absent ACP outputs,
QUERYONLY makes no edit and obeys the same capacity limit, and their combination is invalid. These rules follow
the [Windows insertion contract](https://learn.microsoft.com/en-us/windows/win32/api/textstor/nf-textstor-itextstoreacp-inserttextatselection).
Prepared-layout notification follows the [Windows layout contract](https://learn.microsoft.com/en-us/windows/win32/api/textstor/nf-textstor-itextstoreacpsink-onlayoutchange).

Samples/EmbeddedControls --text-input consumes only public headers and the one archive. It demonstrates a
profile-name text field, application-owned TSF attachment, physical geometry, private-clipboard output checks,
focus cancellation and Unicode rendering alongside the supplied-device toggle and slider. Live mode uses
per-monitor DPI and resizes the caller-owned target/swap chain outside composition. Real IME/touch/UIA acceptance
remains a separate gate; no UIA bridge is claimed by this text-service sample.


### Shared embedded UI Automation providers

EmbeddedHost exposes lazy AttachAccessibility, UpdateAccessibility, GetAccessibilityProvider and
DisconnectAccessibility methods. The low-level bridge reuses the native provider/pattern implementation. The
application supplies a module-local EmbeddedAccessibilitySite for parent/sibling navigation, fragment-root identity,
OS-focus requests and posted/coalesced completion work. A consumer plugin adapts its own COM site to this C++
interface; no application HWND or C++ library ownership crosses its ABI. This component alone does not attach
RedXe's window or establish its release gate.

Attach after coherent preparation on the application's COM STA, with a nonzero process-unique attachment runtime ID.
The application provides the displayed physical-screen viewport; its dimensions must match prepared pixels. DIP
bounds convert once and clip to that viewport, including negative monitor origins. The same root provider identity
is reused during attachment. Child runtime IDs also distinguish the retained control's lifetime, so replacement at
the same tree path cannot reuse a surviving old provider. Providers advertise UseComThreading; standard UIA COM
proxies marshal actions to the owning STA. Raw foreign-thread actions fail instead of touching controls. See the
Windows [provider threading contract](https://learn.microsoft.com/en-us/windows/win32/api/uiautomationcore/ne-uiautomationcore-provideroptions).

Call UpdateAccessibility after changed preparation, placement or OS focus. Unchanged updates reuse the snapshot
without allocation. No snapshot work occurs in Composite, and consumers that never attach accessibility pay no
snapshot allocation or UIA wake-up cost. Prepared snapshots expose confirmed Toggle, RangeValue, text/value and
focus state. Changed active snapshots raise applicable property, text, focus, structure and (for a multi-select Tree)
selection events only while UIA clients listen. Hidden controls leave navigation; background modal views must be disconnected by the application.
ActionCompleted allows the application to post one coalesced refresh/focus/navigation operation, without reentering
the tree inside an accessibility callback.

Hide, zero-size suspension, device replacement and detach disconnect the target before destroying controls. Old
provider actions return UIA_E_ELEMENTNOTAVAILABLE after disconnect/root replacement, and a reattachment has a new
identity. A surviving reference never accesses a replaced control by path. The application is responsible for
keeping the module containing provider code mapped until surviving references can no longer call it; releasing the
control tree is not permission to unload code still referenced by COM.

Slider::RequestValue is the user-action counterpart to silent model SetValue. It validates range, enabled state and
the absence of an active drag, then reports one Commit through the same callback used by keyboard input. UIA uses
this operation, so accessible volume edits reach the consumer model. UIA text setters validate bounded UTF-16 and
revalidate lifetime after callbacks; a callback that destroys its field cannot be followed by stale synchronization.

The component's synthetic tests exercise all three AV control kinds, physical geometry at 144 DPI, real COM
marshaling, text edits, same-path replacement, hidden/detached references and zero allocation in 1,000 clean updates.
End-to-end application tree/event routing, real screen-reader/IME/touch acceptance and matched resource gates
remain required before the RedXe AV text/UIA release gate (`embedded-host-text-uia-bridge`).

A Grid row returned by Selection.GetSelection must provide working SelectionItem IsSelected and
SelectionContainer getters even when it is offscreen or beyond the bounded row-materialization cache.
These queries use immutable selection IDs and must not materialize every selected row. Removing the
model/row invalidates retained providers; stale selection containers cannot survive that removal.

A Tree with `SetMultiSelectEnabled(true)` reports `CanSelectMultiple`, lists every selected visible item from
Selection.GetSelection in visible order and answers SelectionItem `IsSelected` from that set, while only its focused
item (which may be outside the selection) reports `HasKeyboardFocus` and is what the window's `GetFocus` names.
SelectionItem `Select` replaces the selection, `AddToSelection` adds an item and `RemoveFromSelection` removes it through
the tree's own delegate callbacks, and `SetFocus` moves the focus alone. A publish that changes a multi-select tree's
selection raises, only while a client listens and after the focus announcement of the same publish,
`ElementSelected` (the selection became one item that was not selected), else `ElementAddedToSelection` and
`ElementRemovedFromSelection` per item with the `IsSelected` property change, else, past 20 items or when a selected item
left the tree, `Selection_Invalidated` on the tree; embedded hosts raise them from `UpdateAccessibility`. A tree without
multi-select reports and raises what it always did. See [Tree multi-select](UI_ControlsAndLayout.md#tree-multi-select).

Native consumers that know an attached HWND can acquire its canonical root with
`DxUi::CreateWindowHostAccessibilityProvider(hwnd)` from `<DxUi/DxUi.h>`. Adopt the returned owned COM
reference with `wil::com_ptr::attach`; null means the window or attachment is unavailable. Call only for a
window in the same process. A foreign-thread call synchronously dispatches to the owner, which must pump
messages. Repeated acquisitions share identity during one attachment. Detach invalidates access to the retired
tree; reattachment creates a distinct identity. Keep the owning module loaded while any provider is retained.
This API requires neither private implementation headers nor a consumer diagnostics build define.
