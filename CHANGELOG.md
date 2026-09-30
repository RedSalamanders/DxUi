# Changelog

## Unreleased

- Follow up the review's reported items as decided (plan `ReviewFollowUps_2026-09-29`):
  - Multiline `Grid` cells keep their shaped layouts in 32-way set-associative tables keyed by a library-owned mixed
    hash of the value and layout box, growing (to 16,384) while the values of the current and the previous paint crowd
    a set: a repaint of unchanged cells shapes nothing and a one-row scroll either way shapes only the entering row
    (scrolling up no longer evicts the rows still in view), for any number of distinct visible values. Values whose
    visible text is the same share its omitted-tail layout through a keyed table instead of a scan of every entry. Key
    and display strings reuse power-of-two storage, so dirty frames allocate no more than the direct-mapped cache did.
    A value shapes only what it can show, wrapped or on one line: a 100,000-unit cell lays out a few hundred units
    (243 at `SetLineClamp(1)`, where the old prefix never stopped; 971 at a clamp of 1,000 in a short row, sized by the
    lines that fit), and it is retained by that prefix, so its repaint and hover shape nothing and another value
    starting the same way shares it. Keys round layout boxes to 1/64 DIP, so rows at heights such as compact density's no
    longer miss after a scroll. The omission marker takes the direction of the text it ends, read by character (an
    emoji ending Arabic text no longer sends it right), and trailing lines of other spaces or default-ignorable
    characters add nothing. A `MultilineGridDistinct` benchmark scene gives every cell its own text.
  - Single-line `Grid` cells keep their caption layouts in the same table instead of laying the caption out on every
    paint and every hover, with pixels identical to the previous centred-text draw; a long leading-aligned caption
    shapes only the prefix its cell can show (363 units for a 100,000-unit caption over three paints).
  - Window hosts raise UI Automation focus changes as focus moves inside their focused window, naming the focused
    tree item or grid row as GetFocus does, and the window itself once no control has focus; a window that lost the
    foreground announces nothing, and the window's own activation is left to the system's focus event. An element's
    SetFocus moves logical focus before Win32 focus, and UI Automation's SetFocus no longer holds the accessibility
    mutex while the host runs callbacks and raises events. A window-host element (tree items and grid rows too) whose
    control was removed or replaced at the same path reports `UIA_E_ELEMENTNOTAVAILABLE`, with runtime ids from a
    per-process serial that no later control reuses, and such a republish, a whole-tree `SetRoot` included, raises
    StructureChanged. A child of a collapsed status root reports its own events. A focus callback that removes its
    control leaves no focus behind (a rootless host keeps a live one); a control disabled or removed while focused
    loses focus, published, at the host's next message; a republish never touches a focused control its panel removed.
  - While a menu is open its popups show the arrow cursor and a window of the menu's thread outside them chooses its
    own; closing lets that window choose again (except when the owner's destruction closes it). Closing a described
    menu returns all its memory, even while a client holds its row elements.
  - A disclosure's chevron rotation starts at its first animation tick (so a change made while hidden rotates once
    shown) and a gap in the ticks pauses it; unsupplied alert colors keep distinct, readable default tones outside high
    contrast, and a derived partner is black or white by contrast measured as the pair paints (4.5:1 for any supplied
    fill, translucent ones over the window background included);
    right-to-left tab titles, `NumericStepper` labels and units and the `ColorPicker` hex caption read right to left
    from their start; `NumericStepper::SetStepButtonNames` (default "Increase" / "Decrease") and `ColorPicker::Labels`
    step-button names, caption and swatch widths (bounded to 4,096 DIPs) serve translated captions.
  - Source-breaking renames drop the repeated namespace: `IGridModel`, `IGridDelegate`, `ITreeModel`, `ITreeDelegate`,
    `RunModalLoop`, `IsRenderStageActiveForDebug`, `EmitRenderMutationBlockedForDebug`, and in `DxUi::Typography`
    `Spec`, `GetSpec`, `PerfEmitter`, `GetPerfEmitter`, `SetPerfEmitter`, `EmitPerfCounter`, `FontFamilyCacheEntry`,
    `TextFormatCacheEntry`, `GetMeasurementCacheMutex`, `GetFontFamilyCache`, `GetTextFormatCache`,
    `kFamilyCacheMissMetric` and `kTextFormatCacheMissMetric`. No aliases remain; consumers rename when they move
    their pin. API revision stays 2, as for the earlier modal-loop rename.
