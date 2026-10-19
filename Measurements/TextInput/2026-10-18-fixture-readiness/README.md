# Native-input fixture readiness

This DxUi-owned packet follows the two failed local NativeTextInput executions in the
[capture-reason packet](../../InteractiveLease/2026-10-18-capture-reasons/README.md).
The [qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#native-input-fixture-readiness-follow-through-18-october-2026)
owns acceptance and the remaining gates. [evidence.json](evidence.json) binds the source, images and receipts.
The fixture repairs and controlled regressions pass; full ASan foreground qualification remains open.

The Release failure occurred before the read-only/masked transition at the preview assertion. Its original
receipt lacks observations that could establish the desktop event or subcase. The fixture guarded activation
but asserted the later IMM sequence outside that guard. The repaired fixture guards activation, composition
START, preview delivery and the policy transition together, with fresh state after each discarded attempt.
Every original preview, cancellation, notification, policy, composition and TSF assertion remains mandatory.
Controlled regressions withdraw focus before and after preview for both policies and inspect the held attempt.

The ASan Debug failure has direct setup evidence: activation returned false at foreground transfer, no
character was typed, zero loss events were counted, and the guard nevertheless reported a held sequence.
Explicit activation outcome now distinguishes refusal from a sequence that held the foreground. Refusal
consumes one of the existing five attempts; exhaustion fails setup rather than producing a capability skip.
A ready held sequence is never repeated because a correctness predicate failed. The undo regression forces
initial refusal without a loss event, then retains the original independent edit, composition and two undos.

Passive IMM observations are limited to five attempts and four recorded stages per transition: after setup,
after START, after preview delivery and after the policy change. Unexecuted stages produce no observation.
Rows contain booleans, text length/equality, notification counts and existing foreground-loss counts. They
contain no document text or new desktop identities. Observation adds no activation, pump or timeout extension.

The private `Z:/DxUiInput` packet preserves the preceding source, all six runtime images and DLLs, original
logs/receipts, the pre-implementation Release ComplexUi baseline and counterfactual source/images. The
counterfactual initial-activation guard fails the new deterministic regression; it accepts a refused initial
activation as a held sequence. Raw desktop/process identities remain private. Historical failed executions
are retained separately from repaired-source results, and an isolated pass never replaces a failed full run.

| Check | Result |
| --- | --- |
| x64 Debug/Release/ASan Debug and ARM64 Debug/Release/ASan Debug builds | All six pass; ARM64 is cross-build evidence only. |
| Noninteractive affected suites | 19 per x64 profile, 57 passing executions, zero skips; three pure guard regressions and 45 InteractiveLease cases per profile. |
| Full Menu and NativeTextInput, x64 Debug and Release | All four executions pass: 110 Menu and 167 NativeTextInput cases per profile, zero skips, verified restoration. |
| Focused Release | Five original/controlled cases pass with zero skips and verified restoration. |
| Controlled ASan Debug | Three nonactivating NativeTextInput cases pass through the lease, including both new native regressions, with zero skips and verified restoration. |
| Full ASan Debug, first lease | Menu completes 110 cases with one activation skip. NativeTextInput completes 83 cases, then fails the masked eligibility preview. Desktop restoration verifies. |
| Full ASan Debug, one unchanged-input countercheck | Menu fails the cross-thread-window setup; NativeTextInput completes 104 cases, then fails initial activation after five refusals. Desktop restoration verifies. |

The first repaired-source ASan failure's trace records six characters and one notification **before START**
in the masked subcase, with activation/focus/native session true and no foreground loss. Preview then contains
eleven characters rather than the expected ten. The person at the desktop confirmed interacting during that
run. That is consistent with the unexpected edit, but the specific input was not traced. The ready failure is
not retried by the guard. The countercheck uses identical source and executable hashes and retains five explicit
`foreground-transfer` refusals, no `OnChar`, no edit and `sequenceHeld=false`: the repaired guard correctly
fails setup at the unchanged bound. Neither failure is a sanitizer memory finding.

The unmodified Menu countercheck fails `create a window on another thread`. Its assertion combines the
existing 5,000 ms readiness wait with a non-null HWND check; it does not distinguish timeout from a null
creation result, or record registration/creation/activation stage and error. Its cause remains unqualified.
No further full retry was run. Controlled passes establish the deterministic repair, not full ASan foreground
acceptance or the cause of either failed lease.

One serialized Release ComplexUi comparison against the retained pre-implementation baseline reports
`advice-required`: nine timing/FPS flags, clean FPS 2168.03 to 1946.12 (**-10.24%**) and dirty FPS 566.87
to 520.90 (**-8.11%**). No process-memory, exact-budget or allocation metric flags. Library source,
benchmark and toolchain fingerprints match; executable hashes differ and both images are retained. This
Embedded benchmark does not execute the fixture helpers, so it cannot attribute their overhead or establish
change-induced degradation. The unpaired sample and earlier timing/memory flags remain retained; no repeat
or rebaseline replaces them. Performance acceptance stays on HOLD pending stable matched controls and trusted
paired qualification, with profiling-backed optimization, scope reduction or deferral advice if degradation
is confirmed.

Next: obtain a separately identified full ASan lease with the desktop left available; retain these failures.
For persistent setup refusal, add bounded activation-stage/admission observations without changing ownership
or limits. For the Menu setup failure, distinguish registration, creation, activation and readiness publication
with immediate Windows errors under the same 5,000 ms bound before choosing a repair. Native ARM64 capture
attribution still requires the separately identified hosted diagnostic in the owning ledger.

Final repository checks use `validate.ps1` and `format.ps1 -Check` on the isolated checkout with exact task
inputs. The private packet retains their logs; unrelated gallery and raw-measurement work is hash-guarded.

The 800 ms activation limit, five-attempt bound, 900 ms hover guard, 180 ms pointer bound, watchdog/lease and
30-second automatic Start limits remain unchanged, as do 5% timing/FPS, 2% memory and all exact budgets and
allocation ceilings. `qualified=false` and all remaining qualification gates stay open. Cross-builds cannot
establish native ARM64 behavior; real IME/AT/touch, physical-input/device-removal, opt-in resources, consumers
and trusted paired-performance acceptance remain separate.

This fixture-only repair changes no library API or control pixels. Documentation and the validation contract
are updated; gallery regeneration is unnecessary.
