# Rich-text commands, clipboard, export and file drop: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the structural Markdown command planner
(`src/Controls/DxUi.RichTextCommands.cpp`), clipboard transport and CF_HTML/CF_HDROP/DIB decoding
(`src/Controls/TextClipboard.cpp`), the OLE file/text drop target, export to HTML, and the editor paste and drop entry
points that feed them.
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity.

Result: no P0 or P1. The untrusted-data boundary (CF_HTML offsets, DROPFILES, DIB, HGLOBAL ownership, export escaping)
holds; the defects are in the structural command planner and in two hardening gaps. The former CMD-10 is spec-mandated
behavior and moved to the questions.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| CMD-01 | P2 | Confirmed | Task toggle and SetList(Task) edit the wrong bytes when item content starts on the line after its marker | `src/Controls/DxUi.RichTextCommands.cpp:1029` | M |
| CMD-02 | P2 | Confirmed | MoveLineUp/Down moves a parent list marker along with a first nested item that shares its line | `src/Controls/DxUi.RichTextCommands.cpp:1897` | S |
| CMD-03 | P3 | Confirmed | Paragraph MoveLine can re-interpret a moved line as block structure | `src/Controls/DxUi.RichTextCommands.cpp:1910` | M |
| CMD-04 | P3 | Partly refuted (dead code confirmed) | Dead blank-line check in `CanInsertMath` | `src/Controls/DxUi.RichTextCommands.cpp:380` | S |
| CMD-05 | P3 | Confirmed | InsertFootnote accepts a label ending in a lone backslash | `src/Controls/DxUi.RichTextCommands.cpp:2825` | S |
| CMD-06 | P3 | Confirmed (unmeasured) | Every Rich-mode Backspace/Delete normalizes every leaf in the document | `src/Controls/DxUi.RichTextCommands.cpp:4026` | S |
| CMD-07 | P3 | Confirmed | Bidi and format controls in file paths reach visible link labels | `src/Controls/DxUi.RichImagePaths.h:41` | S |
| CMD-08 | P3 | Confirmed (code span); plausible (HTML, YAML) | Raw-context file paste inserts unescaped path text that breaks the enclosing token | `src/Controls/DxUi.RichTextEditor.cpp:1135` | S |
| CMD-09 | P3 | Confirmed | CF_UNICODETEXT is read and copied with no size cap | `src/Controls/TextClipboard.cpp:359` | S |

## P2 findings

### CMD-01 — Task toggle and SetList(Task) edit the wrong bytes when item content starts on the line after its marker

