# Controls and layout

`RichTextEditor::WrapSelectionInMarkdownBlock` transforms a nonempty source selection into a block quote
or fenced code block in Rich or Source presentation. It retains every selected and unselected source unit;
quote markers are inserted per selected physical line, and code content remains literal. Inserted blank
separators isolate the block from adjacent prose without deleting either partial-line fragment. Existing
selected CRLF/LF/CR spellings remain exact; added separators use the document's first line ending, or CRLF
when none exists. A code fence exceeds every selected backtick run. A selected document terminator remains
a terminator after wrapping. The transformed body stays selected in its original direction, and one undo
or redo restores the complete source/selection transaction.

Empty selections, unsupported block kinds, invalid code-language text, disabled/read-only controls, active
composition/text-service previews and capacity overflow decline without changing source, history or text
notifications. Applications own their empty-selection templates. Code-language text is valid UTF-16, at most
255 units, and excludes NUL, CR, LF and backticks. This source operation needs no prepared semantic geometry.

Control continuations after synchronous preparation, text notification or accessibility publication must
recheck their weak lifetime before member access. TextField imports/history/clear/caret paths and ComboBox
variant/text/import paths complete safely when scheduling retires their tree. FormField's late measure
invalidation must likewise revalidate before accessibility synchronization. Existing native/embedded
ownership and callback exception contracts continue to apply.

RichTextEditor `RichTextEditorAppearance::colorizeMarkdownSource` is opt-in and
uses the configured lossless Markdown analysis profile. Source glyph geometry
MUST stay identical: syntax delimiters receive subdued ink, heading/list markers
and exact source link destinations receive accent ink, and selection ink wins.
The experimental direct renderer has its own default-off `directSourceInkPreview` switch; the ordinary colored DirectWrite path remains the default until paired performance qualification succeeds. No source text, UTF-16 offset, history or text-service state changes. Color ranges
MUST publish only after complete owner preparation, be disjoint/sorted and stay
bounded to 65,536 staging events. Source/host/layout retirement cancels pending
ink. Capacity overflow falls back to the whole ordinary source view and is
remembered for that syntax identity. High contrast disables custom source ink.
Every owner preparation advance MUST share a 256-step budget across block-marker
walks, inline delimiter runs, link child walks, raw destination scans,
incremental bottom-up event ordering and range publication. Long scans and sort
passes resume on later advances; no partial range set is visible before completion.
List-item accent ink MUST use the exact marker range retained by block parsing,
including nested, ordered, quoted, tab-indented and task-list items.

The source-ink paint route is source-only and must preserve canonical DirectWrite
shaping, glyph advances/offsets, source/display mapping and selection geometry. It
may paint at most 32 clipped ink spans in one direct-draw/prepared-layout call
and considers at most 256 glyphs or 512 UTF-16 units in one glyph run.
Monospaced monochrome runs may use the existing brush-segmented route. A
proportional monochrome or color-font run may be drawn only as its original
intact shaped run when every covered glyph uses one identical borrowed
`ID2D1Brush` pointer. Color-font drawing additionally requires a successful
runtime query for `ID2D1DeviceContext7` and uses
`ID2D1DeviceContext7::DrawGlyphRunWithColorSupport`. Complete run, description,
cluster, effect, inline-object, source text and brush checks must finish before
any source-ink paint begins. A preflight refusal uses the canonical layout
renderer and preserves every syntax color. No target, brush or device-context
capability may be retained by a layout or cross a target/device replacement. A
retained prepared-layout copy and replacement target must match independent
canonical full-layout pixels. These paint limits are separate from the 65,536
source preparation-event bound above. Selection and text geometry remain
unchanged. Errors after painting starts follow the host/device error path and
must not cause a second canonical paint.

Status: normative intended contract

Button presentation is opt-in and independent of `ButtonVariant` and command/TogglePattern semantics.
`Filled` preserves existing behavior; `Subtle` exposes the parent ground while idle, `Outline` retains a perimeter,
and a checked `Tab` exposes an accent underline. Hover/press, disabled state and keyboard focus remain visible.
High contrast preserves the existing system color pair and focus/border treatment. Primary actions retain their
filled treatment. Leading Fluent icons and trailing keyboard hints on Standard/DropDown/Repeat buttons participate
in intrinsic measurement, reserve the same paint lanes and mirror with flow direction. Hints describe consumer-owned
shortcuts; they never register keys or change the button's accessible command name.
Hyperlink button underline width is prepared with the current text, content font,
host typography, flow and bounds before painting. Hover painting uses only the
prepared width and creates no DirectWrite measuring layout. Text, font, bounds,
flow and host typography/resource changes retire the measurement.

Label mnemonic changes invalidate the owner only when the normalized mnemonic
changes. Labels with a mnemonic or typography override prepare their DirectWrite
layout before paint, keyed by text, mnemonic, typography, flow, bounds and host
resource/typography identity. Repeated paints reuse that layout; labels with no
mnemonic and no typography override retain their ordinary text draw path.

`TreeAppearance` may suppress background/border or decorative expanders and may reserve the expander lane on
leaf rows. Hit testing follows the visible lane; keyboard expansion, hierarchy, selection and UI Automation retain
the same borrowed model. High contrast forces the system surface and border. `RichTextEditorAppearance::drawFocusIndicator`
may omit decorative editor focus chrome for a document surface, without retiring text geometry, selection or history;
the caret remains independent and high contrast always forces the focus indicator.

Tree row painting, hit testing, tooltip and accessibility geometry use one canonical row calculation, including
hidden/reserved expanders. Neutral decorations opt into subdued unselected icons and count badges; a dot badge
retains its textual accessible description as the TreeItem's UI Automation `ItemStatus`; `Name` remains the row text.
For a legacy mutable model, `NotifyItemPresentationChanged(itemId)` publishes a badge text/tone/dot-only update for
an existing visible row after the caller updates its data. IDs, order, row text, depth, child/expanded state and visible
membership must stay unchanged. The notification invalidates badge/tooltip text measurements, paint and accessibility
status without reconciling selection, focus, reorder state or scroll. It returns false without publication for a missing
or nonvisible row and for models that supply prepared accessibility rows. A prepared source publishes status through
its immutable row view and the normal prepared-source update path. Subtle Splitters paint a thin idle seam while
preserving the full resize hit target and keyboard/drag behavior; hover, dragging, keyboard focus and high contrast
retain the normal handle.

`RichText::SemanticSpacing` optionally supplies per-level heading margins and container indentation to semantic
document layout. All values are finite: margins 0..256 DIP, indentation 0..64 DIP. Heading margins are minima and
credit real source blank-line caret boxes; no source site is dropped, compressed or fabricated. An absent option
preserves existing geometry. The option participates in complete geometry reuse/retirement and is validated before
replacing a published layout. Ordinary source-view text geometry stays governed by its existing source policy.
Last reviewed: 2026-10-25

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
A host owns its root's bounds: a window host sets them to its client area when it installs the root, resizes, changes
DPI and paints, and an embedded view sets them to its prepared size at every preparation. Equal bounds change nothing;
an application insets content inside the root, never by moving the root.

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
DirectWrite font roles, flow direction and density. Wrapping requires an explicit opt-in and finite width, and
wrapped text measures without the space that ends each wrapped line, which hangs past that width; single-line
measurement preserves its natural text extent, trailing space included. Toggle reserves the larger of its two localized
state labels. Single-line editors measure stable font/chrome requirements and use the owning layout's minimum
or explicit preferred width; typing, selecting an item or nudging a value does not resize the editor. A masked
multiline field measures its displayed mask. Shaping is bounded to 65,536 UTF-16 units per measured text value and
per container pass across cache misses and nested measures. `MeasureContext::remainingTextUnits` optionally borrows
an existing pass budget; nested containers preserve it and cached results consume no units. Excess input fails
before text-layout allocation. No character-count width approximation is a measurement fallback.

`FlexPanel` owns bounded, reusable numeric measurement and arrangement records for at most 1,024 live child
controls (a slot `TakeChild` emptied does not count). It supports horizontal or vertical order, explicit wrapping, padding, gaps, child minimum/maximum
constraints, cross-axis alignment and positive growth. A child maximum caps growth; remaining growth is
redistributed among eligible siblings. Hidden children occupy no flow space. RTL mirrors physical rectangles
while preserving logical ownership and focus/accessibility order. Measurement cache identity includes the
child's measurement revision, constraints, host typography revision, DPI, flow and density. Content, font,
wrapping, chrome, availability, ownership and inherited settings invalidate the relevant records. Color,
checked state, hover and position-only changes reuse shape results. Containers retain no DirectWrite layout
per leaf, and hidden containers retire their numeric entries for one fresh preparation when shown again. Showing a
control asks the host for that preparation even when no layout owner sits on its own ancestor chain, so an owner
below a plain container that was hidden (an unselected tab page) is laid out when the container is shown.

`TextField::SetFontRole` selects one valid shared `FontRole` for its text and placeholder, with `Body` as the
default. The role is used consistently for intrinsic measurement, single- and multiline shaping/paint, selection,
caret and hit-test geometry, scrolling, keyboard movement and native/embedded text-service geometry. Changing the
role is idempotent for the current value; an invalid enum value leaves state unchanged. A change retires the
single- and multiline shaped-layout caches and intrinsic measurement revision, then refreshes attached input and
accessibility geometry. It preserves the draft, selection, undo/redo history and active text composition.

Intrinsic owners request work through the shared pending-layout traversal used by native painting and embedded
`Prepare`. Layout, shaping and allocation remain outside embedded `Composite`. A superseded layout (a callback
that changed the tree mid-pass) prevents the host from publishing that preparation and requests another one. A
layout that fails (text beyond the shaping budget, an unsupported child without an explicit size, arithmetic
overflow) keeps the owner's previous geometry, which the frame paints, and is reported once through the diagnostics
sink; the owner is laid out again when its content, constraints or bounds change. An `E_OUTOFMEMORY` result receives one cooperative retry per layout request; a second failure remains latched until a new request. A failure never blanks the
window. Measure invalidation moves no geometry until that preparation, so it leaves the interaction revision alone:
pending layout neither drops input nor cancels a press. After a successful deferred pass the native host
republishes the UI Automation snapshot, whose bounds an earlier publication may have read before the pass. The
layout owner stages all numeric geometry before the first
extensible bounds callback and leaves child bounds unchanged on validation, allocation, measurement or arithmetic
failure. After custom measurement or bounds callbacks it rechecks lifetime, ownership, host, geometry and input
revisions. A callback that retires, reparents or replaces the owner/child, changes sibling geometry, or starts a
newer layout aborts the stale pass. Geometry authored by such a callback remains authoritative; already completed
callbacks are not rolled back, and the incomplete outer pass does not publish its content extent or frame.

Flex arrangement stages absolute endpoints in logical line order. Positive intrinsic extents, gaps and padding that collapse at a
finite origin fail with arithmetic overflow before any bounds callback. An inward-rounded endpoint advances
outward, with following anchors consuming that corrected edge; represented minima, gaps, order and applicable
growth maxima remain authoritative. A fixed extent (minimum equal to maximum) whose span no float endpoint represents
exactly takes the nearest endpoint, at most one representable step past the maximum, while that step is under
1/1024 of the extent; coordinates too coarse for the extent still fail. Optional Grow/Stretch additions can round inward while preserving the
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

A form supports at most 256 live fields (a removed field frees its place). `AddField` requires a nonempty, unique identity and bounds its
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

The [described-menu plan](../Plans/Done/MenuDescriptions_2026-09-21.md) extends native command
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
  the panel and the child's host still exist); the empty slot is skipped by every panel operation, and
  `CompactChildren()` removes the empty slots in order: indices after a removed slot shift, a flex child keeps its
  options and a radio group its choice.
  PageHost participates in the same inheritance chain as Panel; its pages resolve through the PageHost rather than
  directly through the host. Root replacement snapshots the incoming control's previous inherited values before
  interaction-reset callbacks can retire its old parent, then reparents without dereferencing that old chain.
- TabControl stages title measurements, header geometry, attached-tab path geometries and its host palette and text-format resources before painting. Host transfer, DPI, content and overflow scrolling retire the affected cache and schedule visual preparation; the first subsequent prepared frame draws the current tabs without creating a title layout or rebuilding header geometry during Paint.
- Tree walks go through logical children (`Control::GetLogicalChildCount`/`GetLogicalChild`): a Panel's children and a
  PageHost's current page. Membership (`IsControlInTree`), Tab order, native text input and its text store, the
  ComboBox scroll-container lookup and the UI Automation tree all use that one walk, so the content of a page takes
  focus by Tab, receives IME and TSF input and has UI Automation elements exactly as the same content in a Panel does.
  Element paths index logical children. Logical-child accessors are stable, read-only traversal queries on the
  owning UI thread: they must not mutate the tree, invoke application callbacks or pump messages. Returned child
  pointers are borrowed until the next supported tree mutation; traversals do not support destructive getter reentry.

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
`Button::SetContentFontSizeDip(std::optional<float>)` optionally sizes the caller's text or icon content in all
Button variants. Null preserves the existing role defaults (Body 13 DIP, Icon 12 DIP). Accept only finite
6..96-DIP sizes, normalized to 1/64 DIP; return `E_INVALIDARG` without mutation for invalid input, `S_FALSE`
for a normalized no-op and `S_OK` for a change. The getter returns the normalized optional value. Intrinsic
measurement, wrapping, glyph painting and hyperlink underline width MUST use the same requested size while
retaining the host role's UI/icon family, fallback, weight, alignment and flow direction. Internal dropdown,
selector, split and acknowledged disclosure chevrons retain their original chrome metrics. Selector buttons reserve an empty leading lane to balance the trailing chevron; leading icons and shortcut text are not rendered or measured for this variant. Consumers can
request 14-DIP chrome labels and 18-DIP line icons without changing global typography or creating local controls.
Reuse the host's bounded configured-format cache and common drawing/measurement helpers; stable painting MUST
not allocate new formats per frame. A changed override invalidates intrinsic measurement before requesting
paint, and a synchronous embedded preparation callback may measure the new state or retire its control; the
setter MUST stop touching a retired control. Native glyph capture, wrapped intrinsic metrics, cache stability,
resource-generation replacement and embedded callback tests cover the opt-in behavior.
`Label` text, multiline, typography/font-role and `Separator` orientation setters follow this callback boundary as well. In an embedded
view, measurement invalidation may synchronously reach the consumer's preparation-scheduling callback; the consumer
may retire or replace controls through a `ControlHost` reference borrowed before that callback. The callback must not
re-enter `EmbeddedHost` preparation, composition, input, device or lifecycle methods. A setter continuing after the
callback MUST check its captured weak lifetime before touching the control again, and MUST verify current host
ownership before continuing with host-specific work. Whole-host destruction from inside a callback is not covered.

An `IconOnly` base `Button` with exactly one valid Unicode scalar may retain one `IDWriteTextLayout` for its current
host format, flow direction and exact DIP extent. Prepare it through the visual-only owner preparation channel,
never lazily in `Paint`; this channel does not mark content geometry changed, publish text-input/UIA state or delay
input readiness. An owner preparation pass resolves the effective format and compares its exact COM identity with
the retained key before taking the warm return; only a changed key looks up the DirectWrite factory and creates a
replacement layout. Thus clearing the bounded host format map cannot reuse a layout with a different format pointer.
Paint the retained layout at the current origin with the current foreground brush and the same
DirectWrite draw options as the ordinary text path, so state/color changes do not rebuild it. The key retains the
effective format identity and host/resource/typography generation; text, format, size, flow, host detach, hide and
device/resource cancellation retire it. At most one layout is retained per eligible Button. Toggle and RadioButton
descendants keep their distinct paint paths and do not prepare a base-Button resource. Empty, multi-scalar and
malformed UTF-16 content, and any failed optional preparation, continue through the existing `DrawTextW` path.
Never change the caller's text or geometry to make the cache eligible. Exact-pixel tests cover supported glyph/fallback, flow, DPI, bounds, rendering
state and text-antialias cases against the original path; any unsupported case remains on that path.

When the bounded configured-format cache is evicted, the host marks visual preparation for the next owner update.
Embedded hosting trims that cache before its control preparation pass, so an IconOnly Button resolves the new exact
format identity and replaces its layout before painting the same frame. Eviction does not change geometry or input
readiness; a clean composition does not trim caches or schedule preparation.

Control callbacks that synchronously transfer native mouse capture use the same rule. Before transfer, publish state
needed by the ensuing capture notification and capture the requesting control's weak lifetime. The previous HWND's
`WM_CAPTURECHANGED` callback may replace the requesting host's root before capture returns; afterward the requester
MUST verify that its lifetime and current host ownership still match before reading members or continuing input.
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

The public API exposes Panel, PageHost, CardPanel, Separator, Label, Button, Toggle, Checkbox, RadioButton, RadioButtons,
ProgressBar, PageIndicator, ThroughputGraph, Slider, Toolbar, MenuBar, TabControl, ColorSwatch, TextField, ComboBox, TagPicker,
StatusStrip, PopupLayer, StackPanel, ScrollPanel, TooltipLayer, Tree, Grid, Splitter, NumericStepper, ColorPicker,
FlexPanel, FormField, FormPanel and RichTextEditor.
GetControlCatalog returns immutable
descriptors; CreateControl constructs the selected kind and returns E_INVALIDARG for unknown kinds, leaving the
existing result intact on failure. Controls with models are configured by the caller; the factory does not invent
application data. Constructors and examples are ordinary code, with no source-code generator or second control copy.

Adding a control requires an aligned public declaration/implementation, catalog entry/factory case, meaningful
behavior tests and a populated gallery tile. `gallery.ps1` generates light, dark, rainbow light/dark and high-contrast
PNG sheets with interactive-state variants, plus a supplied-device toggle/slider image. Gallery rendering supplements
runtime tests; empty-control construction alone does not prove selection, keyboard, editing or accessibility behavior.

`Separator` is a decorative, nonfocusable control with horizontal and vertical orientations. It reports UIA Separator
control type and exposes neither Invoke nor a pointer hit target. In high contrast it uses the system border color;
`Toolbar::AddSeparator` uses the toolbar's existing divider color. `Label::SetTypographyOverride` and
`Tree::SetTypographyOverride` are optional; font size is bounded to 6–96 DIP and normalized to 1/64 DIP, weights are
100–900, and Label character spacing is bounded to −4..16 DIP. Invalid values leave the prior override unchanged;
`nullopt` restores role-default behavior. Tree current/focused rows use the configured current-row weight. Labels and
Tree rows use host-cached DirectWrite formats and do not mutate shared font-role formats.

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
With motion it sweeps from before the track to past its end every 2,000 ms. An enabled, visible, unpaused bar requests host
ticks, and each tick advances the sweep by the elapsed time and invalidates the host. A long gap advances only by its
remainder of a loop. A tick time earlier than the previous one re-seeds the timing without moving the segment. Under
reduced motion the segment rests centered, from 30% to 70% of the track: the bar requests no ticks, `Tick` reports
false and every paint is identical. Restoring motion resumes the sweep at the next paint when animation is unpaused.
`SetAnimationPaused(true)` freezes the current indeterminate phase and requests no further animation ticks from the bar. `Tick` returns false while paused. Pause preserves the value, enabled state and indeterminate semantics. `SetAnimationPaused(false)` wakes an attached, enabled, visible indeterminate bar when motion is allowed; its first tick re-seeds the clock without adding the interval spent paused. Reduced motion retains its centered resting segment. Animation is unpaused by default. Consumers retain ownership of paused-status captions and scheduling of unrelated controls.
Value, minimum and maximum changes also mark native accessibility publication dirty, so subscribed range
properties and the next owner query reflect the current bounds without another unrelated mutation.

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
also leaves the prior range intact. A release that invokes a preview callback revalidates its drag generation, host and capture owner before committing. A callback that transfers capture cancels only the old draft; a canceled and restarted drag retains its own capture and release.

