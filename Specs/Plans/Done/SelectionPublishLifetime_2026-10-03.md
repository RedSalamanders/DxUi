# Selection publish lifetime

- **Status**: DONE (3 October 2026). Every guard has a regression that fails without it, the noninteractive suites pass
  in x64 Debug, Release and ASan Debug, the three ARM64 configurations build, and the paired Release set against main is
  within the noise budget. The consumer pins and the rest of the delegate audit remain separate.
- **Owner**: Tree, Grid and accessibility validation.
- **Scope**: A native accessibility publish can dispatch messages while raising its events and destroy the control
  that called it. Stop selection and input continuations when that happens, using the existing lifetime token, and
  stop a grid that its selection delegate destroyed in the paths the same review reached. Consumer pin changes and the
  rest of the application-delegate audit remain separate.

## Why

A review of the Tree and UI Automation selection work found that `Tree::SelectVisibleIndex` and `Grid::SelectRow`
checked the lifetime token after the selection delegate, then published accessibility and returned success. The
publish raises focus and selection events in-process (`RefreshWindowHostAccessibilitySnapshot`), the event raiser
already says that a UI Automation call can dispatch messages that hide, remove or replace the control, and the callers
treat a true result as a live control: a tree's press went on to expand or arm a drag, its double click to invoke, its
keys and Ctrl+A to invalidate, `Tree::NotifyDataChanged` to call the pending `OnTreeSelectionSetChanged`, and a grid's
press, keys and Ctrl+A to invalidate. The finding held for each named path.

Two independent read-only audits of `DxUi.Tree.cpp` and `DxUi.Grid.cpp` (every continuation after a delegate or a
publish, and the UI Automation callers) then found:

- `Tree::RequestRemoveVisibleItemFromSelection` returned true after a publish that destroyed the tree, unlike Select
  and AddToSelection. Fixed: it reports the destroyed tree.
- The keyboard group collapse of a grid took its lifetime token after the selection delegate (a use-after-free once
  the delegate destroyed the grid), and a press on a group header invalidated after it. Fixed.
- `Grid::NotifyDataChanged`, `SetModel` and `SetSelectionMode` published after a selection delegate that destroyed the
  grid (a use-after-free), which a checkbox toggle whose model change drops the selected row also reaches from input,
  and `Grid::ToggleCheckboxCell` invalidated after that. Fixed.
