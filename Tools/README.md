# Tool inventory

Repository tooling is PowerShell. Consumer-facing entry points keep the version promises listed in
[`capabilities.json`](../capabilities.json); internal validation and scoped-iteration tools require PowerShell 7.
Root entry points (`build.ps1`, `test.ps1`, `Test-Changes.ps1`, `Test-PrePush.ps1`, `format.ps1`, `gallery.ps1`,
`performance.ps1`, `performance-paired.ps1`, `test-consumer.ps1`, `validate.ps1` and the `validate-*.ps1` scripts)
are the stable developer interface. `validate.ps1` runs the five
validators and the tooling tests in turn and reports every failure before it fails; CI's validation job runs it, and
`test.ps1` runs the tooling tests beside its native suites.

| Tool | Purpose |
| --- | --- |
| Validation.psm1 | The runner behind `validate.ps1` (`Invoke-DxUiValidation`, each step in its own process so one step's module imports cannot disturb the next) and the validators behind `validate-skills.ps1` (front matter as a strict `key: value` subset of YAML, and instructions), `validate-specs.ps1` (authority, local links, active-plan indexing, docs, gallery, design system, measurement receipts and changelog fragments), `validate-dependencies.ps1` (historical origin metadata, owned paths, exact pending dependencies, supported-source independence, the 150-character path budget a consumer's restore allows, and the consumer interface `capabilities.json` lists: each script and the parameters consumers pass, each module function and its parameters, the MSBuild files and the header root, read from parse trees), `validate-test-port.ps1` (every inherited test has a retained entry point or an explicit exclusion reason) and `validate-build-matrix.ps1` (every live native project and solution has six configurations; `-Root` also audits a consumer). Their file scans skip nested git checkouts, such as worktrees under `.claude/worktrees` |
| PerformanceComparison.psm1, Compare-Performance.ps1 | Matched complex-UI measurements, immutable 5% timing/2% memory bands and exact budgets; historical report compatibility (`Compare-PerformanceSet`, exact Mann-Whitney); and migrated paired-block analysis (`Compare-PairedBlockSet`, independent-block exact sign-flip p-values with Holm familywise correction) |
| Changelog.psm1, Fold-Changelog.ps1 | Changelog fragments under `Changes/`: their name and shape rules, which `validate-specs.ps1` checks, and the fold that moves them under `CHANGELOG.md`'s Unreleased heading, newest first, and removes them. A malformed fragment stops the fold before anything changes |
| PairedRun.psm1 | Baseline/candidate selection, worktree overlay and restore, legacy schedule reading, and independent ABBA/BAAB draws per block with retained protocol/seed/order and a twelve-block minimum |
| PerformancePolicy.psm1, PerformanceAcceptancePolicy.v1.json | Full receipt provenance from MSBuild-resolved toolchain/SDK inputs, installed dependency and runtime-DLL closure, and runner GPU/driver/DPI; trust is rooted only in an independently qualified versioned policy present and unchanged in the measured base; unavailable identity remains inconclusive and a candidate cannot self-approve its policy |
| BenchmarkGate.psm1, Get-BenchmarkScope.ps1, Publish-BenchmarkVerdict.ps1 | The hosted paired gate of `ci.yml` ([contract](../Specs/Core/Core_PerformanceAndResources.md#hosted-paired-gate)): measured-path routing, merge-ref/base comparison, retained verdicts, job summary and annotations. During migration the gate treats historic six-run reports as read-only and fails closed with `policy-review-required` until the measured base has a qualified policy and every receipt has complete matching identity. |
| NativeScope.psm1, Get-NativeScope.ps1 | Which runs of `ci.yml` need its six native jobs ([contract](../Specs/Testing/Testing_Validation.md#inherited-coverage-and-new-evidence)): a push to main and a manual run always (consumers adopt the main commit whose latest push run succeeded), a pull request unless every path it changes is documentation (`Get-DocumentationScopeRules`: Markdown anywhere, `Specs`, `Changes`, `Measurements`, `docs`, `.agents`, `LICENSE` and the formatting and gallery workflows; any other path, one these rules do not know included, needs them). `Get-NativeScope.ps1` is the step of the `native-scope` job: it writes the `native` output and a job summary naming the paths that decided, takes the pull request's commits and paths from `BenchmarkGate.psm1`, and throws when they cannot be read, which leaves the native jobs running |
| ScopedTesting.psm1, Test-Changes.ps1, Test-PrePush.ps1 | Affected, Full and six-profile PrePush accounting; exact successful reuse; UTF-8 path discovery; explicit capability-skip and `INTERACTIVE_NOT_RUN` status; verified open-PR delegation against its actual base; Test-PrePush shares profile-independent tooling receipts across all six profiles |
| Test-CiGate.ps1, CapabilitySkipPolicy.psm1 | Aggregate required workflow result policy and exact per-lane/profile capability-skip allowances; missing, failed, cancelled and unclassified work fails closed |
| Commit-Gallery.ps1 | The last step of the manual Publish docs gallery workflow: commits a regenerated `docs/gallery` on the checked-out branch and pushes it (an ordinary push, never forced), unless no sheet, HTML index or README changed; a new `generation.json` alone is not a change. `-NoPush` commits and stops |
| Install-ClangFormat.ps1 | Fetches the pinned clang-format 22.1.3 wheel, checks its SHA-256 and extracts only the executable under `.build/format` |
| validate_consumer.ps1 | Exact consumer revision/API/target lock checks; the API revision is `capabilities.json`'s |
| ConsumerUpdate.psm1 | Shared bounded, read-only advisory about a newer validated main commit; never changes a pin or fails a build |
| ConsumerBuild.psm1 | Evaluate actual MSBuild/compiler/linker/SDK identities and produce an isolated consumer output fingerprint, which includes the pinned source's API revision |
| VisualStudio.psm1 | The one Visual Studio discovery (`Get-DxUiVisualStudioInstallation`: `vswhere -latest -prerelease -requires Microsoft.Component.MSBuild`) that `build.ps1`, `test-consumer.ps1` and `vcpkg-install.ps1` share, and `Get-DxUiDefaultToolset`, which reads the installation's default MSVC toolset (`VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt`) and fails with the file and the repair when it is missing or malformed. `format.ps1` asks vswhere for clang-format instead, which needs no MSBuild |
| VcpkgTriplet.psm1 | What `vcpkg-install.ps1` pins vcpkg with, so it builds with the toolset MSBuild uses: `Get-DxUiVcpkgOverlayTripletText` (pure: the stock triplet's text unchanged, in its line endings, then `VCPKG_VISUAL_STUDIO_PATH` and `VCPKG_PLATFORM_TOOLSET_VERSION`) and `Update-DxUiVcpkgOverlayTriplet` (writes `<directory>/<triplet>.cmake`, only when its bytes change). Consumers with their own vcpkg installers can import both modules from the pinned checkout. Like `vcpkg-install.ps1`, both stay ASCII and Windows PowerShell 5.1 compatible, because RedSalamander launches the pinned installer with `powershell.exe` |
| tests/Invoke-ToolingTests.ps1 | Runs the validator and comparator tests below; needs no native build |
| tests/Test-Validation.ps1 | Owned-source evolution, path/dependency boundaries, a second static library and skill front matter; the consumer interface (a script or module removed, a function no longer exported, a renamed or newly mandatory parameter, unsupported PowerShell versions, a missing MSBuild file or header root, and an invalid revision); a nested checkout under owned source; `validate.ps1` running every step and reporting each failure, and its steps, CI and `test.ps1` staying wired |
| tests/Test-Docs.ps1 | Catalog usage coverage, gallery integrity, fenced-code link parsing, measurement ownership and the design system; changelog fragments (one dated bullet each) and their fold (newest first, removed after, nothing changed by a malformed one, the changelog's own line endings kept); a whole specification tree, and nested git checkouts (a worktree, a clone) left out of the Markdown scan and its count; `Commit-Gallery.ps1` against fixture repositories with a bare origin (a changed sheet is committed and pushed alone, a new receipt or an identical rewrite is not a change, the push is never forced), and the gallery workflow's manual-dispatch-only safeguards |
| tests/Test-TestPort.ps1 | Count, origin, missing case and incomplete-disposition regressions |
| tests/Test-BuildMatrix.ps1 | Missing configurations, duplicate entries, silent Debug fallback and missing solution build mappings |
| tests/Test-PerformanceComparison.ps1 | Regression, missing evidence, fixture mismatch and hard-budget failures; exact raw-peak plus run-median budgets; same-binary/toolchain, workload and missing-round rejection; every stored comparison reproduced exactly; the exact rank test against closed forms, a published example and an enumeration with ties; and migrated paired-block verdicts |
| tests/Test-PerformancePolicy.ps1, Test-PerformanceComparison.ps1, Test-PairedRun.ps1 | Trust mutation, missing provenance, balanced seeded schedule, minimum-p arithmetic under Holm, exact sign-flip inference and exact-budget behavior |
| tests/Test-BenchmarkGate.ps1 | Measured-path routing, merge-ref/base selection, legacy readability, policy-review-required rejection, identity failures, job summary/annotations, and workflow wiring including block minimum, timeouts and pinned actions |
| tests/Test-NativeScope.ps1 | The documentation rules (every kind of documentation, every other kind of path and near misses, normalization, a change whose paths could not be read, and every tracked source, tool and document of this repository), the scope step on fixture merge commits and for pushes and manual runs, and the workflows' triggers and wiring (pushes validate main alone, the native jobs wait for the scope and run when it fails) |
| tests/Test-PairedRun.ps1 | Tree selection, the commit and fingerprint refusal rules, named-tree validation, the overlay and its restore on fixture trees, the repeated run schedule, the source fingerprint against its historic computation, and that the measurement scripts parse |
| tests/TestSupport.psm1 | Assertions and fixture trees under `.build/tooling-fixtures` for those tests |
| tests/Test-ConsumerUpdate.ps1 | Same/new/pending/failed/divergent/offline advisory decisions and immutable-pin checks, without network access |
| tests/Test-AsanRuntime.ps1 | Actual MSBuild ARM64 runtime staging and fail-closed missing-runtime regression |
| tests/Test-TestFilter.ps1 | The built `DxUi.ControlTests.exe` runs only the tests `--test=` names, in suite order, fails an unknown or malformed name and a fixture suite, and runs every registered test without it; no runner calls a test directly, bypassing the filter; `test.ps1` runs it after the build |
| SuiteFailure.psm1 | `Get-SuiteFailureReport`: what `test.ps1` prints and throws for a control suite that exits nonzero, its exit code, the `TIMEOUT:` line of a run the runner's watchdog ended (exit code 124) and the last lines of its log |
| tests/Test-SuiteFailure.ps1 | `Get-SuiteFailureReport` on fixture logs (a watchdog timeout with its test, a `TIMEOUT:` line beyond the last lines, a failed check that is not a timeout, a missing log) and that `test.ps1` uses it, records the timeout in the receipt and passes `-TestTimeout` on; needs no native build |
| tests/Test-VcpkgTriplet.ps1 | The default toolset read from fixture installations (a version file with CRLF, LF or a byte-order mark; a missing, empty or malformed one fails with the file and the repair); the overlay as the stock text unchanged plus the two pins in the stock's line endings, read back by CMake when `cmake` is on PATH; written only when its bytes change (an unchanged run, and a patch update of the default toolset, leave the timestamp alone); that `vcpkg-install.ps1` keeps its parameters, reads the toolset before it clones or runs vcpkg and passes `--overlay-triplets`; that `build.ps1`, `vcpkg-install.ps1`, `test-consumer.ps1` and `Test-AsanRuntime.ps1` ask the shared discovery and have no `vswhere` call of their own; and that the installer and both modules are ASCII and parse in Windows PowerShell 5.1 (where `powershell.exe` exists). Needs no Visual Studio, vcpkg or native build |
| tests/Test-TestWatchdog.ps1 | The built `DxUi.ControlTests.exe` ends a test, or a fixture suite, that never returns at its deadline with `TIMEOUT: <name> after <N> s` and exit code 124 (through its hidden `--watchdog-self-test` switch, every run bounded), while the same test with the watchdog off still hangs, a fixture that reports progress outlives its deadline, `--test-timeout` values are parsed and rejected as documented, `test.ps1`'s failure report finds the line, and every Menu driver thread starts with its failure guard; a failed runtime check (`--failure-report-self-test`) ends the run with exit code 3 and its report instead of waiting on a dialog; `test.ps1` runs it after the build |
| InteractiveRun.psm1 | What `test.ps1 -Interactive` selects, refuses and asks of the interactive desktop lease, as pure functions: the interactive suites (`Menu` and `NativeTextInput` by default, `MenuResources` and `MenuResourceScaling` when named; kept equal to the runner's `suiteCanActivate`), the refusals made before anything is built (a CI environment, no interactive window station), the estimate the confirmation shows, the plan file the lease reads and the result file it writes, what a result means and what is wrong with one, `Test-DxUiDesktopAvailable` (the lease's read-only `--check`) and `Invoke-DxUiInteractiveLease`, which only `-Interactive` ever calls |
| tests/Test-InteractiveMode.ps1 | The selection (the default two suites, named fixtures, a suite that does not need the desktop refused by name), every refusal and its reason, the estimate, the plan's command lines, the result read back as the lease writes it, the lease's exit codes equal to `LeaseExit` in the C++ header, that `test.ps1` reaches the lease only inside `if ($Interactive)` and settles it before building anything, that a suite that needs real focus never gets `--no-activate` and every other control suite always does, and two bounded runs of `test.ps1 -Interactive` under a CI environment that refuse before they build, run or write anything; needs no native build and never asks for a desktop |
| tests/Test-InteractiveLease.ps1 | The built `DxUi.InteractiveLease.exe`: its `--self-test` (children with their logs, exit codes, bound and stop, the session's lease, and the confirmation in every answer and the warning, opened on a private desktop that is never the input desktop), its read-only `--check` and the result file the PowerShell reader reads, its usage errors before anything is shown, and that it ends only processes it started and grants the foreground only to its child; starts no run, so it never asks for a desktop; `test.ps1` runs it after the build in a run that includes the `InteractiveLease` suite, and before an interactive run takes the desktop |

The migrated exact test enumerates the actual independent, equiprobable ABBA/BAAB order assignments under the null.
Each complementary order balances position effects within its block; no global order-count constraint is imposed.
Before qualifying a policy version, reviewers inspect a same-binary A/A study on the matching runner, toolchain,
harness and scenarios, including full identity and the randomized schedule. Calibration builds one explicit revision
once and uses that executable for both labels; every invocation writes its own receipt. Review per-metric control
behavior and exact-budget findings. Unstable or flagged controls leave the affected inference inconclusive and
require a quiet-host rerun. This manual evidence review has no automatic numeric A/A threshold; A/A cannot qualify
a pull request.

New complex-UI receipts preserve forty ordered frame/preparation/composition samples per round and report QPC
frequency/counter ticks separately from the C++ clock's nominal period. The comparator checks their completeness
and their agreement with the unchanged FPS/percentiles; archived receipts without the diagnostic fields remain
readable. These fields help investigate quantization in very short no-op preparation and add no acceptance metric,
threshold or policy qualification. A changed comparator source still requires renewed source-identity review.

Policy seeding is a separate review and governance step. A qualified base policy binds a judge version and its
normalized source SHA-256, assignment protocol and normalized `PairedRun.psm1` source SHA-256. The candidate judge and
assignment generator must match those approved identities, even when both judges return the same labels. A scheduler
change requires renewed review rather than inheriting approval through an unchanged protocol string. Until the measured
base has an independently qualified policy, a candidate policy remains
`policy-review-required`. Reviewers must establish a policy-seeding merge path before the migrated gate can qualify
later changes. The candidate's proposed source hashes are review targets; no workflow silently qualifies the first policy.

`test-consumer.ps1` restores and builds a relocated exact-pin public consumer in an isolated output directory,
compiles the frozen API revision-4 fixture and public helper headers, renders the supplied-device example, and rejects invalid-pin/dirty-source cases. `gallery.ps1` generates the
five-theme control catalog, supplied-device image and HTML index from compiled native code.
Add `-PublishDocs` to publish reviewed gallery snapshots to docs. `performance.ps1` captures completed offscreen
complex-UI FPS/memory and optionally compares a baseline; `test.ps1` includes that report even for filtered suites and,
with `-Tests`, runs only the named tests of a control suite (`--test=` of `DxUi.ControlTests.exe`).
`performance-paired.ps1` measures a baseline (a revision or a named working tree) and a candidate (this checkout, a
revision or a named tree) serially with one copied harness, which includes the comparator, so a revision from before
the PowerShell tools is measured the same way. The overlay verifies destination bytes after copying, before build
reuse and around each measurement, including legacy include aliases. It verifies original backup hashes during
rollback and attempts all restorations; copy corruption or failed restoration cannot supply a successful study.
CI runs it on a hosted runner: for every pull request to `main` that
changes something the benchmark measures (the job's first step decides, and its summary names the files), and on a
manual dispatch with `benchmark_baseline`. The manual `benchmark_calibration_aa` boolean is a separate diagnostic option:
it requires identical explicit baseline and candidate revisions, builds that revision once, and measures the same binary
under both labels. It never qualifies a pull request; its receipts and summary are still uploaded when the policy is
unqualified. Both modes publish the verdict in the job summary and upload `paired-benchmark-x64-Release`; only a pull
request is gated, failing on a confirmed degradation or an inconclusive run
(see the gate's row above and the [contract](../Specs/Core/Core_PerformanceAndResources.md#hosted-paired-gate)).

`test.ps1 -Interactive` runs the control suites that need the person's real foreground, keyboard focus and pointer (`Menu`,
`NativeTextInput` and the two menu resource fixtures) under `DxUi.InteractiveLease.exe`, a native test program that `build.ps1`
builds beside the test executables (`Tests/InteractiveLease`; it is not a tool and needs no runtime beyond Windows). The lease
refuses where there is no interactive desktop, asks first, shows a warning for the length of the run and puts the foreground
window, its keyboard focus and the pointer back however the run ends. Run it only when the person at the desktop has agreed to the
time; the whole contract is in the [validation contract](../Specs/Testing/Testing_Validation.md).

The separately verified GitHub-hosted mode uses the same restoring lease. A refused ordinary warning activation can
try a bounded click on its own temporarily clipped input patch, after exact authorization, held-button and target checks.
Before moving the pointer and again before button-down, it rejects active or unreadable capture on the foreground/lease thread.
The lease observes the matching release, actual foreground and restored style/window region before starting a child;
every failure remains a failed run. Native self-tests exercise targeting and cleanup on a private desktop without
moving the person's pointer or taking focus. Hosted input acceptance requires fresh hosted execution.

Build/test receipts and scratch belong under `.build`. No tool uploads test data or changes audio/camera state.
Do not introduce a personal Codex path or silently install dependencies as part of validation.
