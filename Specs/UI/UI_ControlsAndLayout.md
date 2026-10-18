# Controls and layout

Status: normative intended contract
Last reviewed: 2026-10-18

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

RadioButton's optional group is a lifetime-checked observer of its direct owning RadioButtons. An extracted or
reparented choice selects independently until added/configured in its new group; it cannot change or dereference
its former group. ScrollPanel forwards primary and secondary child presses/releases in content coordinates,
including nonzero scroll offsets and captured release; its scrollbar responds only to the primary button.
Pointer and keyboard context anchors map back through every ancestor ScrollPanel before screen conversion,
including ancestors separated by PageHost. Tree/Grid delegate anchors use that same mapping.

Controls retain state independently of GPU resources. Buttons, toggles, sliders, selectors and text fields are shared
implementations, not application-local clones. An active gesture has a defined preview, commit and cancel lifecycle.
External state acknowledgement is separate from local intent. Hidden controls leave focus and accessibility trees.

If both requested and fallback solid-brush creation fail, painters omit that primitive without passing null to
Direct2D. Other prepared content and later recovery remain usable. Overlay dismissal revalidates container lifetime
after extensible child callbacks and stops if the container was retired.
StackPanel layout reacquires owned child storage after each bounds callback and stops an outer pass after owner/child
retirement or a nested change to layout inputs. It allocates no traversal snapshot. Call ApplyLayout after changing
children, bounds or layout settings; a nested completed reflow is not overwritten by stale outer geometry.

## Opt-in intrinsic measurement and flow

`Control::Measure(MeasureContext, LayoutConstraints, DesiredSize&)` is an additive, non-pure extension point.
Legacy custom controls return `E_NOTIMPL`; `Panel`, `StackPanel` and manually arranged controls keep their existing
layout contract. A layout owner supplies an explicit size for unsupported children. The previous arranged bounds
never act as an implicit preferred size. Constraints use finite nonnegative minima and optional finite maxima;
an absent maximum axis is unbounded. Negative, nonfinite or contradictory constraints fail without changing
the caller's desired-size output. Desired size contains an optional baseline and an explicit overflow flag.
Minimum and intrinsic extents remain authoritative when the available parent extent is too small; a container
reports overflow instead of silently shrinking an interactive target or truncating measured content.

Label, Button, Toggle, Checkbox, TextField, ComboBox and NumericStepper measure with the current host's
DirectWrite font roles, flow direction and density. Wrapping requires an explicit opt-in and finite width;
single-line measurement preserves its natural text extent. Toggle reserves the larger of its two localized
state labels. Single-line editors measure stable font/chrome requirements and use the owning layout's minimum
or explicit preferred width; typing, selecting an item or nudging a value does not resize the editor. A masked
multiline field measures its displayed mask. Shaping is bounded to 65,536 UTF-16 units per measured text value and
per container pass across cache misses and nested measures. `MeasureContext::remainingTextUnits` optionally borrows
an existing pass budget; nested containers preserve it and cached results consume no units. Excess input fails
before text-layout allocation. No character-count width approximation is a measurement fallback.

`FlexPanel` owns bounded, reusable numeric measurement and arrangement records for at most 1,024 logical child
slots. It supports horizontal or vertical order, explicit wrapping, padding, gaps, child minimum/maximum
constraints, cross-axis alignment and positive growth. A child maximum caps growth; remaining growth is
redistributed among eligible siblings. Hidden children occupy no flow space. RTL mirrors physical rectangles
while preserving logical ownership and focus/accessibility order. Measurement cache identity includes the
child's measurement revision, constraints, host typography revision, DPI, flow and density. Content, font,
wrapping, chrome, availability, ownership and inherited settings invalidate the relevant records. Color,
checked state, hover and position-only changes reuse shape results. Containers retain no DirectWrite layout
per leaf, and hidden containers retire their numeric entries for one fresh preparation when shown again.

Intrinsic owners request work through the shared pending-layout traversal used by native painting and embedded
`Prepare`. Layout, shaping and allocation remain outside embedded `Composite`. A failed or superseded layout
prevents the host from publishing that preparation. The layout owner stages all numeric geometry before the first
extensible bounds callback and leaves child bounds unchanged on validation, allocation, measurement or arithmetic
failure. After custom measurement or bounds callbacks it rechecks lifetime, ownership, host, geometry and input
revisions. A callback that retires, reparents or replaces the owner/child, changes sibling geometry, or starts a
newer layout aborts the stale pass. Geometry authored by such a callback remains authoritative; already completed
callbacks are not rolled back, and the incomplete outer pass does not publish its content extent or frame.

Flex arrangement stages absolute endpoints in logical line order. Positive intrinsic extents, gaps and padding that collapse at a
finite origin fail with arithmetic overflow before any bounds callback. An inward-rounded endpoint advances
outward, with following anchors consuming that corrected edge; represented minima, gaps, order and applicable
growth maxima remain authoritative. Optional Grow/Stretch additions can round inward while preserving the
intrinsic minimum; an uncapped minimum-size overflow remains valid when no smaller endpoint preserves that minimum.
Bounded trailing insets are checked against the represented parent far edge;
End alignment uses the represented line endpoint. `Measure` returns the preferred extent; successful `ApplyLayout`
publishes at least the occupied represented extent plus padding, and reports overflow when that content or its
absolute geometry with padding exceeds its bounds.
Scratch generations retire an older arrangement after a nested pure measure replaces its reusable records.
`ClearChildren` retires entry records before detach callbacks, preserving options and layouts authored for newer children.

### Reusable form composition

`FormPanel` and `FormField` are opt-in retained layout owners. The application explicitly selects one or two
uniform field columns and labels above or beside editors; no hidden breakpoint or settings schema chooses those
values. Beside labels share a measured width across currently visible fields, capped to leave the editor lane
usable. Description and error text wrap in the editor lane. Flow direction mirrors rectangles without changing
logical ownership, tab order, editor identity, selection, draft or composition. Footer actions remain separate
siblings arranged by their owner, using the existing measured-action helper when required.

A form supports at most 256 logical field slots. `AddField` requires a nonempty, unique identity and bounds its
identity/initial label to 65,536 UTF-16 units. Arbitrary visible non-field children fail form layout rather than
being silently ignored. A field owns one editor and three text parts; a custom editor returning `E_NOTIMPL`
uses the explicitly supplied preferred editor size, whose default is 180 by 32 DIP. Preferred width is at least
48 DIP and preferred height is positive. Above fields retain at least 48 DIP and beside fields at least 108 DIP
per column. These are library geometry minima, not a product touch-target policy. When the parent is narrower,
content retains those minima and reports overflow; the owner provides scrolling or a more suitable column tier.
Successful editor measurement can declare a larger intrinsic minimum, including numeric label/unit chrome. The form
retains that minimum and expands its uniform column width rather than collapsing the semantic editor or overlapping
neighboring columns. An unsupported editor's preferred fallback width may shrink to the lane while preserving 48 DIP.
Column widths settle in at most four preparation passes under the same aggregate shaping budget. A custom minimum
that keeps growing with each offered width fails with `HRESULT_FROM_WIN32(ERROR_RETRY)` before geometry changes.
The settled width is cached by input revision, typography, host, available width and horizontal origin, so unchanged
overflowing forms do not repeatedly rediscover their minima.

Measurement uses the same aggregate text budget as `FlexPanel`, numeric per-part caches and current host
typography. A form's shared-label measurement is pure: it does not mutate a field's layout inputs or revisions
while measuring. Unchanged preparation performs no new shaping. Complete numeric geometry is validated before
any bounds callback, using each part's final represented endpoint width as its cache key. The commit half performs no
measurement or allocation from translation rounding. Reentrant measurement/arrangement stops the older pass and preserves a newer completed
pass. Invalid constraints, arithmetic overflow and measurement failures leave pre-existing child geometry intact.