Off-center thumb grabs retain the pointer offset, continue outside bounds under
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

### Tree expansion animation capacity

Tree expansion and collapse retain their model, selection, focus, delegate and accessibility semantics at any model size. Row and chevron transitions are optional: if either visible-row snapshot would exceed 4096 rows, or animation preparation cannot allocate, the accepted change appears immediately. Admitted transitions prepare stable-ID indexes once and reuse them during painting. Reduced-motion settings also apply the change immediately.

### TextField supplementary character input

Character input buffers a UTF-16 lead surrogate privately until its matching trail arrives, then inserts and publishes the complete pair as one edit. An orphan trail is consumed without changing text. Caret or selection commands, pointer input, focus changes, text replacement, text-store imports and read-only transitions cancel a pending lead; no change callback or accessibility publication exposes it alone.
### Grid keyboard paging and column identity

Page Up and Page Down use the body viewport height and prepared row/group geometry, with at least one-row progress where possible. Unhandled keys return before querying the model or enumerating rows. A model refresh compares the cached column IDs; when those identities change, widths, display order and current-cell index are rebuilt for the new columns rather than retained by position.
### Tree refresh and input routing

A model refresh reconciles identities and clamps the scroll offset without moving the viewport to an unchanged focus. Typeahead cycles after the current row on a fresh one-character search and retains the current match as the prefix grows. A first keyboard action that selects the first row reports that selection; movement reports its final landing once. Ctrl and Shift row-selection gestures do not trigger single-click invocation. A wheel event directed outward at a scroll extent bubbles to an enclosing scroller, including partial detents.
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
Ctrl with any movement key moves Grid focus without changing membership, Ctrl+Space toggles the focused row, and Space
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

Grid focus is a stable row identity independent of selected membership. The focused row draws a focus ring of its own,
selected or not, while keyboard focus is visible (always in high contrast while the grid has focus), with Tree's ring
metrics and contrast rule. `RequestFocusRow`, Ctrl with any movement key (Up, Down, Home, End, Page Up, Page Down) and
UIA `SetFocus` move focus without selecting a row or changing the range anchor. `RequestFocusCell(row, column)` also makes
that column current and reveals it horizontally; Ctrl+Left and Ctrl+Right choose the current column in display order,
mirrored in RTL (a one-column grid leaves both keys unhandled), and once the keyboard chose a column the focused row
also outlines its cell until the next click or a new model. None
of these alters selection or anchor. `GetFocusedRowId` names the focused row. A selection the application publishes
(`GetSelectionModel()` then `NotifyDataChanged`) leaves the focus as it is, so a focus the application cleared stays
cleared and publication raises no focused-row callback; `SetSelectedRowIds` silently selects the listed visible rows and
makes the last one the focused row and the anchor. The keys start from the focused row, else from the last selected row,
else from the first row. A normal row gesture replaces membership and moves the anchor to that row; Shift extends from the anchor;
Ctrl toggles the touched row and moves the anchor there, even when it toggles the row off. Ctrl+Space toggles the focused
row. Ctrl+A selects all visible rows and focuses the final row in visible order. A single-selection Grid permits an
empty selection through Ctrl+Space or UIA removal. UIA AddToSelection is idempotent for an already selected row and
preserves the current anchor; in single-selection mode it refuses to add another row while one is selected.

Selection callbacks report membership changes and focused-row callbacks report focus changes. State is committed before
callbacks; callers stop if callback reentrancy retires the Grid or replaces its model or delegate. A callback that
refreshes the model or changes any control's layout does not end the gesture: its remaining reports, reveal, context
menu, checkbox toggle, group restoration and UIA result follow, with rows re-resolved by stable id (see
[the architecture contract](../Core/Core_Architecture.md)). The same holds for Tree. Model reconciliation drops rows that
are no longer visible and clears an anchor whose row left the visible model. UIA focus and selection operations do not
activate rows.

Ctrl+C copies the selected rows in visible selection order and columns in displayed order. Fields containing quotes,
tabs or line breaks use quoted TSV syntax and embedded quotes are doubled; all other Unicode values are preserved. With
nothing selected Ctrl+C is left unhandled and the clipboard keeps what it holds. The grid binds no key to full-value
inspection, since F1 is the application's Help: `InspectFocusedCell` reveals the focused row's cell in the current
column and opens the persistent, scrollable inspection surface on its full value, and returns false when nothing is
inspected (no focused visible row, no column, an empty value, or a grid outside a host). `IsFocusedCellClipped` reports
whether that cell draws less than its whole value, so that an application offers an Inspect command (typically in its
context menu, which a touch press-and-hold also opens) only when it helps. A touch double-tap activates its row, as a
double-click does. After a selection callback, activation resolves the touched stable row again, and a checkbox double-
click resolves a nonempty column ID again. Removing that row or column, or replacing the model, cancels the action.
Unnamed columns retain their ordinal contract. Group collapse reports
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

The [owned Unicode rich-text editor plan](../Plans/Done/RichTextEditor_2026-10-17.md)
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
`Document::ReplaceWholeText(text, selection)` accepts complete service text but
records only the scalar/CRLF-safe changed interval, sharing untouched source
pieces. It MUST use the same interval comparison as transient preview updates,
with ordinary one-transaction undo/redo semantics and exact supplied selection.
Equal content MUST preserve source identity/revision/history and acknowledge
selection without allocating source/history storage. Invalid input or allocation
failure MUST leave source, selection, revision and both histories unchanged.
Notifying RichTextEditor service imports MUST use this localized recording path;
preview/cancel imports retain transient semantics. Other controls keep their
existing adapters. Comparison of bounded complete text remains indivisible;
this is not a large-file or total constant-time claim.
Validate future selections against the virtual old-prefix/insertion/old-suffix
before allocating source storage/history, including a newly joined CRLF. A
same-content replacement only acknowledges its exact valid selection; it allocates
no source/history, advances no revision and preserves undo/redo. Stage every
changed tree and required history copy before publishing. Allocation failure in
replacement, transient replacement, reset, undo or redo preserves all state.
An active changed transient preview keeps history entries but makes undo and redo
unavailable until its entry source is restored. A recorded edit that supersedes
the preview records the entry source, not uncommitted preview text, for undo.
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

Tracked edits may reuse a proven unchanged closed Markdown root and its complete source-correlated subtree; prefix roots require an unchanged document-owned separator, and shifted suffixes retain their unchanged continuation through EOF. Unsupported or ambiguous closure, reference-definition staging, incompatible profiles and source-region parses use the normal parser. Reuse is cooperatively prepared, publishes only a complete parse, and rebuilds inline resolution and identities against the new source; cancellation leaves the prior immutable result intact.

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

Rich view exposes an authored link or image title through the host's delayed
supplemental tooltip when its visible content is hovered. Editable editors require
Ctrl; read-only editors do not. A nested image title takes precedence over its
containing link title. Otherwise the control's consumer tooltip remains eligible.
Tooltip selection does not mutate source, selection, history or focus. Source
changes, scrolling, hiding and control detachment retire the previous title.
Editor setters commit before clearing a visible title tooltip; synchronous
embedded invalidation may call a newer setter, and that newer state wins.
After clearing visible tooltip content, the host resolves the hovered target and
text again before publication. This also applies when a generic consumer tooltip
was visible: its invalidation callback may replace the candidate rich document.
An expanded inline source owner's edit help is not an authored hover title.

## Retained RichTextEditor v0 preview (D2c)

Diagnostic native math corpus cases observe recursive parser entry addresses on the actual thread stack and require 128 KiB remaining for the 64-level array, arrow, infix, general-fraction, unary-accent and math-style fixtures, including mixed nesting. The shared corpus also prepares 64-level arrays, fractions and structured binary expressions on the ordinary thread stack. This sampled entry-address guard is not a complete stack high-water measurement. Matrix and substack rows accept either the double-backslash or `\cr` separator. Double-backslash separators accept nonnegative row-spacing dimensions in pt (0–64), em or ex (0–4), using the mathematical absolute-unit scale for pt. Unsupported dimensions and dimensions after `\cr` retain complete formula source.

The bounded math parser supports one `\over`, `\atop`, `\choose`, `\brace` or `\brack` infix operator per math group; nested groups independently admit an operator. Ruled and ruleless fractions and the three fenced forms retain their distinct geometry. Duplicate operators reject the complete formula to source. General fractions and above accept nonnegative bounded rule dimensions in pt (0–64), em or ex (0–4), and general fractions accept single-character delimiters and style selectors 0–3. Unbraced native delimiter commands and braced above dimensions are accepted; unsupported values, units and delimiters reject to source. Semantic layout accepts a finite `mathAbsoluteUnitScale` in (0,16], default 1, applied only to point dimensions; the editor supplies its zoom and geometry reuse includes this value. v0 text layout ignores the semantic-only option. Continued fractions use display style and reserve a numerator strut; optional alignment rejects to source. The array environment accepts 1–64 l/c/r columns with independently aligned cells and bounded rectangular padding; single solid or dashed vertical and horizontal rules stage between or beside cells; repeated rules reject to source. Dashed rules cap each rule at 128 segments and the formula at 32,768 staged lines. Display align, alignat, gather and equation environments support one text tag or starred tag per row. Independently prepared labels align to their actual matrix-row baselines and the containing semantic box trailing edge; duplicate row tags, nested row tags, unsupported environments and inline tagged formulas retain source. Row labels are staged under the shared geometry budget and reused on width-only reflow. Nonumber and notag are consumed without an atom because the profile does not synthesize automatic equation numbers; explicit tags remain independent. Alignat and alignedat admit 1–32 column pairs, pads incomplete rows and adds no automatic inter-column gap. Starred matrix families accept a bounded optional l/c/r cell-alignment argument and retain their fence family. Subarray accepts one l or c column and stages script-style cells; darray uses the bounded array column/rule grammar with display-style cells. Native rcases place the brace on the right; dcases and drcases use display-style contents, with at most two columns. Dot and double-dot accents stage filled circles with their actual ink bounds. Single italic-glyph accents use the logical glyph center plus half the font italic correction, capped at one quarter em; compound bases retain box-centered placement. Native over/under left, right and bidirectional arrows, over-harpoons and double right arrows stage directed strokes; over/under line segments retain both end bars. Native check, widecheck, breve, acute, grave, mathring, underbar and utilde accents stage bounded, direction-specific geometry during preparation. When no usable font variant exists, hat and widehat retain a triangular vector fallback; ring strokes reserve their small-size ascent. Over/underbrace and over/undergroup stage distinct outline geometry and above/below script placement. Textup, textmd and textnormal groups reset the appropriate inherited font attributes; text family, weight and shape groups may nest within the normal depth bound. Flat text and colored text accept single-scalar TeX accents as base-plus-combining sequences; malformed and multi-scalar arguments retain source. Textsf and texttt use configured body and code font families; textrm returns to the math family. The generation-scoped format key distinguishes these roles; emph toggles inherited italic style. Poor-man bold retains the source font, stages an offset glyph overprint and includes that ink in prepared bounds. Scoped legacy math font switches are bounded and respect nested explicit alphabet overrides; bold/frak/Bbb consume one argument. Mathscr accepts uppercase Latin letters in a distinct Segoe Script face only after the system family and complete alphabet coverage are verified; missing coverage refuses the exact formula source. Its system face is an approximation of the pinned KaTeX script font. Unsupported text-family nesting preserves source. Bounded extensible left/right, bidirectional, double, hook, double-headed, opposing, single and paired harpoon, maps-to and equality relations stage their shafts and independently positioned upper and optional lower labels.

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
The [qualification packet](../../Measurements/RichText/README.md)
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
Private semantic DirectWrite preparation consumes bounded display text and runs without fabricating source identity arrays; public Projection preparation retains its strict source mapping validation. Semantic staging remains transactional, and matching public headers and library builds remain required.

Semantic paint stages order-preserving viewport interval indexes for box frames, element decorations, code numbers, heading rules and table rows as part of its unpublished geometry. Draw queries them without allocation and preserves authored paint order even for overlapping bounds. The query includes a 3-DIP paint outset so strokes and antialiasing crossing a viewport edge remain visible; rebuilding an index stages its replacement before publication. Application-supplied decoration rectangles use the same order-preserving interval index to cull offscreen work. Selection normally uses the prepared union mask and draws one viewport pass; if that resource is unavailable, its per-rectangle fallback remains correct and benefits from indexed viewport culling, but may redraw a box more than once.
Whole-layout preparation/reuse/source units are reported separately and cannot be
credited as bounded incremental work. Original height/raster failures remain
retained. All three x64 profiles pass geometry and 108 exact WARP pairs each,
including default/fractional/large-origin cases, height-changing edits, hidden-only
suffixes and real FFFC source. Default 13-DIP native/embedded editors avoid whole
work; native/embedded UIA scrolling preserves exact source. All six build profiles
pass; ARM64 remains compile-only. The [positioned-line record](../../Measurements/RichText/README.md)
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

`SynchronizeReadOnlySourceSnapshot(snapshot, selection)` is an owner-thread setter for a read-only Source view.
It shares the immutable source tree without materializing a replacement string or copying source revision counters.
Changed source advances the target's own monotonic revision, clears only its undo/redo history, and retires the same
highlights, image/source-edit state and accessibility as `SetText`. It invalidates stale geometry while retaining reusable immutable chunks and the mirror scroll offset, clamped to the new extent; ordinary selection/text-service
publication runs last. The parser remains independent and publishes at the target revision. Same-snapshot selection
changes follow `SetSelection`, including clearing visual caret affinity; identical snapshot/selection is idempotent.
An absent snapshot, invalid scalar/CRLF boundary, exhausted target revision or incompatible target profile returns
false without mutation. Valid empty documents are admitted. Detached and embedded setters need no private HWND.
Callers must revalidate their owner/control/source identities after callbacks before continuing a larger update.

`TryAdoptReadOnlyMarkdownSyntax(snapshot, syntax)` optionally reuses a completed canonical parse after source
synchronization. It requires the exact current immutable snapshot, read-only Source mode and matching configured
profile/extensions. Region-scoped syntax is refused even when its resolved region is empty. A mismatch returns
`S_FALSE` without changing pending or published work. Allocation failure returns `E_OUTOFMEMORY` with the prior
publication intact. Ordinary independent parsing remains available whenever reuse is unavailable.

Successful adoption shares only immutable source/AST collections through one flattened canonical owner. Its
wrapper publishes the target's revision and a fresh document identity; node identities are meaningful only with
that document identity. It retains no publisher control, host, worker or graphics resources. Subsequent ordinary
parsing starts a fresh identity namespace rather than reconciling foreign node IDs. Cancel retains publication;
Clear releases it; repeated same-owner/snapshot adoption keeps the same wrapper without allocation.
The setter changes no source, selection, history, text/selection notifications, literal glyph geometry or scroll.
Only syntax-dependent ink/analysis is retired, and preparation is scheduled last. The caller must revalidate
control/root/source ownership after scheduling, which may re-enter or retire its owner.

RichTextEditor document-file metadata MUST be an explicit canonical absolute
Unicode path or empty (unsaved); it MUST NOT infer a base from process CWD or
change source/history/composition. Prepared canonical Rich-mode file-reference paste
MUST generate escaped Markdown images for recognized image suffixes and named
Markdown links for other files, relative to the document parent;
unsaved/cross-root paths MUST use an absolute file URI. Component case MUST be
preserved, including case-sensitive directory correctness. Code/HTML/metadata,
active source scopes and partial atomic inline contexts MUST retain literal
paste. Multiple files MUST preserve clipboard order, including mixed images and links. Ordinary paragraphs use blank
paragraph separators; list and quote continuations retain their containing prefixes,
and a table cell uses a single space so no row or table is split. Ambiguous or
cross-container source selections MUST refuse rather than alter surrounding structure.
Image descriptions use the filename stem; text link
labels retain the complete filename. A path exceeding the 512-component lexical
reference budget is unrepresentable, even when it fits the clipboard size limit.
If any structured path is unrepresentable,
the complete original list MUST remain quoted literal text; Rich-mode source escapes
Markdown punctuation while opaque/Source mode retains raw spelling. One paste MUST record one transaction
and preserve exact undo/redo and backward selections. Path construction is
lexical, bounded and fallible; no control-owned I/O, file copy/move or remote
fetch is implied. Display requires caller-serviced immutable image resources;
unserviced or declined resources retain their alternative. File links retain
the existing inert link presentation. Paste MUST NOT activate/open a file, query
existence or register drag-and-drop; classification uses the path suffix only.
An opt-in bitmap-paste handler may save validated owned DIB bytes and return a
supported absolute image path. It runs after clipboard release; the editor
rechecks source, selection epoch, binding, focus and composition before the
same one-transaction file-reference insertion. Invalid, canceled or stale
results do not mutate source. The application owns any asset side effects.

The file-drop transport uses the same displayed-image/relative-file-link
policy as paste. Its insertion target MUST be a prepared source caret at the drop point;
preview MUST NOT move the existing selection, and one accepted file list MUST
produce one atomic insertion whose undo restores that selection. Native and
embedded transports MUST share path/context rules and the application-owned image
pipeline. Native and embedded applications own OLE registration and routing;
the library provides the bounded adapter and guarded insertion services. The
[file-drop plan](../Plans/Done/RichTextFileDrop_2026-10-20.md) retains current-source
qualification and consumer adoption separately from implementation.

Pure structural command/deletion planning MUST reject syntax carrying a
presentation-only source-region hint before staging any document change, even
when the requested selection lies outside that region. Callers obtain canonical
syntax for structural planning.

Raster image resources MUST be immutable premultiplied BGRA, fully decoded before
owner publication. Separately enabled SVG resources retain immutable admitted
bytes and use the bounded preparation contract below. The raster profile uses the built-in PNG/JPEG/BMP/GIF/
TIFF codecs and first static frame; GIF canvas placement/transparency/background
and valid JPEG/TIFF EXIF orientation MUST be preserved. Missing, malformed or
unsupported optional orientation metadata MUST fall back to unrotated decoded
pixels; out-of-memory remains a failure. Unsupported resources MUST keep
the source-exact alternative. Admission is at most 16 MiB encoded, 4096 per side,
4,194,304 decoded pixels, 32 unique active image sources and 64 MiB unique retained
CPU pixels. Admitted image placements, mathematical expressions and diagrams
share a source-ordered limit of 128 objects. An image beyond that limit retains
its text alternative and authored title, and reports an ImageCapacity diagnostic
over its original source. Rejected objects do not consume an admitted slot.
Shared immutable pixel objects MUST be counted/uploaded once. Other source/format
capacity contracts are unchanged; process/driver and transient staging overhead
require separate measurement.

Local destination resolution MUST use explicit document metadata, strict UTF-8
URI decoding and bounded lexical Windows paths. Encoded separators/NUL, device
names, ambiguous extended/trailing-component spellings and malformed/remote URI
schemes MUST NOT become file requests. Drive/server root case is insensitive;
directory/share/file component spelling remains exact. Relative paths require
a saved base; the control MUST NOT query CWD or perform file/network I/O.

Remote image admission MUST be disabled by default. Explicit
`SetRemoteImagesEnabled(true)` MAY expose absolute HTTP(S) destinations as
`RichImageSourceKind::RemoteHttp` requests. `Source()` retains the URL's exact
path/query while normalizing ASCII scheme/host case; `FilePath()` MUST be empty
for a remote request. Admission accepts ASCII DNS/punycode and bracketed IPv6
hosts, rejects credentials, fragments, controls, malformed percent escapes and
unsupported schemes, and bounds each URL to 8,192 UTF-16 units. This is lexical
admission, not network authorization or a fetch implementation. A caller MUST
apply its own transport, TLS, timeout, cancellation and encoded-size policy,
then reuse the existing bounded `RichImage::Decode` utility on its worker.
The library MUST NOT own HTTP, DNS, a worker, a proxy, cookies or a network cache.
Disabling the policy MUST immediately cancel remote jobs and evict their retained
pixels. Local jobs/pixels MUST retain identity and admissible completion during
the policy-only bounded preparation rebuild. Re-enabling remote admission creates
fresh remote identities; old completions MUST never republish pixels. Every policy
transition MUST preserve exact document source, selection, history and IME state.

