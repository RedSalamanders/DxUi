# Markdown, front matter and source projection: findings

Appendix to [the review plan](../ProductionReview_2026-11-02.md). Scope: the Markdown block, inline and incremental
parsers, YAML and TOML front-matter validation, and Markdown source projection (`src/Controls/DxUi.Markdown*.h/.cpp`,
`src/Controls/DxUi.YamlMetadata.cpp`, `src/Controls/DxUi.RichMarkdownProjection.cpp`, `include/DxUi/MarkdownSyntax.h`,
`include/DxUi/RichMarkdown.h`, the corpus and Unicode-data generators and their tests).
Reviewed read-only at `6df17007` (window `a0f67fd3..6df17007`); nothing was built or executed. Each finding was
challenged by an independent red-team verifier; severity below is the verified severity. Runtime magnitudes are static
estimates, not measurements.

Headline: three P1 defects. Rendered text silently disappears between two autolinks on one line (MD-01). The
incremental parser imports front matter at the wrong offset after ordinary edits at document start (MD-02). A lone CR
in YAML front matter aborts the process in Debug and ASan Debug builds (MD-04). The P2 findings are one RichMarkdown
block-interruption gap and three quadratic scans reachable within the 64 Ki source cap with crafted input.

| ID | Sev | Verdict | Title | Primary location | Effort |
| --- | --- | --- | --- | --- | --- |
| MD-01 | P1 | Confirmed | Autolink deferred suffix is never decoded when the next link starts in a later sibling: text vanishes | `src/Controls/DxUi.MarkdownInlines.h:1326` | S |
| MD-02 | P1 | Confirmed | Closed-root suffix reuse ignores the document-start rule of front matter | `src/Controls/DxUi.MarkdownSyntax.cpp:682` | S |
| MD-04 | P1 | Confirmed | Lone CR in YAML front matter aborts Debug and ASan Debug builds; Release misreports | `src/Controls/DxUi.YamlMetadata.cpp:79` | S-M |
| MD-03 | P2 | Confirmed | `$$` and `\[` openers do not interrupt lazy continuation or GFM table bodies | `src/Controls/DxUi.MarkdownBlocks.h:1143` | S |
| MD-05 | P2 | Confirmed | Quadratic inline raw-HTML terminator search for unterminated comment, PI, CDATA and declaration openers | `src/Controls/DxUi.MarkdownInlines.h:1060` | S-M |
| MD-06 | P2 | Confirmed | Quadratic link-destination scan: unbounded parenthesis nesting in `InlineLink` | `src/Controls/DxUi.MarkdownInlines.h:765` | S |
| MD-07 | P2 | Confirmed | Quadratic paragraph re-flattening: `StartTable` calls `References` on every delimiter-like line | `src/Controls/DxUi.MarkdownBlocks.h:477` | S |
| MD-08 | P3 | Confirmed | Inline math opener probe rescans up to 16 Ki units per opener when an opaque token ends the probe | `src/Controls/DxUi.MarkdownInlines.h:1508` | S |
| MD-09 | P3 | Confirmed | Non-delimiter `=`, `+`, `~` and multi-`^` runs create unmergeable Text nodes | `src/Controls/DxUi.MarkdownInlines.h:1744` | S |
| MD-10 | P3 | Confirmed | Wiki links take precedence over valid inline `$` math, contradicting the docs | `src/Controls/DxUi.MarkdownInlines.h:1685` | S |
| MD-11 | P3 | Confirmed | Math-probe HTML lookahead misses a processing instruction that starts with the three characters `<?>` | `src/Controls/DxUi.MarkdownInlines.h:1068` | S |
| MD-12 | P3 | Confirmed | A recursive YAML key is reported as `YamlFrontMatterCapacity` | `src/Controls/DxUi.YamlMetadata.cpp:477` | S |
| MD-13 | P3 | Confirmed | YAML admission limits are undocumented although the public header says they are documented | `include/DxUi/MarkdownSyntax.h:119` | S |
| MD-14 | P3 | Confirmed | The pinned YAML corpus is CRLF-converted while its README calls it the exact upstream tree | `ThirdParty/Markdown/corpora/yaml-test-suite-6ad3d2c/source.tar` | S |
| MD-15 | P3 | Confirmed | Dead code from superseded designs (closed-root paragraph import, display-math inline branches) | `src/Controls/DxUi.MarkdownBlocks.h:1037` | S |
| MD-16 | P3 | Confirmed | A leading BOM blocks block recognition everywhere except the front-matter fence | `src/Controls/DxUi.MarkdownBlocks.h:663` | S |
| MD-17 | P3 | Confirmed | YAML alias resolution is a reverse linear scan over anchors | `src/Controls/DxUi.YamlMetadata.cpp:217` | S |

## P1 findings

### MD-01 — Autolink deferred suffix is never decoded when the next link starts in a later sibling: text vanishes

- **Severity** P1 (reviewer P1, verifier P1) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `c0a4f583` "Bound autolink preparation and preserve Markdown source semantics"
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1326-1365` (`AutolinkSplice`), `:1409-1483` (`ResolveAutolinks`,
  region end at `:1473-1479`), `:1941-1982` (autolink walk before `Emphasis`),
  `src/Controls/DxUi.RichSemanticDocument.cpp:1326`
- **What**: with the Autolinks extension (default in `Gfm029` via `All` and in `RichMarkdown` via `RichAll`,
  `src/Controls/DxUi.MarkdownSyntax.cpp:32-34`), the autolink pass runs over a flat sibling list before emphasis is
  resolved. A region spans every consecutive `Kind::Text` sibling (`:1420-1426`); delimiter runs and bracket openers are
  separate Text nodes because they are appended with `mergeText=false` (`:1728`, `:1744`), which also stops the next
  plain text from merging (`:284`). After an extended autolink, `AutolinkSplice` appends the trailing text of the same
  node with an **empty value** and remembers it only in the single slot `autolinkRegion_->deferred` (`:1359-1360`). It is
  decoded only if the next link starts inside that node (prefix decode, `:1340`) or if it is still `region.current` at
  region end (`:1473`). When the next link starts in a later node, the skip loop
  `while (nodes_[current].source.last <= range.first)` (`:1329-1330`) walks past it and the slot is overwritten. The node
  keeps `value == L""`; `RichSemanticDocument` emits nothing for an empty Text value (`RichSemanticDocument.cpp:1326`).
  Before `c0a4f583` the suffix was decoded eagerly.
- **Failure scenario** (Gfm029 or RichMarkdown defaults): `Visit www.a.com today, **not** www.b.com.`
  Siblings before the walk: `Visit www.a.com today, ` [0,23), `**`, `not`, `**`, ` www.b.com.` [30,41). Link 1 [6,15)
  leaves the empty suffix S1 [15,23) = ` today, ` in `deferred`. Link 2 [31,40) starts in the fifth node; S1 and the next
  three nodes are skipped, the new suffix `.` replaces `deferred`, and only it is decoded. Published forest:
  `Text("Visit ")`, Link, `Text(value="", source=" today, ")`, `Strong(not)`, `Text(" ")`, Link, `Text(".")`. Rendered:
  "Visit www.a.comnot www.b.com." Other triggers (any separate Text node between the two links on one line):
  - intraword underscore: `see www.a.com for snake_case and www.b.com` loses ` for snake`;
  - bolded second URL: `a https://x.example and **https://y.example**` loses ` and `;
  - bracket opener: `www.a.com x [y www.b.com`;
  - RichMarkdown non-delimiter runs (MD-09): `www.a.com is fast = good, www.b.com`;
  - second link ending exactly at its node end (no new suffix, so `current != deferred` at region end):
    `www.a.com x **y** www.b.com` loses ` x `.
