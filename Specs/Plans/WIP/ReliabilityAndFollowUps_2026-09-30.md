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

- [x] 1. NativeTextInput survives a foreground thief. `TestNativeTextInputBackendActivatesTsfDocumentOnFocus` fails when
  another window takes the foreground back right after the test window activates: activation is attempted and succeeds,
  then the document is gone. A foreground log showed the desktop app retaking it 30-95 ms after each test window; three
  of six local x64 Debug runs failed, and the unchanged baseline failed the same way. Accept: ten consecutive local runs
  end in a pass or an explicit environment skip naming the foreground process, never the false failure; a genuine TSF
  activation regression still fails. Done: the harness counts the `WM_ACTIVATEAPP` (FALSE) a takeover sends and
  `RunWhileForegroundHeld` repeats a focus-and-pump sequence (five runs at most) until none arrives, so the four tests
  that pump after taking focus (of 129; an emulated thief failed exactly those) make their former assertions on a run
  that kept the foreground and skip naming the thief when none does; ten consecutive `test.ps1 -Suites NativeTextInput`
  runs passed, three of them after a real takeover it repeated (`claude.exe` twice in the TSF test, PowerToys' Mouse
  Without Borders helper once in the host-focus test).
- [x] 2. A deterministic described-menu memory check. The test walks the process heap, so on a software renderer (the
  GPU-less CI runners) it can only require half of the open memory back: the renderer's surfaces and caches swing by up
  to about 3 MB per cycle. Accept: a test-only counter of DxUi's own live described-menu resources returns to its
  pre-open value after close while a client holds row elements, on every renderer; pinning the rows fails the test.
  Done: `DebugGetContextMenuResources` counts live menu popups, described-row text layouts and menu-popup accessibility
  records, which a 200-row menu raises from 0 to 1, 400 and 200 and, with eight row elements held, closing returns to
  0, 0 and 0 on any renderer (the heap is only printed); pinning a snapshot, keeping one after close or never freeing
  a popup fails the test.
- [x] 3. The control-test runner runs single tests. Only whole suites run today, which takes minutes. Accept:
  `--test=<Name>[,<Name>]` (and `test.ps1 -Tests`) runs only the named tests; an unknown name fails the run; unfiltered
  suites still run every test. Done: every runner registers its tests as `DXUI_RUN_TEST(TestName);` and `--test=` /
  `-Tests` runs only the named ones (one Menu test in 4.3 s inside `test.ps1`), an unknown name exits 2, and the 17
  unfiltered suites start the same 1,006 tests in the same order as their runners did at `cf6722f`.

