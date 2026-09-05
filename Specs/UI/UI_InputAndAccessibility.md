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