Validation is presentation only. `ValidationState::Error` requires a nonempty textual explanation; required,
description and validation metadata belong to the stable semantic editor (the inner text field of a
`NumericStepper`). Their UI Automation contract is specified in [Input and accessibility](UI_InputAndAccessibility.md).
`AddEditor` refuses a second editor and removes a newly added editor when binding fails. Setters contain named
allocation failures; sequential text/UIA publication can be partially applied when later allocation or reentrancy
fails, so callers inspect the returned HRESULT and retry or replace the presentation. They never commit values,
save settings or choose a focus successor implicitly. Removing or hiding a field follows the existing host
focus/capture/composition retirement rules; position-only reflow cancels obsolete pointer geometry without
reconstructing or blurring an available editor.
`ClearChildren` retires the entry snapshot's semantic pointers before detachment callbacks run. A replacement editor
bound by such a callback remains owned and recognized after the older clear returns, and a second editor is refused.

Registered `std::function` callbacks own shared callable storage. Dispatch retains that storage rather than copying
the target, preserving mutable callback state across invocations without allocating callback storage during dispatch,
including when the callback replaces its registration or retires the sender. TagPicker selection notification arguments refer to a
dispatch-owned snapshot for the duration of the call. Callers that continue the original
action release their dispatch snapshot before revalidating the sender's lifetime: final captured-resource destruction
can itself retire the sender. A destroyed sender consumes the accepted input without further
mutation, invalidation or focus changes through its stale pointer.

If replacement callback-storage allocation fails, the existing subscription remains installed. Passing an empty
callback clears the registration. Replacing private callback storage changes the library's C++ object layouts;
consumers must rebuild against the matching pinned headers and static library.

Layout accepts explicit bounds/DPI, reports usable minimums and prepares coherent visuals/hit rectangles together.
Do not reduce a touch target below the consumer's minimum to hide an overflow. Labels and state values may not clip
or overlap. Optional content is removed before required controls. Geometry is recomputed on relevant changes only.
Consumers own density tiers and responsive policy; DxUi contains no AV profile or XENEON dimension rules.

### Described native menu entries

Context menu sessions default to left-to-right flow. A right-to-left session mirrors menu columns,
uses right-to-left text shaping, and opens submenus toward the left before flipping to the right when
the work area requires it. It does not reverse the characters in any menu string. The popup's hit
geometry and keyboard navigation follow the same flow direction.

The [described-menu plan](../Plans/WIP/MenuDescriptions_2026-09-21.md) extends native command
rows with opt-in literal secondary text. Standard/Toggle/Radio/Info rows must measure both fields
at the final monitor-constrained width, including the scrollbar lane, then publish matching
paint/hit geometry. The lane decision uses the viewport the popup will actually have: the requested
height rounded to whole device pixels inside the work area. Overflow of at most half a device pixel
is rounding slack; it neither reserves the lane nor scrolls, so the lane reserved before sizing and
the final scrollbar always agree at fractional scales. Both fields wrap without silent truncation.
Retain prepared layouts across unchanged paints. When creation or measurement of one optional
description row fails and its primary label can still be prepared, keep that command row using
ordinary primary-label rendering and preserve its command/accessibility identity. Failures in shared
format/resource preparation or later scrollbar-width/DPI reflow still abort the popup if preparation
cannot complete; this row fallback does not promise recovery from every reflow failure. A popup whose
session closed while it was being positioned is never shown. Long individual
rows may exceed the viewport: keyboard navigation reveals their beginning and wheel/scrollbar
interaction exposes the remainder. Preserve existing one-line behavior when secondary text is absent.
A described row holds one native text layout for both fields, because the shaping storage a layout keeps is
mostly per layout (about 17 KB) and little per character (about 30 bytes): its text is the label, an empty spacer
paragraph and the description. The description range takes the small text format's font (family, size, weight,
style, stretch, locale) and the layout its tab stop, since only the description can hold a tab, while everything
else a layout holds for all its text comes from the body format, which the host creates like the small one.
The two fields keep the geometry two separate layouts gave them. Both wrap at the row's text width, the label at
its own height, and the description starts a fixed 3 DIP gap below the label's height rounded up to whole DIPs:
the spacer paragraph's font size is set per row and width until its height is exactly what that distance needs, and
a layout whose spacer cannot reach it fails the preparation like any other failure. The row is 16 DIP of padding
around the two rounded heights and the gap. One call draws the row: the label in the row's text color and the
description, through a drawing effect on its range, in the accelerator color of the row's state, so disabled,
hovered and plain rows keep their colors, and color glyphs, tab stops and line breaks stay as in a layout of its
own. The effect is one brush per popup and device, recolored for each row and recreated with the device. Rows never
share a layout, whatever their text; command IDs, accessible identities, row state and hit rectangles stay
independent. DPI changes prepare a fresh coherent set, and scrollbar reflow narrows each row to an absolute final
width from the width it was prepared at, so repeated reflows cannot narrow a row cumulatively.
Qualification is still in progress; this is not a consumer or native-platform acceptance claim.

### Localized adaptive layout acceptance

The shared implementation and synthetic acceptance below are qualified by the
[localized adaptive layout plan](../Plans/Done/LocalizedAdaptiveLayout_2026-09-19.md) and its
[final native receipts](../../Measurements/LocalizedAdaptiveLayout/2026-09-20/native-ci-main-78b3/README.md).
They extend existing controls and add no new control kind to the catalog. Consumer adoption,
physical application DPI presentation and real assistive-technology journeys require separate evidence.

- A measured action group accepts caller-owned ordered controls, current typography, available DIP
  width and spacing. Wrap whole controls to further rows; a single overlong label may wrap and grow
  its control. Preserve model, visual, keyboard and accessibility order. Never silently shorten an
  action label, reduce font size, or move a caller-required action into an overflow menu.
- Recompute desired size after text, font, DPI, visibility, validation and available-width changes.
  Prepare one coherent layout for paint, hit testing, tooltip and UIA bounds before publishing it.
  Fixed-control counts and equal-width slots are not valid substitutes for text measurement.
- Measure status/badge text with its icon and padding, as well as action text. An application-owned
  header may reflow its regions, but a status container must not crop a complete localized label.
  Shorter translations alone do not qualify layout: include deliberately long French labels.
- In a constrained composition, reserve measured fixed-region space before arranging scrollable
  content. No child action may paint or receive input through a sibling footer. Qualify several
  stacked cards and variable-height action groups, not only an isolated group on an empty canvas.
- Wrapped Unicode labels/values expose complete text, preserve underlying values and respect text
  boundaries. Applications choose path separator preferences and own copy commands. Filename,
  filesystem metadata and operation policy do not belong in shared controls.
- A scrolling body with separately arranged actions supports short viewports. Reflow preserves a
  stable scroll/focus anchor, clamps offsets and makes the focused control reachable. The consumer
  supplies work-area constraints; a child control does not move or activate the host window.
- Disclosure preserves caller-owned state, exposes expanded/collapsed semantics and removes hidden
  children from navigation, input and rendering work. The chevron's rotation runs on its own animation ticks: it
  starts at the first one, so a state changed while the button or an ancestor is hidden rotates once it is shown
  again, from where it rested, and a gap in the ticks (hidden mid-rotation, or a stalled thread) advances it at most
  100 ms, so it resumes where it paused instead of jumping to the end. Reduced motion snaps it. Checkbox checked state remains distinct from
  focus/hover/pressed/disabled; checked paint and UIA Toggle always agree. Checkbox honors opt-in
  `SetMultiline(true)` for its complete caption. Measure Body text within the control width minus
  36 DIP and reserve 6 DIP of vertical padding; its default remains single-line.