Data image admission is independently disabled by default. When enabled with
`SetDataImagesEnabled`, a bounded, well-formed opaque `data:` destination
produces a `RichImageSourceKind::Data` request whose `Source()` preserves the
exact spelling and whose `FilePath()` is empty. Admission does not decode or
authorize its media type: the application owns validation and decoding under the
same image limits. Disabling it cancels its jobs and evicts its pixels without
retiring local, remote or wiki jobs; toggling the remote policy likewise retains
data jobs. Stale completions cannot publish after cancellation.

An application may register an image activation intent callback. Ctrl+click
and Ctrl+Enter request activation on a current prepared image; an enclosing
link retains its existing navigation priority. The callback borrows the exact
semantic source range, destination, title and alternative while the immutable
publication is held. A stale publication or active composition is refused;
the library performs no navigation or source/history mutation. Image UIA source
editing retains its existing action and is not changed into activation.

Animation timing and decoding remain application-owned. The owner may submit a
bounded immutable replacement frame against a completed image request and the
exact current acknowledged semantic document. Stale source, binding, pending
preparation or composition refuses the replacement; a refused frame leaves the
current pixels intact. Frame publication shares the existing image count and
decoded-byte limits and does not mutate document source, selection or history.

SVG image completion is separately disabled by default. Callers MAY explicitly
construct an immutable SVG resource with nominal dimensions and enable the
editor policy. Admission accepts ASCII XML in the SVG namespace with `svg`,
`g`, `path`, `rect`, `circle`, `ellipse`, `line`, `polyline` and `polygon`
and attributes `viewBox`, `width`, `height`, `x`, `y`, `cx`, `cy`, `r`, `rx`,
`ry`, `x1`, `x2`, `y1`, `y2`, `d`, `points`, `fill`, `stroke`,
`stroke-width`, `opacity`, `fill-opacity`, `stroke-opacity`, `stroke-linecap`,
`stroke-linejoin`, `fill-rule` and `transform`. It rejects DTDs,
entities, style sheets, references, embedded images, scripts and animation; in
particular `use` is not supported. This profile is bounded to 262,144 encoded
bytes, 2,048 elements, depth 64 and 65,536 aggregate path/point units; embedded
images have a zero budget. The existing dimension, pixel-equivalent and total
retained-byte limits still apply. XML admission belongs on a caller worker.
Direct2D SVG document, stream and private same-device context creation occur in
preparation. The bounded SVG raster is prepared at the authored, viewport-clamped
size and current DPI in budgeted pixel tiles. Its logical BGRA pixel bytes and
source bytes share the existing candidate retained-image ceiling; identical resources share a raster
only at the same pixel dimensions. Completed GPU bitmaps are private to the
device generation; Draw uses the ordinary bitmap path, with no SVG parsing,
device queries or SVG-specific resource creation. Unsupported targets, oversized
rasters or unsupported SVG content keep that object's alternative; device loss
follows normal preparation recovery. Disabling SVG evicts SVG resources.

In the opt-in Subtle Markdown style, fenced and indented code blocks have a neutral themed fill and a six-DIP inner frame; narrow viewports clamp horizontal insets to retain a one-DIP text width. Their source caret and selection use the inset content geometry. A fenced block with a language token of at most 128 UTF-16 units shows that token in a nonselectable prepared header; longer labels are omitted. When the frame has room for both its font-scaled gutter and at least 56 DIP of content, a nonselectable gutter displays prepared physical line numbers, aligned after wrapping and mirrored for right-to-left layout. Preparation admits at most 4096 number layouts per document, omits blocks with more than 4096 visual lines and stops numbering when that cap is reached; narrower frames omit the gutter. Default presentation retains its original code frame.

An application may register a localized accessible name and code-copy intent callback. In Subtle style the prepared header then exposes a pointer action and a distinct UIA Invoke child; Ctrl+Shift+C invokes the block containing the source caret. The callback receives a borrowed counted rendered-code view with the block's normalized line endings, including any NUL; the application owns clipboard format policy. The existing code-element Invoke remains source editing. Stale, canceled, detached, or retired gestures and peers cannot invoke a replacement owner, and the action never changes source, selection or history.
Source presentation may disable wrapping and opt into physical line numbers and space, tab and line-ending guides. The line-number lane grows with source length and zoom, clamped to preserve at least half the available text viewport. The gutter and guide coordinates are staged cooperatively from the exact counted source and current geometry; they do not add source units, selection, or text-service content. CRLF is one physical ending, and the trailing empty line remains addressable. Published guides use the current viewport and font, support high contrast, and Paint reads cached layouts/coordinates. Horizontal extent and offset apply to unwrapped Source and opt-in overflowing Rich tables; scrolling changes neither source nor history, translates painting, hit testing, TSF and UIA geometry together, clamps to the available extent, and notifies the registered owner callback. Shift+wheel may scroll horizontally; edge input bubbles. Source right-to-left scrolling follows the opposite visual direction.
An optional code tokenizer receives borrowed language and rendered code views on the owner preparation thread. Built-in JSON, C-like, PowerShell and diff tokenizers supply bounded comment, keyword and literal spans. Application results must contain at most 4096 sorted, disjoint, nonempty UTF-16 ranges within the code body, without splitting surrogate pairs; invalid results or tokenizer failures use ordinary ink. The editor stages all ranges until the current semantic publication is complete, validates owner, binding and source after every callback, and invokes at most one code tokenizer per preparation turn. Paint uses the cached ranges; selected text and high contrast take precedence. Changing the tokenizer requests a fresh owner preparation without editing source or history.
Image requests MUST execute in caller-owned scheduling, with callback-free
cancellation and source/base/binding/visibility validation at owner completion.
The caller authorizes file access, including any UNC policy. Source replacement,
presentation withdrawal, hide/rebind/destruction MUST cancel old jobs; completed
pixels MAY survive unrelated edits and device replacement. For a tracked edit, an in-flight request MAY remain active when at least one prior image source range is untouched; completion stays private until the new semantic document confirms its source and binding. An affected source range or untracked replacement cancels the old request. Failed/capacity
completion MUST remain cached across unrelated source edits while preserving editable source/alternative. Explicit reload, document location or remote-image policy changes retry failed sources. Resource statistics count only actual failed completions as failed sources; canceled requests are neither pending nor failed while awaiting re-admission.
Replacing accessibility labels without changing source retains eligible in-flight image requests and already-staged pixels privately until the new semantic document rebinds them. Resource-only completion MUST NOT edit source/history/selection or cancel IME. Multiple accepted image completions before the next owner preparation stage pixels privately and publish one immutable resource snapshot; allocation failure preserves the previous publication and leaves staged pixels retryable.
Request preparation processes at most four image candidates per owner turn;
the editor's image-work diagnostic counts actual visits separately from geometry block work.
caller adapters MUST wake valid queued work when draining or cancel-pruning
results frees a completion reservation; closure MUST serialize posting with
owner retirement without joining a codec/filesystem call.
Identity-orientation raster decoding writes into the unpublished owned pixel
buffer and publishes that buffer only after successful decode and cancellation
checks; it does not allocate a second full decoded image merely to copy pixels.
Non-identity orientation and GIF canvas composition retain their bounded staging.
GPU preparation performs at most one data upload per turn and never exceeds
`LayoutWorkBudget::maximumUploadBytes` (4 MiB by default, minimum 4 bytes for
image-capable semantic geometry; text-only/v0 services ignore this budget).
An image fitting the budget uses one bitmap creation/upload. Larger images
allocate private bounded bitmap storage, then copy complete pixel tiles across
turns; even a budget smaller than one source row makes progress. Bitmap allocation
itself is indivisible and remains subject to image dimension/decoded-byte bounds.
Only complete bitmaps can enter a published geometry; duplicate placements share
the completed bitmap. A width-only reflow on the same factory, immutable image
snapshot, target and resource epoch reuses completed raster bitmaps by image
identity without another upload or tile copy. A target or epoch change rebuilds
the target-bound bitmap. `LayoutAdvance::uploadedBytes` reports transferred BGRA
pixel bytes, excluding unused source-row padding. Native image
geometry MUST use actual inline baseline/aspect rather than the larger SDK line
selection cell. Text ACPs, UIA alternatives and structural atomicity remain intact.
Only the private SDK layout buffer MAY neutralize image-alternative code units
to equal-length object placeholders; authoritative source/display strings,
alternative names and maps MUST remain unchanged. This prevents script/emoji
analysis from splitting one atomic image into multiple native rectangles.
Reused placements MUST consume exactly one preparation event, without an extra
common phase-tail count. Invalid zero layout/event budgets or semantic upload
budgets below four bytes MUST preserve staging
and create no bitmap. Bitmap memory/device HRESULT failures MUST retire staging,
preserve the prior complete geometry and permit an explicit current retry.
`D2DERR_UNSUPPORTED_OPERATION` for an image upload is target-local: preparation
uses the image alternative, tries duplicate shared pixels only once for that
preparation, and permits a retry when the preparation key changes.

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

`MarkdownSyntax.h` supplies immutable exact source, a root source partition,
a linked container/leaf tree and an independent complete lexical partition.
`Nodes()[0]` is the document; revision-local parent/firstChild/nextSibling indices
express source order. Container ranges include their raw prefixes and contain
child ranges. Blank trivia belongs to Source/Blocks rather than semantic nodes.
Node content-line spans exclude container prefixes and retain any unconsumed
columns of a partly consumed tab. Delimiters, original line endings and unknown
content are never reconstructed from decoded values.

Block recognition includes nested quotes and ordered/bullet lists, empty items,
lazy continuation, interruption precedence, tight/loose lists, indented/fenced
code, ATX/setext headings, thematic breaks, paragraphs and multiline reference
definitions. Reference label/destination/title ranges and prefix-stripped lines
remain exact source; global label folding and resolution follow the block pass.
The pinned dialect's seven HTML-block forms are opaque literal source, with
container/EOF termination and their appropriate closing/blank-line conditions.
CommonMark 0.31.2 textarea/search behavior remains distinct from strict GFM 0.29.
Resolved inline forests and GFM extension metadata are implemented in M3; its
bounded profile qualification is retained in the [sealed M3 packet](../../Measurements/RichText/README.md). Later bounded semantic rendering and editing are implemented as specified below; final-source qualification remains separate.
BlockStatistics records deterministic prefix-inspection and container-finalization
work. Indentation lookahead stops at its required threshold; a line's marker
metadata rejects incompatible thematic suffixes without rescanning each nested
list suffix. Open containers share a deferred accepted source end and finalize
once, preventing lazy lines from updating every ancestor. These counters cover
the named work, not complete parser CPU or a latency guarantee.

DocumentIdentity scopes nonzero node identities to one parser lifetime until
Clear; move transfers that scope, and reuse of the moved-from parser creates a
new one. Revisions retain immutable old trees. Reparse the entire bounded block
source before reconciling identities; the older projection's line-local widening
is unsafe for fences/containers/references. Prefer Begin(SourceSnapshot, ...):
tracked source ancestry distinguishes equal paragraphs and pending sibling edits.
Trusted unchanged/tracked/full-replacement snapshot metadata MUST be honored
before reading source units for common-prefix/suffix correlation. A full
replacement MUST NOT populate an old-child identity index that cannot be reused;
the parser's document-root identity remains stable and old child identities are
discarded. String-only input still needs the existing UTF-16 unit comparison.
This removes redundant correlation work, not source materialization, line scans
or the remaining bounded parser stages; it is not a wall-clock deadline promise.
Map unchanged prefix/suffix and compatible enclosing nodes under the same parent
identity; changed kinds, reparented nodes, lost ancestry and newly created nodes
receive new identities. Deleted identities are not resurrected from an unbounded
syntax history. At edit boundaries prefer an exact surviving prefix/suffix node,
then a compatible enclosing node; deleting its first/last scalar must not replace
the identity of a paragraph which remains present. Distinct generated missing
table cells can share one empty source range: reconcile equal keys in their
source traversal order, consuming each prior identity at most once. Unchanged
cells retain distinct identities across unrelated edits and undo/redo.
String Begin uses deterministic
common-prefix/suffix mapping and
cannot infer which equal paragraph an external caller removed. Profile changes
may replace node identities; Clear explicitly starts a new document namespace.

Every result carries its CommonMark0312, Gfm029 or RichMarkdown target, extension
options and source revision. M3 supplies base inline parsing and specified GFM
extensions. Later sections define the implemented bounded RichMarkdown math, Mermaid, footnote, front-matter and semantic editing profiles; full conformance and final-source qualification remain separate.
The Gfm029 HTML declaration block start requires an uppercase ASCII letter after
`<!`, as in its pinned grammar; CommonMark0312 and RichMarkdown accept either
ASCII letter case.
When a quote prefix fails to match, a valid list marker starts a new block
outside that quote, including an empty item or a non-one ordered start. A
type-seven HTML opener closes that quote in Gfm029; CommonMark0312 and
RichMarkdown preserve the lazy paragraph continuation.
For a valid GFM table delimiter after a multi-line paragraph, only its last
physical line becomes the table header; preceding lines remain a paragraph,
and each block retains its original source prefixes and EOLs.
When reference definitions are split from a following paragraph, each block
owns its own physical-line prefix and EOL; prefix-stripped content coordinates
remain distinct from the complete source range.
Begin validates/owns at most 65,536 UTF-16 units, accepting raw NUL and rejecting
unpaired surrogates. Reference-label folding replaces raw NUL with U+FFFD before
matching, consistently with normalized inline text; exact Source() stays intact.
The RichText document, UTF-8 codecs, text-service state and UIA retain counted NUL as an exact source unit. Clipboard CF_UNICODETEXT cannot represent counted NUL: copying a selection containing one fails without changing clipboard or source. Other controls retain their own admission rules.
Equal source/revision/profile/change requests coalesce.
Snapshot admission uses bounded source materialization and tracked change queries;
unchanged completed snapshots avoid materialization. Invalid/failed Begin keeps
the old request; failed Advance cancels staging and keeps the prior publication.
One staged and one published syntax are retained. Source/lexical scanning, root
partitioning and identity indexing/reconciliation resume at stored cursors.
Zero budget does no work. Budgets count steps: physical-line transitions,
paragraph-reference finalization, construct lookahead, emphasis resolution, sorting, comparisons and allocations
remain bounded indivisible work, not per-character or wall-clock guarantees.
Lexical candidates cover ASCII escapes, semicolon named/numeric references,
line endings and NUL replacement. Code/HTML/link/delimiter context must resolve
candidates before their values can become formatted text.

HTML recognition and standard expected HTML are independent of product rendering.
The intended native formatted renderer uses an explicit safe supported subset;
scripts, event attributes and automatic remote resource loading do not execute.
Unsupported/active HTML stays visible editable source. There is no browser DOM.
This stage supplies syntax metadata and does not implement semantic HTML display.

### Resolved inline and GFM analysis

`MarkdownSyntax::Inlines()` is a dense immutable forest rooted by each leaf's
`firstInline`. It resolves ASCII escapes/entities, Unicode punctuation flanking,
emphasis/strong and the rule of three, exact-length code spans, inline/reference
links, nested-link exclusion, image alternatives, URI/email autolinks, inline
HTML and soft/hard breaks. Code and HTML remain opaque to entity/escape decoding.
Code values normalize physical line endings to spaces and apply the standard
single leading/trailing-space rule. Exact source spelling is retained separately.

`MarkdownExtensions` defaults to None for CommonMark, All for Gfm029 and
RichAll for RichMarkdown. Explicit options select Tables, TaskLists, Strikethrough, Autolinks,
TagFilter and the RichMarkdown-only Highlight, Superscript, Subscript, Inserted, WikiLinks,
DefinitionLists and Abbreviations flags; the latter two are opt-in and are not
included in the existing RichAll default. RichOptionalMask is the full admissible
RichMarkdown bit mask. `SetMarkdownAnalysisExtensions` selects an explicit mask
for the editor's current profile, or null to restore its default; changing profile
resets the explicit mask. The mask participates in syntax adoption, cancellation,
semantic publication and reuse identity; changing it preserves source and history;
unknown bits and unsupported profile/option combinations are rejected
atomically. Options participate in request coalescing/publication identity.
RichMarkdown resolves exact `==...==`, `^...^`, `~...~` and `++...++` delimiter
runs as marked, superscript, subscript and underlined insertion respectively;
`~~...~~` remains strikethrough. Delimiters obey inline flanking and nesting,
code/HTML/math ownership and exact source mapping. GFM/CommonMark keep the new
spellings literal. Invalid run lengths remain literal. The existing script
renderer prepares typography and falls back to the exact owning source leaf if
the target cannot draw script content; a matching resource-key change retries.
RichMarkdown WikiLinks accepts `[[target]]`, `[[target|alias]]` and their
`!`-prefixed image embeds. Targets and labels are counted, case-sensitive
unresolved text of at most 2048 UTF-16 units each. Empty, nested, escaped,
multiline, control-bearing or multiply separated candidates remain literal;
code, HTML and math retain precedence. Link activation exposes the target,
label and wiki identity to the application instead of following local fragments.
Wiki embeds create bounded `WikiTarget` image requests with an empty FilePath;
the application resolves the target and supplies immutable pixels. Neither
path performs filesystem, vault or network resolution. Lookahead work is
cooperatively bounded and amortized linear over a leaf.

With DefinitionLists enabled, one plain term line followed by 1–16 `: `
description lines in the same paragraph projects a source-owned definition
list, term and descriptions. Descriptions support single-line Markdown inline
formatting; HTML and inline owners crossing lines stay ordinary. Each line is
at most 1,024 UTF-16 units; empty, over-bound, malformed or aggregate-capacity
rejected forms retain normal Markdown presentation. With
Abbreviations enabled, up to 128 unique top-level single-line declarations of
`*[KEY]: expansion` are recognized; keys are 1–64 ASCII letters/digits, and
expansions are 1–512 UTF-16 units without controls or angle brackets. Their first
accepted declaration is hidden only in semantic presentation. A matching exact,
case-sensitive standalone ASCII word gets a source-owned abbreviation element,
its authored expansion as UIA HelpText and the ordinary read-only/Ctrl hover title.
Code, transformed tokens and words adjacent to Unicode or Markdown delimiters
are left ordinary. Duplicate, malformed or over-capacity declarations remain
visible as ordinary Markdown. Accepted hidden declarations retain F2 source
editing. Declaration collection and text matching yield cooperatively. Changing
the extension mask preserves same-source in-flight image requests. No source,
selection, history, or network behavior
changes, and CommonMark/GFM cannot enable these two flags.

GFM tables are recognized before cell inlines, retain alignment/header metadata,
unescape cell pipes even in code, pad missing cells and retain ignored excess
source. Task metadata belongs to the list item, requires its first paragraph
and marker whitespace, and does not yet supply an interactive checkbox command.
Strike requires two tildes under this pinned profile.
If reference extraction removes an entire candidate paragraph, clear the active
leaf before processing the table delimiter line. A definition-only paragraph
cannot serve as a header or be extended/extracted again after retirement.

Extended autolinks respect bracket/code/HTML ownership, consume URL interiors
before remaining emphasis, and retain literal entity/escape spelling inside the
URL. Text surrounding them is contextually decoded. Opaque Html values carry
`filteredHtmlOpeners`: UTF-16 value offsets whose '<' is filtered by the GFM
tagfilter rule. This is metadata, not sanitization or a browser execution API.
Tagfilter matches complete names followed by GFM ASCII whitespace (including VT),
`>` or `/>`; a slash followed by other text is not a tag terminator.

