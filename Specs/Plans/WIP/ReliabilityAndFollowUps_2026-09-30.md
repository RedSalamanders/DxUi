# Test reliability, conclusive paired evidence and review follow-ups

Status: ACTIVE (2026-09-30). Library, tests and tooling; no consumer pin changes.
Base: the PowerShell tooling port (PR #30) on the merged review work (`5561b61`).
Owning contracts: [testing and validation](../../Testing/Testing_Validation.md),
[performance](../../Core/Core_PerformanceAndResources.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[controls and layout](../../UI/UI_ControlsAndLayout.md) and [documentation](../../Core/Core_Documentation.md).

The developer decided on 2026-09-30 to implement the eleven improvements found while closing the
[review follow-ups](../Done/ReviewFollowUps_2026-09-29.md), each implemented by a cheaper agent in its own worktree and
reviewed before it merges here, until this plan can move to Done.

## Execution

### Test reliability

- [ ] 1. NativeTextInput survives a foreground thief. `TestNativeTextInputBackendActivatesTsfDocumentOnFocus` fails when
  another window takes the foreground back right after the test window activates: activation is attempted and succeeds,
  then the document is gone. A foreground log showed the desktop app retaking it 30-95 ms after each test window; three
  of six local x64 Debug runs failed, and the unchanged baseline failed the same way. Accept: ten consecutive local runs
  end in a pass or an explicit environment skip naming the foreground process, never the false failure; a genuine TSF
  activation regression still fails.
- [ ] 2. A deterministic described-menu memory check. The test walks the process heap, so on a software renderer (the
  GPU-less CI runners) it can only require half of the open memory back: the renderer's surfaces and caches swing by up
  to about 3 MB per cycle. Accept: a test-only counter of DxUi's own live described-menu resources returns to its
  pre-open value after close while a client holds row elements, on every renderer; pinning the rows fails the test.
- [ ] 3. The control-test runner runs single tests. Only whole suites run today, which takes minutes. Accept:
  `--test=<Name>[,<Name>]` (and `test.ps1 -Tests`) runs only the named tests; an unknown name fails the run; unfiltered
  suites still run every test.

### Performance evidence

- [ ] 4. Paired sets can establish a result on a noisy machine. One A1/B1/B2/A2 pass gives each side two runs, and on
  2026-09-30 all 14 same-binary controls on the developer laptop drifted beyond their bands. Accept: repeated
  interleaved passes and a per-metric verdict from an exact rank test together with the investigation band; exact
  budgets stay exact; the performance contract states the rule.
- [ ] 5. Paired runs compare two trees. `performance-paired.ps1` refuses two uncommitted states on one commit, so the
  review follow-up sets were run by hand. Accept: `-BaselinePath` and `-CandidatePath` measure existing trees with the
  same harness overlay; identical sources are refused instead of identical commits.

### Library follow-ups

- [ ] 6. Window-host UI Automation providers resolve their control without a tree scan. Each provider call searches the
  retained tree (`FindAccessibilityPathForTarget`), O(N) per call and O(N^2) for a client walking every element.
  Accept: resolution through an index built when a snapshot is published, with a deterministic test showing constant
  work per resolution in a large tree; behavior unchanged.
- [ ] 7. A click that activates the window announces the clicked control once. Today the system's activation focus
  event and the host's own focus change can both report it. Accept: a UI Automation client test counts one event.
- [ ] 8. A reparented ColorPicker is current. The review recorded a stale arrangement after reparenting; its cached
  brushes may belong to the old host's device too. Accept: after moving between hosts or metrics, the picker matches one
  created in the new place.
- [ ] 9. Grid layouts of a grid that stops painting are released. A painting grid already releases every layout its
  paint did not use, so what it keeps is its visible cells' layouts, which the frame-rate-first decision needs, plus
  reusable string storage. A hidden, detached or re-modelled grid keeps its last paint's layouts until a next paint that
  may never come. Accept: an attribution of what a painting grid holds, and release on hide, detach and model change,
  with the painting path, its allocation budget and its tests unchanged.

### Tooling

- [ ] 10. One validation entry point. Accept: `validate.ps1` runs the five validators and the tooling tests, reporting
  every failure; CI and `test.ps1` run the tooling tests.
- [ ] 11. Publishing `docs/gallery` after a merge is not manual. Accept: a manual-dispatch workflow regenerates and
  commits it on a chosen branch, with the same safeguards as the formatting workflow's apply mode.

### Close-out

- [ ] Every branch reviewed (diff, falsification, tests) and merged here; `test.ps1` in x64 Debug, Release and ASan
  Debug, the three ARM64 builds, the validators, tooling tests and `format.ps1 -Check`; paired evidence for item 9 with
  item 4's runner on a quiet machine; specs, docs and CHANGELOG; this plan moved to Done.

## Execution model

Agents work in separate worktrees on one branch per group: tests (1-3), grid (9), picker (8) and tooling (4, 5, 10,
11) in a first wave, accessibility (6, 7) after the tests branch merges. The Menu, NativeTextInput, MenuResources and
MenuResourceScaling suites need the real foreground, so only one agent runs them at a time. Agents run no benchmarks;
paired measurements are taken here once the work is merged.
