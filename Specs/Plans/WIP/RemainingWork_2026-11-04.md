# DxUi remaining correctness and qualification

Status: ACTIVE
Updated: 7 November 2026
Owner: DxUi maintainers; consumer policy and adoption remain with each application.

This is the **only active DxUi plan**. The [consolidation record](../Done/WipConsolidation_2026-11-04.md)
maps every unchecked item from the four former queues to a current owner or explicit disposition. Archived
reviews/proposals retain detail and source identities without becoming parallel execution plans. Domain
specifications and `capabilities.json` remain authoritative.

The owner's **7 November 2026** delivery scope requires x64 Debug, Release and ASan Debug runtime tests and
ARM64 compilation in those three profiles. Native ARM64 runtime work is explicitly set aside for this delivery;
compilation does not establish runtime support, and the normal two-architecture production release gate is unchanged.

## Verified starting point

Reviewed main: `7257f3e31e575c633e8cbbbf3a74f3115f2eeb73` ([PR163](https://github.com/RedSalamanders/DxUi/pull/163)).
RedTilde's starting point consumes API 4 at `703a96c788a79c1e7b28097226a2ca0a735f11bc`. Its explicit quote-margin
follow-up adopts `8c6c2818` in [PR45](https://github.com/RedSalamanders/RedTilde/pull/45); publishing or merging
the library never advances another consumer's lock.

The later diagram-label correction uses API-4 implementation `2a7e7c28` in
[PR166](https://github.com/RedSalamanders/DxUi/pull/166). Its
[scoped closeout](../Done/DiagramLabelBackgrounds_2026-11-04.md) records six builds, all three x64 affected
profiles, the real ASan probe, the 19-suite Release union and RedTilde's original exact-pin comparisons.

| Completed scope | Evidence and limit |
| --- | --- |
| Styled Mermaid spans and ER/class shared attachment defects (FLOW-01/02), bounded sequence syntax expansion and readable default strokes | [Display closeout](../Done/MermaidDisplayCorrections_2026-11-03.md), exact implementation `703a96c7`. The broader per-element failure channel remains D-A1. |
| Label-aware corridors, table/code insets, accent task markers and optional completed-task strike-through | [Spacing/task closeout](../Done/DiagramSpacingAndTaskPresentation_2026-11-03.md), [packet](../../../Measurements/DiagramSpacing_2026-11-03/README.md). |
| Source-specific x64 Release library/consumer checks | 19 noninteractive SDK suites with zero capability skips; independent RedTilde 73-case checks and hidden captures. Other scope retains its own owner. |
| Latest completed main Release qualification | [Run 42146665655](https://github.com/RedSalamanders/DxUi/actions/runs/42146665655) passed all seven jobs at `460ddfa3`: native x64 (23 suites), native ARM64 (21), both assertion probes, the test gate and both production SDK roles. [Verified original receipts, all fifteen artifact wrappers and inner package hashes](../../../Measurements/DiagramMathAndWorkspaceCorrections_2026-11-06/hosted-qualification.json) bind this to implementation `8dc328de`. This does not qualify the later parser candidate, Debug/ASan ARM64, physical devices or paired performance. |
| Pending-Update timing accounting | Instrumentation and source-specific six-profile evidence are archived in [FrameUpdateAccounting](../Done/FrameUpdateAccounting_2026-10-26.md). This is accurate accounting, not a CPU improvement or performance-policy acceptance. |
| Compatibility branch publication D10 | Recorded published branches/SHAs remain in the [review decisions](../Done/ProductionReview_2026-11-02.md#wave-0--decisions-that-unblock-implementation); consumer adoption is D-C1. |

[API 5 PR162](https://github.com/RedSalamanders/DxUi/pull/162) merged on 6 November as `31a06270`.
The [failure-policy closeout](../Done/FailurePolicy_2026-11-02.md)
records qualified library source `f6791b5070efd1fa8dc86601bc9422ae9a321d2e`, x64 results and the owner's acceptance
of measured construction safety costs. That historical x64 packet does not establish native ARM64 execution,
formal frame-policy qualification or product adoption. Later hosted results stay source-specific;
earlier `d0179db6` evidence remains bound to its checkpoint.


Historical API 4 diagram/math/document/input implementation: `8dc328de`. The [scoped closeout](../Done/DiagramMathAndWorkspaceCorrections_2026-11-06.md) records six zero-warning builds, all nineteen noninteractive suites in each x64 profile, real ASan detection, 593 actual compiled inputs, native raster/geometry regressions and refreshed gallery. It closes the supplied examples, D3/D5, CTRL-01, MD-01/02/04 and MATH-01; D4 and the remaining parsing, rendering, failure and qualification rows retain their owners.

The parser follow-up is qualified locally at `0ffaa1a6`, API 4. Its [scoped closeout](../Done/MarkdownScanBounds_2026-11-06.md)
closes MD-03/05/06/07 with all six zero-warning builds, 57 noninteractive suite executions, real ASan detection,
594 actual compiled inputs, independent work bounds and refreshed native gallery. Remaining rows below keep their scope.
The same parser source now passes all seven hosted Release jobs at main `2ffe66c2` in
[run 42165386025](https://github.com/RedSalamanders/DxUi/actions/runs/42165386025): 23 x64/21 ARM64 suites,
assertion probes and both production SDK smoke checks. Its [verified receipt](../../../Measurements/MarkdownScanBounds_2026-11-06/hosted-qualification.json)
is independent of the later editor work and the remaining platform/performance gates.

## Priority 0 — lifetime and failure boundaries

<a id="d-l1"></a>
### D-L1 — embedded control lifetime

- [x] Implement the approved D0 deferral of embedded preparation wakes while input entry points are on the
  stack; deliver the wake once after target revalidation. Guard direct TextField setters/history/clear paths
  that can invalidate and then continue using a retired control (PRIOR-03).
- [x] Retain retiring-target and reentrant-positive fixtures, update hosting contracts and complete native
  teardown/resource coverage at `f765aeca`: six zero-warning builds, all 19 noninteractive suites in each
  x64 profile and the real ASan detector. [Scoped closeout](../Done/SelectionBlocksAndInputLifetime_2026-11-05.md)
  retains the original three sanitizer counterexamples and working-input/native archives.

Owner: `src/Rendering/Embedded.cpp`, `src/Controls/DxUi.TextInput.cpp`, input dispatch and `docs/hosting.md`.

<a id="d-a1"></a>
### D-A1 — checked failures and per-element admission

- [x] Reconcile PR162 against main and D2/D13: expected input/resource failures stay checked, local element
  refusal preserves editable source/diagnostics, and unexpected C++ failures follow the agreed app-owned fatal
  protocol. Resolve conflicting older exception-injection wording before changing the contract.
- [x] Complete the library candidate's build, x64 runtime/foreground, source-pin consumer and SDK/PDB checks,
  and record the owner-accepted source-matched construction measurements. Native ARM64 is deferred; formal
  frame-policy qualification remains D-P1.
- [x] Complete PRIOR-07's native host-wide preparation abort and error reporting locally. The
  [scoped closeout](../Done/NativePreparationIsolation_2026-11-07.md) binds implementation `2c8b267e`,
  same-object baseline/intermediate failures, independent healthy-sibling WARP pixels, explicit recovery,
  six zero-warning builds, 66 ordinary x64 suite executions and actual ASan. Fresh hosted native Menu,
  ARM64 and external-consumer acceptance remains D-Q1.
- [ ] Finish parser/preparer admission and per-element failure isolation across diagrams/math/images.
  Preserve checked capacity/unsupported causes and regression evidence; unexpected C++ exceptions follow
  the API-5 fatal contract rather than the superseded whole-view exception-recovery proposal.
- [x] Qualify the narrow DirectWrite lazy-formatting metrics boundary and 65,536-unit source/caret regression.
  The [scoped closeout](../Done/DirectWriteNativeFormatting_2026-11-07.md) binds `4ce9b6a9`, six zero-warning
  builds, 66 ordinary x64 suite executions, actual ASan and original-member verification. Original RedTilde
  `1d843628` objects fail fatally with SDK `5907e4f5`; replacing only the layout object lets oversized math
  complete and leaves the independent watch defect as an ordinary failure. First failures remain retained.
  The application's later ASan image-policy failure remains with its own candidate; this library scope does
  not qualify the full API-5 consumer.
- [x] Retire the native animation window before failure-host uninstallation and prevent callback rearming during
  Detach. The [scoped closeout](../Done/AnimationWindowRetirement_2026-11-07.md) retains the original consumer
  TLS diagnosis, independent public same-object failures/passes, sibling/cache and in-tick coverage, six
  zero-warning builds and 66 ordinary x64 suite executions at `06b8d518`. Explicit application adoption and
  fresh hosted/native-platform acceptance remain D-C1/D-Q1.

This coordinates the active API work; it does not adopt API 5 in any application or duplicate that branch's queue.

## Priority 1 — editing, parsing and presentation correctness

<a id="d-e1"></a>
### D-E1 — source guides and editor behavior

- [x] Complete the SDK portion of the 5 November chat request: Insert Quote and Code Block wrap selected source through
  shared DxUi logic in Rich and Source modes, preserve outside text/EOLs/direction and one-step undo/redo,
  and decline atomically for composition, read-only and capacity. The
  [source-bound closeout](../Done/SelectionBlocksAndInputLifetime_2026-11-05.md) records exact-output/history
  regressions and all three x64 profiles.
- [x] Verify RedTilde's real Insert commands against published API-4 `f765aeca`: [app PR58](https://github.com/RedSalamanders/RedTilde/pull/58)
  passes six builds, all 83 cases and 36 captures per x64 profile, actual Compose/Source/Split command dispatch,
  history/focus and refusals. [Consumer closeout](https://github.com/RedSalamanders/RedTilde/blob/main/Specs/Plans/Done/SelectedBlockInsertions_2026-11-05.md)
  preserves the original content-loss failure and broader acceptance boundaries.
- [x] Implement D1: cosmetic code ink/line numbers/whitespace never withhold content, cancel a gesture or
  refuse input; do bounded viewport/change work and show the previous cosmetic state while preparing
  (EDIT-01/02/04, RENDER-01/14).
  [Scoped closeout](../Done/EditorCosmeticPreparation_2026-11-06.md): API 4 `ba94f4eb`, six zero-warning builds,
  57 noninteractive suite executions, real ASan detection, independent work/source oracles and refreshed gallery.
  Consumer, native-platform/physical and paired-performance acceptance remain separately owned below.
  Hosted main `14386f36` now passes all seven Release jobs, 23 native x64/21 native ARM64 suites,
  assertion probes and both production SDK smoke checks in
  [run 42193587525](https://github.com/RedSalamanders/DxUi/actions/runs/42193587525);
  [verified original receipts and package hashes](../../../Measurements/EditorCosmeticPreparation_2026-11-06/hosted-qualification.json)
  close this source's Release gate without accepting later clipboard, Debug/ASan ARM64 or physical/performance work.
- [x] Implement D3's 5 November decision: permit natural source navigation/editing across the former active
  interval, including Backspace/Delete/Tab and nested fences. Update the source-editing scope from the current
  caret/selection instead of trapping input at an old boundary (EDIT-07/11).
- [x] Complete EDIT-03/06/09/10: metadata-only composition pixels, read-only shortcut propagation,
  contextual Unicode pairing and disclosure/zoom source-anchor preservation. The
  [scoped closeout](../Done/EditorInputViewport_2026-11-07.md) records retained baseline failures,
  actual WARP pixels/anchors, 200 reused chunks, six zero-warning builds, 66 ordinary x64 suite
  executions and actual ASan detection at `efef569e` (API 5). Explicit caret/source replacement,
  source/history/notification and resource regressions are covered. Native ARM64, physical input,
  paired performance and consumer adoption remain separately owned below.
- [x] Complete CMD-01/02/03/04/05: actual task paragraphs, shared parent markers, paragraph structural
  ownership and trailing footnote escapes. [Scoped closeout](../Done/CommandOwnership_2026-11-06.md)
  retains four same-fixture baseline failures, valid blank display-math characterization, six zero-warning
  builds, 66 ordinary native x64 suite executions and actual ASan detection at `c59094f5` (API 5).
  This closes command ownership locally; the editor, native ARM64, consumer and performance gates stay separate.

<a id="d-i1"></a>
### D-I1 — Unicode, clipboard and native input

- [x] Preserve pending surrogate leads across `VK_PACKET` key transitions/modifiers in both editors and hosts
  (CTRL-01), with real-message replay and no weakened assertions.
- [ ] Reconcile the native IME capacity and quoted-file-paste fixes required by RedTilde DX-4/DX-3 against the
  exact candidate; keep source, selection, history, decline paths and callback ownership intact.
- [x] Implement D4's SDK text-first default when Unicode and bitmap formats coexist, with an explicit
  native/embedded application image-first policy API and source-preserving decline behavior.
  [Scoped closeout](../Done/ClipboardPreference_2026-11-06.md): API 4 `a1915ffd`, six zero-warning builds,
  57 noninteractive suite executions, actual ASan detection and private format/ownership/source oracles.
  Combined API-5 source `977a9fb0` independently passes six zero-warning builds, all 66 noninteractive x64
  suite executions and actual ASan detection, with 680 original inputs and all native/run archive members
  verified. The original API-4 cohort stays separate; current-source hosted native acceptance remains D-Q1.
  RedTilde's bitmap-asset persistence and settings UI remain consumer work; no such UI is claimed here.
  The original compiled transport counterexample changes from bitmap to Unicode with the candidate library.
- [x] Implement D5: classify `//host/...` and `file://host/...` image destinations as network requests and
  apply remote-image permission; never route them through an ungated local-file path.
- [ ] Qualify remaining focus/TSF/menu closure and mask/IME findings from the
  [prior-review register](../Done/ProductionReview_2026-11-02/prior-closure.md). Preserve unexplained historical
  failures; do not infer their causes from later green results.
- [x] Qualify PRIOR-21/15's masked-field IMM/TSF correction locally. The
  [scoped closeout](../Done/MaskedNativeInputPolicy_2026-11-07.md) binds metadata-only status/password scope,
  real association restoration, external replacements/siblings, six zero-warning builds, 66 ordinary x64 suite
  executions and actual ASan at `5907e4f5`. Unchanged external fixture objects provide fail-before/pass-after
  evidence; physical IME acceptance and broader application adoption remain D-Q1/D-C1.
- [x] Complete PRIOR-25/26/27: all-collapsed Grid keyboard expansion, cancellation of unavailable forwarded
  ScrollPanel drag owners and NumericStepper terminal notifications through binding echoes/external updates.
  The [scoped completion](../Done/InputStateContinuity_2026-11-07.md) retains original baseline/first-candidate
  failures, unchanged-object diagnostic proof, six zero-warning builds, all 66 ordinary x64 suite executions
  and actual ASan detection at `a287710c`. Native/physical, tooling and consumer gates stay separately owned.

<a id="d-m1"></a>
### D-M1 — Markdown, math and diagram correctness

- [x] Reproduce and fix the 5 November examples: `graph BT` pipeline ranks/compact fan-in routing,
  class `direction LR` with `Document <|-- Markdown`, wide accents/dot weight/vector clearance,
  and gathered matrices/angle delimiters with non-overlapping row spacing. Preserve source and use
  deterministic native captures with source/DPI provenance for the Mermaid/LaTeX reference comparisons.
- [x] Correct deferred autolink suffix loss, front-matter incremental root reuse and CR-only YAML handling
  (MD-01/02/04). Expected malformed input must stay bounded and editable rather than aborting a process.
- [x] Resolve display-block interruption and pathological scan budgets (MD-03/05/06/07), with deterministic
  seeds, independent expected results and bounded logical work. [Source-bound closeout](../Done/MarkdownScanBounds_2026-11-06.md).
- [x] Complete display-style aligned/gathered/alignat cells and centered script-style substack (MATH-01); preserve source and measured row/delimiter geometry in the [scoped closeout](../Done/DiagramMathAndWorkspaceCorrections_2026-11-06.md).
- [x] Complete outstanding diagram note/routing/budget issues (FLOW-03/05), preserving exact source, local editing, geometry, history, UIA and resource limits. [Source-bound closeout](../Done/SemanticAndDiagramResources_2026-11-07.md) includes independent note collisions, 512-edge measured caption reuse, six builds and all 66 ordinary x64 suite executions.

FLOW-01/02's published fixes are complete. Do not recreate them as new work; independent regressions and
any *different* remaining failure-channel problem belong to D-A1.

<a id="d-r1"></a>
### D-R1 — semantic resources, rendering and accessibility

- [x] Match native arrow-label/cardinality backgrounds to the diagram canvas, with prepared contrast fallback,
  preserved notes/groups/selection, native raster/resource checks and exact-pin RedTilde capture verification.
  [Scoped closeout](../Done/DiagramLabelBackgrounds_2026-11-04.md) includes the cache-eviction brush-retirement
  counterfactual and source-bound final native matrix.
- [x] Correct quote/callout inner margins in shared geometry, including nested containers, lists, tables,
  RTL, narrow widths and source ACP hit testing; verify the consumer's themed-scrollbar adoption with an exact
  API-4 pin. The [scoped closeout](../Done/QuoteMarginsAndSequenceAllocation_2026-11-04.md) records all three x64
  affected-suite passes, the Debug allocation repair, native gallery review and consumer verification.
- [x] Compute color-glyph capability once per preparation and preserve the documented script/emoji fallback
  (SEM-01/RENDER-05), with actual target tests. The [scoped closeout](../Done/ScriptColorPreparation_2026-11-06.md)
  records source `1352cc7f`, six zero-warning builds, all 66 noninteractive x64 suite executions, actual ASan,
  original 680-input/native/run member verification and refreshed gallery provenance. Actual WARP pixels,
  observed WIC capability, controlled E_NOINTERFACE, cancellation and checked failure preserve publication.
  The unchanged compiled probe fails with the prior library and passes with this one. D6's broader policy,
  native ARM64/physical and formal paired-performance acceptance remain separately owned.
- [x] Resolve image-completion/animation re-preparation, placeholder direction, lost list markers and
  supplementary-plane word starts (SEM-02/03/04/05). [Source-bound closeout](../Done/SemanticAndDiagramResources_2026-11-07.md)
  records independent retained-baseline failures, six zero-warning builds, all 66 ordinary x64 suite executions,
  real ASan detection and reviewed harness gallery. Native platforms, consumers and performance remain separate.
- [x] Restore bounded chunk reuse with nondefault Source line height (RENDER-02), preserving exact sequential
  line geometry and pixels, clean/dirty/hidden work and per-frame resource rules. The
  [scoped closeout](../Done/SourceSpacingAndRadioNaming_2026-11-07.md) records six zero-warning builds,
  66 ordinary x64 suite executions, actual ASan, original-member verification and reviewed harness captures.
- [x] Name RadioButtons groups with actual native UIA Name events, unchanged-header suppression and embedded
  publication (A11Y-01); retained providers preserve navigation, selection and detach. The same
  [scoped closeout](../Done/SourceSpacingAndRadioNaming_2026-11-07.md) retains the baseline counterexample.
- [x] Raise the remaining semantic state events (A11Y-02), with actual native/embedded UIA-client event,
  idempotence, source/rebind cancellation and detach checks. The
  [scoped closeout](../Done/SemanticStateEvents_2026-11-07.md) retains six baseline failures and candidate
  passes, six zero-warning builds, 66 ordinary x64 suite executions and actual ASan; physical AT remains separate.

The remaining before-production P2/P3 IDs in the
[review inventory](../Done/ProductionReview_2026-11-02.md#before-production-p2-set) stay owned by D-E1,
D-I1, D-M1, D-R1 or D-G1 according to their area. Their detailed triggers are retained in the appendices;
omitting an ID from this summary does not dispose of it.

<a id="d-t1"></a>
## D-T1 — meaningful regression and adversarial coverage

- [ ] Add counterfactual fail-before/pass-after coverage for the retained P1 findings and earlier fixes that
  lack it (PRIOR-08); replace self-referential crossing, CF_HTML and font-truncation oracles.
  The separate regression-guard cohort adds actual grow-after-scroll, pending-sibling capture, unequal RTL
  cell widths, setter invalidations, native read-only fallback, unassisted UIA scroll publication and normal
  borrowed-model retirement assertions. API 5 supersedes R28-44's recoverable allocation proposal. These new
  guards remain pending native/counterfactual qualification and do not close this row by source inspection.
- [ ] Add deterministic pathological-input/fuzz/cancellation coverage for Markdown, diagrams and math;
  retain exact seeds, budgets, raw failures and source/artifact identities.
  The current test cohort adds signed-area crossing checks, independent CF_HTML wire/producer bytes,
  exact-size MATH buffers and 64 fixed-seed cases per grammar domain. Its safe ASan addressability proof
  passes 628 checks without an invalid read. Native integration and the six-profile matrix remain pending;
  the earlier review's seven counterfactual obligations are not closed by this partial oracle proof.
- [ ] Diagnose the retained failed-consumer-publication fixture's ten-second closure failure observed during
  the long-checkout validator run. Progress/body admission stayed correctly refused; an explicit-exit proposal
  also failed and was rejected, while a focused replay of the original source passes all 14 cases. Preserve
  both outcomes without assigning the old failure a cause or extending its watchdog to hide it.
- [x] Derive the semantic diagram allocation diagnostic's count from its actual array/shared execution bound:
  twenty semantic and five geometry scenarios. The [scoped closeout](../Done/ScriptColorPreparation_2026-11-06.md)
  retains original logs and source/per-request evidence; this corrects the label without changing the corpus or
  accepting an earlier paired verdict.
- [x] Separate descendant-timeout fixture startup from pipe drainage; retain missing-admission diagnostics,
  exact native process identity and cooperative release. [Scoped closeout](../Done/DescendantTimeoutFixture_2026-11-06.md)
  records all 168 profiling cases and a deliberate old-budget counterexample, without accepting performance.
- [x] Diagnose long-checkout tooling portability: the [scoped closeout](../Done/LongCheckoutTooling_2026-11-07.md)
  retains the failed MSVC aggregate and extended-prefix experiment, adds early launch-path admission, and
  fixes classic compiler resource staging. Both setup entrypoints pass 15 cases and a 180-character scratch
  root passes 58 evidence checks. The older 261-character hard-link failure remains unattributed; current
  hard-link success and supported bounded paths do not qualify the original failed aggregate.
- [ ] Update relevant catalog/factory, usage, gallery and design-system entries for actual control changes.
  Product screenshots come from owned harness scenarios with invented content and build/DPI provenance.

<a id="d-p1"></a>
## D-P1 — performance policy and resource acceptance

The fresh source-paired [run 42106904205](https://github.com/RedSalamanders/DxUi/actions/runs/42106904205)
stopped at benchmark attestation before measurement: a module-name lookup saw two checkout modules.
The [scoped tooling closeout](../Done/PairedAttestationModuleOwnership_2026-11-05.md) preserves the exact
failure and independent repair. No performance verdict exists for that original run. The repaired manual pair [42115893044](https://github.com/RedSalamanders/DxUi/actions/runs/42115893044) completed measurement but remains inconclusive: the base policy is unqualified, all 72 controls are unstable and dual-judge replay disagrees on Default dirty/frameP95Ms. All 144 raw receipt hashes/identities verify. This is the older 0acd5608/f765aeca pair, not acceptance for 8dc328de.

API-5 hosted [run 42201620823](https://github.com/RedSalamanders/DxUi/actions/runs/42201620823) also completed
inconclusively for `1ea6d522`/`a6ea0c15` with driver `44fdc9c1`: all 144 hashes verify, but 71/72 controls are
unstable and the base policy remains unqualified. The migrated judge flags clean preparation in Default and
MultilineGridDistinct (median 0.0003 to 0.0005 ms); the legacy judge additionally flags MultilineGrid preparation
and Default dirty private/peak memory (+2.10%/+2.11%), which the migrated judge places within noise. Preserve
these disagreements and [the retained triage](../../../Measurements/FailurePolicy_2026-11-06/current-hosted-triage.json).
The owner accepted the separate construction study's measured safety overhead; this does not qualify the frame
policy, erase its findings or close this section.

- [x] Correct unassessed verdict presentation after policy/evidence refusal. The already merged
  [scoped closeout](../Done/PerformanceVerdictPresentation_2026-11-06.md) retains the original 56/57
  baseline, all 57 candidate and 34 unchanged inference checks, plus both real refusal projections.
  This changes presentation only; trusted policy, controls and paired performance remain open below.
- [ ] Qualify the trusted measurement policy and controls independently. Preserve timing/memory witnesses,
  exact raw reports/seals, missing-context/refusal history and unstable-control verdicts.
- [ ] Complete the bounded timing/heap/resource attribution pilot before a fresh canonical same-binary A/A;
  retain independent orders, all scenarios, cache variants, public phase markers and the existing thresholds.
- [ ] Reconcile the pooled/blockwise migration disagreement and Holm slots with independent replay; a pilot,
  exact budget maximum or candidate green result does not qualify the base policy.
- [ ] Establish accepted paired evidence for the chosen candidate and affected consumer pilots; obtain measured
  advice for confirmed degradation without silently rebasing. Host/control, S0-S2 and FrameUpdate instrumentation
  acceptance feed this one owner; RedTilde R10 remains independently owned by the app.

<a id="d-q1"></a>
## D-Q1 — remaining platform, input and consumer acceptance

- [x] Reconcile the selected API-4 diagram candidate `2a7e7c28`: all six builds, eight affected suites in each
  x64 profile, the real ASan detector and all 19 noninteractive Release suites pass with zero warnings/errors
  and zero capability skips. [Exact evidence](../../../Measurements/DiagramLabelBackgrounds_2026-11-04/qualification.json)
  includes rendering/WARP/resource coverage. API-5 library x64 qualification is separately closed; cross-builds do not establish native ARM64 runtime.
- [x] Verify native Release main `ab850a40` in hosted run 41849905967: all 23 x64 suites and 21 ARM64 suites
  execute with zero skips, both assertion probes pass, and both production SDK ZIPs pass extracted-consumer
  smoke. This supplies current API-4 native Release coverage without accepting the remaining profiles or fixtures.
- [x] Qualify API-5 input-state source `a287710c` at exact hosted main `d603b7b3`: Debug `42388962206`
  and ASan `42388964704` execute 21 suites on both native architectures; Release `42389300701` executes
  23 x64/21 ARM64 suites and both production SDK smoke roles. The
  [source-bound packet](../../../Measurements/InputStateContinuity_2026-11-07/hosted-qualification.json)
  independently verifies original wrappers, canonical payloads and every inner SDK member. The later
  animation/masked-input changes, physical input, formal performance and consumer adoption retain their gates.
- [x] Qualify API-5 preparation source `2c8b267e` at exact hosted main `45288065`: Debug/ASan runs
  `42371883610`/`42371891344` pass 21 suites on each architecture, and Release `42371899884` passes
  23 x64/21 ARM64 suites plus both production SDK roles after the combined native gate. Original wrapper,
  canonical payload and inner package/library/PDB/license bytes are independently verified in the
  [hosted packet](../../../Measurements/NativePreparationIsolation_2026-11-07/hosted-qualification.json).
  Later input-state source and consumer adoption remain separate. Historical failures below stay failed;
  none supplies a retrospective cause for an earlier missing-output failure.
  The [current local correction](../../../Measurements/SemanticStateEvents_2026-11-07/README.md) at
  `93052665` diagnoses a source-bound ASan `LNK4300`, proves ARM64 inline-frame progress on two original
  dumps, and changes owned Menu obstruction ordering. Six builds and 66 ordinary x64 suite executions pass;
  its then-required fresh hosted execution is superseded by the exact preparation source above. Original failed cohorts below remain
  failed, and causes of older output-less or missing-log failures remain unassigned.
  [Run 42246752571](https://github.com/RedSalamanders/DxUi/actions/runs/42246752571), exact main `2f0a7bb9`,
  passes x64 but stops ARM64 in the filter preflight: unfiltered Grid exits 1 before normal ARM64 suites run.
  The wrapper omitted Grid's child output, so its cause remains unassigned. All eight original wrappers and
  the original job log are retained. The reporting correction preserves child output/identity before assertions;
  a later source must independently qualify. Production was withheld.
  Exact main `bc08f35e` adds the child diagnostics. Its Release run
  [42266539593](https://github.com/RedSalamanders/DxUi/actions/runs/42266539593) and ASan run
  [42265980391](https://github.com/RedSalamanders/DxUi/actions/runs/42265980391) now retain an ARM64
  saved-context unwind failure at the named Grid callback, after the child correctly terminates. The legacy
  walk returns an apparent PAC-signed return address. The PAC-aware verifier and complete failed-preflight
  dump/image retention are now implemented at `3a5ae6b0`, with six warning-free builds, 66 ordinary x64 suite
  executions, actual ASan detection and 58 evidence fixtures in the
  [scoped closeout](../Done/Arm64FatalUnwind_2026-11-06.md). Native ARM64 execution and the complete hosted
  test-before-production gate remain required. These original failed cohorts remain retained, and their
  evidence does not retrospectively assign a cause to the earlier output-less failure.
  At exact main `ac495f56`, native Debug [42302215623](https://github.com/RedSalamanders/DxUi/actions/runs/42302215623)
  verifies both named Grid origins with the PAC-aware walker (four attribute queries each), then stops in
  a later ordinary paint test. Original matched EXE/PDB symbols identify an unwrapped `D3D11CreateDevice`
  call; its native implementation's internally handled exception was intercepted before an HRESULT returned.
  The [source-bound triage](../../../Measurements/NativeDeviceBoundary_2026-11-07/README.md) retains that
  distinction and a deterministic hardware/WARP/debug-layer counterfactual. Its
  [completed local scope](../Done/NativeDeviceExceptionBoundary_2026-11-07.md) at `c4272b0d` passes six
  zero-warning builds, 66 ordinary x64 suite executions and actual ASan with original-member verification.
  At exact main `054766f6`, Grid's named origins pass in all three ARM64 profiles.
  Full Debug [42322543703](https://github.com/RedSalamanders/DxUi/actions/runs/42322543703)
  still rejects a console-covered Menu cursor fixture; ASan
  [42322548004](https://github.com/RedSalamanders/DxUi/actions/runs/42322548004)
  passes all requested native suites but rejects two external-consumer build warnings;
  Release [42322551824](https://github.com/RedSalamanders/DxUi/actions/runs/42322551824)
  stops at three saved-context origin checks. Original wrappers and every canonical payload member
  are verified. The missing original consumer build log leaves its warning causes unassigned.
  [Focused verifier and retention evidence](../../../Measurements/NativeQualificationVerifier_2026-11-07/README.md)
  adds a bounded ARM64 frame-pointer fallback, unchanged strict rejection controls, actual diagnostic
  records and retained consumer sources/images/logs. Native x64 Foundation and warning-free x64/ARM64
  cross-builds pass; the new ARM64 fallback and complete test-before-production gate remain unaccepted
  until fresh hosted execution. This does not assign a cause to the older output-less Grid failure.
- [x] Record the owner's 7 November deferral of native ARM64 runtime, including opt-in
  MenuResources/MenuResourceScaling, for this delivery. Require all three ARM64 compilation checks and keep
  every historical runtime failure/refusal. Do not claim those cross-builds establish runtime acceptance.
- [ ] Complete candidate-specific x64 foreground coverage. Ordinary noninteractive suites do not close it.
- [ ] Qualify physical device removal/occlusion, real IME during reflow, assistive technology and touch on named
  hosts. Arrange focus-taking runs at the agreed time through the existing restoring desktop lease.
- [ ] Keep original failed/refused foreground runs and causes unassigned until independently demonstrated.

<a id="d-c1"></a>
### D-C1 — consumer adoption and supported pilots

- [ ] Verify RedPrism/RedXe adoption of the already published compatibility branches (PRIOR-02) in their own
  repositories, without resetting separately owned branches or changing pins implicitly.
- [ ] Qualify each chosen General/card/prompt/profile pilot and exact pin through the consumer's own harness;
  leave optional additions deferred if the pilot has no demonstrated value.
- [ ] Coordinate the
  [single RedTilde plan](https://github.com/RedSalamanders/RedTilde/blob/main/Specs/Plans/WIP/RemainingWork_2026-11-04.md), preserving its explicit API-4 pin
  `2a7e7c28`. Its repaired main qualification passes at `1c600d60` in
  [run 41853861400](https://github.com/RedSalamanders/RedTilde/actions/runs/41853861400): both native 75-case
  Release roles and production ZIPs pass. Later app candidates, physical input and R10 retain their own acceptance.

<a id="d-g1"></a>
## D-G1 — SDK notices, evidence and process

- [x] Complete DOCS-07: preserve full Unicode/WHATWG notices, fix SDK-local links, verify the exact license
  set/bytes before and after extraction, and include notice inputs in CI identity. Independent refusal tests,
  complete tooling and a real x64 production/extracted-consumer smoke pass in the
  [scoped closeout](../Done/SdkDataNotices_2026-11-05.md). Other governance and platform gates remain below.
- [ ] Apply forward evidence aliases/schema changes, then execute approved D9 history redaction as a separate
  coordinated operation with private originals, all-ref inventory, reviewed mirror, commit map, validation,
  consumer repins and worktree repair. This consolidation does not rewrite history or change repository access.
- [ ] Finish remaining DOCS/TOOL/HOST findings and separate normative contracts from evidence history in the
  large controls/rich-text guides. Record a disposition for legacy unchecked Done items without rewriting raw evidence.
- [ ] Follow the unchanged Changes/fold workflow; obsolete dated fragment counts are not instructions to squash
  notes or bypass the separate default-branch fold. Preserve pending owner decisions on broader administration.

<a id="d-decisions"></a>
## Decisions and deferred scope

Approved D0/D1/D2 remain the lifetime, cosmetic-preparation and per-element policies above. D9's coordinated
privacy operation is approved but unexecuted here; D10 publication is completed. Details and all prior answers remain
in [the archived decision register](../Done/ProductionReview_2026-11-02.md#wave-0--decisions-that-unblock-implementation).

Owner choices received on **5 November 2026** are requirements. D3 and D5 are implemented and qualified in
the current scoped closeout; D4's SDK policy is implemented in its scoped closeout:

- **D3 — natural source editing/navigation.** Do not contain edits inside the old active region. Allow the
  source-editing scope to follow natural caret/selection movement. This replaces the archived containment recommendation.
- **D4 — text-first paste.** Prefer available Unicode text over a bitmap by default; applications may opt into image-first.
- **D5 — network image destinations.** Treat both `//host/...` and `file://host/...` as network and apply remote-image permission.

The [current input contract](../../UI/UI_InputAndAccessibility.md#current-input-contract) owns D3/D5 and
the [implemented paste policy](../../UI/UI_InputAndAccessibility.md) owns D4. RedTilde's image-asset adoption
remains consumer-owned. D7's aligned/gathered/alignat display-style behavior is implemented in the current math closeout.

| Still open | Required owner choice |
| --- | --- |
| D6 | Platform-specific color-glyph/script fallback. |
| D8 | Public Debug declarations and supported API boundary. |
| D11 | Broader legacy Done-checkbox disposition. This cleanup handles transferred WIP items, not all old Done records. |
| D12 | Broader fragment administration; current fragment/fold policy remains authoritative. |
| D13 | Consumer adoption of the implemented API-5 callback/fatal host protocol; PR162's library contract and qualification are completed, while each product owns its migration. |

<a id="d-h1"></a>
**HOLD D-H1:** S3-S7 overlay sessions, variable-height lists, read-only Markdown help, motion and theme metadata
remain unimplemented proposals. Their B/C/D/E implementation stages, pilot choices, ownership questions and stage
acceptance remain in [the archived proposal](../Done/ConsumerUiImprovements_S3_S7_2026-10-11.md). Accept a stage
explicitly before work; they are not release blockers created by this cleanup. ARCH-06..20's optional structural
proposals likewise require a selected scope after the correctness work.

## Completion rules

Close a required row only with an actual source-bound implementation/regression, authoritative contract/capability
update and applicable native/resource evidence or explicit permitted owner disposition. Track unexpected failures,
source drift and blocking external state honestly; never convert a filtered/failed run to success. Use affected and
PrePush accounting without repeating identical obligations. This is now the execution plan for the user's
implementation request. Each selected slice includes code, meaningful regressions, domain documentation and
applicable native builds. Foreground input still requires agreement to the time; consumer migration, coordinated
history rewriting and release acceptance keep their separate explicit boundaries.
