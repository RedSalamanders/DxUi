# Documentation and performance evidence

Status: COMPLETE (2026-09-05)

Owning contracts: [architecture](../../Core/Core_Architecture.md),
[performance](../../Core/Core_PerformanceAndResources.md), [validation](../../Testing/Testing_Validation.md),
[build](../../Build/Build_ToolchainAndConsumption.md), [controls](../../UI/UI_ControlsAndLayout.md).

- [x] Archive import records under Specs/Done with an explanation; preserve validator coverage.
- [x] Publish docs, all-control usage guidance and generated gallery; link from README.
- [x] Require documentation/gallery maintenance and measured before/after performance with regression advice.
- [x] Record complex-UI FPS and memory with every test invocation and provide baseline comparison tooling.
- [x] Correct the project license/notices and add formatting automation.
- [x] Validate tooling, gallery, x64 Debug/Release tests and ARM64 Debug/Release cross-builds; state native limits.
- [x] Persist results and close this plan without closing unrelated consumer work.

## Validation and limits

All four configurations build, and all 18 test.ps1 suites pass in x64 Debug and Release. Each configuration records
nine menu capability skips requiring an interactive desktop; these are not runtime proof of those cases. Native
ARM64 was not executed on this X64 machine. The existing native ARM64 GitHub jobs remain the execution gate.
No remote workflow was triggered or represented as validated by this local work.

All nine skills, local spec/doc links, 26 control usage entries, six generated gallery image hashes, archived origin
metadata and 941 inherited case dispositions validate. All four archived files match their original Git blobs.
The 32 tooling regression tests pass, including comparison rejection, missing docs/images, and fenced C++ lambdas.
PowerShell parses, workflow YAML parses, format.ps1 -Check and git diff --check pass. A no-op formatting pass preserves
source timestamps. All six documentation images and the populated complex-UI screenshot were visually reviewed.

The final Release fixture reports five-round medians of 2,401.96 clean FPS and 494.17 dirty FPS, dirty p95 2.407 ms,
28,856,320 process private bytes, a 3,686,400-byte surface and zero composition C++ allocations. These are completed
offscreen WARP measurements on Ryzen 9 9950X3D / Windows 10.0.26200, Balanced power, not displayed FPS or a soak test.
Receipts and per-suite logs remain under .build/reports and .build/logs; docs-baseline-Release.json and
docs-repeatability-Release.json record the retained baseline and the within-noise-budget comparison.

No library implementation, public headers, API revision or consumer imports changed. The matched runs use the same
library source fingerprint (0BFF172850D2862BC05C682DD7802A05644F915DE0CE13FD7283A534F3BE1DB6), establishing measurement
workflow repeatability rather than claiming an implementation speedup. New library development still needs an
actual before/after pair, targeted workload and any applicable hardware/consumer evidence. Existing consumer plans
and the separately investigated native-menu CI issue are outside this completed documentation/tooling scope.
