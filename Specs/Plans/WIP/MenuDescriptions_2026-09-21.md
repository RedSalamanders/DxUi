# Described native menu entries

Status: **ACTIVE**. Library scope supporting independently qualified consumer adoption.
Baseline: `78b3de389a189c7f86f611787e0489fb6d474218`.

## September 27 ordinary-menu accessibility correction

The consumer's real provider destination-menu test found that `36b2f4b` exposes no UIA children
when every entry has empty `secondaryText`. Root confirms that accessibility population and
synchronization incorrectly depend on the description-layout vector. Ordinary and described
entries need the same semantic row contract; adding artificial consumer descriptions is not a fix.
The unchanged combined Release performance baseline is retained before production edits. A focused
nonactivating regression, library correction and independent qualification are in progress. This
is a new correction, not covered by the earlier six-profile receipts or timing acceptance.

This change targets semantic rows and dispatch only. Review existing gallery pixels for unexpected
changes; the intended paint/layout contract is unchanged. Consumer pin adoption remains explicit.

The old-code plain-menu controls fail the first-child assertion; fresh candidate B4/B5 pass the
full identity, checked/disabled/noncommand, exact Invoke and teardown checks. B1/B2 collectors
copied stale old-code suite receipts after `performance.ps1` stopped their invocations before the
suite loop. Those copied receipts are explicitly excluded; actual failed performance reports and
all originals remain retained. Collectors now bind receipt completion time and executable SHA.
Independent review adds a 128-row scroll regression: it first fails the clipped-row offscreen
property, then exposes a missing snapshot update after logical focus scrolls a row into view.
The narrow native MenuItem offscreen correction and geometry-change-only snapshot refresh now
pass both the ordinary-menu and general Accessibility suites. The first broad run's unrelated
TextRange worker-start failure remains retained; the focused rerun passes. Remaining profiles,
paired final-source resources/performance and explicit consumer pin adoption remain open.

The correction adds no public API, paint/layout rule or gallery visual. Existing gallery pixels
therefore remain applicable; native CI still regenerates the normal gallery as a validation gate.
Current external evidence: `C:/RedSalamander.Perf/evidence/i26-ui/menu-plain-uia-20260927`.
The reviewed [ordinary-menu packet](../../../Measurements/MenuDescriptions/2026-09-27/ordinary-menu-accessibility/README.md)
now preserves 693 original mapped files with byte hashes, exact portable verification, all excluded
observations and the later forced-rebuild investigation. Consumer evidence stays in RedSalamander.