Label comparison uses locale-independent full default Unicode 17 case folding,
without Unicode normalization. First definition in source order wins. CommonMark
collapses label spaces/tabs/line endings; legacy GFM also includes VT/FF under
its ASCII-whitespace definition. CommonMark treats Unicode P/S as punctuation;
strict GFM uses ASCII punctuation plus Unicode P. Inline declaration rules also
remain dialect-specific. The pinned GFM printed short-comment expectations are
retained even where adjacent older prose differs. Strict extended domains require
a period/nonempty segments and reject underscores in their final two segments;
Unicode letter/number domains are recognized from pinned data.

Inline indices are revision-local; block identities do not validate inline
semantics. Any reference edit reparses dependent inlines even when the referring
paragraph's source/identity did not change. `value`, `destination` and `title`
are semantic strings; even Text can contain decoded escapes/entities. `source`
is the enclosing raw span, not an interior ACP map. M4 must provide transformed,
generated/repeated/object editing affinities before renderer integration.

`LexemeSourceAnchor` keeps transformed-value leading/trailing affinity explicit,
including a named reference producing two Unicode scalars. Original spelling
always remains available. Text lexical ACPs are not automatically legal editing
boundaries; callers retain the model's scalar/CRLF checks. These lexical anchors
do not replace a complete semantic map.

### Semantic coordinate and transaction primitives (M4a/b)

`RichSemantic.h` adds a preparation-only `SourceDisplayMap`, independent of v0
Projection's monotonic constraints. Its immutable source/display strings and
display-ordered spans distinguish exact source-backed spelling, atomic transformed
values, generated text with empty source ownership and atomic source objects.
Every display unit belongs to exactly one nonempty span. Primary source ownership
is disjoint; repeated spans require an exact primary source range and kind.
Display order may differ from source order. Admission caps source at 65,536 UTF-16
units, display at 131,072 and spans at 65,536, before copying/index allocation.
Malformed UTF-16, scalar/CRLF splits, unknown enums, incomplete coverage and
ambiguous primary ownership are errors. This does not raise Document capacity.

Leading/trailing affinities resolve shared edges, hidden source and transformed
interiors explicitly. Source caret lookup uses the declared primary occurrence;
display hits on a repeated occurrence still resolve to original source. Hidden
source interiors use the next/previous primary edge, with display end/start as
outside fallbacks. Exact owned span edges retain their corresponding display
edge. These are scalar/CRLF coordinates, separate from logical grapheme navigation.
`SnapDisplayBoundary` moves an arbitrary bounded ACP to the chosen side of an
atomic span or scalar/CRLF join.

`DisplayRanges` and `SourceRanges` return sorted merged fragments, rather than
an enclosing endpoint interval. Generated spans contribute no source-selection
glyphs or replacement ranges. A nonempty intersection with a transformed/object
span expands to its entire opposite range; empty ranges stay empty. Repeated
source fragments are deduplicated, so a visual deletion cannot accidentally
replace unrelated source between reordered occurrences. Callers normalize a
backward selection before range lookup while retaining its anchor/caret direction.

`ApplySourceEdits` requires the exact immutable expected SourceSnapshot and the
current Document revision. Numeric revision equality and equal foreign spelling
are insufficient; `SourceSnapshot::IsSameRevision` compares immutable ownership.
Edits use ascending disjoint source ranges; touching ranges are allowed but two
edits with the same start are rejected. Replacement strings are borrowed only
for the synchronous call. Validate ranges, replacement Unicode and resulting
capacity, stage one replacement envelope preserving all untouched gaps, then
delegate future-selection/join validation and publication to Document.Replace.
A source change records one revision/history entry; unchanged spelling only
updates a valid selection. Exceptions preserve source, selection, revision and
both histories. This helper does not bypass a retained control's editable,
notification or lifecycle guards. RichTextEditor exposes its own ApplySourceEdits
adapter through the existing Changed/SelectionChanged path, enforcing those
guards and invalidating stale parser/geometry access before notification. It
preserves the pure helper's single history entry and exact source coordinates.
Command-state list membership covers the complete selected source interval,
including nested items and unlisted gaps, rather than sampling only endpoints.
The query uses a small bounded stack and parent-link recovery at deeper nesting;
the latter may add ancestor-depth work. It does not allocate or build edit strings.
Heading availability shares the planner's source and container-prefix admission,
including task-list restrictions, Setext conversion and resulting source capacity.
Its prefix proof uses a bounded stack with parent-link recovery at deeper nesting;
the recovery can add ancestor-depth work. Inside resolved footnote definitions,
an unprepared positive command availability remains unknown until the planner's final physical
prefix checks can also be proved without constructing edits. Consumers needing
exact availability for an arbitrary command and its payload may call
`PrepareMarkdownCommandState` on the owner thread, then read the same request
with `QueryMarkdownCommandState`. Preparation uses the actual command planner,
owns the request payload and keeps at most one result; it may allocate or throw,
does not commit an edit or notify consumers, and preserves the previous prepared
result on failure. A false preparation result means the owner or semantic input
is currently unavailable. Query remains allocation-free: an unprepared request
has unknown availability unless a cheap proof applies, while a matching prepared
result reports the planner's exact availability. The key includes source and
syntax identity, selection epoch, attachment, path policy and the current table
caret site and geometry. Source, selection, presentation, enabled, read-only,
attachment and geometry retirement discard the cached result. Composition
always refuses command execution and reports unavailable during composition;
if it ends without changing the keyed inputs, the same prepared result remains
valid. Consumers must prepare again when a key changes.
When a focused native input session has an active or indeterminate composition,
command-state availability is false for every structural command, matching
execution; current style and list presentation remain queryable.
At the start of a heading's visible content, `JoinBackward` removes its ATX or
setext heading spelling through the same structural heading conversion; inside
the heading it remains unavailable. The operation is a single undo transaction.
At the first paragraph of a quote, a backward join outdents the entire quote
only when its outer prefixes are proven; parser-owned lazy continuation lines
remain exact. Otherwise it leaves the source intact. The nearest quote or list
container determines which level is removed.
`SourceSnapshot::LocationAt` and `RichTextEditor::GetSourceCaretLocation` return
zero-based logical source line and UTF-16 column, treating CRLF as one break.
They visit immutable source pieces without allocating or flattening, in time
proportional to the inspected prefix. Invalid snapshots, offsets and edit
boundaries have no location; visual wrapping and reordered content do not alter it.
`GetPreparedCaretWordSourceRanges` requires current prepared layout and returns
the caret's UIA word expanded to complete Unicode graphemes as source-owned
ranges. Hidden delimiters and generated display text do not become source;
transformed spans retain their complete source owner and disjoint ranges stay
separate. Queries copy a local prepared-display window, expanding only until
word and grapheme boundaries are proved; an unusually long word or cluster may
require the full bounded display. A stale layout or storage failure
returns no result.

### Semantic document preparation (M4c bounded preview)

RichSemanticDocument.h defines an immutable SemanticDocument and an owner-serialized
SemanticBuilder. This CPU component does not enable the retained renderer by
itself. Begin retains immutable MarkdownSyntax; Advance
resumes block/inline events and publishes one complete map, style partition,
role graph and ordered geometry-paragraph metadata. Held publications remain
immutable. Identical syntax ownership coalesces; a different syntax object with
an equal numeric revision remains a different request. Failed Begin preserves a
previous pending request; failed work retires staging and preserves publication.
Cancel keeps publication; Clear releases builder ownership without invalidating
held documents. No worker, graphics object, HWND or timer belongs to this service.

Admission bounds the combined block/inline graph to 32,768 nodes and geometry
paragraphs to 4,096, in addition to Document and SourceDisplayMap capacities.
These are preview admission limits, not larger-document or all-input latency
qualification. Counted source NUL is retained by Document/codecs/text services;
the Markdown normalization rules replace it only in the derived display value.
Each event is counted; leaf normalization, value tokenization, image alternative
flattening and final map/index construction are bounded indivisible operations,
not hard time deadlines. Empty/padded cells use generated content; amplification
must fail before SDK work. Ordered list numbering visits each sibling once.

Every resolved Text/Code/Html value must be reproduced from normalized source
tokens before publishing its interior map. Literal URL labels retain contextual
entity/escape spelling; ordinary Text decodes lexical references/escapes. Code
retains opaque spelling, normalizes line endings to spaces and hides its fences
and standard trimmed spaces. Removed container prefixes stay outside glyph spans.
Virtual tab columns are generated, not duplicated source ownership. Soft breaks
default to flow spaces; opt-in `SoftBreakPolicy::LineBreak` displays them as
line breaks while retaining the exact source range. Hard breaks remain line
breaks. Changing this policy retires the semantic publication without changing
source or history. Generic source fragments
do not by themselves implement grammar-aware line joining or empty-format cleanup.

The role graph retains headings, quotes, lists/items, code, tables/rows/cells,
links, images, task objects and rules. Cells retain header/alignment/row/column
metadata. Source task markers and image alternatives are atomic source objects;
list markers and padding are generated. Destinations/titles are inert metadata;
no resource fetch, script, DOM or implicit navigation follows. The current pure
builder uses the bounded native HTML policy below, with balanced validation and
source-preserving literal fallback. Rendering, source caret sites,
structural commands are specified in M4e below. The semantic UIA contract is in
[input/accessibility](UI_InputAndAccessibility.md). The bounded M4f implementation
is [functionally qualified](../../Measurements/RichText/README.md);
physical IME/AT, native ARM64 runtime and D4c performance acceptance remain open.

`AccessibilityLabels` is immutable, callback-free configuration for library-generated
semantic names/help: footnotes, equations, diagram vocabulary/fallbacks, metadata
diagnostics and source-entry/exit hints. `SemanticBuilder::SetAccessibilityLabels`
and `RichTextEditor::SetAccessibilityLabels` accept a shared const dictionary;
installing a different identity schedules retained native or embedded preparation,
while reinstalling the same identity leaves settled work unchanged. The host
scheduling callback may retire the editor; it is the setter's final owner access.
Null uses byte-identical English defaults. Entries copy their construction-time
views and require a unique known `AccessibilityTextId`, nonempty valid UTF-16 with
no NUL and at most 1,024 units per entry. Missing entries use their English default.
Footnote name/help templates contain `{0}` exactly once. Return-reference
name/help templates contain `{0}` (reference ordinal) and `{1}` (footnote ordinal)
exactly once each, in any order; no other braces or format directives are accepted
in numbered templates. Other entries are literal text. Invalid configuration
throws before it can be installed; construction and preparation remain fallible.
Each native diagram family exposes its own header and help, including pie slice
and Gantt task vocabulary, identifying/non-identifying ER relationships and empty
family diagnostics. Display and fenced math closing-delimiter diagnostics use
the same dictionary. Authored titles, labels, dates, values and TeX remain exact.

Changing dictionary identity retires pending/published semantics and stale UIA
children before rebuilding. Held documents retain their own immutable dictionary.
The same dictionary is a no-op. A retained editor preserves exact source, selection,
history, source-editing region and focus, and emits no text-change callback.
Authored names, labels, destinations, titles and descriptions remain authored
text; localization changes no visible projection or source/display ownership.
UIA reads use only immutable document data and invoke no application localizer.

These pure primitives remain independent of graphics. The explicit retained
rendering preview below consumes them. M4e adds source-exact structural editing;
semantic UIA uses the acknowledged document and prepared source-format runs.
Profile-only analysis keeps v0 rendering.

### Retained semantic rendering preview (M4d)

`SetMarkdownRenderingEnabled(true)` explicitly opts a RichTextEditor with a
non-null analysis profile into the semantic native geometry backend. The default
and profile-only analysis retain v0 pixels. Clearing the profile disables this
option. Source view always displays exact raw source; the requested Rich view is
preserved when semantic admission instead requires SourceFallback.
`GetMarkdownRenderingStatus` distinguishes Disabled, Pending, Semantic and
SourceFallback. `GetSemanticDocument` returns only the completed current syntax
owner's immutable CPU document and never prepares work. Geometry readiness is
separate: a completed semantic document can precede its SDK layouts.

The backend stages independent paragraph/cell DirectWrite layouts, styles,
indentation, equal-width table columns with declared physical alignment, RTL
column placement, quote/rule lines and code/image/cell outlines. Destinations are
inert; images display caller-supplied native pixels when available and otherwise
their owned alternative. The control does not download media. Links use
native underlining in inherited text ink. Generated list/task markers are preview
text; M4e task gestures commit exact source checkbox edits through the host.
Every host turn bounds SDK calls and metadata events independently. Layout,
row placement, source line indexing and element bounds must complete before one
publication; input/paint/geometry queries never drain staging. No HWND, worker,
timer, graphics device or scheduler belongs to this backend.

`ScrollSourceRangeIntoView` is a geometry query. It validates the current owner,
enabled state and source boundaries, then returns `false` while the prepared
layout is unavailable. Pending geometry is normal control flow: the query does
not throw, advance or reschedule owner preparation, or change scroll, selection,
history or source. With a current layout it retains the existing caret reveal
and source-range scrolling behavior.

#### Native markers, links and HTML (M4d.3)

RichTextEditor presentation is library-owned. `SetAppearance` MUST validate its
complete value before publication and preserve source/history/selection. The
default input frame, host body/code families, density inset and native line
spacing remain compatible. Opt-in borderless/window-background presentation,
centered maximum text width, font overrides, line-height multiplier and heading
scale or per-level heading sizes/heights MUST govern the same geometry used for drawing, caret, selection, pointer
hit-testing and accessibility. Invalid/nonfinite values cannot leave partial
state. Appearance geometry changes retire pending/current geometry; paint-only
changes invalidate pixels without reparsing or editing. Explicit line height is
font-size-relative (for example 16 DIP * 1.65); inline image/math ascent/descent
must still fit completely. Borderless presentation retains a keyboard-visible
focus outline and a persistent outline while focused in high contrast.

`RichTextLinkUnderlinePolicy::Always` is the default. `HoverOrCaret` MUST apply
the underline to a complete semantic link while the pointer is over an actual
visible glyph fragment or the focused caret is inside its source range. Blank
space within a multiline link's union bounds is not a link hit. Explicit
Underline style and high contrast MUST remain underlined. Pointer leave, scroll,
hide, detach and source/layout replacement MUST retire stale hover state.
Theme accent link ink is opt-in. Link decoration changes MUST preserve native
layout generation, source maps, hit geometry, source bytes and undo history.

Source search highlights are bounded source intervals, painted below selection
without changing source/history. Any source mutation clears stale highlights.
Focus mode is optional: inactive paragraphs use subdued ink while the caret's
paragraph retains normal ink. High contrast disables dimming. Neither feature
introduces a worker, timer, HWND or idle activity.

Native no-wrap prefix measurement establishes a shared maximum marker advance
per list. Ordinary marker/body layouts share a first baseline; wrapped lines and
loose continuation paragraphs retain the body's leading text edge. Nested list
gutters accumulate with the existing bounded container indentation, mirror for
RTL and apply to nested table frames/clips as well as glyphs. Existing standalone
marker rows before first-child list/table/code/HTML/rule blocks remain explicit.
Marker and body layouts count separately toward the 4,096 native layout ceiling.
Owned checked/unchecked task glyphs MUST use the same monochrome symbol face
(`Segoe UI Symbol`) within marker layouts. Their advance, baseline and hit-target
geometry MUST be independent of checkbox state: toggling cannot move the bullet,
body caret or wrapping, including a list with only one task. Generated ordinals
and body text retain the requested fonts; ordinary body Unicode is unaffected.
Generated marker ink has no owned source selection glyphs. Native movement across
boxes preserves logical direction; equal ACPs are separate stops only for explicit
source sites such as padded cells. Home on a text body uses its text edge;
Ctrl+Home retains the exact raw document start.

Only the following native HTML subset is projected. Tag and attribute names use
ASCII case-insensitive matching; opening/closing tag identities must balance.

RichTextEditor font zoom is an owner-thread presentation option in `.25..4`.
It scales inherited/explicit body, code and heading font sizes, clamping the
effective configured sizes to `1..512` DIP. It preserves source, selection,
history and focus and is allowed in read-only views. Ctrl+wheel and Ctrl+Plus,
Minus and zero adjust/reset the same option. Non-finite/out-of-range public
requests refuse atomically. Optional `sourceMonospace` uses the configured code
family/size in Source view; false retains the existing body-font presentation.
`tabStopDip` is zero for the SDK default or `1..4096` DIP, independent of the
number of spaces inserted by source Tab commands. All typography options are
part of retained layout/reuse keys and require owner preparation before drawing.

| Form | Native behavior and validation |
| --- | --- |
| b, strong, i, em, u, s, del, code, span, kbd, ins | Attribute-free nested styles: bold, italic, underline, strike, code font or a transparent span. Keyboard text uses the code font; inserted text is underlined. |
| mark | Attribute-free highlighted text, combinable with the nested styles above. Source delimiters remain hidden with exact source ownership. |
| sup, sub | Attribute-free text scripts use a 0.75-size native font and a shifted baseline, retaining every authored source ACP, selection and undo operation. Ordinary nested inline text styles and links are supported; nested conflicting script tags and embedded images or math keep the complete leaf literal. Color-font runs require a compatible color-glyph render target; if unavailable, that HTML leaf alone falls back to exact source and retries when its target changes. |
| img, img/ | Exactly one quoted src, optional quoted alt/title, and no other attributes. The complete tag owns one atomic image alternative and uses the ordinary bounded image resource pipeline. |
| hr, hr/ | One standalone attribute-free rule in its HTML block, with only surrounding whitespace. It uses native thematic-break geometry, accessibility and source editing. Inline or attributed forms remain literal. |
| a | Exactly one quoted href and optional quoted title, with no other attributes. Destination/title references are decoded; the resolved label is an inert underlined link. |
| br, br/, br / | Attribute-free void break, mapped atomically from its complete raw tag to one native LF. A closing br tag or another self-closing style tag is unsupported. |
| p, div, h1-h6, pre | At most one outer wrapper in an HTML block, outside which only whitespace is permitted. `p` and `div` optionally accept one quoted `align` attribute with `left`, `center`, or `right` (case-insensitive), interpreted as physical paragraph alignment in either reading direction. Paragraph/heading/code presentation uses existing native styles; nested/sibling block wrappers and other attributes are unsupported. |
| details, summary | A complete HTML block with one outer `details`, optional bare `open`, and one nonempty first-child `summary` is a bounded disclosure. The summary accepts native inline text styles but not links or images; the body accepts the ordinary inline native HTML subset and no nested block wrappers or disclosures. A summary click, Ctrl+Tab then Space, or UIA ExpandCollapse changes editor-owned view state without changing source, selection, revision, or history. Closed bodies remain in exact source and Source view. Override state is bounded to 4,096 block identities, follows surviving incremental identities, and is retired on full source replacement. Malformed, empty-summary, unknown-attribute and other unsupported forms retain the complete literal leaf. |
| `<!-- ... -->` | Complete comments are hidden; their bodies are never scanned as tags, Markdown or entities. Their exact source remains available in Source view and raw ACPs. |

RichMarkdown additionally recognizes a block quote beginning with a standalone
`[!KIND]` line as a callout. Supported case-insensitive kinds are note, tip,
important, warning, caution, info, todo, success, question, failure, danger, bug,
example and quote. A plain authored title may follow the marker; `+` or `-`
immediately after `]` makes the callout initially expanded or collapsed.
Titles are at most 256 UTF-16 units and the heading line at most 512; unsupported
kinds, formatted titles and malformed headers remain ordinary quotes. The native
view shows an icon, bold title and theme-accent bar, using text ink in high
contrast. Foldable titles use the same mouse, keyboard and UIA disclosure action
and bounded per-view state as HTML details. The original marker, quote prefixes,
body, selection and history remain authoritative. CommonMark/GFM profiles do not
apply this extension.

