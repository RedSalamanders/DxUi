# Localized adaptive layout and interaction readiness

Status: **ACTIVE**. Planned 2026-09-19 at `b125edbf4cdd639d1cabd686f9dc3fe8aab1f374`.
Implementation is in progress. This plan owns generic capability work; RedSalamander I26 owns
application presentation, conflict policy, settings, localization and product screenshots.

## Authority and boundary

Read [controls/layout](../../UI/UI_ControlsAndLayout.md), [input/accessibility](../../UI/UI_InputAndAccessibility.md),
[window hosting](../../Rendering/Rendering_Win32Host.md), [performance](../../Core/Core_PerformanceAndResources.md),
[validation](../../Testing/Testing_Validation.md) and [documentation/gallery](../../Core/Core_Documentation.md).
Requirements are intended acceptance, not new implemented capabilities. Keep `capabilities.json`
truthful. The consumer pins `f5c7fc7403e352425b8e82b7a4da1c042d942ede`, a different input from this
checkout. Library completion never implicitly changes its pin or qualifies product behavior.

Use existing Button, Checkbox, Label, StackPanel, ScrollPanel, Grid, ThroughputGraph and host services
first. Extend canonical shared code when a generic gap is demonstrated. Do not add file-operation
policy, consumer resource IDs or settings persistence to DxUi. Synthetic French fixtures and long
Unicode values must run without the RedSalamander repository or services.

## Execution model and continuation

The user approved recurring execution through verified closeout on 2026-09-19. Use **GPT-6 Astra /
High** for L0 and shared layout/rendering, lifetime, DPI/input/UIA changes. Use **GPT-5.6 Sol / High**
for established fixtures, examples and routine documentation/wiring. Reserve **Astra / Extra High**
for a bounded high-risk review or unexplained difficult failure, then return to the normal setting.
No fixed token-saving claim is made; retain available usage/rework evidence and all quality gates.

One implementation agent operates serially with the consumer's I26 checkpoint and existing task
`01a0a940-8323-7cd0-bf53-1b687224c4cf`; do not create a second scheduler for this library plan.
Complete coherent code/test/spec slices, read only relevant source/receipts and avoid repeated
repository-wide discovery. Use supported task model overrides at slice boundaries, not merely a
model name inside a prompt. Retain source/build/fixture provenance and explicit consumer pin adoption.
Move this plan to Done only after its own gates pass. Domain specs remain canonical; I26 continues
until its separate product gates pass, then disables the shared recurring continuation.

## Source map

- Public contracts: `include/DxUi/DxUi.h` and typography/input/accessibility headers.
- Retained measurement/bounds: `src/Controls/DxUi.cpp`, `DxUi.Controls.cpp`, `DxUi.Grid.cpp`,
  `DxUi.Accessibility.cpp`, `DxUi.WindowHost.cpp`; reuse owning helpers and one rendering route.
- Tests: `Tests/Controls/DxUiTests.Controls.cpp`, `.MultilineText.cpp`, `.Grid.cpp`, `.Accessibility.cpp`,
  `.WindowHost.cpp`, `.Rendering.cpp`, and supplied-device tests under `Tests/Embedded`.
- Additions: public control catalog, `src/Controls/ControlCatalog.cpp`, populated gallery fixture,
  `docs/controls.md` and generated `docs/gallery`. A layout helper needs tests/examples, not a
  fictitious control factory entry.

## L0 — Characterization and baseline

- [x] Check drift and map each need to an existing API or demonstrated generic gap: measured wrapping
  actions, Unicode labels, checkbox/disclosure state, constrained body/actions, Grid/detail layout,
  font/DPI/validation invalidation and coherent UIA/input bounds.
- [ ] Add a bounded synthetic fixture with long French actions/paragraphs, Unicode values, checkbox,
  expandable graph, fixed actions and selected grid detail at 480/640/760 DIP and short height.
  Give it an identity/hash and run the same fixture against the unchanged library before implementation.
- [x] Retain the first independent French action-layout witness at 480/640/760 DIP, using 96/144/192/96
  pixel-rounding scales. Existing StackPanel overflows; measured action flow passes. This partial
  fixture does not complete the full scene or physical DPI qualification above.
- [ ] Retain correctness failures and common complex-UI plus targeted performance receipts, exact
  source/config/compiler/device identity, raw repetitions and allocation/resource counts.
- [x] Record the existing APIs and smallest generic extensions chosen. This is an engineering
  decision, not an unresolved product question. Do not add a control merely to match a mockup.

Exit: library-independent witnesses, a retained matched baseline and explicit implementation map.

### L0 checkpoint, 2026-09-19