- **Severity** P2 (reviewer P2, verifier P2; borderline P3 because the input is rare and the edit is undoable) ·
  **Category** bug · **Verdict** Confirmed · **Confidence** high · **Effort** M · **Introduced** `c1a77e42f`
  (2026-10-21, before the window; `1ac5a873` in the window touched `Task()` without fixing it)
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:1029-1035` (`Prefix` fallback), `:2559-2565` (multi-item
  `Task`), `:2477-2478` (`SetList` task insert), `:1064-1074` (`LabelStart`), `:2479-2480` and `:2662-2668`
  (`LabelStart` consumers in `SetList` and `Join`)
- **What**: `Prefix()` takes `content` from the parser only when the first child paragraph starts on the marker line
  (`:1026-1028`). Otherwise it computes
  `prefix.content = end - at <= 4u && end < line.bodyEnd ? end : at + (at < end ? 1u : 0u)`.
  For an item whose marker line is empty (`-` then the text on the next line), `end == at == line.bodyEnd`, so `content`
  is the end-of-line position of the marker line. The parser still treats the item as a task: `ListMarker` gives an empty
  marker `indent = 2` (`src/Controls/DxUi.MarkdownBlocks.h:246`), so the next line is matched as item content (`:1120`),
  and the inline parser flags a GFM task from the first paragraph's text whatever source line it sits on
  (`src/Controls/DxUi.MarkdownInlines.h:621-731`). Two call sites then edit at `content` without looking at the text:
  - multi-item `Task()` does `Add(prefix.content + 1u, prefix.content + 2u, checked ? L" " : L"x")` and never checks
    `text[prefix.content] == L'['` (the single-item path checks at `:2581`);
  - `SetList(Task)` does `Add(prefix.content, prefix.content, L"[ ] ")`.
  `LabelStart()` adds 3 to the same position, so converting a next-line task item to a non-task kind
  (`Add(prefix.content, LabelStart(item, prefix), L"")` at `:2480`) deletes the line break and indentation instead of
  `[ ] `, and probably collides with the `ShiftFollowing` edits ("Overlapping structural command edits").
- **Failure scenario**:
  1. Source `- [ ] a\n-\n  [ ] b\n`, selection `{0,18}`, ToggleTask. `SelectedItems()` returns both items, both are
     tasks, so `Add(10,11,"x")` replaces the first indentation space of line 3. Result `- [x] a\n-\nx [ ] b\n`; the
     second item becomes empty and `x [ ] b` leaves the list. With CRLF line ends `Add(11,12,...)` splits CR|LF, throws
     `out_of_range`, and the keyboard path swallows it (`src/Controls/DxUi.RichTextEditor.cpp:6312-6319`); a direct API
     caller sees the exception.
  2. Source `-\n  text\n`, caret `{4,4}`, Ctrl+Enter. ExitCodeBlock is Unavailable (`DxUi.RichTextEditor.cpp:6116-6124`),
     ToggleTask falls through to `SetList(Task)`, `Add(1,1,"[ ] ")` yields `-[ ] \n  text`. `-[ ]` is not a list
     marker, so the whole item becomes a paragraph.
  Nothing validates the result: `Finish()` (`:1806-1845`) and `ApplySourceEdits`
  (`src/Controls/DxUi.RichSemantic.cpp:275`) never reparse.
- **Fix**:
  - Extend `ItemPrefix` (`:852`) with `bool contentOnMarkerLine` (true when the `if` branch at `:1026-1028` is taken) and
    `std::size_t firstParagraphContent` (`nodes[first].lines.front().source.first` when the first child is a Paragraph,
    otherwise `MarkdownNoNode`). Do NOT change the meaning of `prefix.content`: `SetList` kind changes (`:2465`),
    `ShiftItem` and the other callers rely on it being on the marker line.
  - `Task()` multi-item path (`:2559-2565`): locate the checkbox at `firstParagraphContent`, apply the same
    `text[x] == L'['` and `text[x + 2u] == L']'` check as `:2581`, and reject the whole command (clear edits, return)
    if it fails.
  - `SetList(Task)` (`:2477-2478`): insert `[ ] ` at `firstParagraphContent` instead of `prefix.content`. For
    `-\n  text` this gives `-\n  [ ] text`. Rejecting when `!contentOnMarkerLine` is an acceptable minimal alternative.
  - `LabelStart()` (`:1064`) and its consumers (`:2480`, `:2667`, `TaskLabelCanUnwrap` `:1075`): start from
    `firstParagraphContent` for task items so the deleted range is exactly `[ ] `, or reject when
    `!contentOnMarkerLine`.
  - Single-item `Task()` already refuses a next-line task (`:2581`); after the fix it may toggle it instead, which is
    optional.
- **Regression test**: `Tests/Support/Support.Tests.RichSemantic.h`, `InlineTasksLinks` (task rows at `:737-741`). Each
  of these fails at `6df17007` and must hold after the fix; repeat each with CRLF line ends:
  - `Exact(L"- [ ] a\n-\n  [ ] b\n", {0u, 18u}, ToggleTask, L"- [x] a\n-\n  [x] b\n")` (today `x [ ] b`);
  - `Exact(L"-\n  text\n", {4u, 4u}, ToggleTask, L"-\n  [ ] text\n")`, or `Unavailable` if the minimal fix is chosen
    (today `-[ ] \n  text`);
  - `SetList(Unordered)` over `-\n  [ ] b\n` with the caret in `b` expects `-\n  b\n` or `Unavailable`, never an
    exception or overlapping edits (covers `LabelStart`).
- **Verifier notes**: both scenarios traced to the byte, including the trailing `{18,18,18}` line entry and `end=17` in
  `SelectedItems()`. The fallback only produces a wrong `content` for a paragraph when the marker line is otherwise
  empty; non-paragraph first children are rejected earlier by the Task paths. `LabelStart` and `Join` use of the same
  position was added by the verifier.
- **Lead note**: effort raised from S to M because `LabelStart` and its three consumers must change with the flag.

### CMD-02 — MoveLineUp/Down moves a parent list marker along with a first nested item that shares its line

- **Severity** P2 (reviewer P2, verifier P2; low-frequency input, undoable) · **Category** bug ·
  **Verdict** Confirmed · **Confidence** high · **Effort** S · **Introduced** `96260272`
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:1897-1951` (`MoveLine`, items branch `:1901-1909`);
  `src/Controls/DxUi.MarkdownBlocks.h:556`
- **What**: the items branch swaps `nodes[item].block.source` with the sibling's source. Container nodes start at the
  physical line start (`MarkdownBlocks.h:556`, `node.block.source = {lineFirst_, lineEnd_}`), so the first item of a
  list that opens on its parent's marker line (`- - a`) owns the parent's `- ` prefix bytes, and the swap moves them.
  The only guard is `first.last != second.first`, which passes. `RestrictToNote` protects footnotes; lists inside lists
  are unprotected. The editor calls the command with no pre-filter (`src/Controls/DxUi.RichTextEditor.cpp:5705-5712`).
- **Failure scenario**: source `- - a\n  - b\n`, caret `{10,10}` (on `b`), Alt+Up. `NodeAt(ListItem, 10)` is inner `b`
  (`{6,12}`), `PreviousSibling` is inner `a` (`{0,6}`), the value built is `"  - b\n- - a\n"`, reported Changed. One
  outer item with nested `[a, b]` becomes two outer items: `b`, then an item holding a nested list with `a`.
  `1. - a\n   - b` behaves the same, and Alt+Down with the caret on `a` does too. Undo restores the source.
- **Fix**: in the items branch, reject when an ancestor `ListItem` or `FootnoteDefinition` of EITHER item starts on that
  item's own physical line: `Line(nodes[ancestor].block.source.first).first == Line(nodes[x].block.source.first).first`
  for `x` in `{item, adjacent}`. Both items must be tested: the moving `b` has no such ancestor on its own line; only
  the adjacent `a` does. Do NOT include `BlockQuote` ancestors in this rule: `> - a\n> - b` repeats the quote prefix on
  every line and swaps correctly today, and the rule as a plain "ancestor starts on the same line" test would refuse it.
  (A quote that opens on the item's line but whose prefix is not repeated on later lines would need the lazy-line check
  from CMD-03.) Swapping from each item's own marker is not sufficient on its own, because the shared container
  prefix would then need rewriting; rejecting matches the "mixed ownership refuses" contract
  (`Specs/UI/UI_ControlsAndLayout.md:1925-1929`).
- **Regression test**: `Tests/Support/Support.Tests.RichSemantic.h`, `MoveLinesAndItems` (`:970`):
  - `Unavailable(L"- - a\n  - b\n", {10u, 10u}, MoveLineUp)`; today it rewrites the list;
  - `Unavailable(L"- - a\n  - b\n", {4u, 4u}, MoveLineDown)` and the `1. - a\n   - b` pair;
  - keep a positive row `Exact(L"> - a\n> - b\n", ..., L"> - b\n> - a\n")` so the fix cannot over-reject quotes.
