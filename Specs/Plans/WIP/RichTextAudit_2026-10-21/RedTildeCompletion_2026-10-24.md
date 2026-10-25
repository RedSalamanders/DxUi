# RedTilde completion: current-source audit continuation — 2026-10-24

## Progress checklist

### Current 2fed qualification checkpoint — 25 October 2026

- [x] PR #102 integration: merged as `97b26bad7fcdb0aa3dbb2efd4bceac77aab5a295`; merged tree `2c0641bc363bf713cfc709d721c3f6549dd5aef8` is identical to reviewed DxUi source `2fed82465dcee9f213753754e0e8e54f353cf38f`.
- [x] Local matrix: six x64/ARM64 Release, Debug and ASan Debug builds passed; three x64 profiles each passed 19 ordinary suites with zero skips, the ASan use-after-free canary was detected separately, and ARM64 local profiles are cross-builds only. The custody packet covers 74 build artifacts and 317 payload streams.
- [x] Authorized foreground run: Menu, NativeTextInput, MenuResources and MenuResourceScaling passed in the x64 Release interactive lease; foreground, focus and cursor were restored. This does not qualify physical IME or assistive technology.
- [x] Hosted native checkpoint: all six native profiles passed in both the PR102 run and exact-head manual run. The PR compiled a synthetic merge of `2fed8246`; its tree matches the reviewed and merged-main trees. Manual run `40219420793` completed successfully, while PR run `40222621151` still had its paired benchmark pending at the audited snapshot. See the separate [PR](../../../../Measurements/RedTildeCompletion_2026-10-24/hosted-2fed-pr102-native/README.md) and [manual](../../../../Measurements/RedTildeCompletion_2026-10-24/hosted-2fed-manual-native/README.md) packets.
- [x] Historical CD35 paired evidence: publish its independently replayed data separately in the [CD35 packet](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-cd35-paired/README.md); the verdict remains inconclusive under an unqualified base policy. It is not pooled with the current 2fed packet.
- [ ] Performance and product acceptance remain open: the PR102 paired benchmark is pending; the historical paired verdict does not qualify the policy. The earlier `4faf16ed` ARM64 Debug Menu failure cause remains unexplained by later passing runs.
- [x] P10 integration follow-up: push and merge PR #101 as `4faf16ed`, whose tree exactly matches reviewed head `29ff0fd4`. Integration leaves performance and product acceptance open.
- [x] P10 cache counterfactual: preserve all 74 qualified SDK native artifacts across six profiles, then add the common default-format alias regression on unchanged `4faf16ed` production. The Release build passes and the focused run fails at the expected default-first pointer-identity assertion (`20261025T154703108Z-43392-a3537c62112942ac825031f0a32759ca`). No whole-feature gain is inferred.
- [x] P10 cache repair qualification: canonical default-format reuse passes both request orders, normalized sizes and distinct style/size/weight cases at `2fed8246`; zero-spacing layout/pixel parity and nonzero tracking also pass. The source-bound local and hosted matrices are complete as recorded above. The missing-font case runs conditionally on actual availability; no forced-font seam is claimed. RedTilde's three-reference study at `9025be4` reports no resolved change against its actual PR base and retains five original-UI flags, so no measured improvement or performance acceptance is inferred.
- [x] P10 zero-spacing Label counterfactual: the unchanged `cd35e988` x64 Release baseline parity case passes; the approved candidate skips only zero-value `SetCharacterSpacing` calls in Label measure and paint while retaining cached layouts and the nonzero HRESULT path. The candidate focused rerun is pending; do not infer a CPU or memory gain from the test's unpaired report.
- [ ] P10 zero-spacing Label qualification: commit the reviewed source/spec/test change, pass the focused candidate tests and required x64/ARM64 build matrix plus ordinary suites, then review paired performance evidence. Keep the candidate result distinct from baseline `cd35e988` and the separately observed hosted ARM64 Debug pass.
- [x] P10 integration: push and merge PR #100 as `dc67aed6` at the owner's explicit request; its tree exactly matches qualified documentation head `9a642c52`. This is implementation integration, not performance-policy or product acceptance.
- [x] P10 CI checkpoint: workflow `40187599911` passes static/tooling checks and x64 Debug/Release; at 11:46 UTC, remaining native profiles and paired collection are still pending. Earlier six-profile runtime evidence remains tied to `594d67fb` / compiled merge `8c78b5ad`.
- [x] P10 later native checkpoint: the same workflow subsequently passes all six native jobs, including ARM64 and ASan. Paired collection is still running at 12:31 UTC; this checkpoint covers merged implementation `9a642c52`, not the new timer diagnostic.
- [x] P10 control investigation checkpoint: retain the per-metric/control, chronology, timer and memory analysis for the hosted packets and separate timer cohort. The [current 9a control replay](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-9a-final/README.md) and earlier `594d67fb` investigation remain distinct evidence; neither qualifies controls or policy.
- [x] P10 control replay checkpoint: independently match the 9a packet's 144 raw hashes, 1,872 metric observations and all 72 statuses; publish the [9a projection](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-9a-final/control-summary.md). It reports 71/72 unstable controls and 248 breaches. The earlier `594d67fb` packet reports 69/72 and 307 breaches. These are different commits and must not be pooled. The b4 four-process timer diagnostic separately passes its 512-row/schedule and six CLI checks.
- [ ] P10 repair/qualification: verify any source or diagnostic fix with the required regressions and fixed matched-harness comparisons, preserving exact budgets, every raw receipt and the unqualified measured-base policy.
- [x] P10 repeated-work baseline: the retained pre-repair Release executable passes its focused non-activating case and reports 1,792 focus-resolution visits for 128 interested label changes. A narrow content-only coalescing patch is implemented with dirty-revision, synchronous general-focus, stale-path and text/toggle-query checks. The source-bound local x64 suite matrix and ARM64 cross-builds pass at `1714f8b0`; hosted native qualification is recorded separately below. Performance policy and product acceptance remain open.
- [x] P10 common-fixture counterfactual: the same final test source fails against actual PR #101-base production at 1,820 visits and passes against `1714f8b0` at 14. This is the comparable repeated-work result; the earlier 1,792 count uses the earlier fixture. The public [1714 local qualification packet](../../../../Measurements/RedTildeCompletion_2026-10-24/local-qualification-1714f8b0/README.md) records all 57 ordinary x64 suite receipts and all 15 ARM64 cross-build artifact hashes with source and attestation bindings. Neither operation counts nor cross-builds substitute for whole-feature performance or native ARM64 runtime qualification.
- [x] P10 later CI checkpoint: workflow `40187599911` finishes all six native jobs successfully, collects its paired packet, then fails verdict publication and `ci-gate` with an unqualified base policy. The independent new-packet replay finds 71/72 unstable controls and 248/1,872 breached observations; this packet is separate from `594d67fb` and does not qualify the policy.
- [x] P10 final hosted checkpoint: workflow `40197354712` finishes all six native jobs, all static/tooling jobs and the ARM64 ASan consumer checks successfully. Paired collection finishes, but verdict publication and `ci-gate` fail solely because the base policy is not independently qualified. The [new source-bound packet](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-1714-final/README.md) retains 144 receipts, 72 unstable controls, 384/1,872 breached observations and all 78 metric rows. This supersedes the earlier in-progress CI snapshot; controls, observer qualification and policy acceptance remain open.
- [ ] P10 handoff: synchronize domain contracts and changelog, review current hashes, push/merge reviewed follow-up and retain the consumer's hosted ARM64 gate if billing remains blocked.
- [x] Inspect current main, consumer pin, existing audit ownership and primary reference projects.
- [x] Independently re-read the source locations admitted below; distinguish current defects from old findings.
- [x] Route library work to existing W/T/C rows and consumer execution to one coordinator.
- [ ] W0.3: revalidate every remaining historical finding and completeness row against the implementation revision.
- [x] W1.1 bounded defect: reproduce and fix table-column memory safety and retained source/selection/history regressions; frozen `cfe875fd` profiles pass.
- [ ] W1.2/W1.5: implement local object fallback, failure-key memoization and correct semantic/UIA ownership.
- [ ] W1.6/W1.12/C4/C5/T1: correct accepted math behavior, implement TeX layout fundamentals and broaden conformance tests.
- [ ] W1.7/C6/C7/C8/T2: correct Mermaid semantics, expand native profiles and qualify model/layout/render/source behavior.
- [ ] W1.11/T3/T4: reconcile status data, tests/scopes and cross-host/fault/resource coverage.
- [ ] W3: measure and qualify semantic reuse; keep W4/W5 consolidation conditional on evidence.
- [ ] Close library gates and exact consumer handoff with current-source evidence; do not close parent HOLDs by inference.
- [x] Late focused checks: pass both Tree status/prepared-row Debug cases, the injected math Debug case and full Embedded suite, and the Label retirement case under x64 ASan Debug; final matrix and counterfactuals remain open.
- [x] baff43d8 final local SDK x64 matrix: all 19 ordinary suites pass with zero skips in Debug, Release and ASan Debug; the real ASan detection probe passes.
- [x] baff43d8 ARM64 cross-builds: Debug, Release and ASan Debug build successfully; this is not native ARM64 execution.
- [x] baff43d8 late counterfactuals and Tree regressions: six source-bound counterfactual variants and the late Tree cases complete; their evidence remains distinct from whole-source acceptance.
- [x] baff43d8 hosted paired evidence: publish all 78 independently replayed rows and source bindings; zero regressions do not qualify the unqualified base policy or unstable controls.
- [x] Diagnose the remaining be1ad177 Embedded ASan failure: the first publication's rows are independently enqueued, so the worker can gate Alpha alone and leave Beta's first value pending for coalescing. A longer wait does not make that batch atomic.
- [x] Validate the deterministic Tree fixture at `594d67fb`: all 19 ordinary suites pass in each x64 profile and all six hosted Full native steps pass, preserving ordinary deadlines, held silence, per-ID coalescing, pruning and exact counts.
- [x] Current local qualification: all six `594d67fb` builds pass; independently verify 74 artifacts, 57 suite/executable bindings and 288 payload files, with real ASan detection and zero ordinary capability skips.
- [x] Current hosted native qualification: workflow `40177964080` passes all six native jobs, relocated consumers and galleries. Optional STL-annotations-disabled consumers pass in both ASan profiles; menu-resource scaling passes in x64 Release only.
- [x] Current hosted evidence publication: independently replay all six Full payload inventories (496 files), source/probe bindings and the lead verification; publish the sanitized packet with exact head versus compiled merge identities and artifact-retention limits.
- [x] Current actual-base paired publication: publish all 78 rows/936 block effects, 144 receipts, both measured executable bindings and the lead replay for `594d67fb`. Zero flags coexist with 69/72 unstable controls, observer cadence breaches and an inconclusive policy gate; no acceptance is implied.
- [x] Current foreground menu/input: complete the agreed normal Start-confirmed four-suite run at `594d67fb`; verify zero skips, all 17 payload files, current executable bytes and restoration. The report's dirty-source inventory limit remains explicit.
- [ ] Close the remaining performance-policy/advice and product-acceptance gates; retain the failed be1ad177 ASan attempt and all earlier evidence. Current native/build/input completion is recorded separately above.