- **Impact**: display loss with the source intact (`Document` and `MarkdownSyntax::Source()` are unchanged). Every
  output derived from the semantic document loses the text: Rich view, UIA text, and text and HTML export through
  `ExportSemanticDocument` and `ExportMarkdownSyntax` (`src/Controls/DxUi.RichTextExport.cpp:466-486`), so exported files
  silently omit it. Unproven secondary hazard: the hidden source range sits between visible spans, so a Rich-view range
  delete across the visible neighbours also deletes the invisible text.
- **Fix**: guarantee that every suffix node created by `AutolinkSplice` is decoded exactly once before the region is
  released. At the top of `AutolinkSplice`, if `autolinkRegion_->deferred != MarkdownNoNode` and
  `nodes_[deferred].source.last <= range.first`, decode it now (same `Decode` over its normalized range, add to
  `autolinkDecodedUnits_`) and clear `deferred`. If `deferred` becomes `beginNode`, it is prefix-decoded or unlinked, so
  clear it as well. At region end in `ResolveAutolinks`, decode any remaining `deferred` regardless of `region.current`.
  Each source unit is still decoded at most once, so the linear bound that motivated `c0a4f583` holds; keep the
  `autolinkDecodedUnits <= size` capacity assertion.
- **Regression test**: `Tests/Controls/Controls.Tests.RichMarkdown.cpp` (or next to the autolink cases in
  `Tests/Support/Support.Tests.MarkdownCapacity.h`). Parse each input above with `Gfm029` (the `=` case with
  `RichMarkdown`). Assert that no Text inline has an empty value and a non-empty source. Assert that the Text inline
  whose source slice is ` today, ` has value ` today, `, and that `MarkdownHtmlTests::Html(*syntax)` contains the lost
  text (the helper emits `node.value`, `Tests/Support/Support.Tests.MarkdownHtml.h:136`). All fail at `6df17007`.
- **Verifier notes**: traced node by node through scan, region growth, both splices and region end. No later pass
  refills empty Text values. No existing test has two autolinks separated by a non-mergeable node: the `links` capacity
  case puts every link in one node, and `invalidHosts` contains `_` but accepts no link
  (`Tests/Support/Support.Tests.MarkdownCapacity.h:103-123`); the GFM spec corpus has no such line.

### MD-02 — Closed-root suffix reuse ignores the document-start rule of front matter

- **Severity** P1 (reviewer P1, verifier P1) · **Category** bug · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S · **Introduced** `48ef3ee6` "Reuse proven closed Markdown root subtrees"
- **Location**: `src/Controls/DxUi.MarkdownSyntax.cpp:682-719` (`ScanBlockUnit` reuse gate),
  `src/Controls/DxUi.MarkdownSyntax.cpp:540-671` (`ClosedRootUnit`), `src/Controls/DxUi.MarkdownBlocks.h:663`
  (`FrontMatterLine`: `if (first != 0u) return false;`), `:984-987` (`CanImportClosedRoot`), `:992-1019`
  (`ImportClosedNode`)
- **What**: in suffix mode (`cursor >= commonNewEnd`) an old root at `oldPosition = commonOldEnd + (cursor - commonNewEnd)`
  is imported whenever no container or leaf is open and the old root is closed. The comment "A suffix is unchanged
  through EOF, so its existing closure remains valid" holds for every block except RichMarkdown front matter, whose
  recognition depends on absolute position 0. The gate runs before `blocks.Line()`, so `FrontMatterLine` is never
  consulted for an imported root, and `ClosedRootUnit` validation rejects only SourceEditor and ReferenceDefinition
  roots (`:574-582`). `ImportClosedNode` copies `frontMatterKind`, `closed` and the shifted `info` range.
- **Failure scenarios** (RichMarkdown, editor or `MarkdownParser::Begin(SourceSnapshot, ...)` with tracked edits):
  1. Published `---\ntitle: a\n---\n\nBody\n`; `Document::Replace(0, 0, L"Intro\n\n")` (tracked interval `{0, 0, 7}`).
     Cursor 0 is neither prefix nor suffix; `Intro` opens a paragraph; the blank line closes it. At cursor 7 the gate
     finds the old FrontMatter at `oldPosition 0` and imports it at 7..24. Incremental result: Paragraph,
     **FrontMatter at 7** (YAML-validated, presented as a collapsed Properties header), Paragraph. Fresh parse:
     Paragraph, ThematicBreak, SetextHeading(`title: a`), Paragraph. Pressing Enter at offset 0
     (`Replace(0, 0, L"\n")`) reproduces it the same way.
  2. Published `Intro\n\n---\ntitle: a\n---\n`; `Replace(0, 7, L"")` (interval `{0, 7, 0}`). At cursor 0 (suffix,
     `oldPosition 7`) the old Paragraph is skipped and the old ThematicBreak (closed by default,
     `include/DxUi/MarkdownSyntax.h:83`) and SetextHeading are imported. Fresh parse: FrontMatter. Deleting a stray blank
     line above front matter therefore does not make it recognized.
- **Persistence**: scenario 1 persists across later edits elsewhere. The imported FrontMatter is followed by a
  document-owned blank line, so prefix reuse re-imports it (`MarkdownSyntax.cpp:705-709`). It heals only when an edit
  reaches it or on a fresh parse (for example reopening the file). Scenario 2 heals on the next edit below it: prefix
  reuse of the ThematicBreak needs a following blank line, which is absent, so cursor 0 is reparsed through
  `FrontMatterLine`.
- **Reach**: every ordinary edit uses this path. `RichTextEditor` calls `Begin(Snapshot(), ...)` whenever the published
  syntax is stale outside source-editing envelopes (`src/Controls/DxUi.RichTextEditor.cpp:3689-3693`). `Begin` passes
  `SourceSnapshot::ChangedRangeFrom`, which is `TrackedEdits` for same-lineage edits
  (`src/Controls/DxUi.MarkdownSyntax.cpp:933-941`, `src/Controls/DxUi.RichDocument.cpp:541`). Source view is affected
  too: source ink keys on FrontMatter (`src/Controls/DxUi.RichSourceInk.h:208`), and metadata targets reject Markdown
  style commands.
- **Fix**: in `ScanBlockUnit`, in suffix mode, refuse reuse when `(cursor == 0) != (oldPosition == 0)`. This covers
  both directions of the only absolute-position rule (`first != 0`, optional BOM included); prefix mode already has
  `cursor == oldPosition`. Restricting it to RichMarkdown is optional (one compare). If more position-sensitive kinds
  appear, centralize a `PositionSensitive(kind, position)` predicate shared by the gate and `ClosedRootUnit` validation.