**Consumer enumeration settlement hook, 2026-09-20:** I26 now exposes pane-scoped UI-thread success
and failure receipts after queued work, exact attempted folder/status, reentrant stale suppression
and safe self-unsubscription. Reused 18/18 baseline, two retained 15/18 failed iterations (moved-from
success path, then absent early-failure path) and final verified 18/18 matrix. The consumer also fixes
two C4242 initializer sites and corrects its previous zero-warning claim; this build has zero warnings.
Paired existing renderer common median/p95: 66584/66851 -> 66601/719862 us;
differing: 66590/66863 -> 66594/66799 us. The common p95 spike is retained without an assumed cause.
An unchanged-C++ confirmation also passes 18/18 and returns to common 66539/66835, differing
66548/66834 us; all six consumer code-file hashes match. This is focused Debug evidence, not final
resource/native/configuration acceptance. The full FileOperationState owner transaction, visible
inspection controls and return remain open. No library source/API/pin/visual changes; docs/gallery
review requires no regenerated images. Shared-layout publication approval, exact-main qualification
and explicit consumer adoption remain separate gates. Both plans remain ACTIVE.

**Consumer pane-selection component, 2026-09-20:** I26 adds view-lifetime/visit/generation validation
and provider-rule-aware unique item selection, including truthful pending/failed enumeration and
reentrant-owner protection. Baseline 12/12, initial 9/12 and final 18/18 native Debug receipts are
retained. The initial failure was a test read sharing conflict with the pending destination binding;
compatible native read verifies the unchanged bytes without relaxing the engine. Paired existing
renderer medians change less than 0.15%, with lower p95; added selection work is candidate-only.
No DxUi source/API/pin or rendered pixels changed; docs/gallery need no regeneration. Owner
completion/failure, live endpoint/foreground checks, visible controls and popup return remain R3's
next work. Shared action adoption still awaits publication approval and exact-main qualification.
Both plans remain ACTIVE with their remaining native/configuration/resource/AT gates unchanged.

**Consumer inspection prerequisite, 2026-09-20:** I26 now carries immutable admitted source/destination
inspection facts and checks prompt freshness on the UI thread. Baseline, candidate and a variance-driven
unchanged-source confirmation each pass 12/12 focused native Debug checks. Candidate differing-name
redraw p95 outliers are retained; confirmation returns near baseline, without claiming final performance
or resource qualification. Pane navigation, inspection actions and return-to-control are not implemented
by this slice. Consumer domain spec records that distinction. No DxUi code/API/pin or rendered pixels
changed; docs/gallery need no regeneration. The next work is the consumer completion transaction;
measured action adoption still requires publication approval, exact-main CI and explicit qualified pin.
Both plans remain ACTIVE and all remaining native/configuration/AT/resource gates remain open.

**Consumer harness follow-up, 2026-09-20:** I26 repairs the reproduced OLE stream-format persistence
failure. It retains 0/3 baseline and 0/1 diagnostic receipts, passes 6/6 focused checks and reruns the
previous twelve-case matrix at **36/36**, zero skips. A subsequent pre-replacement verification
guard and truthful failure-count metric pass all six affected checks on their final source.
Every directed repetition verifies all seven
saved byte-format clipboard contents after restoration. Unsupported media/descriptors fail before
mutation. The original 34/36 receipt is retained. Only consumer test code changed: no library code,
gallery pixels, API or pin changes. R3 pending inspection/return is the next independent slice;
measured action adoption still awaits publication approval and exact-main qualification. This is
focused Debug evidence, not final redesign/Release/Full/native-DPI/AT/resource acceptance.

**Previous consumer checkpoint, 2026-09-20:** I26 implements the identical-admitted-leaf common
heading and expanded waiting-state deduplication using the existing consumer renderer. Its focused
French UIA/count/order/pixel fixture passes 3/3; the independent regenerated 300-frame static gallery
passes 1/1 at native 144/96 DPI. The broader consumer run is 34/36: PNG clipboard snapshot retention
fails in throughput repetitions 2/3 before mutation and remains an owned-harness investigation.
Narrow actions still clip; one-item/differing-name polish and pending inspection/return remain open.
These changes do not alter DxUi source/API/capabilities/gallery or the consumer pin. Reuse library
qualification; do not repeat unchanged builds. Publication/main-CI approval and qualified adoption
remain pending, so L0/L1/L2/L3 and final consumer gates are not closed. The consumer checkpoint owns
exact raw logs, paired Debug timings, 311 captures and the next Astra High work slice.

**Consumer interaction review, 2026-09-20:** the user rejected clipped buttons/badges, repeated
waiting/name text and missing pending-item inspection. I26's normative spec now requires one common
filename heading, one state indicator, stacked folder/metadata sections, and inspect/return in the
last-active pane. All file identity, navigation, decision and persistence behavior stays in the
consumer. Its current audit inventories 26 action kinds plus menus/dialogs/window routes in 32
journeys. It retains 309 fresh French harness captures at native 144/96 DPI (48 directly reviewed),
two passing 1/1 gallery cases, and an explicitly separate HTML proposal. The broad catalog's synthetic
conflict seeds exercise unqualified fallback; they do not qualify the admitted-name design.