One leaf permits at most 4,096 tags and 32 simultaneous opening tags. Unknown
tags, declarations, attributes (including class/style/events), malformed,
crossed or unclosed tags, depth/tag overflow, overlapping Markdown/native anchors
and ineligible HTML destinations reject the complete leaf before any tag is
hidden. The resulting leaf contents use literal monospaced presentation after
ordinary outer Markdown container normalization. This is a local literal policy;
the retained backend can still report Semantic. SourceFallback is reserved for
the existing whole-document/native-admission path.

HTML destination eligibility allows http, https and mailto schemes plus empty,
relative and fragment values. Decode references first, then reject ASCII spaces,
C0/C1 controls, quotes, angle brackets, backslashes and other schemes. These values
remain inert metadata: the control resolves no URL, opens no resource and makes
no network request. Image src additionally permits file URLs and excludes mailto;
decoded alternatives remain literal text. Only explicit caller-owned image requests
can load pixels, with the existing local-path admission and default-disabled remote
policy. Other HTML structures, including HTML lists/tables,
remain literal; their Markdown counterparts retain their native behavior.

Valid inline HTML preserves surrounding Markdown inline semantics. HTML-block
contents preserve normalized whitespace/EOLs and treat Markdown spelling as text.
Character references use the pinned syntax decoder and atomic raw token ownership;
decoded markup characters remain text and are never scanned again as tags. This
policy implements native text presentation, without browser DOM/CSS layout or
recovery semantics. Fenced/inline code and image alternatives retain their existing
literal ownership. Empty balanced HTML records a real interior insertion site;
typing changes exact source between the retained tag pairs in one undo transaction.
Semantic roles/styles do not replace raw TSF/UIA ACPs. Link invocation and full
structural commands follow M4e below; semantic style accessibility uses source
Format units and the acknowledged publication under the input contract.

Selection, composition, text-service rectangles and source-range scrolling use
the map's actual fragments, including atomic transformed/object spans. Hidden
syntax receives no glyph rectangle. Source line units partition raw ACPs in
document order, including original EOLs/trivia; cells have their own wrapped line
units. A wrapping atomic object remains one source unit rather than overlapping
intervals. Source caret sites are separate from glyph ownership. The block
recognizer records physically empty and container-only lines with their exact
raw source/EOL range, insertion ACP and current container owner. The native
backend cooperatively merges empty caret boxes with semantic paragraphs/cells;
code-body empty lines reuse their existing SDK geometry. A definitions-only or
empty document still has a native caret, and source EOF has its trailing line.
An empty list marker reuses its marker paragraph instead of adding a second row.
Source line units retain the preceding line's closing syntax/EOL, the empty
physical line's complete original EOL, and one final empty EOF unit. A trailing
caret site must not create an additional unit containing only leftover EOL text.

Empty and padded cells retain native visual site affinity when several sites
share one raw row-end ACP. Pointer placement, logical Left/Right, Home/End,
vertical navigation, caret scrolling and text-service caret bounds use that
affinity. It survives same-source reflow/retirement and unchanged text-service
synchronization; source replacement, explicit selection or mode/profile changes
clear it. It never becomes document text, a virtual ACP or an undo entry.
Noncollapsed source range bounds still contain only owned glyph fragments.
All native marker/body/paragraph/cell/caret layouts share the 4,096-layout ceiling;
overflow selects exact source fallback once for that immutable syntax.

Collapsed visual deletion expands a source caret inside a transformed/object
token to the complete owned token and commits mapped fragments in one source
transaction. Programmatic replacement and TSF retain exact raw ACPs. Generated
separators require the explicit compatible M4e join policy. M4e owns
empty-format cleanup and padded-cell materialization. An empty cell
scaffold has no owned token to delete; an empty physical source line uses raw
Unicode graphemes and whole CRLF for ordinary deletion. Typing and text-service
imports retain exact source replacement semantics. Semantic typing at a padded
site instead materializes the selected cell at its real row-end ACP. Pending geometry uses
exact source graphemes rather than applying v0 grammar to another dialect.

A semantic length/admission rejection records the exact immutable syntax owner,
retires staging and prepares exact source geometry once. It preserves requested
mode, source, selection and history; idle turns do not retry that revision. A
source/profile/opt-in change permits a new request. Allocation/SDK/internal
invariant failures remain failed preparation and must not self-reschedule.
Native/embedded resource, accessibility, gallery and platform qualification stay
explicit in the WIP plan; this opt-in does not close M4 as a whole.

RichTextEditor analysis is opt-in through `SetMarkdownAnalysisProfile`; null
disables it. The default and formatted pixels retain their v0 behavior. Analysis
uses ControlHost turns before geometry, with a shared 4,096-operation parser and semantic-builder budget per turn. The semantic builder may start in the parser completion turn only with its remaining budget.
`GetMarkdownSyntax` never advances preparation and returns only a completed current
source/revision/profile, or null. Source edits invalidate current-result access
immediately, cancel pending analysis and preserve held immutable revisions.
Profile changes preserve source, selection and undo/redo. Hiding, zero extent,
rebinding and resource retirement cancel pending work. No extra worker/HWND/timer
is permitted in either host. Completed pure analysis is independent of geometry.

Document, codecs, semantic source ownership and text services preserve counted NUL.
CF_UNICODETEXT refuses copies containing it without changing source or clipboard;
this clipboard transport limit does not narrow source admission. Physical-input,
capacity/performance and complete extension qualification remain separate.

The bounded base block-tree/identity slice is qualified in the
[M2 evidence packet](../../Measurements/RichText/README.md):
568 independent block-section expectations, all source inputs, native/embedded
behaviors, Unicode edge identities, prefix-work/resource/failure predicates,
57 ordinary x64 scope obligations, six builds and 324 exact raster pairs pass.
Inline/extension/semantic acceptance stays open. ARM64 is compile-only; source
limits and indivisible-work caveats above remain normative.

### Source-exact structural editing (M4e)

`RichTextCommands.h` MUST build owned immutable command plans from current syntax,
snapshot/revision and directional source selection without parsing, advancing
preparation or calling a host. Applying a plan MUST validate those exact source
and selection identities and commit all replacements through the M4b transaction.
Unchanged source bytes and EOLs remain exact. One undo/redo restores source and
selection. Rejection/allocation/capacity failure preserves source, revision,
selection, undo and redo. Identical edits and cell navigation add no history.

The retained Execute/ApplyMarkdownCommand API requires editable semantic Rich
view and current analysis; composition/pending analysis/incompatible targets
return Unavailable. Stale plans throw before mutation. No command synchronously
reparses or prepares native geometry. Changes publish through existing host text
services and may reenter/retire their owner; no continuation may touch a retired
owner. Pure command plans carry no host binding.

List commands continue/exit markers, change selected sibling list kinds,
indent/outdent one level, and join compatible single-paragraph siblings/plain
paragraphs. They MUST preserve unaffected sibling numbering, task spelling,
labels and line endings. Marker digit-width changes move owned continuation/
child indentation. Nested ambiguous tabs, missing lazy prefixes and incompatible
subtrees reject the whole command. Heading levels 0..6 edit ATX/setext markers;
ATX conversion prefixes each affected physical content line, with empty source
sites admitted and task/heading conflicts rejected.

Style commands own Markdown strong/emphasis/strike/code delimiters, preserve
existing nested syntax and safe spelling, protecting literal punctuation from
altered delimiter/block contexts, and split enclosing styles around
partial selections. Code conversion MUST use resolved source-unit values and
escape punctuation when removing code so literal text stays literal. New
cross-line code spans, partial multiline-code removal and image/HTML conversion
are unavailable. Link commands own only label wrappers/metadata; reference
definitions remain untouched. URI/autolink labels MUST retain their
literal Unicode values when retargeted. Unlink MUST escape literal punctuation
as needed so GFM does not recreate the same automatic link or expose a new block.
Validated native anchors preserve quote/case/
attribute trivia when changing href/title. Removing an opening HTML-block anchor
MUST be rejected because it would reinterpret the literal block body. Marker
deltas use tab columns; relocating an owned marker tab across stops is
unavailable. Destinations obey the native inert
URI policy. Task commands edit only state units for existing tasks, invert each
selected task, or insert task markers where permitted by the explicit dialect.
Removing task syntax MUST reject a literal label that would be reinterpreted as
another block marker; it must not silently reformat that label.

Rich Markdown Enter defaults to the existing raw-newline continuation policy.
Opt-in `MarkdownEnterPolicy::NewParagraph` inserts an EOL, the proven container
prefix, a second EOL and the prefix again in an ordinary paragraph. List and
opaque-code continuation retain their existing policies; Shift+Enter remains a
hard break. The request's Enter policy is part of prepared command-state identity.
Source view and pending-analysis fallback retain their existing raw insertion.

GFM table commands create/remove tables, insert/delete rows/columns and edit or
navigate cells. Unaffected cell/escape/alignment/pipe/EOL spelling MUST survive.
Header deletion promotes the first body row while retaining the delimiter row;
last row/column deletion removes the table. Padded cells materialize only the
missing raw delimiters/cells at their real row end; targetCell indexes the exact
syntax to disambiguate equal ACPs. Navigation keeps native visual affinity;
forward navigation from the last cell appends a row. Limits are 64 columns,
256 body rows, 4,096 declared cells and the existing 64 Ki UTF-16 source cap.
New structure uses local EOL, then first document EOL, then CRLF. Cell source
must be single-line Unicode with escaped pipes and no dangling escape.
`SetTableAlignment` changes only the selected column's delimiter colons, retaining
its hyphen and surrounding whitespace spelling. `MoveTableRowUp/Down` exchange
adjacent row contents, including the header, while retaining each physical line's
container prefix and EOL. A sparse promoted header gains the missing empty cells;
a wider one extends the delimiter within the declared table limits.
`MoveTableColumnLeft/Right` exchange adjacent raw cells
and their delimiter alignment in every row; implicit empty cells are materialized
only when needed. Boundary moves and selections outside the owned row or cell
are unavailable. Directional selection follows the moved content. These commands
use the same syntax-local `targetCell`, atomic source transaction and undo path.
An application clipboard hook can pass counted Unicode text to `InsertTable`
with `tableInputFormat` set to TSV or CSV. The first record becomes the header,
short records are padded and a final record terminator does not add a row.
Double-quoted fields allow separators, CR/LF and doubled quotes; malformed quoting
is rejected atomically. Literal cell text is encoded as Markdown, including
numeric character references for spaces and line breaks. Quoted CRLF and CR
normalize to LF in the resulting cell text. The same table limits
apply to input and generated source; preparation keys the format and exact text.
Ordinary paste is unchanged unless the application invokes this command.
Alt+Up/Down moves the selected physical line within its paragraph, or a whole
adjacent sibling list item including nested contents, as one source transaction.
Source mode moves physical lines directly. Selection follows its original text;
mixed ownership, list boundaries, active source sessions and composition refuse
the action. Original list markers are not renumbered automatically.
`RenumberList` explicitly numbers the current ordered sibling list from its
first marker or the requested `listStart`, preserving punctuation and adjusting
owned continuation indentation when marker width changes. It rejects overflow
or ambiguous indentation atomically and is one undoable transaction.
Tasks default to `EditableOnly`. An application may opt into
`AlwaysInteractive` to permit pointer, focused Space and UIA Toggle actions on
an enabled read-only editor. Each action changes only the exact task marker and
records one undo transaction; other source edits remain read-only. Undo becomes
available under the normal editable policy. Composition, stale publication and
disabled controls still reject actions. Policy changes retire old task peers.
`VisitSemanticWords` enumerates complete prose words intersecting a requested
display interval with borrowed text and source fragments, using the existing
Unicode word/grapheme policy. It excludes generated text, code, literal HTML
blocks, metadata, URLs and native objects; supported inline HTML styling keeps
its visible prose. `CountWords` applies the same policy to the full immutable
semantic document. `GetVisibleSemanticDisplayRange` returns a conservative
current-layout envelope or no value while publication is pending. Consumers
must validate document identity before publishing decorations or suggestions;
existing context-menu and decoration callbacks own the UI. The interval bounds
word traversal; exclusion preparation currently scans document elements, runs
and spans and must not be called from Paint. No dictionary or reading-rate policy
is supplied by the library.
Rendered export MUST operate on one immutable semantic publication. Full or
half-open display-range export produces owned Unicode text and inert escaped
HTML, with readable native-object alternatives and no document-authored active
HTML or external resource loads. UTF-16 scalar and CRLF boundaries are required;
each output is bounded to 4 Mi UTF-16 units. Prepared-page drawing MUST clip a
vertical slice of the current retained layout at the exact prepared viewport
width, use caller-supplied target-compatible brushes, avoid C++ heap allocation,
and preserve source, selection, scroll and history. The caller owns the D2D
target, page iteration and print/PDF/image transport; stale geometry fails.
The syntax convenience entry builds a default semantic publication synchronously
outside Paint, and retains the same bounded export and failure contract.
HTML heading IDs MUST use the published collision-resolved heading slugs. Local
fragment links without a target in the selected export, and unresolved wiki
targets, MUST remain inert text. Display line breaks outside preformatted blocks
MUST be emitted as HTML line-break elements.
When a display selection intersects only part of a generated list marker,
HTML uses a flat preformatted block containing the selected rendered text, so
the browser preserves whitespace and cannot add a second marker; this fallback drops rich structure for
that selection and still replaces native objects with their text alternatives.

`autoParagraphDirection` is an opt-in semantic layout policy. Each paragraph
uses the first Unicode 17 strong L, R or AL scalar outside nested bidi isolates.
the pinned DerivedBidiClass defaults and explicit overrides apply to unassigned
scalars and noncharacters as well as assigned characters.
When no strong scalar exists it inherits the host direction. List marker and
indent placement follow the paragraph. Table cell text resolves independently,
while physical table column order remains governed by the host direction.
Changing this policy retires geometry and accessibility publication without
changing source, history or in-flight image ownership. The direction policy is
part of the layout and native-resource reuse identity.

Semantic visual deletion MUST remove empty owned Markdown/validated inline
HTML wrappers only when their complete body is deleted, recursively. Block HTML
wrappers remain outside this cleanup. Code-span body coverage uses normalized
display units mapped to their exact source, including the single CommonMark
padding unit removed at each edge and CR, LF, CRLF or container prefixes; an
all-space code span retains its spaces. Raw programmatic/TSF replacement MUST NOT
apply this semantic cleanup policy. An explicit clipboard Cut MUST first publish
the exact selected source, then apply the same semantic deletion when the owner
and selection are still current; generic text-service clients retain their raw
replacement behavior. Unsupported mixed opaque targets reject
atomically. These operations do not provide general Markdown reformatting,
structured UIA publication, extension rendering or consumer qualification.
Semantic visible-range queries include only intersecting text or caret lines;
table frame padding alone MUST NOT make offscreen cell source or display text
visible. Intersecting native objects retain their complete source ownership.
Semantic paint culling MUST include prepared glyph overhang in addition to the
logical frame; this paint-only bound does not enlarge hit or visible-text ranges.

### RichMarkdown metadata envelopes (M5a)

Status: source-envelope and bounded single-document YAML/TOML source-validity
validation are implemented. Applications own property schemas and structured
editing. Physical input, native ARM64 and broader M5 qualification remain separate.

Only RichMarkdown MUST recognize an exact `---` (YAML 1.2.2) or `+++` (TOML
1.0.0) first physical line, optionally preceded by one document BOM. No leading
blank, indentation, trailing whitespace, container prefix or later thematic
break qualifies. CommonMark0312/Gfm029 MUST retain their existing block grammar.
YAML closes with an exact `---` or `...` line; TOML with an exact `+++` line.
Fence recognition precedes parsing the metadata language, including invalid
or incomplete quoted values. No JSON front matter is inferred.

The `FrontMatter` leaf MUST retain the exact whole source envelope, a separate
fence-excluding content range, language and closed state. Missing close consumes
the source through EOF and reports `block.closed == false`; it MUST NOT discard
text or fabricate a close. Metadata content MUST NOT define Markdown references,
headings, lists or inlines in the following document. Unknown fields, comments,
spelling, order, BOM and original EOLs MUST survive every untouched source range.

A closed metadata envelope with no validation diagnostic presents a collapsed,
localized Properties disclosure by default. Expansion shows the source-owned
fence-excluding content as dimmed literal text; high contrast uses ordinary text.
Mouse, keyboard and UIA use the same bounded view-state contract as HTML details;
F2 enters the exact raw envelope and Escape exits source presentation. No property
schema, value conversion or writeback is inferred. Invalid, over-capacity and
unclosed metadata remains fully literal, including fences, with its diagnostic.
Source ACPs/history remain authoritative. Markdown structural style,
heading, list, quote and table commands MUST reject metadata targets atomically; ordinary
source replacement/undo/redo and source-view selection remain available.
Envelope recognition alone MUST NOT be presented as YAML/TOML validity or full
extension conformance. Closed TOML content is checked against TOML 1.0.0 without changing source; invalid content and nesting beyond 128 values or dotted-key segments, or more than 1,024 total key segments, have distinct source-owned diagnostics. Missing fences take precedence. Closed YAML content is checked using the YAML 1.2.2 core schema; syntax, duplicate mapping keys, unresolved aliases and resource-capacity refusals are distinct diagnostics. The pinned [TOML](../../ThirdParty/Markdown/corpora/toml-test-96fd088/README.md) and [YAML](../../ThirdParty/Markdown/corpora/yaml-test-suite-6ad3d2c/README.md) validity corpora and [native test](../../Tests/Controls/Controls.Tests.RichMarkdown.cpp) exercise 717 TOML and 382 single-document YAML cases, with 20 multi-document YAML streams explicitly excluded and two duplicate-key outcomes checked as diagnostics. The [allocation-failure sweeps](../../Tests/Embedded/Embedded.Tests.RichTextResources.h) cover observed staging failure and retry; capacity refusals remain distinct from invalid syntax. These cases validate source admission and diagnostics, not decoded values or complete YAML/TOML value conformance. See the [source-bound evidence report](../../Measurements/WipDelivery_2026-11-02/README.md) and [shared qualification ledger](../Plans/Done/ConsumerUiQualification_2026-10-11.md) for execution and remaining platform status.
An unclosed envelope MUST publish an `UnclosedFrontMatter` diagnostic with the
exact opening marker range and revision-local owner node. Semantic UIA HelpText
MUST explain the missing closing fence. Source edits retire that provider; a
closed successor MUST have no stale missing-fence issue. This envelope diagnostic
does not validate the YAML/TOML payload.

### Native Mermaid flowchart profile

Decoded label trimming MUST clamp style spans to the final UTF-16 label and discard empty spans, including the
implicit-close edge-label path. Parser publication, CPU layout and native painting share model-only admission.
An inadmissible parsed model retains that fence as source; factory/typography/measurement errors retain their
own service-level admission behavior. Do not turn unrelated view-option errors into per-diagram fallback.

ER/class endpoints with cardinalities use distinct attachment slots on each node side, in stable relationship
declaration order. Node cross extents reserve sufficient space for the slots and measured endpoint labels before
rank placement. Paths, arrow/diamond endpoints, cardinalities and any original-order route comparison use the
same attachment coordinates. Collision/extent checks remain fail-closed; supported ordinary fan-in/fan-out must
not fall back merely because every relationship previously used one centre anchor.

The owner's extension engine choice is C++ only. `DxUiNativeFlowchart1` is the
named limited grammar below. Its bounded CPU parser, geometric layout, native
painting, F2 source-object editing and semantic UIA diagram publication are
implemented. An individual object is rendered only after its geometry and native
resources publish successfully; unsupported, malformed or over-capacity objects
retain exact source and their diagnostic. This implementation status does not
establish full Mermaid compatibility, physical accessibility or consumer
qualification. Other bounded diagram families have their own documented profiles.