- Grid summary rows are bounded and match hit/selection geometry. Complete selected-item text can
  be arranged in a separate wrapped detail view without duplicating the grid's data authority.
  Multiline cells honor `SetLineClamp` and available complete-line height, with an ellipsis when
  content is omitted. Explicit paragraphs follow the same contract as automatically wrapped lines.
  Trailing line breaks (CR, LF, NEL, VT, FF, LS, PS) and blank lines after them are not content: they
  add no ellipsis and do not shift centring, and the ellipsis follows the last visible character. When not even one line fits, the
  first line is centred and clipped to the text area like a single-line cell, never left blank or
  painted into neighbouring rows. A partially visible row keeps its full-cell layout and is clipped
  without reflow or recentering. Rendering does not shorten model, clipboard, tooltip or UIA values;
  hovering offers the full value whenever paint omits, trims or clips text, or a horizontally
  scrolled viewport hides part of it. Consumers reserve sufficient row height for readable text.
  Every visible multiline value keeps its shaped layout from paint to paint in a table keyed by value (identical
  values share one layout, and values whose visible text is the same share its omitted-tail layout): a repaint of
  unchanged cells shapes nothing and scrolling either way shapes only the rows that enter. The table grows while the
  values of the current and the previous paint crowd one of its sets, up to 16,384 entries, and returns what a paint
  no longer uses. Keys round the layout box to 1/64 DIP, so cells of one size share an entry wherever scrolling puts them (its
  layout keeps the exact box of the cell that built it, which the others differ from by far less than a pixel).
  A value shapes only what it can show: whole paragraphs until the lines that can show are filled (the clamp, or fewer
  when the cell's height holds fewer at 0.8 em a line) and, within them, a prefix that is final once it lays out a line
  beyond the one after those lines or, unwrapped, once it overflows the cell with no right-to-left or
  directional-control text (such text shapes its whole line). A first prefix sized from the font that falls short
  doubles at most three times, then takes its paragraphs. An unwrapped line's height, and so its centring, comes from
  the text it shows. An entry keeps the text it was shaped from, with how the value continues after it (no break, a
  break, a CR LF pair), so a value far longer than a cell can show is retained by that prefix, any value starting the
  same way shares it, and its repaint shapes nothing; a key longer than 4,096 units, or twice the first prefix of a
  larger cell, is laid out per use (a whole long right-to-left line). The omission marker ends the visible text in that
  text's own direction, read by character (an emoji or other symbol has none): in a left-to-right grid a
  right-to-left last line gets it at its left end. Trailing lines of white space or of default-ignorable characters
  (zero-width space and joiners, directional marks, embeddings and isolates, variation selectors, byte order mark, soft
  hyphen) add nothing. Layouts are shared by paint and the tooltip; clean composition does no shaping. Qualification
  is tracked in the grid overflow WIP plan.
- Single-line Grid captions keep their layouts in the same table, keyed by the whole caption (up to 4,096 units; a
  longer one is laid out per use), and paint exactly as a centred-text draw of the caption would: unwrapped,
  vertically centred and clipped to the text rectangle. A leading-aligned caption on one paragraph shapes only the
  prefix that overflows its cell, when that prefix holds no right-to-left or directional-control text; centred and
  trailing captions overflow by their whole line, so they shape all of it. The hover check reads the painted layout.
- A painting Grid keeps the layouts of what it drew last (its visible cells and those of a partly visible row, however
  far it has scrolled) and the string storage of the entries it released, so that later keys allocate nothing. A Grid
  that stops painting returns all of it, since no later paint will release what its last paint used: hidden itself or
  under a hidden ancestor (a tab page that is not selected, a collapsed panel or page host), in a hidden embedded view,
  removed from its host or given another model, it releases its layouts, their key strings and its tables at once, and
  showing it shapes only what it then shows, as its first paint did. `Control::OnHidden` carries the notice and panels
  and page hosts forward it; the painting path, its allocations and its layouts' lifetime while painting are unchanged.
  A hidden or minimized native window keeps its controls' layouts, as it keeps its swap chain for a quick show.
- In right-to-left flow a control's text format reads right to left, so captions and titles use leading alignment
  (their start side, beside a mirrored indicator or at a tab's right edge), never trailing.
- A control's flow direction and density are inherited through its parents (a root takes its host's density), and a
  parent's own change is announced to its children (`OnFlowDirectionChanged`, `OnDensityChanged`). Moving a control to
  another parent, a host's root or a page (`ControlHost::SetRoot`, `PageHost::SetPage`; `Control::Reparent`) announces
  the values it now inherits the same way: once, after it stands in its final place, and only when they differ. A
  control with its own flow direction or density, a child that inherits what its parent already has, and a tree being
  torn down hear nothing. So a control that keeps an arrangement for them (steppers, the color picker, tab headers,
  stack layouts, tree and grid row metrics) is current in its new place, and one that keys its layout on them (the
  menu bar) was never stale. A child leaves a panel by moving its owning pointer out of `Panel::GetChildren()` (while
  the panel and the child's host still exist); the empty slot is skipped by every panel operation.
  PageHost participates in the same inheritance chain as Panel; its pages resolve through the PageHost rather than
  directly through the host. Root replacement snapshots the incoming control's previous inherited values before
  interaction-reset callbacks can retire its old parent, then reparents without dereferencing that old chain.

The consumer chooses information hierarchy and whether repeated text is useful. Shared controls
must support a single semantic heading with associated labelled values and complete exact-value
detail, without requiring hidden duplicate labels or repeated names to obtain full UIA text.
Filesystem name deduplication, pane navigation, operation decisions and recovery remain consumer
responsibilities. A pixel/bounds pass does not establish readable content or complete interaction.

Layout/measurement runs on invalidation outside clean composition. Cached resources and history
remain bounded; hidden content requests no repaint/tick solely to maintain a visual animation.
French long-sentence fixtures at 96/144/192 DPI and constrained width/height qualify these contracts
in both native and supplied-device hosts. Library fixtures are synthetic and consumer-independent.

Implemented and qualified, 2026-09-20: `ArrangeMeasuredActions` provides ordered, allocation-free
geometry from already measured sizes, with unchanged outputs on invalid/capacity/overflow failure.
Hidden `{0,0}` entries retain their index and receive empty bounds. The caller measures overlong
labels at the available width and reserves the returned action height before laying out body content.
`Button::SetMultiline` is opt-in for Standard text buttons, with 12/8 DIP per-side text padding.
A multiline `TextField` draws its caret only inside its text viewport, at every DPI and editable or read-only: a caret on a
partly visible line is cut at the viewport's edge, and one the wheel scrolled out of view is not drawn.
Checkbox honors the inherited multiline option with the indicator/text geometry above. Disclosure
uses acknowledged state and exposes the ExpandCollapse contract in the input/accessibility domain.
Native x64/ARM64 Debug, Release and ASan suites, WARP scenes and reviewed five-theme gallery sheets
qualify these library capabilities. ARM64 Menu records nine foreground capability skips per profile;
real consumer screen-reader, physical mixed-DPI and application adoption remain separate gates.

DxUi owns the implemented controls in `src/Controls` and their standalone tests in `Tests/Controls`.
Source is edited in place here; historical import records do not freeze it. Consumer-specific bridges remain
separate work as recorded in capabilities.json.

### Public catalog and gallery

The public API exposes Panel, PageHost, CardPanel, Label, Button, Toggle, Checkbox, RadioButton, RadioButtons,
ProgressBar, PageIndicator, ThroughputGraph, Slider, Toolbar, MenuBar, TabControl, ColorSwatch, TextField, ComboBox, TagPicker,
StatusStrip, PopupLayer, StackPanel, ScrollPanel, TooltipLayer, Tree, Grid, Splitter, NumericStepper and ColorPicker.
GetControlCatalog returns immutable
descriptors; CreateControl constructs the selected kind and returns E_INVALIDARG for unknown kinds, leaving the
existing result intact on failure. Controls with models are configured by the caller; the factory does not invent
application data. Constructors and examples are ordinary code, with no source-code generator or second control copy.

Adding a control requires an aligned public declaration/implementation, catalog entry/factory case, meaningful
behavior tests and a populated gallery tile. `gallery.ps1` generates light, dark, rainbow light/dark and high-contrast
PNG sheets with interactive-state variants, plus a supplied-device toggle/slider image. Gallery rendering supplements
runtime tests; empty-control construction alone does not prove selection, keyboard, editing or accessibility behavior.

### Page indicator

`PageIndicator` is a non-scrolling position strip. `SetPageCount` accepts 0 through 16. Fewer than two pages paint
nothing, report empty hit bounds, and ignore pointer and keyboard input. `SetSelectedIndex` clamps and does not invoke
`SetOnSelected`. A click on a dot, Left/Right, Home and End change the selected index and notify once. The control
does not wrap. Idle dots use subdued text; the selected dot uses the theme accent. Preferred strip height is
`PageIndicator::kStripHeightDip` (20 DIP). Consumers that draw a matching strip without hosting the control MUST use
the same DIP radius, selected radius and gap constants.

### Progress bar

`ProgressBar` paints a determinate fill, two segmented values or an indeterminate segment. The track is 2 DIP, or
4 DIP while indeterminate, unless `SetTrackHeightDip` sets a height. The indeterminate segment is 40% of the track.
With motion it sweeps from before the track to past its end every 2,000 ms. An enabled, visible bar requests host
ticks, and each tick advances the sweep by the elapsed time and invalidates the host. A long gap advances only by its
remainder of a loop. A tick time earlier than the previous one re-seeds the timing without moving the segment. Under
reduced motion the segment rests centered, from 30% to 70% of the track: the bar requests no ticks, `Tick` reports
false and every paint is identical. Restoring motion resumes the sweep at the next paint.

### Slider

Painted chrome and pointer geometry are independent. The gray chrome disc is ink only. Hit testing may be larger than any
painted disc so a fat finger can grab the thumb. Neither layer is sized from the other. While a touch contact drags the
slider, a translucent touch halo paints the 48 DIP grab area, so feedback shows around the finger that covers the thumb.

```
  cross-axis (horizontal slider, DIP)

  48  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  hit band (unpainted); touch halo while a touch drags
  24  ████████ chrome disc (fixed) ████████
  20                                           hovered inner thumb
  16                                           pressed inner thumb
  14                                           resting inner thumb
   6  ══════════════════════════════════════    track
   0
```

| Layer | Rest | Hover | Pressed | Role |
| --- | --- | --- | --- | --- |
| Track | 6 DIP capsule | — | — | Paint. Fill and remainder share that thickness with no extra stroke. Inset 12 DIP from each end. |
| Inner thumb | 14 DIP | 20 DIP | 16 DIP | Paint. Accent fill matching the track: visible at rest, with a thin chrome rim on hover and clear pressed feedback. |
| Chrome disc | 24 DIP | 24 DIP | 24 DIP | Paint. Opaque gray disc under the inner thumb. It does not scale with hover or press and is not the hit target. |
| Touch halo | — | — | 48 DIP while a touch drags | Paint. The accent at 24% opacity under the chrome disc. It grows from the disc to the grab area as the drag begins and shrinks away after the release; a cancelled drag removes it at once. High contrast draws it as an opaque 2 DIP accent ring. Paint is clipped to the Slider's bounds and any active ancestor viewport and host clips; a short Slider shows only the part that fits and never enlarges its hit target. A mouse or pen drag never shows it. |
| Hit band | 48 DIP | 48 DIP | 48 DIP | Pointer only. Centered on the track. Clipped to control bounds when the control is shorter. Not painted. |
| Thumb grab | 24 DIP radius from thumb center | same | same | Pointer only. Half the hit band. A contact in this circle drags from the current value; a contact on the track outside it seeks. |

Keyboard and `RequestValue` ease the painted inner thumb to the new value; pointer drags and `SetValue` snap.
Reduced motion snaps every visual. Keyboard steps still use `SetStep` / `SetLargeStep`. Consumers that need a
fat-finger target size the control to at least the 48 DIP hit band; they do not enlarge the chrome disc to match. Give the
Slider 48 DIP of cross-axis room to show the full touch halo and keep neighboring content outside its bounds; a scrolled
viewport or host edge clips both the halo and the control's rendering. Popup and menu overlay layers paint above the halo.
An acknowledgement through `SetValue` snaps and stops position animation even when it equals the accepted target.
Non-finite values, range limits and steps leave the previous valid state unchanged. A range whose span overflows
also leaves the prior range intact. Off-center thumb grabs retain the pointer offset, continue outside bounds under
capture, and restore the initial value on cancellation without a later release committing it.

Each control also requires accurate usage documentation in `docs/controls.md`. Code changes review affected docs
and regenerate changed visuals into `docs/gallery` under [the documentation contract](../Core/Core_Documentation.md).

### Splitter

`Splitter` is a two-pane separator whose panes the consumer positions from `GetFirstPaneBounds` and
`GetSecondPaneBounds`; the control owns no children. `SetOrientation` selects a vertical bar (first pane leading,
mirrored in right-to-left flow) or a horizontal bar (first pane above). The position is the DIP offset of the
separator's leading edge from the control's leading edge. `SetPosition` clamps between `SetMinimumFirstPane` and the
extent minus the thickness and `SetMinimumSecondPane`, repaints and does not notify; a position set before the first
layout keeps its value until bounds arrive. Only the separator plus `kHitSlopDip` (2 DIP) on each side is hittable;
the pane areas fall through to the controls beneath. Dragging previews (`SplitterChangePhase::Preview`), release
commits, Escape while dragging and capture loss restore the drag-start position and notify `Cancel`. Left/Right
(vertical bar) or Up/Down (horizontal bar) move `kKeyboardStepDip` (8 DIP), Shift moves `kKeyboardLargeStepDip`
(32 DIP), Home and End go to the limits; keyboard moves and `RequestPosition` notify `Commit` once. The separator
paints the theme border at rest, a border/accent blend when hovered and the accent while dragging, with a three-dot
grip; the horizontal or vertical resize cursor applies over the hit band and during a drag. UI Automation exposes it
as a Thumb (a focusable separator) with RangeValue: the position between the limits the pane minimums allow, small
and large changes equal to the keyboard steps, and `SetValue` committing once like the keyboard (refused mid-drag).
Consumers name it with `SetAccessibleName`. The persisted position is consumer state. In an EmbeddedHost the consumer applies pane bounds on its next preparation, not inside the change
callback. That bounds revision keeps the drag while the splitter stays in the tree and it and its ancestors stay
enabled and visible
([`Rendering_EmbeddedD3D11.md`](../Rendering/Rendering_EmbeddedD3D11.md)).

### Numeric stepper

`NumericStepper` is a Panel that owns a `TextField`, an increment and a decrement icon button, an optional leading
label (`SetLabel(text, widthDip)`) and an optional trailing unit (`SetUnit(text, widthDip)`); label and unit are
painted, not child controls. Values clamp to `SetMinimum` / `SetMaximum` (default ±1e9) and format with
`SetDecimals` (0–6) using `.` as the separator; `ParseValue` accepts an optional sign, digits and one `.` or `,`
fraction with surrounding whitespace. Text that parses previews immediately (`NumericStepperChangePhase::Preview`)
and opens an edit whose start value Escape restores (`Cancel`). Enter, focus loss, the buttons, Up/Down (Shift:
`SetLargeStep`), `RequestValue` and `Nudge` commit once; text that does not parse reverts to the committed value,
and an open edit whose text no longer parses ends as `Cancel`, never committing the preview it abandoned.
`SetValue` clamps, rewrites the text and never notifies; `SetDecimals` re-rounds the value the same way. A step finer
than the shown decimals still moves one shown unit. Text changed without focus (UI Automation's ValuePattern) has no
later Enter or focus loss and commits, or reverts, at once. The step buttons draw icon glyphs, so they carry UI Automation names:
English "Increase" and "Decrease" by default, and the consumer's complete localized names through
`SetStepButtonNames(increase, decrease)`. An
unchanged text is not rewritten, so the field keeps its caret and undo history. Disabling the stepper disables its
three children.
Right-to-left flow mirrors label, field, unit and buttons; label and unit read right to left with leading alignment.

### Color picker

`ColorPicker` is a Panel with a saturation/value field (`kFieldDip` square), a vertical hue strip, new and current
swatches, R/G/B `NumericStepper`s (0–255), a hex `TextField` (`#RRGGBB`, `RRGGBB`, `#RGB`, `RGB`) and OK/Cancel
buttons whose captions come from `SetLabels`; the library ships no localized strings for it. `Labels` also sizes the
R/G/B caption slot (`channelLabelWidthDip`, 14), the hex caption slot (`hexLabelWidthDip`, 26) and each swatch, whose
caption it fits beneath (`swatchWidthDip`, 44), so translated captions are not cut to the English widths; a picker
wider than `kDefaultWidthDip` leaves room for wider swatches. A width that is not finite takes its default, and the
widths stay within 0 and `kMaxLabelWidthDip` (4,096); the hex caption is drawn in the slot `Arrange` reserves for it.
`Labels` also names the channel steppers' step buttons (`increaseRed` … `decreaseBlue`, English "Increase red" …
"Decrease blue" by default) as whole phrases, since word order varies between languages. `SetColor` sets the
editing and current colors without notifying; `SetCurrentColor` changes the reference swatch only. Pointer drags on
the field or strip, typed component values, hex text and `SampleColor` (host eyedropper) preview
(`ColorPickerChangePhase::Preview`); OK, Enter and `Commit` copy the editing color into the current swatch and
notify `Commit`; Cancel, Escape, `Cancel` and capture loss during a drag restore the current color and notify
`Cancel`. Left/Right step saturation (mirrored in right-to-left flow) and Up/Down step value by 1/255, Page Up/Down
step hue by one degree; Shift multiplies by ten. Grays keep the last hue and black keeps the last saturation so the
field marker does not jump. `HsvFromArgb`, `ArgbFromHsv`, `ParseHexColor` and `FormatHexColor` are public helpers.
The field or component being typed in keeps its text, caret and undo history while the others follow; focus loss,
Enter, OK and Cancel normalize it (`#RRGGBB`). A canceled component edit restores its value and the picker follows it
with a preview. The field paints the pure hue under unit-space white and black gradients placed by a brush transform:
its three gradient brushes are created once per Direct2D device (held by reference, so a recreated device cannot
alias them) and neither hue changes nor layout moves recreate them; a host change releases them and the device
reference, and the next paint on the new host's device makes them again. A picker moved to another parent or host is
arranged for the flow direction its new place gives it (a move announces it, as the layout list above says: a picker
moved out of a right-to-left parent no longer keeps its children mirrored) and lays out and paints as one created
there, whatever the new host's dpi, theme or density. Alpha is always opaque.

### Consumer-selected popup row minimum

ComboBox exposes `SetMinimumPopupItemHeight` / `GetMinimumPopupItemHeight` for an optional per-instance DIP minimum.
Zero preserves the theme default. Non-finite, negative or greater-than-4096 inputs leave the prior setting unchanged.
The effective height is the greater of the theme row height and the minimum, consistently used for layout, painting,
hit testing, wheel/scroll geometry and keyboard visibility. Changing an open popup invalidates layout and hover,
ends thumb dragging and keeps the selected item visible. No change event or selection is generated by changing density.
Consumers provide sufficient viewport space or a larger selector view; DxUi never shrinks rows to conceal overflow.

### High-contrast primary buttons

Primary buttons preserve the opaque selection-fill/selection-text color pair in high contrast across idle, hover,
pressed and focused states. Decorative blends, animation strength and heuristic text-color substitution must not
dilute that pair. Disabled primary buttons use the button surface and disabled text, retain an opaque border and
hide focus. Enabled focus remains visible for pointer and keyboard and uses the undiluted palette focus color.
The normal light/dark appearance keeps its existing visual treatment.

### Tree rows at the viewport edge

A tree paints its rows only inside its viewport, the frame's interior within its 2 DIP inset. A row can straddle the
viewport's top or bottom edge: the viewport need not hold a whole number of rows, and a thumb drag can leave a fraction of
one. Such a row shows only its part inside: its fill, focus ring, text, badge and expander are clipped at the edge. The
same applies to rows an expansion or collapse animates. The reorder marker is clipped 1 DIP past the viewport, inside
the inset, so an insertion line on the edge keeps its full 2 DIP.

### Tree row drag

`Tree::SetReorderEnabled` arms a pointer drag on a row (not the expander or the scrollbar). After the pointer moves
at least 4 DIP, the tree draws an insertion line on the top or bottom half of the row under the pointer, or highlights
the row when the pointer is in its middle and that row has children. Release calls `ITreeDelegate::OnTreeReorder`
once with the source id, the target id and `TreeDropPlace` (`Before`, `After` or `Inside`). The tree does not change
the model. The dragged row's own visible descendants (the deeper rows that follow it) are never targets, so a drop
cannot make a row its own ancestor. Escape and capture loss cancel and do not call the delegate; so do disabling or
hiding the tree or an ancestor, and `SetModel`. `NotifyDataChanged` during a drag re-resolves the dragged row by id
and clears the target until the next pointer move; a removed row ends the drag. A wheel scroll during the drag
retargets the row now under the pointer, and moving within one drop zone repaints nothing. The release point
decides the drop, a second (right) button cancels, and model id 0 is an ordinary row. A click that does not travel
4 DIP selects as before, does not reorder and reports its release unhandled. Row drag is pointer-only: consumers
provide the keyboard or command equivalent. Row `iconText` in the private-use range uses the icon font; any other
icon text keeps the small UI font, as in Grid. With multi-select the drag keeps this single-source contract: see
the next section for how a press on a selected row and a drop relate to the selection.

### Tree multi-select

`Tree::SetMultiSelectEnabled(true)` opts a tree into several selected items. It is off by default. The call is silent.
Enabling starts with the selected item alone; disabling keeps the focused item if it is
selected, else the last selected one, else nothing. The selection is a `GridSelectionModel` (the class Grid uses), so
its ids, anchor and gestures are Grid's; the model stays UI-thread only.

The focused (current) item and the selection set are distinct. `GetFocusedItemId` reports the focused row, which owns
the focus ring and is where the keys start. `GetSelectedItemId` reports the primary selected member; focus-only moves
do not change it. A replacement or range gesture makes its row primary, toggling or adding a row makes that row primary,
and removing the primary chooses the last remaining selected row in visible order. A model change preserves the primary
while it remains selected, otherwise choosing the last remaining member. Ctrl+A preserves the primary, or uses the
focused row if no primary exists, falling back to the last visible row when there is no focus. `GetSelectedItemIds` returns the selection in
visible order and `IsItemSelected` tests one id; both also answer for a single-select tree (its one item, or none).
`SetSelectedItemId` selects one item alone, `SetSelectedItemIds` selects the listed ids that are visible rows (the last
listed visible row is the primary, focus and anchor) and `SetFocusedItemId` moves the focus alone; all are silent.

| Gesture (multi-select on) | Effect |
| --- | --- |
| Click | Selects the row alone; it is the anchor. |
| Ctrl+click | Toggles the row and focuses it; the touched row becomes the range anchor, including when toggled off. |
| Shift+click | Replaces the selection with the visible range from the anchor to the row, which is focused. With no anchor the focused row is one; with neither, or when the anchor is no longer a visible row, the row alone. |
| Up, Down, Home, End, Page Up, Page Down | As before for a single tree: select and focus the row reached. |
| Shift + those keys | Range from the anchor to the row reached, like Shift+click. |
| Ctrl + those keys | Moves the focus alone. |
| Left, Right | Collapse or expand the focused group and leave the selection alone, even with Shift or Ctrl held. Where they move to the parent or to the first row of an expanded group they follow the keys above (plain selects, Shift extends, Ctrl moves the focus alone). |
| Ctrl+Space | Toggles the focused row (Space alone still invokes it). The space character it sends is not typeahead. |
| Ctrl+A | Selects every visible row. The primary stays if still selected; otherwise the focused item becomes primary, or the last visible row if there is no focus. Focus stays, or moves to the first row if there is none. |
| Typeahead | Selects the match alone, as a plain key does. |
| Expander | Expands or collapses and focuses its row; the selection is untouched, even with Ctrl or Shift held. |
| Right-click | On a row of a multi-selection it keeps the selection and focuses the row, so the command applies to `GetSelectedItemIds`; on any other row it selects that row alone. |
| Double-click | Activates as before; with Ctrl or Shift held it leaves the selection alone. |

A first key on a tree with no focused item starts from the first row, which only takes the focus, so that
Ctrl+Space selects it and Shift+Down selects it and the next. Grid's current row is also independent of its selected set.
Ctrl+Up and Ctrl+Down move Grid focus without changing membership, Ctrl+Space toggles the focused row, and Space
toggles a checkbox in the active row. Shift movement extends selection from the anchor. In both controls Ctrl
toggles the touched row and makes it the new range anchor. A tree has no checkboxes and a focused item apart from its
selection, so it follows the list-view convention in which Ctrl with a movement key moves the focus and Ctrl+Space
toggles the focused item. Shift (range from the anchor, replacing the selection), Ctrl+click (toggle), Ctrl+A, the
right-click rule and the anchor are Grid's.

`ITreeDelegate::OnTreeSelectionChanged(id)` reports the primary selected member when nonempty membership changes;
focus-only moves use `OnTreeFocusedItemChanged(tree, id)`. `OnTreeSelectionSetChanged(ids)` reports the full membership
change (default no-op, so existing delegates compile), including an empty single-select selection. It is called once per
change after the primary callback when that callback has a member to report; `ids` is an owned copy in visible order.
The same selection is never reported twice: a click on the one selected row, Ctrl+A on a full selection, and a model
change that only moved rows notify the set not at all. State is complete before each callback, so a delegate reads the
finished selection from the tree, may change it, or may destroy the tree (nothing is touched after the call). The setters,
`SetMultiSelectEnabled` and `SetFocusedItemId` call nothing.

The selection survives what the model does to other rows. `NotifyDataChanged` and `SetModel` drop selected ids that are
no longer visible rows (removed, or hidden by a collapsed ancestor: a collapse that hides selected rows deselects
them, and expanding does not bring them back, as a single selection always behaved) and put the rest in the model's
order. A dropped focused item is cleared without a callback, as before. Scrolling changes nothing and a click after it
toggles the row under the pointer. After `NotifyDataChanged` the set holds visible rows only, and its membership tests are
linear in its size, as in Grid.

Row drag and the selection: a press with Ctrl or Shift is a selection gesture and never starts a drag. A press on a
row outside the selection selects it alone and arms the drag. A plain press on a row of a multi-selection (with
`SetReorderEnabled(true)`) keeps the selection while the pointer is down; the row takes the focus, and if the press
becomes a drag the drop names that row alone (`TreeDrop::sourceId`) and leaves the selection as it was, so a delegate
that finds the source in `GetSelectedItemIds` may apply the drop to all of them (the library moves nothing and invents
no multi-item drop). A release that never became a drag selects that row alone (one `OnTreeSelectionSetChanged`, no
second `OnTreeSelectionChanged`). Escape, capture loss, a second button, `SetModel` and a removed row cancel the drag
and leave the multi-selection intact. Without `SetReorderEnabled` a press on a selected row selects it alone at once,
as in Grid.

Painting: every selected row takes the selection roles (`selectionFill` and `selectionText`, or `selectionInactiveFill`
with text checked against its painted fill while the tree lacks focus; rainbow mode tints each selected row by its own
hash), and only the focused row shows the focus ring, even when it is not selected. Inactive Tree text prefers `text`
and Grid text prefers `selectionText` when the configured ink reaches 4.5:1 after compositing; otherwise it uses the
most contrasting neutral. High contrast retains the supplied HighlightText pair. A selected Tree focus ring contrasts
with the actual resolved row fill, including its rainbow tint.

UI Automation reports the selection set. The tree's Selection pattern reports `CanSelectMultiple` true and lists every
selected item in visible order; each selected item reports `IsSelected`; only the focused item reports
`HasKeyboardFocus`. `SelectionItem.Select` replaces the selection, `AddToSelection` adds the item (and focuses it;
adding a selected item changes nothing) and `RemoveFromSelection` removes it and leaves the focus; each goes through the
same callbacks as the gesture it stands for. `SetFocus` on an item moves the focus and leaves the selection. The events
follow WPF's rules for its selectors: a selection that became exactly one item that was not selected before raises
`ElementSelected` for it, any other change raises `ElementAddedToSelection` and `ElementRemovedFromSelection` per item,
and more than 20 changed items, or a selected item that left the tree, raise one `Selection_Invalidated` on the tree
instead; items named one by one also raise the `IsSelected` property change, and a republish that changed no
selection, or rows that only moved, raise nothing. A tree without multi-select reports what it always did, and its one
selected item raises the same events when it changes (`ElementSelected` for an item that became the selection,
`ElementRemovedFromSelection` when it is cleared), as a Grid's rows do; turning multi-select off removes the items it
drops. The events are raised with the same publish that raises focus changes (window hosts after the focus
announcement, embedded hosts from `UpdateAccessibility`). They reach a client subscribed to the window in a window whose
only semantic control is the tree as in any other: the window's root element stands for the tree, is the parent of its
items, and is where the invalidation is raised (see the collapsed semantic root and the
[selection events](UI_InputAndAccessibility.md#selection-events) in Input and accessibility).

### Grid focus, selection and inspection

Grid focus is a stable row identity independent of selected membership. `RequestFocusRow`, Ctrl+Up/Down and UIA
`SetFocus` move focus without selecting a row or changing the range anchor. `RequestFocusCell(row, column)` also makes
that column active and reveals it horizontally; it does not alter selection or anchor. `GetFocusedRowId` names the
focused row. A normal row gesture replaces membership and moves the anchor to that row; Shift extends from the anchor;
Ctrl toggles the touched row and moves the anchor there, even when it toggles the row off. Ctrl+Space toggles the focused
row. Ctrl+A selects all visible rows and focuses the final row in visible order. A single-selection Grid permits an
empty selection through Ctrl+Space or UIA removal. UIA AddToSelection is idempotent for an already selected row and
preserves the current anchor; in single-selection mode it refuses to add another row while one is selected.

Selection callbacks report membership changes and focused-row callbacks report focus changes. State is committed before
callbacks; callers stop if callback reentrancy retires the Grid. Model reconciliation drops rows that are no longer
visible and clears an anchor whose row left the visible model. UIA focus and selection operations do not activate rows.

Ctrl+C copies the selected rows in visible selection order and columns in displayed order. Fields containing quotes,
tabs or line breaks use quoted TSV syntax and embedded quotes are doubled; all other Unicode values are preserved. F1
inspects the full value in the focused row and active column. Touch double-tap on a cell opens the same persistent,
scrollable inspection surface. After a selection callback, inspection and activation resolve the touched stable row
again; inspection also resolves a nonempty column ID again and reads the current cell. Removing that row or column,
or replacing the model, cancels the action. Unnamed columns retain their ordinal contract. Group collapse reports
any replacement focus through the focused-row callback and stops when that callback retires the Grid.
RTL mirrors column geometry, adornments, keyboard left/right behavior and horizontal
scrollbar direction; offscreen cell focus reveals the cell using the same logical scroll offset.

### Localized built-in text

Consumers supply owned per-instance strings through `ComboBox::SetNoMatchesText`,
`Tree::SetEmptyStateText`, and the existing `Grid::SetEmptyStateText`. Empty overrides restore
English defaults (No matches / No data). ComboBox remeasures an open empty-result popup when
its string changes; Tree/Grid invalidate their empty view. String updates do not alter selection,
query text, focus or callbacks. DxUi does not load consumer numeric resource IDs or cache a
process-global language. Each product/module provides its current translated strings.

## Intended rich-text editor contract

The [owned Unicode rich-text editor plan](../Plans/WIP/RichTextEditor_2026-10-17.md)
tracks the retained RichTextEditor v0 preview now implemented in the library and
catalog. It is bounded to the existing 64 Ki UTF-16/128-history model, with D2d
shared native/embedded source TSF/IME/UIA adapters. Full rich-text/Markdown and
real OS/AT qualification remain pending; prototype evidence alone never
qualifies DxUi behavior.

The intended control owns source revisions, structured rich projections, edits,
selection and history; its source/formatted/read-only views preserve unchanged
Markdown bytes and unsupported content. It uses public DirectWrite/Direct2D and
existing host services without a hidden Microsoft editing/layout engine. Native
and embedded text/geometry/accessibility contracts, themes/DPI, cancellation,
read-only/disabled state and control lifetime remain mandatory. Embedded hosting
adds no private HWND/device/swap chain/worker/timer, and composition performs no
layout/raster/allocation/I/O/wait. Application persistence and assets stay app-owned.

Before full rich-text support is published, migrate canonical source/tests once, pin parsing and
extension conformance contracts, populate catalog/gallery/design-system/docs,
and close the native/embedded/platform/resource/performance gates. Consumer pin
and product adoption remain separate from library implementation.

## Implemented bounded rich-text preparation core

The reusable v0 source now belongs to DxUi: RichDocument.h, RichMarkdown.h and
RichTextLayout.h in include/DxUi and DxUi.RichDocument/RichMarkdownProjection/
RichTextLayout.cpp in src/Controls. The [core guide](../../docs/rich-text.md)
records exact API/limits. This additive preparation API is distinct from the
retained RichTextEditor preview above; it remains independently usable without a host.

The source model must preserve Unicode scalar/CRLF boundaries, immutable shared
snapshots, transactional selection/history and failure atomicity. Piece-aware
boundary/slice/edit operations must not flatten complete old/new source text.

Logical rich navigation and mouse hit stops use the shared Unicode 17.0.0 default
extended grapheme service. Prepared layout computes those stops in one forward
pass; glyph shaping and explicit ACP caret/range geometry remain DirectWrite
operations. Source edits and text services may retain valid scalar/CRLF offsets
inside an element. This does not qualify visual bidi/word navigation or physical
IME/assistive-technology behavior. Password mask indexing uses the same forward
walker rather than repeatedly recovering the entire preceding context.

PreparedLayout::DeletionRange validates its display offset and readiness, returns
the complete containing grapheme for an interior ACP, and uses the existing
prepared stops without allocation. Clear retires this query along with geometry.
At exact boundaries, backwards/forwards deletion chooses the ordinary neighbor;
an empty edge yields an empty range. Explicit source-range replacement remains
independent of this collapsed keyboard policy.
Immutable storage is a persistent B+ piece tree with maximum fanout 32 and at
least 16 entries per non-root node. Exclusive cumulative ends index pieces and
subtrees. Offset lookup is O(log P) for P pieces; a slice of K intersecting pieces
visits O(log P + K) entries. Edits copy boundary paths, share untouched subtrees,
skip fully removed middle subtrees, coalesce rewritten contiguous buffer slices
and rebalance before publication. Only the final root may collapse height.
No per-read cache or mutable shared index is retained. Worker reads use a captured
immutable snapshot, not a concurrent call into a mutating Document.
Validate future selections against the virtual old-prefix/insertion/old-suffix
before allocating source storage/history, including a newly joined CRLF. A
same-content replacement only acknowledges its exact valid selection; it allocates
no source/history, advances no revision and preserves undo/redo. Stage every
changed tree and required history copy before publishing. Allocation failure in
replacement, transient replacement, reset, undo or redo preserves all state.
Document source-taking operations borrow std::wstring_view synchronously and
copy changed source into immutable owned buffers before returning. Recognize
valid no-ops before creating owning strings or staging history. This avoids
Debug iterator-proxy allocations from string moves and unnecessary default
history containers. Never retain the caller's view.
Default, moved-from and fully deleted snapshots behave as empty queryable values.
ChangedRangeFrom reports an unchanged source, a tracked conservative replacement
interval or a whole-source fallback. Both coordinate spaces preserve scalar/CRLF
slice boundaries, including CRLF newly joined by replacement. Tracked intervals
can contain unchanged context. Immutable revision metadata retains a 32-edit
active page and at most three sealed numeric pages: the latest 96 edits are
always available, with at most 128 retained. Undo/redo and copied-document
branches resolve a bounded common ancestor; reset and missing history fall back.
Transient previews carry edit metadata without creating undo/redo or retaining
intermediate preview buffers. Queries allocate nothing and mutate no cache.
Whole-text transient input must be validated, then narrowed by common prefix/
suffix to a source replacement with scalar/CRLF-safe boundaries. Compare shared
fragments without flattening old source; reuse untouched buffers and tree paths.
Same-content previews acknowledge selection without allocating source/history or
advancing source revision. No caller view is retained after the call.
Stage source, revision metadata and history before publication; failed metadata
allocation or page rollover preserves all state and reports no phantom change.
Current storage is bounded at 64 Ki UTF-16 units/128 history items. The earlier
projection-block checkpoint did not establish incremental geometry, host scheduling
or large-file acceptance. The guarded retained-geometry candidate below is now
connected to both hosting modes; its remaining qualification is explicit.

PreparedProjection prepares the same v0 grammar as Project in immutable physical
source-line blocks, including CR/LF/CRLF endings. Source/display prefixes index
local coordinates; every source ACP, display unit and both source affinities must
match the canonical flat projection exactly. Adjacent equivalent styles coalesce
to the same flat runs. Local block revisions are zero; the service's revision and
mode identify the complete preparation. Source identity matters independently of
the caller's numeric revision, including copied-document sibling branches.
Tracked changes widen to complete old physical lines plus a neighbor on each side;
unchanged outer payloads remain shared. Reset/missing ancestry/mode changes rebuild
the complete bounded source. Two empty sources may remain semantically unchanged
across unrelated identities. Adjacent equal physical lines may share one immutable
payload. Preparation statistics count processed/reused block slots and the widened
source slice, not individual parser calls or timing acceptance.
Stage every new payload and prefix index before nonthrowing publication. Failure
must preserve the entire prior state, revision, statistics and block identities.
Clear releases all owned prepared state; default/moved/retired services reject
content/mapping queries. Valid mapping and unchanged preparation allocate nothing.
DisplayText is explicit materialization; ordinary updates slice only changed
context. Prefix-index rebuilding remains linear in physical line count. This
line-local widening is not valid for an unimplemented CommonMark block parser.
Preparation is owner-thread work; the service owns no worker/device/window/timer.

Prepared DirectWrite state uses WIL ownership, caller font settings and borrowed
factory/target services. Invalid projection/style/settings must leave the prior
projection/geometry generation intact. Explicit retirement rejects geometry while
preserving the separate document. Raster drawing is preparation; embedding must
not call it from composition. No HWND/device/thread/timer/worker belongs to the
core. The caller supplies scheduling and stale-result/lifetime checks until the
retained/shared preparation integration is implemented.

## Retained RichTextEditor v0 preview (D2c)

The bounded retained-geometry component is functionally qualified:
PreparedDocumentLayout stages immutable projection clones and DirectWrite chunks
jointly. Geometry chunks target 1,024 source units at physical-line boundaries;
127 chunk slots plus one optional whole-layout compatibility slot cap published
layouts at 128, with bounded compaction before further chunk allocation.
Source edits make geometry unavailable immediately while retaining old chunks
for owner-thread reuse. Full retirement discards the active graph. Options,
factory identity, source identity/change interval and mode govern reuse; equal
numeric source revisions alone never establish compatibility. A nonfinal chunk's
phantom terminal line must not add height. Native/embedded geometry, exact WARP
pixels, complete observed C++ allocation schedules and source-limit resource
evidence pass in all three x64 profiles, with all ARM64 cross-builds and tooling.
The [qualification packet](../../Measurements/RichText/2026-10-18-retained-geometry/README.md)
retains original failures and exact source/artifact bindings. It provides no shared
worker, arbitrary-thread DirectWrite access or complete Markdown engine.
The qualified positioned-line implementation seeds each noninitial layout with an invisible
DirectWrite inline object and line break. The two private UTF-16 units never enter
source/display maps, Unicode navigation or text-service content. DirectWrite
accumulates real lines in document coordinates and draws every chunk at the same
origin, preserving its mixed-font caret and color-font behavior. Reuse requires
the same vertical origin; a height-changing edit rebuilds dependent suffixes and
reports all that work. An inexact synthetic line or an empty displayed suffix
retains canonical whole geometry, including terminal-font behavior.
Whole-layout preparation/reuse/source units are reported separately and cannot be
credited as bounded incremental work. Original height/raster failures remain
retained. All three x64 profiles pass geometry and 108 exact WARP pairs each,
including default/fractional/large-origin cases, height-changing edits, hidden-only
suffixes and real FFFC source. Default 13-DIP native/embedded editors avoid whole
work; native/embedded UIA scrolling preserves exact source. All six build profiles
pass; ARM64 remains compile-only. The [positioned-line record](../../Measurements/RichText/2026-10-18-positioned-lines/README.md)
retains the unchanged five-trial observations without timing acceptance. Shared
preparation, full syntax/semantic editing and platform/performance gates stay open.
Clear must not construct
STL containers inside noexcept retirement. Geometry staging fills fallibly
constructed projection storage in place rather than depending on optional NRVO;
the original Debug point-173 termination remains retained.

The control on DxUi.h owns bounded source/selection/history, one guarded
PreparedDocumentLayout and a vertical scroll offset. It borrows ControlHost DWrite/D2D,
Body/Monospace typography, brushes, theme/DPI, focus, capture, Unicode clipboard,
invalidation and event-driven caret animation in both hosting modes. It creates
no HWND, device, swap chain, thread or timer. All mutation/preparation is owner-
thread work. Rasterization belongs to preparation, never composition.

Source offsets include hidden markers and obey scalar/CRLF boundaries. View
changes preserve bytes/history. SetText resets history without notification;
source commands notify only after an actual committed change. Notifications and
clipboard operations recheck lifetime/revision/selection where applicable; no
callback may resume an old mutation after owner retirement/rebinding. Standard
notification failures leave the committed edit intact. Read-only permits selection/
copy while rejecting mutation; disabled controls reject input. Incomplete UTF-16
character pairs never enter the model. AltGr Unicode entry remains accepted.

The [guide](../../docs/rich-text.md) lists exact navigation/format/clipboard
commands and v0 limits. Layout survives palette/selection repaint, rebuilds on
source/mode/width/typography/flow/DPI changes and releases on hiding/rebinding.
Caret reveal and wheel scroll remain within the text viewport. D2d enables the
shared native TSF/IMM and embedded source text adapter plus Document/Text/Text2/
TextEdit UIA, mapped source geometry and single-transaction composition. Actual
IME/AT/physical/platform qualification remains separate under the input contract.
The capability is explicitly a preview, not complete rich-text/Markdown support.

Cooperative preparation: content owners
opt into ControlHost's shared preparation traversal. S_FALSE retains one pending
owner request for a later turn; failure permits explicit retry without an
automatic retry loop. Rich text stages one immutable source/geometry request
and publishes it atomically. Source/mode/binding/width/typography/resource changes
cancel obsolete work. Geometry queries must not drain preparation; unavailable
current geometry returns the existing optional/error result. Source text, Unicode
logical edits, history and IME transactions remain available while reflow is
pending. Pixel completion precedes text-service/UIA geometry notification.

Native window hiding and either zero-size dimension cancel retained content,
including a height-only resize whose shaping width stays unchanged. This does
not discard the native swap chain on WM_SHOWWINDOW/WM_SETREDRAW. The next real
visible/sized owner preparation rebuilds the current request; source, selection
and history survive suspension.

BeginPreparation still constructs the bounded v0 projection/indexes at once.
Logical pending edits may project bounded source without DirectWrite; neither
path is a resumable complete Markdown parser. These limits and indivisible
long-paragraph/whole SDK calls remain pending performance work.

### Lossless Markdown analysis preview

`MarkdownSyntax.h` supplies independent immutable source, root leaf-block ranges
and context-free lexical candidates. Block ranges and, separately, lexical ranges
partition the entire original source, including delimiters, unknown constructs,
mixed line endings and NUL. Content/info ranges point into that source. Implemented
root recognition covers paragraphs, blank lines, ATX/setext headings, thematic
breaks and fenced code, including unfinished code through EOF. It does not parse
containers, references, HTML blocks or indented code yet. Lexical candidates cover
ASCII punctuation escapes, semicolon-terminated named/numeric references, line
endings and NUL replacement. Candidate decoding does not establish inline
precedence: code spans, raw HTML, links and delimiter resolution remain necessary
before using those values as rendered rich text.

Every result carries the requested CommonMark0312, Gfm029 or RichMarkdown profile
and source revision. These are separate grammar targets, not conformance claims.
`MarkdownParser::Begin` validates and owns at most 65,536 UTF-16 units, accepting
NUL and rejecting unpaired surrogates before publication. Begin/copy/comparison
are bounded but indivisible. `Advance` resumes at its stored cursors and consumes
no more than the requested operation budget. One operation scans one scalar/CRLF
or source unit, finalizes one line or recognizes a bounded <=33-unit lexical
candidate. Classification uses accumulated line metadata, never another full-line
scan. It owns at most one staged and one published revision. Zero budget does no
work. Equal pending/completed source/revision/profile coalesces; equal revision
with different text/profile does not. Invalid/failed Begin preserves the old
request; failed Advance cancels staging and retains the published revision.

`LexemeSourceAnchor` keeps transformed-value leading/trailing affinity explicit,
including a named reference producing two Unicode scalars. Original spelling
always remains available. Text lexical ACPs are not automatically legal editing
boundaries; callers retain the model's scalar/CRLF checks. Generated/reordered
semantic objects and rich transaction anchors remain a later stage.

RichTextEditor analysis is opt-in through `SetMarkdownAnalysisProfile`; null
disables it. The default and formatted pixels retain their v0 behavior. Analysis
uses ControlHost turns before geometry, with a 4,096-operation default per turn.
`GetMarkdownSyntax` never advances preparation and returns only a completed current
source/revision/profile, or null. Source edits invalidate current-result access
immediately, cancel pending analysis and preserve held immutable revisions.
Profile changes preserve source, selection and undo/redo. Hiding, zero extent,
rebinding and resource retirement cancel pending work. No extra worker/HWND/timer
is permitted in either host. Completed pure analysis is independent of geometry.

The raw parser's NUL admission deliberately does not change the existing Document,
codec, clipboard or source text-store admission contract. End-to-end NUL editing,
complete pinned grammar/HTML output, extensions and semantic commands remain open.
