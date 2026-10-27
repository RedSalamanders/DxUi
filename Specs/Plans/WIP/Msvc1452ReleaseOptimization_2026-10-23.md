# MSVC 14.52 and Release optimization

Status: ACTIVE qualification; hosted native cohort verified; paired, performance/manual and release holds remain
Date: 2026-10-23

## Hosted setup repair checkpoint (2026-10-27)

### Merged repair and independently replayed evidence closeout

The human merged runtime PR #119 as `e3447765` on 2026-10-27T13:06:47Z with the reviewed tree unchanged. Head `767dffc5`,
Actions merge `38d9b21e` and base `9291e259` have a verified equal head/merge tree. All six native builds/Full jobs and
eight public-consumer qualifications passed; independent replay verified 505 payload streams, 130 suite reports,
two expected ASan detections, 124 Menu cases/profile and all ten negatives/twelve module probes per consumer.
The [source-bound archive](../../../Measurements/Toolchain/2026-10-27-hosted-msvc/README.md) retains ARM initial-focus
limits, original failed attempts and a separate local x64 Release four-case/124-case Menu supplement with recorded
initial focus and exact restoration. Old 631 ARM Menu causes remain unattributed; diagnostics changed, assertions did not.

Actual Windows tooling took 15m37s and ARM64 Release's consumer 16m04s. Both narrowly reviewed 20-minute windows
preserve every check and statistical/resource band. Complete-catalog MSBuild timestamp staging passed the real
installed-MSBuild counterfactual and changed-option negative. Actual paired preflight matched, and both judges'
calculations independently replayed exactly. Final publication refused the unqualified base policy; 70/72 unstable
controls, 249/1872 drifted observations and all-three-scenario dual-judge disagreements retain performance qualification holds.

- [x] Publish and validate the bounded compiler/setup, consumer-window and complete-catalog staging source on PR #119.
- [x] Independently replay the exact six-profile native/provider/consumer cohort and local Menu supplement.
- [x] Preserve/replay the final paired result; keep policy and physical-input limits separate from native passes.
- [x] Root and an independent raw-ZIP reviewer verified native bindings before the external human merge.
- [ ] Complete independent paired review; retain policy/noise/judge holds separately.
- [ ] Publish this sealed summary-only packet and domain/WIP closeout in a separately reviewed docs-only PR.

The separate main run 40509208565 is in progress at this snapshot; x64 Debug Full failed NativeTextInput
foreign-document TSF focus preservation while Menu and consumer checks passed. Cause remains unattributed.
Push-scoped paired skip is not new qualification.

Broader five-repository performance/manual/release work below remains ACTIVE. No consumer pin changes or performance
acceptance are implied by this SDK cohort.

### Attempt 631: preserved failures and strict paired setup repair

Historical attempt; the merged repair and subsequent distinct producer evidence are recorded above.