The body starts with `flowchart` or `graph`, horizontal whitespace and one exact
direction: `TB`/`TD`, `BT`, `LR` or `RL`. Statements end at a physical CR/LF/CRLF
or a semicolon outside quoted labels and shapes. Horizontal whitespace is space
or tab. Ordinary `%%` comments outside labels run to the line end. Diagram-local
YAML, `%%{...}%%` configuration, directives and other families are unsupported.

An identifier is `[A-Za-z_][A-Za-z0-9_]*` with exact case-sensitive identity.
`end`, `subgraph`, `direction`, `style`, `class`, `classDef`, `click`,
`linkStyle` and `note` are reserved except for the bounded subgraph statements below. A node is an identifier alone or followed by `[label]`
(rectangle), `(label)` (rounded), `((label))` (circle), `(((label)))` (double circle), `[(label)]` (cylinder), `([label])` (stadium
capsule), `{label}` (diamond), `{{label}}` (hexagon), `[[label]]` (subroutine),
`>label]` (asymmetric), `[/label/]` and `[\label\]` (parallelograms),
`[/label\]` and `[\label/]` (trapezoids), or `(-label-)` (ellipse). Other expanded shape syntax is unsupported. An explicit declaration replaces the
node's previous label/shape; a bare endpoint preserves it. Node order is first
appearance order and edge order is source order, including cycles/self-loops.

An edge chain is `endpoint-group (edge endpoint-group)*`, where an
`endpoint-group` is one node or nodes joined by `&`. Edges include solid `-->` / `---`,
dotted `-.->` / `-.-`, thick `==>` / `===`, and invisible `~~~`; visible edges may
carry `|label|` or bounded plain inline text (`-- text -->`, `-- text ---`,
`-. text .->`, `-. text .-`, `== text ==>`, `== text ===`). Each edge expands the Cartesian product of its
left and right endpoint groups in stable source order; a chain continues from
the preceding right-hand group. Quotes and pipe characters inside inline edge text
remain unsupported rather than being displayed literally. Other expanded, reversed and named edge syntax
is unsupported. In particular `A---oB`/`A---xB` MUST NOT be silently
treated as edges to IDs `oB`/`xB`; use whitespace before such IDs for a plain line.
Quoted labels allow Unicode scalar text and `\"`, `\\`, `\n` escapes. Unquoted
labels keep backslashes literally, trim surrounding horizontal whitespace and must quote delimiter/semicolon
text. In flowchart node and pipe-edge labels, Mermaid `#name;` and decimal
`#number;` references use the pinned Markdown character-reference decoder;
`<br>`, `<br/>` and `<br />` insert line breaks. Node, pipe-edge and inline-edge
labels also accept properly nested, attribute-free `<b>`, `<strong>`, `<i>`,
`<em>`, `<u>`, `<s>` and `<span>` pairs, with at most eight nested tags and 32
styled ranges; DirectWrite applies styles before measurement. Unsupported tags,
attributes, unbalanced markup, references and decoded controls other than
tab/newline retain exact source. Literal
ampersands are label text; Markdown labels, icon syntax and inline math labels
remain unsupported; they MUST NOT silently acquire a different label meaning.
Hash characters without a well-formed `#name;`/`#number;` reference, plain
comparison characters and ordinary text containing `fa:` are literal; actual
`fa:fa-` / variant-prefix icons and HTML-like tags remain unsupported.
Visible solid, dotted and thick links admit up to 16 minimum ranks using Mermaid's
extra dash, dot or equals syntax, including the right side of an inline label.
The DAG ranking honors each forward constraint; a link above the bound or an
accumulated rank outside the 256-rank layout limit fails without partial geometry.
Back edges and self-loops do not constrain the DAG ranks.

Admission limits are 16,384 source UTF-16 units, 256 nodes, 512 expanded edges, 1,024
nonempty statements including the header, 1,024 decoded label units and 128 ID
units. Check each bound before adding generated data. Capacity, unsupported and
malformed outcomes retain the complete body with a source-ranged diagnostic and
expose no partially prepared graph. Failure of an allocation retires staging,
preserves the prior immutable publication and permits explicit retry.

The private CPU parser MUST coalesce identical source/revision requests, reject
invalid source admission without replacing pending work, yield during scanning,
support cancellation and reject stale publication at its future host adapter.
One statement parse is an indivisible bounded operation; event budgets are not
wall-clock guarantees. It owns no worker, HWND, graphics device or timer.

The internal layout service takes one already measured label size per node and
edge. It MUST NOT create SDK layouts, retain source/device resources in its
published geometry, or allocate while advancing. Stable first-seen DFS removes
gray back edges and self-loops from ranking; stable Kahn traversal assigns
longest-path ranks to the remaining DAG. Nodes retain first-seen order in each
rank. Rectangles/rounded rectangles pad labels, circles contain the padded label
diagonal, diamonds contain twice the padded width/height, hexagons double padded width to contain the label in their straight middle band, subroutines reserve room for inset rules, and stadium capsules
keep the complete measured label inside their straight middle band. Their centerline
ports lie on the capsule boundary in all four flow directions. Direction changes
flow without rotating text. An unlabeled forward edge between adjacent ranks
without parallel edges, groups, notes or cardinality uses an orthogonal elbow within the empty
inter-rank corridor, with a straight centerline when its endpoints align. A
complete matching of equal-width nodes in two adjacent ranks also orders targets
by their source when the source nodes precede the targets in model order. Model
identities and source order remain intact. Other edges use exterior lanes sized for their own labels and minimum
separation; unlabeled lanes may be shared only when their inclusive rank intervals
do not intersect. Labeled lanes remain exclusive. When two ranks contain exactly one
member of each non-nested group, columns align in group order and reserve the
largest node or title width, preserving model identity while avoiding unrelated
group overlap. A two-rank single group with one member per rank and one
ungrouped sibling uses separate member and sibling columns. Other group
configurations retain collision detection.
For ungrouped flowcharts without notes, two stable adjacent-rank barycenter
sweeps may reorder nodes within their ranks without changing model identities.
A candidate is used only when both adjacent-rank endpoint inversions and proper
interior crossings between orthogonal route segments strictly decrease compared
with source order; otherwise source-order positions and routes are restored.
Ties are stable. Groups and notes retain their collision-checked placement, and
long edges participate in routing but not barycenter scoring. Each scoring,
sorting and route-comparison event handles one bounded node or edge.
Endpoint corridors remain outside full rank rows, including for skipped-rank edges,
cycles, self-loops and parallel edges. Crossings are permitted; optimal routing
and upstream Mermaid coordinate compatibility are not claimed.

Native node and edge labels wrap with DirectWrite at 12.5 times the selected font
size (225 DIP at 18 DIP), preserving explicit line breaks and complete Unicode
text. Ink and multiline height participate in geometry and capacity checks. This
is the default. `diagramLabelWidthDip` optionally sets a 1–4096 DIP width for
all native diagram labels in the editor; zero retains the font-relative default.
Changing it invalidates label geometry and preserves source, selection and history.

All shapes, labels, routes and arrowheads MUST fit the complete diagram bounds,
with a 65,536-DIP extent limit per axis. Node/aggregate extent rejection publishes
an explicit capacity issue with no partial geometry. Invalid/nonfinite/negative
measurements and failed admission preserve pending work and prior publication.
Successful replacement and cancellation retire only staging. Cooperative events
handle a node, edge, DFS transition or rank; stable Kahn selection may inspect up
to 256 nodes. Published geometry is independent of source and label SDK objects;
the future owner adapter must budget every actual label-shaping SDK call and
reject stale results before painting or UIA publication. CPU geometry alone does
not change the editor's exact source fallback or establish diagram rendering.

Native diagram paint preparation MUST consume the owner's common SDK-layout
allowance for every actual DirectWrite label creation, including empty node
labels; empty edge labels need no layout. Formats, label ink/logical extents,
CPU geometry and bounded diamond/arrow paths are staged before drawing. Label
metrics retain Unicode/bidi/multiline ink and whitespace, without rotating text
or clamping excessive extents into a renderable diagram. Plain labels inherit
the requested host font, size, locale and reading direction. Paragraph width,
code font and paragraph alignment do not control intrinsic diagram geometry.

Prepared label/path state MUST retain factories with explicit COM ownership,
with no owned target/device/host/worker. Coalescing checks the graph identity,
factories, relevant typography and separate typography/resource epochs; epoch
pairs MUST NOT be reduced to a sum/xor that can alias. Invalid admission leaves
pending work intact. Failed preparation discards staging and keeps the previous
publication; callers may explicitly retry. Drawing is raster preparation on a
compatible caller-owned target, with supplied brushes and BeginDraw/EndDraw. It
MUST restore the caller transform and create no C++/SDK resources. Edge labels
paint last with a two-DIP padded opaque backing supplied by the caller, so crossing
routes cannot obscure their ink. Padding fits within the geometry's eight-DIP
outer margins. The fill brush MUST be opaque. CPU geometry
remains source-independent; labels and device-independent factory paths may
survive replacement borrowed devices with the same factory. Final process/GPU
resource, device-loss and host object qualification remain separate requirements.

The bounded math parser exposes a stable typed issue for the named malformed and unsupported target examples while retaining its source-preserving human-readable fallback. Its target corpus checks the complete ordered AST for M01–M20, including matrix dimensions and explicit style/command payloads; equivalent adjacent ordinary glyphs may be stored as one text node.

Native math preparation uses `LayoutOptions::mathFontFamily` (Cambria Math by
default) and the layout's locale. A retained editor's empty
`RichTextEditorAppearance::mathFontFamily` retains that default; a nonempty value
selects the mathematical family independently of body/code fonts. Names use the
same valid-UTF-16, no-NUL, 255-unit admission as other font families. Fonts without
usable MATH data retain deterministic em/stroke fallbacks. The selected family's
MATH constants are read once per semantic geometry generation and shared across
formulas. Text formats are cached within that owner-thread staging generation by
exact size, weight and italic style, at most 4,096 entries; no format is recreated
for another leaf with that key. Paint never queries or mutates the cache.
A width-only reflow may reuse finalized DirectWrite math layouts while rebuilding
placement geometry, provided the semantic document, factory, render target,
resource epochs, image bindings and all other layout options match. Changed
mathematical family or locale also retires incompatible math. Clearing the owner
releases its resources; no global factory/font cache extends their lifetime.

The shared private layout-options comparison retains v0's intentional omission
of math family, semantic presentation and code-copy affordance. Semantic compatibility
includes every option. Math leaves and diagram labels consume both their existing
layout budget and a block step; bitmap tiles consume a block step and their pixel
byte budget. These counts bound work units, not elapsed time. `DebugGetState`
reports the next pending stage across Markdown, semantics, image resources,
geometry, code ink, source ink and source guides. `preparationPending` includes
all these cooperative stages; cancellation retires them. Overlay fallback dirtiness
remains separately reported and does not withhold an otherwise ready frame.
`lastAdvanceUploadBytes` reports the last geometry advance's upload work.

The pinned Mermaid 11.4.1 inventory and all 105 original flowchart examples MUST
remain accounted for. Independent subset expectations distinguish 41 admitted
graphs from 64 unsupported definitions and record each issue and exact UTF-16 diagnostic range, including the documented stadium and
the original endpoint-group fan-out examples.
The family matrix separately identifies the limited flowchart, state, class and
sequence, pie, ER, Gantt and mind map profiles, the example collection, and 13 unsupported diagram families. Generated
first-example fixtures specify the exact expected outcome and diagnostic for all
20 diagram-family documents: the state example prepares five nodes and six
transitions; the class example prepares six nodes and four relationships, and
the sequence example prepares two participants and three messages, and the pie
example prepares three slices, and the ER example prepares four entities and three
relationships. The Gantt reference example retains source because it uses task
identifiers and dependencies; the mind map example retains source because it uses an icon. The remaining 13 examples are unsupported-family fallbacks. These cases measure
local subset coverage, not complete Mermaid family conformance. Original reference
bytes and MIT notice remain under `ThirdParty/Markdown/mermaid-11.4.1`.

### Source-backed native diagram objects

RichMarkdown recognizes case-sensitive `mermaid` fenced code as a native diagram
candidate. Trailing horizontal whitespace is accepted; fence options receive an
unsupported diagnostic. Other language words and the pinned CommonMark/GFM
profiles retain ordinary code interpretation. Unclosed, malformed, unsupported,
empty and over-capacity candidates retain code presentation with an accessible
explanation and an exact selectable source diagnostic. Diagnostics MUST contain
whole UTF-16 scalars and original CRLF pairs.

Semantic preparation MUST advance graph parsing and alternative construction
cooperatively. A document processes at most 128 diagram candidates and 32,768 combined native
records across math AST nodes, graph nodes/edges/groups and image placements;
the candidate bound is separate
from the shared admitted-object limit. Prepared graphs use one atomic
object display character owning the complete original fence. Optional document
state retains the CPU model and code-body provenance, including removed container
prefixes, virtual indentation and normalized EOLs. Source spelling and source
text-store offsets remain unchanged. Alternative names include direction, node
identities/labels and connections. `SemanticDocument::Diagnostics()` references
original source and the owning semantic element; `Syntax()` remains immutable.

Native geometry MUST consume the same owner SDK allowance for graph labels and
the positioning paragraph. A no-op DirectWrite inline object reserves intrinsic
metrics and supports hit testing/selection; native drawing uses the known owner
target. Carets around a graphic remain text-sized. Prepared geometry and UIA
publication must match the same source, binding, typography and resource request.
The diagram child exposes Image semantics, a meaningful alternative and an exact
raw fence range. Source changes and presentation withdrawal retire old children.
F2 and guarded UIA Invoke enter an acknowledged code/HTML/metadata or math object's
exact source in place. Esc, F2 or Ctrl+Enter leaves that presentation. Enabled
read-only owners permit source inspection; edits still require an editable owner.
The editor accepts Ctrl+Insert for copy, Shift+Insert for paste and Shift+Delete
for cut. Ctrl+Shift+V inserts the clipboard's Unicode text without optional
HTML, file-reference or URL conversion, subject to ordinary edit and composition guards.
F2 also opens a Markdown link or image's exact inline envelope. Its UIA
ExpandCollapse action enters and exits that presentation; a link's Invoke retains
its navigation action. Native HTML links and images do not use this inline action.
Hidden Markdown reference definitions may be opened by F2 at their source caret
or by `BeginReferenceDefinitionSourceEdit` with the acknowledged syntax and its
revision-local definition node. The latter rejects stale syntax, non-definition
nodes and active composition; read-only inspection remains available. Their raw
definition becomes a source-editing block until exit, with ordinary undo and
source-session rebasing while incomplete.
Neither presentation transition edits source/history or acquires OS focus.
Transitions MUST cancel the editor's old pointer gesture and release its capture
before publishing the new presentation, with lifetime/binding guards after
native capture release.
F2 MUST accept both source caret edges of an object. At a shared boundary,
another object's leading envelope takes priority over the preceding trailing
fallback. The previous object's trailing entry remains usable before ordinary
text and at raw EOF. Entry/exit may coalesce before a preparation turn.
An active native/embedded composition MUST refuse entry and exit. Old semantic
providers retire immediately on either transition, including at equal source
revision. Pending or stale fragments MUST NOT act on another presentation.
The pure `SemanticBuilder::Begin(syntax, sourceBlock)` overload prepares one
revision-local fenced/indented-code, HTML-block or front-matter leaf as exact
source. `SemanticDocument::SourceBlock()` identifies that requested syntax index;
`MarkdownNoNode` restores ordinary semantics. The caller MUST take the index
from the supplied immutable syntax. Other/container targets are invalid.
Code descendants inside an unreferenced/duplicate footnote declaration are
invalid targets: their declaration is already presented as one literal leaf.
The scoped leaf MUST retain its complete raw envelope, prefixes and original
EOLs in linear source/display spans, including a closing fence without final
EOL. It MUST bypass value decoding/native diagram parsing and avoid duplicating
generated list/footnote markers. Surrounding leaves keep their ordinary semantic
rendering. A source-block request is part of coalescing/publication identity;
invalid admission MUST preserve existing publication and pending work. No SDK
resource belongs to this pure request.

The retained editor uses `MarkdownParser::BeginSourceRegion(snapshot, revision,
region)` while a source scope is active. A nonzero session and scalar/CRLF-safe
ordered range are admitted before replacing pending work. The synthetic
`SourceEditor` leaf owns complete physical lines, including their first-line
prefixes, and remains opaque even when fences, references or metadata are
temporarily invalid. Empty documents/trailing insertion lines retain a zero-span
leaf and real native caret geometry. `SourceRegion()` reports the effective
physical-line envelope; `SourceRegionNode()` is a final dense node index. A
hinted syntax is an explicitly noncanonical editing presentation. Ordinary
`Begin` overloads MUST remove that hint even at unchanged source/revision, and
MUST preserve the pinned CommonMark/GFM grammar. The semantic builder's default
request automatically selects the hinted leaf; conflicting explicit targets
are invalid. Session identity participates in coalescing and synthetic node
identity; ordinary interior edits preserve that node within its session.

All source edits, Unicode character input, previews, cancellation, undo and redo
continue through the one authoritative Document and full source text store.
Tracked interior changes rebase the editing envelope before callbacks. For block scopes, EOL joins
expand to complete physical lines on preparation. Outside changes or selection,
SetText and explicit mode/profile/rendering replacement retire the local scope.
Raw source input MUST bypass rich list/table/atomic-object deletion commands;
ordinary source grapheme/range deletion and literal CRLF insertion apply.
Leaving presentation MUST preserve committed edits and redo; Esc is not a
second unrecorded rollback. Other paragraphs retain semantic rendering. An inline math, Markdown link or Markdown image scope owns its exact raw interval within the existing paragraph. The inline region preserves neighboring Markdown formatting while its text is invalid or contains physical newlines; it does not widen to a physical paragraph.
Selected native objects MUST retain the resolved selection text/surface pair,
including high contrast. Their opaque node and edge-label backing uses the
selection surface composited over the ordinary opaque object surface; it MUST
NOT repaint selected interiors with the ordinary input fill.
When native geometry capacity selects source fallback, Delete, Backspace and
selected-range deletion MUST operate on visible raw source graphemes/ranges.
Retained CPU object semantics MUST NOT expand those edits to the hidden fence.

An intrinsic diagram that cannot fit the available paragraph width or font/extent
limits causes usable exact source presentation. This rejection is keyed by the
semantic document, width, reading direction, typography/resource epochs and host
binding. Repeated unchanged requests MUST add no parsing/layout work. Width/font/
resource/binding changes permit retry without altering text, selection or history.
Transient memory failures remain retryable and MUST NOT create a capacity latch.

### Bounded Markdown staging and source fallback

`MarkdownSyntax::MaximumNodeRecords` is 32,768 combined block/inline staging
records. Source length admission remains 65,536 UTF-16 units. Temporary shells
count until their compaction pass completes. A table row MUST admit its row plus
all header-width real/padded cells against the remaining global node budget
before allocating that row; separate tables cannot reset the budget. Inline
append and opaque HTML roots share the remaining combined budget. Overflow
throws `length_error`, retires staging and preserves the previous publication.
No excessive/partial tree may be published for later semantic admission to reject.

Each generated table cell MUST consume a separate resumable parser event. No
following physical line or final block compaction may run while a row has pending
cells. Cancellation/source replacement can discard a partially prepared row;
published source/identities remain immutable. Bounds apply before amplification,
not after allocating a product of body rows and header width.

