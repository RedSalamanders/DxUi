# Final local SDK matrix — 25 October

The [reviewed matrix](summary.json) records clean SDK commit
`594d67fb0fcc3829541211a7c8f100df046c87e3`, tree `fa4193a259c9e2b087a21399ec13cde387202002`.
All six builds pass. Each x64 profile passes all 19 ordinary suites with zero capability skips; the
isolated ASan detection probe also passes. This is SDK evidence, separate from application qualification.

| Profile | Build | Ordinary native suites | Capability skips | Retained artifacts |
| --- | --- | ---: | ---: | ---: |
| x64 Debug | Passed | 19/19 | 0 | 12 |
| x64 Release | Passed | 19/19 | 0 | 12 |
| x64 ASan Debug | Passed | 19/19 | 0 | 13 |
| ARM64 Debug | Passed | Not run | Not evaluated | 12 |
| ARM64 Release | Passed | Not run | Not evaluated | 12 |
| ARM64 ASan Debug | Passed | Not run | Not evaluated | 13 |

Independent replay verifies all 74 retained artifacts, all 57 suite/executable bindings and all 288 completed
payload files (98 ASan, 95 Debug and 95 Release), with no hash mismatch. The ASan positive probe reports
detection with its expected nonzero exit; it is separate from ordinary suite outcomes. The corrected embedded
Tree notification fixture executes in all three x64 profiles, preserving first/final counts, ordinary deadlines,
held silence, per-ID coalescing and stale-ID pruning.

The JSON retains both matrix run IDs and digests, six build-receipt and artifact-manifest digests, all 30
executable digests, runner script digests, three native completion digests and the independent-audit digest.
Native evidence run references use SHA-256 of the exact private run ID plus its UTC timestamp; these references
are not altered official run IDs. Original receipts, binaries, logs and failed attempts remain private beneath
the owning `.build`. Publication does not execute a new test or retag any source-bound result.

The top-level workload/fixture input hashes identify the library-owned Complex UI preflight used by the native
runner. They match the six source files and the later same-code foreground report; they are fixture metadata,
not the complete source closure of all 19 suites or an additional performance comparison.

ARM64 rows here are cross-builds only. The [hosted native packet](../hosted-594d67fb/README.md) records separate
actual x64/ARM64 execution and its retention limits. Foreground menu/input,
performance-policy acceptance, physical/assistive-technology coverage and RedTilde product qualification retain
their own obligations. The historical clean-checkout observation belongs to the recorded source commit;
later documentation changes require an explicit unchanged-input/artifact check.
