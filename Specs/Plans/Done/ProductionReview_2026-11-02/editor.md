# Rich text editor core: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: `RichTextEditor` input (keyboard, character,
pointer), preparation and paint (`src/Controls/DxUi.RichTextEditor.cpp`), the helpers it drives (source guides, code
ink, overlay geometry) and the host preparation, paint, file-drop and text-input paths that consume its results.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

Result: two P1s.
- **EDIT-01.** Cosmetic decoration (source guides, code ink) withholds the whole frame and is restaged from zero on
  every edit. It absorbs RENDER-01 and RENDER-14.
- **EDIT-11.** One Backspace or Delete at the edge of a source-edit region changes text outside the region.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| EDIT-01 | P1 | Confirmed | Source guides and code ink withhold the frame and are restaged from zero on every edit | `src/Controls/DxUi.RichTextEditor.cpp:3672` | M |
| EDIT-11 | P1 | Confirmed | Backspace/Delete at a source-edit region edge edits outside the region and can break a closing fence | `src/Controls/DxUi.RichTextEditor.cpp:5113` | S |
| EDIT-02 | P2 | Confirmed | The host cancels the editor's own pointer gesture while its cosmetic stage is pending | `src/Controls/DxUi.Controls.cpp:2556` | S |
| EDIT-03 | P2 | Confirmed | IME composition and conversion underlines go stale when only composition metadata changes | `src/Controls/DxUi.RichTextEditor.cpp:4548` | M |
| EDIT-04 | P2 | Confirmed | A code tokenizer `bad_alloc` is retried on the same block forever and withholds every frame | `src/Controls/DxUi.RichTextEditor.cpp:6859` | S |
| EDIT-06 | P2 | Confirmed | Toggling a disclosure (or zooming) discards all geometry and scrolls back to the caret | `src/Controls/DxUi.RichTextEditor.cpp:2256` | M |
| EDIT-07 | P2 | Confirmed | Multi-line Tab/Shift+Tab in a code block nested in a list or quote edits the container prefix | `src/Controls/DxUi.RichTextEditor.cpp:6178` | M |
| EDIT-09 | P2 | Confirmed | Auto-pair doubles quotes, underscores and asterisks inside words and never types over a closer | `src/Controls/DxUi.RichTextEditor.cpp:5506` | S |
| EDIT-10 | P2 | Confirmed | Read-only editors report editing chords (Ctrl+X/V/Z/Y/B/I, Shift+Insert/Delete, Ctrl+Enter) as handled | `src/Controls/DxUi.RichTextEditor.cpp:6031` | S |
| EDIT-05 | P3 | Confirmed | Ctrl+Tab element cycling never yields to host traversal, contradicting the docs | `src/Controls/DxUi.RichTextEditor.cpp:5975` | S |
| EDIT-08 | P3 | Plausible | Tab leaves the editor while the Markdown syntax for the current revision is still pending | `src/Controls/DxUi.RichTextEditor.cpp:6231` | M |
| EDIT-12 | P3 | Confirmed | Text paste at an empty or synthesized table-cell site bypasses the cell transaction | `src/Controls/DxUi.RichTextEditor.cpp:6094` | S |
| EDIT-13 | P3 | Confirmed | A throwing Ctrl+Shift+C code-copy callback leaves the key unhandled after delivery | `src/Controls/DxUi.RichTextEditor.cpp:5865` | S |
| EDIT-14 | P3 | Confirmed | Ctrl+Enter in a link with no link handler skips Exit-code-block and Toggle-task | `src/Controls/DxUi.RichTextEditor.cpp:6013` | S |
| EDIT-15 | P3 | Confirmed | Default-presentation Paint creates marked-text and native-object brushes on first use | `src/Controls/DxUi.RichTextEditor.cpp:4616` | S |
| EDIT-16 | P3 | Confirmed | The Source gutter width follows document length, so crossing 10^k units reflows the whole layout | `src/Controls/DxUi.RichTextEditor.cpp:3021` | S |
| EDIT-17 | P3 | Partly confirmed | Whole-document source copies on some keystroke paths | `src/Controls/DxUi.RichTextEditor.cpp:5349` | S |
| EDIT-19 | P3 | Confirmed | The Backspace/Delete run precompute records a predicted revision and caret before `Erase` runs | `src/Controls/DxUi.RichTextEditor.cpp:6270` | S |
| EDIT-20 | P3 | Plausible | Crossing native-object rejection ranges return `E_INVALIDARG` without recording any state | `src/Controls/DxUi.RichTextEditor.cpp:3540` | S |

## P1 findings

### EDIT-01 — Source guides and code ink withhold the frame and are restaged from zero on every edit

