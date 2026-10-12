# UI composition qualification and consumer adoption

- **Status**: ACTIVE; unfinished acceptance split from the original 4 October plan on 11 October 2026.
- **Scope**: accepted S0-S2 performance/policy, native-platform and physical-input qualification, plus separately owned
  consumer pilots and pins. S0-S2 library implementation and recorded local x64 validation are complete in
  [Done](../Done/ConsumerUiImprovements_S0_S2_2026-10-04.md).
- **Deferred implementation**: [S3-S7 proposals](ConsumerUiImprovements_S3_S7_2026-10-11.md) are unimplemented and
  require separate scope acceptance. This qualification plan does not authorize those subsystems.
- **Ownership**: DxUi retains library measurements/platform qualification; consumer product behavior and adoption
  measurements remain in each application repository. No consumer is changed implicitly.

## 1. Authority and evidence boundaries

The original plan's S1/S2 exit evidence combined implemented library work with broader acceptance. The requested
split closes the library record while preserving all incomplete performance, platform, physical-input and product
obligations here. Domain contracts remain authoritative: [performance/resources](../../Core/Core_PerformanceAndResources.md),
[validation](../../Testing/Testing_Validation.md), [input/accessibility](../../UI/UI_InputAndAccessibility.md),
[controls/layout](../../UI/UI_ControlsAndLayout.md) and [consumption](../../Build/Build_ToolchainAndConsumption.md).

The [original native/form/Grid packet](../../../Measurements/Layout/2026-10-10/README.md) and later
[automatic-Start packet](../../../Measurements/Performance/2026-10-10-profiling/validation/auto-start/README.md)
retain exact local source/build/suite identities. Six builds, 57 noninteractive and 12 foreground x64 executions at
the latter frozen source pass with zero native skips and verified restoration. Cross-builds remain separate from
native ARM64 runtime; synthetic UIA, Unicode/reflow and WARP do not qualify real IME, screen readers, touch or hardware.
Original UIA extra-event and native-input setup cause investigations remain in
[ProductionReview](ProductionReview_2026-10-05.md), rather than being recorded as proved fixes here.

## 2. Paired performance and policy acceptance

- [ ] Establish accepted paired non-regression and a qualified trusted policy for the S0-S2 candidate.
- [ ] Attribute or resolve the retained timing/memory excursions and timing/FPS flags using matched fixtures and
  stable controls; retain measured developer advice where degradation is confirmed.
- [ ] Complete the required hosted paired gate when the PR changes what its benchmark measures. An enabled pending
  gate remains pending until its actual result is successful; a candidate-only diagnostic cannot qualify it.
- [x] Harden the production paired gate's role-specific summary/raw provenance binding and absolute whole-frame
  allocation checks before policy qualification; independent integration review checks both on the retained raw reports.

The repaired [v2 A/A repeat](../../../Measurements/Performance/2026-10-10-aa-v2/README.md) completes all 144 reports,
but all 72 controls remain unstable. It does not qualify the seed policy or establish performance acceptance for
S0-S2; trusted-base requirements and thresholds stay unchanged. The original retained baseline's drift remains a
diagnostic and cannot be relabeled a policy qualification.

The fresh [12 October clean-harness A/A attempt](../../../Measurements/Performance/2026-10-12-aa-qualification/README.md)
at `0bd0e99c` uses one attested x64 Release/WARP executable for both labels, 12 blocks and all 144 receipts.
Both judges replay from the retained bytes, but every same-binary control remains unstable (72/72), 54/78
timing/memory cells drift, and the judges disagree on three timing slots. Absolute clean/dirty/composition
allocation limits hold. The quiet-window agreement is not a continuous host-load trace. The base policy remains
`policy-review-required`; stable-control attribution remains outstanding. The separate
[hosted A/A packet](../../../Measurements/Performance/2026-10-12-aa-qualification/hosted/README.md) at the same
commit has 71/72 unstable controls and 39/78 drifting timing/memory cells. Its paired diagnostic job succeeds,
but strict/required-policy replay is inconclusive and the upload lacks executable/build bytes for independent
rehashing. This is still no qualified trusted policy.

