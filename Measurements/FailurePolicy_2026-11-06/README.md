# API 5 failure-policy library qualification

Qualified implementation: `f6791b5070efd1fa8dc86601bc9422ae9a321d2e`, API 5, failure-host protocol 2.
This record covers the library, the application-owned example, clean-pin integration fixtures and x64 execution.
It does not qualify a consumer product, native ARM64 execution, physical input/assistive technology, or formal
frame-performance acceptance. The owner explicitly deferred native ARM64 execution and accepted the measured
safety overhead below on 6 November 2026 while requiring the performance limitations to remain explicit.

## Executed acceptance

| Check | Result |
| --- | --- |
| x64 and ARM64 Debug, Release and ASan Debug builds | Six passes; zero warnings, errors or tool-process exceptions. ARM64 is cross-build evidence. |
| x64 ordinary suites | 66 suite executions across three profiles; zero capability skips. |
| Menu, NativeTextInput, MenuResources, MenuResourceScaling | 12 suite executions across three profiles; zero skips. Every approved automatic-start lease used a 30-second countdown and verified desktop restoration. |
| Clean-pin consumers | Debug, Release and both ASan annotation variants pass. Each records ten rejected pin/build mismatches, 12 healthy module checks, two native-allowance checks and 16 fatal-context checks. |
| Actual ASan control lifetime | A real control read after destruction reports heap-use-after-free with allocation, free and read frames under both annotation policies; no competing sanitizer callback is installed. |
| Validators and formatting | Five root validators, complete tooling roster and clang-format 22.1.3 check pass. |
| SDK `0.1.202611065` | Extracted-only x64 Release smoke, producer-PDB isolation, missing-packaged-PDB and first-party-warning negative controls pass. Independent review verifies the library/PDB/ZIP identities, 348 unique headers confined to SDK/toolchain roots, and all eleven notices with original license bytes and offline links. |

The [matrix](native-matrix-independent-16.json), [ordinary](ordinary-profiles-16.json),
[foreground](foreground-profiles-17.json), [consumers](consumer-independent-16.json),
[ASan](control-asan.json), [tooling](tooling.json), [SDK](sdk-065.json) and
[license](independent-sdk-license-f679.json) summaries retain actual source and artifact identities.
Raw dumps, symbols, desktop identities and complete local receipts remain in the owned private evidence roots.

## Measured cost and owner decision

The unchanged construction fixture compared main `1ea6d52295294aaad05a19de6dc679484d640c88` with
`d975aa7dbc2cbad330b01a90935bcf96d5811f41`: 12 independent randomized ABBA/BAAB blocks, 48 processes,
100 warmup and 5,000 measured operations per control/process. Counting and timing use separate passes.
Both Release libraries were rebuilt with the same 25-file measurement overlay and strict evaluated-profile
compatibility. `f6791b50` changes only the proposed judge-review hash and prose after `d975aa7d`; library,
native test, fixture and project source remains identical. See [the source bridge](source-equivalence.json).

| Control | Median paired construction delta | Allocation calls, baseline/candidate | Requested bytes, baseline/candidate | Observed peak bytes, baseline/candidate |
| --- | ---: | ---: | ---: | ---: |
| Separator | +10.355 ns | 2 / 2 | 328 / 328 | 328 / 367 |
| Panel | +2.400 ns | 2 / 2 | 344 / 344 | 344 / 383 |
| RichTextEditor | +16.285 ns | 9 / 9 | 5,620 / 5,684 | 5,620 / 5,723 |

Every census ends at zero net live bytes. These are descriptive construction results and allocator-requested/native
block observations, not committed/resident memory, a causal attribution, or formal frame-performance clearance.
The [complete summary](construction-summary.json) preserves the definitions. CSV production used `fr-FR` numeric
formatting; this JSON contains parsed numeric values.

The owner chose **"Accept the measured safety overhead; keep the performance limitations explicit."** No threshold,
numeric policy, baseline or qualification flag was relaxed. The proposed judge-review hash follows its current
source bytes; the policy remains explicitly unqualified.

## Open performance and platform ownership

At this closeout snapshot, hosted pair [42201620823](https://github.com/RedSalamanders/DxUi/actions/runs/42201620823) is still
collecting main `1ea6d522` against implementation `a6ea0c15`. Later library C++ is unchanged; raw receipts retain
their original identities. No accepted verdict is claimed. Formal policy/control qualification remains
[D-P1](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md#d-p1).

The older [42192490717](https://github.com/RedSalamanders/DxUi/actions/runs/42192490717) is historical
`86c62873`/`946d5a28`, before the D1 integration. All 144 receipt hashes verify, but control drift and the unqualified
base policy make it inconclusive. Its retained summaries flag clean preparation from 0.0005 to 0.0008 ms;
both judges agree. These are unqualified findings, not causal regression or non-regression proof. The gate correctly
refuses early; its empty, unjudged scenario list must not be read as evidence of no flags. See
[the historical triage](historical-hosted-triage.json).

Native ARM64 execution, real product adoption and broader per-element admission remain separately owned in
[the remaining-work register](../../Specs/Plans/WIP/RemainingWork_2026-11-04.md).

## Retained failures

The first foreground attempt observed another application regain focus; the user confirmed desktop use.
The lease verified restoration, and later fixtures did not run. [Its failed disposition](failed-foreground16.json)
remains distinct from the successful fresh retry; no code cause is established or prior receipt relabelled.
The preceding stale judge-review-hash validator failure, PID-directory collision, incorrect throwing-WIL query
and earlier native/UIA failures retain their original logs/artifacts under the
[completed library plan](../../Specs/Plans/Done/FailurePolicy_2026-11-02.md).

Use [the migration guide](../../docs/upgrade.md) for host installation, checked results, full rebuilds and the
exact source pin. Changing a pin alone does not migrate an application.
