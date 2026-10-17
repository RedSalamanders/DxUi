# Library qualification and consumer adoption

- **Status**: ACTIVE; unfinished acceptance split from the original 4 October plan on 11 October 2026.
- **Scope**: shared library performance/policy, native-platform and physical-input qualification execution, including
  ProductionReview and accepted S0-S2 candidates, plus separately owned
  consumer pilots and pins. S0-S2 library implementation and recorded local x64 validation are complete in
  [Done](../Done/ConsumerUiImprovements_S0_S2_2026-10-04.md).
- **Deferred implementation**: [S3-S7 proposals](ConsumerUiImprovements_S3_S7_2026-10-11.md) are unimplemented and
  require separate scope acceptance. This qualification plan does not authorize those subsystems.
- **Ownership**: DxUi retains library measurements/platform qualification; consumer product behavior and adoption
  measurements remain in each application repository. No consumer is changed implicitly.

## Shared qualification ownership (16 October 2026)

This is the single execution ledger for cross-plan qualification. Feature/remediation plans own implementation,
defect attribution and their source-specific functional evidence, and link here for shared acceptance rather than
copying its procedures or independently closing the same gate. Domain contracts remain normative.

| Obligation | Execution owner and remaining evidence |
| --- | --- |
| Paired policy and non-regression | This plan: stable matched controls, independent seed-policy review, a trusted measured base and candidate-specific paired/hosted results under the owning performance contract. Policy remains unqualified. |
| Native ARM64 and opt-in resources | This plan: exact-source native profiles and separate MenuResources/MenuResourceScaling evidence; cross-builds or ordinary hosted foreground passes do not substitute. |
| Physical input, real IME and assistive technology | This plan tracks unresolved library scenarios and environment admission; each application owns its product/device/service execution and acceptance. Synthetic UIA/input is a separate fact. |
| Consumer publication, pins and rollback | The [API-4 handoff register](ProductionReview_2026-10-05/api4-consumer-handoffs.md) owns consumer actions. This plan references their evidence without changing pins or declaring product acceptance. |

Keep receipt identities separate for each candidate. Existing S0-S2 and ProductionReview receipts cannot qualify
the later [host/control fixes](../Done/HostControlReviewFixes_2026-10-16.md) merely because a shared gate has one owner.
Historical failures and approved tradeoffs retain their original scope; no threshold or baseline is changed here.

### Host/control candidate acceptance

- [x] Obtain current-candidate x64 Menu, NativeTextInput, MenuResources and MenuResourceScaling receipts in
  Debug, Release and ASan Debug under the automatic Start policy approved on 17 October. This ledger owns this foreground
  acceptance; the [host/control plan](../Done/HostControlReviewFixes_2026-10-16.md) closes its local dependency.
  Its [local packet](../../../Measurements/HostControlReviewFixes/2026-10-17/README.md) identifies the frozen inputs,
  57 noninteractive suite passes, six builds and four relocated synthetic consumers. Older foreground receipts
  cannot qualify this candidate. Native ARM64 and physical-input/IME/AT acceptance remain separate.
  All twelve current-candidate foreground suite executions pass with zero skips and verified restoration across
  four successful leases: one each for Debug and Release, and two for ASan. All 831 named cases complete, and each
  receipt matches its executable hash and the requested/native 30-second automatic Start policy.
  The first ASan attempt records a native-focused-host skip, fails to grant the next child foreground and fails
  foreground/pointer restoration. Its next lease is refused before any child starts because the warning cannot
  take foreground, with all desktop states verified unchanged. A third attempt fails the hidden-host initial-focus
  assertion and cannot launch the resource child; its desktop states are restored or unchanged. All three private
  failures/refusals remain retained. The later ASan passes at identical compiled inputs do not establish their causes.

- [ ] Resolve the host/control candidate's diagnostic memory uncertainty through stable controls and exact-source
  paired qualification. Its first MultilineGrid comparison flags clean private/peak and dirty peak by about
  1.28/1.31/1.54 MiB; the unchanged candidate's repeat is within budget. Keep the first flag and both receipts:
  a passing repeat is not attribution, and none of these ordinary comparisons qualify the trusted policy.

