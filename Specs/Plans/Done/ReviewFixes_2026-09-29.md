# Review of the last fifteen days: defects, resources and accessibility

Status: COMPLETE (2026-09-29). Library scope; consumer adoption stays separate.
Base: `40c6c21` (main). Window reviewed: `b129956..40c6c21` (2026-09-14 to 2026-09-28, 115 commits: editor
controls, grid line clamp, described menus, localized layout and disclosure, tree reorder, core-control fixes and
the paired benchmark runner).
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md) and [performance](../../Core/Core_PerformanceAndResources.md).

Seven reviewers each took one slice adversarially (editor controls, grid clamp, described menus, core controls, tree
reorder, tooling, hot-path performance); most high-severity findings were reproduced with probes. Every finding was
checked against the code before it was acted on. Each fix below has a test that fails on `40c6c21`.

## Fixed

Capture and lifetime:

- [x] `EmbeddedHost::CapturedDragContinues` (from `4f72476`) read the captured control before proving it was still
  in the tree: a control destroyed mid-drag (`Panel::ClearChildren`) was read after free at the next `Prepare`. It
  checked only the control's own flags, so hiding or disabling an ancestor kept the drag. Both paths now resolve the
  capture through one shared live-tree query (`IsControlEffectivelyInteractive` / `IsControlInTree`), replacing two
  local copies of the same walk.
- [x] WindowHost dropped the capture of a control that became disabled or hidden (or whose ancestor did) without
  `OnCaptureLost`, leaving a Splitter, ColorPicker, Slider or Tree mid-drag for the next press. `CancelStaleCapture`
  now cancels it at message entry, where no dispatched control is borrowed, as EmbeddedHost already did.

Editor controls:

- [x] Splitter and ColorPicker passed possibly-null brushes straight to Direct2D (the forced-null-brush suite crashed
  them); so did TextField's focus bar and button hovers. Their draws are skipped like the shared helpers'.
- [x] ColorPicker cached its gradient brushes by the raw `ID2D1DeviceContext*`. A recreated context at the same
  address made every preparation fail with D2DERR_WRONG_RESOURCE_DOMAIN. The brushes are now keyed by a retained
  `ID2D1Device` and are hue-independent unit-space gradients placed by transform, so hue drags and layout moves
  create nothing (previously three stop collections and brushes per hue change).
- [x] ColorPicker rewrote the field being typed in (moving the caret before the last hex digit and clearing undo) and
  reset a component stepper's edit on every preview; a canceled component left the picker on the abandoned preview.
  It re-arranged nothing on a runtime flow change, and in right-to-left flow the keys mirrored the saturation axis
  while paint and pointer did not. Escape in the hex field did nothing. The unused `_dragStartArgb` is gone.
- [x] NumericStepper committed a stale preview when an edit's text stopped parsing (the spec says it reverts), sent
  no terminal event for an edit that returned to its start value, never committed a UI Automation value change, kept
  an unrounded value after `SetDecimals`, refused one direction for a step finer than its decimals, rejected
  full-width and Arabic-Indic digits, and rewrote (and republished) unchanged text on every sync.
- [x] Splitter put its separator outside an extent smaller than both minimums, and was absent from the UIA tree; it
  is now a Thumb with RangeValue whose `SetValue` commits like the keyboard.

Tree reorder (`b13984f`):

- [x] A row could be dropped into its own subtree (a cycle for any model that obeys the drop). The drop at release
  was the last move's, and a wheel scroll never retargeted. `SetModel`, `NotifyDataChanged` (a removed source row),
  a right press and `SetReorderEnabled(false)` left stale drag state and capture. Model id 0 could not be dragged.
  Paint scanned the model by id each frame and every move invalidated. A plain click reported a handled drag.
- [x] The same commit drew every row's `iconText` in the icon font, so the published gallery's C/B/I/T tree icons
  became missing-glyph boxes. Tree now uses Grid's rule (private-use glyphs only), shared as `ResolveIconTextFontRole`.

Grid multiline clamp (`71d6446` and follow-ups):

- [x] Cells were drawn with `D2D1_DRAW_TEXT_OPTIONS_CLIP` on a fractional layout box (summed line heights, centred).
  A reviewer's same-binary A/B attributed most of the accepted multiline memory growth to that option (private
  bytes 32–35 MB with it, 29.4–29.8 MB without, 25 MB at base; drawing the same prepared layouts through the old
  path matched base) and showed it shaving the last line's descender row. Cells now draw with color fonts only;
  trimming, `SetMaxHeight` and the tall-first-line clip already bound the text. Described menu text likewise.
- [x] Trailing NEL, VT and FF, and blank lines after a break, added a phantom line (a false ellipsis or a centring
  shift); a 0.5 DIP slack let a trimmed caption offer no tooltip; an exception after the cache key was written left a
  half-built entry that later paints trusted; and single-line captions, laid out against the full cell since
  `71d6446`, offered no tooltip when a horizontal scroll hid their start (the contract already required one).

Menus and accessibility:

- [x] Described Info rows grew for their description but never painted it; described text dropped color glyphs;
  arrow-key navigation raised no UIA focus change (Win32 focus stays on the root popup), so screen readers could not
  follow it; a UIA SetFocus on a submenu's root activated it and dismissed the whole menu; a latent loop in the
  posted invoke is bounded.
- [x] Every window-host event provider pinned a full accessibility snapshot, which UIA retains while a client listens
  (measured tens to hundreds of KB per disclosure toggle until the window closed). Window-host providers now pin
  none; event providers share one factory.

Core controls:

