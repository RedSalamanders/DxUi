# Validation and evidence

Status: normative intended contract
Last reviewed: 2026-09-08

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

Every supported capability has executable evidence and a truthful status in `capabilities.json`.

The opt-in `MenuResourceScaling` control suite characterizes described-menu memory
without bitmap capture. It rotates and reverses cases across 32 cycles, varying
0/1/2/4/8/12 descriptions in a fixed twelve-entry menu and comparing plain/described
12/24/48-entry menus. All cases must retain the same window extent; report DPI and
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
is rejected. Locally use the authorized warning/desktop-lease harness and verify
focus and physical cursor restoration. A blocked activation path can leave a
temporary 1-by-1 HWND despite correct internal layout, invalidating the measurement.
Do not change the general activation guard or count failed v2/v3 samples as passes.
`MenuTextLayoutResources` separately measures creation, metric computation and
release for twelve primary layouts, twelve secondary layouts and their pairs.
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
Use the [performance contract](../Core/Core_PerformanceAndResources.md) for before/after acceptance and regression advice.
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
force the saved cursor position over unexpected movement merely to make the check pass.
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
shows which tests a missing capability left unrun; a skip is not proof of that capability. CI defines all six native jobs;
their fresh receipts establish execution results.
Physical touch, a human IME session and screen-reader interaction are manual adoption checks, not implied by synthetic
messages or a green foundation suite.

EmbeddedTests independently verifies supplied-device rendering and state changes with pixel readback outside the
rendering path, preview/commit/cancel, scaling and resource limits, pool/view isolation and device replacement.
A 1,000-call warmed composition loop intercepts C++ allocation operators and verifies no heap calls, surface
allocations or extra preparations. Its elapsed time is reported, not a machine-independent pass threshold. The complex
benchmark additionally fails a clean round with any C++ allocation and a dirty round above the configuration's
per-frame allocation ceiling recorded in its receipt. Readback and PNG generation are fixture-only operations. The
public standalone consumer must compile without private headers.

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

Native menu input fixtures wait for a visible popup: the hidden measurement HWND is not ready for input.
Cold creation has a separate five-second setup allowance; owner-message-flood hover and invocation checks
retain their 800 ms deadlines after setup. Capture readiness similarly waits for the final visible surface.
The interactive owner-message-flood fixture aligns the physical cursor with its posted pointer target:
Windows-generated capture moves must describe the same position. It restores the original position only
if the pointer remains at the fixture target, preserving intervening human movement. The 2,000-message
flood, hover/paint assertions and 800 ms hover/invocation bounds remain unchanged.

The native disclosure automation client subscribes from its own MTA thread while the owner thread pumps.
Cold client setup (COM, `ElementFromHandle` and property-event subscription) has a separate 20-second
allowance, because hosted x64 runners have exceeded the former 3000 ms bound. Acknowledged expansion, collapse
and unsubscribe keep their 3000 ms deadlines. Every run logs the setup stage, HRESULT, stage durations and
longest owner-thread pump, which separates a stalled provider thread from slow UIA client initialization.
This is a bounded setup allowance, not a root-cause fix.

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
