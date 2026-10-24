# MSVC 14.52 and Release optimization

Status: ACTIVE qualification; implementation complete
Date: 2026-10-23

## Checklist

- [x] Create isolated `ej/msvc1452-release-20261023` worktrees for DxUi, RedXe, RedSalamander, RedPrism and RedTilde; preserve concurrent source work.
- [x] Audit 984 project/configuration/platform profiles without building; retain complete compiler/linker metadata and raw evaluation output.
- [x] Verify installed/default MSVC 14.52.37025, x64/ARM64 target compilers and sanitizer libraries. The previous 14.51 installation is absent; both sides use 14.52 so the experiment isolates Release policy, not a compiler-version speedup.
- [x] Build and freeze the original x64 Release DxUi images/archive and capture an initial preimplementation complex-UI receipt. Other builds may be active: this first receipt is diagnostic, not non-regression evidence.
- [x] Require MSVC 14.52.x in every native configuration; retain exact compiler/linker identity for each archive and consumer.
- [x] Enable explicit Release `/Gy`, `/GL`, `/OPT:REF` and full `/LTCG`, keeping Debug/ASan optimization behavior unchanged.
- [x] Forward policy and exact toolset into pinned consumer archives without changing adopted API pins; give changed policy an independent dependency-output identity.
- [x] Add evaluated-profile and negative-control tooling tests; update build/consumption contracts and usage documentation.
- [x] Execute the local build/native/tooling matrix; passing receipts, failed attempts and remaining acceptance limits are recorded below.
- [ ] Measure frozen baseline/candidate executable and DLL sizes, archive sizes, build duration and counterbalanced runtime/resource samples on unchanged fixtures. Preserve every failed/noisy attempt; do not silently rebaseline.
- [ ] Publish reviewed local evidence and a five-repository settings-difference report. Application measurements stay in their owning repository.
- [ ] Fold the changelog, validate formatting/specs/tooling, and move this plan to Done only after its local implementation and required qualification complete.

## Scope and decisions

The owning contract is [Build_ToolchainAndConsumption](../../Build/Build_ToolchainAndConsumption.md). The standalone library, all sample/test executables and every consumer's final EXE/DLL must participate. Resource-only satellites have no C++ functions, and an external Makefile project is not evidence that the external repository's compilation changed.

Use the installed default 14.52 patch and validate its family; never hard-code a developer installation or silently accept another compiler family. Full LTCG preserves RedXe's existing avoidance of stale incremental LTCG products. `/OPT:ICF` remains governed by the toolset/WPO defaults and existing project policy.

The other four repositories each own an indexed plan with this filename. Their existing DxUi API pins remain intact: ordinary MSBuild project-reference properties carry WPO and the exact selected toolset. Compiler-policy changes require separate cache identity and fresh archive/product evidence.

## Baseline and evidence

Base: `b69f452f157efb2beaf710504f9d4e23db0b9eb5`. Generated records are under `.build/msvc1452/`: `audit-before/settings.json`, compressed raw metadata, `baseline-build.json`, `baseline-sizes.json`, `baseline-bin/`, and `baseline-performance-initial.json`. Initial baseline build succeeded on x64 Release. Reviewed receipts will be retained under `Measurements/Toolchain/2026-10-23-msvc1452/`.

The comparison must distinguish configuration inspection, compilation, native functional tests, runtime measurements and performance-policy acceptance. Current rich-text/performance holds are not closed by this build-policy work.

## Qualification workspace and current gates

The user authorized E:\DxUi.Perf on 2026-10-23 for worktrees/builds/evidence. Final qualification lives in E:\DxUi.Perf\msvc1452 on ej/msvc1452-qualification-20261023; original Z: baselines remain retained and hash-verified frozen copies exist under E:\DxUi.Perf\msvc1452-frozen. The four consumer API pins are unchanged. Source edits during initial builds were correctly rejected by attestation; retain those attempts and rerun against frozen inputs.

The user approved interactive tests when ready. DxUi preserves its Start/Cancel and desktop-restoration checks. The user explicitly approved the visible 30-second automatic-start countdown for remaining retries. Native ARM64 execution is unavailable on this x64 host. Hosted CI provisions and verifies the required 14.52 family through .github/actions/setup-msvc; the download/install branch remains pending a real GitHub-hosted execution. Local -VerifyOnly and refusal controls must pass without installing software. All required gates stay open until their receipts are reviewed.

## Local qualification checkpoint

