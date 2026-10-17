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

This changes test infrastructure only. Library, benchmark, public behavior and gallery pixels are unchanged;
no gallery regeneration is needed. No baseline, policy, investigation band, exact budget or allocation ceiling is
changed. All existing performance-policy, ARM64 opt-in resource, physical input/device-removal, real IME/AT/touch
and consumer acceptance gates remain open. Source-specific functional repair does not close shared qualification.