Status: **IMPLEMENTED; performance qualification and product acceptance remain open**. This appendix updates evidence
and sequencing for the [existing audit](../RichTextAudit_2026-10-21.md); its implementation work-item IDs remain
authoritative. The separate RedTilde repository's `Specs/Plans/WIP/RedesignCompletionAndRendering_2026-10-24.md`
contains the cross-repository sequence, compatibility test matrices, reviewed commands and closeout conditions.
This appendix adds no second defect backlog or independent acceptance policy.

### Current qualification — 25 October

SDK `594d67fb0fcc3829541211a7c8f100df046c87e3` passes all six local builds and all 19 ordinary suites in
x64 Debug, Release and ASan Debug, with zero capability skips and real sanitizer detection. Independent replay
verifies 74 retained artifacts, all 57 suite/executable bindings and 288 completed payload files without a hash
mismatch. The [local matrix packet](../../../../Measurements/RedTildeCompletion_2026-10-24/qualification-594d67fb/README.md)
retains source, runner, receipt, completion and executable digests. ARM64 local rows are cross-build only.

Hosted workflow `40177964080` passes all six actual Full native steps and complete native jobs, their relocated
public consumers and galleries. Both ASan profiles also pass the STL-annotations-disabled consumer; the other
four optional instances are skipped. Menu-resource scaling passes only in x64 Release; its other five instances
are skipped. The workflow compiles merge `8c78b5adf821daa216bee53572b3f9aae7d40ef7`, with first parent actual PR
base `b06f534b` and second parent head `594d67fb`; its tree equals the head's tree. Preserve the recorded compiled
merge in receipts. The [hosted native packet](../../../../Measurements/RedTildeCompletion_2026-10-24/hosted-594d67fb/README.md)
retains six matching provider ZIP digests, all 126 Full-suite outcomes, independent replay of 496 payload files,
both deliberate sanitizer probes and the lead's repeated verification. Hosted Full has 21 suites, including
Menu and NativeTextInput under the runner's owned lease; the local noninteractive catalogue has 19. The ZIPs do
not retain native build binaries, so their reported compiler/executable digests are not independently rehashed
binary artifacts; optional payload inventories are not separately replayed. The
[fresh paired packet](../../../../Measurements/RedTildeCompletion_2026-10-24/sdk-594d67fb/README.md) retains the
completed collection, all 78 rows/936 block effects and 144 receipt digests, with repeated independent arithmetic
and projection checks. Unlike the native ZIPs, its paired ZIP retains 24 EXE/PDB/library files; both measured
EmbeddedTests executable hashes match all 72 corresponding run receipts. Baseline receipts report a dirty
checkout with three named harness overlays but no complete dirty-path inventory; candidate receipts report a
clean compiled merge. Source/object and compiler/linker bytes are absent. It has zero regression flags and
69/72 unstable controls. The overall workflow fails its inconclusive policy gate. Observer replay places all
144 completions inside the sample span but finds 28 QPC and 29 UTC sample-start periods above the retained
2.5-second consistency bound; whole-machine weighted CPU is about 36.6%. Continuous/quiet-host coverage and
performance are not accepted. No new AA calibration or candidate policy self-approval is implied.

