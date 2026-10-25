# Menu fixture popup readiness

Status: ACTIVE. Source-specific repair requested 17 October 2026; shared acceptance remains owned by
[ConsumerUiQualification](ConsumerUiQualification_2026-10-11.md).

PR #82's main run [39043832683](https://github.com/RedSalamanders/DxUi/actions/runs/39043832683), at `1c1eb10b`,
fails x64 Debug's first Menu case, `TestContextMenuPopupScrollsOversizedContent`. Its discovered HWND is hidden;
the state query supplies no rendered scrollbar state. The other five native profiles and eight consumers pass.
This is a launched failure with zero capability skips. The aggregate gate correctly fails. The exact slow
initialization stage is unmeasured; neither a passing repeat nor source identity establishes its cause.

The oversized-menu fixtures still discover the hidden measurement HWND, unlike the main menu helper which
requires visibility. Consolidate that existing process/owner/visibility rule before querying popup state.
Keep real render, scrollbar, navigation and lifetime assertions, bounded dispatch, driver cleanup and watchdogs.
Generic discovery uses the main fixture's existing five-second bound; popup-state queries and acceptance
assertions retain their existing limits. This does not change performance thresholds.

- [x] Retain the unchanged-source Release baseline and compiled-input identity before candidate measurement.
- [x] Share the readiness predicate across generic, text-labelled and gallery popup discovery, with bounded waiting
  and failure cleanup for the two interactive fixture implementations.
- [x] Add a registered, deterministic initialization regression on a private non-input desktop: reject hidden
  initialization and wrong owner/class, accept the revealed same HWND, and reject a never-ready/hidden-again HWND.
- [x] Verify the regression rejects the legacy hidden-HWND behavior, then passes with the repair.
- [x] Run affected noninteractive x64 Debug/Release/ASan Debug checks and all three ARM64 builds; record exact scope.
- [x] Execute the repaired Menu suite in all three x64 configurations under an explicitly agreed restoring desktop
  lease; retain the failed full Debug run separately from the standalone diagnostic and other profile results.
- [ ] Resolve the separately observed full-run Debug healthy-state-probe failure from bounded dispatch/lifecycle
  evidence, retaining the failure and without widening deadlines or treating a repeat as causal proof.
- [x] Establish modal dispatch readiness in both timeout fixtures within the remainder of their existing discovery
  budget, and retain initial healthy-probe answer/elapsed/lifecycle diagnostics on failure.
- [x] Verify the visible-initialization regression with and without the acknowledgement wait, then rerun affected checks.
- [x] Complete validators and format checks on the final intended snapshot.
- [ ] Observe fresh native CI before claiming merged-source functional acceptance.

## Local evidence

### ARM64 Debug hover-switch follow-up (18 October 2026)

PR #84's first-attempt main run [39072493730](https://github.com/RedSalamanders/DxUi/actions/runs/39072493730),
at `51ef9f292418d7c11fa1058b897ee84d52674106`, fails the 73rd Menu case,
`TestMenuBarHoverMessageSwitchesRootWhilePopupOverlapsMenuBar`, in native ARM64 Debug. The explicit hover post
succeeds, but the existing 900 ms discovery wait does not find a visible owned `Plugins one` popup. The first
72 cases complete; later Menu cases do not execute. The healthy-state and initialization regressions pass
before this distinct assertion. NativeTextInput completes 165/165 cases, and all eight enabled consumer checks
pass. The other five native profiles pass. The aggregate correctly rejects the failed native matrix.

Successful posting proves queueing, and the failed discovery does not identify dispatch, callback acceptance,
replacement creation, visibility, dismissal or text-query failure. Retain the original untraced receipt and
diagnose those boundaries before attributing it. The shared ARM64 lease has restored foreground/pointer and
unrecorded initial focus; `focus=none` is not evidence that focus caused the hover failure.

- [x] Capture bounded UI/driver phase traces through the existing synchronous borrowed diagnostics sink,
  including hover sequence acceptance, replacement preparation and discovery outcomes, without desktop identities.
- [x] Establish a source-reachable cause, repair that boundary and add a deterministic regression/counterfactual.
- [x] Rerun affected noninteractive checks, explicitly agreed x64 Menu diagnostics/full suites and all ARM64 builds.
- [x] Reconcile the source-specific result and remaining native ARM64 observation against ConsumerUiQualification.

The 180 ms pointer-only guard, 900 ms explicit-hover guard, probe deadlines, watchdogs, restore rules and all
performance thresholds remain unchanged. Shared qualification checkboxes and `qualified=false` stay unchanged.
The trace is test infrastructure; it does not change public behavior or gallery pixels.

