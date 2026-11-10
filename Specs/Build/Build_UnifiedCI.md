# Unified organization CI contract

This specification owns the project's CI routing. The versioned implementation and policy tests live in
[RedSalamanders/.github](https://github.com/RedSalamanders/.github). The exact reviewed full commit is recorded
in .ci/project.json and every reusable workflow caller. Update those pins together through a reviewed PR.
Older references to automatic native PR matrices or nightly native runs are superseded by this contract.

The manual `sdk-inputs.yml` caller produces dependency inputs for one of the three explicitly reviewed
application source pins; it does not advance them. Select All, x64 or ARM64 on main. The pinned shared
controller verifies canonical full checkout bytes and normally integrated producer tooling before native
setup, then builds Release/MD inputs and runs all extracted-SDK/header/license/symbol/warning gates on
the selected native architecture. Successful attempt-bound artifacts contain immutable dependency bytes
and their independently retained descriptors. They grant no application, main, merge or publication verdict.
Missing or incompatible SDK inputs retain governed source builds. Application adoption and authenticated
cross-repository consumption remain separately tracked by the organization acceleration plan.

The explicit `sdk_contract` choice retains `LegacyV1` as its default and offers
`QualifiedAbiV2` producer references. V2 selects the native compiler host before
restore/build, retains every existing SDK qualification gate and emits a separate
portable tool/header/library reference bound to the unchanged source/API, original
input key/manifest, public-header Git tree and native SDK receipt. The successful
source/platform/attempt archive includes that reference. It does not change the
SDK payload or qualify an application. Consumer trust in a new attempt still
requires its reviewed caller/engine, archive authentication and actual ABI checks;
the reference cannot authenticate itself. Missing or incompatible qualified inputs
cannot remove the existing source-build and native consumer obligations.

PRs run changed-source clang-format feedback and the reviewed compiler-free static subset. Formatting drift
fails and retains a patch. The explicit Commit PR formatting action accepts an open same-repository PR,
checks its current head, commits only selected formatted source paths by ordinary fast-forward push and
explicitly dispatches fresh compiler-free checks for that exact head. Forks retain a patch for their author.
PR checks never count as native acceptance. Compiler-dependent tooling runs in main qualification.

The compiler-free static adapter also runs the scoped planner's Git/recording-process controls with native
builds disabled. Local-iteration child fixtures isolate CI markers, and a separate hosted control verifies
that CI executes fresh work and never adopts or publishes a local iteration baseline. These controls report
before native build setup and do not satisfy native/runtime qualification or the complete tooling obligation.

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

The canonical Release regression invocation selects `-ParallelSuites 2` for the complete normal set.
Only independent non-foreground suite children overlap. Native build, tooling, performance, filter/watchdog
preflight, relocated consumer qualification, gallery and foreground suites keep their existing execution
and desktop ownership contracts. Every suite retains its actual log/count/receipt; failures and skips retain
their gates. Debug and ASan diagnostic qualification keep the serial default. This changes scheduling,
not declared coverage. New native x64/ARM64 results are required before acceptance of this hosted rollout.

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

Manual diagnostic qualification accepts `platform=All` (the compatibility default), `x64` or `ARM64`.
A compiler-free selector rejects any other value before native setup. Single-architecture results establish only
their recorded scope; they do not advance the normal two-architecture Release/production gate. A delivery with
explicit owner approval may require x64 runtime tests and all three ARM64 compilation profiles while setting
aside ARM64 runtime acceptance. Keep that scope decision and any older runtime failures in its evidence ledger.

DxUi has no hosted publication workflow, including SDK releases, Prepare, winget or documentation-gallery publication.
The normal native qualification pipeline still verifies and retains both SDK ZIPs as internal artifacts. Those ZIPs
and their extracted-consumer receipts do not create a public release. Local gallery generation remains available
through gallery.ps1 -PublishDocs; reviewed generated documentation enters through an ordinary pull request.
The five application projects retain their separate explicit artifact-only publication contract.
The implementation remains unqualified until the organization rollout plan records actual hosted acceptance.
Source changes, tests written, static checks, and the shared engine's own behavior tests alone do not establish
that all six native product pipelines passed.

## Project adapter

The adapter retains Full native suites, hosted foreground leases, the relocated source-pin consumer, native runner/watchdog checks, gallery generation and x64 Release menu resource coverage. Compiler fixtures run once on the x64 native host. ProductionOnly builds the DxUi archive without test/sample targets and does not publish a full-test/performance attestation. New-SdkPackage stages public headers, WIL public dependencies and notices, the Release archive and its matching compiler PDB beside it, integration props and source/compiler/version metadata. A consumer compiles and runs using only the extracted SDK; it cannot rebuild producer source. The SDK requires the exact recorded MSVC version for its LTCG objects and the matching /MD Release architecture. Existing source-pin consumer contracts and benchmark acceptance requirements remain independent.



Local gallery generation remains available for reviewed documentation pull requests. DxUi has no hosted gallery publication or commit workflow. Manual benchmark.yml retains original outputs and continuous host observations, requires a passing verdict by default and labels A/A as diagnostic. Existing baseline-policy/judge and CR-29/Q12 owner obligations stay open. SDK consumer diagnostics are retained in .build/logs and its original source/compiler/library/archive receipt is retained beside the internal production ZIP.

The SDK ZIP carries the unchanged reviewed NativeBuildPolicy.targets and imports it from DxUi.Sdk.props. Public SDK/WIL headers are external dependencies of the extracted consumer; consumer-owned translation units retain /Wall and the zero-warning gate. Packaging compiles/links/runs a consumer using only extracted files, verifies the library and compiler PDB hashes are unchanged, then compiles a separate C4189 control and requires diagnostic validation to reject it with /WX still disabled. Both builds retain complete MSBuild success and diagnostic summaries for the strict gate. The manifest and receipt bind the library and PDB hashes; the receipt records the policy hash and that negative control; no producer test or source tree is packaged.

During extracted linking, the packager opens the producer PDB for read with exclusive sharing, failing closed if another user holds it. The positive consumer must link without warnings using the extracted PDB. A separate-output negative control temporarily removes only the fresh extracted PDB, requires a successful link with LNK4099 and requires the zero-warning gate to reject it. The extracted PDB is restored and the producer handle is released on every exit; neither the producer artifact nor positive outputs are moved. The receipt records both isolation and negative-control results.

The SDK's `licenses/` contains the project MIT license, source notices, the actual restored WIL copyright,
KaTeX, toml++, rapidyaml and its bundled notices, libfyaml and its bundled xxHash notice, Unicode License V3 for embedded Unicode 17 data, and the
full WHATWG notice for the embedded entity table. Preserve original license bytes. Source-notice license
links resolve within that directory; repository-only provenance links name the exact public source commit.
`SdkNotices.psm1` verifies the exact file set and byte identities before ZIP creation and after extraction.
The extracted manifest's license inventory must match those files. The manifest/receipt record every path
and SHA-256, and successful complete packaging records `licenseClosurePassed`. Independent tooling tests
pin the required set, compare original upstream bytes, check offline links and ZIP extraction, and reject
missing/mutated licenses, changed source notices and a moving source identity. These license checks do not
replace the native extracted-consumer smoke or platform/runtime acceptance.
The project manifest explicitly includes `LICENSE.txt`, `THIRD-PARTY-NOTICES.md` and `docs/licenses/**`
in the native/packaging input closure, overriding the ordinary Markdown/documentation exclusions. A notice-only
change must invalidate the packaging candidate. Tooling tests pin those declarations and check the real
classifier when the pinned engine checkout is available; ordinary documentation retains its normal exclusion.

The extracted SDK consumer runs outside all producer Git checkouts: hosted RUNNER_TEMP or an explicitly selected existing approved local SmokeRoot. It disables vcpkg/forced producer imports and validates the actual compiler header dependency report against only the extracted SDK, selected Visual Studio and Windows SDK. Unpackaged headers fail before the runtime smoke can qualify the archive. The header report is retained with build diagnostics.

## Emergency PR merge

An organization owner may explicitly bypass PR merge checks during a recorded emergency through the GitHub PR override or gh pr merge --admin --match-head-commit FULL_SHA. Record the reason, blocked checks, exact reviewed head, rollback and follow-up in the PR and merge commit. The default-branch ruleset uses OrganizationAdmin with pull_request bypass; automation receives no bypass permission and direct/force pushes stay protected. Failed checks remain their actual results, and normal main qualification still runs. Production ZIP/SDK and publication requirements remain unchanged.

Follow the common [emergency merge runbook](https://github.com/RedSalamanders/.github/blob/main/Specs/EmergencyMerge.md), including the reason file and exact-head CLI procedure. No emergency merge is an ordinary fallback after a failed merge attempt. Account restrictions must be resolved before enabling the ruleset; the pending RedPrism/RedTilde public visibility change remains the owner's operation.

## Dependency metadata coverage

The organization's shared engine runs a weekly and explicit manual metadata observer across all six default branches. It inventories the coordinated engine SHA, vcpkg baseline/tool/floors/overrides and custom dependency locks, and reports available producer updates as advisories. It never restores or compiles native dependencies, edits pins or treats an upstream main commit as qualified. Missing/private or undeployed repositories remain explicit incomplete coverage. Built-in dependency review and security alerts retain their supported scopes; custom lock inventory is not a native/transitive vulnerability scan. See the common [dependency status contract](https://github.com/RedSalamanders/.github/blob/main/Specs/CI.md).

Qualification uploads retain adapter evidence separately from workspace logs. Never mix an external sandbox root
with workspace paths in one artifact input; the Windows drive-root common ancestor can become drive-relative.
This preserves diagnostics after failure without moving test data or bypassing native acceptance.
The PR delegation identity binds exact bytes and the membership of `.github/`, `.ci/`, `Build/`, `Tools/`, `Tests/`, root PowerShell entrypoints, `Directory.Build.*`, and the manifest (excluding its own digest). Coverage remains empty for compiler-free PRs; a stale or unreviewed digest cannot delegate execution. CODEOWNERS covers those tracked policy surfaces, subject to the repository's independently configured review rules.

The extracted SDK smoke directly includes the public FileDrop and RichImage headers, validates the source-owned Unicode drag data object without a window, and admits a bounded SVG. These paths exercise the packaged archive's file-drop and SVG link closure; the source-pin consumer independently compiles FileDrop as a standalone public header. Smoke qualification requires a fresh actual package/link/run and is not established by syntax checking.
## Dependency cache reuse

Default Release library build provenance is separate from native test results. Its SDK input record binds the
complete canonical v2 source/dependency/evaluated-profile/toolchain identity, exact source commit and root,
dirty-state classification, and the library/compiler-PDB bytes. Record only after zero-warning compilation and
matching before/after native inputs; revoke before another attempt and on failure. Library-only provenance must
never create Full, test-execution or publication evidence. Custom outputs and other configurations retain their
own existing contracts. SDK packaging requires matching clean committed source and checks provenance again after
extracted-consumer qualification. A library/PDB pair with stale provenance cannot be labelled as current source.

After successful regression, the adapter exports only the Release library/PDB and input record for the current
workflow run and attempt. The shared workflow transfers that immutable input set to production using an
attempt-specific artifact name. Production recomputes the complete native state after dependency restore;
only an exact clean-source/profile/toolchain/byte match omits the library rebuild. Missing/incompatible inputs
use the governed ProductionOnly source build; unsafe paths or malformed transfers refuse with retained evidence.
Import revokes unrelated Full, selected-target, iteration and performance receipts. It never imports test executables,
objects or test verdicts. Both architecture regression branches and extracted-SDK smoke remain required.
Packaging separately admits only evaluated /MD, latest C++, LTCG, compiler-PDB Release inputs with STL annotations
enabled, without ASan, forced-definition overrides or opaque CL/_CL_/additional options. Source and extracted
consumer qualification, all license checks, header closure and PDB-isolation negative controls remain mandatory.
This handoff does not yet implement cross-repository SDK distribution, consumer source pins or local candidates.

The pinned shared engine restores vcpkg's ABI-indexed package cache for normal and manual native qualification. Exact engine/input keys are preferred; fallback lookup stays within the same runner OS, host architecture and target platform, and the pinned vcpkg restore still selects each package by its computed ABI. A miss rebuilds the package. This supplies dependency inputs only: product objects, DxUi SDK outputs and build/test receipts are not restored by this cache. Dependency pins, native execution, failure detectors and production/publication gates remain required. The owning behavior is the shared engine's Specs/CI.md.

The pinned shared conformance rejects extension-wide Markdown exclusions. Pin maintenance migrates only the original global default to owned prose locations and known guides. Runtime/test include exceptions retain precedence; historical artifacts keep their original source identity.

CI callers and the manifest pin the reviewed qualified-distribution controller together.
Previously qualified producer artifacts retain their original caller/controller identity;
updating this checkout never rewrites their provenance or grants a verdict to consumers.
Main still executes both native regressions before production SDK smoke qualification.
