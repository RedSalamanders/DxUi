# Production review and remediation of the recent merges

- **Status**: ACTIVE (8 October corrections archived; accepted 9 October implementation and qualification in progress)
- **Date**: 2026-10-05
- **Requested window**: 28 September through 8 October 2026, from first-parent base `6b344566` to `bea676a`:
  41 landings, pull requests #28 through #68. The retained scope inventory names all changed paths.
- **Historical review**: `9f5bc07` (main on 2026-09-25) to `49a9988`: 45 merged pull requests, #21 to #65. The
  historical findings and recommendations remain below and are superseded by the current dispositions.
- **Revalidated**: against `e5ebbb5` (#66, #67) on 2026-10-06; see [Revalidation](#revalidation-at-e5ebbb5).
- **Current remediation**: started 2026-10-08 on `codex/production-review-remediation-2026-10-08`, from clean
  `bea676a`. The requested ten-day window is 28 September through 8 October, with first-parent base `6b344566`.
  This includes #28-#68 and retains surrounding/pre-existing defects needed to make those paths safe.
- **Recommendations**: [all 31 answers](#recommended-answers) revised on 2026-10-09 by the lead reviewer;
  accepted by the user on 2026-10-09 with "go for all that". Implementation and qualification remain separate.
- **Owning contracts**: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
  [theme](../../UI/UI_ThemeAndTypography.md), [Win32 host](../../Rendering/Rendering_Win32Host.md),
  [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [performance](../../Core/Core_PerformanceAndResources.md),
  [validation](../../Testing/Testing_Validation.md) and [toolchain](../../Build/Build_ToolchainAndConsumption.md).
- **Evidence**: [reviewer findings and verifier verdicts](ProductionReview_2026-10-05/findings.md). The identifiers in
  parentheses below (for example `host-core-1`) are its entries, each with the failure scenario, a fix and a failing test.

## How the review was made

### Accepted implementation (9 October)

The user accepted Q1-Q31. Continue on the existing feature branch, preserve the immutable 8 October packet as the
pre-change baseline, and retain new evidence separately. The lead integrates and independently checks delegated
Grid/Tree, text-input and tooling work. Prior receipts qualify their exact archived source only.

The accepted implementation is integrated. The checkboxes below remain open until their associated qualification
obligations finish; the performance policy remains a proposed migration awaiting concrete calibration and review.

- [ ] Q1/Q2: ownership-safe child/tab extraction, compatible API guidance and measured dispatch retirement.
- [ ] Q3/Q4/Q5/Q15: coalesced lazy native publication, canonical attachment identity, shared unlocked action execution
  and independent target synchronization; preserve first-query freshness and every reentrancy boundary.
- [ ] Q8/Q9/Q10/Q11: Unicode menu mnemonics, described-row identity/fallback/sizing, complete RTL contracts and system
  high-contrast colors, with deterministic harness captures.
- [ ] Q12/Q13/Q14/Q19: independent Grid focus, shared anchors, UIA selection/event semantics, quoted TSV and accessible
  full-cell inspection; centralize shared tooltip bounds.
- [ ] Q17/Q18: committed-composition undo, masked-field text-service/disclosure policy and diagnosed bounded transport.
- [ ] Q20/Q21/Q23/Q24/Q27-Q31: required aggregate workflow, explicit skips/interactive obligations, six-profile
  PrePush accounting, consumer signature/version fixtures and instrumentation identity.
- [ ] Q22: versioned trusted benchmark acceptance, randomized independent blocks/A-A calibration, protected policy
  migration and separate median/peak errata; no weakened threshold or silent rebaseline.
- [ ] Q25/Q26: reconcile plans/capabilities, actual touch provenance, bounded feedback overlap and adoption trackers.
- [ ] Qualify the final source with relevant native x64 Debug/Release/ASan, all ARM64 builds, consumer fixture,
  WARP/device-loss/resource evidence, gallery, validators/format and an exact-source CI candidate.
- [ ] Apply the approved required CI rule only after its workflow and concrete candidate are verified.
- [ ] Finish separately recorded real desktop/IME/AT/touch/native-ARM64 and publication/adoption obligations using
  available environments. Agreement to desktop timing remains explicit; hardware absence is not a pass.

### Current iteration evidence (9 October)

- Debug23 compiled the focus-transfer and menu assertion repairs, but Tooling stopped at the desktop-refusal fixture's
  repository-wide output snapshot. The lead isolated the actual entry point, its imports and scope manifest in the
  existing temporary-fixture mechanism. Refusal now checks only that child's outputs, including unchanged sentinel
  bytes, lengths and timestamps; all original refusal and no-execution assertions remain. The focused redirected
  run passed all 18 InteractiveMode cases after adding the required manifest. Current-source native qualification
  remains pending, and the failed Debug23 validator run supplies no overall pass.
- Debug22, build `3370d463877645bd9ec31936497fe5e2`, passed all 19 noninteractive suites with no skips and all
  validators/tooling, including balanced reentrant attachment refusal and recovery. The authorized desktop lease
  then failed in plain-menu record accounting before provider acquisition and native TSF retention after focus
  transfer; foreground, keyboard focus and pointer were restored. Independent lead review confirmed the menu
  assertion required obsolete eager publication and native outer activation erroneously retired the newer session
  committed by synchronous WM_SETFOCUS on the same editor. The former now checks all records after requesting the
  provider; the latter guards cleanup by complete session identity and adds a held-foreground regression. These
  corrections require fresh qualification. An unrelated RedPrism analysis build was observed and left untouched;
  the concurrent unpaired timing receipt supplies no performance conclusion.
- Debug19 passed all validators/tooling and 18 of 19 native suites, but the process-exit WindowHost regression
  exposed an expired native observer preventing session cleanup. Deactivation now validates session identity
  independently of control liveness, and process-exit TSF detachment abandons its target before native callbacks.
  Debug20, build `55fbe4d263bd4978922e9ba2fd3b2e7f`, passed all 19 noninteractive suites with zero capability skips
  and all validators/tooling. The subsequent lead teardown audit found Attach could register a new HWND from a
  focus-loss callback before the original reservation was released. Attach now refuses while detach is active;
  a nonactivating regression verifies callback delivery, balanced registries and a successful later attachment.
  That final correction supersedes Debug20's compiled-source identity and requires a fresh build and runtime check.
- A fresh fetch still places `main` at `bea676a`; the remediation stays on its focused feature branch. Debug15/16
  stopped at compilation defects in the new TSF fixtures and declarations; the lead corrected them. Debug17 stopped
  before compilation at two renamed inherited-test mappings and the workflow contract digest. These failed attempts
  provide no native qualification. The digest was refreshed using the existing normalized workflow hash and its
  focused tooling checks passed. The final TSF callback audit also covers staged external edits, shared thread-manager
  focus ownership and same-control retired-store eligibility before the next frozen-source run.
- A consumer-source capacity audit identifies RedSalamander's 50-row history ceiling for described menus. The lead
  checked its construction and normalization and enlarged the library's synthetic scrolling fixture to that size,
  including keyboard reachability and command identity of the final row. Runtime menu/host/payload concurrency is
  still unmeasured; retain the diagnosed 128-slot registries. See the [source-only audit](../../../Measurements/Review-2026-10-09/ConsumerCapacity/README.md).
- The lead cancelled Debug14 during compilation after a further surrounding-code audit confirmed TSF document
  `Pop`/`Push` invoke registered thread-manager sinks. Reentrant replacement can retire the editor before activation
  resumes or replace the session before teardown completes. Staged activation, session identity checks and actual
  thread-manager event-sink regressions are being implemented before repeating qualification. The cancelled run
  supplies no successful build or suite evidence. Unrelated RedPrism processes were inspected and left alone.
- The TSF self-edit notification repair was independently reviewed twice. Standalone diagnostic stores cannot queue
  work for the host's different production store. Deferred notifications retain the store and sink, validate the
  connection and target between callbacks, reject retired session cookies, and preserve callback-side external edits.
  Allocation failure keeps pending work for a later real trigger without a self-reposting loop. Current-source native
  runtime qualification remains outstanding.
- Debug build `0ad3ccdf1d3d4595ad1905d9069870bc` passed all 19 noninteractive native suites with zero capability
  skips, all five validators and tooling. Earlier ASan build `a122d0578228472585a6ec08335e3921` also passed all 19
  noninteractive suites, tooling and the expected isolated sanitizer-detection probe. Subsequent desktop-driven
  corrections supersede their exact-source identities; neither receipt closes the interactive gate.
- The approved Debug desktop leases restored focus and pointer on failure. The first run stopped at focused-control
  removal and a new TSF fixture that bypassed native character synchronization. The corrected fixture now uses host
  `WM_CHAR` dispatch and its composition/undo/cancellation test passes. The next run reached deferred-focus counter
  assertions and TSF callback invalidation. Focus tests now observe the client event before counting the deferred
  announcement, and their recorder filters cached process identity before retaining names from the desktop.
  Independent primary-contract review found native ACP self-edit notifications violate Microsoft's TSF contract;
  that behavior and its old compatibility assertions were corrected without weakening the invalidation test.
- A/A orchestration now builds one physical tree and measures the same executable under both labels. Independent
  lead tooling tests passed all 20 paired-run cases; the complete tooling runner passed afterward. Explicit hosted A/A
  dispatch support is implemented and the lead's final benchmark-gate fixtures passed all 44 cases. These are
  orchestration checks, not measured calibration or policy approval.
- Benchmark policy migration has a deliberate bootstrap obligation: `main` has no qualified versioned policy,
  and a seed candidate cannot authorize itself. Keep the aggregate outside required branch protection until a
  dedicated seed policy receives named maintainer review of the frozen judge/hash, unchanged thresholds, full
  A/A and paired evidence, and both judges' decisions on identical reports. A seed verdict remains
  `policy-review-required`; after it lands, a normal candidate must pass from that trusted base before the approved
  required-status rule is enabled. No generic bypass or threshold reduction is authorized.
- The earlier private design-system artifact sign-in did not publish files. The repository's committed
  `Specs/DesignSystem` and `docs/gallery` now define publication; external upload is no longer a gate.
- The frozen native implementation passed complete x64 Debug and Release runs: all 19 suites, zero capability skips,
  at builds `dded8e2f5b5e484499dd8bf42106d6f9` and `777e66f77a334952a27905bfc70934b6`. Debug ran all five validators
  and tooling; Release reused their exact matching receipt. `format.ps1 -Check` passed. Current ASan, ARM64 builds,
  approved desktop runs, exact-pin consumer and matched measurements remain open.
- Independent lead/agent review found A/A calibration separately built two worktrees although its judge requires
  identical executable hashes. The lead confirmed the actual Release PE embeds its absolute PDB path. Calibration
  orchestration was corrected to build one physical tree and measure that binary under both randomized labels,
  preserving independent receipts, exact identity checks and all existing policy/threshold obligations. This tooling
  correction does not change the native implementation; final source accounting must include the new orchestration.
- Debug build `803749dfebb848458714d81f3cfa6efc` passed all validators/tooling and 18/19 native suites with zero skips,
  including the new same-event geometry/input and native focus regressions. Control stopped at the PageHost fixture's
  old expectation that a changed hit query returns its container. The lead checked the host geometry contract and
  changed the fixture to require rejection, followed by a fresh hit on the replacement page. No full receipt is
  claimed for that iteration. Earlier Debug compilation attempts exposed a protected helper access and two missing
  stable-row lookup overrides in new fixtures; the current build includes their corrections.
- Refreshed `origin/main`: it remains `bea676a`; the working changes are isolated on the remediation branch.
- ASan build `f52dd536e0eb47be82feab28975888a2` and all six affected suites passed with no skips. The completed 10k-row
  rebuild measured 37,252 us, below its 250 ms budget. This source was superseded by the final geometry/input fixes.
- The subsequent full ASan iteration at build `39e807e97031437b9514f2156248eaa7` passed 18/19 native suites, including
  Rendering/device-loss and Accessibility, plus all validators/tooling. Embedded stopped at the lead's new fixture
  asserting that ordinary Grid Up must report handled. Grid selection completes on Down; the corrected assertion
  verifies capture release. This failed run does not provide a full receipt or qualify later source changes.
- The final bounded query audit confirmed flow/density, Tree/Grid metrics and density-changing themes left prepared
  hit geometry eligible for input. Those setters now reuse the weak-owner geometry scope guard, removing duplicate
  lifetime/invalidation tails. The shared query guard also captures host identity and geometry epoch. Native virtual
  hits and hover tails stop after geometry changes; embedded input rechecks pruning, hover and hit boundaries before
  acting. New two-column checkbox and synthetic hover/hit regressions require zero old-pixel actions followed by one
  correct retried action. Captured embedded events dispatch once directly, avoiding a synthetic duplicate Move before
  the real draft event. Current-source full profiles remain pending.
- ASan build `dbf448f079d94b1fa7bcaec5139f341f` passed; Embedded, Control and WindowHost passed with no capability
  skips, including the new PageHost recovery and StackPanel layout cases. Accessibility passed its completed 10k-row
  rebuild budget, then stopped in an old single-select fixture that used AddToSelection to replace the parent. The
  lead verified the Q14 contract and independent rejection fixture, retained rejection/state assertions and changed
  the replacement action to Select. This partial run is not a complete Accessibility receipt.
- The lead independently confirmed two further frame-coherence holes: Tree/Grid delegate setters advanced only local
  query revisions, and native child attach/detach/visibility/enabled mutations did not advance the host geometry
  revision. Both now advance host geometry and attachment changes invalidate both hosts. New WARP delegate-query
  and native unchanged-root clearing/extraction/reparenting/visibility/enabled capture regressions require partial
  rejection and explicit preparation recovery. The fresh affected ASan run remains pending.
- The isolated InteractiveMode tooling fixture passed all 18 cases after other log writers stopped. The lead also
  independently ran the final proposed judge trust/WARP provenance fixtures: PerformancePolicy 5/5, PairedRun 19/19,
  BenchmarkGate 44/44 and PerformanceComparison 32/32. The base policy still lacks qualified calibration; these tests
  prove trust enforcement behavior, not performance acceptance or production qualification.
- The lead reproduced and repaired a null-model dereference in Grid paint metrics cleanup and an existing Tree
  expansion action that rejected a successful delegate `NotifyDataChanged`. Query epochs now protect borrowed reads;
  binding epochs separately determine whether an action can complete after publishing its edit. The Grid double-click
  tests then exposed lost inspection after a same-model reorder; dispatch now resolves fresh stable identities after
  that callback. These corrections remain subject to the running affected ASan suites and final profile matrix.
- A further read-only parent review confirmed that PageHost replacement during render can leave zero incoming bounds
  until resize, and StackPanel layout retains a live child span across virtual bounds callbacks. Add actual geometry
  recovery and retirement assertions, then repair the shared preparation/traversal paths before the final source freeze.
- ASan build `75b8982d81184826bd347fe5d7167918` passed and the complete affected run passed Embedded (3,959 checks,
  1,000 warm composites with zero C++ allocations), Grid, Tree and Rendering. Control stopped in a hover fixture that
  retired the root through its overlay query before the intended ordinary query; WindowHost stopped because its new
  unattached-root input fixture set bounds before SetRoot reset them. Accessibility stopped in an old timing fixture
  that measured the lazy dirty request without querying the provider. The lead corrected those fixtures, retaining
  the 250 ms completed-rebuild budget. No complete current-source gate passed in that run.
- PageHost now defers paint-time incoming bounds through private pending preparation state, with no public virtual
  hook or task queue. Native capture and embedded Prepare discard the partial replacement frame, then synchronize
  page bounds in Update on the next explicit preparation. New native and WARP assertions require recovery without
  resizing or advancing animation. Gallery regeneration remains a final-source obligation for the broader visual work;
  these internal lifetime/layout fixes introduce no new gallery appearance.
- The lead reviewed the delegated StackPanel repair, rejected an unnecessary snapshot allocation in noexcept layout
  callers, and required child-lifetime checks and corrected gap/RTL fixtures. The final implementation reuses the
  existing guarded parent traversal and a private layout revision; tests cover child clearing, root replacement,
  changed gap and nested RTL horizontal reflow. Fresh executable validation remains pending.
- A second validator run passed all five validators but its tooling run reported PerformanceComparison and
  InteractiveMode failures while tooling edits/tests were concurrent. The subsequent isolated lead comparison run
  passed all 32 cases. Recheck InteractiveMode alone and rerun the complete tooling/validation gate on frozen source;
  no concurrent or partially passed run supplies final validation evidence.
- Retained additional failing diagnostics under
  [ModelGetterReentrancy](../../../Measurements/Review-2026-10-09/ModelGetterReentrancy/README.md), with intermediate
  build IDs and explicit limits on missing old source/artifact identity. They cannot authorize result reuse.
- Extended the getter audit through parent and host call chains: Panel, PageHost and ScrollPanel traversals now
  revalidate storage/ownership after virtual callbacks, native hit testing refuses retired targets, and native and
  embedded painting reject incomplete root-replacement frames. Added actual Direct2D retirement/recovery cases,
  standard-exception input/paint cases and selection collection failures. The lead corrected agent fixture bounds,
  a duplicate Grid local and exact-size vector growth that would lose amortized insertion performance. Fresh
  ASan compilation and runtime qualification are in progress; these changes have not yet earned a green receipt.
- Independently ran the updated benchmark gate fixtures: all 44 passed, including fractional band, raw/Holm p-value
  and separate raw-maximum/median/peak-budget projection. Calibration, paired performance and trusted policy seeding
  remain open; fixture success does not qualify those acceptance gates.
- Independently reran `validate.ps1` after the tooling-agent repairs: all five validators and the full tooling test
  runner passed. This predates the subsequent benchmark-policy and interactive-routing edits and must be repeated.
- The first Debug build exposed Grid reading-direction conversion, menu chevron and protected text-import access
  errors. The library compiled after correction; the next build exposed new Grid fixture declarations/point/pointer
  access errors and missing default focus-delegate definitions. These failures are not successful build evidence.
- Independent follow-up found that UIA cell focus lost the active column, idempotent Tree Add changed focus, inspection
  dropped capture cancellation and allowed the remainder of a dismissed gesture through. Corrections and regression
  coverage are being integrated. RTL reveal, overlay shaping and final-glyph reachability are included.
- Added a nonactivating native keystroke/selection mutation fixture to retain a scheduling baseline before the lazy
  publication change. A before/after number alone cannot close the paired performance gate.
- Menu descriptions, popup-size and focus-announcement fixtures now belong to the explicitly interactive Menu runner.
  The former NewControls skip allowances and routing descriptions are being reconciled; no desktop result is implied.
- Current command logs are under `.build/logs/production-review-2026-10-09-*`; generated reports remain under
  `.build/reports/`. The immutable 8 October packet is unchanged.
- The user authorized the restoring desktop lease for Menu and NativeTextInput on 9 October, after noninteractive
  checks pass, with up to fifteen minutes reserved. This authorization is not a test result.
- The lead found and corrected two additional interleavings while checking delegated work: a snapshot-derived peer
  must retain the identity it inspected even when a replacement occupies the same path; a delayed visual-line range
  operation must reject a conflicting concurrent endpoint change instead of overwriting it. Native ranges read the
  current document while retaining control identity, rather than pinning a complete historical window snapshot.
- Focus loss now publishes its transition before gain. This lets focus restoration replace an earlier pending host
  announcement even when loss and gain occur before the queued event drain.
- Slider touch feedback is clipped to its declared bounds and active ancestor clips. Ordinary thumb, tick and focus
  painting is unchanged. A deterministic harness test covers neighboring pixels/hit testing, overlays and scrolling.
- The final audit confirmed and repaired missing focused-row callbacks in mouse/keyboard group collapse, selection
  insertion order leaking into clipboard row order, and stale row/column ordinals after double-click selection
  callbacks. The shared selection/focus notifier guards retirement; inspection reads the current stable cell.
- A complete Accessibility iteration exposed text events raised through a second peer for a collapsed TextField
  root. Generic diff events now use the canonical element factory with the inspected control identity. The existing
  real-client subscribed-window regression must pass in the final profiles before this correction is qualified.
- The first final native run passed Foundation but stopped in the new skip parser on its empty skip list. Empty and
  multiple-entry regression cases now pass; this tooling failure is not a successful native-suite receipt.
- Native text-input audit found that policy transitions could retain an IMM preview, edit-message routing could
  read a replacement pointer before rejecting an ineligible editor, and a new completed TSF composition could reuse
  the previous one's undo base within one lock. Corrections and regression cases are integrated for current-source
  validation. Hide/zero-size cancellation and late-message rejection preserve actual HWND focus. Cancellation tracks
  the preview separately from the synchronized host cache so newer application text cannot be mistaken for a preview.
- A second audit caught stale TSF staging surviving an external edit into the next composition within the same lock.
  Stale completion and rejected writes now discard their staging and edit base; the following composition starts
  from current application text and receives its own undo unit. The registered regression exercises both paths.
- Transport intake now requires the token's exact HWND, message and type. A compatible HRESULT Invoke callback
  reports menu queue exhaustion and permits retry after capacity is freed; UIA target retirement takes precedence
  over a callback result. Attachment is exclusive/idempotent, rejects incomplete accessibility registration with
  balanced rollback, and uses a fixed bounded process-exit snapshot. The lead reviewed every new agent test and
  corrected adoption of four owned COM references in attachment fixtures.
- The first final dispatch build failed on an incomplete SDK UIA header include; the next exposed a local-name
  shadow warning under warnings-as-errors. Both were corrected. Neither failed build supplies qualification.
- Final surrounding-code audit confirmed that children beneath a disabled visible Panel still advertised enabled
  UIA state and accepted actions/text-service locks. Effective state now flows through the linear publication walk;
  owner-side actions and TSF use the existing interaction helper. Disable/re-enable regressions are being qualified.
- The latest five validators and tooling runner passed. Native iteration then stopped after Foundation because a
  nested forced module import removed the caller's suite-reporting function. The import now preserves the caller;
  all five skip-policy regressions pass, including that actual caller/module sequence. This stopped iteration is
  not an all-suite result. Its automatic performance sample was unpaired and overlapped other compiler processes.
- The first complete current-source Debug iteration passed 14 of 19 native suites with no skips; Embedded, ReadOnly,
  Rendering, Accessibility and WindowHost failed. It exposed a real ignored `transferNativeFocus=false` editor path
  and missing embedded preview ownership marker. Both are repaired for a fresh candidate run. Hidden-cache fixtures
  now use visible nonactivating HWNDs; the password test holds the reveal peer before disabled-focus pruning. The RTL
  fixture replaces its invalid unmarked/right-aligned pixel anchor with explicit correct and opposite-direction
  marker references. The lead independently reviewed every repair; the failed iteration is not qualification.
- Follow-up ASan regressions proved Tree and Grid model getters could replace the root and leave snapshot construction
  reading freed owners. The lead retained both failing sanitizer traces and is integrating abortable navigation,
  hit-geometry and focused-lookup construction with guards in their nested queries. The initial agent fixtures also
  required correction for the noexcept model contract, COM QueryInterface and collapsed-root provider lifetime.
  Disabled Tree/Grid item actions now resolve their owning control before checking effective state. The focused ASan
  disabled-item and RTL marker regressions pass; the complete current-source profiles remain required.
- The later Release iteration failed Rendering and Accessibility; ASan additionally failed Embedded when the shared
  UIA client could not finish setup in twenty seconds. Its executable hash matched the run receipt and no sanitizer
  error appeared in that failure. The shared client now uses the existing bounded CUIAutomation8 request pattern;
  COM activation precedes those settings and remains covered by the test-process startup deadline. A rerun is required.

### Archived verification and implementation (8 October)

The archived reviewer verdicts below are claims to recheck, not executable qualification. The lead is reviewing
surrounding callers and independently inspecting each delegated patch. Three cheaper agents cover Grid, text input
and tooling; the lead owns host/Tree/menu/accessibility integration, documentation and final validation.
Clear corrections proceed under the developer's instruction; API/product-policy decisions stay in the questions
section. This changes the library alone, with no implicit consumer adoption.

- [x] Verify the clean checkout, exact merge window and existing plan; create the focused remediation branch.
- [x] Build the unchanged x64 Release candidate and retain three offscreen Default benchmark receipts under
  `.build/reports/ProductionReview-2026-10-08/`. The first run was much slower than the next two controls; keep it
  as evidence of environmental instability, and do not use it to claim an improvement.
- [x] Reproduce and repair confirmed lifetime faults, including surrounding callbacks and publication callers.
- [x] Repair clear keyboard and scoped-validation defects with meaningful regression cases.
- [x] Classify current-source corrections, refuted claims and remaining architecture/policy questions; archived verdicts are not runtime qualification.
- [x] Run affected explanations, targeted regressions and 19 noninteractive native suites in each x64 Debug/Release/ASan profile; preserve 12 NewControls capability skips per profile.
- [x] Build ARM64 Debug/Release/ASan; retain native ARM64 and real focus/IME/assistive-technology gates separately.
- [x] Compare the same retained benchmark fixture, review affected docs/gallery, run all validators and formatting.

Focus-taking suites require agreement to the time under `AGENTS.md` and
[build-dxui](../../../.agents/skills/build-dxui/SKILL.md): "only when the person at the desktop has agreed to the time".
The archived 8 October run had no desktop authorization. The 9 October authorization and subsequent results belong
to the current iteration ledger; no noninteractive result closes that gate.

#### Current dispositions

This table records the qualified 8 October source and supersedes the earlier review verdicts. The appendix retains the original reports,
including hypotheses and duplicate identifiers; neither its old "confirmed" labels nor an agent's report establish
current runtime correctness. Source fixes below were built and locally validated as the archived candidate; new
9 October changes require their own exact-source evidence.

| Area | Current-source correction | Evidence and remaining boundary |
|---|---|---|
| Root/focus lifetime | Snapshot the incoming root's prior inheritance before reset callbacks can retire its parent; share the normal guarded reparent path; retain the current owner through notifications; capture requested focus lifetime before pruning. | Original promotion and nested reset promotion fail under ASan; the latter covers both root replacement and host detach. Requested-focus, callback-destruction and inheritance regressions added. A pointer callback test already passed before the fix and is supplemental evidence only. |
| Grid | Keep the moving range endpoint separately from ordered membership; guard checkbox/group callbacks and layout publications; resolve group IDs after mutation and stop when delegate/model retires. | Tests cover upward ranges, model/delegate mutation, nested selection destruction, disabled requests and focused endpoint metadata. The larger independent-focus/navigation design remains Q12. |
| Tree | Offer only handled keys to first-row navigation; re-hit-test after focus callbacks; reject drops into any selected subtree; stop destroyed/disabled requests. | Original unhandled-key test fails and pointer model-reset test crashes before correction. Unparsed edit cancellation and selection-policy questions stay separate. |
| Base/containers | Guard mutation callbacks inside Control, Panel, PageHost and TabControl; resolve page identity after callbacks; bound traversal and retain entry-owned children during clear; share guarded overlay dismissal with ScrollPanel. | Direct destruction, sibling removal, promotion, close/focus and third-page transition regressions added. Panel and ScrollPanel overlay retirement both reproduce ASan use-after-free. `AddChild`/`AddTab` return null when their owner retires. This is a local repair, not A5's deferred-destruction architecture. |
| Editors/native input | Propagate text-callback survival; snapshot callbacks/text arguments; revalidate after native activation/IME imports; leave caption messages with the HWND; retain reversed EM_SETSEL orientation and CRLF offsets. | TextField/ComboBox/caption tests are nonactivating; native IME/focus timing and composition acceptance remain Q17 and an interactive gate. |
| Callback storage/cleanup | Share registered callable storage across dispatch; preserve mutable captured state and the old subscription on allocation failure; release dispatch snapshots before survival checks. Native menu refresh follows the same rule. | 25 mutable-state cases fail with per-dispatch copies; ScrollPanel already passes. Seven cleanup cases reproduce ASan use-after-free in TextField, ComboBox, tabs, MenuBar and Tab traversal; blur is supplemental. A native-menu mutable-state case independently fails. The final candidate passes these plus the original 22 callback lifetime cases. |
| TSF service | Consume a pending lock once while another lock is active; retain flags and post once on release; retry failed posts on the next real request. | Nested-loop and failed-post regressions; no timer or immediate repost loop. Real TIP/IME acceptance remains separate. |
| UIA actions/transport | Resolve under the mutex, act outside it, revalidate target lifetime before host access; make AddToSelection idempotent and disabled requests fail; provider creation uses bounded token transport. | Original worker snapshot query blocks inside a Toggle callback. Added destruction, worker-query, bounded owner dispatch and bogus-token tests. No out-of-process UIA reentrancy claim is made. |
| UIA state/events | Only the focused Grid endpoint reports focus; native range values, limits and steps republish/announce; Tree items and Grid headers/rows/cells derive offscreen state from published clipped geometry; embedded focus events name the focused item/row; terminal TextRange moves return zero. | Real subscribed clients in nonactivating fixtures plus provider-query, partial/full ancestor-clipping and terminal-movement regressions. Native foreground announcements remain interactive; selection policy is Q14. |
| Rendering failure | Guard every direct solid-brush use in Grid, scrollbar, Tree focus and ComboBox split/popup painting. | Four separate ASan regressions crash inside Direct2D with forced-null brushes before correction; faulted capture and recovery are checked. The archived Button claim is refuted: its shared chrome helper already guards the brush. |
| Menus | Drain async sessions before process-exit sweep and recheck registered hosts; dispatch unrelated timers; handle sent modal cancellation; apply pixel-rounding slack consistently; reconcile pointer/focus/scroll geometry. | MenuExitLifetime is nonactivating. Capturing modal, physical pointer and monitor-specific fractional-DPI fixtures remain capability/interactive gates. |
| Contrast | Reuse linear WCAG contrast and compositing; retain readable configured inactive text and exact HC HighlightText; measure Tree focus against its actual resolved fill. | Palette/control tests cover light, dark, alpha, rainbow, Aquatic-like and Desert-like pairs; all six harness gallery images are regenerated and visually reviewed. |
| Scoped evidence | Decode Git as UTF-8; include real git/gh identity; attest the built source rather than reread concurrent state; reject skip-bearing receipts for reuse; label empty/partial/pending work truthfully; fix scope fan-outs. | Tooling regressions cover source staging equivalence, non-MD docs, tool/environment identity, missing obligations and composed fixtures. Pending CI remains pending. |

#### Claims corrected or still unresolved

- Tree Ctrl-click currently collects IDs, not every `TreeItemData`; that archived allocation claim is refuted. Expansion
  animation still has linear lookups per animated row and deserves a separate measured optimization.
- Same-value NumericStepper `SetValue` is an authoritative model update under its contract. Ending the typed edit is
  deliberate; Escape while the buffer is unparseable has no agreed revert timing and remains a product question.
- Original EM_SETSEL start -1 mapped to the text end, not zero. Reversed-range loss was real and has been repaired.
- NumericStepper does not expose RangeValue; the claimed stale step-size RangeValue property is refuted. Slider does
  expose it, and its silent limit/step setters required publication and preparation requests.
- The Embedded regression initially treated a zero-length anchor as selected text. The assertion was corrected after
  tracing ordinary MouseDown/MouseUp; no production edit was needed for that assertion failure.
- The selection diff checks the >20 threshold before the exactly-one replacement case. This is still a source-backed
  discrepancy with the stated WPF comparison; retain Q14's explicit priority decision.
- Native TSF composition/history/geometry, masked-field input scope, lazy snapshot publication, single-select/UIA
  focus policy, collapsed-group/horizontal Grid navigation, tooltip overflow inspection, RTL and capacity/error behavior
  remain in the owning questions and architecture proposals. They are not passed or silently deferred release gates.
- Interaction-lease Ctrl+C restoration is plausible from pipeline ownership, not demonstrated by a desktop interruption
  here. Preserve it as an unqualified scenario rather than a confirmed reproduction.
- Final read-only audit found that a nested tab resize could be overwritten by the outer layout's cached content
  rectangle. A regression now exercises that sequence. Raw rearrangement of a Panel's mutable child slots can still
  skip a propagation callback, and extracting a tab page through the span can desynchronize its tab metadata; the
  documented extraction leaves an empty slot, while arbitrary rearrangement and TabControl extraction have no
  established contract. These source-backed boundary cases stay in Q2/A5, not a claimed passing scenario.

#### Historical decisions and release gates (8 October)

This is the archived 8 October decision inventory. The user accepted the revised answers on 9 October;
the current implementation and qualification checklist above now owns outstanding work.

- **Ownership and scheduling (Q2, Q3, Q4; A1/A5):** define whether arbitrary rearrangement through the mutable
  child span and extraction of tab pages are supported; choose an ownership-safe transfer API and whether
  destruction/publication is deferred to the message boundary. The repaired documented promotion path does not
  qualify those broader operations. Lazy UIA publication also needs an explicit first-client/event timing contract.
- **Grid and UIA policy (Q12-Q15; A3):** choose independent keyboard focus, Ctrl/range anchors, single-select
  Add/Remove semantics, the exactly-one event priority over the 20-item bound, and root-provider identity when
  the semantic tree changes. Current large-selection event behavior is now disclosed in the usage docs.
- **Native input and menu policy (Q8/Q9/Q17):** settle layout-sensitive/duplicate mnemonics, described-row failure
  fallback and spoken-description placement, IME undo, masked-field TIP/input scope and edit-message ownership.
- **UX and limits (Q10/Q11/Q18/Q19/Q26):** define RTL coverage, application-color overrides in high contrast,
  payload/row capacity limits, full clipped-cell access for keyboard/touch and real-device slider feedback.
- **Validation and release process (Q20/Q21/Q23-Q25/Q27-Q31):** choose the required aggregate CI check and
  cancellation/skip policy, consumer-interface PowerShell/API migration requirements and the obligations of an
  empty affected run. Existing source fixes disclose missing work; they do not enable branch protection or
  substitute for consumer adoption.

Release qualification still needs agreed real-focus Menu/NativeTextInput and menu resource runs, the 12 omitted
desktop/DPI scenarios, native ARM64 runtime, real IME/assistive-technology/touch acceptance, final committed
design-system/gallery updates and separately owned consumer pin handoffs. The proposal to use a lock per accessibility
target and larger selection/publication deduplications remain measured architecture work, not hidden scope in
these fixes. No consumer project or pin is changed by this candidate.

#### Current evidence and qualification ledger

- The retained pre-implementation Default Release benchmark and two controls identify `bea676a`, WARP, 1280×720,
  96 DPI, 83 controls, 1,000 model rows, 40 frames and five rounds. Environmental drift prevents a single-run improvement claim.
- Before host/Tree fixes, targeted ASan logs in `.build/reports/ProductionReview-2026-10-08/red-*.log` record promotion
  use-after-free, the Tree model-reset crash, unhandled-key selection and a blocked worker query. These builds included
  other in-progress patches; they are targeted negative evidence, not clean-baseline qualification for the whole tree.
- The final follow-up red logs reproduce a freed-parent read during focus-reset promotion, false Tree onscreen state
  for an entirely clipped row, and stale Slider limits/clamped values. These defects were found after an earlier
  three-profile pass; that pass does not qualify the final repaired source.
- Later red logs independently reproduce Panel/ScrollPanel overlay owner retirement, four null-brush paint crashes,
  and false onscreen metadata for Grid fragments fully clipped by an ancestor ScrollPanel. Earlier passing matrices
  are superseded by the source containing these corrections.
- The final callback audit reproduces 22 additional ASan failures before its repairs: self-replacement/destruction
  frees the executing callable in Control, TextField, ComboBox, PageIndicator, ScrollPanel, TagPicker, MenuBar and
  host Tab/Escape callbacks. TagPicker also borrows a selection span that retirement invalidates; tab-boundary
  replacement leaves a stale next-focus target. Local callable/argument snapshots and lifetime checks repair
  these paths. The delegated tests were independently strengthened to read captures after replacement, eliminate
  outside ownership, leave a nonempty TagPicker removal argument and exercise the TextField Backspace tail.
  `callback-red-cases.json` maps every failing case to its log; the final green matrix must include all of them.
- Independent review then caught mutable-state loss from copying callbacks per dispatch. Retained shared callable
  storage repairs all registered control and host callbacks and native-menu refresh, with one allocation-failure
  helper. Capture destruction is itself a reentrancy boundary: seven cleanup tests reproduce ASan failures after
  premature survival checks. These repairs supersede all previous complete profile results.
- The final two callback regressions independently demonstrate accessible-invoke replacement retiring its
  control inside old capture cleanup (ASan use-after-free), and ColorSwatch requesting another repaint after its
  keyboard callback retires the root. The latter does not reproduce an ASan read because `Control::Invalidate`
  currently delegates without reading members; the strengthened test measures the erroneous post-retirement repaint.
- Provider creation's pointer-to-token transport changes its registered message to protocol version 2, as required
  by the shared message contract. The existing exact-name regression fails when the old version is retained.
  This final integration correction supersedes the preceding ASan pass and Release run; the final matrix is rerun.
- Harness gallery generation independently reproduced an expired borrowed Tree model during Slider capture
  cancellation at host teardown. The GalleryScene is now declared before its attached window, and owns models
  before its root, so teardown respects the borrowing contract. This is a fixture lifetime correction; the library
  and benchmark executable remain byte-identical. Native test artifacts are rebuilt and revalidated after this fixture edit.
- Final simplification preserves the protected `Control::Reparent(Panel*, ControlHost*)` extension signature,
  uses the private generic helper for PageHost, removes redundant Grid offscreen flags, removes three scope rules
  for nonexistent files and includes native Grid/InteractiveLease consumers of the shared tooling fixture helper.
- The first complete ASan iteration passed 18 of 19 noninteractive native suites; Embedded failed the invalid new
  assertion described above. NewControls recorded eleven explicit capability skips. No passing full receipt was claimed.
- The final formatted source, including gallery fixture ordering, has compiled identity `e2ab708c48e9de9e55c9a1bfd3601e3899a3909d09e9e95a8807f7ac59018ac0`
  and library fingerprint `FAEF6725B5A0B5DF2C2C39A107F9FE012A5A9F8B13D6B9ABFAD2F154CE558D2E`.
  All 19 selected native suites pass in x64 Debug, Release and ASan Debug; NewControls records 12 capability skips
  per profile and is deliberately not reusable. The final accessibility scope runs once per profile and is reused
  only with the exact unchanged source/artifact/profile/environment identity. ARM64 Debug, Release and ASan Debug all build, with no native
  execution claim. Earlier source-changing/format-overlap runs published no attestation and are superseded.
  The [retained packet](../../../Measurements/Review-2026-10-08/README.md) contains 57 raw suite logs/receipts,
  profile build/run logs, exact candidate source bytes, a reviewable patch and source fingerprints.
- The paired Default, MultilineGrid and MultilineGridDistinct comparisons use the same untouched harness,
  x64 Release WARP fixture and three ABBA repetitions (six runs per side). All three sets are within the investigation
  bands, with no metric flagged as regressed or improved. The canonical gate passes as no change established;
  it does not establish that no regression exists. Respectively 16, 10 and 14 of 26 metrics held inside their bands
  in every same-binary control; smaller shifts in the others could not be resolved against runner drift.
  Library and benchmark executable hashes still match after the gallery-only fixture edit. All raw paired reports,
  summaries, control comparisons and hashes are retained in the packet.
- All six Release harness gallery images are regenerated and visually reviewed; the five-theme gallery passes
  under ASan after the borrowing-order correction. Source, executable and image hashes are retained. Formatting,
  `git diff --check` and the five validators plus tooling tests pass. Final PrePush accounting runs after this
  documentation/evidence closeout; its log and exact reusable receipts are under `.build/reports/` and disclose
  the still-unqualified capability/desktop/CI gates. Compilation failures during integration are retained and
  are not test passes.
- Real Menu/NativeTextInput focus, MenuResources/MenuResourceScaling, actual IME/AT/touch, native ARM64 runtime,
  consumer pin handoffs and final committed design-system/gallery updates remain explicitly unqualified. Local
  source/previews and harness gallery are the required publication surface.
- Live GitHub APIs on 8 October show active main rules for pull requests, deletion and non-fast-forward prevention,
  no required-status-check rule, and HTTP 404 from legacy branch protection. That confirms the current Q21 gate gap;
  an enabled candidate workflow or a pending check is not required merge coverage.

Q1's documented root promotion, Q5's unlocked application actions, Q6's live async-menu exit sweep, Q7's modal
cancellation and Q16's embedded item focus have concrete local corrections above. Their historical question text
is retained for context; implementation does not close the corresponding interactive/platform/resource acceptance.

The historical sections from "Verdict" onward describe the merged source before this remediation. Their unchecked
items are a review backlog; use the current table and qualification ledger to assess what this candidate actually fixed.

The original 5 October review was static only: nothing was built or run then. Nineteen reviewers each took one area: menu loop and lifetime, menu layout,
UIA lifetime and threading, UIA selection semantics, Grid selection, Grid multiline text, Tree, core and editor controls,
text input, the host core, the public API, cross-cutting simplification, test support, two test-quality slices, two
tooling slices, CI and process. Each production area then had a second reviewer looking only for what the first one
missed: side effects in surrounding code, interactions between pull requests, and error and destruction paths. A
completeness critic named six uncovered risk classes (UIA test falsifiability, the accessibility mutex with nested loops,
high-contrast/RTL/DPI visuals, embedded-host parity, the performance evidence pipeline, ARM64/ASan runtime), and each was
reviewed in turn.

Every finding was checked adversarially at `49a9988`. High and critical findings got two verifiers, one tracing the path
from a reachable entry point and one trying to refute it. Medium findings got one verifier, and low findings and
simplifications got a batch verifier per area. Of 274 reported findings, 7 were refuted, 223 confirmed and 44 judged
plausible (real code path, but depending on an unestablished condition). After merging duplicates, about 150 distinct
items remain. The lead reviewer re-read the code for every item marked **[lead]**.

The previous review ([ReviewFixes](../Done/ReviewFixes_2026-09-29.md)) covered 14 to 28 September. Items it deliberately
declined were not reported again. Items marked *pre-existing* predate the window but sit in code the window changed or
depends on.

## Revalidation at e5ebbb5

On 2026-10-06 main moved to `e5ebbb5` with #66 and #67 (scoped testing). Neither touches `src`, `include` or `Samples`.

- **Production findings**: every one still holds, at the same line.
- **Test-file findings**: #66 renamed the test files to the `Scope.Tests.Something` scheme. Only include lines changed,
  one for one, so their lines are unchanged too.
- **Tooling and CI findings**: of those in files #66 modified, nine still hold at moved lines and three still hold with
  changed context (noted in their items). Two are fixed by #66: a plain `test.ps1` no longer runs the focus-taking
  suites.
- **Appendix**: shows every finding at its `e5ebbb5` path and line, with an "At e5ebbb5" status.

#66 and #67 also add about 650 lines of new tooling, so they were reviewed the same way: two reviewers (the scoped-testing
engine; its integration with `test.ps1`, CI and the paired runs), a second look each, and adversarial verification. Of 33
findings, 29 were confirmed, 3 plausible and 1 refuted, none of high severity. They are in
[Scoped testing (#66, #67)](#scoped-testing-66-67) and in the appendix under `scoped-*`.

## Verdict

**Not production-ready as merged.** The main reasons:

- **Use-after-free reachable through documented API.** At least seven paths, from `ControlHost::SetRoot` (#31), the host
  pointer-down path, Grid checkbox/group delegates, TextField's clear button and the process-exit host sweep.
- **Per-call-site lifetime checks.** #57 and #60 put a lifetime check at each call site that may destroy the control. Many
  sites in ComboBox, TextField, TabControl, the Grid and the UIA actions are still unguarded. The structural fix is A1/A5 below.
- **Keyboard regressions.** A focused Tree with no current item selects row 0 on *any* key (Tab, Ctrl, Shift). A Grid
  cannot extend a Shift range upward.
- **Accessibility gaps in the new features.**
  - Range values are never announced in a window host.
  - Embedded hosts raise no focus events for tree items or grid rows.
  - UIA actions run application callbacks under a process-wide lock.
  - The Tree's focus ring is invisible on a selected row.
- **Test infrastructure.** Ctrl+C under `test.ps1 -Interactive` can kill the lease before it restores the desktop.

#66 and #67 change no library code. Their scoped-testing tooling has no high-severity defect, but a plain `test.ps1` can
now pass without running anything, and `-Mode PrePush` can report coverage that no required check enforces.

The design, documentation and evidence culture is strong. Most findings are gaps in otherwise careful work, and the
fixes are local.

## P0: memory safety, hangs and crashes

Each fix lands with an ASan test that fails at `49a9988`.

- [ ] **`ControlHost::SetRoot` frees the old root before `Reparent` reads the moved control's old parent** (#31) **[lead]**.
  `DxUi.WindowHost.cpp:1367` assigns `_root` (destroying the old tree), then `Reparent` walks `_parent` through
  `GetFlowDirection`/`GetDensity` (`DxUi.cpp:919`). This is reached by promoting a child of the current root, which the
  `Panel::GetChildren` comment (`DxUi.h:1671`) documents. Before #31, `SetParent(nullptr)` never read the parent. Fix:
  keep the old root alive until after `Reparent`, as `PageHost::SetPage` does, and see A5.
  (`public-api-architecture-1`, `host-core-1`, `uia-lifetime-threading-1`, `cross-cutting-simplification-1`)
- [ ] **Host pointer-down takes native focus, then reuses the clicked control unchecked** (#57 gap) **[lead]**.
  `DxUi.WindowHost.cpp:2731-2744`: `SetFocus(hwnd)` sends `WM_SETFOCUS`, and `OnSetFocus` may run `SetFocusControl` and
  the application's focus callback, which #57 made a supported place to rebuild the tree. `SetFocusControl(liveControl)`,
  `CaptureMouse` and `RememberPointerButtonDown` then dereference a freed control. `SetFocusControl` itself takes the
  requested control's lifetime only after `PruneStaleInteractionState`, which can run `OnFocusChanged(false)`.
  (`host-core-3`, `cross-cutting-simplification-3`)
- [ ] **Grid checkbox and group delegates** (#60 gap) **[lead]**.
  - `ToggleCheckboxCell` takes its lifetime token *after* `OnGridCheckboxToggled` (`DxUi.Grid.cpp:5210-5213`).
  - The group-header press (`:3610`), the keyboard group toggle (`:3990`) and the `ApplyGroupLayout` loop (`:1363`) keep
    using the grid after `OnGridGroupToggled`.
  - An application that calls `NotifyDataChanged` there (as docs/controls.md tells it to) runs the selection delegate,
    which may rebuild the view. (`grid-selection-lifetime-2`, `process-docs-specs-3`)
- [ ] **Async menu open at process exit** (#23, #41) **[lead]**. `ShutdownAllWindowHostsForProcessExit` detaches the root
  popup's host. `Detach` calls `ReleaseCapture`, whose `WM_CAPTURECHANGED` finalizes the async menu and destroys that same
  host mid-`Detach` (`DxUi.WindowHost.cpp:1288`, `DxUi.Menu.cpp:2271`). The sweep's snapshot also still holds the freed
  submenu hosts. Fix: close open menus before the sweep and re-check the registry for each target.
  (`menu-loop-lifetime-2`)
- [ ] **Callbacks that destroy their control: remaining unguarded sites.**
  - [ ] TextField clear button writes after `onTextChanged` destroyed the field: `SetTextAndNotify` drops
    `NotifyChanged()`'s result (`DxUi.TextInput.cpp:1752`, `:1056`) **[lead]** (`text-input-1`).
  - [ ] `ImportTextInputState(state, true)` always returns true, so the native IMM commit (result plus composition in
    one message) and `WM_PASTE`/`WM_UNDO` keep using a field `onTextChanged` destroyed
    (`DxUi.NativeTextInput.cpp:1395`, `DxUi.TextInput.cpp:2969`) (`text-input-2`).
  - [ ] ComboBox selection and text callbacks; `NotifyTextChanged` calls the stored `std::function` in place
    (`DxUi.ComboBox.cpp:2314`, `:2660`) (`controls-editor-theme-1`, `text-input-10`).
  - [ ] TabControl: a tab switch publishes inside its page-visibility loop. `RemoveTab`/`SelectTab` reuse an index that
    focus callbacks may shift (`DxUi.Controls.cpp:7214`, `:6603`) (`public-api-architecture-5`, `controls-editor-theme-13`).
  - [ ] `ActivateNativeTextInputSession` keeps using the control after its own `SetFocus(_hwnd)`
    (`DxUi.NativeTextInput.cpp:591`) (`text-input-4`).
  - [ ] Tree press/double-click read the model through a hit test made before the focus callbacks
    (`DxUi.Tree.cpp:1128`). Keyboard landing calls raw `SetFocus` and continues (`:1380`, plausible). `RequestExpandedState`
    reports success after the publish destroyed the tree (`:698`) (`tree-2`, `tree-7`, `tree-9`).
  - [ ] UIA actions:
    - `SetFocus` on a tree item focuses the tree through a raw pointer after a publish that may destroy it
      (`DxUi.Accessibility.cpp:8435`).
    - `RemoveFromSelection` on grid rows touches the host with no survived() check (`:8866`).
    - Elements created with control identity 0 can never detect destruction (`:5654`), so `ExecuteSelect` can focus a
      freed Grid or Tree.

    (`tree-5`, `uia-selection-semantics-11`, `public-api-architecture-12`, `grid-selection-lifetime-17`,
    `uia-lifetime-threading-5`, `gap-uia-mutex-nested-loop-matrix-2`)
  - [ ] TextField/ComboBox continue after `SyncTextInput`, whose native accessibility publish raises UIA events. The
    window host dereferences the host after its own raises. Both are plausible: they depend on question Q3
    (`text-input-5`, `public-api-architecture-13`, `gap-uia-test-falsifiability-3`).
- [ ] **UIA actions run application code under the process-wide accessibility mutex** **[lead]**.
  - `ExecuteToggle`, `SetStringValue`, `SetRangeValue`, `Select`, `AddToSelection`, `RemoveFromSelection` and tree
    `Expand` keep the static `std::recursive_mutex` locked while delegates, focus callbacks, nested modal loops (a delegate
    that opens a dialog) and UIA raises run (`DxUi.Accessibility.cpp:8601`, `:8716`).
  - Every provider call on every thread blocks meanwhile, including Narrator's queries of other windows. A delegate that
    synchronously waits on another DxUi UI thread deadlocks.
  - `SetFocus` and `Invoke` already release the lock first. Fix: one resolve, release, act, revalidate helper for all
    eleven actions (A4).

  (`uia-lifetime-threading-4`, `uia-selection-semantics-1`, `host-core-2`, `cross-cutting-simplification-2`,
  `gap-uia-mutex-nested-loop-matrix-1`)

## P1: user-visible defects

Keyboard and pointer:

- [ ] **Tree: any key selects or focuses row 0 and scrolls to the top when no item is current** (#35, #53) **[lead]**.
  `DxUi.Tree.cpp:1389` runs the "first key" step before the switch, so Tab, Shift, Ctrl, Alt, Escape and the F-keys
  (the host offers Tab to the control first) change selection silently, without `OnTreeSelectionChanged`. Since #53 they
  also announce `ElementSelected`. Fix: only for keys the switch handles. (`tree-1`)
- [ ] **Grid: Shift+Up/PgUp/Home cannot extend a range upward** **[lead]**. The current row is
  `GetOrderedSelection().back()`, and `SetRange` stores the range in visible order. So after an upward range the "current"
  row is the anchor. A data refresh also reorders a Ctrl+click selection, which moves the keyboard position, moves UIA
  focus and fires `OnGridSelectionChanged` with nothing changed. Fix: a focused row of its own (A3).
  (`grid-selection-lifetime-1`, `uia-selection-semantics-2`, `grid-selection-lifetime-5`)
- [ ] **Embedded `Cancel` clears the pending double-click** **[lead]**. The reference sample (and docs/hosting.md's
  "Cancel on capture loss") sends Cancel on every `ReleaseCapture`, so embedded double-clicks never register: word
  selection, row activation. The window host does not clear it (`Embedded.cpp:438`). *Pre-existing*, in a function #29
  edited. (`gap-embedded-host-parity-3`)
- [ ] **Menu loop swallows every `WM_TIMER` with id 1 on the thread** **[lead]**. `kSubmenuHoverTimerId` and the
  `AnimationDispatcher` timer are both 1. The loop `continue`s without dispatching (`DxUi.Menu.cpp:5862`), so animations
  in other windows freeze while a modal menu is open. Fix: also require `FindPopupForHwnd(msg.hwnd)`.
  (`menu-loop-lifetime-1`)
- [ ] **Plain menus at 125%/175% reserve a scrollbar lane and scroll by a fraction of a DIP** (#24). The half-pixel
  tolerance was applied to described popups only (`DxUi.Menu.cpp:1183`). Every plain menu with an odd separator count at
  125% is affected. (`menu-layout-ux-1`)
- [ ] **Synchronous `ContextMenu::Show` never dismisses on deactivation or `WM_CANCELMODE`**: its branch is unreachable
  (`DxUi.Menu.cpp:5839`) (`menu-loop-lifetime-3`, question Q7).
- [ ] **Menu scrolling and pointer state.**
  - #59's edge reveal also runs when a submenu opens from hover or click, scrolling the parent under a stationary pointer
    without a repaint or UIA bounds (`DxUi.Menu.cpp:1306`, `:4073`).
  - Wheel and scrollbar scrolling keep the hovered row and its submenu timer on a row that moved (`:4980`).
  - Pointer targeting ignores z-order over the root's hidden scrollbar lane (`:4651`).

  (`menu-layout-ux-2`, `menu-loop-lifetime-5`, `menu-layout-ux-6`, `menu-loop-lifetime-6`)
- [ ] **Menu mnemonics compare the virtual key, not the typed character**, so non-Latin and accented labels are
  unreachable and the wrong command can run on other layouts. The first match invokes immediately even when several
  rows share the key (`DxUi.Menu.cpp:5380`, `:5420`) (`menu-loop-lifetime-4`, `menu-layout-ux-7`, `menu-layout-ux-4`,
  Q8).
- [ ] **Grid navigation**:
  - columns beyond the viewport cannot be reached by keyboard or horizontal wheel (`DxUi.Grid.cpp:4136`);
  - collapsed groups are unreachable by keyboard and UIA, and with every row grouped and collapsed the grid ignores all
    keys (`:3938`);
  - deleting the selected row loses the keyboard position;
  - `SetSelectionMode(Single)` keeps the oldest row, not the current one.

  (`grid-selection-lifetime-12`, `grid-selection-lifetime-3`, `grid-selection-lifetime-10`, `grid-selection-lifetime-18`)
- [ ] **Tree multi-selection drag accepts drops onto selected rows or inside another selected row's subtree**. The
  documented whole-selection move then creates a cycle (`DxUi.Tree.cpp:333`). Related: a pending release-collapse
  overrides selection made while the press is held, and `SetSelectedItemId` in multi-select keeps a non-visible id.
  (`tree-3`, `public-api-architecture-6`, `tree-8`, `public-api-architecture-14`)
- [ ] **Slider and editor controls**:
  - transitions freeze when an ancestor hides the control mid-transition, so a stale halo or pressed thumb shows on
    re-show (#62, `DxUi.Controls.cpp:4907`);
  - a slider removed mid-drag never gets `Cancel`;
  - a same-value `SetValue` during a typed NumericStepper edit ends the edit (`DxUi.EditorControls.cpp:929`);
  - Escape does not revert unparsed text;
  - slider setters are order-dependent (tick marks, `LargeStep`).

  (`controls-editor-theme-5`, `-10`, `-4`, `-12`, `-11`)
- [ ] **Host capture and inheritance**:
  - #29's stale-capture cancel misses controls captured inside a ScrollPanel (`DxUi.Controls.cpp:8948`) and runs only at
    message entry, not on `SetFocusControl`'s prune (`DxUi.WindowHost.cpp:4139`);
  - PageHost pages never inherit flow direction or density (#31, `DxUi.Controls.cpp:1380`).

  (`host-core-6`, `host-core-4`, `host-core-8`)

Text input:

- [ ] **The host window's title messages go to the focused text field** **[lead]**. The native backend is the only one,
  so every window host has this. While a text control has focus, `WM_GETTEXT`, `WM_GETTEXTLENGTH` and `WM_SETTEXT`
  sent to the host HWND are answered by that field (`DxUi.WindowHost.cpp:2977`, `DxUi.NativeTextInput.cpp:1071`,
  `:1096`).
  - An application that calls `SetWindowTextW` on its window while the user types replaces the field's text and fires
    `onTextChanged`; the title does not change.
  - The accessibility publish takes the window name from `GetWindowTextW` (`DxUi.Accessibility.cpp:953`), so UIA
    clients read the field's text, a masked field's plaintext included, as the window's name.

  Fix: stop answering these three messages for the host window (Q17). (`text-input-3`)
- [ ] **Native TSF edits go through `SetText`**: IME commits, the emoji panel and dictation reset a multiline field's
  scroll to the top and clear undo (`DxUi.TextStoreACP.cpp:389`). *Pre-existing*. Apply them through
  `ImportTextInputState` as the IMM and embedded paths do. (`text-input-7`, Q17)
- [ ] **IME placement and composition**:
  - single-line `GetTextExt` ignores horizontal scroll and the masked display text (`DxUi.TextStoreACP.cpp:272`);
  - the native TSF path applies every composition preview as a committed, notifying edit (`:1157`);
  - #55's multiline caret clip half-erases the caret at line start (`DxUi.TextInput.cpp:1605`);
  - the IME caret rect is not clipped.

  (`text-input-13`, `text-input-11`, `text-input-6`, `text-input-16`)
- [ ] `EM_SETSEL` with a reversed range drops the selection, and with start -1 moves the caret to 0. An IMM
  composition continues into another DxUi field of the same HWND. (`text-input-20`, `text-input-12`)

Grid multiline cells (#22, #36):

- [ ] **The full-value tooltip has no size bound**: it grows past the window, cuts at 256 DIP with no marker, and is
  re-laid out on every mouse move (`DxUi.Controls.cpp:9467`) (`grid-multiline-1`).
- [ ] **Single-line cells still break on separators**: a trailing CR/LF shifts and clips the caption, and
  U+2028/U+2029/NEL/VT/FF hide later lines with no tooltip (`DxUi.Grid.cpp:2538`) (`grid-multiline-4`).
- [ ] The Marquee band paints outside its track over the previous column, including under reduced motion (#28,
  `DxUi.Grid.cpp:477`) (`grid-multiline-2`).
- [ ] Copying multiline cells writes raw CR/LF/TAB into the TSV, so a pasted row breaks apart, and the #36 test locks
  this in (`grid-multiline-3`, Q19).
- [ ] Low:
  - no horizontal clip in very narrow multiline cells;
  - hover tooltip ignores viewport-cut cells;
  - justified text loses justification when lines are omitted;
  - Spinner/Marquee captions are measured with the wrong font.

  (`grid-multiline-7`, `-6`, `-11`, `-12`)

## P1: accessibility

Announcements and focus:

- [ ] **Range values never reach UIA clients in a window host** **[lead]**.
  - Slider, Splitter and ProgressBar never republish the snapshot after a keyboard step, a UIA `SetValue` or a
    programmatic value.
  - The window-host diff raises no property events at all (only structure, focus and selection).
  - Narrator is silent, and `RangeValue.Value` stays stale.

  (`controls-editor-theme-2`, `uia-selection-semantics-9` for menu slider rows)
- [ ] **Embedded hosts raise no focus event when focus moves between tree items or grid rows**, or onto a newly added
  or replaced focused control. On gaining focus they announce the Tree, not the item `GetFocus` reports. Fix: one focus
  rule for both hosts (A6). (`gap-embedded-host-parity-1`, `grid-selection-lifetime-8`, `tree-6`,
  `uia-lifetime-threading-6`, `uia-selection-semantics-10`, Q16)
- [ ] **`HasKeyboardFocus` is wrong in three places**:
  - every selected grid row reports it (the Tree fix was not applied to Grid);
  - an embedded view without keyboard focus reports it for the focused item;
  - the window root is announced as focused but answers false.

  (`uia-lifetime-threading-8`, `uia-selection-semantics-7`, `grid-selection-lifetime-20`, `tree-14`,
  `uia-lifetime-threading-9`)
- [ ] **`SetFocusControl` prunes first and announces an intermediate "window" focus before the requested control**. The
  publish's `GetWindowTextW` re-enters `HandleMessage`, and a nested republish is then overwritten by a stale snapshot.
  (`uia-lifetime-threading-7`, `host-core-9`, `uia-lifetime-threading-2`)
- [ ] **Missing UIA events**:
  - expanding or collapsing a tree item raises nothing;
  - wheel scrolling never republishes, so bounds and hit tests go stale;
  - `OnDpiChanged` never republishes;
  - `ApplyGroupLayout` changes selection without publishing.

  (`uia-selection-semantics-8`, `-14`, `host-core-7`, `grid-selection-lifetime-13`)
- [ ] After Ctrl+A on a grid of more than about 280 rows, UIA focus names a row element the snapshot does not hold (no
  name or control type) (`grid-selection-lifetime-7`).

Selection patterns:

- [ ] **`AddToSelection` on an already-selected grid row deselects it**: it is implemented as a Ctrl+click toggle
  (`DxUi.Accessibility.cpp:8803`) **[lead]** (`uia-selection-semantics-3`, `grid-selection-lifetime-4`).
- [ ] **UIA `SetFocus` on a tree item changes a single-select tree's selection without `OnTreeSelectionChanged`**. On a
  grid row it replaces a multi-row selection. `RemoveFromSelection` on a single-select tree clears silently.
  (`tree-4`, `uia-lifetime-threading-10`, `uia-selection-semantics-12`, `grid-selection-lifetime-9`,
  `uia-selection-semantics-4`, Q14)
- [ ] Disabled Tree and Grid accept UIA Select, AddToSelection, RemoveFromSelection and Expand/Collapse
  (`uia-selection-semantics-5`, `tree-15`).
- [ ] A selection that becomes exactly one new item is reported as `Selection_Invalidated` whenever more than 20 were
  selected before (WPF reports `ElementSelected`) (`uia-selection-semantics-6`, Q14).
- [ ] Tree items always report `IsOffscreen=false` (`uia-selection-semantics-16`).

Menus (#56):

- [ ] **Row names leave out the accelerator column**: shortcuts, Info values and a slider's current stop are never
  exposed. Ordinary rows expose no submenu ExpandCollapse state. (`menu-layout-ux-5`, `uia-selection-semantics-15`)
- [ ] **UIA focus on a row leaves the hovered row selected**, and a later pointer move clears the keyboard row without
  repaint or UIA sync, so Enter invokes a row other than the one UIA reports. Closing a submenu or a UIA `SetFocus` on a
  row raises no focus event. A root switch focuses the owner in between. (`menu-layout-ux-3`,
  `uia-selection-semantics-13`, `uia-lifetime-threading-17`, `menu-loop-lifetime-7`)

Text ranges:

- [ ] `Move` reports `moved=1` without moving at the last character and for every Paragraph/Page/Document move, so
  "move until 0" clients never stop. Visual-line moves overflow `int` for large counts. (`uia-lifetime-threading-11`, `-18`)

Visual accessibility (gallery regeneration and committed design-system update required):

- [ ] **The Tree focus ring has the selection fill's own color**, so with multi-select the current row is invisible once
  selected (1:1 in the default light theme, about 1.2:1 in dark and HC) (#35, `DxUi.Tree.cpp:183`). The Grid draws no
  focus indicator at all. (`gap-visual-modes-hc-rtl-dpi-1`, `grid-selection-lifetime-11`)
- [ ] **Unfocused selected-row text is never contrast-checked**: the Tree fails in every Windows HC theme (about 1.4:1),
  the Grid in the default light theme (about 1.9:1) (`DxUi.Tree.cpp:78`). *Pre-existing*, but multi-select makes whole
  blocks unreadable. `ChooseContrastingTextColor` picks failing colors on mid tones, and #29 added a correct WCAG helper
  beside it instead of fixing it (A7). (`gap-visual-modes-hc-rtl-dpi-2`, `-6`)
- [ ] Described-menu descriptions fall to about 3.5:1 in the light HC theme (Desert) (`gap-visual-modes-hc-rtl-dpi-3`,
  Q11).
- [ ] Tree, described menus, Grid and determinate ProgressBar ignore `FlowDirection`, and no public document records the
  limit (`gap-visual-modes-hc-rtl-dpi-4`, `controls-editor-theme-8`, Q10).

## Security and privacy

- [ ] **A masked TextField answers `WM_GETTEXT` with its plaintext** **[lead]**. Any process in the session can send it,
  unlike a Win32 `ES_PASSWORD` edit. The edit-message shim also answers the host window's own title messages
  (`DxUi.NativeTextInput.cpp:1071`). Masked fields also expose the plaintext to TSF text services and accept IME
  composition (`DxUi.TextStoreACP.cpp:339`). (`text-input-3`, `text-input-9`, Q17)
- [ ] **Registered messages trust their parameters** **[lead]**.
  - `AccessibilityCreateProvider` writes a provider pointer through a raw `lParam` (`DxUi.Accessibility.cpp:9696`), and
    its sender uses an untimed cross-thread `SendMessageW`.
  - Use the existing token-based `SendMessagePayload` with a timeout. That removes both the arbitrary-address write from
    another process and the unbounded wait.

  (`host-core-5`; `menu-loop-lifetime-9` was refuted as a *security* issue on the same-session trust model, Q18)
- [ ] The gallery publish job keeps its `contents: write` token in `.git/config` while it restores, builds and runs
  branch code (`.github/workflows/gallery.yml:94`). Use `persist-credentials: false` and push with the token only in
  the commit step. (`ci-workflows-4`)

## Resources and performance

- [ ] **Accessibility publish cost**:
  - each publish costs O(selected rows) model lookups even after #43 made paint flat;
  - every key press scans all rows, and Ctrl+C scans all groups per selected row;
  - `Tree::SetSelectedItemIds` is quadratic;
  - every Ctrl+click copies every visible `TreeItemData`.

  (`grid-selection-lifetime-6`, `-19`, `cross-cutting-simplification-5`, `public-api-architecture-4`)
- [ ] **Snapshot rebuilt eagerly**: every focus, selection or text change rebuilds the whole window snapshot (every
  expanded tree item, every grid row on screen) even when no UIA client listens. Only the diff is gated on
  `UiaClientsAreListening` (lead observation, Q4, A1).
- [ ] **Text-range providers pin a full window snapshot each**, one per caret move. #29 stopped element providers doing
  this, and embedded event providers still do. Every text-range call copies the control's whole text.
  (`uia-lifetime-threading-3`, `cross-cutting-simplification-4`, `gap-embedded-host-parity-2`,
  `uia-lifetime-threading-19`)
- [ ] **Multiline TextField shapes its text twice per frame**, caret blink included. The multiline `GetTextExt`
  fallback rebuilds a full layout per index. (`text-input-8`, `text-input-14`)
- [ ] **Posted payloads**:
  - the registry serves at most 128 windows (hosts plus open popups) and silently refuses later ones, so UIA actions
    and menu UIA invoke stop working;
  - a full registry is not diagnosed;
  - its static destructor frees entries without the lock.

  (`host-core-10`, `menu-loop-lifetime-11`, `uia-lifetime-threading-20`, `gap-uia-mutex-nested-loop-matrix-3`,
  `menu-loop-lifetime-8`, `gap-arm64-asan-runtime-7`)
- [ ] **Allocation failure terminates**: the window-host publish and the selection diff allocate inside `noexcept`
  functions, and `ParseMenuLabel` is `noexcept` but allocates, so #24's `bad_alloc` handlers cannot catch it.
  `GetSelection` creates one provider per selected row without bound. (`uia-selection-semantics-17`,
  `uia-lifetime-threading-14`, `menu-layout-ux-11`)
- [ ] Low:
  - described rows build a layout per row with no row bound, twice after a DPI move;
  - the omitted-tail layout table forgets shared layouts;
  - zero-extent preparation keeps the Grid's layouts;
  - tree animation does linear id scans per row per frame.

  (`menu-layout-ux-12`, `-9`, `grid-multiline-5`, `gap-embedded-host-parity-6`, `tree-10`)

## Test infrastructure, tests and CI

Interactive lease (#48, #63):

- [ ] **Ctrl+C kills `DxUi.InteractiveLease.exe` through the PowerShell pipeline before it restores the desktop**
  **[lead]**. The lease runs as a redirected native command (`| Out-Host`, and its output is assigned), and pwsh kills
  those when the pipeline stops (`Tools/InteractiveRun.psm1:223`). Start it with `Start-Process -NoNewWindow -PassThru`
  and wait for it in `finally`. (`tooling-runners-1`, `test-support-infra-1`)
- [ ] **Consent and restore edge cases**:
  - a declined, timed-out or interrupted confirmation still moves the pointer and takes focus;
  - the pointer is recorded before the dialog, so every mouse-confirmed run blames a fixture;
  - the Start button's access key works without Alt;
  - the console-close grace is shorter than the lease's worst-case clean-up;
  - the watchdog does not cover a hang in process exit.

  (`test-support-infra-2`, `-3`, `-4`, `-8`, `-6`)
- [x] A plain `test.ps1` ran Menu and NativeTextInput without the lease. Fixed by #66: they left the default list, and a
  local run that names them without `-Interactive` is refused (`test.ps1:29`, `:56`). (`process-docs-specs-1`,
  `tooling-runners-2`)
- [ ] **The non-interactive NewControls lane still touches the desktop**: it moves the physical pointer
  (`TestMenuChoosesTheCursorWhenItOpensAndCloses`), and its activation-dependent description tests silently pass.
  (`tests-menu-a11y-host-1`, `gap-arm64-asan-runtime-4`, `tests-menu-a11y-host-6`, Q20)

UIA test clients:

- [ ] **Clients can hang or miss events**:
  - `UiaTestClient` subscribes desktop-wide without #63's hang protection;
  - `TryAsk` destroys the request functor while the client thread may run it;
  - `FocusEventClient` and four hand-written client threads stop listening after fixed 10-30 s, so late "no event"
    checks pass vacuously (A9).

  (`gap-arm64-asan-runtime-3`, `test-support-infra-7`, `test-support-infra-5`, `gap-arm64-asan-runtime-9`,
  `tests-menu-a11y-host-3`, `gap-uia-test-falsifiability-8`)

Tests that cannot fail:

- [ ] **Event and lifetime assertions**:
  - every selection-event test deduplicates what it hears, so a double raise (double Narrator announcement) cannot fail;
  - "no event" assertions rely on a 500 ms or 1 s quiet window;
  - embedded event tests check presence only and invalidate by hand;
  - callback-destroys-control tests assert things that hold after a use-after-free;
  - #60's replacement tests cannot see a stale `_host` in Release;
  - no test covers a focus callback destroying the control during native `SetFocus`, or a UIA action into a delegate
    that runs a nested loop.

  (`gap-uia-test-falsifiability-1`, `-2`, `-6`, `tree-16`, `tests-grid-tree-render-3`, `gap-uia-test-falsifiability-5`,
  `tests-grid-tree-render-4`, `gap-uia-mutex-nested-loop-matrix-5`)
- [ ] **Visual baselines**: their tolerance let #62's slider redesign through with no baseline update
  (`Tests/Controls/Controls.Tests.DxUiTestHelpers.h:567`) (`tests-grid-tree-render-1`, Q20).
- [ ] **Wall-clock races**: the touch-halo and hover animation tests (the flake #44 fixed for tooltips), the 500 ms
  focus-gain-turn limits, the disclosure client's `changes >= 2`, and the cursor test's early return that leaves an
  async menu open. (`tests-grid-tree-render-2`, `gap-arm64-asan-runtime-5`, `-2`, `tests-menu-a11y-host-2`, `-5`,
  `gap-uia-test-falsifiability-4`, `tests-menu-a11y-host-4`)

CI (#46, #49, #61):

- [ ] **CI counts capability skips as a pass**, so the ARM64 and ASan focus checks can go green without running. This
  matters more since #66: `test.ps1 -Full` on GitHub Actions (`test.ps1:51`) is now the only automatic run of Menu and
  NativeTextInput, and a suite with skips still prints PASS (`test.ps1:199`). Keep a per-lane expected-skip baseline
  and fail on new skips. (`gap-arm64-asan-runtime-1`, Q20)
- [ ] **No CI result can block a merge**. Verified on 2026-10-06: main's only ruleset ("main protect") has deletion,
  non-fast-forward and pull-request rules but no required status checks, there is no classic branch protection, and
  auto-merge is off. Even with protection, no check can require the native matrix: a documentation-only pull request
  reports it under an unexpanded name. Run 37489742358 shows two failing native jobs beside a green `validation`. A
  gallery commit pushed to main leaves main's head without a push run. (`ci-workflows-1`, `ci-workflows-2`, Q21)
- [ ] **Timeouts**: the ASan step budgets exceed the 40-minute job limit (6 minutes of headroom on ARM64), and the
  validation and format jobs have no timeout. (`ci-workflows-3`, `gap-arm64-asan-runtime-6`, `ci-workflows-8`)
- [ ] Low:
  - the native-scope "license" rule names `LICENSE` while the file is `LICENSE.txt`;
  - `Commit-Gallery.ps1` commits everything already staged;
  - nothing tests the docs-only-skip premise.

  (`ci-workflows-6`, `tooling-perf-gate-9`, `ci-workflows-5`, `ci-workflows-7`)

Benchmark gate:

- [ ] **Unmeasured inputs can make a real regression pass**:
  - the identical-library "noise" rule keys on a fingerprint that leaves out build and restore inputs (`DxUi.sln`,
    `build.ps1`, `vcpkg-configuration.json`, toolchain modules, the benchmark project), so a regression caused only by a
    build change can pass the PR gate;
  - the allocation counters live in `EmbeddedTests.cpp`, outside both the fingerprint and the paired overlay.

  (`tooling-perf-gate-1`, `gap-perf-evidence-pipeline-1`, `tooling-perf-gate-4`, `gap-perf-evidence-pipeline-3`, Q22)
- [ ] **Statistics and evidence handling**:
  - the rank test treats the 12 A,B,B,A runs as independent;
  - `Restore-HarnessOverlay` overwrites harness files without checking they are unchanged, and since #66 the overlay
    also writes the renamed fixtures under their old names into old revisions (`Tools/PairedRun.psm1:14`, `:139`);
  - `Publish-BenchmarkVerdict` without `-Reports` judges the newest `summary.json`, possibly a previous run's.

  (`tooling-perf-gate-2`, `-3`, `-5`, `gap-perf-evidence-pipeline-4`)
- [ ] Low:
  - the control-drift rule is implemented twice;
  - the paired run builds the whole solution twice;
  - receipts and paired worktrees are never pruned.

  (`tooling-perf-gate-7`, `-6`, `gap-perf-evidence-pipeline-5`)

Tooling:

- [ ] **Consumer-facing scripts**:
  - a relative `-Root` resolves against the process directory, not the PowerShell location;
  - `Validation.psm1` fails to import in Windows PowerShell 5.1, which breaks `validate-build-matrix.ps1 -Root <consumer>`.

  (`tooling-runners-4`, `tooling-runners-3`, Q23)
- [ ] **API-revision gate**:
  - the consumer-interface gate checks only that files exist;
  - it treats a new mandatory parameter as compatible;
  - the revision 3 migration list omits most of #29's renames (`RunModalLoop`, debug and `Typography` names).

  (`public-api-architecture-7`, `-8`, `-2`, Q24)
- [ ] Low:
  - `-Interactive -Tests` without `-Suites` takes the desktop for a run that must fail;
  - a lease-bound timeout is reported without the test name;
  - `Test-InteractiveMode` fails while other runs write logs;
  - `Test-AsanRuntime` leaves a directory per run;
  - `format.ps1` keeps its own vswhere discovery;
  - `Test-TestFilter` re-runs the Grid suite.

  (`tooling-runners-6`, `-5`, `-7`, `-8`, `-9`, `-10`)

## Scoped testing (#66, #67)

- [ ] **A plain `test.ps1` can run nothing and exit 0** **[lead]**.
  - With only `-Configuration`, `-Platform` or `-SkipBuild`, it hands off to `Test-Changes.ps1` in Affected mode
    (`test.ps1:39`). On a clean checkout of main (the README quick start, a pinned or shallow consumer checkout) that
    selects no scope, prints "No local test execution is required by this plan." and exits 0 (`Test-Changes.ps1:69`).
  - Selection is by changed path only, so a Visual Studio, Windows or driver update on a clean tree also runs nothing.
  - AGENTS.md's code-change requirement and `Core_PerformanceAndResources.md:257` still describe `test.ps1` as the gate
    that always benchmarks and runs tooling.
  - Fix: report NOT_EVALUATED with a non-zero exit when nothing ran, or make Affected an explicit switch, and update
    the documents.

  (`scoped-integration-2`, `scoped-engine-6`, `scoped-integration-3`, Q27)
- [ ] **PrePush can report coverage that nothing enforces** **[lead]**. It delegates (`Tools/ScopedTesting.psm1:224`):
  - native scopes to CI jobs that, by `ci.yml:59-62`'s own rule, no required check may name;
  - from main or a detached HEAD, where no pull request will run;
  - after checking the candidate's `ci.yml` digest, although a pull_request run uses the merge ref's workflow;
  - with only `ci.yml` bound, not the scripts that decide what CI runs (`NativeScope.psm1`, `test.ps1`);
  - InteractiveLease, whose confirmation and warning proofs may skip on hosted runners;
  - after checking the manifest's hard-coded repository, not the remote the push goes to.

  Fix: delegate only from a non-default branch, bind the CI-deciding scripts, compare with `origin/<base>`'s workflow,
  and keep native scopes local until one required aggregate check covers them (`ci-workflows-1`).
  (`scoped-engine-1`, `scoped-integration-4`, `scoped-engine-5`, `-7`, `-10`, `-13`, `-18`, Q21, Q28)
- [ ] **Scope rules miss inputs** **[lead]**:
  - `Tools/tests/**` selects only Tooling (`validate.ps1` and `Test-AsanRuntime.ps1`). But `Test-TestFilter`,
    `Test-TestWatchdog` and `Test-InteractiveLease` run only on `test.ps1`'s native path (`test.ps1:84-91`), so a
    change to them never runs them.
  - Files `validate.ps1` reads (gallery, Measurements, non-Markdown docs, Changes) select no scope.
  - The ComboBox fan-out omits NewControls, which tests TagPicker.
  - Three rules name files that do not exist, and nothing checks that rules match.
  - Edits to Menu or NativeTextInput tests fall back to "full" and print `FULL_NONINTERACTIVE_PASSED` without running
    their suite.

  Fix: a validator that every rule matches a tracked file and every fixture maps to a scope that runs it, and an
  explicit NOT_RUN obligation for the foreground suites. (`scoped-integration-1`, `scoped-engine-2`, `-3`,
  `scoped-integration-8`, `scoped-engine-9`, `scoped-integration-11`, `scoped-engine-14`, `scoped-integration-9`, Q29)
- [ ] **Reused evidence can be stale**:
  - **[lead]** A pass with capability skips becomes a reusable receipt, and the identity ignores desktop and session
    state. A run under a covered or locked desktop is reused later, when the skipped tests would run.
  - The environment identity omits the GPU driver, `d2d1`/UIA/TSF runtime DLLs, the OS build revision, fonts, DPI, the
    `DXUI_*` performance switches, and git/gh and their configuration.
  - Git output is decoded with the console code page, so a non-ASCII tracked path (none today) would silently leave the
    identity.
  - The start/end snapshot cannot see an input that changes and reverts during a run.

  Fix: never reuse a run that skipped, or require an equal skip set; widen the identity; decode git as UTF-8.
  (`scoped-integration-5`, `scoped-engine-4`, `-12`, `-17`, `-8`, `-16`, Q30)
- [ ] **"Full" is not CI's full**. `FULL_NONINTERACTIVE_PASSED` and `test.ps1 -Full` omit `test-consumer.ps1` (both
  variants), the MenuTextLayoutResources fixture and `gallery.ps1`. So a broken `Tests/ConsumerModules` source or
  `Build/DxUi.Consumer.props` is never compiled locally. (`scoped-integration-6`, `scoped-engine-15`)
- [ ] **CI**:
  - The required `validation` check can now be skipped. It needs `windows-tooling` with `if: !cancelled()`, and a run
    cancelled while `windows-tooling` runs reports `validation` as skipped, which GitHub counts as passing (plausible,
    Q31).
  - Nothing guards the CI-only Menu/NativeTextInput addition (`test.ps1:51`).
  - `GITHUB_ACTIONS` alone authorizes the focus suites without the lease, self-hosted runners included.

  (`scoped-integration-7`, `-13`, `-10`)
- [ ] **The renames made every retained pre-rename benchmark baseline unusable**. `benchmarkSha256` hashes paths, so
  `test.ps1 -PerformanceBaseline` with a pre-rename receipt fails "Unmatched fixture" for byte-identical fixtures; only
  `performance-paired.ps1` compares across the rename. The paired overlay's legacy aliases are hard-coded and invisible
  to its SkipBuild check. (`scoped-integration-14`, `-12`)
- [ ] `Test-Changes.ps1` says it does not touch the desktop, but scopes it selects (NewControls) move the physical
  pointer (`scoped-integration-15`; see the NewControls item above).

## API and contract consistency

- [ ] `Grid::RequestSelectRow`, `RequestRemoveRowSelection` and `RequestToggleCheckboxCell` return true for a grid that
  did not survive, unlike Tree's requests. UIA actions whose callbacks destroyed the element report success or
  `NOTSUPPORTED` instead of `UIA_E_ELEMENTNOTAVAILABLE`. (`public-api-architecture-3`, `grid-selection-lifetime-16`,
  `uia-lifetime-threading-15`)
- [ ] `GridColumnDesc::multiline` and `textAlignment` are public but never read. The Grid's text-layout cache internals
  live in `DxUi.h`. The menu-internal `transferNativeFocus` flag is on the public `ControlHost::SetFocusControl`.
  (`grid-multiline-8`, `grid-multiline-9`, `public-api-architecture-11`)
- [ ] **Remaining null-brush draws**: Grid, Scrollbar, Tree, Button and ComboBox still pass possibly-null brushes to
  Direct2D, the class #29 removed from the editor controls. (`controls-editor-theme-14`, `-6`,
  `cross-cutting-simplification-10`)
- [ ] Parent navigation builds a separate, non-canonical root provider, and replaced canonical roots are never
  disconnected (`uia-lifetime-threading-16`).

## Process and documentation

- [ ] **Stale WIP plans and indexes**:
  - [SliderTouchHalo](SliderTouchHalo_2026-10-04.md) still lists the merge as open.
  - [TreeReorder](TreeReorder_2026-09-21.md) and the Tree contract keep a row-straddle limit #38 removed.
  - [GridTextOverflow](GridTextOverflow_2026-09-21.md) describes the replaced V11 cache.
  - [CiRunScope](CiRunScope_2026-10-04.md)'s validation figure does not reproduce.
  - The Done index omits three plans.

  (`process-docs-specs-5`, `-6`, `tree-13`, `grid-multiline-10`, `process-docs-specs-7`, `-4`, Q25)
- [ ] **Stale public docs**:
  - docs/controls.md still calls described menus in progress, pending qualification;
  - `ThemeColors.h` documents the pre-#29 alert fallback.

  (`process-docs-specs-2`, `controls-editor-theme-9`, `public-api-architecture-9`)
- [ ] The reference embedded sample never sets `PointerEvent::device`, so the touch feedback #62 documents never
  appears there (`gap-embedded-host-parity-7`).

## Simplification and architecture proposals

Ordered by payoff. A1, A3 and A5 remove whole defect classes listed above rather than individual sites.

- [ ] **A1. Make accessibility publication non-reentrant.** Today `RefreshAccessibilitySnapshot()`, a `const noexcept`
  method, raises UIA events synchronously. #60 assumes those can dispatch messages that destroy `this`, so every caller
  needs a lifetime check, and many still lack one (ComboBox 9 calls, TextField 7, TabControl, the UIA actions).
  - Queue the publish and raise it from the host once the message turn ends (window host) or in `UpdateAccessibility`
    (embedded). Then no control method can be destroyed by its own publish, and about two dozen guards added by
    #57/#60 become unnecessary.
  - The same queue lets the host skip building snapshots while no client listens and build on the first
    `WM_GETOBJECT`, which removes the per-keystroke whole-window rebuild.
  - Decide with Q3 and Q4.
- [ ] **A2. One selection-commit helper per selector.** Grid repeats the "copy previous, mutate, compare, delegate,
  check lifetime, publish, check lifetime" sequence 9 times and Tree about 6. The copies have drifted: `ApplyGroupLayout`
  never publishes, and `ToggleCheckboxCell` takes its token late. Add `[[nodiscard]] bool PublishSelectionChange(previous)`
  and a `Survives(f)` helper on `Control`: about -150 lines. (`cross-cutting-simplification-6`, `grid-selection-lifetime-14`,
  `tree-11`, `tests-grid-tree-render-5`, `grid-selection-lifetime-15`)
- [ ] **A3. Give Grid a focused row separate from its selection**, as Tree has since #35, using Tree's
  Replace/Range/Toggle/FocusOnly modes. One change fixes:
  - Shift+Up and refresh reordering;
  - `HasKeyboardFocus` on every row and the Ctrl+A focus element;
  - UIA `SetFocus` replacing the selection;
  - the missing focus indicator and the deletion fallback;
  - the documented Ctrl+Up/Down quirk.

  Grid and Tree then share one gesture contract. (`grid-selection-lifetime-21`, Q12, Q13)
- [ ] **A4. One `RunOnWindowThreadAction` helper (resolve under the lock, act unlocked, revalidate, map to
  `UIA_E_ELEMENTNOTAVAILABLE`) for all eleven UIA actions.** Then replace the process-wide `recursive_mutex` with a
  per-target non-recursive lock: snapshots are already atomic per target, and the recursion hides same-thread
  reentrancy. (`gap-uia-mutex-nested-loop-matrix-4`, Q5)
- [ ] **A5. Make tree mutation ownership-safe.**
  - Add `Panel::TakeChild(index)`, which clears `_parent` and `_host`, and stop documenting raw moves out of
    `GetChildren()`.
  - Have the host defer destruction of a replaced root or removed subtree to the end of the message turn. Callbacks
    then cannot free a control while a caller still holds it, which closes the P0 callback class by construction
    instead of site by site.
  - Also have `SetFocusControl` take the requested lifetime before it prunes.

  (lead proposal, Q1, Q2)
- [ ] **A6. One focus rule for both hosts.** Compute `SameFocusedElement` for embedded targets too, and raise the event
  on the snapshot's focused fragment (the item or row `GetFocus` reports), gated on placement focus. Remove the per-record
  focus event from `RaiseEmbeddedAccessibilityChanges`. (`gap-embedded-host-parity-1`)
- [ ] **A7. One contrast resolver in `Theme.cpp`.** Fix `ChooseContrastingTextColor` with the WCAG helper #29 added,
  and resolve the unfocused-selection text and the selected-row focus ring once for Tree and Grid.
  (`gap-visual-modes-hc-rtl-dpi-6`, `-2`, `-1`)
- [ ] **A8. One cross-thread call primitive in `Support/PostedPayload.h`.** #47's `MenuDebugDispatch` re-implements the
  accessibility UI-action Pending/Taken/Abandoned handshake. Use the same primitive for `AccessibilityCreateProvider`,
  and make the registry capacity a diagnosed limit. (`cross-cutting-simplification-9`, `menu-loop-lifetime-10`, about -60
  lines)
- [ ] **A9. One UIA test client.** Move `FocusEventClient` and the four hand-written client threads to
  `UiaTest::Client`. Give it #63's hang protection, and record raw (not deduplicated) event counts.
- [ ] **A10. Smaller deduplications (behavior-preserving)**:
  - `MixAccessibilityHash` duplicates the layout-key hash;
  - `PaintedOver` duplicates `CompositeOverBackground`;
  - a private `ControlBelongsToBranch` and `RevalidateScrollPanelChild` duplicate `IsControlInTree` and
    `RevalidateDispatchedControl`;
  - four id-set membership shapes exist;
  - six copies of the target-release lambda sit beside manual `Release()` calls (AGENTS.md: no manual Release);
  - the two `ComputeItemLayoutMetrics` overloads repeat the row layout;
  - the menu visible-height clamp is written three times, and a dead branch remains;
  - `Show` duplicates `EndAsyncMenuInteraction`;
  - the text store has duplicated tree walkers and a dead LRU branch;
  - the control-drift rule is written twice;
  - `format.ps1` keeps its own vswhere discovery.

  (`cross-cutting-simplification-7`, `-8`, `-11`, `gap-visual-modes-hc-rtl-dpi-7`, `tree-12`, `menu-layout-ux-8`,
  `menu-loop-lifetime-12`, `text-input-19`, `tooling-perf-gate-7`, `tooling-runners-9`)

## Questions for the developer

Lifetime and architecture:

1. **Q1.** Is promoting a descendant of the current root through the same host's `SetRoot` supported? The
   `GetChildren()` comment says so. The fix is needed either way, but the answer decides whether A5's `TakeChild`
   replaces raw moves.
2. **Q2.** Should DxUi controls survive *any* callback destroying them (selection, checkbox, group, text-changed, blur,
   context menu), or should docs/controls.md say every rebuild must be posted? Today only the selection delegate may
   rebuild synchronously. A5's deferred destruction would make the answer "always". Does the mutable `GetChildren()`
   span support rearrangement during propagation, or extraction of a TabControl page without `RemoveTab`? The
   current pointer/span bookkeeping cannot guarantee those cases; define that boundary before claiming them safe.
3. **Q3.** Is there measured evidence that `UiaRaiseAutomationEvent` dispatches window messages on the raising thread
   (non-COM-threaded providers, out-of-process clients)? The #60 guards, and several "plausible" items above, rest on it.
   A1 makes it moot.
4. **Q4.** Is the listener-independent, whole-window snapshot rebuild on every keystroke, focus and selection change an
   accepted cost, or should building wait for a client (A1)?
5. **Q5.** Was keeping the accessibility mutex across application callbacks in seven Execute* actions deliberate (unlike
   SetFocus and Invoke)? Are processes with DxUi hosts on several UI threads (RedSalamander viewers or plugins) supported?
6. **Q6.** When do consumers call `ShutdownAllWindowHostsForProcessExit`: only after every DxUi window is gone, or with
   windows and `ShowAsync` menus alive (`WM_ENDSESSION`, a tray Exit)?

Menus:

7. **Q7.** Should a synchronous `ContextMenu::Show` close on deactivation and `WM_CANCELMODE`, as `ShowAsync` and
   `TrackPopupMenu` do?
8. **Q8.** Should mnemonics follow the active layout's character (`WM_CHAR`/`WM_SYSCHAR`, as `NativeMenuBarHost` does),
   and should duplicate mnemonics cycle instead of invoking the first match?
9. **Q9.** Described menus:
   - should a menu with any described row always take the maximum width (456 DIP)?
   - should the description be the UIA `FullDescription`/`HelpText` instead of joined into `Name`?
   - should one row that fails to prepare fall back to plain rather than refusing the menu?
   - what is the largest described row count consumers will show?

Visual design:

10. **Q10.** Are Tree, menus, Grid and ProgressBar meant to mirror under `FlowDirection::RightToLeft`, or should the
    "not mirrored yet" limit be documented?
11. **Q11.** In high contrast, should application-supplied alert colors and the slider halo accent be honored, or should
    system colors be forced? Is the blended description color acceptable in HC?

Grid, Tree and UIA semantics:

12. **Q12.** Should Grid adopt Tree's focused-row model (A3): Ctrl+arrows move focus alone, Ctrl+Space toggles, and the
    current row has a focus indicator? Should deleting the selected rows move to the row that took their place?
13. **Q13.** Should Ctrl+click and Ctrl+Space move the range anchor (Explorer and ListView behavior) in Tree and Grid?
14. **Q14.** UIA selection contract:
    - `AddToSelection`/`RemoveFromSelection` on a single-select control: replace/clear, or `UIA_E_INVALIDOPERATION`?
    - UIA `SetFocus` on a tree item or grid row: notify the delegate? keep a multi-selection?
    - Should `Select` move keyboard focus?
    - Should "became exactly one new item" win over the more-than-20 rule, as in WPF?
15. **Q15.** Since #55, a window root that clients hold becomes "gone" whenever the window flips between one semantic
    control and several (a status label shown next to a single Tree). Intended?
16. **Q16.** For embedded hosts, should DxUi raise item-level focus events (A6), or does the application site own that?

Text input:

17. **Q17.** Text input policy:
    - Should IME commits be undoable?
    - Do real TIPs use the native `ITextStoreACP` path in a WindowHost, and has a real IME or the emoji panel been run
      against a scrolled multiline field?
    - For masked fields: refuse `WM_GETTEXT` from other processes, disable IME as `ES_PASSWORD` does, or keep IME with an
      `IS_PASSWORD` input scope?
    - Should the Win32 edit-message shim answer for a host attached to a top-level window at all?

Security and capacity:

18. **Q18.** What is the threat model for registered messages: are other processes in the session trusted? What is the
    largest number of simultaneously attached hosts plus open popups RedSalamander needs (the payload registry holds
    128)?

Grid multiline:

19. **Q19.** What clipboard format should multiline cells use (raw TSV, locked in by a #36 test; quoted TSV; or an extra
    CSV/HTML format)? How should keyboard-only and touch users reach a clamped cell's full value? Should the tooltip bound
    live in `TooltipLayer` (all controls) or the Grid?

Tests and CI:

20. **Q20.** Test policy:
    - Should the non-interactive NewControls lane keep tests that move the pointer or need activation?
    - Should CI fail on capability skips beyond a per-lane baseline?
    - What is the visual-baseline policy, and is the 2%-of-pixels budget a deliberate cross-GPU allowance?
    - Would you accept a clock seam for the 500 ms focus-gain turn?
21. **Q21.** Which checks does branch protection require today (any native job before #61)? Is auto-merge used? Is the
    gallery workflow meant to push to main directly?
22. **Q22.** Benchmark gate:
    - Should it correct for multiple comparisons?
    - Should it take its decision code from the base branch?
    - Should the "noise" downgrade use the scope step's changed-path categories instead of the library fingerprint?
    - The 2026-09-27 packet calls a +7,327,744-byte dirty multiline peak "within" an envelope the contract records as
      +7,012,352: which number is right?

Tooling, API and process:

23. **Q23.** Which PowerShell do consumers use for `validate-build-matrix.ps1 -Root`? Must the consumer-interface scripts
    stay Windows PowerShell 5.1-compatible?
24. **Q24.** Should the revision 3 migration notes be amended for #29's renames, and should CI enforce the revision rule
    (for example a frozen per-revision consumer compile fixture)?
25. **Q25.** Which WIP plans of the window close now? Should `capabilities.json` list tree multi-select, grid multiline,
    ordinary-menu UIA and the slider touch halo, and move described-menu qualification out of pending?
26. **Q26.** Was the slider touch halo checked on real touch hardware? Did #62 merge before its hosted paired benchmark
    verdict (its plan still lists it)? Is the halo painting over siblings outside the slider's bounds intended?

Scoped testing (#66, #67):

27. **Q27.** Should a plain `test.ps1` that ran nothing exit 0? Does anything outside this repository (consumer
    qualification, pinned or shallow checkouts) call a plain `test.ps1`, and is the README quick start meant to run
    Affected iteration? Should Affected also select scopes when the environment or toolset changed?
28. **Q28.** Is PrePush meant to run once per profile (six times)? How does an x64-only developer satisfy ARM64 PrePush?
    Should delegation work for stacked pull requests (base other than main), and be refused on main or a detached
    HEAD?
29. **Q29.** Scope rules:
    - Are the rules that match no file placeholders for a planned split, or leftovers?
    - Is TagPicker the only control composed in `src` from another control?
    - Should non-Markdown `docs/` and `Measurements/` files select Tooling?
    - Should the foreground suites get an explicit NOT_RUN obligation when their sources change?
30. **Q30.** Should a run that recorded capability skips be reusable? Do developer consoles use UTF-8 output? Should
    `DXUI_GRAPH_PERF` or `DXUI_PERF_JSONL_PATH` runs go through `Test-Changes.ps1`?
31. **Q31.** CI behavior:
    - On the hosted runners, does `DxUi.InteractiveLease.exe --self-test` skip its confirmation and warning proofs?
    - Are self-hosted runners used or planned?
    - After a manual cancel or a `windows-tooling` timeout, does GitHub report `validation` as skipped or as cancelled?
    - Is `windows-tooling` itself required? (See also Q21.)

## Recommended answers

Revised by the lead reviewer on 2026-10-09 after checking the current implementation, owning contracts and Microsoft
documentation. This section supersedes the 6 October proposals and was accepted by the user on 9 October.
"Locally corrected" refers to this uncommitted feature branch:
the fixes have not landed on main.

The retained [review packet](../../../Measurements/Review-2026-10-08/README.md) records the exact closeout snapshot
before the accepted implementation. Its archived source and hashes remain immutable. The 9 October code changes
invalidate reuse of those qualification receipts; fresh builds, tests and measurements are required for the candidate.

- **Q1 — Root promotion (technical).** Support the documented descendant promotion; the lifetime/inheritance repair
  is locally corrected. Add an ownership-safe `TakeChild` API that detaches parent/host links while preserving effective
  inherited settings. Deprecate mutable owning slots through a compatible API revision. Tab pages must use
  `RemoveTab` or an explicit metadata-aware extraction API.
- **Q2 — Callback destruction (architecture).** Permit a callback to replace or retire its control tree. Keep
  lifetime checks at every external callback and callable-capture cleanup boundary, including borrowed models.
  Centralize ownership-safe mutation and dispatch; evaluate deferred physical subtree cleanup at a bounded dispatch
  boundary while invalidating logical identities immediately. Deferred cleanup does not establish model lifetime
  or make publication non-reentrant. Arbitrary raw-slot rearrangement and tab-page extraction remain unsupported
  until an explicit API contract exists. Destruction of the whole host requires a separately qualified contract.
- **Q3 — UIA reentrancy evidence (technical).** Continue treating publication and UIA clients as possible reentrancy
  boundaries. The worker-query regression proves the action-lock problem, not out-of-process message dispatch
  inside `UiaRaise*`. Measure the latter with a bounded external client and real assistive technology, after agreement
  to the desktop time, on each qualified platform. Queuing publication still needs lifetime checks when the queue drains.
- **Q4 — Accessibility publication (architecture).** Replace unconditional whole-window rebuilding with dirty,
  coalesced publication and lazy initial snapshot construction. Both first `WM_GETOBJECT` access and explicit provider
  creation must obtain a fresh coherent snapshot; later queries/events need an explicit flush/version contract.
  Embedded hosts retain explicit application-scheduled accessibility updates, with no work in composition. Retain
  paired keystroke/selection measurements before changing scheduling.
- **Q5 — Action locks and threads (technical).** Never retain the snapshot mutex across application callbacks;
  this is locally corrected. Consolidate resolve/unlock/act/revalidate operations, then evaluate a non-recursive
  lock per target. Support independent hosts on several UI threads with owner-thread mutation and immutable
  cross-thread snapshots. Do not infer that arbitrary control calls become thread-safe.
- **Q6 — Process-exit sweep (technical).** Support early and idempotent shutdown with live windows and async menus.
  Close async menus without application callbacks, recheck registry membership before each host, and avoid tearing
  down a modal loop on its current stack. The current sweep is locally corrected; consumer call-site adoption remains
  separately owned.
- **Q7 — Synchronous menu cancellation (technical).** Close on deactivation and sent `WM_CANCELMODE`, consistent
  with the promised popup behavior. Keep the deliberate bare-capture-loss recapture policy. Source is locally
  corrected; real-focus modal/resource qualification remains open.
- **Q8 — Mnemonics (product).** Match Unicode characters from `WM_CHAR`/`WM_SYSCHAR` in the active keyboard layout;
  reserve virtual keys for navigation. Explicit ampersand mnemonics take precedence. Duplicate matches cycle focus;
  a unique match invokes. Qualify alternate layouts, dead keys and IME-generated characters.
- **Q9 — Described menus (product).** Size to content within the existing 128–456 DIP range; retain an explicit fixed
  width option. Keep `Name` concise and sufficient to identify the command, including a path when it disambiguates.
  Put explanatory description in `FullDescription`/`HelpText`, avoiding duplicate speech. Preparation failure may
  fall back to a plain row only when its command, primary label and accessible identity remain valid; otherwise
  fail with a diagnostic. Keep a measured resource bound. Discover the largest real consumer list before selecting
  a row limit; do not invent one.
- **Q10 — RTL (product).** Document current mirroring limitations immediately. Implement complete mirroring per
  control under a dedicated plan: menus, Tree and Grid need layout, hit testing, navigation and mixed-script tests
  together. ProgressBar can be a smaller first change following its orientation contract. Consumer RTL demand sets
  rollout priority; do not silently promise coverage from mirrored text alone.
- **Q11 — High contrast (technical).** Use the user's system-color pairs, including selected foreground/background;
  app alert colors and translucent secondary text must not override them. Communicate severity with text or icons.
  Use Highlight/HighlightText where appropriate for slider feedback; reserve GrayText for disabled content.
  Qualify all four contrast themes, custom colors and live switching.
  See [Microsoft contrast-theme guidance](https://learn.microsoft.com/en-us/windows/apps/design/accessibility/high-contrast-themes).
- **Q12 — Grid focus (product/API).** Adopt independent focused-row identity: Ctrl+arrows move focus, Ctrl+Space
  toggles selection, and Shift uses the focused endpoint with a visible focus indicator. After deletion, focus the
  row at the old index, or the previous last row. Select that replacement when deletion removed the last selected
  item; do not undo an intentional model selection clear.
  Check consumer navigation assumptions before changing the default.
- **Q13 — Range anchor (product).** Ctrl+click and Ctrl+Space move the anchor to the touched item, whether selected
  or deselected, consistently in Tree and Grid. Focus-only Ctrl+arrows and UIA Add/Remove do not move the gesture anchor.
- **Q14 — UIA selection (technical/product).** Adding an already-selected item succeeds idempotently. Adding another
  item in a single-select container with an existing selection returns `UIA_E_INVALIDOPERATION`.
  Removal may clear an optional selection; reject removal of the last item only when the control actually reports
  `IsSelectionRequired=true`. Do not force that property true merely because selection is single.
  `SetFocus` preserves multi-selection and calls the focus delegate; selection callbacks run only for selection changes.
  `Select` changes selection/current item without activating the OS window. The exactly-one selected-item event wins
  over the >20 invalidation threshold. Disabled and retired targets return the corresponding UIA errors.
  Idempotence, disabled checks and action lifetime are locally corrected; policy changes remain proposed.
  See [Microsoft SelectionItem contract](https://learn.microsoft.com/en-us/dotnet/framework/ui-automation/implementing-the-ui-automation-selectionitem-control-pattern)
  and [Selection pattern](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingselection).
- **Q15 — Provider root identity (architecture).** Keep the attachment/window root stable when sibling controls appear
  or disappear. Do not collapse a Panel into its sole semantic child based on child count. Direct semantic roots may
  use an explicit documented role. Actual control replacement invalidates replaced peers; detach/reattach obtains
  a new attachment identity.
- **Q16 — Embedded focus events (technical).** DxUi raises its item/row-level focus events; the application supplies
  truthful OS placement focus and schedules `UpdateAccessibility`. Avoid duplicate events in consumer adapters.
  The library correction is locally implemented; audit each adapter at adoption.
- **Q17 — Text input (technical/product).** Treat each committed composition, emoji insertion or dictation phrase as
  one undo unit; preview/cancel and selection-only imports add no undo history. Code-path tests do not qualify real
  TIPs: run actual IME/emoji interaction in scrolled multiline fields with clipping, DPI and read-only transitions.
  Disable text services for password fields; `IS_PASSWORD` is unsupported and is not a security mechanism. Prevent
  clear-text disclosure through field messages, UIA or clipboard. Host `WM_GETTEXT`, `WM_GETTEXTLENGTH` and
  `WM_SETTEXT` retain window-caption semantics, as locally corrected. Edit/clipboard routing applies only to a focused,
  eligible editor. See [Microsoft InputScope](https://learn.microsoft.com/en-us/windows/win32/api/inputscope/ne-inputscope-inputscope)
  and [WM_GETTEXT](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-gettext).
- **Q18 — Message trust and capacity (technical).** Treat registered-message parameters as untrusted inputs, not
  authentication. Validate token, process, attachment/lifetime and protocol identity; bound waits and reject late
  or cancelled work. Keep raw pointers out of cross-thread transport and do not relax UIPI. Token-based provider
  transport is locally corrected. Retain bounded payload and host registries, diagnose exhaustion and stress recovery.
  Measure actual concurrent hosts/popups before changing the current 128 limits.
- **Q19 — Multiline copy and inspection (product).** Use quoted Unicode TSV for tabs, newlines and quotes, with
  round-trip tests against target consumers. Keep an explicit raw-TSV compatibility option only when adoption needs
  it; add no CSV/HTML format yet. Full values must be reachable through UIA/copy and a keyboard/touch inspection
  action; hover alone is insufficient. Put monitor/DPI-aware tooltip clipping, bounds and wrapping in shared
  `TooltipLayer`; consumer detail views remain consumer-owned.
- **Q20 — Test policy (process).** Move every real-focus/pointer fixture to the explicit interactive lane. Fail CI on
  unexpected capability skips against a reviewed per-test, per-lane allowance; accepted noninteractive skips still
  leave their qualification obligation open. Review intentional visual changes and regenerate baselines in the same
  PR. Calibrate pixel/channel tolerances on WARP and qualified native GPUs before replacing the 2% allowance.
  Accept a test-only per-host clock seam for timing, while retaining a real-time/restoration integration case.
- **Q21 — Required CI and publication (process).** Live APIs on 9 October show no required-status-check rule and
  auto-merge disabled. Add one always-running `ci-gate` that checks Windows/Linux tooling, scope classification,
  formatting, all six native/consumer profiles when needed, and required paired measurements. Treat missing,
  failed or cancelled dependencies as failures; allow native/performance skips only after positive scope classification.
  Make that aggregate required after workflow qualification and explicit authorization to change repository rules.
  Keep auto-merge disabled until those protections are effective. Publish gallery changes through a branch/PR.
  GitHub can report a skipped job as success:
  [status-check behavior](https://docs.github.com/en/pull-requests/reference/status-checks).
- **Q22 — Benchmark acceptance (technical/process).** Predeclare primary metric families, calibrate with A/A runs,
  use randomized balanced blocks and analyze independent blocks. Add multiple-comparison control with enough
  independent observations; simply applying Bonferroni to six dependent runs is not useful qualification.
  Confirm flagged regressions independently. Use a trusted, versioned base acceptance policy; changes to the judge
  must compare base/candidate decisions on identical data and receive explicit policy review.
  Paths select measurement obligations; an unchanged fingerprint alone does not prove unchanged execution.
  Noise attribution needs matching compiled dependency/toolchain/harness/fixture identity and preferably measured
  executable byte identity. The +7,012,352 median-private and +7,327,744 peak-private figures measure different
  quantities: record separate envelopes and append an erratum to the old packet's "within" claim without rewriting
  archived evidence. Current paired passes mean
  no change established, not proof of performance equivalence. These study-design recommendations apply
  [NIST randomized-block guidance](https://www.itl.nist.gov/div898/handbook/pri/section3/pri332.htm)
  to this fixture.
- **Q23 — PowerShell contract (technical/process).** Declare supported versions per consumer entry point.
  Preserve the published 5.1/7 restore contract; internal validators may require PowerShell 7 explicitly rather
  than a broad compatibility port. Normalize relative `-Root` before resolving modules and test from a different
  working directory under each promised version. Audit actual consumer invocation before changing that promise.
- **Q24 — API revision (process).** Complete revision-3 migration notes for the missed renames. Freeze representative
  consumer compile/import fixtures per revision and detect newly mandatory script parameters. Breaking source
  changes need a revision bump or a compatible alias for an agreed transition. Private class-layout changes still
  require matching headers and a rebuilt static library; they do not independently prove a source API break.
- **Q25 — Plans and capabilities (process).** Close only completed, independently qualified library scope with
  current contracts/tests; keep native-platform, interactive and consumer handoffs explicitly separate. Record
  implemented tree multi-select, multiline Grid, menu UIA and touch feedback truthfully, alongside qualification
  limits. Do not promote described menus to unrestricted production support or close this remediation plan while
  their owning gates remain open.
- **Q26 — Slider proof and overlap (technical/process).** Real touch is not qualified by synthetic input; retain that
  label until a device test is recorded. Rechecked PR #62: its paired verdict succeeded at 12:28:24 UTC on 4 October,
  before merge at 12:50:03 UTC. Keep the intentional 48-DIP touch feedback, with parent/viewport clipping and enough
  layout spacing to avoid obscuring neighboring content or menus. Record hit-test and visual-overlap behavior.
  See [PR #62](https://github.com/RedSalamanders/DxUi/pull/62)
  and [its paired run](https://github.com/RedSalamanders/DxUi/actions/runs/37201375166/job/111433592012).
- **Q27 — Affected default and empty work (process).** Keep the accepted Affected default and run `-Explain` first.
  An empty iteration may exit zero only as `NO_WORK`/`NOT_EVALUATED`, never repository success. Qualification and
  consumer commands must explicitly request/account for their full obligations and fail incomplete acceptance.
  Environment/toolset changes invalidate matching evidence; use explicit verification/PrePush to qualify a clean
  checkout in a changed environment. Audit consumer entry points rather than assuming they never call plain
  `test.ps1`. Withdraw the old unconditional-Full recommendation.
- **Q28 — PrePush matrix and delegation (process).** Provide one accounting entry point for all six profiles,
  sharing identical tooling once. Reuse only exact evidence. ARM64 cross-builds prove compilation; native runtime
  must run on ARM64 locally or in a required candidate CI gate. Pending CI remains `CI_PENDING`, and absent native
  coverage remains `NOT_RUN`, never passed. Use the real PR base/merge candidate for stacked PRs. Refuse delegation
  for dirty trees, default-branch or detached HEADs, foreign remotes, or unrequired/unverified coverage.
- **Q29 — Scope dependencies (technical).** Remove dead rules and validate every declared rule/fixture mapping.
  TagPicker is not the only composition: ComboBox feeds TagPicker, TextField feeds NumericStepper, and NumericStepper/
  TextField feed ColorPicker. Encode these actual dependencies or retain conservative widening. All non-build
  changes, including non-Markdown documentation/measurement inputs, need tooling classification. Changed foreground
  suites must yield explicit `INTERACTIVE_NOT_RUN` obligations until qualified. Most immediate rule/fan-out repairs
  are locally corrected; broadened composition validation remains an improvement.
- **Q30 — Reuse, Unicode and instrumentation (technical).** Never reuse a skip-bearing receipt. The current
  NewControls result has 12 capability skips per x64 profile and is intentionally rerun; an allowance does not
  turn it into whole-scope evidence. Decode Git as UTF-8 independently of console encoding. Both corrections are
  local. Keep performance, filtered and mutation runs distinct from ordinary qualification; include relevant
  `DXUI_*`, runtime/toolset/device/font/DPI identities without exposing secrets. Graph/performance instrumentation
  must not accidentally reuse or create an ordinary test pass.
- **Q31 — Hosted focus and cancellation (process).** Hosted lease self-tests must prove confirmation/warning behavior,
  not tolerate skips. The live repository runner inventory is empty on 9 October; organization runners or future
  self-hosted use require their own audit. Restrict a CI focus bypass to verified hosted-runner metadata, not merely
  `GITHUB_ACTIONS=true`. Self-hosted focus needs a dedicated explicitly authorized desktop or the normal lease.
  The aggregate runs despite dependency failure/cancellation and rejects missing work; qualification must not
  depend on whether GitHub labels a dependent job skipped or cancelled. Require the aggregate, not Windows tooling
  alone. Current candidate workflows do not themselves establish required merge coverage.

Recommended order: retain the proven point fixes; implement ownership/publication/action helpers with measured
lifetime behavior; settle Grid/UIA and text contracts; enforce the aggregate CI/skip/revision policy; then complete
interactive/native-platform/consumer qualification. RTL, broader copy formats and optional visual changes remain
separate scoped improvements unless an active consumer requires them.

## Historical proposed sequence (6 October)

1. **P0 point fixes** that are small and independent of the questions: `SetRoot` order, the pointer-down revalidation,
   the checkbox/group tokens, the clear button and `ImportTextInputState` liveness, closing menus before the exit sweep,
   and the timer-id and Tree first-key fixes. Each comes with an ASan test that fails at `49a9988`.
2. **Decide Q1-Q5**, then implement A1/A4/A5. These replace the remaining P0 callback items and the mutex item
   structurally rather than site by site. Measure the publish change on the complex-UI and grid-selection benchmarks
   against a retained baseline.
3. **Keyboard, Grid (A3), menu and text-input defects (P1)**, then accessibility. Exercise with a real UIA client
   (Narrator or Accessibility Insights) where the tests cannot.
4. **Visual accessibility (A7)**, with the gallery and design system regenerated through `gallery.ps1 -PublishDocs`.
5. **Tooling, tests and CI**: lease Ctrl+C, skip baseline, gate fingerprint, consumer-interface gate, and the scoped-testing
   items (an empty plain run, PrePush delegation, rule coverage, reuse identity) once Q27-Q31 are answered.
6. **Documentation and plan close-out**, then move this plan to Done.

Validation follows AGENTS.md:

- `validate.ps1` and `format.ps1 -Check`;
- `test.ps1` in x64 Debug, Release and ASan Debug, and builds in the three ARM64 configurations;
- the focus suites through `test.ps1 -Interactive` only with the person's agreement;
- paired benchmark evidence for the publish, selection and text-layout changes;
- gallery regeneration for visual changes.

Consumer adoption (RedXe, RedSalamander) stays separate.

### 9 October follow-up: focus request supersession

- Debug24 affected execution passed all 19 noninteractive scopes with no capability skips and all validators/tooling.
  Its approved desktop execution failed the modal owner timer fixture and the TSF post-commit staging fixture;
  both the original native focus-transfer test and its new nested-activation regression passed. The lease restored
  foreground, keyboard focus and pointer. This is not interactive qualification.
- Independently traced and repaired a parent `SetFocusControl` continuation that overwrote a newer focus selected
  from control callbacks or TSF `OnPopContext`. Added request-generation checks and standard-exception containment,
  with noninteractive loss/gain/host-callback and retirement regressions and a real TSF different-editor regression.
- Repaired the modal timer fixture's pre-popup safety deadline and added failure diagnostics. Repaired TSF staging
  to call activation after deliberately retiring its document; cache synchronization does not activate TSF.
- These source changes invalidate the Debug24 build attestation. Fresh affected Debug and approved desktop runs,
  then Release/ASan/ARM64 and the existing measurement/publication/consumer/CI gates, remain required.
- Debug25 built `3011d3d953fc42feb0f539ed4760520c` but its validators rejected two incorrectly shaped changelog
  fragments. Every remaining validator and tooling test ran and passed; no overall pass is claimed. The fragments
  now follow Changes/README.md. Independent audit then found before-base throwing blur and old-tree focus retirement
  gaps; both received concrete regressions before the next build.
- Debug26, build `7b8abde2d2454c8a985f94b9b7016950`, passed all 19 noninteractive scopes with zero capability skips,
  all validators and tooling. The four new focus supersession, throwing-blur, reset/successor and retirement cases
  all executed successfully. A follow-up audit found the sole text-service focus-restoration option could activate
  an inactive cached editor during reset. The option is removed and an approved desktop regression now covers reset
  and root replacement while another window holds keyboard focus. This source needs fresh qualification.
- The Menu fractional-DPI scenario is being changed to deterministic synthetic 120/168 DPI reflow; two foreground
  acquisition fixtures are being converted to fresh-window held-foreground attempts. Their previous skips remain
  incomplete acceptance. The focus/session changes do not change gallery pixels directly; public hosting guidance
  is updated, and the broader review's final visual gallery regeneration remains required.
- Debug27, build `6c5d088bc83a43838716e6fad333e1fe`, passed all 19 noninteractive scopes with zero capability skips,
  all validators and tooling. Independent review then confirmed that a host focus notification removing its editor
  cleared logical focus but left its native session/cache alive until a later prune. Immediate retirement now covers
  both destruction and returning a live child, with normal and throwing notifications. Fresh qualification is required.
- Debug29, build `3d677af56e8a4f61b6aa9dd8656f15ce`, passed all 19 noninteractive scopes with zero skips, validators,
  tooling and formatting. Its desktop run passed the nested focus-transfer, inactive-reset and four editor-removal
  cases, then failed Menu owner acquisition and foreign TSF document retention. Debug30's focused host/control/embedded
  checks passed; its desktop diagnostics proved foreign document creation/focus succeeded, then association restoration
  changed focus before the `OnPopContext` notification. Native retirement and staged rollback now preserve that external focus with generation
  and document checks. The Menu run advanced through submenu focus and timer tests, then exposed synchronous Escape
  cleanup racing its posted mnemonic; the redundant success cleanup is removed. Both failed runs restored the desktop
  and supply no whole interactive-suite qualification.
- Debug31, build `5104df53f02444b5ba19d92b2bdafcd8`, passed its focused WindowHost check. Its desktop TSF callback
  case now proves external document retention, staged text/layout reconciliation and different-editor supersession;
  process-exit Pop/refocus retirement also passed. NativeTextInput later stopped at an obsolete disabled-ancestor
  self-edit sink-echo expectation. The fixture now checks returned ACP extents, exact retained text/caret and one
  application callback with no sink echo. Independent audit found no further obsolete counter assertions.
- Debug31's Menu startup timeout coincided with an independently launched RedPrism Release build and many active
  compiler processes. Leave those processes untouched and rerun the existing deadline in a quieter window before
  classifying the result. The lease restored foreground, keyboard focus and pointer; no full interactive pass exists.
- The lead independently confirmed the application-side `TextInputServices::Clear`/`SetClient` path still allowed
  cancellation, client reads and TSF callbacks to clear or overwrite a replacement session. A separate delegated
  repair stages ownership and immutable dispatch generations, moves old resources before callbacks, shares native
  focus-association preservation, and makes Detach reject resurrection. The lead will review every change and run
  the new replacement/teardown regressions before the final configuration and measurement matrix.
- Debug32, build `f820c021afc94acfb8bea7e87059eec2`, passed all 19 noninteractive scopes with zero skips,
  validators, tooling and formatting. The lead's post-build review then identified retirement Read reattaching the
  same client before old-store cancellation. A private successor check now runs before and after Read; its native
  regression proves the replacement preview survives and ordinary later retirement still cancels once.
- Debug33's focused Embedded/EditorControls/TextField/WindowHost execution passed with zero skips; full validators
  and tooling passed. Its approved desktop run passed all six new application text-service regressions, native TSF
  foreign-focus retention and process-exit retirement. Menu and NativeTextInput still failed later, so no complete
  interactive qualification is claimed. Foreground/focus were unchanged and the lease restored the pointer.
- The lead independently confirmed both Debug33 stopping points were fixture defects: the Unicode menu test posted
  WM_CHAR to its own thread without pumping it while waiting, and the IMM replacement test assumed SetText moved
  an existing caret to the end. The local menu wait now dispatches its posted character and verifies exact closing
  command identity; the IMM test checks insertion at the retained caret and its preservation through late cancellation.
  Fresh source/build validation and complete desktop runs remain required.
- Debug34, build `5b304281346f4eb6904fe62bcff3f4f4`, passed the focused WindowHost check. The desktop run passed
  the repaired Unicode dispatch and retained-caret scenarios but stopped at two later assertions. The lead verified
  the RTL test compared shadow-expanded HWNDs instead of visible surfaces; it now checks row-edge anchoring and
  leftward preference when the work area permits it. The native public setter exposed a production defect: new text
  retained old IMM metadata. Immediate retirement now rejects late payloads until a fresh START. Independent audit
  also confirmed a notifying result callback could start a successor then have its base/preview erased by the old
  handler. Composition revisions and owned cleanup cover replacement, nested start, focus transfer and ordinary
  synchronization for result-only and continuing messages. Fresh complete qualification remains required.
  These input fixes and geometry assertions do not directly change gallery pixels; the broader visual review's
  final gallery regeneration and publication remain open.
- Debug36, build `7c213e0f6cfd44e8844545e4ca288a98`, passed all 19 noninteractive scopes with zero skips,
  all five validators, tooling and formatting. The approved desktop invocation timed out at its 120-second
  confirmation; neither Menu nor NativeTextInput started, and foreground/focus/pointer were unchanged.
  Desktop qualification remains open. The lead requested only a new timing preference and continues offscreen work.
  The final IMM follow-up now handles an active no-GCS cancellation, keeps read-string updates distinct, and tests
  retained history and dropped late payloads. The caret-layout fixture no longer sends a contradictory cancellation
  message merely to refresh geometry. The new native scenarios still require execution under the lease.
- Release36, build `2250c586bd4549448b4c87faf01cdde2`, passed focused Embedded/EditorControls/TextField/WindowHost
  execution with zero skips. The lead's continuation audit confirmed cancellation still called a virtual export
  and then restored text through its old raw control without rechecking ownership. Destruction, newer editor,
  same-editor composition and throwing-export regressions are being added before the next source freeze.
- The accessibility scheduling baseline refresh stopped before overlay because its old harness lacks the new
  acceptance policy. An explicit platform-only identity now separates that pre-overlay check from full benchmark
  provenance; the lead independently passed all seven focused tooling cases. Actual checkout verification then
  found `MSBuildAllProjects` reported different partial import lists despite equal effective compiler options.
  Both complete `/pp` expanded project texts are byte-equivalent after checkout-root and line-ending normalization,
  SHA-256 `86B95D4E8BABBD4DBF4EFF3E3FDD3E150549FC5CCBDC86C3446DA9395659A23C`. Full identity will hash those
  actual expanded inputs before the refresh is retried. No baseline overlay, timing study or performance result
  has yet been produced by this refresh attempt; the original setup receipt remains unchanged.
- Debug37, build `e0417857d2ea48eab6f607318279cdb1`, passed all 19 noninteractive scopes with zero skips,
  all five validators, the complete tooling runner and formatting. Cancellation now checks the editor lifetime,
  session, composition generation and cached owned preview after its virtual read. The lead strengthened the
  delegated regression with owned-read recovery and allocation/standard exceptions after successor focus;
  these new native desktop cases still require execution under the lease.
- Canonical x64 and ARM64 dependency restore completed with VS 18 Insiders/default MSVC `14.51.36231`.
  The prior candidate installation had no generated toolset overlay and different package ABI records despite
  equal installed headers. The pinned restore recovered the baseline's exact x64 dependency identity. The lead
  verified equal actual toolchain, dependency and environment hashes across both study roots; it did not weaken
  comparison rules or edit installed products manually. Logs and equality records are retained under `.build/logs`.
  Fresh final-profile build attestation is required after this restore.
- A lead audit then confirmed scoped build and native reuse identities omitted restored dependency inputs.
  A changed installed header/library/status could leave `-SkipBuild` eligible with stale output. The shared
  platform-specific build-input identity is being extended and exercised with unchanged-artifact regressions
  before the final matrix and scheduling study. The original archived receipts are not being relabelled.
- The shared scoped build identity now includes the actual selected-platform installed dependency closure, status,
  package inventory and ABI metadata. The lead independently passed all 34 focused cases after strengthening
  per-file mutation isolation, missing-input refusals and x64/ARM64 separation.
- Debug38 (`236b4d5c749f4e03955bdb55310fb551`), Release38 (`1228a0fb7d32492d89aadf49d6242712`) and
  ASanDebug38 (`6ed0001f07674028ab0e65bdf44c1083`) each passed all 19 noninteractive scopes with zero skips.
  Debug and Release ran all validators and tooling; ASan reused the exact Release tooling receipt and passed
  its sanitizer detection probe. ARM64 Debug (`4f1dc4afe47942a8bde6cfea4e475d00`), Release
  (`40adff6901184b4bb32d3f329e273d3a`) and ASanDebug (`cfd4570d6908491999398fc4cee0289d`) cross-builds passed.
  Native ARM64 runtime qualification remains NOT RUN.
- A lead identity audit caught the scoped-testing fixture restoring absent environment variables as present-empty
  on this PowerShell host. Production identities correctly distinguish those states. Fixture cleanup now preserves
  presence and value; the lead independently verified all 34 cases and replayed the instrumentation case with
  initially present-empty variables. The before/after environment identities match. This tooling edit requires
  fresh final receipt accounting; the previous native executable results are retained, not relabelled.
- The baseline refresh also exposed a historical tool inventory error: its preserved compilation log invoked
  Hostx86 tools, although the original inventory recorded Hostx64 binaries. The original manifest and log are
  unchanged. The refresh now records this erratum, verifies the preserved log, fully rebuilds with attested
  current tools, and still requires equal actual toolchain/dependency/harness/environment identities.
- Debug39's current Foundation check passed. Its approved Menu/NativeTextInput lease confirmation timed out
  before either suite started; foreground, keyboard focus and pointer were unchanged. Whole interactive-suite
  qualification, final visual publication, paired acceptance and consumer/CI handoff remain open.
- The baseline refresh passed after two disclosed payload-call signature bridges in its compiled copy; the archived
  implementation remains SHA-256 `E65FDAE8F9B6D1BEFD6070337167D0A03CD997AFA69813BF5D852B82C6C70268`.
  The compiled copy is `A2848B84BAFD73FD1E59945FAE56B511B848C6EF1603F14FB846D7FA62828FBD`; their diff is
  exactly the two reviewed expressions. The versioned setup manifest records those changes and compiler-host erratum.
- The completed scheduling study (`20261009T201426Z-seed-20261009`) passed all 48 serial named-test processes and
  retained 672 records across 12 randomized independent blocks. Source, binaries, dependencies, toolchain, harness,
  environment and UIA-listening state stayed stable. The lead verified every raw log hash and independently
  recomputed all six geometric effects, exact sign-flip p-values and Holm-adjusted p-values (`0.0029296875`).
  Keystroke batch total cost fell 95.92%; selection batch total cost fell 98.43%. Snapshot builds changed from
  384/128 per batch to zero during mutations and one at the first query. First-query cost rose from representative
  11.90/8.10 microseconds to 431.83/321.93 microseconds; this is a measured cost, not silently rebaselined.
- Developer advice for that tradeoff: retain coalescing and its fresh first-query assertions given the measured total
  reduction; optimize incrementally only after profiling larger-tree/query-heavy workloads with their own retained
  baselines. Scope coalescing more narrowly, or defer it, if a consumer's measured query-latency budget requires it.
  This Debug synthetic study sets no new acceptance threshold and cannot replace Release policy qualification.
- The developer explicitly accepted that measured scheduling tradeoff on 9 October: retain coalescing and record
  the increased first-query cost. The durable rationale, bounded publication/snapshot budget, freshness requirement
  and separate larger-tree/query-heavy consumer qualification now belong in the performance contract.
- Final Release gallery regeneration passed and the lead visually reviewed all six actual harness captures.
  The canonical design-system files under `Specs/DesignSystem` and the six images under `docs/gallery` are the
  repository publication. Commit and review their final changes with the rest of the project.
- Debug40b, Release40b and ASanDebug40b passed all 19 noninteractive scopes with zero skips. Debug reused only
  Foundation's exact current-source Debug39 receipt; all other native obligations executed. The ASan detection
  probe, five validators, complete tooling runner and clang-format 22.1.3 check passed. The lead independently
  recomputed all 57 receipt identities and verified every corresponding report and current executable hash;
  [the preserved record](../../../Measurements/Review-2026-10-09/NativeQualification40/README.md) gives their limits.
- The final reuse audit found no additional demonstrated defect. The validation contract now makes explicit that
  scoped receipts qualify existing attested bytes in the recorded environment; they do not hash the entire external
  compiler/SDK installation or replace fresh buildability evidence. Resolved toolchain content remains mandatory
  for paired performance studies. Desktop, consumer, paired Release policy and native ARM64 remain open.
- Git's default line-ending conversion would alter retained hashed records after checkout. Byte-preservation
  attributes now cover the sealed 8 October packet and the 9 October raw scheduling/qualification records. No native
  C++ behavior changed, but the conservative build/run identity did; Debug42, Release42 and ASanDebug42 each executed
  and passed all 19 scopes with zero skips. The lead independently verified all 57 current identities and executable
  hashes. ASan detection, validators, tooling and formatting passed. The additional noninteractive Release
  MenuTextLayoutResources fixture passed with zero skips. The
  [final archive](../../../Measurements/Review-2026-10-09/NativeQualification42/README.md) preserves the receipts;
  iteration 40 remains historical. Whole desktop suites, consumer fixtures, Release paired policy and native ARM64
  are separate unfinished gates.
- Debug43's approved desktop lease passed the complete Menu suite with zero skips, then NativeTextInput stopped
  at the eligibility-transition fixture's preview assertion. The lease restored foreground, focus and pointer.
  The confirmed setup defect is a missing START before the preview: the cancellation latch rejects late payloads
  until a fresh transaction starts. The lead corrected an initial focus diagnosis after verifying that the
  interactive runner permits activation and SetFocusControl defaults to requesting native focus. The fixture
  now also explicitly activates its window and verifies keyboard focus and the native session before START.
  The nearby hidden-host regression already has START and explicit activation. No production guard was relaxed;
  fresh desktop execution remains required. Debug44's subsequent lease confirmation timed out without starting
  a child; foreground, focus and pointer were unchanged.
- After final fixture formatting, all six canonical builds passed in iteration 45: x64 and ARM64 Debug, Release
  and ASan Debug. All five validators, complete tooling and formatting passed. The lead independently checked
  current source/build-input/artifact identities and the original build logs in the
  [post-build inventory](../../../Measurements/Review-2026-10-09/BuildQualification45/README.md). Direct build.ps1
  does not publish scoped receipts; no stale scoped build receipt was reused or relabelled. Debug45's desktop
  confirmation also timed out before any child ran, leaving foreground, focus and pointer unchanged. Remaining
  native profile obligations will be accounted against the concrete committed PR and its actual CI coverage.
- The first clean candidate, `ffc64c8`, is published as draft PR #70, with all six native CI profiles started.
  Its four relocated x64 consumer cases passed: Debug, Release, ASan Debug and ASan Debug with STL annotations
  disabled. Each checked API revision 4, rendering, twelve EXE/DLL ownership probes and ten rejected pin/build
  mismatches. Their receipts remain bound to that exact commit.
- Ubuntu CI exposed three host-path assumptions in PerformancePolicy fixtures. Windows CI also exposed the
  dual-judge fixture's hardcoded legacy version after HEAD acquired a versioned judge. The lead independently
  retrieved the failed job log, replaced fixture roots with native temporary paths and derived the immutable
  base version from its actual source. An added rejection assertion retains strict base-version verification.
  The isolated correction passed PerformancePolicy 8/8 and BenchmarkGate 44/44 on Windows; complete Windows
  validation 47 and native-filesystem Linux validation 48 passed, using independently digest-checked portable
  PowerShell 7.6.6 for Linux. An initial WSL mounted-drive attempt failed Git-object cleanup and attempted to
  pass Linux paths to an inherited Windows powershell.exe; the native-filesystem, Linux-only PATH run resolves
  those environment artifacts without changing the affected production tools. Production
  provenance checks, judge source, policy approval and thresholds are unchanged. Final CI must qualify the
  corrected committed candidate; the failed first validation run is retained as a failure.
- All three first-candidate x64 CI profiles passed the nineteen noninteractive scopes and Menu with zero skips,
  then failed the same stale caption-fallback fixture after the revised IMM cases completed. The lead verified
  the logs and the contract: HWND caption messages must reach the application window procedure. The fixture
  now checks EM_GETSEL's default edit-message fallback, separately asserts caption-message decline, and reads
  the title through the real HWND route. A noninteractive WindowHost regression covers all three caption
  messages, application overrides, default HWND behavior and independent editor text, with and without logical
  native-editor focus. Iteration 48 passed the whole noninteractive x64 Debug WindowHost suite with zero skips,
  its fresh canonical build, five validators and formatting. The lead independently recomputed its current
  build/run receipt identities and verified the executable hash; the
  [retained follow-up](../../../Measurements/Review-2026-10-09/ToolingPortability48/README.md) preserves them.
  No production caption routing or IME guard was changed; final-source CI remains required.
- First-candidate ARM64 Debug and ASan Debug likewise passed the nineteen noninteractive suites with zero
  skips, but failed the oversized-menu first-frame assertion and the application TSF activation assertion.
  The lead verified the logs and rejected an unproven shared unavailable-desktop diagnosis. The follow-up
  logs the last popup state, visibility and observed foreground on timeout, and native activation state on
  failure. Hosted foreground runs also reuse the lease's read-only desktop probe for diagnosis. Both suites
  still execute, and their failures/skips retain their existing gate meaning; no skip approval, timing-budget
  relaxation or production focus change was introduced. Those diagnostics supersede iteration 48's exact
  source identity and require fresh final-source CI qualification.
- The diagnostic C++ changes compiled in canonical x64 Debug and WindowHost passed again with zero skips.
  Complete Windows validation 49 passed after the hosted read-only-probe guard fixture was updated; all eighteen
  InteractiveMode cases still require desktop takeover to remain exclusively inside the restoring lease path.
  The fixture portability/caption correction and failure diagnostics are a focused follow-up to draft PR #70.
