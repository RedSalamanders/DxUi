# Review follow-ups: grid cache, window-host accessibility, menus and naming

Status: COMPLETE (2026-09-30). Library scope; consumer adoption stays separate.
Base: the uncommitted [review fixes](ReviewFixes_2026-09-29.md) on `40c6c21`.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[theme](../../UI/UI_ThemeAndTypography.md) and [performance](../../Core/Core_PerformanceAndResources.md).

The developer decided the items the review reported without changing them:

- Grid text layouts: the best frame rate first, then the least memory for it.
- Window hosts raise UI Automation focus changes and never let a stale element act on a replacement control.
- Described menus may hold memory only while a menu is open.
- The disclosure chevron animates even when its state changed while hidden.
- Alert colors default to distinct, readable tones.
- Right-to-left tab titles start at the right.
- Names never repeat the namespace (`DxUi::RunModalLoop`, not `DxUi::RunDxUiModalLoop`).
- NumericStepper step buttons and ColorPicker captions take consumer names and fit translated captions.
- While a menu is open the arrow cursor shows over its popups, and outside them the window under the pointer
  chooses its own cursor again.

## Execution

- [x] Add the `MultilineGridDistinct` benchmark scene (every cell its own text; the Tree keeps short names) and
  measure the retained baseline (the review-fix tree) on it, `MultilineGrid` and `Default` before changing the cache.
- [x] Replace the direct-mapped text-layout cache with a 32-way set-associative table keyed by a mixed, address-free
  hash; shape only the prefix a value can show; place the omission marker on the text's own side.
- [x] Act on the adversarial review of that cache (probes and pixel sweeps by a second agent). An unwrapped prefix is
  final once it overflows the cell with no right-to-left or directional-control text: at `SetLineClamp(1)` the old
  prefix never stopped, and doubling made a 100,000-unit value 1.7–4.3 times slower than shaping it once; it now
  shapes 243 units. A prefix is final at a line beyond the one after the clamp (the reviewer's cut sweep found 14
  last-line differences in 414,965 with one line fewer, none with this rule), the first prefix is sized from the font,
  and a prefix that falls short doubles at most three times before taking its paragraphs. Entries keep the text they
  were shaped from, so a long value is retained by its prefix and its repaint shapes nothing. Entries used by the
  previous paint are no longer evicted (scrolling up drew the entering row first and cascaded through the rows still
  in view), the bound is 16,384 entries (at 1,024 the reviewer measured 17% misses with 1,178 visible cells and 36%
with 1,558), omitted tails are shared through
  a keyed table instead of a scan of every entry, the marker scan reads surrogate pairs as characters, the trailing
  blank trim covers every space and invisible character, and the hash is the library's own FNV-1a.
- [x] Find what the multiline scenes spend their frames on. Hiding the grid changed nothing; the Tree drawing the
  long French names through the color-font path (the camera emoji) costs about 7 ms of a 10 ms WARP frame. A host-wide
  retained layout for every text draw hit 99.7% yet did not change FPS and added about 0.8 MB, so it was reverted.
- [x] Keep dirty-frame allocations at or below the old cache: deterministic hashing (no format address), victims that
  reuse string storage, power-of-two key and display strings, 32 ways (both multiline scenes: 1,080 per round).
- [x] Paired A1/B1/B2/A2 comparison of the final tree on all three scenes, recorded in the performance contract:
  [two local sets](../../../Measurements/ReviewFollowUps/2026-09-29/paired-local/README.md), run by hand because both
  trees are uncommitted on `40c6c21`. Allocations are equal or lower and clean rounds allocate nothing; the `Default`
  dirty rate is higher in all four crossings (+5.1% to +14.5%); B's median private bytes average -0.08 to +0.52 MB
  from A's per scene and phase. Every same-binary control drifted beyond its band, so the sets establish neither the
  gain nor the cost; the developer decides on the memory after a quiet-fixture repeat.
- [x] Window-host UIA focus-changed events from the difference between published snapshots, for the element GetFocus
  reports (a tree item, a grid row, a control, or the window once nothing has focus), only while the window holds the
  foreground's keyboard focus; the window's own activation is left to the system's focus event, which UI Automation
  resolves through the fragment root's GetFocus (a revert of the forced activation announcement passed every test, so
  it only duplicated that event). An element's SetFocus moves logical focus before Win32 focus. Lifetime identity and
  runtime ids for window-host elements, tree items and grid rows included, whose calls report
  `UIA_E_ELEMENTNOTAVAILABLE` once their control is gone or replaced; StructureChanged for such a republish; a
  collapsed root reports only its own control's events; a focus callback that removes its control leaves no focus.
