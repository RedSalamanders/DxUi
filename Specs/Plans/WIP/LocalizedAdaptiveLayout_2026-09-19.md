# Localized adaptive layout and interaction readiness

Status: **ACTIVE**. Planned 2026-09-19 at `b125edbf4cdd639d1cabd686f9dc3fe8aab1f374`.
Implementation has not started. This plan owns generic capability work; RedSalamander I26 owns
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
- [ ] Retain correctness failures and common complex-UI plus targeted performance receipts, exact
  source/config/compiler/device identity, raw repetitions and allocation/resource counts.
- [ ] Record the existing APIs and smallest generic extensions chosen. This is an engineering
  decision, not an unresolved product question. Do not add a control merely to match a mockup.

Exit: library-independent witnesses, a retained matched baseline and explicit implementation map.

### L0 checkpoint, 2026-09-19

Source remains `b125edbf4cdd639d1cabd686f9dc3fe8aab1f374`, with planning-only changes. The common
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

- [ ] Run x64 Debug, Release and ASan Debug suites, including controls and embedded, and build all
  three ARM64 configurations. Record native runtime gaps honestly.
- [ ] Renderer changes require WARP, device loss, clean/dirty/hidden and bounded resource/lifetime
  evidence. No hidden painting/ticks, clean-composition heap/layout work or unexplained growth.
- [ ] Compare paired common and targeted fixtures serially with matching compiler/config/device/DPI;
  retain five-round medians and raw samples, frame/input/preparation costs, allocations, surfaces,
  private bytes and working set. Follow current ceilings/investigation bands. Confirmed degradation
  needs measured optimization/scope/deferral advice, never silent rebaselining or relaxed budgets.
- [ ] Update docs and publish/review gallery sheets with `gallery.ps1 -PublishDocs`; validate skills,
  specs, dependencies and formatting. Public examples use supported headers.
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

This planning change is documentation-only: no compiled inputs, implemented API claims, gallery
pixels or performance baseline changed. Baseline collection precedes L1, not this update.