- [x] 12. The Menu suite never hangs a CI job. On PR 30's pull-request run (36715415304), x64 ASan Debug started the
  Menu suite at 12:38 and printed nothing more until the job's 40-minute limit cancelled it; the push run of the same
  commit passed the job in 11 minutes, and the PR changed no C++. Accept: the hanging wait is identified (the job's
  uploaded suite log names the last test started) and bounded, so a stuck test fails fast with its name, and its root
  cause is fixed when it is in the library.
  Outcome: every control test and fixture suite without named tests runs under a condition-variable watchdog (300 s by
  default, `--test-timeout=<seconds>`, 0 off) that ends a hung run with `TIMEOUT: <name> after <N> s` and exit code 124,
  which `test.ps1` prints beside the log's last lines and a self-test proves (the same test with the watchdog off hangs), the
  audit found no unbounded polling loop, `INFINITE` wait or UI Automation wait without a deadline but a driver thread that
  gave up before it found its popup left `ContextMenu::Show` running for good and a UI Automation client thread was joined
  after a bounded wait (both fixed, the Menu suite's foreground run pending), and the wait that hung PR 30's job is not
  identified (its log is not kept), so the next hang names its test.

### Performance evidence

- [x] 4. Paired sets can establish a result on a noisy machine. One A1/B1/B2/A2 pass gives each side two runs, and on
  2026-09-30 all 14 same-binary controls on the developer laptop drifted beyond their bands. Accept: repeated
  interleaved passes and a per-metric verdict from an exact rank test together with the investigation band; exact
  budgets stay exact; the performance contract states the rule.
  Outcome: `-Repetitions` (default 3) repeats the interleaved pass and `Compare-PerformanceSet` gives each metric a
  verdict from an exact Mann-Whitney U test with the investigation band, exact budgets staying exact and same-binary
  spread only reported; the contract's paired-sets rule states it, six against six reaches p = 0.0022, and a real
  quiet-machine set remains for the close-out.
- [x] 5. Paired runs compare two trees. `performance-paired.ps1` refuses two uncommitted states on one commit, so the
  review follow-up sets were run by hand. Accept: `-BaselinePath` and `-CandidatePath` measure existing trees with the
  same harness overlay; identical sources are refused instead of identical commits.
  Outcome: `-BaselinePath` and `-CandidatePath` measure named working trees as they are, overlaying the harness and
  restoring their files afterwards, and a pair with a named tree is refused on an identical library source fingerprint
  instead of an identical commit; the selection, refusal, overlay and fingerprint logic sits in the tested
  `Tools/PairedRun.psm1`, and a real paired run on hardware remains for the close-out.

### Library follow-ups

- [ ] 6. Window-host UI Automation providers resolve their control without a tree scan. Each provider call searches the
  retained tree (`FindAccessibilityPathForTarget`), O(N) per call and O(N^2) for a client walking every element.
  Accept: resolution through an index built when a snapshot is published, with a deterministic test showing constant
  work per resolution in a large tree; behavior unchanged.
- [ ] 7. A click that activates the window announces the clicked control once. Today the system's activation focus
  event and the host's own focus change can both report it. Accept: a UI Automation client test counts one event.
- [x] 8. A reparented ColorPicker is current. The review recorded a stale arrangement after reparenting; its cached
  brushes may belong to the old host's device too. Accept: after moving between hosts or metrics, the picker matches one
  created in the new place. Done: a move announced no flow direction or density, so a picker, stepper, tab header or
  stack moved out of a right-to-left parent stayed mirrored and a tree or grid moved between densities kept its row
  metrics, and `Control::Reparent` now announces what a moved control inherits (once, in its final place, only when
  it differs) while the picker also releases its gradients and device reference when its host changes and
  `Panel::ClearChildren` skips a slot emptied through `GetChildren()`; dpi, theme and density moves already matched a
  fresh picker, and tests compare arrangements and painted windows with fresh controls and count the announcements,
  each failing without the change it covers (a tab control across windows, the menu bar and the text field pass either
  way: they invalidate on a host change or key their layouts), while the gallery is byte-identical.
- [x] 9. Grid layouts of a grid that stops painting are released. A painting grid already releases every layout its
  paint did not use, so what it keeps is its visible cells' layouts, which the frame-rate-first decision needs, plus
  reusable string storage. A hidden, detached or re-modelled grid keeps its last paint's layouts until a next paint that
  may never come. Accept: an attribution of what a painting grid holds, and release on hide, detach and model change,
  with the painting path, its allocation budget and its tests unchanged. Done on `improve/grid-layout-release`: a
  painting grid holds one value layout per cell its last paint drew, however far it has scrolled (28 in 32 entries for
  the 6x4 multiline test grid, 66 in 128 for 10x6, 44 in 64 for ten single-line rows; the tail table keeps 32 entries
  and no layout once a paint hits), 3,080 to 5,940 UTF-16 units (6 to 12 KB) of key strings and 5.1 to 13.6 KB of
  tables, small beside the layouts themselves (a Release heap walk finds about 20 KB each: 570 KB for the 6x4 grid,
  1.3 MB for 10x6, 934 KB for the single-line grid), and a grid hidden (itself, under a hidden panel, page host or
  unselected tab, or in a hidden embedded view), detached or given another model now returns all of it and its ellipsis
  sign, while a repaint and a one-row scroll lay out exactly what they did.

### Tooling

- [x] 10. One validation entry point. Accept: `validate.ps1` runs the five validators and the tooling tests, reporting
  every failure; CI and `test.ps1` run the tooling tests.
  Outcome: `validate.ps1` runs the five validators and the tooling tests, each in its own process, and reports every
  failure before it fails; CI's validation job runs it and `test.ps1` runs the tooling tests. The validators' file scans
  also skip nested git checkouts (worktrees), which had multiplied the Markdown count in a main checkout.
- [x] 11. Publishing `docs/gallery` after a merge is not manual. Accept: a manual-dispatch workflow regenerates and
  commits it on a chosen branch, with the same safeguards as the formatting workflow's apply mode.
  Outcome: the manual `Publish docs gallery` workflow regenerates the gallery natively and commits it through the
  tested `Tools/Commit-Gallery.ps1` with the formatting workflow's safeguards (dispatch only, an explicit boolean input,
  `contents: write` for that job, an ordinary push, a no-op when only `generation.json` changed); its first real run on
  hosted runners remains for the close-out.

### Close-out

- [ ] Every branch reviewed (diff, falsification, tests) and merged here; `test.ps1` in x64 Debug, Release and ASan
  Debug, the three ARM64 builds, the validators, tooling tests and `format.ps1 -Check`; paired evidence for item 9 with
  item 4's runner on a quiet machine; specs, docs and CHANGELOG; this plan moved to Done.

## Execution model

Agents work in separate worktrees on one branch per group: tests (1-3), grid (9), picker (8) and tooling (4, 5, 10,
11) in a first wave, accessibility (6, 7) after the tests branch merges. The Menu, NativeTextInput, MenuResources and
MenuResourceScaling suites need the real foreground, so only one agent runs them at a time. Agents run no benchmarks;
paired measurements are taken here once the work is merged.
