# Prepared Tree property-event qualification — 2026-10-06

## Qualified scope

Library functional and configuration qualification is complete for pushed source `36c0ff66bd7574038ed2e255f79847a81bf054c7` on `codex/p1-prepared-tree-accessibility`.

| Attempt | Target | Actual child result | Suite/helper result | Readback |
| --- | --- | --- | --- | --- |
| `matrix-r8` | x64 Debug tests | Exit 0; 2026-10-06 06:22:29.7339847–06:25:06.6731747 UTC | 20/20 suite receipts passed; original helper passed | 200 source files, 10 products, 43 diagnostics |
| `matrix-r9` | x64 Release tests | Exit 0; 2026-10-06 06:26:16.0753082–06:29:39.9961149 UTC | 20/20 suite receipts passed; original helper passed | 200 source files, 10 products, 43 diagnostics |
| `matrix-r10` | x64 ASan Debug tests | Exit 0; 2026-10-06 06:30:57.0031160–06:35:24.5389778 UTC | 20/20 actual suite receipts passed; actual heap-use-after-free probe detected; proof SHA-256 `0D11AFE456A1702C7841659815DAA612F30C676B9CD3A7C14081169B6BCAA141` | Independent supplement verified 200 source files, 10 products, 44 diagnostics. The original helper failed when another process held the ARM64 ASan build log. |
| `matrix-r11` | ARM64 Debug cross-build | Exit 0; 2026-10-06 06:35:00.1267014–06:35:06.6883231 UTC | Original helper failed while another process held the x64 ASan accessibility test log | Independent supplement verified 200 source files, 10 products, 1 diagnostic |
| `matrix-r12` | ARM64 Release cross-build | Exit 0; 2026-10-06 06:35:09.7846531–06:35:16.4281151 UTC | Helper passed; the controller rejected its singleton diagnostic as a bad diagnostic list | Independent supplement verified 200 source files, 10 products, 1 diagnostic |
| `matrix-r13` | ARM64 ASan Debug cross-build | Exit 0; 2026-10-06 06:35:19.5431898–06:35:26.4729744 UTC | Original helper passed | Independent supplement verified 200 source files, 10 products, 1 diagnostic |

The matrix establishes successful x64 test execution, real x64 ASan instrumentation and its intentional use-after-free detection probe, and successful ARM64 cross-builds. It does not establish native ARM64 runtime behavior. Interactive-desktop-only checks remain capability-skipped as recorded in the original suite receipts.

## Independent readback supplement

The fresh supplement at `.build/prepared-tree-proof/property-event-supplement-r1/` copies and verifies 908 files across the existing actual-child attempts r10–r13. Its copy-manifest SHA-256 is `EBB63A9DD1AFFB70304926BC908E66D39A49A29B1B83A9939AE505F6A54897EE`. Each attempt has 200 verified source copies and 10 verified product copies; verified diagnostic copies are 44, 1, 1 and 1 respectively. The exact per-attempt actual-child UTC intervals, exit codes, r10 suite receipts, original validation booleans and native-ARM-runtime disclaimers are retained in `result.json`; every copied file and readback result is recorded in `copy-manifest.json`.

Original outcomes remain visible: r10 and r11 child runs passed while their original helper checks failed because another process held a shared log; r12's helper passed but its controller rejected the singleton diagnostic list; r13 passed. The readback supplement verifies evidence for those existing child runs and does not replace or erase any failure.

## Remaining qualification

The paired Release common-scene result with three scene sets and zero confirmed regressions belongs to source `5f1ccdb6b5decde9f8ef6137e8b646dba51555c4`. Its existing qualification wording and `paired-summary.json` are preserved verbatim; that result does not establish paired non-regression for `36c0ff66bd7574038ed2e255f79847a81bf054c7`. The current source's matched paired qualification remains open.

At r208, exact-pin consumer interaction coverage was 18/18, but retry failed and the new fixtures had not executed.
The later r211 passes interaction/retry/edit and provider16/capacity12 before stalling during real embedded
notification publication. The captured dump proves inline delivery on the publishing STA and motivates the
separate structure-delivery draft. Shape, successful client re-entry, the full consumer matrix and large-row
cost qualification remain open. This historical property-provider evidence establishes no production activation.
