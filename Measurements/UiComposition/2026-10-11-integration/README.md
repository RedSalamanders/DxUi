# S0-S2 current-main integration, 11 October 2026

This packet records the current-main integration of intrinsic sizing, Flex layout and stable forms. The developer
requested implementation integration with S3-S7 and unfinished qualification retained in WIP. It supplies focused
functional evidence; performance/policy, native ARM64 foreground, physical input/device and consumer adoption remain
separate acceptance gates under the [qualification plan](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md).

## Final ARM64 ASan budget candidate: PENDING

The final workflow/test/digest candidate has not yet been committed: **source commit PENDING**. Its exact workflow,
coverage manifest and digest, compiled-input identity, library/build fingerprints and before/after provenance are
also PENDING. Historical results at `061748aa` and `284a9ec6` remain evidence for those source identities only.
Root will rebuild and revalidate the final committed candidate before its results replace these pending entries.

The proposed CI repair raises only the ARM64 ASan enclosing job budget from 40 to 60 minutes after the retained
job exhausted its 40-minute bound. Existing step caps and correctness, sanitizer, provenance, deterministic-resource
and qualification checks remain required. The failed job, its actual workflow/source identity and timing must be
retained separately; successful completion of the revised job remains PENDING. A larger enclosing job budget is
neither a passed check nor performance-policy or degradation acceptance.

| Final budget-candidate profile | Focused correctness | Foreground suites |
| --- | --- | --- |
| x64 Debug | PENDING: final-source build, 14 form/quiet cases and 6 real-client embedded cases in `local/budget-debug/` | PENDING: four full suites, native skips, start mode and restoration in `local/budget-focus-debug/` |
| x64 Release | PENDING: final-source build, 14 form/quiet cases and 6 real-client embedded cases in `local/budget-release/` | PENDING: four full suites, native skips, start mode and restoration in `local/budget-focus-release/` |
| x64 ASan Debug | PENDING: final-source build, 14 form/quiet cases, 6 real-client embedded cases and deliberate sanitizer detection in `local/budget-asan/` | PENDING: four full suites, native skips, start mode and restoration in `local/budget-focus-asan/` |

These final-budget folder names are planned archival destinations; all contents and outcomes remain PENDING.

Final-source validation/format/tooling results, synthetic-consumer outcomes, required ARM64 builds and exact-head
PR/merge-tree CI remain PENDING until their own scoped evidence is reviewed. Every completed local packet must bind
actual source, dependency, compiler/SDK, artifact and environment identities before/after execution. The final-source
compiled-inputs-before receipt, archival comparison, copied-original receipts and updated packet seal are PENDING;
root will supply their exact paths and actual values. The original broader qualification remains historical at its
own source and does not establish final-candidate acceptance.

## Historical formatted run at 061748aa

The formatted native source is `061748aab88c8db0b623e8a244ba884d9193b933`, incorporating main
`ec680e866a218b57000a1780018c00101a437f91`. Its format-only test-source change joins the forced IME fixture's
`Require` guard onto one line. It precedes the later workflow/test/digest repair and qualifies its own recorded
source and artifact identities.

| Historical formatted profile | Focused correctness | Foreground suites |
| --- | --- | --- |
| x64 Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips in `local/formatted-debug/` | All four full suites pass; zero skips; restoration verified in `local/formatted-focus-debug/` |
| x64 Release | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips in `local/formatted-release/` | All four full suites pass; zero skips; restoration verified in `local/formatted-focus-release/` |
| x64 ASan Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips; deliberate sanitizer detection passes in `local/formatted-asan/` | All four full suites pass; zero skips; restoration verified in `local/formatted-focus-asan/` |

All three confirmations are `autostarted`: foreground/focus remain unchanged and the pointer is restored.
Both original and forced IME undo cases complete inside every full NativeTextInput suite. Actual before/after and
post-focus identities match; the lead independently rehashed the current executables and checked the raw completion,
skip, delivery and restoration witnesses. The library fingerprint remains
`D1C3380BFDDAA3A1BD37534C666FF24A29C5936A27185DBFA4D26EE8B0D11A46`.

`formatted-inputs-before.receipt.txt` binds compiled-input identity
`1b1630a8538fc33777c6d4d4805c57db1829462b15e17c97367a2cfd2a6727df`.
`formatted-archival-inputs/` compares all 24 actual compiler/SDK, dependency, artifact, harness and environment
fields across the formatted evidence archival, allowing only source commit/dirty bookkeeping to differ. This proof
precedes the CI-budget edit and does not bridge that subsequent workflow/test/digest change. The earlier
`284a9ec6` archival comparison remains separate.

## Historical pre-format candidate

The three-profile results below bind native source `284a9ec60cc767cad16775e4131b597ff1101b23`. Its retained library
fingerprint is `D1C3380BFDDAA3A1BD37534C666FF24A29C5936A27185DBFA4D26EE8B0D11A46`.
`local/final-debug/`, `local/final-release/` and `local/final-asan/` retain its focused cases and build receipts;
`local/focus-debug/`, `local/focus-release/` and `local/focus-asan/` retain its four-suite foreground runs.
Those folder names retain their original historical identity. They are not the formatted-source refresh.

