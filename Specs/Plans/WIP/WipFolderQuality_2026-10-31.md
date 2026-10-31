# WIP folder quality, 31 October 2026

Status: SPEC READY. This is a review of the folder and a sequence for repairing it.
No library, test or tooling code has changed for it.
Date: 2026-10-31.
Audited tip: `5f31f2b3` (merge of #157, 31 October 2026, 19:20 Europe/Paris). `include/` and `src/`
are byte-identical to delivery tip `f2940cc2`. Later `main` commits are CI evidence, changelog folds
and the 31 October reviews.
Owning contract: [specification authority](../../README.md). Delivery sequencing stays in
[the 15-day red-team](CodeReview_2026-10-31.md). DX-* repair text stays in
[the delivery review](DeliveryReview_2026-10-31.md). This plan does not become a third sequencer.

## Verdict

The remaining product work is real. The folder that is supposed to sequence it is not reliable enough
to use as the delivery queue.

At `5f31f2b3` the index listed 23 plans: 17 ACTIVE, 5 SPEC READY, 1 HOLD, about 19,800 lines of Markdown
under `Specs/Plans/WIP/`. The two plans dated today still match the library. Several older index rows
describe gates that later evidence already moved, and two plans number "Wave 1" differently. A consumer
pin of current `main` is still the wrong ship.

## What was checked

Read the index, `capabilities.json`, `README.md`, `Specs/Plans/Done/README.md`, and the status and
remaining-work sections of every indexed plan. Re-opened the stop-ship sites named by the 31 October
reviews. Read the four consumer locks in the sibling checkouts. Confirmed `762983a0` is an ancestor of
`HEAD` and of none of those locks. Confirmed the wall-compat branches
`origin/ej/wall-compat-redprism-20261028`, `origin/ej/wall-compat-redxe-20261028` and
`origin/ej/wall-compat-redsalamander-20261028` exist. Nothing was compiled or executed.

Not fully read: the bodies of `RichTextEditor_2026-10-17.md` (3,635 lines),
`ProductionReview_2026-10-05.md` (2,148) and its findings (2,527),
`RichTextAudit_2026-10-21/findings.md` (3,013), `ConsumerUiQualification_2026-10-11.md` (1,244),
and the middles of the 10 October and 28 October code reviews. Claims from those files below are from
their headers, checklists and the source lines this review opened. Wall-compat branch contents were not
diffed. CiRunScope's hosted documentation-only observation was not re-run.

## Delivery order

"Delivered" here means an honest pin. A green x64 suite is not that pin. Paired performance stays
unqualified; say so on the pin and do not wait for a policy that cannot currently pass before landing
correctness fixes.

The four locks, read from `Dependencies/DxUi.lock.json` on 31 October 2026:

| Product | Lock | API | What the next pin can take |
| --- | --- | --- | --- |
| RedPrism | `a059e07c` | 3 | The wall-compat branch, not `main`. |
| RedXe | `616104f3` | 4 | The wall-compat branch, not `main`. |
| RedSalamander | `4cdf1ce5` | 4 | A later `main` commit. The RedSalamander wall-compat branch is not this lock. |
| RedTilde | `6ed0512c` | 4 | A later `main` commit. This lock contains the rich editor. |

Keyboard-cancel `762983a0` is on `main` only. UIA focus still does not cancel
(`DxUi.Menu.cpp` 3512-3527). `RouteMenuKeyboardEvent` cancels every popup timer before it interprets
the key (5454-5457), and `WM_CHAR` / `WM_SYSCHAR` reach that function (5888-5915).

Land work in this order. [CodeReview_2026-10-31](CodeReview_2026-10-31.md) already sequences it as
Waves 0-4. [DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md) Wave 1 is the repair text for
four of those items (menu UIA, Enter, IME capacity, quoted paste). It leaves `Grid::SetModel`,
device-loss on resize, and the other confirmed P1s in the tree that would be pinned.

1. Answer the Wave 0 decisions in the 15-day review before coding the repairs that fork on them:
   preparation re-entry, `bad_alloc` at input, `\\` outside math rows, and `:::`, `accTitle`, `accDescr`.
   The recommended answers are already written there.
2. Menu ownership on `main` and on both product wall-compat branches in one effort. Cancel when keyboard
   or UIA focus actually takes the row. Refuse a stale hover timer. Leave an unmatched `WM_CHAR` alone.
   Copying the keyboard loop into `OnFocusChanged` would cancel too much.
3. Before RedTilde or RedSalamander pin `main`, land the user-visible editor repairs and the
   memory and device holes that are still in current source:
   - Quoted file paste appends the raw path between quotes (`DxUi.RichTextEditor.cpp` 427-444).
   - Source Enter, Shift+Enter and a refused paragraph command assign `L"\r\n"` (2349). Paste already
     selects the document ending (381-387).
   - `GetMaximumDocumentTextUnits` returns `size_t` max (`TextStoreTarget.h` 36-38).
   - `GetStatus` reports neither read-only nor multiline (`DxUi.TextStoreACP.cpp` 1335-1344).
   - `Grid::SetModel` returns on `bad_alloc` before `_model = model` (`DxUi.Grid.cpp` 1173-1180).
     The later catch at 1230-1233 is a different hole: the new model is already installed.
   - `ResizeBuffers` failure discards size-dependent resources and returns (`DxUi.WindowHost.cpp`
     4261-4269). Device loss on flush, draw or present also drops device resources and invalidates
     (4588-4598).
   - The rest of Wave 2 in the 15-day review: Grid/Tree bounds, capture cancel, line pitch, RTL table
     width, read-only shortcuts, math and Mermaid fallbacks, selection invalidation.
4. Pin RedTilde, then RedSalamander, to that `main` commit. Re-pin RedPrism and RedXe to their
   wall-compat branches. RedPrism stays on API revision 3 until that product moves.
5. Keep file drag-and-drop, S3-S7, API revision 5, and the rest of the 241-finding rich-text audit
   out of this pin. `commands-01` on the cited delete-column path is already guarded
   (`DxUi.RichTextCommands.cpp` 2096-2099). Do not schedule that path again.
6. Qualification stays in [ConsumerUiQualification_2026-10-11](ConsumerUiQualification_2026-10-11.md).
   Record the pin as a correctness pin with performance unqualified.

`apiRevision` stays 4.

## Adversarial readings

Five readings of the same tree. The order above is the resolution, not a sixth compromise that drops
the conflicts.

### Stop-ship maintainer

The next code that matters is the set that can corrupt a document, drop a device, or open the wrong
menu. DeliveryReview Wave 1 covers paste, IME capacity, Enter and UIA focus. It does not cover
`Grid::SetModel` or resize device-loss. Those two are in the bytes a pin of `main` would ship. The
241-finding audit and another review document do not reduce that set.

### RedTilde editor

Writers hit Enter, paste, IME, read-only keys, line pitch and RTL tables. They do not hit
`Separator` construction under `bad_alloc`, and no current consumer hits multi-select plus reorder
plus single-click invoke (DX-1). The editor plan's unchecked ARM64 Debug B2 row
(`RichTextEditor_2026-10-17.md` 260-262) records an unattributed Menu failure from CI `39587588088`.
[PopupReadiness_2026-10-17](PopupReadiness_2026-10-17.md) 37-40 records a later six-profile Menu pass
at SDK `7020121d`. That later pass does not attribute the earlier run, so the checkbox stays. The
index should stop listing B2 as the live editor task. The live editor task is Wave 2, then the pin.
File drop is specified and unimplemented; do not advertise it on the pin.

### Four-product release

A fix that exists only on DxUi `main` does not ship. RedPrism and RedXe cannot take `main`. Their
next ship is the menu port on the wall-compat branches. Editor repairs are RedTilde and
RedSalamander work. The menu port can land while Wave 2 is still open on `main`. Holding the menu
port for the rich-text audit ships nothing to the two products that diverged.

### Qualification skeptic

`capabilities.json` still lists `rich-text-editor`, native ARM64 runtime, physical touch, described-menu
qualification and consumer API-revision-4 adoption as pending. The editor plan's retained paired study
has 70 of 72 controls unstable and an unqualified base policy. The production-review index still
describes a repaired A/A with all 72 controls unstable. The 31 October reviews were not compiled.
A later hosted Menu matrix does not make performance accepted. State that on the pin. Do not treat
an unqualified paired study as a reason to leave the verified defects in the tree.

### Folder critic

The index cannot answer "what is next" without the 15-day review, and that review sends repair text
to a second plan whose Wave 1 is a different set. Three code-review plans are indexed. RichText audit
is SPEC READY in the index and IMPLEMENTING in the plan. About half the Markdown is two findings
registers and one editor ledger. Merged library work from September and October is still ACTIVE
because the open qualification rows were never split into the one qualification owner. Another review
of the same window will make the folder worse. This document is the last review admitted to the index
until the moves below are done.

### Where they agree

Current `main` is not a consumer pin. The menu bug is the only defect all four products need from
this tip. Performance acceptance is not available from the studies on record. S3-S7 and file drop are
outside the pin. `apiRevision` stays 4.

### Where the resolution picks a side

- The pin gate is Waves 0-2 of the 15-day review, not DeliveryReview Wave 1 alone. The maintainer's
  list and the editor's list both land before the RedTilde pin. DX-1 and the low DX items land on
  `main` in that same window so they are not left behind the pin; they do not block the wall-compat
  menu port.
- The menu port does not wait for Wave 2. The release reading wins for RedPrism and RedXe.
- Correctness is not frozen for the paired-performance policy. The skeptic's reading wins on the
  label of the pin and loses on blocking the fixes.
- Historical reviews stay reachable and lose sequencer status. The folder reading wins.

## Accuracy of the indexed plans

| Plan | Index label | Remaining work | Folder action |
| --- | --- | --- | --- |
| CodeReview_2026-10-31 | SPEC READY | Stop-ship list re-checked in this review. Header's `origin/main` SHA is behind `5f31f2b3`; library bytes match. | Keep. Sole delivery sequencer. |
| DeliveryReview_2026-10-31 | SPEC READY | DX-* repair text matches current lines. Cited `main` `07ff5741` is behind HEAD; library bytes match. Wave 1 is a subset of the pin. | Keep as the repair text. Point Wave 1 readers at the 15-day pin gate. |
| NativeBuildPolicyCoherence_2026-10-28 | ACTIVE | Policy targets exist. Release freeze, quiet paired performance and closeout are open. | Split the landed policy narrative to Done. |
| SubtleMarkdownPresentation_2026-10-27 | ACTIVE | Opt-in style exists. ARM64 runtime, physical input, paired performance and consumer qualification are open. | Keep until those rows close. |
| Msvc1452ReleaseOptimization_2026-10-23 | ACTIVE | Six-profile cohort is recorded done in the plan. Size matrix, five-repo report and performance holds are open. | Keep the open rows. Move the archived cohort narrative to Done. |
| SourceInkRendererQualification_2026-10-27 | ACTIVE | Renderer merge and six source-bound profiles are recorded. Consumer performance and parent platform holds are open. | Split. Holds point at the qualification ledger. |
| FrameUpdateAccounting_2026-10-26 | ACTIVE | Metric work is recorded. Performance acceptance and the historical PR #109 Menu failure are open. | Split. |
| ReadOnlySourceSnapshot_2026-10-26 | ACTIVE | API revision 4 merge is recorded. Paired performance is inconclusive. PR #108 Menu follow-up stays with PopupReadiness. | Split. |
| RichTextFileDrop_2026-10-20 | SPEC READY | D1-D6 not started, by the plan. This review did not search the editor for a partial implementation. | Keep. Out of the pin. |
| RichTextEditor_2026-10-17 | ACTIVE | M5/M6, D4c performance HOLD, and physical/platform/consumer gates are open. B2 is a retained unattributed failure, not the next feature. | Shrink to the open checklist. Index must stop leading with B2. |
| RichTextAudit_2026-10-21 | SPEC READY | Body says IMPLEMENTING. Continuation records merged math/Mermaid corrections. Waves are still mostly open. `commands-01` cited path is guarded. | Index becomes ACTIVE. Drop "no library code has changed". |
| PopupReadiness_2026-10-17 | ACTIVE | Six profiles pass at SDK `7020121d` (lines 37-40). Open: four modal fixture reruns (343-345) and unattributed historical failures. | Index must match the later pass. Keep the fixture row. |
| ProductionReview_2026-10-05 | ACTIVE | Unstable controls, policy, native ARM64, physical input and consumer qualification are open. | Evidence appendix of the qualification ledger. Not a sequencer. |
| CiRunScope_2026-10-04 | ACTIVE | Implementation is checked. Hosted documentation-only observation is unchecked. | Close after that observation, or say the observation is the only open row. |
| ConsumerUiQualification_2026-10-11 | ACTIVE | The shared HOLD ledger. All 20 HOLDs stay. | Keep. The only qualification owner. |
| CodeReview_2026-10-28 | SPEC READY | P1s still match source. Repair sequencing moved to the 15-day review. | Historical register. Remove from the active queue. |
| CodeReview_2026-10-10 | ACTIVE | Window #87-#96 local matrix is recorded done. ARM64, manual, performance and admin rows are open. | Historical register plus pointers into the qualification ledger. |
| ScopedTesting_2026-10-05 | ACTIVE | Tooling rows are checked. Closeout and foreground/ARM64 gates are open. | Persist the durable rule, then shrink. |
| SliderTouchHalo_2026-10-04 | ACTIVE | Library feature merged in #62. Physical touch and Q26 clipping are open. | Split the merge from the open rows. |
| GridTextOverflow_2026-09-21 | ACTIVE | Merged in #22. `grid-multiline-text` is supported. Native AT and consumer adoption are open. | Split. |
| MenuDescriptions_2026-09-21 | ACTIVE | Library merged. Plan-specific foreground and resource rows are still open. | Reconcile those rows with the qualification ledger before closing. |
| CodexBranchReview_2026-10-02 | ACTIVE | Ported fixes are in main. Wider delegate audit is open. | Keep only the delegate-audit row. |
| ConsumerUiImprovements_S3_S7_2026-10-11 | HOLD | Proposals, unimplemented. | Leave on HOLD. |

No indexed plan is empty. None of the merged-library plans can move to Done unchanged: each still names
a plan-specific open row. The split is "landed story to Done, open row stays indexed," not deletion.

## Folder defects

### Index prose sits in the entry list

`Specs/Plans/WIP/README.md` line 9, before this plan, is the blank-line rule, placed between the
native-build entry and Subtle Markdown. Entries are supposed to be separated only by blank lines.
Move the rule into the preamble. This change does that.

### Status words are undefined

`Specs/README.md` describes WIP, Done and a HOLD for a deferred consumer migration. It does not define
ACTIVE or SPEC READY. Plan bodies also say NOT STARTED, IMPLEMENTING and SPECIFICATION READY. The
preamble of the index now gives the three index words a working meaning. Promote that sentence into
`Specs/README.md` when it is accepted, and make plan status lines use only those words.

### Two Wave 1s

DeliveryReview Wave 1 is menu UIA, Enter, IME capacity and quoted paste, then lower-urgency DX items.
The 15-day review's Wave 1 is quoted paste, IME capacity, `SetModel`, device-loss and the corrected
menu ownership model. Enter is its Wave 2. RichText audit Waves 0-5 and the 28 October review's waves
are further sequences. An implementer who finishes "Wave 1" has not finished one agreed set.

Keep wave numbers inside the 15-day review only. Other plans cite DX-* and R28-* ids.

### Three code reviews are live

10 October, 28 October and 31 October are all indexed. The 31 October plan is the one that says what
to do next. The earlier two stay as evidence and lose the active label.

### The index disagrees with later text in the same folder

- Rich text index row: SPEC READY and "no library code has changed". Plan line 3: IMPLEMENTING.
  The 24 October continuation records merged corrections.
- Rich text editor index row still leads with the ARM64 Debug B2 hover failure. The plan's own later
  sections and PopupReadiness's SDK `7020121d` pass are more recent. B2 remains an unattributed
  historical failure, which is a different statement.
- PopupReadiness index row still leads with the 25 October ARM64 Debug overlapping-popup failure.
  The plan's checklist records the later six-profile pass and a still-open fixture rerun.

### Evidence weight

`ProductionReview_2026-10-05/findings.md` is 2,527 lines. `RichTextAudit_2026-10-21/findings.md` is
3,013. `CodeReview_2026-10-10/` holds qualification JSON and `review-repro-tests.patch`.
`Specs/README.md` says generated measurement reports live under `.build`, and reviewed evidence has
a home under `Measurements/`. These files are historical records, so they can move with their links
updated in one change. A partial move will break the reviews.

### One qualification owner, several ledgers

`ConsumerUiQualification_2026-10-11.md` says it is the single qualification owner. Source ink, the
read-only snapshot, frame-update accounting, production review, popup readiness and the editor plan
each carry long hosted and paired narratives. Open gates should be rows in the one ledger. Local
plans keep only attribution that the ledger does not.

## Folder repair sequence

- [ ] Promote the index preamble's three status words into `Specs/README.md`, and align every plan
      status line with them. RichText audit becomes ACTIVE. File drop says SPEC READY.
- [ ] Rewrite the stale index sentences for the editor plan, PopupReadiness and the rich-text audit.
      State what is still open. Point B2 and the 25 October ARM64 failure at their retained evidence.
- [ ] Stamp the 10 October and 28 October code reviews as historical registers. Drop them from the
      delivery queue. Leave the files reachable until a Done move updates every inbound link.
- [ ] Add one paragraph at the top of the 15-day review: Waves 0-2 are the pin gate; DeliveryReview
      Wave 1 is repair text for a subset; wave numbers elsewhere are not this sequence.
- [ ] Split each merged-library plan in the table above. The Done record states the landed scope.
      The indexed remainder is the open checklist, or a pointer into ConsumerUiQualification when
      the open row is a shared HOLD.
- [ ] Move the two findings registers, the 10 October JSON and the repro patch only together with
      updated links. Prefer `Measurements/` or a Done appendix. Do not leave a WIP path and a copy.
- [ ] Cap any plan that remains indexed at a short checklist. The editor plan and the production
      review are the first cuts. Receipts stay linked, not inlined.
- [ ] Admit no further review plan to this index until the stamps and the stale sentences above are done.

Move this plan to Done when those boxes are done and the index lists one delivery sequencer, one
qualification ledger, and only plans that still have unlanded work. DX-11-style optional items in
other plans are unaffected.

## Citation check

A later pass checked the code and spec anchors, not only the plan status lines.
`include/` and `src/` are still byte-identical to `f2940cc2`, so a line number that was
right on 31 October is still the same bytes. That does not make every older anchor right.

### Delivery review and the 15-day table

The repair sites named by [DeliveryReview_2026-10-31](DeliveryReview_2026-10-31.md) are still
the code they describe:

- `Eol` returns the line ending, else another line's, else `L"\r\n"`
  (`DxUi.RichTextCommands.cpp` 138-145). Source Enter still assigns `L"\r\n"`
  (`DxUi.RichTextEditor.cpp` 2349).
- `Document::MaximumUnits` is `64 * 1024` (`RichDocument.h` 94). Over-capacity throws
  `std::length_error` (`DxUi.RichDocument.cpp` 24 and 711). The native ceiling is still
  `size_t` max (`TextStoreTarget.h` 36-38). The client ceiling is still 65536
  (`TextInputServices.cpp` 17 and 75-77). Import still catches `std::logic_error`
  (`DxUi.RichTextEditor.cpp` 1847-1851).
- Quoted paste still appends the raw path (`DxUi.RichTextEditor.cpp` 441-444).
  `ValidLocalPathText` still rejects quotes and newlines (`DxUi.RichImagePaths.h` 43).
  Embedded paste still calls the same function (`Embedded.cpp` 1188).
- UIA focus still sets `keyboardIndex` and does not cancel (`DxUi.Menu.cpp` 3512-3527).
  Keyboard routing still cancels first (`5454-5457`), including `WM_CHAR` (`5888-5915`).
- `Grid::SetModel` still returns before `_model = model` on `bad_alloc` (`DxUi.Grid.cpp` 1177-1180).
- Resize device-loss still discards size-dependent resources only (`DxUi.WindowHost.cpp` 4262-4269).
  Present-path device-loss still drops device resources (`4588-4598`).
- `GetStatus` still sets only `TS_SS_NOHIDDENTEXT` (`DxUi.TextStoreACP.cpp` 1342-1343).
- Unfocused Tree text still passes `theme.selectionText` (`DxUi.Tree.cpp` 83). The resolver
  still returns that color when it clears 4.5:1 (`DxUi.Theme.cpp` 252-264).
- The mouse-up invoke path still uses `guard.IsCurrent()` after collapse and does not
  resume that guard (`DxUi.Tree.cpp` 1919-1939). `CollapseSelectionToItem` does resume
  (`3064`, `3072`).
- The typography-override label measure still uses `widthIncludingTrailingWhitespace`
  and does not set `baselineDip` (`DxUi.Layout.cpp` 281-283). `MeasureText` still uses
  ink width when wrapping and sets `baselineDip` (`103-105`).

The same paragraphs are still in the normative specs:

- UIA focus follows the menu session rule (`UI_InputAndAccessibility.md` 386-388).
  Keyboard navigation cancels cascade timers, and a queued timer must not replace a
  keyboard-focused child (`430-432`). The timer paragraph names keyboard navigation.
  It does not name UIA. The queued-timer sentence covers a child that already has
  `keyboardIndex`, which is what UIA focus writes.
- Inactive Tree text prefers `text`; Grid text prefers `selectionText`
  (`UI_ControlsAndLayout.md` 711-712). A model refresh does not end a Tree gesture
  (`752-756`).
- Hardware Enter falls back to raw CRLF, and Shift+Enter keeps that replacement
  (`UI_InputAndAccessibility.md` 1156-1157). `docs/rich-text.md` 256 still says
  "Enter as CRLF". The current contract requires the behavior the delivery plan
  wants to change. The code matches the contract. The repair is a contract edit
  plus a code edit.
- Additions do not increment `apiRevision`, and an equal revision does not permit
  mixing old and new objects (`Build_ToolchainAndConsumption.md` 221-239). The
  mixing sentence is 238-239, just past the plan's cited end line 235.

### 28 October P1 anchors

Each P1 was opened at the cited lines. The behavior is still there. These anchors
are off by a few lines, or name the wrong line for one clause:

| Claim | Cited | What is there |
| --- | --- | --- |
| R28-03 chunk size about 1,024 | `DxUi.RichDocumentLayout.cpp` 375 | Line 375 increments `reusedChunks`. `TargetChunkUnits = 1024` is line 14. The spacing skip `lineHeight > 0 && ! textOffset` is `DxUi.RichTextLayout.cpp` 321. Line 247 is the two-unit `textOffset`, not that test. |
| R28-04 `HorizontalTextBounds` returns `{left, width}` | `DxUi.RichTextLayout.cpp` 424 | Line 424 is `LineMetrics`. `HorizontalTextBounds` returns `{textLeft, textWidth}` at 430-434. The RTL subtraction at `DxUi.RichSemanticGeometry.cpp` 820 is the cited line. |
| R28-07 `\` is in the escape set | `DxUi.NativeMath.cpp` 491 | The set containing `\` is line 492. |
| R28-10 `FinishLinkLabel` | `DxUi.RichSourceInk.h` 264-270 | `FinishMarkerRun` is 269. `FinishLinkLabel` is 279-295 and still subdues the first unit, then the tail. |
| R28-12 setters that do invalidate | `DxUi.Grid.cpp` 6851 | `RequestInvalidate` is 6845. Line 6851 is the catch comment. `SetHeaderBusy` (1434) and `SetSortSpec` (1444-1471) still do not invalidate. `Tree::SetSelectedItemId` still ends at 871 with a snapshot refresh and no invalidate. `SetSelectedItemIds` still ends at 983 the same way. |
| R28-13 first key starts at the first row | `UI_ControlsAndLayout.md` 679 | That sentence is line 673. |

`OnBoundsChanged` is still absent on Grid and Tree. `ComputeVisibleSpan` still clamps only below zero (`DxUi.Grid.cpp` 7742). Tree paint still passes the raw vertical offset into that span (`DxUi.Tree.cpp` 1293-1294). Pending content still cancels capture (`DxUi.Controls.cpp` 2468-2472). `GetStatus` is unchanged. `\|` still returns the bar character (`DxUi.NativeMath.cpp` 214-215). The reserved flowchart words at `DxUi.NativeFlowchart.cpp` 177 still omit `accTitle` and `accDescr`, and `:::` is still taken as a description (`399-409`). Printable key-down still returns handled for any focused text-input control (`DxUi.WindowHost.cpp` 3720-3723), read-only included. UIA `ScrollIntoView` still scrolls and returns without a new publish (`DxUi.Accessibility.cpp` 7456-7459). `NotifyDataChanged` still reveals the focused row (`DxUi.Tree.cpp` 711-715). Typeahead still starts at the next row (`3476`).

`Tools/Test-CiGate.ps1` and `Tools/Modules/Documentation/Changelog.psm1` exist. The 28 October citations of those files used short paths. The dedupe comment is at `Changelog.psm1` 231, and the removed-fragment refusal is at 196-197.

### Rich-text audit register

`RichTextAudit_2026-10-21/findings.md` cites lines against `fa5821c8`. Of 243 unique `src/`, `include/`, `docs/`, `Tests/` or `Specs/` paths with a line number, 7 are still the same text at the same line, 162 are the same text elsewhere in the file, 33 no longer contain that line text, and 40 name a file that is not at that path. The median move is 65 lines. About 69 of the moves are more than 100 lines.

The missing paths are mostly the old `Scope.Tests.*` names. Those suites now live under names such as `Tests/Controls/Controls.Tests.RichSemanticDocument.cpp` and `Tests/Support/Support.Tests.RichSemantic.h`. `RichTextResources.h` is now `Tests/Embedded/Embedded.Tests.RichTextResources.h`. A citation written as `UI\_ControlsAndLayout.md` is the Markdown escape of a file that does exist.

This pass did not re-judge every one of the 1,092 finding narratives. A shifted line means the cited statement is still in the file. A gone line means that exact source text is not, which can be a fix or a rewrite. `commands-01`'s cited delete-column path is the guarded one at `DxUi.RichTextCommands.cpp` 2096-2099; the audit's line 1936 is 161 lines above that guard.

## Rejected conclusions

- The folder is full of finished plans that can be deleted. Every indexed plan still has an open row.
- DeliveryReview Wave 1 is the full stop-ship list. `SetModel`, resize device-loss and the other P1s
  re-checked above are still in `src/`.
- Library line numbers from the 31 October reviews are void because `main` moved. `git diff f2940cc2
  HEAD -- include src` is empty.
- The later six-profile Menu pass closes the editor plan's B2 checkbox. The plan requires attribution
  of that failed run. The index change is to stop presenting B2 as the current editor task.
- `commands-01` is still an open blocker on the delete-column path the 31 October review named.
  `column > 0` is present at `DxUi.RichTextCommands.cpp` 2096.