Generic acceptance is clarified in controls/layout and input/accessibility: measure status text
with icons/padding as well as actions; reserve sibling footer/fixed-action space before scrolling;
support one semantic heading and complete values without duplicate hidden labels; validate actual
transitions and focus recovery rather than capture/Invoke success alone. Source/API/capabilities,
consumer pin and library gallery pixels are unchanged, so no library performance or rendered pass
is inferred. Reuse retained standalone qualification. Publication/main-CI approval and explicit pin
adoption remain pending. Consumer R1 single-name/status work and R3 qualified inspection can proceed
independently; R2 shared action-flow adoption follows the normal qualified dependency route. Both
plans remain ACTIVE and retain all final native/input/AT/resource gates.
Review closeout passes all nine skills, spec/link validation and pinned formatting. Dependency
validation passes on a tracked-source export of `5537a1e` with these three document updates at
`C:/RedSalamander.Perf/evidence/i26-ui/dxui-interaction-review-export`; the ignored personal project
file is preserved. This validates source independence, not a new runtime or consumer adoption gate.

The consumer now publishes qualified transfer/delete filename and parent-location facts and renders
separate full-width rows, Incoming above Existing. The question respects each provider's filename;
unknown profiles/item identities/roots and rename schedules retain exact paths. I26 retains an
unchanged-renderer 0/3 baseline, 6/6 focused and 33/33 final Debug checks with native-144-DPI French
captures. This is consumer adapter/presentation work only; no library source/API/gallery or pin
changed. Shared measured-action adoption still awaits the pending publication/main-CI permission;
standalone qualification was not repeated. Both plans retain their remaining acceptance gates.

The consumer clipboard gate is now repaired: snapshots retain the returned storage-medium type
instead of an enumerated acceptable-media mask. An owned-object probe reproduces the original
type mismatch; native tests verify exact rich-format bytes and three guarded OLE flush/readback
cycles. Focused checks pass 6/6; the final ten-case Debug set passes 30/30, no skips/warnings/errors
or audit issues. I26 retains the original 25/27 and two test-authoring failures with full provenance.
Only consumer test code changed; library source/API/pin/gallery pixels are untouched. Qualified
publication/main-CI approval and explicit adoption remain pending; all other final gates stay open.

Latest consumer follow-up removes stale ordinary transfer paths, mini-progress and concurrency
text from active conflicts, and measures the card from its decision content/actions. The prior
policy-only progress flag did not detect the painted mini-bar. I26 now retains actual hosted-tree
absence checks, complete metadata pixels/UIA, a failing pre-change baseline, 3/3 focused passes
and a 25/27 broad Debug run with six reviewed French native-144-DPI captures. Two throughput repetitions fail the existing clipboard-preservation prerequisite (HTML Format, 0x800401D3); broad acceptance remains open. No shared code,
API, pin or library gallery pixels changed. Publication/main-CI permission and explicit qualified
pin adoption remain pending; no unchanged library requalification was repeated.

Consumer metadata closeout is committed as `b522a2bf`; this library's coordination update is
`a5712b0`. Current skills/specs/format checks pass. Read-only GitHub checks still show main at
`b125edbf4cdd639d1cabd686f9dc3fe8aab1f374` with no PR for this branch. A concrete PR description
is prepared under the consumer evidence root (`i26-ui/dxui-layout-pr-body.md`), and publication/
conditional-merge authorization is pending in the user task. Do not interpret silence as approval.
While pending, I26 can independently remove stale ordinary transfer presentation from conflicts;
no library rebuild, duplicate action-layout helper or manual dependency-pin bypass is needed.

Latest consumer step (2026-09-20) moves conflict metadata from a restricted heading-side column
to a complete measured row below each exact path, with stable separate UIA roles. A retained clean
old-renderer fixture fails 0/3 as expected; the focused candidate passes 3/3 and final nine-case
Debug set passes 27/27, zero skips/warnings/errors/audit issues. I26 retains all failed attempts,
same-fixture timings (about 66.5 ms per eight redraws) and six reviewed French native-144-DPI
before/after captures. French actions still clip at 480 DIP; complete conflict/Issues presentation,
short scrolling and final native/AT/Full/Release/performance gates remain open. No library code/API,
pin or gallery pixels changed, so no library gallery regeneration is needed for this step.
Next shared dependency is the already-qualified action-flow revision's publication/main CI and
explicit consumer pin adoption; preserve the original qualification evidence and ownership boundary.

The consumer has committed keyboard-path implementation/evidence as `b859cbc3` and qualified
the eight-case Release set in a separate short checkout, `C:/RsI26Release`, against the unchanged pin.
The first longer checkout failed Git path-length restoration; shortening the workspace resolves
that setup failure without editing generated dependency source. Release passes 24/24 with zero skips
and compiler warnings/errors. Four native-144-DPI French captures are reviewed; its original sandbox
advisory and clean post-archive audit are both retained in I26. No application was stopped to free an
output. These consumer results do not replace Full, final performance, native mixed-DPI or AT gates.