The bounded current math/Mermaid profiles remain the selected scope. The new fixture correction changes no
production code, API or product dependency, and the consumer remains pinned to production revision `baff43d8`.
The [current foreground packet](../../../../Measurements/RedTildeCompletion_2026-10-24/foreground-594d67fb/README.md)
records all four x64 Release menu/input suites passing with zero skips after normal Start confirmation and
verified foreground/focus/pointer restoration. Independent replay verifies all 17 payload files, the three
executed/preflight binaries and the matrix build identity. Its unpaired preflight records a dirty checkout;
audit-time changes are documentation/evidence only, but the report lacks a complete dirty-path inventory.
RedTilde's later documentation head `be41564` has byte-identical
six-profile app artifacts and its hosted run `40182090717` again executes no steps because of the account
payment/spending restriction. The owner-requested RedTilde hosted ARM64 gate stays open. Current R10/D4c cost,
control and policy advice remains open; the implementation plans therefore stay in WIP.

### Deterministic held-batch correction — 25 October

The `be1ad177` local continuation builds all six profiles and passes the x64 Debug and Release Embedded suites.
Its ASan Debug Embedded run `20261025T081258851Z-668-07d2fc3321c5433aa631f233e811e215` fails after the full
20-second gated allowance. The frozen client trace contains Alpha's first and final values and Beta's final value,
but no Beta first value; there is no sanitizer memory report. The queue moves currently pending properties to a
worker-owned batch before the diagnostic gate. Because rows enqueue separately, Alpha can be extracted while Beta
remains pending and is legitimately coalesced to its final value. This is a fixture-ordering defect, not evidence
that the library must deliver every intermediate status or that another timeout increase is appropriate.

The corrected fixture first receives both distinct first values under the unchanged 3-second ordinary deadline,
then holds one unrelated Gamma event before queuing the middle/final updates and removing Gamma's ID. A separate
held-event mark preserves the silence check; all original exact first/final, superseded-value, stale-ID and
replacement getter assertions remain. Gamma's already extracted event is outside pending pruning and is not an
asserted outcome. The existing 20-second gated allowance is unchanged. No runtime code, public hook or dependency
is added. New source-bound local and hosted validation was pending at this correction checkpoint; the current
qualification above records its completed native profiles. The failed attempts remain retained.

### Late SDK checkpoint — baff43d8 (25 October)

The final local SDK x64 matrix at `baff43d8` passes all 19 ordinary suites with zero skips in Debug, Release and ASan Debug; the real ASan detection probe passes. ARM64 Debug, Release and ASan Debug cross-builds pass, but do not establish native ARM64 execution. Six source-bound late counterfactual variants and the late Tree regression cases are complete; those focused results do not replace whole-source qualification.

