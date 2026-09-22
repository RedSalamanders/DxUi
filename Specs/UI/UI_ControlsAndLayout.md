# Controls and layout

Status: normative intended contract
Last reviewed: 2026-09-19

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Controls retain state independently of GPU resources. Buttons, toggles, sliders, selectors and text fields are shared
implementations, not application-local clones. An active gesture has a defined preview, commit and cancel lifecycle.
External state acknowledgement is separate from local intent. Hidden controls leave focus and accessibility trees.

Layout accepts explicit bounds/DPI, reports usable minimums and prepares coherent visuals/hit rectangles together.
Do not reduce a touch target below the consumer's minimum to hide an overflow. Labels and state values may not clip
or overlap. Optional content is removed before required controls. Geometry is recomputed on relevant changes only.
Consumers own density tiers and responsive policy; DxUi contains no AV profile or XENEON dimension rules.

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
  children from navigation, input and rendering work. Checkbox checked state remains distinct from
  focus/hover/pressed/disabled; checked paint and UIA Toggle always agree. Checkbox honors opt-in
  `SetMultiline(true)` for its complete caption. Measure Body text within the control width minus
  36 DIP and reserve 6 DIP of vertical padding; its default remains single-line.
- Grid summary rows are bounded and match hit/selection geometry. Complete selected-item text can
  be arranged in a separate wrapped detail view without duplicating the grid's data authority.

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
grip; the horizontal or vertical resize cursor applies over the hit band and during a drag. The persisted position is
consumer state. In an EmbeddedHost the consumer applies pane bounds on its next preparation, not inside the change
callback. That bounds revision keeps the drag while the splitter stays in the tree, enabled and visible
([`Rendering_EmbeddedD3D11.md`](../Rendering/Rendering_EmbeddedD3D11.md)).

### Numeric stepper

`NumericStepper` is a Panel that owns a `TextField`, an increment and a decrement icon button, an optional leading
label (`SetLabel(text, widthDip)`) and an optional trailing unit (`SetUnit(text, widthDip)`); label and unit are
painted, not child controls. Values clamp to `SetMinimum` / `SetMaximum` (default ±1e9) and format with
`SetDecimals` (0–6) using `.` as the separator; `ParseValue` accepts an optional sign, digits and one `.` or `,`
fraction with surrounding whitespace. Text that parses previews immediately (`NumericStepperChangePhase::Preview`)
and opens an edit whose start value Escape restores (`Cancel`). Enter, focus loss, the buttons, Up/Down (Shift:
`SetLargeStep`), `RequestValue` and `Nudge` commit once; text that does not parse reverts to the committed value.
`SetValue` clamps, rewrites the text and never notifies. Disabling the stepper disables its three children.
Right-to-left flow mirrors label, field, unit and buttons.

### Color picker

`ColorPicker` is a Panel with a saturation/value field (`kFieldDip` square), a vertical hue strip, new and current
swatches, R/G/B `NumericStepper`s (0–255), a hex `TextField` (`#RRGGBB`, `RRGGBB`, `#RGB`, `RGB`) and OK/Cancel
buttons whose captions come from `SetLabels`; the library ships no localized strings for it. `SetColor` sets the
editing and current colors without notifying; `SetCurrentColor` changes the reference swatch only. Pointer drags on
the field or strip, typed component values, hex text and `SampleColor` (host eyedropper) preview
(`ColorPickerChangePhase::Preview`); OK, Enter and `Commit` copy the editing color into the current swatch and
notify `Commit`; Cancel, Escape, `Cancel` and capture loss during a drag restore the current color and notify
`Cancel`. Left/Right step saturation (mirrored in right-to-left flow) and Up/Down step value by 1/255, Page Up/Down
step hue by one degree; Shift multiplies by ten. Grays keep the last hue and black keeps the last saturation so the
field marker does not jump. `HsvFromArgb`, `ArgbFromHsv`, `ParseHexColor` and `FormatHexColor` are public helpers.
Gradient brushes are created on the first paint per device context and reused until the hue or geometry changes.
Alpha is always opaque.

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
the row when the pointer is in its middle and that row has children. Release calls `IDxTreeDelegate::OnTreeReorder`
once with the source id, the target id and `TreeDropPlace` (`Before`, `After` or `Inside`). The tree does not change
the model. Escape and capture loss cancel and do not call the delegate. A click that does not travel 4 DIP selects
as before and does not reorder.

### Localized built-in text

Consumers supply owned per-instance strings through `ComboBox::SetNoMatchesText`,
`Tree::SetEmptyStateText`, and the existing `Grid::SetEmptyStateText`. Empty overrides restore
English defaults (No matches / No data). ComboBox remeasures an open empty-result popup when
its string changes; Tree/Grid invalidate their empty view. String updates do not alter selection,
query text, focus or callbacks. DxUi does not load consumer numeric resource IDs or cache a
process-global language. Each product/module provides its current translated strings.