- Fix defects found by reviewing the last fifteen days of changes (plan `ReviewFixes_2026-09-29`):
  - Captured drags: the embedded rule resolves the capture through the live tree before touching it (a destroyed
    captured control was read after free) and requires every ancestor to stay visible and enabled. WindowHost now
    cancels a drag whose control or ancestor is disabled or hidden (`OnCaptureLost`) instead of dropping the capture
    silently and leaving the control mid-drag.
  - `Splitter`, `NumericStepper`, `ColorPicker` and `TextField`'s focus bar and button hovers skip a draw when no
    solid brush is available, as the shared helpers do. `Splitter` keeps its separator inside an extent smaller
    than both pane minimums and is exposed to UI Automation as a Thumb with RangeValue.
  - `ColorPicker` keys its gradients by a retained `ID2D1Device` (a recreated device context could reuse the cached
    address and fail every preparation with D2DERR_WRONG_RESOURCE_DOMAIN). The gradients are hue-independent and
    placed by transform, so hue drags and moves create no resources. The field being typed in keeps its text, caret
    and undo history; a canceled component edit is followed; Escape in the hex field cancels; runtime flow changes
    re-arrange the fields; right-to-left flow mirrors the saturation axis the keys already mirrored.
  - `NumericStepper` ends every previewed edit with Commit or Cancel (an unparsable edit reverts with Cancel),
    commits text changed without focus (UI Automation), re-rounds on `SetDecimals`, never sticks on a step finer
    than its decimals, reads full-width and Arabic-Indic digits, and rewrites its field only when the text changed.
  - `Tree` row drags never target the dragged row's own subtree, follow model changes and wheel scrolls, decide the
    drop at the release point, cancel on a second button, `SetModel` or `SetReorderEnabled(false)`, treat id 0 as an
    ordinary row, paint without a per-frame id scan and repaint only when the drop changes. Row `iconText` again
    uses the UI font unless it is a private-use glyph (letter icons had rendered as missing-glyph boxes).
  - Multiline `Grid` cells no longer draw with `D2D1_DRAW_TEXT_OPTIONS_CLIP` on their fractional layout box (it cut
    the last line's descenders and accounted for most of the multiline memory growth: dirty-round private bytes fall
    about 5.5 MB on the MultilineGrid fixture); trailing NEL/VT/FF and blank tail lines add no ellipsis; any
    trimming offers the tooltip; and a scrolled single-line caption offers its full value.
  - Described menus paint Info-row descriptions, render color glyphs, raise UI Automation focus changes as the
    keyboard moves through rows, and a UIA SetFocus on a popup root no longer dismisses the menu. Window-host
    UIA event providers no longer pin a full accessibility snapshot each while a client listens.
  - Right-to-left `Checkbox` and `RadioButton` captions sit beside their indicator. `ProgressBar` rejects non-finite
    values and an empty indeterminate bar requests no frames. `Slider` no longer drops a change as small as its
    smallest step.
  - `performance-paired.ps1` judges same-source controls two-sided (`unstable-control`), fixes `-SkipBuild` and
    annotates findings in CI; `performance.ps1` never re-stamps a stale receipt and keeps one default receipt per
    scenario. API revision stays 2 (additive; private members and one Tree debug field changed).
- Honor reduced motion in `ProgressBar`: an indeterminate bar rests its segment centered, requests no animation
  ticks and paints identically every frame; restoring motion resumes the 2 s sweep. `Tick` re-seeds when the tick
  clock moves backwards and drops whole loops from long gaps, so no elapsed time can corrupt the sweep phase.
  API revision stays 2 (additive diagnostics accessor only).
