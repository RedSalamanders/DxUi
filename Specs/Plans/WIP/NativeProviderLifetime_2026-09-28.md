# Native accessibility provider lifetime

Status: **ACTIVE**. Owner: File Operations integration; independent generic library correction.
Baseline: qualified code `73ba9365726cce30299290ab9ae8afa065c4fe3d`, documentation closeout `3022541`.

## Evidence and scope

RedSalamander's new real queue-control journey fails after the first successful UIA reorder.
The original failure remains in consumer evidence `queue-controls-v12-20260928`; V13 adds
stage, resolved AutomationId, HRESULT and native-focus diagnostics without relaxing assertions.
Source review finds that ordinary native providers retain a tree path, whereas the existing
embedded snapshot guard also binds that path to the control's lifetime token. A path reused by
a rebuilt native tree must not give an old provider authority over its replacement. Runtime
attribution is confirmed by V13 run `20260927T224744Z-94348-ef87c5cf22024e3fa035042660351a0a`:
the same retained provider resolves AutomationId `FileOperations.Action.11.3.0` before reorder
and `FileOperations.Action.11.2.0` after rebuild. Both Invoke calls return S_OK; the latter
incorrectly reorders task 2. Native focus and foreground remain unchanged. The consumer retains
the full trace and exact frozen build receipt; no assertion is relaxed.

Reuse the existing weak lifetime token and snapshot ownership. No consumer operation policy,
new public capability, timer, cache, or new identity system belongs in this correction. Root
providers without a semantic control continue to represent their live host; semantic roots,
child and virtual-child providers retain their original owning control identity. Preserve
native-menu instance rules, embedded bridges,
thread marshaling, reentrant callbacks, full text, focus and provider teardown.

## Execution

- [x] Confirm the consumer failure stage; retain baseline output and exact compiled inputs.
- [x] Add a focused native regression for an old provider/text range after child/root replacement
  and path reuse; run it against unchanged library code before applying the production fix.
- [x] Extend the existing identity guard to the native path without changing stable-provider
  behavior or holding a snapshot mutex across application callbacks. Review queued actions and
  reentrant focus/invocation, including virtual rows owned by replaced controls.
- [ ] Run affected local suites, required x64 Debug/Release/ASan and all ARM64 configurations;
  retain native execution versus build-only/interactive-skip distinctions.
- [ ] Compare the retained baseline with the identical resource fixture; retain every flag.
- [ ] Update domain contracts/docs, run validators/format and qualify the exact published source.
- [ ] Explicitly adopt the corrected pin in RedSalamander and rerun the failing real control
  journey. Keep consumer I26/H4 and other owners' gates separate.
- [ ] Move this plan to Done after its own required gates and handoff pass.

The preceding Grid/Menu plans remain completed records of their scoped work and receipts.
They do not waive this newly exposed generic provider-lifetime correction. No implementation
pass is inferred from a source-supported hypothesis or an unrelated successful Menu suite.

## Execution checkpoint

September 28: tests-only Release build succeeds; the first new regression fails on unchanged
73ba code with every stale operation returning S_OK and the replacement callback running.
Original logs/source/binary hashes are retained externally in
`native-uia-lifetime-red-20260928`. The first candidate misses native token publication and
rejects a live provider; keep that failed result in `native-uia-lifetime-candidate-v1-20260928`.
Root finds and removes the embedded-only publication condition. Candidate v2 passes the full
Accessibility suite, including child/root replacement, runtime IDs, fresh canonical acquisition,
and queued stale Invoke/Select. All native semantic records now publish existing control tokens;
there is no new token allocation or record field. Weak references retain existing token storage
until snapshots/providers retire, and guarded calls add lookup/refcount work.

Fresh pre-change x64 Release/Debug/ASan performance reports remain in
`native-uia-lifetime-baseline-20260928`. The first Release comparison flags private memory;
two further candidates retain an advice-required result and a within-noise-budget result.
No baseline is replaced and no uniform performance pass or confirmed causal degradation is
inferred. Correctness runs omit the comparator argument solely to execute suites independently
of that resource gate; their unpaired measurements do not qualify non-regression. Full paired
resource reconciliation remains required. Local Release's 18 nonactivating suites pass; other
profiles are in progress. Menu/native-text foreground coverage and ARM gates remain separate.

Root/agent review also identifies a focus-callback reentry hazard in the existing native Invoke
and focus path. A focused regression/fix is being prepared; the final-callback replacement test
does not establish safety when focus itself destroys the original control.

The focused Debug regression then reproduces false S_OK after focus replaces the tree.
Root reviews/applies existing-token checks around Button focus callbacks and guarded native
UIA re-resolution before continuing. Both real raw-provider focus/Invoke rounds now reject the
retired receiver, invoke no old/replacement action, and preserve the callback-selected successor.
The complete Accessibility and Tooltip suites pass on this candidate. The noactivation witness
does not claim to execute Win32 activation callbacks; existing foreground/CI gates remain.

Debug's initial broad run has 17 passes and a Tooltip failure. Instrumentation proves that a
requested 50ms pump actually takes 267ms, exceeding the 100ms deadline (it can also pass before
the delayed timer message is dispatched). Root replaces that invalid wall-time assumption with
explicit before/after deadline ticks using the existing seam, plus an independent bounded real
attached-host timer-expiry check. No production tooltip timing changes. Keep the initial failure
and diagnostic pass in `native-uia-lifetime-correctness-20260928` / `native-uia-focus-red-20260928`.
The final x64 Debug, Release and ASan runs each pass all 18 requested suites (54 passes).
ARM64 initially stops before compilation because this checkout has no ARM dependencies;
the supported restore succeeds and all three cross-builds continue serially. Keep both the
original preflight failure and restored build logs under `native-uia-final-matrix-20260928`,
whose source hashes freeze the four code/test
inputs. Skills/specs/dependency/format validators pass. This is a nonvisual lifetime correction:
control pixels/layout/themes and gallery examples are unchanged; usage and normative accessibility
docs are updated, so no gallery republishing is needed for this patch.
