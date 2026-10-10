# Scoped testing and validation cost

Status: ACTIVE: implementation and qualification in progress.
Date: 2026-10-05
Owner: [current validation contract](../../Testing/Testing_Validation.md)

## Accepted request

All active native test sources and helpers use `Scope.Tests.Something.h/.cpp`; Scope identifies the component and Something the scenario, runner, fixture, or helper. Migrate project entries, includes, inventories, tooling and current documentation together. Immutable historical evidence and imported/external source are not active test files.

Everyday iteration selects affected scopes from committed changes since the merge base plus staged, unstaged, deleted, renamed and untracked paths. Report paths and reasons. Unknown executable inputs, shared infrastructure and dependency/build changes conservatively widen coverage. A focused pass is never a full repository verdict.

Reuse only successful local evidence for identical source/dependency closure, executable/runtime bytes, architecture, configuration, runner arguments and environment. Invalidate before execution; publish only after success and an unchanged post-run identity. Corrupt/missing evidence executes again. Explicit force runs, performance investigations and environment-dependent/manual acceptance remain available.

Before a PR push, full coverage is required across local and CI execution. Delegate only the identical scopes/configuration that the enabled PR workflow actually runs. A nightly gate is not a PR gate. A base/merge-tree change, configuration difference or modified runner invalidates equivalence. Keep main/release acceptance distinct from PR feedback.

## Review evidence

- DxUi: test.ps1 supports suite and named-test filters, but unconditionally runs tooling and a benchmark; CI runs six native profiles plus a separate validation job and paired benchmark. The initially running main validation subsequently passed; an earlier main ASan menu-popup fixture had failed even though its PR was green.
- RedXe: test.ps1 is an unconditional sequence of standalone suites, tooling, host WARP and crash checks. PR CI runs all tests on x64 Release; main runs all six profiles. Latest main CI passed.
- RedSalamander: governed Affected, ExplainPlan and exact Resume already exist. Reuse these contracts. PR CI runs Suite PR on x64/ARM64 Debug; long in-product CI suites run nightly and sanitizer suites weekly. Latest CI failed in format, migration and native jobs.
- RedPrism: test.ps1 orchestrates document, IO, host, compositor and in-product checks on the current UI branch. No checked-in or enabled GitHub workflow was found. Plan only; do not rename files, edit runners, build, or change CI during this work.

## Implementation sequence

- [x] Inventory active source files, test scopes, dependencies, desktop/network requirements and CI coverage.
- [x] Rename active native tests/helpers and reconcile all current callers; enforce naming.
- [x] Provide explainable affected/default iteration and explicit full/pre-push entrypoints.
- [x] Prove conservative fallback, rename/delete/untracked discovery, dependency fan-out and invalid selectors.
- [x] Prove unchanged reuse, source/binary/configuration/environment invalidation, failures and post-run mutations.
- [x] Align CI commands and pre-push delegation without weakening existing main/release gates.
- [ ] Run focused tooling/scenario checks, applicable builds and noninteractive native tests; record unavailable/manual gates separately.
- [ ] Persist durable requirements in the owning contract and agent guidance; close only after required qualification.

## Qualification constraints

No test may seize the person's foreground or pointer without agreement to the time. Use existing harness desktop leases. Native ARM64 claims require ARM64 execution; cross-build evidence is separate. No consumer pin changes are implied by library changes. Retain baseline/candidate fixture and source provenance for resource comparisons.

## Implementation and reviewed qualification

All 56 active native tests/helpers use contextual names, with current includes/projects/docs/inherited-case accounting reconciled. Historical origin filenames and hashes remain historical; currentFile resolves renamed retained tests. Exact test-source mappings and reviewed direct Grid/Tree/ComboBox/TextField callers narrow local work; unknown/shared inputs widen. Tooling selects validate.ps1 once plus the Windows runtime-staging fixture. Explicit Full CI retains its six native profiles, menu/text-input coverage, consumer builds and gallery/paired benchmark jobs.

Qualification completed: all 19 noninteractive Debug suites; ASan Foundation, Embedded, Control, Rendering and WindowHost with the deliberate detection probe; x64 Debug/Release/ASan builds and ARM64 Debug/Release/ASan cross-builds; validate.ps1 including all tooling fixtures; pinned formatter check. Eighteen scoped-testing regressions prove discovery, conservative selection, CI coverage, source/binary/profile/environment invalidation, unchanged reuse, stale SkipBuild refusal and failure/post-run-mutation rejection through the public runner. Native runs retain capability skips, especially foreground UIA; those are not exercised paths. Tooling's second identical invocation reports REUSED. Broad native coverage in other profiles is delegated to the forthcoming exact PR workflow, not asserted locally.

Current GitHub main CI [37352873568](https://github.com/RedSalamanders/DxUi/actions/runs/37352873568) passed on 19ca44d95a0c. The earlier [37220881885](https://github.com/RedSalamanders/DxUi/actions/runs/37220881885) failed the ASan menu popup fixture. These are existing revision evidence, not execution of this uncommitted migration. The candidate workflow digest is checked before delegation; GitHub must also report that workflow enabled. Pending CI remains explicit.

Raw local logs/receipts are under .build/logs/scoped-testing-* and .build/reports/scoped-tests. The retained Release benchmark and candidate Debug/per-profile observations are unpaired; they establish no resource non-regression claim. No renderer, control behavior, consumer pin or gallery pixels changed. Pending closeout: the changed PR workflow/native matrix, required foreground/IME/AT acceptance with the person's timed lease, and native ARM64 runtime where claimed. Keep this plan active until the required migration qualification is accepted; do not report repository PASSED from focused/noninteractive evidence.

Final receipt review includes documentation, skill Markdown and source-origin mappings in validator identities. Uncommon inline/generator extensions invalidate build attestation. Documentation edits select only independent validators; native scopes remain omitted. The two standalone runner fixtures pass all eighteen cases, including those invalidation boundaries.

DxUi preserves one Windows tooling qualification and one Linux portability qualification, with the staging fixture only once. Conditional native PR coverage reuses its existing NativeScope contract; skipped native jobs are never delegated. Native receipts retain identical-code reuse across validator prose edits. The standalone fixtures cover these boundaries in eighteen cases.

Candidate CI passed the native Debug suites but exposed old generated filenames in the relocated multi-module consumer project. The generated entries now use the renamed source filenames. Pre-merge review also repaired the paired overlay for old baseline include paths: its current payload reaches both current and historical include names, and restoration returns the historical files exactly. The owning paired-overlay fixtures pass 17 cases and benchmark-gate fixtures pass 41. Hosted candidate consumer/profile/performance qualification remains pending.

Post-merge policy review adds a clean committed candidate barrier around PR coverage discovery. Staged, unstaged, untracked and concurrent changes, including a concurrent clean commit, retain local obligations. The focused scoped-runner suite passes all 19 cases. Original PR #66 passed all six native/consumer profiles, tooling and paired benchmark before merge. This tooling-only follow-up does not repeat local native qualification or close remaining manual/runtime gates.