- Never paint status invisibly: `MakeThemePalette` still copies supplied alert colors, but a zero-alpha alert
  color, including the `ThemeColors` zero default, falls back to `windowBackground` for fills and `text` for
  text. High-contrast themes without alert colors now show toned grid rows, tone badges and throughput limits.
- Add the editor consumer controls `Splitter`, `NumericStepper` and `ColorPicker`: preview/commit/cancel notifications,
  keyboard operation, right-to-left mirroring, disabled and focus-visible states, and the public `HsvFromArgb` /
  `ArgbFromHsv` / `ParseHexColor` / `FormatHexColor` helpers. `WindowHostCursorKind::VerticalResize` maps to the
  vertical resize cursor. Catalog/factory count is 30. API revision stays 2 (additive).
- Paint `Slider` like the Windows volume flyout: a 6 DIP capsule track (fill and remainder the same thickness), a fixed
  20 DIP gray chrome disc, and an accent inner thumb that matches the track at rest and grows to 16 DIP on hover
  (12 DIP pressed). Pointer hit testing stays an unpainted 48 DIP band with a 24 DIP grab radius. Embedded
  `DispatchPointer` synthesizes control double-click (word selection) using the system interval and a 16 DIP slop.

- Release the embedded surface on `SetVisible(false)` and zero-extent `Prepare`: `surfaceBytes` reports 0 while
  hidden or zero-sized, and the next visible sized preparation reallocates exactly one surface with identical pixels.
  `AdvanceAnimation` no longer marks the view dirty unconditionally; every `Tick` that changes visual state
  invalidates (caret blink flips, button/page transitions, tooltip show/hide, grid/tree/menu animation), and
  `ControlHost::RequestAnimation` wakes an embedded view only when animation becomes requested, so paint-time
  requests (indeterminate progress, busy grids) no longer re-dirty the view inside every preparation. Bound the
  per-host solid-brush (256) and configured-text-format (96) caches with a trim at preparation/paint start, report
  `cachedBrushes`/`cachedTextFormats` in `EmbeddedStatistics`, and gate the complex benchmark at zero clean-round and
  64 (Release) / 320 (Debug) dirty per-frame C++ allocations. API revision stays 2 (additive).
- Add `PageIndicator`: a bottom strip of dots for paged surfaces. Fewer than two pages paint nothing and are not
  hittable. Click, Left/Right/Home/End and `SetSelectedIndex` share one selected index; only user input fires
  `SetOnSelected`. Catalog/factory count is 27.
- Deliver API revision 2 through one DxUi.lib: public controls, a 26-control catalog/factory, neutral themes and
  diagnostics, and native plus supplied-device embedded hosting. Foundation is part of the same archive.
- Separate dirty preparation from allocation-free D3D11 composition; support logical capture, DPI, visibility,
  device replacement, shared device pools, and slider preview/commit/cancel notifications.
- Add a public toggle/slider consumer, relocated exact-pin consumption tests, five-theme all-control gallery,
  supplied-device WARP regressions and all 853 reusable inherited runtime cases. Record the 88 exclusions.
- Add revision-checked embedded text snapshots, application-side TSF/clipboard services, and lazy embedded UIA
  attach with synthetic tests. Close library extraction and the first RedXe pin/synthetic adapters (`redxe-adapter`).
  Matched performance and real IME/AT remain RedXe AV gates.

- Make this repository the canonical home of DxUi: editable src/Controls and Tests/Controls, neutral namespace,
  one frame runtime, historical provenance metadata and explicit pending dependencies; remove the duplicate tree.

- Bootstrap independent private repository, guidance, skills, design contracts, validators and build/CI entrypoints.
- Preserve source/test provenance from RedSalamander and extract application-independent frame-runtime foundation.
- Plan RedXe-first control/embedded-host integration; retain RedSalamander migration as a separate HOLD plan.
