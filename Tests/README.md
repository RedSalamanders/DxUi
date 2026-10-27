# Running only the tests your change affects

```powershell
./Test-Changes.ps1 -Explain
./Test-Changes.ps1 -Configuration Debug
./Test-PrePush.ps1 -Explain
# After reviewing the six-profile plan:
./Test-PrePush.ps1
./Test-Changes.ps1 -Mode Full -Configuration Debug
```

The default uses local merge-base changes plus staged, unstaged and untracked work. Read the explanation before a large run. Unknown code/build inputs widen coverage. Selected work is partial evidence. PR delegation is pending CI work, and only an enabled workflow with matching reviewed candidate bytes/profile can justify it.
An empty plan reports `NO_WORK; repository NOT_EVALUATED`; a fully delegated plan reports `NO_LOCAL_WORK; CI_PENDING`.
Capability skips are reported explicitly and prevent native-suite receipt reuse. `FULL_SCOPES_PASSED` covers the scope
manifest, while foreground, consumer, gallery and native-platform qualification remain separate obligations.

Native test files use `<Component>.Tests.Something.h/.cpp`, with a real component such as `Controls`, `Support` or `Embedded`; literal `Scope` is rejected. Register new active files in `native-test-files.json`. `test-scopes.json` owns standalone scope/PR metadata. Shared-library or runtime byte changes conservatively invalidate local standalone reuse. No consumer dependency pin changes are part of this workflow.

`Embedded` also checks the prepared-state and normalization oracle for the clean
Prepare timer diagnostic. Its isolated `--test-clean-prepare-timer-oracle` entry
point runs the same assertions. The opt-in command
`DxUi.EmbeddedTests.exe --benchmark-clean-prepare-floor <fresh-report.json> <unsigned-seed>`
retains raw QPC ticks for 1, 8, 64 and 512 clean calls per bracket, with a separate
empty-loop reference. It refuses an existing report path. These observations
characterize timer resolution; they do not replace canonical benchmark metrics
or qualify the performance policy.

`EditorControls` runs `TestUnicode17DefaultGraphemeCorpus`: all 766 pinned original
vectors, every UTF-16 offset, next/previous movement, nearest snapping and counts.
`TextField` adds Unicode 17 password-mask source geometry/hit/deletion cases;
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
```

Repeated unchanged scopes print REUSED. Force reruns them. SkipBuild requires an attestation established by a prior Test-Changes build; stale binaries are rejected. Explicit test.ps1 suite/named-test calls retain the lower-level diagnostic interface. Its ordinary call now routes to affected iteration; Full is explicit. An unknown local comparison ref fails with Git's diagnostic rather than silently selecting no work.

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
the relocated sample, compiles all eight public rich-text headers independently,
and renders the rich sample with explicit document-location metadata. Its receipt
hashes the new capture. This supplements the existing minimal/complex/text-service
and multi-module checks; it does not qualify physical IME/AT or performance.

The named `EditorControls` tests cover native resource/pixel publication, the
real clipboard-to-worker path and bounded loader queues/cancellation/closure.
The named native/embedded decoded-image tests are in `Accessibility`; ordinary
`Embedded` adds DPI, RTL, selection, hide/show and device replacement checks.

Isolated failure children retain the same ordinary Embedded assertions:

```powershell
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-decode-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --test-rich-image-resource-failures
./.build/x64/Debug/DxUi.EmbeddedTests.exe --capture-rich-images output.png Dark
```

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


Generated output is retained per invocation under `.build/evidence/test-runs/<id>/payload`. Read the
[retention contract](../Specs/Testing/Testing_RunEvidence.md) for failed/interrupted runs and verification.
