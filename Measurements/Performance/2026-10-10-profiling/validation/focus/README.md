# Fresh foreground validation

All attempts remain separate. The four directories below retain commit `5763d57e` and its original native bytes;
they precede the bounded setup-diagnostic test change. No later retry erases a failure or qualifies different bytes.

| Attempt | Complete scope results | Desktop restoration |
| --- | --- | --- |
| [Debug attempt 2](Debug-attempt2-failed/README.md) | Menu passes; NativeTextInput setup fails; resource execution unvalidated | Foreground/focus gone; pointer failed |
| [Debug attempt 3](Debug-attempt3-pass/README.md) | Four complete scopes pass, zero skips | Foreground/focus unchanged; pointer restored |
| [Release attempt 1](Release-attempt1-failed/README.md) | Menu popup discovery fails; three other scopes pass, zero skips | Foreground/focus unchanged; pointer restored |
| [ASan Debug attempt 1](ASanDebug-attempt1-failed/README.md) | Menu passes; NativeTextInput setup fails; resource execution unvalidated | Foreground failed; focus skipped; pointer failed |

The earlier [Debug confirmation timeout](../focus-Debug-attempt1/interactive-result.txt) launched no child and
remains canceled evidence. The resource launch refusal can follow OS child creation/resume before the foreground
grant; it proves no validated foreground execution, not absence of process creation. A later available desktop
check does not prove successful foreground or pointer writes at the failure time.

The repeated NativeTextInput assertion occurs before IME preview or resizing. It combines activation, actual HWND
focus and retained-session state. Neither its failure nor the later grant/restoration failure proves a library defect
or user interference. The Release failure precedes shadow-margin dismissal and means no matching visible popup was
observed by the bounded search; no lifecycle trace establishes whether a popup HWND ever existed.

The new test diagnostics retain at most five attempts and four passive observations per attempt: activation terminal
branch, foreground/focus/active HWND identities and process/thread IDs, host/client dimensions, visibility, logical
and acknowledged focus, session/TSF state and existing counters. Observations are sequential, not atomic or a Windows
message history. Each attempt resets its success flag. Assertions, activation order, timeout and retry policy remain
intact; diagnostics supply no performance, product or restoration acceptance. The corrected binaries pass
[six builds and 57 noninteractive executions](setup-runs/README.md), zero skips, with complete actual provenance.
Their fresh ASan Start confirmation times out before any child runs and verifies all three desktop states unchanged.
Fresh Debug/Release/ASan foreground execution remains open; the setup observations have not yet run.

Every archive-copy receipt maps immutable external snapshot paths, lengths and SHA-256 values to retained bytes.
The historical reviewer helper is retained as `snapshot-reviewer.source.receipt.txt`, not as a portable runnable tool.
Its hash matches the original successful/failed review receipts that used it. Parent benchmarks are unpaired
observations, including the slow Release report. Keep them separate from the twelve-report diagnostic pilot.