The controlled Release counterfactual freezes the new trace/regression source and links the unchanged `51ef9f29`
library. The original overlapping-popup case passes, while the new
`TestMenuBarHoverMessageSwitchesRootWithPopupPaintPending` reproduces the original hover-switch assertion.
Its existing peek hook keeps popup paint pending for at most 1,500 ms, armed before the unchanged 180 ms
pointer-only guard. The trace records 136 paint injections, a successful hover post and no hover callback entry. Discovery
continues to answer with the old root's text: 57 discovery calls, longest text probe 9 ms, zero dropped trace
records, and a live old popup at the failed hover guard. The configured guard stays 900 ms; the recorded wait
is 915.018 ms including polling overhead. The failing receipt and verified restoring lease remain retained.

This proves a source-reachable starvation path: the modal loop fetches input, then pending popup paint, then
falls back to the general queue; registered menu-bar hover was missing from the prioritized input set.
Continuous paint can therefore prevent its already queued message from reaching the callback. The repair adds
only that exact registered message after the existing mouse/key checks and before paint. Registration failure
cannot turn into a zero-through-zero queue peek. Sequence, callback acceptance, lifetime checks and cleanup stay
unchanged. The deterministic test uses the existing hook and does not add a production tracing API.

Each trace has separate single-writer UI/driver channels of 64 fixed records, relative times and static phase
labels, coalesced discovery flags and an explicit dropped count. Storage is allocated before opening the menu;
borrowed diagnostic text, HWNDs, coordinates and foreground identities are not retained. Phase rows print only
after failure, after the menu returns and the driver joins; successful cases print a bounded summary. The exact
same regression source is used on both sides of the controlled library comparison. These observations establish
the controlled path, not which path the original untraced native ARM64 failure took.

With only the production priority check changed, the identical frozen regression source passes both focused
Release cases, including the pending-paint assertion and its nonzero-injection requirement. Both traces report
zero dropped records and the receipt reports zero capability skips. The repaired lease completes with foreground
and focus unchanged and pointer restored. The private packet retains the failing and passing executables,
source hashes, filtered receipts and logs separately; a passing focused case does not replace full-suite evidence.

The pre-change raw Release benchmark receipt and first post-change standalone receipt use the same benchmark
hash, fixture and machine, with verifiable v2 identities, but are unpaired observations. Clean/dirty FPS medians
are 2,419.65/606.272 before and 2,377.09/605.074 after. The ordinary comparator retains `advice-required` flags:
clean frame-p95 0.4993 to 0.5458 ms (+9.31%), clean composition CPU-p95 0.0118 to 0.0134 ms (+13.56%), and dirty
composition CPU-p95 0.0186 to 0.0268 ms (+44.09%). Process-memory changes stay within the existing 2% band and
exact allocation/surface budgets hold. These flags remain unresolved; neither FPS proximity nor source inspection
establishes non-regression. The original benchmark executable from this new baseline run was not separately
archived before rebuilding, so its raw receipt is retained without claiming independently rehashable original
binary retention. Later candidate repeats cannot repair that gap or qualify the trusted policy.

Two predeclared repeats after all builds complete retain the same candidate executable/source/benchmark hashes
and verifiable identities. Repeat 1 remains `advice-required` for clean frame-p95 (+6.63%) and dirty composition
CPU-p95 (+17.74%); repeat 2 is `within-noise-budget`. Both raw runs and comparisons are retained alongside the
first flagged sample. No build owned by this run competes with these standalone observations, but continuous
host-load admission and stable matched controls are absent. The later within-budget sample neither attributes
nor waives the first two flags. Performance qualification, the original memory uncertainty and all other HOLDs
remain open; fresh retained base/candidate controls and independent policy review remain the owning ledger's work.

The repaired hover candidate passes all 19 noninteractive suites and all 110 full Menu cases in each x64
profile. The audit binds all 60 native suite receipts to the executable hashes, balances named-test markers,
requires both hover cases exactly once per full Menu run and reports zero capability skips. Both ASan phases
detect the isolated intentional instrumentation fault separately from the passing ordinary suites. All three
full leases preserve or restore foreground/focus/pointer under the approved 30-second automatic Start policy.

| Hover candidate local profile | Noninteractive suites | Full Menu |
| --- | --- | --- |
| Debug | 19/19 pass, zero skips | 110/110 cases pass, zero skips |
| Release | 19/19 pass, zero skips | 110/110 cases pass, zero skips |
| ASan Debug | 19/19 pass, zero skips | 110/110 cases pass, zero skips |

All three ARM64 configurations also build. A separate PE audit verifies all 30 executable outputs across the
six configurations have the requested target architecture; it establishes no ARM64 execution. Fresh exact-source
native ARM64 Debug observation remains required before source-specific native acceptance. If the assertion
recurs, retain this bounded trace to distinguish queued input, callback/sequence rejection, replacement creation,
visibility, discovery response and dismissal. Keep the original untraced ARM64 failure and the earlier untraced
healthy-probe failure open; neither is retrospectively attributed by this controlled regression.

