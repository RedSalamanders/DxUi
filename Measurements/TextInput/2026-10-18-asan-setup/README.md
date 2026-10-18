# ASan foreground setup diagnostics and repair

Status: setup repairs and requested local native checks complete. Qualification remains HOLD. The
[qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#asan-foreground-setup-follow-through-18-october-2026)
remains the single owner of acceptance and next steps. [Sanitized evidence](evidence.json) binds runtime inputs,
artifacts, receipts, diagnostic attempts and the performance comparison. Raw desktop records stay private.

The Menu witness now creates/shows without activation, publishes readiness or its immediate registration/creation
error, then waits for permission to activate. Its sent-message-only readiness wait preserves queued input/probes
and rejects publication after the original 5,000 ms absolute deadline. Owner acquisition follows client/witness
preparation. The background queue-focus state is established explicitly while the witness retains actual
foreground; intentional transfers preserve total loss history and have separate unexpected-loss counters.
A held behavioral failure never retries. Owned-worker and shared focus-client teardown wait for actual kernel
thread termination while servicing required callbacks, including COM cleanup.

Seven new WindowHost cases cover the blocked non-pumping counterfactual, sent-callback completion with queued
input preserved, absolute/late readiness, actual thread exit, expected-versus-foreign loss accounting, a missing
target without stale diagnostics, and fresh-client cache creation without prior view/element lookup. Three Menu
cases cover publication before activation, a real creation failure, and focus withdrawn during prepared setup.
The original negative and positive UIA event predicates remain. Fresh-client tests do not establish a clean Windows session.

| x64 configuration | Noninteractive suites | WindowHost cases | InteractiveLease cases | Full Menu | Full NativeTextInput |
| --- | ---: | ---: | ---: | ---: | ---: |
| Debug | 19 | 133 | 45 | 113 | 167 |
| Release | 19 | 133 | 45 | 113 | 167 |
| ASan Debug | 19 | 133 | 45 | 113 | 167 |

All 57 noninteractive and six full foreground suite executions pass with zero capability skips. All three
foreground leases use the approved 30-second automatic Start and verify foreground, focus and pointer:
Debug/ASan are unchanged; Release restores foreground/focus with the pointer unchanged. x64 and ARM64 build
in all three configurations; ARM64 receipts establish cross-buildability only. MenuResources and
MenuResourceScaling were not rerun in this slice. Repository checks use `validate.ps1` and `format.ps1 -Check`.

The retained diagnostics remain separate:

- The first Menu diagnostic passes both controlled witness cases, then fails UIA setup without stage attribution.
- The second publishes witness readiness successfully but its foreground request is refused; one skip fails qualification.
- The prepared diagnostic isolates `CreateCacheRequest` returning `E_FAIL` (`0x80004005`) after COM activation,
  timeout configuration and interface query succeed, before subscription or witness creation. A separate
  nonactivating COM probe and the fresh-client native regression pass. The initialization cause remains unresolved;
  no speculative warm-up, cache retry or longer timeout was added. Later passing full suites do not explain it.
- Focused ASan NativeTextInput passes all five selected cases. Its original undo scenario records a refused direct
  request followed by successful attachment/request/detachment and actual foreground/focus read-back. Final full
  runs pass too. The five historical refusals remain unreproduced and unattributed.

Optional scalar observations record existing direct/fallback calls, documented BOOL results, immediate attachment
errors and final read-back. They add no requests, pumping, retries or waits. The
[SetForegroundWindow contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
does not assign last-error semantics to its BOOL. Microsoft's
[foreground handshake explanation](https://devblogs.microsoft.com/oldnewthing/20161118-00/?p=94745/)
provides context; the controlled counterfactual proves the wait hazard without attributing the original timeout.

The single serialized Release comparison with the retained pre-implementation fixture is `advice-required`:
clean FPS -16.11%, frame p95 +16.01%, compose CPU p95 +9.66%, dirty private bytes +3.10% and private peak +3.08%.
These five unpaired flags remain recorded. Defer performance acceptance until stable controls and trusted paired
evidence support attribution; do not rebaseline or infer non-regression from the functional passes. Earlier nine
timing flags, older comparison flags, MultilineGrid memory uncertainty and retention limits remain attached to
their original evidence.

Private `Z:/DxUiSetup` retains preceding source, immutable baseline-image references, distinct diagnostic/final
images and raw receipts. The [preceding packet](../2026-10-18-fixture-readiness/README.md) preserves the original
failed full ASan leases. All 20 remaining checkbox gates, the 800 ms polling parameter, five-attempt policy,
readiness/UIA/hover/pointer/watchdog/lease limits, 5% timing/FPS and 2% memory bands, exact budgets,
allocation ceilings and `qualified=false` policy remain unchanged. Native ARM64/resource observation, physical
input/device removal, real IME/AT/touch, consumer acceptance and trusted paired qualification stay open.

No production library, API, consumer adapter or control pixels change. Documentation/gallery review requires
no visual regeneration; unrelated gallery and raw measurement files are preserved.