- **Verifier notes**: traced through `NodeAt`, `PreviousSibling` and the swap. The reviewer's "of either item" wording is
  necessary; the BlockQuote clause in the reviewer's fix was only correct with its parenthetical qualifier.

## P3 findings

### CMD-03 — Paragraph MoveLine can re-interpret a moved line as block structure

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** M · **Introduced** `96260272`
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:1910-1924`
- **What**: the paragraph branch swaps two physical lines whenever both belong to the same Paragraph node and neither is
  a list item. Some continuation lines are paragraph text only because CommonMark forbids them from interrupting a
  paragraph: an ordered marker that does not start at 1 (`DxUi.MarkdownBlocks.h:1287`), a line indented four or more
  columns, a type-7 HTML start, a lazy quote continuation (`:1147-1151`). Moved to the first line they open a block.
  The reverse also holds: a setext underline of `=` or `-` moved below another line becomes a heading, and a first
  line that is a link reference definition becomes plain text when moved down.
- **Failure scenario**: `a\n2. b`, caret `{4,4}`, Alt+Up gives `2. b\na` (an ordered list starting at 2). `a\n    b`
  gives an indented code block. `> a\nb` (lazy), Alt+Down on `> a`, gives `b\n> a` (the quote/paragraph relationship
  flips). `===\nb`, Alt+Down on line 1, gives `b\n===` (an H1). `[x]: /u\nb`, Alt+Down on line 1, turns the definition
  into text.
- **Fix**: the planner never reparses, so add a lexical rejection in the paragraph branch after the two lines and the
  new order are known:
  - reject if either line fails `OuterOffset(paragraph, line, at)` or the container prefixes `Slice(line.first, at)`
    of the two lines differ (lazy lines);
  - reject if the line that becomes the first line, after its container prefix, has four or more columns of
    indentation or (after up to three spaces) starts an ordered or bullet marker, `>`, `#`, `<`, `|`, a fence
    (three backticks, `~~~`, `$$`, `\[`) or a thematic break;
  - reject if the line that becomes a continuation line consists only of `=` or only of `-` characters (setext
    underline, trailing spaces allowed);
  - reject if the old first line starts with `[` and contains `]:` (link reference definition).
- **Regression test**: `MoveLinesAndItems`: `Unavailable(L"a\n2. b\n", {4u, 4u}, MoveLineUp)`,
  `Unavailable(L"a\n    b\n", ..., MoveLineUp)`, `Unavailable(L"> a\nb\n", {0u, 0u}, MoveLineDown)`,
  `Unavailable(L"===\nb\n", {0u, 0u}, MoveLineDown)`, `Unavailable(L"[x]: /u\nb\n", {0u, 0u}, MoveLineDown)`; keep
  `Exact(L"alpha\nbeta", {1u, 4u}, MoveLineDown, L"beta\nalpha")` as the positive row.
- **Verifier notes**: the parser interrupt rules were read directly; the rule is not run. The setext and link-reference
  hazards were added by the verifier.

### CMD-04 — Dead blank-line check in `CanInsertMath`

