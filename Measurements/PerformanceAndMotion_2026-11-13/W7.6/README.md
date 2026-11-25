# W7.6: code blocks

Receipt for plan item `W7.6` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Counters only; no timing. Embedded host, WARP, x64 Debug (the counters do not depend on the configuration; Release and
ASan Debug agree on the gate tests).

- **Before**: the parent of this item's change, integration tip `aaad7e45e`, with the new counters (`rich.codeNumberLayouts`,
  `rich.codeBadgeLayouts`, `rich.caretHitTests`) added to its code and nothing else, in the three places that create a
  number layout, create a badge layout and read a caret box. Command:
  `performance-fixtures.ps1 -Fixture FX8 -Variant default|full -FixtureHost Embedded -Configuration Debug`.
- **After**: this item's branch on `aaad7e45e`, same command. `FX8` is the unchanged definition
  (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx8.h`): `default` has a 250-line numbered fence, `full` the plan's
  2,000-line one. The existing `FX8` receipts are not regenerated here.
- **After W7.7**: on the integration tip `665551410` (W7.7's changed `FX8` document and two more phases, `seek-formula` and
  `formula-insert`) the gate runs again: warm 250 / 2,000 number layouts and 1 badge layout, 0 number and 0 badge layouts
  in every phase except the width step (1 badge), `formula-insert` included. Only the document-level caret reads moved
  (table below), because the text shifted. The before/after tables above were measured before W7.7.
- **Scenario**: one-character prose inserts (40), Enter (10), arrow keys (50), inserts inside the fence (30) and next to
  the table (30), and a 30-step width resize; each step is one action with every owner turn of its preparation.

## Number and badge layouts created per step (maximum over the steps of the phase; total over the phase)

| Variant | Phase | Before numbers | After numbers | Before badges | After badges |
| --- | --- | --- | --- | --- | --- |
| `full` | warm (4 steps) | 2,000 (4,000) | 2,000 (2,000) | 1 (2) | 1 (1) |
| `full` | prose insert | 2,000 (80,000 over 40) | 0 | 1 | 0 |
| `full` | Enter | 2,000 (20,000 over 10) | 0 | 1 | 0 |
| `full` | arrow | 0 | 0 | 0 | 0 |
| `full` | insert in the fence | 2,000 (60,000 over 30) | 0 | 1 | 0 |
| `full` | insert next to the table | 2,000 (60,000 over 30) | 0 | 1 | 0 |
| `full` | width step | 2,000 (60,000 over 30) | 0 | 1 | 1 (30 over 30) |
| `default` | prose insert | 250 (10,000) | 0 | 1 | 0 |
| `default` | Enter | 250 (2,500) | 0 | 1 | 0 |
| `default` | insert in the fence | 250 (7,500) | 0 | 1 | 0 |
| `default` | insert next to the table | 250 (7,500) | 0 | 1 | 0 |
| `default` | width step | 250 (7,500) | 0 | 1 | 1 (30 over 30) |

A character typed in the fence adds no line, so it adds no new distinct number: 0 is "at most the new distinct numbers".
A line added in a fence builds exactly the numbers no fence has yet (`TestRichCodeBlockLayoutsAreSharedAcrossEdits`: a line
added to a short fence builds 0, to the longest builds 1). A width step builds one badge per language because the badge's
layout width follows the document's; no number layout depends on the width (their gutter is fixed by the digit count).

Other counters of the same runs, `full`, per step (maximum): text layouts created (`resources.textLayout.created`) in a prose
insert 3,260 before, 1,259 after (the 2,000 number layouts and the badge, and nothing else changed); Enter 3,268 and 1,267;
insert in the fence 3,269 and 1,268; width step 4,526 and 2,526. C++ allocations in a prose insert 314,580 and 205,276;
bytes copied out of other `RichText::Document`s per step (`rich.otherDocumentBytesCopied`, one document of a few units per
number layout) 27,584 and 0 (12 in a width step: the one badge). On `default`: text layouts in a prose insert 416 and 165,
allocations 39,001 and 25,761.

## Caret reads per visible-range query (`rich.caretHitTests`)

`PreparedLayout::TryVisibleRange`, the old line-by-line scan (the test reference) against the search, over the same
queries (every swept line boundary and one single-precision unit either side, with windows of 0, one unit, one line and
two and a half lines; `Controls.Tests.RichCodeBlocks.cpp`, Debug):

| Layout | Lines | Queries | Search max / mean | Old scan max / mean |
| --- | --- | --- | --- | --- |
| wrapped Latin | 482 | 23,281 | 6 / 4 | 482 / 242 |
| wrapped Latin, multiplier 1.65 | 482 | 23,281 | 4 / 3 | 482 / 242 |
| empty lines | 11 | 673 | 10 / 7 | 11 / 6 |
| 65,536 newlines | 65,538 | 721 | 16 / 8 | 65,538 / 33,322 |
| 65,536 units, multiplier 1.65 | 49,153 | 817 | 20 / 8 | 49,153 / 25,589 |
| 131,072 semantic units | 131,074 | 721 | 26 / 13 | 131,074 / 64,851 |
| Hebrew or mixed bidi with a multiplier (not decided stacked: stepped) | 162 to 242 | 7,921 to 11,761 | 162 / 82 to 242 / 122 | the same |

The plan's "0 `HitTestTextPosition` calls per visible-range query" is not met, and cannot be: DirectWrite's caret tops are
not the sums of the line heights (below). Layouts that are not decided to have stacked caret boxes (the gate in
`PreparedLayout::TryVisibleRange`'s comment: one style, no inline object or script run, and in uniform spacing printable
ASCII only) read as many boxes as before.

The document-level `PreparedDocumentLayout::TryVisibleRange` (W7.3's rule, unchanged: a line is visible when its placed caret
box meets the window) searches each chunk meeting the window whose caret boxes are stacked. On `FX8`'s full text (65,280
units, 61 chunks, a 720 DIP window at 24 scroll positions; `Embedded.Tests.CodeBlocksFx8.h`), caret reads per query, search
against stepping: Source mode max 34 / mean 9 against 178 / 90; Rich mode with a line-height multiplier of 1.5 max 24 /
mean 8 against 158 / 70, with equal answers. On suite-sized chunked documents of 10 chunks (15,301 queries around every
line boundary): max 56 / mean 4 against 482 / 29; a document of mixed run sizes is stepped and reads the same as before.

## Why prefix sums of line heights cannot be the answer (counter-example)

1,201 wrapped lines at 173.3 DIP, 13.7 DIP font (`TestRichCodeBlockLineHeightSumsAreNotCaretTops`):
- natural spacing: the caret top differs from the float sum of the line metrics on 231 lines; first at line 1, caret
  18.2220688 against sum 18.2220707 (line height 18.2220707, caret height equal on every line);
- line-height multiplier 1.65: the caret box is the glyph box inside the line: height 18.2220707 against a line height of
  22.6049995 on all 1,201 lines, and its top 0.628808975 on line 0 against a sum of 0.

## Stacked caret boxes: what was tried to break the search

The search needs a box's top and bottom not to decrease from one line to the next. Preparation decides that only for one
style (no inline object or script run) and, in uniform spacing, printable ASCII. A multiplier below 1 does not exist
(preparation refuses it); at 1.0 and 1.15 the glyph box (about 1.33 em in Segoe UI) is taller than the line. With the
search forced by a test switch (`testRichForceVisibleRangeSearch`), against the stepping reference, on layouts the gate
steps: mixed run sizes (a heading line after a body line and the reverse) at multipliers 0, 1.0, 1.15 and 1.65: 0 differences
in 7,057 queries each; fallback fonts (CJK, emoji and Hebrew first glyphs among ASCII lines) at 1.0, 1.15 and 1.65:
0 differences in 5,185 queries each. No fixture broke the search, so the gate is the argument in the code comment and not a failing case; the
gate costs the paragraph layouts of a semantic document (they have bold, links, code runs) their search, and keeps it for
code fences, plain Source text and chunks.

## Turns when every number is a cache hit

A hit spends no layout of a turn's budget. What bounds a turn is `while (result.blockSteps < budget.maximumBlockSteps)` at
the head of `PreparedSemanticGeometry::Advance`'s loop, and a number placed (hit or built) ends its iteration in
`++result.blockSteps`. `TestRichCodeBlockTurnsAreBoundedWhenEveryNumberHits`, a 2,000-line fence prepared again with every
number a hit: 12,949 block steps in all (the whole document's phases), 827 owner turns at 16 steps a turn, 220 at 64 and 69 at
256; at most 16, 64 and 256 steps in a turn; at most 1 layout in a turn (a paragraph), 0 number or badge layouts built. Turns
that pass between boxes without placing a number are bounded by the box count and were so before.

## Part 4: the clip of a number stays

Not delivered, and no skip code ships. The rule that decided "fits" from the ink (glyph overhang included, widened by
2.25 device pixels) was exact in isolation: over 96, 120, 144, 150.5 and 192 dpi, five transforms (identity, fractional
translation, two scales, a mirror), fractional positions and clip margins from -1 to 4 pixels, all 94 clips it let go drew
the pixels the clip draws, and 76 clips narrower than its margin changed pixels; the geometry's own draw with the clips left
out was equal at those dpis under translation, scale and rotation. The incremental-versus-fresh oracle on the native host
disagreed: script `richCodeBlocks`, seed `0x5EED0111`, step 0 (the initial frame): with the clips left out, 592 BGRA
pixels in row y = 142, x = 20 to 611, differ by one level (217 against 218) from the frame of the same preparation drawn
again with every number clipped (the editor's frame of a fence, a horizontal edge). With every number clipped, both seeds
pass all 28 steps. Isolated: the only difference is the clip push; the differing row is not part of anything a number draws;
the isolated draws of the geometry (opaque or translucent brushes, 90 widths and sizes) are equal. Not isolated: which
Direct2D behaviour changes the row; the editor's draw (selection and decoration passes, layers) differs from the isolated
draws in a way I did not find.

## Plan claims that were wrong or loose

- "Prefix sums of line heights from `LineMetrics` and a binary search": the sums are not the caret tops; they only seed a
  search over real caret boxes.
- "Skip the clip when a layout fits its gutter": the number's clip is its fence's frame (`box.frame`), not the gutter.
- "Badge layouts keyed by language": the layout's width follows the document's, so the key has the width.
- The callers the item names in Source mode (`DxUi.RichTextEditor.cpp`) reach the document-level function, which W7.3
  owns; it reads caret boxes per chunk and is searched per stacked chunk since round 2.
- `HitTestTextPosition` per query is never 0 without a source of the exact caret tops.