### Remaining legacy clock audit

- [ ] Independently reproduce and reconcile remaining system-uptime/frame-timestamp interactions in Button
  interaction starts, TextField/ComboBox caret resets, Tree expansion, Grid sort glyphs and first-tooltip deadlines.
  Exercise zero and large application epochs, intermediate pixels, completion and idle cleanliness before claiming
  a library-wide epoch-independent clock contract. The reviewed host/control fix explicitly qualifies only the
  requested PageHost/Slider transition starts. These additional source leads have not been runtime-falsified or
  silently recorded as corrected.

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

### PR #80 paired-control investigation (17 October 2026)

The [reviewed evidence](../../../Measurements/Performance/2026-10-17-pr80-controls/README.md) binds base `99e8b313`,
tested merge candidate `652c1ecb`, actual merge `02b59a61`, seed `425707354` and all 144 hash-verified receipts.
Both frozen judges and all 72 controls replay. The paired measurement completes, but its publisher and the PR
aggregate fail on the unqualified measured-base policy. The separate post-merge functional workflow passes all
six native profiles, 128 suite receipts with zero skips and eight consumers; push skips paired measurement.

The 71 unstable controls comprise 70 with timing/FPS drift and 19 with memory drift, overlapping in 18 controls.
There is no exact-budget drift. Fifty of the 54 timing/memory scenario-phase slots drift at least once; these
are investigation-band crossings, not 50 established regressions. All three migrated sets have zero regression
flags. Clean preparation is quantized at 100 ns, but 67 controls also drift in non-preparation timing/FPS metrics.
Retain Default B07's 584.4771 ms dirty frame-p95 round and 2,619.8089 ms individual frame; its cause is unproved.
MultilineGrid A07/A08 has a separate memory-only control flag, already showing a 620 KiB private-byte difference
after warm-up, before capture. Neither observation attributes the earlier local 1.28/1.31/1.54 MiB memory flags.

The one judge disagreement is Distinct dirty frame-p95: legacy pooled-run improvement -6.07955%, p=0.0319363,
versus paired-block effect -3.91524%, raw p=264/4096=0.064453125, Holm-adjusted p=1, within-noise. The different
estimators and independent units explain the reproduced labels; Holm is not the only difference. Preserve the
original `verdictAgreement=false`. Matching overall set labels cannot waive the full-metric migration check.

This plan owns the following execution sequence; the benchmark/tooling owner supplies the diagnostics, and the
developer/independent reviewer owns policy approval. No other feature plan co-owns or closes these gates.

- [ ] Keep the current raw reports/seals privately and arrange independently rehashable retention before the next
  fresh calibration/paired run: exact executables/library/symbols, build attestations and compiler/SDK/dependency
  closure. Publish only the sanitized summary. Missing original binaries cannot be recovered from this upload;
  a rebuild does not repair that original-evidence gap and must be recorded as a new build/run.
- [ ] Diagnose timing on one frozen executable with bounded, separately labeled probes: empty clock-pair/loop
  overhead and 1/64/256-call clean-Prepare batches, plus a trace covering model update, Prepare, Composite,
  target binding and blocking completion. Correlate long frames with CPU scheduling, WARP/completion and
  process-memory observations. Retain trace overhead and environment admission; elapsed remainder is not GPU time.
  Preserve canonical 20-frame warm-up, capture, 40-frame rounds and every original outlier. A changed diagnostic
  harness needs its own identity and cannot replace or pass the canonical acceptance workload.
- [ ] Localize the memory-only witness with matched stage/heap/native-resource snapshots around scene creation,
  warm-up, capture, dirty rounds, hidden state and teardown. Any capture/warm-up/sampling isolation is a separate
  diagnostic fixture with original receipts retained. Establish allocator/component or lifetime attribution
  before claiming a leak, dismissing variability or resolving the original local memory gate.