- **Severity** P3, cleanup only (reviewer P3 bug, verifier P3) · **Category** simplification · **Verdict** Partly
  refuted: the dead code is confirmed, the defect claim is refuted · **Confidence** high · **Effort** S ·
  **Introduced** `e1926271` (2026-11-01; the loop itself came from `59173c29`)
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:352-379` (display branch ends `return true;`) and `:380-398`
  (unreachable)
- **What**: the display branch returns at `:378`, so at `:380` `display` is always false and `! display` returns false
  for any multi-line payload. The loop at `:382-397` ("A blank physical line would split the Markdown leaf") can never
  run. It was written for the old inline `\[x\ny\]` form (`59173c29`); `e1926271` ("Insert display math as whole
  source lines") added the early `return true` and left the loop behind. The reviewer attributed the regression to
  `38b7b9e7`, but the parent of `38b7b9e7` already contained the early return.
- **Behavior is correct, not a bug**: `Math()` (`:2281-2315`) now emits `\[`, an end of line, the payload, an end of
  line and `\]`. A lone `\[` opens a `FencedCode` node with `fenceMarker '['`
  (`src/Controls/DxUi.MarkdownBlocks.h:1367-1379`), and the fence branch consumes blank lines as content
  (`:1167-1195`) before the `if (empty)` branch at `:1243`. Payload `a\r\n\r\nb` therefore yields one display-math
  block with a blank line inside, not literal text. The test added in `e1926271` asserts that shape
  (`Support.Tests.RichSemantic.h:420-423`), and the spec says a top-level supplied expression "may contain multiple
  physical lines" with no blank-line rule (`Specs/UI/UI_InputAndAccessibility.md:1426-1428`).
- **Fix**: delete `:380-398` and replace the tail with `return payload.find_first_of(L"\r\n") == payload.npos;`
  (inline math only; behavior-identical). Do NOT move the blank-line loop into the display branch and do NOT reject
  payload lines that would interrupt a paragraph (`- x`, `# x`, fences): inside the `[` fence they are content, and
  rejecting them would refuse ordinary LaTeX continuation lines.
- **Regression test**: a characterization test, not a failing one (it passes before and after): in
  `Support.Tests.RichSemantic.h` next to `:410-423`, `Exact(L"", {}, request, L"\\[\r\na\n\nb\r\n\\]", Selection{4u, 8u},
  profile)` with `mathDisplay = true`, `text = L"a\n\nb"`, plus a `Parse` of the result asserting exactly one
  `FencedCode` node with `fenceMarker == L'['`. It pins the behavior the deleted check appeared to forbid.
- **Verifier notes**: attribution corrected via `git show 38b7b9e7^`, `e1926271` and `59173c29`; both are in the window.

### CMD-05 — InsertFootnote accepts a label ending in a lone backslash

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `e2d15fae`
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:2825`, `:2870-2872`; `src/Controls/DxUi.MarkdownLabels.h:11-36`
- **What**: `ValidFootnoteLabel(L"a\\")` returns true, because `escaped` is only set when a following character exists.
  `Footnote()` serializes `[^a\]` and `[^a\]: `. The block scanner (`src/Controls/DxUi.MarkdownBlocks.h:160-169`) and the
  inline scanner treat `\]` as escaped, so neither a reference nor a definition is produced.
- **Failure scenario**: an application label dialog passes `note\`. The command reports Changed and inserts literal
  `[^note\]` text plus a stray `[^note\]: ` paragraph. An odd trailing run (`note\\\`) behaves the same.
- **Fix**: make `ValidFootnoteLabel` return false when the last unit is a backslash that was not itself consumed as an
  escape (that is, `escaped == false` while processing the final unit). This is safe for the parser: both callers
  (`MarkdownBlocks.h:177`, `src/Controls/DxUi.MarkdownInlines.h:831`) pass labels bounded by an unescaped `]`, which
  ends in an even run of backslashes. `Footnote()` then throws `invalid_argument` through its existing check at `:2825`.
- **Regression test**: `InlineTasksLinks` footnote rows (`Support.Tests.RichSemantic.h:785-809`):
  `Reject<std::invalid_argument>` for `request.text = L"note\\"` and for `L"note\\\\\\"` (three backslashes); a row for
  `L"note\\\\"` (two backslashes, even run) stays accepted. The first two fail today.
- **Verifier notes**: both parser callers checked; the label cannot reach them with an odd trailing run.

### CMD-06 — Every Rich-mode Backspace/Delete normalizes every leaf in the document

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed (cost profile from code,
  magnitude unmeasured) · **Confidence** medium · **Effort** S · **Introduced** `c1a77e42f`; code-span work added by
  `3d365075` and `e149f04f`
- **Location**: `src/Controls/DxUi.RichTextCommands.cpp:4026-4057` (`Delete`); callers
  `src/Controls/DxUi.RichTextEditor.cpp:5129`, `:5207`
- **What**: `BuildMarkdownDeletion` runs for every Rich-mode Backspace/Delete. For every TextLeaf and HtmlBlock in the
  document the loop runs `NormalizeSemanticLeaf` (`src/Controls/DxUi.RichSemanticValues.h:28`, per-unit `push_back` of a
  24-byte `SemanticValueUnit` plus one `wchar_t`, with reallocation) and then `NativeHtmlPlan::Inline/Block`. Only the
  code-span subset is limited to intersected ranges (`:4018-4021`). The planner constructor also rebuilds the full
  `lines` index and `opaqueInlines` (`:882-905`).
- **Failure scenario**: a 64 Ki-unit document with a few thousand paragraphs allocates roughly 1.5 MB of temporary unit
  records per keystroke, plus the planner index. Not measured; the repository rules require a paired measurement.
- **Fix**: after the first `merge()` (`:4009`), skip leaves whose `[block.source.first, block.source.last)` does not
  intersect any range in `owned` (binary search by `range.last <= leaf.first`). The filter is sound: leaves are
  disjoint, and a wrapper (native HTML pair or code span) can only become active when an owned range covers its body,
  which implies that range intersects the wrapper's leaf; this holds for empty-body wrappers such as `<b></b>` with
  half-open intersection. The filter does not cover the global wrapper loop at `:4012-4024` (it visits every inline) nor
  the planner constructor, so per-keystroke cost stays O(document), only cheaper. Optionally reuse one `SemanticValue`
  buffer across leaves.
- **Regression test**: under `DXUI_ENABLE_DIAGNOSTICS`, add a counter of normalized leaves and assert it equals the
  number of leaves intersected by the deletion (Backspace in the last paragraph of a many-paragraph document: 1).
  Equivalence is pinned by golden plans captured at `6df17007` for a corpus with nested Markdown wrappers, native
  `<b>..</b>` pairs (including `<b></b>`), code spans and a multi-leaf selection; the plans must be byte-identical
  after the change. Add the paired complex-UI measurement required by `AGENTS.md`.
- **Verifier notes**: soundness argument checked by induction over leaves; magnitude is unmeasured.

### CMD-07 — Bidi and format controls in file paths reach visible link labels

- **Severity** P3 (reviewer P3, verifier P3; hardening, product decision) · **Category** security · **Verdict**
  Confirmed · **Confidence** medium · **Effort** S
- **Location**: `src/Controls/DxUi.RichImagePaths.h:41-48` (`ValidLocalPathText`), `:517-532` (label); reached from
  `src/Controls/DxUi.RichTextEditor.cpp:1046-1054`, `:1115-1129`
- **What**: paths from CF_HDROP (clipboard or OLE drop, any process) are validated for surrogates, NUL, `"`, CR/LF,
  `<>|?*` and C0 controls only. U+202A-U+202E, U+2066-U+2069, U+200B-U+200F, U+061C, U+2060 and U+FEFF pass into the
  link label (`AppendMarkdownLiteralPath` escapes ASCII punctuation only) and into the Rich-mode escaped literal
  fallback. The editor does not strip bidi controls from displayed text. Link activation is application-owned, but the
  visible label is a spoofing vector. The exported HTML never carries `file:` links (`SafeLink`,
  `src/Controls/DxUi.RichTextExport.cpp:65-69`), but the label text would appear there.
