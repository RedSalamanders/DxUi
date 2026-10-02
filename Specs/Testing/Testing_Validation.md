# Validation and evidence

Status: normative intended contract
Last reviewed: 2026-10-01

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Every supported capability has executable evidence and a truthful status in `capabilities.json`.

The opt-in `MenuResourceScaling` control suite characterizes described-menu memory
without bitmap capture. It rotates and reverses cases across 32 cycles, varying
0/1/2/4/8/12 descriptions in a fixed twelve-entry menu and comparing plain/described
12/24/48-entry menus. Fixture v5 adds described 12/24/48-entry menus whose captions
all differ, because the native layouts a menu holds can depend on whether captions
repeat (every earlier case repeats one caption in every row). All cases must retain the same window extent; report DPI and
before/rendered/closed process and handle counters. The caller's item vector is
already built at the before sample. Object `sizeof` values exclude native resources
and dynamic allocations. Process-private deltas are not per-entry allocation sizes.
Fixture v2 additionally samples busy/free bytes, entry overhead and region counters
from process heaps using the bounded shared `Tests/Support/HeapDiagnostic.h` helper.
It records errors per heap, allocates nothing while walking and holds only one heap
lock at a time. These totals exclude non-heap native allocations and cannot identify
call stacks or attribute a native text-layout object's exact cost. Heap sampling is
diagnostic work outside timed rendering; retain v1 separately rather than replacing it.
Release x64 CI retains this diagnostic even if another native suite fails; its
results do not waive that failure or establish before/after source non-regression.
Fixture v4 places a visible, owned parent on one native monitor and records the
actual pixel extent and DPI for every opening. `MenuResources` and
`MenuResourceScaling` require normal foreground menu interaction; `--no-activate`
is rejected. Locally run them with `test.ps1 -Interactive -Suites <Suite>`, which
holds the desktop lease and verifies focus and physical cursor restoration (see
Interactive tests below). A blocked activation path can leave a
temporary 1-by-1 HWND despite correct internal layout, invalidating the measurement.
Do not change the general activation guard or count failed v2/v3 samples as passes.
`MenuTextLayoutResources` separately measures creation, metric computation and
release for twelve primary layouts, twelve secondary layouts, their pairs (what a
described row held before it held one layout) and twelve rows of one formatted
layout each (the label, a spacer paragraph and the description in one layout, which
is what a described row holds now; fixture v2 adds this mode).
It uses the same French strings and Body/Small formats, at a declared 396-DIP
width, with no popup, semantic tree or paint. The 32 rotated cycles distinguish
native layout costs from the rest of a menu; they are not a substitute for actual
menu resource measurements. This opt-in diagnostic remains nonactivating and its
Release CI run retains the common benchmark receipt.
`test.ps1` builds the selected configuration, runs Foundation, Embedded and all inherited control suites and records a JSON receipt with
architecture, configuration, time and executable path under `.build/reports`. Failures propagate as nonzero exits.
Every invocation also runs the populated complex-UI benchmark through `performance.ps1` and includes FPS/memory
scenarios and the performance receipt path in each suite receipt. `-PerformanceBaseline` requires a matched
comparison and fails for a suspected regression or invalid evidence; no baseline is explicitly `unpaired`.
`-Interactive` selects the control suites that need the real desktop and runs them under the desktop lease after all of this
(see Interactive tests below); no other option or run reaches the lease.
Use the [performance contract](../Core/Core_PerformanceAndResources.md) for before/after acceptance and regression advice.
A pull request to `main` that changes something the benchmark measures also runs the paired benchmark on a hosted runner
(the contract's [hosted paired gate](../Core/Core_PerformanceAndResources.md#hosted-paired-gate)), and
`Tools/tests/Test-BenchmarkGate.ps1` runs with the other tooling tests in every `validate.ps1` and `test.ps1` run.
The short WARP benchmark reports completed offscreen throughput, not display refresh or hardware acceptance.
`DxUi.ControlTests.exe --suite=<Suite> --test=<Name>[,<Name>...]` (`test.ps1 -Suites <Suite> -Tests <Name>[,<Name>]`) runs
only the named test functions of the selected suite, so one test iterates in seconds instead of a whole suite in minutes.
Names are exact, case-sensitive function names, and every suite runner registers each of its tests as
`DXUI_RUN_TEST(TestName);`, which checks the filter and prints the test's `[START]`/`[DONE]` markers; a test is never
called directly from a runner. A name that no selected suite registers fails the run (exit code 2, naming it) instead of
passing with nothing run, and the fixture suites without named tests (`MenuResources`, `MenuResourceScaling`,
`MenuTextLayoutResources`, `MenuExitLifetime`, `Gallery`, `ButtonContrast`) reject `--test`. Without the option every
test of a suite runs, in its order. A filtered `test.ps1` run is partial evidence: it still runs the benchmark, but its log
and receipt take a `.filtered` suffix and record the names, so they never replace the receipt of the whole suite.
`Tools/tests/Test-TestFilter.ps1` checks this contract against the built executable in every `test.ps1` run that includes a
control suite, and that no runner calls a test directly.

Every control test is bounded, so a hung test cannot hold a CI job. On the pull-request run of PR 30 the x64 ASan Debug job
printed nothing for the 35 minutes between starting the Menu suite and the job's 40-minute limit, and its log named no test.
`DXUI_RUN_TEST` arms a watchdog before each test and disarms it after, and the runner arms a fixture suite without named tests
(`MenuResources`, `MenuResourceScaling`, `MenuTextLayoutResources`, `MenuExitLifetime`, `Gallery`, `ButtonContrast`) as one
unit; the three resource fixtures call `NoteDxUiTestProgress` at every sample, so their deadline restarts and bounds a cycle
instead of the whole fixture. One watchdog thread (`Tests/Support/TestWatchdog.h`) waits for the armed deadline on a condition
variable with a timeout and never polls. A test that outlives its deadline ends the run: the watchdog writes
`TIMEOUT: <TestName> after <N> s` to the stderr handle and terminates the process with exit code 124 (the code of
`timeout(1)`; a failed check exits 1 and a usage error 2). It terminates instead of unwinding because the test thread is stuck
and may hold the locks of the C++ streams, so the line bypasses them and what stdout still buffers is lost; the
`[START]`/`[DONE]` markers and failures are on stderr. The deadline is 300 s per test; `--test-timeout=<seconds>`
(`test.ps1 -TestTimeout <seconds>`) sets it and 0 turns it off, as debugging a test needs. Every run prints its deadline on a
`[WATCHDOG]` line and every `[DONE]` marker carries the duration (`[DONE] <TestName> (1.234 s)`), so a log shows how close a
test came. The number comes from measurement: on 2026-09-30, on the developer's machine while other builds ran, the slowest of
the 816 tests of the 15 non-foreground suites took 2.5 s in x64 Debug (`TestColorPickerMovedBetweenHostsMatchesAFreshOne`),
2.4 s in x64 Release (`TestGridReleasesItsLayoutsWhenItStopsPainting`) and 7.5 s in x64 ASan Debug
(`TestAttachedLargeGroupedGridLongRunScrollingStaysBoundedWithoutResizeChurn`), the slowest suite (Rendering) took 18 s, 17 s and
33 s, and of the fixture suites `Gallery` took 5 s, 4.5 s and 10 s and `ButtonContrast`, `MenuTextLayoutResources` and
`MenuExitLifetime` 3 s at most. 300 s is forty times the slowest ASan test and about five times a UI Automation client test that waits
out its allowances (20 s to set up, 20 s to end and several 3 s notification waits), and it ends a hung job in five minutes
instead of at the 40-minute limit. The foreground suites take the same deadline; the `[DONE]` durations of a foreground run show
how close any of their tests comes. Native CI also bounds its steps, at about twice their slowest runs on 2026-09-30:
`test.ps1` at 25 minutes (13 at most) and each consumer build at 15 (9 at most), so a hang the watchdog cannot reach
(Foundation, Embedded, the consumer) still leaves the job time to upload its logs.
`test.ps1` reports a control suite that exits nonzero with its exit code, the `TIMEOUT:` line when the watchdog ended it (also
stored as `timeout` in the suite's receipt) and the last twelve lines of its log (`Tools/SuiteFailure.psm1`), and its final
error carries the first two: `DxUi failed suites: Menu exited with code 124 (TIMEOUT: <TestName> after 300 s)`.
The hidden switch `DxUi.ControlTests.exe --watchdog-self-test[=test|fixture|progress]` is in no suite and reached by no
`test.ps1` run: `test` runs one test that blocks forever on an event nobody sets, `fixture` a fixture suite that does, and
`progress` a fixture that outlives its deadline as a whole but reports progress every second and so ends by itself.
`Tools/tests/Test-TestWatchdog.ps1` runs them (2 s deadline, 3 s for `progress`) and asserts the exit code, the `TIMEOUT:` line,
that the run ended at its deadline and spent almost no CPU waiting; that the same test with `--test-timeout=0` is still hanging
when the script gives up on it after 6 s (the falsification of the rest: every run in the script is bounded, kills only its own
child, and a broken watchdog fails a case instead of hanging the script); the option's defaults and its rejection of malformed
values; and that the log of a redirected run, as `test.ps1` writes it, yields a failure report naming the exit code and the hung
test. `test.ps1` runs it after the build in every run that includes a control suite.
A test process never waits on a dialog. Every native test executable first calls `Tests/Support/FailureReports.h`.
- In a Debug build, a failed runtime check (an STL range check, a CRT assertion) writes its report to stderr and ends the
  run with exit code 3, instead of opening the CRT's modal Abort/Retry/Ignore box.
- Windows Error Reporting's dialog is suppressed.
- The hidden switch `--failure-report-self-test` fails such a check on purpose. `Test-TestWatchdog.ps1` requires it to end
  within its bound, with exit code 3 and the report among the printed lines, or with exit code 0 in Release, which has no
  such checks.
The foundation suite covers timing edge cases, nested stage restoration, reduced motion and injected diagnostics.

Repository tools are PowerShell 7 scripts with no other runtime; the validators live in `Tools/Validation.psm1` and
the performance comparator in `Tools/PerformanceComparison.psm1`, and `Tools/tests/Invoke-ToolingTests.ps1` runs their
tests. `validate.ps1` is the one validation entry point: it runs the five validators below and the tooling tests, each
in its own process, and reports every failure before it fails. CI's validation job runs it, and `test.ps1` runs the
tooling tests beside its native suites. Every script under `Tools/tests` sets `Set-StrictMode -Version Latest` itself,
as `test.ps1` and `Invoke-ToolingTests.ps1` do, because the scripts they call inherit it: a script that is not strict
itself can pass when run alone and fail under them, and a tooling test requires it. The validators' file scans do not
enter a nested git checkout, a directory other than the scanned root that holds a `.git` file or directory (such as an
agent's worktree under `.claude/worktrees`): another checkout's files are not this tree's, and a half-edited copy
cannot fail it.
`validate-skills.ps1` checks all repository skills, reading front matter as a strict `key: value` subset of YAML
that any YAML parser reads the same way; `validate-specs.ps1`
checks local links, normative documents and plan indexes; `validate-dependencies.ps1` verifies historical origin
metadata, current ownership paths and the exact pending dependency inventory. Original hashes describe the original
commit only: editing owned source must not fail a hash check. New unresolved includes, missing owned files,
developer-local files, escaping/missing build inputs and a second static-library target are rejected. Supported source may not reach
into a consumer. Validators need no personal Codex installation.

The required native matrix is x64/ARM64 and Debug/Release/ASan Debug. Cross-compilation is not a runtime pass. CI uses explicit
VS 2026 images with separate native ARM64 execution. Record image/compiler identity in build logs. No fixture changes
real audio/camera defaults, user settings or application data.
Interactive menu drivers wait for the popup's modal capture after visibility and use its DPI context. When
asserting a retained painted hover, their physical cursor agrees with the delivered point so OS-generated moves
cannot undo the fixture's input. Restore that cursor only while it still has the test's position; do not overwrite
human movement. Failed hover assertions must report a failure, including outside-dismiss tests.
Cursor fixtures retain the original and actual aligned positions in physical coordinates,
and restore only while the cursor still has that aligned position. Popup-context alignment
must not apply a second DPI conversion. An outer harness verifies restoration and must not
force the saved cursor position over unexpected movement merely to make the check pass: the
desktop lease of `test.ps1 -Interactive` is that harness, and it reports the position the run left (`cursor.atExit`) beside
the one it restored, so a pointer the fixtures did not put back is shown, never counted as theirs.
A blocking `ContextMenu::Show` runs its modal loop on the owner thread and returns only when the menu closes, and the driver
thread is what closes it, so a driver that fails before it has a popup to dismiss (the popup can come up later than the driver's
wait on a slow runner) once left the owner thread there for good: the test hung where it should have reported the failure the
driver recorded. Every driver in the Menu suite therefore begins with `DismissMenusIfDriverFails`, which does nothing when the
driver succeeded and otherwise keeps closing the popups of its owner for up to eight seconds, stopping early once the ones it
closed are gone; `TestMenuDriverThatFailsBeforeItsPopupComesUpStillClosesTheMenu` (in the described-menu group, so also in the
nonactivating NewControls lane) has a driver that fails at once, before its popup exists, and requires the guard to close the
menu that then comes up and the owner thread's `Show` to return, and a source scan in `Tools/tests/Test-TestWatchdog.ps1`
requires the guard first in every driver. A UI Automation client thread a Menu or Accessibility test joins is waited for,
pumping, for as long as its setup was allowed (20 s) and then fails the test, since its teardown may need the pumping thread and
a join does not pump.

Embedded WARP fixtures cover DPI, dirty/clean/hidden behavior, paint-dirty pointer Down after hover or keyboard
focus, alpha, hostile state, negative origins, device loss,
multi-instance lifetime, failed preparation, surface release on hide and zero extent (`surfaceBytes` 0, exactly one
reallocation, pixel-identical restoration, device replacement while hidden), tick-driven dirtying (idle root, caret
blink phase, indeterminate progress) and brush/text-format cache bounds. Hardware presentation, consumer UIA/IME
bridges and real-touch checks remain adoption gates (`embedded-host-text-uia-bridge`).
Preserve imported baselines with their provenance; rebaseline only after reviewing the intended change.
Performance receipts name source/configuration/hardware and include all preparation, texture/cache and recovery costs.

### Inherited coverage and new evidence

`Specs/Done/SourceImport/test-port.json` accounts for all 941 inherited named test cases: 853 reusable runtime cases retained and
88 excluded with individual reasons. Exclusions are application/test-infrastructure utilities not used by DxUi or
source-text assertions; original cases remain available at the recorded source commit. No failed runtime case may
be reclassified merely to obtain a green build. The posted-payload stress case now fills the library's 128-entry
ceiling, with new saturation, wrong-type and stale-token ownership tests in EmbeddedTests.

[Current source-policy dispositions](SourcePolicyDispositions.json) reconcile all 65 historical source-text
exclusions without rewriting the sealed import record. Four mixed cases regain their runtime portions: ScrollPanel
child removal, TabControl close callbacks, and host pointer/hover callbacks that replace the root. Native ASAN
execution covers stale-pointer use. Each other row names runtime replacements, an explicit retirement of helper
spelling/legacy diagnostics, a reviewed ownership policy, or the consumer-owned guard. A code review is not automated
proof of every allocation/API failure or secure wipe; performance, native and manual gates remain separate.
The validator rejects missing/duplicate dispositions and deleted runtime replacements. Future behavior changes
must update the owning runtime contract and its tests; helper names and statement ordering are not public APIs.

Tests/Controls retains eight original visual baselines. Native suites cover control state/layout, theme, grids/trees,
animation (including slider hover/press easing and keyboard thumb travel), native text and IME events, UIA lifetime and menus. Capability skips are emitted in logs and copied into
receipts, and `test.ps1` prints each under its suite with the test it belongs to (`skipped <Test>: <reason>`), so a CI log
shows which tests a missing capability left unrun; a skip is not proof of that capability, and an interactive run
(`test.ps1 -Interactive`, see below) fails on one. CI defines all six native jobs;
their fresh receipts establish execution results.
Physical touch, a human IME session and screen-reader interaction are manual adoption checks, not implied by synthetic
messages or a green foundation suite.

The Grid selection model is held to a copy of its original linear logic. Fixed-seed randomized runs apply thousands of mixed
operations to both and require equal counts, order, anchors and answers after every one, over universes from one id to 5,200
so that selections pass the two sizes at which the model changes how it works: 1,024 ids, up to which `IsSelected` scans the
selection and above which it binary searches the ascending copy, and 4,096 ids, whose room a model gives back when the next
selection needs half of it or less. Focused cases sit on each boundary (membership at every size around 1,024; a selection of
exactly 4,096 ids keeps its room through `Clear` and one of 4,097 gives it back; a range over exactly half of the room gives it
back and one id more reuses it) and a Grid-level case does the same after Ctrl+A, a click and a Shift+click. The room is
asserted through `GridSelectionModel::DebugGetBuffers`, the library's own exact count of both buffers' capacity, so it holds
whatever the allocator keeps. Choosing between the scan and the search cannot change an answer, so no assertion can tell a
limit of 1,023 from 1,024 or a reversed comparison from the right one: the per-call measurement of the
[selection record](../../Measurements/GridSelection/2026-10-01/README.md) does.

EmbeddedTests independently verifies supplied-device rendering and state changes with pixel readback outside the
rendering path, preview/commit/cancel, scaling and resource limits, pool/view isolation and device replacement.
A 1,000-call warmed composition loop intercepts C++ allocation operators and verifies no heap calls, surface
allocations or extra preparations. Its elapsed time is reported, not a machine-independent pass threshold. The complex
benchmark additionally fails a clean round with any C++ allocation and a dirty round above the configuration's
per-frame allocation ceiling recorded in its receipt. Readback and PNG generation are fixture-only operations. The
public standalone consumer must compile without private headers.

The Grid's bounded multiline cells are verified by 16 tests that never compare stored pixels. Each paints a scenario and a
fresh twin on one device (the same cells in a new grid, a short twin of a long value, the unscrolled capture shifted, a
value with a literal ellipsis) and requires them equal, so a difference is a defect whatever the machine's rasterization.
They are `TestGridCopyOfTrimmedMultilineCellsIsExact` and
`TestGridTextLayoutTableStopsAtItsCeilingEvictsTheLeastRecentlyUsedAndHalves` (Grid), eleven `TestGridMultiline...` tests in
`Tests/Controls/GridMultilineRenderingTests.h` (Rendering), two multiline-cell tests (Accessibility) and
`TestEmbeddedMultilineGridFrenchCells` (Embedded, read back from a WARP device). `Grid::DebugSetTextLayoutEntryLimit`
lowers the layout tables' 16,384-entry ceiling so a window can reach it; the Grid test drives the real tables at the
production ceiling. Fixtures that move a control between hosts are shared in `Tests/Controls/DxUiTestMovedControls.h`. Each
test was also run against a temporary mutation of the library (a switch compiled into a scratch build and never committed) and
fails at the assertion that names the defect. The reviewed table of 33 pairs of a mutant and a test, the x64 Debug, Release
and ASan Debug logs and receipts, and the limits (a pixel test cannot see a cache eviction order or the surrogate guard)
are in [the verification packet](../../Measurements/GridTextOverflow/2026-09-30/verification/README.md). CI's native ARM64
Debug, Release and ASan Debug jobs run the same suites; workflow run 36951713344 passed them with no capability skip.

`validate-test-port.ps1` enforces the original case count, unique origins, explicit exclusion reasons and retained/renamed entrypoints. Tooling regression tests verify that deleting a retained case or its disposition fails.
`test.ps1` also runs the deterministic advisory fixture in `Tools/tests/Test-ConsumerUpdate.ps1`.
It verifies same-pin silence, newer green main, pending/failed/missing/wrong-SHA validation,
divergent/ahead pins, malformed upstream identity and offline behavior without network access.
The exact lock remains byte-identical and each invocation emits at most one notice.

Motion-dependent fixtures explicitly choose an animated theme; reduced-motion fixtures explicitly disable motion.
They never change the user's Windows animation preference. Popup pixel capture waits for the visible final-sized
window so a temporary sizing HWND cannot satisfy visual-baseline readiness. Pixel assertions and original baselines
remain unchanged across runner environments.

The default `MenuExitLifetime` process suite leaves an asynchronous menu open with capture and calls
`std::exit(0)`, so CRT thread-local teardown destroys the live controller. Returning normally fails the suite;
ASan profiles catch reentrant destruction. It needs no foreground input.

The described-menu release fixture (`TestDescribedMenuReleasesItsMemoryWhenItCloses`) opens a 200-row menu twice, holds
eight of its UI Automation row elements past the close and asserts on `DebugGetContextMenuResources`, the library's own
exact counts of live menu popups, row text layouts and menu-popup accessibility records: they rise while the menu is open
and return to their value before it opened once it closed. The count is independent of the renderer and the allocator,
so it holds on a software renderer (WARP, or the Basic Render Driver of a GPU-less runner) and under AddressSanitizer.
The process heap is printed beside it for diagnosis only: a software renderer's surfaces and caches share it and swing by
up to about 3 MB between identical open/close cycles.

A menu without descriptions has the same elements. `TestPlainMenuAccessibilityInvokesAndDisconnects` reads a plain popup's
rows through its provider: a radio row's Toggle state, a disabled command's `IsEnabled` and refused Invoke, an information
row with no Invoke, and a command's exact name, MenuItem role and retained bounds. It then focuses and invokes that command
and asserts that the closed popup returned every record. `TestPlainMenuAccessibilityScrollsFocusedRow` checks that a row
the viewport cuts off is offscreen without a rectangle until UIA focus scrolls it in, and that the first row then is.
`TestMenuNativeFocusSelectsNoRowAndRestoresTheChosenOne` gives a plain and a described popup native focus with nothing
chosen (no row is selected), then again after a Down (the chosen row is UIA's focus again). They are in the described-menu
group, so they run in the Menu lane and in the nonactivating NewControls lane.

Fixtures that take real focus can lose it to another application: the desktop application hosting a developer's
session took the foreground back 30-95 ms after each test window activated. Windows then sends the window
`WM_ACTIVATEAPP` (FALSE), `WM_ACTIVATE` (inactive) and `WM_KILLFOCUS`, and the host releases its native text session,
TSF document included, as designed, so a fixture that asserts state which only holds while the window keeps the
foreground failed on a takeover it says nothing about. Such a fixture runs its focus-and-pump sequence through
`RunWhileForegroundHeld` (`Tests/Controls/DxUiTestHelpers.h`): the harness window counts the `WM_ACTIVATEAPP` (FALSE)
deliveries, the sequence repeats after a takeover (the window re-activates through `TryActivateDxUiTestWindow`; five runs
at most) and the assertions are made on the run that kept the foreground, so they are exactly those of a run without a
thief. A run in which no application took the foreground is never repeated, so a regression still fails, and each
repeat is logged as `[FOREGROUND] <executable> (process <id>) took the foreground in run <n> of <max>`. When another
application takes the foreground in every run, the fixture records a capability skip naming its executable and process
id. The NativeTextInput fixtures that pump after taking focus (host focus, the TSF document, the system caret, the
key-to-paint scenario) use it, and two deterministic fixtures deliver the takeover as Windows sends it to a window nobody
can activate: the sequence repeats once, and stops after the maximum with the application named.
A fixture with several windows, or with windows a later run must not inherit (one UI Automation has never seen), plays
each run with windows of its own through `RunUntilForegroundHeld`, of which `RunWhileForegroundHeld` is the one-window
form. The run reports whether another application took the foreground from any of its windows, and the same rule
decides. The Menu suite's click-activation and reactivation fixtures do so (`PlayActivatingClickTest`). Each attempt
builds its scenario afresh. An expectation that fails records the first failure, with the names the UI Automation
client heard, instead of ending the test, and a wait in an attempt that failed or lost the foreground returns at once.
The attempt that kept the foreground decides, so its first failure fails the test. Under `--foreground-thief`, which
takes the foreground within 95 ms of every activation, these fixtures record the skip after five attempts.
`DxUi.ControlTests.exe --foreground-thief[=<minMs>,<maxMs>]` (default 30,95) reproduces the desktop application: a worker
thread takes the foreground for its own window that long after a window of the process became the foreground window. It
reports how often it did, and says so when no window of the process ever held the foreground (Windows keeps it with the
application the user is working in, so there was nothing to take and no takeover was exercised). It needs real focus, so
`--no-activate` rejects it, and it is an opt-in check that a suite survives a thief, not part of `test.ps1`.

Tooltip timer fixtures decide nothing by wall-clock time. A native tooltip's show and hide deadlines are on the UI thread's
animation dispatcher clock. A tick moves that clock by the time since the previous tick, but by no more than the dispatcher's
50 ms hitch clamp, so a runner that stalls moves it little. An idle dispatcher instead reads the wall clock and restarts
from it at its first tick. So the hide-delay fixtures keep the dispatcher ticking with a subscription of their own, take
the deadline from its clock, and dispatch one message at a time, so each tick is observed with the state it left. A delay of twice the clamp is certain to have ticks before its deadline, and the tooltip must be
visible after every one of them and hidden after the first tick at or after the deadline. A pointer move one tick into the
delay must keep the tooltip past the old deadline. A ten-second limit only ends a run whose ticks never come.

The passive-tooltip fixture shows its window without activating it, because a host ticks its tooltip only while its window
is visible. It checks the display lifetime before its click, because the click hides the tooltip: mouse-up releases capture,
and losing capture clears the tooltip. It shows the tooltip with a tick past the longest show delay a mouse hover time allows
(2.5 s). The five-second lifetime runs from that tick, so the tooltip must be visible 4,999 ms after it and hidden at 5,000 ms.

Native menu input fixtures wait for a visible popup: the hidden measurement HWND is not ready for input.
Cold creation has a separate five-second setup allowance; owner-message-flood hover and invocation checks
retain their 800 ms deadlines after setup. Capture readiness similarly waits for the final visible surface.
The interactive owner-message-flood fixture aligns the physical cursor with its posted pointer target:
Windows-generated capture moves must describe the same position. It restores the original position only
if the pointer remains at the fixture target, preserving intervening human movement. The 2,000-message
flood, hover/paint assertions and 800 ms hover/invocation bounds remain unchanged.

A menu test that probes a popup living on another thread waits a bounded time for the answer. The probes cover the popup's
state, item rectangles, paint, text and layout, its backdrop, and a bitmap capture.
- **Where the answer goes.** The popup's own thread answers into a dispatch shared by the caller and the probe's payload. The
  popup is never read from the caller's thread.
- **The bounds.** The state probe is posted, so the menu loop answers it ahead of owner traffic, and is bounded at one second.
  The others are sent, answered at the popup thread's next message retrieval, and bounded at three seconds.
- **A popup thread that does not answer.** The probe fails instead of holding its driver, and with it the open menu, until the
  watchdog ends the run.
- **A late answer.** An answer that comes after the caller gave up lands in the shared dispatch, never in the caller's frame.
  A payload that is never taken is freed with its window.
- **The fixtures.** `TestContextMenuDebugStateProbeBoundsWedgedWindowThread` and
  `TestContextMenuDebugCaptureBoundsWedgedWindowThread` wedge the popup thread in a stalled handler. They require each probe
  to fail at its bound, before the release, and the next capture to succeed.
- **Diagnosis.** On a timeout, the capture readiness wait also prints its longest probe. About 3,000 ms means the popup thread
  stopped answering.

The native disclosure automation client subscribes from its own MTA thread while the owner thread pumps.
Cold client setup (COM, `ElementFromHandle` and property-event subscription) has a separate 20-second
allowance, because hosted x64 runners have exceeded the former 3000 ms bound. Acknowledged expansion, collapse
and unsubscribe keep their 3000 ms deadlines. Every run logs the setup stage, HRESULT, stage durations and
longest owner-thread pump, which separates a stalled provider thread from slow UIA client initialization.
This is a bounded setup allowance, not a root-cause fix.

UI Automation navigation and events are tested with an in-process client, `Tests/Support/UiaTestClient.h`, which the control and
the embedded suites share, so that a test asserts what a client sees and not what a provider says about itself. The client runs
on an MTA thread of its own, as a screen reader is another process, and the thread that owns the providers pumps its messages
in every wait. It subscribes at a window's element (and its subtree) to the automation events, property changes, structure
changes and, desktop-wide, focus changes a test names, and records each with the name, control type and automation id UI
Automation cached for its sender. It walks the content view with a tree walker (the title bar UI Automation adds to every
window is not part of it): the first and last child, the siblings and the parent of an element, a search of an element's
children, and the elements the Selection, SelectionItem and GridItem patterns name, which it compares as UI Automation does,
by runtime id. A client that does not start, or a request unanswered after 20 s, ends the run with a message; a failed
expectation first prints the tree the client walks and the events it heard, and an event that takes a second or more to arrive
is logged.

The Accessibility suite uses it on an `AttachedHostWindow` for the collapsed semantic root of the
[input and accessibility contract](../UI/UI_InputAndAccessibility.md). A window whose only control is a Tree, a Grid or a masked
TextField is walked and listened to beside the same control in a window with a button, under the same expectations, so that
what a client sees of the control's parts is held to be the same in both: the first and last child and the siblings of the
control's element, a search of its children, the parent of each part, the selection container and containing grid they name,
and the canonical identity of the element (the parent of an item, and its fragment root, is the same COM object the window's
root provider is). The twin with a second control passed before the fix; the single-control tests failed, with no child below
the window's element and with no item event heard while an event raised on the window's element itself was.

The [selection events](../UI/UI_InputAndAccessibility.md#selection-events) are the library's own, heard from the right sender (an
item or a row, and the control's element for the invalidation of a selection) by a client subscribed to the window, again for a
control that fills its window and for its twin beside a button or a label. A test step drives a change through the window's own
messages (a click with its MK_CONTROL or MK_SHIFT flags, keys with Ctrl held, which need no foreground), a UI Automation request
or the application's setters, and `UiaTest::HearSelectionEvents` returns the different selection events heard for that step once
the stream of events stopped, which the step requires to be exactly what it expects. The steps cover a single-selection tree's
click, Up and setters, its clearing and items that leave it; a grid's click, Down, Ctrl+click, Shift+click, clearing, Ctrl+A,
exactly 20 and 21 changes, rows out of view and a selected row that leaves the model; and a multi-select tree's set (selected,
added, removed, invalidated, 30 selected rows that only moved, and the items that turning multi-select off drops). In both
trees, a selected item that leaves the tree while another becomes the whole selection is heard as that item being selected,
and one that leaves without that is an invalidation. A diagnostics
hook, `DebugSetAccessibilitySelectionEventHookForTest` (`Tests/Support/SelectionEventInterruption.h`), runs after each selection
event the library raises and stands for what can run while they are raised: hiding the grid, replacing the window's root or
detaching the host there must end the raising after that one event. Each of these tests failed against the library as it was
before single selections and grids raised the events (the client heard nothing, or for the multi-select tree nothing when
multi-select was turned off), and single-point mutants of the change each fail a named step. The text events of a field are the
library's own, raised by its native text-input session as the test drives it. A cell's events, which the library raises none of,
are raised by the test on the cell's element to show that they reach the client through its row. The focus change the host
announces for the item the keyboard reached needs the window to hold the foreground: the Menu suite tests it for a tree and a
grid that fill their window, and records a capability skip where no desktop is available. `test.ps1 -Interactive` runs it, with the
person's agreement (see [Interactive tests](#interactive-tests)).

The embedded suite uses the same client through `Tests/Embedded/EmbeddedUiaBridge.h`, which plays the application: a window
whose provider hosts the window and has the view's root element for its only child, with the view's site adapted to it (the
parent of the view's root and the fragment root of every element in it are that provider) and COM threading, as the view's own
providers have. `EmbeddedUiaTests.h` attaches views whose only control is a Tree and a Grid to it. The client walks from the
application's element through the view's root and the control to its parts and back; hears the events the view raises when it
publishes a change (the control takes the keyboard focus the application reports); and hears the selection events the view
raises itself from `UpdateAccessibility`: a single-selection tree's click, Up, setter and clearing, a grid's click, Ctrl+click,
Shift+click, clearing and Ctrl+A, and a multi-select tree's selected, added, removed and invalidated items, with none for a
publish that changed no selection. Hiding the view or replacing its root while they are raised (the hook again) ends the raising.
The embedded host never collapses its root, so the walk and the multi-select events did not fail before the collapsed-root fix;
they hold the chain a collapsed window host's root must match and prove the client against the embedded providers.

## Interactive tests

The control suites whose contract needs real focus (`Menu`, `NativeTextInput` and the `MenuResources` and `MenuResourceScaling`
fixtures) take the person's foreground window, keyboard focus and pointer, and record a capability skip where a run cannot get them.
`DxUi.ControlTests.exe` rejects `--no-activate` for exactly these four, and `Tools/tests/Test-InteractiveMode.ps1` keeps the list in
`Tools/InteractiveRun.psm1` equal to the runner's. `test.ps1 -Interactive` runs them, with the person's agreement, under the
interactive desktop lease: `DxUi.InteractiveLease.exe` (`Tests/InteractiveLease`; its logic is `Tests/Support/DesktopLease.h` and
`Tests/Support/InteractiveLease.h`, which the control tests exercise without a desktop).

- **Selection.** Without `-Suites` the run is `Menu` and `NativeTextInput`; the two fixtures run when they are named. Any other name
  is refused, by name, before anything is built, so the lease holds only what needs the desktop. `-Tests` and `-TestTimeout` work as
  for any run. Every other control suite keeps `--no-activate`, and the lease is reached only through `-Interactive`. A run without
  it is unchanged: it passes these four suites no `--no-activate`, which the runner rejects, so on a desktop someone is working at it
  can take focus; leave them out of `-Suites` there, and ask for them through `-Interactive`.
- **Refusal.** A run refuses, before anything is built, in a CI job (`CI`, `GITHUB_ACTIONS`, `TF_BUILD` and the like are set) and in
  a process without an interactive window station (a service, a scheduled task, a remote shell). After the build the lease checks the
  session natively (`DxUi.InteractiveLease.exe --check`, which shows and takes nothing) and checks again when it is about to ask: the
  window station is the visible one, the session is not 0 and is active, the input desktop can be opened and is this process's (a
  locked screen, a secure desktop and a disconnected session fail here), no screen saver runs and the pointer position can be read.
  The reason is printed, the run produces no receipt and nothing was shown or taken.
- **After everything else.** The run does what every `test.ps1` run does before its suites (consumer advisory, tooling tests, runtime
  staging, build, the filter, watchdog and lease self-tests, the benchmark), then takes the desktop for the suites alone, so the
  watchdog contract, which bounds a hung test while the suites hold the desktop, has been proved against this executable first.
  The development machine spends about a minute and a half on that, then about a minute and a half on the lease in x64 Debug
  (`Menu` 43 s and `NativeTextInput` 16 s on 2026-09-30; 54 s and 22 s in ASan Debug), and the person's confirmation in between.
- **Asking.** The lease records the desktop, then shows a dialog that says what will happen, for about how long, and that the
  person's window, focus and pointer are restored afterwards. Its default button is Cancel, so a key pressed by accident as the dialog
  appears cannot start a takeover; Start needs a click or Alt+S. If nobody answers within two minutes the answer is Cancel: nothing
  was taken and the run exits with code 21. Ctrl+C while it asks ends the run as well.
- **During the run.** A banner at the top of the primary monitor, above every window and click-through, says that the tests are
  running and to keep hands off the keyboard and mouse; it also holds the foreground that the child is granted. The session-wide lease
  `Local\DxUi.InteractiveTestRun.v1` makes a second run refuse (exit code 22) instead of queueing behind the first, and the system and
  the display stay awake. Each suite is a child of its own, in a kill-on-close job, with its output in its log. The foreground is
  granted to that child alone and never to any process, and the lease ends only a process it started: a child that outlives its bound
  (15 minutes, or three times `-TestTimeout` when that is longer; none with `-TestTimeout 0`) ends with the watchdog's exit code, 124.
- **Restoring.** The foreground window, then its keyboard focus, then the pointer in physical coordinates, last because activating a
  window can move it. Each is verified by reading the desktop again, never by trusting a call's result (the foreground is asked for
  three times and read back for up to a second each time), and the pointer must land exactly. This happens on every exit path: passing
  suites, a failing suite, the watchdog's 124, a hung child, a child that cannot start, a cancelled or unanswered confirmation, a
  warning that cannot be shown, and Ctrl+C or a closing console. It cannot happen if the lease process itself is killed; its job then
  ends the child. Windows may refuse to restore a window of a process with higher integrity than the lease (an elevated terminal
  while the lease is not elevated); the report then says `failed`, and the person's window stays where Windows left it.
- **Reporting.** The line `Restoration: foreground=... focus=... cursor=...` names what became of each part: `unchanged` (the run
  left it as the person had it), `restored`, `gone` (the window closed during the run), `failed`, `skipped` (the focus of a window
  that could not be made the foreground) or `none` (nothing was recorded). The result also keeps the pointer positions, the one the
  person had (`cursor.saved`) and the one the run left (`cursor.atExit`), and whether the run had moved anything
  (`runMovedSomething`): restoring is an action, not a check, so a pointer the fixtures did not put back is shown and never counted
  as theirs.
- **Passing.** Every suite exits 0, none records a capability skip (an interactive run exists to run what other runs skip, so a skip
  means the desktop did not provide it) and no part is `failed`. Logs and receipts carry the suffix `.interactive`
  (`.interactive.filtered` with `-Tests`), so they never replace those of the run that records the suite's skips; a receipt records the
  lease: its state, the confirmation, what became of each part, the pointer positions and the window the person had.

`DxUi.InteractiveLease.exe` exits 0 when every suite passed and the desktop is as it was, 1 when a suite failed, 2 for a malformed
command line, 20 when there is no interactive desktop, 21 when the confirmation was cancelled or unanswered, 22 when another run holds
the lease, 23 when the warning could not be shown (the desktop was not taken), 24 when a suite could not be started, 25 when the
suites passed but the desktop could not be given back and 26 when the run was stopped; `Tools/tests/Test-InteractiveMode.ps1` keeps
`Tools/InteractiveRun.psm1`'s table equal to `LeaseExit` in `Tests/Support/InteractiveLease.h`.

The `InteractiveLease` control suite (in every `test.ps1` run, with `--no-activate`) restores a desktop built of fake windows, a fake
pointer and a fake foreground, so no window is activated and no pointer moves: untouched, moved, refused, never-sticking and
closed-window foregrounds, a focus a window resets, a pointer an activation moves, and each refusal the desktop check names; and it
runs the whole lease against fake services on every path (declined, unanswered, stopped, a warning that cannot be shown, a child
that cannot start, fails or is ended by the watchdog, a desktop that cannot be restored) and requires the restoration, the release
of the session's lease and of the display after each. Its few tests of the Windows backend only read the desktop, or read a window
the test created and never showed. `DxUi.InteractiveLease.exe --self-test` proves the Windows services without anyone's desktop:
children with their logs, exit codes, bound and stop, the session's lease, and the dialog (every answer, the unanswered one included)
and the warning, which open on a private desktop that is never the input desktop. `Tools/tests/Test-InteractiveLease.ps1` runs it
with `--check` and the usage errors (`test.ps1` runs it after the build in a run that includes the `InteractiveLease` suite, and
before an interactive run takes the desktop), and `Tools/tests/Test-InteractiveMode.ps1` covers the selection, the refusals (two runs of
`test.ps1`, under a CI environment and bounded, end before anything is built), the plan and result files and that `test.ps1` reaches
the lease only through `-Interactive`. What none of them proves is the foreground hand-off on a real desktop: only an interactive run
does, and its receipts are that evidence. The six runs of 2026-09-30 ran under RedSalamander's harness and are evidence for what they
ran, not for this lease.

## Independent library workloads

The complex benchmark uses the same synthetic scene as `DxUi.EmbeddedControls.exe --complex-ui`, with no application
services, settings or checkout. EmbeddedTests covers the sample's slider/progress preview and cancellation binding.
Measurements include the scene, benchmark and graphics-helper hashes. Changed fixture identities cannot establish
before/after library regressions. Consumer adoption evidence is owned by its repository; standalone library receipts
live under Measurements and are linked from docs. The external-consumer check renders both sample modes.


Application text-service tests exercise the production COM store with a bounded fake application adapter:
initial insertion before composition start, changed composition ranges, one final commit, cancellation, callback
disconnection, replacement focus, stale revisions, backward/read-only selection, negative screen coordinates,
clipping, text capacity, nested synchronous rejection and deferred asynchronous lock coalescing. A test-owned
window verifies real TSF document attachment and teardown on an explicit STA. Clipboard tests use private memory,
cover failed-copy cut, stale paste, policy, malformed UTF-16, terminators and the embedded edit capacity.
The production Windows transport is exercised through private clipboard ownership calls with real WIL-owned
HGLOBAL allocations: native Grid copy and TextField paste/cut at 65,535/65,536/65,537/100,000 UTF-16 units, checked
allocation arithmetic, allocation/ownership/publication failure, and one open attempt under contention. Large
valid native payloads must still be rejected by the embedded service without changing the document. The control runner and
its clipboard setup/read helpers share that private backend; system clipboard acceptance is a separate manual check.

The public text-service sample is also copied into the relocated exact-pin consumer fixture and executed with
--text-input --output. It checks Unicode paste using an injected private clipboard, a real TSF document attachment
on a hidden application-owned window, and revision-checked geometry; it then saves text-consumer.png. This extends
public compilation/link/render proof without changing the user's clipboard or claiming real IME interaction.
Embedded tests cover missing/dirty/stale geometry, 144-DPI DIP output and cancellation before device-loss draft
capture. Native text tests cover insertion flags, capacity prediction, staged NOLAYOUT, prepared notification,
and sink callbacks releasing the final caller reference. The initial optional sample capture is visually reviewed.
