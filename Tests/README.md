# Running only the tests your change affects

```powershell
./Test-Changes.ps1 -Explain
./Test-Changes.ps1 -Configuration Debug
./Test-Changes.ps1 -Mode PrePush -Explain
# After reviewing the six-profile plan:
./Test-Changes.ps1 -Mode PrePush
./Test-Changes.ps1 -Mode Full -Configuration Debug
```

The default uses the last successful local affected iteration for the selected profile for up to
24 hours and otherwise compares with the local merge base, including staged, unstaged and untracked
work. Changed profile outputs widen the plan. Full, PrePush, Force, explicit scopes and an explicit
base keep their stated obligations. Selected native work builds the whole profile and runs the
affected suites. A failed rerun revokes the prior baseline. The plan lists any interactive work
for separate execution.

An empty plan reports `NOTHING_SELECTED`. `FULL_PASSED` covers every applicable scope of the plan.
Both end with `repository NOT_EVALUATED`. Capability skips prevent native-suite receipt reuse.
PrePush builds all six profiles and runs the three x64 profiles. Main qualification separately
accounts for native ARM64, hosted foreground, the relocated consumer and the Gallery.

Final observations that can be checked after fixture teardown may use `CheckDxUiTestResult`.
It records a failed run and returns normally, allowing retained UIA STA/module state to drain;
the runner returns 1 and never prints its success footer. Return immediately after a false
result when later work depends on it. Keep `Require` for invalid fixture conditions that
cannot safely continue. Original immediate-exit UIA timeouts remain failed evidence.

External consumer fatal-module scenarios check both capture and throw provenance.
The escaping-native cases require the SEH filter symbol in the record's live
capture stack and the original named throw in the independently unwound saved
exception context. Both checks require the corresponding module's PDB; a filter
stack alone cannot qualify the failure's origin.

Native test files use `<Component>.Tests.Something.h/.cpp`, with a real component such as `Controls`, `Support` or `Embedded`; literal `Scope` is rejected. Register new active files in `native-test-files.json`. `test-scopes.json` owns standalone scope/PR metadata. Shared-library or runtime byte changes conservatively invalidate local standalone reuse. No consumer dependency pin changes are part of this workflow.

`Embedded` also checks the prepared-state and normalization oracle for the clean
Prepare timer diagnostic. Its isolated `--test-clean-prepare-timer-oracle` entry
point runs the same assertions, including sticky rejection of an earlier non-`S_FALSE` result in a clean batch
and exact `S_FALSE` for every warmup. Prepared-state and no-work counters remain independently checked. The opt-in command
`DxUi.EmbeddedTests.exe --benchmark-clean-prepare-floor <fresh-report.json> <unsigned-seed>`
retains raw QPC ticks for 1, 8, 64 and 512 clean calls per bracket, with a separate
empty-loop reference. It refuses an existing report path. These observations
characterize timer resolution; they do not replace canonical benchmark metrics
or qualify the performance policy.

`EditorControls` runs `TestUnicode17DefaultGraphemeCorpus`: all 766 pinned original
vectors, every UTF-16 offset, next/previous movement, nearest snapping and counts.
`TestTextElementAnchoredSteppingMatchesFromZero` compares the anchored wrappers with
the from-zero segmentation on that corpus and on 4,096 seeded mixes (CJK, emoji,
combining marks, regional indicators, Hangul jamo, Indic, controls, lone surrogates);
`TestTextElementSteppingVisitsBoundedUnits` counts code units visited per step on
65,536 units of CJK and emoji. `TextField` adds Unicode 17 password-mask source geometry/hit/deletion cases;
`Accessibility` checks rich character range commands, arrows, deletion and undo
in native and embedded hosts. `Embedded` measures a 49,152-unit mixed-script
forward walk (16,384 elements, zero C++ allocation calls/bytes) and oversized
UTF-8 rejection (at most 4 KiB of C++ requests). `EditorControls` also covers exact
UTF-8 admission limits. These deterministic oracles do not qualify latency,
physical IME/AT or visual bidi navigation. Offline generator/pin negative tests
run in the ordinary tooling suite; see [Unicode provenance](../ThirdParty/Unicode/README.md).

