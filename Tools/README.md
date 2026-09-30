# Tool inventory

Every repository tool is a PowerShell 7 script or module; no other runtime is needed. Root entry points
(`build.ps1`, `test.ps1`, `format.ps1`, `gallery.ps1`, `performance.ps1`, `performance-paired.ps1`, `test-consumer.ps1`,
`validate.ps1` and the `validate-*.ps1` scripts) are the stable developer interface. `validate.ps1` runs the five
validators and the tooling tests in turn and reports every failure before it fails; CI's validation job runs it, and
`test.ps1` runs the tooling tests beside its native suites.

| Tool | Purpose |
| --- | --- |
| Validation.psm1 | The runner behind `validate.ps1` (`Invoke-DxUiValidation`, each step in its own process so one step's module imports cannot disturb the next) and the validators behind `validate-skills.ps1` (front matter as a strict `key: value` subset of YAML, and instructions), `validate-specs.ps1` (authority, local links, active-plan indexing, docs, gallery, design system and measurement receipts), `validate-dependencies.ps1` (historical origin metadata, owned paths, exact pending dependencies and supported-source independence), `validate-test-port.ps1` (every inherited test has a retained entry point or an explicit exclusion reason) and `validate-build-matrix.ps1` (every live native project and solution has six configurations; `-Root` also audits a consumer). Their file scans skip nested git checkouts, such as worktrees under `.claude/worktrees` |
| PerformanceComparison.psm1, Compare-Performance.ps1 | Matched complex-UI measurements, noise bands, resource budgets and regression advice; the comparison JSON `performance.ps1` and `performance-paired.ps1` write; the library source fingerprint and benchmark input list every receipt records; and the paired-set verdict (`Compare-PerformanceSet`: an exact Mann-Whitney U test per phase and metric with the bands, exact budgets stay exact) |
| PairedRun.psm1 | What `performance-paired.ps1` measures and in what order: baseline and candidate selection, refusal of a pair with nothing to compare, named-tree validation, the harness overlay onto a tree with its restore, and the repeated A, B, B, A schedule |
| Commit-Gallery.ps1 | The last step of the manual Publish docs gallery workflow: commits a regenerated `docs/gallery` on the checked-out branch and pushes it (an ordinary push, never forced), unless no sheet, HTML index or README changed; a new `generation.json` alone is not a change. `-NoPush` commits and stops |
| Install-ClangFormat.ps1 | Fetches the pinned clang-format 22.1.3 wheel, checks its SHA-256 and extracts only the executable under `.build/format` |
| validate_consumer.ps1 | Exact consumer revision/API/target lock checks |
| ConsumerUpdate.psm1 | Shared bounded, read-only advisory about a newer validated main commit; never changes a pin or fails a build |
| ConsumerBuild.psm1 | Evaluate actual MSBuild/compiler/linker/SDK identities and produce an isolated consumer output fingerprint |
| tests/Invoke-ToolingTests.ps1 | Runs the validator and comparator tests below; needs no native build |
| tests/Test-Validation.ps1 | Owned-source evolution, path/dependency boundaries, a second static library and skill front matter; a nested checkout under owned source; `validate.ps1` running every step and reporting each failure, and its steps, CI and `test.ps1` staying wired |
| tests/Test-Docs.ps1 | Catalog usage coverage, gallery integrity, fenced-code link parsing, measurement ownership and the design system; a whole specification tree, and nested git checkouts (a worktree, a clone) left out of the Markdown scan and its count; `Commit-Gallery.ps1` against fixture repositories with a bare origin (a changed sheet is committed and pushed alone, a new receipt or an identical rewrite is not a change, the push is never forced), and the gallery workflow's manual-dispatch-only safeguards |
| tests/Test-TestPort.ps1 | Count, origin, missing case and incomplete-disposition regressions |
| tests/Test-BuildMatrix.ps1 | Missing configurations, duplicate entries, silent Debug fallback and missing solution build mappings |
| tests/Test-PerformanceComparison.ps1 | Regression, missing evidence, fixture mismatch and hard-budget failures; every stored comparison reproduced exactly; the exact rank test against closed forms, a published example and an enumeration with ties; and set verdicts on synthetic receipts (identical distributions, a 20% FPS drop, a shift inside the band, exact budgets, spread, too few runs) |
| tests/Test-PairedRun.ps1 | Tree selection, the commit and fingerprint refusal rules, named-tree validation, the overlay and its restore on fixture trees, the repeated run schedule, the source fingerprint against its historic computation, and that the measurement scripts parse |
| tests/TestSupport.psm1 | Assertions and fixture trees under `.build/tooling-fixtures` for those tests |
| tests/Test-ConsumerUpdate.ps1 | Same/new/pending/failed/divergent/offline advisory decisions and immutable-pin checks, without network access |
| tests/Test-AsanRuntime.ps1 | Actual MSBuild ARM64 runtime staging and fail-closed missing-runtime regression |
| tests/Test-TestFilter.ps1 | The built `DxUi.ControlTests.exe` runs only the tests `--test=` names, in suite order, fails an unknown or malformed name and a fixture suite, and runs every registered test without it; no runner calls a test directly, bypassing the filter; `test.ps1` runs it after the build |
| SuiteFailure.psm1 | `Get-SuiteFailureReport`: what `test.ps1` prints and throws for a control suite that exits nonzero, its exit code, the `TIMEOUT:` line of a run the runner's watchdog ended (exit code 124) and the last lines of its log |
| tests/Test-SuiteFailure.ps1 | `Get-SuiteFailureReport` on fixture logs (a watchdog timeout with its test, a `TIMEOUT:` line beyond the last lines, a failed check that is not a timeout, a missing log) and that `test.ps1` uses it, records the timeout in the receipt and passes `-TestTimeout` on; needs no native build |
| tests/Test-TestWatchdog.ps1 | The built `DxUi.ControlTests.exe` ends a test, or a fixture suite, that never returns at its deadline with `TIMEOUT: <name> after <N> s` and exit code 124 (through its hidden `--watchdog-self-test` switch, every run bounded), while the same test with the watchdog off still hangs, a fixture that reports progress outlives its deadline, `--test-timeout` values are parsed and rejected as documented, `test.ps1`'s failure report finds the line, and every Menu driver thread starts with its failure guard; `test.ps1` runs it after the build |

`test-consumer.ps1` restores and builds a relocated exact-pin public consumer in an isolated output directory,
renders the supplied-device example, and rejects five invalid-pin/dirty-source cases. `gallery.ps1` generates the
five-theme control catalog, supplied-device image and HTML index from compiled native code.
Add `-PublishDocs` to publish reviewed gallery snapshots to docs. `performance.ps1` captures completed offscreen
complex-UI FPS/memory and optionally compares a baseline; `test.ps1` includes that report even for filtered suites and,
with `-Tests`, runs only the named tests of a control suite (`--test=` of `DxUi.ControlTests.exe`).
`performance-paired.ps1` measures a baseline (a revision or a named working tree) and a candidate (this checkout, a
revision or a named tree) serially with one copied harness, which includes the comparator, so a revision from before
the PowerShell tools is measured the same way.

Build/test receipts and scratch belong under `.build`. No tool uploads test data or changes audio/camera state.
Do not introduce a personal Codex path or silently install dependencies as part of validation.
