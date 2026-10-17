# Pre-change DxUi Release baseline, 2026-10-17

Independent DxUi-owned Default complex-UI WARP workload, collected before any
rich-text library implementation change at source commit
9f848b6464f166aa89229b80cd0e2ac22dbed42b. Only planning documentation was dirty;
compiled library inputs were unchanged. The report binds exact source, executable,
fixture, build/toolchain/dependency/environment identities through the existing
v2 attestation path. It is a successful pre-change measurement, not a candidate
comparison, new-editor performance result or production acceptance.

[Native report](baseline.json) and [comparison](baseline.comparison.json).
Comparison status: **unpaired**. Five clean and five dirty rounds measure
completed offscreen WARP work, including clear and one-pixel readback; these
numbers are not swap-chain presentation rates. Current existing-control workload
does not instantiate the proposed RichTextEditor. Its own accepted-command and
large-document fixtures, paired study and performance-policy acceptance remain
future gates.

Command: performance.ps1 -Configuration Release -Platform x64 -OutputPath
.build/reports/RichText-prechange-baseline.json (without SkipBuild). The Release
build and before/after identities passed. The earlier Z-volume build failed with
LNK1116/error112 and produced no measurement; it was not promoted to a baseline.
After relocation the E-volume build used fresh outputs, not relocated tlogs.

The active rich-text implementation plan links here. Existing unrelated feature/
consumer qualification and the migrated policy review retain their own scope.