Hosted workflow `40154492633` binds PR #100 head `baff43d8` and actual base `b06f534b`. Four ordinary native jobs pass. Two ASan Embedded gated batches time out on the existing 3-second ItemStatus notification wait; no sanitizer memory report was produced. The later `be1ad177` test-only correction uses the existing 20-second request allowance, but its local ASan attempt still fails with a missing Beta first value, as recorded above. The deterministic fixture correction and new validation were open at this checkpoint; the current qualification above records the completed native profiles. Corrected test executables are distinct artifacts; the test edits do not change the measured production source.

The hosted benchmark still binds production source fingerprint `F423000D1AD799EDBAA3EE17B121480C5AE567CC75E2BCE2AF0D783AC7AB26F1` and the same six workload inputs. The canonical [SDK baff43d8 hosted packet](../../../../Measurements/RedTildeCompletion_2026-10-24/sdk-baff43d8/README.md) retains all 78 paired rows and independent numeric/integrity replays. It reports zero regressions, but 71/72 same-side controls are unstable and the base policy is unqualified; no performance acceptance is claimed. The baseline has a clean fingerprint match from prior verification but is marked dirty with three known harness overlays and no complete dirty-path inventory.

This owner-selected work remains limited to the bounded current profiles. Broader compatibility families and full font assemblies are deferred. Physical/foreground input, native ARM64 execution and consumer integration gates remain open; no full-parity, release or repository-wide acceptance claim follows from this checkpoint.
### Frozen-source checkpoint — 25 October

DxUi `cfe875fd815b0a39336100df9bde8f29ea2f0aa0` and RedTilde `fd24623a9f27d04b85deb27a43a9af724dc030ca`
each pass six local builds. SDK x64 Debug, Release and ASan Debug pass all 19 ordinary suites with zero skips,
including a real ASan detection probe. The app's corresponding profiles pass 54 cases and 23 captures, plus their
recovery/crash probes. The owner-started app keyboard and four SDK foreground suites pass with desktop restoration.
SDK hosted workflow `40135398414` passes all six Full native profiles, relocated consumers and galleries, with
optional per-profile skips recorded separately. Its paired collection completes but the verdict is inconclusive:
the measured-base policy lacks independent qualification, so the overall workflow fails. RedTilde's hosted
workflow never started because of the account payment/spending restriction. The owner explicitly chose to keep
its native ARM64 gate open. A cross-build is not native execution.

The fresh app comparison retains 384 native runs and 16,003 independent checks. The original-UI cohort has five
regression flags: Compose command +12.54% and private memory +2.013%; Split command +14.58%, owner CPU +13.44%
and working set +2.61%. Only the flagged Split working-set control is stable. Against the actual starting redesign,
all twelve A/B slots have zero regression findings, including three improvements. Host observations are complete
but loaded, not verified quiet. The sixteen-process subphase diagnostic and 24-process hardware/WARP resource
study remain separate descriptive evidence; neither accepts the cost or attributes a retained allocation.

The current bounded math and flowchart/state/class profiles are implemented and exercised on those named frozen
profiles. Full TeX font assemblies, additional diagram families and MM-36's disclosed marker-geometry limit retain
their existing owners. A later review is correcting Tree badge ItemStatus publication and the consumer's Files
dirty transition, and is completing native-object HRESULT fault classification/coverage. Those edits require
new red/green runs and source-bound qualification. The same review also found Label text/multiline setters
continuing after synchronous embedded preparation can retire their root; weak-lifetime guards and extensions to
the existing retirement case are patched but await separate sanitizer diagnostics and final-source evidence.
The same-executable SDK A/A calibration completed at `cfe875fd` in one separate physical worktree, retaining
144 receipts and 7,158 observer samples. Its independent audit recomputes all 78 slots and the observer passes
coverage/continuity checks under about 25% system CPU load. All 24 within-side control comparisons are unstable
in each A/A scenario; the completed collection does not qualify a noise floor. The retained hosted actual-base comparison also
recomputes all 78 slots/144 receipts with no resolved regression and predominantly unstable controls. The dirty
baseline's library fingerprint exactly matches the clean actual PR base under Windows checkout attributes;
three tooling overlays are recorded outside that scope, but no complete dirty-path listing is retained.
The AA legacy/migrated disagreement is
improved versus within-noise, with zero regressions in both judges. Policy remains unqualified and no result
may be relabeled as execution of later source. All six hosted Full payload inventories are independently
byte-replayed; the two intentional ASan detection probes are validated separately from ordinary suite exits.
The [canonical frozen SDK packet](../../../../Measurements/RedTildeCompletion_2026-10-24/sdk-cfe875fd/README.md)
retains all 156 metric rows, role/build/harness digests, control instability and independent-audit bindings.

### Late-source focused checks — 25 October

Both Tree client cases start and finish in Debug run
`20261025T021304993Z-27628-17cf022cb9d24837bad2d536b899bb18`, with zero skips. The preceding original-source
counterfactual reproduces missing dot-badge ItemStatus. A first fixed-source attempt failed because its style-only
test incorrectly expected an empty getter value when no event was expected; that test-authoring failure is retained.
The corrected test keeps the current badge value and expected event separate.

The injected native math containment case passes in Debug run
`20261025T015619165Z-47744-17c82175da7846ef9b9048f8a2372f5b`; the unfiltered Embedded suite passes 455,561 checks
in `20261025T015815367Z-45528-48a04e2b701849a08533f314b035b44d`. The Label text/multiline retirement regression
passes with zero skips under x64 ASan Debug in
`20261025T022224940Z-19880-ef222af848c049d0a63c5171d80a5a17`. Canonical build/executable and official run/payload
bindings are retained beside the private diagnostic attempts. The independent late-focused audit verifies the
Debug receipts and both Tree case completions; it does not claim the separate sanitizer run.

