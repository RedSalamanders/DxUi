# Standard controls and native text input: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: `src/Controls` standard controls (Label, Button,
Slider, TabControl, ScrollPanel, Tree, Grid, Menu, ComboBox, editor controls), TextField, native text input, the TSF text
store and the embedded `TextInputServices` clipboard and text paths.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| CTRL-01 | P1 | Confirmed | TextField and RichTextEditor drop supplementary characters delivered as `VK_PACKET` | `src/Controls/DxUi.TextInput.cpp:2222`, `src/Controls/DxUi.RichTextEditor.cpp:5659` | M |
| CTRL-02 | P3 | Confirmed | Every Label runs a full-tree `IsControlInTree` DFS in visual preparation | `src/Controls/DxUi.Controls.cpp:2944` | S |
| CTRL-03 | P3 | Plausible | TSF `GetStatus` copies the document and can fail with `TS_E_INVALIDPOS` | `src/Controls/DxUi.TextStoreACP.cpp:1347` | S |
| CTRL-04 | P3 | Confirmed | Grid navigation keys are O(rows) in time and allocation (duplicates prior R28-33, still open) | `src/Controls/DxUi.Grid.cpp:5886` | M |
| CTRL-05 | P3 | Confirmed | Bitmap paste has no declared decline/fallback path | `src/Controls/TextInputServices.cpp:873` | S |
| CTRL-06 | P3 | Confirmed | Paint trusts unvalidated prepared inputs (Button shortcut width, TabControl header geometry) | `src/Controls/DxUi.Controls.cpp:3805` | S |
| CTRL-07 | P3 | Plausible | `Slider::OnMouseDown` leaves `_dragging` set after a refused capture | `src/Controls/DxUi.Controls.cpp:6991` | S |
| CTRL-08 | P3 | Confirmed | Menu keyboard-ownership revision check is unreachable; duplicated stationary-pointer guard | `src/Controls/DxUi.Menu.cpp:4473` | S |
| CTRL-09 | P3 | Confirmed | `TextClipboardPasteTransform` receives raw clipboard text; the contract is undocumented | `src/Controls/TextInputServices.cpp:918` | S |
| CTRL-10 | P3 | Plausible | Grid column-identity reset does not end in-flight header gestures | `src/Controls/DxUi.Grid.cpp:1874` | S |
| CTRL-11 | P3 | Plausible | TextField setters invalidate before normalizing caret, selection and layout caches | `src/Controls/DxUi.TextInput.cpp:963` | S |
| CTRL-12 | P3 | Plausible | Nested TSF activation that also fails leaves the HWND associated with a popped document | `src/Controls/DxUi.NativeTextInput.cpp:858` | M |
| CTRL-13 | P3 | Confirmed | Heap work on the TextField paint path (pre-existing; reduced in the window) | `src/Controls/DxUi.TextInput.cpp:827` | S |
| CTRL-14 | P3 | Confirmed | The split detach protocol changes behaviour for legacy containers and refused removals | `src/Controls/DxUi.Controls.cpp:1298` | S |

Counts: P0 0, P1 1, P2 0, P3 13. Verdicts: 9 confirmed, 5 plausible, 0 refuted.

## P1 findings

### CTRL-01 — TextField and RichTextEditor drop supplementary characters delivered as `VK_PACKET`

- **Severity** P1 (reviewer P1, verifier P1) · **Category** ime / bug (silent data loss of typed text) ·
  **Verdict** Confirmed · **Confidence** high (code path) / medium-high (Win32 message order is documented behaviour) ·
  **Effort** M · **Introduced** `15e0b956` (TextField, 2026-11-01) and `c1a77e42` (RichTextEditor, 2026-10-21; before the
  review window)
- **Location**: `src/Controls/DxUi.TextInput.cpp:2222` (first statement of `TextField::OnKeyDown`:
  `_pendingLeadSurrogate = 0;`) with `src/Controls/DxUi.TextInput.cpp:2748-2755` (`TextField::OnChar` buffering);
  `src/Controls/DxUi.RichTextEditor.cpp:5659` (`RichTextEditor::OnKeyDown`: `_state->pendingLeadSurrogate = 0;`) with
  `src/Controls/DxUi.RichTextEditor.cpp:5309-5323` (`RichTextEditor::OnChar`);
  `src/Controls/DxUi.WindowHost.cpp:3756-3897` (every `WM_KEYDOWN` reaches `OnKeyDown`: `:3790` for `VK_TAB`, `:3839`
  otherwise); `src/Rendering/Embedded.cpp:1032` (`EmbeddedHost::DispatchKeyImpl` forwards every key to `OnKeyDown`) and
  `src/Rendering/Embedded.cpp:1077` (`DispatchCharacter` forwards to `OnChar`).
- **What**: both editors hold a UTF-16 lead surrogate privately until its trail arrives (TextField: `OnChar` keeps it in
  `_pendingLeadSurrogate` and consumes a trail that has no lead; RichTextEditor: `_state->pendingLeadSurrogate`, same
  rule). Both `OnKeyDown` functions clear the pending lead for every key, with no filter. `VK_PACKET`, the key that
  `SendInput(KEYEVENTF_UNICODE)` generates for each UTF-16 unit, is not filtered anywhere in the window host or the
  embedded host. The editable ComboBox embeds a TextField and is affected as well. `VK_PACKET` appears nowhere in
  `src`, `Tests`, `Specs` or `docs`, so no test or document covers it.
- **Failure scenario**: a remote-desktop session (RDP Unicode keyboard events), AutoHotkey `Send {U+1F600}`, a password
  manager's auto-type, an on-screen keyboard or a dictation tool injects U+1F600 into a focused field containing `A`.
  The window receives `WM_KEYDOWN(VK_PACKET)`, `WM_CHAR(0xD83D)`, `WM_KEYUP(VK_PACKET)`, `WM_KEYDOWN(VK_PACKET)`,
  `WM_CHAR(0xDE00)`, `WM_KEYUP(VK_PACKET)` (`TranslateMessage` posts each `WM_CHAR`; posted messages are retrieved before
  hardware input, so each `WM_CHAR` follows its own key-down). The second key-down clears the lead; the trail then finds
  no lead and is consumed. The text stays `A` and nothing is reported. Before `15e0b956` the TextField inserted each unit
  separately and the pair survived; the RichTextEditor has lost such input since `c1a77e42`. Physical keyboard layouts
  (both `WM_CHAR` units come from one `TranslateMessage`) and IME or TSF commits (they go through the text store) are
  not affected.
