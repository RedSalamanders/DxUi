# Reviewed branch integration, 10 October 2026

PR #69 adds native GridPattern queries and the first complete frame after swap-chain creation, resize or recreation. Review reconciled the old draft with current lazy accessibility publication and model guards, then reproduced and repaired two model-assignment races. A cached GetItem and a parent-row query can no longer bind an old row lookup to a newly assigned model at the same address.

The source baseline is bb1abcf84a170a1330a47b681a124d8d38d6fa8d. The final reviewed head is 1432180d9b252083663481ad8838bdd16f3d3ad8, tested through PR merge 4508f06332f8d670b7a2e17a86d8bde3c8aba3e7. Its parents are the baseline and reviewed head; both merge/head trees are 463b3dde84cb840e10de9d82b2ef7366f97d6123. The library input fingerprint is E6DB772710D1C2050CC2A7B6F3D38AC07F1A21EB84EAB046A209AEFB70591570. Tree equality establishes the code reviewed by the PR workflow, not a substitute for runtime results.

Merged [PR #69](https://github.com/RedSalamanders/DxUi/pull/69) as e4e53059dc12721da27aade8ec1923ebdb3e4737, whose parents and tree match the reviewed PR merge. [Workflow 38046179264](https://github.com/RedSalamanders/DxUi/actions/runs/38046179264) completed with a failure conclusion: the ARM64 foreground suites refused execution, and the paired gate remains policy-review-required. This is a reviewed functional integration, not full platform, performance or release qualification.

| Profile | Passed native suites | Foreground suites not executed | Passed consumers |
| --- | ---: | ---: | ---: |
| x64 Debug | 21 | 0 | 1 |
| x64 Release | 23, including two resource fixtures | 0 | 1 |
| x64 ASan Debug | 21 | 0 | 2 |
| ARM64 Debug | 19 | 2 | 1 |
| ARM64 Release | 19 | 2 | 1 |
| ARM64 ASan Debug | 19 | 2 | 2 |

The audit binds 128 native suite/log pairs (122 passes, six foreground refusals), eight exact-pin/API-4 synthetic consumer receipts and seven native benchmark receipts. Both sanitizer profiles detect the deliberate heap-use-after-free probe. All ten named Grid cases and the first-full-presentation regression complete in all six profiles. The two refused ARM64 suites are Menu and NativeTextInput, exit 24 before input, with no capability skip promoted to a pass. Consumer receipts each retain twelve native-module and ten negative checks.

The completed x64 Release paired study uses seed 1142142027 and twelve independent ABBA/BAAB blocks per scenario, 144 receipts and 1,440 rounds. The audit verifies every receipt SHA and independently recalculates 7,200 timing fields from 172,800 raw samples. Both unchanged judges reproduce all 78 within-noise final decisions and agree; no deterministic resource median or raw-round-peak rises are recorded. These decisions do not qualify the base policy. The official verdict is inconclusive/policy-review-required.

| Raw timing warning, clean phase | Change | Raw p | Holm-adjusted p |
| --- | ---: | ---: | ---: |
| Default frameP95Ms | +5.687% | 0.057617 | 1.0 |
| MultilineGrid frameP50Ms | +6.687% | 0.818359 | 1.0 |
| MultilineGrid frameP95Ms | +10.958% | 0.092773 | 1.0 |
| MultilineGrid prepareP95Ms | +8.378% | 0.211914 | 1.0 |

Controls remain unstable in 71/72 comparisons: Default 23/24, MultilineGrid 24/24, Distinct 24/24. The raw study retains 55 individual advice-required comparisons (22/20/13), including warnings outside the final corrected decisions. No measured speed gain or accepted non-regression is inferred. The implementation's functional merge leaves controlled-host investigation and independent policy qualification open; this record does not waive the owning performance contract.

The native and paired audits reopen every report and relevant terminal log, verify source identity and receipt bytes, and retain the recorded executable hashes. CI executables are absent from downloaded artifacts, so those binary hashes were not independently recomputed. The local corrected ControlTests executable was independently hashed. A red reproducer binary was overwritten by its corrective build and is not frozen in this packet.

## Local checks

All ten focused Grid cases pass. The first complete corrected Debug Accessibility run failed an existing selection-event expectation after late events from the preceding step. The failing case passed in isolation on the same binary; the repeated complete nonactivating suite passed in 112.191 seconds. Preserve both outcomes without treating the first failure as an accepted pass or claiming a diagnosed root cause. validate.ps1, all five validators/tooling tests and format.ps1 -Check pass. gallery.ps1 -PublishDocs produced byte-identical five theme sheets and embedded sample; only generation provenance changed. PrePush accounts for twenty scopes as pending CI and explicitly retains the four local focus obligations. No personal desktop consent was requested and no focus-taking suite was run locally.

A clean-main x64 Release benchmark was retained before edits, at the exact baseline source/fingerprint. Local performance runs are unpaired and establish no performance non-regression.

## Earlier candidates and limits

The 910fd82 integrated matrix was superseded by the cached-cell repair; its cancelled paired work has no complete summary. The completed 99daa32 matrix predates the row-navigation repair and is historical evidence. It passes all x64 suites/resources, ARM64 noninteractive suites and eight synthetic consumers. Its two ARM64 foreground suites refused before input in each profile. Its paired study independently rechecks 144 receipts, 1,440 rounds and 7,200 timing fields, with all 78 final metric decisions within noise and no deterministic resource rises. Default clean frameP95Ms and composeCpuP95Ms have raw increases of 7.344% and 7.059%; raw p-values are 0.089355 and 0.113281, with Holm-adjusted p=1.0 for both. Controls are unstable in 65/72 comparisons. The official result is inconclusive/policy-review-required. Those warnings remain recorded and do not supply accepted non-regression evidence.

The independent Q22 policy qualification, native foreground acceptance and consumer product/platform qualification remain open under the existing production review. No judge, threshold, workload, baseline or consumer pin changed during this integration.

The prepared-tree branch aa4c5955ea55394ddb5aab8fbc40ca51dcbad44b is retained. Its older implementation needs current-main reconciliation and exact-source paired/consumer checks; its plan still records embedded notification re-entry stalls, source admission/retirement and large-row host qualification. Its clean branch worktree and dirty detached baseline are preserved.

## Frozen packages

- [1432180-reviewed-ci.zip](1432180-reviewed-ci.zip) — SHA-256 `C6D618D42BCF66C1149E91792F18F8CF47FF1312C0DC4F703B1AD251E313B6A7`; entry identities in [1432180-reviewed-ci-manifest.txt](1432180-reviewed-ci-manifest.txt).
- [910fd82-superseded-ci.zip](910fd82-superseded-ci.zip) — SHA-256 `706AC2ABB45A4A373EFCDA2DF54736A4F996AD98BBFCF8069918505522A0B836`; entry identities in [910fd82-superseded-ci-manifest.txt](910fd82-superseded-ci-manifest.txt).
- [99daa32-current-ci.zip](99daa32-current-ci.zip) — SHA-256 `B3E88604AE59AA559C93E84BABA96C0CC44E6DF60E6269F51000040AAD685073`; entry identities in [99daa32-current-ci-manifest.txt](99daa32-current-ci-manifest.txt).
- [local-review-evidence.zip](local-review-evidence.zip) — SHA-256 `200DADA7DCEFBFFD851F579594F52F0F7065979CC7E4DA73CD4689DCA5C2B753`; entry identities in [local-review-evidence-manifest.txt](local-review-evidence-manifest.txt).

Each ZIP is reopened and every entry checked against the original SHA-256 and length manifest before retention. Original historical ViewerSpace evidence is unchanged. Archives identify their source candidate; earlier results do not qualify the final source. These are library-owned fixtures and relocated synthetic consumers, with no application/service dependency or product qualification implied.