- [x] Menus: described-menu memory is returned on close even while a client holds eight row elements (live heap
  within 6.6 KB of before; 8.3 MB while open); arrow cursor over popups, the same-thread window's own cursor outside,
  chosen when the menu opens and again when it closes, forwarded from a posted message so a window closing the menu
  from its `WM_SETCURSOR` handling frees nothing the menu still uses.
- [x] Chevron (starts at its first tick, a gap pauses it), alert tones (a derived partner is black or white by
  measured contrast), right-to-left leading text (tab titles, stepper label and unit, hex caption), namespace-free
  names, step-button names (`NumericStepper`, `ColorPicker::Labels`), bounded caption widths and the hex caption slot.
- [x] Docs, specifications, design-system component pages and CHANGELOG.
- [x] Tests that fail without each change: a copy of this tree in a detached worktree, rebuilt with fixes reverted and
  assertions made non-fatal (the revert scripts and logs stayed in the session). All fixes reverted at once failed 36
  assertions in six suites; then window-host accessibility (9), the grid prefix, marker, blanks, retention and
  eviction with the activation policy (12), eviction, alert contrast, the chevron gap and picker captions (18), chevron
  anchoring (1) and the cursor restored when a menu closes (1). Reverting the forced activation announcement alone
  failed nothing, so it was removed. The cursor chosen when a menu opens has no failing test.
- [x] Second adversarial review round (grid cache; accessibility, chevron, picker, theme, menus) and its fixes:
  - Grid: keys round the layout box to 1/64 DIP (exact float keys re-keyed 10–18% of visible rows per one-row scroll at
    row heights such as compact density's); the lines that size shaping stop at what the cell's height can show (a
    clamp of 1,000 in a short row shaped about 87,000 units per paint, now 971); a retained key may be as long as twice
    the first prefix (wide cells were never retained); a prefix entry records a following CR LF pair; every
    default-ignorable character counts as blank. An unwrapped line's height now comes from the text it shows
    (documented, not reverted: centring on what shows).
  - Accessibility: a root swap is diffed against the tree before it (`SetRoot` announced nothing); a republish finds
    the focused control in the tree before touching it (a panel that cleared its children left a dangling focus the
    next accessible property change dereferenced); a focus the host prunes (a disabled or removed control) is
    published; runtime ids come from a per-process serial instead of a reusable address; UIA SetFocus resolves under
    the accessibility mutex and acts without it; a rootless host keeps a live focused control after its callback.
  - Theme: contrast is measured on what paints, so a translucent supplied fill gets readable text.
  - Menus: closing a menu because its owner is being destroyed messages no window to choose a cursor.
  - Not changed: a click that activates the window can report the clicked control twice (the system's activation event
    and the host's own); the reviewer's O(N) identity lookup per provider call and a reparented picker's stale
    arrangement are recorded as possible follow-ups.
  - Falsified the same way: the round's theme, accessibility and grid fixes reverted together failed 17 assertions
    (Accessibility 5, Rendering 9, Theme 3); with the focused-control check reverted, an ASan build reports a
    heap-use-after-free in `Control::GetAccessibilityRole` under `PublishWindowHostAccessibilitySnapshot`; the
    single-line captions' prefix and rounded keys failed 5; the published prune failed 1 once its test removed the
    control and let UI Automation's late duplicate event settle. Not falsified: the destroyed-owner menu close and the
    recorded CR LF break shape, which no test input reaches.
- [x] Single-line Grid captions (the frame-cost runs found single-line cells the slowest variant: 58.9 against 91.5
  FPS) keep their layouts in the same table, keyed by the whole caption, drawn with the layout exactly where
  `DrawCenteredText` drew the text (gallery byte-identical); a leading-aligned caption on one paragraph shapes only
  the prefix that overflows its cell; the hover check reads the painted layout instead of measuring again. Keys
  round the box to 1/64 DIP while layouts keep the exact box of the cell that built them, for both cell kinds.
- [x] Gallery: this round renders all six images byte-identical to the review-fix tree on this machine. The review
  fixes' own changes (see their plan) and this machine's drift since publishing (a clean `40c6c21` build no longer
  matches `docs/gallery`) leave `docs/gallery` to the CI republish after commit, as before.
- [x] x64 Debug, Release and ASan Debug `test.ps1` (all 20 suites), ARM64 Debug, Release and ASan Debug builds,
  `format.ps1 -Check`, `validate-specs.ps1` and `validate-dependencies.ps1`; the paired benchmark above.
  NativeTextInput activates its window. At close-out it failed three of six x64 Debug runs of this tree and one of two
  of the review-fix tree; in the five runs with a foreground log, the desktop app hosting the session took the
  foreground back within 100 ms of each test window, failing or not. The recorded run passes. `validate-skills.ps1`
  needs PyYAML, which is not installed; no skill changed.