- **Fix**:
  1. Keep the pending lead across a `VK_PACKET` key-down and its key-up. Clear it only on focus loss, on a non-trail
     `WM_CHAR` (both `OnChar` functions already do this), or on an editing command (caret or selection movement, delete,
     undo, paste, Enter and similar), plus the existing text, selection, pointer and read-only resets.
  2. In both `TextField::OnKeyDown` and `RichTextEditor::OnKeyDown` replace the unconditional clear with a conditional
     one. Add a small shared predicate (for example `KeyKeepsPendingSurrogate(UINT virtualKey)`, next to
     `IsUtf16LeadSurrogate` in `src/Controls/DxUi.TextElements.h`) that returns true for `VK_PACKET` and for the bare
     modifier keys (`VK_SHIFT`, `VK_CONTROL`, `VK_MENU` and their left/right variants), and use it in both editors so
     they cannot diverge. Do not add a clear to either `OnKeyUp`.
  3. No embedded-specific code is needed because `EmbeddedHost::DispatchKey` and `DispatchCharacter` call the same
     control functions. Say in `docs/hosting.md` that a consumer forwards `VK_PACKET` key-down/key-up and each translated
     `WM_CHAR` unit unchanged and in order.
  4. Update the "TextField supplementary character input" paragraph of `Specs/UI/UI_ControlsAndLayout.md` (added by
     `15e0b956`) to say that `VK_PACKET` and bare modifier key events do not cancel a pending lead, and add a
     `Changes/` fragment.
- **Regression test**:
  1. Window host (`Tests/Controls/DxUi.Tests.WindowHost.cpp`, or `Tests/Controls/Controls.Tests.HostControlRegressions.cpp`):
     focused TextField containing `A`; drive `ControlHost::HandleMessage` with `WM_KEYDOWN(VK_PACKET)`,
     `WM_CHAR(0xD83D)`, `WM_KEYUP(VK_PACKET)`, `WM_KEYDOWN(VK_PACKET)`, `WM_CHAR(0xDE00)`, `WM_KEYUP(VK_PACKET)`. Assert the
     text is `A` followed by the pair, with one text-changed notification carrying the whole pair. Today the text stays
     `A`.
  2. Same replay for a focused RichTextEditor in `Tests/Controls/Controls.Tests.RichTextInput.cpp`; assert the document
     text contains the pair once.
  3. Embedded host (`Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h`): for a TextField and a RichTextEditor in an
     `EmbeddedHost` view, call `DispatchKey(VK_PACKET, true, 0u)`, `DispatchCharacter(lead, 0u)`,
     `DispatchKey(VK_PACKET, false, 0u)`, then the same for the trail; assert the pair is inserted.
  4. Negative cases for both editors: a `VK_HOME`, `VK_BACK`, `VK_LEFT` or Ctrl+Z key-down between lead and trail cancels
     the lead (the trail is consumed and the text is unchanged); a `VK_SHIFT` key-down between them does not; focus loss
     cancels; a non-trail `WM_CHAR` after the lead drops the lead.
  5. Change `TestTextFieldBuffersSurrogatesBeforePublishing` (`Tests/Controls/DxUi.Tests.TextField.cpp:2316-2338`). It
     encodes the unconditional-clear contract (it calls `OnChar(lead)` and `OnChar(trail)` back to back and only tries
     `VK_HOME` as an intervening key-down, `:2332-2333`). Keep the `VK_HOME` case as an editing-command cancel and add the
     `VK_PACKET` and modifier cases so the test no longer asserts that any key-down cancels the lead.
- **Verifier notes**: the window-host route was read from `DxUi.WindowHost.cpp:3756-3897`: no `VK_PACKET` filter,
  `HandleTooltipInspectionKey` does not consume it, the IME-composition early return applies only while composing, and
  `DispatchKeyboardMessage` special-cases only `VK_RETURN`/`VK_SPACE`, so the application's normal pump calls
  `TranslateMessage` for `VK_PACKET`. `TextInputServices::PreTranslate` (`src/Controls/TextInputServices.cpp:957-981`) runs
  `TestKeyDown` first; input methods normally do not eat `VK_PACKET`, so this precedes rather than replaces the
  `WM_CHAR` path. Whether a particular input method intercepts `VK_PACKET` is not statically provable; when it commits
  through the text store there is no bug. The intervening `WM_KEYUP` is harmless (neither `OnKeyUp` clears the lead). The
  RichTextEditor defect is pre-existing and was found by the verifier, not the reviewer.
- **Lead note**: severity held at P1 as a regression of working behaviour with silent data loss and a common trigger
  (remote-desktop Unicode input); P2 is defensible only if injected supplementary-plane input is judged niche.

## P3 findings

### CTRL-02 — Every Label runs a full-tree `IsControlInTree` DFS in visual preparation

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf · **Verdict** Confirmed (mechanism) · **Confidence**
  high (complexity) / medium (real-world magnitude) · **Effort** S · **Introduced** `f6c6fcdb` (2026-11-01)
- **Location**: `src/Controls/DxUi.Controls.cpp:2782-2785` (`Label::Label` calls `EnableVisualContentPreparation()`),
  `:2796-2815` (bounds, flow, density and DPI changes request preparation), `:2944` (`Label::PrepareVisualContent` runs
  `IsControlInTree(root, this)` before the `_typographyOverride || GetMnemonic()` test at `:2946`),
  `src/Controls/DxUi.WindowHost.cpp:1195-1213` (`IsControlInTree`, an unbounded logical-child DFS),
  `src/Controls/DxUi.Controls.cpp:2451` (`PreparePendingPageLayout` calls `PrepareVisualContent` for each pending control).
- **What**: every Label, including those with no mnemonic and no typography override, is now a visual-preparation
  participant. When it is prepared, it first walks the host tree from the root to prove membership and only then finds
  it has nothing to prepare. The preparation walk already reached it from the root.
- **Failure scenario**: the cost is the sum of each prepared Label's position in the DFS. Every Label is prepared on the
  first pass, on theme, typography or resource-generation changes (`CancelPreparedVisualContent`,
  `src/Controls/DxUi.Controls.cpp:2578-2598`, marks every control pending), and on live-resize frames for the Labels
  whose bounds change. For the menu popup, `PopulateMenuAccessibility` (`src/Controls/DxUi.Menu.cpp:3546-3585`) adds one
  `MenuAccessibilityItem<Label>` per row as a direct child of one `MenuContentControl`, so row i costs i+1 loop
  iterations (about 3-4 virtual calls each) and opening costs about N squared over 2 iterations: N=200 is about 20k
  (0.1 ms), N=1,000 about 500k (3 ms), N=2,000 about 2M (10-16 ms, a dropped frame). A wheel step is not quadratic:
  `SynchronizeMenuAccessibility` (`:3594-3615`) calls `SetBounds` only on rows whose viewport-clamped rect changed, which
  is the 15-30 rows in the viewport, about 25-30k iterations (0.2 ms) at N=2,000.
- **Fix**: in `Label::PrepareVisualContent`, test `_typographyOverride || GetMnemonic()` first and cancel/return before
  the tree walk; this removes the cost for every plain Label with no behaviour change. Dropping `IsControlInTree` is not
  a no-op: the preparation walk also visits a `PageHost` outgoing page (`:2492-2495`), which `IsControlInTree` rejects
  today because `PageHost::GetLogicalChild` exposes only the current page (`:2059-2072`); `current()` already checks
  `GetHost() == &host`. If the walk is kept for override/mnemonic Labels, accept that behaviour. Optionally enable
  preparation lazily from `SetMnemonic` and `SetTypographyOverride` (the latter already does at `:2907`), keeping the
  "mnemonic cleared" transition able to cancel the retained layout.