Latest consumer slice adds exact-path keyboard access through an application-owned Label adapter,
existing focus chrome/tooltip and the canonical consumer clipboard helper. The base focus callback
must be acknowledged: an initial missing-outline capture is retained and the corrected raw UIA
focus plus pixel witness passes all three repetitions. Final eight-case Debug checks pass 24/24;
the unchanged resource fixture passes 3/3 with 75 lifetimes, 4,800 redraws, balanced host/device
attachments and no hidden frames. I26 retains every failed attempt, both images and matched
resource/timing records under its `2026-09-19-path-keyboard` gallery and TestRuns. No library
source/API/pin/gallery pixels changed; no library gallery regeneration is required. Input authoring
guidance now records the existing base-callback requirement. General long-detail selection/scroll,
remaining consumer layout/Issues, Release/Full/native DPI/AT and publication/main CI/pin adoption
remain open. Before repeating library work, reconcile the already-retained full-scene/matrix/
interleaved receipts and successful clean-checkout dependency validation below. The unrelated
personal project file stays untouched. Both plans remain ACTIVE.
This documentation closeout passes all nine skills, spec/link validation and pinned formatting.
The consumer's fresh resource comparison reports baseline 32-redraw medians about 254 ms and
candidate medians about 253 ms; the initial roughly 4% increase does not reproduce. All four runs
remain retained, without claiming final performance acceptance or a new library runtime pass.

Previous consumer slice separates measured rate/ETA labels from the history plot using cached
DirectWrite layouts and existing semantic Label regions; the pinned ThroughputGraph still owns
the plot/current-rate marker. Normal French 480/640/760-DIP and synthetic long French 480-DIP
WGC captures are reviewed in I26's `2026-09-19-graph-labels` gallery. Native Debug normal UI checks
pass 24/24; corrected long-French pixels/bounds/UIA checks pass 3/3. An invisible-label candidate
and a trailing-whitespace bounds false positive remain explicitly rejected/retained. The final
build removes one test initializer warning and is clean; no canonical library code/API/pin or
library gallery pixels changed, so no library gallery regeneration is required for this slice.

I26 now retains a paired resource fixture: 75 popup lifetimes and 4,800 synchronous redraws per
run, real hidden message pumping, private/working-set/GUI/process handles/CPU totals, and host/device
attachment return on each close. All four baseline/candidate runs pass 3/3. Actual geometry is
760 x 384 DIP at native 144 DPI. First-pair 32-redraw medians are about 244 ms versus 253–256 ms;
repeat medians are 243–245 ms versus 245–246 ms on unchanged candidate source. All samples and
fixture identity are retained; the larger slowdown did not reproduce and final performance
qualification remains open. Bounded lifetimes do not qualify GPU/allocations/long soak or zero
CPU/wakeups. Consumer evidence stays in its repository and does not replace standalone L3 gates.
Next consumer work is exact-path keyboard/full detail and remaining adaptive/Issues presentation;
an isolated Release checkout may avoid the independently launched app's exact-output lock.
Both plans remain ACTIVE. Native mixed-DPI, Full, publication/main CI/explicit pin adoption and
the unchanged developer-local-settings dependency-validator blocker remain open.
Documentation closeout passes skills/specs/format. Consumer closeout passes pinned formatting,
spec inventory, all source/metric/capture identity checks, eight explicit archives (86 files)
and whole archive inventory (5,049 files). No new library performance or runtime pass is inferred.

Consumer recovery qualification now reuses `DebugSimulateDeviceLoss` from its unchanged pin.
Test-only hooks leave old consumer resources intact so ordinary rendering must reject the old
frame and replace the context. Initial Debug evidence passes 3/3; final recovery/hidden/progress
evidence passes 6/6 with no skips, zero-warning/error builds and clean runner audits. Nine final
losses recover readable French harness captures and disclosure actions; nine 359–375-ms hidden
message-pump intervals add zero hosted frames, then showing resumes normal rendering. I26 retains
both receipts, every recovery/hidden sample and digest-linked gallery provenance. No canonical
library source/API/pin or gallery pixels changed. This is bounded transition qualification, not
hardware removal, memory retention, CPU/wake-up, native DPI or full paired-performance acceptance.
Next consumer work measures resource/idle retention, then graph labels and exact-path keyboard
access. Publication/current-main CI/explicit adoption and the preserved developer-local-settings
dependency-validator blocker remain open. Both plans remain ACTIVE.

Latest consumer follow-up: the recurring missing legacy client layer now has a reproduced boundary
signature: S_OK legacy draws with post-draw occlusion and matching sizes while hosted painting
continues. RedSalamander replaced that independent HWND target with a prepared Direct2D command list
on the existing pinned WindowHost device and a consumer root that replays it beneath its controls.
No canonical library code/API/pin changed; no library gallery pixels require regeneration. The final
eight-case Debug set passes 24/24 with French covered/uncovered WGC witnesses, zero-size recovery,
paths/UIA, both native failure surfaces, conflict and completed navigation checks. Seven images and
all failures are retained in I26's `2026-09-19-single-surface` gallery/TestRuns. Obsolete consumer
three-action fixtures were aligned with its approved four-action contract, without policy changes.
The original sandbox advisory remains recorded alongside a clean post-archive audit. Prepared-frame
device-loss/hidden-idle resource qualification, matched performance, Release/native DPI, remaining
layout/input work and publication/current-main CI/explicit pin adoption stay open. Continue those
consumer gates with Astra High; both plans remain ACTIVE.
Documentation closeout passes skills/specs/format checks. `validate-dependencies.ps1` rejects the
pre-existing ignored `src/DxUi.vcxproj.user` (September 13 timestamp) as developer-local settings
in owned source. That unrelated file is preserved; dependency validation remains explicitly blocked.

