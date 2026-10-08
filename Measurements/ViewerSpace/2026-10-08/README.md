# ViewerSpace shared-library qualification — 2026-10-08

Saved at the user-requested stop. All files are exact copies; manifest.txt records source paths, lengths and SHA-256.

All six native profiles (x64 and ARM64 Debug, Release and ASan Debug) passed the 19 requested noninteractive suites, with 10 existing NewControls capability skips per profile. ASan leak detection was disabled; no leak-detection claim is made.

The first Release paired set covered Default, MultilineGrid, MultilineGridDistinct and MultilineGridRetention, six runs per side. Only Retention flagged a set regression: clean frame median 0.753 to 0.843 ms (+11.85%, p=0.0411). Same-binary controls drifted substantially, and a product Explain ran during part of this set.

The unchanged-source quiet Retention repeat also used six runs per side. Its set verdict is within-noise-budget, with zero regressed and zero improved metrics. It contains severe same-binary drift and individual comparison warnings. The repeat did not confirm the initial flag, but unstable controls mean it did not establish that there is no performance cost; it establishes no speed gain. Both sets, including all warnings, are retained.

The library candidate is commit `6c436c213ac0fac181b2faafd1daf6085bdedc23` on `codex/viewer-space-grid-pattern`, based on `e5ebbb5c6046dd2cae5e32415cd7869cb7fad424`; draft PR #69 is unchanged. The original `.build` directories and binaries are retained. Broader RedSalamander application, ARM64 consumer, assistive-technology, IME and hardware qualification remain open.

Scoped runner receipts are preserved as .receipt.txt with their original JSON bytes; they are execution attestations, not benchmark measurements. Paired summaries are preserved as summary.comparison.json. These naming changes separate evidence kinds without altering the recorded bytes.
The first hosted PR #69 paired gate is retained separately in [ci-first-37835055279](ci-first-37835055279/README.md). It compared baseline `bea676a1f841` with merge candidate `e9806509a952`; Default dirty `composeCpuP95Ms` flagged +11.76% (p=0.0238), but 32.79% same-binary control drift makes this result inconclusive. The flag remains retained and unconfirmed. The other two scenarios were within-noise-budget.
The paired-gate retry (workflow attempt 2, job 113526687217) is retained in [ci-retry-37835055279](ci-retry-37835055279/README.md). It had no flagged metrics and passed within-noise-budget in all three scenarios, but same-binary controls were unstable in every scenario. The retry did not confirm a regression and did not demonstrate a gain; these measurements do not rule out a smaller cost.

## Separate consumer qualification

The identified RedSalamander consumer candidate was qualified separately from these DxUi library measurements. Build receipt `901fac275e316639902aa132eba650a8e00136b4cd20c112bc1a9b7b707ce97c` records source snapshot fingerprint `4badec2a4e9b50a09e8e4b7a88934ee3f214f024cee8a8e838e04c1b68685d4e`, production base `1277ffdec9cf78b5452d29f695aae7806fffe207`, and DxUi library pin `6c436c213ac0fac181b2faafd1daf6085bdedc23`. The x64 consumer passed native run `20261008T214504Z-18008-81aa3d580f704116953f9593a67afdec` (2/2 selected cases). The analysis-host case passed detach/stale-UIA and callback-guard checks; the lifecycle case produced eight actual 144-DPI frames, all of which passed edge probes. In the 06-refused frame, a stale header exposed an independent application-cache bug; its edge probes still passed, so this is an application follow-up rather than a DxUi edge-host failure, and the captures do not establish blanket UI acceptance. This evidence qualifies those specific consumer contracts only, not full application, ARM64 consumer, assistive-technology, IME or hardware acceptance.