- **Regression test**: `Tests/Controls/Controls.Tests.IntrinsicLayout.cpp`: build 2,000 plain Labels under one Panel
  whose `GetLogicalChild` is counted (a counting Panel subclass avoids a production counter in a hot function), prepare,
  resize the root, prepare again, and assert the count is O(N); today it is about N squared over 2. In
  `Tests/Controls/DxUi.Tests.Menu.cpp`, bound the preparation work at popup open for a 500-row scrolling menu. Do not
  assert a per-wheel-step bound: it already holds. The complex-UI FPS receipt should show the change.
- **Verifier notes**: the call order, the constructor change and the introducing commit were read at HEAD. The
  reviewer's "O(rows squared) on every wheel step" sub-claim was refuted (see the Refuted table).
- **Lead note**: severity P3 because it takes thousands of flat Labels in one container to cost a frame; reconsider as P2
  if a consumer tree has 2,000 or more Labels in one container.

### CTRL-03 — TSF `GetStatus` copies the document and can fail with `TS_E_INVALIDPOS`

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf / api (TSF contract) · **Verdict** Plausible ·
  **Confidence** medium · **Effort** S · **Introduced** `b5869cbc` (2026-11-01)
- **Location**: `src/Controls/DxUi.TextStoreACP.cpp:1347-1364` (`GetStatus`), `:1771` (`ReadState`),
  `:634-656` (`NativeTextStoreTarget::ReadState`), `:716-733` (`IsCurrentTextServiceTarget`), `:765-791`
  (`ReadBackingState`), `src/Controls/DxUi.NativeTextInput.cpp:2099` (`TryReadNativeTextInputState` copies the cache),
  `src/Controls/TextInputServices.cpp:125-157` (client store `ReadState`, a synchronous `TextInputClient::Read`).
- **What**: to report `TS_SD_READONLY` and `TS_SS_MULTILINE`, `GetStatus` copies the whole text twice on the native path
  (cache copy, then `outState.text = nativeState.text`) and takes an application snapshot on the embedded path. When the
  target is not current (host HWND not the Win32 focus, masked, disconnected, or out of memory) it returns
  `TS_E_INVALIDPOS`. `ITextStoreACP::GetStatus` documents only `S_OK` and `E_INVALIDARG`. Before `b5869cbc` it always
  returned `S_OK`.
- **Failure scenario**: a TIP calls `ITfContext::GetStatus` during a focus transition and receives a failure instead of
  "read-only"; a large document pays two full copies per query on top of the per-keystroke copies. A 64K-unit document is
  128 KB, so the copy is about 10-20 microseconds; the per-call cost is small, and what TSF does with an undocumented
  failure is not statically provable. The failing states are mostly ones in which no edit session follows. The other store
  methods (`QueryInsert`, `ReplaceTextRange`) already return the same failure under the same conditions.
- **Fix**: add a cheap `ReadStatus(bool& readOnly, bool& multiline)` to `TextStoreTarget`
  (`src/Controls/TextStoreTarget.h`) and implement it in both targets: the native target reads `IsReadOnly()` and
  `IsMultiline()` from the live control, the client store uses the cached `_snapshot` flags or one `Read` without copying
  text. When the target is unavailable, return `S_OK` with `TS_SD_READONLY` (or the last observed flags) instead of an
  error.
- **Regression test**: `Tests/Controls/DxUi.Tests.NativeTextInput.cpp`: move focus elsewhere in a headless fixture (the
  `hwnd == nullptr` shortcut still checks `GetFocusControl()`) and assert `GetStatus` returns `S_OK` with
  `TS_SD_READONLY`; today it returns `TS_E_INVALIDPOS`. With a counting `TextInputClient` in an embedded store fixture,
  assert `GetStatus` does not call `Read`; today it calls it once per query.
- **Verifier notes**: the copy and the failure return were confirmed at HEAD; the existing test
  (`Tests/Controls/DxUi.Tests.NativeTextInput.cpp:9848-9862`) covers only the success, read-only and multiline cases.

### CTRL-04 — Grid navigation keys are O(rows) in time and allocation

- **Severity** P3 (reviewer P2, verifier P3) · **Category** perf (pre-existing; duplicates prior R28-33, still open) ·
  **Verdict** Confirmed · **Confidence** high · **Effort** M · **Introduced** `ffc64c89` (2026-10-09), `f28f55d0`
  (2026-09-05), `8252aa27` (2026-10-21); not in the review window
- **Location**: `src/Controls/DxUi.Grid.cpp:5886` (`CollectVisibleRowIndices(rowCount, groups)`, defined at `:574-605`),
  `:3322` (`FindKeyboardStartRow`: `std::ranges::find(visibleRows, row)`), `:6084` (`std::ranges::find(visibleRows,
  currentRow)`), `:6087-6112` (the viewport paging added in the window, which searches the same full vector).
- **What**: every whitelisted key (Up, Down, Left, Right, Home, End, PageUp, PageDown, Space, Enter; the whitelist at
  `:5856-5869` exempts others) builds a vector of all visible row indices and does two linear searches over it. "Visible"
  means every model row that is not inside a collapsed group, not the rows in the viewport.
- **Failure scenario**: with a 1M-row ungrouped model, each Down allocates and fills 8 MB and scans it about twice: about
  3-4 ms in Release (a held key at 30 Hz is about 10% of a core), far more in MSVC Debug with checked iterators. The
  repository's own benchmark exercises 1M-row Grids (`Tests/Embedded/Embedded.Tests.GridSelectionBenchmark.h:837,853`).
- **Fix**: derive the neighbour, first and last visible rows and the visible ordinal directly from the ordered group
  descriptors (O(groups)), use `FindRowByStableId` and group-collapse checks instead of `find`, and give paging a virtual
  "visible ordinal to row" mapping over groups. Cheap interim step: when `groups` is empty, the ordinal equals the row
  index, so skip the vector (neighbour is row plus or minus 1) and resolve the focused row with `FindRowByStableId`.
- **Regression test**: a Grid unit test (or `Tests/Embedded/Embedded.Tests.GridSelectionBenchmark.h`) with a counting
  `IGridModel` of 1M ungrouped rows: press Down 100 times and assert allocations and model calls per key are O(1).
  Counting `FindRowByStableId` and `GetRowCount` calls is the simpler oracle if no allocation hook exists.
- **Verifier notes**: none of the cited lines is in the `a0f67fd3..6df17007` diff. It is already tracked as R28-33
  (unchecked) in `Specs/Plans/Done/CodeReview_2026-10-28.md` and as grid-selection-lifetime-19 in
  `Specs/Plans/Done/ProductionReview_2026-10-05/findings.md`.
- **Lead note**: this finding duplicates prior R28-33, which is still open; track it there and do not count it as new.
  Reconsider as P2 only if the product commits to held-key navigation of 1M-row Grids.

### CTRL-05 — Bitmap paste has no declared decline/fallback path

- **Severity** P3 (reviewer P2, verifier P3) · **Category** api / design · **Verdict** Confirmed (behaviour) ·
  **Confidence** medium · **Effort** S · **Introduced** `d85eadf5` (2026-11-01)