Retained parser-capacity fallback MUST bind the exact source snapshot, document
revision and analysis profile. It prepares usable source geometry while preserving
the requested view, text, directional selection and undo/redo. Current rejection
permits source-layout acknowledgement without an unavailable syntax tree; idle,
flow/typography or resource preparation MUST NOT repeatedly parse that same request.
Edits/reset/undo/redo/profile changes re-evaluate their own request. Transient
`bad_alloc` remains retryable `E_OUTOFMEMORY` and MUST NOT latch capacity fallback.
These limits do not establish larger-document or worst-case latency qualification;
final capacity/performance profiles remain separate M6 work; counted raw NUL
admission follows the exact source contract above.

### RichMarkdown footnote definition grammar

`DxUiFootnotes1` is a separate RichMarkdown extension; CommonMark0312 and Gfm029
MUST retain their pinned grammar. Definition containers, global label binding
and reachable numbering have focused native/embedded coverage. Footer projection
and navigation are in development; final M5 qualification remains open.

A definition starts with `[^label]:` after at most three space columns beyond
the current quote/list prefixes. The nonblank label has at most 999 Unicode
scalars, retains its exact raw spelling and allows escaped brackets/backslashes.
Consume one optional space column after the colon; preserve additional initial
indentation, including partial tabs, for ordinary block recognition. Subsequent
non-lazy body lines consume four columns relative to the outer container. Ordinary
paragraph lazy continuation remains valid; blank lines allow multiple paragraphs.
Empty definitions stay open across blank lines for an indented body, but do not
capture following unindented non-lazy blocks. A new definition interrupts an
existing paragraph/definition at its actual outer level. Nested definitions are
literal body text, and MUST NOT become accidental ordinary link definitions.
Code, opaque HTML and metadata envelopes do not acquire footnote definitions.

The `FootnoteDefinition` container owns the whole raw source, exact marker/label
ranges and parsed block children. Definition order, comments/trivia, indentation,
Unicode, EOL spelling and compatible identity MUST survive untouched edits. During
the initial fallback stage, render the complete definition literally once with
source ACPs; do not independently render its parsed child subtree or add extra
caret rows for its child blank trivia. Structural commands touching this fallback
MUST be unavailable atomically; raw source replacement and undo/redo remain valid.
Resolved definitions use the separate note-aware command rules below.

Global resolution uses the existing Unicode 17 default case-fold and ASCII
whitespace collapse without changing label source. No NFC/NFKC or entity decoding
is applied to identities. Binding/numbering is immutable syntax data; rendering
and backlink navigation are separate semantic/native obligations.

`MarkdownSyntax::Footnotes()` MUST expose unique rows sorted by normalized label.
A row names the first source definition, its reachable ordinal (zero if unused)
and final dense inline indexes in reachable reading order. A
`FootnoteReference` inline owns its complete raw marker. Optional
`FootnoteReferences()` records, sorted by final inline index, hold row indexes and
one-based reachable occurrences; `FootnoteReferenceFor(index)` borrows that
immutable record or returns null for an ordinary inline. Undefined references
use `MarkdownNoNode`, and unvisited/undefined occurrences use zero. Extension
metadata MUST NOT enlarge every ordinary inline node.
Raw table escape spelling MUST participate in the same identity as a definition.
Explicit inline/reference links, images, code and escaped bracket openers take
precedence; nested bracket labels MUST NOT create nested interactive references.

Later duplicate definitions MUST retain their source and publish
`DuplicateFootnoteDefinition` at the exact later label range. Missing references
MUST retain literal source and publish `UndefinedFootnote` at the whole marker.
Diagnostics MUST be ordered by raw source position and name a revision-local
owning block. The first definition wins without renaming or reordering source.

Number all main-content references first in reading order, skipping definitions
at their original positions. Then traverse newly reachable primary definition
bodies in discovery order, each once; references there may queue further bodies.
Cycles MUST terminate, repeated references share a number and get distinct
one-based occurrences, and unused/duplicate bodies MUST NOT affect numbering.
Bindings and diagnostics stay immutable across later edits/cancellation and
cannot be reused solely because a numeric revision matches. No host, SDK, worker
or source editing occurs in this CPU pass.

Reordered footer definitions require display-order caret-site ownership,
complete visual-line indexing and source-exact selection/edit/UIA navigation
before rendered footnote support can close.

The semantic projection MUST append each reachable primary definition once as
a document-root `Footnote` element, in ordinal order, with its original raw
definition envelope and parsed body descendants. Unused/duplicate definitions
remain literal Code leaves in their source positions. Resolved references use
superscript decimal numbers, atomically owning the complete raw marker; missing
references remain literal. The first body descendant carries a generated note
number. An empty definition retains a real source insertion site.
The moved body MUST NOT inherit list markers/gutters or quote indentation from
its original declaration ancestors. Its own nested lists/quotes remain intact.
Original container source envelopes can overlap a moved definition; generic UIA
enclosure MUST consider the selected primary display fragments, so a range
spanning retained container content and a footer encloses the document. Hidden
definition labels/blank source positions belong to the footer's semantic tree.
Explicit `RangeFromChild` retains its requested child identity and raw envelope.

Each occurrence MUST have a named generated backlink with zero source ownership
inside the original definition. `SemanticDocument::NavigationTarget` carries its
separate complete raw reference target; forward links target the definition
content insertion. Generated navigation MUST NOT acquire raw text ownership or
enclose unrelated raw ranges at a shared ACP. Native preparation MUST merge empty
sites by display order and index visual line units as an ordered raw-source
partition; generated backlink-only rows add no raw line units. Reordered note
data MUST remain optional, without enlarging ordinary inline records.

Resolved footnote body selections use the ordinary structural planners with note-aware opening-marker and continuation prefixes. Every staged edit must preserve the definition label and existing continuation ownership; inserted physical lines retain the enclosing note and outer containers. Unresolved literal definitions remain guarded. F8 and Shift+F8 select the next or previous exact source diagnostic, wrapping in source order. Selection requires a current syntax/layout, no composition, and a live unchanged host; it changes neither source/history nor focus. The same source selection is exposed through UIA TextPattern.
`InsertFootnote` accepts an exact raw valid label through `request.text` in
RichMarkdown and requires a collapsed, proven inline insertion site. It rejects
an existing definition or reference with the same normalized identity, including
unresolved references, so insertion cannot retarget existing source. It inserts
the reference at the caret and a top-level empty definition at document end in
one source transaction, using the existing EOL policy and placing the caret at
the new definition. Unclosed terminal fences or metadata, terminal HTML blocks,
unmatched inline brackets and a preceding odd escape run are unavailable.

Retained selection remains one source-contiguous anchor/caret pair. A selection
crossing main text and a footer may highlight disjoint visual fragments. Copy,
deletion, composition replacement and UIA ranges MUST retain that exact raw
interval; display order MUST NOT silently create a multi-fragment source edit.
`NavigateSemanticElement` MUST require current acknowledged geometry/publication,
an enabled binding and no active composition. It selects and scrolls without
editing history or acquiring keyboard focus; read-only navigation is allowed.
UIA exposes internal references/backlinks as Hyperlink + Invoke, dispatching
native mutations to the owner and rejecting embedded foreign-thread mutation.
External destinations remain inert until the application registers a link activation handler; registration and removal retire the prior semantic accessibility publication. Source/presentation/binding retirement MUST
make held navigation providers unavailable before a replacement publication.
Ctrl+click activates internal links; Ctrl+Enter at a source-bearing reference
activates its definition. Ctrl+Tab and Ctrl+Shift+Tab cycle generated backlinks in display order without changing the raw selection; the focused backlink has a visible outline and Ctrl+Enter activates it. Navigation is refused during composition and never transfers keyboard focus.
Resolved footnote references are link actions for native HTML admission. They
MUST NOT overlap a native `<a>` envelope: preserve the entire conflicting leaf
literally, as with nested Markdown/native anchors. The footer's independent
backlinks still target exact raw references. Unresolved literal markers add no
action and do not invalidate an otherwise supported HTML anchor.

## Caption button presentation and nonclient hover

`ButtonAppearance::Caption` and `CaptionClose` retain button invocation and
accessibility semantics with square transparent idle surfaces and immediate hover
feedback. Caption controls use neutral hover; close uses the paired theme roles
`captionCloseFill`, `captionClosePressedFill` and `captionCloseText`. High contrast
retains the system style's fill/text/focus pair.

Native hosts may report `HTMINBUTTON`, `HTMAXBUTTON` and `HTCLOSE` over their
client-rendered controls. `ControlHost::HandleMessage` observes the corresponding
`WM_NCMOUSEMOVE` with screen-to-client/DPI conversion and nonclient leave tracking;
it clears hover for other nonclient regions and on `WM_NCMOUSELEAVE`. These
messages remain unhandled so callers preserve native default processing, including
maximize hover/Snap. Consumers own frame hit geometry and activation policy. They
must prevent duplicate native/client dispatch, retain resizing/dragging, and cancel
a pressed gesture on capture loss.

## Optional subtle Markdown presentation

Unordered task rows use the checkbox as their sole visible marker; ordinary bullet rows keep their bullet, and
ordered tasks retain their source numbering. Checkbox source ranges, editing, native hit testing and accessibility
remain source-owned. Code language badges and physical line numbers use the caller's foreground text brush,
including high contrast, rather than the low-contrast decorative border brush. This changes ink only and adds no
brush creation or layout work during drawing.

`RichText::MarkdownPresentationStyle::Subtle` on `LayoutOptions` and
`RichTextEditorAppearance` selects document decoration independently of parsing.
The default remains `Default`. Changing the style retires prepared geometry and
is part of its cache identity; invalid enum values fail before publication.
Source text, selection, undo history, source view and semantic roles do not change.

Subtle H1/H2 headings have a one-DIP separator below the text, included in element
bounds and backed by at least fourteen DIP of following space. Tables use bounded
natural and minimum nonbreaking column measurements plus twenty DIP of horizontal cell padding.
When natural widths exceed the viewport, columns first retain their widest word or
native object's minimum and share remaining width according to their wrappable
content. Only when the summed minima exceed the viewport are those minima reduced
proportionally, allowing DirectWrite's emergency character breaks. Tables draw
horizontal row rules without vertical cell borders.
Opt-in `tableOverflow` on the editor appearance or pure layout options preserves
those minima even when they exceed the viewport. The retained width includes the
complete table; both reading directions use nonnegative physical coordinates so
the editor's horizontal scroll API can reach every cell. With the option off,
tables retain viewport-bounded emergency wrapping. This view option never rewrites
source alignment markers or column definitions.
RTL placement and explicit cell alignment remain supported. Standalone diagrams
and display-only mathematical paragraphs center through prepared DWrite geometry,
so source hit testing and caret placement agree with drawing. Inline math retains
the surrounding paragraph's alignment. Math edge carets use the neighboring text's height and baseline on the same visual line while preserving the exact object-edge x and wrapped-line affinity. When no ordinary glyph shares that line, preparation caches the enclosing body/code/heading font metrics and aligns the caret to the line baseline; caret queries do no new font work. Quotes accept a shaded background.

The editor resolves neutral decoration from `borderDefault` and shades quotes with
an eight-percent blend of ordinary text ink over the document fill during preparation.
This remains visible when input/card roles coincide. High contrast uses resolved text/surface
colors. Caller-provided drawing brushes remain supported by prepared layouts.
Quote and callout frames reserve six DIP of inner space on both horizontal sides in either presentation style.
All descendants, including list markers, nested containers and tables, use that reduced content width before
native layout. Each container's prepared frame encloses its children with the same inset; narrow viewports
clamp the reservation to retain at least one DIP of text width. Drawing, source carets, selection and hit testing
use these prepared coordinates. Literal Source presentation receives no synthetic quote padding.
Subtle table headers and inline code use the same theme-resolved neutral surface
as quotes and code blocks. Inline chips retain source-derived rectangles during
preparation; painting adds no layout or range mapping. At more than 16,384 chip
rectangles, the generation omits all cosmetic chips while retaining code font,
source, selection and accessible content. Header fill is independent of body rows.
Subtle table columns include ten-DIP horizontal cell insets (clamped to preserve a positive text width in
exceptionally narrow cells). Inline-code spans reserve three DIP at both scalar endpoints using native character
spacing, so wrapping, carets, hit testing and chip backgrounds share the same width. These endpoint insets do not
apply to fenced/indented code contents or literal source mode.

Task markers use the prepared semantic accent brush in the editor; high contrast and dimmed/selected passes
retain the appropriate foreground ink. Ordered task numbers keep ordinary ink. `strikeCompletedTasks` on the
editor appearance and semantic layout options defaults to false. When enabled, the nearest checked list item's
body runs receive presentation-only strikethrough. Nested ordinary or unchecked items keep their own state.
Explicit source strikethrough remains independent. This option invalidates semantic geometry without changing
source, selection, revision, command history or accessibility text. Marker metrics and task interaction remain exact.

Adjacent labeled flowchart/ER/class ranks without groups, notes or parallel links reserve measured label and
cardinality space along the graph's major axis, plus measured cross-axis clearance between sibling boxes so
branch labels have separate room. Bands that mix fan-in and fan-out retain separate exterior label lanes to
avoid coincident midpoints in dense joins. Other simple orthogonal routes use the inter-rank corridor instead of
an exterior detour. Wider labels expand LR/RL separation; taller labels expand TB/BT separation. More complex
routes retain bounded exterior lanes and existing exact-source capacity fallback. Preparation reserves one bounded
rank-gap array and bounded rank-classification flags; painting retains no graph model and creates no layouts or brushes.
HTML `mark` shares the bounded retained inline-surface geometry in either Markdown
presentation. Its theme-resolved neutral fill keeps ordinary text ink; high
contrast outlines the marked range instead. The selected-text pass uses the
resolved selection surface. No source markup is rewritten.
The new style's runtime and consumer qualification is tracked in the
[active plan](../Plans/Done/SubtleMarkdownPresentation_2026-10-27.md); this contract
does not establish completed platform or performance qualification.

## Native in-document mathematics and broader diagrams (RedTilde continuation)

RichMarkdown recognizes `$...$` and `\(...\)` inside ordinary
inline leaves. Single-dollar math is single-line and requires nonspace content edges;
a closing dollar followed by a digit is currency text. Explicit `\(...\)` delimiters
permit edge whitespace and a following digit, while retaining the physical-line bound.
Escaped delimiters, code spans, other fenced/indented code and CommonMark/GFM
profiles retain their existing grammar. A closed `math` or `latex` fenced block
is one display-style native formula over the complete source fence; an unclosed
fence or unsupported expression preserves its exact source with a diagnostic.
Paired `$$` or `\[` / `\]` delimiters on their own logical lines own display
math across intervening blank lines. An unclosed opener preserves the complete
owned source with a diagnostic. Double-dollar text within a sentence is literal.
The syntax snapshot represents these display owners as fenced-code leaves with
the `$` or `[` marker respectively; semantic publication recognizes them as
Math, and source editing addresses the complete leaf. Container termination
still bounds a nested source leaf.
Original source, delimiters, CRLF,
selection and history remain authoritative; one successful formula owns one atomic
native display position and a `SemanticRole::Math` child named with its TeX notation.

`RichMath.h` exposes the data-only `ParseMath` model. Supported notation includes
fractions (`frac`, `dfrac`, `tfrac`), indexed roots, simultaneous scripts, Greek and
operator symbols, named functions, text/operator names (including bold and italic
text commands), roman/bold/italic and
blackboard, script, Fraktur, sans-serif and monospace mathematical alphabets, accents,
scalable paired delimiters (including single and double vertical arrows), matrices,
cases, aligned and gathered rows. Alphabet
mapping covers ASCII letters; blackboard, sans-serif and monospace also map ASCII
digits. Other characters are preserved. Matrix rows use `\\` and cells use `&`; shorter rows
are padded with empty cells to the widest row, within the total-cell limit. User macros, packages, file input, evaluation and full LaTeX
compatibility are unsupported. Rejection retains the complete literal equation with
a source-ranged diagnostic, rather than drawing a misleading partial expression.

