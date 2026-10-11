# S0-S2 remote-main integration and final PR evidence

S0-S2 implementation merged normally through [PR #73](https://github.com/RedSalamanders/DxUi/pull/73).
The actual remote-main merge is `670207429d62ecc541769b5dd6c70b9607745e64`, with ordered parents
`ec680e866a218b57000a1780018c00101a437f91` and `4bd3480170140090ab2d8ff883d05507281b52ee`.
Its tree is `e695cf74db0e2fc1a5c8c8d183cfb0bd136d639d`, identical to the reviewed tested merge
`80d875a1de91d2b2c8602faa5614cf75eab6025b`. The actual [merge proof](final-ci/files/f1028.txt)
retains the server response, Git object, branch identity and exact-head review binding.

This closes implementation integration under the dated [validation decision](../../../Specs/Testing/Testing_Validation.md#scoped-iteration-and-pr-coverage).
It does not establish CI or performance acceptance. [S0-S2 is Done](../../../Specs/Plans/Done/ConsumerUiImprovements_S0_S2_2026-10-04.md);
[qualification](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md) and
[S3-S7 proposals](../../../Specs/Plans/WIP/ConsumerUiImprovements_S3_S7_2026-10-11.md) remain WIP. Consumer pins are unchanged.

## Current-source CI and root review

[Run399, attempt 1](https://github.com/RedSalamanders/DxUi/actions/runs/38172114803) completed with literal **failure**.
Its candidate is `4bd3480170140090ab2d8ff883d05507281b52ee`, base `ec680e866a218b57000a1780018c00101a437f91`,
and tested merge/tree are the identities above. The [collection receipt](final-ci/files/f0973.txt),
[actual run](final-ci/files/f1018.txt), [jobs](final-ci/files/f0975.txt) and
[tested Git object](final-ci/files/f0990.txt) preserve those original outcomes.

| Scope | Actual result and qualification boundary |
| --- | --- |
| Windows/Linux tooling, format, scope | All four jobs succeed on the actual tested source. |
| x64 Debug/Release/ASan Debug | All three Full jobs succeed. |
| Six native profiles | 128 receipts: 122 pass, six ARM foreground suites never launch; zero capability skips. All 38 intrinsic/Flex/form/UIA functions complete once per profile, 228 executions. |
| Embedded behavior | Each profile completes 4,990 checks and 1,000 warm composites with zero tracked C++ allocations. |
| Synthetic consumers | All eight pass their API/CRT/architecture/fingerprint checks. This is no application adoption. |
| ASan negative probes | Both detect expected use-after-free with nonzero exit on the same Foundation bytes used by passing ordinary suites. |
| Native resources | Recorded six-profile native benchmark resource budgets hold; clean-frame/composition C++ allocations and hidden preparations/composites stay zero. |
| Actual gallery | 18 harness images are root reviewed and bound to the current-source logs/receipts and final collected bytes. |
| ARM64 foreground | Each native profile passes 19 ordinary suites inside its 45-minute Full window. Menu and NativeTextInput refuse before warning click and all suite input; both remain unlaunched failures. No timeout/cancellation occurred. |
| Paired/aggregate | Measurement completes; verdict publication and aggregate gate fail. Qualification stays inconclusive. |

Root independently reviewed agents' code, numeric replay, native/consumer/ASan accounting and the actual gallery.
The [full functional/visual binding](final-ci/files/f1015.txt) compares all 128 receipt/log/performance records and
18 images against the root's current-run partial reviews. The [ARM before-input proof](final-ci/files/f1152.txt)
checks exact warning guards, unlaunched suite results and restoration evidence. Foreign WWAHost occludes the warning
endpoint; the common message cannot identify the first/second ownership guard or prove continuous pointer history.
Saved/exit pointer and foreground/focus/pointer restoration states are unchanged. No capability pass is substituted.

## Paired performance remains unqualified

The [root full replay](final-ci/files/f1130.txt) and [independent full replay](final-ci/files/f1132.txt) reproduce
all 144 raw reports, 1,440 rounds, both complete 78-slot method decisions, 72 same-side controls and 24 exact budgets.
They use identical hash-verified, once-parsed report bytes, with source/harness/role/profile bindings and seal checks
before and after review. The [raw role/budget review](final-ci/files/f1017.txt) separately verifies the scheduled
identities, supported baseline overlay and literal native fixture. The library fingerprint changes between sides;
the baseline retains `sourceDirty=true` for its supported current-harness overlay.

Both methods contain zero regression verdicts. Each scenario truthfully records `verdictAgreement=false` because
legacy clean/prepareP95Ms is `improved`, while the migrated independent-block/Holm decision is `within-noise`.
All other 75 verdict labels agree. The original [frozen full reviewer](final-ci/files/f1014.txt) stays **BLOCKED**;
the base policy stays unqualified, the original publication stays `policy-review-required`/inconclusive and the
aggregate stays failed. The [explicit root review](final-ci/files/f1016.txt) permits only implementation integration
under the dated decision. It neither changes the production helper nor relabels its stopped replay as successful.

Controls are unstable in 69/72 comparisons (22/23/24 by scenario), with 272/1,872 control cells outside their bands.
There are no held regression verdicts or confirmed Outcomes, but these facts establish no non-regression or speedup.
Raw clean-frame/composition C++ allocations and hidden preparations/composites are zero. Release dirty allocations
are Default 2,145-2,160 and both Grid scenes 1,080, below 2,560 per 40-frame round. Both sides retain 3,686,400
surface/replacement bytes.

The [root descriptive review](final-ci/files/f1151.txt) and independently reproduced
[sample diagnostics](final-ci/files/f1129.txt) retain three investigation targets:

- All 28,800 clean preparation samples are exact 100 ns QPC multiples. Legacy medians shift 0.6 to 0.5 microseconds;
  the different methods' changed decisions require review, not a quantization waiver or speedup claim.
- Distinct-Grid B06 has elevated composition in four of five rounds. The paired clean composition effect is +9.211%,
  p=0.852051/Holm p=1; its B05-to-B06 control changes +759.064%. The raw worse-band observation remains retained,
  without a held or confirmed regression verdict. No run is removed or rebaselined.
- Default dirty private-memory peak shifts +0.531666%, with 11/12 positive blocks, Holm p=0.025391 and a median
  difference of 92,160 bytes. Its B17-to-B18 same-metric control shifts +3.964681%. Preserve this smaller directional
  trend for bounded attribution; do not dismiss it as noise or claim causal acceptance.

Earlier Grid/FPS findings remain open at their own identities. Trusted-policy calibration, producer role/absolute
budget hardening and matched stable-control investigation remain in the qualification plan.

## Retained bytes and limits

[copy.receipt.txt](final-ci/copy.receipt.txt) maps all 1,172 copied originals to indexed filenames, with original path,
role, byte length and SHA-256. The complete packet contains those copies, the map and
[SHA256SUMS](final-ci/SHA256SUMS); its seal covers 1,173 files and has SHA-256
`0CDB27F0143DC17EF61DF6320E2036619FFC3DB8C7F713919C7F8FCBD477EBF3`.
Copied original bytes total 32,528,871. The original 1,021-file CI seal and its literal failed/incomplete outcomes
remain unchanged. This sibling document is outside that sealed child packet. The historical parent
[cold-build packet](README.md) and its 153-file seal remain unchanged.

Original JSON/Markdown/workflow bytes retain their content under receipt extensions; copied helper scripts,
logs and actual PNGs keep their bytes. The map reconstructs original names, including nested raw report paths.
Only the actual reviewed helper is authoritative: [full reviewer](final-ci/files/f1047.ps1) and
[numeric replay driver](final-ci/files/f1031.ps1); earlier helper revisions/failed setup attempts are historical.
The caller's first source-array preflight failed before archive mutation and its predecessor is retained separately.
Current source, original inputs, complete copied inventory, effective Git attributes and final seal are verified.

Hosted uploads omit compiler, SDK, dependency and executable byte inventories. Full-v2 fields bind recorded identities,
but unavailable input bytes cannot be independently reconstructed or rehashed from those uploads. The baseline library
fingerprint is bound to all scheduled raw baseline reports; this review does not recreate filtered baseline file bytes.
Synthetic WARP and library consumers do not qualify physical devices, real IME, assistive technology, touch or product
adoption. Native ARM foreground/resource acceptance and separate main/release acceptance remain open.