- **Regression test**: `Tests/Support/Support.Tests.MarkdownSourceCorrelation.h`, inside `ClosedRootDifferential`
  (reuse its `equal` lambda). For each of: scenario 1, the Enter-at-offset-0 variant and scenario 2, build a `Document`,
  complete a tracked parser (`Begin(document.Snapshot(), ...)`) and a string parser (`Begin(document.Text(), ...)`) with
  RichMarkdown, apply the `Replace`, complete both again and call `equal(*changed, *expected)`. Also assert root kinds
  directly: no FrontMatter root after scenario 1, a FrontMatter root at 0 after scenario 2. For scenario 1, apply a
  second edit below `Body` and compare again (persistence). The string parser derives the same correlation interval,
  so the identity comparison in `equal` is a valid oracle. All fail at `6df17007` on node kind.
- **Verifier notes**: the test exclusion is real. The FrontMatter row
  `{L"---\nname: value\n---\n", FrontMatter, false, true}` (`Support.Tests.MarkdownSourceCorrelation.h:237`) skips
  suffix mode, and every case only edits `TARGET` to `TARGET!` (`:304-319`), which cannot change structure.

### MD-04 — Lone CR in YAML front matter aborts Debug and ASan Debug builds; Release misreports

- **Severity** P1 (reviewer P2, verifier P1) · **Category** bug (crash in supported configurations) · **Verdict**
  Confirmed (mechanism corrected) · **Confidence** high for Debug, medium for Release · **Effort** S-M
- **Location**: `src/Controls/DxUi.YamlMetadata.cpp:79-120` (`ParseYamlEvents`), `:90-99` (handler-local callbacks),
  `:555-562` (control-character filter admits U+000D), `src/Controls/DxUi.MarkdownMetadata.cpp:21-24` (`MetadataUtf8`,
  "Do not normalize EOLs"), `ThirdParty/rapidyaml/0.16.0/rapidyaml.hpp:21764-21786`
  (`LineContents::reset_with_next_line`), `:18680-18681`, `:18762-18768`, `:25688-25696`, `:25744-25747`,
  `Directory.Build.props:33`
- **What**: Markdown accepts a lone CR as a line ending (`MarkdownSyntax.cpp:735-741`) and `FrontMatterLine`
  recognizes the envelope with CR lines. YAML 1.2.2 `b-break` also accepts a lone CR. The YAML payload reaches rapidyaml
  unchanged. `parse_in_place_ev` calls `_scan_line()` first (`rapidyaml.hpp:35610-35620`), and
  `reset_with_next_line` splits lines only on `\n`, strips trailing CRs, then asserts
  `RYML_ASSERT_BASIC_(rem.find('\r') == npos)`.
  - **Debug and ASan Debug** (`NDEBUG` is defined only for Release, `Directory.Build.props:33`): `C4_USE_ASSERT` is 1,
    so `RYML_ASSERT_BASIC_` is `RYML_CHECK_BASIC_` (`:18680-18681`). That macro calls `err_basic(get_callbacks(), ...)`,
    which uses rapidyaml's **global** callbacks (`:18762-18768`, `:25846-25849`, `:25744-25747`), not the handler-local
    callbacks that `ParseYamlEvents` installs. DxUi never calls `set_callbacks`, so the default `error_basic_impl` prints
    to stderr and calls `abort()` (`:25688-25696`; `RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS` is not defined, so
    `RYML_WITH_EXCEPTIONS_` is off, `:18114-18116`). `YamlBasicError` and `MetadataIssue::Invalid` are never reached.
  - **Release**: the assert is compiled out and `rem` keeps the interior CR. The likely result is a parse error mapped
    to `InvalidYamlFrontMatter` for valid metadata, but the Release outcome is **not statically proven**.
- **Failure scenario**: RichMarkdown source `---\rtitle: a\rtags: b\r---\r# Doc\r`, or any lone CR inside a YAML
  envelope of an LF or CRLF file. Validation runs in `IdentityUnit` on the owner thread (`MarkdownSyntax.cpp:430-441`),
  so a Debug consumer build that opens such a file terminates; a Release build shows the metadata fully literal with
  `InvalidYamlFrontMatter`.