These are focused late-source results. Remaining math and Label counterfactuals, the final source matrix,
fresh foreground runs, current-source performance comparisons and external ARM64 gates remain open. No later
source inherits the frozen `cfe875fd` performance result or qualified policy.

The earlier checkpoints below remain revision-bound history. Do not promote their passes to the later changes,
erase their failures, qualify candidate policy from itself, or close the parent HOLDs by inference.

### Execution evidence — 24 October

The combined Debug diagnostics reproduce and repair table/source-history corruption, local semantic/geometry fallback,
math ownership/limits/recognition and prepared placement, Mermaid fan-out/state-label semantics, hollow inheritance and
capsule rendering. All 105 pinned flowcharts and the 20-family outcome matrix remain represented. The bounded math
profile has 30 positive formulas, 20 negative forms and 27 symbol goldens; full KaTeX or Mermaid parity is not claimed.

`combined-14` passes full EditorControls (69.9 s) and Embedded (55.2 s; 455,446 checks), plus the expanded parser and
layout/paint allocation sweeps. The initial Debug failure-injection termination and its exact later reproduction are
preserved, including the unavailable original binary boundary. `combined-16` characterizes interested focus lookup:
66,048 visits before, 1,792 after for 128 mutations among 512 unrelated siblings, with stale/hidden focus assertions.
The real ProgressBar range-client regression fails before and passes after publication changes. These diagnostics
do not replace final-source Debug/Release/ASan profiles, ARM64 builds/native execution, or paired performance acceptance.

The interactive lease setup denial regression fails before admission is made mandatory. Its descendant regression
exposes job accounting reaching zero before a process handle signals; retained verified member handles correct this
observed completion race. `combined-19` passes all 98 lease self-checks. A first direct Accessibility invocation omitted
`--no-activate` and failed its native-focus invariant; that invocation and exact executable are retained separately.
The governed rerun and subsequent final profiles own their own results. Seven historical whole-feature performance
regressions, including approximately 11% higher private memory, remain open pending a fresh actual-base comparison.

The governed full Accessibility rerun passes in 123.6 s. Both repositories' static/tooling validators pass; the separately
selected installed CMake also passes all 12 triplet checks. The owner selected finishing current profiles and qualification,
leaving full TeX font assemblies and additional Mermaid families in the existing language roadmap. That scope decision
does not close their W/T/C items or turn the bounded profiles into full KaTeX/Mermaid compatibility.

### Current bounded dispositions and qualification checkpoint

The profile implementation is `f5a2cea9`; `94bf9801` adds the lease source-contract correction and `8a38e598`
adds the gallery fixture repair. At `8a38e598`, all six official SDK builds pass. All 19 ordinary x64 suites pass
in both Debug and ASan Debug with zero skips, including a real ASan detection probe. The earlier SDK94 Release
19-suite and four foreground-suite passes remain separately bound to their source and binaries. Hosted ARM64
Debug/Release native suites pass; ARM64 ASan's native step fails and is under investigation. Later consumer
steps do not erase that failure.

The next host source snapshot (`866fb8fc62eb06756f65845e5130a10a8e8bb4fb3a805deba0ebd0ccc7713187`)
passes all 14 targeted Release cases in `host-regressions-green-v2` at 21:29 UTC. The prior epoch diagnostic
reproduces unnecessary full-tree work after unrelated presentation changes, while its corrected tab fixture passes.
The production run covers bounded/reset-scoped submissions, native-paint and capture failures, tree mutation,
live moved/expired observers, reentrant cleanup and final-state cache validation. These focused passes do not
replace the final native/platform matrix, foreground suites or fresh whole-feature performance comparisons.

| Existing owner | Completed bounded portion | Remaining boundary |
| --- | --- | --- |
| W1.1 | Table-column zero/short-row bounds and retained source/selection/history regressions | Other broad structural roadmap items retain their original owners. |
| W1.2/W1.5 | Per-formula fallback, failure-key retry bounds, independent Math ownership and parent link title/destination preservation | Do not infer completion of the whole shared native-object admission-ledger roadmap. |
| W1.6/C4/T1 | Explicit delimiter recognition, glyph identities, modifier/script binding, shared root/matrix budgets and typed atom/style/limits layout | Full command/font assemblies and the complete target inventory remain open. W1.12's separate geometry/DPI requirements are not closed by atom-layout work. |
| W1.7/C6/C7/C8/T2 | Current flowchart/state/class grammar, descriptions, fan-out, stadium ports and typed hollow inheritance with directed model/layout/render/source tests | The broader family, routing, wrapping/style and language tracks remain open. |
| W1.11 | Bounded math status/limits and limited state/class matrix truth repaired together with their tests | The full documentation/gallery/platform closeout needs final-source evidence. |

The actual-base same-SDK app comparison and independent audit are complete at `fa0757d`; seven regressions remain,
including +12.23%/+12.68% private bytes in Compose/Split. The separate starting-redesign cohort resolves no change.
Those records are retained without accepting a cost or qualifying D4c. New production memory/latency fixes trigger
new fixed-scope studies; they do not replace or pool the older observations.

## Revision and drift

Planned at DxUi `ceffe76a429d15039ceff1f7dfae7e1d7126323e`, clean main. RedTilde main is
`c7456854c8f964bb9ea067295e6435f99d426515` and pins `c600993861a337012e2633356c3d35c5e129bbf3`, API 4.
At inspection there is no diff under `src`, `include` or `Tests` between that pin and the current DxUi HEAD.
Do not confuse source equality with exact binary/environment/runtime-receipt equality.