Updated 2026-10-23T13:06:21.8133932Z. All six local builds and all 19 noninteractive suites in x64 Debug, Release and ASan Debug passed. All four foreground suites passed in each x64 profile, including the explicitly approved 30-second automatic-start Release retry. Foreground/focus/pointer restoration was verified. Earlier failed Menu and unstarted/manual-timeout attempts remain retained; repeats do not erase them.

Native ARM64 runtime and a real hosted CI provisioning run remain unavailable/pending. The plan stays ACTIVE: local builds do not establish native ARM64 support or close existing feature/performance holds. Paired runtime/resource analysis and reviewed image sizes are recorded in Measurements/Toolchain/2026-10-23-msvc1452/; the consolidated report is retained externally under the approved E:/DxUi.Perf/reports/Msvc1452ReleaseOptimization_2026-10-23/. All first-party native C++/header/shader files remain unchanged.

## Requested test-failure remediation

The user limited completion to local x64 tests and ARM64 cross-builds. Native ARM64 runtime and hosted provisioning remain separate acceptance gates. The original optimizer study above remains historical and native-source-identical; the new repair phase changes native code.

- [x] Reproduce the RedSalamander Issues-pane HRESULT in a standalone DxUi regression: sibling relayout during AddToSelection returns UIA_E_NOTSUPPORTED before the fix. Retained before receipt: E:/DxUi.Perf/failure-remediation/Debug-regression-before-envmatched.log.
- [x] Replace post-commit geometry guards with the existing binding/lifetime guard in Grid add/remove selection, matching the existing exclusive Select route.
- [x] Pass positive relayout and negative same-pointer-rebind controls, then full x64 Debug/Release/ASan tests and ARM64 cross-builds.
- [ ] Explicitly adopt the qualified repair in RedSalamander and record its new exact pin separately from the original optimizer experiment.
- [ ] Refresh final binary/performance evidence and close the local-scope plan only after all five repositories pass.

Docs/gallery review: this changes selection action results and callback safety, so the input/accessibility contract and control guide are updated. It changes no visual fixture, control appearance, layout algorithm or gallery pixels; gallery regeneration is unnecessary.

## Repaired-source qualification checkpoint (2026-10-24)

All six repaired-source builds passed. The 19 ordinary and four explicitly authorized real-focus suites passed in each x64 Debug, Release and ASan Debug configuration. The ASan interactive attempt that exhausted storage remains failed; the clean retry passed at 2026-10-24T00:04:46Z with desktop restoration. Source validators and formatting passed before this prose reconciliation and run again for handoff. Printed timing from concurrent functional runs remains diagnostic.

The minimal compatible repair is retained as unpublished commit 06205d6242be408bbb7bb624fb61174a89a97854 and GridSelectionRelayout.bundle. RedSalamander explicitly adopts this exact local pin; its fresh Full qualification remains a separate consumer obligation. RedXe, RedPrism and RedTilde retain their original pins. Normal remote restore/publication is a separate pre-merge/release gate, and no remote write is performed here.

The original optimizer-only frozen pair is unchanged. A separate post-remediation frozen pair compares its optimized candidate with the repaired optimized library on the same complex-UI fixture. The embedded sample stays 1,878,528 bytes; the native archive changes from 183,461,828 to 183,465,694 bytes. These repair deltas do not alter the original /GL archive-versus-shipping-image interpretation. The new quiet paired performance run and cross-repository closeout remain pending.
## Paused unknown-progress repair

RedSalamander candidate5a's cadence receipt recorded 48 repaint snapshots in 1.2 seconds for a paused, unknown-byte card with only one popup timer tick. Its synthetic known-byte fixture also omitted the authoritative discovered-bytes flag. Candidate5b repairs that fixture and adds a separate explicit unknown-byte paused scenario, preserving the resource failure before the control repair.

The canonical ProgressBar now has explicit animation pause state, independent of enabled and determinate state. Setter, Paint and Tick honor it; resume re-seeds time without jumping across a pause. A native phase/idle/resume test and an existing real UIA range-event client test cover the new state. The default property is unchanged and the existing reduced-motion gallery remains pixel-identical, so no static gallery regeneration is needed. New local native matrix qualification and exact backport adoption in RedSalamander remain pending. The earlier selection-only v2 matrix stays retained as historical evidence, and its E: frozen candidate is a pre-pause baseline.

- [ ] Pass paused-progress regressions and all 23 suites in each x64 profile, plus all six builds.
- [ ] Adopt the exact additive API-4 backport only in RedSalamander and close its unknown-paused cadence failure.
- [ ] Refresh final measurements and the five-repository report; retain all original failures and separate external gates.