- **Location**: `src/Controls/TextInputServices.cpp:873-885` (bitmap branch returns `ApplyBitmapPaste`'s result
  directly), `src/Controls/TextClipboard.cpp:89-108` (`ReadSelectedBitmapContent` reads CF_DIBV5/CF_DIB before
  `CF_UNICODETEXT` and returns once one validates), `include/DxUi/TextInputServices.h:145-151`
  (`TextClipboardBitmapPaste`), `Specs/UI/UI_InputAndAccessibility.md:935-937`.
- **What**: when a client opts in (`AcceptsBitmap(snapshot)` returns true) and the clipboard carries a valid DIB, the
  text is never read, and a decline from `ApplyBitmapPaste` (`S_FALSE` or `E_NOTIMPL`) is returned as the paste result
  with no text fallback. The ordering itself (files, then DIB, then HTML and Unicode) is the written contract and the
  feature is opt-in. The gap is that `AcceptsBitmap` cannot see what else the clipboard offers and `ApplyBitmapPaste`
  has no decline semantics, unlike `TransformPaste` (`include/DxUi/TextInputServices.h:117`: `S_FALSE`/`E_NOTIMPL`
  keeps ordinary Unicode paste).
- **Failure scenario**: a user copies a cell range in Excel (it offers `CF_UNICODETEXT`, HTML and a DIB) and pastes into
  a RichTextEditor client that accepts bitmaps. The client gets an image of the cells, and if it declines there is no
  paste at all.
- **Fix**: give `ApplyBitmapPaste` the same decline contract as `TransformPaste`: read the text as well in the bitmap
  branch, and fall back to the text/HTML path on `S_FALSE` or `E_NOTIMPL`; optionally pass the presence of other formats
  in `TextClipboardContent` so `AcceptsBitmap`'s caller can decide. Changing the global priority contradicts the spec and
  needs the spec change first.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextInput.cpp`: private clipboard with `CF_UNICODETEXT` plus a
  valid `CF_DIB` and a client whose `ApplyBitmapPaste` returns `E_NOTIMPL`; assert the text is inserted. Today the paste
  returns the decline and inserts nothing. A second case where the client returns `S_OK` asserts the bitmap handler runs
  once.
- **Verifier notes**: confirmed in code; the reviewer's "bitmap before text" framing is the documented design, so only
  the missing decline path is a defect.

### CTRL-06 — Paint trusts unvalidated prepared inputs (Button shortcut width, TabControl header geometry)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed (shortcut), Plausible (tab) ·
  **Confidence** high (shortcut) / medium (tab) · **Effort** S · **Introduced** `e9ed8916` (shortcut, 2026-11-01),
  `f7665d64` (tab, 2026-11-02)
- **Location**: `src/Controls/DxUi.Controls.cpp:3805` (`Button::Paint`: `_preparedShortcutWidthDip + 10.0f`), `:3209-3239`
  (preparation), `:3572-3584` (`SetShortcutText` zeroes the width); `:9776-9783` (`TabControl::Paint`),
  `:760-775` (`DrawPreparedTopRoundedAttachedRect`), `:10075-10129` (`TabControl::PrepareVisualContent`).
- **What**: the hyperlink underline checks `_preparedHyperlinkValid`, host, revisions, flow and bounds
  (`:3835-3837`); the shortcut width is used with no check. `TabControl::Paint` never compares
  `geometry.sourceRect` with `GetTabRect(index)` or `geometry.owner` with the host.
- **Failure scenario**: if the shortcut measurement fails (`bad_alloc`, or `hostStillCurrent()` false), or `Paint` runs with
  no preceding preparation pass (a direct-paint test or capture path), the width stays 0 and the hint renders as a 10 DIP
  box that the text overflows; `_visualPreparationPending` is cleared before the call (`:2453`) so nothing retries. A
  tab header highlight can paint at the previous tab for one frame when a callback changes selection after the
  TabControl was visited in the same pass. The ordinary path is safe because a frame prepares before it paints
  (`src/Controls/DxUi.WindowHost.cpp:4642`, `src/Rendering/Embedded.cpp:552`).
- **Fix**: gate the shortcut width the same way as the hyperlink (`_preparedShortcutValid`, host, resource and
  typography revisions, flow), and otherwise skip the hint or fall back to a paint-time measurement. In
  `TabControl::Paint`, use the prepared geometry only if `sourceRect == GetTabRect(index)` and `owner == &host`;
  otherwise fall back to `FillRectangleWithColor`.
- **Regression test**: `Tests/Controls/DxUi.Tests.Controls.cpp`: call `SetShortcutText`, paint before preparation (or force
  the measurement to fail), and assert the hint rect width is at least the measured text width; today it is 10 DIP. For
  the tab, use the `_debugFailNextHeaderGeometry` hook plus a selection change after the visit.
- **Verifier notes**: the shortcut path needs a preparation failure or an unprepared paint; the tab path needs a
  same-pass callback mutation. Both narrow.

### CTRL-07 — `Slider::OnMouseDown` leaves `_dragging` set after a refused capture

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Plausible · **Confidence** low-medium ·
  **Effort** S · **Introduced** `21940805` (2026-11-01)
- **Location**: `src/Controls/DxUi.Controls.cpp:6973` (`_dragging = true`), `:6990-6992` (`host.CaptureMouse(this)` and the
  early return), `:7000-7008` (`OnMouseMove` updates the value whenever `_dragging`), `:7115-7129` (`OnCaptureLost`),
  `src/Controls/DxUi.WindowHost.cpp:2285-2308` (`ControlHost::CaptureMouse` refuses a control not in the host's tree).
- **What**: the guard `if (lifetime.expired() || GetHost() != originalHost || (originalHost && host.GetCapturedControl() !=
  this)) return true;` returns without resetting `_dragging`, `_touchDragging` or `_dragThumbPointerOffsetDip`.
- **Failure scenario**: the slider keeps `_dragging` without capture and later plain hover moves change its value and
  emit Preview notifications. Reproduction is narrow: `originalHost` is read after `FocusControlAndSurvive`, nothing but
  `CaptureMouse` runs between `:6989` and `:6991`, and `CaptureMouse` sets `_capturedControl` before `SetCapture`, so the
  guard is taken only when the focus callback re-parented the slider into a different host (`originalHost` is that
  host, `host.CaptureMouse` refuses, `GetCapturedControl() != this`). A callback that only detaches it (null host) passes
  the guard and proceeds to `UpdateValueFromPoint`.
- **Fix**: on that early return reset `_dragging`, `_touchDragging` and `_dragThumbPointerOffsetDip` and increment
  `_dragEpoch`, with no Cancel notification (no Preview was emitted, and `_value` has not changed).
- **Regression test**: `Tests/Controls/DxUi.Tests.Controls.cpp`: in the slider's `SetOnFocusChanged` callback, move the
  slider into a second `WindowHost`'s tree; then `OnMouseDown` on the first host, `OnMouseMove`, and assert the value is
  unchanged and no Preview callback ran. Today the value follows the pointer. Triggering by calling
  `host.CaptureMouse(other)` inside the focus callback does not reproduce it, because the slider's own `CaptureMouse`
  then wins.
- **Verifier notes**: the unrolled state was confirmed; reachability is only through cross-host re-parenting in a focus
  callback.

### CTRL-08 — Menu keyboard-ownership revision check is unreachable; duplicated stationary-pointer guard

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification / test-gap · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `b5869cbc` (revision, 2026-11-01), `5aff059e` (2026-11-01) and
  `c2c9c67c` (2026-11-02) (duplicate guard)
- **Location**: `src/Controls/DxUi.Menu.cpp:4473-4477` (`hoverOwnershipRevision != keyboardOwnershipRevision`),
  `:4324-4330` (`TakeKeyboardOwnership`), `:4416` and `:4431` (revision capture when arming a timer), `:4468`
  (`hoverTimerId == 0` early return), `:5055-5056` and `:5060-5061` (identical `any_of(keyboardIndex)` returns).
- **What**: `TakeKeyboardOwnership` increments the revision and cancels every popup's timer in the same step, and a new
  timer captures the new revision, so the revision check can only fire for a live timer on a popup that was not in
  `controller.popups` when ownership was taken, which cannot happen. The only incrementer is `TakeKeyboardOwnership`.
  The two stationary-pointer returns are identical; the first comes from `c2c9c67c`, the second from `5aff059e` (two
  commits implementing the same fix).
- **Failure scenario**: none; it is dead state that looks like a safety net. A real queued `WM_TIMER` (`KillTimer` does
  not purge posted ones) delivered after a new same-id timer is armed would be treated as that timer firing early, and
  the revision cannot catch it because the new timer captured the current revision; no code or test covers that.
- **Fix**: remove `hoverOwnershipRevision`, `keyboardOwnershipRevision` and the check at `:4473-4477`, and delete one of the
  two duplicate returns. If a defence against the same-id queued timer is wanted, use a per-arming generation carried in a
  timer identity that `KillTimer` makes stale, and test that instead.
- **Regression test**: none for the removal (behaviour is unchanged). If the generation defence is added, post a stale
  `WM_TIMER` after re-arming and assert the submenu does not open early.
- **Verifier notes**: `TestMenuKeyboardInputCancelsPendingSubmenuHoverTimers`
  (`Tests/Controls/DxUi.Tests.Menu.cpp:5689-5985`) sends `SendMessageW(root, WM_TIMER, 1u, 0)` only after steps that just
  cancelled the timer (it asserts `! rootState.hoverTimerActive` before each), so the handler returns at `:4468` and the
  test passes with `:4473-4477` deleted. It is still a valid oracle for the realistic stale-timer behaviour (zeroed id),
  not for the revision mechanism.

### CTRL-09 — `TextClipboardPasteTransform` receives raw clipboard text; the contract is undocumented

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api · **Verdict** Confirmed (contract gap) ·
  **Confidence** medium · **Effort** S · **Introduced** `b5869cbc` (2026-11-01)
- **Location**: `src/Controls/TextInputServices.cpp:900-932` (normalization, then `TransformPaste`, then only size and
  Unicode validation), `include/DxUi/TextInputServices.h:112-120`.
- **What**: `b5869cbc` removed `content.text.assign(replacement);` before the transform, so the transform sees the raw
  clipboard string and a successful result replaces the normalized text. This is deliberate: the spec
  (`Specs/UI/UI_InputAndAccessibility.md`, "the transform preserves the clipboard text's CRLF, CR and LF spelling;
  generic fallback retains normalization and single-line conversion") and an earlier audit
  (`Specs/Plans/Done/RichTextAudit_2026-10-21/findings.md:2119`) ask for it. The header is silent on it.
- **Failure scenario**: a single-line embedded client whose transform rewrites only file or HTML content and otherwise
  returns `content.text` passes CRLF or tabs into a single-line field.
- **Fix**: document in `include/DxUi/TextInputServices.h` that `TextClipboardContent::text` is the raw clipboard string
  and that a successful transform owns normalization; additionally apply the single-line conversion (`\n` and `\t` to
  space) to `transformed` when `! snapshot.state.multiline`, which is compatible with the spec because RichText is
  multiline.
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextInput.cpp` or `Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h`:
  a single-line client with a passthrough transform and the clipboard `a`, CR, LF, `b`, tab, `c`; assert the applied text
  contains no LF or tab. Decide the contract first: the spec as written would let a single-line transform return CRLF.