Run [40485094386](https://github.com/RedSalamanders/DxUi/actions/runs/40485094386) binds head
`631d8a95cf59c520bed1cc98c21cefc2f612586b`, merge `0f942cc859821fadada1e1da560a6e71b0a2c9f1` and base
`9291e259331c1dd376c028dcda8e70798765139f`. All six builds and all eight final relocated-consumer receipts passed.
ARM64 Release completed its ten negatives and twelve module checks in approximately 15m44s, validating the narrow
20-minute operational window. All x64 native jobs and ARM64 ASan passed. ARM64 Debug and Release Full failed in
different Menu cases; downstream consumer success does not establish Full success. Those desktop assertions remain
unchanged and their failures remain open. The overall workflow and aggregate gate failed.

Paired job 122124779896 refused `toolchainIdentity` compatibility before consuming measurement slots. Both per-side
attestations passed; retained hosted digests are opaque and do not expose which profile input differed. A controlled
local metadata-only experiment with identical source bytes reproduced an omission: existing benchmark-only timestamp
staging equalized the benchmark profile while library `MSBuildAllProjects` selected a project on one side and an
import on the other. Staging both attested projects made both complete normalized profiles exactly equal. Every
original timestamp was restored exactly. This local counterfactual establishes the setup defect; it does not decode
the historical hosted digests or establish a performance verdict.

The bounded repair shares one canonical library/benchmark project catalog between attestation evaluation and fresh-build
timestamp staging. It retains every evaluated property and strict cross-side equality. Real installed-MSBuild coverage
retains opposing-timestamp metadata, reproduces the old omission, checks complete-profile equality, and proves a changed
compiler option still fails admission. Reused builds retain their attested context. No statistical performance band or resource budget changes.
The Windows tooling job alone moves from 15 to 20 minutes: attempt 631 already consumed 14m29s before the new
approximately two-minute real-MSBuild regression. Compiler setup and every tooling/ASan check remain required;
Ubuntu, format, native and consumer ceilings remain unchanged. Mutation tests reject missing, old or widened allowances.

Private preservation under `.build/msvc-setup-20261027/attempt-631/sealed-failure-02` binds all six native and paired original
provider ZIP digests/lengths, raw job status/logs and 505 replayed completion-manifest streams. The failed ARM Full outcomes,
expected ASan detections, complete consumers and ARM initial-focus-unavailable limits remain distinct. Archive seal SHA-256:
`0CEDF462E67817756298E035030B41DE595E1CCADDB275565099F314C4E7C17B`.

- [x] Review complete-catalog staging and real-MSBuild/negative regressions independently before publishing the next repair head.
- [x] Bind the next hosted run to its own head/merge and replay actual native/consumer/paired outcomes; no unchanged rerun or retagging.

### Subsequent consumer timeout (new operational repair)

Historical failed attempt; its operational repair and later actual completion evidence are recorded above.

Repair head `4a562108fdf067d31aae5fee563ba90b2e603d09`, Actions merge `fcd8ac46aca6b5cc00d3387ad98bd9645f944c34`
into base `9291e259331c1dd376c028dcda8e70798765139f`, passed actual compiler provisioning, every native build and
every Full native suite in run 40474717807. Five profile jobs completed successfully. ARM64 Release job 122090646802
then exhausted its separate 15-minute relocated-consumer window, after all twelve module probes passed, before
publishing the final consumer receipt; its gallery did not run. The overall workflow failed and is not SDK-wide green.
Its failed provider ZIP 12723278191 is retained with verified digest
`3d7e2911b115214b92744b97bdce53d86c33fd4f0e85a70330e9a266da5eb7db`, separately from any later attempt.

The successful ARM64 Debug tail took 146 seconds for pin-negative/clean checks and 17 seconds for build-identity
negatives, compared with 8.2 seconds on x64 Release. Failed ARM64 Release reached module-probe success at 13m31s,
leaving only 90 seconds; a comparable ordinary tail projects 16m14s, not a completed receipt. Every distinct cold
LTCG EXE/DLL link and all ten negatives remain necessary. The reviewed operational change grants only ARM64 Release's
regular consumer 20 minutes and its enclosing job 155; every other window, native check and statistical band is unchanged.
Per-case start/outcome/duration and unique incomplete progress records will expose the exact case if another attempt ends.

- [x] Validate the narrow window/progress change, push a fresh PR #119 head, and bind the new hosted result to its own head/merge.
- [x] Replay all final provider ZIPs, payload streams, Full/ASan and complete consumer receipts before claiming a passing matrix for that producer.
- [x] Prepare source-bound public evidence and WIP/build closeout in private staging; publication is tracked above.

Main `9291e259331c1dd376c028dcda8e70798765139f`, run [40472278769](https://github.com/RedSalamanders/DxUi/actions/runs/40472278769), failed compiler preparation in all six native profiles. The x64 Debug [job 122082077031](https://github.com/RedSalamanders/DxUi/actions/runs/40472278769/job/122082077031) reports at 08:16:22 UTC: `The variable '$LASTEXITCODE' cannot be retrieved because it has not been set.` GUI bootstrapper extraction continues after that exception. The script's call operator returned before GUI completion, and its strict-mode ambient exit-code read failed; this is setup evidence, not a compiler/native test failure. Raw logs and every fixture attempt remain privately under `.build/msvc-setup-20261027/` and `.build/msvc-setup-fixtures/` in the isolated repair checkout.

- [x] Replace asynchronous GUI invocation with explicit hidden wait and the owned process's exit code; quote the install path, retain output, and use canonical downstream Visual Studio discovery after completion.
- [x] Add deterministic delayed GUI/provider coverage for unset/stale ambient exit state, success/reboot/failure, spaces, required tools and no-install refusal paths; no actual local installation.
- [x] Complete full validators, focused tooling controls and formatting; publish the focused repair PR for independent review.
- [x] Review real hosted provisioning and subsequent six-profile receipts. Preserve channel/component or default-selection failures if the actual installer reveals them; never downgrade the compiler or treat fixtures as native qualification.

Docs/gallery review: CI setup changes no public control behavior or pixels; no gallery regeneration is needed. Existing local, performance, interactive and native ARM64 acceptance boundaries below remain in force.

Local repair evidence: `fixture-diagnostics.log` passes 12/12 delayed-GUI/provider cases, including retained stdout/stderr and detailed installer logs. Restoring the old asynchronous call in `Setup-no-wait.control.ps1` reproduces the hosted unset-exit-code exception and fails eight cases (`no-wait-negative.log`), while the four no-install controls still pass. Real local `-VerifyOnly` passes default 14.52.37025; pinned clang-format 22.1.3 `-Check` passes. These focused controls and read-only compiler discovery do not establish native runtime or hosted provisioning success.

Independent review caught a fixture invocation-scope defect: unnecessary `GetNewClosure` wrappers hid the script-local helper only when another script invoked the suite. The initial full `validate.log` retained that failure; all five validators and all other tooling suites passed. Removing the synchronous-loop closures makes fresh direct and nested invocations pass 12/12 each (`fixture-direct-fixed.log`, `fixture-nested-fixed.log`); `Test-MsvcSetupEntrypoints.ps1` now requires both forms in ordinary tooling validation.

Repair [PR #119](https://github.com/RedSalamanders/DxUi/pull/119), initial run [40473800871](https://github.com/RedSalamanders/DxUi/actions/runs/40473800871), has the first actual hosted setup success: Windows-tooling job 122086989170 advanced from successful compiler preparation to validation. Its subsequent native/runtime gates remain pending. The fixture correction requires a fresh PR run; neither that setup success nor local fixtures establish a complete repository pass.

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

## Main integration - 2026-10-27

The approved integration combines current main with the retained MSVC 14.52 work, preserving newer callback guards and the pause API. Qualification of this combined source is recorded separately from earlier branch receipts. Historical passes and remaining native, interactive, performance and release gates are retained; the merge does not close them automatically.

On 2026-10-27 the user explicitly authorized local validation followed by merge, deferring hosted CI and native ARM64 execution. RedPrism/RedTilde hosted jobs were refused for GitHub billing/spending restrictions; RedXe/DxUi hosted provisioning failed before qualification. Local checks do not convert these results into CI passes. Native ARM64, interactive/manual, paired-performance and release gates remain open.