Latest consumer follow-up: ordinary running/Delete/completed paths now expose exact French/Unicode
values on existing tooltip/UIA regions, including bounded in-flight rows and live replacement/removal.
The unchanged baseline lacks those values; two candidate Debug runs each pass 3/3 with real raw-provider
name/help/ID/bounds assertions. No library source/API/pin/gallery pixel changed. The repeated baseline
lost its legacy text/background while hosted controls remained, despite S_OK legacy EndDraw; the
consumer harness correctly rejected all three repetitions. A final unchanged candidate recovered.
All timing flags, compact digest-bound rows and a rejected screenshot are retained in I26's
`2026-09-19-path-access` gallery and four TestRuns archives. This intermittent composition/capture
failure remains unattributed; diagnose the existing boundary before another controlled comparison,
without weakening probes. Keyboard copy, visible full detail, hover/AT, graph-label separation,
Release/native DPI and final performance remain open. Publication/main CI and explicit pin adoption
are still pending. Both plans remain ACTIVE; library gallery regeneration is unnecessary for this
consumer-only semantic change.

Latest consumer follow-up: the custom-speed dialog now creates on its owner's monitor and measures
wrapped label/help/error height using existing hosted font APIs and the consumer's canonical helper.
Resize/validation reflow grows the client without shrinking on correction; minimized/empty sizes
cannot trigger growth. The French constrained-client witness fails before and passes in three final
Debug repetitions, including existing UIA/modal-shutdown checks. Five harness captures are reviewed.
An initial p95 increase is retained alongside paired repetitions where it did not repeat; no broad
performance or native mixed-DPI claim is made. No library API/source/pin/gallery pixels changed, so
library gallery regeneration is unnecessary. Short-work-area fallback, full locale/keyboard and
native transitions remain open, as do publication/current-main CI and explicit adoption. Both plans
remain ACTIVE; next consumer work concerns exact long paths or graph-label separation.

Latest consumer follow-up: narrow French footer actions now use cached measurements of the existing
hosted Body font instead of shrinking to 72 DIP. Pause/Resume reserves the larger label width.
Retained unchanged-fixture Debug evidence fails before and passes after at 480/640/760/480 DIP,
with active footer targets and reviewed harness captures. Both builds are warning-free; both sandbox
audits are clean. Eight-redraw medians are 66,405.5/66,561 us; native/Release/resource acceptance is
still open. This is application layout using the unchanged pin: no shared API/control or gallery
pixels changed and library gallery regeneration is unnecessary. Next consumer slice is the speed
dialog help/error overlap. General wrapping, publication/current-main CI, explicit adoption and
native mixed-DPI gates remain open. Both plans remain ACTIVE.

Latest consumer follow-up: the French discovery action now measures its hosted Body typography
instead of forcing an 84-DIP slot, with a single retained label/format cache. Independent live bounds
and text-width assertions fail before and pass after at 480/640/760/480 DIP; reviewed French harness
images show the complete label. Paired Debug eight-redraw medians are 66,430/66,422.5 us, with one
14-us cache miss. This is consumer-only geometry repair using the existing pin, not shared action-flow
adoption or Release/FPS/resource qualification. Narrow footer and long-path defects remain. The
consumer archives preserve the original sandbox advisory plus a clean post-archive audit. No DxUi
code/API/gallery pixels changed, so library gallery regeneration is unnecessary for this checkpoint.
Next consumer work is footer/path and speed-dialog witnesses; library publication/main-CI permission,
explicit pin adoption and native mixed-DPI gates remain open. Keep both plans ACTIVE.

Consumer discovery-localization follow-up: RedSalamander now loads the open-discovery counter
sentence from the active satellite and retains a failing English-under-French baseline plus passing
semantic and harness evidence. Its reviewed 480/760-DIP French captures make the remaining consumer
geometry failure more concrete: long task actions clip off the left edge and long source paths lose
substantial context. No DxUi code or consumer pin changed in that bounded repair. The next Astra High
slice should convert those observations into measured consumer assertions and adopt this plan's
qualified action-flow revision only through the supported immutable-pin workflow; publication/main-CI
permission remains pending and must not be bypassed.