```powershell
git diff --stat ceffe76a429d15039ceff1f7dfae7e1d7126323e..HEAD -- src include Tests ThirdParty Tools
git status --short
./Test-Changes.ps1 -Explain
```

Compare every changed excerpt and uncommitted path before implementing. Existing finding locations refer to
the old audit revision unless explicitly revalidated here. The old 278-entry classification is historical; it
does not prove every finding survives current main. Keep the 20 refuted and all disputed entries visible.

## Current-source findings rechecked by the lead

| Existing owner | Current source | Verified behavior and required regression | Effort / fix risk / confidence |
| --- | --- | --- | --- |
| W1.1 / commands-01 | `src/Controls/DxUi.RichTextCommands.cpp:1904–1936` | Semantic header has multiple columns, body can have one raw cell; column zero deletion reaches `slots[column - 1u]`. Test short/padded/pipe-less rows, containers, exact result/caret and one undo/redo transaction. | S / medium / high static |
| W1.2 / math-parser-01, math-layout-01 | `DxUi.NativeMath.cpp:69–100`; `DxUi.PreparedMath.h:332–340`; `DxUi.RichTextEditor.cpp:1337–1347` | A text leaf can contain a newline; preparation demands one line and can reject the full semantic document. Reject/normalize under the specified grammar and contain any residual failure per formula. Test other rich content remains rich and idle does not retry indefinitely. | M / medium / high static path |
| W1.5 / semantic-03 | `DxUi.RichSemanticDocument.cpp:341–345` | Rejected math stores its diagnostic in `visit.parent.title`. Test a rejected formula in a titled link and assert link title/destination, math alternative and client-observed UIA HelpText separately. | S / low / high |
| W1.6 / math-parser-08; C4 | `DxUi.NativeMath.cpp:240–241,389–412` | Limits/nolimits are Space atoms; scripts attach to that atom. Use an operator modifier or explicit Unsupported until placement is correct; assert AST binding and actual glyph origins. | M / medium / high |
| W1.6 / math-parser-02/03 | `DxUi.MarkdownInlines.h:1215,1218–1231` | Dollar whitespace/digit rules also apply to explicit slash-parenthesis delimiters. Add spaced slash notation, a following digit and candidate closers inside code/HTML; characterize precedence against the pinned contract before fixing it. | M / medium / high for shared heuristic; precedence needs tests |
| C4 / MA-01/02/22/37 | `DxUi.PreparedMath.h:41–52,62–84,303–310`; `DxUi.NativeMath.cpp:365–368` | Generic text/upright defaults and fixed layout ratios do not establish TeX atom spacing, italic correction, display/text/script styles or operator limits. Add these fundamentals before broader accepted commands. | L / high / high compatibility gap |
| W1.7 / mermaid-01 | `DxUi.NativeFlowchart.cpp:69–119,253–277,290–298` | State alias and description paths bypass common label validation. Alias assigns description; colon appends description to implicit ID. Assert equivalent normalized labels and common markup/control/capacity rules. | M / medium / high |
| W1.7 / family grammar | `DxUi.NativeFlowchart.cpp:147–165,238–250,329–334` | Generic node shapes and pipe labels are shared across families; direction uses a fixed whitespace spelling. Add independent family grammar cases and precise Unsupported/Malformed results before broadening admission. | M / medium / high for code sharing; grammar verdicts need pinned oracles |
| W1.11 / status-truth-01/03 | `ThirdParty/Markdown/profiles/DxUiMath1-target.json`; `mermaid-11.4.1/dxui-family-matrix.json` | Math status says not implemented; state/class families are labeled unsupported despite native subsets. Effective math matrix cap is 128 cells, although public comment says 16x16. Reconcile data, public header, docs and assertions together. | S / low / high |
| T1/T2/T4 | `Tests/Controls/Scope.Tests.RichSemanticDocument.cpp:14–99`; `Tests/Support/Scope.Tests.MarkdownBlocks.h:365–400` | Math tests cover broad parsing and alignment but not TeX fundamentals; flowchart corpus mostly outcomes/counts and special-cases state inside unsupported families. Add semantic/geometry/editor/resource oracles, not more count-only success cases. | M–L / low / high test gap |

All paths in the table without a directory prefix are under `src/Controls`, except named ThirdParty/Tests paths.
No new native reproducer ran during planning; the table deliberately distinguishes static evidence from runtime
qualification. Record fail-before/pass-after evidence as implementation starts.

## Exact work packages and dependency order

1. **W0.1–W0.3:** retain baseline and revalidate critical source/finding drift before fixes. The full file/finding
   sweep may continue alongside focused correctness work; finish it before broad refactoring or claiming a complete
   review. The coordinator's inventory covers tracked first-party code/build/test inputs; initial state is pending.
2. **W1.1/W1.3:** memory safety, source corruption and admission accounting. Use existing command/parser fixtures
   and native/embedded retained editor undo tests; preserve the original 1,324 CommonMark/GFM semantic vectors.
3. **W1.2/W1.5:** local failure containment and one truthful object admission ledger. Amend the owning failure/
   fallback contract; preserve previous complete publication, explicit transient-device retries and allocation policy.
4. **W1.6/W1.12 + T1 + C4:** math parser/recognizer corrections, then atom/style/MATH layout correctness. C5 breadth
   follows the fixed fundamentals. Add AST and glyph/line oracles before increasing supported rows.
5. **W1.7 + T2 + C6/C7:** correct current Mermaid grammar, then graph/routing/shape/subgraph parity. C8 expands
   state/class, sequence, ER/pie/gantt/mindmap and the remaining pinned families through explicit matrix rows.