The text-element movement cases add a second independent 766-vector oracle for
signed bulk commands (all offsets, zero counts, +/-3 and INT_MIN/INT_MAX), plus
indexed/streaming deletion at every interior ACP and boundary. Fixed five-trial
Release traversal observations retain identical oracle/method bytes before and
after the change; they have no performance acceptance verdict. Maximum-size
native and embedded rich providers verify exact moved counts, endpoint collapse,
explicit selection and unchanged source/history. Interior deletion cases cover
legacy single/multiline fields, editable ComboBox and both rich hosts, including
undo/redo and preservation of explicit noncollapsed source intervals. Embedded
also requires zero C++ allocation calls/bytes for 49,152-unit bulk queries and
both deletion-query paths. These cases remain ordinary non-activating tests.

RichDocument storage cases retain the same fragmented edit/read oracles across
the vector/tree change. Split/merge stress uses an independent flat Unicode
model, captured snapshots, cross-node CRLF joins and undo/redo. Maximum-fragment
coverage builds 65,536 source pieces and repeatedly grows/collapses the tree.
Embedded requires
at most 4 MiB retained C++ bytes after 4,096 interleaved edits with capped history,
zero tracked delta after destruction and zero C++ allocations for same-content
replacement. It sweeps every observed allocation point in replace, transient
replace, reset, undo and redo, comparing complete histories after failure.
Single-backward movement has a fixed large-source observation and retains the
complete corpus/count oracle; elapsed times have no acceptance assertion.
Source-change tests add independent interval reconstruction, scalar/CRLF joins,
undo/redo/branches, transient-page rollover/expiry, reset, concurrency and moved
values. Embedded adds zero-allocation queries after retired previews, a 256 KiB
transient-storage bound and allocation-failure sweeps through page sealing.

For independent resource investigations, force fresh execution and retain paired fixture/build provenance. Timing, native ARM64, real devices, IME/assistive technology and foreground input remain separate qualification requirements. These commands do not manufacture those claims.

Foundation includes controlled TSF-association restoration coverage using in-process COM doubles. It needs no
COM activation, HWND operation or foreground input. Its twelve cases cover an empty post-association focus with
non-null previous association, independent and retiring document ownership, third-document and newer-transition
preservation, failed API calls, and 64 alternating replacement/retirement cycles with balanced COM references.
The real NativeTextInput callback/reentrancy cases remain separately foreground-gated; these controls do not
attribute a hosted desktop failure or replace physical-input/IME qualification.

`TestNativeTextInputNonTextFocusHiddenAndZeroSizePreservesForegroundOwner` runs under the existing approved
foreground lease. Ten observations cover Button/Tree targets on hidden, zero-width, zero-height and visible hosts,
including native-transfer and logical-only controls. Every request changes logical target and asserts actual HWND
client/host geometry, immediate native ownership and ownership after pumping. The owned geometry fixture uses
`SWP_NOSENDCHANGING` to avoid default overlapped-window minimum-width validation. One attempt records all rows;
an observed unexpected transfer to the owned target fails before external foreground unavailability is classified.

The `Control` scope includes intrinsic measurement/Flex/form correctness, the localized form matrix and the
256-field inventory/cache boundary. `Accessibility` includes native/embedded form provider and notification
tests. `performance-forms.ps1` supplies a separate candidate-only synthetic resource diagnostic; its populated
pixel/cache preflights and lifecycle observations do not replace these correctness scopes or consumer tests.