- [ ] Run a fresh canonical same-binary A/A calibration after the bounded attribution pilot, with predeclared
  independent orders, at least twelve blocks, all three scenarios, complete provenance and continuous host-load
  evidence. Retain failures and all per-metric controls; do not select a passing seed/repeat or invent an A/A
  numeric pass threshold. Independent review must assess the resulting controls under the owning contract.
- [ ] Review the migration disagreement using the retained 24-run pooled calculation and twelve-block/4,096-order
  calculation, all 26 Holm slots and the full-label equivalence refusal. Record the independent decision before
  qualifying a versioned policy in the measured base. Keep original decisions unchanged; any approved future
  judge/fixture change requires its own frozen identities, calibration and candidate-specific paired study.

All remaining gates stay open: trusted policy/paired acceptance, the earlier memory uncertainty, ARM64 opt-in
resources, physical input/device removal, real IME/AT/touch and consumer adoption. Investigation bands stay
5% timing/FPS and 2% process memory; exact budgets and absolute allocation ceilings are unchanged. This
documentation-only follow-up changes no runtime, tooling, public usage or visual behavior; gallery pixels need no update.

### Bounded timing and memory attribution pilot (17 October 2026)

The [pilot packet](../../../Measurements/Performance/2026-10-17-attribution-pilot/README.md) completes three
unchanged canonical reference runs and a separately identified x64 Release diagnostic: 24/24 valid fresh processes,
10,080 frames, all three scenarios and two fixed-order observations per timing/heap/capture mode, in 808.049 seconds
of the 1,800-second budget. Every process passes before/after v2 identity and raw/diagnostic/controller hash checks.
Fresh outputs, symbols, attestations, compiler/SDK/dependency bytes and source inputs remain privately retained.
The original PR #80 binary gap remains; the new prototype is not a supported shipped workflow or old-binary replay.

The pilot supplies partial attribution, not performance qualification:

- Empty clock-pair and single-call batch p95 are both 100 ns in the six detailed/no-heap capture processes.
  Repeated hot-call batches differ substantially from the 300-800 ns frame-Prepare p95 medians; disassembly and
  linked bytes confirm the batch call loop remains. No batch mean replaces a canonical metric or changes a band.
- Stage sampling itself costs 2.337-3.523 seconds/process, versus 9.35-13.98 ms for the separately timed optional
  heap block. Detailed dirty-window overhead outside frame spans is 50.05-72.97 microseconds/frame, versus
  1.15-2.46 in plain mode. Sampling outside timing can change later readiness; mode differences are not isolated
  causal overhead estimates. Plain-mode dirty FPS still differs by +12.15%, +24.27% and +18.19% across repeats.
- Complete stage telemetry and 3,530 host samples do not supply a kernel scheduling trace. WPR refuses system
  performance policy with `0xc5585011`; its exact cause is unproved. Sampled CPU consumption beyond the owned child
  is appreciable, without a competing-process or per-frame causal attribution. No quiet/dedicated-host pass is claimed.
- Capture-on private deltas are 3,816-4,188 KiB; capture-off deltas are zero. A separate MultilineGrid pre-capture
  pair differs by 1,256 KiB with different sampled heap usage/thread population, while view surface/Grid counters
  hold. Released snapshots show substantial free/committed heap capacity alongside smaller busy totals. These
  observations do not identify allocation owners or resolve either historical memory flag.
- Detailed frames distinguish Prepare-heavy and blocking-Map-heavy cases. The longest measured frame is 12.9317 ms;
  the historical 584.4771/2,619.8089 ms witnesses are not reproduced or explained. Map is not isolated GPU time.

The existing unchecked timing and memory actions above remain open. Their next bounded work is to time/isolate
the expensive non-heap stage observers with matched observer-off/on diagnostics; obtain a profiling-capable
environment or diagnose the WPR refusal before attributing CPU-ready/worker/completion delays; and collect matched
native allocation/worker ownership evidence at warm20 and teardown without purging caches. Keep capture in the
canonical workload. Only after observer/environment admission and independent review should this owner schedule
fresh canonical A/A with at least twelve independent blocks and all three scenarios. The two observations per
pilot mode are not that calibration. Policy/migration review, every remaining platform/physical/product gate,
original disagreement flags, thresholds and budgets stay unchanged. This repository update adds documentation
only; public runtime/test behavior and gallery pixels are unchanged.