- [x] Right-to-left Checkbox and RadioButton captions were aligned trailing in a right-to-left reading direction,
  that is to the far side of the row. ProgressBar accepted NaN and infinity (reaching UIA, where NaN never compares
  equal) and an empty indeterminate bar kept the frame loop alive. Slider's "same value" tolerance equalled its
  smallest step, dropping real changes.

Tooling:

- [x] `performance-paired.ps1` judged same-source controls with the one-sided regression test: the retained
  2026-09-28 hosted control swung +28.6% clean FPS and was reported within budget. Controls are now two-sided
  (`unstable-control`). `-SkipBuild` always failed on the new worktree; it now skips only this checkout. Findings are
  annotated in CI. `performance.ps1` no longer re-stamps a stale receipt left at its output path and keeps one
  default receipt per scenario.

## Reported, not changed

- Grid's text-layout cache stays direct-mapped (32 slots), as decided. Its slot takes the low bits of `std::hash`,
  which FNV-1a leaves nearly unmixed: texts that differ by a counter crowd a few slots and re-shape each other every
  paint (about 22 of 24 distinct cells per frame in a reviewer's harness). Fibonacci-mixing the key was implemented
  and measured: it adds 3 dirty-round C++ allocations per 40 frames (+0.28%) on the MultilineGrid fixture, whose
  six keys FNV happens to spread, so under the no-growth allocation rule it was reverted for a developer decision. The MultilineGrid fixture gives all four columns of a row identical text, so
  it understates misses. With the CLIP cost removed, the rejected set-associative policy (about 1 MB) deserves a new
  decision against a distinct-text fixture. Cells over 4,096 units are still shaped in full on every paint and hover
  (65 ms at 100k units); capping the shaped prefix fixes it. A trailing "…" follows the paragraph direction, so it
  lands on the wrong side in right-to-left text.
- Window hosts raise no UIA focus-changed event for ordinary intra-window focus moves (Tab); only described menu
  rows now do. Window-host providers still resolve controls by path, so a stale provider can act on a control that
  replaced the original at the same path (embedded hosts guard this by lifetime).
- Described menus retain two shaped layouts and a proxy control per row (about 36–40 KB per row); virtualizing to the
  viewport would bound it. With a submenu open, the parent row keeps logical focus.
- `MenuFlyoutItem` grew by 64 bytes per item for the two new strings; `MenuDescriptionMemory_2026-09-27` keeps
  investigating the clean-scene waiver, for which no code cause was found.
- Smaller items: the disclosure chevron can stay unsettled when its transition starts while hidden; the alert-color
  fallback is per color rather than per pair; TabControl titles repeat the right-to-left trailing alignment; the
  modal-loop type rename has no compatibility aliases; step buttons and ColorPicker captions need consumer names and
  widths.

## Validation

- [x] Falsification: a detached `40c6c21` worktree with only the new tests (assertions made non-fatal for that run)
  fails every new check in x64 Debug: EditorControls (23 assertions), Tree (8), WindowHost (the forced-null-brush
  editor render terminates the process with 0xC000041D; with it skipped, the disabled/hidden capture checks fail),
  Accessibility (Splitter UIA), NewControls (ProgressBar, Slider), Menu (Info description, UIA focus changes,
  submenu-root SetFocus) and Embedded (right-to-left caption, hidden ancestor). Its ASan Debug EmbeddedTests report
  `heap-use-after-free` in `Control::IsEnabled` from `EmbeddedHost::CapturedDragContinues` in `Prepare`, freed by the
  captured Splitter's destructor in `Panel::ClearChildren`. The fixed tree passes the same tests.
- [x] Local x64 Debug: Accessibility, Menu, NewControls, Control, EditorControls, Tree, WindowHost, Grid, TextField and
  Embedded (2,717 checks) pass. The pinned clang-format 22.1.3 check, `validate-specs` and `validate-dependencies`
  pass; 42 of 44 tool tests pass and the other two (and `validate-skills`) need PyYAML, absent here; no skill changed.
- [x] Final code, `test.ps1`: x64 Release, Debug and ASan Debug each pass all 20 suites (ASan with no report and its
  detection probe passing). An earlier ASan run caught a timing flaw in the new UIA focus test (an idle second before
  the keys); the test now waits on the client's first-row event instead and passes in all three configurations.
- [x] Performance: a [local paired set](../../../Measurements/ReviewFixes/2026-09-29/paired-local/README.md) against
  `40c6c21` keeps every deterministic budget (0 clean, 1,080 and 2,160 dirty C++ allocations) and lowers MultilineGrid
  private bytes (dirty −4.0 to −5.9 MB, dirty peak about −9.7 MB, clean −5.0 to −6.9 MB). Timing is not concluded:
  both same-binary controls are unstable, which the corrected paired runner now reports. The
  [slot-hash experiment](../../../Measurements/ReviewFixes/2026-09-29/slot-hash-experiment/README.md) records the
  reverted hash change.
- [x] Gallery: rendering both trees on this machine changes only the ColorPicker tile (one-level gradient rounding),
  the Tree tile (letters instead of missing-glyph boxes) and 2–3 Grid pixels, identically in all five themes; the
  embedded example is byte-identical to the published one. `docs/gallery` still shows the Tree boxes and should be
  republished from CI after commit, as for earlier gallery changes.
- [x] ARM64 Debug, Release and ASan Debug build cleanly (`/W4 /WX`); there is no native ARM64 host here, so their
  suites remain for CI. This machine's vcpkg picks an MSVC 14.52 toolset without its debug CRT when configuring
  `wil:arm64-windows`, so the pinned, header-only WIL headers from the x64 restore were staged for these builds.
