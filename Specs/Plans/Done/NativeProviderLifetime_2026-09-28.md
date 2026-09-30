# Native accessibility provider lifetime

Status: **DONE — 2026-09-30**. Owner: File Operations integration; independent generic library correction.
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
- [x] Run affected local suites, required x64 Debug/Release/ASan and all ARM64 configurations;
  retain native execution versus build-only/interactive-skip distinctions.
- [x] Compare the retained baseline with the identical resource fixture; retain every flag.
- [x] Update domain contracts/docs, run validators/format and qualify the exact published source.
- [x] Explicitly adopt the corrected pin in RedSalamander and rerun the failing real control
  journey. Keep consumer I26/H4 and other owners' gates separate.
- [x] Move this plan to Done after its own required gates and handoff pass.

The preceding Grid/Menu plans remain completed records of their scoped work and receipts.
They do not waive this newly exposed generic provider-lifetime correction. No implementation
pass is inferred from a source-supported hypothesis or an unrelated successful Menu suite.

## Execution checkpoint

September 30 closeout: primary RedSalamander explicitly selects the published and qualified
`49c963f5751a6754e6b08e0362b96cfc7fc64414` lock, replacing `73ba936...`. Its isolated
consumer already passes the originally failing real queue-provider journey and Skip on this
exact pin: `queue-controls-v14-20260928/qualification.json`, build receipt
`e53f1fa569709a6bbd733500b19b0d9f81bf8d23ca10acfe3f97e0915d96421f`, runs
`20260927T232901Z-105272-6abfec6f33c94dd3b834ce63e0d40ac6` and
`20260927T232939Z-105272-62e82f3ed41b4e5aacd50dbc85b3f8c9`. Source and binary hashes
are retained with those consumer receipts. This is explicit development-branch adoption;
consumer full product/visual/resource I26 and H4 gates remain open in their own repository.
Library skills/spec/dependency validators pass in `native-uia-closeout-validation-v2-20260930`.
The initial incorrectly splatted format invocation failed; the corrected explicit
`format.ps1 -Check` succeeds with clang-format 22.1.3 (`format-corrected.log`). Original
failures remain. Packet verification passes for both measurement archives. No pixels changed
in this library correction, so documentation review requires no new gallery image.

September 30 final resource review: the matched 1,000-frame A-B-B-A driver and all
original-header restoration/rebuild checks complete. The verified supplement retains 475
logical files / 348 payloads. The [qualification review](../../../Measurements/NativeProviderLifetime/Qualification_2026-09-30.md)
reconciles the exact49 native CI, native-menu heap samples, original short series and all
long comparator flags. Eight of 12 long comparisons remain advice-required; no blanket
automated performance pass is claimed. Opposite short-run private-memory direction and
same-binary variation prevent causal attribution of the smaller long-run increase; allocation
and surface counters match exactly. The reviewed comparison gate is complete with those
explicit limits. Primary-consumer adoption and final validators/index handoff remain pending.

September 30: exact-source CI [36358394952](https://github.com/RedSalamanders/DxUi/actions/runs/36358394952)
completes successfully on `49c963f5751a6754e6b08e0362b96cfc7fc64414`. Root verifies all 122
successful suite receipts, eight external-consumer receipts and two expected positive ASan detections
in `C:/RedSalamander.Perf/evidence/i26-ui/native-uia-ci49-20260928`. Each of the three ARM64 Menu
receipts explicitly skips nine desktop-dependent cases; every other suite receipt has an empty skip
list. These are native platform results with those specific limitations, not a claim that skipped
desktop paths ran. The historical local 54-suite and ARM64 cross-build receipts remain unchanged.

The unchanged73/current49 short A-B-B-A crossover retains all comparisons: same-binary repetitions
also cross investigation bands. A reviewed external matched 1,000-frame crossover is in progress
under `native-uia-long-crossover-20260930`; its original-header restoration and full rebuilds are
part of the driver. This is additional offscreen evidence, not a replacement baseline, a native UIA
cost measurement or permission to dismiss flagged metrics. Performance reconciliation, evidence
packet extension and explicit primary-consumer adoption remain pending.

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

The three restored ARM64 cross-builds pass. Code is pushed as
`49c963f5751a6754e6b08e0362b96cfc7fc64414`; exact-source native CI run `36358394952` is pending.
Root verifies and retains the [portable evidence packet](../../../Measurements/NativeProviderLifetime/2026-09-28/README.md)
with 159 original paths / 141 byte payloads, including all 54 fresh suite receipts, original
red/focus-red logs, baseline/repeats and ARM restoration/builds. The matched ordinary-menu
fixture has unchanged source, geometry and 12/128 accessible children; Release/Debug open
heap means decrease slightly. ASan heap-walker zeros remain uninformative. A one-handle
residual in some 12-row samples remains recorded; it is not an identified provider leak.
The independent common-UI comparator flags Release and Debug, with ASan within noise.
That offscreen fixture never attaches UIA; previous same-binary Release repetitions vary.
Keep every flag pending confirmation instead of attributing or dismissing it automatically.

Isolated RedSalamander V14 explicitly tests 49c963f while primary remains on 73ba. The original
actual queue-reorder/stale-provider journey now passes: run
`20260927T232901Z-105272-6abfec6f33c94dd3b834ce63e0d40ac6`, 3/0/0 including setup/cleanup, zero
disk findings. Independent Skip journey also passes 3/0/0 in
`20260927T232939Z-105272-62e82f3ed41b4e5aacd50dbc85b3f8c9`. The full Release build receipt is
`e53f1fa569709a6bbd733500b19b0d9f81bf8d23ca10acfe3f97e0915d96421f`; its four test-only signed
conversion warnings are retained and corrected separately in primary, not hidden in the receipt.
This verifies the corrected consumer route, not completed product adoption or I26 closeout.
