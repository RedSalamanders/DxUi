# Code review of the ten-day merge window (#29-#71)

## Current reconciliation — 21 October 2026

Status: ACTIVE on `ej/review-remediation-20261021`. The original review branch remains a recovery reference.
Current-main baseline: `04cfc530c412bc5c903e966a0cc9ba653937f560`; its Release WARP benchmark and exact build receipt
were retained before modifying source. Those unpaired observations alone establish no non-regression verdict.

- [x] Retain the original review and current-main source; measure the current-main fixture before implementation.
- [x] Port the authored remediation without dropping the newer RichText/Markdown, semantic UIA, preparation, Menu and maintenance contracts.
- [ ] Qualify and repair reconciliation-specific regressions, including pending rich text, source-only keyboard input, rich range actions and allocation instrumentation.
- [ ] Reconcile native inventories, contextual filenames, scope/CI identity, documentation and the Changes workflow.
- [ ] Run affected noninteractive x64 scopes in Debug/Release/ASan and ARM64 builds; retain unavailable foreground/native platform gates explicitly.
- [ ] Regenerate/review the gallery and compare matched resource/performance evidence without rebaselining.
- [ ] Review the final patch and prepare the PR with exact evidence and remaining gates.

Current repository guidance permits named exception catches only. The reconciled UIA action boundary handles
`std::exception` and maps `std::bad_alloc` to `E_OUTOFMEMORY`; it has no catch-all. The historical draft's claim
of containing an unsupported non-standard throw is superseded by this current contract. No native focus-taking
suite starts without agreement to its time. Historical validation below remains source-specific.