| Historical local profile | Focused correctness | Foreground suites |
| --- | --- | --- |
| x64 Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips | All four full suites pass; zero skips; restoration verified |
| x64 Release | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips | All four full suites pass; zero skips; restoration verified |
| x64 ASan Debug | All 14 form/quiet cases and 6 real-client embedded cases pass; zero skips; expected-failure sanitizer detection passes | Menu, NativeTextInput, MenuResources and MenuResourceScaling pass; zero skips; restoration verified |

The foreground warnings use the approved 30-second automatic-Start mode. Raw native results distinguish clicked
`started` from no-click `autostarted`, and record foreground, keyboard focus and pointer restoration. Filtered tests
and these four suites do not establish a full repository pass. All three historical `284a9ec6` confirmations were `autostarted`;
each corresponding lease restored the pointer and left foreground/focus unchanged. Earlier `before-retry-focus-*`
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
Temporary worker stderr tracing and the further 15-second observation were removed before the historical
`284a9ec6` and `061748aa` runs.
The production library sources used by historical `284a9ec6` and `061748aa` match `35041fc5`; bounded owner/client
failure diagnostics remain. The final budget candidate requires its own pending source/provenance review above.

The earlier `before-retry-*` packets bind native source `c2d05357`: all 20 selected cases pass in three profiles,
and the Debug/Release foreground suites pass. Two ASan foreground attempts fail the IME undo setup assertion;
the same-binary isolated repeat passes. These are historical results and do not qualify the final test bytes.
`input-activation-timeout` records an admission timeout before any character was typed. It does not establish the
cause of the Windows foreground observation. `input-repeat-edit` then retains a real first attempt that typed `a`
and lost foreground; the retry appended another `a`. The field retained the discarded text and undo history.

`input-forced-negative` at `1341f494` forces one loss on a `WS_EX_NOACTIVATE` window and fails the original setup
assertion after exactly two attempts (`a`, then `aa`). `input-invalid-guard` preserves an earlier incorrect guard
failure; it is not regression evidence. The behavioral repair differs from the corrected negative by one reset,
`SetText(L"")`, after successful admission and before same-target focus. The shared original composition notification,
two-undo and late-result assertions remain; the forced case also checks that a third undo cannot expose discarded
history or notify again. This synthetic fixture still requests native thread keyboard focus and remains under
the restoring NativeTextInput lease. `input-forced-positive` passes at historical source `284a9ec6`: the second attempt
enters with the discarded `a`, resets it, and records lengths `1/1/1`; original composition/two-undo and empty
third-undo assertions complete. Both corrected negative and positive bind actual pre/post identities, zero skips,
approved no-click automatic start and verified foreground/focus/pointer restoration. This does not qualify physical
IME. At `284a9ec6`, the same forced case and original late-result case also complete inside all three full
NativeTextInput suites. Those historical suites have zero native skips and verified restoration, alongside each
profile's other three foreground suites. Later `061748aa` results remain historical as described above; the final
budget candidate remains PENDING.

## Evidence layout and limits

`local/` keeps original logs/receipts, actual pre/post identities and the host-specific PowerShell drivers.
`local/copy.receipt.txt` binds each copied original by length and SHA-256; `SHA256SUMS` seals the published packet.
The historical `local/final-*` profile folders retain all 14 selected form/quiet tests, the six-case embedded CLI and
build receipts at `284a9ec6`. Historical `local/focus-*` folders retain canonical four-suite plans, native lease
results, all suite/log pairs and restoration proof at that same source. `compiled-inputs-before.receipt.txt` and
`archival-inputs/` describe that earlier prose/evidence archival only; they do not bridge the subsequent test-source
format edit. `local/formatted-<profile>/` and `local/formatted-focus-<profile>/` belong to the later historical
`061748aa` run, with its own formatted compiled-input/archival proof. Pending entries above retain incomplete
collection and review explicitly. `local/budget-debug/`, `local/budget-release/`, `local/budget-asan/` and the
matching `local/budget-focus-<profile>/` folders are planned for the final budget candidate; their results and
separate provenance proof remain PENDING until bound to its actual future commit. Every added raw receipt belongs
in the copied-original receipt and packet
seal before closeout. Archived production inputs are reproduction material excluded from supported targets.

Current-source CI results must be inspected at the PR's exact head, base and merge tree. Enabled/pending jobs are
pending evidence; unqualified policy findings and verified ARM64 refusals before input remain incomplete/failed
qualification. This packet never supplies a successful aggregate `ci-gate` or accepts measured degradation.
The [dated integration decision](../../../Specs/Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage)
preserves deterministic resource limits, sanitizer/correctness obligations and ordinary release acceptance.





