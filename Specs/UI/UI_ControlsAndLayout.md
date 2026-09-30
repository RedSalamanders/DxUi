# Controls and layout

Status: normative intended contract
Last reviewed: 2026-09-30

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Controls retain state independently of GPU resources. Buttons, toggles, sliders, selectors and text fields are shared
implementations, not application-local clones. An active gesture has a defined preview, commit and cancel lifecycle.
External state acknowledgement is separate from local intent. Hidden controls leave focus and accessibility trees.

Layout accepts explicit bounds/DPI, reports usable minimums and prepares coherent visuals/hit rectangles together.
Do not reduce a touch target below the consumer's minimum to hide an overflow. Labels and state values may not clip
or overlap. Optional content is removed before required controls. Geometry is recomputed on relevant changes only.
Consumers own density tiers and responsive policy; DxUi contains no AV profile or XENEON dimension rules.

### Described native menu entries

The [described-menu plan](../Plans/WIP/MenuDescriptions_2026-09-21.md) extends native command
rows with opt-in literal secondary text. Standard/Toggle/Radio/Info rows must measure both fields
at the final monitor-constrained width, including the scrollbar lane, then publish matching
paint/hit geometry. The lane decision uses the viewport the popup will actually have: the requested
height rounded to whole device pixels inside the work area. Overflow of at most half a device pixel
is rounding slack; it neither reserves the lane nor scrolls, so the lane reserved before sizing and
the final scrollbar always agree at fractional scales. Both fields wrap without silent truncation.
Retain prepared layouts across unchanged paints; a failed reflow closes the menu without selecting
a command, and a popup whose session closed while it was being positioned is never shown. Long individual
rows may exceed the viewport: keyboard navigation reveals their beginning and wheel/scrollbar
interaction exposes the remainder. Preserve existing one-line behavior when secondary text is absent.
Identical text with the same font role and available width may share native layout storage within
one popup preparation. Sharing must preserve separate command IDs, accessible identities, row state
and hit rectangles. The lookup is preparation-local and released before paint; no process-wide
text cache is retained. Scrollbar reflow sets an absolute final width, including for shared layouts,
so repeated rows cannot cumulatively narrow one another. DPI changes prepare a fresh coherent set.
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
painted disc so a fat finger can grab the thumb. Neither layer is sized from the other.

```
  cross-axis (horizontal slider, DIP)

  48  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  hit band (unpainted)
  20  ████████ chrome disc (fixed) ████████
  16                                           hovered inner thumb
  12                                           pressed inner thumb
   6  ══════════════════════════════════════    track and rest inner thumb
   0
```

| Layer | Rest | Hover | Pressed | Role |
| --- | --- | --- | --- | --- |
| Track | 6 DIP capsule | — | — | Paint. Fill and remainder share that thickness with no extra stroke. Inset 12 DIP from each end. |
| Inner thumb | 6 DIP | 16 DIP | 12 DIP | Paint. Accent fill matching the track. Rest equals the track so the chrome reads as a gray ring; hover grows the inner until a thin chrome rim remains. |
| Chrome disc | 20 DIP | 20 DIP | 20 DIP | Paint. Opaque gray disc under the inner thumb. It does not scale with hover or press and is not the hit target. |
| Hit band | 48 DIP | 48 DIP | 48 DIP | Pointer only. Centered on the track. Clipped to control bounds when the control is shorter. Not painted. |
| Thumb grab | 24 DIP radius from thumb center | same | same | Pointer only. Half the hit band. A contact in this circle drags from the current value; a contact on the track outside it seeks. |

Keyboard and `RequestValue` ease the painted inner thumb to the new value; pointer drags and `SetValue` snap.
Reduced motion snaps every visual. Keyboard steps still use `SetStep` / `SetLargeStep`. Consumers that need a
fat-finger target size the control to at least the 48 DIP hit band; they do not enlarge the chrome disc to match.
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
icon text keeps the small UI font, as in Grid.

### Localized built-in text

Consumers supply owned per-instance strings through `ComboBox::SetNoMatchesText`,
`Tree::SetEmptyStateText`, and the existing `Grid::SetEmptyStateText`. Empty overrides restore
English defaults (No matches / No data). ComboBox remeasures an open empty-result popup when
its string changes; Tree/Grid invalidate their empty view. String updates do not alter selection,
query text, focus or callbacks. DxUi does not load consumer numeric resource IDs or cache a
process-global language. Each product/module provides its current translated strings.