Local Debug and ASan Debug each pass all 18 nonactivating suites at `15be545`. Native CI
`36349046872` additionally catches an ordinary-menu first-Down regression: generic host activation
selects the first newly exposed semantic child before menu keyboard dispatch. This is a keyboard
assertion, not a sanitizer memory diagnostic (the sanitizer's deliberate detection probe succeeds).
The correction preserves an empty menu-owned selection on native focus arrival; existing selected
rows still use the host's normal focus restoration. A plain/described nonactivating regression first
fails on `15be545`, then passes with first/second Down and acknowledged focus reentry. The focused
Release MenuAccessibility/NewControls/Accessibility/WindowHost suites pass. New full-profile CI and
consumer qualification remain required. The previous native failures remain retained.

At `d45c3610`, all 18 local nonactivating suites pass in each x64 configuration. Native CI
`36349836006` passes x64 Debug and Release but ASan hits the existing 4,096-row End-to-visible
one-second limit. The identical fixture is now also reachable behind the established activation
blocker; it rejects test-process foreground acquisition or native-focus changes and prints timing
before any bound failure. External-to-external foreground changes are recorded without failing or
restoring another application's focus.
Local ASan reproduces 1,244,850 us total (1,097,268 dispatch, 8,673 pumping, 138,909 query), with
the correct last row and eight painted rows. A separately frozen temporary stage diagnostic
attributes a subsequent 990,118-us observation chiefly to navigation-snapshot construction
(766,160 us), plus old-snapshot retirement (79,122 us). This near-threshold pass does not resolve
the failure. Optimize snapshot construction and qualify the final source without those temporary
diagnostics; keep the original bounds and all semantic rows.

The forced-rebuild comparison reproduces 1,022,545 us End-to-visible on the original snapshot
builder versus 344,345 us after reserving the exact semantic-node capacity and constructing records
in place. Opening changes from 3,090,947 to 1,087,745 us; both resolve the last row and paint eight.
The pre-count follows the builder's visibility/path limits, including descendants of semantic panels
and offscreen menu rows. Independent review confirms record references end before recursion and
publication remains private until complete. Temporary stage probes are now removed; final source
must pass the three local profiles and a new six-profile native CI run before adoption.
An earlier incremental A observation is excluded: copying an older timestamp did not rebuild its
object file. Both valid comparison variants use `build.ps1 -Rebuild`; original failed and interrupted
runs and the correction remain under `menu-plain-uia-20260927/ci-d45-diagnosis`.

The source without production probes passes all 18 nonactivating suites in each x64 ASan Debug,
Release and Debug configuration (54 fresh, executable-hash-checked receipts). ASan opening is
1,114,133 us and End-to-visible 371,768 us; Release is 46,931/6,686 us. Required static validators
and formatting pass. These final local profiles are under `profiles-navigation-final`; native CI
and final combined resource/consumer acceptance still remain separate gates.

The user explicitly accepts the measured ordinary-menu live-heap cost: approximately 23 KiB for
twelve entries and 226 KiB for 128, with no corresponding retained increase in the four-cycle
fixture. The [owning resource contract](../../Core/Core_PerformanceAndResources.md#i26-ordinary-menu-accessibility-memory-tradeoff)
records this bounded decision. It does not waive the large-menu failure or native/consumer gates.
The isolated RedSalamander V9 candidate passes its real destination-menu Copy/Move output journey
and nine overlap/live-output regressions; its primary pin remains unchanged pending qualification.

## September 27 timing decision and source ablation

The user explicitly accepts the recorded grid/menu timing tradeoffs. The menu acceptance is
now bounded in [the owning performance contract](../../Core/Core_PerformanceAndResources.md#i26-described-menu-timing-tradeoff);
original flags and thresholds stay unchanged. No further timing-advice question is needed
for these recorded costs; new regressions and all other required gates remain separate.

The [six-run source-ablation packet](../../../Measurements/MenuDescriptions/2026-09-27/header-ablation/README.md)
retains A1/H1/B1/B2/H2/A2, all nine comparisons and the failed H dependency setup. A's successful
fresh build is reused only after source, fixture, log and binary checks; H/B are freshly rebuilt.
The eight fixture/tool inputs match. H adds only two debug-header fields over fb857d4; B is
the unchanged qualified 36a1c43 menu source. Clean P95 A/H rises 7.442%/9.230%, H/B changes
-3.643%/+1.595%, and A/B rises 3.528%/10.972%. Different .text hashes and C:/Z: checkout paths
prevent a causal conclusion. Root corrected the cheaper-model curator's binary binding,
source-byte handling, archive paths and control-number transcription before execution.

Next: merge the qualified grid/menu components in the existing managed combined checkout,
run the required combined qualification, and only then adopt its exact pin in RedSalamander.
The consumer is not implicitly changed by this library evidence/contract update.

The combined `codex/fileops-ui-qualified` branch merges grid `047414e` and menu `0f35bab`.
Root and a checked cheaper-model review preserve both benchmark command families, all six
outside-frame memory samples, the grid retention/heap paths and existing budgets. The merged
JSON closes each array once. The original five rectangles plus `hasBitmapIcon` aggregate prefix
is preserved in the public menu debug record; new diagnostics follow that prefix. No command
identity, focus policy or rendering decision is changed by conflict resolution. Inherited gallery
images retain their parent provenance until the combined build regenerates them. Combined native
qualification is now recorded in the
[combined September 27 packet](../../../Measurements/LocalizedAdaptiveLayout/2026-09-27/README.md):
all six native CI profiles and relocated consumers pass, with ARM64 desktop-capability skips
retained. The gallery is regenerated and reviewed. Eight matched performance runs preserve
all twelve `advice-required` comparisons and their same-variant controls; no threshold changes.
RedSalamander is testing the exact combined pin in an isolated candidate checkout, retaining
its runnable old-pin baseline. Final resource interpretation and application adoption remain open.

## September 23 process-local attribution

The [nine-phase heap diagnostic](../../../Measurements/MenuDescriptions/2026-09-23/common-heap-phases/README.md)
uses identical opt-in fixture instrumentation and clean Release rebuilds. At dirty completion
the candidate has +438,272 private bytes but -32,602 live heap bytes, with +528,384 committed
and +451,296 free heap bytes. After teardown, live heap bytes differ by -338, while the
additional commitment/free capacity persists. The fixture creates no described menu; this
pair supports allocator-capacity retention, not live per-entry layout growth. It does not
attribute all earlier samples, waive performance flags or qualify the consumer pin. Next:
separate CPU frame stages from GPU completion waits and include a same-binary control.
Diagnostic timing is excluded from acceptance; no system-wide tracing was enabled.

## September 23 frame-stage follow-up

The [opt-in stage packet](../../../Measurements/MenuDescriptions/2026-09-23/common-frame-stages/README.md)
preserves identical fixture inputs and the serial A1/A2/B1/B2/A3 sequence. All five runs pass;
all fifty rounds reconcile. Same-binary clean/dirty spreads exceed the bracketed A/B differences,
which reverse sign. Clean differences concentrate in GPU completion; dirty differences include
preparation and GPU completion. These diagnostics establish no stable timing regression and do
not pass or waive the standard paired gate. Individual diagnostic reports lack per-run clocks and
binary identities; retain the actual driver and post-build hash receipts with that explicit limit.
The owned diagnostic headers have been restored. Fresh ordinary rebuilds and the predeclared
matched crossover are now retained below; diagnostic timing remains excluded from acceptance.

## September 23 ordinary crossover

The [ordinary A1/B1/B2/A2 packet](../../../Measurements/MenuDescriptions/2026-09-23/common-crossover/README.md)
retains fresh warning/error-free Release builds, all eight identical fixture inputs, per-invocation
UTC and unchanged executable/library hashes, four successful benchmark executions, and all four
comparison failures. Clean frame P95 rises by 0.0492 and 0.0345 ms (+11.192%/+7.997%) in both A/B
pairs, while the A/A and B/B controls improve. This repeated timing flag remains unresolved.
Dirty private-memory flags occur only in pair two and also in the B/B control; these four runs do
not establish a stable candidate memory increase. Allocations and surfaces are unchanged.

At that checkpoint the user had been asked about the measured timing cost. The explicit
September 27 timing acceptance above supersedes that pending question. The accepted V11 grid
memory cost remains separate. No benchmark threshold or baseline was changed.
The process-local heap evidence and 36% lower live menu shaping storage remain independent facts;
no causal claim about the shared-scene timing follows from them.

## Contract and scope

Implement optional primary/secondary fields for native command menu entries. Both fields
wrap at the actual monitor-constrained width. Prepare their layout outside paint and reuse
it until width/DPI changes. Preserve row identity through scrolling, pointer/keyboard and
UIA invocation. Descriptions contain literal Unicode, while the primary label retains the
existing mnemonic encoding. The consumer supplies destination identity and eligibility;
DxUi contains no filesystem parsing or operation authority.

Existing one-line menus retain their layout. Described menus expose full per-entry names,
native MenuItem roles and invocation through the existing menu dispatcher. Modal tracking retains
owner focus; asynchronous tracking uses its root popup and restores the prior owner control on
dismissal. Logical navigation preserves the session's native focus target. Surviving providers must
disconnect after popup destruction. Full consumer assistive-technology qualification remains
in RedSalamander's File Operations plan.

## Execution

- [x] Retain unchanged Release baseline: Embedded, Control, NewControls pass; complex UI
  performance/resources retained in `Measurements/MenuDescriptions/2026-09-21/baseline-release`.
- [x] Implement wrapped rows, scrollbar/work-area/DPI remeasurement and accessible identity.
- [x] Verify French Unicode, repeated names, short viewports, pointer/keyboard/UIA, disabled
  entries, submenu behavior, owner focus and retained-provider teardown.
- [x] Run x64 Debug/Release/ASan nonactivating tests and all ARM64 builds.
- [ ] Complete directed input restoration across configurations and qualify paired performance/resources.
- [x] Regenerate/review documentation gallery; run skills/spec/dependency/format validators.
- [x] Run all six native configuration profiles on the sharing candidate, retaining ARM64 desktop skips.
- [ ] Resolve resource gates and record explicit consumer pin handoff/rollback.
- [ ] Update domain contracts and move this plan to Done only after all required gates pass.

## Checkpoint

**September 23: popup-local text sharing passes all six native profiles; resource qualification remains open.**
An isolated twelve-row French probe found 453,754 live bytes for separate primary/secondary
layouts, 250,346 for one formatted layout per row, and 246,338 for shared identical primary
text plus distinct secondary text. Each returned to baseline after release. The candidate
uses text/font/width sharing to preserve the existing two-field drawing and spacing contracts.
Its lookup exists only during preparation; row and command identities remain separate. Reflow
uses absolute widths, with regression assertions for every repeated row through DPI round trips.
All seventeen non-activating Release suites pass with zero skips. Directed Menu/NativeTextInput
cases pass; an initial Menu cursor mismatch is retained, and its diagnostic follow-up verifies
all three pointer restores plus final child focus/foreground/cursor. The actual menu probe
passes all 320 openings with restored focus/cursor. A clean `fb857d4` worktree
retains the exact pre-sharing production and v4 fixture for a current matched comparison.
Matched whole-menu live heap falls from 575,310 to 367,894 bytes for twelve repeated-caption
entries (36%). The first common comparison and serial ABBA retain timing flags. Clean rebuilding
produces identical benchmark code bytes, rejecting incremental code layout as an explanation.
These results do not waive common-scene flags or qualify adoption. Candidate `f72941b`
passes native CI `35831275327` in all six profiles without a retry: nineteen regular
suites per profile, zero x64 skips and nine existing ARM64 Menu desktop-capability skips
per profile. All external consumers, both annotation-disabled ASan variants and intentional
ASan detection probes pass. Both extra x64 Release resource diagnostics pass. The
[native receipts](../../../Measurements/MenuDescriptions/2026-09-23/native-ci-sharing/README.md)
retain exact source identity, skips and raw outcomes.
Full logs and probe sources: `C:/RedSalamander.Perf/evidence/i26-ui/menu-combined-layout-probe/`.
The [reviewed sharing packet](../../../Measurements/MenuDescriptions/2026-09-23/popup-text-sharing/README.md)
retains the isolated experiment and current matched menu results.
Docs/gallery review: this is storage sharing only, with unchanged fonts, field positions, row
geometry, colors and hit behavior. No new control or gallery example is needed; verify the
existing gallery hashes on the final candidate. Regeneration confirms unchanged described-menu
pixels; two animated progress bars differ, documented in the packet. Static validators pass.
Native configuration execution is complete for this exact candidate. Resource/timing
acceptance, explicitly qualified consumer handoff and consumer DPI/AT remain open; no
unchanged timing retry or consumer pin update is justified by these functional results.

**Current: local foreground resource fixture resolved; memory gate remains open.**
Both unchanged described baseline `356006a` and lifetime/reflow candidate `d461095`
pass all 320 v4 openings at equal 476-by-322 pixels / 96 DPI, with no heap errors
and verified focus/cursor restoration. These real async-menu probes require
foreground mode; their suite classification now rejects `--no-activate`. The
general activation guard is unchanged. Earlier failed probes remain invalid,
and temporary library/CBT tracing was restored before the matched measurements.
[Paired results and limitations](../../../Measurements/MenuDescriptions/2026-09-21/local-reflow-v4/README.md).
Reflow reuse reduces private deltas in this pair but leaves live described-row
storage essentially unchanged. A serial [ABBA repeat](../../../Measurements/MenuDescriptions/2026-09-21/common-reflow-abba/README.md)
retains clean private increases of 1.0 and 2.1 million bytes and timing flags;
no unchanged retry or silent rebaseline is warranted. No menu/grid resource waiver
or consumer pin adoption is established.
Native CI `35656340075` on `d461095` has now succeeded in all six profiles;
[its reviewed receipts](../../../Measurements/MenuDescriptions/2026-09-21/native-ci-lifetime/README.md)
retain the explicit ARM64 desktop skips. The opt-in
[text-layout-only diagnostic](../../../Measurements/MenuDescriptions/2026-09-21/text-layout-isolation/README.md)
passes both native Release CI and local `test.ps1`, exactly matching twelve-pair
live heap: 13,272 bytes created, 453,754 after metrics, zero after release.
CI `35659847692` now passes all six native profiles after one x64 Release job retry.
The first attempt's disclosure subscription timeout at ElementFromHandle/E_PENDING/
3,000 ms and its raw receipt remain retained; the retry classifies an intermittent
failure and does not claim a product repair. All six external consumers, both ASan
detection probes and annotation-disabled variants pass. Nine existing ARM64 Menu
desktop skips remain explicit per profile. See the
[qualified test-source receipts](../../../Measurements/MenuDescriptions/2026-09-21/native-ci-diagnostics/README.md).
Next: finish resource qualification
and product broad-order validation, then qualify explicit consumer adoption.

**Previous repair:** the independent MenuExitLifetime
process regression reproduces a baseline ASan use-after-free during CRT teardown
of an open captured async menu. The controller now marks finalization before
releasing capture or destroying members, preventing synchronous recursive deletion.
Four focused x64 ASan suites pass with zero skips.
[Baseline/candidate trace](../../../Measurements/MenuDescriptions/2026-09-21/exit-lifetime/README.md).
The added default process suite covers this boundary in every native CI profile.
This nonvisual lifetime repair leaves gallery pixels unchanged. The earlier
positioning failure and pending matrix below are superseded by the current checkpoint.


**Current 2026-09-21:** source `7ada987` completes all six native CI profiles
in run `35650904698`: eighteen regular suites per profile, zero x64 skips,
nine pre-existing ARM64 Menu desktop skips; external consumers and ASan probes
pass. [Reviewed packet](../../../Measurements/MenuDescriptions/2026-09-21/native-ci-reflow/README.md).
The six Release gallery PNGs match described baseline `356006a` byte-for-byte.
No resource waiver or consumer handoff is established by this functional CI.

Local paired measurement has exposed an intermittent window-position failure
in the retained described baseline. The native HWND remains at (0,0,1,1) while
its internal menu rectangle is (82,107,558,429); SetWindowPos reports success.
The original offscreen v2 fixture and native-monitor-anchored v3 both reproduce
it. All failed attempts remain under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-reflow-local-20260921/`.
The v3 fixture records every extent and mismatch details before rejecting the
sample. No failed/partial resource run may be analyzed as a complete comparison.
Temporary diagnostic-only edits in the separate baseline worktree trace native
positioning and downstream CBT hooks; preserve their patches, then restore them
before paired acceptance. They are not library behavior changes for adoption.

Next: resolve the positioning failure, run identical local baseline/candidate
resource fixtures, then qualify resource direction and final consumer handoff.
Production candidate reuses freshly prepared layout pairs for scrollbar width;
French DPI roundtrip assertions guard against cumulative narrowing/row growth.
The grid branch and its resource gate remain separate. No tradeoff is approved.

**Previous investigation history (superseded status retained below):**

Current resource investigation: `c1f047b` passes x64 Debug/Release/ASan native CI;
ARM64 jobs remain active. Its no-capture scaling probe passes 320 open/close cycles
at fixed 476x322 pixels / 96 DPI. `sizeof(MenuFlyoutItem)=248` and
`sizeof(Toggle)=760`; neither includes dynamic/native storage. Process deltas are
strongly non-monotonic (including negative values), so the probe does not establish
a fixed or marginal per-entry allocation cost. Preserve its raw inputs and the
earlier 1.15 MiB observation, without inferring resource approval.
The follow-up v2 adds bounded process-heap busy/free measurements, promoting the
grid diagnostic pattern into shared test support. No production behavior changes.
Local qualification waits for RedSalamander Full; native CI qualification is pending.

Current 2026-09-21: native CI on published `a474cf2` reproduces an incorrect new
focus assertion across profiles. `BeginAsyncMenuInteraction` has always activated
the root popup for keyboard dispatch; the test and its new contract prose wrongly
expected owner focus immediately after ShowAsync. The correction preserves those
existing modes, asserts stable session focus on navigation, and adds a directed
plain/described journey restoring an actual owned Edit child after dismissal.
It does not remove the UIA logical-focus or retained-provider checks. Local/native
qualification of this correction is pending; all original CI failures stay retained.

The user explicitly challenged the 1.15 MiB / twelve-entry interpretation. No
memory tradeoff has been accepted. The new `MenuResourceScaling` fixture measures
the first-description fixed activation separately from increasing described-row
counts and total entries, with fixed window dimensions and no screenshot readback.
It runs explicitly in Release x64 CI while the local RedSalamander Full run owns
the desktop/build lane. Inputs are also copied to the original-library worktree
for a later local matched comparison. Results are pending, not a resource pass.
The optional fields create two text layouts per described row; any description
also creates the whole menu's semantic tree. That source fact does not establish
the allocator cost of either component. UIA/DirectWrite/shared allocator costs
must not be labelled 100 KB of row storage by dividing a process total by twelve.

CI baseline: https://github.com/RedSalamanders/DxUi/actions/runs/35644837020
External raw logs: `C:/RedSalamander.Perf/evidence/i26-ui/menu-ci-a474cf2`.
Scaling harness work: `C:/RedSalamander.Perf/evidence/i26-ui/menu-resource-scaling-20260921`.
Production behavior and gallery inputs are unchanged by the focus-test correction
and new diagnostic; no gallery regeneration is needed for this slice. No consumer
dependency changed. Finish the new CI/local qualification and archive evidence
before accepting or publishing a qualified consumer pin.

2026-09-21: wrapped primary/secondary fields, retained layout, scrolling/DPI reflow,
native MenuItem identities and guarded queued invocation are implemented in
`codex/menu-description-layout`, worktree `Z:/src/DxUi-worktrees/i26-menu-description`.
The separate grid-overflow branch and its unresolved resource/native-runtime gates
are not included. No consumer pin changed.

All 16 non-activating suites passed in x64 Debug, Release and ASan Debug with zero
skips; the isolated ASan detection probe also passed. Subsequent focused Release V5
NewControls/Accessibility/WindowHost tests pass, including screen-reader logical focus,
described submenu navigation, exact repeated-name selection, full accessible names,
pointer hits on descriptions and stale queued invocation after cancellation/reopen.
V5 changed the UIA SetFocus path: nonactivating native MenuItem requests preserve owner
Win32 focus. Final focused Debug and ASan checks also pass, as do all three ARM64
cross-builds on the final source. Synthetic DPI
messages are explicitly not native mixed-monitor acceptance.

The five published theme sheets were regenerated through the gallery harness and
visually reviewed. The first directed Menu attempt stopped before its suite because
an unmatched foreground-wrapper performance comparison failed; it also failed
foreground/cursor restoration. This is retained as a failed attempt, not input evidence.
Do not repeat that unchanged wrapper/powershell/performance combination.

Three alternating unchanged/candidate Release pairs retain a repeated clean private
memory increase (~0.8–1.18 MiB per process median). Allocations, surface bytes and hidden
work are unchanged; the common fixture does not instantiate menus. The user requested
an explanation of the added memory before deciding on its cost. No resource waiver
has been granted. Identical benchmark-only phase sampling is now being applied to
the unchanged implementation and candidate to distinguish device/scene/warm/capture
costs. Retain both fixture hashes and all outcomes; never silently rebaseline.

The sixty-cycle same-process comparison now finds almost identical warmed private
medians (~32.8 MiB), rather than a fixed 1.1 MiB retained increment. Timing remains
unqualified (candidate tail slowdown); raw phases/rounds and limitations are in
`Measurements/MenuDescriptions/2026-09-21/README.md`. A matched-size actual menu probe
finds ~1.15 MiB extra while twelve described entries are open, plus a separate ~0.81 MiB
harness capture cost. After the user asked for attribution, these results were explained
and acceptance of **only that measured open-menu cost** was requested. No reply/waiver
is recorded yet; it would not waive the other gates.

The direct Release Menu suite now passes with zero skips and verified physical cursor,
foreground and child-focus restoration. The fixture preserves existing popup-context
placement but saves/restores physical positions only while the cursor is still at the
fixture's actual aligned position. The outer wrapper no longer forcibly overwrites
unexpected movement. Release NativeTextInput then passes functionally, but its cursor
check fails; remaining foreground runs stopped. The failed double-conversion experiment
and all restoration failures are retained. Do not repeat foreground tests until an idle
input window is available; never force cursor restoration to manufacture a pass.

Next library action: resolve the pending scoped resource direction and remaining timing/
retention attribution, finish directed Debug/ASan input and NativeTextInput restoration,
then native ARM64 CI and qualified publication/handoff. No consumer pin changes yet.
Independent RedSalamander P3 progress localization can proceed on its existing pin with
Sol High while these library gates remain open. Do not merge the separate grid branch.

Final review correction: the UIA logical-focus rule now applies to native MenuItem roles
without depending on WS_EX_NOACTIVATE (root menus do not necessarily have that style).
The described-menu tests also run at the start of the foreground Menu suite; the
nonactivating lane alone cannot prove absence of native focus transfer. Allocation failure
while constructing new semantic rows rejects the unpublished popup and discards partial
nodes through the existing unregistered-popup cleanup. Final focused checks and cross-build
logs use the `menu-root-focus-*` prefix. The prior foreground pass and memory receipts
precede this final guard correction; they remain evidence for their exact source, not a
qualification waiver for the correction. Final directed checks and paired acceptance remain open.
All nine final focused suite runs (three suites in three x64 configurations) and all three
final ARM64 cross-builds pass. Skills/spec/dependency/format validators pass. The gallery
was regenerated on the corrected source; the two changed theme images were reviewed again.
No build/test process from this task remains active at the checkpoint.