- **Fix** (both parts):
  1. Normalize CR and CRLF to LF in the private YAML UTF-8 staging only (a YAML-specific conversion inside
     `ParseYamlEvents`, or a flag on `MetadataUtf8`). The published source and the whole-content diagnostic range are
     unchanged. Keep the TOML path byte-exact; TOML 1.0 forbids a lone CR, so `InvalidTomlFrontMatter` stays correct.
  2. Install DxUi-private global rapidyaml callbacks once (`dxui_private_c4::yml::set_callbacks`; the `c4` namespace is
     renamed in `YamlMetadata.cpp:17`, so no other rapidyaml user is affected) before any handler is constructed. Their
     error functions throw the contained `YamlSyntaxError` (or `YamlCapacityError` for allocation) instead of
     `abort()`. In Debug, `RYML_NOEXCEPT` is empty when assertions are on (`:17637-17640`), so the throw reaches the
     existing `catch` in `ParseYamlEvents`. Give the global allocate and free the same budget accounting as
     `AllocateYaml` (for example through a thread-local pointer to the current `Budget`).
  Other paths checked that can still abort or bypass the budget:
  - rapidyaml has 86 non-callback `RYML_ASSERT_BASIC_` sites (Debug only) and about 10 non-callback parse or basic
    check sites. Part 2 covers all of them. The always-on ones found are not reachable as traced: `normalize_tag_long`
    overlap check (`:26182`, DxUi's source and arena are separate strings), the `ParseEngine` null-handler check
    (`:27059`), `to_str_sub` (`:35786-35787`, event formatting), `:30853` (inside `RYML_DBG`), and file I/O helpers.
  - `EventHandlerInts` owns a `TagCache` (`:22579`) whose `detail::stack` is default-constructed with
    `get_callbacks()` (`:20202`, `:20792`). With `resolve_tags_all(true)`, more than 16 distinct resolved tags
    allocate through the global allocator, today `malloc`, outside the 4 MiB budget and the allocation-failure sweeps,
    and an allocation failure aborts in every configuration. Part 2 routes this through the budget. Residual: in
    Release `TagCache::add` is `noexcept`, so a throwing allocation there still terminates; it is reachable only on
    genuine out-of-memory, because 16-entry growth cannot approach the 4 MiB budget within 64 Ki source units.
  - c4core `C4_ASSERT`/`C4_XASSERT` (209 sites, Debug only) and always-on `C4_CHECK` (12 sites) call
    `c4::handle_error`, which aborts unless an error callback leaves first (`:25423-25470`). Those frames are
    `noexcept` (`C4_NOEXCEPT` is `noexcept` because `C4_ERROR_THROWS_EXCEPTION` is not defined), so a throwing callback
    would terminate as well. They guard internal invariants (substring bounds); none was found reachable from input.
    Cover them with the Debug corpus run below rather than with a callback.
- **Regression test**: `Tests/Controls/Controls.Tests.RichMarkdown.cpp`,
  `TestYamlFrontMatterValidationAndSourcePreservation`: add `L"---\rtitle: a\rtags: b\r---\r# Doc\r"` expecting a
  closed YAML FrontMatter and no diagnostics, plus a mixed `\r\n`/`\r`/`\n` envelope. Call
  `Detail::ValidateYamlMetadata(L"title: a\rtags: b\r")` and expect `MetadataIssue::None`. At `6df17007` the Debug
  and ASan Debug test processes abort. Add a case with 20 distinct explicit tags to the allocation-failure sweep in
  `Tests/Embedded/Embedded.Tests.RichTextResources.h` to prove that tag-cache allocations are budgeted and retryable.
  Run the YAML corpus in Debug in LF, CR and CRLF forms (see MD-14).
- **Verifier notes**: no YAML CR-only test exists (only the TOML scenario at
  `Tests/Support/Support.Tests.MarkdownBlocks.h:220`), which is consistent with the Debug suites passing. Severity
  raised because document content terminates a process in configurations that AGENTS.md requires to build and test and
  that consumers link.

## P2 findings

### MD-03 — `$$` and `\[` openers do not interrupt lazy continuation or GFM table bodies

- **Severity** P2 (reviewer P2, verifier P2) · **Category** bug / conformance (RichMarkdown) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S · **Introduced** `43589e4c` "Own display math delimiters as source blocks"
- **Location**: `src/Controls/DxUi.MarkdownBlocks.h:1143-1146` (lazy `interrupts`), `:1251-1263` (table close list),
  `:1367-1380` (display-math opener), `:291-305` (`Fence` accepts only backtick and tilde)
- **What**: at top level a `$$` or `\[` line closes an open paragraph and opens display math (`:1367-1380`), but neither
  "can this line start a block" list includes it. A lazy line is continued when it does not interrupt
  (`:1147-1151`), and the table branch calls `ContinueTable` unless the line is a thematic break, ATX heading, fence,
  HTML start, quote, list marker or footnote. Inline, `$$` is literal (`MarkdownInlines.h:1488-1493`).
  `docs/rich-text.md:1593-1594` says paired `$$` or `\[` delimiters on their own logical lines own display math.
- **Failure scenario** (RichMarkdown): `> a\n$$\nx\n$$\n` gives a blockquote paragraph `a $$ x $$` (all lazy); with a
  backtick fence instead, the quote closes. Same for `- a\n$$\nx\n$$\n`. `| a |\n| - |\n| 1 |\n$$\nx\n$$\n` makes `$$`,
  `x` and `$$` three table rows instead of ending the table.
- **Fix**: add a `DisplayMathFence(Cursor)` predicate (RichMarkdown only, indent at most 3, `$$` or `\[` followed only by
  spaces or tabs, the same test as `:1367-1368`) and use it in both `interrupts` and the table close list. The durable
  fix is the shared block-start predicate (proposal 1).
- **Regression test**: `Tests/Support/Support.Tests.MarkdownBlocks.h` scenarios with RichMarkdown. Assert root kinds:
  first input → BlockQuote containing Paragraph `a`, then FencedCode with `fenceMarker == L'$'`; second → List, then
  FencedCode; third → Table with one body row, then FencedCode. All fail at `6df17007`.
- **Verifier notes**: the top-level and lazy paths were traced to disagree for the same line.

### MD-05 — Quadratic inline raw-HTML terminator search for unterminated comment, PI, CDATA and declaration openers

- **Severity** P2 (reviewer P2, verifier P2) · **Category** perf · **Verdict** Confirmed · **Confidence** high (shape),
  medium (magnitude) · **Effort** S-M · **Introduced** pre-existing in the ordinary scanner; the window bounded only
  angle autolinks (`32c11296`) and added the terminator index for the RichMarkdown math probe
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1044-1105` (`HtmlEnd`: `terminated` uses
  `input.find(suffix, prefix.size())`, declarations use `input.find(L'>', ...)`), `:1598-1599` (called from `Scan()` for
  every `<` without lookahead), `:642-699` (`htmlNext_` built only for RichMarkdown leaves containing `$` or `\`)
- **What**: every unterminated `<!--`, `<?`, `<![CDATA[` or `<!X` opener searches to the end of the leaf. A failed
  search proves that no later opener of the same kind can succeed either, but nothing records it. All profiles.
- **Failure scenario**: a single-line paragraph of 32,768 `<?` pairs (65,536 units, the `Document::MaximumUnits` cap;
  the text stays one merged Text node) performs about 32 Ki searches over about 32 Ki units each, roughly 5·10^8
  candidate steps. `Advance(4096)` performs one `Scan` per unit (`MarkdownSyntax.cpp:1014-1029`), so about 2 Ki searches
  run per owner turn. Estimate: with a scalar substring search about 0.25 s per turn and about 2 s per reparse; with a
  vectorized STL search a few ms per turn. Same shape for `<!--` repeated.
- **Fix**: per leaf, record the last occurrence of `-->`, `?>`, `]]>` and `>` once (linear). A search whose start lies
  after the last occurrence fails in O(1). A successful search is consumed by the emitted Html node, so total work is
  linear. Keep the `<!-->` and `<!--->` special cases, which `HtmlEnd` evaluates after `terminated` today. Alternative:
  build `htmlNext_` for every leaf containing `<` and call `HtmlEnd(cursor_, &lookahead)` (36 bytes per unit; apply the
  MD-11 fix first).
- **Regression test**: `Tests/Support/Support.Tests.MarkdownCapacity.h`, next to the `angles` case. Add a deterministic
  `htmlInspectionUnits` counter to the inline statistics (as `autolinkInspectionUnits` is published). Inputs in all
  three profiles: 16 Ki `<?`, 12 Ki `<!--`, 7 Ki `<![CDATA[`, and 21 Ki `<!A` (in Gfm029, `<!A ` with a trailing
  space). Assert counter ≤ 8 × size and a doubling ratio ≤ 2.5. Fails at `6df17007` once the counter exists.
- **Verifier notes**: reachable within the 64 Ki cap; bounded; crafted input only; not a hang per owner turn.

### MD-06 — Quadratic link-destination scan: unbounded parenthesis nesting in `InlineLink`

- **Severity** P2 (reviewer P2, verifier P2) · **Category** perf / conformance · **Verdict** Confirmed · **Confidence**
  high · **Effort** S · **Introduced** pre-existing
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:763-789` (non-angle destination loop), reached from every
  `CloseBracket()` (`:848-858`, `:1732`)
- **What**: `(` increments `depth` without a limit. An unbalanced destination scans to the first space or control
  character or the end of the leaf, then fails at `if (depth) return false;`. Every closing bracket that is followed
  by an opening parenthesis repeats the scan.
- **Failure scenario**: a single-line paragraph of 16,000 repetitions of the four characters `[`, `x`, `]`, `(` (64,000
  units; 32,000 inline nodes, just under `MaximumNodeRecords`). Each `]` scans to the end: about 16 Ki × 32 Ki ≈ 5·10^8
  simple iterations, roughly 0.3-0.5 s per reparse and at most about 50 ms per owner turn. The lowest of the three
  quadratic findings.
- **Fix**: return false when `depth` exceeds 32, as cmark does. A failing destination must grow `depth` by one per
  repetition (a `)` at depth 0 ends the destination successfully), so the cap bounds every failing scan to about 33
  repetitions. Document the limit in `docs/rich-text.md`.
- **Regression test**: `Tests/Support/Support.Tests.MarkdownCapacity.h`: the input above in all three profiles with a
  destination-scan counter (doubling ratio ≤ 2.5). Keep conformance: the CommonMark spec vector with link text `link`
  and the nested destination `foo(and(bar))` still parses as a link, and a destination nested 33 deep is literal.
- **Verifier notes**: confirmed reachable within the node cap; magnitude is an estimate.

### MD-07 — Quadratic paragraph re-flattening: `StartTable` calls `References` on every delimiter-like line

- **Severity** P2 (reviewer P2, verifier P2) · **Category** perf · **Verdict** Confirmed (scenario input corrected) ·
  **Confidence** high · **Effort** S · **Introduced** pre-existing (same order at `a0f67fd3`)
- **Location**: `src/Controls/DxUi.MarkdownBlocks.h:469-494` (`StartTable`, `References(leaf_)` at `:477` before any
  cell-count check), `:827-850` (`References` flattens the paragraph into `std::wstring text` and
  `std::vector<std::size_t> offsets` without reserve)
- **What**: in Gfm029 and RichMarkdown (Tables enabled), each line made only of space, `|`, `:`, `-` and tab that
  contains `-` re-flattens the whole open paragraph when it starts with `[`. `ReferenceAt` itself is bounded here by the
  999-unit label limit (`:716-717`). If no definition is found nothing changes, so the next such line repeats the work.
- **Failure scenario**: a paragraph whose header and delimiter cell counts never match. Start with `-|-`: the first
  delimiter line `-|` would form a table immediately, because `[x` and `-|` both have one cell
  (`src/Controls/DxUi.MarkdownTableCells.h:17-25` strips an edge pipe).

  ```
  [x
  -|-
  -|
  -|-
  -|
  ...   (alternate up to about 16,000 lines, about 56 Ki units)
  ```

  Each line compares one cell with two or two with one, so every line stays paragraph text (none is a list marker,
  thematic break or setext underline). About 16 Ki calls × about 28 Ki units ≈ 4.5·10^8 pushes into each of two
  buffers, plus large-block reallocation. Estimate: about 0.1-0.25 s per owner turn (about 1 Ki lines per
  `Advance(4096)`) and 1-3 s per reparse. Above about 16 Ki lines the inline phase exceeds the node cap and the
  publication is rejected, but only after the quadratic block phase has run.
- **Fix**: run the cheap checks on `lines.back()` first (pipe presence, header and delimiter cell counts, alignment
  syntax). Call `References` only when they pass, then re-derive the header from `lines.back()` as the code already does
  (`References` removes only leading lines or retires the paragraph). A successful table start closes the paragraph, so
  `References` runs at most once per paragraph plus once at close. Pitfall: this changes the observable outcome in the
  Question 4 shape (definitions are no longer extracted early when the cheap checks fail); rerun the GFM corpus.
- **Regression test**: `Tests/Support/Support.Tests.MarkdownCapacity.h`: the corrected input at 4,000 and 8,000 lines
  in Gfm029 and RichMarkdown. Assert one Paragraph root and no Table, and that a new reference-flatten counter in
  `BlockStatistics()` grows at most 2.5× when the input doubles (4× today).
- **Verifier notes**: the worst of the three quadratic findings; crafted input only; bounded by the 64 Ki cap.

## P3 findings

### MD-08 — Inline math opener probe rescans up to 16 Ki units per opener when an opaque token ends the probe

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** medium ·
  **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1508-1563`, flag 4 at `:665`
- **What**: the `noCandidate` shortcut applies only when no eligible closer exists and either the line ends before the
  limit or the remaining text is at most `MathMaximumSourceUnits` (16,384). Otherwise each opener walks forward until a
  matched backtick run or HTML or autolink token (`:1529-1550`), a closer or the limit. When an opaque token stops it,
  the opener becomes a literal `$` and the next opener rescans the same span. A `$` preceded by a space never qualifies
  as a closer, so this pattern has no candidate at all.
- **Failure scenario** (RichMarkdown, one line, 65,457 units; the reviewer's original input was 40 units over the cap):

  ```
  B     = "$x " repeated 5,450 times, then the 9 characters: `` `c` ``
  input = B B B followed by 16,380 letters a
  ```

  About 1.3·10^8 probe steps per reparse, about 30 ms per owner turn.
- **Fix**: per leaf, memoize the earliest opaque token at or after each position (the backtick and HTML indexes
  already exist). When the next eligible closer lies beyond that token, or none exists, the opener fails in O(1).
- **Regression test**: `testMathOwnershipProbeWork` checks in `Tests/Support/Support.Tests.RichSemantic.h` (lines
  440-550): one block of k openers, the code span and a 16,400-letter tail, with k = 2,000 and 4,000; assert the
  existing ≤ 3× ratio. Today the ratio is about 4×.

### MD-09 — Non-delimiter `=`, `+`, `~` and multi-`^` runs create unmergeable Text nodes

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf / simplification · **Verdict** Confirmed (one example
  corrected) · **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1741-1748`
- **What**: the run is appended with `mergeText=false` before the length check returns for runs that cannot be
  delimiters, so RichMarkdown `a = b`, `x+y`, `a ^^ b`, `===` and GFM single `~` split the surrounding text into three
  nodes without creating a delimiter record. A single `^` with Superscript enabled is a real delimiter run and
  legitimately gets its own node, so `2^10` is not an example. The split also widens MD-01's trigger set. A 64 Ki note
  of 16,384 repetitions of `a = ` reaches 32,768 inline nodes and the whole document falls back to source with a
  capacity rejection.
- **Fix**: perform the length and extension check first; append non-delimiter runs with `mergeText=true` and keep
  `false` only when a delimiter record is pushed. Merging is safe because no delimiter record references the node.
- **Regression test**: RichMarkdown `a = b + c ^^ d` yields exactly one Text inline; Gfm029 `a ~ b` likewise; 16,384 ×
  `a = ` in RichMarkdown publishes without `length_error`.

### MD-10 — Wiki links take precedence over valid inline `$` math, contradicting the docs

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance / doc · **Verdict** Confirmed · **Confidence**
  medium · **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1652-1723` (body validation `:1685-1698` rejects backtick, `<`,
  backslash and controls, but not `$`), `docs/rich-text.md:1054` ("Code/HTML/math keep precedence")
- **What**: the scanner reaches `[[` before the inner `$`, so a wiki object wins. `$y]] z$` is valid inline math
  (non-space edges, no trailing digit). The documented precedence holds for `\(` (backslash is rejected) but not `$`.
- **Failure scenario** (RichMarkdown): `[[x $y]] z$` gives a wiki link to `x $y` and a literal ` z$`.
- **Fix**: when the body contains an unescaped `$`, probe whether a valid math span starting inside the body extends
  past `wikiClose_`, and reject the wiki object if it does; or reject `$` in bodies like the backtick; or amend the
  docs to say wiki bodies take precedence over `$` math.
- **Regression test**: `TestRichMarkdownWikiLinks` in `Tests/Controls/Controls.Tests.RichMarkdown.cpp`: the input
  yields no `wiki` inline and one Math inline with value `y]] z` (or, with the doc amendment, pin the current result).

### MD-11 — Math-probe HTML lookahead misses a processing instruction that starts with the three characters `<?>`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** bug (scanner and probe disagree) · **Verdict** Confirmed ·
  **Confidence** high · **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownInlines.h:1060-1074` (lookahead branch), index `:684-698`
- **What**: `htmlNext_[i][2]` is the first `?>` at or after `i`. For `<?>x?>` it is `start + 1`, overlapping the opener,
  so `absolute >= start + prefix.size()` fails and the lookahead returns 0, while the non-lookahead `find("?>", 2)`
  accepts the processing instruction. Comments and CDATA are not affected (their overlapping forms `<!-->` and `<!--->`
  are special-cased).
- **Failure scenario** (RichMarkdown): `$a <?>x$ ?>` → the probe does not stop at the opaque PI and emits Math `a <?>x`;
  the regular scanner would let the PI own `<?>x$ ?>`.
- **Fix**: look up `htmlNext_[start + prefix.size()][slot]` for all three terminators. The row exists because
  `starts_with(prefix)` was checked and `htmlNext_` has `size() + 1` rows.
- **Regression test**: `Tests/Support/Support.Tests.RichSemantic.h`: the input yields no Math inline and one Html inline
  `<?>x$ ?>`.

### MD-12 — A recursive YAML key is reported as `YamlFrontMatterCapacity`

- **Severity** P3 (reviewer P3, verifier P3) · **Category** api / doc · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.YamlMetadata.cpp:475-478` (`CanonicalYamlNode`: `active[index]` throws
  `YamlCapacityError`); pinned by `Tests/Controls/Controls.Tests.RichMarkdown.cpp:953`
- **What**: `? &cycle [*cycle]\n: value` is three lines. "Capacity" tells the user to shrink the document, but no size
  helps. Reporting `InvalidYamlFrontMatter` would also be wrong: a recursive anchor is a well-formed YAML representation
  graph; DxUi simply cannot compare it as a key.
- **Fix**: add a distinct diagnostic, for example `MarkdownDiagnosticCode::UnsupportedYamlKey` with its own
  `MetadataIssue`, accessibility text and docs entry, and throw a dedicated exception on a cycle. Do not map it to
  Invalid.
- **Regression test**: update `Controls.Tests.RichMarkdown.cpp:953` to expect the new code; keep a large non-recursive
  key that still yields `YamlFrontMatterCapacity`.

### MD-13 — YAML admission limits are undocumented although the public header says they are documented

- **Severity** P3 (reviewer P3, verifier P3) · **Category** doc (truthfulness) · **Verdict** Confirmed (narrowed) ·
  **Confidence** high · **Effort** S
- **Location**: `include/DxUi/MarkdownSyntax.h:119-122` ("validated within documented admission limits"),
  `docs/rich-text.md:1517-1522`, `Specs/UI/UI_ControlsAndLayout.md:2033`
- **What**: YAML limits appear in neither docs nor Specs: 4,096 nodes and depth 128 (`YamlMetadata.cpp:151`), 256 Ki
  event words and a 512 KiB arena (`:85-86`), a 4 MiB parser allocation budget (`:39`), 100,000 canonical visits,
  8 MiB total and 1 MiB per-key canonical bytes (`:465-478`), digit limits treated as capacity (`:294`, `:384`,
  `:393`) and 128 `%TAG` directives (`:16`). TOML: the 1,024 total-key-segment cap is documented in
  `Specs/UI/UI_ControlsAndLayout.md:2033`; only `docs/rich-text.md` omits it and says "nesting-capacity refusal".
- **Fix**: add a limits table to `docs/rich-text.md#leading-metadata-envelopes` and to the Specs paragraph, and state
  the TOML total-key cap in `docs/rich-text.md`.
- **Regression test**: none (documentation); `validate.ps1` covers links.

### MD-14 — The pinned YAML corpus is CRLF-converted while its README calls it the exact upstream tree

- **Severity** P3 (reviewer P3, verifier P3) · **Category** test-gap / doc · **Verdict** Confirmed · **Confidence**
  high · **Effort** S
- **Location**: `ThirdParty/Markdown/corpora/yaml-test-suite-6ad3d2c/source.tar`, `.../README.md`,
  `Tests/Controls/Controls.Tests.MetadataCorpus.h:728+`, `Tools/Generate-MetadataCorpus.ps1`
- **What**: 397 of the 402 `in.yaml` members are CRLF, none is LF-only, five have no line break, and the generated
  header embeds the CRLF bytes (for example `\x2D\x0D\x0A` in 229Q). Proof that the archive is not the upstream bytes:
  the README's "original input bytes" SHA-256 for P76L (`b9cb38…`) equals the archive member with CR removed, while the
  stored member hashes to `15eae4…`. All 382 conformance cases therefore exercise only CRLF; the common LF form has about
  20 hand-written cases. The TOML archive is mostly LF (289 of 300 sampled).
- **Fix**: regenerate `source.tar` with `git -c core.autocrlf=false archive` at the pinned commit, re-pin the hash,
  regenerate the header, and make the generator verify the README per-case hashes against the archive members. Run each
  YAML case in LF, CRLF and CR forms (the CR form would have caught MD-04).
- **Regression test**: `Tools/Generate-MetadataCorpus.ps1 -Check` fails on a CRLF archive; the native corpus test runs
  all three line-ending forms.

### MD-15 — Dead code from superseded designs

- **Severity** P3 (reviewer P3, verifier P3) · **Category** simplification · **Verdict** Confirmed · **Confidence**
  high · **Effort** S
- **Location**: `src/Controls/DxUi.MarkdownBlocks.h:1037-1063`, `src/Controls/DxUi.MarkdownInlines.h:1494-1499`,
  `:1564-1573`, `:656-669`
- **What**: `ImportClosedRootParagraph` has no callers (added in `7b5e4455`, superseded by `ImportClosedNode` in
  `48ef3ee6`). In `Scan`, `$$` returns at `:1488`, so `width` is 1 for `$`, and only `\(` is admitted, so `display` is
  always false. Dead as a result: slots 5 and 7 and index flags 2 (`\]`) and 8 (`$$`), the `L"$$"` and `L"\\]"` closer
  choices, and the `display ? text_.size() : ...` fallback.
- **Fix**: delete them; this shrinks the 9-slot `htmlNext_` row (36 bytes per text unit per leaf) to 7 slots.
- **Regression test**: none; existing math-probe and closed-root tests must keep passing.

### MD-16 — A leading BOM blocks block recognition everywhere except the front-matter fence

- **Severity** P3 (reviewer P3, verifier P3) · **Category** conformance · **Verdict** Confirmed · **Confidence**
  medium (intent unclear) · **Effort** S · **Introduced** pre-existing
- **Location**: `src/Controls/DxUi.MarkdownBlocks.h:663-668` (the only U+FEFF handling in Markdown code), `Line()` at
  `:1064`
- **What**: `Document` keeps an accepted BOM as U+FEFF (`docs/rich-text.md:817`), so `﻿# Title` parses as a
  paragraph starting with U+FEFF. The same applies to lists, fences, thematic breaks and tables at document start.
  cmark skips an initial BOM.
- **Fix**: in `Line()` for `first == 0`, start the block cursor after a leading U+FEFF and keep it in source ownership
  as prefix trivia, as `FrontMatterLine` already does.
- **Regression test**: `﻿# Title` yields an AtxHeading root; `﻿---\ntitle: a\n---\n` is still FrontMatter;
  `Source()` round-trips unchanged.

### MD-17 — YAML alias resolution is a reverse linear scan over anchors

- **Severity** P3 (reviewer P3, verifier P3) · **Category** perf · **Verdict** Confirmed · **Confidence** high ·
  **Effort** S
- **Location**: `src/Controls/DxUi.YamlMetadata.cpp:217-231`, `:149-160`
- **What**: O(aliases × anchors). An alias passes an existing index to `Add`, so `nodes.size()` does not grow and
  aliases are not counted against the 4,096-node cap. Within 64 Ki units this is about 1.6·10^7 short comparisons in one
  indivisible `IdentityUnit` step (tens of ms at worst).
- **Fix**: `std::unordered_map<std::string, std::size_t>` updated as anchors appear (latest definition wins, matching
  YAML anchor redefinition).
- **Regression test**: 2,000 anchors and 16,000 aliases validate with a linear work counter or within a generous time
  bound.

## Refuted

None. All 17 findings survived verification.

| ID | Claim | Why refuted (file:line) |
| --- | --- | --- |
| none | | |

## Area architecture and simplification proposals

1. **One block-start predicate.** Paragraph interruption, lazy continuation (`MarkdownBlocks.h:1143`), table
   continuation (`:1254`) and the source-region and front-matter gates each keep their own list of "this line starts a
   block", and they already differ (`FootnoteAt(cursor)` versus `FootnoteAt(cursor, true)`). MD-03 is the third
   construct missing from one list; earlier ones were the type-7 HTML and empty-item fixes in this window. A single
   `StartsBlock(Cursor, Context{paragraphOpen, lazy, profile})` makes new block kinds impossible to forget. Files:
   `src/Controls/DxUi.MarkdownBlocks.h`. Risk: low with the CommonMark and GFM corpora. Effort: M. Before production:
   no (the MD-03 fix suffices); after.
2. **Differential incremental-versus-fresh harness for `MarkdownParser`.** Closed-root and lexical reuse are
   correctness-critical caches, and `ClosedRootDifferential` only performs a structure-neutral edit. A seeded
   random-edit harness (insert or delete blank lines, `>`, `- `, `---`, `+++`, `$$`, fences, edits at offset 0 and EOF)
   comparing full publications with the existing `equal` lambda would have caught MD-02. Files:
   `Tests/Support/Support.Tests.MarkdownSourceCorrelation.h`. Risk: none (test only). Effort: M. Before production: yes.
3. **Per-leaf terminator facts for opaque inline tokens.** Four last-occurrence offsets per leaf fix MD-05 with less
   memory than building the 36-byte-per-unit `htmlNext_` row for every leaf containing `<`; fix MD-11 in the same
   change. Files: `src/Controls/DxUi.MarkdownInlines.h`. Risk: low. Effort: S. Before production: with MD-05.
4. **Deferred-decode invariant.** Make "every Text node created by `AutolinkSplice` is decoded exactly once before the
   region is released" explicit, and add a Debug assertion in `Compact` that no Text node has an empty value with a
   non-empty source. The assertion's premise (no legitimate empty Text node) was not verified; check it against the
   corpora first. Files: `src/Controls/DxUi.MarkdownInlines.h`. Risk: low. Effort: S. Before production: with MD-01.
5. **YAML-private staging and callbacks.** `ValidateYamlMetadata` owns a YAML-specific UTF-8 conversion that
   normalizes CR and CRLF, separate from the byte-exact TOML conversion, and installs the DxUi-private global rapidyaml
   callbacks (MD-04). `MetadataUtf8`'s contract that the published source and accessibility text are never rewritten
   stays true, because only private staging changes. Files: `src/Controls/DxUi.YamlMetadata.cpp`,
   `src/Controls/DxUi.MarkdownMetadata.cpp`. Risk: low. Effort: S-M. Before production: yes.

## Test-quality observations

- `ClosedRootDifferential` (`Tests/Support/Support.Tests.MarkdownSourceCorrelation.h:213-319`) edits only `TARGET` to
  `TARGET!` and skips suffix mode for FrontMatter instead of asserting no reuse, so MD-02 cannot fail it.
- `FrontMatterTests::CheckSyntax` (`Tests/Support/Support.Tests.MarkdownBlocks.h:234-262`) counts only
  `UnclosedFrontMatter` diagnostics. The "TOML BOM and CR lines" scenario (`:220`) silently produces
  `InvalidTomlFrontMatter`, and any spurious YAML or TOML validation diagnostic in the 22 scenarios goes unnoticed.
  Assert the full diagnostic set per scenario.
- The autolink capacity tests (`emails`, `invalidHosts`, `links`) never place two accepted links in different sibling
  nodes, which is MD-01's trigger and a very common sentence shape.
- The math-probe linearity tests use small counts (300-800) and a ≤ 3× per doubling ratio, which detects quadratic
  growth only once the quadratic term dominates; the opaque-token-terminated pattern (MD-08) is absent.
- `Tests/Support/Support.Tests.MarkdownCapacity.h` has no pathological cases for HTML comment, PI, CDATA and
  declaration openers (MD-05), deep link-destination parentheses (MD-06) or `[`-led paragraphs with delimiter-like
  lines (MD-07).
- `Tools/tests/Test-MarkdownData.ps1` "Unicode 17 first-strong tables" re-implements the generator's algorithm (same
  regular expressions and precedence) as its oracle; only four hand-picked scalars are independent checks.
- The metadata corpus test treats any outcome other than None and Capacity as a correct "Invalid"
  (`Tests/Controls/Controls.Tests.RichMarkdown.cpp:1005`). Combined with CRLF-only inputs (MD-14), the LF path rests on
  about 20 hand-written cases and the CR path on none (MD-04).
- `TestYamlFrontMatterValidationAndSourcePreservation` pins the recursive-key Capacity mislabel (MD-12).
- The lazy-continuation and table scenarios (`MarkdownBlockTests::scenarios`, `splitTables`) have no RichMarkdown block
  (`$$`, `[^n]:`) after an unmatched container or a table (MD-03).

## Questions for the developer

1. Is a line containing only `$$x$$` intentionally literal? GitHub and Obsidian render it as display math; the docs say
   "double-dollar text embedded in a sentence is literal", which does not clearly cover it. Recommended answer: either
   accept a single-line `$$x$$` alone on its line as display math or state explicitly in `docs/rich-text.md` that it is
   literal. The premise holds (`MarkdownBlocks.h:1367-1368` requires nothing after `$$`; inline `$$` is literal).
2. RichMarkdown `~x~` is subscript, while github.com renders a single tilde as strikethrough. Is that divergence
   intended for consumers importing GitHub-authored notes? Recommended answer: keep subscript but document the
   divergence, and let consumers drop `Subscript` from the extension set when importing GitHub notes.
3. Should a leading U+FEFF be ignored for all block recognition, or only for front matter? Recommended answer: for all
   block recognition, keeping it as source-owned prefix trivia (MD-16).
4. `StartTable` extracts reference definitions from an open paragraph before the paragraph closes. In Gfm029 the input
   below loses its multi-line title: when the `-|-` line is probed, `ReferenceAt` falls back to a definition without a
   title (`MarkdownBlocks.h:781-825`), and the rest stays paragraph text. Has this been compared with cmark-gfm
   0.29.0.gfm.13? CommonMark is unaffected because Tables is off. Recommended answer: compare with the reference; the
   MD-07 fix (cheap checks before `References`) removes the early extraction in this shape. The DxUi half was verified;
   the cmark-gfm half was not.

   ```
   [foo]: /url
   "ti
   -|-
   tle"
   ```

5. Should a recursive alias key be Invalid rather than Capacity? Recommended answer: neither; use a distinct
   "unsupported key" code (false premise in the Invalid option — see verifier: recursive anchors are well-formed YAML).
6. Was the CRLF YAML corpus intentional (the README mentions a "Windows CRLF checkout")? Recommended answer: no;
   regenerate with LF bytes and run LF, CRLF and CR forms (MD-14).
7. Out of area but related: `RichTextEditor` catches only `std::bad_alloc` and `std::exception` around the public
   `CodeTokenizer` callback (`src/Controls/DxUi.RichTextEditor.cpp:6840-6854`). A non-`std` exception escapes the
   preparation path with `codeCallbackActive` still set. Is the callback contract "must not throw"?
   `include/DxUi/RichCodeTokenizer.h` does not say so. Recommended answer: document the contract in the header and
   restore `codeCallbackActive` with a scope guard regardless of how the call exits. The premise was verified.

## Coverage

| File | Depth |
| --- | --- |
| `src/Controls/DxUi.MarkdownInlines.h` | full (read end to end; every window hunk) |
| `src/Controls/DxUi.MarkdownBlocks.h` | full |
| `src/Controls/DxUi.MarkdownSyntax.cpp` | diff plus touched functions (`Preparation`, `ScanBlockUnit`, `ClosedRootUnit`, `ScanLexicalUnit`, `SetPrevious`, `IdentityUnit`, `TryAdoptParsedSnapshot`, `BeginSourceRegion`, `Advance`, `CharacterReference`, `ValidateSource`); accessor bodies around lines 200-300 skimmed |
| `include/DxUi/MarkdownSyntax.h` | full |
| `src/Controls/DxUi.MarkdownUnicodeData.h` (generated) | data shape and lookups (`Contains`, `kFold` use in labels); rows not reviewed |
| `src/Controls/DxUi.MarkdownEntityData.h` (generated) | unchanged in window; lookup (`CharacterReference` `lower_bound`, 33-unit bound) reviewed; rows not reviewed |
| `Tools/Generate-MarkdownUnicodeData.ps1` | diff in full (bidi defaults and explicit overrides) plus the `@missing` input |
| `src/Controls/DxUi.MarkdownLabels.h` | full |
| `src/Controls/DxUi.MarkdownMetadata.cpp`, `src/Controls/DxUi.MarkdownMetadata.h` | full (vendored toml++ patch strings spot-checked) |
| `src/Controls/DxUi.YamlMetadata.cpp` | full (rapidyaml line splitting, `RYML_NOEXCEPT`, allocator, stack and global-callback paths checked by the verifier) |
| `src/Controls/DxUi.RichMarkdownProjection.cpp` | diff plus touched functions (`Builder`, `FillProjection`, `ProjectBlock`, `ProjectRichSourceWindow` and its caller in `RichSourceWindow.h`) |
| `include/DxUi/RichMarkdown.h` | full |
| `include/DxUi/RichCodeTokenizer.h` | full (header only; implementation out of scope) |
| `Tools/Generate-MetadataCorpus.ps1` | full (archive bytes inspected) |
| `src/Controls/DxUi.MarkdownTableCells.h`, `src/Controls/DxUi.MarkdownSourceCorrelation.h` | full (unchanged data-flow dependencies) |
| `Tests/Support/Support.Tests.MarkdownBlocks.h` | Markdown parts of the diff; Mermaid and flowchart parts skipped (diagram area) |
| `Tests/Support/Support.Tests.MarkdownSourceCorrelation.h` | diff in full |
| `Tests/Support/Support.Tests.MarkdownCapacity.h` | full |
| `Tests/Controls/Controls.Tests.MetadataCorpus.h` (generated) | structure and line-ending census of all YAML cases; rows not individually reviewed |
| `Tests/Controls/Controls.Tests.RichMarkdown.cpp` | diff (front matter, YAML and TOML, corpus driver, inline extensions, wiki links) |
| `Tools/tests/Test-MarkdownData.ps1` | diff in full |
| `Tests/Support/Support.Tests.RichSemantic.h` | math-probe tests only (lines 440-550), for MD-08 |

### Checked and holds

The reviewer and verifier looked for each of these and found them sound:

- **Lexical prefix and suffix reuse** (`SetPrevious`, `ScanLexicalUnit`): the prefix margin covers the 33-unit entity
  lookahead; the suffix copy begins only when the new scanner is exactly at an old token start, and lexing from a token
  boundary is context-free.
- **Closed-root prefix reuse**: a document-owned blank line after the root guarantees full closure; lists, footnotes and
  indented code absorb their trailing blanks and are never prefix-reused; reference-definition subtrees are rejected.
  Only suffix reuse is affected by MD-02.
- **Autolink facts tables** (host, email, dot, underscore) match the scans they replaced, surrogates included; rejected
  angle autolinks stop at `<` or space and are linear.
- **Escaped table pipes**: source ownership (`Range` +2 and `offsets_` at the backslash) is consistent with
  `MarkdownTableCells` and `NormalizedOffset`.
- **Emphasis rule of three** uses original run lengths (matches cmark and commonmark.js); the `~`, `^`, `=`, `+` floors
  are separated by the length-mod-3 category, which is exact for their admitted lengths.
- **Inline capacity**: every `nodes_.emplace_back` in inlines and the `References` splice checks `MaximumNodeRecords`
  without underflow; failure throws `length_error` and keeps the previous publication.
- **YAML bounds**: alias bombs as keys are bounded by visits, bytes and depth; values are never expanded; handler-local
  allocation failures have no `RYML_NOEXCEPT` frames on their path; `MetadataUtf8`'s `source[++index]` is safe because
  content is line-bounded and admission rejects unpaired surrogates. Correction: the global-callback paths in MD-04 were
  missed by this check.
- **TOML** depth and key patch strings exist in the vendored `toml.hpp` (lines 15001 and 15534-15536).
- **`DecodeHeadingFragment`** percent-decoding bounds and NUL rejection are correct.
