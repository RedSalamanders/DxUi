# DxUi remaining correctness and qualification

Status: ACTIVE
Updated: 4 November 2026
Owner: DxUi maintainers; consumer policy and adoption remain with each application.

This is the **only active DxUi plan**. The [consolidation record](../Done/WipConsolidation_2026-11-04.md)
maps every unchecked item from the four former queues to a current owner or explicit disposition. Archived
reviews/proposals retain detail and source identities without becoming parallel execution plans. Domain
specifications and `capabilities.json` remain authoritative.

## Verified starting point

Reviewed main: `7257f3e31e575c633e8cbbbf3a74f3115f2eeb73` ([PR163](https://github.com/RedSalamanders/DxUi/pull/163)).
RedTilde independently consumes API 4 at `703a96c788a79c1e7b28097226a2ca0a735f11bc`; publishing or merging
the library never advances another consumer's lock.

| Completed scope | Evidence and limit |
| --- | --- |
| Styled Mermaid spans and ER/class shared attachment defects (FLOW-01/02), bounded sequence syntax expansion and readable default strokes | [Display closeout](../Done/MermaidDisplayCorrections_2026-11-03.md), exact implementation `703a96c7`. The broader per-element failure channel remains D-A1. |
| Label-aware corridors, table/code insets, accent task markers and optional completed-task strike-through | [Spacing/task closeout](../Done/DiagramSpacingAndTaskPresentation_2026-11-03.md), [packet](../../../Measurements/DiagramSpacing_2026-11-03/README.md). |
| Source-specific x64 Release library/consumer checks | 19 noninteractive SDK suites with zero capability skips; independent RedTilde 73-case checks and hidden captures. Other scope retains its own owner. |
| Latest main Release qualification | [Run 41715666957](https://github.com/RedSalamanders/DxUi/actions/runs/41715666957) passed native x64 and ARM64 regression, test-gate, production and completion jobs at `7257f3e3`. It does not supply opt-in ARM64 resource/focus fixtures, Debug/ASan or physical-device acceptance. |
| Pending-Update timing accounting | Instrumentation and source-specific six-profile evidence are archived in [FrameUpdateAccounting](../Done/FrameUpdateAccounting_2026-10-26.md). This is accurate accounting, not a CPU improvement or performance-policy acceptance. |
| Compatibility branch publication D10 | Recorded published branches/SHAs remain in the [review decisions](../Done/ProductionReview_2026-11-02.md#wave-0--decisions-that-unblock-implementation); consumer adoption is D-C1. |

[API 5 PR162](https://github.com/RedSalamanders/DxUi/pull/162) is **open and not merge-ready** at
`d0179db6751d8eb3d56019950a5920500c024594`. Its body records an earlier validation checkpoint and pending
extended work. Treat checked failures, host-owned fatal protocol 2 and SDK/PDB changes as that in-progress
candidate, not main's contract or an accepted consumer migration.

## Priority 0 — lifetime and failure boundaries

<a id="d-l1"></a>
### D-L1 — embedded control lifetime

- [ ] Implement the approved D0 deferral of embedded preparation wakes while input entry points are on the
  stack; deliver the wake once after target revalidation. Guard direct TextField setters/history/clear paths
  that can invalidate and then continue using a retired control (PRIOR-03).
- [ ] Retain retiring-target and reentrant-positive fixtures in window/embedded hosts, update hosting contracts
  and run appropriate native teardown/resource coverage. Main's TextField/Embedded files did not change in
  the Mermaid follow-up; this P0 is not closed by that work.

Owner: `src/Rendering/Embedded.cpp`, `src/Controls/DxUi.TextInput.cpp`, input dispatch and `docs/hosting.md`.

<a id="d-a1"></a>
### D-A1 — checked failures and per-element admission

- [ ] Reconcile PR162 against main and D2/D13: expected input/resource failures stay checked, local element
  refusal preserves editable source/diagnostics, and unexpected C++ failures follow the agreed app-owned fatal
  protocol. Resolve conflicting older exception-injection wording before changing the contract.
- [ ] Complete the candidate's pending build/runtime/source-pin consumer, SDK/PDB negative controls and
  source-matched performance/foreground obligations before review/merge. Keep native ARM64 deferral explicit.
- [ ] Check parser/preparer admission and per-element failure isolation across diagrams/math/images, including
  PRIOR-07's host-wide preparation abort and error visibility. Preserve actual causes and regression evidence.

This coordinates the active API work; it does not adopt API 5 in any application or duplicate that branch's queue.

## Priority 1 — editing, parsing and presentation correctness

<a id="d-e1"></a>
### D-E1 — source guides and editor behavior

- [ ] Implement D1: cosmetic code ink/line numbers/whitespace never withhold content, cancel a gesture or
  refuse input; do bounded viewport/change work and show the previous cosmetic state while preparing
  (EDIT-01/02/04, RENDER-01/14).
- [ ] Apply the selected source-region policy to Backspace/Delete/Tab and nested fences without editing
  outside the active interval (EDIT-07/11; D3 is still an owner decision).
- [ ] Resolve remaining editor/command correctness: IME underline staleness, disclosure/zoom geometry loss,
  auto-pairing within words, read-only chord handling, wrong task-command byte ranges and parent-marker
  movement (EDIT-03/06/09/10, CMD-01/02).

<a id="d-i1"></a>
### D-I1 — Unicode, clipboard and native input

- [ ] Preserve pending surrogate leads across `VK_PACKET` key transitions/modifiers in both editors and hosts
  (CTRL-01), with real-message replay and no weakened assertions.
- [ ] Reconcile the native IME capacity and quoted-file-paste fixes required by RedTilde DX-4/DX-3 against the
  exact candidate; keep source, selection, history, decline paths and callback ownership intact.
- [ ] Qualify remaining focus/TSF/menu closure and mask/IME findings from the
  [prior-review register](../Done/ProductionReview_2026-11-02/prior-closure.md). Preserve unexplained historical
  failures; do not infer their causes from later green results.

<a id="d-m1"></a>
### D-M1 — Markdown, math and diagram correctness

- [ ] Correct deferred autolink suffix loss, front-matter incremental root reuse and CR-only YAML handling
  (MD-01/02/04). Expected malformed input must stay bounded and editable rather than aborting a process.
- [ ] Resolve display-block interruption and pathological scan budgets (MD-03/05/06/07), with deterministic
  seeds, independent expected results and cancellation/step limits.
- [ ] Complete display-style math cells and outstanding diagram note/routing/budget issues (MATH-01,
  FLOW-03/05), preserving exact source, local editing, geometry, history, UIA and resource limits.

FLOW-01/02's published fixes are complete. Do not recreate them as new work; independent regressions and
any *different* remaining failure-channel problem belong to D-A1.

<a id="d-r1"></a>
### D-R1 — semantic resources, rendering and accessibility

- [ ] Correct quote/callout inner margins in shared geometry, including nested containers, lists, tables,
  RTL, narrow widths and source ACP hit testing; verify the consumer's themed-scrollbar adoption with an exact
  API-4 pin. Record the scoped closeout after native tests, documentation and hidden captures pass.
- [ ] Compute color-glyph capability once per preparation and preserve the intended script/emoji fallback
  (SEM-01/RENDER-05; D6 pending), with actual target tests.
- [ ] Resolve image-completion/animation re-preparation, placeholder direction, lost list markers and
  supplementary-plane word starts (SEM-02/03/04/05).
- [ ] Restore bounded chunk reuse with nondefault line height (RENDER-02); preserve clean/dirty/hidden work,
  per-frame resource rules and device-generation behavior.
- [ ] Name RadioButtons groups and raise semantic state events; qualify UIA-client navigation/events/detach
  (A11Y-01/02), rather than inferring accessibility from pixels.

The remaining before-production P2/P3 IDs in the
[review inventory](../Done/ProductionReview_2026-11-02.md#before-production-p2-set) stay owned by D-E1,
D-I1, D-M1, D-R1 or D-G1 according to their area. Their detailed triggers are retained in the appendices;
omitting an ID from this summary does not dispose of it.

<a id="d-t1"></a>
## D-T1 — meaningful regression and adversarial coverage

- [ ] Add counterfactual fail-before/pass-after coverage for the retained P1 findings and earlier fixes that
  lack it (PRIOR-08); replace self-referential crossing, CF_HTML and font-truncation oracles.
- [ ] Add deterministic pathological-input/fuzz/cancellation coverage for Markdown, diagrams and math;
  retain exact seeds, budgets, raw failures and source/artifact identities.
- [ ] Update relevant catalog/factory, usage, gallery and design-system entries for actual control changes.
  Product screenshots come from owned harness scenarios with invented content and build/DPI provenance.

<a id="d-p1"></a>
## D-P1 — performance policy and resource acceptance

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

- [ ] Reconcile current-candidate Debug/ASan, real ASan detection, resource/WARP and any required six-profile
  obligations with their exact evidence. The successful main Release run closes only its declared roles.
- [ ] Complete or retain the explicit owner deferral for native ARM64 opt-in MenuResources/MenuResourceScaling
  and candidate-specific foreground coverage. Native cross-builds and ordinary hosted suites do not close them.
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
  [single RedTilde plan](https://github.com/RedSalamanders/RedTilde/blob/main/Specs/Plans/WIP/RemainingWork_2026-11-04.md), preserving its API 4
  development pin and failed current main qualification until separately repaired. SDK success is not app acceptance.

<a id="d-g1"></a>
## D-G1 — SDK notices, evidence and process

- [ ] Reconcile Unicode/WHATWG notices, shipped license links and independent SDK package contents (DOCS-07)
  against the current candidate; current ZIP success alone does not close every older license finding.
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

| Still open | Required owner choice |
| --- | --- |
| D3 | Containment for every source-region edit. |
| D4 | Text versus bitmap clipboard priority and decline contract. |
| D5 | Network classification/policy for scheme-relative and file-host image URLs. |
| D6 | Platform-specific color-glyph/script fallback. |
| D7 | Display-style parity of aligned/gathered/equation cells. |
| D8 | Public Debug declarations and supported API boundary. |
| D11 | Broader legacy Done-checkbox disposition. This cleanup handles transferred WIP items, not all old Done records. |
| D12 | Broader fragment administration; current fragment/fold policy remains authoritative. |
| D13 | Consumer-callback allocation/fatal policy and shared implementation; reconcile PR162 before adoption. |

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
PrePush accounting without repeating identical obligations. No native build, foreground input, experimental collection,
consumer migration, history rewrite or publication is part of this documentation-only cleanup.
