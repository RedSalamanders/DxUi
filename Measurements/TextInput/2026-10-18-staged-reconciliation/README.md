# Staged TSF reconciliation count boundary, 18 October 2026

Owner: DxUi. Shared qualification is owned by
[ConsumerUiQualification](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).
The structured [evidence](evidence.json) binds the source, images, receipts and validation stages.
Raw logs, desktop records and build images remain private under `Z:/DxUiTsf`.

## Failure and demonstrated boundary defect

[PR #87 ARM64 Release](https://github.com/RedSalamanders/DxUi/actions/runs/39160627044/job/117773034720),
at `ee63697c6c91a139aded97daa95a2335181d3dcc`, passes Menu but fails NativeTextInput's exact
post-commit text/layout notification-count assertion. The preceding retained-text and drained-work assertions pass.
Its unchanged TSF test body and production implementation do not establish that the shared harness had no effect.
The original log contains no before/after notification counts, so its precise ordering remains unattributed.

A bounded instrumentation-only x64 Release run records one text and one layout notification, an unchanged session
cookie/activation count, retained foreground/focus and verified desktop restoration. It passes the original predicate.
A controlled queued geometry change demonstrates why that predicate cannot be evaluated after an arbitrary queue drain:

| Observation | Text notifications | Layout notifications |
| --- | --- | --- |
| Reconciliation alone | 0 to 1 | 0 to 1 |
| Following queued geometry, measured separately | 1 to 1 | 1 to 2 |
| Old whole-queue boundary with both operations | 0 to 1 | 0 to 2 |

The counterfactual explicitly requires the old predicate to be false while the legitimate geometry change runs.
This proves a count-boundary defect. It does not reconstruct the missing historical ARM64 counts.
The three focused probe assertions pass with zero skips, but that probe's lease fails cursor restoration.
That failed lease is retained separately and cannot qualify complete foreground validation or identify what moved the pointer.
Desktop interaction is confirmed for that countdown/run; it is context, not proof of the restoration failure's cause.

## Repair and diagnostics

The existing test now dispatches the current generation-tagged production reconciliation handler and checks its
exact one-text/one-layout predicate before pumping independent work. A second registered case keeps geometry queued
until after that boundary; a third retains the old whole-queue counterfactual. A duplicate record is checked after
the queue drains and must produce no text, selection or layout notification. The original whole-queue drained-work
check remains after the pump because a sink may legitimately request another deferred lock.

Two scalar diagnostic records per existing bounded foreground attempt retain counts, session/activation identity,
geometry, posted state and actual focus/foreground. They publish neither document text nor desktop identities and
add no waits, activation requests or retries. The ordinary assertion and existing reentrancy/lifetime predicates remain.

Final validation is recorded separately from the intermediate probe in the structured evidence. The six builds
and 57 affected noninteractive x64 executions pass with zero skips. All five candidate validators, all 18 tooling
groups and the pinned formatting check pass. The first `validate.ps1` run also reports unrelated local measurement
metadata/path failures and a missing bullet in this change's fragment; the fragment is corrected and the five
validators pass against the isolated candidate. The original validation log is retained rather than overwritten.
Cross-builds are not native ARM64 execution, and a successful repeat cannot attribute the original failure or
the failed cursor restoration.

Full NativeTextInput passes 169/169 cases in each x64 configuration (Debug, Release and ASan Debug), with zero
capability skips and verified foreground/focus/pointer restoration under the approved automatic-Start leases.
The final packet binds those runs to the frozen source and 74 archived image/symbol/library files. Full Menu and
the opt-in menu resource fixtures are not rerun locally in this follow-up; their earlier receipts keep their original
source/image identities, and the new PR head requires its own hosted Full validation.

The single retained Release comparison is `within-noise-budget`, with zero new flags under the unchanged 5% timing/FPS
and 2% memory bands. It is unpaired and establishes no non-regression or causal attribution. Earlier timing/memory
flags, including the five recorded with the original PR #87 setup slice, remain unresolved.

The pre-change measurement used the existing isolated checkout: its runtime bytes matched the published PR input,
while its Git label still named the earlier base with copied changes. The checkout was then aligned to the exact
published commit without changing those bytes. Both recorded Git labels/dirty states and the source/benchmark/toolchain
fingerprint comparisons remain explicit; neither alignment nor a matching fingerprint makes this a paired comparison.
The library-source, benchmark, toolchain and environment fingerprints match; the executable hashes differ.

## Acceptance remains open

All 20 qualification gates stay open. Native ARM64 observation, opt-in menu resources, physical input/device removal,
real IME/AT/touch, consumer pins/product acceptance and trusted paired performance are separate obligations.
The 900 ms hover guard, 180 ms pointer guard, other deadlines, attempt bounds, 5% timing/FPS and 2% memory bands,
exact budgets, allocation ceilings and `qualified=false` policy are unchanged. Earlier timing flags, MultilineGrid
memory uncertainty and retention limits remain. No unpaired measurement establishes non-regression.

This changes test observation and coverage only. Production library code, public APIs, consumer adapters, control
pixels and gallery examples are unchanged; the documentation/gallery review requires no visual regeneration.