Latest consumer follow-up: its shared screenshot harness now requires foreground/background pixel
witnesses for the throughput fixture, retries rejected content with bounded non-activating repaint,
and verifies absent-color rejection. Final native Debug passes three repetitions and Release one;
eight current French images are visually reviewed with readable legacy and hosted layers. Earlier
loss proved intermittent because probes alone also passed before retries. Its original cause is
unattributed; this is capture integrity qualification, not a DxUi renderer repair or full visual/UIA
acceptance. No library code/pin changed. Consumer checkpoint routes the next bounded discovery
localization repair to Sol High; shared layout/publication and physical DPI gates stay open.

Consumer throughput follow-up: RedSalamander now wires the existing pinned disclosure chrome and
per-card persistence, with focused Debug/Release state passes. Its screenshot review rejects Debug
legacy text/background loss while Release remains readable; investigate in the consumer/harness
before attributing it to this library. No library pin changes implicitly. The consumer also confirms
two remaining generic questions for L1/L2: Disclosure paints only its chevron (a labelled header
currently requires an adjacent consumer label), and ThroughputGraph current text is painted only
with a positive visible marker, suppressing zero/unknown labels. Reuse or extend the canonical
controls with independent tests if the consumer cannot meet its contract using supported primitives.
Do not treat the current state tests as full disclosure UIA, visual or mixed-DPI acceptance.

Latest continuation: final x64 Debug and Release each cover all 18 suites after the disclosure-event
correction; Menu has nine/eight foreground capability skips respectively, others zero. Release resumed
eight unfinished suites against the same binary after an interrupted execution session. Final ASan
and all three ARM64 cross-builds are already retained. See `final-debug`, `final-release`, `full-asan`
and `arm64-builds` under the measurement date. Final five-theme gallery was published and visually
reviewed: the French multiline tile now fits its 128-DIP tile without caption overlap.

The [ten alternating paired processes](../../../Measurements/LocalizedAdaptiveLayout/2026-09-19/interleaved-final/README.md)
retain every flag and do not confirm a systematic common-fixture regression; all deterministic
resource budgets are unchanged. Clean process-memory variation remains explicitly documented, not
converted to a threshold waiver or equivalence claim. Next: validate a clean immutable revision,
then explicitly adopt that pin and measure the consumer; native/product and long-run gates stay open.
The ignored personal `src/DxUi.vcxproj.user` remains untouched and requires clean-checkout dependency
validation. The consumer Checkbox implementation is committed as `df8fbc81`.

Implementation/evidence commit: `26a3d044843396ce77c347593c04fe0a6016c00f`. The managed clean checkout
`C:/Users/eric/.codex/worktrees/localized-layout-qualified/DxUi` passed dependency validation (58
historical origins, 56 owned paths, zero pending records), all nine skills and spec validation
(81 Markdown files, 20 authority files, one active plan). Original-checkout formatting also passed.
The personal project file remains intact. RedSalamander's standard updater requires green main CI;
publication of this tested branch/draft PR has been put to the user as the explicit lifecycle step.
Do not bypass the updater or claim remote restore/pin adoption before that step. Independent consumer
settings work can proceed on its existing pin while publication is pending. Both plans remain ACTIVE.

Current continuation after commits `f7fe1e4` (library plan) and consumer `9d3318382`:

- Added `ArrangeMeasuredActions` in canonical public header/controls: allocation-free ordered geometry,
  whole-control wrapping, RTL, hidden entries, and atomic invalid/capacity/overflow failure.
- Standard Buttons gain opt-in multiline paint with 12/8-DIP per-side padding; existing default/other
  variants remain unchanged. Application policy and text-measurement invalidation remain caller-owned.
- [Baseline](../../../Measurements/LocalizedAdaptiveLayout/2026-09-19/action-flow-baseline/README.md)
  retains the expected failure, unchanged library input, raw common benchmark, fixture and suite receipt.
  The fixture changed only by clang-format whitespace afterward; both fingerprints are retained.
- [Candidate evidence](../../../Measurements/LocalizedAdaptiveLayout/2026-09-19/action-flow-first-candidate/README.md):
  Release Control, Rendering and Embedded passed with zero skips. Assertions cover fit, order,
  RTL/hidden/empty actions, unchanged outputs on error, overflow, focus/keyboard/disabled behavior,
  and actual multiple-line text pixels. Targeted median: 847 -> 426 us per 24,000 layouts.
- Common performance acceptance is OPEN: first and third comparisons flagged process memory;
  the intervening repeat passed. Raw runs are retained; no rebaseline or waiver. The expanded-suite
  run records an unpaired common benchmark and establishes correctness only. Investigate variability
  using isolated retained baseline/candidate executables before pin adoption.
- Added usage/contract docs and regenerated the five-theme gallery with a French multiline tile.
  Review revealed the new 72-DIP button needs a taller gallery tile; correct that before final publish.
- Debug Control/Rendering/Embedded/Foundation/Accessibility/Animation/WindowHost passed with zero skips.
  Added disclosure ExpandCollapse state/property/events and native snapshot refresh on acknowledged
  state changes/clear. Requests are idempotent and invoke outside the snapshot mutex; callback root
  replacement is safe. Native and embedded disclosure tests pass, including caller-owned body hiding
  and focus recovery. Failed intermediate root-alias/stale-snapshot cases remain in the task logs.