`performance-profile.ps1 -Configuration Release -Platform x64 -Scenario Default` runs a separate, fixed startup/CPU/cache
diagnostic; repeat with `-Scenario MultilineGrid` and optional `-Heap` in a declared bounded serial pilot. Reports retain
phase query failures, bounded worker-thread inventories, raw frame samples and sampling cost. The guarded runner limits
only its owned child to 120 seconds and requires exact v2 build/source/tool/dependency/artifact identity. Its 20/100/200
warm-up checkpoints and heap-off/on observations do not change canonical performance acceptance or qualify the CI policy.
Use `-BoundaryTimers Off` and `-BoundaryTimers On` for the separate Default-only
clear/composition/readback diagnostic. It records absolute QPC boundaries in fixed
forward/reverse controls after 200 warm frames, without the phase/heap sampler.
Declare a finite interleaved fresh-process schedule before measuring. Timers can
perturb execution, and omitting a command changes the workload; CPU call intervals
are not GPU duration or scheduler delay. Heap and MultilineGrid options cannot be
combined with this diagnostic. Exact source/build/raw-byte and child-timeout guards
still apply; no trace permission or non-regression acceptance is implied.
`performance-timing.ps1` runs the hardware frame timing method (A/B/B/A sessions of at least 1,000 frames and 2 s,
UI-thread cycles and late-read GPU timestamps, equivalence against 3%/5% margins); see [performance](../docs/performance.md#frame-timing-on-hardware).
`DxUi.EmbeddedTests.exe --test-frame-timing` checks its native mechanics on WARP and runs in the ordinary `Embedded`
suite; `Tools/tests/Test-FrameTiming.ps1` checks its statistics and machine-state restoration with fakes. Neither
supplies a timing value or a calibration.
`performance-attribution.ps1` runs the hardware attribution method (plan `W0.7`: timing, phases and sampling passes per
fixture, size and frame, and the window host's `Present`); see [performance](../docs/performance.md#where-a-frames-time-goes).
`DxUi.EmbeddedTests.exe --test-frame-attribution` checks its native mechanics on WARP (module classes, every pass on
`FX5`, wrapped-call time inside preparation, samples inside a frame's work) and runs in the ordinary `Embedded` suite;
`Tools/tests/Test-FrameAttribution.ps1` checks its summaries, the decision reading and the runner's admissions with fakes.
UIA client regressions fail a quiet-wait timeout and retain client/provider timelines for the unchanged Grid selection
case. A quiet interval and a passing repeat do not establish the cause of a previously delayed event.
The shared UIA client's owner waits wake for incoming COM messages with a bounded message wait. Sender caching and
the three-second notification deadline stay unchanged. The integrated form regression checks all four presentation
properties, unchanged-state silence and clearing the state; bounded owner attempts and client arrivals remain
failure diagnostics. The retained comparison isolates owner-message dispatch, without qualifying unrelated UIA delays.
The [retained UIA investigation](../Measurements/UiaSelection/2026-10-10/README.md) also supplies a fifteen-case
source-only corruption replay. It checks the archived reader without launching native tests; use a fresh output
directory and keep its synthetic cases separate from runtime evidence.
```powershell
./Test-Changes.ps1 -Scopes <name> -SkipBuild
./Test-Changes.ps1 -Mode Full -Force
./test.ps1 -Full
./test.ps1 -Full -ParallelSuites 4
```

Repeated unchanged scopes print REUSED. Force reruns them. SkipBuild requires matching requested native target/source/artifact coverage and v2 toolchain receipts established by a prior canonical build; stale source or binaries are rejected, and Full requires full-solution coverage. Explicit test.ps1 suite/named-test calls retain the lower-level diagnostic interface. Its ordinary call now routes to affected iteration; Full is explicit. An unknown local comparison ref fails with Git's diagnostic rather than silently selecting no work.

Affected and explicit suite calls build their native target union, including the performance executable, sanitizer
probe and desktop helper when required. `build.ps1 -Projects DxUi.FoundationTests,DxUi.EmbeddedTests` exposes that
union directly. A selected receipt can satisfy matching subsets but cannot satisfy Full or an added target.
Full/PrePush keep the full solution; missing symbols/runtime assets or incompatible proof refuse SkipBuild.

`-ParallelSuites` opts the lower-level runner into 1–8 simultaneous non-foreground
suite processes (default 1). Performance and preflight run before the batch; a
foreground lease stays serial and separate. Each suite keeps its own log and receipt.

PrePush lists the separate focus-taking and menu-resource suites as `INTERACTIVE_NOT_RUN`; run those only with `test.ps1 -Interactive` after the person at the desktop agrees to the time. Capability-skipped scopes remain partial evidence and do not create reusable receipts.

The zero-size NativeTextInput fixture prints bounded `[FOCUS-SETUP]` observations after its guarded setup sequence.
Activation terminal branch, native/logical focus, session/TSF state, host/client dimensions and existing counters
distinguish initial setup from the later resize assertion. `sequenceHeld` describes the complete guarded sequence;
stage reads are sequential observations, not message history. A missing foreground-loss message does not prove
initial acquisition. Setup failure remains a failure, and retries retain all earlier attempts and their exact bytes.

The IME undo-grouping fixture records bounded `[IME-UNDO-SETUP]` observations. Each admitted attempt clears its
field with `SetText(L"")` before `SetFocusControl`, then creates one independent typed `a`. This resets any text and
undo history from a discarded attempt while preserving the same editor and its existing focus behavior.
`TestNativeImeUndoGroupingResetsDiscardedForegroundAttempt` forces one discarded typed attempt, checks the original
composition notifications and two undos, then requires a third undo to return false without changing text or
notifications. The ordinary foreground case retains its late-result assertions. The forced window is excluded
from helper foreground activation by `WS_EX_NOACTIVATE`; it still requests native thread keyboard focus and runs
under the NativeTextInput lease. This is synthetic retry coverage; physical IME and product gates remain separate.

The activation helper services sent messages through `MsgWaitForMultipleObjectsEx` and `PeekMessageW` with
`PM_NOREMOVE`, retaining its 800 ms polling deadline and leaving queued input to the normal pump. Sent callbacks
can outlast a polling interval; the native watchdog still bounds the test. Foreground checks, grants and retry
limits remain unchanged.

`DxUi.ControlTests` disables window ghosting for its process at startup. Its tests paint with `UpdateWindow` and seldom
run a message loop, so after five seconds Windows would treat a test window as hung and ghost it; a ghosted window's
invalidations leave no update region, which made paint assertions fail by wall-clock time on slow (ASan) runs.

Verified GitHub-hosted Windows jobs run requested foreground suites through the same restoring lease with a parent
anchor and a child-specific foreground grant. Both the PowerShell launcher and native `--run-hosted` mode check the
exact hosted markers before taking that runner's desktop. Hosted receipts use `.hosted` and record lease results;
ordinary local and self-hosted runs still require the person's time agreement through `-Interactive`, followed by
the native Start confirmation unless automatic start was explicitly approved.
When the person has explicitly approved automatic start and the test time, add `-InteractiveAutoStart`:

```powershell
./test.ps1 -Configuration Release -Platform x64 -SkipBuild -Interactive -InteractiveAutoStart -Suites Menu,NativeTextInput,MenuResources,MenuResourceScaling
```

The visible dialog defaults to Start and starts after 30 seconds if nobody clicks. Cancel, Esc, closing the dialog
and Ctrl+C stop it. Ordinary `-Interactive` keeps its two-minute Cancel default. Both modes use the same warning,
desktop refusal and verified restoration. Automatic receipts record the native mode/countdown and distinguish
`autostarted` from a clicked `started`; missing or mismatched automatic metadata fails the parent run.
The printed summary uses all three restoration statuses. A false `runMovedSomething` flag or exit code 0 without a
result cannot establish an unchanged desktop; saved/exit pointer coordinates alone do not identify what moved it.
If a verified hosted runner refuses ordinary warning activation, the lease can briefly clip its warning to a marked
input patch and click that owned point. It rechecks authorization/targeting, balances and observes the mouse release,
rejects capture on the foreground or lease thread, uses an opaque nonlayered patch, and restores and verifies the full
warning style, layered attributes and shape before launching any child. Refused targets retain read-only geometry
and hit-window diagnostics. Exposing the patch also returns the warning to the top of the regular topmost band if
another window entered it later; actual hit testing must still prove ownership before any input. Failure remains a
failed run. This fallback does not run on a person's ordinary local desktop; its private-desktop tests take no personal
input or focus.

On a verified GitHub-hosted native ARM64 run, `test.ps1` checks for the image's first-sign-in
`WWAHost` CoreWindow immediately before the foreground lease. If that exact window holds the
foreground, it suppresses the current user's privacy setup prompt and minimizes that window
and a visible `UserOOBEWindowClass` broker window without terminating either process. The lease
does not treat a minimized window's stale thread focus HWND as active keyboard focus; it still
restores and verifies the captured foreground window and pointer, and retains focus verification
for a visible foreground window. The lease still checks the desktop and exact warning input
target; preparation cannot turn a failed or skipped foreground suite into a pass. Ordinary
local and self-hosted runs do not prepare or alter the desktop.

Rich projection-block coverage lives in Scope.Tests.RichProjectionBlocks under
EditorControls, with mapping/no-op allocations, every changed-preparation failure
point, 1,024-line retained-history and maximum-source memory cases in
Scope.Tests.RichProjectionResources under Embedded. Full text/style/source-unit
and every ACP/affinity comparisons include Unicode edits, line joins/splits,
branches with equal external revisions, mode/reset/expiry, held immutable payloads,
moved/retired services and maximum line count. Fixed whole-source/block initial
and 128-edit trials remain descriptive; they do not qualify retained geometry,
host scheduling or the completed-frame performance hold.
The isolated Embedded diagnostic flags `--test-rich-snapshot-slice-failures` and
`--test-rich-projection-failures` trace the same sweeps as the ordinary suite.
Only those explicit child modes install a terminate handler (exit 197) to record
an unexpected noexcept failure without a CRT dialog. Correct code returns zero;
the original Debug failure is a preserved negative diagnostic, never a suite pass.
## Native rich images

`test-consumer.ps1` also copies the application-owned image-loader header into
the relocated sample, compiles the public rich-text and math headers independently,
and renders the rich sample with explicit document-location metadata. Its receipt
hashes the new capture. This supplements the existing minimal/complex/text-service
and multi-module checks; it does not qualify physical IME/AT or performance.

The named `EditorControls` tests cover native resource/pixel publication, the
real clipboard-to-worker path and bounded loader queues/cancellation/closure.
`TestRichTextEditorNativeMessageSchedulingAndFailure` additionally checks that a
checked native preparation failure leaves a healthy sibling's actual text pixels
visible and mutable, remains idle across unrelated captures and cooperative
intrinsic-layout turns, preserves exact
source on both `E_FAIL` and checked `E_OUTOFMEMORY`, permits a sibling's explicit
command without native focus transfer, declines pointer edits against unavailable
geometry, and recovers through the normal
cooperative message-loop path after a content change. An edited ready document
retains its exact source and Undo/Redo across a later preparation failure. The
pixel oracle is restricted to the declared label rectangle, excluding other
controls' hover/chrome. Its baseline frame failure
uses the deferred result so owned window/module cleanup completes before exit 1.
The named native/embedded decoded-image tests are in `Accessibility`; ordinary
`Embedded` adds DPI, RTL, selection, hide/show and device replacement checks.

Isolated failure children retain the same ordinary Embedded assertions:

```powershell
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-decode-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-resource-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-decode-memory
./.build/x64/Debug/DxUi.EmbeddedTests.exe --capture-rich-images output.png Dark
```

`--test-rich-image-decode-memory` runs `TestRichImageDecodeMemory` alone, so the decode memory gate can run in any
configuration: one decode of each `RichImageTests::DecodeScenarios` image and of the 16 MiB encoded-cap PNG (also
through `LoadFile`) under the counting allocator, reporting peak tracked bytes and encoded-size and decoded-size
buffers, then checking exact pixels and the decode bound.

The resource child sweeps observed allocation schedules for staging, dispatch,
completion and loaded Unicode geometry, then checks actual upload coalescing,
owner work budgets, first/later bitmap memory/device HRESULT failures and 128
native placements with excess alternatives preserved. The capture mode uses the
actual sample scene and deterministic caller-supplied pixels without a desktop
window. These cases do not establish physical input/AT, native ARM64 execution
or paired performance acceptance.

The lease logs its original focus capture reason before confirmation and writes `focus.capture.reason` and
`focus.capture.error` in its native result. The six reasons distinguish absent foreground/thread, minimized
foreground, failed query, successful null focus and recorded focus. Only query failure carries a meaningful
Windows error; other branches carry zero. Child receipts share `lease.focusCapture` (`Reason`, `QueryError`)
and keep their own durations. Console summaries display the reason separately from restoration: focus `none`
remains unverified. Older receipts without the diagnostic establish no reason, and malformed new diagnostics
fail validation. The new fields contain no desktop identities; retain existing raw logs privately.
See the [capture diagnostic packet](../Measurements/InteractiveLease/2026-10-18-capture-reasons/README.md) for
the source/receipt hashes and explicit remaining qualification limits.

The IMM eligibility and undo fixtures report initial activation separately from foreground-loss events. Refused activation uses
the existing five-attempt bound and 800 ms helper limit; exhaustion fails setup rather than skipping. The IMM
eligibility fixture guards the complete preview/policy transition and asserts all original predicates only on
the held attempt, with fresh state after a discarded attempt. Its bounded trace records booleans, lengths and
equality, not document text. Controlled regressions cover loss on both sides of preview and initial refusal.
The [fixture-readiness packet](../Measurements/TextInput/2026-10-18-fixture-readiness/README.md) retains
the original failures, deterministic counterfactual and repaired-source validation separately.

The Menu foreground witness publishes creation/error before activation. Its sent-message-only readiness wait
preserves queued input and the absolute 5,000 ms bound. Owner acquisition follows client/witness preparation;
intentional transfers and external losses have separate counters. Teardown waits for actual thread exit while
servicing callbacks. Optional activation diagnostics record existing operations only; UIA setup failures identify
their stage and HRESULT. See the [ASan setup packet](../Measurements/TextInput/2026-10-18-asan-setup/README.md)
for retained diagnostic failures and current validation limits. No setup failure or passing repeat closes a
remaining qualification gate.

## Performance fixtures

Plan item W0.4 defines fixtures `FX1`-`FX14` once each; every consumer runs that one definition: the counter and memory
receipt, the hardware timing harness, the determinism gate, the incremental-versus-fresh oracle and item gate tests.
Contract: [performance fixtures](../Specs/Testing/Testing_Validation.md#performance-fixtures).

| File | Role | Edited to add a fixture? |
| --- | --- | --- |
| `Tests/Embedded/Embedded.Tests.PerformanceFixture.h` | Host-neutral model: `Definition`, `Variant`, `Phase`, `Scenario`, `HostPort`, records, comparator, receipt JSON | No |
| `Tests/Embedded/Embedded.Tests.PerformanceFixtureFx<N>.h` | One fixture: its `Definition` and `Scenario` | Yes: only this file |
| `Tests/Embedded/Embedded.Tests.PerformanceFixtureCatalog.h` | `kCatalog`, one line per fixture, already listing FX1-FX14 | No |
| `Tests/Embedded/Embedded.Tests.PerformanceFixtures.h` | Embedded runner (`RunEmbedded`, `EmbeddedInstance`), receipts, determinism gate, `FrameTiming::MakeFixture` | No |
| `Tests/Controls/Controls.Tests.PerformanceFixtures.cpp` | Window runner (`RunWindow`), its determinism gate and receipt command | No |
| `Tests/Embedded/Embedded.Tests.PerformanceFixtureSelfTest.h` | The framework's own fixture (not in the catalog) | No |
| `Tests/Controls/Controls.Tests.PerformanceFixtureFx4.cpp` | `FX4`'s own interactive runner, its determinism gate (Menu suite) and receipt | Only for `FX4` |

**Interactive-only fixtures.** `Definition::interactiveOnly` means the fixture takes the foreground, so only the
interactive run executes it. `FX4` (menus) is the one such fixture: a `MenuPopup` session activates and focuses its popup,
takes mouse capture, is clamped onto a visible monitor and blurs real screen pixels, and no non-activating seam exists
(the developer's decision of 14 November 2026). Such a fixture has no `Scenario` (`create` stays `nullptr`); its own runner
in `DxUi.ControlTests.exe` runs it. Every ordinary path skips it, prints `FX4: interactive, not run` and never fails on it
or opens a popup: both determinism gates, both executables' `--fixture-receipt`, `performance-fixtures.ps1` without
`-Interactive`, `FrameTiming::MakeFixture` and `performance-timing.ps1`. The catalog JSON exposes `interactiveOnly`. Its two
interactive entry points, both under the desktop lease and only after the person at the desktop agrees to the time:

```powershell
./test.ps1 -Configuration Release -Interactive -Suites Menu -Tests TestPerformanceFixtureFx4IsDeterministic   # two runs, equal counters
./performance-fixtures.ps1 -Fixture FX4 -Interactive                                                          # the receipt, one lease child
```

The runner fails, and writes no receipt, when the activation was blocked (the application window cannot take the
foreground; a popup that is 1 by 1 pixels, differs from its layout rectangle, or is not the foreground window with focus
and capture) or when the person's input reached the menu (a hovered row, an unexpected keyboard row, a dismissed menu).
It never moves the pointer. The run sheet is
[`Measurements/PerformanceAndMotion_2026-11-13/FX4/README.md`](../Measurements/PerformanceAndMotion_2026-11-13/FX4/README.md).

**Building a declared fixture.** Every fixture already has its header and catalog line, so fixtures built at the same
time touch different files. In `Embedded.Tests.PerformanceFixtureFx<N>.h`:

1. Write a `Scenario`. `Build(Context&)` builds the tree on `context.controls` (the host is attached and themed: the dark
   default palette with `reducedMotion` from the definition). `Step(Context&, phase, step)` applies one action: change
   controls directly, or send input through `context.port` (`Pointer`, `Key`, `Character` in device pixels of
   `context.variant`; `AdvanceTime` for the frozen fixture clock; `Resize`). Never read the real clock, sleep, render or
   use randomness: the runner renders exactly one frame after every step, and two runs must record equal counters.
   Accept every step index, also beyond the declared count (the timing harness plays the timed phase indefinitely).
   The scenario may own models its controls borrow; the runner destroys the host first.
2. Fill the `Definition`: `hosts` (`kEmbedded`, `kWindow` or `kBothHosts`), `variants` (`variants[0]` is the default;
   mark hardware-only sizes such as 3840x2160 at 192 DPI `hardwareOnly`, and full-size WARP scenes too heavy to run twice
   in every suite, such as a million-row list or a 4K sweep, `receiptOnly`: receipts and timing run them, the suites'
   determinism gate does not, so keep `variants[0]` small; a scenario may read `context.variant.name` to size its
   scene), `phases` (a first `PhaseKind::Warmup` phase
   settles first-use state and gives hit testing a prepared layout; measured phases are compared by the determinism gate),
   `timedPhase`, `sources` (repository paths of scene files, hashed into the receipt) and `create`. A scene whose controls prepare cooperatively over several
   owner turns sets `settlePreparation`: the embedded runner then repeats a step's frame until the host needs no further
   preparation, so one step is one action with all its turns (FX8; embedded host only).
3. A scene helper too large for the fixture header goes in a new `Tests/Embedded/Embedded.Tests.<Name>.h`, added to
   `Tests/native-test-files.json`. Reuse existing scene helpers (`Controls.Tests.AccessibilityFx6.h`, the tooltip hover-path
   helpers, `BuildCjkEmojiFieldText`) by moving them into such a header rather than copying them.

A fixture that attaches UI Automation (`FX6`) sets `DebugSetAccessibilityNotificationDeliveryManualForTest` and completes
the notification queue at a fixed point of each step (`DebugCompletePendingAccessibilityNotificationsForTest`): the
thread-pool delivery would otherwise decide how much the owner allocates, and would raise events to whoever listens on
the desktop.

**What keeps a window step deterministic.** The window runner renders a step's frame from what the step queued: it
pumps the messages the host posted to itself, leaves `WM_TIMER` and the system caret timer queued (time reaches the host
only through the fixture clock) and paints with `UpdateWindow`. The desktop is kept out of the step: every message
another thread sends to the fixture window (a UI Automation client's `WM_GETOBJECT` and `WM_GETTEXT`, setting and theme
broadcasts) is answered by `DefWindowProc` without the host, and the accessibility build gate answers as if a client
listens (`DebugSetAccessibilitySystemListenersForTest(true)`) whatever the desktop's clients do. Without that, a client's
`WM_GETOBJECT` arriving between two paints of one step publishes the snapshot early and splits the step's one coalesced
publication into two (`FX10` then copies its rich document twice). When the gate fails it prints, for the differing step
and the one before it, every counter that differs and the events that reached the host in one run and not the other:
window messages (marked when another thread sent them), messages dequeued, an invalid region painted by `UpdateWindow`,
fixture ticks, snapshot builds, a change of `UiaClientsAreListening` and the frame schedule left waiting. A new
difference names its event there: if it is a real-time or queue leak, fix the runner; if it is library work that is
asynchronous by design, give it a delivery seam completed at a fixed point of the step; if the same inputs produce
different work, report it as a library defect. Never retry, tolerate or skip.

The fixture header compiles in both test executables: use only DxUi, the standard library and the model header, never
`Check`, `Require`, `GraphicsFixture` or a test window. Nothing else changes: the determinism gate, receipts and
`performance-timing.ps1 -Fixture FX<N>` find the fixture through the catalog.

**Adding a step or a variant** to a built fixture: raise the phase's `steps` or add a `Phase` (its index is the `phase`
argument of `Step`), or add a `Variant`. A change to the steps of a fixture with retained receipts changes their meaning:
take new receipts with `-Replace` and say why in the change.

**A gate assertion** for a plan item lives in that item's own test file and runs the definition:

```cpp
// Embedded suite (Check, Hr available): FX3 adds no layout per steady frame.
const auto run = PerformanceFixtures::RunEmbedded(PerformanceFixtures::Fx3::kDefinition, PerformanceFixtures::Fx3::kVariants[0]);
Check(PerformanceFixtures::Summarize(run.Phase("wheel"), "resources.textLayout.created").maximum == 0u, "FX3 wheel steps create no layout");
```

Counter names are `cppAllocations` (Embedded suite only), `created`, `churn` and each W0.1 counter as `<group>.<name>`
(`frame.paintExecutions`, `resources.textLayout.created`). A definition that sets `largeAllocationBytes` also gets
`cppLargeAllocations` (Embedded suite only): the step's C++ allocations of at least that many bytes, such as `FX7`'s
copies of its 64 Ki-unit document. To measure an action of your own on the live scene, pass
`RunOptions::afterPhases` (embedded) or drive an `EmbeddedInstance` yourself: `Instantiate`, `Create`, then `Step` and
`Frame` around a `Detail::WorkCounterScope`. `RunOptions::countDrawCalls` installs the counting COM wrappers so
that `frame.drawCalls` is counted (it is 0 otherwise). Every receipt reports the `G13` memory lines (`EvaluateG13`;
`RunOptions::probeGridCells` adds the Grid cell probe, on for receipts); a fixture whose line is over budget adds a row
to `kG13Exceptions` naming the item that closes it, and `G13Failures` is the gate. The oracle (`Support.Tests.IncrementalParity.h`) replays a fixture
through `TotalSteps` and `StepAt`: its script's build function builds the scenario on the oracle scene's host and its step
function applies `StepAt(definition, k - 1)` through a `HostPort` over that scene; no such adapter exists yet.

**Running.**

```powershell
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-performance-fixtures   # self-test, determinism gate, FX5 work-counter gate
./test.ps1 -Suites Rendering -Tests TestPerformanceFixtureFrameworkOnWindowHost,TestPerformanceFixturesAreDeterministicOnWindowHost
./performance-fixtures.ps1 -Fixture FX5                                 # receipt under Measurements/PerformanceAndMotion_2026-11-13/FX5
./performance-fixtures.ps1 -Fixture FX5 -Variant uhd-192dpi -Adapter Hardware -SkipBuild
./performance-fixtures.ps1 -Fixture FX5 -CountDrawCalls -SkipBuild             # also frame.drawCalls, under the counting COM wrappers
./performance-timing.ps1 -Purpose Smoke -Fixture FX5 -Adapter Warp -Sessions 1   # the same definition, timed
./.build/x64/Release/DxUi.ControlTests.exe --fx13-cold-start Measurements/PerformanceAndMotion_2026-11-13/FX13/cold-start/cold-start-x64-release.json 5
```

`FX13`'s cold start to the first presented frame is wall-clock time of fresh processes, so it is receipt-only in the
sense of a `receiptOnly` variant (its receipt command runs it, the determinism gate never does; its JSON says
`"receiptOnly":true`), and it lives outside the definition (`Controls.Tests.PerformanceFixtureColdStart.cpp`): each child builds the
`FX13` application on a WARP `WindowHost` shown without activation off every monitor, and stamps its entry and its first
present from the trace provider's test sink; the presented image must show a drawn frame. Its numbers are never a verdict.

The ordinary Embedded and Rendering suites run the self-tests and the determinism gate. The receipt script records
provenance (source commit and dirty flag, build profile, compiler, OS build, adapter and driver or WARP version, display
refresh rates, power plan and a host-idle observation), changes nothing on the machine, never overwrites a receipt
without `-Replace`, and regenerates the fixture directory's README. Timing fields stay "not measured" unless
`-TimingReceipt` names a hardware calibration or comparison from `performance-timing.ps1`.

**Presentation-mode comparison (`W0.9`).** `FX5` (its window form: `ComplexUiScene::BuildRoot` under the host's own `ControlHost`, the
definition still embedded-only) and `FX1`'s multiline page, read as the text-heavy page, run on a `WindowHost` in `HwndSwapChain` and in
`CompositionSwapChain`, on WARP and on the default hardware adapter. `DxUi.ControlTests.exe --presentation-modes <out.json> <FX5|FX1>
Warp|Hardware hwnd-first|composition-first <timing frames> <judged 0|1>` runs one case in one process (both modes, each on a fresh thread
through `OpenWindowFixtureSession`, the one `WindowInstance` the fixture runners use, in a non-activating off-screen window), and
`performance-presentation.ps1` runs every case in both orders, adds provenance and writes
`Measurements/PerformanceAndMotion_2026-11-13/W0.9/presentation-mode.json` (checked by `Tools/PresentationModes.psm1`, tested by
`Tools/tests/Test-PresentationModes.ps1`). Pixels come from the presented image (`Support.Tests.PresentedWindowCapture.h`): the alpha every pixel
presents is recovered exactly from the window read over a black and over a white backdrop, and the capture is calibrated each run against the
back buffer. Counters and pixels decide; frame time and input to present are recorded and marked "not judged, machine busy" unless the
script runs with `-Judged`, which takes at least 1,000 frames per mode and only when the idle preflight of the W0.2 harness admits the host.
The script never runs `performance-timing.ps1`, never switches the power plan and shows nothing that takes focus. A monitor move that changes
DPI and a remote session cannot be driven from the test program: the receipt lists them as not run with the manual steps, and its DPI case is
the host's `WM_DPICHANGED` seam, labelled a simulation.

```powershell
./performance-presentation.ps1                                   # the receipt: both adapters, both fixtures, both orders (timing not judged)
./performance-presentation.ps1 -Adapter Hardware -Judged -TimingFrames 2000 -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W0.9/judged -Replace   # a quiet machine
./test.ps1 -Suites WindowHost -Tests TestPresentationModeComparisonOnWarp,TestPresentationModeComparisonOnHardware
```

Generated output is retained per invocation under `.build/evidence/test-runs/<id>/payload`. Read the
[retention contract](../Specs/Testing/Testing_RunEvidence.md) for failed/interrupted runs and verification.