6. **W1.4/W1.8/W1.10 + T3/T4:** input, embedded host, UIA and full source/fallback/lifetime integration. Add missing
   Accessibility scope mapping for rich/public-header changes; native and embedded must run the same semantic cases.
7. **W3.1–W3.6:** reuse geometry/blocks/objects, then measure on unchanged fixtures. W4/W5 are conditional
   consolidation; introduce adapters before collapsing pipelines. Do not require a universal object abstraction
   to deliver a small correctness fix or a bounded new family.
8. **Library/consumer handoff:** all relevant x64 configurations, six builds and separate native/input/paired gates.
   Update exact-pin consumer evidence in RedTilde; leave other consumer migrations with their current owners.

Waves sharing `DxUi.RichTextEditor.cpp`, `DxUi.RichSemanticGeometry.cpp` or the parser units must serialize their
edits. Independent engine fixtures can be researched in parallel; each cheaper executor gets exact scope,
test commands and stop conditions. The lead verifies the final diff and independently inspects each new oracle.

## Compatibility and test contract to implement

Keep old reference bytes stable: Mermaid 11.4.1 commit `9868f3a4c3784e214d8b6c74277ca542774e7de9` and KaTeX
0.16.11 commit `fbbea3573439d51e6568356f5886053b111b1f88`. Live docs inspected on 24 October show newer
versions; any refresh is a versioned corpus migration. Every row needs a stable ID, provenance, source example,
expected status/reason/range, normalized semantic model, applicable geometry/render oracle and native/embedded/
consumer test IDs. Separate policy exclusions, malformed syntax, valid unsupported syntax and admitted behavior.

- Math: atom classes/spacing, variable/operator fonts, style propagation, scripts/limits, fractions/roots/accents/
  delimiters, matrix/cases/aligned/gathered layout, explicit display envelopes, Unicode and source spans.
- Mermaid: every original flowchart/family fence, then accepted shapes/edges/labels/subgraphs, state/class
  equivalences, sequence semantics and each expanded family's typed model. Include multiple directions, cycles,
  disconnected/parallel/back/self edges, long labels and shape-aware endpoint/routing invariants.
- Both: exact-source local fallback, parent semantic ownership, hit/caret/selection, F2 apply/cancel/undo/redo,
  UIA client traversal and event/name/help assertions, high contrast, RTL, fractional DPI and clipped ink.
- Limits: old and proposed boundary cases, invalid UTF-16, control characters, source/node/record/depth/geometry
  bounds, allocation failure at every observed point, cancellation/replacement during each stage, same numeric
  revision with changed immutable source, device/font/width changes and clean/hidden idle work counts.
- Security/policy: no script/directive/action execution, file/process/network/package access or user macro
  expansion. An external reference tool is an offline development oracle, never a shipped helper invoked by a document.
- Expected output comes from independent specification/reference/hand-checked data. A candidate parser must not
  generate its own expected AST/geometry. Unsupported status for all rows is not evidence of supported compatibility.

Existing test anchors are `Scope.Tests.RichMarkdown.cpp`, `Scope.Tests.RichSemanticDocument.cpp`,
`Scope.Tests.RichSemanticGeometry.cpp`, `Scope.Tests.RichTextEditor.cpp`, `Scope.Tests.RichTextAccessibility.cpp`,
and Support/Embedded semantic/resource fixtures. Reuse shared preparation/heap/fault helpers before adding one.
Register new `Scope.Tests.*` files in `Tests/native-test-files.json` and update test scopes/callers/project inputs.
Changes to `Tools/Generate-NativeFlowchartTestData.ps1` or new tooling also require the Tools policy/inventory/help
and offline negative/generation tests; keep this planning pass free of tool changes.

## Reference projects and adoption decision

The owner explicitly excludes new dependencies. Extend owned C++ mechanisms, studying concrete algorithms and
independent test ideas. MicroTeX, Mermaid renderers and KaTeX are reference material; replacement engines,
Rust/FFI backends and imported rendering or font packages are outside this execution.