- **Verifier notes**: the removal and the silent header were confirmed; the behaviour is spec-conformant for RichText.

### CTRL-10 — Grid column-identity reset does not end in-flight header gestures

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Plausible · **Confidence** medium ·
  **Effort** S · **Introduced** `2c554db4` (2026-11-01)
- **Location**: `src/Controls/DxUi.Grid.cpp:1874-1896` (the identity reset in `NotifyDataChanged`), `:4968-4984`
  (`OnMouseMove` resize guard), `:5758-5826` (header release: reorder and sort).
- **What**: when column IDs change, widths, order and `_activeColumn` are cleared, but `_resizeColumn`,
  `_dragReorderColumn`, `_pressedHeaderColumn` and `_hoveredColumn` keep their old indices. The next move ends a resize
  only while `_columnWidths` is still empty (`:4980`); once a paint rebuilds the widths the gesture resumes on the new
  column with the old `_resizeInitialWidthDip`. On release, reorder is guarded by a bounds check of `draggedColumn`
  against `_columnDisplayIndexByModel.size()` and sort is not.
- **Failure scenario**: the user is resizing, reordering or pressing a header while a live refresh swaps the column set;
  the release resizes, reorders or sorts whichever column now has that index. Needs a column-set change during an active
  header gesture, so rare. The loop also copies two strings per column per notification (`GetColumn` returns
  `GridColumnDesc` by value), which is under a microsecond for about 10 columns.
- **Fix**: when `changed`, also end the header gestures (the `CancelCapturedControlInteraction` path) and reset
  `_hoveredColumn`, `_pressedHeaderColumn`, `_dragReorderColumn` and `_resizeColumn`. The ID comparison can stay as it is.
- **Regression test**: `Tests/Controls/DxUi.Tests.Grid.cpp`: start a header resize on column 1, `SetColumns` with new IDs,
  `NotifyDataChanged`, then move and release, once with and once without an intervening paint; assert no column width
  changed and the gesture ended.
- **Verifier notes**: reset behaviour and partial mitigation read at HEAD.

### CTRL-11 — TextField setters invalidate before normalizing caret, selection and layout caches

- **Severity** P3 (reviewer P3, verifier P3) · **Category** lifetime / bug · **Verdict** Plausible · **Confidence** low ·
  **Effort** S · **Introduced** ordering predates the window; the `lifetime.expired()` checks are from `21940805`
- **Location**: `src/Controls/DxUi.TextInput.cpp:963-1000` (`SetText`), `:1131-1152` (`SetMasked`), `:1343-1358`
  (`SetMultiline`), `src/Controls/DxUi.cpp:1116-1137` (`InvalidateMeasure` ends in `_host->Invalidate()`).
- **What**: `_text` is replaced and `InvalidateMeasure()` runs before `_caretIndex` is clamped and the anchor and layout
  caches are reset; an embedded host's invalidate callback can reach application code, which is why the lifetime checks
  exist.
