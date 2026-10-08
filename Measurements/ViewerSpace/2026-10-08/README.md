# ViewerSpace shared-library qualification — 2026-10-08

Saved at the user-requested stop. All files are exact copies; manifest.txt records source paths, lengths and SHA-256.

x64 Debug/Release/ASan Debug passed all 19 requested noninteractive suites per profile, with 10 existing NewControls capability skips per profile. ARM64 Debug/Release/ASan Debug logs are cross-build evidence only. ASan leak detection was disabled; no leak-detection claim is made.

The first Release paired set covered Default, MultilineGrid, MultilineGridDistinct and MultilineGridRetention, six runs per side. Only Retention flagged a set regression: clean frame median 0.753 to 0.843 ms (+11.85%, p=0.0411). Same-binary controls drifted substantially, and a product Explain ran during part of this set.

The unchanged-source quiet Retention repeat also used six runs per side. Its set verdict is within-noise-budget, with zero regressed and zero improved metrics. It contains severe same-binary drift and individual comparison warnings. This repeat did not confirm the initial finding; it does not establish a speed gain. Both sets, including all warnings, are retained.

Source is uncommitted on codex/viewer-space-grid-pattern, based on e5ebbb5c6046dd2cae5e32415cd7869cb7fad424. The original .build directories and binaries are retained. No commit, push, PR, consumer pin adoption, native ARM64 runtime or consumer presentation acceptance has occurred. See the RedSalamander ViewerSpace_Continuation_2026-10-08.md note for resume steps.

Scoped runner receipts are preserved as .receipt.txt with their original JSON bytes; they are execution attestations, not benchmark measurements. Paired summaries are preserved as summary.comparison.json. These naming changes separate evidence kinds without altering the recorded bytes.
