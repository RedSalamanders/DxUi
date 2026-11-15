# W7.2 Incremental parse reuse — Markdown parse work per one-character insert

Plan item W7.2 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior
in [controls and layout](../../../Specs/UI/UI_ControlsAndLayout.md) ("Incremental parse reuse") and the bounds in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md).

- **Scenario** (`Tests/Embedded/Embedded.Tests.MarkdownParseReuse.h`, `TestFx8ParserInsertKinds`): a `MarkdownParser`
  (RichMarkdown profile) fed tracked `SourceSnapshot`s of the FX8 text (`PerformanceFixtures::Fx8::BuildSource`) at both
  scales: `default` (7,936 units) and `full` (65,280 units, the text of the `source-full` and `multiplier-full`
  variants). Per insert kind, 40 one-character inserts at one site, each followed by a complete parse, measured one by
  one: prose, fenced block and next to the table are the FX8 sites; table cell is inside row N/2; list item and next to
  front matter edit the FX8 text with a YAML front matter in front and a list (with a lazy line) before the first math
  block, since FX8 has neither. The bound is the source span from the start of the top-level block enclosing the insert
  to the end of the block after it. The fixture itself (`TestFx8ParseWorkPerInsert`) runs FX8 `default` and `source`
  through the editor and checks every measured insert step against the same bound.
- **Counts**: work counters `rich.markdownBlockUnitsScanned`, `rich.markdownLexicalUnitsScanned`,
  `rich.markdownInlineUnitsScanned` (source units each scan stepped over), `rich.markdownInlineLeavesParsed` and
  `...Copied`, `rich.markdownNodesCopied`, `rich.identityMapNodes`, `rich.markdownFrontMatterValidations`,
  `rich.markdownSourceUnitsCopied` and `...Materialized` (units read out of the piece tree), and the Embedded suite's
  C++ allocation counter (calls and requested bytes) around each parse.
- **Commands**: `DxUi.EmbeddedTests.exe --test-markdown-parse-reuse` (the Embedded suite runs the same two tests);
  `DxUi.ControlTests.exe --suite=EditorControls --test=TestMarkdownParseReuseOracleFuzz` and its two neighbours.
- **Source**: "before" is base `714e1f77e` plus the counters and the measuring tests only (`before-*.txt`); "after" is
  the W7.2 change (the commit carrying these files). **Build**: x64 Release, MSVC 19.52.37025. **Adapter**: none (parser
  only) for the per-kind receipt, WARP for the fixture run. **OS**: Windows 11 build 26300. No timing is recorded; the
  counts do not depend on the configuration.
- **Files**: [before](before-parser-insert-kinds.txt), [after](after-parser-insert-kinds-x64-release.txt) (both include
  the fixture run where it completed), [oracle before](before-oracle-fuzz.txt) (the oracle's first failure on the base:
  a defect of the existing closed-root reuse, fixed here), [oracle after](after-oracle-x64-release.txt).

Per insert, maximum source units scanned by block / lexical / inline passes, and inline leaves parsed (average), against
the bound (the gate is every scan at most the bound):

| Scale | Insert kind | Bound | Before: block / lexical / inline | After: block / lexical / inline | Leaves parsed before / after |
| --- | --- | --- | --- | --- | --- |
| default | prose | 275–314 | 172 / 313 / **5,770** | 135 / 133 / 134 | 162 / 1 |
| default | fenced block | 1,582–1,621 | 1,479 / 81 / **5,730** | 1,441 / 45 / 0 | 162 / 0 |
| default | next to the table | 180–219 | 168 / 153 / **5,770** | 131 / 129 / 130 | 162 / 1 |
| default | table cell | 924–963 | 910 / 117 / **5,770** | 873 / 70 / 398 | 162 / 130 |
| default | list item | 104–143 | **232** / 119 / **5,823** | 143 / 53 / 93 | 164 / 2 |
| default | next to front matter | 118–157 | 151 / 156 / **5,823** | 62 / 61 / 59 | 164 / 1 |
| full | prose | 275–314 | **391** / 313 / **48,420** | 135 / 133 / 134 | 1,256 / 1 |
| full | fenced block | 13,082–13,121 | **13,198** / 81 / **48,380** | 12,941 / 46 / 0 | 1,256 / 0 |
| full | next to the table | 180–219 | **387** / 155 / **48,420** | 131 / 129 / 130 | 1,256 / 1 |
| full | table cell | 6,615–6,654 | **6,820** / 121 / **48,420** | 6,564 / 72 / 3,114 | 1,256 / 1,005 |
| full | list item | 104–143 | **451** / 119 / **48,473** | 143 / 53 / 93 | 1,258 / 2 |
| full | next to front matter | 118–157 | **370** / 156 / **48,473** | 62 / 61 / 59 | 1,258 / 1 |

Bold marks a value over the bound. Through the editor (FX8 `default` and `source`, every measured step) the maxima after are:
prose 135 / 133 / 134 (bound 314), fence 1,431 / 35 / 0 (bound 1,611), next to the table 121 / 119 / 120 (bound 209).

Other per-insert work at the full scale, prose (before → after): YAML/TOML validations next to front matter 1 → 0;
identity-map nodes 1,465 → 2; nodes copied 1,463 → 5,405 (inline forests are now copied, shifted, instead of parsed);
source units materialized from the piece tree 65,300 → 1 (the owned source copy is still 65,300 units, composed from the
previous publication); C++ allocations 13,651 → 8,515, requested bytes 6.67 MB → 5.75 MB.

What remains O(document) per edit: every imported node, inline and lexeme is copied with its absolute range shifted, and
the root, compaction and identity passes visit every record once. The public `Nodes()`, `Inlines()`, `Lexemes()` and a
contiguous `Source()` require it; removing it is a public-type change (API revision 6, W7.1 part 2).