- [mermaid-rs-renderer at 3726ccb](https://github.com/1jehuang/mermaid-rs-renderer/tree/3726ccbffe0e8032361eb9668694b24f77858060):
  study stage invariants and route-label obstacles. Its [manifest](https://github.com/1jehuang/mermaid-rs-renderer/blob/3726ccbffe0e8032361eb9668694b24f77858060/Cargo.toml)
  identifies Rust 2024/version 0.3.1/MIT; embedding it would add build/ABI/source/UIA obligations.
- [MicroTeX at 0e3707f](https://github.com/NanoMichael/MicroTeX/tree/0e3707f6dafebb121d98b53c64364d16fefe481d):
  study row atom classes/kerning and delimiter glyph assemblies. Its [README](https://github.com/NanoMichael/MicroTeX/blob/0e3707f6dafebb121d98b53c64364d16fefe481d/README.md)
  documents C++17/CMake/GDI+ Windows integration. Main MIT licensing does not settle all font/data redistribution.
- [KaTeX supported functions](https://katex.org/docs/supported.html) and [options](https://katex.org/docs/options.html):
  use as semantic references; pin corresponding source and configuration for a comparison. Macro/trust defaults
  from a web renderer do not override the library's finite non-executing notation policy.

## Reviewed commands and exit criteria

From `D:\DxUi`:

```powershell
./Test-Changes.ps1 -Explain
./test.ps1 -Platform x64 -Configuration Debug -Suites EditorControls,Embedded,Accessibility
./test.ps1 -Platform x64 -Configuration Release -Suites EditorControls,Embedded,Accessibility
./test.ps1 -Platform x64 -Configuration 'ASan Debug' -Suites EditorControls,Embedded,Accessibility
./validate.ps1
./format.ps1 -Check
./Test-PrePush.ps1 -Explain
```

Use the broader affected Control/Tree/Theme/Rendering/WindowHost suites for shared presentation changes.
Final qualification requires full applicable x64 tests, six builds, real sanitizer detection, recorded WARP/device/
clean/dirty/hidden resource evidence, accepted paired comparison, and native ARM64/input obligations under the
existing contracts. The coordinator gives the current RedTilde commands and final pin tests.

No interactive suite runs without the applicable agreement to time and the governed restoring lease. Preserve
still-applicable human authorization; a historical successful run alone is not present scheduling agreement.
No runtime, gallery, platform or performance claim is established by this plan's static validation.

### Current source and evidence closeout — 25 October

The content-only accessibility mutation change is at source revision `1714f8b0c0fa628bcef56de76078f85c869fc50c`.
The local x64 Debug, Release and ASan Debug profiles each pass all 19 ordinary suites. ARM64 Debug, Release and
ASan Debug cross-builds pass; these are not native ARM64 execution. The hosted Debug and Release ARM64 profiles
pass. The [public local qualification packet](../../../../Measurements/RedTildeCompletion_2026-10-24/local-qualification-1714f8b0/README.md)
records all 57 x64 suite/executable matches, their current executable/build-attestation digests, and all 15 ARM64
cross-build artifact hashes and PE architectures. Hosted CI run `40197354712` targets this source: all six Full
native steps pass. The [completed hosted packet](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-1714-final/README.md)
supersedes its earlier in-progress snapshot: all six native jobs and consumer checks pass, including the ARM64 ASan
annotations-disabled consumer. The paired verdict remains inconclusive because the base policy is unqualified.
Independent replay retains 72/72 unstable controls, 384/1,872 breached observations, 30/78 metric keys held in all
controls and no key breached in all controls. The observer records shared-host activity and coverage gaps; it does
not establish a quiet host or isolate one cause. No threshold, fixture or baseline change is accepted from these
observations. Interactive qualification remains open.

The separately retained [9a hosted control replay](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/hosted-9a-final/README.md)
is inconclusive because the base policy requires review; its 71/72 unstable controls and 248/1,872 breaches do not
qualify the policy or establish quiet-host performance. The older `594d67fb` packet's 69/72 result is a different
commit and remains separate. The independently audited [1714 timer-floor aggregate](../../../../Measurements/RedTildeCompletion_2026-10-24/controls-investigation-20261025/clean-prepare-timer-1714f8b0-public-summary.json)
contains 512 rows, four raw report hashes, six passing CLI contract checks, recomputed group totals, and four
manifest-verified frozen artifacts. It retains the empty-loop reference separately and makes no latency, acceptance,
quiet-host, or cross-cohort pooling claim.

The earlier b4 timer executable SHA was checked before and after its diagnostic interval, but the executable itself
was not archived before the later rebuild. The current 1714 frozen artifact packet does not repair that b4 custody
gap; the two diagnostics and their evidence chains remain separate. The performance-policy, consumer, and interactive
gates remain open.

Stop a slice on mismatched source, an unproved bug, an unplanned public ABI change, unsupported font/license/
resource assumptions, incompatible baselines or repeat verification failure. Do not weaken tests/rebaseline to
finish. Complete work-item code, regression evidence, normative docs, capability status and the required folded
changelog together. The parent audit, rich-editor M5/M6/D4c and shared qualification close only under their own
criteria; app publication is not library acceptance.

### Configured text-format cache alias — 25 October

The test-only counterfactual on source `4faf16ed647df71cd7d5623c1682738592771c3e` reproduces the duplicate role-default format: x64 Release builds, then the focused WindowHost case fails at the expected pointer-identity assertion (`20261025T154703108Z-43392-a3537c62112942ac825031f0a32759ca`). After the cache-key alias implementation, the x64 Release rebuild passes and the same focused case passes (`20261025T160700740Z-7252-f60c20a641c04905ad6fae5ca3418aed`). This run also emits an unpaired offscreen performance receipt; it is not paired optimization evidence. The six-profile build matrix, x64 Full ordinary suites, paired performance validation, and interactive/platform obligations remain open.

### Zero-spacing Label mutation — 25 October

The reviewed implementation skips `IDWriteTextLayout1` queries and `SetCharacterSpacing` calls only when a Label typography override's character spacing is zero. Nonzero spacing retains the previous query, mutation and failure handling; retained layouts and cache identity are unchanged. DirectWrite's zero default was checked on a fresh layout at both the first and last text positions, and a test-only parity case on the unchanged `cd35e988` source passed in x64 Release (`20261025T171341303Z-11288-66b6a0e4678547d8b999ac5d0f0dfa47`). It compares RTL wrapped metrics and role-default versus exact-default override pixels for LTR, wrapped RTL and mnemonic text, while checking that nonzero tracking remains visible. The x64 Release build and this baseline parity case passed before the production change. The candidate build and rerun are pending; the six-profile matrix, ordinary Full suites, paired performance validation, and interactive/platform obligations remain open.

The baseline test run also emitted its standard unpaired complex-UI report; it is not optimization evidence. An initial test invocation used the nonexistent suite name `IntrinsicLayout` and was rejected before running tests; the corrected `Control` suite passed. A first, superseded test overlay failed to compile after including unintended fixture edits; that source/log is preserved privately and is not a product failure. The corrected test-only overlay is the sole baseline test change. Four interactive suites passed on the unchanged `cd35e988` source (`20261025T162748129Z-13244-69ae807e8df24109ad5544547bc053ff`); this is source-bound baseline proof, not candidate interactive qualification. A separate hosted ARM64 Debug success is not attributed to this candidate source. Current CI/profile and all consumer gates remain separately tracked.
