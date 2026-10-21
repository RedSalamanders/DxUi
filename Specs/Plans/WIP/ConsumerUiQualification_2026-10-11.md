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

### PR #89 calibration contribution and policy-review proposal (20 October 2026)

The owner requested fresh A/A evidence and a review proposal while preparing
[PR #89](https://github.com/RedSalamanders/DxUi/pull/89). This contribution does
not qualify the policy or close another candidate's acceptance. The retained
[RichText summary](../../../Measurements/RichText/README.md) owns its functional
results and clear/readback investigation.

- [x] Repair reproducible paired-build provenance mismatches before measurement:
  common benchmark project/options, recorded/restored timestamp staging and full
  v2 preflight. Keep library sources/projects independent; 31 tooling cases pass.
- [x] Add continuous Windows host observations and hosted retention of the
  original native outputs/attestations; retain all failures and sampling costs.
- [x] Complete the canonical hosted same-binary A/A run with twelve independent
  blocks and Default, MultilineGrid and MultilineGridDistinct. Seed 1341918500
  was generated once; all orders/controls are retained, with no favorable retry.
  The retained 17 October attribution pilot precedes this calibration. Hosted
  collection used a separate machine and does not qualify this candidate.
- [x] Complete the fixed eight-process main/candidate boundary pilot in a
  coordinated build-free window. The first attempt lacked a private dispatcher
  branch and produced no report; the second ran out of disk and retained an
  incomplete observer. A third completed/replayed all eight processes, but its
  continuous observations establish compiler/linker activity. Keep it as
  under-load diagnostics; repeat the complete protocol without reusing slots.
  Frozen library sources are main `f65e4a4b` and retained candidate `e325158a`,
  with a shared diagnostic harness. This attribution pair predates the menu
  repair and does not qualify runtime/test commit `61efca4c`.
  A fourth fresh attempt follows the sorting handoff and confirmed RedPrism
  pause. Four of eight processes replay independently, including 1,280 frames
  and matching captures; the pre-slot guard refuses process five on compiler
  presence. Its complete 180-sample observer records advancing compiler CPU
  before refusal. The owner remains unproven; later live compilers start after
  the refusal and must not be substituted as its cause. Collection ends and the
  pause is released. That protocol remains incomplete; none of its slots is reused.
  A fifth attempt follows actual worker-stop confirmations from both active
  build chats and a fresh machine-wide preflight. All eight new processes
  complete in 327.67 seconds; original reports, both retained executables,
  source/build identities, 2,560 frames and matching captures replay. Its
  314-sample observer records zero compiler/linker rows and zero other DxUi
  native-test rows; property-evaluation observations remain separate. Both
  pauses are released before offline replay. Full-frame same-ranked Map shares
  are 96.69%/94.80%. Full-frame ordering reverses between timers-off/on; two
  fresh processes per role/mode do not establish isolated observer overhead or
  candidate speed. Observer collection median/p95 is 38.98/46.80 ms, maximum
  sample-start gap 1.081 s and sampled-span CPU 4.41% of one core. Sampling does
  not exclude unknown work or activity between samples, and supplies no kernel
  scheduling/GPU attribution. This closes the declared coordinated diagnostic
  collection, while all canonical acceptance and independent-policy gates stay open.
- [x] Download/seal the original A/A artifact, replay both frozen judges and all
  controls from identical bytes, inspect continuous load and every metric, and
  record the exact source, run and closure limitations here.
- [ ] Obtain independent review of the following proposal before any separate
  trusted-policy change in main. PR #89 remains draft until its required gates
  are satisfied; an A/A diagnostic green check cannot qualify a regression gate.

**Proposed decision: retain the existing policy and HOLD pending the review.**
The review target remains `dxui-complex-ui-paired-v2`, judge
`dxui-paired-block-sign-flip-holm-v2`, the existing judge/assignment SHA-256 values
in `Tools/PerformanceAcceptancePolicy.v1.json`, twelve independent ABBA/BAAB
blocks, all 26 Holm slots per scenario, alpha 0.05, timing/FPS 5%, memory 2% and
unchanged exact budgets. No new numeric A/A pass threshold is proposed.

The independent reviewer must decide whether the independent-order design,
same-byte legacy/migrated differences, all controls/outliers and observed host
load support this frozen policy. Unstable A/A controls require an explicit
decision or a separately frozen fixture/judge investigation, not automatic
dismissal because the executable is identical. Hosted artifacts now retain the
actual application binaries/library/symbols and attestations; compiler/SDK bytes
are hashed by build identity but are not redistributed in public artifacts.
Independently rehashable access to that toolchain/dependency closure remains an
admission question. A rebuild cannot replace missing original outputs.

Only an independently approved policy seed merged into the measured base can
provide policy trust. After that seed, rerun the candidate-specific required
paired gate against that exact main revision. Preserve the unqualified-base
refusals and earlier failures. This proposal grants no policy approval, baseline
replacement, performance tradeoff or consumer acceptance.

Fresh collection completed in
[workflow 39468472800](https://github.com/RedSalamanders/DxUi/actions/runs/39468472800),
with both explicit revisions pinned to
`e0fa1d7a2efd81b80bc575978bc4e029a083b0af`. The paired job succeeded
diagnostically; the workflow aggregate failed its separate older-budget native
jobs. The A/A roles shared one physical tree/build. Both frozen judges and all
144 receipts, 72 controls and 26 metrics per scenario replay exactly. Both judges
label all three sets within budget, with no metric-label disagreement. Nevertheless
controls drift in 22/24 Default, 24/24 MultilineGrid and 23/24 MultilineGridDistinct
comparisons: **69/72 unstable controls remain unresolved**. This is not approved
calibration or accepted candidate non-regression. A single study cannot establish
family-wise false-positive coverage or qualification across machines.

The original artifact ZIP is 81,369,231 bytes; its downloaded SHA-256 matches
GitHub's `f4fd1a8864e6e759f5ec7aeb40cedcd8fffff6b1e009db3b1605b54f32fba5d1`.
The original measured executable rehashes to
`171FDE902B8125E418927B2B0CB051484E4D2A5B73A257D98F1324D4D6BECC3C`.
Original outputs/symbols, attestations, reports, observer stream, logs and separate
replays remain privately retained. The observer completion seal verifies 6,344
consecutive samples over 6,382.28 s. Median/p95 collection is 2.26/3.50 ms,
maximum 205.49 ms; maximum start interval is 1,246.76 ms. Sampled-span observer
CPU is 0.333% of one logical core. Guest OS CPU median/p95 is 26.95/62.69%; coarse
intervals with a benchmark process present are 52.34/82.42%. These include measured
work and do not identify external load, exact-frame preemption or physical-host
activity. The 6,351 inaccessible/exited process observations remain explicit.
Compiler/SDK/dependency-closure access remains an independent admission question.

The later PR #89 main/candidate study in
[workflow 39480512299](https://github.com/RedSalamanders/DxUi/actions/runs/39480512299)
compares main `f65e4a4b` with tested merge `6e2f455b` (head `61efca4c`), seed
`1202880588`. Its original 138,507,992-byte paired ZIP matches GitHub SHA-256
`6ecaf47e3ad0bf49df69944923ef6103bd60e434ae5b83f9c25366778b0581fb`.
Separate replay rehashes both original role executables and verifies all 144
receipts, both judges and 72 controls. Metric labels agree in this study, while
controls drift in 21/24 Default, 24/24 MultilineGrid and 20/24 Distinct comparisons
(65/72 total). Keep the preceding study's disagreements and A/A instability.
The measurement step succeeded; its required publisher and aggregate gate failed
because the measured-base policy is not independently qualified. Six sealed
native artifacts additionally verify 126 native scope passes, zero capability
skips, two resource fixtures and all eight relocated-consumer variants; those
functional results do not grant performance acceptance.

After feature-history consolidation, the current head `335c5157` is tested at
tree-identical merge `17969dc3` in
[workflow 39500910925](https://github.com/RedSalamanders/DxUi/actions/runs/39500910925).
Six downloaded original artifacts verify 126 native scope passes, zero skips,
two resource fixtures and eight consumer variants. The 138,976,283-byte paired
ZIP matches GitHub SHA-256
`9aa72831e97715150236e9f2845cadbff87814ea5830571e6395640d66cf4fa5`.
All 144 receipts, both original role executables, both judges and 72 controls
replay under seed `1568326520`. All 72 controls drift. MultilineGrid clean
composition p95 is flagged by the legacy judge and within-noise under the
migrated judge; retain both results. The unqualified measured-base policy still
fails the required publisher and aggregate. This run supplies no policy approval.

The current observer stream verifies 5,753 samples over 5,928.18 s. Collection
median/p95 is 2.40/5.76 ms, with maximum 5,318.65 ms; maximum sample-start
interval is 7,772.71 ms. Sampled-span observer CPU is 0.368% of one logical core.
These bounds do not provide frame-aligned scheduling or physical-host visibility.
The original packets and previous studies remain retained. A private local-audit
repair preserves ISO UTC precision and passes French/US positive replays plus
malformed child/observer timestamp rejection, four checks in total. The broader
build-process inventory includes CMake/Ninja; its original cl/link subset still
reproduces 1,283 rows, 1,064 CPU advances and 24 coarse intervals containing timed
windows. This is checker validation on prior under-load data, not a fresh pilot.

The instability is not limited to sub-microsecond preparation quantization.
Maximum absolute same-binary Default clean-frame p95 change is 25.48% in A/A
and 44.59% in study `39480512299`. These are maxima over retained controls,
not candidate effects or attributed external load. Only 10/26, 10/26 and 14/26
metric slots in that study hold every control within its declared band
(A/A: 11/26, 10/26, 16/26). Keep frame/composition and memory stability in the
review alongside timing resolution; do not infer that one explanation closes all
observed drift.

The independent review must resolve these concrete questions before a seed is
proposed for approval; collecting this evidence does not answer them by authority:

| Review question | Current evidence and unresolved decision |
| --- | --- |
| Can the fixture hold the declared bands? | Same-executable A/A still has 69/72 unstable controls. Review per-metric drift and the needed frozen stability investigation; do not treat identical binaries as an exemption. |
| Does the migrated method reproduce every result? | Each retained study replays all 144 receipts, full 26-slot families, assignments and both judges. A/A and the latest candidate labels agree, while the preceding main/candidate study disagrees on clean preparation in all three scenarios. Preserve every result. |
| Can clean preparation resolve its timing band? | The preceding legacy flags use 0.0005 versus 0.0006 ms. Review raw clocks and resolution against the unchanged 5% band; neither small absolute cost nor a passing migrated label dismisses the flag. |
| Is this enough calibration? | One predeclared three-scenario A/A study does not estimate false-positive coverage across repeated studies or machines. Freeze any further calibration design before collection. |
| What does the observer establish? | Sequence/hash/cost/gap replay succeeds; inaccessible/exited observations remain explicit. One-second guest observations cannot establish frame-level scheduling, physical-host quietness or GPU duration. |
| Can another reviewer close actual-byte identity? | New original executable/library/symbol bytes are retained; older missing outputs cannot be rebuilt into evidence. Compiler/SDK/dependency access still needs independent admission. |
| What authorizes eventual PR acceptance? | A separately approved trusted policy in main and fresh exact candidate-specific required qualification, with stable controls and unchanged exact budgets. Neither this proposal nor candidate code grants that approval. |

PR #89's local ASan Menu failure is separately under repair. A controlled idle-input
run retained a stationary captured move changing keyboard index 0 to hover index 1
with unchanged production Menu source. Four root/submenu sent/posted regression
cases now accompany a missing-history initialization fix. An earlier attempt had
actual changing pointer coordinates and is retained as unsuitable stationary-input
evidence. Original binaries/symbols, failures and restoration receipts are retained
privately. The repaired source passed the four new cases and the original Right-arrow
case in focused ASan execution, with zero skips and verified desktop restoration.
Full current-source hosted qualification is verified above. Latest local Release
and Debug each pass all four foreground scopes with zero skips; Debug verifies
complete restoration, while Release fails keyboard-focus restoration. Both saved
Task Manager, so the passing repeat does not attribute that failure. The earlier
ASan saved-window disappearance and Explorer focus failure remain explicit local
restoration limitations. Three earlier passing isolated repeats did not attribute
or erase the complete-suite failures. Keep further local restoration diagnosis
separate from the policy review and preserve its strict assertion.

- [ ] Establish accepted paired non-regression and a qualified trusted policy for the S0-S2 candidate.
- [ ] Attribute or resolve the retained timing/memory excursions and timing/FPS flags using matched fixtures and
  stable controls; retain measured developer advice where degradation is confirmed.
- [ ] Complete the required hosted paired gate when the PR changes what its benchmark measures. An enabled pending
  gate remains pending until its actual result is successful; a candidate-only diagnostic cannot qualify it.
- [x] Harden the production paired gate's role-specific summary/raw provenance binding and absolute whole-frame
  allocation checks before policy qualification; independent integration review checks both on the retained raw reports.

### PR #89 current-source follow-through (21 October 2026)

The owner explicitly chose to keep PR #89 draft for independent performance-policy
review. The retained three-scenario A/A contribution still has 69/72 unstable
controls; no policy approval or implementation-only readiness exception is supplied.

At head `c1a77e42`, tested merge `6110c7e0`, original workflow `39555699478`
passes four native profiles. x64 Release fails child foreground admission and
restoration; ARM64 ASan fails the visible-initialization healthy Menu probe with
the popup alive but hidden. All six original native ZIPs are retained and verified
against GitHub sizes/digests. Eight consumer variants and both Release resource
fixtures pass separately. Docs-gallery generation/upload is skipped after the
Release failure. This source has no full functional acceptance claim. Preserve
both failures separately from earlier complete-source passes and any future repeat.

Paired collection completes and its 139,034,543-byte original ZIP matches GitHub
SHA-256 `fba9638f339acd220b520520262110e025de31e3d50f05edeac75cb5f0b566fd`.
Under seed `981054997`, both original role executables, all 144 receipts, both
frozen judges and 72 controls replay. Judges agree on metric labels, but 70/72
controls drift (22/24 Default, 24/24 MultilineGrid, 24/24 MultilineGridDistinct).
The publisher rejects acceptance because the measured-base policy is unqualified;
the aggregate also fails. All original workflow logs are retained. The observer
replays 5,903 samples over 6,081.45 s, median/p95 collection 2.46/5.18 ms,
maximum sample-start interval 9.24 s and sampled-span observer CPU 0.363% of one
logical core. These observations do not establish host quietness, attribution,
scheduler/GPU duration or accepted non-regression.

A new approved local Release rebuild and one bounded four-scope run at that head
pass with zero skips and verified foreground/focus/pointer outcomes. Pre-confirmation
focus is live/recorded; foreground and focus remain unchanged and pointer is restored.
This qualifies the local run without attributing prior failures or replacing
hosted acceptance. Original native outputs are retained and executable/lease hashes
match the receipts. There is no remaining authorization for another local focus run.

The PowerShell launch-failure handler also had a reproduced evidence-retention
defect: canonical failure replacement destroyed any existing child output. It now
first copies exact raw bytes to unique unqualified `.log` siblings, while current
receipts remain failed. Two new regressions fail before repair and all 40 wrapper
cases pass afterwards. Missing output, repeated refusals, literal Unicode paths and
failed retention are covered. This fixes future evidence preservation only;
foreground behavior and existing probe deadlines are unchanged.

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

PR #82's post-merge native run fails x64 Debug's first Menu case. The source-specific
[popup-readiness repair](PopupReadiness_2026-10-17.md) owns its diagnosis, implementation and functional evidence;
this ledger retains all existing shared qualification obligations and thresholds unchanged.
Its final local repair passes 57 fresh noninteractive x64 suite executions and all three ARM64 cross-builds.
Full Menu passes 109/109 cases in each of x64 Debug, Release and ASan Debug, with zero skips and verified
desktop restoration. A controlled visible-initialization regression fails the healthy probe after 1,009 ms
when only the modal-readiness wait is removed; all original probe deadlines and assertion bands remain unchanged.
The original untraced full Debug failure stays retained and unattributed; these passes do not identify its
precise stalling operation. Fresh native CI was pending at that local snapshot; the later PR #83 observation
and its focus-evidence limits are recorded below. Every shared qualification gate remains unchanged.

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

### ARM64 lease focus investigation (17 October 2026)

[PR #83](https://github.com/RedSalamanders/DxUi/pull/83) merged as `7f476777f0275dd778438e1b294dba14766cf3d7`.
Its first-attempt [post-merge run 39064416921](https://github.com/RedSalamanders/DxUi/actions/runs/39064416921)
passes all six native profiles, eight enabled public-consumer checks and `ci-gate`. The downloaded artifacts
contain 128 passing suite receipts with zero capability skips; each profile completes 109/109 Menu and
165/165 NativeTextInput cases. Push skips paired measurement, so this supplies no new paired-performance result.
The original untraced Debug healthy-probe failure remains unattributed.

**Accounting correction:** the six ARM64 Menu/NativeTextInput receipt entries represent **three shared leases**,
one per configuration, rather than six independent focus snapshots or restoration attempts.
[`test.ps1`](../../../test.ps1) constructs one foreground plan for both children and invokes one lease before
writing either receipt. All shared lease fields agree between each pair; only `lease.seconds` differs because
it records that child's duration. Each Full job log contains one native capture-description line and one native
restoration line. Repeating the state in two child receipts does not add an independent observation.

| Native ARM64 profile | Preparation and saved-foreground evidence | Shared restoration result | Child execution |
| --- | --- | --- | --- |
| [Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39064416921/job/117488981967) | CoreWindow and broker minimized; saved description matches the prepared class/process | foreground `restored`, focus `none`, cursor `restored` | Menu 109/109; NativeTextInput 165/165; zero skips |
| [Release](https://github.com/RedSalamanders/DxUi/actions/runs/39064416921/job/117488982080) | CoreWindow and broker minimized; saved description matches the prepared class/process | foreground `restored`, focus `none`, cursor `restored` | Menu 109/109; NativeTextInput 165/165; zero skips |
| [ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39064416921/job/117488981979) | CoreWindow and broker minimized; saved description matches the prepared class/process | foreground `restored`, focus `none`, cursor `restored` | Menu 109/109; NativeTextInput 165/165; zero skips |

The source establishes the meaning of `none`:

- [`DesktopLease::Capture/Read/RestoreFocus`](../../../Tests/Support/Support.Tests.DesktopLease.h) saves the
  pre-confirmation desktop. A missing foreground thread or a minimized foreground window yields a null saved
  focus; otherwise the then-current `Win32DesktopBackend::ThreadFocus` returns `GUITHREADINFO.hwndFocus` on query success and null on
  query failure. A successful query can itself return a null focus. `RestoreFocus` returns `NotRecorded`
  (`none`) immediately when the saved focus is null. It does not attempt `FocusWindow` in that branch.
- [`RunWithDesktop`](../../../Tests/Support/Support.Tests.InteractiveLease.h) captures before confirmation,
  warning activation or either child. This status describes the lease's original desktop snapshot, not a
  control's focus inside Menu or NativeTextInput and not a focus-restoration timeout after those children.
- [`Prepare-HostedArmDesktop.ps1`](../../../Tools/Prepare-HostedArmDesktop.ps1) runs immediately before the native
  lease, only under verified hosted/native-ARM64 conditions. It minimizes the selected first-sign-in WWAHost
  `Windows.UI.Core.CoreWindow` and the visible `UserOOBEWindowClass` broker, checks minimization, and leaves
  their processes alive. Each of the three job logs records both preparations, then a saved foreground description
  for the same CoreWindow class and WWAHost process. Matching process identity was checked privately; it does
  not establish that the HWND was identical or remained minimized at capture.
- [`RestoreReport::Succeeded`](../../../Tests/Support/Support.Tests.DesktopLease.h) and
  [`Get-DxUiLeaseProblems/Get-DxUiLeaseSummary`](../../../Tools/InteractiveRun.psm1) retain the existing distinction:
  `none` is unrecorded rather than `failed`, so a completed child run can pass while its summary explicitly says
  desktop restoration was not fully verified. A recorded focus that cannot be restored remains a failed lease.

**Supported explanation, with a remaining observation gap:** runner preparation followed by the intentional
minimized-foreground guard best explains these records. The capture and focus-restoration code is common to
x64 and ARM64; the special preparation/environment is ARM64-specific. The three x64 Full jobs instead describe
Windows Terminal as their saved foreground and report focus `unchanged`. This environment comparison is not a
controlled architecture experiment and does not establish an ARM64 control-focus defect.

The archived result omits the capture-time minimized flag, foreground-thread availability, whether a focus query
ran, its success/error and whether its returned HWND was null. Source and logs therefore cannot distinguish the
minimized guard from the other null-focus paths for that exact snapshot. A preparation log before capture is not
a capture-time observation. Do not report the exact branch as proved or relabel `none` as verified restoration.
Raw job logs, twelve child receipts, hashes and the comparison audit remain privately retained; this section is
the sanitized result.

Existing deterministic coverage supports the intended guard without filling that runtime gap:
`TestDesktopLeaseDoesNotCaptureStaleFocusFromMinimizedForeground` completes once in the `InteractiveLease`
log of every one of the six native profiles. Its fake desktop has a stale focus on a minimized foreground;
the test requires a null saved focus, `NotRecorded`, successful foreground/pointer restoration and no focus-setting
call. The fake-desktop result is not evidence of which branch the real hosted snapshot took.

Concrete next steps, owned here by the qualification/test-harness owner:

1. Implemented by the [18 October diagnostic slice](#bounded-focus-capture-diagnostics-18-october-2026): add a
   bounded capture-reason diagnostic to the original `DesktopLease::Read` snapshot, before confirmation or
   warning activation. Distinguish absent foreground/thread, minimized foreground, failed focus query, successful
   query with null HWND and recorded live focus. Preserve query success and immediately captured error information
   where meaningful; do not infer the original state from a second later query. Keep raw desktop identities private.
   Copy the one shared snapshot/reason into both child receipts while retaining their independent durations.
2. Implemented by that diagnostic slice: extend the fake-backend/result/presentation checks for those distinct
   outcomes and for visible-focus restoration
   and read-back failure. Retain the existing minimized-stale-focus regression, failed-restoration rejection,
   hosted-marker/ownership guards and the difference between `none`, `unchanged`, `restored` and `failed`.
3. Run one separately identified native ARM64 Debug hosted diagnostic of Menu plus NativeTextInput and inspect its
   single capture reason and restoration. Bind source, executable and result hashes, retain the original records,
   and keep all existing preparation, warning, child and restoration limits. This diagnostic is attribution work,
   not a replacement for the original receipts or performance/physical-input qualification.
4. If the minimized-window branch is confirmed, retain it as an intentional absence of restorable active focus.
   Obtain separate native ARM64 evidence starting with a genuinely visible foreground and recorded live focus
   before claiming a complete focus round trip. Any ordinary interactive run requires the person's time agreement.
   Do not change the capture target, revive the privacy UI or invent a focus HWND to make the status appear verified.

No remaining qualification checkbox is closed by this investigation. ARM64 opt-in resources, physical input and
device removal, real IME/AT/touch, consumer adoption, trusted policy and paired acceptance remain open. Timing/FPS
and process-memory bands remain 5% and 2%; exact budgets, allocation ceilings, probe deadlines and the
`qualified=false` policy are unchanged. The 17 October investigation changed documentation only; its runtime/test
behavior and gallery pixels were unchanged, so it required no gallery regeneration. The later diagnostic
implementation and its validation follow below.

### Bounded focus capture diagnostics (18 October 2026)

This test-harness slice starts from PR #86's merged `0dbbad09e10d9e71873f2ac37b3b1765b33930b5`.
It implements attribution instrumentation; the historical ARM64 `focus=none` observations still lack a capture
reason and cannot be retroactively assigned to a branch. This plan remains the single shared qualification owner.

[`DesktopLease::Read`](../../../Tests/Support/Support.Tests.DesktopLease.h) now saves a fixed enum and one
unsigned Windows error code alongside the original desktop state. Eligible capture performs one
`GetGUIThreadInfo` query; failure preserves `GetLastError` immediately, and successful/skipped queries retain
zero. No second diagnostic query, worker, timer, event stream or changed capture target is introduced.

| Original capture reason | Query observation | Focus-restoration implication |
| --- | --- | --- |
| `no-foreground` | No query; foreground was absent | No saved focus; `none` remains unverified |
| `no-foreground-thread` | No query; captured foreground had no available thread | No saved focus; `none` remains unverified |
| `foreground-minimized` | No query; minimized guard intentionally rejects stale thread focus | No saved focus; `none` remains unverified |
| `query-failed` | Query failed; immediate error code retained, including zero if the API supplied none | No saved focus; `none` remains unverified |
| `query-returned-null` | Query succeeded with null focus | No saved focus; `none` remains unverified |
| `recorded` | Query succeeded with nonnull focus | Original focus target is restored/read back under the existing contract; failure still fails the lease |

[`RunWithDesktop/FormatLeaseResult`](../../../Tests/Support/Support.Tests.InteractiveLease.h) logs this
snapshot before confirmation and writes exactly `focus.capture.reason` and `focus.capture.error` to the native
result. [`InteractiveRun.psm1`](../../../Tools/InteractiveRun.psm1) validates both fields and presents capture
separately from restoration. Missing historical diagnostics establish no reason; repeated, partial, unknown,
out-of-range or contradictory diagnostics fail validation. [`test.ps1`](../../../test.ps1) copies the one
shared `lease.focusCapture` (`Reason`, `QueryError`) to both children while retaining each child's duration.
The added diagnostic contains no HWND, PID, thread ID or desktop identity. Existing raw logs stay private.

Deterministic coverage extends [`InteractiveLease`](../../../Tests/Controls/DxUi.Tests.InteractiveLease.cpp)
and [`Test-InteractiveMode.ps1`](../../../Tools/tests/Test-InteractiveMode.ps1): all six paths; skipped-query
and exactly-one-query counts; immediate errors surviving later desktop reads; immutable original capture when
confirmation and children change the desktop; the existing minimized-stale-focus and visible-focus restoration
cases; successful focus-setting followed by failed read-back; native result serialization; legacy/malformed
metadata; honest `none`/failure presentation; and actual runner receipt mapping for both child durations.
Real Win32 backend checks remain read-only and use a never-shown test window.

The final receipt-path review also repairs the missing-native-result fallback in `test.ps1`: it now supplies
null capture metadata and an empty problem list, so strict mode can still write the failed child receipt.
The actual runner-assignment regression fails against the preceding driver with a missing `FocusCapture`
property and passes against the repair, using the same test body. The 36-case PowerShell suite includes that
path. Native/build checks are refreshed after the driver repair; the counterfactual and prior attempts remain
in the private packet. This fallback does not invent a capture reason or turn a missing result into success.

The [public evidence packet](../../../Measurements/InteractiveLease/2026-10-18-capture-reasons/README.md)
records source/artifact identities and sanitized results; original logs and attempts remain in private
`Z:/DxUiFocus`. After the final missing-result repair, all six builds passed: x64 and ARM64 Debug, Release
and ASan Debug. The three x64 noninteractive runs passed all 19 selected suites each, with zero capability
skips and 45 completed InteractiveLease cases per configuration. The isolated ASan detection probe passed.
These selected passes leave repository qualification `NOT_EVALUATED`; ARM64 builds are cross-build evidence.
The initial PowerShell pass exposed numeric-type mismatches in three new assertions; corrected expectations
yield 36/36 passing cases. A signed/unsigned comparison in a new C++ assertion failed `/WX` in the initial
x64 Debug, ARM64 Debug and x64 Release attempts; an explicit `DWORD` expectation repairs it. Those failed
attempts are retained alongside the passing final matrix.

The approved foreground runs use three separate shared Menu/NativeTextInput leases, each with automatic Start
enabled and the visible 30-second countdown (Start remains available). All three original snapshots report
`recorded`/error zero; both child receipts retain identical shared metadata and separate child durations.

| Local x64 profile | Menu | NativeTextInput | Foreground / focus / pointer restoration |
| --- | --- | --- | --- |
| Debug | 110 cases pass | 165 cases pass | restored / restored / unchanged |
| Release | 110 cases pass | 83 cases complete; case 84 fails | unchanged / unchanged / restored |
| ASan Debug | 110 cases pass | 103 cases complete; case 104 fails | restored / restored / restored |

All six executions record zero capability skips. These are four passing complete suites and two failed complete
suite attempts, not six passes. All three desktop restorations are verified independently of child outcomes.

The approved local Release foreground run exposes a separate unresolved failure:
[`TestNativeTextInputImePreviewCancelsWhenFieldBecomesIneligible`](../../../Tests/Controls/DxUi.Tests.NativeTextInput.cpp)
completes 83 earlier cases, then fails the 84th case at `native IMM transition fixture exposes only its active
preview`. It fails before the field becomes read-only/masked. Menu passes all 110 cases. The shared lease
retains `recorded`/error zero and verifies foreground/focus unchanged, pointer restored; valid diagnostics and
restoration do not turn the failed child into a pass. One isolated run of that exact case on the same Release
executable passes with zero skips and verified restoration. The full failed receipt remains authoritative for
that execution; no full repeat replaces it and no cause is established by the isolated pass.

Source review identifies a diagnostic gap: this fixture's `RunWhileForegroundHeld` covers activation/session
setup, then returns before `WM_IME_STARTCOMPOSITION` and `WM_IME_COMPOSITION`. The native handler rejects a
lost-focus/ineligible host or stale session/composition revision, but the failed assertion records neither the
actual preview nor stage/session observations. The lease's original desktop snapshot cannot explain state
inside that later child. The next bounded investigation should retain passive foreground/focus/session,
composition/revision and text-equality/length observations around START, payload delivery and the preview
assertion, including the read-only/masked subcase. Exercise a controlled focus/session loss before attributing
the failure. Keep preview semantics, assertions, guards and deadlines unchanged; this is an open native-input
qualification observation, not a demonstrated consequence of capture diagnostics. Library and NativeTextInput
fixture sources are unchanged in this slice.

ASan Debug stops later in
[`TestNativeImeCommitOwnsOneUndoUnitAndLateResultPreservesApplicationText`](../../../Tests/Controls/DxUi.Tests.NativeTextInput.cpp):
103 cases complete, and case 104 fails `undo fixture creates one earlier independent typed edit`.
Its existing `[IME-UNDO-SETUP]` trace records one attempt, `activated=false`, terminal `foreground-transfer`,
`onChar=false`, all three text lengths zero, logical target/native session true, zero foreground losses and
`sequenceHeld=true`. Source confirms failed activation returns before `OnChar`; composition and undo assertions
have not run. This establishes a setup failure, without explaining why Windows refused/lost the foreground
transfer. A no-loss observation does not itself establish successful activation. The bounded
[follow-through below](#native-input-fixture-readiness-follow-through-18-october-2026) exercises initial activation
refusal explicitly, preserves the existing 800 ms activation limit and five-run bound, and keeps successful
activation plus the original text/composition/undo predicates
mandatory. No full ASan retry replaces this receipt; it is an assertion failure, not an ASan memory diagnosis.
The intentional isolated ASan detection probe remains separate.

The retained pre-implementation Debug receipt was compared with the first diagnostic candidate before the
final PowerShell missing-result repair. The ordinary comparator reports **seven timing flags**, including
clean FPS about **13.94% lower**. That candidate completed while this run's ARM64 ASan build was active.
One subsequent serialized repeat uses the **same retained candidate executable** after this run's other
builds/tests finished and reports **ten timing flags**, including clean FPS about **54.80% lower** and dirty
FPS about **9.97% lower**. Neither comparison flags process memory, exact surface budgets or allocations.
Both comparisons and all original samples are retained; no passing repeat, causal attribution or accepted
non-regression is claimed, and there is no repeat-until-green loop.

These smoke observations have explicit limits: library/benchmark source fingerprints and compiler version
match, but executable hashes and v2 toolchain identities differ between baseline and candidate. Candidate
validation added the discovered CMake directory to PATH. Baseline receipts and artifact hashes were retained,
but the original baseline image was not; the diagnostic candidate images are retained. A separate child-shell
attempt was refused by build attestation before native sampling; the original parent environment matched all
four saved identities and admitted the actual repeat without relaxing attestation. The timed Embedded fixture
does not execute the desktop lease, so these numbers cannot measure the new capture diagnostic's overhead.
They do not qualify the policy or establish a change-induced regression. The public packet retains both flag
sets and these source/image/environment distinctions.

Performance acceptance stays on HOLD. The next performance action remains stable matched controls with both
images retained and trusted paired-policy qualification. Defer performance acceptance; seek profiling-backed
optimization, scope-reduction or deferral advice if stable matched evidence confirms degradation. Do not
change control hot paths, widen thresholds or replace the retained baseline to make this smoke comparison pass.

The next runtime step remains **open**: run the separately identified native ARM64 Debug hosted Menu plus
NativeTextInput diagnostic described above, bind source/executable/native-result hashes and inspect one shared
capture reason. This x64 machine's deterministic tests and ARM64 cross-builds cannot supply that observation.
If it confirms `foreground-minimized`, retain intentional unrecorded focus and obtain separate native evidence
with a genuinely visible foreground and recorded live focus before claiming a verified round trip. Do not
revive the privacy UI or change the capture target for a better-looking status.

No remaining qualification checkbox is closed. All performance-policy, paired acceptance, ARM64 opt-in resource,
physical-input/device-removal, real IME/AT/touch and consumer gates remain open. The 900 ms hover guard,
180 ms pointer bound, restoration/child/countdown limits, 5% timing/FPS and 2% memory bands, exact budgets,
allocation ceilings and `qualified=false` policy are unchanged. Production library code, public APIs and gallery
pixels are unchanged; this diagnostic slice needs no gallery regeneration.

### Native-input fixture readiness follow-through (18 October 2026)

This repair continues the two local failures retained in the bounded-capture packet above. It does not replace
those failed executions or attribute the historical Release failure to a particular desktop event. The shared
qualification owner and every remaining acceptance gate stay here.

Source review confirms two fixture gaps. The IMM eligibility test checked only activation inside its foreground
guard, then asserted preview outside it. The undo setup returned early after failed activation, while the generic
guard interpreted zero loss events as a held sequence. The ASan trace directly records that second path.

The repaired IMM fixture observes the complete activation/START/preview/read-only-or-masked transition under
`RunUntilForegroundHeld`, builds fresh state for each discarded attempt, and defers the original correctness
predicates until a held attempt is selected. Five fixed attempts contain four passive scalar stage observations
each. Controlled regressions remove focus before and after preview for both policy transitions, without an
external application. A missing preview on a held attempt still fails immediately; correctness failures are not
retried. Initial activation is now an explicit attempt outcome: refusal consumes one of the existing five
attempts, and exhaustion fails setup rather than skipping. Undo setup reports that outcome and retains its
original independent edit, composition commit and two-undo assertions. Pure guard regressions cover initial
refusal without any loss event, bounded permanent refusal, takeover classification and a held failed predicate.

The [fixture-readiness packet](../../../Measurements/TextInput/2026-10-18-fixture-readiness/README.md) and its
[hash-bound evidence](../../../Measurements/TextInput/2026-10-18-fixture-readiness/evidence.json) retain six
passing builds and 57 passing noninteractive suite executions, with zero skips. The three pure guard
regressions and 45 InteractiveLease cases pass in each x64 profile; the isolated ASan detection probe detects
its intentional fault. Full Debug and Release Menu/NativeTextInput pass all 110/167 cases respectively with
zero skips and verified restoration. Five focused Release cases pass. Three controlled nonactivating ASan
cases pass under the lease, including both new native regressions. These controlled passes do not close the
full ASan foreground gate. The old initial-activation behavior fails the new pure regression in a retained
Release counterfactual; the separate setup-only IMM mutant was built but not executed.

Two full ASan leases remain failed. In the first, Menu completes 110 cases with one activation skip;
NativeTextInput completes 83 cases and fails the masked preview. Its trace already shows length six,
one notification and a nonmatching base **before composition START**, with successful activation/focus/native
session and zero foreground losses. The person at the desktop confirms interacting during that run; this is
consistent with the extra edit, without identifying the specific input. The guard does not repeat that ready
correctness failure. One countercheck at identical source/executable hashes fails the unmodified Menu fixture
after 13 cases at `create a window on another thread`; the existing 5,000 ms wait/null-HWND predicate records
neither the failed stage nor a Windows error, so it cannot establish a creation failure versus readiness timeout.
NativeTextInput completes 104 cases, then records five `foreground-transfer` refusals, no `OnChar`, no edit,
zero loss events and `sequenceHeld=false`. The repaired guard correctly fails setup at the existing bound,
rather than treating failed acquisition as a held editor sequence or a capability skip. Both leases verify
restoration; neither is an ASan memory finding. No further full retry replaces them.

Before implementation, `Z:/DxUiInput` retains the previous source, all six profile images and original
logs/receipts, plus a new Release ComplexUi baseline. The one serialized candidate comparison flags nine
timing/FPS metrics: clean FPS is 10.24% lower and dirty FPS 8.11% lower. Process memory, exact budgets and
allocation metrics do not flag. Library/benchmark/toolchain fingerprints match; executable hashes differ,
and both baseline and candidate images are retained. The timed Embedded fixture does not execute the readiness
helpers; this unpaired sample cannot attribute their cost or establish change-induced regression. Retain these
flags alongside the earlier seven/ten-flag observations and memory uncertainties. Performance acceptance stays
on HOLD; no repeat, threshold change or rebaseline replaces the sample.

Concrete remaining work, owned here:

1. Obtain a separately identified full ASan foreground run with the desktop left available. Keep both failed
   leases and the original pre-repair failures. Controlled fixtures are separate evidence.
2. If acquisition remains refused, record bounded activation/admission stages under the existing limits.
   For the unmodified Menu setup, distinguish class registration, window creation, activation and readiness
   publication with immediately captured errors under the same 5,000 ms bound before choosing a repair.
3. Run the separately identified native ARM64 capture diagnostic; cross-builds and x64 receipts cannot
   establish its capture reason or focus round trip.
4. Establish stable matched performance controls with both images retained and qualify the trusted policy.
   Seek profiling-backed optimization, scope reduction or deferral advice if degradation is confirmed.

The final repository review runs `validate.ps1` and `format.ps1 -Check` on the isolated exact-input checkout;
private logs retain their outcomes and hash guards preserve unrelated work. No remaining checkbox is closed.
Native ARM64 execution, opt-in resources, physical input/device removal, real IME/AT/touch, consumer acceptance
and trusted paired performance qualification remain open. The 800 ms activation limit, five-attempt bound, 900 ms hover guard,
180 ms pointer bound, watchdog/lease/countdown limits, 5% timing/FPS and 2% memory bands, exact budgets,
allocation ceilings and `qualified=false` policy are unchanged. This fixture-only change affects no library API
or control pixels; gallery regeneration is unnecessary.

### ASan foreground setup follow-through (18 October 2026)

The [setup packet](../../../Measurements/TextInput/2026-10-18-asan-setup/README.md) continues the failed full
ASan leases in the [fixture-readiness packet](../../../Measurements/TextInput/2026-10-18-fixture-readiness/README.md).
Original receipts, confirmed desktop interaction, activation refusals and the uninstrumented Menu creation timeout
remain retained. The requested local native checks pass; no remaining qualification checkbox is closed.

Menu creation and activation are now separate. The worker shows without activation, publishes readiness or an
immediate registration/creation error, and waits for permission to activate. The readiness wait services sent
callbacks without draining queued input/probes and rejects readiness after its original 5,000 ms deadline.
The owner is reacquired after UIA/witness preparation. The background queue-focus state is explicitly established
while the witness retains actual foreground, rather than assumed to remain stale across setup. Intentional
transfers keep total loss history and separate unexpected-loss counters. Held behavioral failures never retry.
The same 800 ms polling parameter, five-attempt policy and 2,000 ms foreground observation remain. Teardown waits for
actual kernel thread termination while servicing callbacks; a completed worker-body flag is insufficient for
COM cleanup. Deterministic tests cover these protocol boundaries and the non-pumping callback counterfactual.

Bounded native diagnostics record existing direct/fallback requests, attachment/detachment outcomes and immediate
attachment errors. `SetForegroundWindow` contributes its documented BOOL, not a stale last error. The focused ASan
NativeTextInput run passes all five selected cases with zero skips and verified restoration: direct acquisition
is refused, the attached fallback succeeds, and both actual foreground and focus read back true. This establishes
that this fallback works in that run; it does not attribute the five historical refusals or justify bypassing them.
No additional native activation operation, retry or wait is introduced by the diagnostics.

Three retained Menu diagnostics expose separate failures. The first passes both controlled witness cases then
fails UIA setup without stage attribution. The second creates the witness successfully but its activation request
is refused and the old fixture records one skip. The prepared fixture's next run fails earlier: COM activation,
2,000/3,000 ms UIA timeout settings and the base-interface query succeed, but `CreateCacheRequest` returns
`E_FAIL` (`0x80004005`), before event subscription or witness creation. A separate nonactivating COM probe creates
a cache successfully. These observations do not establish an initialization cause; do not claim a warm-up or a
passing repeat repaired that failure. A cold-client regression isolates cache creation from foreground behavior.

The final source has six passing builds (x64/ARM64 Debug, Release and ASan Debug), 57 passing affected x64
noninteractive suite executions, and all six full Menu/NativeTextInput executions passing without capability
skips. Each profile completes WindowHost 133 cases, InteractiveLease 45, Menu 113 and NativeTextInput 167.
All three approved automatic-Start leases verify restoration: Debug/ASan desktop states are unchanged; Release
restores foreground/focus and leaves the pointer unchanged. The formerly failing setup transition passes in each
profile. These current-source passes do not attribute the retained earlier failures. Resource fixtures were not
rerun here, and cross-builds supply no native ARM64 execution.

One serialized Release comparison against the retained pre-implementation baseline raises five unpaired flags:
clean FPS -16.11%, frame p95 +16.01%, compose CPU p95 +9.66%, dirty private bytes +3.10% and private peak +3.08%.
The status remains `advice-required`; defer acceptance until stable controls and trusted paired evidence provide
attribution. No repeat, rebaseline, optimization claim or threshold change replaces these observations. Earlier
nine timing flags, older comparison flags and the MultilineGrid memory uncertainty remain retained.

This ledger remains the single owner of these next steps:

1. For another cache-factory failure, preserve the exact source/image, stage/HRESULT, Windows build and UIA-core
   version/hash, then isolate cold client setup with and without the prepared host. Collect bounded call attribution
   before proposing an initialization change; do not add warm-ups, retries or longer UIA timeouts on speculation.
2. For another native refusal, use the direct/attachment/request/detach and final read-back fields to identify the
   failed operation. Preserve all five attempts and any observed external loss; permanent refusal remains failed setup.
3. Observe the same capture-reason fields on native ARM64. `focus=none` remains unverified restoration, and a cross-build
   supplies no runtime evidence. Run resource, physical-input, real IME/AT/touch and consumer obligations separately.
4. Keep timing/memory advice on HOLD until stable controls and trusted paired evidence support a decision. Existing
   flags, the MultilineGrid uncertainty and retention limits remain attached to their original measurements.

The private `Z:/DxUiSetup` packet retains preceding source/images, each diagnostic's source/image identity and
raw receipt, and a pre-implementation Release ComplexUi baseline. Native ARM64, opt-in resources, physical input/
device removal, real IME/AT/touch, consumer and trusted paired-policy gates stay open. Earlier timing flags,
MultilineGrid memory uncertainty and retention limitations remain. All deadlines, thresholds, budgets,
allocation ceilings and `qualified=false` policy are unchanged. This harness slice changes no library API,
consumer adapter or control pixels; documentation/gallery review requires no visual regeneration.

### PR #87 post-merge observation (19 October 2026)

[PR #87](https://github.com/RedSalamanders/DxUi/pull/87) merged as
`5da68bcec49c0df2c9158ede168779ebf9737a52` under the user's explicit implementation-only exception.
The [post-merge run 39187090924](https://github.com/RedSalamanders/DxUi/actions/runs/39187090924)
passes all six native jobs, eight enabled public-consumer checks, validation, Windows tooling, formatting,
native scope and `ci-gate`; the separate formatting workflow also passes. The
[reviewed post-merge summary](../../../Measurements/TextInput/2026-10-19-pr87-postmerge/README.md)
binds that exact source to 126 core suite receipts plus two x64 Release characterization receipts,
all successful with zero skips. Every profile completes Menu 113/113 and NativeTextInput 169/169.

Retained traces verify the corrected reconciliation boundary on native ARM64: one text/one layout notification
for the current turn, a separate later geometry/layout notification and an inert duplicate dispatch, with stable
session/activation identity. The whole-queue counterfactual still exposes the old count predicate. These passes
do not reconstruct the original ARM64 failure's missing counts or attribute its precise event ordering.

ARM64 Debug, Release and ASan Debug retain `focus=none`, `focusCapture.Reason=foreground-minimized` and query error 0.
These are three shared Full leases, not six independent Menu/NativeTextInput snapshots. The initial foreground
was minimized before confirmation and either child; no initial keyboard focus was saved, and the no-saved-focus
path returns `none` without attempting focus restoration. This is neither a child focus failure nor a restoration
timeout. Native functional execution passes, while ARM64 focus restoration remains unverified.

Next, retain the source-specific captures and obtain separately approved ordinary-host/native ARM64 evidence
with a visible foreground and recorded initial keyboard focus, then verify restoration. Keep missing initial
focus separate from child focus behavior; preserve capture/refusal checks, deadlines and attempt bounds.
No new activation retries or longer waits are authorized by this evidence update.

The push skips paired measurement by design. The final PR paired run remains inconclusive with
`policy-review-required`/`qualified=false`, and its aggregate failed on that result. The authorized implementation-only
merge and green push CI establish no accepted paired non-regression. All 20 qualification HOLDs remain open,
with all deadlines, thresholds, exact budgets, allocation ceilings and policy unchanged. Earlier performance flags,
MultilineGrid memory uncertainty, failed/refused/cancelled receipts, historical attribution gaps and retention
limits remain. Native ARM64 opt-in resources, physical input/device removal, real IME/AT/touch and consumer
pins/product acceptance remain separate obligations. This documentation-only update changes no pixels or examples;
gallery regeneration is unnecessary.

### PR #87 staged TSF notification follow-up (18 October 2026)

PR #87 run `39160627044`, at `ee63697c6c91a139aded97daa95a2335181d3dcc`, passes five Full native
runs but fails ARM64 Release NativeTextInput in `TestNativeTextInputTsfCallbacksPreserveReentrantSameControlSession`:
the exact post-commit text/layout notification-count predicate fails after the earlier text-retention and
posted-work-drain predicates pass. Menu passes with zero capability skips in that job. The test body and production
implementation are unchanged; shared-harness timing effects are not excluded. The original receipt omits the
before/after notification counts, so missing, duplicated and earlier notifications remain distinct hypotheses.
The failed log/report and their hashes are retained privately under `Z:/DxUiSetup`; a green repeat cannot attribute it.

The [bounded count-boundary packet](../../../Measurements/TextInput/2026-10-18-staged-reconciliation/README.md)
retains an instrumentation-only Release pass and a controlled queued-geometry counterfactual. The former records
text/layout counts `0/1` and `0/1`, with unchanged session/activation identity. In the counterfactual a whole pump
records one text notification and two layout notifications: the exact old predicate is false while valid independent
geometry work runs. Dispatching the current production reconciliation turn separately records one text/one layout;
the later queued geometry then adds its own layout notification. This proves the observation-boundary defect,
without reconstructing the original ARM64 ordering.

The repair retains the exact one-text/one-layout predicate for that turn, the existing reentrancy/lifetime checks,
and the original drained-work check after the full pump. A sink may legitimately request a later deferred lock.
Two registered cases cover following queued geometry and the old whole-queue counterfactual; an idle duplicate
record must repeat none of the three notification families. Two bounded scalar records per existing foreground
attempt retain counts, cookies, activation identity, geometry and actual focus/foreground, with no new wait or retry.

The intermediate three-case probe has zero native skips and passing assertions, but its lease fails cursor restoration.
The person confirms desktop interaction and readiness for the remaining approved runs. That is context rather than
causal proof: the failed lease and its source/image identities remain retained, and cannot qualify complete foreground
validation. The final matrix has six successful builds, 57 affected noninteractive x64 executions and three full
NativeTextInput runs (169 cases each, zero skips and verified desktop restoration), bound separately to final source
and images. The five candidate validators, all 18 tooling groups and formatting pass. The initial whole-checkout
validation reports unrelated local measurement metadata/path errors and a malformed new changelog fragment;
the fragment is fixed and all five validators pass in the isolated candidate. The failed log remains retained.
The single retained Release comparison is within the unchanged noise bands with zero new flags; it is unpaired
and clears none of the earlier flags. Full Menu/resource receipts remain attached to their earlier source/images;
at that local evidence cutoff, new-head hosted Full validation and native ARM64 execution were pending;
the exact merged-source observation above records their later results. No exact assertion, deadline, threshold, attempt bound,
budget or policy is relaxed, and all 20 qualification gates remain open throughout this investigation.

ARM64 Debug and Release receipts now identify `focusCapture.Reason=foreground-minimized`, with query error 0.
That describes the unavailable initial focus in those captures; neither lease verifies focus restoration.
This test-only repair changes no production library/API, consumer adapter or control pixels; the documentation/gallery
review requires no visual regeneration. The post-merge observation above retains bounded native ARM64 Release
counts and session identity at the exact merged source. Earlier failures, performance flags,
MultilineGrid memory uncertainty and retention limits remain attached to their original evidence.

### PR #84 hover-switch follow-up (18 October 2026)

PR #84's post-merge [run 39072493730](https://github.com/RedSalamanders/DxUi/actions/runs/39072493730),
attempt 1 at `51ef9f292418d7c11fa1058b897ee84d52674106`, fails native ARM64 Debug Menu at
`TestMenuBarHoverMessageSwitchesRootWhilePopupOverlapsMenuBar`: 72 cases complete, and the 73rd fails the
explicit-hover root-switch assertion. The other five native profiles, all six NativeTextInput executions
(165 cases each) and all eight enabled consumers pass. Of 128 native suite receipts, 127 pass and one fails,
with zero capability skips; the aggregate correctly fails. Push skips paired measurement. Keep this failed
receipt separately from the earlier successful PR #83 observation and any later repaired-source result.

The [popup-readiness plan](PopupReadiness_2026-10-17.md) owns this source-specific diagnosis and repair. Its bounded
controlled Release test reproduces the exact assertion with the unchanged library: hover posting succeeds,
the trace records 136 paint injections and the hover callback never enters; the old visible popup continues answering text
queries, longest 9 ms. Registered hover input was fetched only through the general queue after popup paint.
Prioritizing its exact registered message after mouse/key input repairs that source-reachable starvation path.
The deterministic pending-paint case uses the existing test hook; its regression/trace source is frozen across
the controlled library comparison. The original untraced ARM64 occurrence does not establish that it took
this path, and `focus=none` is still not causal evidence. Raw traces and desktop identities remain private.

The repaired candidate passes 57 noninteractive x64 suite executions and all 110 full Menu cases in each of
Debug, Release and ASan Debug: 330 full Menu case completions, zero capability skips, hash-bound receipts and
verified restoration in all three approved leases. Its unchanged-source regression fails against the old
library and passes against the priority fix. These source-specific functional results do not establish
performance acceptance: the new unpaired Release comparison retains clean frame-p95 and clean/dirty composition
CPU-p95 flags under the existing bands. The first raw baseline receipt lacks separately archived original
benchmark binary bytes; rebuilding cannot repair that original-evidence gap. Keep the flags and retention limit.
Two predeclared repeats on the same candidate binary yield another timing-flagged `advice-required` comparison
and then `within-noise-budget`. All three observations remain retained; the last does not waive or explain the
earlier flags. They lack stable paired controls and continuous host-load admission and do not qualify policy.

All three ARM64 configurations cross-build successfully; output PE architecture checks match all six requested
configurations. The later exact-source native observation is recorded in the PR #85 section below. Cross-builds
and local x64 passes cannot substitute for native observation or resolve the original untraced occurrence.
The separate focus-capture reason and verified live-focus round trip remain open under the preceding
investigation. MenuResources/MenuResourceScaling, physical input,
real IME/AT/touch, consumer adoption, memory attribution and trusted-policy/paired acceptance remain open.
The 900 ms hover guard, 180 ms pointer guard, all other deadlines, 5% timing/FPS and 2% memory bands, exact
budgets, allocation ceilings and `qualified=false` are unchanged. Queue priority and diagnostic tests change
no gallery pixels or usage API; no visual regeneration or consumer pin change is required.

### PR #85 post-merge functional evidence (18 October 2026)

[PR #85](https://github.com/RedSalamanders/DxUi/pull/85) merged as
`7b59070b5567a8d8a8673d8b163e6bf700aa2af8` under explicit implementation-only authorization, retaining the
inconclusive paired/failed PR aggregate result and all qualification HOLDs. Its first-attempt
[post-merge run 39123726986](https://github.com/RedSalamanders/DxUi/actions/runs/39123726986) passes all six
native jobs, eight enabled relocated public-consumer checks, Ubuntu/Windows validation and tooling, formatting
and `ci-gate`. The separate [C++ formatting run](https://github.com/RedSalamanders/DxUi/actions/runs/39123726999)
also passes for that exact merge. Four non-ASan annotations-disabled consumer steps are disabled by the matrix;
they are not capability skips. No CI failure, retry or additional repair was needed in this post-merge run.

The six downloaded native artifacts contain 128 passing suite receipts and zero capability skips. Every x64 and
native ARM64 Debug, Release and ASan Debug profile completes 110/110 Menu and 165/165 NativeTextInput cases.
Both `TestMenuBarHoverMessageSwitchesRootWhilePopupOverlapsMenuBar` and
`TestMenuBarHoverMessageSwitchesRootWithPopupPaintPending` complete once per profile, with two passing bounded
hover trace summaries and zero dropped records. The healthy/wedged state probe, visible/hidden initialization
and minimized-stale-focus regressions also complete in all six profiles. Both ASan profiles diagnose the separate
intentional heap-use-after-free probe while their ordinary suites pass. Consumer receipts and performance raw
reports identify the exact clean merge source, and control-suite executable receipt hashes and performance
report/executable references agree per profile. The retained artifact hashes audit uploaded logs and receipts;
the upload does not contain the native executables for independent binary rehashing.

All three x64 Full leases verify foreground/focus/pointer restoration. ARM64 retains this separate limitation:

| Native ARM64 profile | Shared Full lease restoration | Child execution |
| --- | --- | --- |
| [Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39123726986/job/117664462059) | foreground `restored`, focus `none`, pointer `restored` | Menu 110/110; NativeTextInput 165/165; zero skips |
| [Release](https://github.com/RedSalamanders/DxUi/actions/runs/39123726986/job/117664462008) | foreground `restored`, focus `none`, pointer `restored` | Menu 110/110; NativeTextInput 165/165; zero skips |
| [ASan Debug](https://github.com/RedSalamanders/DxUi/actions/runs/39123726986/job/117664462065) | foreground `restored`, focus `none`, pointer `restored` | Menu 110/110; NativeTextInput 165/165; zero skips |

These are **three shared leases**, each represented by Menu and NativeTextInput child receipts, rather than six
independent snapshots. `focus=none` records unavailable initial focus, so these passes do not establish a verified
focus round trip or identify the capture-time null-focus branch. It is not a capability skip or proof of a control
focus defect, and it does not attribute the original untraced ARM64 hover failure. Keep that failed PR #84 receipt
and the original healthy-probe failure separately. This plan still owns the preceding capture-reason diagnostic,
distinct-outcome regression coverage and separately identified native ARM64 focus diagnostic. A genuinely visible
foreground with recorded live focus remains required before claiming complete focus restoration; all admission,
warning, ownership, child, probe and restoration limits stay in force. Raw desktop identities and audit packets
remain private; this is their sanitized result.

Push skips paired measurement, so its green aggregate supplies no new paired result. PR #85's
[paired run 39117607398](https://github.com/RedSalamanders/DxUi/actions/runs/39117607398), comparing base
`51ef9f292418` with tested merge `6d020ed6ab59`, remains inconclusive because the base policy is unqualified;
its paired and aggregate checks fail. Its retained comparisons have 69/72 unstable same-binary controls
(23/24 per scenario). The three diagnostic sets report zero regressed metrics and judge agreement, which cannot
qualify that policy, waive earlier memory/timing flags or repair the original binary-retention gaps. The tested
merge and actual merge have the same source tree; that identity does not establish non-regression.

No remaining qualification checkbox is closed by these functional observations. ARM64 focus restoration and
opt-in resources, physical input/device removal, real IME/AT/touch, consumer adoption, memory attribution and
trusted-policy/paired acceptance remain open. Preserve the 900 ms hover and 180 ms pointer guards, all other
deadlines, 5% timing/FPS and 2% memory bands, exact budgets, allocation ceilings and `qualified=false`. This
evidence-only follow-up changes no runtime/test behavior, public usage, consumer pins or gallery pixels; no
visual regeneration is required.

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