### Earlier candidate evidence

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

That S0-S2 final PR study has unstable controls in 69/72 comparisons. Retain Distinct-Grid B06's four elevated composition rounds:
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

- [x] Complete hosted native ARM64 Menu and NativeTextInput foreground functional execution in Debug, Release and
  ASan Debug at the PR #78 candidate and merged `main` identities, with zero skips and a successful aggregate gate.
- [x] Complete the separately approved local x64 Debug, Release and ASan Debug MenuResources and
  MenuResourceScaling suites at clean merged-main `e0260dde`, with zero skips and verified desktop restoration.
- [ ] Complete native ARM64 MenuResources and MenuResourceScaling fixture execution. The hosted Full runs
  cover Menu and NativeTextInput, not these opt-in resource fixtures.
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
for the failing post-merge run; ARM64 foreground acceptance was still open at that source identity.

The first [PR #78](https://github.com/RedSalamanders/DxUi/pull/78) fixture follow-up still failed Menu in
ARM64 Debug and ASan Debug at [run 38298780317](https://github.com/RedSalamanders/DxUi/actions/runs/38298780317).
Its revised test-only head `500f93d4` activates modal fixture owners before popup launch. The exact
[run 38305728123](https://github.com/RedSalamanders/DxUi/actions/runs/38305728123) passes all six native jobs,
the relocated/STL-annotation consumer checks that apply to each profile, and `ci-gate`. The uploaded ARM64
Debug, Release and ASan Debug logs each contain 108/108 completed Menu cases and 165/165 completed NativeTextInput
cases, with no skip, failure or timeout lines. PR #78 merged to `main` as `fa4da2cb`. Its fresh
[main run 38351048921](https://github.com/RedSalamanders/DxUi/actions/runs/38351048921) succeeds in all six native
profiles and `ci-gate`. Downloaded logs for each profile again show 108/108 completed Menu cases and 165/165
completed NativeTextInput cases, with no skip, failure or timeout lines. ARM64 Menu receipts report native Arm64,
successful hosted lease completion and foreground/cursor restoration; the saved focus was unavailable, and a
saved/exit cursor check does not establish continuous pointer history. This closes the hosted ARM64 core foreground
functional gate at that exact merged source and CI identity. Opt-in resource fixtures, local/manual focus,
physical-input, device-removal, paired-performance-policy and consumer-product acceptance remain separate gates.

The merged [PR #76](https://github.com/RedSalamanders/DxUi/pull/76) source `e0260dde` has a separately
[13 October main qualification summary](../../../Measurements/Performance/2026-10-13-main-qualification/README.md).
[Main run 38428878942](https://github.com/RedSalamanders/DxUi/actions/runs/38428878942) passes all six
native profiles and `ci-gate`, with 128 suite receipts, zero skips, 108/108 Menu and 165/165
NativeTextInput cases in every profile, and three native Arm64 identities. After the user approved
automatic Start, all six opt-in resource suite executions passed locally across the three clean-source
x64 profiles, with recorded foreground/focus/pointer restoration. The prior ordinary Release Start
timeout is retained separately and did not launch a child. This closes the local x64 resource execution
slice only; ARM64 opt-in resource execution and physical input remain open.

The locally retained, audited fixed twelve-report profiling pilot is exact-source and internally replayable, but
its Default heap-on sixth report has a 699.49 ms clean probe against 19.66 and 18.78 ms in the other
two such reports. It does not establish the cause. The PR #76 paired gate failed trusted-base policy
with 69/72 unstable same-side controls; the successful main-push workflow skipped that paired job.
The preceding 72/72 unstable A/A control study remains unresolved. No accepted paired non-regression
or trusted policy is claimed. Documentation links to the new packet; this evidence-only change alters
no visuals or controls, so gallery pixels need no regeneration.

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