- **Failure scenario**: an embedded host whose invalidate callback prepares, paints or reads UI Automation or TSF state
  synchronously sees `_caretIndex` larger than the text size, a stale anchor and layout caches built for the old text.
  Needs a synchronous re-entrant scheduler, which the embedded contract discourages.
- **Fix**: normalize all derived state first, then call `InvalidateMeasure()` and the other callbacks.
- **Regression test**: `Tests/Embedded/Embedded.Tests.EmbeddedTextInput.h`: from the view's invalidate hook call
  `ExportTextInputState` and the caret-rect APIs after `SetText(shorter)` and assert `caretIndex` is within the text.
- **Verifier notes**: ordering confirmed; impact is low.

### CTRL-12 — Nested TSF activation that also fails leaves the HWND associated with a popped document

- **Severity** P3 (reviewer P3, verifier P3) · **Category** ime / lifetime · **Verdict** Plausible · **Confidence** low ·
  **Effort** M · **Introduced** `1091ddf8` (2026-11-01)
- **Location**: `src/Controls/DxUi.NativeTextInput.cpp:858-884` (`failActivation`), `:787` (nested activation bumps the
  transition revision), `src/Controls/TsfFocus.h:29-49` (`RestoreThreadManagerFocusAssociation`).
- **What**: the predecessor splice covers only a nested session that succeeded (`:865-868`). If the nested activation
  fails after `AssociateFocus`, its own `failActivation` restores the association to its predecessor, which is the outer
  staged document. The outer `failActivation` then skips its own restore because `activationRevision` is stale (`:870`)
  and pops the document, so the HWND stays associated with a popped document and the application's original predecessor
  is lost.
- **Failure scenario**: a TIP or sink re-enters `SetFocusControl` during `SetFocus` or `Push` and the inner activation
  fails. Whether TSF clears an association when the document manager is released is not statically provable.
- **Fix**: in `failActivation`, when no session is active and the association is still the staged document, restore
  `previousFocusDocumentMgr` even if the revision is stale. Use the tracked value rather than querying with
  `AssociateFocus`, which replaces the association as a side effect.
- **Regression test**: extend `VerifyNativeTextInputTsfCallbacksPreserveReentrantSameControlSession`
  (`Tests/Controls/DxUi.Tests.NativeTextInput.cpp:972`): in `onSetFocus`, re-enter `SetFocusControl(requested)` and make
  the inner request non-current (for example move Win32 focus away in the inner `Push` callback); after both unwind,
  assert the association equals the sentinel (needs a thread-manager mock that records `AssociateFocus`).
- **Verifier notes**: the code reading is consistent with the claim; the TSF-side consequence is the unproven step.

### CTRL-13 — Heap work on the TextField paint path (pre-existing; reduced in the window)

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf (repository rule: paint does no heap work) ·
  **Verdict** Confirmed · **Confidence** high · **Effort** S · **Introduced** `f28f55d0` (2026-09-05); the window
  reduced it
- **Location**: `src/Controls/DxUi.TextInput.cpp:827` (`std::vector` of 64 `DWRITE_HIT_TEST_METRICS` per paint while a
  selection exists), `:87-115` (`BuildTextInputUnderlineRects` returns a fresh vector), `:810` (a new
  `IDWriteTextLayout` per paint in `DrawMultilineSelection`), `:1656-1701` (`GetDisplayText()` copy,
  `GetMultilineLineMetrics` vector, and two `BuildNativeCompositionUnderlineRects` calls per paint, each reading and
  copying the cached native text for the focused field).
- **What**: the window changed the metrics vector from a range-sized allocation to 64 entries (an improvement), but the
  allocation is still per paint, and the same path does larger per-paint work than the vector.
- **Failure scenario**: a focused multiline field with a selection or composition allocates a text-sized copy and a new
  layout on every frame.
- **Fix**: route selection and composition rectangles through the prepared multiline layout cache that
  `GetOrCreateMultilineLayout` already holds, and read the native composition ranges without copying the text. A reusable
  member buffer for the metrics is acceptable but is the smallest cost on this path.
- **Regression test**: a debug allocation counter around repeated `Paint` of a focused multiline field with a selection;
  expect zero allocations after the first frame (it will show more than the vector today).
- **Verifier notes**: not a window regression; listed because the touched lines are on the paint path.

### CTRL-14 — The split detach protocol changes behaviour for legacy containers and refused removals

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api / doc · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S · **Introduced** `9fd341a8`, `45edc269` (2026-11-01)
- **Location**: `include/DxUi/DxUi.h:1917-1928`, `src/Controls/DxUi.Controls.cpp:1298-1319`
  (`NotifyContainersOfDetaching`), `:1321-1394` (`TakeChild`), `:10923-10961` (`ScrollPanel::OnDescendantDetaching` and
  `OnDescendantDetached`), `:10895-10901` (`LiveInnerHovered`).
- **What**: (a) ScrollPanel hover cleanup moved from `OnDescendantDetaching` to the new `OnDescendantDetached`, and the
  one-argument `NotifyContainersOfDetaching(child)` maps to the Detaching phase. A derived Panel that removes children
  itself (through the protected `AccessChildren`) and calls only the legacy form never ends ScrollPanel inner hover, and
  the hovered control gets no `OnHoverChanged(false)` or `OnMouseLeave`; `LiveInnerHovered` later forgets it silently.
  (b) When `TakeChild` refuses (focus returned into the subtree), capture was already cancelled in the Detaching phase.
  The header comment (`include/DxUi/DxUi.h:1918-1920`) says the first phase may refuse extraction.
- **Failure scenario**: a consumer container with custom removal that relied on ScrollPanel's hover cleanup leaves a
  detached control in its hovered state.
- **Fix**: have the one-argument overload run both phases around the caller's removal, or document the two-phase protocol in
  `docs/controls.md` and `Specs/UI/UI_ControlsAndLayout.md`; the documentation route is cheaper and avoids calling
  `OnDescendantDetached` before removal. State in the spec that a refused removal still cancels capture.
- **Regression test**: extend `TestPanelLegacyDetachNotificationCall` (`Tests/Controls/DxUi.Tests.Controls.cpp:2213`) with
  a hovered row inside a ScrollPanel and a legacy-call container, and assert hover ends once.
- **Verifier notes**: the existing test only counts that `OnDescendantDetaching` ran once.

## Refuted

No finding was refuted. Sub-claims refuted inside surviving findings:

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| CTRL-02 (sub-claim) | The menu proxy-row cost is O(rows squared) on every wheel step | `src/Controls/DxUi.Menu.cpp:3602-3615`: `SetBounds` is called only on rows whose viewport-clamped rect changed, so only the 15-30 rows in the viewport are re-prepared per step (viewport rows times N, not N squared) |
| CTRL-13 (framing) | The per-paint metrics allocation is a window-introduced issue | `git diff a0f67fd3..6df17007` replaces range-sized vectors with 64 entries; the paint-path code dates from `f28f55d0` (2026-09-05) |
| CTRL-08 (attribution) | The duplicate guard comes from `b5869cbc` and `c2c9c67c` | `src/Controls/DxUi.Menu.cpp:5055-5061` blame: `c2c9c67c` and `5aff059e`; `b5869cbc` introduced only the revision |
| CTRL-07 (test) | Calling `host.CaptureMouse(other)` in the focus callback reproduces the leak | `src/Controls/DxUi.Controls.cpp:6967-6990`: the callback runs before `_dragging` is set and the slider's own `CaptureMouse(this)` then takes capture |