The unchanged `1c1eb10b` x64 Release baseline was built and measured before copying the repair into an isolated
checkout. Its library-source fingerprint is `C02AFF7704D4193463D40370AE852AFA549031F351515D542CB4E8CB605C1BB6`;
the candidate's first receipt has that same fingerprint and benchmark hash. The retained baseline dirty-round
median is 600.342 completed offscreen FPS. These are unpaired observations, not performance acceptance. Candidate
test invocations also report FPS/memory; those made alongside ARM64 compilation retain that concurrency limitation.

The registered regression passes through the supported filtered Release `Control` invocation. A separate
counterfactual compiles the same regression body with only the visibility predicate removed (apart from include
paths and trailing blank lines): it fails at the hidden-initialization assertion with exit 1; the repaired helper
passes with exit 0. No input-desktop switch or foreground interaction is used for this regression. The private
diagnostic packet retains both builds, outputs, source hashes and the baseline artifacts; it is separate from
canonical performance acceptance and the independently recorded real Menu runs.

The visibility-only candidate passes all 19 noninteractive suites in each of x64 Debug, Release and ASan Debug: 57 suite
receipts, zero failures and zero capability skips. The initialization regression completes once in each profile.
The receipt audit checks executable hashes and balanced named-test start/completion markers. The isolated ASan
instrumentation probe diagnoses its deliberate use-after-free; that expected child failure is separate from
the passing suites. All three ARM64 configurations build; native ARM64 execution remains open.

Repository tooling passes all 18 groups, with identity-matched reuse between x64 profiles, and the pinned
22.1.3 format check passes. PrePush accounting retains the four separate foreground suites. This repair runs
only the explicitly approved Menu scope locally; NativeTextInput and both menu-resource fixtures remain
separate acceptance obligations. Final documentation closeout is revalidated before delivery.

## Retained full-run Menu failure

The approved full Debug Menu run fails `TestContextMenuDebugStateProbeBoundsWedgedWindowThread` at its initial
healthy-state assertion, before the deliberate wedge is installed: 49 named cases start, 48 complete, and zero
capability skips are reported. Both oversized-popup cases pass earlier in that same run. The desktop lease
completes with foreground/focus unchanged and the pointer restored. Later cases do not execute.

The same Debug executable passes the one-case fresh-process diagnostic (2.052 seconds); the full Release and
ASan Debug Menu runs each pass all 108 cases, including that probe, with zero skips and verified lease restoration. These passes
do not explain the Debug full-run failure. Its combined assertion records neither the probe's elapsed time
nor whether it failed to answer, observed a dismissed HWND, or returned unexpected item data. Do not label it
a timeout, initialization defect, foreground loss or noise without that missing evidence.

The repair below adds initial-probe result/lifecycle/elapsed diagnostics, a controlled readiness regression and
fresh full Menu runs in all three profiles. These establish a source-reachable failure path, not the precise
operation that stalled the original untraced run. If it recurs after the repair, retain posted-request handler
entry/completion/abandonment, modal-loop phase and foreground-loss observations before attributing it. The original
failure and fresh native CI remain open alongside the shared qualification HOLDs.

### Dispatch-readiness repair

A subsequent trace-instrumented full Debug run passes all 108 cases, with zero skips and verified desktop
restoration. Its healthy query answers immediately with the expected item; popup creation and modal-loop entry
complete before the query. This is diagnostic evidence on different test bytes and does not attribute the original
untraced failure. Raw traces and desktop identities remain private.

The source permits a visible popup to answer the sent item-text discovery while initialization or a sent callback
still prevents posted dispatch. Both timeout fixtures now require the existing modal-loop peek acknowledgement
before probing. Discovery and this acknowledgement share the original five-second setup budget; no fresh allowance
starts after discovery. State dispatch remains one second, capture remains three seconds, and the wedge release,
elapsed assertion bands and watchdogs are unchanged. No retry of the healthy query is introduced.
The releaser's existing three-second handler-entry wait starts when the probe is prepared after readiness,
rather than during initialization; a slow setup therefore cannot release the stall before it is armed.

`TestContextMenuDebugStateProbeWaitsForModalDispatchAfterVisibleInitialization` uses the existing reentrant peek
hook to hold a visible popup before its first queued dispatch. It services sent messages until the driver discovers
the expected text, then withholds posted dispatch for 1,200 ms. It requires that this stage was actually observed,
followed by the original healthy-state and deliberate-wedge assertions. Removing only the acknowledgement wait
must reproduce the original healthy-state assertion failure within the unchanged one-second probe deadline.
The normal probe case and capture recovery retain their original assertions. Healthy failures now retain answer,
elapsed, item-count and window validity/visibility/owner-match observations without raw desktop identifiers.

