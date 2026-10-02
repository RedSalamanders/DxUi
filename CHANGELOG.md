# Changelog

New entries start as fragments under [Changes](Changes/README.md) and are folded in here, newest first.

## Unreleased

- The described-menu memory waiver is removed (plan `MenuDescriptionMemory_2026-09-27`). It accepted up to 1,175,552
  clean-round private bytes (+4.6%) that the September 21 pairs measured for described native menus. A local paired
  set of six runs per side (`e47c836` against `6f769ab`, the hosted set's harness) now finds +0.36% (p = 0.70), and dirty
  -0.89% (p = 0.56), within the bands, as the hosted set did (-0.50%, -0.70%). Described menus carry no memory envelope
  anymore; the receipts are under `Measurements/MenuDescriptions/2026-09-30/paired-local`.
- `TestMenuSurvivesAWindowClosingItFromSetCursor` runs instead of skipping everywhere. Since it was added (PR 29) it
  skipped in every run, local and x64 CI alike, as "another window covers the menu-closing window". On a developer
  desktop the cover was the desktop application's own window, which takes the foreground back while the suite runs and
  rises above the test's non-topmost window. The window is now topmost, and the skip names the covering window's
  class and process. The full Menu suite runs with no skip on an interactive desktop.
- Consumers can restore current pins again. #22 added measurement receipts with tracked paths of up to 176 characters.
  A consumer restores its pin under `<root>\.build\dependencies\DxUi\source\<commit>\`, and Git without long paths
  fails there once the full path passes 259 characters. RedXe's `main` CI has failed at restore since it pinned
  `6b34456`, and RedSalamander could not restore `a0b4934` from `D:\RedSalamander`.
  - Every tracked path is now at most 150 characters, so any consumer root of up to 35 characters restores.
    `validate-dependencies.ps1` rejects a longer tracked or untracked-but-unignored path
    (`Build_ToolchainAndConsumption`).
  - 767 evidence files were renamed, with their bytes unchanged: CI artifact directories dropped `native-`,
    `Performance-*` receipts keep 8 of their 32 run-id digits, and `associative-cache-rejected` became
    `assoc-cache-rejected`. Hash lists and manifests name the new paths, and each packet's README records the rename.
- The Menu suite's real-client focus fixtures (the click that activates a window UI Automation has or has not seen, and
  the reactivation of a seen window) no longer fail when another application takes the foreground (plan
  `SeenWindowReactivation_2026-09-30`, item 3). One failed a local Debug run while the desktop application and the Start
  menu took it. They now play each attempt with windows of their own through `RunUntilForegroundHeld`
  (`Tests/Controls/DxUiTestHelpers.h`), the multi-window form of `RunWhileForegroundHeld`, which is now written on top of
  it. A failed expectation records the first failure, with the names the client heard, instead of ending the test, and
  an attempt that another application took the foreground from, from any of its windows, is played again, five times at
  most. The attempt that kept the foreground decides the test, and one that never did records a skip naming the thief.
  Under `--foreground-thief` all three record that skip after five attempts, and never fail. Item 7's first version,
  announcing every move of a gain's turn, and removing the reactivation announcement each still fail their test.
- Activating a window UI Automation has seen announces its focused control (plan `SeenWindowReactivation_2026-09-30`).
  UI Automation answers the first focus event of a window with a call of the fragment root's `GetFocus`, and its later
  ones from whether the window's root element has the keyboard focus, which it does not while the focus is inside a
  control. The host announced nothing for what a gain focuses or restores, so switching back to such a window (Alt+Tab)
  was reported by nothing, and a screen reader did not say which control had the focus. The host now reads, as a gain
  begins and before it publishes anything, how many `GetFocus` calls have begun on the window. When any had, it
  announces the focused element once the gain's message-loop turn ends: the restored control, the one the activation
  focused, or the window when no control has focus. It does not when it already announced a move of that turn (the
  click that activates the window) or when the root element stands for its single focused control, which UI
  Automation reports itself. A window's first activation stays the answer of its first `GetFocus` call, with no
  duplicate. The message the host posts at a gain now names its turn, so the message of an earlier gain, which the
  window lost before the loop turned, no longer ends a later turn. The WindowHost suite tests the decision without a
  desktop, including a stale turn message and a single-control root. The Menu suite reactivates a seen window with an
  in-process UI Automation client, once restoring a control and once focusing the first, and requires the client to
  hear that control once. Five mutants each fail a test. API revision stays 2 (additive diagnostics accessor
  `ControlHost::DebugGetReactivationAnnouncementCount`; private members changed).
- Window-host UI Automation providers resolve their control without scanning the tree (plan
  `ReliabilityAndFollowUps_2026-09-30`, item 6): every provider call searched the published records for its control's
  path (several times per call, so a client walking every element of a window paid for the whole tree on each one), and
  every event searched the live tree for its control. Each snapshot now carries lookup tables built as it is published
  (records by path, hit rectangles by kind, path and item, and control addresses), so a provider call examines a few
  table slots and an event a search of the sorted addresses (about log2 of the records) and a walk down its path. In a
  window of 1,960 buttons a `Navigate` call examined 4,901 records on average (9,796 at most) and now 5.8 (21), a
  property read 2,942 (5,880) and now 3.5 (15), `get_BoundingRectangle` 2,942 (3,921) and now 3.4 (11), a pattern
  query 2,942 (5,880) and now 3.5 (15), and an event's control lookup 1,002 tree nodes (2,001) and now 17 (17); a tree
  of 245 buttons costs the same per call as one of 1,960. What a provider or event reaches is unchanged: the tables
  describe the snapshot's own tree, a control they hold is confirmed against the live tree before it is used, one they
  do not hold (added since the publish, hidden, no element) is searched for there as before, and stale elements still
  answer `UIA_E_ELEMENTNOTAVAILABLE`. What lives inside one control (a tree's items by id, a grid's rows and cells) and
  hit testing a point still scan. Test-only diagnostics count what resolutions examine, resolve an event's path and
  check the tables against a scan of the records.
- A click that activates a window announces the clicked control once (plan `ReliabilityAndFollowUps_2026-09-30`, item
  7): the click sets its control after the window's `WM_SETFOCUS`, in the same turn of its message loop and before UI
  Automation has acted on the system's activation focus event, and the host announced that move besides the event, so a
  client could hear the clicked control twice. What the event reports depends on what UI Automation knows of the window:
  the first focus event of a window it has not reported before is answered with a call of the fragment root's
  `GetFocus`, on whatever thread, which reads the published snapshot when it runs, and its later ones without
  `GetFocus`, from whether the window's root element has the keyboard focus, which reports nothing while the focus is
  inside a control (observed on Windows 11 build 26200). A window that gained focus now leaves a focus move of that turn
  to the event only while no `GetFocus` call has ever begun on it, and announces the move itself otherwise (at worst as
  a duplicate, never not at all): each call counts itself in the window's provider target before it loads the snapshot,
  and the host reads the count after it stores the snapshot of the move, all four operations sequentially consistent, so
  with the count at zero every call that follows loads the snapshot after the store and reports the clicked control. The
  host posts itself a message at the gain whose dispatch ends the turn (as does losing focus, or 500 ms without it,
  should a window procedure never hand it to the host); a click in the window that is already active, or any move in a
  later turn, is announced by the host as before. The WindowHost suite tests the decision without a desktop (a first
  gain's move left to the system and announced once a call has begun, a window asked before, the turn's three ends, and
  a call that a test gate holds on a thread of its own before it counts itself), and the Menu suite plays a click that
  activates a window with an in-process UI Automation client in both cases: a window UI Automation has asked before (the
  client hears the clicked control once, and last) and a first activation whose `GetFocus` call the gate holds until
  after the click (it hears the clicked control as often as it hears the control of a first activation, and never the
  control the activation focused on the way). A window procedure must pass the private message (`WM_APP + 0x06D`) to
  `HandleMessage`, as it does the accessibility ones. API revision stays 2 (additive diagnostics accessors
  `ControlHost::DebugIsInFocusGainTurn` and `DebugGetFocusMovesLeftToSystemCount`; private members changed).
- A hung control test ends the run with its name instead of holding a CI job (plan `ReliabilityAndFollowUps_2026-09-30`,
  item 12). On the pull-request run of PR 30 the x64 ASan Debug job printed nothing for the 35 minutes between starting the
  Menu suite and the job's 40-minute limit, and its log named no test. Every control test a suite runner starts through
  `DXUI_RUN_TEST`, and every fixture suite without named tests, now runs under a watchdog: one thread waits for the deadline
  on a condition variable (it never polls) and, when a test outlives it, writes `TIMEOUT: <TestName> after <N> s` and
  terminates the process with exit code 124, since a stuck test cannot be unwound. The deadline is 300 s, forty times the
  slowest of the 816 non-foreground tests under AddressSanitizer (7.5 s); `DxUi.ControlTests.exe --test-timeout=<seconds>` and
  `test.ps1 -TestTimeout <seconds>` set it and 0 turns it off. Every run prints its deadline, every `[DONE]` marker carries the
  test's duration, and `test.ps1` reports a failing suite's exit code and `TIMEOUT:` line beside the last lines of its log
  (`Tools/SuiteFailure.psm1`) and prints every capability skip under its suite as `skipped <Test>: <reason>`, so a CI log
  shows which tests a missing desktop or foreground left unrun without the uploaded suite log. The three resource fixtures report each sample, so their deadline bounds a cycle. A hidden
  `--watchdog-self-test` switch runs a test that never returns, and `Tools/tests/Test-TestWatchdog.ps1` asserts the exit code,
  the line and the time, and that with the watchdog off the same test still hangs. The audit of the Menu, NativeTextInput and
  resource suites found no unbounded polling loop, `INFINITE` wait or UI Automation wait without a deadline, but two ways a
  test could still hang: a driver thread that gave up before it found its popup left the owner thread in `ContextMenu::Show`'s
  modal loop for good (every driver now starts with `DismissMenusIfDriverFails`, which a new test with a driver that fails
  at once requires to close the menu, and a source scan requires in every driver), and a UI Automation client thread was
  joined after a pumped wait of 3 s although its teardown may need the pumping thread (the Menu and Accessibility tests
  that join one now wait as long as its setup was allowed, 20 s, and fail the test). Tests and tooling only: no library code
  changed, and the five gallery sheets are byte-identical, so `docs/gallery` needs no update.
- A `Grid` that stops painting returns its retained text layouts (plan `ReliabilityAndFollowUps_2026-09-30`, item 9):
  hidden itself or under a hidden ancestor (an unselected tab page, a collapsed panel or page host), in a hidden
  embedded view, detached from its host or given another model, it releases its layouts, their key strings, its tables
  and its ellipsis sign at once, and showing it lays out only the cells it then shows, once. Until now such a grid kept
  its last paint's layouts until a next paint that might never come. A painting grid holds one layout per cell it drew
  last, however far it has scrolled (28 in 32 entries for a 6x4 multiline test grid, 66 in 128 for 10x6, 44 in 64 for
  ten single-line rows) with 6 to 12 KB of key strings and 5 to 14 KB of tables; the layouts are the bulk, about 20 KB
  of process heap each (570 KB for the 6x4 grid, 934 KB for the single-line grid, measured in Release as what a hide
  returns). Painting, the layouts' lifetime while painting and its allocations are unchanged. The hook is a protected
  `Control::OnHidden`, which panels and page hosts forward; a hidden or minimized native window keeps its layouts.
  With this attribution the developer accepted the retention the review follow-ups introduced for their frame rate
  (`Default` dirty rate +11.3%, p = 0.029, over both 2026-09-29 sets): the performance contract's accepted Grid layout
  retention bounds it to one layout per cell of the last paint, and none while the grid is not painting.
- Publishing `docs/gallery` after a merge is no longer a manual copy (plan `ReliabilityAndFollowUps_2026-09-30`, item
  11): the manual `Publish docs gallery` workflow (`.github/workflows/gallery.yml`) regenerates the gallery from a native
  x64 Release build with `gallery.ps1 -PublishDocs`, validates the specifications and commits it to the branch it runs
  on. Like the formatting workflow's apply mode it runs only on `workflow_dispatch` with an explicit boolean input, holds
  `contents: write` for that one job, pushes normally and never forces, and never runs on `pull_request_target`. It
  commits only when a sheet, the HTML index or the README changed (`generation.json` records the source commit and so
  differs after every commit); `Tools/Commit-Gallery.ps1` makes that decision and is tested against fixture repositories.
  `docs/gallery` itself was last rendered on a hosted runner from `ed1dea9`, before PR 29's visual changes; it is
  regenerated here on the developer machine from the final sources (`main` and this tree render byte-identical sheets
  there), and the design system is republished with those sheets and the guidelines PR 29 changed.
- One validation entry point (plan `ReliabilityAndFollowUps_2026-09-30`, item 10): `validate.ps1` runs the five
  validators and the tooling tests, each in its own process, reports every failure before it fails, and is what CI's
  validation job runs; `test.ps1` now also runs the tooling tests. The validators' file scans no longer enter a nested
  git checkout (a directory other than the scanned root holding a `.git` file or directory, such as an agent's worktree
  under `.claude/worktrees`): validating a main checkout counted every worktree's copy of the Markdown (1,069 files
  instead of about 214), so a half-edited worktree could fail it. Every tooling test script now sets strict mode itself,
  which a tooling test requires: `test.ps1` is strict and the scripts it calls inherit it, so the new watchdog test passed
  11 of 11 cases alone and failed 6 under `test.ps1` (`.Count` on a function's output unrolled to one line or none).
- Paired sets can establish a result on a noisy machine (plan `ReliabilityAndFollowUps_2026-09-30`, item 4):
  `performance-paired.ps1 -Repetitions N` (default 3, at most 10) repeats the interleaved A, B, B, A pass, so each side
  has 2N runs, and judges every phase and metric on all runs at once: an exact two-sided Mann-Whitney U test (counted
  over the observed ranks, ties included, no approximation) of the baseline run medians against the candidate's, with
  the median shift and the metric's band (5% timing and FPS, 2% process memory). A metric is `regressed` or `improved`
  only when p < 0.05 and the shift exceeds the band, and any rise in an exact budget (surface bytes, replacement peak,
  allocations) in any candidate run is `regressed`; a set with a regressed metric is `advice-required`. Same-binary
  spread is reported, not a veto. Six runs against six reach p = 0.0022 when completely separated; two against two
  cannot reach 0.05. The first pass keeps its receipt and comparison names, and `Compare-PerformanceSet` in
  `Tools/PerformanceComparison.psm1` is tested on synthetic receipts. The performance contract and guide state the rule.
- `performance-paired.ps1` compares two trees (plan `ReliabilityAndFollowUps_2026-09-30`, item 5): `-BaselinePath` and
  `-CandidatePath` measure existing DxUi working trees as they are, uncommitted work included, with this checkout's
  harness written into them for the run and their files put back afterwards. A pair with a named tree is refused when
  the two library source fingerprints are identical instead of when the commits are; revisions keep the commit rule.
  `summary.json` records each side's revision or path, commit and fingerprint. The receipt's source fingerprint and
  benchmark input list moved unchanged into `Tools/PerformanceComparison.psm1`, and `Tools/PairedRun.psm1` with its
  tests owns tree selection, the refusal rules, named-tree validation and the harness overlay.
- A moved control is told what it now inherits (plan `ReliabilityAndFollowUps_2026-09-30`, item 8). A control's flow
  direction and density are inherited through its parents and a parent's own change is announced to its children, but
  a move (`ControlHost::SetRoot`, `PageHost::SetPage`, `Panel::AddChild`) announced nothing, so a control that keeps an
  arrangement for them kept the one of its old place: a `ColorPicker` (its steppers, hex field and buttons), a
  `NumericStepper`, a `TabControl` (its header, unless its host changed too) or a horizontal `StackPanel` moved out of
  a right-to-left parent stayed mirrored, and a `Tree` or `Grid` moved between densities kept its row metrics.
  `Control::Reparent` (which `SetParent` calls, and `SetRoot` and `SetPage` use in place of `SetParent` plus
  `PropagateHost`) compares what the control inherits before and after and announces `OnFlowDirectionChanged` /
  `OnDensityChanged` once, in its final place, only when a value differs: adding a child that inherits what its parent
  has, replacing a root and clearing a panel announce nothing, while a child added under a right-to-left or compact
  parent, and a root arriving in a compact host, now hear what they inherit. `ColorPicker` also releases its gradient
  brushes and their Direct2D device reference when its host changes instead of holding the old host's device until its
  next paint, and `Panel::ClearChildren` skips the empty slot a child moved out through `GetChildren()` leaves (it
  dereferenced it). The menu bar and the text field key their layouts on these inputs and were never stale. API
  revision stays 2 (additive diagnostics accessor `ColorPicker::DebugHasCachedBrushes` and protected
  `Control::Reparent`).
- The described-menu memory check is deterministic. `DebugGetContextMenuResources` (a test-only diagnostics hook) counts
  the live menu popups, the text layouts their described rows hold and the accessibility records of menu-popup
  snapshots, exactly and whatever the renderer and the allocator keep; the fixture asserts that all three return to
  their value before the menu opened after it closes while a client holds eight row elements. It no longer bounds the
  process heap, which a software renderer's surfaces and caches (WARP, or the Basic Render Driver of a GPU-less
  runner) swing by up to about 3 MB, and it now runs under AddressSanitizer too. A window-host provider that pins a
  snapshot, a target that keeps its last snapshot after the window closes and a popup that is never freed each fail it.
- NativeTextInput's focus fixtures survive another application taking the foreground. Windows then deactivates the
  window and the host releases its native text session, TSF document included, as designed; the desktop application
  hosting a session did so 30-95 ms after each test window activated and failed the TSF document fixture in three of
  six local runs. The four fixtures that pump after taking focus repeat their sequence (`RunWhileForegroundHeld`, five
  runs at most) until no application takes the foreground and then make exactly their former assertions; when one takes
  it every time they record a capability skip naming its executable, and a regression with the foreground held still
  fails. Two deterministic fixtures deliver the takeover as Windows sends it, and `DxUi.ControlTests.exe
  --foreground-thief[=<minMs>,<maxMs>]` reproduces the desktop application on demand.
- The control-test runner runs single tests: `DxUi.ControlTests.exe --suite=<Suite> --test=<Name>[,<Name>...]` and
  `test.ps1 -Suites <Suite> -Tests <Name>[,<Name>]` run only the named test functions of the suite. Every suite runner
  registers its tests as `DXUI_RUN_TEST(TestName);`, which checks the filter and prints the test's `[START]`/`[DONE]`
  markers. A name no selected suite registers, a malformed list or a fixture suite without named tests fails the run
  (exit code 2) instead of passing with nothing run, and a filtered `test.ps1` run keeps its own log and receipt
  (`*.filtered`), so it never replaces the receipt of the whole suite.
- Repository tooling is PowerShell only (plan `PowerShellTooling_2026-09-30`): the spec, skill, dependency,
  inherited-test and build-matrix validators, the performance comparator and their tests are PowerShell modules and
  scripts, so `test.ps1`, the validators and CI need no Python, pip or PyYAML. The comparator reproduces every stored
  paired comparison under `Measurements` exactly. New root entry points `validate-test-port.ps1` and
  `validate-build-matrix.ps1` join the other validators, `Tools/tests/Invoke-ToolingTests.ps1` runs the tooling tests,
  and `Tools/Install-ClangFormat.ps1` installs the checksum-pinned formatter that `format.ps1` now finds first.
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