## Area architecture and simplification proposals

1. **Attachment check without a root DFS.** Add an `IsAttachedTo(const ControlHost&)` helper that walks `_parent` to the
   root and compares it with `host.GetRoot()`, for the per-control hot paths (`Label`/`Button::PrepareVisualContent`,
   `NotifyTextInputLayoutChanged`, `IsCurrentTextServiceTarget`, `ActivateNativeTextInputSession`). Rationale: removes the
   O(N) walk (CTRL-02). Files: `src/Controls/DxUi.WindowHost.cpp`, `DxUi.Controls.cpp`, `DxUi.TextStoreACP.cpp`,
   `DxUi.NativeTextInput.cpp`. Risk: medium, not low. `IsControlInTree` is a logical-child walk with 69 call sites and
   logical membership differs from `_parent` membership: a `PageHost` outgoing page has a `_parent` but is not a logical
   child (`PageHost::GetLogicalChild`, `src/Controls/DxUi.Controls.cpp:2059-2072`). Introduce the helper separately and
   migrate call sites one by one, starting with the hot ones. Effort S-M. After production, except the CTRL-02 reorder
   (before).
2. **Stop copying whole documents per keystroke.** One keystroke on a native TextField or RichTextEditor does
   `ExportTextInputState`, `previousState = _nativeTextInputStateCache` (`src/Controls/DxUi.NativeTextInput.cpp:1068-1074`),
   `NotifyExternalChangesIfNeeded`, `notificationState = currentState` and up to three `continuationIsCurrent()` reads,
   plus `GetStatus` (CTRL-03). Replace text equality checks with a monotonically increasing text revision exported with
   the state and copy only on actual change. Files: `DxUi.NativeTextInput.cpp`, `DxUi.TextStoreACP.cpp`. Risk: medium (the
   revision must cover every mutator). Effort M. After production.
3. **Grid keyboard over group descriptors** (CTRL-04). A visible-ordinal to row mapping in O(groups) removes the per-key full
   vector and would also serve `OnContextMenu(keyboard)` (`src/Controls/DxUi.Grid.cpp:6184`). Risk: medium. Effort M. The
   ungrouped interim step before production; the redesign after.
4. **Bulk selection setters.** `Tree::SetSelectedItemIds` (`src/Controls/DxUi.Tree.cpp:931`) and `Grid::SetSelectedRowIds`
   (`src/Controls/DxUi.Grid.cpp:6941`) build an `unordered_set` of all visible ids only to find `last`. Compute `last` from
   the requested set instead. `AssignInOrder(visibleIds, ...)` still iterates the visible ids, so only the hash set (tens
   of MB at 1M rows) is saved. Risk: low. Effort S. After production.
5. **A single prepared-value validity helper.** Label, Button (icon, hyperlink, shortcut) and TabControl repeat a
   seven-field key (host, revision, resources, typography, flow, width, height). A small `PreparedKey` with
   `Matches(host, ...)` would have made the CTRL-06 omission impossible. Risk: low. Effort S. After production.
6. **Menu ownership.** Remove the redundant revision and the duplicated guard (CTRL-08). Risk: low. Effort S. Before
   production.

## Test-quality observations

- `TestTextFieldBuffersSurrogatesBeforePublishing` (`Tests/Controls/DxUi.Tests.TextField.cpp:2316-2338`) calls `OnChar(lead)`
  and `OnChar(trail)` back to back and never models the `WM_KEYDOWN(VK_PACKET)` that real injected input produces between
  them; its only intervening key-down is `VK_HOME`, so it locks in the unconditional clear (CTRL-01). No RichTextEditor or
  embedded surrogate-pair test exists.
- `TestMenuKeyboardInputCancelsPendingSubmenuHoverTimers` (`Tests/Controls/DxUi.Tests.Menu.cpp:5689`) sends
  `SendMessageW(root, WM_TIMER, 1u, 0)` after the timer was just cancelled, so the handler returns at `hoverTimerId == 0`
  and the test passes with the ownership-revision code deleted. It does not exercise a queued `WM_TIMER` racing a newly
  armed same-id timer (CTRL-08).
- The `GetStatus` assertions inside `TestRichTextNativeTsfCompositionAndDisconnection`
  (`Tests/Controls/DxUi.Tests.NativeTextInput.cpp:9848-9862`) cover the read-only and multiline flags and
  `OnStatusChange`, not the failure path (unfocused HWND, masked, disconnected) or its cost (CTRL-03).
- `TestPanelLegacyDetachNotificationCall` (`Tests/Controls/DxUi.Tests.Controls.cpp:2213`) only counts that
  `OnDescendantDetaching` ran once; it does not assert the ScrollPanel hover cleanup a legacy caller relied on (CTRL-14).
- Slider forwarded capture: `TestSliderReleaseThroughHostPreservesReplacementCapture`
  (`Tests/Controls/Controls.Tests.HostControlRegressions.cpp`) routes through `HandleMessage` and is good;
  `TestSliderReleasePreservesCaptureTransferredByPreview` and `TestSliderOldReleaseCannotFinishRestartedDrag` call
  `OnMouseDown` and `OnMouseUp` directly and miss host routing. No test covers the capture-refused `OnMouseDown` path
  (CTRL-07).
- Good oracles: `GridSelectionModel::AssignInOrder` (independent expected order and duplicates),
  `TestIntrinsicLayoutAllocationRetryIsBounded` (counts attempts across new requests and idle frames) and
  `TestTreeLargeExpansionSkipsAnimation` (debug capture counter).
- `Tests/Embedded/Embedded.Tests.GridSelectionBenchmark.h`: the window changes only reorder output and statistics
  evaluation. Nothing benchmarks keyboard navigation or `SetSelectedRowIds` on large models (CTRL-04).

## Questions for the developer

1. Is bitmap-over-text priority a deliberate product decision for RedTilde (CTRL-05)? Office and Excel copies always
   include a DIB. Recommended answer: keep the documented order, but give `ApplyBitmapPaste` a decline contract with a text
   fallback.