Status: ACTIVE: decisions accepted on 2026-10-11 and 2026-10-18 (see [Accepted decisions](#accepted-decisions-2026-10-11) and
[their sequel](#accepted-decisions-2026-10-18)); remediation in progress.
The merges after the window (#73-#84) and the remediation itself were reviewed on 2026-10-18; see
[the later merges](#review-of-the-later-merges-73-84-and-of-the-remediation-2026-10-18) and questions 11 to 14.
Date: 2026-10-10
Window: first-parent `40c6c21` (main on 2026-09-28) to `60e1cbf` (main on 2026-10-10): 43 landings, #29 to #71.
Related: [Production review and remediation](ProductionReview_2026-10-05.md). That plan statically reviewed #21-#68; its
remediation landed as #70. This review covers the whole window at `60e1cbf` and does not repeat items that plan still
tracks, except where the #70 correction is incomplete or regressed something.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[Win32 host](../../Rendering/Rendering_Win32Host.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[performance](../../Core/Core_PerformanceAndResources.md), [validation](../../Testing/Testing_Validation.md) and
[toolchain](../../Build/Build_ToolchainAndConsumption.md).
Evidence: [temporary reproduction tests](CodeReview_2026-10-10/review-repro-tests.patch). They are nonactivating and apply
cleanly to `60e1cbf` (`git apply`); they print `[REVIEW]` lines and are not meant to be committed as-is.

## Accepted decisions (2026-10-11)

The developer accepted every recommendation under [Questions](#questions-for-the-developer) except Q4, and asked for
the whole remediation ("go for all the rest").

- **Q4 changed.** Stop committing CI archives, and also purge the committed archives from Git history. The developer
  accepts that the rewrite gives every later commit a new identity, so consumer pins to rewritten commits must be
  re-pinned.
  - The purge runs once, as the last step, after the remediation lands.
  - It needs the `main` non-fast-forward rule lifted for the force push, and a backup bundle first.
  - GitHub keeps read-only `refs/pull/*` objects until GitHub Support purges them; that is reported, not assumed.
- **Q1-Q3 and Q5-Q10 accepted as written.**
- `main` moved to `ec680e8` (#72, prepared Tree accessibility) after the review. The remediation is based on it, and
  line references above stay those of `60e1cbf`.

## Accepted decisions (2026-10-18)

The developer answered Q11 to Q16.

- **Q11 accepted as written.** Raw packets move to release assets with SHA-256 manifests in git and leave history in the
  same rewrite as the zip archives; the Measurements validator gains size, count and extension limits and a
  personal-path scan in the same change.
- **Q12 accepted, with CODEOWNERS now.** The benchmark verdict and `Test-CiGate` run from a checkout of the base commit,
  with the expected base and head SHAs passed in. CODEOWNERS names `@eric-jesover` for `.github`, `Tools`, the
  performance scripts, `.agents` and `AGENTS.md`. Requiring a code-owner review in the ruleset would block every
  self-merge while there is one maintainer, so that switch stays the developer's.
- **Q13 accepted.** One agreed run of the focus-taking suites through `test.ps1 -Interactive`, which asks at the desktop
  before it starts, before the pull request merges. It also validates CR-13 and CR-20 to CR-22.
- **Q14, Q15 and Q16 accepted as written.**
- Still to be given at the time: the go-ahead for pushing and the pull request, the release-asset uploads, the ruleset
  changes and the history rewrite's force push.

## Verdict

**Not production-ready.** The window adds 18.0K and removes 4.5K lines in `src` and `include`. #70 alone adds 11.8K of
them, and it merged without an independent review and with the aggregate CI check failing.

- **Three crashes reproduced, plus a publication stall.** These are the release blockers.
  - A screen reader's `WM_GETOBJECT` dereferences a null provider (CR-1). An ordinary Grid refresh triggers it
    deterministically by stalling native UIA publication (CR-2).
  - Two use-after-free paths reachable through documented APIs (CR-3, CR-4).
- **#70 regressions in everyday interaction** (CR-5 to CR-11, CR-14). A selection callback that only enables a toolbar
  button cancels the rest of the gesture. Editors on a `PageHost` page get no IME or TSF. The Grid focus model is
  incomplete: there is no focus indicator, focus is not seeded from the selection, Ctrl+End toggles a row, and touch
  double-tap no longer activates.
- **Pre-existing defects in code this window touched,** reproduced:
  - `PageHost` content is invisible to UI Automation and unreachable by Tab.
  - Keyboard toggles never reach UI Automation.
  - AltGr letters run Ctrl shortcuts in text fields. A Polish `ą` replaces the whole text, and a Czech `@` pastes.
- **Process.** `main` has been red on every push since #70: the hosted ARM64 foreground lease refuses, so `ci-gate`
  fails. The repository tree grew from 22 MiB to 142 MiB in ten days, including 57 committed CI zip archives.

The rest is strong. All 19 noninteractive x64 Debug suites pass locally at `60e1cbf`. Most guard and lifetime repairs in
#70 hold under the reproduction attempts. The defects are concentrated in the patterns listed under
[Architecture](#architecture-and-simplification).

## How the review was made

- **Scope.** Ten area reviewers each took one slice of the diff: host core, controls/theme, UIA infrastructure, UIA
  patterns and the #69 Grid peers, Grid, Tree, menus, text input, public API/embedded/docs, and tooling/CI. They read
  `git diff 40c6c21 60e1cbf` together with the surrounding code at HEAD.
- **Verification.** The lead re-checked every finding kept here against the code at the cited lines.
  - Findings marked **[repro]** failed in a temporary nonactivating test under x64 Debug.
  - **[ASan]** means AddressSanitizer reported the fault under x64 ASan Debug.
  - **[code]** means the lead traced the path in the source.
  - **[reported]** means a reviewer executed or traced it and the lead only spot-checked it.
  - Refuted or unsupported claims were dropped.
- **Repairs probed.** For CR-1 and CR-2 the fix was tried locally: the crash disappeared and all ten existing GridPattern
  cases still passed. The probe was then reverted. No production source is changed by this plan.
- **Baseline.** `test.ps1 -SkipTooling` passed all 19 default suites in x64 Debug with zero capability skips. No
  focus-taking suite was run.
- **CI state.** Read with `gh run list` and `gh pr view`. Runs 38035785181 (`bb1abcf`) and 38049208908 (`60e1cbf`)
  failed in the ARM64 native jobs (one was cancelled) and in `ci-gate`. PRs #70 and #69 merged with `ci-gate` failing.
  The last green `main` validation run is `bea676a`.

| ID | Sev | Area | Introduced | Evidence |
|---|---|---|---|---|
| CR-1 | P0 | UIA provider, null deref on `WM_GETOBJECT` | #70 | [repro] [ASan] |
| CR-2 | P0 | Grid UIA, stale requested rows block publication | #69 | [repro] |
| CR-3 | P0 | Host, `WM_CAPTURECHANGED` use-after-free | pre-window; #70 kept | [repro] [ASan] |
| CR-4 | P0 | ScrollPanel inner pointers after `TakeChild` | #70 | [repro] [ASan] |
| CR-5 | P1 | Tree/Grid, post-callback guard aborts gestures | #70 | [repro] |
| CR-6 | P1 | PageHost content: text input, Tab, UIA | #70 (IME); pre-window (Tab, UIA) | [repro] |
| CR-7 | P1 | Grid focused row has no indicator | #70 | [code] |
| CR-8 | P1 | Grid focus not seeded from app selection | #70 | [repro] |
| CR-9 | P1 | Grid touch double-tap no longer activates | #70 | [repro] |
| CR-10 | P1 | Grid Ctrl+Home/End/PgUp/PgDn toggle rows | #70 | [repro] |
| CR-11 | P1 | Grid F1 inspection takes the Help key | #70 | [code] |
| CR-12 | P1 | Toggle/Checkbox keys never republish UIA | pre-window; prior misfix | [repro] |
| CR-13 | P1 | AltGr letters run Ctrl shortcuts | pre-window | [repro] |
| CR-14 | P1 | Non-focusable TabControl cannot remove a focused page | #70 | [repro] |
| CR-15 | P1 | `main` CI red since #70 | #70 | `gh` |
| CR-16 | P1 | Embedded `Prepare` E_ABORT never re-requests preparation | #70 | [code] |
| CR-17 | P1 | Embedded `UpdateAccessibility` terminates on model exceptions | #70 asymmetry | [code] |

## P0: crashes and memory safety

- [x] **CR-1 `WM_GETOBJECT` dereferences a null root provider whenever the first flush fails** (#70) [repro] [ASan]
  - **Done (2026-10-11).** `AcquireCanonicalRootProvider` returns the canonical root after a failed flush, and
    `ReturnWindowHostAccessibilityProvider` answers 0 only when no root exists (out of memory). Tests:
    `TestGridUiaPatternWindowAnswersAfterRequestedRowsAreDeleted` and
    `TestGridUiaPatternWindowAnswersWhenTheFirstPublicationFails` (a model that throws on first contact).
  - **Where.** `ReturnWindowHostAccessibilityProvider` runs `rootProvider.query_to(...)` with no null check
    ([Accessibility.cpp:11152](../../../src/Controls/DxUi.Accessibility.cpp#L11152)). WIL's query policy calls
    `ptr->QueryInterface`. `AcquireCanonicalRootProvider` returns null when `FlushNativeAccessibilitySnapshot` leaves the
    target dirty ([5259-5263](../../../src/Controls/DxUi.Accessibility.cpp#L5259)). Every other caller of
    `AcquireCanonicalRootProvider` checks for null.
  - **Triggers.**
    - CR-2.
    - A Tree or Grid model getter that throws `std::exception` during the first build. The flush catches it and returns E_FAIL.
    - Out of memory.
    - A getter or `WM_GETTEXT` handler that dirties the host on each of the two attempts.
  - **Reproduction.** `TestReviewGridUiaWmGetObjectAfterRequestedRowsDeleted`:
    1. Four offscreen rows are fetched through GridPattern `GetItem`.
    2. The model shrinks and calls `NotifyDataChanged`.
    3. A same-thread `SendMessageW(hwnd, WM_GETOBJECT, 0, UiaRootObjectId)` exits with 0xC0000005. ASan places the fault in
       `wil::details::default_query_policy::query_dispatch`, called from line 11155.
  - **Fix.** Return 0 when the root is null; the guard was probed. Better, return the canonical root even when the flush
    failed: the root reads the snapshot at call time, so a stale-but-coherent snapshot is a valid answer.
  - **Test.** The repro test, plus a throwing model getter on first contact.
- [x] **CR-2 A remembered GetItem row that an ordinary refresh deleted aborts the whole native publication** (#69) [repro]
  - **Done (2026-10-11).** A deleted remembered row, or one whose cells the model cannot read (a standard exception,
    including allocation failure), drops only that request; a grid or binding change still abandons the build. No
    automatic repost: a persistently failing model would otherwise spin the queue. Tests:
    `TestGridUiaPatternPublishesAfterAnOrdinaryRefreshDeletesRequestedRows` and
    `TestGridUiaPatternDropsARequestedRowItsModelCannotRead`.
  - **Where.** `AppendRequestedGridRows` treats a row that no longer exists (`! rowIndex`) as a reentrant change: it removes
    the request and returns false ([Accessibility.cpp:2429-2435](../../../src/Controls/DxUi.Accessibility.cpp#L2429)).
    - Each publish attempt therefore drops one stale request and fails.
    - The flush makes two attempts ([11417](../../../src/Controls/DxUi.Accessibility.cpp#L11417)).
    - The drain neither loops nor reposts ([11436-11446](../../../src/Controls/DxUi.Accessibility.cpp#L11436)).
  - **Consequences.**
    - The window keeps publishing the pre-deletion snapshot: the old RowCount and deleted rows, for worker-thread clients.
    - The selection and focus events of the change are lost.
    - GetItem on a valid row returns `UIA_E_ELEMENTNOTAVAILABLE` ([9419-9423](../../../src/Controls/DxUi.Accessibility.cpp#L9419)).
    - Root acquisition returns null, which then crashes through CR-1.
  - **Measured.** In `TestReviewGridUiaRequestedRowsDeletedByOrdinaryShrink`, sixteen stale requests and one pumped drain
    were followed by seven consecutive null root acquisitions.
  - **Realistic trigger.** A Narrator user moves through table cells of a long virtual list, then the list refreshes or
    filters.
  - **Fix.** `RemoveRequestedGridRow` and `continue` for a deleted row. Return false only when the grid, root or binding is
    no longer current. Probed: zero failures, all ten GridPattern cases pass.
  - **Related.**
    - A model that throws `GetCellData` for one remembered row poisons every later publish until 16 newer requests evict
      it. Wrap each request in its own try/catch and drop it.
    - When a flush ends dirty without a new mutation, repost it.
- [x] **CR-3 `WM_CAPTURECHANGED` reads the hovered control after a capture-lost callback freed it** (pre-window; #70 kept) [repro] [ASan]
  - **Done (2026-10-11).** The hover observer is resolved through the live tree after the capture-loss callback, and its
    callbacks run through `TryControlCallback`. The audit found no other host read of an observer pointer between a
    callback and the next message's prune. Test: `TestWindowHostCaptureLossCallbackCanRemoveTheHoveredControl`.
  - **Where.** `CancelCapturedControlInteraction()` runs `OnCaptureLost`, and with it application code
    ([WindowHost.cpp:3368](../../../src/Controls/DxUi.WindowHost.cpp#L3368)). Then `_hoveredControl->GetLifetimeToken()` is
    read with no prune ([3369-3372](../../../src/Controls/DxUi.WindowHost.cpp#L3369)).
  - **Reproduction.** `TestReviewCaptureChangedAfterCancelCallbackRetiresHoveredSlider`. During a Slider drag, its Cancel
    callback calls `container->ClearChildren()`, which the accepted Q2 explicitly allows. ASan reports a
    heap-use-after-free at line 3372; the memory was freed by the Slider deleting destructor.
  - **Fix.** Snapshot the hovered lifetime before cancelling, or call `PruneStaleInteractionState()` before touching hover.
    Audit every other read of host observer pointers that follows a callback in the same message.
- [x] **CR-4 `ScrollPanel` keeps raw inner hovered/captured child pointers across `Panel::TakeChild`** (#70) [repro] [ASan]
  - **Done (2026-10-11).** `TakeChild` calls the new protected `Panel::OnDescendantDetaching` on the panel and every
    enclosing panel, so a nested row is covered too. `ScrollPanel` delivers capture loss and hover end there, keeps each
    inner pointer with its lifetime and revalidates it at every use, and keeps a vanished pressed child's gesture until
    the release so that it reaches no sibling. Tests: four `TestScrollPanel*` cases in the Control suite.
  - **Where.** `TakeChild` cancels only host-level capture and focus inside the extracted subtree
    ([Controls.cpp:1281-1336](../../../src/Controls/DxUi.Controls.cpp#L1281)). `ScrollPanel` overrides only
    `ClearChildren`, so `_innerHoveredChild` and `_innerCapturedChild` still name the freed control.
    `UpdateInnerHover` reads `oldHovered->GetLifetimeToken()`
    ([9570](../../../src/Controls/DxUi.Controls.cpp#L9570), from `OnMouseMove` at 9954).
  - **Reproduction.** `TestReviewScrollPanelHoveredChildTakenThenMouseMove`: hover a child, `TakeChild(0)` and destroy it,
    then move the mouse. ASan reports a heap-use-after-free at line 9570.
  - **Realistic trigger.** A row's own Delete button inside a ScrollPanel removes the row.
  - **Fix.** Add a protected `Panel::OnChildDetached(Control&)` hook called by `TakeChild`, and implement it in `ScrollPanel`.
    It clears the inner pointers and delivers `OnCaptureLost` while the child is still attached. Revalidate both pointers
    at the top of every handler. `RadioButton::_group` has the same raw-pointer shape after extraction (plausible).

## P1: regressions and defects users will hit

- [x] **CR-5 A host-wide geometry epoch in the post-callback check cancels the rest of Tree and Grid gestures** (#70) [repro]
  - **Done (2026-10-11).** `BorrowedControlModelGuard::ResumeAfterCallback()` is the one rule after delegate callbacks,
    focus handlers and UI Automation event delivery: same control, host, model and delegate, then the new revisions
    become the baseline. Every Tree and Grid site uses it; rows are re-resolved by stable id and the Grid keyboard reveal
    recomputes from the focused row. Grid reports go through `NotifySelectionAndFocusChanges` (which now also stops on a
    throwing focus callback, and `SetSelectionMode` reports the focus it moves). A press or double-click whose focus
    handlers moved, scrolled or mirrored the control itself is still consumed (`IsSameHitTarget`), which keeps
    `TestWindowHostGridFocusMutationCannotRetargetTheCurrentClick`. `ToggleCheckboxCell` publishes the edit itself
    only when the delegate has not. Tests: `TestTreeGesturesContinueWhenACallbackChangesAnotherControl`,
    `TestTreeClickSurvivesAFocusHandlerThatChangesAnotherControl`, `TestTreeExpansionAnimatesWhenTheDelegatePublishes`
    (new diagnostics probe `Tree::DebugGetExpansionAnimationItemId`) and the three Grid counterparts; all six fail when
    the resume check is made strict again. Tree's report sequences keep their per-site exception handling until A5.
  - **Mechanism.** `BorrowedControlModelGuard::IsCurrent()` includes the host `_interactionRevision`
    ([DxUi.Internal.h:73-82](../../../src/Controls/DxUi.Internal.h#L73)). `SetEnabled`, `SetVisible`, `SetBounds`,
    `PropagateHost`, `PageHost::SetPage` and any `NotifyDataChanged` on any control of the host advance it.
    Tree and Grid use `IsCurrent()` as the "may I continue?" test after delegate callbacks. Examples:
    - Tree `SelectVisibleIndex` ([Tree.cpp:2712-2736](../../../src/Controls/DxUi.Tree.cpp#L2712)).
    - Grid `OnMouseDown` ([Grid.cpp:5138-5172](../../../src/Controls/DxUi.Grid.cpp#L5138)).
    - Grid `ApplyGroupLayout` ([1699-1717](../../../src/Controls/DxUi.Grid.cpp#L1699)).
    - Tree `ToggleExpanded` ([Tree.cpp:3036-3054](../../../src/Controls/DxUi.Tree.cpp#L3036)).
    - The UIA request methods.
    The Grid double-click path already does it correctly: it checks `IsBindingCurrent()` and rebuilds the guard
    ([5403-5409](../../../src/Controls/DxUi.Grid.cpp#L5403)).
  - **Reproduction.** A selection callback that only toggles a toolbar button's enabled state:
    - Tree Down still reports its primary callback, but `OnTreeSelectionSetChanged` and `OnTreeFocusedItemChanged` never
      arrive (`set=0 focus=0`, against `1/1` without the toggle).
    - `RequestSelectVisibleItem` returns false although the item is selected, so UIA reports a failure.
    - A Grid right-click on an unselected row selects it but shows no context menu (control case: one menu).
    - `RequestAddRowSelection` returns false although the row was added, and no focus callback arrives.
  - **Code-traced siblings.**
    - A checkbox click on an unselected row does not toggle.
    - Down at the viewport edge does not reveal.
    - `ApplyGroupLayout` stops after the first group when the delegate publishes.
    - A chevron click does not expand, and a delegate that calls `NotifyDataChanged` in `OnTreeToggleExpanded` leaves the
      expansion unanimated.
    - The tree drag is not armed.
  - **Fix.** After each external callback, require `IsBindingCurrent()`, re-resolve rows by stable id, and rebuild the
    guard before the next borrowed read. Keep strict `IsCurrent()` for geometry-derived values only. Implement this once
    in a shared commit helper (A1) rather than in roughly 20 call sites.
  - **Test.** Callbacks that call `SetEnabled`, `SetBounds`, `SetPage` and `NotifyDataChanged` on every gesture, plus
    UIA Select/Add/Remove/SetFocus.
- [x] **CR-6 `PageHost` page content is not treated as part of the tree** (#70 for text input; pre-window for Tab and UIA) [repro]
  - **Done (2026-10-11), with A3.** `IsControlInTree` is the one membership walk through logical children; the private
    copies in NativeTextInput and TextStoreACP and the duplicates `ControlBelongsToTree` (host) and
    `ControlBelongsToBranch` (controls) are gone. Tab order, the ComboBox scroll-container lookup and every UI Automation
    walker (path resolution, visibility, navigation, point hits, label association and target search) use logical
    children, and element paths index them, which leaves every Panel path unchanged. Tests:
    `TestWindowHostPageHostContentTakesTextInputAndTab` and `TestAccessibilityPageHostPageContentHasElements`; both fail
    against the old walkers. A3's O(depth) membership waits for the retirement of raw slot moves.
  - `PageHost` is a `Control`, not a `Panel`, and exposes its page only through `GetLogicalChild`. Several walkers
    descend only through `Panel::GetChildren()`: native text input, the text store, Tab order, ComboBox's scroll-panel
    lookup and the UIA builder.
  - **Text input.** #70 added the Panel-only `NativeTextInputControlBelongsToTree` to the activation gate
    ([NativeTextInput.cpp:34-57](../../../src/Controls/DxUi.NativeTextInput.cpp#L34),
    [607-612](../../../src/Controls/DxUi.NativeTextInput.cpp#L607)). A focused TextField on a page therefore gets no
    native session: no TSF document, IME, emoji panel or system caret. The WM_IME_* messages are then reported handled
    and dropped. Repro: `activeTextInput=0 nativeSession=0`, against `1/1` in a Panel.
    `TextStoreControlBelongsToTree` has the same shape ([TextStoreACP.cpp:165-188](../../../src/Controls/DxUi.TextStoreACP.cpp#L165)).
  - **Tab.** `CollectFocusableControls` recurses through Panels only
    ([WindowHost.cpp:923-942](../../../src/Controls/DxUi.WindowHost.cpp#L923)). Repro: Tab from the control before a
    page focuses the control after it, so page content is keyboard-unreachable.
  - **UI Automation.** The snapshot traversal ([Accessibility.cpp:1835](../../../src/Controls/DxUi.Accessibility.cpp#L1835),
    [2185-2204](../../../src/Controls/DxUi.Accessibility.cpp#L2185)) and `FindAccessibilityPathForTarget` are Panel-only.
    Repro: the page field has no element path, while a sibling button does.
  - **Fix.** Use one logical-children walker everywhere, which is `IsControlInTree`'s; delete the private copies (A3). The
    UIA builder must then use logical child indices in its paths. Tests: a TextField, Tab order and a UIA walk inside a
    PageHost page.
- [x] **CR-7 The independently focused Grid row is never drawn** (#70) [code]
  - **Done (2026-10-18).** `ResolveGridRowFocus` gives the focused row Tree's ring and contrast rule while keyboard focus
    is visible (always in high contrast); `GridDebugRowVisualState` reports `current`, `showFocus` and `focusArgb`. The
    design-system preview and stylesheet show the ring. Test: `TestGridFocusedRowIsDrawnAndCtrlKeysMoveOnlyTheFocus`.
  - Q12 required "a visible focus indicator".
  - `Paint` passes only `rowSelected`, `HasFocus()` and `rowHovered` to `ResolveGridRowVisuals`
    ([Grid.cpp:4012](../../../src/Controls/DxUi.Grid.cpp#L4012)), and nothing draws `_currentRowId`.
  - After Ctrl+Down the user cannot see the row that Ctrl+Space or Enter will act on. An unselected focused grid shows no
    focus at all (WCAG 2.4.7, high contrast). Tree has the equivalent indicator.
  - **Fix.** A focus ring on the current row while the grid has focus and keyboard focus is visible (always in high
    contrast), contrasted against the painted fill as Tree does. Add `showFocus` to `GridDebugRowVisualState`, and
    regenerate the gallery.
- [x] **CR-8 Grid keys act on row 0 after the application selects a row** (#70 regression) [repro]
  - **Done (2026-10-18).** `FindKeyboardStartRow` starts the keys, Enter and the keyboard context menu from the focused
    row, else the last selected row; `SetSelectedRowIds` mirrors Tree. The silent seeding in
    `ReconcileSelectionForVisibleRows` first proposed here was implemented and then removed after the self-review: it
    overrode an application's `SetFocusedRowId(nullopt)` at every refresh, raised an extra focus callback from
    `NotifyDataChanged` (a delegate that cleared the focus there recursed) and seeded the last row in model order. Test:
    `TestGridKeysStartFromAPublishedSelection`.
  - `OnKeyDown` starts from `_currentRowId` only and falls back to the first visible row
    ([Grid.cpp:5698-5710](../../../src/Controls/DxUi.Grid.cpp#L5698)). Before #70 it started from the last selected row.
  - The documented way to publish a selection, `GetSelectionModel()` plus `NotifyDataChanged` (docs/controls.md), never
    seeds focus.
  - **Repro.** Row 5 is selected programmatically. Enter then activates row 0 (in a file list, probably `..`), and Down
    selects row 1. Space, Ctrl+Space, F1 and the keyboard context menu use the same `currentRow`.
  - **Fix.** Treat a missing or hidden focus as the primary selected row. Seed `_currentRowId` silently in
    `ReconcileSelectionForVisibleRows`, and add `Grid::SetSelectedRowIds` mirroring Tree.
- [x] **CR-9 Grid touch double-tap no longer activates rows** (#70 regression) [repro]
  - **Done (2026-10-18).** The touch branch is gone; a touch double-tap activates as a double-click does (Q3). Test:
    `TestGridTouchDoubleTapActivatesTheRowLikeTheMouse`.
  - The touch branch of `OnMouseDoubleClick` selects and opens the inspection tooltip, then returns
    ([Grid.cpp:5398-5435](../../../src/Controls/DxUi.Grid.cpp#L5398)).
  - Repro: `activations=0 inspection=1` for touch, `activations=1` for mouse. A touch-only user cannot open a row.
  - See Q3.
- [x] **CR-10 Grid Ctrl+Home/End/PageUp/PageDown toggle the destination row** (#70, Q12 incomplete) [repro]
  - **Done (2026-10-18).** Ctrl with any movement key goes through `RequestFocusRow` and leaves selection and anchor.
    Test: `TestGridFocusedRowIsDrawnAndCtrlKeysMoveOnlyTheFocus`.
  - Only Up and Down take the focus-only path ([Grid.cpp:5902-5910](../../../src/Controls/DxUi.Grid.cpp#L5902)). The other
    keys reach `SelectRow(row, MK_CONTROL)`, which is Toggle and moves the anchor.
  - Repro: with row 5 selected, Ctrl+End leaves two rows selected, including the last.
  - Tree implements all six keys. Share Tree's `SelectMode` key map (A2).
- [x] **CR-11 Grid F1 is full-cell inspection, which takes the Windows Help key** (#70) [code]
  - **Done (2026-10-18), with Q2.** No key is bound: `InspectFocusedCell` (reveals the focused cell, then opens the value
    view; a cell with nothing to show changes nothing) and `IsFocusedCellClipped` let the application offer an Inspect
    command. Ctrl+Left and Ctrl+Right choose the current column (unhandled with one column), which the focused row
    outlines until the next click or a new model. Tests: `TestGridInspectsTheFocusedCellOnlyOnRequest`,
    `TestGridCtrlLeftAndRightChooseTheCurrentColumn`.
  - F1 is consumed on every path, including when nothing is inspected ([Grid.cpp:5664-5696](../../../src/Controls/DxUi.Grid.cpp#L5664)).
    Application Help (F1 in RedSalamander) never fires while a Grid has focus.
  - While the tooltip is open, the host routes Up/Down/PgUp/PgDn/Home/End to it
    ([WindowHost.cpp:3174](../../../src/Controls/DxUi.WindowHost.cpp#L3174),
    [Controls.cpp:10211-10230](../../../src/Controls/DxUi.Controls.cpp#L10211)). Arrows silently stop moving the grid,
    and the tooltip is not a UIA element.
  - The inspected column is `_activeColumn`, which no key can change, so keyboard users reach only one column. An
    offscreen focused row shows nothing.
  - The behavior is specified in UI_ControlsAndLayout but missing from docs/controls.md. See Q2.
- [x] **CR-12 Keyboard toggles of Toggle and Checkbox never reach UI Automation** (pre-window; misfix of `controls-editor-theme-2`) [repro]
  - **Done (2026-10-18).** The host republishes after a key or mnemonic that a live target handled (the mouse path's
    rule), `Checkbox::SetIndeterminate` publishes, and the snapshot carries the Indeterminate toggle state. Test:
    `TestAccessibilityKeyboardToggleStateReachesUiAutomation`.
  - `Toggle::ApplyCheckedState` does not mark the snapshot dirty ([Controls.cpp:3372-3389](../../../src/Controls/DxUi.Controls.cpp#L3372)).
    The host republishes after a handled mouse-up ([WindowHost.cpp:3109](../../../src/Controls/DxUi.WindowHost.cpp#L3109))
    but never after a handled key.
  - Repro: Space checks the box, but UIA `ToggleState` stays Off even on an owner-thread query, and no property event is raised.
  - #70 fixed this per control for range values only. **Fix.** Publish in `ApplyCheckedState` and Checkbox indeterminate
    changes. Better, mark dirty once in the host when a key was handled by a live target, as the mouse path does.
- [x] **CR-13 AltGr+letter runs the Ctrl editing shortcuts in TextField and editable ComboBox** (pre-window) [repro]
  - **Done (2026-10-18, Q7).** A window host leaves a `WM_SYSCHAR` that names no mnemonic to `DefWindowProc` (Alt+Space
    opens the window menu), and `TextField` and `ComboBox` type no character that arrives with Alt alone, while AltGr
    (Ctrl+Alt) still types. Tests: the Alt steps of `TestAltGrCharactersRunNoEditingShortcuts`, and
    `TestNativeTextInputBackendLeavesASystemCharacterToTheSystem` in the focus-taking suite, for the agreed run.
  - **Partly done (2026-10-18).** Ctrl with Alt runs no editing shortcut in TextField or the editable ComboBox (Grid
    and Tree already excluded it). Test: `TestAltGrCharactersRunNoEditingShortcuts`, which encodes the review's
    reproduction. Remaining, with Q7: `WM_SYSCHAR` still reaches a focused editor as text (`RouteFocusedCharInput`), so
    Alt+Space never opens the system menu over an editor. The test that pins it (`alphaZ`) is in the focus-taking
    NativeTextInput suite, so that change waits for the agreed interactive run (Q13).
  - AltGr arrives as Ctrl+Alt. The host records both ([WindowHost.cpp:4905-4925](../../../src/Controls/DxUi.WindowHost.cpp#L4905)),
    and the Ctrl block tests only `ModifiersContainCtrl` ([TextInput.cpp:2160](../../../src/Controls/DxUi.TextInput.cpp#L2160);
    ComboBox.cpp:1258-1357).
  - Repro: "ab" plus Polish AltGr+A (`ą`) becomes "ą". SelectAll runs, then the character replaces the text.
  - On Czech, Slovak, Hungarian and Croatian layouts AltGr+V types `@`, so typing an e-mail address pastes the clipboard.
    AltGr+Z undoes and AltGr+X cuts.
  - **Fix.** Ignore Ctrl shortcuts when Alt is also down, in every control. The embedded `TextInputServices::PreTranslate`
    already does. See Q7.
- [x] **CR-14 A non-focusable TabControl cannot remove or take a page that holds focus** (#70 regression) [repro]
  - **Done (2026-10-18).** When the TabControl refuses the focus (not focusable, disabled or hidden) and no callback made
    a newer request, the removal clears the focus as `Panel::TakeChild` does. Test:
    `TestNonFocusableTabControlRemovesAFocusedPage`, which encodes the review's reproduction.
  - `RemoveTabByIdentity` moves focus to the TabControl. `SetFocusControl` ignores a non-focusable target, so focus stays in
    the page, and the new post-callback checks then refuse the removal
    ([Controls.cpp:7250-7266](../../../src/Controls/DxUi.Controls.cpp#L7250)).
  - Repro: two tabs before `RemoveTab(1)`, two after. `CloseTab` and `TakeTab` silently do nothing.
  - **Fix.** When the transfer is refused, clear focus as `Panel::TakeChild` does.
- [ ] **CR-15 `main` has been red on every push since #70** [`gh`]
  - **Update (2026-10-18).** No longer permanently red: from #76 to #85, `main` passed on every merge except #82 (x64
    Debug) and #84 (ARM64 Debug), both a Menu-suite failure. #85 fixes the #84 one (menu-bar hover behind a frontmost
    popup). The #82 one (an oversized context menu's scrollbar state before its first frame) is still an open flake. Q5's
    NOT_RUN obligation is therefore not needed; what remains is that flake, then making `ci-gate` required, which
    changes the ruleset and waits for your approval.
  - On the hosted `windows-11-vs2026-arm` runner the lease's warning activation is refused: a `Windows.UI.Core.CoreWindow`
    holds the foreground. The Menu and NativeTextInput suites therefore exit 24, and `ci-gate` fails. #69 also merged
    with this conclusion (run 38046179264).
  - A permanently red aggregate cannot catch a real regression, and it trains people to merge red.
  - See Q5. Never report a refused suite as passed.
- [x] **CR-16 Embedded `Prepare` can return E_ABORT and never ask for preparation again** (#70) [code]
  - **Done (2026-10-18).** Every superseded-frame return requests preparation first; the header and docs/hosting.md
    call E_ABORT retryable, and the sample skips that frame instead of quitting. Test: the preparation-retirement case in
    `Embedded.Tests.Embedded.cpp` now checks the request. That check was reasoned, not run, against the old code: the
    old run aborts earlier, in the CR-17 test.
  - `Prepare` sets `s.dirty = true` before painting ([Embedded.cpp:290-291](../../../src/Rendering/Embedded.cpp#L290)).
    The new E_ABORT returns ([341-342](../../../src/Rendering/Embedded.cpp#L341),
    [362-366](../../../src/Rendering/Embedded.cpp#L362)) skip the tail that re-requested preparation.
  - `MarkDirty` calls `requestPreparation` only on a clean-to-dirty transition
    ([172-181](../../../src/Rendering/Embedded.cpp#L172)), so later invalidations are swallowed too.
  - A callback-driven host, which is the scheduling hosting.md teaches, keeps a blank, non-interactive view.
  - The public header and docs/hosting.md do not say that E_ABORT is retryable, and the sample treats it as fatal (`PostQuitMessage`).
  - **Fix.** Call `requestPreparation` before returning E_ABORT, document the code as retryable, and make the sample retry.
- [x] **CR-17 Embedded accessibility terminates the process on a model exception** (#70 asymmetry) [code]
  - **Done (2026-10-18).** `UpdateAccessibility` catches `std::exception`, reports it through the diagnostics sink,
    keeps the prior snapshot and leaves the preparation unpublished, so the next call retries. Test:
    `TestEmbeddedAccessibilityContainsAThrowingModelGetter`; the old code ends the process with `std::terminate`.
  - `EmbeddedHost::UpdateAccessibility` and `AttachAccessibility` are `noexcept` and catch only `bad_alloc`
    ([Accessibility.cpp:11564-11577](../../../src/Controls/DxUi.Accessibility.cpp#L11564)).
  - The snapshot build calls throwing model methods directly: `GetVisibleItem` at
    [2006](../../../src/Controls/DxUi.Accessibility.cpp#L2006), `GetColumn` at 2060, and `GetCellData`.
  - The native flush and the embedded `Prepare` both catch `std::exception`.
  - **Fix.** Catch `std::exception`, keep the prior snapshot, and do not advance `preparation`.

## P2: defects with narrower triggers

Items marked [reported] were traced by a reviewer. The lead checked them only against the cited lines.

- [ ] **CR-18 Embedded `GetItem` publishes outside `UpdateAccessibility`** (#69) [code]
  - **Partly done (2026-10-18).** Embedded targets keep their own event baseline (A4): `UpdateAccessibility` diffs and
    raises against the snapshot it last raised events for, so a GetItem publication in between no longer swallows the
    pending selection and focus events. Test: the GetItem step of
    `TestEmbeddedSingleGridEventsReachAClientSubscribedToTheApplicationsWindow`. Remaining: the GetItem publication still
    reads live geometry without the view's coherence, which needs a coherence query from the provider into the view.
  - The cache-miss path publishes and discards the event diff ([Accessibility.cpp:9410-9415](../../../src/Controls/DxUi.Accessibility.cpp#L9410)).
    The embedded diff baseline is the published snapshot itself ([1098](../../../src/Controls/DxUi.Accessibility.cpp#L1098)),
    so selection, focus and value events pending since the last update are never raised.
  - It also publishes geometry that was never prepared.
  - **Fix.** Keep a separate embedded `eventBaseline`, as native does, and require `InputIsCoherent`.
- [x] **CR-19 Grid UIA peers outside the viewport are half-functional** (#69 and pre-window) [code]
  - **Done (2026-10-18, with Q15).** Columns count in display order for `GetItem`, `GridItem.Column`, a row's cells and
    name, and the headers; every column has a header element (offscreen when scrolled out), so a cell of a scrolled-out
    column names its header; rows and cells have ScrollItem, and SetFocus and Toggle on a cell scroll it into view first.
    Tests: `TestGridUiaPatternFollowsTheColumnDisplayOrder`, `TestGridUiaPatternScrollsAnOffscreenItemIntoView`,
    `TestGridUiaPatternFocusesACellOfAScrolledOutColumn`. A row needs no reveal for its own actions: the grid's row
    requests accept any row outside a collapsed group, the viewport only bounds the snapshot.
  - **Partly done (2026-10-18).** A row whose cells the snapshot holds (on screen, or materialized by `GetItem`) has its
    cells as children, and its cells are siblings (`SnapshotGridRowHasCells`; the navigation step of the offscreen
    `GetItem` test in `Controls.Tests.GridUiaPattern.cpp`). Remaining, as a design: the grid's own requests
    (`RequestSelectRow`, `RequestToggleCheckboxCell`, `RequestFocusCell`) act on rows on screen only, so SelectionItem,
    Toggle and SetFocus on an offscreen row need a ScrollItem path (scroll the row in, then act) rather than a wider
    resolver; headers of scrolled-out columns, and display order for `GridItem.Column` and `GetItem` (Q15).
  - Rows reached through `GetItem` or a selection are not navigable, because child and sibling navigation requires
    `gridVisibleRowIds` ([Accessibility.cpp:2282-2285](../../../src/Controls/DxUi.Accessibility.cpp#L2282),
    [3034](../../../src/Controls/DxUi.Accessibility.cpp#L3034)). They also lack SelectionItem.
  - Cells in scrolled-out columns are returned by `GetItem`, but Toggle, Value and SetFocus fail, because
    `ResolveGridCellData` requires a viewport column ([9284-9288](../../../src/Controls/DxUi.Accessibility.cpp#L9284)).
    Their column headers are unavailable.
  - [reported] Cell order, `GridItem.Column` and `GetItem` use model order, while headers, pixels and Ctrl+C use display
    order after a column reorder.
- [ ] **CR-20 Native menu (HMENU) conversion loses `&&` and every explicit mnemonic** (pre-window; defeats Q8 for HMENU menus) [code]
  - **Done (2026-10-18), pending the agreed interactive run for the Menu suite's conversion test.** A converted entry keeps
    its `&` and `&&` and the popup decodes them (`TestNativeMenuConversionKeepsAmpersandsForThePopupToDecode` in the
    WindowHost suite); the menu bar conversion keeps stripping titles into its separate mnemonic.
  - `StripNativeMenuMnemonicMarkers` turns `&&` into `&` and drops single `&`
    ([NativeMenuInterop.cpp:23-44](../../../src/Controls/NativeMenuInterop.cpp#L23),
    [168](../../../src/Controls/NativeMenuInterop.cpp#L168)). The popup then parses the result with `ParseMenuLabel` again
    ([Menu.cpp:362-397](../../../src/Controls/DxUi.Menu.cpp#L362)).
  - `Save && Close` displays `Save  Close`, with a space mnemonic. `R&&D` displays `RD`. `E&xit` answers to `E`, not `X`.
  - **Fix.** Pass the raw text and let the popup decode it. The existing "strips mnemonics" test pins the current behavior (Q8).
- [ ] **CR-21 Menu keyboard defects from the #70 rework** [code]
  - **Done (2026-10-18), pending the agreed interactive run for its Menu tests, except the opening key's character.**
    Right in a right-to-left submenu closes only it, and Left on a plain submenu row there moves to the next root as
    Right does left-to-right, where it closed the submenu (`TestMenuKeyboardRightArrowInRightToLeftClosesOnlyTheSubmenu`); a
    slider row's mnemonic selects the slider and AltGr keeps the menu open (`TestMenuSliderMnemonicAndAltGrKeepTheMenuOpen`);
    the idle branch rechecks `running` after its peek (`TestMenuLoopEndsWhenAnotherThreadDismissesIt`).
  - **Deferred: the opening key's character.** Whether the WM_CHAR that `TranslateMessage` posts for a key that opens a
    menu reaches the popup's loop depends on the application's handler, and no reproduction exists yet; it needs the
    interactive desktop to establish before any filtering, which could eat a real first keystroke.
  - In RTL, Right closes the submenu and then also switches the root, because there is no `else`
    ([Menu.cpp:5544-5558](../../../src/Controls/DxUi.Menu.cpp#L5544)).
  - A unique mnemonic on a Slider row invokes the row's command id (normally 0) and closes the menu
    ([5612-5615](../../../src/Controls/DxUi.Menu.cpp#L5612), [920-927](../../../src/Controls/DxUi.Menu.cpp#L920)).
  - [reported, plausible] Any Alt keydown dismisses the menu, AltGr included, so AltGr characters can never be mnemonics.
  - [reported, plausible] The WM_CHAR of the key that opened the menu is taken as a mnemonic.
  - [reported, plausible] The synchronous loop's idle branch never rechecks `running` after a sent dismissal
    (5957-5995), so a dismissed menu can block until the next message.
- [ ] **CR-22 Text input edge defects** (pre-window) [code]
  - **Done (2026-10-18), pending the agreed interactive run for its NativeTextInput tests.** Alt+key is CR-13's fix. One TSF
    edit inserts at most 65,536 units while a longer document still takes edits
    (`TestNativeTextInputTextStoreEditsALongDocument`); `AdviseSink` lets the advised sink change its mask and refuses
    another with `CONNECT_E_ADVISELIMIT` (`TestNativeTextInputTextStoreReAdviseChangesTheSinksMask`).
  - **Alt+key.** `WM_SYSCHAR` is routed into a focused editor and inserted, because `TextField::OnChar` ignores modifiers
    ([WindowHost.cpp:3344-3355](../../../src/Controls/DxUi.WindowHost.cpp#L3344)). Alt+Space types a space instead of
    opening the system menu, and menu-bar access keys are typed. A test pins it.
  - **Document size.** TSF edits are refused once the document exceeds 65,536 UTF-16 units, however small the edit
    ([TextStoreACP.cpp:1685-1688](../../../src/Controls/DxUi.TextStoreACP.cpp#L1685), 1774-1777). IME, emoji and dictation
    then fail in large multiline fields.
  - **AdviseSink.** It returns E_FAIL when a sink re-advises to change its mask ([1127](../../../src/Controls/DxUi.TextStoreACP.cpp#L1127)).
    The contract requires S_OK for the same sink and CONNECT_E_ADVISELIMIT for a different one.
- [x] **CR-23 Host focus and resize races** (#70) [code]
  - **Done (2026-10-18).** `SetFocusControl` validates the target before it advances the request revision, so a refused
    request (disabled, hidden, unfocusable or outside the tree) changes nothing and cannot cancel the request it runs
    inside; `OnSize` prunes interaction state after its layout and publication callbacks. Test:
    `TestWindowHostRefusedFocusRequestInABlurHandlerKeepsTheOuterRequest`. The TabControl removal of CR-14 relies on
    the unchanged revision to recognize a refused transfer.
  - `SetFocusControl` advances the request revision before validating the target
    ([WindowHost.cpp:1806](../../../src/Controls/DxUi.WindowHost.cpp#L1806), 1829-1833). A blur handler that asks to focus
    a disabled or hidden control cancels the outer request after `_focusedControl` was cleared, so nothing is focused
    and no callback or publish follows.
  - `OnSize` dereferences `_focusedControl` after `SetBounds` and publication callbacks ([4388-4409](../../../src/Controls/DxUi.WindowHost.cpp#L4388)).
    That is a use-after-free if an adaptive-layout callback retires it (plausible).
- [x] **CR-24 Exception containment is inconsistent at `noexcept` boundaries** [code]
  - **Done (2026-10-18).** The Tab boundary callback, hover and move dispatch, the supplemental tooltip copy and
    Tree's focus delegate under UI Automation's SetFocus are contained; UIA `SetValue` bounds the client's string before
    copying it for the window thread. Tests: `TestWindowHostContainsThrowingCallbacksOnNoexceptPaths` and
    `TestTreeFocusRequestContainsAThrowingDelegate`. The UI Automation actions now have A5's single boundary: the
    provider's `Execute*` actions are no longer `noexcept`, and `RunUiThreadAction` contains what escapes them, a
    non-`std` exception included (Q6), for an action called on the window's thread and for one dispatched to it, with a
    diagnostic and a republication (`TestAccessibilityActionsContainAThrowingCallback`). A text range's Select, and the visual-line
    and bounds work a text range dispatches to the window's thread, run through the same boundary, shared as
    `ContainAccessibilityUiAction`. The window host's last direct calls from
    `noexcept` helpers are contained: a throwing mnemonic or character counts as taken, and a throwing Tick stops the tree's
    ticks until a control asks again (the mnemonic, character and tick steps of
    `TestWindowHostContainsThrowingCallbacksOnNoexceptPaths`). The embedded entry points already contain standard
    exceptions; per Q6 the input-and-accessibility contract and the hosting guide now say that DxUi's callbacks throw
    only `std::exception` types, and only the UI Automation action boundary also contains another type.
  - These paths can throw out of a `noexcept` function, which calls `std::terminate`:
    - `HandleTabNavigation` calls `_onTabBoundary` uncontained ([WindowHost.cpp:2060](../../../src/Controls/DxUi.WindowHost.cpp#L2060));
      `_onFocusChanged` is contained.
    - `UpdateHover` calls `OnHoverChanged`, `OnMouseLeave` and `OnMouseMove` directly
      ([4813-4839](../../../src/Controls/DxUi.WindowHost.cpp#L4813)); `PruneStaleInteractionState` wraps the same calls.
    - `Tree::RequestFocusVisibleItem` calls the focus delegate from the `noexcept` UIA `SetFocus` executor
      ([Tree.cpp:897-932](../../../src/Controls/DxUi.Tree.cpp#L897)).
    - UIA `SetValue` copies an unbounded client string before checking its 65,536 cap
      ([Accessibility.cpp:8128](../../../src/Controls/DxUi.Accessibility.cpp#L8128)).
  - `TryControlCallback` catches only `std::exception`, so C++/WinRT `hresult_error` still terminates (Q6).
- [ ] **CR-25 Hot-path costs that scale with the tree** [code]
  - **Partly done (2026-10-18).** The raw-message trace gathers its arguments only while a diagnostics sink listens, and
    `RefreshWindowHostAccessibilitySnapshot` returns before any walk when no client listens (with CR-12). The per-move tree
    walks and the Tree snapshot's full copy remain, for A6 with paired measurements.
  - Every traced pointer, focus and capture message evaluates the whole diagnostic argument list before
    `TraceWindowHostDiagnostics` returns ([WindowHost.cpp:2748-2814](../../../src/Controls/DxUi.WindowHost.cpp#L2748)).
    That list includes an extra hit test, `WindowFromPoint`, two class-name strings and five control descriptions.
    **Fix.** Guard the block with `IsContextMenuDiagnosticsEnabled()`.
  - One `WM_MOUSEMOVE` performs about 6-9 full-tree walks: prune, hit validation, post-callback revalidation and tooltip
    target. Hit testing also re-walks each subtree at every nesting level ([Controls.cpp:44](../../../src/Controls/DxUi.Controls.cpp#L44)).
  - `RefreshWindowHostAccessibilitySnapshot` walks the tree for the focused control before checking whether any client is
    interested ([Accessibility.cpp:11023-11045](../../../src/Controls/DxUi.Accessibility.cpp#L11023)). That happens on
    every keystroke and selection change, also with no screen reader.
  - The Tree UIA snapshot copies every visible item on every publish ([2003-2023](../../../src/Controls/DxUi.Accessibility.cpp#L2003)):
    100K records per keystroke while a screen reader runs. The Grid is viewport-bounded.
- [x] **CR-26 Smaller UX defects** [code]
  - **Done (2026-10-18).** A dragged tab passes a neighbor only when the pointer crosses that neighbor's midpoint
    (`TestTabControlReorderingReportsStableMove` now checks no early swap, no flipping and the last slot; it and the
    NewControls reorder test had pinned the early swap). StackPanel extents carry the child's lifetime
    (`TestStackPanelExtentsDoNotOutliveTheirChildren`, which depends on the allocator reusing an address).
    `GetModelAssignmentGeneration` ignores delegate changes (`TestGridModelAssignmentGenerationIgnoresDelegateChanges`).
    Tree expansion under reduced motion is R-1 and Grid Ctrl+C is CR-11. The window host's per-paint root bounds are now the
    documented contract (2026-10-18): the host owns the root's bounds, which installation, resize and DPI changes already
    reset, and equal bounds change nothing, so the paint only makes an application's inset of the root end predictably
    (Controls and layout; getting started; the inset step of `TestWindowHostCaptureDiscardsFrameWhenPaintReplacesItsRoot`).
  - **Tab drag-reorder.** It picks the first tab whose midpoint lies right of the pointer, so the dragged tab swaps at once,
    flips on every move and cannot reach the last slot ([Controls.cpp:8104-8157](../../../src/Controls/DxUi.Controls.cpp#L8104)).
    It is on by default and pre-window.
  - **Tree expansion under reduced motion.** The expansion animation state is built and painted without the reduced-motion
    gate, but no frame is scheduled, so new rows stay invisible until the next event
    ([Tree.cpp:963](../../../src/Controls/DxUi.Tree.cpp#L963), 985, 1130-1131). Pre-window.
  - **StackPanel.** `_childExtents` is only appended, keyed by raw pointer, so a recycled address inherits a stale extent
    ([Controls.cpp:9087-9103](../../../src/Controls/DxUi.Controls.cpp#L9087)).
  - **Grid Ctrl+C with nothing selected.** It writes an empty string to the clipboard and reports the key handled
    ([Grid.cpp:6010-6013](../../../src/Controls/DxUi.Grid.cpp#L6010)).
  - **Grid assignment generation.** `GetModelAssignmentGeneration()` also changes on `SetDelegate`
    ([DxUi.h:3719-3724](../../../include/DxUi/DxUi.h#L3719); Grid.cpp:1305). Every GridItem peer and RuntimeId is
    retired although the model is unchanged, contrary to its comment.
  - **Window host render.** `Render` now forces the root bounds to the client rectangle on every frame
    ([WindowHost.cpp:3947-3951](../../../src/Controls/DxUi.WindowHost.cpp#L3947)). This is an undocumented behavior change
    for hosts that inset the root.
- [ ] **CR-27 Accessibility gaps outside the new features** (pre-window) [code]
  - **Mostly done (2026-10-18).** A handled wheel scroll and a DPI change republish. Test:
    `TestAccessibilityPointHitsFollowAWheelScroll`. A RadioButton is a RadioButton element with SelectionItem (no
    Invoke); Select goes through its mnemonic path inside `TryControlCallback`, a chosen one refuses RemoveFromSelection
    and a group with another choice refuses AddToSelection; `RadioButton::SetChecked` publishes as `Toggle::SetChecked`
    does, and a group assigns every state before one publication. Test: `TestAccessibilityRadioButtonsAreSelectionItems`.
    A tree item's expansion change raises ExpandCollapseState from the item, matched across the inserted or removed
    block, and a changed row set raises the structure invalidation. Test:
    `TestTreeExpansionReachesAClientSubscribedToTheWindow`. A TabControl is a Tab element whose own fragments are its
    tabs: a TabItem kind (stable tab ids, header point hits clipped to the strip, SelectionItem, runtime ids, focus as
    the selected tab, selection events and structure changes on tab changes), Selection single and required, published
    from `SyncLayout`. Test: `TestAccessibilityTabControlTabsAreSelectionItems`. Its selection, close-request and closed
    callbacks are contained (a failing close request keeps the tab). A page indicator with at least two pages is a Slider element whose RangeValue is the page
    (1 to the count; SetValue chooses the nearest whole page), and it no longer overwrites its name with an English
    "Page 2 of 5". Test: `TestAccessibilityPageIndicatorIsAPageSlider`. A choice in a RadioButtons group and a tab report their place as PositionInSet and SizeOfSet, which a screen reader otherwise
    counts over every radio button under the flat root (the position steps of both tests). A tree with prepared rows raises the expansion change of its focused item
    before or after the change, read through both sources' `FindItem` without traversing them
    (`TestPreparedTreeExpansionOfTheFocusedItemReachesAClient`). Remaining: the RadioButtons group as an element
    (SelectionContainer is null), and the expansion of an unfocused prepared item, which raises the structure change
    only.
  - TabControl, PageIndicator and RadioButtons are not semantic controls. RadioButton is exposed as a Button with no
    selected state ([Accessibility.cpp:1232-1240](../../../src/Controls/DxUi.Accessibility.cpp#L1232)).
  - Wheel scrolling and `OnDpiChanged` never republish, so bounds, offscreen state and point hits go stale. Both are prior-open.
  - Tree expand and collapse raise no ExpandCollapse or structure events. Prior-open.
- [x] **CR-28 Registered-message and window-property trust (Q18 incomplete)** [code]
  - **Done (2026-10-18).** The property needs no version of its own: a pointer that another copy of DxUi in the process
    registered is not in this copy's registry, so it reads as absent. The process-exit detach carries a per-process nonce (`std::random_device`), and a host
    ignores the message without it (`TestWindowHostIgnoresAForeignProcessExitDetachMessage`). A pointer read from the
    `DxUi.ControlHost` property is used only when it names a target this process registered for that window
    (`TestAccessibilityRefusesAForgedTargetProperty`; its forgery is now zeroed, which the old code AddRef'd, released
    and deleted, where a 0xCD fill was refused by chance). Payload transport, done too: `SendMessagePayload` sends with
    `SMTO_NORMAL`, so a sender goes on answering messages sent to its own windows
    (`TestAccessibilityProviderRequestsBetweenTwoUiThreadsAnswerEachOther`), and a payload whose send timed out, or whose
    UI Automation action was abandoned, is reclaimed with `ReclaimMessagePayload` instead of holding a registry slot and
    its provider references until a hung receiver answered (`TestAccessibilityTimedOutActionsGiveBackTheirPayloadSlots`).
    A per-window share of the slots was not needed once nothing waits in them after its sender left.
  - `WindowHostProcessExitDetach` is a well-known parameterless registered message. Its handler detaches the host for any
    sender ([WindowHost.cpp:2696-2705](../../../src/Controls/DxUi.WindowHost.cpp#L2696)), so a same-session process can
    blank a DxUi application. Fix: a per-process nonce, as CreateProvider v2 does for its token.
  - The `DxUi.ControlHost` window property holds a raw, unversioned C++ pointer that is AddRef'd without validation
    ([Accessibility.cpp:1150-1156](../../../src/Controls/DxUi.Accessibility.cpp#L1150)). Validate it against a process-local
    registry, and version it like the messages.
  - [reported, plausible] `SendMessagePayload` uses `SMTO_BLOCK`, so two UI threads raising notifications on each other's
    windows wait the full 5 s.
  - [reported, plausible] The 128-slot payload registry is process-wide, so one busy or hung window can exhaust it for
    every window.

## Tooling, CI and process

- [ ] **CR-29 The paired benchmark verdict is never recomputed from trusted code** (#70) [code]
  - **Partly done (2026-10-18, Q12).** The verdict already replays both judges on the retained receipt bytes. It now takes
    the workflow's base and merge commits as its trust anchor (`-ExpectedBaseSha`, `-ExpectedCandidateSha`), hashes the
    measured tree's judge before loading anything and runs only the trusted tree's approved copy (`-RepositoryRoot`,
    `-CandidateRoot`); the scope steps and `Test-CiGate` check the named pair; `.github/CODEOWNERS` exists with a
    tooling test. Remaining, as the second step: `ci.yml` runs the scope steps, the verdict (a `paired-verdict` job on a
    fresh runner, from a base-commit checkout, over the downloaded artifact) and `ci-gate` from the base commit and passes
    the three commits, with the workflow digest and structure tests updated. It must land after this change, whose
    parameters its base needs.
  - `Test-DualJudgeReceiptEvidence` checks that the recorded judge decisions agree and that receipt hashes match
    ([BenchmarkGate.psm1:255-291](../../../Tools/BenchmarkGate.psm1#L255)). It never re-runs either judge.
  - The decisions are written by the candidate's own `performance-paired.ps1`, and there is no CODEOWNERS.
  - **Fix.** Re-run the base-pinned judge on the receipts and require equality. Add CODEOWNERS for `Tools/` and `.github/`.
- [ ] **CR-30 Tooling defects reported by the tooling reviewer** [reported]
  - **Partly done (2026-10-18).** `format.yml`'s write job no longer runs for `main`. Changes to the window host, the TSF
    text store and the accessibility providers now owe the interactive suites they run under (Menu, NativeTextInput)
    an INTERACTIVE_NOT_RUN obligation, and the manifest's repeated NewControls rule is gone (the shared-path and
    no-repeat assertions of Test-ScopedTesting's obligation case). The gate and fingerprint items stay
    with Q12, since their code is hash-pinned and changing it needs re-approval; the write token's exposure to branch
    scripts during the dispatch-only reformat remains.
  - **Done (2026-10-18, Q12).** A rise of an exact budget's raw-round maximum alone, which only the paired-block judge
    regresses, is the one expected judge difference in both agreement checks (`BenchmarkGate.psm1`, `performance-paired.ps1`)
    and reaches the verdict table; any other disagreement names its metrics. An exact budget's change is that of its
    medians. `Get-SourceFingerprint` reads git's names as UTF-8 bytes, so a non-ASCII library input no longer throws.
    Measured paths match in any case. The (still unqualified) policy's judge and assignment hashes name the new sources.
    Tests: the peak-only, exact-change, case-variant and non-ASCII fingerprint cases. Remaining: the write token during
    the dispatch-only reformat.
  - The lead checked the cited lines. The reviewer executed all but the last.
  - The paired gate:
    - When the two judges disagree, the result is "inconclusive" with no metric table. A peak-only exact-budget regression
      can therefore never surface as degraded (executed on synthetic receipts).
    - Exact budgets always display +0.00% (executed).
  - Under a non-ASCII path (`src/Größe.cpp`), `Get-SourceFingerprint` throws because git output is not decoded as UTF-8
    (executed).
  - Changes to `WindowHost.cpp`, `TextStoreACP.cpp` and `Accessibility.cpp` produce no INTERACTIVE_NOT_RUN obligation
    (Q29 gap).
  - `format.yml`'s `reformat` job does not exclude `main`, unlike `gallery.yml`; the pull-request rule should reject its
    push. It also keeps the write token in `.git/config` while it runs branch scripts.
- [ ] **CR-31 Prior-open tooling items, re-confirmed**
  - `test.ps1 -Interactive` pipes the lease through `Out-Host` ([InteractiveRun.psm1:245](../../../Tools/InteractiveRun.psm1#L245)),
    so Ctrl+C can kill it before it restores the desktop.
  - The allocation counter is outside the benchmark overlay.
  - PrePush binds only `ci.yml` bytes.
  - **Done (2026-10-18): the allocation counter.** The replaced allocation functions and counters are in
    `Tests/Embedded/Embedded.Tests.BenchmarkInstrument.h`, a benchmark input (hashed, overlaid, a profiling input).
  - **Done (2026-10-18, Q12) except the allocation counter.** `test.ps1 -Interactive` starts the lease attached to its
    console (`Invoke-DxUiLeaseExecutable`) instead of piping it through `Out-Host`, which PowerShell ends with the pipeline
    on Ctrl+C; the lease restores the desktop while the run waits. PrePush delegation binds a coverage contract
    (`prContractFiles` in `Tests/test-scopes.json`: the workflow, the runners, the native-scope classifier and the modules
    they import) and the coverage the manifest claims, not the workflow alone. The Ctrl+C path itself is exercised only
    by a person at the desktop.
- [ ] **CR-32 Test reliability**
  - Seen 2026-10-18: one x64 ASan Debug full run at `4215010` failed `TestWindowHostSecondaryPressCapturesOnlyWhileItOwnsThePointer`
    ("a secondary press still held captures its control"); the same head passed it in x64 Debug and Release, and five
    isolated and four whole-suite ASan Debug reruns passed. The combined check is split into the press reaching the
    button, the thread keeping the button state the test gave it (a message read in between resyncs it), no other
    window of the thread holding the pointer, and the capture, so a recurrence names its cause.
  - Six UIA event tests use fixed 500 ms quiet periods and take 75 of the Accessibility suite's 101 s in Debug.
  - The BranchIntegration record keeps an undiagnosed full-suite failure caused by late events from a preceding step.
    Diagnose it, or isolate each test's UIA client, before trusting a red or green result there.
- [x] **CR-33 Test-coverage gaps exposed by this review**
  - **Done (2026-10-18).** Each path below now has an asserting test, added with its fix (CR-1 to CR-14).
  - These paths have no test today:
    - callbacks that touch host geometry
    - PageHost content (text input, Tab, UIA)
    - stale GetItem requests
    - `WM_GETOBJECT` after a failed flush
    - capture loss that retires the hovered control
    - extraction of a hovered ScrollPanel child
    - AltGr input
    - keyboard toggle UIA state
    - Grid touch activation
  - The [patch](CodeReview_2026-10-10/review-repro-tests.patch) reproduces each. Convert it into asserting tests during
    remediation.

## Repository, API and documentation hygiene

- [ ] **CR-34 Evidence archives are bloating the public repository** (#69, #70, #71)
  - **Waiting (2026-10-18).** Q4 accepted the purge; Q11 asks whether it also takes the raw packets, and the rewrite (backup
    bundle, ruleset lifted, force push) needs your go-ahead at the time. The validator limits land with the cleanup,
    since today's tree would fail them.
  - **Prepared (2026-10-18), Q11 accepted.** The evidence inventory is done (74 packets; 9,467 raw files, 295.6 MiB;
    personal-path, e-mail, machine and runner-name findings), and `Test-BenchmarkGate.ps1` reads the two hosted A/A sets
    from `Tools/tests/fixtures`, so a purge no longer breaks it. Before any upload or rewrite: Q18 (raw assets public
    or private), Q19 (whether the older raw files go in the same rewrite) and Q20 (curated files with personal paths).
    Then: deterministic bundles and SHA-256 manifests, draft releases verified by download, a backup bundle and a fresh
    mirror for `git filter-repo`, the force push of heads and tags with the ruleset lifted, the dangling README links
    and retention docs, and last the validator limits and personal-path scan.
  - The tracked tree grew from 22 to 142 MiB, and Measurements from 14.5 to 130 MiB.
  - 57 zip archives (92 MiB) are already compressed, so packing cannot shrink them, and they include superseded CI runs.
  - Every pinned consumer restores the full tree.
  - Some packets carry developer machine paths (`C:\Users\...`) and a mix of commit identities.
  - **Fix.** Keep READMEs, manifests and SHA-256 in git, and move archives to release assets or retained artifacts. Add a
    validator cap on blob size. Do not rewrite history while consumers pin commits (Q4).
- [x] **CR-35 API revision 4 notes and fixture are incomplete** [code]
  - **Done (2026-10-18).** Build_ToolchainAndConsumption.md lists what a consumer moving to revision 4 changes (null
    `AddChild`/`AddTab` results, extraction that can keep its child and the empty `SetRoot` it then causes, the
    membership-only `OnTreeSelectionChanged`, the menu name and HelpText, `flowDirection` set by name), and the
    revision-4 fragment points to it. `Revision4.cpp` now derives a consumer's Grid and Tree models and delegates and
    initializes `ContextMenuSessionCallbacks` by name, so a new pure virtual, a changed overridden signature or a
    reordered member stops it compiling (compiled alone with the gate's `/W4 /WX` flags). No byte pin: every edit of
    the fixture is already a reviewed diff, and the consumer gate compiles it on every profile.
  - The revision-4 fragment lists topics, not the changes each consumer must make. Examples:
    - `AddChild`/`AddTab` may return null.
    - `OnTreeSelectionChanged` became membership-only.
    - Menu `Name` versus `HelpText` changed.
    - `ContextMenuSessionCallbacks::flowDirection` was inserted mid-aggregate.
    - `RemoveTab` can now refuse.
  - `Tools/ConsumerApi/Revision4.cpp` asserts only the revision-4 additions. It never derives from the model or delegate
    interfaces, and nothing pins its bytes.
  - Panel `TakeChild` leaves a null slot, while TabControl's override erases it. `SetRoot(panel->TakeChild(i))` with a
    refused extraction destroys the whole tree.
- [x] **CR-36 Stale documentation**
  - **Done (2026-10-18).** docs/controls.md says what the menu code and the input contract do (the label or
    `accessibleName` is the name and the secondary text its HelpText; the sample no longer names the path twice); the
    Tree example's `OnTreeSelectionChanged` is the primary selected member, with focus-only moves on
    `OnTreeFocusedItemChanged`; the Grid row covers the inspection command (CR-11), the touch double-tap, the Ctrl+C
    format and the focus-split calls, and the Tree row the silent focus setter; the set callback was already documented
    for both modes; the header comment of `RefreshWindowHostAccessibilitySnapshot` describes the coalesced flush.
  - docs/controls.md says the menu UIA name includes the secondary text. The code puts it in HelpText, and the sample would
    now speak the path twice ([controls.md:54-55](../../../docs/controls.md)).
  - The Tree example uses `OnTreeSelectionChanged` as "the focused row moved".
  - The docs say single-select trees make no set callback.
  - F1 and touch inspection, quoted-TSV copy and the focus-split APIs are not in the consumer docs.
  - The header comment of `RefreshWindowHostAccessibilitySnapshot` still says it republishes.
- [ ] **CR-37 Test hooks ship in the public header**
  - **Deferred by Q9 (2026-10-18)** to the next API revision: moving the `Debug*` declarations to `DxUi/Testing.h` and
    compiling fault injection out of release libraries changes the consumer API, and revision 4 is the one consumers are
    adopting.
  - `DXUI_ENABLE_DIAGNOSTICS` is unconditionally 1 ([Configuration.h](../../../include/DxUi/Configuration.h)).
  - DxUi.h declares about 140 `Debug*` members, about 30 of them new in this window, including process-global fault injection.
  - Every rename is formally an API-revision event (Q9).

## Architecture and simplification

- **A1. Separate "binding still current" from "geometry still current", and commit through one helper.**
  - About 375 `IsCurrent()` checks in Grid.cpp and 175 in Tree.cpp interleave borrowed reads with revalidation.
  - Strict checks after callbacks cause CR-5, and checks missed at single sites caused CR-3 and CR-4.
  - Use a read-validate-commit shape. Read model data into locals, check once, then commit selection, focus and notifications
    through one `CommitSelectionAndFocus(previous)` helper shared by Tree and Grid. That helper is the A2 of the production
    review, which is still open. Estimate: 300-600 lines fewer.
  - Q1 asks whether model getters may stay arbitrary reentrancy points. Narrowing that contract would delete most of these checks.
- **A2. One selection gesture contract.** Give Grid Tree's `SelectMode {Replace, Range, Toggle, FocusOnly}` key map and
  its focus indicator. This fixes CR-7, CR-10 and the drift catalogued by the Grid and Tree reviewers (about 120 lines).
- **A3. One logical-tree walker.** Replace the Panel-only walkers (Tab order in WindowHost, NativeTextInput, TextStoreACP,
  ComboBox) and the UIA traversal with `IsControlInTree` / `GetLogicalChild`. `ControlBelongsToBranch` and the host's
  `ControlBelongsToTree` are correct duplicates of `IsControlInTree` and can go too. This fixes CR-6 and saves about 70
  lines.
  - Once raw slot moves are retired (production review Q1), membership can become O(depth) through `_parent` after a
    lifetime check. That removes most of the per-message walks in CR-25.
- **A4. Make lazy publication total.**
  - Return the canonical root even after a failed flush.
  - Treat stale requests as best effort.
  - Repost when a flush ends dirty.
  - Give embedded targets their own event baseline.
  - Check `interested` before any walk.
  - Consider flushing on the owner before completing a UIA action, so a `SetValue` followed by a read is consistent.
  - About 70 lines of the publication state are dead: `WindowHostSnapshotChanges::structureChanged`, `previous` and
    `current`, `diffBaseline`, the inner `if (! target.embedded)` in an embedded-only block, the unreachable re-register
    branch, and `SameSemanticControls`.
- **A5. One exception boundary.**
  - Internal dispatch helpers that call virtuals or application callbacks should not be `noexcept`. Contain exceptions once
    at the window procedure (already a function-try-block), at embedded entry points and at COM methods.
  - That removes about 20 local try/catch ladders, and CR-24 with them.
- **A6. Smaller deduplications.**
  - **Render.** The two `Render` overloads are about 95% identical
    ([WindowHost.cpp:3916](../../../src/Controls/DxUi.WindowHost.cpp#L3916) and 4164). The release-only copy had to be
    patched twice by #69. About -170 lines.
  - **UIA providers.** About -300 lines: 94 `CaptureSnapshot()` preludes, a duplicated grid-cell resolver, four RangeValue
    getters, and the hand-written resolve/act/revalidate blocks in `Execute*OnWindowThread`. Q5's helper was not delivered.
  - **PageHost.** About -150 lines through a `ForEachPageCallback`, like `ForEachPanelChildCallback`.
  - **Menu.** Parse labels once per popup; `ParseMenuLabel` is `noexcept` but allocates on every paint and key. Share one
    cross-thread call primitive with the UIA actions (production review A8).
- **A7. Evidence outside git, and test hooks in `DxUi/Testing.h`.** See CR-34 and CR-37.

## Review of the later merges (#73-#84) and of the remediation (2026-10-18)

The remediation is rebased on `51ef9f2` (#84). Five read-only reviewers took the merges since the window and the
remediation's own commits: intrinsic layout and FlexPanel (#73), forms (#73), input, hosting and context menus (#80),
tooling, CI and evidence (#74-#84), and `origin/main..HEAD`. The lead re-checked every finding against the code. Each fix
has a test that fails against the code before it, unless the entry says the failure was only reasoned.

### Fixed

- [x] **N-1 P1 Any layout failure leaves the window unpainted** (#73). A failed `PrepareIntrinsicLayout` kept the owner
  pending and made `PreparePendingPageLayout` return false: native Render skipped the frame, embedded Prepare returned
  E_ABORT, and nothing logged or retried. An oversized paste, a long description or an unsupported child froze the window.
  The owner now keeps its previous geometry, the frame paints, the failure is reported once through the diagnostics sink,
  and the owner is laid out again when its content, constraints or bounds change; a skipped (superseded) frame invalidates
  again. Test: `TestIntrinsicLayoutFailureKeepsPaintingThePreviousGeometry` (E_ABORT before).
- [x] **N-2 P1 A Flex or Form under a hidden plain container is never laid out when shown** (#73). `InvalidateMeasure`
  sets the host flag only when an owner is on the control's own ancestor chain. Showing a control now asks for the
  preparation. Test: `TestIntrinsicLayoutReachesOwnersUnderAShownContainer`.
- [x] **N-3 P1 Native UIA bounds were published before the deferred layout and never refreshed** (#73). The host republishes
  after each successful pass that arranged an owner. Reasoned; the accessibility suite passes, no UIA-client test added.
- [x] **N-4 P1 Every measure invalidation under a layout owner closed the embedded input gate** (#73 forms). A validation
  message shown by one character dropped the next one, or the trail half of a surrogate pair. `InvalidateMeasure` no
  longer advances the interaction revision (geometry moves only when the next preparation arranges it), and keys,
  characters and text services are refused only by a hidden, suspended, unprepared or callback-suspended view. Unchanged
  FormField setters are no-ops. Test: `TestFormsEmbeddedValidationOnEachEditKeepsInput`.
- [x] **N-5 P2 Removed children counted against the Flex (1,024) and Form (256) limits** (#73): `TakeChild` leaves null
  slots. The limits count live children. Test: `TestFormsPartsLeaveCleanlyAndPreferencesShrink`. The slots themselves
  still accumulate until `ClearChildren` (see N-21).
- [x] **N-6 P2 A fixed-size Flex child failed layout at inexact origins** (#73): no float endpoint represents the span, so
  the maximum check failed at ordinary widths and DPI-scaled offsets. One representable step past the maximum is
  accepted while it is under 1/1024 of the extent. Test: `TestFlexFixedExtentsArrangeAtInexactOrigins`.
- [x] **N-7 P2 RadioButton measured a push button's chrome** (24 DIP for 40 painted). Test:
  `TestRadioButtonMeasuresTheChromeItPaints`.
- [x] **N-8 P2 FormField parts** (#73 forms): raw label, description and validation pointers were used after a move
  through `GetChildren()`; a smaller `SetEditorPreferredSize` could not shrink an `E_NOTIMPL` editor (the fallback was
  cached); a removed editor left the label's mnemonic target and the field's help text and automation id on itself.
  Parts resolve through lifetime-checked accessors after every callback-capable call. Test:
  `TestFormsPartsLeaveCleanlyAndPreferencesShrink`.
- [x] **N-9 P3 `Checkbox::OnMouseUp`, `OnKeyDown` and `OnMnemonic` had become protected** (#73). Public again.
- [x] **N-10 P2 A Grid sort glyph never settled** (pre-window, found reviewing #80): after one sort every later animation
  anywhere in the window repainted the grid, and the old column kept its 18 DIP title gap. Assertions added to
  `TestGridTickReturnsTrueForSortGlyphTransition` and `TestGridHeaderClickMovesSortGlyphToClickedColumn`.
- [x] **N-11 P2 A secondary press installed capture after its handler already ended the press** (#80, host half
  pre-existing): a blocking `ContextMenu::Show` read the release, or `ShowAsync` lost its popup to `SetCapture`. The host
  and ScrollPanel capture only while the press still owns the pointer. Test:
  `TestWindowHostSecondaryPressCapturesOnlyWhileItOwnsThePointer`.
- [x] **N-12 P2 Printable keys typed into a focused editor reached the application's key fallback** (#80): a single-key
  accelerator acted on every letter. They stay handled; other keys still reach the application. Test: the editor case of
  `TestNativeUnhandledKeysReachApplicationFallback`. `WM_CHAR` stays always handled, so an application still cannot see
  an unconsumed character (unchanged).
- [x] **N-13 Tooling.** Every CI checkout sets `persist-credentials: false` (the reviewed workflow digest is updated), and
  a hosted ARM64 desktop that cannot be prepared fails only the foreground suites instead of stopping `test.ps1` before
  any suite runs. No tooling test covers the new failure branch yet.
- [x] **R-1 to R-12 The remediation's own defects** (self-review of `origin/main..HEAD`):
  - R-1 Tree row expansion animated under reduced motion with no frame to advance it (CR-5 removed the early return
    that hid it).
  - R-2 A Grid right-click opened the menu for the row under the pointer after the selection callbacks (now the pressed
    row by id, none when removed). Test: `TestGridRightClickMenuFollowsThePressedRow`.
  - R-3 `ScrollPanel::OnDescendantDetaching` asked membership of a subtree a capture-loss callback had destroyed
    (an access violation in Debug). Test: `TestScrollPanelSurvivesACaptureCallbackThatRetiresTheDetachingRow`.
  - R-4 The CR-8 focus seeding (see CR-8).
  - R-5 `IsSameHitTarget` compared places only: a focus handler that refreshed rows in place retargeted the click. The
    stable row, group or item id is compared too (both Tree and Grid tests).
  - R-6 The detaching hook stopped at a PageHost and TabControl's tab removal skipped it; the hook is protected now, so
    a derived panel chains to it. Test: `TestScrollPanelEndsTheGestureOfARowLeavingAPageOrATab`.
  - R-7 A checkbox toggle and `RequestFocusCell` used a column index across callbacks; they recheck the column id
    (no test yet).
  - R-8 `RequestExpandedState` reported success for a refusal the delegate published (UIA Expand returned S_OK).
    Checked at the end of `TestTreeExpansionAnimatesWhenTheDelegatePublishes`, which the old code fails earlier, at R-1.
  - R-9 ScrollPanel kept a forwarded gesture after the capture was released silently. Test:
    `TestScrollPanelGestureEndsWhenTheCaptureIsReleasedSilently`.
  - R-10 `SetModel` kept the current-cell outline; Ctrl+Left/Right were consumed with one column; `InspectFocusedCell`
    scrolled before refusing an empty value. Only the one-column case has a test.
  - R-11 The CR-1 spec said the root answers as an empty window before the first publication; it answers
    `UIA_E_ELEMENTNOTAVAILABLE`, and the spec now says so.
  - R-12 The CR-17 failure log used `OutputDebugStringW`; it uses the diagnostics sink like the rest of the library.

### Open

- [x] **N-14 P1 Embedded legacy clocks** (#80): the Grid sort glyph, Tree expansion, the first tooltip deadline, caret
  phases and Button transitions stamp `GetTickCount64()` while an embedded application supplies its own epoch, so with a
  clock below system uptime the sort glyph animates forever and the Tree expansion freezes. Already the "Remaining legacy
  clock audit" item of [ConsumerUiQualification](ConsumerUiQualification_2026-10-11.md); not duplicated here. See Q14.
- [x] **N-15 P2 Native start stamps use system uptime while ticks follow the dispatcher timeline** (#80; PageHost, Slider,
  Tree, Grid). The dispatcher loses every stall over 50 ms, so a page transition can stay at progress 0 and input-dead for
  that lag. Same ledger and question.
  - **Done (2026-10-18, Q14) for N-14 and N-15.** `ControlHost::GetAnimationTickMs()` is the one animation clock: the
    dispatcher timeline natively, and embedded a timeline that `AdvanceAnimation` advances by the difference between
    consecutive supplied ticks, rebased after a pause in ticking (the request edge, showing, regaining an extent, a new
    attachment, which continues from the clock read before). PageHost, Slider, Button, the carets, Tree, Grid, tooltips
    and the menu slider stamp it; `Tick` receives it, so the per-control first-tick anchors and `_lastAnimationTickMs`
    are gone. The Button disclosure chevron and scrollbars keep their first-tick anchors on purpose; input timing stays on
    real time. A Tree expansion asks for its own ticks (a UI Automation Expand asked for none), a chevron runs its whole
    duration rather than ending at a tick at its start, a Grid or Tree with no host snaps instead of stamping, and
    `TooltipLayer::BeginHideDelay` saturates. Tests: `Scope.Tests.AnimationClock` (embedded epochs per control against a
    view clock far below and far above uptime, pauses, attachments, a native stall); restoring the uptime stamps fails
    them. The native Animation tests that read uptime as their base hold the host clock instead
    (`DebugSetAnimationClockForTest`), which removes their wall-clock race.
- [ ] **N-16 P2 Flex reshapes every child on any size change, with O(n²) bookkeeping** (#73). Needs a paired measurement
  before and after; belongs with A6.
- [ ] **N-17 P3 layout leftovers** (#73): the baseline is computed and never consumed; pure `Measure` runs arrangement
  staging; `HasLayoutOverflow` reports 1-2 ulp excesses; wrapped text measures trailing whitespace; inverted bounds return
  E_INVALIDARG; no flex-shrink (documented); `ReplaceDevice` changes typography without requesting layout; unbounded
  nesting depth. Done (2026-10-18): `Toggle::SetStateLabels` returns early when nothing changed.
  `EmbeddedHost::ReplaceDevice` requests the intrinsic layout pass, so the next preparation remeasures text through
  the new factory (the last step of `TestIntrinsicPreparationIsSharedByNativeAndEmbeddedHosts`). Wrapped text is measured without
  the space that ends each wrapped line, which hangs past the width it wrapped to, so two words wrapped one per line fit
  the width that holds either (`TestWrappedTextIgnoresTheSpaceThatEndsEachLine`); one line keeps its trailing space.
- [ ] **N-18 P3 forms leftovers**:
  - Done (2026-10-18): an empty label is hidden, so `LabeledBy` never names an unnamed element
    (`TestFormsHideAnEmptyLabel`). A field keeps its label, description and validation parts: `TakeChild` refuses them,
    `ClearChildren` removes the editor and other additions, keeps the parts' text and state, and compacts the emptied
    slots (`TestFormFieldKeepsItsPartsThroughTakeChildAndClearChildren`; the replacement-editor test now counts the parts).
  - Decided (2026-10-18): a new error is not announced by the library. It changes the editor's `ItemStatus`; when to speak
    it (Apply, focus leaving) is the application's call, through `RaiseWindowHostAccessibilityNotification`, which the
    FormField design note and the control guide now say. Announcing every change would speak at each keystroke of a
    validating field.
  - Open: the logical order puts the description and error before the editor (a reorder needs an insertion API, or a
    remap kept consistent with accessibility paths and `TakeChild` indices); `SyncAccessibility` owns the editor's help
    text and automation id (documented ownership); editor names come from a root-wide walk per editor; `[[nodiscard]]` on
    the HRESULT setters would break consumers that ignore them; there are no getters for description, required or
    validation text.
- [x] **N-19 P3 input leftovers** (#80). Done (2026-10-18):
  - NumericStepper, ColorPicker and TagPicker no longer give up their own parts through `TakeChild` or `ClearChildren`
    (`TestCompositeControlsKeepTheirOwnParts`); the legacy mutable `GetChildren()` span can still destroy one, as for
    every panel.
  - A right double-click is a press: ScrollPanel forwards a double-click as the child's double-click and treats it as a
    press on its scrollbar, and Grid, Tree and ComboBox map a right double-click to their right press (their menu opens
    again). Tree's scrollbar double-click pages again (it returned false). Tests:
    `TestScrollPanelForwardsSecondaryPressInContentCoordinates`, `TestScrollPanelScrollbarDoubleClickPagesAgain`,
    `TestTreeDoubleClicksWithoutARowMeaningActAsPresses`, `TestGridRightClickInvokesContextMenuForHitRow`.
  - Focus churn: the host clears the focus on an unhandled press only when no focus request was accepted during the
    dispatch and the press continued no captured gesture
    (`TestScrollPanelKeepsTheFocusAChildTakesOnAnUnhandledPress`).
  - Chord: a press during a forwarded gesture reaches the gesture's child, the inner capture records its button and
    ends only with that button's release, a thumb drag ignores the right button, and WindowHost keeps the capture
    while the other routed button is still down. Unscrolled controls had the same stranding: the second button's
    release ended the capture (`TestASecondButtonDuringADragLeavesTheDragToItsControl`, both layouts).
  - `PageHost::Paint` requests animation while a transition is active
    (`TestPageHostTransitionBegunWhileHiddenResumesWhenPainted`); `ComboBox::Tick` no longer returns `_open`
    (`TestOpenComboBoxListKeepsNoFramesTicking`).
  - Keyboard anchors start at the reading-order edge and are clamped into the scroll viewports, gathered in the same
    walk that precedes the virtual hit-bounds query; Grid and Tree call `ResolveContextMenuAnchor` unqualified and
    mirror their row anchor in RTL (`TestKeyboardContextAnchorsStayInsideTheScrollViewport`). Left: a Grid or Tree
    inside a scrolled ScrollPanel clamps its keyboard row anchor to its own content only.
- [ ] **N-20 Tooling and evidence** (#74-#84):
  - Done (2026-10-18): the legacy rank test counts in doubles, so nineteen and twenty blocks no longer overflow; committed
    judge, policy and assignment sources are hashed from their exact bytes, whatever the console's code page or a final
    newline. Open: the zero-median block effect (Q17).
  - CR-29 is half fixed: the judge is pinned, but the code that applies the pin (`BenchmarkGate.psm1`, `Test-CiGate.ps1`,
    `ci.yml` itself) is read from the pull request's checkout, the verdict takes its base commit from the summary it
    judges, and there is no CODEOWNERS. See Q12. Done (2026-10-18): the scripts take the workflow's commits and trusted
    tree (CR-29), and CODEOWNERS names `@eric-jesover`; the `ci.yml` switch is the follow-up step.
  - The tracked tree grew from 154.7 to 316.9 MiB (+5,273 files). `Test-DxUiMeasurements` reads only `*.json` and limits
    nothing; 222 files embed a personal `C:\Users\` path, receipts carry runner host names and API dumps an e-mail
    address. See Q11.
  - The UIA client still waits fixed quiet periods (CR-32); its quiet timeout ends the process, as every failed
    `Require` does in this fail-fast runner, so it is left as is.
  - `Prepare-HostedArmDesktop.ps1` never reverts its two HKCU values, its guard is environment variables only, and its
    guard test passes vacuously off ARM64. The lease's `Read()` saves a minimized foreground that `BringToForeground`
    then restores. Done (2026-10-18): the script's whole precondition is one exported guard,
    `Test-DxUiHostedArmDesktopRunner`, tested marker by marker and against x64, x86 and Arm systems on any machine; the
    script test proves the script asks it, once, before loading Win32 or writing the registry. Left as is: the HKCU values
    exist only on the disposable hosted image the guard admits, whose GitHub markers are the lease's trust model; and the
    hosted ARM64 evidence (foreground `restored` in all three profiles) comes from restoring the minimized first-sign-in
    CoreWindow, so keeping a minimized foreground minimized waits for a hosted ARM64 run proving that
    `SetForegroundWindow` alone holds it.
  - `git show` blobs are hashed after console decoding; `git diff` runs without `-z`; scope matching is case-sensitive
    on a case-insensitive checkout; the subset-sum DP overflows at 19 or more blocks; a zero block median throws; the
    paired artifact uploads neither executables nor `build.json`. Done (2026-10-18): `Get-PullRequestChangedPaths` reads
    `git diff -z` as UTF-8 through a process (`Invoke-GateGitBytes`), so a name git would quote or the console would
    misdecode still matches its rule (BenchmarkGate case "a non-ASCII path is named as git stores it"). Left: the scope
    test deliberately pins case-sensitive matching, which belongs with the trust model of Q12; the `git show` decoding
    and the two judge edge cases change the pinned judge hash, so they wait for its re-approval.
- [ ] **N-21 Remediation leftovers**: null child slots accumulate until `ClearChildren` (compaction during an index-based
  traversal would skip children, so it needs an explicit safe point); `ResolveGridRowFocus` duplicates Tree's rule; a
  `NotifyDataChanged` nested in a selection callback repeats a report with the same payload; `_activeColumn` and the
  inspection surface are invisible to UI Automation; `WM_CAPTURECHANGED` matches the hovered control by address.
  - Done (2026-10-18): Tree and Grid rows resolve their focus ring through one `ResolveRowFocusVisuals` (the gallery
    suite's pixels are unchanged).
  - Done (2026-10-18, Q16): `Panel::CompactChildren()` removes the empty slots in order when the application holds no
    index; a flex panel moves each child's options with it and a radio group keeps its choice
    (`TestPanelCompactChildrenClosesTheSlotsTakeChildLeft`, `TestFlexCompactChildrenKeepsEachChildsOptions`).
  - Left for A1's `CommitSelectionAndFocus`: reports carry the current state, never a delta, and an outer operation
    whose delegate refreshed the model inside its selection callback sends its pending focus (Grid, Tree) or selection
    set (Tree) report although the nested `NotifyDataChanged` delivered the same payload. An exact deduplication needs
    a selection revision that also covers Tree's single selection, at Tree's six report sites; deduplicating against
    the last delivered value instead would drop a report after an unreported programmatic change.
  - Left as is: a cell's element already exposes its full value, which the inspection surface shows only visually. The
    current column is not UI Automation focus: the focused element stays the row, so Ctrl+Left and Ctrl+Right are
    silent. Making the focused element the current cell, as a data grid does, depends on Q15's column order.
  - Left as is: an address the capture-lost callback freed and a new control in the tree reused gets a redundant hover
    leave; the host never dereferences a control outside the tree, because every message first prunes the interaction
    pointers against the live tree. Lifetime tokens beside the host's hover, focus and capture pointers would remove
    this class.

## Questions for the developer

1. **Model getter contract (architecture).** Should `IGridModel`/`ITreeModel` getters remain arbitrary reentrancy points,
   as accepted in production review Q2?
   - The alternative is to declare them pure queries: a debug build asserts on any DxUi mutation inside a getter, and a
     release build defers it.
   - Recommendation: narrow getters to queries and keep delegate callbacks as reentrancy points, checked with
     `IsBindingCurrent()`. This removes most of A1's checks and the class of bug behind CR-5.
2. **Grid inspection key (product).** F1 conflicts with Help (CR-11). Options:
   - keep F1;
   - move to Shift+F10/Apps-style context actions;
   - expose `Grid::InspectFocusedCell()` for applications to bind, and offer Ctrl+Left/Right to choose the column.
   Recommendation: no built-in F1; provide the API and a context-menu action. Return false when nothing is inspected.
3. **Touch double-tap (product).** Should it activate the row, as before #70 and as the mouse does, or inspect it?
   Recommendation: activate. Offer inspection through long-press or the context menu, and only when the cell is clipped.
4. **Evidence storage (process).** Stop committing CI zip archives (CR-34)? Any history rewrite would invalidate the
   commits that consumers pin. Recommendation: keep the history, move future archives to release assets with SHA-256 in
   git, and add a blob-size validator.
5. **Red `main` (process).** Until a qualified ARM64 desktop exists, should the hosted ARM64 foreground suites become an
   explicit `NOT_RUN` obligation that `ci-gate` reports but does not fail on? Should merging with a failing aggregate stop?
   Recommendation: yes to both. Then make `ci-gate` required, as production review Q21 already accepted.
6. **Exception policy.** Should application callbacks that throw non-`std::exception` types, such as C++/WinRT
   `hresult_error`, terminate? Recommendation: document "callbacks must not throw non-std exceptions". Contain them at
   the single boundary of A5, with a diagnostic, rather than adding `catch (...)` at every site.
7. **AltGr and Alt (product).** Should Ctrl+Alt never trigger a Ctrl shortcut in editors, and should `WM_SYSCHAR` reach
   `DefWindowProc` when it is not a mnemonic (Alt+Space opens the system menu)? Recommendation: yes to both, as in the
   Win32 Edit control. The test that pins `alphaZ` changes.
8. **Native menu mnemonics.** Should HMENU conversion keep explicit `&` mnemonics so that Q8's "explicit wins" applies to
   RedSalamander's native menus? Recommendation: yes. The existing strip test changes.
9. **Test hooks.** Move the `Debug*` declarations to an excluded `DxUi/Testing.h` and stop compiling fault injection into
   release libraries, at the next API revision? Recommendation: yes.
10. **Priority.** Is `PageHost` used for consumer settings or editor pages? Today its content is invisible to screen
    readers and to Tab (CR-6). Recommendation: fix with A3 before any consumer adopts revision 4.

Asked on 2026-10-18, after the review of #73-#84:

11. **History purge scope (extends Q4).** The accepted purge covers the 58 CI zip archives (103.8 MiB). Since then raw
    evidence added 162 MiB and 5,213 files (the cold-build packet's 1,172 raw dumps alone are 38.8 MiB), and 222 files
    embed a personal `C:\Users\` path, receipts name the runner hosts, and API dumps hold an e-mail address. Extend the
    same rewrite to those raw packets? Recommendation: yes. Move raw packets to release assets with SHA-256 manifests in
    git (first decoupling the tooling tests that read two retained packets), purge them in the same rewrite as the zips,
    and in the same change add size, count and extension limits and a personal-path scan to the Measurements validator.
    The scan would fail today's tree, so it cannot land before the cleanup.
12. **Trusted CI gate (CR-29, N-20).** Run the benchmark verdict and `Test-CiGate` from a checkout of the base commit
    (or an organization required workflow pinned to `main`), with the expected base and head SHAs passed in, and add
    CODEOWNERS for `.github`, `Tools`, the performance scripts, `.agents` and `AGENTS.md`? With one maintainer, a
    required code-owner review blocks every self-merge. Recommendation: the base-checkout gate now; CODEOWNERS when a
    second maintainer exists.
13. **Interactive run.** Menu, NativeTextInput and the two menu resource suites take real focus and must run through
    `test.ps1 -Interactive`, which asks first, warns while it runs and restores the desktop; it prints its estimate
    before starting. Recommendation: one agreed run before the pull request merges.
14. **Embedded clock (N-14, N-15).** Give every control one host-supplied clock (the embedded application's ticks, or
    the dispatcher's timeline natively) instead of anchoring each transition on its first tick? Recommendation: yes. It
    removes the per-control special cases and the epoch-0 surprises; it needs epoch-0 and large-epoch tests per control.
15. **Grid column order for UI Automation (CR-19).** After a column reorder, should `GridItem.Column`, a row's cell order
    and `GetItem(row, column)` follow the display order that headers, pixels and Ctrl+C use? A screen reader then names
    the column it sees, but a client that kept model column indices sees them move. Recommendation: yes, display order,
    with the model index kept for the application's own delegate calls; and a ScrollItem pattern on rows and cells so
    actions on offscreen rows scroll them in first.
16. **Empty child slots (N-21).** `Panel::TakeChild(index)` leaves an empty slot, so indices stay valid while an
    application takes children in a loop, and only `ClearChildren` compacts. A panel whose children are swapped for a
    long session keeps one empty slot per swap, and every walk visits them. Options: compact implicitly at a host safe
    point (the next preparation), which shifts indices an application may hold; add an explicit
    `Panel::CompactChildren()`; or make `AddChild` reuse the first empty slot. Recommendation: `CompactChildren()`,
    documented beside `TakeChild`, because neither implicit form keeps the index promise.

Asked on 2026-10-18, after Q11's evidence inventory and the Q12 work:

17. **Zero block median (N-20).** The paired-block judge forms a block's log effect from the two sides' medians and throws
    when one of them is zero; a timing metric such as `prepareP95Ms` can be zero for a whole block. Options: both zero is
    no change; one zero is clamped to a floor from the clock's resolution; or one zero makes the slot not estimable (p = 1,
    kept in the Holm family). Recommendation: both zero is an effect of 0; exactly one zero is not estimable, reported as
    such and never a regression or an improvement by itself (an exact budget's rise from zero is already caught by its
    exact rule).
18. **Where raw evidence goes (Q11).** A release asset of this public repository republishes what the raw packets hold:
    945 text files and 8 zips name a personal Windows profile, 16 files hold the e-mail address, 1,016 name the
    development machine. Moving them solves the size, not the privacy. Options: sanitize before uploading (which changes
    the bytes the packets' hash lists bind), keep the raw assets private and commit only their SHA-256 manifests (as the
    newer packets already do), or publish them as they are. Recommendation: keep them private and commit only manifests.
19. **Purge scope (Q11).** Q11's figures describe the zips plus the raw files of the ten packets #73 to #86 added (263 MiB;
    188 of main's 503 commits get new ids). The validator's personal-path and extension rules hold only if the 4,393 older
    raw files go too (298 MiB in all; about 486 commits rewritten), or with a waiver list for the older packets.
    Recommendation: remove the older raw files in the same single rewrite, so the validator needs no waiver.
20. **Curated files, commit identities and runner names (Q11).** After that purge, 47 curated files still name a personal
    path, 37 of them bound by a hash list. Commit metadata holds the author e-mail addresses. Receipts name GitHub's
    hosted runner machines. Recommendation: allow-list the 47 files by path and hash in a reviewed policy file instead
    of rewriting hash-bound bytes; leave commit identities alone; do not count hosted runner names as personal.

## Proposed sequence

- [x] 1. P0 hotfix: CR-1 to CR-4, each with the patch's test turned into an assertion, run under x64 Debug and ASan.
  - Implemented with ten tests; nine of them fail (four access violations) against the unfixed library. All 19
    non-interactive suites pass in x64 Debug and ASan Debug.
- [ ] 2. CR-5 through the shared commit helper (A1/A2); include the Grid group, double-click and UIA request paths.
  - CR-5 is done (see its entry). The read-validate-commit rewrite of A1 waits for Q1's pure getters (step 7); A2 is
    done with the Grid focus model (step 4).
- [x] 3. CR-6 through one logical-tree walker (A3), with text input, Tab and UIA tests on PageHost.
- [x] 4. The Grid focus model: CR-7 to CR-11. Visual changes regenerate the gallery and the design-system previews.
  - CR-7 to CR-11 are done and the design-system preview shows the focus ring and current cell. A regenerated gallery
    (x64 Debug, 2026-10-18) is byte-identical to the published sheets: no tile shows keyboard focus, so nothing is
    republished.
- [ ] 5. Accessibility publication: CR-12, CR-17, CR-18, CR-19 and the A4 edges.
  - CR-12 and CR-17 are done; CR-18, CR-19 and A4 remain.
- [ ] 5a. The later merges (#73 to #86; #85 and #86 add no defect): N-1 to N-15, N-19 and R-1 to R-12 are done, as
  are Q14 to Q16; N-16 to N-18, N-20 and the rest of N-21 remain, with Q11 and Q12 and the interactive run of Q13.
- [ ] 6. Input: CR-13, CR-14 and CR-20 to CR-24.
  - CR-14, CR-23 and CR-24 are done; CR-13 and CR-20 to CR-22 are done in code and wait for the agreed interactive run
    (Q13) for their focus-taking tests. CR-21's opening-key character is deferred.
- [ ] 7. CI and process: CR-15, CR-29 to CR-32. Then make `ci-gate` required.
- [ ] 8. Hygiene: CR-34 to CR-37 and the API-revision notes.
  - CR-35 (with the revision-4 notes) and CR-36 are done; CR-34 waits for Q11 and the purge's go-ahead; Q9 deferred
    CR-37 to the next API revision.
- [ ] 9. Simplifications A5 and A6 after the fixes, with paired measurements on the existing fixtures.
- [ ] 10. Validate per AGENTS.md:
  - `test.ps1` x64 Debug, Release and ASan Debug, and ARM64 builds;
  - `validate.ps1` and `format.ps1 -Check`;
  - the agreed `-Interactive` run for Menu and NativeTextInput.
  Consumer adoption stays separate.
  - Done (2026-10-18): at `4215010`, `test.ps1 -Full` passes all 19 suites with no capability skips in x64 Debug and
    Release, the three ARM64 configurations build, and the exact-pin consumer gate passes with the extended revision-4
    fixture. x64 ASan Debug failed one WindowHost check there once (CR-32); at `149e867`, which only splits that check,
    it passes all 19 suites, WindowHost passes three x64 Debug reruns, and ARM64 builds again. `validate.ps1` and
    `format.ps1 -Check` pass at the head. Open: the agreed interactive run (Q13).
  - Done (2026-10-19): at `b89f20a`, after the answers to Q11 to Q16, `test.ps1 -Full` passes all 19 suites in x64 Debug,
    Release and ASan Debug, the three ARM64 configurations build, the exact-pin consumer gate passes, and `validate.ps1`
    and `format.ps1 -Check` pass, with the tree unchanged by the runs. The complex-UI rounds are unpaired readings and
    establish no non-regression (Release dirty 621 to 663 FPS, clean 1,135 to 2,833 FPS with one noisy round). Open:
    the agreed interactive run (Q13), which also covers CR-13 and CR-20 to CR-22.

## Checked and found sound

The reviewers and the lead found these sound at HEAD:

- **Host and lifetime.**
  - Posted-payload token transport (token, window, message and type all matched).
  - The process-exit sweep.
  - Attach/Detach rollback.
  - `SetRoot` keeping the old root alive through reparenting.
  - First-full-present flag handling.
- **Grid selection.**
  - `GridSelectionModel` exception safety.
  - Q13 anchors.
  - Q14 Add/Remove/SetFocus semantics in Tree and Grid.
  - The exactly-one-selected event winning over the 20-item invalidation.
  - Quoted-TSV copy.
- **UIA infrastructure.**
  - Immutable cross-thread snapshots behind `atomic<shared_ptr>`.
  - No lock held across a callback or `UiaRaise*`.
  - Balanced COM reference counts.
  - Injective runtime ids.
- **TSF.** RequestLock, coalesced asynchronous grants, deferred self-edit notifications and session identity.
- **Embedded composition.** `Composite` does no heap work, layout or blocking.
- **Workflow security.** SHA-pinned actions, read-only default token, no `pull_request_target`, and no untrusted
  `${{ }}` inside `run:`.