- The corrected taller multiline gallery tile is built in Debug; final Release publish remains due.
  An isolated baseline worktree at `f7fe1e4` was created at
  `C:/Users/eric/.codex/worktrees/layout-performance-baseline/DxUi` to resolve the memory variability
  without changing either active checkout. Its independent Release build completed and five serial
  fresh-process measurements preserve the original compiled source/fixture identities. Unchanged
  baseline dirty private-byte medians range 28,921,856–30,588,928, exceeding the comparison's 2% band.
  Original and flagged receipts remain retained; candidate process analysis is pending.
  Logs: `C:/RedSalamander.Perf/evidence/i26-ui/dxui-layout-*` and `dxui-localized-*`.
- Skills/spec validation passes. Dependency validation still rejects personal `src/DxUi.vcxproj.user`;
  preserve it. The pre-existing `DxUi.cpp:33` alignment is corrected and the full formatting check
  passed before the latest fixture addition. Full configuration matrix remains open.
- Added an independent short-viewport scene containing French/unbroken Unicode text, a checked
  control, graph, selected Grid/detail and fixed measured actions. It exercises 480/640/760 DIP and
  logical 96/144/192/96 DPI, body scrolling, hit targets, focus/state retention and allocation-free
  clean composition. Release checks pass after servicing the documented initial discovery tick
  before asserting hidden-graph idle. This is not native mixed-DPI consumer evidence.
- Full Release and Debug runs each passed all 18 suites, with nine Menu foreground-capability skips
  and zero skips in the other suites. Their receipts are retained under `full-release`/`full-debug`.
  An additional review fixed missing native disclosure notifications and a state-only early-out in
  embedded notification detection. A real native UIA subscriber now verifies both transitions;
  targeted Debug Accessibility/Embedded pass zero skips in `disclosure-events`. Full configuration
  receipts before that correction do not qualify it. ASan Debug passed its detection probe and
  16 suites before the execution session disappeared during Accessibility at 12:18 UTC. Its partial
  log and completed receipts are retained in `full-asan`; Accessibility/WindowHost resumed against
  the same binaries and passed with zero skips. All 18 ASan suites are covered across these two
  invocations. ARM64 Debug/Release/ASan cross-builds passed (retained in `arm64-builds`); no native
  ARM64 runtime claim is made. Final Release rebuilding/tests are now running serially.
- The full scene now takes only control-configuration/layout adapters; its identical fixture hash
  is `25F4A6935D39F605D0BC2E6B589DF46E4AC158392951D5B81D12B1CD598C4C6F`. The unchanged library
  fails measured action bounds in the isolated worktree (`short-viewport-baseline`); the candidate
  passes in full Debug. This later baseline does not replace the original pre-implementation fixture.
- Five fresh candidate processes do not consistently reproduce the memory flag (+0.65% median
  dirty private bytes across process medians, overlapping ranges). Clean frame p95 and dirty CPU
  composition p95 still need interleaved timing investigation; no performance acceptance is claimed.
  Next: finish the matrix, paired analysis and gallery, then explicitly adopt a qualified immutable
  commit. Consumer Checkbox wiring uses its existing pin and passes native Toggle/Space/pointer tests.

Initial characterization used `b125edbf4cdd639d1cabd686f9dc3fe8aab1f374`, with planning-only changes. The common
Release fixture was freshly rebuilt and its five raw rounds retained in
[the baseline record](../../../Measurements/LocalizedAdaptiveLayout/2026-09-19/README.md).
This is unpaired and does not complete the targeted-fixture or candidate validation gates.

Reuse `Typography::MeasureSingleLineTextMetrics`, `MeasureSingleLineTextWidthPx`,
`MeasureWrappedTextHeightPx`, `Label::SetMultiline`, Checkbox, ScrollPanel and Grid. StackPanel's
caller-supplied extents do not provide wrapping action flow. Button currently paints single-line
text, and the generic control UIA pattern path exposes no ExpandCollapse provider for its existing
disclosure chrome. L1/L2 therefore target reusable action-flow geometry, opt-in multiline Button
measurement/paint and confirmed disclosure accessibility, preserving existing defaults.

The first consumer defect (zero-height ordinary actions) is application layout, requiring no library
API change or pin update. Its strengthened native witness and redraw batch live in RedSalamander;
they do not replace the library-independent localized fixture still unchecked above. That consumer
repair now passes its settled x64 Debug geometry/hit-target and pause/resize/resume/cancel witness,
with a matched redraw comparison and 300 French harness captures. Both current physical displays
report 144 DPI; those captures do not replace fresh 96-DPI consumer qualification. No library code
or consumer pin changed in this first slice. Continue with the unchecked localized fixture here.

The next consumer slice now publishes four eligible collision actions and Replace/Remplacer, with
focused Debug policy/native integration checks passing. Its retained `2026-09-19-conflict-actions`
capture pack under the I26 review contains 300 French harness images: at 760 DIP the four labels
fit, while at 480 DIP the equal-width row clips the longer actions. This strengthens the action-flow
failure witness; it does not replace the independent fixture or change library code/pin ownership.
Both physical displays still report 144 DPI. Next: implement the bounded L0 fixture above against
unchanged shared code before introducing reusable measured wrapping.