2. Do any consumers (RedXe, RedSalamander) paint a `Label` that is not reachable through `GetLogicalChild` (a composite
   control's member that it paints itself)? Since `f6c6fcdb` such a Label loses its mnemonic underline and typography
   override, because `Label::Paint` falls back to `DrawCenteredText` with no prepared layout. Recommended answer: no, none
   expected; first-party composites add their Labels with `AddChild`.
3. The intrinsic-layout OOM retry (`6d074e7b`) retries on the next turn, which costs one skipped frame
   (`PreparePendingPageLayout` returns false and the Win32 paint aborts, `src/Controls/DxUi.WindowHost.cpp:4642-4649`),
   and then keeps stale geometry until a new request. Was a longer back-off, or a retry on the next real invalidation,
   considered? Recommended answer: keep the one-turn retry, since it is bounded and tested.
4. Tree: with no focused row, the first Down focuses row 0 silently and then moves to row 1, so row 0 is skipped
   (`src/Controls/DxUi.Tree.cpp:2246-2280`). This was the same before. Is it intended, given the comment "The first key
   starts from the first row"? Recommended answer: select row 0 on the first Down, as a Win32 TreeView does, unless the
   skip is a documented choice.
5. `NotifyDataChanged` no longer reveals the focused row (`9e14ae92`). Should `docs/controls.md` (Tree row) say so? The new
   4096-row expansion-animation cap, wheel bubbling at the extents and Grid viewport-based PageUp/PageDown are also
   undocumented there. Recommended answer: yes; the repository requires a documentation review for every code change.
6. Are `GridSelectionModel::AssignInOrder`, `ControlHost::NotifyTextInputLayoutChanged`/`GetNativeTextCompositionActive`,
   and the new clipboard extension interfaces (`TextClipboardHtmlPaste`, `SelectiveStructuredTextClipboard`,
   `BitmapStructuredTextClipboard`, `HtmlTextClipboardWriter`) supported public API? None appears in `docs/` or the
   contracts beyond rich-text mentions. Recommended answer: document the clipboard interfaces as supported; keep the other
   two internal.

(No question rests on a false premise; the verifier checked each premise at HEAD.)

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.Controls.cpp` | diff + touched functions (all 2,144 diff lines; `Panel::NotifyContainersOfDetaching`/`TakeChild`, `ControlHost::PreparePendingPageLayout`, Label preparation/paint/setters, `Button::PrepareVisualContent`/`Paint`/`OnBoundsChanged`/`SetText`/`OnMouseUp`/`OnCaptureLost`, Slider mouse/escape/capture, TabControl layout cache/preparation/paint/`RemoveTabByIdentity`/`SelectTab`, ScrollPanel detach/`DispatchPress`/`OnMouseUp`, ProgressBar setters) |
| `src/Controls/DxUi.Tree.cpp` | diff + touched functions (`SetSelected*`, `ToggleExpanded`, `CaptureVisibleItems`, `BeginTreeExpansionAnimation`, `Paint` animation, `OnMouse*`, `OnKeyDown`, `HitTestPoint`/`ComputeItemLayoutMetrics`, `OnBoundsChanged`, tooltip cache, typeahead) |
| `src/Controls/DxUi.Grid.cpp` | diff + touched functions (`AssignInOrder`, `SetModel`, `SetSelectionMode`, busy/sort setters, `NotifyDataChanged`, `OnKeyDown`, `EnsureColumnWidths`, `ApplyColumnLayout`, `SetSelectedRowIds`, `Tick` busy path, header gesture release) |
| `src/Controls/DxUi.Menu.cpp` | diff + touched functions (`TakeKeyboardOwnership`, timers, `HandleSubmenuHoverTimer`, `RouteMenuPointerHover`, `RouteMenuKeyboardEvent`, `CreateMenuPopupWindow` post-show, `SynchronizeMenuAccessibility`, `PopulateMenuAccessibility`, popup window procedure accessibility messages, async controller registry) |
| `src/Controls/DxUi.TextInput.cpp` | diff + touched functions (`SetText` and setters, `OnChar`/`OnKeyDown` surrogate path, `ResolveCursorKind`, range-rect metrics, `ImportTextInputState`) |
| `src/Controls/DxUi.NativeTextInput.cpp` | diff + touched functions (`ActivateNativeTextInputSession`, `SecureClear...`, `DeactivateNativeTextInputSession`, `ActivateNativeTextInputTsf` including the `failActivation` splice, `DeactivateNativeTextInputTsf`, `SyncNativeTextInputSession`, `NotifyTextInputLayoutChanged`) |
| `src/Controls/DxUi.TextStoreACP.cpp` | diff + touched functions (`TryResolveTextStoreRangeRect`, `GetRangeScreenRect`, `GetStatus`, `QueryInsert`, `InsertTextAtSelection`/`ReplaceTextRange`, `ReadState`/`ReadBackingState`, `IsCurrentTextServiceTarget`, `NotifyExternalChangesIfNeeded`) |
| `src/Controls/TextStoreTarget.h` | full (unchanged in the window) |
| `src/Controls/TextInputServices.cpp` | diff + touched functions (`Clipboard` fully, `SetClient`, `NotifyChanged`, client store `ReadState`/`ApplyState`/`BeginEdit`) |
| `include/DxUi/TextInputServices.h` | full (diff) |
| `src/Controls/DxUi.ComboBox.cpp` | diff + touched functions (host-transfer guards, `CommitSelection`, `RequestPopup`, `OnFocusChanged`, range rects) |
| `src/Controls/DxUi.EditorControls.cpp` | diff + touched functions (Splitter and ColorPicker Escape capture, NumericStepper setters) |
| `src/Controls/DxUi.SingleLineTextEditing.cpp` | diff + touched functions (ASCII grapheme shortcuts checked against `FindNext/PreviousTextElementBoundary` in `DxUi.TextElements.cpp`) |
| `src/Controls/DxUi.Layout.cpp` | diff + touched function (`Label::MeasureOverride` typography path) |
| `src/Controls/DxUi.Theme.cpp` | diff (include path only) |
| `src/Controls/DxUi.Forms.cpp` | skipped (no change in the window) |
| `src/Controls/DxUi.Accessibility.cpp` (radio group only) | diff of the radio hunks in `1f661c7e`, `05d9a361`, `c7144a1a`; the rest belongs to the accessibility reviewer |
| `src/Controls/TextClipboard.cpp` | targeted read (bitmap/HTML/text selection order) for CTRL-05; not reviewed fully |
| `src/Controls/DxUi.RichTextEditor.cpp` | targeted read by the verifier (`OnKeyDown` and `OnChar` surrogate handling) for CTRL-01; otherwise outside this area |
| `src/Controls/DxUi.WindowHost.cpp`, `src/Rendering/Embedded.cpp` | targeted read by the verifier (keyboard routing, capture, preparation pass) for CTRL-01, CTRL-02, CTRL-07 |
| Tests: `DxUi.Tests.Menu.cpp` | new keyboard-ownership test read fully; other Menu diff skimmed for test names |
| Tests: `DxUi.Tests.Tree.cpp`, `DxUi.Tests.Grid.cpp`, `DxUi.Tests.NativeTextInput.cpp`, `DxUi.Tests.TextField.cpp`, `DxUi.Tests.Controls.cpp` (slider, panel, scroll tests), `Controls.Tests.IntrinsicLayout.cpp` (retry), `Controls.Tests.HostControlRegressions.cpp` (slider), `Embedded.Tests.EmbeddedTextInput.h`, `Embedded.Tests.GridSelectionBenchmark.h` | new and changed tests read |
| Tests: `DxUi.Tests.ComboBox.cpp`, `DxUi.Tests.EditorControls.cpp`, `DxUi.Tests.NewControls.cpp`, `Controls.Tests.Forms.cpp` | diff skimmed (test names and assertions only) |
