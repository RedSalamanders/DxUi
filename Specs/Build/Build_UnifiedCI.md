# Unified organization CI contract

This specification owns the project's CI routing. The versioned implementation and policy tests live in
[RedSalamanders/.github](https://github.com/RedSalamanders/.github). The exact reviewed full commit is recorded
in .ci/project.json and every reusable workflow caller. Update those pins together through a reviewed PR.
Older references to automatic native PR matrices or nightly native runs are superseded by this contract.

PRs run changed-source clang-format feedback and the reviewed compiler-free static subset. Formatting drift
fails and retains a patch. The explicit Commit PR formatting action accepts an open same-repository PR,
checks its current head, commits only selected formatted source paths by ordinary fast-forward push and
explicitly dispatches fresh compiler-free checks for that exact head. Forks retain a patch for their author.
PR checks never count as native acceptance. Compiler-dependent tooling runs in main qualification.

After main has remained unchanged for five continuous minutes, its cheap coordinator submits one native
admission. Cumulative native identity uses the complete committed Git input tree, file modes, dependencies,
project adapter and exact engine/policy pins. It is not a truncated files API response or last-merge-only diff.
A code merge followed by prose still requires qualification. Excluded-only edits retain the previous actual
success, failure, cancellation or pending status. Failed unchanged inputs require an explicit fresh retry.
Unknown paths are meaningful; unreadable inputs fail closed. Renames inspect both paths and deletions count.
There is no maximum-deferral override while merges continue.

.ci/project.json declares exclusions and runtime exceptions. Prose, plans, historical measurements and Changes
fragments normally do not require a native build. Shipped schemas, test fixtures, runtime examples, generated
build inputs, resources, dependencies and tooling remain meaningful, including runtime files with .md suffix.
Do not exclude all of Specs or docs without retaining their product/runtime exceptions.

The native workflow has a separate non-cancelling serialized queue. New pushes may cancel only obsolete
cheap coordinators. A stale queued automatic candidate exits before provisioning a compiler. Each admitted
pipeline freezes one commit, input digest, engine and version. Both native x64 and native ARM64 execute:

1. Release with test targets/hooks enabled; matching source/dependency/toolchain/build evidence is retained.
2. Actual normal regression execution through the canonical test harness and an always-on Release failure probe.
3. A global AND gate verifies both architectures before either production Release job may begin.
4. Separate production-only Release outputs, clean-extraction native smoke and ZIP artifact retention.

These are separate test and production roles. They intentionally compile production code twice; neither a PR
matrix nor a second push matrix repeats the same role. Test receipts and production artifacts never share
mutable cached object outputs. Dependency binary caches include the engine, native architecture, manifests,
locks, restore/toolchain scripts, props/targets and adapter closure. Cache reuse cannot waive a test gate.

Receipts bind repository, exact source/input identity, workflow run, architecture, role, count unit, positive
discovery/execution, active failure probe and artifact digests. Canonical per-case evidence remains authoritative
where the harness provides it; suite counts are explicitly suite counts, not invented native case counts.
Missing/zero/filtered execution, an unexpected skip, timeout, disabled failure checks or an identity mismatch
cannot qualify production. A focused pass is not Full repository or release qualification.

Debug, ASan, paired benchmarks, repeated stress, and other extended obligations run through explicit manual
qualification. Their canonical warnings, native-host rules, budgets, evidence preservation and desktop leases
remain unchanged. There are no scheduled native builds. Scheduled dependency, security and status automation
remains enabled; CodeQL uses explicit C/C++ build mode none, which does not invoke a native compiler.
Compiler-free CodeQL does not analyze generated code as completely as a build-based scan:
[GitHub build-mode contract](https://docs.github.com/en/code-security/how-tos/find-and-fix-code-vulnerabilities/manage-your-configuration/codeql-for-compiled-languages).

Publication is an explicit trusted-main action, defaulting to prepare-only. Select an original successful
qualify.yml run with complete matching receipts and retained ZIPs for both native architectures. Publication
promotes those exact bytes without building, restamping, signing afterwards or substituting an older green
run for a later failure. Missing/expired artifacts require a fresh explicit qualification. Existing conflicting
tags, assets or winget submissions are retained for review and never overwritten. Product releases publish
ZIP assets only. Applications support winget; the DxUi SDK does not. winget requires anonymous immutable
release URLs, matching ZIP hashes, the configured nested executable, an explicit product license and a working
publication credential. Private visibility or unavailable credentials are readiness gates, never permission
to change visibility, billing, licensing or access.

The implementation remains unqualified until the organization rollout plan records actual hosted acceptance.
Source changes, tests written, static checks, and the shared engine's own behavior tests alone do not establish
that all six native product pipelines passed.

## Project adapter

The adapter retains Full native suites, hosted foreground leases, the relocated source-pin consumer, native runner/watchdog checks, gallery generation and x64 Release menu resource coverage. Compiler fixtures run once on the x64 native host. ProductionOnly builds the DxUi archive without test/sample targets and does not publish a full-test/performance attestation. New-SdkPackage stages public headers, WIL public dependencies and notices, the Release archive, integration props and source/compiler/version metadata. A consumer compiles and runs using only the extracted SDK; it cannot rebuild producer source. The SDK requires the exact recorded MSVC version for its LTCG objects and the matching /MD Release architecture. Existing source-pin consumer contracts and benchmark acceptance requirements remain independent.



Manual gallery generation executes native project code with read-only credentials. A separate trusted-main writer verifies the frozen PR head, commits only generated gallery data by ordinary push and explicitly dispatches fresh PR checks. Manual benchmark.yml retains original outputs and continuous host observations, requires a passing verdict by default and labels A/A as diagnostic. Existing baseline-policy/judge and CR-29/Q12 owner obligations stay open. SDK consumer diagnostics are retained in .build/logs and its original source/compiler/library/archive receipt is retained beside the internal production ZIP.

The SDK ZIP carries the unchanged reviewed NativeBuildPolicy.targets and imports it from DxUi.Sdk.props. Public SDK/WIL headers are external dependencies of the extracted consumer; consumer-owned translation units retain /Wall and the zero-warning gate. Packaging compiles/links/runs a consumer using only extracted files, verifies the library hash is unchanged, then compiles a separate C4189 control and requires diagnostic validation to reject it with /WX still disabled. The receipt records the policy hash and that negative control; no producer test or source tree is packaged.