- **Failure scenario**: a hostile page or app puts a CF_HDROP named `report<U+202E>fdp.exe` on the clipboard. Pasting
  yields a link whose label displays as `reportexe.pdf` with a destination ending in `report%E2%80%AEfdp.exe`.
- **Fix**: when building the label (`:517-531`) and the Rich-mode literal fallback (`EscapeFileReferenceLiteral`,
  `src/Controls/DxUi.RichTextCommands.cpp:4098`), replace every character in the set U+061C, U+200B-U+200F,
  U+202A-U+202E, U+2060, U+2066-U+2069 and U+FEFF with U+FFFD. All are single UTF-16 units, so the existing capacity
  arithmetic (`labelUnits`, `:520-527`) stays valid. Leave the destination exact (it is already percent-encoded) and
  leave raw/code contexts alone (they keep literal text by contract). Do NOT use a numeric character reference such as
  `&#x202E;`: `DecodeSemanticText` (`src/Controls/DxUi.RichSemanticValues.h:92-93`) decodes character references back to
  the character, so the label would show U+202E again. Whether to substitute or to refuse the reference form and fall
  back to the escaped literal is a product decision.
- **Regression test**: `Tests/Support/Support.Tests.RichImagePaths.h`:
  `BuildMarkdownFileReference(L"", L"C:\\a\u202Efdp.exe")` must produce a label with none of the listed characters and a
  destination still containing `%E2%80%AE`; an ordinary Unicode name (`C:\\\u6771.png`) must be unchanged. Fails today.
- **Verifier notes**: Windows Explorer displays the same spoof for the same file, so the paste does not create a new
  display ambiguity beyond the Markdown label. The numeric-reference mitigation proposed by the reviewer was found
  ineffective.

### CMD-08 — Raw-context file paste inserts unescaped path text that breaks the enclosing token

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug · **Verdict** Confirmed for the inline-code case,
  plausible for HTML attribute and YAML · **Confidence** medium · **Effort** S
- **Location**: `src/Controls/DxUi.RichTextEditor.cpp:1083-1101`, `:1135-1151`; `CanPasteMarkdownFileReferences`
  `:877-925`
- **What**: when `references` is false (a code, HTML, front-matter or footnote-literal block, or the caret strictly
  inside an inline code/HTML/link/image token), each path is inserted as `"` + raw path + `"` with no escaping.
  `ReplaceSelection` (`:2329`) is a plain replace. A single-file list is affected too: `context` and `inlineOpaque` are
  built only for lists of more than one path, but the quoting loop runs either way. Three cases break:
  - inside an inline code span, a path containing a backtick (a legal NTFS character) closes the span, and a hostile
    name such as ``a` ... `b.txt`` can inject further Markdown (for example a link) after it;
  - inside a native HTML tag or a link/image token, the wrapping `"` breaks the token;
  - in YAML front matter, `"C:\new\table.png"` is a double-quoted scalar whose `\n` and `\t` are escapes.
  `docs/rich-text.md:540-541` promises "keep literal text"; the text is kept but the surrounding token is not.
- **Failure scenario**: caret inside `` `ab|cd` `` (one or two files), clipboard holds ``C:\x`y.txt``. Result
  `` `ab"C:\x`y.txt"cd` ``: the span closes at the second backtick and the rest of the paragraph's inline structure
  shifts.
- **Fix**: follow the precedent at `:1137-1138` (refuse a path containing `|` in a table cell: `return false`).
  Compute the enclosing token kind independently of `context` (the current `inlineOpaque` lambda also loses the kind),
  then: refuse the whole paste when a path contains a backtick and the caret is inside a code span; for an HTML, link
  or image token refuse file paste entirely (or insert the path unquoted and refuse names containing the token's
  delimiters, a product decision); for FrontMatter emit a YAML single-quoted scalar `'` + path with `'` doubled + `'`,
  or refuse. A refused paste inserts nothing (`ApplyClipboardFilePaste` returns false).
- **Regression test**: `Tests/Controls/Controls.Tests.RichTextInput.cpp` file-paste fixture: caret inside a code span, a
  one-file list and a two-file list, one name containing a backtick, expects unchanged source and no undo entry; a name
  without a backtick is still inserted quoted. A front-matter caret with `C:\new\table.png` expects
  `'C:\new\table.png'`, and a name containing `'` expects it doubled. Fails today.
- **Verifier notes**: the code-span case was traced to the plain replace; HTML and YAML follow from the same quoting
  loop but depend on the product decision above.

### CMD-09 — CF_UNICODETEXT is read and copied with no size cap