- **Severity** P1 (reviewer P1, verifier P1)
- **Category** perf, bug
- **Verdict** Confirmed
- **Confidence** high for the mechanism; medium for wall time (not measured)
- **Effort** M
- **Merged** RENDER-01, RENDER-14 — see `rendering-images.md`
- **Introduced**:
  - source guides: `5277381d`, with follow-ups `82729d33` and `a8ada883`;
  - code ink: `6c5eeaa9`;
  - the same S_FALSE gating for source ink predates the window (`e4f1790d`).
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp`:
    - `Changed` 3112-3131;
    - `RetireLayout` 3288-3293;
    - `ImportTextInputState` 4566;
    - `PrepareContent` 3669-3678 and 4034-4043;
    - `PrepareCodeInk` 6783-6896 (one call per turn at 6889);
    - `PrepareSourceGuides` 6897-6915;
    - paint gates 4700, 4708-4709 and 4877-4878.
  - `src/Controls/DxUi.RichSourceGuides.h:43-139`.
  - `src/Controls/DxUi.Controls.cpp`: 2440-2441, 2509, 2565, 2575, 2678.
  - `src/Controls/DxUi.WindowHost.cpp`: 2538, 4642-4649.
  - `src/Rendering/Embedded.cpp`: 488-489, 555-563, 696-697, 785-786, 832-833.
  - Contract: `Specs/UI/UI_ControlsAndLayout.md:1184` and `:2206-2213`.
- **What**:
  - **Everything restarts from zero on every revision.**
    - `Changed` runs on every source edit, and `RetireLayout` on every view-state retirement. `ImportTextInputState`
      calls `RetireLayout(true)` on every transient text-service import.
    - Together they clear `sourceInk`, `sourceGuides`, `codeInkDocument`, `codeInk`, `codeInkStaged` and
      `codeInkComplete`.
    - `RichSourceGuides::Clear` also drops the published `numbers_` and `marks_`.
    - `Begin` (`DxUi.RichSourceGuides.h:58-79`) copies the whole source (`source.Slice(0u, source.Length())`) into a
      new stage at line 1.
  - **The guides cost one DirectWrite layout per physical line, with no cap.**
    - `Advance` (80-139) builds a complete `Document` → `Project` → `PreparedLayout` for every physical line. That is
      one `CreateTextFormat` and one `CreateTextLayout` each (`src/Controls/DxUi.RichTextLayout.cpp:257`, `:291`).
    - Each turn is capped at 4 layouts (`if (layouts == 4u) return false;`, 92-93) and 256 work units. Nothing caps
      the total and nothing limits it to the viewport.
    - Whitespace marks cost one work unit per source unit, plus two `SourceCaret` calls per space, tab or line ending.
  - **Code ink re-tokenizes the whole document.** `PrepareCodeInk` restarts at element 0 for every new semantic
    document and makes one tokenizer call per turn (6889).
  - **The editor reports this cosmetic work as pending content.**
    - `PrepareContent` returns S_FALSE from `PrepareCodeInk` (3669-3671, 4034-4036) and from
      `PrepareSourceInk() && PrepareSourceGuides(host) ? S_OK : S_FALSE` (3672, 4037).
    - `ControlHost::PreparePendingPageLayout` stores it as `_contentPreparationPending` (`DxUi.Controls.cpp:2440-2441`),
      aggregates it (2509) and returns false (2575).
  - **The native host draws and presents nothing.** `ControlHost::Render` returns before `BeginDraw`
    (`DxUi.WindowHost.cpp:4642-4649`) and re-invalidates, because `_contentPreparationContinues` is set.
    - The swap chain keeps the last presented frame. The window shows stale content: the typed character, caret and
      selection are missing. Every control in the window is frozen, not only the gutter.
    - Turns are back-to-back `WM_PAINT` dispatches with no Present, so cost is limited by CPU.
    - Input messages are handled before `WM_PAINT`, so each keystroke restarts the stage. Continuous typing therefore
      stops the window from repainting.
  - **The embedded host composites nothing and refuses pointer input.**
    - `EmbeddedHost::Prepare` sets `coherent=false` and `interactionLayoutAvailable=false`
      (`src/Rendering/Embedded.cpp:488-489`), then returns S_FALSE before `BeginDraw` (555-563).
    - `Composite` returns S_FALSE and draws nothing (696-697). The view is not composited at all, not even with its
      previous texture.
    - `DispatchPointer` refuses all pointer input (832-833), and text-geometry queries refuse (1483, 1503).
    - Keyboard input continues, because the editor returns true from `AcceptsTextInputDuringPreparation` (812-816,
      3322-3325).
    - `include/DxUi/Embedded.h:150-151` tells embedders to prepare on a later owner turn. The reference sample
      presents once per turn (`Samples/EmbeddedControls/Main.cpp:171-203`), so that means one turn per displayed frame.
  - **These are also held back while it runs:**
    - `PublishPreparedContent` (`DxUi.Controls.cpp:2678`) defers TSF `NotifyTextInputLayoutChanged` and the post-frame
      UIA refresh.
    - File-drop preview is refused (`DxUi.WindowHost.cpp:2538`; `Embedded.cpp:785-786`).
  - **These are not held back:**
    - Text and selection reach TSF immediately (`SelectionChanged` → `SyncTextInput`, 3221-3222).
    - The native host refreshes UIA after every handled key (`DxUi.WindowHost.cpp:3856-3858`).
  - **Scope.** All three features are opt-in:
    - `sourceLineNumbers` and `sourceWhitespaceMarks` default to false (`include/DxUi/DxUi.h:3195-3196`);
    - code ink needs `SetCodeTokenizer`.
  - **The gating is currently specified**, so the fix must change the spec:
    - `UI_ControlsAndLayout.md:1184` says the editor "stages all ranges until the current semantic publication is
      complete".
    - `:2210-2213` says `preparationPending` includes code ink, source ink and source guides. Only overlay dirtiness
      "does not withhold an otherwise ready frame".
- **Failure scenario** (turn counts are exact; wall time is estimated, not measured):

| Case | Turns per keystroke | Work per keystroke | Effect |
| --- | --- | --- | --- |
| Source view, `sourceLineNumbers=true`, 64 Ki units in about 1,600 lines of 40 units | About 400 (4 lines per turn, 168 of the 256 work units) | About 1,600 format + layout pairs and a 128 KB copy | Native: tens of ms of stale frame (estimate). Embedded at 60 Hz: about 6.7 s per keystroke with the view not composited |
| Worst case: 65,536 line-feed-only lines | About 16,384 | 65,537 retained `PreparedLayout` objects, each with its own `IDWriteTextFormat` and `IDWriteTextLayout` | An unbounded resource count. Embedded: about 4.5 min per keystroke |
| `sourceWhitespaceMarks=true` only | 256 per edit | One work unit per source unit | Same withholding |
| Rich view with a tokenizer and 50 fenced blocks | 50 + elements / 256 | 50 tokenizer calls over whole blocks | Same withholding |

  While typing continues, the stage restarts each time, so neither host shows the typed text until the user pauses.

- **Fix**. Both parts are required: each one alone leaves a P1.
  1. **Never withhold the frame.**
     - Call `EnableVisualContentPreparation()`, override `PrepareVisualContent` and
       `CancelVisualContentPreparation`, and run code ink, source ink and source guides there.
     - Visual preparation is excluded from the `PreparePendingPageLayout` result (`DxUi.Controls.cpp:2451-2458`,
       2575). `docs/hosting.md:43` already says it does not hold input readiness.
     - `PrepareSourceGuides`, `PrepareSourceInk` and `PrepareCodeInk` must no longer feed the S_FALSE at 3669-3672 and
       4034-4037.
     - **Keep drawing the previous publication** instead of hiding it behind `Current(...)` and `codeInkComplete`:
       - lines before the change: keep their y;
       - lines after the change: shift by the y delta of the edit (`ChangedRangeFrom` plus the line-ending delta);
       - drop the shifted part until restaged when the delta is not exact, for example wrapped Source.
     - Never draw numbers at stale positions.
     - Pitfalls:
       - Nothing re-wakes visual work: `_contentPreparationContinues` covers content only. Request the next turn
         explicitly (`RequestVisualContentPreparation()` plus `RequestInvalidate()`), and stop requesting once the
         stage is idle.
       - `PrepareVisualContent` is `noexcept`. Catch `std::bad_alloc` and keep the atomic publication pinned by
         `TestSourceGuideAllocationAtomicity` (`Tests/Embedded/Embedded.Tests.RichGeometryResources.h:539-602`).
       - Move the brush pre-warm (`host.GetSolidBrush(host.GetTheme().subduedText)` at 6913, 3649-3653) with the stage,
         so Paint still allocates nothing.
  2. **Make the work proportional to the viewport or the change.**
     - **Guides:**
       - Stage only the lines whose y lies in the viewport plus one viewport of margin on each side.
       - Get the first line number by counting line endings in the snapshot prefix. CRLF counts as one ending, and
         the trailing empty line stays addressable.
       - Prepare digit glyphs once per font, DPI and resource generation. Either use `GetGlyphIndices` with
         `DrawGlyphRun`, or keep a bounded LRU of at most 256 number layouts.
       - Stage marks only for the same window, from the snapshot, without copying the whole source.
       - Add the window to the `Current(...)` key.
     - **Code ink:** keep it per code-block identity and re-tokenize only blocks whose source changed
       (`ChangedRangeFrom`). Shift unaffected ranges by the edit delta.
     - Optionally budget a turn by work or time rather than "one callback".
  3. **Change the contract in the same change.** In `Specs/UI/UI_ControlsAndLayout.md`, under "Retained
     RichTextEditor v0 preview (D2c)":
     - Line 1184 ("stages all ranges until the current semantic publication is complete"): code ink becomes
       non-withholding decoration, published when ready.
     - The cooperative-stage paragraph at 2206-2213 (currently under "Native Mermaid flowchart profile"): remove code
       ink, source ink and source guides from `preparationPending`, or report them separately with a new
       `decorationPending` field, and state that they never withhold a ready frame, input or publication.
     - Update `docs/rich-text.md:314-325` to match.
- **Regression test**:
  1. `Tests/Embedded/Embedded.Tests.RichGeometryResources.h`, extending `TestEmbeddedSourceGuidesPixels`.
     - Setup: a 2,000-line source in Source view with `sourceLineNumbers=true` and `sourceWhitespaceMarks=true`.
       Prepare to completion with `PrepareRichEmbeddedContent`.
     - Action: type one character, then run exactly one `view.Prepare(480u, 240u, 96.0f)`.
     - Assert S_OK. Assert the readback shows the edit, and that gutter pixels above the edited line match the
       pre-edit capture.
     - At `6df17007` this fails: the call returns S_FALSE and `Composite` returns S_FALSE.
     - Then keep preparing until idle. Assert the gutter matches a full-prepare capture and `NeedsPreparation()`
       becomes false; there must be no endless repaint.
  2. Same file: add a diagnostic count of retained number layouts or glyph runs. After a 10,000-line source, assert
     it is at most the visible line count plus a constant. At `6df17007` it equals the line count.
  3. `Tests/Embedded/Embedded.Tests.Embedded.cpp` near 1729: a counting tokenizer and 20 fenced blocks.
     - After an edit inside one block, assert the frame is presented within the geometry turns plus one.
     - Assert the tokenizer was called once, not 20 times.
  4. Replace the yield assertion at `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:645-646`
     (`Check(view.NeedsPreparation(), "large guide preparation yields before publication")`), which pins today's
     behaviour. Keep the supersession check at 647-649.
- **Verifier notes**:
  - RENDER-01 said S_FALSE does not block the frame. That is wrong; the trace above shows it does. The visible effect
    is a frozen native frame or an uncomposited embedded view, not an empty gutter.
  - The reviewer's 20 s figure applies only to frame-paced embedders; native turns are not paced at 60 Hz.
  - The reviewer's "UIA deferred" is only partly true; see "not held back" above.
  - EDIT-02 and EDIT-04 depend on this finding.
- **Lead note**:
  - EDIT-01 is the canonical record for RENDER-01 and RENDER-14 (same root cause, one fix).
  - This fix keeps drawing the previous gutter with the edit delta applied, and drops lines whose position is unknown.
    That reconciles the "never withhold the frame" decision with the misalignment concern in RENDER-01's original
    fix step 4.

### EDIT-11 — Backspace/Delete at a source-edit region edge edits outside the region and can break a closing fence

- **Severity** P1 (reviewer P2, verifier P2; raised by the lead)
- **Category** bug, data integrity
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Introduced** `78770e4e` (the word-wise clamp was added without clamping the grapheme path)
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - `Erase` source-editing branch 5092-5121: the word path clamps at 5106-5110; the grapheme path does not, at
    5113-5116.
  - `OnKeyDown` run precompute 6258-6259.
  - `UpdateSemanticSourceRegion` 2293-2297.
  - `BeginSemanticSourceEdit` 2184-2199.
- **What**:
  - Ctrl+Backspace and Ctrl+Delete clamp the deletion to `sourceEditing->region.source`, so the code intends to keep
    deletions inside the region.
  - Plain Backspace and Delete use `GetTextElementDeletionRange` unclamped, so at the region edge they remove one text
    element outside the region: a space, a bracket, or a whole CRLF.
  - `UpdateSemanticSourceRegion` then sees `change.begin < range.first` or `change.previousEnd > range.last`, and
    silently ends the source-edit session (2294-2297).
- **Failure scenario**:
  1. **Inline link.** The source is `see [x]` immediately followed by `(u)` and CRLF: an inline link with `[` at
     offset 4 and the region [4,10). It is spelled in two pieces here only because the repository link validator
     reads code spans.
     1. F2 on the link. `BeginSemanticSourceEdit` puts the caret at `source->first` = 4 (2193).
     2. Backspace. The precompute and `Erase` compute [3,4), and `ReplaceImpl(3,4)` deletes the space before `[`.
     3. Because 3 is less than 4, the session ends.
     4. The paragraph now renders "seex" as a link. The user deleted text they could not see in the source envelope,
        and left editing mode.
  2. **Code block, the structural case.** Source `~~~js\r\nx\r\n~~~\r\nnext para\r\n`. A block region ends at the
     closing fence's line end (`block.source.last = lineEnd_`, `src/Controls/DxUi.MarkdownBlocks.h:582`).
     1. F2 into the block, then press End or Ctrl+End so the caret sits at region end (after the closing `~~~`).
     2. Press Delete. The CRLF after the fence is removed as one element.
     3. The line becomes `~~~next para`. A closing fence cannot carry text, so the block stays open and swallows the
        rest of the document.
     4. The session ends. One keystroke has corrupted the document's structure, and only Undo restores it.
- **Fix**:
  - In the non-word branch of `Erase`'s source-editing path, clamp `first` and `last` to
    `_state->sourceEditing->region.source`, exactly as the word branch does.
  - Return false (unhandled; no edit, no history entry) when the clamped range is empty.
  - Apply the same clamp in the `OnKeyDown` precompute (6252-6259), so the typing run never records an edit that does
    not happen.
  - Keep the session-ending rule in `UpdateSemanticSourceRegion` for genuine foreign edits.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h`, beside the existing source-region tests.
  - **Case 1:** the inline document above. Call `BeginSemanticSourceEdit` (or F2), then `OnKeyDown(VK_BACK, 0)`.
    Assert `GetText()` is unchanged and `GetSemanticSourceRegion()` is still set with the same session.
  - **Case 2:** the fence document above. Begin block source editing, set the caret to region end, then
    `OnKeyDown(VK_DELETE, 0)`. Assert the text is unchanged and `GetMarkdownSyntax()` still has a closed fenced block
    followed by a paragraph.
  - Both fail at `6df17007`: text changes and the region ends.
  - Keep the existing Ctrl+Backspace clamp assertions.
- **Verifier notes**: traced the region bounds (2184-2199; `MarkdownBlocks.h:572-582`), the deletion ranges, and the
  session-retirement rule. The contract sentence "Source-region edits never reach outside their active interval"
  (`Specs/UI/UI_InputAndAccessibility.md:1309`) sits in the Tab paragraph. The word-path clamp is the clearer evidence
  of intent.
- **Lead note**: raised to P1.
  - A single Backspace or Delete at the edge of a source-edit region modifies text outside the region. It can break a
    closing code fence so that the block swallows the rest of the document.
  - That is silent structural corruption from one keystroke, even though Undo exists.

## P2 findings

### EDIT-02 — The host cancels the editor's own pointer gesture while its cosmetic stage is pending

- **Severity** P2 (reviewer P1, verifier P2)
- **Category** bug
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Depends on** EDIT-01
- **Location**:
  - `src/Controls/DxUi.Controls.cpp:2530-2563` (`pendingOnPath`, then `CancelCapturedControlInteraction`);
  - `src/Controls/DxUi.WindowHost.cpp:5473-5498`;
  - `src/Controls/DxUi.RichTextEditor.cpp`: `OnMouseDown` 6451, 6457, 6472, 6591; `OnCaptureLost` 7325-7337;
    `PrepareContent` 3664-3678.
- **What**:
  - The R28-02 fix is correct for unrelated controls: `pendingOnPath` cancels only when the gesture owner, or an
    ancestor on its path, is content-pending. But the editor is its own gesture owner.
  - `OnMouseDown` accepts a press once `LayoutIsCurrent` is true, and takes capture.
  - The next host pass is already queued by the EDIT-01 re-invalidate. It calls `PrepareContent`, which returns S_FALSE
    from code ink, source ink or guides.
  - The host then applies "pending geometry cannot receive the matching Up" and calls `CancelCapturedControlInteraction`.
    That reaches `OnCaptureLost`, which clears `dragging`, `scrollbarDragging`, `taskPressPlan` and the code-press
    state. Geometry is current, so the rationale does not apply.
- **Failure scenario**:
  1. Source view with line numbers, a 2,000-line file, native host. Type a character.
  2. Within the cosmetic window (estimated tens of ms to under a second), press and drag to select, or grab the
     scrollbar thumb. The drag stops extending on the next turn.
  3. In Rich view with a tokenizer, a task-checkbox or code-copy press made just after an edit is cancelled the same
     way.
  - In embedded hosts the press does not reach the editor at all: `DispatchPointer` refuses while not coherent
    (`src/Rendering/Embedded.cpp:832-833`).
- **Fix**: closed by EDIT-01 part 1, because cosmetic stages stop reporting content-pending. A host-level guard against
  the same class is optional: let a control report "geometry current" separately from "content pending", and skip
  cancellation when the gesture owner's geometry is current.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextEditor.cpp`, modelled on
  `TestRichTextEditorPointerGestureExclusion` (860).
  1. Set a tokenizer and two fenced blocks; prepare to idle.
  2. `OnChar('x')`. Prepare until `DebugGetState().layoutReady && DebugGetState().codeInkPreparationPending`.
  3. `OnMouseDown` on text, through the host so capture is taken. Run one host preparation pass.
  4. `OnMouseMove` to another line.
  5. Assert `IsSelectionGestureActive()` and that the selection extended. At `6df17007` the capture is cancelled.
- **Verifier notes**: the gesture's own `RequestContentPreparation` (3225) does not cause the cancel. With cosmetics
  complete, `PrepareContent` returns S_OK. Exposure is bounded to the cosmetic window after an edit, resize or
  `RetireLayout`, hence P2.

### EDIT-03 — IME composition and conversion underlines go stale when only composition metadata changes

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** ime
- **Verdict** Confirmed (embedded path proven statically; IMM path plausible)
- **Confidence** medium-high
- **Effort** M
- **Introduced** `16c187b2` (prepared overlay geometry) combined with `da86f66e` (no-op import early return)
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp`: 3337-3388 (`PrepareOverlayGeometry`, which reads composition only when
    `overlaySelectionDirty`) and 4545-4549 (early return).
  - `src/Rendering/Embedded.cpp:1418-1426`.
  - `src/Controls/DxUi.NativeTextInput.cpp`: 1292-1329, 1868-1889, 2135-2173.
- **What**:
  - Paint used to read `TryReadNativeTextInputState` every frame. Now the composition and conversion rectangles are
    prepared only when the overlay is dirty.
  - An identical silent import returns before `SelectionChanged` (4548), so nothing dirties the overlay.
  - Several producers change only metadata, and none of them dirties or invalidates the control:
    - `SetNativeTextInputCompositionRange`;
    - the IMM path, which imports and then sets the conversion target;
    - `UpdateNativeTextInputCaret`;
    - embedded preview, which writes the target and then imports identical text.
  - This breaks `Specs/UI/UI_InputAndAccessibility.md`: "native composition rectangles are staged with current layout
    before painting".
- **Failure scenario**:
  - **Embedded.** `ApplyTextInput` Preview A with conversion target [3,4), then prepare. Then Preview B with identical
    text and caret but target [4,5). `++_state->revision` repaints, but with the old `conversionRectangles`, so the
    thick underline stays on clause 1.
  - **IMM.** Moving the target clause with Right gives the same stale result when the IME keeps the caret unchanged.
- **Fix**:
  - Add `ControlHost::_nativeTextInputCompositionRevision`. Bump it in `SetNativeTextInputCompositionRange`,
    `ApplyNativeTextInputCompositionStateToCache` and `ClearNativeTextInputCompositionState`, then call
    `RequestContentPreparation(false)` and `RequestInvalidate()` on the native-input control.
  - `PrepareOverlayGeometry` stores the revision it consumed and re-stages the composition rectangles when it differs.
  - Keep the 4548 early return, which protects prepared commands.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextInput.cpp`, next to the composition capture at
  1334-1338 (which asserts nothing about the underline).
  - Run the embedded Preview A, then B, as above, with a prepare after each.
  - Expose prepared conversion rectangles in `DebugGetState()`, or sample pixels under clause 2.
  - Assert the rectangles moved to [4,5). This fails at `6df17007`.
- **Verifier notes**: the IMM half depends on the IME reporting an unchanged caret while the target clause moves.

### EDIT-04 — A code tokenizer `bad_alloc` is retried on the same block forever and withholds every frame

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** bug
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Depends on** EDIT-01 (frame withholding); keeps its own fix
- **Introduced** `6c5eeaa9`
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp:6841-6862`;
  - `src/Controls/DxUi.Controls.cpp:2440-2447`, `:2565`;
  - `src/Controls/DxUi.WindowHost.cpp:4642-4649`;
  - `src/Rendering/Embedded.cpp:489`, `:555-566`, `:818-826`.
- **What**:
  - Any other tokenizer exception leaves the block in ordinary ink and advances the cursor. `std::bad_alloc` returns
    `E_OUTOFMEMORY` before `++codeInkCursor`, so every later pass calls the tokenizer on the same block again.
  - The host records a content failure with no self-wake (`complete=false`, `_contentPreparationContinues=false`).
  - **Native:** each later paint attempt retries and aborts, so the window never presents while the block exists.
  - **Embedded:** `Prepare` returns `E_OUTOFMEMORY` with `interactionLayoutAvailable=false`, so keyboard and pointer
    are both refused. The view stays dead until the application retries, and each retry fails again.
  - This contradicts `Specs/UI/UI_ControlsAndLayout.md:1184`: "tokenizer failures use ordinary ink".
- **Failure scenario**: an application tokenizer throws `std::bad_alloc` deterministically for one pathological block
  (for example a quadratic `reserve` on a 60K-unit block). The window or view stops presenting for as long as that
  document is shown.
- **Fix**:
  - Treat consumer `bad_alloc` as a failure of this block only: keep the block in ordinary ink, advance
    `codeInkCursor`, and return S_FALSE to continue. A single retry per document revision is optional.
  - Never return a failing HRESULT without progress.
  - EDIT-01 removes the frame withholding, but this cursor fix is still required so the stage cannot spin.
- **Regression test**: `Tests/Embedded/Embedded.Tests.Embedded.cpp` near 1729.
  - The tokenizer throws `std::bad_alloc` for language `boom`. The document has a `boom` block and a `cpp` block.
  - Prepare repeatedly. Assert `Prepare` returns S_OK, the view becomes ready, and the `cpp` block is inked.
  - At `6df17007`, `Prepare` keeps returning `E_OUTOFMEMORY`.
- **Verifier notes**: there is no busy loop. The failure repeats on every invalidation and blocks frames until then.

### EDIT-06 — Toggling a disclosure (or zooming) discards all geometry and scrolls back to the caret

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** bug
- **Verdict** Confirmed
- **Confidence** high
- **Effort** M
- **Introduced** `c1a77e42` (before the window)
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - `SetSemanticDisclosure` 2203-2227;
  - `SourcePresentationChanged` 2256-2276;
  - `RetireLayout` 3305 (`Clear`) and 3310 (`revealCaret = true`);
  - `SelectionChanged` default `revealCaret=true` (`include/DxUi/DxUi.h:3721`);
  - `UpdateCaretScroll` 4156-4162;
  - `OnMouseDown` 6481-6487;
  - `SetAppearance` 1454-1458.
- **What**: a pure view-state change re-reveals the caret.
  - The disclosure click returns before `HitSource` (6487), so the caret does not move.
  - `SourcePresentationChanged` calls `RetireLayout()`, which does a full `Clear` instead of reusing chunks, and sets
    `revealCaret`. `SelectionChanged()` sets it again.
  - After preparation, `UpdateCaretScroll` scrolls to the caret.
  - Zoom goes through `SetAppearance` → `RetireLayout()` and behaves the same.
- **Failure scenario**: a long document with the caret at the top.
  1. Ctrl+Tab to a `details` disclosure near the end. `SetScrollOffsetDip` sets `revealCaret=false` (2955).
  2. Press Space (5883), the documented way to toggle it.
  3. The block toggles and the view jumps back to the top. A summary click or UIA ExpandCollapse does the same.
- **Fix**:
  - Remove `revealCaret = true` from `RetireLayout`. Callers that move the caret request the reveal themselves.
  - `SourcePresentationChanged` for a disclosure calls `SelectionChanged(true, false, true, false)`, keeps the toggled
    element's top at the same viewport y after preparation, and uses `RetireLayout(true)` so unaffected chunks are
    reused.
  - Whether zoom should reveal the caret is a product decision (question 4).
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h`.
  1. 200 paragraphs, then a disclosure; caret at 0.
  2. `SetScrollOffsetDip` to the disclosure; prepare; click the summary (or Ctrl+Tab then Space); prepare.
  3. Assert `GetScrollInfo()->offsetDip` is unchanged within rounding and `DebugGetState().reusedGeometryChunks > 0`.
  4. At `6df17007` the offset becomes 0.
- **Verifier notes**: the existing behaviour predates the window. The reveal on zoom may be intended.

### EDIT-07 — Multi-line Tab/Shift+Tab in a code block nested in a list or quote edits the container prefix

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** bug
- **Verdict** Confirmed (both cases)
- **Confidence** high
- **Effort** M
- **Introduced** `a6adcc88`
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp`: 6146-6157 (scope = `node.block.content`), 6178-6180 (line-start scan),
    6191-6198 (remove/insert at the raw line start), 6203-6204;
  - `src/Controls/DxUi.MarkdownBlocks.h:572-582` (`content.first` is the first content line's `cursor.offset`; later
    lines keep their container prefixes).
- **What**:
  - For every selected line after the first, `line` is the physical line start, which sits before `> ` or the list
    content indent.
  - Tab inserts indentation before the container marker. Shift+Tab removes container indentation.
- **Failure scenario** (`sourceTabSpaces=4`):
  - **(a) List-nested fence.** Source `1. item\n   ~~~\n   a\n   b\n   ~~~\n`, with `a`..`b` selected.
    1. Shift+Tab leaves line `a` unchanged.
    2. It removes the 3 container spaces before `b`.
    3. `b` drops to column 0, so the list item and its fence close.
    4. `   ~~~` opens a new fence that runs to the end of the document.
  - **(b) Quote-nested fence.** Source `> ~~~\n> a\n> b\n> ~~~\n`, with both lines selected.
    1. Tab inserts 4 spaces before the second `>`.
    2. `    > b` is no longer a quote marker, and lazy continuation does not apply to fences.
    3. The quote and the fence break.
- **Fix**:
  - Take each line's insertion point from the parser line records (`nodes_[n].lines`, `cursor.offset` per content
    line) and insert or remove only after the container prefix.
  - Shift+Tab must never remove characters before the per-line content offset.
  - The fallback is to restrict multi-line indentation to code blocks whose ancestors are only `Document`.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h` near the Tab tests (1398-1442). Use the two
  documents above. Assert the `GetMarkdownSyntax()` node kinds (List, ListItem, BlockQuote, FencedCode) are unchanged,
  and only the code lines gained or lost indentation. Both fail at `6df17007`.
- **Verifier notes**: Tab, as opposed to Shift+Tab, inside a list-nested fence is harmless, because extra indentation
  before the content indent stays in the item.

### EDIT-09 — Auto-pair doubles quotes, underscores and asterisks inside words and never types over a closer

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** bug (contract defect)
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Introduced** `8c7c0975`
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp`: 129-142 (`AutoPairClosing`), 5489-5501 (table cells), 5506-5525
    (`OnChar`);
  - contract `Specs/UI/UI_InputAndAccessibility.md:1378-1387`.
- **What**:
  - With `SetAutoPairEnabled(true)` (default off), any single `(`, `[`, `{`, `"`, `'`, `*` or `_` typed with a
    collapsed selection inserts its closer after the caret.
  - There is no word-context check and no type-over of an existing closer.
  - Pairing precedes smart typography, so smart apostrophes are unreachable when both are enabled.
- **Failure scenario** (the closer lands after the caret):
  - Typing `don't` produces `don't'`.
  - Typing `snake_case` produces `snake_case_`.
  - Typing `f(x)` produces `f(x))`.
  - With smart typography also on, typing `it's` produces `it's'` and never `it’s`.
- **Fix**:
  - Skip pairing for `'`, `"`, `_` and `*` when the preceding or following code point is a word character
    (`IsHistoryWordElement`-style check).
  - When the typed character equals the character after the caret, and that character was inserted as a pair closer in
    the current typing run, move the caret over it instead of inserting.
  - Update `UI_InputAndAccessibility.md:1378-1387` in the same change.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h` near 599.
  - `SetText(L"don")`, caret at 3, then `OnChar('\'')` and `OnChar('t')`: expect `don't`.
  - Empty document, then `OnChar('(')`, `OnChar('x')`, `OnChar(')')`: expect `(x)`.
  - Both fail at `6df17007`.
- **Verifier notes**: the implementation matches the current contract, so the defect is in the contract. It stays P2
  because enabling the opt-in breaks every contraction in prose.

### EDIT-10 — Read-only editors report editing chords (Ctrl+X/V/Z/Y/B/I, Shift+Insert/Delete, Ctrl+Enter) as handled

- **Severity** P2 (reviewer P2, verifier P2)
- **Category** api
- **Verdict** Confirmed (read-only part)
- **Confidence** high
- **Effort** S
- **Introduced** `c1a77e42` for X, V, Z, Y, B and I; the Shift+Insert, Shift+Delete and Ctrl+Shift+V aliases were added
  in the window
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - aliases 5682-5687;
  - `X` 6031-6032;
  - `V` 6045-6046;
  - `Z` and `Y` 6102-6103;
  - `B` and `I` 6104-6115;
  - Ctrl+Enter 6116-6125.
  - Compare `src/Controls/DxUi.TextInput.cpp:2346`, `:2368`.
- **What**:
  - R28-40 fixed bare printable keys (`AcceptsTypedText`). The chord path still returns true when a read-only editor can
    do nothing:
    - X and V return true early;
    - `Undo()` and `Redo()` are discarded, then the key returns true;
    - `ExecuteMarkdownCommand` returns `Unavailable` (2609), then the key returns true.
  - DxUi's own `TextField` declines read-only Ctrl+X and Ctrl+V, so the two controls are inconsistent.
- **Failure scenario**: a focused read-only Markdown viewer in RedSalamander or RedXe. The application's Ctrl+B,
  Ctrl+Z, Shift+Delete or Ctrl+Enter shortcut, handled after DxUi, or the embedded `DispatchKey` result, never fires.
- **Fix**:
  - When `! Editable()`, return false for X, V (including the aliases), Z, Y, B and I.
  - Return false for Ctrl+Enter when both `ExitCodeBlock` and `ToggleTask` are `Unavailable`.
  - In an editable Rich paragraph, Ctrl+Enter is documented to toggle tasks ("Ctrl+Enter toggles tasks",
    `Specs/UI/UI_InputAndAccessibility.md:1294`). `ToggleTask` turns a plain paragraph into a task item (`SetList(Task)`,
    `src/Controls/DxUi.RichTextCommands.cpp:2544-2580`).
  - So the status-based return does not free Ctrl+Enter in editable composers. A "send" opt-out is a separate product
    decision (question 7).
  - Document the "handled only when an action ran or was refused for a transient reason" policy next to
    `AcceptsTypedText`.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextEditor.cpp`, with a read-only Rich editor.
  - `OnKeyDown('B', MK_CONTROL)`, `OnKeyDown('Z', MK_CONTROL)`, `OnKeyDown(VK_DELETE, MK_SHIFT)` and
    `OnKeyDown(VK_RETURN, MK_CONTROL)` each return false, and the text and history are unchanged.
  - All return true at `6df17007`.
  - Do not assert that editable Ctrl+Enter returns false; that contradicts the contract.
- **Verifier notes**: the reviewer's claim that Ctrl+Enter is "swallowed" in editable Rich view is corrected above. The
  key edits the document by contract.

## P3 findings

### EDIT-05 — Ctrl+Tab element cycling never yields to host traversal, contradicting the docs

- **Severity** P3 (reviewer P2, verifier P3)
- **Category** a11y, contract
- **Verdict** Confirmed (behaviour; the "keyboard trap" claim is refuted)
- **Confidence** high
- **Effort** S
- **Introduced** `02091fb4`
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp:5949-5991` (target `next` or else `first`, `previous` or else `last`;
    `SetScrollOffsetDip(bounds->top)` at 5990);
  - `docs/rich-text.md:192`;
  - `Specs/UI/UI_InputAndAccessibility.md:1309-1310`;
  - `Specs/UI/UI_ControlsAndLayout.md:2481`.
- **What**:
  - In Rich view with any FootnoteBacklink, Disclosure or eligible Task element, Ctrl+Tab and Ctrl+Shift+Tab cycle
    those elements, wrap around, and always report handled.
  - Every step scrolls the element to the viewport top, even when it is already visible.
  - The wrap is specified ("cycle ... never transfers keyboard focus", `UI_ControlsAndLayout.md:2481`) and tested
    (`Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:3088-3090`).
  - It contradicts "Ctrl+Tab remains host traversal" in the `sourceTabSpaces` paragraph of the docs and input spec.
- **Failure scenario**: a keyboard user in a note with one task presses Ctrl+Tab to leave the editor (the Windows
  convention for multiline edits). Ctrl+Tab keeps selecting the task.
  - Plain Tab still leaves from any non-list, non-table paragraph (6237-6238 → `src/Controls/DxUi.WindowHost.cpp:3812`)
    and from the first item of a list (`src/Controls/DxUi.RichTextCommands.cpp:1305-1307`). So this is not a WCAG 2.1.2
    trap.
- **Fix**:
  - Resolve the contract.
  - Recommended: past the last element (or before the first), clear `keyboardNavigationElement` and return false once,
    so the host traverses; the next Ctrl+Tab cycles again.
  - Scroll only when the element is outside the viewport.
  - Update both docs sentences in the same change.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp` near 3083.
  - Two tasks: Ctrl+Tab twice selects them; the third Ctrl+Tab returns false.
  - This replaces the wrap assertion at 3088-3090; it is not an addition. It fails at `6df17007`.
- **Verifier notes**: downgraded because an escape exists and the cycling is deliberate. What remains is a docs
  contradiction plus a broken conventional exit.

### EDIT-08 — Tab leaves the editor while the Markdown syntax for the current revision is still pending

- **Severity** P3 (reviewer P2, verifier P3)
- **Category** bug
- **Verdict** Plausible
- **Confidence** medium
- **Effort** M
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - `GetMarkdownSyntax` 1852-1858;
  - Tab eligibility 6146-6157;
  - structural Tab 6231-6239;
  - `ExecuteMarkdownCommand` 2611-2613.
  - `src/Controls/DxUi.WindowHost.cpp:3812`; `src/Rendering/Embedded.cpp:818-826`.
- **What**:
  - After any edit, `GetMarkdownSyntax()` is null until the new revision is parsed.
  - The code-block Tab eligibility then fails, and `NextTableCell` and `IndentList` return `Unavailable`.
  - Tab returns false and the host moves focus.
- **Failure scenario**:
  - **Embedded** (turns are frame-paced and keys are accepted while pending): in a large note with
    `sourceTabSpaces=4`, type `x` inside a fenced block and press Tab within the parse turns. Focus leaves the editor
    and the Tab is lost.
  - **Native:** parse turns run back-to-back, so the race needs input queued faster than human typing.
- **Fix**:
  - For lists and tables this is the specified result (`Specs/UI/UI_InputAndAccessibility.md:1282`, `:1295-1297`), so
    any change is a contract change.
  - Option: when `markdownRendering && Rich && markdownProfile && ! GetMarkdownSyntax()`, consume Tab and replay it as
    a structural intent keyed by revision and selection epoch after the parse publishes (as `taskFocusIntent` does).
  - Exclude `HasCurrentMarkdownCapacityRejection()` and failed parses from that, or focus can never leave.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h`.
  - Prepare a code-block document, `OnChar('x')` without preparing, then `OnKeyDown(VK_TAB, 0)`.
  - Assert it returns true and that the indentation is applied after preparation. It returns false at `6df17007`.
  - Add a capacity-rejected document where Tab still returns false.
- **Verifier notes**: whether the race is reachable depends on the host's cadence; the race is real in frame-paced
  embedders.

### EDIT-12 — Text paste at an empty or synthesized table-cell site bypasses the cell transaction

- **Severity** P3 (reviewer P2, verifier P3)
- **Category** bug
- **Verdict** Confirmed (narrow)
- **Confidence** medium
- **Effort** S
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - Ctrl+Shift+V at 6062-6069 and Ctrl+V at 6094-6097, both through `ReplaceSelection` (2329-2336);
  - compare typing at 5477-5503 and file paste at 1208-1219.
- **What**:
  - Typing and file paste route an empty-cell caret site through `SetTableCell`, which escapes `|` and `\`, pads the
    cell and keeps the caret in it.
  - Text paste inserts raw source at the raw caret and ignores `sourceCaretSite`.
- **Failure scenario**:
  - The caret is on a synthesized missing cell, which shares the row-end ACP. Pasting `abc` lands at the row end, in
    the wrong column.
  - Pasting `a|b` or `line1\r\nline2` into an empty cell adds a column or splits the table.
  - Ordinary plain-text paste into a padded empty cell lands inside the padding and is correct.
  - Typing or pasting `|` into non-empty cells changes structure by contract
    (`Specs/UI/UI_InputAndAccessibility.md:1302-1303`).
- **Fix**:
  - When the selection is collapsed, layout is current and `IsCellSourceSite(selection.caret, sourceCaretSite)`, build
    a `SetTableCell` request with the pasted text normalised to one line (the newline policy is product: a space or
    `<br>`). Let the planner escape pipes.
  - Return handled with no edit when the planner is unavailable.
  - Factor one `InsertAtCaret(text, origin)` shared by typing, file paste and text paste.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h`, table fixture near 1445.
  - Put the caret on an empty cell site, inject clipboard text `a|b`, press Ctrl+V.
  - Assert the column count is unchanged and the cell source is `a\|b`. This fails at `6df17007`.

### EDIT-13 — A throwing Ctrl+Shift+C code-copy callback leaves the key unhandled after delivery

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** api
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Introduced** `9ee982ac`
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - 5864-5866, which calls the callback with no try;
  - `OnKeyDown` catch at 6312-6319, which reaches `InputFailureHandled` (5287).
- **What**:
  - A `std::exception` from the callback makes the delivered key unhandled, because revision, epoch and selection did
    not change. The host may then route Ctrl+Shift+C to an accelerator: a double action.
  - `bad_alloc` is not fatal here, unlike Ctrl+K and Ctrl+F (5797-5845).
- **Fix**: wrap the call as Ctrl+K does: `bad_alloc` → `std::terminate()`, `std::exception` → `Debug::Warning`, then
  `return true`. Ideally share one observer-invocation helper (proposal 3).
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextEditor.cpp`. The callback throws `std::runtime_error`;
  `OnKeyDown('C', MK_CONTROL | MK_SHIFT)` must return true. It returns false at `6df17007`.

### EDIT-14 — Ctrl+Enter in a link with no link handler skips Exit-code-block and Toggle-task

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** bug
- **Verdict** Confirmed
- **Confidence** high
- **Effort** S
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - 6001-6014 (`return NavigateSemanticElement(...)` for any link containing the caret);
  - `NavigateSemanticElement` 2082-2084 (no handler → false) and 2101-2102 (unresolved `#fragment` → false).
- **Failure scenario**: a task item `- [ ] see [doc]` immediately followed by `(x.md)` (an inline link), with the caret
  inside the link and no `SetOnLinkActivated` handler. Ctrl+Enter neither toggles the task nor is handled.
- **Fix**: `return` only when `NavigateSemanticElement` returned true; otherwise fall through to the structural
  `VK_RETURN` handling at 6116-6125.
- **Regression test**: `Tests/Support/Support.Tests.RichSemanticEditor.h`. Use the document above with no link handler.
  `OnKeyDown(VK_RETURN, MK_CONTROL)` returns true and the marker becomes `[x]`. This fails at `6df17007`.

### EDIT-15 — Default-presentation Paint creates marked-text and native-object brushes on first use

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** perf
- **Verdict** Confirmed
- **Confidence** medium
- **Effort** S
- **Location**:
  - `src/Controls/DxUi.RichTextEditor.cpp`: pre-warm for Subtle only at 3635-3648; Paint resolves `opaque` and
    `CompositeOverBackground` at 4616-4625 and the marked-text fill at 4679-4682;
  - `src/Controls/DxUi.WindowHost.cpp:2770-2781`: a cache miss creates the brush and inserts it into a map.
- **What**: the first paint of `==mark==`, math or diagram content in Default presentation allocates D2D resources and
  a map node. That breaks the editor's own "first paint cannot allocate" rule (3640-3641).
- **Fix**: in `PrepareContent`, pre-warm the same colours whenever `semantic->HasMarkedText()` or
  `semantic->HasNativeObjects()` is true, regardless of presentation.
- **Regression test**: brush-cache miss counter (diagnostics) around one Paint of a Default-presentation `==x==`
  document after preparation. Assert zero misses. This fails at `6df17007`.

### EDIT-16 — The Source gutter width follows document length, so crossing 10^k units reflows the whole layout

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** perf
- **Verdict** Confirmed
- **Confidence** high (behaviour), low (impact)
- **Effort** S
- **Introduced** `1e9215ce`
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - `SourceGutterWidth` 3014-3029, digits of `Snapshot().Length() + 1u` at 3021;
  - `TextViewport` 3256-3263;
  - `LayoutIsCurrent` 3318.
- **What**:
  - The lane width depends on the UTF-16 length, not the line count. When `Length() + 1` gains a digit, the text width
    changes and every chunk is re-laid out.
  - The lane is also wider than needed: 5 digits for 100 lines of 20K units.
  - This matches the contract text "lane grows with source length" (`Specs/UI/UI_ControlsAndLayout.md:1183`).
- **Fix**: derive the digits from the prepared line count, with hysteresis. Or reserve the width for
  `MaximumUnits + 1` (5 digits) once. Update the spec sentence.
- **Regression test**: Source view with line numbers and a document of exactly **998** units.
  - Type one character, so `Length() + 1` goes from 999 to 1000, which is 3 → 4 digits.
  - Assert `DebugGetState().reusedGeometryChunks > 0` and an unchanged `TextViewport()` width.
  - Starting at 999 would already be 4 digits and could not fail. This test fails at `6df17007`.

### EDIT-17 — Whole-document source copies on some keystroke paths

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** perf
- **Verdict** Partly confirmed (copies confirmed; the UIA claim is refuted)
- **Confidence** high
- **Effort** S
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`, where `SourceSnapshot::Text()` returns `std::wstring`
  (`include/DxUi/RichDocument.h:66`):
  - Source-view or refused-structural Enter at 5349-5350;
  - Tab with `sourceTabSpaces` at 6138;
  - Alt+Up/Down at 5717;
  - source-region or fallback Backspace/Delete at 5101 and 6259;
  - the empty-source-site branch at 5167;
  - the fence auto-format at 5447.
- **What**: each of these paths copies the whole document (up to 128 KB) per keystroke.
  - **Confirmed:** the copies above.
  - **Refuted, Backspace:** ordinary Source-view Backspace with current layout uses `layout.DeletionRange` (6253-6256,
    5268-5273) and copies nothing.
  - **Refuted, UIA:** `SetScrollOffsetDip` (2969) does not rebuild accessibility synchronously per scroll step.
    `RefreshWindowHostAccessibilitySnapshot` (`src/Controls/DxUi.Accessibility.cpp:13544-13615`) marks the snapshot
    dirty, looks up the focus path and posts one coalesced flush. It flushes synchronously only on a focus change.
- **Fix**:
  - Add a `SourceSnapshot` line-ending query: find the first line ending in Enter, and the previous or next line
    boundary for Tab and Alt+Up/Down.
  - Use windowed `Slice` (the `src/Controls/DxUi.RichSourceWindow.h` helpers) for grapheme ranges.
  - No UIA change is needed.
- **Regression test**: allocation counter around `OnChar('\r')` in Source view on a 60K-unit document. Assert that no
  allocation is at least the document size. This fails at `6df17007`.

### EDIT-19 — The Backspace/Delete run precompute records a predicted revision and caret before `Erase` runs

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** simplification
- **Verdict** Confirmed (harmless fragility)
- **Confidence** medium
- **Effort** S
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp`:
  - `OnKeyDown` 6247-6279 (`TypingRun{kind, Revision() + 1u, ..., range.first, ...}` at 6270-6271);
  - compare the Rich branch at 5222-5236.
- **What**:
  - The run is written before the edit, with an assumed revision and caret. If `Erase` makes no change, throws or takes
    another branch, the stored run is stale.
  - Today this is harmless: later merges require `previous->revision == Revision()`, a matching caret and epoch, and
    `CanUndo()` (6267-6269).
- **Fix**: compute `merge` before the edit, but assign `typingRun` only after `Erase` returns true, from the actual
  `Revision()` and caret, as the Rich branch does.
- **Regression test**: Backspace at caret 0 (a no-op), then type and Backspace. Assert the undo granularity is
  unchanged. This is a characterization test: it passes before and after, and guards the refactor.

### EDIT-20 — Crossing native-object rejection ranges return `E_INVALIDARG` without recording any state

- **Severity** P3 (reviewer P3, verifier P3)
- **Category** bug
- **Verdict** Plausible (not constructed)
- **Confidence** low
- **Effort** S
- **Introduced** `61193941`
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp:3534-3541`, inside the `rejectNativeObjects` lambda of
  `PrepareContent`.
- **What**:
  - A new rejected object that partly overlaps a retained one returns `E_INVALIDARG`.
  - It does this with no `layout.Invalidate()`, no `requestActive=false` and no rejection record. Every later pass
    fails the same way.
  - The host records the failure with no self-wake, so frames are withheld until the next invalidation fails again
    (the same class as EDIT-04).
  - No crossing overlap could be built from the current element kinds, because the inline parser nests HTML and math.
- **Fix**: on a crossing overlap, fall back to `nativeGeometryRejected` for the document (as the non-object path does,
  3573-3584) and return S_FALSE.
- **Regression test**: a diagnostics hook (like `testRichPreparationFailure`) that injects two crossing ranges. Assert
  that preparation completes with source fallback and does not return `E_INVALIDARG`.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| EDIT-18 | `sourceTabUsesTabs` is inert unless `sourceTabSpaces != 0`, and nothing documents the dependency | Documented design. `docs/rich-text.md:191-192` and `Specs/UI/UI_InputAndAccessibility.md:1305-1307` name `sourceTabSpaces` as the enabling switch and `sourceTabUsesTabs` as the character choice. Shift+Tab uses `sourceTabSpaces` as the outdent width (`src/Controls/DxUi.RichTextEditor.cpp:6192`), so the gate at 6134 is deliberate. At most a header-comment nit at `include/DxUi/DxUi.h:3202`. |

## Area architecture and simplification proposals

1. **Decoration is never content** (EDIT-01, EDIT-02, EDIT-04; RENDER-01, RENDER-14).
   - Proposal: code ink, source ink, line numbers and whitespace marks run as visual preparation, which never withholds
     paint, gesture survival, TSF/UIA publication or file drops. Paint keeps the previous publication with the edit
     delta applied.
   - Rationale: one root for three findings.
   - Files: `src/Controls/DxUi.RichTextEditor.cpp`, `src/Controls/DxUi.RichSourceGuides.h`,
     `Specs/UI/UI_ControlsAndLayout.md`.
   - Risk: brush pre-warm and "paint allocates nothing" must move with the stage, and visual work needs its own wake.
     It is insufficient without proposal 2.
   - Effort: M.
   - Before/after: before, about 400 turns with no frame per keystroke on a 1,600-line Source file; after, a frame every
     turn.
2. **Incremental, viewport-bounded decoration caches** (EDIT-01).
   - Proposal: number glyphs cached by font, DPI and resource generation; y values staged for the viewport window only;
     code ink kept per code-block identity and shifted by `ChangedRangeFrom` (the image path already does this with
     `SuspendForEdit`).
   - Effort: M.
   - Before/after: before, O(lines) DirectWrite layouts per edit; after, O(visible lines).
3. **One consumer-callback policy helper** (EDIT-04, EDIT-13).
   - Proposal: `InvokeObserver(callback, args, ObserverFailure::{Fatal, Contained})` for the text, selection, scroll,
     link, image, find, link-edit, code-copy, source-drag, bitmap and tokenizer callbacks.
   - Rationale: there are five policies today: terminate, swallow+handled, swallow+unhandled, HRESULT, and
     retry-without-progress.
   - Effort: S. Before/after: the policy is one table instead of per call site.
4. **One allocation-free composition guard**.
   - Proposal: about 12 call sites build `NativeTextInputState input{...}` and call `TryReadNativeTextInputState`, which
     copies the whole cached text, only to test composition (for example 2173-2176, 6049-6053, 6161-6167, 2068-2071).
     Replace them with `MarkdownCommandInputBlocked` (31-37), which reads the cache with no copy and is equal or
     stricter.
   - `PrepareOverlayGeometry` still needs the indices, but not a text copy.
   - Effort: S.
5. **Explicit reveal intent** (EDIT-06).
   - Proposal: remove `revealCaret = true` from `RetireLayout`. Callers that move the caret request a reveal;
     presentation changes request scroll anchoring.
   - Effort: S.
6. **Split `src/Controls/DxUi.RichTextEditor.cpp` (7.3k lines)**.
   - Proposal: split into input (keyboard, character, pointer), clipboard + drop + source move, preparation + overlay,
     paint, and semantic actions + accessibility.
   - Rationale: every invariant check today requires loading the whole file.
   - Effort: M, mechanical; after production.
7. **Table-cell-aware insertion in one function** (EDIT-12).
   - Proposal: `InsertAtCaret(text, origin)` used by typing, file paste, file drop and text paste.
   - Effort: S.

## Test-quality observations

- `Tests/Controls/Controls.Tests.RichTextInput.cpp:1334-1338` saves `rich-d2d-composition.png` but asserts nothing
  about the underline. No test changes the conversion target without changing the text (EDIT-03).
- `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:645-646` asserts that 600-line guide preparation yields,
  which pins the frame withholding of EDIT-01.
  - Nothing measures frames-to-first-pixel after an edit or counts retained number layouts.
  - Nothing checks that a gesture survives a pending cosmetic stage (EDIT-02).
- The Tab indentation tests (`Tests/Support/Support.Tests.RichSemanticEditor.h:1398-1442`) cover top-level code and
  Source view only. They have no list- or quote-nested fence (EDIT-07) and no Tab while syntax is pending (EDIT-08).
- The auto-pair tests (`Tests/Support/Support.Tests.RichSemanticEditor.h:597-611`) cover an empty document and
  selection wrapping only. They have no in-word apostrophe or underscore and no type-over (EDIT-09).
- `Tests/Controls/Controls.Tests.RichTextAccessibility.cpp:3088-3090` asserts that Ctrl+Tab wraps. The only "Ctrl+Tab
  remains host traversal" check is in Source view (`Tests/Support/Support.Tests.RichSemanticEditor.h:1419`), where no
  element cycling exists (EDIT-05).
- There are no read-only negative tests for chord handling (EDIT-10).
- There are no source-region boundary deletion tests for plain Backspace/Delete. Only the word path is exercised
  (EDIT-11).
- The allocation-failure tests cover document transactions. No consumer-callback `bad_alloc` test exists for the
  tokenizer (EDIT-04) or the Ctrl+Shift+C path (EDIT-13).
- `Tests/Embedded/Embedded.Tests.RichTextResources.h` and most of `Tests/Support/Support.Tests.RichSemanticEditor.h`
  were sampled by grep, not read line by line.

## Questions for the developer

1. **Should code ink and source guides gate the frame (coherent "fully inked" frames), or are they optional
   decoration?**
   - Recommended answer: decoration. Change `Specs/UI/UI_ControlsAndLayout.md:1184` and `:2206-2213` with EDIT-01.
   - False premise — see verifier: the reviewer said the host contract suggests optional decoration. The editor's own
     spec deliberately counts these stages as pending; only `docs/hosting.md:43` points the other way. This is a
     contract change, not a misreading.
2. **Should Ctrl+Tab element cycling wrap and never yield?**
   - Recommended answer: yield once at the end of the cycle (EDIT-05).
   - False premise — see verifier: wrapping is intended and tested
     (`Controls.Tests.RichTextAccessibility.cpp:3088-3090`).
     The open issue is the contradiction with `docs/rich-text.md:192`.
3. **Does "Source-region edits never reach outside their active interval" apply to all edits or only Tab?**
   - Recommended answer: all edits (EDIT-11). The word-deletion clamp already shows that intent.
4. **Should a pure presentation change (disclosure, labels, code-copy registration) ever scroll to the caret? And
   zoom?**
   - Recommended answer: no for disclosures and labels. For zoom, anchor the first visible line (EDIT-06).
5. **Should a consumer-callback `bad_alloc` be fatal or contained?**
   - Recommended answer: contained per call for the tokenizer, which the spec already answers ("tokenizer failures use
     ordinary ink", `Specs/UI/UI_ControlsAndLayout.md:1184`). Fatal for observers. One helper (proposal 3).
6. **Is the auto-pair contract deliberately context-free?**
   - It is, as written (`Specs/UI/UI_InputAndAccessibility.md:1378-1387`).
   - Recommended answer: add word-context and type-over rules (EDIT-09).
7. **Should an editable composer be able to keep Ctrl+Enter for the application (send) instead of task toggling?**
   - Recommended answer: add an opt-out next to `SetTaskTogglePolicy` (EDIT-10).

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.RichTextEditor.cpp` | All 108 diff hunks, plus full HEAD bodies of the touched functions. See note A. Image-resource internals were skimmed (another area). |
| `src/Controls/DxUi.RichEditorLayout.h` | Full (diff and class) |
| `src/Controls/DxUi.RichEditorAccessibilityAccess.h` | Full (new) |
| `src/Controls/DxUi.PreparedLayoutAccess.h` | Full (declarations; definitions belong to the layout area) |
| `src/Controls/DxUi.RichLayoutOptions.h` | Full |
| `src/Controls/DxUi.RichProjectionWindow.h` | Full (declaration); `src/Controls/DxUi.RichSourceWindow.h` 1-200 for the window-range paths |
| `src/Controls/DxUi.RichSourceGuides.h` | Full |
| `include/DxUi/DxUi.h` | Diff and touched declarations: RichTextEditor, RichTextSourceMove, appearance, decoration and activation structs |
| `include/DxUi/RichTextPresentation.h` | Skipped: unchanged in the window |
| Host neighbours (traced, not owned) | See note B |
| `Tests/Controls/Controls.Tests.RichTextEditor.cpp` | Diff assertions read; new tests sampled |
| `Tests/Controls/Controls.Tests.RichTextInput.cpp` | Composition and rebinding tests (1270-1445) read; the rest sampled |
| `Tests/Support/Support.Tests.RichSemanticEditor.h` | Sampled: auto-pair, Tab, coalescing, overlay, transient preview |
| `Tests/Support/Support.Tests.RichTextStateReplacement.h` | Diff read fully |
| `Tests/Embedded/Embedded.Tests.RichTextResources.h` | Grep only (auto-pair, tokenizer) |

Note A — the touched functions in `src/Controls/DxUi.RichTextEditor.cpp`, read in full at HEAD:
- Source and appearance: `SetText`, `SetAppearance`, `SetReadOnly`, `ApplySourceEdits`, scroll APIs.
- Clipboard and drop: clipboard and file paste, `FileDropCaret`/`CommitFileDrop`.
- Semantic actions: `SetSemanticDisclosure`/`SourcePresentationChanged`/`UpdateSemanticSourceRegion`, Markdown
  commands, code-copy APIs.
- Notifications: `Changed`, `SelectionChanged`.
- Preparation: `RetireLayout`, `LayoutIsCurrent`, `PrepareOverlayGeometry`, `PrepareContent`, `PrepareCodeInk`,
  `PrepareSourceGuides`.
- Input and paint: `ImportTextInputState`, `Paint`, `Erase`, `OnChar`, `OnKeyDown`, the mouse handlers, the focus and
  capture handlers.

Note B — host neighbours traced:
- `src/Controls/DxUi.Controls.cpp` 2365-2687;
- `src/Controls/DxUi.WindowHost.cpp` 2526-2547, 2760-2800, 3756-3893, 4594-4650, 5473-5498;
- `src/Rendering/Embedded.cpp` 395-833, 1395-1450;
- `src/Controls/DxUi.TextStoreACP.cpp` 195-215;
- `src/Controls/DxUi.NativeTextInput.cpp` 1254-1329, 1780-1907, 2084-2173;
- `src/Controls/DxUi.Accessibility.cpp` 13544-13615;
- `src/Controls/DxUi.MarkdownBlocks.h` 540-600;
- `src/Controls/DxUi.RichTextCommands.cpp` 1235-1317, 2544-2580;
- `src/Controls/DxUi.TextInput.cpp` 2330-2390;
- `Samples/EmbeddedControls/Main.cpp` 150-205.

Prior items re-verified as fixed: DX-3, DX-4, document-EOL Enter, DX-10, R28-40.