## L1 — Measured adaptive layout

- [ ] Implement demonstrated gaps. Input is ordered children, available bounds, typography and
  spacing; output is desired height and coherent child/clip/scroll rectangles. Wrap whole actions,
  then overlong individual labels; preserve caller-owned order/defaults.
- [ ] Invalidate on text/font/DPI/bounds/visibility/error changes and retain measurements between
  unchanged frames. Text shaping/layout stays outside clean composition.
- [ ] Support complete Unicode text and short body viewports with separate actions; preserve scroll
  anchors and visible focus. No host activation, new worker or autonomous timer.
- [ ] Test empty/one/many actions, a label wider than its viewport, unbroken Unicode values,
  repeated 96->144->96 DPI, 192-DPI logical layout, hide/removal and allocation/capacity failure.

Exit: labels fit, required controls retain positive hit targets, and paint/input/UIA geometry agrees
in native and embedded hosts.

## L2 — State and accessible interaction

- [ ] Reconcile the September 20 consumer audit with generic synthetic acceptance: complete long
  French status/action text, multi-card body/action/footer clipping, one-heading reading order and
  acknowledged focus/state after success, failure, removal and reflow. Do not import file-operation
  policy or pane navigation into library fixtures. Existing API passes are not product journey passes.

- [ ] Correct demonstrated Checkbox paint/Toggle mismatches; distinguish checked/focused/hovered/
  pressed/disabled. Model acknowledgement emits no additional user callback.
- [ ] Reuse or extend disclosure with keyboard activation and UIA ExpandCollapse; collapse removes
  child navigation/render work and returns child focus to the header.
- [ ] Qualify Grid row geometry with external wrapped detail, stable selection/identity and bounded
  virtualization. The application owns record meaning and recovery actions.
- [ ] Test Tab/Shift+Tab, Space, Enter/Escape forwarding, pointer release/capture loss, root replacement,
  hidden-focus pruning and focus after resize/DPI. Include high contrast and reduced motion.
- [ ] New concrete controls require catalog/factory, populated gallery and behavior tests together;
  extend existing controls/examples where sufficient, without duplicate implementations.

Exit: state/input/UIA coherence is proven; real consumer screen-reader, IME/touch and native ARM64
runtime remain separate evidence, never inferred from simulation.

## L3 — Qualification and consumer handoff

- [x] Run x64 Debug, Release and ASan Debug suites, including controls and embedded, and build all
  three ARM64 configurations. Record native runtime gaps honestly.
  Receipts: `final-debug`, `final-release`, `full-asan` and `arm64-builds` in
  `Measurements/LocalizedAdaptiveLayout/2026-09-19`. Interrupted same-binary suite continuations
  and Menu capability skips remain explicit in the checkpoint; no native ARM64 runtime claim.
- [ ] Renderer changes require WARP, device loss, clean/dirty/hidden and bounded resource/lifetime
  evidence. No hidden painting/ticks, clean-composition heap/layout work or unexplained growth.
- [ ] Compare paired common and targeted fixtures serially with matching compiler/config/device/DPI;
  retain five-round medians and raw samples, frame/input/preparation costs, allocations, surfaces,
  private bytes and working set. Follow current ceilings/investigation bands. Confirmed degradation
  needs measured optimization/scope/deferral advice, never silent rebaselining or relaxed budgets.
- [x] Update docs and publish/review gallery sheets with `gallery.ps1 -PublishDocs`; validate skills,
  specs, dependencies and formatting. Public examples use supported headers.
  The five-theme sheets and clean-checkout validation qualify implementation commit `26a3d044`;
  the clean-checkout path and retained personal-file boundary are recorded above.
- [ ] Hand I26 the exact validated immutable revision, receipts, migration notes and remaining gates.
  Its explicit pin update and Full product validation use the existing integration workflow. Never
  edit a second library copy inside the consumer.
- [ ] Reconcile normative/capability status, move this plan to Done and remove its active index entry
  only after its work/gates pass. Consumer I26 may remain ACTIVE; its evidence stays in its repository.

## Command routing

Run `test.ps1 -Platform x64 -Configuration <Debug|Release|'ASan Debug'>` with matching
`-PerformanceBaseline` receipts, and `build.ps1 -Platform ARM64 -Configuration <...>` for each
configuration. Run `validate-skills.ps1`, `validate-specs.ps1`, `validate-dependencies.ps1`,
`format.ps1 -Check` and `gallery.ps1 -PublishDocs` as applicable. Never edit `.build` products or
stop unrelated applications. Application screenshots exclusively use the consumer scenario harness;
notified focus is limited to its authorized interaction lane.

The initial planning change was documentation-only. Current implementation and its incomplete
qualification are recorded in the checkpoint above; baseline collection preceded shared code edits.