- **Severity** P3 (reviewer P3, verifier P3; hardening) · **Category** perf / local DoS · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/TextClipboard.cpp:23-35` (`ReadUnicodeClipboardData`), `:359-381` (`DecodeClipboardText`);
  `:109` (structured read path). Predates the window.
- **What**: the code takes `GlobalSize` unbounded, scans it with `wmemchr`, validates it with `ValidClipboardUnicode`,
  then copies it into a `std::wstring`, all while the clipboard is open. The only cap is `text.max_size()`. The rich
  editor later discards anything above `Document::MaximumUnits` (64 Ki). CF_HTML (266240 bytes) and CF_HDROP (clamped
  scan) are bounded; text is not. Allocation failure is mapped to `E_OUTOFMEMORY`, so the impact is a UI-thread stall
  while holding the clipboard, not a crash.
- **Failure scenario**: another process in the session places a 1 GiB CF_UNICODETEXT. Ctrl+V in the editor validates and
  copies 1 GiB on the UI thread, blocking other clipboard users, before the editor rejects it.
- **Fix**: add a cap without changing existing interface signatures (`docs/rich-text.md:206-208` promises they stay):
  a library constant in `include/DxUi/TextInputServices.h` next to `MaximumFiles` (`:63`) for the structured read, and
  a generous library-wide cap for the shared `Read()` path, which TextField also uses and which cannot know the editor's
  limit. In `DecodeClipboardText` search for the terminator with `wmemchr` over `min(size, cap + 1)` units and return
  `HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW)` before validating or copying when it is not found.
- **Regression test**: `ClipboardTransportFixture` with a 4 MiB CF_UNICODETEXT HGLOBAL: the structured read returns
  `ERROR_BUFFER_OVERFLOW`, leaves the output untouched, and allocates no copy of the payload. Fails today (it returns
  `S_OK`).
- **Verifier notes**: ordinary editors also read the full text; the scenario needs a process in the user's session.

## Refuted

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| CMD-04 (defect claim only) | A display-math payload containing a blank line is admitted and the result is literal `\[`/`\]` text; regression from `38b7b9e7`. | `src/Controls/DxUi.MarkdownBlocks.h:1367-1379` opens a `FencedCode` node for a lone `\[`; `:1167-1195` consume blank lines as fence content before the `if (empty)` branch at `:1243`. The early `return true` that made the check dead is from `e1926271`, not `38b7b9e7`. The dead block itself is kept as the CMD-04 cleanup. |
| CMD-10 (as a defect) | A malformed, ANSI or more-than-16-file CF_HDROP blocking paste is a bug. | `Specs/UI/UI_InputAndAccessibility.md:959` requires ANSI and malformed lists to fail, and `Tests/Controls/Controls.Tests.RichTextInput.cpp:820-821` asserts "explicit failure without silent text fallback". Moved to the questions. |

## Area architecture and simplification proposals

1. **One authority for where an item's content begins.** Make `ItemPrefix` carry `contentOnMarkerLine`,
   `firstParagraphContent` and a checkbox range or none, filled from the parser (`nodes[first].lines.front()`) instead
   of re-scanning padding. This removes three heuristics that disagree (`Prefix`, `LabelStart`, `TaskLabelCanUnwrap`)
   and is the clean form of the CMD-01 fix. Correction to the reviewer's wording: the inline parser does NOT record a
   task marker range today. It records only the `MarkdownTask` enum (`src/Controls/DxUi.MarkdownInlines.h:728`,
   `include/DxUi/MarkdownSyntax.h:221`); a `taskMarker` range exists only on the semantic element
   (`include/DxUi/RichSemanticDocument.h:58`). Either extend the syntax node (a published struct, so a public-header
   change) or derive the checkbox from `firstParagraphContent` and `text[x] == L'['`. Files:
   `src/Controls/DxUi.RichTextCommands.cpp`. Risk low. Effort M. CMD-01 can be fixed first with the flag alone; the
   refactor is not needed before production.
2. **Cache a physical-line index on `MarkdownSyntax`.** Every planner construction rebuilds `lines` and `opaqueInlines`
   for the whole document (`:882-905`), including `FileReferenceInsertionContext` (`:4108`), every Backspace and every
   command query that falls back to the planner. Publishing them once with the immutable syntax removes O(N)
   allocations per keystroke and shares the CRLF-aware line math (`PhysicalAt`, `Line`) between the noexcept admission
   helpers and the planner. Files: `include/DxUi/MarkdownSyntax.h`, `src/Controls/DxUi.RichTextCommands.cpp`. Risk low.
   Effort M. After production, driven by the CMD-06 measurement.
3. **Restrict deletion cleanup to intersected leaves** (CMD-06). The same idea applies to `FindCommandFootnoteOwner`
   (`:145`, scans all nodes) and `CommandNodeAt` (`:40`, an O(nodes) scan called about ten times per admission); a
   sorted interval index on the syntax would make admission O(log N). Effort M. The CMD-06 filter itself is before
   production if the measurement confirms a regression; the index is after.
4. **Clipboard format policy.** Separate "precedence" from "failure": decode each requested format best-effort into its
   own staging slot, then select by precedence, with per-format caps (CMD-09). Unify the read-time and drop-time
   CF_HDROP caps (read: `GlobalSize` unbounded with a clamped scan, `src/Controls/TextClipboard.cpp:72-88`; drop: 256 KiB,
   `:497`). Note that HDROP failure currently blocks text fallback by specification
   (`Specs/UI/UI_InputAndAccessibility.md:959`), so any precedence change needs a spec change first. Effort S-M. After
   production, except the CMD-09 cap.
5. **Library helper for the text-drop route.** `RichFileDropReceiver::DropText` hands applications a raw `IDataObject`
   (`include/DxUi/FileDrop.h:43`). A bounded `ReadDataObjectText(IDataObject*, maxUnits, std::wstring&)` next to
   `ReadDataObjectFilePaths` would stop consumers hand-rolling HGLOBAL parsing; the sample avoids the problem only by
   comparing identity (`Samples/EmbeddedControls/Main.cpp:592`). Effort S. After production.
6. **`CanInsertMath` cleanup** (CMD-04). Correction: the dead code was not produced by a shared tail; it is the early
   `return true` added by `e1926271`. Deleting the dead block is the fix; splitting the function into an inline and a
   display admission is optional readability. Effort S. Before production only as the CMD-04 deletion.

## Test-quality observations

- The CF_HTML oracle mirrors the implementation: `Tests/Support/Support.Tests.RichTextExport.h:171-175` round-trips
  `EncodeClipboardHtml` through the library's own `DecodeClipboardHtml`, so a consistent off-by-one in both passes. Add
  independent byte checks (`bytes.substr(StartHTML, 6) == "<html>"`, the 20 bytes before `StartFragment` equal
  `<!--StartFragment-->`, `bytes.substr(EndFragment, 18)` equals `<!--EndFragment-->`) and one captured real-producer
  sample (Chrome or Word, with SourceURL, `StartHTML != StartFragment` and multibyte text). The decode tests in
  `Tests/Embedded/Embedded.Tests.RichGeometryResources.h:482-537` use synthetic headers where the two are equal.
- The `WriteExport` transport test only checks that `htmlData` exists (`Support.Tests.RichTextExport.h:184-186`). It
  does not decode the published HGLOBAL, so the NUL terminator, the `html.size() + 1` sizing and the offsets of the
  bytes actually written are unchecked.
- No negative tests exist for next-line list content (CMD-01), a nested first child sharing its parent's marker line
  (CMD-02), paragraph lines that cannot interrupt (CMD-03) or a trailing-backslash footnote label (CMD-05).
  `MoveLinesAndItems` (`Support.Tests.RichSemantic.h:970`) covers only the happy cases (`- a\n- b`, `alpha\nbeta`, one
  nested child below its parent).
- For CMD-04 the missing test is the opposite of the reviewer's: a characterization test that a blank-line display
  payload still parses as one `[` fence (see CMD-04).
- Untested drop-target path: both formats offered. The `DropDataObject` fixture offers only CF_HDROP and
  `CreateRichTextDragData` only text, so "a file list wins when both are offered"
  (`src/Controls/DxUi.FileDrop.cpp:184-193`) has no test, and neither does "Ctrl or a missing MOVE in `allowed`
  refuses a text drop while a file drop is still accepted" (`:239`, `:282`).
- Good: the OLE revocation and nested-drop tests (`Tests/Controls/Controls.Tests.RichTextInput.cpp:200-252`) exercise
  reentrancy and medium release counts, and the allocation-failure atomicity tests for clipboard decode
  (`Embedded.Tests.RichGeometryResources.h:505-509`) can fail.
- Delimited table tests do not cover CR-only records, a quote in the middle of an unquoted field (rejected; Excel can
  emit it) or an empty header field. Add one positive or negative row per form to pin the documented behavior.

## Questions for the developer

1. **Bitmap priority over text** (`src/Controls/TextClipboard.cpp:89-108`, documented at `docs/rich-text.md:202-203`).
   Excel and several Office/line-of-business apps publish CF_BITMAP/CF_DIB alongside text, so with a bitmap handler
   installed, pasting copied cells inserts an image instead of the table text. Is that intended? Recommended answer:
   prefer text or HTML when present and use the image only when it is the sole content, or give the handler's installer
   an option to choose; either way record the decision in the documentation. Premise verified.
2. **Should the ANSI/oversize CF_HDROP "explicit failure" stay?** (was CMD-10;
   `src/Controls/TextClipboard.cpp:72-88`.) A malformed, ANSI (`fWide == FALSE`) or more-than-16-file list makes
   Ctrl+V do nothing even when Unicode text is offered, with no feedback. This is spec-mandated
   (`Specs/UI/UI_InputAndAccessibility.md:959`; the spec deliberately differs from DIBs, whose malformed offers "fall
   through", `:942`) and asserted by `Tests/Controls/Controls.Tests.RichTextInput.cpp:820-821`. Recommended answer:
   keep the failure; add user feedback for the more-than-16-file and malformed cases (the common 17-plus-file Explorer
   copy typically carries no CF_UNICODETEXT, so a text fallback would not help it). If a fallback is wanted, change the
   spec sentence and the test together.
3. **Text-drop route semantics.** The adapter forces `DROPEFFECT_MOVE` for any accepted text drop and refuses it when
   Ctrl is held (`src/Controls/DxUi.FileDrop.cpp:239`, `:263-266`, `:282`). Should the API let a receiver choose COPY
   for external text, or is the route permanently restricted to same-editor moves? Recommended answer: restricted to
   same-editor moves (the header comment suggests it); if so rename it `PreviewSourceMove`/`DropSourceMove`.
   Premise verified.
4. **`Image()` classifies `//host/share/x.png` as relative** (`src/Controls/DxUi.RichTextCommands.cpp:2911`, via
   `NativeHtmlDestination`, which accepts a missing colon, `src/Controls/DxUi.RichSemanticHtml.h:113`).
   `ResolveLocalImagePath` (`src/Controls/DxUi.RichImagePaths.h:335-347`) then parses it as a UNC path. The loader's
   default UNC rejection should cover it, but is it intended that an application-authorized network-file policy also
   admits scheme-relative spellings that look relative in source? Recommended answer: classify destinations starting
   with `//` as non-relative so they go through the explicit remote/local policy. The loader's UNC rejection was not
   traced by the verifier.
5. **Footnote insertion when the document ends inside a container.** The definition is appended after two end-of-line
   sequences at column 0 (`src/Controls/DxUi.RichTextCommands.cpp:2873-2903`). Recommended answer: keep it; footnote
   definitions are top-level blocks and a blank line ends the container. Premise verified.
6. **Display math on an empty line inside a loose list item** (raised by the verifier, not tested, predates the
   window). For a caret on a truly empty line inside a list item the site is `MarkdownNoNode`, so `CanInsertMath` skips
   the container-parent check (`:366`) and `Math()` skips `ContinuationPrefix` (`:2294`), inserting the fence at column
   0 and detaching following indented content. The spec says container prefixes are retained
   (`Specs/UI/UI_InputAndAccessibility.md:1425-1428`). Recommended answer: make this site unavailable, or derive the
   continuation prefix from the `EmptyLines()` owner.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.RichTextCommands.cpp` | full (all 4,326 lines at HEAD; window history via diff and blame for flagged areas) |
| `include/DxUi/RichTextCommands.h` | full |
| `src/Controls/TextClipboard.cpp` | full, plus window diff |
| `src/Controls/TextClipboard.h` | full |
| `src/Controls/DxUi.FileDrop.cpp` | full |
| `include/DxUi/FileDrop.h` | full |
| `src/Controls/DxUi.RichFileReferenceContext.h` | full |
| `src/Controls/DxUi.RichTextExport.cpp` | full |
| `include/DxUi/RichTextExport.h` | full |
| `src/Controls/DxUi.RichSemanticHtml.h` | full, plus window diff |
| `Tests/Support/Support.Tests.RichTextExport.h` | full |
| `Tests/Support/Support.Tests.RichFileDrop.h` | full |
| `Tests/Support/Support.Tests.ClipboardFixture.h` | full |
| `Tests/Support/Support.Tests.RichSemantic.h` | diff-relevant sections (command families, lines 640-1060) |
| `Tests/Controls/Controls.Tests.RichTextInput.cpp` | OLE, drop and clipboard sections (100-260, 800-830) |
| `Tests/Embedded/Embedded.Tests.RichGeometryResources.h` | CF_HTML and DIB section (440-540) |
| `src/Controls/DxUi.RichTextEditor.cpp` (caller) | touched functions: UniformListKind, CanPasteMarkdownFileReferences, TryPrepareClipboardPaste, TryPrepareFileReferencesAt, ApplyClipboardBitmapPaste/FilePaste, FileDropCaret, HitTestSourceDropPoint, CommitFileDrop, Begin/Cancel/MoveSelectedSource, CompleteExternalSourceMove, Apply/Build/ExecuteMarkdownCommand, PrepareMarkdownCommandState, QueryMarkdownCommandState, Erase, OnKeyDown Ctrl+V/X/Enter and its catch boundary |
| `src/Controls/DxUi.WindowHost.cpp` (caller) | PreviewFileDrop, CommitFileDrop(Core) |
| `src/Controls/TextInputServices.cpp` (caller) | clipboard command path 820-955 |
| `src/Rendering/Embedded.cpp` (caller) | ApplyClipboardBitmapPaste, TransformClipboardPaste |
| `src/Controls/DxUi.RichImagePaths.h` (data flow) | full (file-reference Markdown builder and path parsing) |
| `src/Controls/DxUi.MarkdownBlocks.h`, `DxUi.MarkdownInlines.h`, `DxUi.MarkdownLabels.h`, `DxUi.MarkdownTableCells.h` | targeted: node source spans, list-item creation, footnote label scanning, task detection, table cell slots |
| `src/Controls/DxUi.RichImageResources.h` | targeted (image source classification only; image loading skipped as out of scope) |

### Checked and holds

What the reviewer attacked and found sound (the verifier re-checked the export link filter and found nothing contrary):

- Export escaping and link scheme allow-list: every text and attribute output goes through `Escape`; `SafeLink` allows
  only `#`, `http(s)://` and `mailto:` and rejects whitespace and control characters; native HTML is re-rendered from the
  semantic tree, never passed through; no `<img>` is emitted; traversal is iterative; output is 4 Mi-bounded with no
  partial publish.
- CF_HTML encode and decode: byte offsets after strict UTF-8 conversion, `<!--EndFragment-->` cannot be injected,
  header keys must start lines and must not repeat, `StartHTML <= StartFragment <= EndFragment <= EndHTML <= size` is
  enforced, every `substr` is in range, output capped at 64 Ki units.
- Clipboard and drop memory ownership: every `GlobalLock` is paired with a scoped `GlobalUnlock`, memory is held in
  `wil::unique_hglobal` and released only after `SetClipboardData` succeeds, a failed second publish frees its buffer and
  reports failure, `ReleaseStgMedium` is always called, `pUnkForRelease` is honoured.
- DROPFILES and DIB validation: `pFiles` bounds, even size, `wchar_t` alignment, per-path and total caps, a scan clamp
  independent of `GlobalSize`, the double-NUL requirement, atomic swap into the output; DIB header size and kind,
  dimension bounds, bitfield mask overlap, overflow-safe stride times height, `biSizeImage` consistency and the 32 MiB
  cap.
- OLE drop target thread checks and reentrancy (`src/Controls/DxUi.FileDrop.cpp`): STA/thread checks, generation and
  attached revalidation after every foreign `QueryGetData`/`GetData`, self keep-alive across callbacks, nested Drop
  refused via `dropping`, detach during `GetData` tested, noexcept methods exception-free, a file list has priority over
  text.
- Table bounds: delimited input caps (64 Ki input, 64 columns, 257 rows, 4096 cells) are checked before edits are
  staged, malformed quoting is rejected atomically, every row has exactly the declared cell nodes so the sibling walks in
  DeleteTableRow/Column cannot run off.
- Exception containment: command, paste and drop exceptions are caught at `RichTextEditor::OnKeyDown`
  (`src/Controls/DxUi.RichTextEditor.cpp:6312-6319`), `ControlHost::CommitFileDropCore`, the Embedded transform and bitmap
  HRESULT wrappers and `HitTestSourceDropPoint`; `QueryMarkdownCommandState` and its helpers neither allocate nor throw;
  HTML converter, bitmap handler, clipboard and drop reads re-check lifetime, focus, revision, binding, selection, epoch
  and interaction after each foreign call, and composition blocks paste, drop and source move.