S0-S2 implementation merged normally through [PR #73](https://github.com/RedSalamanders/DxUi/pull/73), remote merge
`670207429d62ecc541769b5dd6c70b9607745e64`. The [final PR packet](../../../Measurements/CiBudget/2026-10-11-cold-build/final-ci.md)
retains the failed aggregate at candidate `4bd34801`, base `ec680e86`, actual tested merge `80d875a1` and tree
`e695cf74`. Root and an independent reviewer reproduce all 144 reports, both complete 78-slot method decisions,
72 controls and 24 exact budgets with raw role/overlay/profile binding and absolute allocation checks. Both methods
contain zero regression verdicts, but all three scenes disagree between legacy clean preparation `improved` and
migrated `within-noise`. All original agreement flags stay false; policy, frozen reviewer and aggregate remain
unqualified/BLOCKED/failed. This explicit review closes implementation integration only.

Current controls are unstable in 69/72 comparisons. Retain Distinct-Grid B06's four elevated composition rounds:
paired clean composition +9.211%, Holm p=1, and same-side B05-to-B06 control +759.064%. Also retain Default dirty
private peak +0.531666%, 11/12 positive blocks, Holm p=0.025391, median +92,160 bytes and its same-metric B17-to-B18
control +3.964681%. All 28,800 clean preparation samples are exact 100 ns QPC multiples. These observations require
bounded attribution with matched fixtures and stable controls; none proves causal regression, noise or accepted
non-regression. Earlier Grid/FPS flags remain open separately. Independent review does not repair the producer gaps
or qualify the policy. Hosted uploads omit compiler/SDK/dependency/executable inventories; declared full-v2 fields
cannot substitute for independent rehashing of absent bytes.

Integration review identified two producer-check gaps: the diagnostic unchanged-library classification read
summary fingerprints without binding them to each scheduled role's raw receipts, and relative exact-budget
comparisons did not enforce absolute clean/dirty allocation limits when both sides violated them equally. The
production qualified gate now binds each scheduled role's commit/fingerprint, each run's source state and
executable/harness hashes, and its build profile to the hashed raw receipt. Every raw round must meet the fixed
zero-clean and Release 64-per-frame (Debug/ASan Debug 320-per-frame) dirty allocation limits. Synthetic forged
summary and rehashed raw-receipt cases verify these refusals. This is a tooling qualification stage, not a passing
paired measurement or policy qualification. Independent integration review still binds every role, commit,
fingerprint, executable/build identity and overlay state on the retained packet. A baseline made dirty by the
supported current-harness overlay must retain that literal provenance rather than be mislabeled clean. The change
is nonvisual; `docs/performance.md` and the normative performance contract are updated, while gallery pixels need
no regeneration.

The later [bounded profiling/automatic-Start follow-up](../../../Measurements/Performance/2026-10-10-profiling/validation/auto-start/README.md)
retains a comparison with ten timing/FPS flags and about 14% lower clean/dirty FPS, plus two declared candidate-only
controls. Those controls do not qualify baseline-side controls or dismiss the flags as noise. Neither the unchanged
library source in that narrow follow-up nor fresh passing native tests establishes paired non-regression or causality.

The eight-field Manual/Form diagnostic has matching pre-timing pixels/geometry/metadata, zero unchanged/composition
work, 7 KiB extra warm retained plain C++ Form memory and zero tracked plain-live bytes after fixture destruction.
These are bounded candidate-only diagnostics, with no speedup, consumer, hardware or policy-acceptance claim.

Capture current Default/MultilineGrid/MultilineGridDistinct benchmarks before implementation and run them on every
candidate as regression guards. For a new fixture, run the same fixture against both designs under the same harness;
do not invent a zero-cost baseline for a new feature with no equivalent. If only candidate feature costs can be
measured, report them as budgets, separately from regression evidence.

Follow [the paired procedure](../../../docs/performance.md): matched machine/configuration/DPI/renderer/fixture,
at least twelve independently drawn ABBA/BAAB blocks, two runs per side in each block, same-binary noise controls
and reviewed receipts. Current investigation bands are 5% for timing/FPS and 2% for process memory, with statistical and noise
qualification. Existing deterministic composition, surface, cache and hidden-work budgets remain hard limits.
Confirmed degradation requires measured optimization, scope reduction or explicit deferral advice; never silently
rebaseline. Run the hosted paired gate when a PR changes what its benchmark measures.

Report preparation, composition, GPU completion/presentation methodology, private bytes, working set, retained
resources and allocation peaks separately. WARP throughput with a blocking readback is not display cadence.
No source found in this research establishes an apples-to-apples GPUI Kit versus DxUi benchmark; published numbers
from different runtimes/hardware/scenes must not become a performance target or a superiority claim.

## 3. Remaining platform and physical qualification

- [ ] Complete native ARM64 foreground/resource acceptance in Debug, Release and ASan Debug. PR #77's six-profile
  candidate CI passed the foreground suites with zero native skips, but the post-merge `main` run at `c5485ab1`
  failed Menu in ARM64 Debug and Release after it launched. Retain both exact source/build/environment/scope
  identities; investigate these popup-opening failures and obtain stable native foreground/resource evidence.
- [ ] Establish physical hardware/device-removal acceptance separately from synthetic WARP/device-generation tests.
- [ ] Qualify real IME through form reflow, assistive-technology navigation/announcements and physical touch on relevant
  hosts/scenarios. Synthetic assertions and x64 focus suites do not pass those physical/product gates.

Local x64 functional/resource/focus work is completed at its recorded identities, including the later approved
30-second automatic-Start mode. Earlier failures/timeouts/refusals remain retained separately. Do not rerun or reuse
that evidence for changed executable inputs without the validation contract's exact-identity requirements.

PR #73's run at `1cdbfde4` completes all three x64 Full jobs, but all three ARM64 Full steps time out at the prior
25-minute cap before ordinary suite completion. The [cold-build timing packet](../../../Measurements/CiBudget/2026-10-11-cold-build/README.md)
retains those incomplete failures separately from successful clean-pin consumers. The finite ARM64 Full window
is now 45 minutes; every enclosing job fits its selected Full and consumer windows with setup/artifact allowance.
At that stage fresh complete source-bound CI was still required. Neither that timeout nor a raw, unstamped native
benchmark qualifies ARM64 runtime, a foreground-refusal exception, paired performance or a successful `ci-gate`.

Final run399 completes all six native build/provenance stages and the 19 ordinary ARM64 suites per profile within
the 45-minute Full limit, plus all eight consumers and two expected ASan probes. Root reviews exact warning ownership
guards and raw restoration records: foreign WWAHost occlusion refuses before the warning click and all child/suite
input. Saved/exit pointer and foreground/focus/pointer restoration are unchanged. The shared diagnostic cannot
identify which ownership guard refused first or establish continuous pointer history. Those six unlaunched results
remain failed foreground qualification. They are the only native failures admitted by the dated implementation-only
decision; no timeout/cancellation or correctness/resource/sanitizer failure is accepted. Hosted runs do not replace
the separately agreed manual focus/resource, physical-input or product gates. Main/release acceptance stays separate.

The later [PR #77](https://github.com/RedSalamanders/DxUi/pull/77) candidate run
[38285724272](https://github.com/RedSalamanders/DxUi/actions/runs/38285724272) passes all six native profiles,
including Menu and NativeTextInput in ARM64 Debug/Release/ASan Debug with zero native skips, and the aggregate gate.
Its merge to `main` is `c5485ab1`. The subsequent `main` run
[38291837960](https://github.com/RedSalamanders/DxUi/actions/runs/38291837960) instead fails the Menu popup-open
assertion in ARM64 Debug's root-switch fixture and ARM64 Release's explicit-mnemonic fixture; NativeTextInput
passes in both. These are launched fixture failures, not capability skips. The hosted lease reports foreground
and pointer restoration but cannot fully verify pointer history. The successful PR run cannot be substituted
for the failing post-merge run; ARM64 foreground acceptance remains open.

The first [draft PR #78](https://github.com/RedSalamanders/DxUi/pull/78) fixture follow-up still failed Menu in
ARM64 Debug and ASan Debug at [run 38298780317](https://github.com/RedSalamanders/DxUi/actions/runs/38298780317).
Its revised test-only head `500f93d4` activates modal fixture owners before popup launch. The exact
[run 38305728123](https://github.com/RedSalamanders/DxUi/actions/runs/38305728123) passes all six native jobs,
the relocated/STL-annotation consumer checks that apply to each profile, and `ci-gate`. The uploaded ARM64
Debug, Release and ASan Debug logs each contain 108/108 completed Menu cases and 165/165 completed NativeTextInput
cases, with no skip, failure or timeout lines. This is passing candidate evidence; repeat and review the native
foreground result on `main` after merge. Resource, physical-input, device-removal, paired-performance-policy and
consumer-product acceptance remain separate gates.

## 4. Consumer adoption gates

- [ ] A5: adopt one RedSalamander General section and one RedPrism size card in separate consumer changes.
- [ ] A6: consider the RedSalamander speed-limit prompt and RedXe profile editor after those pilots pass.
- [ ] Establish consumer pilot value criteria and leave optional product additions deferred if no need is shown.
- [ ] Record explicit consumer pins/adapters and independent adoption outcomes; retain deferred product gates.

Acceptance: long labels and error messages at 96/144/192 DPI, narrow/wide widths, standard/compact density and both
flow directions remain readable; tab order and IME survive reflow; fixed actions remain available; unchanged forms
perform no new measurement. Compare old/manual and proposed layout on the same synthetic form and consumer scenario.
Record reduced duplicated measurement/bounds logic alongside latency, allocations and memory; a shorter API is not
evidence of a faster UI.

| Consumer | Separate acceptance before its pin is moved |
| --- | --- |
| RedSalamander | Explicit current-pin/API compatibility review (the 4 October inventory had revision 2; the 10 October recheck had revision 3); Preferences draft/Apply/Cancel and page-focus/scroll parity; exact path inspection and background non-activation; conflict action wrapping; native alert lifetime; theme-expression/preview parity |
| RedXe | Profile draft/device revision preservation; at most four profiles; density tiers and 48 DIP targets; confirmed backend state; immediate live AV controls; camera guide commands/content; hidden views keep no services/timers/providers |
| RedPrism | Size/adjustment preview/commit/cancel; focus/card policy including canvas; child dropdown/IME priority; capture and document teardown; live theme settings; app-owned canvas and history/job limits |

Generate before/after application captures only through each consumer's deterministic test harness, retaining
scenario, source, build, DPI and theme provenance. Do not take desktop/manual screenshots. Any proposal mockup is
labelled separately from an actual application capture. S0-S2's native library gallery was generated through
`gallery.ps1 -PublishDocs` and reviewed in all five themes and the supplied-device example. Consumer application
captures and before/after adoption evidence remain outstanding.

Preserve existing RedSalamander ARM64/ASan handoff and RedXe hardware/real-IME/assistive-technology HOLD gates.
Library validation cannot complete those product gates. Keep adoption measurements in the application repository.

The original consumer inspection identities and subsequent 10 October recheck are retained in the S0-S2 Done record.
Recheck current HEAD/pin/API revision and the active product plan before adoption; historical inventories are not
current qualification. C++ object-layout changes require matching rebuilt headers/library/toolchain/CRT even when
an additive change does not increment the API revision.

## 5. Qualification closeout

- [ ] Complete the performance/policy, native-runtime and physical-input gates above at the exact accepted candidate.
- [ ] Qualify chosen consumer pilots/pins in their own repositories, retaining independent deferred/HOLD gates.
- [ ] Reconcile enduring conclusions in the owning domain contracts, retain reviewed receipts and update capability
  limits truthfully; close only completed acceptance scope and update its index.

This record does not close existing menu, Grid, Tree, slider, production-review, consumer or platform plans. The
S0-S2 Done record closes library implementation/local validation only; S3-S7 stay deferred in their own WIP file.
