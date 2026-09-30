# Tool inventory

Every repository tool is a PowerShell 7 script or module; no other runtime is needed. Root entry points
(`build.ps1`, `test.ps1`, `format.ps1`, `gallery.ps1`, `performance.ps1`, `performance-paired.ps1`, `test-consumer.ps1`
and the `validate-*.ps1` scripts) are the stable developer interface.

| Tool | Purpose |
| --- | --- |
| Validation.psm1 | The validators behind `validate-skills.ps1` (front matter as a strict `key: value` subset of YAML, and instructions), `validate-specs.ps1` (authority, local links, active-plan indexing, docs, gallery, design system and measurement receipts), `validate-dependencies.ps1` (historical origin metadata, owned paths, exact pending dependencies and supported-source independence), `validate-test-port.ps1` (every inherited test has a retained entry point or an explicit exclusion reason) and `validate-build-matrix.ps1` (every live native project and solution has six configurations; `-Root` also audits a consumer) |
| PerformanceComparison.psm1, Compare-Performance.ps1 | Matched complex-UI measurements, noise bands, resource budgets and regression advice; the comparison JSON `performance.ps1` and `performance-paired.ps1` write |
| Install-ClangFormat.ps1 | Fetches the pinned clang-format 22.1.3 wheel, checks its SHA-256 and extracts only the executable under `.build/format` |
| validate_consumer.ps1 | Exact consumer revision/API/target lock checks |
| ConsumerUpdate.psm1 | Shared bounded, read-only advisory about a newer validated main commit; never changes a pin or fails a build |
| ConsumerBuild.psm1 | Evaluate actual MSBuild/compiler/linker/SDK identities and produce an isolated consumer output fingerprint |
| tests/Invoke-ToolingTests.ps1 | Runs the validator and comparator tests below; needs no native build |
| tests/Test-Validation.ps1 | Owned-source evolution, path/dependency boundaries, a second static library and skill front matter |
| tests/Test-Docs.ps1 | Catalog usage coverage, gallery integrity, fenced-code link parsing, measurement ownership and the design system |
| tests/Test-TestPort.ps1 | Count, origin, missing case and incomplete-disposition regressions |
| tests/Test-BuildMatrix.ps1 | Missing configurations, duplicate entries, silent Debug fallback and missing solution build mappings |
| tests/Test-PerformanceComparison.ps1 | Regression, missing evidence, fixture mismatch and hard-budget failures; every stored comparison reproduced exactly |
| tests/TestSupport.psm1 | Assertions and fixture trees under `.build/tooling-fixtures` for those tests |
| tests/Test-ConsumerUpdate.ps1 | Same/new/pending/failed/divergent/offline advisory decisions and immutable-pin checks, without network access |
| tests/Test-AsanRuntime.ps1 | Actual MSBuild ARM64 runtime staging and fail-closed missing-runtime regression |
| tests/Test-TestFilter.ps1 | The built `DxUi.ControlTests.exe` runs only the tests `--test=` names, in suite order, fails an unknown or malformed name and a fixture suite, and runs every registered test without it; `test.ps1` runs it after the build |

`test-consumer.ps1` restores and builds a relocated exact-pin public consumer in an isolated output directory,
renders the supplied-device example, and rejects five invalid-pin/dirty-source cases. `gallery.ps1` generates the
five-theme control catalog, supplied-device image and HTML index from compiled native code.
Add `-PublishDocs` to publish reviewed gallery snapshots to docs. `performance.ps1` captures completed offscreen
complex-UI FPS/memory and optionally compares a baseline; `test.ps1` includes that report even for filtered suites and,
with `-Tests`, runs only the named tests of a control suite (`--test=` of `DxUi.ControlTests.exe`).
`performance-paired.ps1` measures a baseline revision and this checkout serially with one copied harness, which
includes the comparator, so a revision from before the PowerShell tools is measured the same way.

Build/test receipts and scratch belong under `.build`. No tool uploads test data or changes audio/camera state.
Do not introduce a personal Codex path or silently install dependencies as part of validation.
