# S0-S2 current-main integration, 11 October 2026

This packet records the current-main integration of intrinsic sizing, Flex layout and stable forms. The developer
requested implementation integration with S3-S7 and unfinished qualification retained in WIP. It supplies focused
functional evidence; performance/policy, native ARM64 foreground, physical input/device and consumer adoption remain
separate acceptance gates under the [qualification plan](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).

## Frozen local candidate

The final native source is `284a9ec60cc767cad16775e4131b597ff1101b23`, incorporating main
`ec680e866a218b57000a1780018c00101a437f91`. Its library fingerprint is
`D1C3380BFDDAA3A1BD37534C666FF24A29C5936A27185DBFA4D26EE8B0D11A46`.
Each local packet binds actual source, dependency, compiler/SDK, artifact and environment identities before/after
execution. Documentation/evidence archival does not change compiled inputs; its content-identity proof is retained
separately. The original broader qualification remains historical at its own source identity.

| Local profile | Focused correctness | Foreground suites |
| --- | --- | --- |
| x64 Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips | All four full suites pass; zero skips; restoration verified |
| x64 Release | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips | All four full suites pass; zero skips; restoration verified |
| x64 ASan Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips; expected-failure sanitizer detection passes | Menu, NativeTextInput, MenuResources and MenuResourceScaling pass; zero skips; restoration verified |

The foreground warnings use the approved 30-second automatic-Start mode. Raw native results distinguish clicked
`started` from no-click `autostarted`, and record foreground, keyboard focus and pointer restoration. Filtered tests
and these four suites do not establish a full repository pass. All three final confirmations were `autostarted`;
each final lease restored the pointer and left foreground/focus unchanged. Earlier `before-retry-focus-*`
confirmations were clicked `started` and remain historical at their own identities.
Canonical complex-UI reports from `test.ps1` are unpaired observations, with no performance-acceptance inference.

## Integration fixes and retained failures

Embedded label notifications now marshal sender and old/new interface values through the Global Interface Table.
Weak event peers do not retain intermediate prepared sources; coalescing keeps the earliest old value and latest
new value. Real-client regressions cover payload readability, owner reentry, explicit pending-batch coalescing,
source retirement, clear/replacement and drained teardown. Duplicate valid first/final callbacks remain permitted;
intermediate values do not.

The `negative` packet replaces only production Accessibility.cpp with the integrated pre-fix bytes at `a0269ddd`
while retaining the then-identical new test harness. Its expected failure proves retained intermediate sources.
It does not reproduce a wrong-apartment failure. `first-positive` and `payload-probe` preserve the earlier strict
duplicate-count failures; `gated-positive` proves six embedded cases pass after deterministic gating, then retains
the separate form deadline failure. Its harness predates the final wait change and is historical evidence.

The form investigation preserves the original three-second failures and progressively bounded diagnostics.
`form-late` observes all four properties arriving twice by about 9.2 seconds, with callbacks completing in the
same clock tick; the initial deadline still fails. `form-wake` changes only the shared test owner's sleep to a
bounded message-aware wait, retaining the client class, sender cache, subscription, UIA timeouts and assertions.
The first batch completes in 78 ms, its reset in 94 ms, and the original idempotence test passes in 2.474 seconds.
These GetTickCount64 observations are a causal harness comparison, without a general throughput claim.
Temporary worker stderr tracing and the further 15-second observation are removed from the final candidate.
The production source matches `35041fc5`; bounded owner/client failure diagnostics remain.

The earlier `before-retry-*` packets bind native source `c2d05357`: all 20 selected cases pass in three profiles,
and the Debug/Release foreground suites pass. Two ASan foreground attempts fail the IME undo setup assertion;
the same-binary isolated repeat passes. These are historical results and do not qualify the final test bytes.
`input-activation-timeout` records an admission timeout before any character was typed. It does not establish the
cause of the Windows foreground observation. `input-repeat-edit` then retains a real first attempt that typed `a`
and lost foreground; the retry appended another `a`. The field retained the discarded text and undo history.

`input-forced-negative` at `1341f494` forces one loss on a `WS_EX_NOACTIVATE` window and fails the original setup
assertion after exactly two attempts (`a`, then `aa`). `input-invalid-guard` preserves an earlier incorrect guard
failure; it is not regression evidence. The final fixture differs from the corrected negative by one reset,
`SetText(L"")`, after successful admission and before same-target focus. The shared original composition notification,
two-undo and late-result assertions remain; the forced case also checks that a third undo cannot expose discarded
history or notify again. This synthetic fixture still requests native thread keyboard focus and remains under
the restoring NativeTextInput lease. `input-forced-positive` passes at the frozen final source: the second attempt
enters with the discarded `a`, resets it, and records lengths `1/1/1`; original composition/two-undo and empty
third-undo assertions complete. Both corrected negative and positive bind actual pre/post identities, zero skips,
approved no-click automatic start and verified foreground/focus/pointer restoration. This does not qualify physical
IME. The same final case and original late-result case also complete inside all three fresh full NativeTextInput
suites. Those suites have zero native skips and verified restoration, alongside each profile's other three
foreground suites.

## Evidence layout and limits

`local/` keeps original logs/receipts, actual pre/post identities and the host-specific PowerShell drivers.
`local/copy.receipt.txt` binds each copied original by length and SHA-256; `SHA256SUMS` seals the published packet.
The final profile folders retain all 14 selected form/quiet tests, the six-case embedded CLI and build receipts.
The focus folders retain canonical four-suite plans, native lease results, all suite/log pairs and restoration proof.
The archived production inputs are reproduction material and are excluded from supported targets.

Current-source CI results must be inspected at the PR's exact head, base and merge tree. Enabled/pending jobs are
pending evidence; unqualified policy findings and verified ARM64 refusals before input remain incomplete/failed
qualification. This packet never supplies a successful aggregate `ci-gate` or accepts measured degradation.
The [dated integration decision](../../../Specs/Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage)
preserves deterministic resource limits, sanitizer/correctness obligations and ordinary release acceptance.
