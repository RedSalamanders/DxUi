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