The bounded native profile records atom classes, font styles, script style and
operator limit policy in its typed model. Variables are italic by default; numbers,
text groups and named operators are upright. Binary atoms at unary positions use
ordinary spacing. Explicit math atom wrappers, display/text/script declarations,
`limits`/`nolimits`, starred `operatorname`, sized `big`/`Big`/`bigg`/`Bigg` delimiters,
and `tilde`/`widetilde` are admitted. Escaped `\|` produces a double vertical bar
as an ordinary atom and as a scalable or sized delimiter. Paired and sized
delimiters also accept `\lparen`, `\rparen`, `\lt`, `\gt`, `/` and `\backslash`.
Text groups collapse source whitespace and remove grouping braces while retaining escaped literal braces. Matrix rows may
have a trailing separator; gathered rows have one column and cases at most two.
`\boldsymbol` and `\bm` request bold while retaining the current math slant;
`\mathnormal` restores default math weight and per-atom slant inside its argument.
Inside an admitted math envelope, `align`, `align*` and `split` use aligned-row
geometry; `gather`, `gather*`, `equation` and `equation*` use centered one-column
geometry. Equation forms admit one row. These bounded aliases do not add numbering.
`\substack{...}` provides up to 16 centered one-column rows, including inside
operator scripts; a trailing row separator is accepted.
`\not` admits ungrouped single relation glyphs `= < > ≤ ≥ ∈ ∋ ⊂ ⊃ ⊆ ⊇ ∼ ≡ ≈ ∣ ∥`
and substitutes their Unicode negations; other operands retain exact source.
Unsupported commands remain local exact source. This is a named native profile,
not a claim of all KaTeX functions or identical TeX font metrics. The selected mathematical face supplies bounded OpenType MATH italic corrections and precomposed vertical delimiter variants when available; larger delimiters use up to 32 distinct part records and 64 total compatible vertical glyph parts per delimiter, with 512 per formula. Connector overlap follows the [OpenType MATH format](https://learn.microsoft.com/en-us/typography/opentype/spec/math); part origins whose ink metrics do not match their advance within two design units use vector fallback. Missing, malformed or oversized font data also falls back. For `\widehat`, `\widetilde` and `\vec`, bounded horizontal variants or assemblies are used when their metrics fit the reserved accent envelope; missing, malformed or incompatible data retains vector fallback. Validated OpenType Device-table deltas for the supported math constants and italic corrections are applied at the formula ppem and converted from pixels to DIP using the effective DPI. Missing or malformed records fall back to the unadjusted design value; variable-font VariationIndex corrections remain unsupported. A single root-level `\tag{label}` or `\tag*{label}` on a display equation stages the body and plain text label separately, centers the body in its containing box and places the label at its trailing edge; the unstarred form adds parentheses. Inline, repeated within one equation or row, nested, or embedded-math labels are rejected to exact source. Supported display align, alignat, gather and equation environments may instead have one independent tag per row as specified above. The full box is the atomic source hit/selection extent; width changes retain shaped body and label. The bounded size declarations `\tiny` through `\Huge` scope to the enclosing group and use the pinned KaTeX size and script-size mapping while preserving source. The bounded annotation commands `\boxed`, `\cancel`, `\bcancel`, `\xcancel`, `\sout`, `\phase` and `\angl` retain their authored source and stage their frame, strike, phasor or actuarial strokes with the formula geometry; nested operands use the same bounds and no brushes or geometry are created during draw. For direct single-glyph script operands whose actual shaped faces match the retained MATH font file, bounded MathKern corner tables adjust horizontal subscript and superscript placement using the two correction heights and OpenType minimum; device deltas are scaled at each glyph ppem. Missing, malformed, oversized, fallback-font or compound-box data uses zero kern. The source and object ownership remain exact. The at-most-4-MiB font snapshot and bounded glyph-indexed tables are shared per geometry generation.
Font lookup and glyph-query HRESULT failures remain retryable; only a known
unsupported font operation or absent/invalid font data uses the vector fallback.
Native `binom`, `dbinom` and `tbinom` form a two-row parenthesized matrix with
explicit display or text style where requested.
A hash-verified KaTeX 0.16.11 source generates 411 bounded alphabetic one-codepoint
symbols and their atom classes; vertical delimiter commands retain the paired native glyph contract.
In math mode ASCII minus and asterisk shape as U+2212 and U+2217; tilde supplies
spacing, and a run of primes becomes a superscript of the preceding atom. A
second explicit superscript on that atom is rejected with the original source.
Math uses the enclosing run's effective body, code or heading size before the
shared 1.15 native-math scale; explicit heading size overrides take precedence.
A caret beside a formula uses the surrounding text line's DirectWrite geometry.
Native `overset`, `underset` and `stackrel` center bounded annotations above or below their base while retaining its atom class; `stackrel` is relational.
Native `\color` and `\textcolor` preserve math atom spacing while applying scoped author ink. `\colorbox` and `\fcolorbox` admit bounded text-mode contents, including nested color and supported text-style commands. Colors are opaque named CSS colors or three- and six-digit hexadecimal values; unsupported colors, alpha forms and embedded math in text-mode boxes remain exact source with a diagnostic. Colored glyphs, rules, variants, borders and backgrounds are staged in authored order. Their deduplicated brushes belong to the exact raster target and are prepared incrementally; paint does not allocate or create resources. Author colors yield to theme ink for selection, high contrast, translucent or unknown backdrops, and fail contrast checks conservatively. Width-only reflow retains completed mathematical shaping and same-target palettes.

Limits are 16,384 source units, 4,096 staging/syntax nodes, 64 parsed nesting
levels, 64 rows/columns and 1,024 cells per formula including generated padding.
Group rows and script wrappers may add AST levels; direct data-only trees have a
separate maximum depth of 130. Math AST nodes, graph nodes/edges/groups and image
placements share a source-ordered ceiling of 32,768 records and 128 admitted
native objects. Capacity refusals publish no partial formula and retain exact
source. Matrix padding is admitted before allocation; matrix measurement walks
each cell once instead of repeatedly finding it from the start of the row tree.
Preparation shapes at most one mathematical leaf per SDK-layout event. Cached
DirectWrite glyphs and line geometry draw in the owner's target with caller brushes;
no filesystem, worker, font downloads or allocation occurs in drawing. Reflow and
device replacement regenerate prepared geometry without changing source.

Every rejected formula has its own Math element, accessible explanation and exact
source diagnostic; rejection cannot overwrite its enclosing link's title or destination.
A persistent native geometry-capacity failure falls back only the affected formula
or complete Mermaid fence, including its original container prefixes and EOLs. Native preparation collects all independently rejected objects in the bounded geometry pass, then rebuilds the semantic publication once; staged partial geometry is never published.
For an individual math formula, DirectWrite `DWRITE_E_UNSUPPORTEDOPERATION` during
leaf preparation is also a persistent object-local rejection: the operation is not
supported by the selected font type. The formula's accessible explanation uses
cause-neutral wording because the same source-preserving view fallback also covers
capacity and font-operation failures. Other semantic content stays rich. The
presentation rejection key includes immutable syntax, width, direction, host
typography/resource generation, effective DPI, binding and local appearance geometry revision.
Repeated idle preparation does not retry the same rejection. A tracked edit outside an unchanged object retains its refusal when its exact syntax owner and diagram container geometry context remain equivalent; changing the view key or object retries it. DirectWrite `GetLineMetrics` reports a one-line-capacity rejection only when it
returns `E_NOT_SUFFICIENT_BUFFER` and reports more than one required line. Other
failed HRESULTs from leaf layout/metrics and image upload, except the explicit
unsupported-image result above, preserve their original
error channel and leave the request retryable. In particular, `E_OUTOFMEMORY`,
`DWRITE_E_UNEXPECTED`, `D2DERR_RECREATE_TARGET` and DXGI device-loss results must not
be converted into permanent source rejection; they remain eligible for explicit
preparation retry or resource-generation recovery.

A deterministic `std::logic_error` in semantic staging or native rich geometry is
logged once and memoized for the exact source, profile, source-editing region,
view, width, DPI, direction, typography, resource generation, binding and local
appearance key. The editor prepares source presentation without changing the
requested view, source, selection or history; idle turns do not retry the rejected
key. A changed key permits rich preparation again. Memory, device and unknown
HRESULT failures remain retryable and do not use this deterministic fallback.

The native Mermaid engine additionally accepts a bounded `stateDiagram` /
`stateDiagram-v2` profile: direction, simple declarations/quoted aliases,
`[*]` initial/final states, explicit `state Name <<choice>>` diamonds and `<<fork>>` / `<<join>>` bars perpendicular to graph direction, transitions with colon labels and state descriptions. Pseudostate labels are not admitted.
The first nonempty quoted or colon state description replaces the fallback ID;
later descriptions append in declaration order, within the shared label limit.
An explicit description equal to the ID still counts as the first description.
The bounded `classDiagram` profile accepts direction, simple class declarations,
colon members and bounded multiline `class Name { ... }` member blocks, directed/undirected associations and inheritance (`<|--`, `--|>`). Blocks require a standalone closing brace; nested blocks and markup retain exact source with a diagnostic. Member types accept nonempty, non-nested `List~int~`-style generic arguments and display them with angle brackets; malformed or nested forms retain exact source.
One alphabetic `<<annotation>>` per existing class is accepted on a separate line
or inside its block and displayed as `«annotation»` above the class name; repeated,
nonalphabetic or undefined-class annotations retain source.
Class diagrams also accept `note "text"` and `note for Name "text"` with
bounded quoted plain text. Standalone notes occupy their own node; attached notes
use a dotted connection and expose their target and text in the graph alternative.
Simple `namespace Name { ... }` groups, nested at most four levels, use the
shared bounded group layout; identifiers remain globally unique, and group
overlap or unsupported namespace syntax retains source with a diagnostic.
State/class layouts use the deterministic native graph router; members are plain
multiline node labels. Inheritance uses the native triangle arrow. This is not
Mermaid coordinate compatibility. Advanced composite-state syntax, nested class generics,
class compartments, relationship multiplicities, other stereotypes, styling,
interaction directives and other diagram families remain exact fenced source.
Invisible edge labels remain unsupported. Other flowchart grammar and source-provenance rules remain unchanged.

Aligned rows alternate right-aligned left operands and left-aligned right operands at each ampersand pair. Cases use left-aligned expression/condition columns (at most two); ordinary matrices center their columns. Gathered rows have one column. Geometry tests compare the actual cached glyph positions consumed by drawing.

Over-capacity notation conservatively retains the remaining physical inline line (display notation: logical leaf) as literal source with a diagnostic. This prevents nested Markdown interpretation and bounds repeated failed delimiter scans. The consumer inspector uses MathMaximumSourceUnits and clears rather than accepting an oversized value.

The bounded `sequenceDiagram` profile retains participants in first-appearance
order, with plain `participant ID [as label]` and `actor ID [as label]` declarations.
Actors draw a foreground stick figure above their measured alias; participant boxes
and actor labels align above dashed lifelines. Ordered solid `->>` and dotted `-->>`
messages use filled heads; `-)` / `--)` use open heads; `->` / `-->` have no head.
Self messages use a loop. Plain `Note left of ID: text`, `Note right of ID: text`,
and `Note over ID[,ID]: text` frames appear in source order among messages and
cover their referenced lifeline(s). Lowercase `note` is also accepted. Bare
`autonumber` numbers messages from one, excluding notes, without changing authored
labels or source. Custom numbering arguments, activations, loops, grouping,
creation/destruction, directives and other excluded forms retain the entire fence
with a diagnostic. The alternative exposes aliases, note text, derived numbers
and chronological order; its text range owns the exact source fence. Notes share
the existing edge/event budget and staged label resources; drawing allocates no
labels or paths. The shared source, node, edge, label and extent budgets apply.

Directionless `flowchart` and `graph` headers use top-to-bottom placement, matching
the pinned Mermaid 11.4.1 grammar. Explicit TB/TD, BT, LR and RL remain supported.

The bounded `pie` profile accepts an optional plain title and `showData`, followed
by up to 16 unique quoted plain labels and nonnegative decimal values with at most 15
integer digits and two fractional digits. The total must be positive; invalid or
unsupported notation retains the exact fence with a diagnostic. Native sectors
follow source order clockwise and use repeated theme-derived shades with a matching
ordered legend; `showData` includes the original value spelling in the legend.
The accessible alternative exposes the title and all labels and values in order.

The bounded `erDiagram` profile accepts named entities, plain attribute blocks,
solid identifying or dotted non-identifying relationships with plain labels, and
the `||`, `o|`, `|o`, `}|`, `|{`, `}o` and `o{` endpoint multiplicities. Entity
IDs may contain ASCII letters, digits, underscores and hyphens after the first
character. Typed multiplicities use bars, circles and crow's feet in native
geometry and are named in the accessible alternative. Unsupported forms retain
the complete source with a diagnostic; diagram source, node, edge, label and
extent budgets apply.

The bounded `gantt` profile accepts `dateFormat YYYY-MM-DD`, an optional plain
title and sections, and up to 32 tasks of the form `label : YYYY-MM-DD, Nd` or
`label : YYYY-MM-DD, YYYY-MM-DD`. Dates are checked Gregorian dates; durations
are positive whole days and intervals are half-open. A task and the complete
chart may span at most 366 days. Native bars use the shared calendar scale and
show section, task and source date/duration text; the accessible alternative
retains the same data in source order. Task identifiers, dependencies, status
tags, exclusions and other directives retain the complete fence with a diagnostic.

The bounded `mindmap` profile accepts one root and up to 127 descendants, with
space-indented levels through depth eight and consistent indentation for each
level. Plain labels and rectangular, rounded and circular label forms are
supported. Native layout places alternating root branches around the central
node, connects descendants to their parents, and preserves source order in the
accessible alternative. Icons, unsupported shapes, tabs and malformed indentation
retain the complete fence with a diagnostic.

Flowchart node styles accept comma-separated `stroke-width:Npx` (1–3),
`stroke-dasharray:5 5` or `0`, `font-weight:bold` or `normal`,
`font-style:italic` or `normal`, and opaque `fill`, `stroke` and `color` values
in `#RGB` or `#RRGGBB` form. `style ID`, named `classDef` and `class ID name`
or `ID:::name` accept bounded comma-separated identifier/class lists; definitions
may follow their use, and direct style overrides classes, which override
`classDef default`. Up to 64 named definitions and 512 assignments are retained.
`linkStyle N` or `linkStyle default` accepts width, dash and stroke for existing
zero-based edges, with explicit edge properties overriding defaults. Unsupported
properties, colors, unknown targets/classes and widths retain exact source with
a diagnostic.

Author colors use target-bound solid brushes prepared incrementally before
publication and recreated for a replacement render target. The chosen text and
outline must contrast against the chosen fill by at least 4.5:1 and 3:1;
edge strokes must contrast against the theme background by 3:1. Otherwise the
corresponding theme colors are used. High contrast and selected/overlay text
use the caller's theme brushes. A translucent document fill or a differently
shaded quote, table header or marked paragraph also uses theme brushes when the
actual backdrop cannot be established by the prepared color context.
Painting creates no palette resources and does
not change borrowed brush attributes.

The retained editor uses each diagram's effective foreground ink for default
connections, arrowheads, lifelines and shape/note outlines in every presentation
style, including Subtle and high contrast. These are document content rather than
decorative rules; the surrounding heading, table and code chrome keeps its separate
decoration brush. Authored colors still use the bounded palette and contrast rules
above. Explicit snapshot/export drawing keeps its caller-supplied stroke contract.
This brush choice performs no new layout, allocation or resource creation during paint.

In the retained editor, arrow-label and relationship-cardinality backing matches the actual diagram canvas
when the effective foreground reaches 4.5:1 contrast there. Subtle presentation uses its shaded code/diagram
surface rather than the node fill. If contrast is insufficient, preparation chooses the smallest neutral
light/dark adjustment that reaches 4.5:1; custom translucent ink that cannot reach that ratio retains its ink
and uses the strongest available neutral backing. The backing remains opaque so crossing routes never show
through label ink. Notes and labels inside filled groups keep their own container fill. Selection, dimmed,
high-contrast and explicit export/overlay passes retain their caller-resolved surfaces. Contrast decisions,
group membership and brush staging occur during bounded preparation; paint does not reshape, allocate,
create resources or change borrowed brush attributes.

Flowchart, state, class, sequence, pie, ER, Gantt and mind map diagrams accept one `accTitle: text` and one
`accDescr: text` or multiline `accDescr {` / `accDescr{` block closed by a
standalone `}`. Values are plain text, each bounded to 1024 UTF-16 units;
multiline descriptions join trimmed physical lines with LF, retaining empty
lines. Duplicate declarations, unsupported control characters and unterminated
blocks retain the original source with a diagnostic. The diagram UI Automation
name uses the authored title when present; its help text uses the authored
description, or the generated graph summary when only a title is supplied.
Without an authored title the generated graph summary remains the name.

Class diagrams also accept realization relationships `A <|.. B` and `B ..|> A`, rendered as a dotted line with the hollow triangle at the interface.

Class diagrams also accept dotted association `A .. B` without an arrowhead.

Class member blocks and colon members retain source order; a horizontal separator
precedes the first member and each subsequent attribute/method group change.
A member containing `(` starts a method group; other members are attributes.
At most 16 groups per class are admitted. Additional groups preserve the exact
source with a capacity diagnostic. Separators use measured text line positions,
including wrapped labels and bidi layout, and remain within the class box.

Class relationship endpoints may carry quoted cardinalities of at most 32 ASCII
units drawn from digits, space, `*`, `.`, `+`, `-` and `?`, for example
`A "1" --> "0..*" B`. The labels follow their original endpoints when a
relation reverses, are independently measured, painted beside those endpoints
and included in the accessible graph alternative. If a cardinality label would
overlap a node or another label, native geometry fails closed and the editor
retains the original source with its geometry diagnostic.

Flowcharts admit up to 16 `subgraph ID` or `subgraph ID[title]` containers,
nested at most four levels and closed by `end`. Each requires at least one node;
IDs and titles obey the existing bounded identifier and label grammar. Nodes
referenced inside a group belong to that group and its ancestors; sibling group
membership conflicts, group endpoints, local `direction`, empty or unclosed
containers retain exact source with a diagnostic. Group titles and containment
are exposed in the accessible graph alternative. Native geometry measures titles,
paints nested containers and fails closed when unrelated groups, nodes or labels
cannot fit without overlap. This is a limited flowchart subset, not unrestricted
Mermaid subgraph compatibility.

State diagrams admit nested `state ID { ... }` composites and
`state "title" as ID { ... }` composites under the same 16-container and
four-level limits as flowchart groups. Initial and final `[*]` pseudostates are
scoped to the enclosing composite, so separate regions do not share a terminal.
Composite boxes, titles and membership are rendered and included in the
accessible graph alternative. Empty/unclosed composites, transitions to a
composite endpoint, overlapping unrelated boxes and unsupported region syntax
retain source with a diagnostic. This profile does not claim unrestricted
Mermaid state-machine support.

State diagrams admit single-line `note left of ID: text` and `note right of
ID: text`. Notes retain their state and composite membership, use the bounded
plain-label grammar, and render a folded-corner box on the requested physical
side with a dotted attachment. Their target and text are included in the
accessible graph alternative. Multiline notes use `note left of ID` or `note right of ID` followed by
plain lines and a standalone `end note`; trimmed lines are joined with LF,
including empty lines and literal semicolons or percent characters. Text is
limited to 1024 UTF-16 units. Colliding notes, unsupported note forms and
unterminated blocks fail closed to exact source.

Class relationships admit composition (`*--`, `--*`, `*..`, `..*`) and
aggregation (`o--`, `--o`, `o..`, `..o`). The diamond remains attached to the
whole in either spelling; composition paints a filled diamond and aggregation a
hollow one. Dot variants use dotted lines. The accessible graph alternative
describes the part-to-whole relationship and preserves any endpoint cardinalities.

`LayoutOptions::dpi` is the effective device DPI for native math positioning,
defaulting to 96 for standalone callers. It must be finite, greater than zero and
at most 4096. Native and embedded editor hosts supply their current host DPI;
DPI changes invalidate reusable math geometry. Shared immutable MATH bytes
remain reusable, but device adjustments are evaluated again for each math style's
font size and DPI.

Opt-in Rich editor URL paste can turn a selected source label into a link using the current Markdown command planner. It requires a current analysis and an eligible http, https, or mailto destination; unsupported selections and invalid destinations keep ordinary paste. The replacement is one undoable source transaction, and Source mode keeps literal paste.

Structured rich-text clipboard reads may independently request file lists and a CF_HTML fragment alongside Unicode text in one ownership attempt; the legacy file-only transport contract is unchanged. The medium is limited to 266,240 bytes, requires ordered, in-bounds HTML and fragment byte offsets and strict UTF-8, and limits the decoded fragment to 65,536 UTF-16 units. An application may opt into HTML-to-Markdown conversion by supplying a synchronous converter; source, selection, focus binding, mode, and converter identity must remain current before its counted result can be committed as one edit. Failed, invalid, or unsupported conversions fall back to the normal Unicode paste policy; stale conversions are declined, and file lists retain precedence. HTML-only data without a successful converter is not an edit.

During pending Source or semantic layout, logical caret and erase operations use a local source window anchored only at proven Unicode-independent boundaries. Context expands across uninterrupted grapheme and word runs and may reach the document edge; CRLF, regional-indicator parity, Indic conjuncts and emoji ZWJ sequences retain the same boundaries as the full source. The v0 Rich fallback projects complete local physical lines, including preceding line context, through the canonical projection builder; hidden-marker and source-affinity behavior matches a complete projection, and long lines or word runs can extend the window to the document edge.

At the end of a nonempty table cell, file-reference paste can infer the table context when the cell uniquely owns that source position. Row-end positions shared with synthetic padded cells require the retained geometry-site identity; a source-only ambiguous request is declined.

Rich clipboard paste preparation declines malformed or over-budget file lists with false and leaves the caller's replacement, source, selection, and undo history unchanged. Allocation failures remain distinguishable from content refusal.

SemanticDocument exposes its source-ordered heading index with level, display name,
collision-resolved fragment slug and exact source/navigation ranges. Fragment resolution
uses the same index as editor navigation. BuildHeadingTableOfContents generates bounded
Markdown links with escaped labels and percent-encoded UTF-8 fragments without editing
the document; a result exceeding the display-unit limit throws length_error.

RichMarkdown admits a bounded presentation suffix immediately after a standalone
image: `{width=48 height=40 align=center caption="Map overview"}`. Fields are
optional, unique and space-separated; dimensions are integer DIPs from 1 through
4096, alignment is left, center or right, and the explicit plain caption is
1–256 UTF-16 units without control characters, quotes, backslashes or closing braces. The
caption retains Markdown punctuation literally even when inline parsing splits it into formatting or link nodes. A single
dimension preserves intrinsic aspect ratio; two set the authored size before
normal viewport-fit admission. Unknown, duplicate, malformed or non-standalone
suffixes remain ordinary literal source. CommonMark and GFM do not consume the
suffix. The image including its suffix remains one atomic source owner; caption
presentation is generated at its authored source anchor, and F2 edits the whole
owner. Caption display is available to text/UIA and rendered export but is not
independently source-editable. Caption is never inferred from a title.

Native HTML images accept bounded quoted integer `width`/`height` and, on a
standalone image leaf, `align` and explicit `data-caption`. The whole supported
leaf validates before projection; unsupported or non-standalone alignment or
caption stays literal. Caption display is generated at the tag's ending source
anchor. Width/height preserve ratio when only one is authored. Geometry and
caption text are prepared with the existing layout and resource budgets; Draw
performs no new shaping or resource allocation.
