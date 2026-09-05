# Input and accessibility

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.



RedXe's adapter supplies widget-local physical coordinates; EmbeddedHost converts them to DIPs exactly once. It extends the generic input
contract to distinguish capture by a control from page-pan arbitration, and forwards Move/Up/Cancel for the captured
pointer. One accepted slider Down owns the gesture; edge navigation retains its reserved region. Cancel on hide,
capture loss, detach and invalidating geometry changes; no volume setter on cancellation.

Keyboard, focus, text and UIA are implementation gates, not optional polish. Add generic keyboard/focus/text events
to the interactive mechanism, including composition and cancellation semantics. The host owns OS focus, message
routing and any TSF/IME HWND association. Reusable services from `DxUi.lib` executes on the host side; bounded
COM/POD transport connects it to the plugin's control tree. No top-level HWND, `DxUi::Control*`, `std::function` or
STL string crosses the RedXe ABI. Clipboard and text-service behavior use explicit host requests.

An optional accessibility mechanism exposes virtual-child providers/immutable snapshots to RedXe's UIA root.
Record interfaces, ownership, UI-thread marshalling, generation invalidation, screen-coordinate transforms, and
provider behavior after detach. A surviving assistive-technology reference must never dereference a destroyed
control or unloaded owner. Hidden controls and background modal contents leave navigation; labels, Toggle,
RangeValue, selection, text/value and live-state patterns must agree with visible confirmed state. The exact bridge
records are designed and tested in the RedXe adoption plan before AV is declared complete.

DxUi must support preview-versus-commit slider events, keyboard steps, cancellation, and externally acknowledged
values. The old `SetOnValueChanged` alone does not establish AV's commit-on-release behavior. AV commands remain in
the plugin; visual changes do not themselves mutate Windows devices. At least one real touch/IME/screen-reader
verification supplements synthetic tests.

### Current library input surface

EmbeddedHost dispatches pointer Down/Move/Up/Wheel/Leave/Cancel, keyboard down/up and character events to its retained
tree, with stale-focus/capture pruning. ControlHost retains native Win32 TSF/IME and UIA behavior, exercised by the
ported suites. EmbeddedHost has no OS focus HWND or cross-plugin text/UIA transport: supplying a full RedXe bridge,
composition/IME routing and assistive-technology attachment remains the adoption gate above. Native popup windows
are a Win32-host capability; embedded consumers use ComboBox/PopupLayer overlays or host-owned menu services.

Slider::SetOnChange reports Preview while dragging and exactly one Commit on accepted release, including an
unchanged final value. Capture loss, Escape, hiding or detach reports Cancel and restores the initial value. Keyboard
steps report Commit. SetValue updates from externally acknowledged state without firing an input callback. Existing
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

This API does not establish an OS text store, IME HWND association, clipboard service or UIA root. Those remain
application-side adoption gates. Native and embedded controls share the existing text model and rendering.

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
requested access. A generation-tagged posted message grants it after the active lock, without recursion or a timer.
Clear/detach invalidates queued messages. NotifyChanged publishes external edits outside an active TSF lock.
Escape clears a composition before ordinary editor handling; the application refreshes its focused client afterward.

The shared clipboard backend opens the clipboard once, without retry sleeps. Reads honor the allocation extent,
require a terminator, reject malformed UTF-16 and cap text at 65,536 units. Copy requires a selection; masked copy/cut
and read-only cut/paste fail before clipboard access. A failed copy cannot delete selected text. Paste normalizes
line endings and commits once against the captured revision. Embedded controls have no clipboard HWND: commands
go through the application service. Automated control suites inject a private clipboard, including their setup/read
helpers, and never use a desktop clipboard as a test data channel.

This implements a TSF/clipboard service, not the complete RedXe adoption. The generic consumer connection, full
display-attribute/IME acceptance, UI Automation attachment and real touch/IME/screen-reader checks remain open.
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
