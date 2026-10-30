# Code review of the ten-day merge window (#87-#137)

Status: SPEC READY. Review only: no library, test or tooling code changed. Remediation waits for the
[questions](#questions-for-the-developer) that decide its scope.
Date: 2026-10-28, refreshed 2026-10-30 (see [30 October refresh](#30-october-refresh)).
Window: first-parent `51ef9f29` (main on 2026-10-18) to `881e5d5f` (main on 2026-10-28): 28 merges, #87 to #121.
About 39K added and 2K removed lines in `src` and `include`, 48K added in `Tests`, 5.7K in tooling and CI
(fixtures excluded). The refresh extends it to `09f6d96a` (main on 2026-10-30): 7 more merges, #122 to #137, with
about 3K changed lines in `src`, `include` and `Tests` and 1.8K in tooling.
Related: [the 10 October review](CodeReview_2026-10-10.md) (CR-* IDs) and [the RichText audit](RichTextAudit_2026-10-21.md)
(its [findings register](RichTextAudit_2026-10-21/findings.md) IDs such as editor-03). This plan does not repeat
findings those plans still track. It lists them only where they are still present, where a claimed fix is
incomplete, or where new code extends them.
Owning contracts: [controls](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md),
[Win32 host](../../Rendering/Rendering_Win32Host.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[performance](../../Core/Core_PerformanceAndResources.md), [validation](../../Testing/Testing_Validation.md) and
[toolchain](../../Build/Build_ToolchainAndConsumption.md).

## Verdict

**Not yet production-ready for a consumer pin; close the P1 items first.** No memory-safety defect was found that is
reachable under the documented contracts, and the recent focus-lifecycle, posted-payload and TSF-retirement work
holds up. The window does ship user-visible defects in common paths:

- Grid and Tree content becomes unreachable after the window grows (R28-01).
- A pending rich-text preparation cancels any drag in the same window (R28-02).
- Source view line pitch changes after the first chunk (R28-03), and RTL Subtle tables collapse (R28-04).
- IME input past the rich editor's capacity is accepted and then lost (R28-05), and IMEs cannot see read-only fields (R28-06).
- Common math (`\\`, `\|`) and Mermaid (`:::`, `accTitle`) input renders wrongly instead of falling back (R28-07 to R28-09).
- A focused read-only editor swallows the application's bare-key shortcuts (R28-40).
- A GPU reset seen first by a resize leaves a native window permanently blank (R28-41, pre-window).

Two contract conflicts need a decision before their fixes can be scoped: whether an embedded scheduling callback may
re-enter the host (Q1), and whether allocation failure at an input boundary terminates the process (Q2).

Five claimed fixes are incomplete or stale: math-parser-12, math-layout-05/06, MM-33/34, mermaid-04 and CR-16
(see [claimed fixes](#claimed-fixes-that-do-not-hold)).

**30 October:** the verdict stands. None of the findings below was fixed by #122-#137, and every P1 still reproduces
by reading at `09f6d96a`. The refresh adds one memory-safety defect under allocation failure, R28-44: `Grid::SetModel`
keeps a pointer to the model the caller is about to destroy. The new merges add no P1. They add:
- a menu regression for typed characters, R28-45;
- test gaps around the new menu behaviour, R28-46;
- build-policy and tooling items, R28-51 to R28-53.

A second side-effect pass over the earlier merges also found three bounded-work regressions, R28-48 to R28-50.

## How the review was made

- **Partition.** The lead reviewed the posted-payload transport, the embedded host, the #121 focus lifecycle, the #117
  RedMonitor integration, the Release `/OPT:ICF` exposure and the embedded sample.
- **Reviewers.** Nine cheaper-model reviewers each read one partition: Win32 host and menus, text input/TSF/clipboard,
  UI Automation, core controls, Grid/Tree, RichTextEditor, semantic geometry/source ink/images, math/Mermaid, and
  tooling/CI. Each read the diff and the surrounding code of every changed function, then tried to refute its own
  findings before reporting.
- **De-duplication.** RichText code merged in #89 was already audited at `fa5821c8`, so rich-text reviewers concentrated
  on `fa5821c8..881e5d5f` and listed still-open audit IDs separately.
- **Verification.** The lead re-read the cited code at `881e5d5f` for every P1 finding except R28-13 and for most P2
  findings. Those are marked **confirmed**. **[reported]** items were traced by a reviewer, and the lead checked only
  the cited lines or gating named in the tag.
- **Limits.** Nothing was built or run. Every finding is a static trace with a concrete scenario. Runtime
  reproduction belongs to the failing-test-first step of each fix.
- **30 October refresh.** Six cheaper-model reviewers worked at `09f6d96a`:
  - two re-traced every item tagged [reported], starting from its text, and confirmed, corrected or refuted it;
  - two made a side-effect pass over 41 source commits of the whole window, given only a list of already-known
    titles;
  - one reviewed the test delta;
  - one reviewed the tooling and CI delta.

  The lead re-read the cited lines of every retained P1/P2 claim and of most P3 claims. Tags now read
  **[confirmed]** (the lead re-read it), **[confirmed 30 Oct]** (re-traced by a second reviewer; the lead checked the
  key lines), **[partly]** or **[refuted]**.

## P1: user-visible defects in common paths

- [ ] **R28-01 Grid and Tree never re-clamp scroll offsets when they grow** (pre-window, `f28f55d0`) [confirmed]
  - Neither control overrides `OnBoundsChanged`, and `ComputeVisibleSpan` only clamps the scroll offset below zero
    ([Grid.cpp:7742](../../../src/Controls/DxUi.Grid.cpp#L7742)). Tree Paint reads the raw offset
    ([Tree.cpp:1293](../../../src/Controls/DxUi.Tree.cpp#L1293)). Grid clamps only while resizing a column.
  - **Failure.** Scroll a 100-row Tree to the bottom, then maximize the window. Paint starts at row 89. The extent is
    now zero, so there is no scrollbar and the wheel refuses. Rows 0-88 stay unreachable until a key or a model refresh.
    The Grid behaves the same way vertically and horizontally.
  - **Fix.** Override `OnBoundsChanged` in both to call the existing clamps. Add native and embedded resize tests.
- [ ] **R28-02 Any pending content preparation cancels an unrelated mouse capture** (#89 `c1a77e42`) [confirmed]
  - [Controls.cpp:2468-2474](../../../src/Controls/DxUi.Controls.cpp#L2468) cancels `_capturedControl` whenever any
    control's `PrepareContent` returned `S_FALSE`. The block just above it correctly compares the captured control's
    own bounds.
  - **Failure.** A large Markdown preview sits next to a Splitter. Dragging the splitter re-wraps the editor, its
    cooperative preparation spans turns, and the drag is cancelled on the next frame with the button still down.
    The same applies to Slider and scrollbar thumbs anywhere in the window.
  - **Fix.** Cancel only when the captured control is inside a subtree with pending content, or its bounds changed.
    Test a Splitter drag beside a pending RichTextEditor.
- [ ] **R28-03 `lineHeightMultiplier` is ignored for every v0/Source chunk after the first** (`4c46e96b`) [confirmed]
  - Positioned chunks get a two-unit prefix, and uniform spacing is applied only `if (lineHeight > 0.0f && ! textOffset)`
    ([RichTextLayout.cpp:247](../../../src/Controls/DxUi.RichTextLayout.cpp#L247),
    [321](../../../src/Controls/DxUi.RichTextLayout.cpp#L321)). Chunks are about 1,024 units
    ([RichDocumentLayout.cpp:375](../../../src/Controls/DxUi.RichDocumentLayout.cpp#L375)).
  - **Failure.** With the gallery's Source appearance (1.6), a Source document longer than about 1 KB changes line
    pitch at the first chunk boundary. Caret, hit-test and selection follow the mixed pitch.
  - **Fix.** Apply the uniform spacing to positioned chunks, or translate the draw origin instead of using the prefix.
    Add a test across at least three chunks.
- [ ] **R28-04 Subtle table columns collapse in RTL** (`83d6a160`) [confirmed]
  - [RichSemanticGeometry.cpp:820](../../../src/Controls/DxUi.RichSemanticGeometry.cpp#L820) computes
    `textBounds.second - textBounds.first`. `HorizontalTextBounds()` returns `{left, width}`
    ([RichTextLayout.cpp:424](../../../src/Controls/DxUi.RichTextLayout.cpp#L424)), and every other caller uses
    `.second` as the width.
  - **Failure.** In an RTL editor the measuring layout is right-aligned in a very wide box, so `left` is large and the
    width becomes 0. Every RTL Subtle column falls to its 24 DIP seed and the cell text wraps at about 12 DIP.
  - **Fix.** Use the width. Return a named `{left, width}` struct so the mistake cannot recur. Assert relative column
    widths in the RTL Subtle test.
- [ ] **R28-05 RichTextEditor TSF edits over capacity are accepted, then fail at lock end** (`8252aa27`, `db3c717f`) [confirmed]
  - The native text-store target keeps the `SIZE_MAX` default ceiling
    ([TextStoreTarget.h:36-39](../../../src/Controls/TextStoreTarget.h#L36)), while `RichText::Document` throws above
    64K units. `QueryInsert`, `InsertTextAtSelection` and `SetText` therefore return `S_OK`, and the import fails at
    `EndEdit` with a generic `E_FAIL`.
  - **Failure.** IME or dictation inserts 1,000 units into a 65,000-unit document. Nothing appears, and the TIP got
    no overflow error it can act on. This breaks the contract's "reject overflow before mutation".
  - **Fix.** Add a virtual capacity query on `Control` (default `SIZE_MAX`, RichTextEditor 64K) and forward it from
    the native target. Test 65,500 units plus a 100-unit insert.
- [ ] **R28-06 The text store never reports read-only or multiline status** (pre-window `f28f55d0`, widened by #89) [confirmed]
  - `GetStatus` always returns no dynamic flags and only `TS_SS_NOHIDDENTEXT`
    ([TextStoreACP.cpp:1335-1345](../../../src/Controls/DxUi.TextStoreACP.cpp#L1335)). Writes are refused with
    `E_ACCESSDENIED`, not `TS_E_READONLY`.
  - **Failure.** A Japanese or Chinese IME opens composition over a read-only TextField or read-only Source view.
    Every write is then refused and the keystrokes vanish. The touch keyboard also shows the single-line layout for
    multiline fields.
  - **Fix.** Report `TS_SD_READONLY` and `TS_SS_MULTILINE`, return `TS_E_READONLY`, and raise `OnStatusChange` when
    read-only flips. Update the pinned `E_ACCESSDENIED` tests.
- [ ] **R28-07 `\\` outside a matrix renders a literal backslash** (#90 `8ab8b9b7`) [confirmed]
  - The escape set at [NativeMath.cpp:491](../../../src/Controls/DxUi.NativeMath.cpp#L491) includes `\`, and the
    row break is recognised only inside a cell (519).
  - **Failure.** `$$a = b \\ c = d$$` draws `a=b\c=d` on one line, where TeX and KaTeX break the line.
  - The 24 October disposition marked math-parser-12 refuted; it is not.
  - **Fix.** Reject `\\` outside rows (source fallback) and add `\backslash`. Multi-line display can come later.
- [ ] **R28-08 `\|` draws a single bar or rejects the formula** (#90) [confirmed]
  - `Delimiter()` returns `|` for `\|` ([NativeMath.cpp:214](../../../src/Controls/DxUi.NativeMath.cpp#L214)), so
    `\left\| v \right\|` draws one stroke. A bare `\|` reaches `Reject()`.
  - **Fix.** Map `\|` to U+2016 in both places, matching `\Vert`. Add both cases to the corpus.
- [ ] **R28-09 State and class diagrams turn `:::`, `accTitle` and `accDescr` into content** (#90; MM-33/34) [confirmed]
  - Node `:` text and edge `:` labels accept any plain text
    ([NativeFlowchart.cpp:399-416](../../../src/Controls/DxUi.NativeFlowchart.cpp#L399), 485-492). The reserved list
    at 177 lacks `accTitle` and `accDescr`.
  - **Failure.** In a state diagram, `A:::cls` labels state A `::cls`. `accTitle: Title` creates a state named
    `accTitle`. In a class diagram, `Animal:::cls` adds a member line `::cls`.
  - The spec says unsupported syntax must not acquire a different meaning. No test contains these constructs.
  - **Fix.** Reject `:`-led descriptions as `UnsupportedStatement`, and reserve `accTitle`, `accDescr`, `title` and
    `cssClass`.
- [ ] **R28-10 Source ink colours bare autolinks and `\(..\)` math wrongly** (`be78a65b`) [confirmed]
  - `FinishLinkLabel` subdues the first character of every link and the tail after the label
    ([RichSourceInk.h:264-270](../../../src/Controls/DxUi.RichSourceInk.h#L264)). A GFM autolink has no label, so
    `https://example.com` shows a grey `h` and no link ink.
  - `FinishMarkerRun` counts the run of the first character, so for `\(x\)` only `\` and the final `)` are subdued.
  - Applies when `colorizeMarkdownSource` is on.
  - **Fix.** Give bare autolinks Link ink over the whole range. Treat `\(`, `\)`, `\[` and `\]` as two-unit
    delimiters. Add both fixtures.
- [ ] **R28-11 A UIA `ScrollIntoView` on rich text leaves the published snapshot stale** (#89) [confirmed]
  - [Accessibility.cpp:7435-7458](../../../src/Controls/DxUi.Accessibility.cpp#L7435) scrolls the editor without
    republishing. The Grid twin does republish.
  - **Failure.** A screen reader scrolls a heading into view, then reads its `BoundingRectangle`, `IsOffscreen` or
    visible ranges, and gets the pre-scroll values. Embedded hosts are unaffected.
  - The test hides the gap by calling `Publish()` after every scroll.
  - **Fix.** Republish after a scroll that moved, as the Grid does, and drop the explicit refresh from the test.
- [ ] **R28-12 Grid and Tree state setters do not repaint** (pre-window and `8252aa27`) [confirmed]
  - `Tree::SetSelectedItemId` and `SetSelectedItemIds` change selection, focus and scroll, then end with an
    accessibility refresh only ([Tree.cpp:842-872](../../../src/Controls/DxUi.Tree.cpp#L842), 934-984). That refresh
    never invalidates and returns early for embedded targets.
  - `Grid::SetHeaderBusy` and `SetHeaderBusyColumn` are bare assignments, and `SetSortSpec` starts a glyph transition
    without an invalidation or a tick ([Grid.cpp:1434-1471](../../../src/Controls/DxUi.Grid.cpp#L1434)). The busy
    spinner is requested only from Paint, so it never starts on its own.
  - [partly] `Grid::SetSelectionMode` misses a repaint only when it collapses a multi-selection (Grid.cpp:1313-1325).
  - Correction: `Grid::SetSelectedRowIds` and `SetFocusedRowId` do invalidate (Grid.cpp:6851).
  - **Failure.** An app selects a tree item, or marks a Grid header busy, from a command, timer or completion. The
    highlight or spinner stays stale until an unrelated repaint, and an embedded host is never asked to prepare.
  - **Fix.** Invalidate at the end of each setter while the binding is current, using the `lifetime` pattern of
    Grid.cpp:6851. No test checks invalidation after any of these setters; `DebugGetInvalidateCount` exists for it.
- [ ] **R28-13 Tree refresh and typeahead fight the user** (pre-window) [confirmed 30 Oct]
  - `NotifyDataChanged` scrolls back to the focused row on every refresh
    ([Tree.cpp:711-716](../../../src/Controls/DxUi.Tree.cpp#L711)). A live tree with status updates yanks the view
    back while the user reads elsewhere. The Grid only clamps.
  - Typeahead always starts after the current row ([Tree.cpp:3476](../../../src/Controls/DxUi.Tree.cpp#L3476)).
    With `Alpha, Alpine, Beta` and `Alpha` selected, `a` moves to `Alpine`, `al` wraps back to `Alpha`, and `alp`
    returns to `Alpine`. The selection oscillates and fires the delegate on every key. Fix: start at the current row
    for a prefix longer than one character.
  - [partly] The first key on a never-focused single-select Tree silently selects row 0 without notifying
    ([Tree.cpp:2128-2142](../../../src/Controls/DxUi.Tree.cpp#L2128)). The spec asks it to start from the first row
    (UI_ControlsAndLayout.md:679), but Return and Space then invoke row 0 with no selection notification.
  - See Q10 for the intended behaviour.
- [ ] **R28-40 A read-only editor swallows printable key-downs meant for the application** (`8252aa27`, `5b85b4b7`) [confirmed]
  - [WindowHost.cpp:3720-3724](../../../src/Controls/DxUi.WindowHost.cpp#L3720) returns "handled" for every printable
    key-down when the focused control `SupportsTextInput()`. That is always true for `RichTextEditor` and `TextField`,
    including read-only and masked ones.
  - **Failure.** A read-only Markdown viewer has focus, and the application's bare-key shortcuts (`/`, `n`, `j`) never
    reach its window procedure. No character is typed either. Non-editable ComboBoxes are not affected.
  - **Fix.** Add a narrow `AcceptsTypedText()` query (read-only and masked aware) for this one condition. Keep
    `SupportsTextInput()` for TSF and UIA. Test a read-only editor plus an application shortcut.

## P2: resiliency, contract and performance defects

- [ ] **R28-14 A setter can touch a control that its own invalidation retired** (`8252aa27`, `f5a2cea9`, #117) [confirmed; scope depends on Q1]
  - Each of these reads `this` after a call that can reach the embedded `requestPreparation` callback:
    - `Panel::CompactChildren` calls `GetHost()` after `InvalidateMeasure()`
      ([Controls.cpp:1367-1369](../../../src/Controls/DxUi.Controls.cpp#L1367)).
    - `ProgressBar::SetMinimum` and `SetMaximum` call `GetHost()` after `RequestInvalidate()` (4880, 4895).
    - `SetIndeterminate` and `SetAnimationPaused` call `RequestInvalidate()` after `RequestAnimation()`. The embedded
      `RequestAnimation` invalidates.
    - [reported] The base `Control::OnFlowDirectionChanged` and `OnDensityChanged` take the lifetime token after
      `InvalidateMeasure()` (DxUi.cpp:1304-1332). `SetAccessibleName`, `SetAccessibleHelpText`,
      `SetAccessibleFormProperties` and `SetAccessibilityRole` call `GetHost()` after `RequestInvalidate()`.
      `2411cbca` guarded `SetVisible` and `PropagateHost` but missed these.
  - **Contract conflict.** [Embedded.h:29](../../../include/DxUi/Embedded.h#L29) says the callback "must not re-enter
    the host". Controls.Tests.IntrinsicLayout.cpp:1043-1097 nevertheless pins retirement from that callback as safe for
    Button, Label and Separator setters. If the tests are the contract, the sites above are use-after-free. If the header
    is, the tests and much of the defensive code are unnecessary. See Q1.
- [ ] **R28-15 Allocation failure terminates the process at some input boundaries and not others** (`4c46e96b`, `a17f7d7b`) [confirmed]
  - These terminate on `std::bad_alloc`:
    - `RichTextEditor::OnKeyDown`, `OnMouseDown`, `OnMouseMove` and `OnMouseUp`
      ([RichTextEditor.cpp:2537](../../../src/Controls/DxUi.RichTextEditor.cpp#L2537), 2636, 2761, 2795).
    - The embedded keyed `DispatchKey` and the receipt `DispatchCharacter`
      ([Embedded.cpp:987](../../../src/Rendering/Embedded.cpp#L987), 1085).
  - The plain `DispatchCharacter` returns false on the same failure, `TransformClipboardPaste` and UI Automation
    return `E_OUTOFMEMORY`, and before this window the key handlers returned false.
  - The contract mandates terminating only at observer notification callbacks ([UI_InputAndAccessibility.md:45](../../UI/UI_InputAndAccessibility.md)).
  - **Failure.** Ctrl+V of a large clipboard under memory pressure stages its allocation atomically, then the handler
    terminates instead of declining the paste. See Q2.
- [ ] **R28-16 A large text-range geometry query can terminate the process** (pre-window `f28f55d0`) [confirmed]
  - `TextField` and `ComboBox` size `std::vector<DWRITE_HIT_TEST_METRICS>` by the requested range
    ([TextInput.cpp:2838](../../../src/Controls/DxUi.TextInput.cpp#L2838), 2893; ComboBox.cpp:1957).
  - The noexcept `TryResolveTextStoreRangeRect` calls the throwing `Control::TryGetTextInputRangeRects` without a catch
    ([TextStoreACP.cpp:111-118](../../../src/Controls/DxUi.TextStoreACP.cpp#L111)).
  - **Failure.** A TIP calls `GetTextExt` on a select-all of a 5M-unit field: about 180 MB for a few records. On
    failure, `bad_alloc` meets `noexcept`.
  - **Fix.** Start small and use the existing `ERROR_INSUFFICIENT_BUFFER` retry, and catch in
    `Control::TryGetTextInputRangeRects`. RichTextEditor already does the latter.
- [ ] **R28-17 A deterministic embedded content failure returns `E_ABORT` and never asks again** (CR-16 partly reversed) [confirmed]
  - [Embedded.cpp:561](../../../src/Rendering/Embedded.cpp#L561) deliberately does not request preparation when
    `PreparePendingPageLayout` fails on unchanged content.
  - [Embedded.h:147](../../../include/DxUi/Embedded.h#L147), CR-16's closing note and the sample all treat `E_ABORT`
    as "superseded; a new preparation was already requested".
  - **Failure.** A control whose `PrepareContent` keeps failing leaves a callback-driven host showing the last frame,
    or a blank one on the first frame, until something else marks it dirty. A polling host retries every frame instead.
  - **Fix.** Return a distinct failure HRESULT (or `S_FALSE` plus a diagnostic), and document retry with backoff. Or
    request once with a bounded backoff. See Q5.
- [ ] **R28-18 Tree item-status events are uncapped and the embedded property queue is unbounded** (`baff43d8`) [confirmed]
  - [Accessibility.cpp:12704-12760](../../../src/Controls/DxUi.Accessibility.cpp#L12704) raises one provider and one
    event per changed badge. The embedded pending key now includes `treeItemId` (13412), against the contract's "one
    pending entry per control identity/property".
  - **Failure.** Refreshing every badge of a 10,000-row tree makes 10,000 synchronous UIA events natively, and 10,000
    queued entries, each with a COM registration, in the embedded host. Selection events already cap at 20.
  - **Fix.** Above a small cap, raise one `ChildrenInvalidated`, cap the queue, and amend the contract sentence.
- [ ] **R28-19 Embedded tree-item event providers pin the creation snapshot** (`8252aa27`, `baff43d8`) [confirmed 30 Oct]
  - `CaptureProviderCreationSnapshot` captures the current snapshot for embedded targets (Accessibility.cpp:5673), and
    the `TreeItemTag` constructor pins it, including `preparedTreeRows` (5758-5767).
  - Expansion events build such a provider and raise it directly (12648-12664). They never use
    `QueueEmbeddedPropertyChange`, which the legacy disclosure property does use (13606).
  - Status events register the same pinned provider in the Global Interface Table (12736, 13396), so a held batch
    keeps the pin until delivery.
  - This contradicts UI_InputAndAccessibility.md:974-976 and 1004-1009: event providers "must not pin complete creation
    snapshots or prepared row epochs". Selection-item event providers (12590) follow the same pattern.
  - **Fix.** Add an item-level `EventTag` constructor (mirror 5709 and 6108-6119) and queue embedded expansion events.
    No test checks pinning by tree-item events.
- [ ] **R28-20 Selecting many ids is quadratic** (`8252aa27`) [confirmed]
  - `Grid::SetSelectedRowIds` and `Tree::SetSelectedItemIds` call `Toggle` per id
    ([Grid.cpp:6817](../../../src/Controls/DxUi.Grid.cpp#L6817), [Tree.cpp:964](../../../src/Controls/DxUi.Tree.cpp#L964)),
    and each `Toggle` does a sorted `vector::insert` (Grid.cpp:1033).
  - **Failure.** "Select all 100,000 search hits" hangs the UI for seconds.
  - **Fix.** Add one `GridSelectionModel::AssignInOrder` that filters the visible order: O(N+k). It replaces both loops.
- [ ] **R28-21 Tree expansion does unbounded work per toggle and per frame** (pre-window) [confirmed 30 Oct]
  - Two full `CaptureVisibleItems()` copies run per toggle (Tree.cpp:1080, 1106). They happen before
    `BeginTreeExpansionAnimation` bails on reduced motion or no host (3398).
  - The animation looks up every removed descendant by linear scan before the viewport check (1387, 1395).
    `CountDescendants` is rescanned per frame (1381), and each painted row does three linear id scans (1324-1326).
  - **Failure.** Collapsing 20,000 visible descendants costs about 2x10^8 comparisons per animation frame.
  - **Fix.** Skip the captures with reduced motion or no host, animate only rows that intersect the viewport, and map
    ids to indices once.
- [ ] **R28-22 Every formula re-reads the system math font, and a width-only reflow rebuilds all formulas** (`f5a2cea9`) [confirmed]
  - The `PreparedMath` constructor calls `ReadMathConstants`
    ([PreparedMath.h:1048](../../../src/Controls/DxUi.PreparedMath.h#L1048)): system font collection, family lookup,
    font face and MATH table, per formula.
  - Geometry reuse keys include `widthDip`, although math geometry is width-independent.
  - **Failure.** A resize drag over 128 formulas repeats 128 table reads and reshapes up to 4,096 leaves per width step.
  - **Fix.** Cache the constants per factory and font, and carry prepared math across width-only changes.
- [ ] **R28-23 Per-object native fallback is discovered one object per full rebuild** (`baff43d8`) [confirmed]
  - The editor records one rejected object, clears the semantic state and rebuilds
    ([RichTextEditor.cpp:1220-1262](../../../src/Controls/DxUi.RichTextEditor.cpp#L1220)). Width rejections ("Diagram
    does not fit this view width") take the same path.
  - A document with k objects that do not fit therefore needs k+1 complete rebuilds.
  - This is worse than first reported. The rejections are reset whenever `GetMarkdownSyntax()` changes (1357-1365),
    and every edit produces new syntax. A narrow pane with wide diagrams therefore pays k+1 rebuilds per keystroke,
    not only per resize.
  - **Fix.** Collect every offender in one geometry pass and reject them together. Key kept rejections by object
    source and width, so that edits elsewhere keep them. The existing test covers one formula plus one diagram only.
- [ ] **R28-24 The read-only Source mirror relays out and scrolls to the top on every sync** (`47a9d259`) [confirmed]
  - `SynchronizeReadOnlySourceSnapshot` uses `RetireLayout()`, which clears every retained chunk, and sets
    `scrollDip = 0` ([RichTextEditor.cpp:191-201](../../../src/Controls/DxUi.RichTextEditor.cpp#L191)). An ordinary
    edit uses `layout.Invalidate()` and keeps reusable chunks.
  - **Failure.** A split view whose mirror follows each keystroke re-lays out the whole document per key, and loses the
    user's own scroll position in the mirror. The contract equates sync with `SetText`, so see Q4.
  - The same refactor fixes R28-30: `Changed()` keeps a stale `hoveredLink`.
- [ ] **R28-25 The direct source-ink renderer is reachable in production before its performance is qualified** (`46d9952d` to `1e28fdff`) [confirmed 30 Oct]
  - It runs when `colorizeMarkdownSource` (default false) is set in Source view outside high contrast. The only
    switch is that opt-in, which disables colour altogether. There is no renderer-only kill switch, no
    `capabilities.json` status, and the plan records failed verdict publication.
  - Source view uses `FontRole::Body`, which is proportional Segoe UI. A DirectWrite run covers a line segment, so any
    line that mixes delimiters and prose fails `HasUniformBrush`. The preflight stops at the first such run. Nothing
    remembers the rejection, so every repaint pays it again, and with the default font the direct route rarely
    completes on real text.
  - An accepted route costs two DirectWrite traversals and two eligibility analyses per run. That is unmeasured.
  - The correctness pass over the five commits found no defect: COM lifetime, cluster-to-role splitting, bidi rejection
    and the pre-paint fallback hold.
  - **Fix.** Remember the rejection per layout generation, or gate on a monospaced base font. Publish a
    preview/unqualified status until paired acceptance passes. See Q6.
- [ ] **R28-26 `Button::Paint` creates a DirectWrite layout for shortcut text on every paint** (`3232941f`) [confirmed]
  - [Controls.cpp:3535](../../../src/Controls/DxUi.Controls.cpp#L3535) calls the uncached
    `MeasureSingleLineTextWidthDip` from Paint. That contradicts "paint performs no layout creation"
    ([performance contract](../../Core/Core_PerformanceAndResources.md)), which `2c38d36a` just enforced for IconOnly.
  - Older Tab and TagPicker paint paths share the pattern; this extends CR-25.
  - **Fix.** Measure in `PrepareVisualContent` and keep the width.
- [ ] **R28-27 Failed remote images are requested again on every edit** (images-clipboard-17, raised) [confirmed]
  - Reuse needs a decoded image or a live pending request (RichImageResources.h:177-178). A failed completion leaves
    neither, so the next document stages a fresh request (186-188).
  - With remote images enabled, a 404, oversized or timed-out URL becomes one request through the consumer's transport
    per keystroke. Missing local files behave the same way. Idle retries do stop, as the contract requires.
  - **Fix.** Add a `failed` flag to the entry, reuse it, and clear it on `ReloadImages` or a location or policy change.
    See Q16.
- [ ] **R28-41 Device loss during swap-chain resize or creation never resets the device** (pre-window `f28f55d0`) [confirmed]
  - A failed `ResizeBuffers` only discards size-dependent resources
    ([WindowHost.cpp:4261-4270](../../../src/Controls/DxUi.WindowHost.cpp#L4261)). Swap-chain, back-buffer and bitmap
    creation failures (4200-4246) do the same. `IsDeviceLossHResult` is consulted only after EndDraw, Flush and Present
    (4905).
  - **Failure.** The GPU resets (TDR, driver update) and the next event is a resize. Every later `Render` retries
    `CreateSwapChainForHwnd` on the removed device and fails. A single-window application stays blank.
    `DebugSimulateDeviceLoss` exercises recovery, not detection.
  - **Fix.** Route device-loss HRESULTs from these calls to the existing EndDraw recovery sequence. Add a
    fault-injection test at resize and at creation.
- [ ] **R28-42 Occluded native windows keep ticking and rendering** (pre-window) [confirmed 30 Oct]
  - Animation suspends only when hidden or minimized (WindowHost.cpp:5651-5658, 834-842). `DXGI_STATUS_OCCLUDED` is a
    success code that Present handling treats as success. Nothing calls `RegisterOcclusionStatusWindow`.
  - **Failure.** A covered window with an indeterminate progress bar or spinner draws every tick for no visible result.
  - The performance contract says occluded work is event-blocked; the Win32 host contract says only "when hidden". See Q17.
  - **Fix.** `RegisterOcclusionStatusWindow`, then suspend the animation subscription while occluded and repaint fully
    on change. This is event-driven, with no polling.
- [ ] **R28-44 `Grid::SetModel` keeps a dangling model pointer after an allocation failure** (pre-window; was R28-33(d)) [confirmed]
  - `SetModel` is `void noexcept`. It copies the previous selection first and returns on `bad_alloc` before
    `_model = model` ([Grid.cpp:1172-1187](../../../src/Controls/DxUi.Grid.cpp#L1172)).
  - The caller cannot see the failure and destroys the old model, as the non-owning contract allows. The Grid still
    points at it. The next paint, hit test or UIA query reads freed memory.
  - The later handler at 1230-1233 already assumes "the newly assigned model installed with cleared view state".
    `Tree::SetModel` installs first (Tree.cpp:568).
  - The trigger is narrow (a large selection under memory pressure), but the result is a use-after-free.
  - **Fix.** Install the model and release old-model resources before any allocating step. Treat a failed selection
    copy as "no previous selection". Test it with the allocation-failure injection the RichText tests already use.
- [ ] **R28-45 Every typed character cancels pending pointer cascade timers** (#131 `762983a0`) [confirmed]
  - `RouteMenuKeyboardEvent` cancels every popup's hover timer before it looks at the key
    ([Menu.cpp:5454-5457](../../../src/Controls/DxUi.Menu.cpp#L5454)).
  - Every `WM_CHAR` and `WM_SYSCHAR` is classified as a mnemonic (5892, 6241). An unmatched mnemonic changes nothing and
    returns `Consumed` (5683-5720). Such characters include a letter with no mnemonic, Backspace, Ctrl+letter, the
    character echoes of Enter/Space/Escape, and the characters AltGr produces. A no-op arrow also cancels.
  - **Failure A.** The pointer rests on `Recent ▸`, and the 400 ms open is pending. The user types a letter with no
    mnemonic. The row stays highlighted and the submenu never opens. Moving within the row does not re-arm the timer;
    only a move to another row does. This undoes the stationary-pointer guarantee that `d4a36c09` added the same day.
  - **Failure B.** Child B is open from row R, and the pointer moved to sibling S, so a close is pending. An inert key
    keeps B open beside the highlighted S.
  - The contract says "keyboard navigation cancels" and that AltGr "preserves the existing pointer timer behavior"
    (UI_InputAndAccessibility.md:430-432). Only the AltGr key-down is exempt; its character still cancels.
  - **Fix.** Cancel where the keyboard takes ownership: on a `keyboardIndex` change, a mnemonic match, and opening,
    closing or switching popups. One `TakeKeyboardOwnership(controller)` helper replaces the loop. Test an unmatched
    `WM_CHAR` and Backspace for both failures.
- [ ] **R28-48 Retained IconOnly preparation walks the whole tree 4-14 times per button** (`2c38d36a`, `9141a76e`, `b7cd1ffe`) [confirmed]
  - `Button::PrepareVisualContent` (Controls.cpp:2983-3128) re-validates with 14 `IsControlInTree` or
    `stillCurrent` sites. Each one is a downward DFS from the root. A warm hit performs 4, or 8 with a custom font
    size; a cold build performs up to 12. Most follow calls that cannot re-enter control code (`GetTextFormat`,
    `GetFontWeight`, a `com_ptr` copy).
  - Every `prepareAllVisual` pass visits each visible IconOnly button. Such a pass runs after a configured-format cache
    eviction (WindowHost.cpp:4361-4364), a hide, a zero size, device loss, `ReplaceDevice` or a typography change.
  - **Failure.** A toolbar-heavy view with 200 icon buttons in 5,000 controls does millions of node visits per such
    event. The "warm path" of `9141a76e` still pays the walks. This extends CR-25 and A6.
  - **Fix.** Validate once on entry. Afterwards compare `GetHost()`, the root lifetime and `_interactionRevision`.
    Key the layout by the configured-format key instead of the format pointer (A17), so an eviction
    stops forcing a whole-window rebuild. Count walks under diagnostics and assert one per button.

## P3: narrower defects

Each item was traced by its reviewer. On 30 October a second reviewer re-traced every sub-claim; the tags record the
outcome.

- [ ] **R28-28 `ControlKind::Separator` was inserted mid-enum** [confirmed]: every later value shifted by one, and
  apiRevision stayed 4 ([ControlCatalog.h:10](../../../include/DxUi/ControlCatalog.h#L10)). Append the new value or
  pin explicit values. See Q14.
- [ ] **R28-29 Rich editor selection notifications** [confirmed (a); partly (b)]
  - (a) Re-registering only the text callback during scheduling drops the deferred selection event
    (RichTextEditor.cpp:1099-1101).
  - (b) Hiding or disabling mid-drag clears `dragging` without publishing (2863-2878). The notification is deferred,
    not lost: the host's stale-capture cancel calls `OnCaptureLost`, which publishes on the next message. Until then
    `IsSelectionGestureActive()` disagrees with the last callback.
- [ ] **R28-30 Stale hover underline after an edit** [confirmed]: `Changed()` does not reset `hoveredLink`
  (1059-1085). The contract requires source replacement to retire hover.
- [ ] **R28-31 Tree-item and text-pattern UIA gaps**
  - [confirmed 30 Oct] A legacy Tree item renamed in place raises no Name or structure event. Status changes compare
    only `badgeText` (Accessibility.cpp:12713), and structure compares only ids (13552-13564).
  - [refuted] Link `UnderlineStyle` reflects hover or caret on purpose: UI_InputAndAccessibility.md:30-31 asks for
    the currently rendered underline.
  - [confirmed 30 Oct; native, not embedded] Text-input events run `FindAccessibilityPathForTarget` before any
    listener check (12834-12845), up to five times per edit. Only two event kinds use the path. Fix: move the search
    inside that branch, or use `FindWindowHostControlPath`.
  - [confirmed] The dead branch is `if (! target.embedded)` nested inside `if (target.embedded && ...)` (1405-1413).
- [ ] **R28-32 Controls**
  - [partly] Failed intrinsic layout is never retried, even for `E_OUTOFMEMORY` (Controls.cpp:2345-2364), and
    `_intrinsicLayoutFailureCount` has no reader. No retry is intentional, and is pinned, for permanent failures; only
    transient codes deserve one bounded retry.
  - [confirmed] The retained Label typography key omits the mnemonic (2824-2826), and `Control::SetMnemonic` neither
    resets the layout nor invalidates (DxUi.cpp:675).
  - [confirmed] `Separator() noexcept` (DxUi.h:2072) hides the allocating lifetime token of `Control`.
  - [confirmed] `TagPicker::ClearChildren` (7201-7209) uses `TakeChild` and never compacts, so null slots remain.
  - [refuted] Selector buttons measure exactly what they paint (Layout.cpp:289-297 against Controls.cpp:907-913). The
    only gap is the reverse: leading icon and shortcut are accepted and ignored.
  - [confirmed, self-healing] `Panel::TakeChild` notifies ScrollPanel observers (1300) before it can refuse (1309-1325).
- [ ] **R28-33 Grid and Tree** [confirmed 30 Oct]
  - Single-click invoke arms on Ctrl/Shift clicks (Tree.cpp:1794-1803). Its `OnMouseUp` uses a const guard and a strict
    `IsCurrent()` (1887, 1934), so a selection callback that calls `NotifyDataChanged` drops the invoke (the CR-5
    pattern).
  - The Tree wheel returns true at its limits (2025, 2039), so an enclosing ScrollPanel cannot scroll. The Grid returns
    false and pins that in a test.
  - Grid Page keys move a fixed 10 rows (5983-5984). Every `Grid::OnKeyDown`, including ignored keys, builds an
    O(row count) `visibleRows` vector (5776).
  - `Grid::SetModel` on allocation failure: promoted to R28-44.
  - `_activeColumn` survives a same-count column change, and so do widths and order (1805-1819, 6440).
    `GridColumnDesc::id` could key them.
- [ ] **R28-34 Rich text**
  - [confirmed] When ink, focus mode, accent links or hover/caret underlines are active, the selection pass draws the
    whole layout once per selection rectangle (RichTextEditor.cpp:1987-2005). A 40-line selection repaints the block
    40 times per caret blink. Fix: one draw clipped to the union.
  - [partly, benign] `nativeObjectRejections` keeps its syntax while hidden (1290). It normally equals the published
    syntax and resets on mismatch.
  - [confirmed, opt-in] A document-leading heading gets its full before-margin (RichSemanticGeometry.cpp:573, 604-611,
    996).
  - [partly] Table measurement uses the marker flags (no wrap, symbol font for ☐/☑) while final layout does not
    (818 and 1134), and cells get no width slack.
- [ ] **R28-35 Math and Mermaid** [confirmed 30 Oct unless noted]
  - Script and fraction shifts use line-box extents, not ink, so exponents float about twice TeX's height
    (PreparedMath.h:573-576, 601, 833, 1099-1114). See [claimed fixes](#claimed-fixes-that-do-not-hold).
  - `DxUiMath1-target.json` drifts from the parser, and no test checks it. The JSON lists `varkappa`, `omicron`,
    `varpi`, `varrho`, `varsigma` and the `\:` `\;` `\!` spacing, which the parser rejects. The parser accepts `sinh`,
    `cosh`, `vdots`, `ddots`, `mapsto`, `propto`, `sim`, `lbrace` and `rbrace`, which the JSON lacks.
  - `MathNode` moves are not `noexcept` (RichMath.h:58-65). This is deliberate for MSVC Debug and bounded at 512 nodes.
  - `\mathbb` letter doubling happens after the source cap (NativeMath.cpp:74-104). Long input then falls back per
    object, which feeds R28-23.
  - Italic correction, matrix atom class and `\ldots` class need polish.
  - [partly] `Specs/UI/UI_ControlsAndLayout.md:1627-1630` still calls Mermaid painting "in development" against
    `capabilities.json`. `docs/rich-text.md` is incomplete but not stale.
- [ ] **R28-36 Text input** [confirmed 30 Oct unless noted]
  - `TextField::OnChar` publishes a lone lead surrogate to the change callback and UIA (TextInput.cpp:2658-2672).
    `RichTextEditor` buffers it (2310); mirror that.
  - [partly] The native store copies the whole text at least 5-7 times per keystroke (NativeTextInput.cpp:1008-1070,
    113, 2028; TextInput.cpp:2979, 2987, 3558).
  - Caret stepping and Backspace scan from index 0 (SingleLineTextEditing.cpp:32, 66; TextElements.cpp:149).
  - Font, DPI and window moves raise no `OnLayoutChange`. The native `NotifyTextStoreChanged` has one caller, so IME
    candidate windows stay misplaced until the next edit.
  - [confirmed by reading; narrow] A nested activation during an outer `SetFocus` records the outer staged document as
    the previous association (NativeTextInput.cpp:883, 912).
- [ ] **R28-43 Win32 host**
  - [confirmed] The `WM_MOUSELEAVE` hover-leave is the one copy of five without `TryControlCallback`
    (WindowHost.cpp:3338-3361). A throw is caught by `HandleMessage`, which skips the tooltip clear, so the tooltip
    stays stale. The in-tree resolution gap is harmless, because the entry prune validates first.
  - [confirmed, coalesced] `SetVisible` sets all three pending flags unconditionally (DxUi.cpp:430-437). That costs
    one whole-tree walk per prepare, not per toggle.
  - [partly] `batchInitialRootChildren` is missing from `docs/hosting.md` and `capabilities.json`. The keyboard
    pretranslation boundary (docs/hosting.md:112-126) and the animation clock (14-20) are documented.
- [ ] **R28-46 The new menu tests prove less than their names** (#131) [confirmed]
  - The stationary-hover test accepts a submenu that is already open, then fires the timer synthetically
    (Tests.Menu.cpp:5051-5058, 988-1018). A regression that opens with no delay, or arms the wrong period, passes.
    Assert "child not open, pending-open timer present" from the UI-thread readiness hook, and let one real timer
    expire.
  - The test samples state cross-thread after dispatch readiness (5035-5040). It distinguishes the pre-fix source only
    when no synthesized move arms the timer first, a race that the source comment at Menu.cpp:4206-4208 admits.
  - Nothing tests AltGr timer preservation (UI_InputAndAccessibility.md:432) or the R28-45 inert-key cases. The
    keyboard test uses only Home, Right, Left and Down.
  - Timing and foreground fragility:
    - 3466 requires a posted move to be processed within a hard 120 ms wait. Hosted run 40757949246 failed ARM64 ASan
      stationary-mouse tests at that source (Measurements/QualificationReliability_2026-10-28/README.md:25).
    - Two hard foreground checks sit outside `RunUntilForegroundHeld` (1902, 2012-2014).
    - Retried attempts discard their failure text.
    - The cursor is parked at the virtual-screen corner, which can be off every monitor on non-rectangular layouts.
    - The disabled-row case passes vacuously if the cursor misses the popup.
  - No assertion was weakened in #122, #127 or #131, and the keyboard test fails against the pre-fix source.
- [ ] **R28-47 `Attach` is no longer idempotent when only an option differs** (`cfe875fd`, `93cb77bd`) [confirmed]
  - A repeat `Attach` on the same HWND and presentation mode returns false with `ERROR_ALREADY_EXISTS` when
    `batchInitialRootChildren` differs. With an unmatched `forceWarp` it returns `ERROR_INVALID_STATE`
    (WindowHost.cpp:1381-1394).
  - Rendering_Win32Host.md:44 says a repeat for the same HWND and presentation mode is idempotent.
  - **Fix.** Name the options in that sentence, or keep the first options and return true.
- [ ] **R28-49 Tree pointer hit testing builds the full row layout** (`f65dcd5f`, `49c7b57b`) [confirmed]
  - `HitTestPoint` now calls `ComputeItemLayoutMetrics` (Tree.cpp:2600) on every mouse move, press, release and
    tooltip query. That function measures badge text (2505) through a 64-entry FIFO cache with a string compare. A
    miss creates a DirectWrite layout in the input path, and more than 64 visible badge strings thrash the cache that
    Paint relies on.
  - **Fix.** Split out `ComputeExpanderLane(item, rowTop)` for hit testing (A19). Add an RTL test with
    `reserveExpanderSpace`; the current hit tests are LTR only.
- [ ] **R28-50 Re-adopting the same Markdown syntax compares the whole text** (`282da2b1`, `4077f57d`) [confirmed]
  - `TryAdoptParsedSnapshot` runs `MatchesText`, a full piece walk, before the early-out that proves the same wrapper
    and the same snapshot revision (MarkdownSyntax.cpp:573-583; RichDocument.cpp:441).
  - An owner that reasserts a large document per frame pays a full compare each time. The contract promises "no
    allocation", which holds, and implies a cheap call.
  - **Fix.** Test the same-owner, same-revision case first.

## Tooling, CI and process

- [ ] **R28-37 CODEOWNERS leaves the scripts CI runs unowned** (`8252aa27`, widened by #127) [confirmed 30 Oct]
  - Unowned: `build.ps1`, `test.ps1`, `test-consumer.ps1`, `vcpkg-install.ps1`, `gallery.ps1`, `format.ps1`,
    `validate*.ps1`, `Test-Changes.ps1`, `Test-PrePush.ps1`, `Directory.Build.*` and `Tests/test-scopes.json`.
  - #127 adds all of `Build/`. `BuildConsole.psm1` decides whether a build prints green, and
    `NativeBuildPolicy.targets` decides `/Wall` and which warnings are suppressed.
  - `Test-CodeOwners` checks the same narrow regex, so it stays green.
  - Effective only if a code-owner ruleset exists; see Q13.
- [ ] **R28-38 `prContractFiles` omits scripts that decide CI results** (`8252aa27`, widened by #127 and #135) [confirmed 30 Oct]
  - Missing: `Tools/Test-CiGate.ps1` (only its test is listed), `PerformancePolicy.psm1` (build attestation),
    `PerformanceComparison.psm1`, `performance.ps1`, `VisualStudio.psm1`, `Validation.psm1`,
    `Get-BenchmarkScope.ps1`, `Publish-BenchmarkVerdict.ps1`, the setup-msvc action and `Directory.Build.props`.
  - New since 28 October: `Build/BuildConsole.psm1`, `Build/NativeBuildPolicy.targets`,
    `Build/PinnedDxUiPolicy.targets`, `Tools/Test-Changelog.ps1` and `Modules/Documentation/Changelog.psm1`. The new
    `ci.yml` step runs the last two.
  - A branch that weakens one of them keeps the digest, so PrePush still defers the native profiles to CI.
  - **Fix.** Derive the list from `ci.yml`-reachable scripts, or bind a hash of `Build/`, `Tools/`, `.github/` and
    root `*.ps1`, and test completeness.
- [ ] **R28-39 Smaller tooling defects** [confirmed 30 Oct unless noted]
  - `Setup.ps1` waits on the VS installer without a bound (`Start-Process -Wait` at :65, no `-TimeoutSec` at :46).
    A composite step has no timeout, so a hang costs the whole job ceiling. In `paired-benchmark`, setup runs before
    the docs-only scope check.
  - `Complete-TestRunEvidence` in `finally` can replace the real suite failure (test.ps1:297). The exit stays nonzero.
  - [latent] `Invoke-ToolingTests` ignores exit codes. Every current test throws instead, so nothing escapes today.
    Non-Windows skips are uncounted.
  - Changelog fold silently drops a fragment whose text equals an existing Unreleased entry
    (Modules/Documentation/Changelog.psm1:231). The dedupe exists for retry idempotence. `8eef81a8` changed only the
    verifier's rename filter, and its test now blesses the drop. Fix: return and print the skipped fragment names.
  - Consumer receipt counts are literals checked against themselves (test-consumer.ps1:148-149,
    ConsumerQualification.psm1:135).
  - `Setup.ps1` hard-codes the SDK location and version.
  - The hosted-runner check is four environment variables (known design).
- [ ] **R28-51 The zero-warning gate can turn green without inspecting a compile** (#127) [confirmed]
  - With `/WX` off, `Assert-NativeBuildDiagnostics` is the only gate. It counts regex matches in the MSBuild log
    (Build/BuildConsole.psm1:6-23). It has four gaps:
    - An empty or truncated log yields 0/0 and passes; verified with an empty file. Nothing requires
      `Build succeeded.` or cross-checks MSBuild's own warning summary, and the green line prints a literal
      "0 warning(s)".
    - `test-consumer.ps1:81,113` builds DxUi.lib from the relocated clone at `/verbosity:minimal` with no log and no
      scan, including the ASan and `-DisableStlAnnotations` variants. Warnings in those archives go unobserved.
    - An incremental rebuild under a non-default `-OutputRoot` compiles nothing and prints green after a warning
      failure. The default root is protected by the attestation, which forces a Rebuild.
    - The regex backtracks polynomially on whitespace: a 400-space line took 1.7 s locally, and a reviewer measured
      5.5 s for 500 and more than 20 s for 1,000. It has no timeout or length cap. Lines such as `warning count: 0`
      are false positives.
  - The tests use one-line synthetic logs written in the regex's own grammar.
  - **Fix.**
    - Require the success marker and agreement with the summary counts.
    - Scan the `test-consumer.ps1` builds.
    - Use a linear pattern (`NonBacktracking` needs PowerShell 7.3, while the tests declare 7.2) or a match timeout
      that fails the build.
    - Test an empty log, a real C4100 compile and a 5,000-space line.
- [ ] **R28-52 Fold submission trusts names on the remote** (#135 `2b05511a`) [confirmed]
  - An existing remote `changelog/fold-<12hex>` branch is reused without checking that it holds the exact fold
    (Tools/Submit-ChangelogFold.ps1:69-72). Only PR review and CI catch a hand-edited one.
  - Open PRs are matched by `headRefName` only (64-66). A fork PR on this public repository with head
    `changelog/fold-x` blocks every fold until someone closes it.
  - **Fix.** Ignore cross-repository PRs, and run `Test-Changelog -Base` on a reused branch before opening the PR.
- [ ] **R28-53 `Test-Changelog -Base` refuses legitimate changes** (#135) [confirmed]
  - It refuses deleting a pending fragment (a revert or withdrawal; Changelog.psm1:196), correcting a published
    entry, and any change outside `## Unreleased`. No release-cut path exists.
  - It also refuses a compare range that holds both a fragment add and its later fold, as with a PrePush against a
    stale `origin/main` or a multi-commit push.
  - PrePush reads the working tree but takes the base from HEAD, so a dirty tree can be judged differently from the
    pushed commit.
  - `-Optional` fails open on any git error, not only a missing file.
  - No CI bypass was found: `CHANGELOG_BASE` falls back to `HEAD^1`. See Q24.
- [ ] **Still open from CR-29, CR-30 and CR-31** [confirmed 30 Oct]
  - `ci.yml:340-346` supplies only `PAIRED_RESULT` and `PAIRED_RELEVANT`. It never supplies `EXPECTED_BASE_SHA`,
    `EXPECTED_MERGE_SHA`, `PAIRED_BASELINE`, `PAIRED_CANDIDATE`, `VERDICT_RESULT` or `VERDICT_CONCLUSION`, so those
    branches of `Test-CiGate.ps1:56-83` stay inert.
  - The scope, verdict and ci-gate steps run from the merge ref.
  - `format.yml` and `gallery.yml` keep `contents: write` with default `persist-credentials` while running branch
    code. Both need a manual dispatch by a writer and exclude main. `gallery.yml` also runs the branch's setup-msvc
    composite.
  - The new changelog step is clean: read-only token, `persist-credentials: false`, and the base passed through `env:`.
  - The interactive Ctrl+C path has no automated coverage.

## Claimed fixes that do not hold

- **math-parser-12** is still present (R28-07). The 24 October disposition calls `\\` a "supported escape". In TeX it
  is a line break.
- **math-layout-05 and -06** use line-box extents, not ink (R28-35). The test checks only ordering, so the status
  should say partial.
- **MM-33 and MM-34** are listed as handled by family-boundary rows. The code accepts both, and no test covers them
  (R28-09).
- **mermaid-04** status text is stale in the UI spec and docs (R28-35).
- **CR-16** "every superseded-frame return requests preparation" is no longer true for content failure (R28-17).
- **CR-25** stays open. New paint-time layout (R28-26), `IsControlInTree` walks in per-event and per-button paths,
  and badge text in the Tree snapshot copy extend it.

Reviewers report these fixed in code, untested here: commands-01 to -11, -14, -15; editor-01; semantic-02, -03;
geometry-03 (partly, see R28-23); CR-4, CR-5, CR-7 to CR-11, CR-14, CR-26.

## Known items still present

These remain owned by their plans. This review only confirms them at `881e5d5f`:

- **RichText audit:**
  - editor-03, -04, -05, -06, -07, -08, -09, -10, -11, -13, -15, -18
  - document-01, -05
  - commands-13, -16 to -19
  - accessibility-01, -02, -03, -04, -06, -07, -09
  - images-clipboard-01, -03 to -05, -08 to -12, -14 to -16
  - geometry-01, -02, -04 to -13, -15
  - math-parser-03, -10, -14, -15, -17
  - math-layout-07, -09, -11, -14 to -17, -19, -21, -22
  - mermaid-02, -08, -11 to -15
  - host-integration-01, -06, -10
- **10 October review:**
  - CR-18, CR-21 (the menu opening key's `WM_CHAR` is still deferred), CR-22.
  - CR-24: 13 Tree report sites without `TryControlCallback`, including the 2 for single-click invoke (Tree.cpp:744,
    758, 1878, 1932, 1947, 1984, 2224, 2365, 2965, 2979, 3063, 3253, 3265). Six sites are wrapped. A throw is caught
    by `HandleMessage`, which leaves the message half-processed.
  - CR-25: `UpdateHover` runs an uncached prune per mouse move (WindowHost.cpp:5492-5497), bypassing `cfe875fd`'s
    message-entry cache. Its only caller has just run the cached prune (3090-3112), so the second one is redundant.
    R28-48 adds a per-button multiplier.
  - CR-27.
  - CR-37: new test hooks in the production surface, such as `_animationClockForTest` read by `GetAnimationTickMs`,
    the initial-batch hooks and `DebugCaptureBitmap(allowHidden)`. `DXUI_ENABLE_DIAGNOSTICS` is fixed at 1, so they
    always compile.
- **Native counterpart of W1.2 and host-integration-04** [confirmed 30 Oct]. When any control's `PrepareContent` keeps
  failing, the native `Render` abandons the whole frame (Controls.cpp:2382-2385, WindowHost.cpp:4724-4731) and
  schedules no retry. One failing editor therefore stops all painting in its window. Failed intrinsic layout, by
  contrast, paints the previous geometry (Controls.cpp:2352-2356). The contract (DxUi.h:1739) asks for "explicit retry
  without a retry loop", which painting the frame while keeping the control pending would satisfy.
  - The no-retry-loop test makes the editor the root, so no test shows whether siblings paint.
  - Related first-frame gap: a non-device-loss Flush or EndDraw failure, a nested `E_ABORT` and an
    `EnsureSizeDependentResources` failure re-invalidate nothing. Only device loss does (WindowHost.cpp:4588-4617,
    4905-4946). A fresh window can stay blank until input. See Q18.

## Architecture and simplification

Ordered by value. Line counts are reviewer estimates; each needs the same tests as a fix.

- **A1 Decide the callback re-entry contract** (Q1). One rule removes either the R28-14 hazards or a large amount of
  defensive re-validation. Tree.cpp has 155 `IsCurrent` sites and Grid.cpp 347.
- **A2 One allocation-failure policy for input and COM boundaries** (Q2). Put it in the input contract and make
  RichTextEditor, the embedded host, TextField and UIA follow it. This removes four duplicated `terminate` blocks.
- **A3 Replace type ladders with capability virtuals.** Behaviour is chosen by `dynamic_cast<RichTextEditor*>` in:
  - embedded paste (Embedded.cpp `TransformClipboardPaste`) and the sample's `AcceptsFilePaths`;
  - UIA `ScrollIntoView` (Accessibility.cpp:7445);
  - the three TSF target ladders (`ReadBackingState`, `ApplyControlState`, `RestoreCompositionBase`).

  A small set of `Control` virtuals (text capacity, file-path paste, range scroll) fixes R28-05 and saves about 60-80
  lines.
- **A4 Simplify the embedded key receipt API.**
  - `EmbeddedKeyDispatchReceipt` carries 13 fields across three overloads and `ShouldTranslateCharacter`.
  - The sample must reroute Return/Space in its message loop.
  - A documented rule plus one output flag would cover the activation-character case with less API: translate only
    when `DispatchKey` did not handle the key.
  - Decide before more consumers adopt it.
- **A4b Delete the dead non-diagnostic `Render`.**
  - `DXUI_ENABLE_DIAGNOSTICS` is fixed at 1, so the `#else` copy of `ControlHost::Render` never compiles
    ([WindowHost.cpp:4390-4624](../../../src/Controls/DxUi.WindowHost.cpp#L4390)). That is about 235 lines that
    `cfe875fd` and `b93d1527` both edited in lockstep.
  - The same applies to the `#if` blocks inside parameter lists of `ResolveControlInteractionState`,
    `CancelStaleCapture` and `classifyInteractionState`.
  - In the same file:
    - drop the redundant `PruneStaleInteractionState()` in `UpdateHover`, since every caller prunes on entry;
    - factor one `LeaveHover()` for the five hover-leave sequences, which fixes R28-43;
    - drop `_preparedVisualResources` and `_preparedVisualTypography`, which always equal the content pair.
- **A5 One operation-currentness guard in `Embedded.cpp`.** The same `attachmentIsCurrent`/`operationIsCurrent`
  lambda pair appears five times (`ReplaceDevice`, `ReleaseSurface`, `SetVisible`, `Prepare`, `PrepareInteraction`).
- **A6 Membership walks.** `IsControlInTree` is a downward DFS called per mouse move, per button preparation and per
  TSF method. An upward `_parent` walk after a lifetime check, or an interaction-revision snapshot, removes O(tree)
  work from hot paths (CR-25).
- **A7 Rich editor reset helper.** `SetText`, `SynchronizeReadOnlySourceSnapshot`, `ImportTextInputState` and
  `Changed()` duplicate 8-10 reset lines. A `RetireSourceDerivedState(keepChunks)` helper fixes R28-24 and R28-30 and
  saves about 30 lines.
- **A8 `RichSourceInk` delta sweep.** A per-offset delta array (at most 64K units) replaces the resumable merge sort,
  its 12 state fields and the 65,536-event cap: about -110 lines.
- **A9 Shared selection assignment** for Grid and Tree (fixes R28-20): about -40 lines.
- **A10 Remove the Tree's explicit single-click capture.** The hosts already capture every handled press. Removing it
  drops `_singleClickCaptureArmed` and five release blocks (about -35 lines) and closes the retirement hazard its test
  guards.
- **A11 Math structure.**
  - Use one `MatrixKind` table instead of three environment-name lists.
  - Store shifts during measure instead of recomputing them in `Place`.
  - Use one node-count and depth validator instead of four.
  - Together about -95 lines.
- **A12 TSF and clipboard reuse.**
  - `AdviseSink` can use `HasSameComIdentity`.
  - Merge the `Clear` and `Restore` association pairs into one helper.
  - `ValidClipboardUnicode` duplicates `RichText::IsValidUtf16`.
  - `std::move` the cached text at TextStoreACP.cpp:766.
  - Together about -50 lines.
- **A13 Tooling.**
  - Add one `vswhere` helper (four copies today).
  - Add one bounded process launcher (about 12 copies in `Tools/tests`).
  - Keep one verdict-agreement rule, shared by `BenchmarkGate.psm1` and `performance-paired.ps1`.
  - Read one MSBuild property for the SDK version.
- **A14 File size.** `DxUi.Accessibility.cpp` (14.3K lines), `DxUi.Controls.cpp` (11.9K) and `DxUi.h` (6.2K) are the
  review bottlenecks. Splitting them by provider/control family would cut review and rebuild cost. They grew by
  1,661, 1,783 and 693 changed lines in this window. This is a mechanical change, best done in a quiet window.
- **A15 Evidence volume.** 12.4 percent of the window's changed files are `Changes/` and about 20 percent are
  `Measurements/` and fixtures. Plans reach 200-255 KB. Consider moving raw evidence to release assets, as CR Q11
  already decided for packets, so code review diffs stay readable.

Added on 30 October:

- **A16 One warning-classification rule instead of 70 copied pragma blocks** (#127).
  - The window adds 264 `#pragma warning(disable ...)` lines across `src`, `include`, `Tests` and `Samples`.
    `src` and `include` now hold 73 push/pop blocks, 37 of them in the public `DxUi.h`.
  - 207 of those lines name `4625 4626 5026 5027`: the notices that an implicit copy or move was deleted, for
    classes that are non-copyable by design.
  - Every block carries the same generic two-line rationale and review date, so the "narrow scope, rationale, owner"
    rule of Build_ConsoleAndPaths.md:18 is met in form only.
  - `IGridModel` took the opposite route and defaulted its copy operations.
  - Classifying 4623/4625/4626/5026/5027 once in `NativeBuildPolicy.targets` beside 4820 would delete the blocks. So
    would one push/pop around the class section of each header. Either keeps real conversion, lifetime and enum
    diagnostics. See Q20.
  - The same commit spells new case labels fully qualified (`::DxUi::RichText::MarkdownBlockKind::...`) beside
    short-named neighbours. Match the surrounding style.
- **A17 Key retained icon layouts by the configured-format key** (R28-48). Then `TrimCaches` no longer forces
  `_forceAllVisualPreparation`, evicted formats are not retained through `com_ptr`, and a working set above 96
  formats stops rebuilding a layout per button each frame. One validation snapshot replaces about 25 re-checks
  (about -40 lines). The four identical `DrawCenteredText` fallbacks in IconOnly paint collapse to one flag.
- **A18 `TakeKeyboardOwnership` for menus** (R28-45). This one helper replaces the unconditional cancel loop and
  states the contract in code.
- **A19 Split Tree row geometry** (R28-49). `ComputeExpanderLane` serves hit testing. Badge, text and mirroring stay
  in Paint and tooltips. The `visibleIndex` overload, which only computes `rowTop`, folds in.
- **A20 Narrow the include roots** (#127). `NativeBuildPolicy.targets` gives the library project itself
  `Tests\Support`, `Tests\Embedded`, `Samples\EmbeddedControls` and `Samples\ComplexUi`. No library source uses them
  today, and the relocated-consumer compile would catch a misuse. Keep `src` for the library and its tests, and give
  the test and sample roots only to their own projects. The shared file also names consumer triplet properties and
  `ThirdParty\RedMonitor`. That is the cost of keeping it byte-identical across five repositories; see Q21.
- **A21 Host message-path cleanups.**
  - `CanReuseMessagePruneSnapshot` re-captures and re-checks after a validation that calls only const virtuals
    (WindowHost.cpp:5156-5209).
  - `MessagePrunePass` duplicates observer lifetimes and paths.
  - "Owner still current for a translated character" is validated in four places: WindowHost.cpp:3009-3019, the
    `WM_KEYDOWN` tail, and Embedded.cpp at about 980 and 1022.
  - `InsertTextAtSelection` pre-checks the capacity that `ReplaceTextRange` checks again.
  - `Render` carries the initial-batch setup twice.
  - Both branches of the `ReplaceDevice` tail clear `preparationRequested` (Embedded.cpp:251-256).

## Questions for the developer

1. **Callback re-entry (R28-14, A1).** May an embedded `requestPreparation` callback retire or replace the control
   tree synchronously? `Embedded.h` says no; the retirement tests say yes. Recommendation: forbid it in the contract
   and enforce it with a re-entrancy guard plus a debug assertion. If you keep it allowed, every setter must become
   retire-safe and needs a test.
2. **Allocation failure at input boundaries (R28-15, A2).** Should a failed key, mouse or paste allocation decline
   the event or terminate the process? Recommendation: decline atomically. Keep `terminate` only where the contract
   already requires it, inside observer notification callbacks.
3. **Focus requested while hidden (#121).** A host that is hidden, zero-sized or inside a `WM_SETREDRAW FALSE` block
   keeps logical focus but never takes keyboard focus when it becomes visible. Should showing such a host transfer
   native focus that a request asked for?
4. **Read-only Source mirror (R28-24).** Should sync keep the mirror's scroll position and reusable chunks, unlike
   `SetText`?
5. **Content failure in embedded `Prepare` (R28-17).** Distinct HRESULT, bounded automatic retry, or documentation only?
6. **Source-ink renderer (R28-25).** Add a kill switch, or a preview status in `capabilities.json`, until the paired
   gate passes? Which Source fonts must it support?
7. **MSVC 14.52 exact pin.** `/GL` archives force every consumer, and `vcpkg-install.ps1`, onto exactly 14.52.x, so a
   routine Visual Studio update breaks consumer builds. Do the RedSalamander and RedXe pins accept that? Should Release
   also ship a non-LTCG archive?
8. **Math.** Should `\\` outside an environment reject (recommended) or start a display line? Is the line-box vertical
   basis acceptable for now? If so, downgrade math-layout-05/06 to partial.
9. **Mermaid.** Should `:::`, `accTitle` and `accDescr` reject in state and class diagrams? Should a diagram wider
   than the view scale down as math does instead of falling back to source?
10. **Tree behaviour (R28-13).** Should `NotifyDataChanged` ever scroll? Should typeahead keep a matching current row?
    Should a first key on an unfocused single-select Tree notify? Should Ctrl/Shift clicks invoke in single-click mode?
11. **TSF.** Focus loss currently discards an in-progress composition, where Win32 Edit commits it. Is that intended?
    May paste retry `OpenClipboard` briefly when a clipboard manager or RDP holds it?
12. **UIA.** `Paragraph` and `Page` text units map to `Document`, so screen-reader paragraph navigation jumps to the
    edges. Is that intended? What cap should replace per-item badge events (R28-18)?
13. **CI governance (R28-37, R28-38).** Is a ruleset requiring code-owner review enabled on `main`? Was leaving
    `Test-CiGate.ps1` and `PerformancePolicy.psm1` out of `prContractFiles` intentional?
    - While the paired policy is `policy-review-required`, every measured-path PR fails as inconclusive. How do such
      PRs merge?
    - Refusing any reparse ancestor blocks checkouts on a folder-mounted Dev Drive. Is that intended?
14. **API revision (R28-28).** Is the `ControlKind` renumbering deliberate? Do it and the public `RichMath.h` additions
    (`MathKind::Delimiter`, new `MathNode` fields) need an API revision bump or changelog note?
15. **Library scope.** `Button::SetThemePreview` and the theme swatch live in the library while AGENTS.md keeps themes in
    consumer adapters. Is it a library feature? Should docs/controls.md say Separator has no intrinsic size?
16. **Remote image failures (R28-27).** Should the library or the application own negative caching?
17. **Occlusion (R28-42).** Does "occluded work is event-blocked" in the performance contract bind the native host, or
    only embedded hosts?
18. **Native preparation failure.** Is the all-or-nothing native frame intended when one control's `PrepareContent`
    keeps failing? After a failed first frame (Flush, EndDraw or a nested `E_ABORT`), nothing re-invalidates, so a fresh
    window can stay blank until input. Should preparation return an HRESULT, so that device loss reaches recovery and
    the first frame retries once?
19. **Keyboard and redraw edge cases.**
    - Should `DispatchKeyboardMessage` also test `_nativeTextInputTsfComposing`? The contract says "active native IME
      composition", but only the IMM flag is checked.
    - `WM_SETREDRAW(FALSE)` now retires prepared layouts (WindowHost.cpp:3245-3249). Does a `ScopedWindowRedrawBlock`
      per interaction then force a full re-layout of large documents?
    - Has `PressStillOwnsPointer`'s `GetKeyState` check been verified for pen barrel buttons and touch-promoted
      presses?

The repository records no answer to Q1-Q19 as of 30 October. The refresh adds:

20. **Warning classification (A16).** May the policy classify the implicit-deletion notices (4623, 4625, 4626, 5026,
    5027) once, as it does 4820, instead of 73 per-class blocks? If not, may each header use one region?
    Recommendation: classify them in the policy. They describe intended non-copyable types, never a defect.
21. **Shared build policy (A20).** `NativeBuildPolicy.targets` and `BuildConsole.psm1` are kept byte-identical in
    five repositories, so DxUi's copy names RedXe, RedPrism and RedSalamander triplets and `ThirdParty\RedMonitor`.
    Is that acceptable in a library repository? What detects drift between the copies?
22. **Menu keyboard and timers (R28-45).** Should only keys that take keyboard ownership cancel pending pointer
    timers? The current code cancels on any character, including an unmatched mnemonic and Backspace. Separately:
    should the 400 ms cascade delay follow the user's `SPI_GETMENUSHOWDELAY`? It is hard-coded (Menu.cpp:81), which
    ignores an accessibility setting. This predates the window.
23. **Embedded printable keys.** The native host reports a printable key-down in a focused editor as handled, so the
    application's bare-key shortcuts do not fire (R28-40). The embedded `DispatchKey` returns false, so an embedder's
    accelerator fires while the user types. Should `DispatchKey` match the native host, or should docs/hosting.md
    tell embedders not to treat bare printable keys as accelerators while `SupportsTextInput()` holds?
24. **Changelog exits (R28-53).** How should a revert that withdraws a pending fragment, a correction to a published
    entry, or a release cut pass `Test-Changelog -Base`? Should a fold that drops a fragment with duplicate text
    report it?
25. **Attach options (R28-47).** Are `batchInitialRootChildren` and `forceWarp` part of an attachment's identity? If
    so, document them; if not, keep the first options and succeed.
26. **Grid model installation (R28-44).** Should `SetModel` guarantee installation under allocation failure, with no
    API change (recommended), or report failure to the caller?

## Proposed sequence

1. **Answers.** Q1, Q2 and Q7 decide the scope of R28-14, R28-15 and A2. Q20 and Q21 decide the scope of A16 and A20.
   Q3-Q6, Q8-Q19 and Q22-Q26 decide behaviour, not effort.
2. **Wave 1: small P1 fixes, each with its failing test first.** R28-01, -02, -03, -04, -07, -08, -09, -10, -11, -12,
   -40, and the memory-safety fix R28-44. Most are under 20 lines. R28-02 and R28-03 are paired items: they touch
   preparation or paint.
3. **Wave 2: input and device contracts.** R28-05, -06, -16, -41, then R28-15 and R28-14 per Q1/Q2, with A3 as the
   vehicle. R28-41 adds WARP device-loss evidence. R28-45 and R28-46 with A18 per Q22, so the menu fix lands with the
   missing tests. The native preparation-failure item per Q18.
4. **Wave 3: bounded work.** R28-18, -20, -21, -22, -23, -24, -26, -27, -48, -49, -50, R28-25 per Q6, R28-42 per
   Q17, and A6/A17. Every item is paired against a retained baseline on the same fixture, with no rebaseline.
5. **Wave 4: CI trust.** R28-37, -38, -39, -51, -52 and -53, together with the open CR-29/CR-30 work. R28-51 comes
   first: since #127 it is the only warning gate.
6. **Wave 5: simplification.** A4 to A13 and A16 to A21, each behaviour-preserving under existing suites. A14 last, in
   an agreed window.

Every code change follows AGENTS.md:
- run `test.ps1` in x64 Debug, Release and ASan Debug, and build all three ARM64 profiles;
- run `validate.ps1` and `format.ps1 -Check`;
- add one `Changes/` fragment (since #135 it is folded on main after merge, not in the change);
- review docs and the gallery under the documentation contract.

Focus-taking suites run only at an agreed time.

## Checked and found sound

- **Posted payloads (`dcabe291`).** The type tag is a distinct writable object per type, so identical-code folding
  cannot merge it. Reclaim versus Take is decided under the registry mutex. `SMTO_NORMAL` removes the mutual-send stall.
  - No other code compares function addresses for identity, so Release `/OPT:ICF` has no other victim.
- **TSF focus restoration (#121).** The empty-focus and same-association restore, never overwriting a different
  document or a newer transition, matches the contract and its 11 mock cases.
- **Hidden-host focus (#121).** It matches the new contract and the existing text-input guard. Q3 is about intended
  behaviour, not a defect.
- **Embedded host.**
  - Attachment and operation revisions guard `Detach`, `ReplaceDevice`, `SetVisible`, `Prepare` and
    `PrepareInteraction` against address reuse after re-attach.
  - The preparation-request latch cannot strand a visible, sized view.
  - The animation timeline saturates.
- **#117.** `ProgressBar::SetAnimationPaused` freezes the phase and resumes from a fresh baseline. The Grid UIA
  selection actions now also revalidate the model generation.
- **Win32 host (reviewer, lines spot-checked).**
  - Message-prune snapshot reuse (`cfe875fd`).
  - `DispatchKeyboardMessage` frames and character permission.
  - The `SetFocusControl` early reject.
  - Non-client hover for Snap.
  - Two-button capture.
  - The `forceWarp` pool lock order.
  - Initial root-child batching.
  - Update-stage timing (`b93d1527`).
  - The menu modal loop's idle wait (`INFINITE` `MsgWait`), AltGr and RTL keys.
- **Reviewers' sound areas.**
  - ScrollPanel gesture ownership, TabControl drag reorder and the IconOnly prepared-layout key.
  - Configured format cache eviction.
  - Snapshot sharing and parse adoption, which is atomic.
  - `SetAppearance` and `SetSourceHighlights` staging.
  - The `SourceInkRenderer` COM and glyph-run handling.
  - Image admission limits and path parsing.
  - Parser recursion and every math/Mermaid cap.
  - TSF lock bracketing and clipboard staging.
  - UIA cross-thread dispatch, lock ordering and text-unit math.
  - `TestRunEvidence` path safety, changelog fold atomicity and the interactive lease's fail-closed parsing.

## 30 October refresh

**What #122-#137 changed.**
- **#122 and #123** (test diagnostics and evidence). Phase diagnostics for unstable complex-UI benchmark controls run
  only from the opt-in command, outside the counted allocations. They run in both arms, so an Off/On contrast cannot
  isolate their own cost. The performance contract already says so.
- **#127** (shared native build policy).
  - `/Wall` without `/WX` in every profile, enforced by the log gate (R28-51).
  - Release-only `/OPT:ICF`, `/ZI` for Debug and `/Zi` for ASan Debug.
  - `src`-rooted private includes and a new consumer import.
  - In `src` and `include` the change is mechanical. Reviewers read every hunk that is not a pragma, include or
    comment, and all of them preserve behaviour: explicit case labels before an existing `default`, casts, the unused
    `FindSizeValueIndex` and four dead test helpers removed, `IGridModel` copy operations defaulted (no ABI change),
    unused lambda captures dropped. Iterator to `.data()` changes in tests lose only a Debug STL bounds assertion.
- **#131** (menu fixes and qualification records).
  - `d4a36c09` arms the cascade delay for a stationary pointer once the popup is shown. It is sound;
    keyboard-initialized popups set `keyboardIndex` first, so they skip it.
  - `762983a0` cancels pointer timers on keyboard input (R28-45, R28-46).
  - `85716c8f` keeps the reading direction in the icon-font fallback. It is sound and documented.
  - Qualification journal recovery retries only sharing and lock violations, with identity checks, and its tests use
    real locks.
- **#135 and #137** (fold changelog fragments on main).
  - No injection was found: argument arrays, strict fragment-name regex, hex branch names.
  - No force push was found.
  - Path traversal and junctions are guarded on every path up to the root.
  - A concurrent fold is refused by the non-fast-forward push and the open-PR check.
  - Gaps are R28-52 and R28-53.

**Refuted during the refresh** (one line each, so they are not raised again):
- Message-prune snapshot reuse going stale, or skipping `CancelStaleCapture`: reuse re-walks the live path with
  lifetime checks.
- `this` after `DispatchMessageW` in `DispatchKeyboardMessage`: destroying the host in a callback is excluded by
  Rendering_Win32Host.md:29.
- `ReplacementFitsCapacity` rejecting deletions: client snapshots are bounded at 65,536 units.
- `33c3086c`, `a5e88086`, `1714f8b0`, `da335d36`, `081c8551`, `93cb77bd` (`forceWarp` is not sticky past the last
  detach) and `dcabe291` (the ICF-safe tag): no defect.
- ABA on configured-format pointers after `TrimCaches`: a pointer for key K always belongs to K.
- Stale IconOnly layout after a hide, page swap or detach: `CancelPreparedVisualContent` and `PropagateHost` retire it.
- Read-only mirror aliasing and notification order (`282da2b1`, `47a9d259`, `5a2131ad`, `1cff7656`): sound, apart from
  the known R28-29(a).
- The clean-Prepare `S_FALSE` invariant: contract-backed (Rendering_EmbeddedD3D11.md:128, 174-175; Embedded.h:144).
  Its report kept `"schema":1` although the timed bracket now includes result classification. The cost is
  negligible, but old and new reports cannot be told apart.
- `/ZI` with `/GL`, and `ClCompile Update` clobbering DxUi per-file metadata: neither happens. Consumers that set
  their own per-file `DisableSpecificWarnings` keep 4746 but lose the five classified notices, so they would fail
  the zero-warning gate. Nothing in DxUi does that.

**Not covered.** The `AdjacentSourceLine` and semantic-spacing geometry of `2411cbca`, beyond its lifetime guards.
Runtime behaviour: nothing was built or run, and every finding is a static trace. #138 (shared CI, `a412f316`) merged
after the window and rewrote the workflows, `Tests/test-scopes.json`, `build.ps1` and `test.ps1`. R28-37, R28-38, R28-39,
R28-51 and the CR-29 to CR-31 entries describe `09f6d96a`; recheck their line references and status against #138.