- Not changed, because every caller is safe: `Tree::RequestExpandedState` and `Tree::CollapseSelectionToItem` return
  true after their publish destroyed the tree (`ToggleExpanded` and `OnMouseUp` check the token themselves, and UI
  Automation's expand uses only the host); a tree's arrow key, after `SetFocus` of the window dispatches
  `WM_SETFOCUS` (which republishes), and a checkbox toggle's own delegate (posted rebuilds by contract) are followed
  only by `Control::Invalidate(host)`, which reads no member of the control. They remain in the wider audit.

## Checklist

- [x] Verify the review against control handlers, snapshot ownership and native event raising.
- [x] Retain current x64 Debug, Release and ASan Debug complex-UI performance baselines before implementation
  (`.build/reports/SelectionPublishBaseline-<configuration>.json`).
- [x] Add regressions that replace the root inside selection event raising, and that replace it from the selection
  delegate during a group collapse and a model change; establish that the existing code fails.
- [x] Guard the affected Tree and Grid continuations and verify every regression scenario.
- [x] Audit both controls independently and settle each finding (above).
- [x] Update the input/accessibility contract, the testing record, the header comments and the usage guidance.
- [x] Run `validate.ps1`, `format.ps1 -Check`, the noninteractive x64 Debug, Release and ASan Debug suites and
  all three ARM64 builds. Compare each performance run with its matching retained baseline.
- [x] Review the final diff, record the validation evidence and move this plan to Done.

## Documentation and gallery

This fixes control lifetime handling during accessibility event delivery and selection delegate calls. Public
signatures, normal selection semantics, layout and painting are unchanged, so gallery images and design-system
previews need no regeneration. Usage guidance describes the reentrancy boundary and the Tree requests' false result.
Native ARM64 runtime and consumer assistive-technology qualification are separate from local cross-builds;
desktop-taking suites require the person's agreement and are excluded here.

## Validation

`Tests/Support/SelectionEventInterruption.h` with a listening `UiaTest::Client` preserves real event delivery while
making root replacement deterministic at the first raised event. The Accessibility suite exercises a tree's Select,
AddToSelection and RemoveFromSelection requests, press, expander, double click, context menu, release of a click on a
multi-selection, arrow key, type-ahead, Ctrl+A, multi-select change and model reconciliation, and a grid's press,
double click, Down, Ctrl+A and keyboard group collapse, each for a control that fills its window and beside a button.
The Grid suite's `TestGridGroupCollapseSelectionDelegateReplacementStopsTheInput` and
`TestGridSelectionDelegateReplacementDuringModelChangesStopsTheGrid` replace the controls from the selection delegate.
AddressSanitizer verifies that handlers do not access the destroyed controls; request results, capture, focus,
invalidation counts and delegate counters supply the behavioral checks.

Evidence (x64 ASan Debug, logs under `.build/logs`):

- Against main's `DxUi.Tree.cpp` and `DxUi.Grid.cpp` with the new tests, 11 of the 12 new tests fail: heap-use-after-free
  in `Tree::OnMouseDown`, `Tree::NotifyDataChanged`, `Control::RequestInvalidate` (multi-select change) and
  `Control::GetHost` (grid group collapse and model changes), and failed result or invalidation checks elsewhere. The
  release of a click on a multi-selection passes there: main already checked the token around it.
- With the change every one passes, and so do the existing selection delegate and event-interruption tests.
- Each of the fourteen guards removed on its own, rebuilt and run, fails a named test (`mutants=14 survivors=0`): the
  tree's five (model reconciliation, multi-select change, `SelectVisibleIndex`, RemoveFromSelection and Ctrl+A) and
  the grid's nine (the keyboard group collapse's two, the group header press, Ctrl+A, `SelectRow`, `NotifyDataChanged`,
  `SetModel`, `SetSelectionMode` and the checkbox toggle).

Closing validation on the final tree:

- `validate.ps1` (the five validators and the tooling tests) and `format.ps1 -Check` pass.
- `test.ps1` without the suites that take the desktop: all 19 suites pass in x64 Debug and in x64 Release. In x64 ASan
  Debug 18 pass and Accessibility failed once, in `TestTreeBesideAnotherControlItemEventsReachAClientSubscribedToTheWindow`:
  the client heard a late repeat of the previous step's event (UI Automation delivers each event twice to an in-process
  client, and the repeat came after `HearSelectionEvents` waited out 500 ms of quiet). The same tests fail the same way
  against main's library (1 run in 12, a repeat of the Up step's event), on a machine whose other applications kept the
  CPU about 45% busy; the suite passed when run again. The flake is a follow-up of its own.
- ARM64 Debug, Release and ASan Debug build. They are cross-builds: native ARM64 execution stays with CI.
- Performance: against the retained baselines, x64 Debug is within the noise budget, while x64 Release (clean FPS about
  7% lower, private bytes about 2 MB higher) and ASan Debug (dirty prepare p95) asked for advice from single runs on
  that busy machine. The complex-UI scene's per-frame update (sliders, progress, `Grid::EnsureRowVisible`) reaches none
  of the changed functions. The paired Release set settles it: main `4cd200d` (a worktree with the checkout's own
  restored dependencies) against this tree, A1, B1, B2, A2 three times, six runs per side on an identical benchmark
  fixture, is within the noise budget with no regressed and no improved metric, memory included (smallest attainable
  p 0.0022), while its same-binary controls drifted by 5 to 15% (33% on clean prepare p95). Nothing was rebaselined.