The final counterfactual removes only the acknowledgement wait, with the regression body, handler-entry scoping
and production library unchanged. It fails the original healthy-state assertion after 1,009 ms: no answer, zero
items, and a still-live, visible, correctly owned popup. Exit 1 and zero capability skips are retained separately;
the desktop lease completes with foreground, focus and pointer unchanged. Source SHA-256 values are
`593DFB1E56D063A61C7DC74A79968C34C3FEB738315DF77EBE0F62B55AFDBD46` for the repair and
`204C17CA8593586912150082535F2754345B2DE76ED5BEE5A3E9813ED1BAC41F` for the counterfactual.

The repaired candidate passes all 19 noninteractive suites in each x64 profile and all 109 full Menu cases in
each profile, including the normal healthy probe, forced-initialization regression and capture recovery. The
receipt audit checks all 60 suite receipts against executable hashes and balanced named-test markers, requires
each regression once per applicable suite and reports zero capability skips. All six configurations build;
ARM64 runtime execution remains unqualified. Both ASan test phases diagnose their isolated intentional fault;
those expected child failures are separate from the passing suites. All three full candidate leases complete
with foreground, focus and pointer unchanged. No healthy query is retried and no deadline or threshold is widened.

| Repaired local profile | Noninteractive suites | Full Menu | ARM64 cross-build |
| --- | --- | --- | --- |
| Debug | 19/19 pass, zero skips | 109/109 cases pass, zero skips | Pass |
| Release | 19/19 pass, zero skips | 109/109 cases pass, zero skips | Pass |
| ASan Debug | 19/19 pass, zero skips | 109/109 cases pass, zero skips | Pass |

The owning library-source fingerprint remains the retained baseline's value. Every runtime invocation records
FPS/memory as unpaired observations; this is not paired performance acceptance. Validators, all 18 tooling groups
and pinned 22.1.3 formatting are checked on the final intended snapshot. The source-specific readiness repair
does not establish the precise stalling operation in the original uninstrumented run; attribution and fresh
native CI remain open. NativeTextInput, both menu-resource fixtures and all shared qualification gates remain separate.

The following matrix predates the dispatch-readiness repair and is retained rather than reused for changed test bytes.

| Local profile | Noninteractive suites | Full Menu | ARM64 cross-build |
| --- | --- | --- | --- |
| Debug | 19/19 pass, zero skips | Failed at the initial healthy-state probe; 48 cases complete | Pass |
| Release | 19/19 pass, zero skips | 108/108 cases pass, zero skips | Pass |
| ASan Debug | 19/19 pass, zero skips | 108/108 cases pass, zero skips | Pass |

All four leases in this earlier matrix (the three full profiles and the separate focused Debug diagnostic) report completed
restoration or unchanged foreground, focus and pointer states. The focused pass never replaces the failed full
suite receipt. The original oversized-popup case completes successfully in all three full profiles. No code,
deadline or threshold changed between these runs; the Debug full/focused receipts bind the same executable hash.

The earlier visibility/dispatch-readiness changes above affect test infrastructure only; the 18 October
hover follow-up additionally changes modal input priority. Neither changes gallery pixels or usage APIs;
no gallery regeneration is needed. No baseline, policy, investigation band, exact budget or allocation ceiling is
changed. All existing performance-policy, ARM64 opt-in resource, physical input/device-removal, real IME/AT/touch
and consumer acceptance gates remain open. Source-specific functional repair does not close shared qualification.

### Latest merged-main native observation (25 October 2026)

DxUi main run [40204528951](https://github.com/RedSalamanders/DxUi/actions/runs/40204528951), built from
`4faf16ed647df71cd7d5623c1682738592771c3e`, completed five of six native profiles successfully. ARM64 Debug
failed `TestMenuBarHoverMessageSwitchesRootWhilePopupOverlapsMenuBar`; the prior comparable ARM64 Debug run
[40197354712](https://github.com/RedSalamanders/DxUi/actions/runs/40197354712) passed, and the menu implementation
and test source blobs are unchanged between those main commits. The failed hosted trace records
`pointer.callback`, `pointer.accepted`, and `root.switch.begin` before the existing 180 ms pointer-only guard fails.
The test's `MenuHoverTraceForTest` had pointer capture disabled, so the accepted event's source, HWND and point are
unknown. Successful `PostMessageW` had not been checked either. This evidence establishes a pointer-driven root
switch during the guard, but does not attribute it to the synthetic posted move or the real cursor.

The focused follow-up enables the trace's existing bounded pointer capture and checks that the probe message was
posted. It changes no production path, hover contract, guard duration or performance threshold. Fresh native
execution and source-specific attribution remain pending; do not treat the 5/6 result as a green matrix.
