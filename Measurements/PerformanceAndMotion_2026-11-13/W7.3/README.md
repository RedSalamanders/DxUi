# W7.3: position-independent chunks and viewport-first preparation

Receipt for plan item `W7.3` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Numbers come from the `FX8` receipts in [FX8](../FX8/README.md) (embedded host, WARP, x64 Release, counters only; no
timing). Each step is one action with every owner turn of preparation it causes.

- **Before**: the `FX8` receipts committed before this item (source `8630870b4`, clean tree, recorded 2026-11-14). The v0
  document layout they measure (`src/Controls/DxUi.RichDocumentLayout.cpp`, `src/Controls/DxUi.RichTextLayout.cpp`) was
  unchanged from that source to this item's base.
- **After**: the six regenerated receipts in [FX8](../FX8/), measured on a clean tree at commit `c10c2244c` (this item
  rebased on the integration branch at `02063cb85`; the final commit adds only these receipts and this README to that
  tree), x64 Release, WARP 10.0.26100.9818 (as each receipt records), command
  `performance-fixtures.ps1 -Fixture FX8 -Adapter Warp -FixtureHost Embedded -Configuration Release -Platform x64 -CountDrawCalls -SkipBuild -Replace`.
  Against the receipts first taken for this item (at `2a98cfa66`), of the work counters only owner turns changed, each by one turn fewer: the suite-sized
  `source` warm step (3 to 2) and `multiplier` warm (12 to 11) and resize steps (3 to 2); the advance that publishes the
  viewport now goes on laying within its budget. Every full-size counter in the tables below is unchanged.
- **Scenario**: the `FX8` definition (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx8.h`). `source*` lays the
  document out in Source mode; `multiplier*` in Rich mode without Markdown rendering at line height 1.5. `default` and
  `full` use semantic geometry, which lays no v0 chunk.

## Chunks laid per step (`rich.chunksLaid`, min..max) and whole layouts (`rich.wholeLayoutsLaid`)

| Variant | Phase | Before chunks | After chunks | Before whole | After whole |
| --- | --- | --- | --- | --- | --- |
| `source-full` | insert (prose, fence, table) | 1..2 | 1 | 0 | 0 |
| `source-full` | Enter | 62..64 | 1 | 0 | 0 |
| `source-full` | arrow | 0 | 0 | 0 | 0 |
| `source-full` | resize step | 61 | 61 | 0 | 0 |
| `source` | insert | 1..2 | 1 | 0 | 0 |
| `source` | Enter | 8..10 | 1 | 0 | 0 |
| `source` | resize step | 8 | 7 | 0 | 0 |
| `multiplier-full` | insert | 0 | 1 | 1 | 0 |
| `multiplier-full` | Enter | 0 | 1 | 1 | 0 |
| `multiplier-full` | resize step | 0 | 61 | 1 | 0 |
| `multiplier` | insert / Enter | 0 | 1 | 1 | 0 |
| `default`, `full` | every phase | 0 | 0 | 0 | 0 |

## Viewport first (after; the counters are new)

| Variant | Phase | `rich.viewportPublications` per step | `rich.offscreenChunksLaidFirst` per step |
| --- | --- | --- | --- |
| `source-full`, `multiplier-full` | warm (first load) | 1 (the first step: the viewport's chunks, placeholders for the rest) | 0 |
| `source-full`, `multiplier-full` | insert, Enter, arrow, seeks | 0 (only the edited chunk is laid, and it meets the viewport) | 0 |
| `source-full`, `multiplier-full` | resize step | 1 (the viewport's chunks published, the other chunks after) | 0 |

Live resizes (several width steps inside one viewport-first window) and edits during that window are not in `FX8`; the
`TestRichDocumentLayoutViewportFirstAcrossWidthSteps` and `TestRichDocumentLayoutEditsDuringViewportFirst` tests count
them: every width step publishes its viewport after at most 8 chunks with 0 off-screen chunks, and an insert, an Enter
and a 308-unit paste each lay 1 chunk before their publication.

## Other per-step counters, full size

| Variant | Phase | Before C++ allocations | After | Before owner turns | After |
| --- | --- | --- | --- | --- | --- |
| `source-full` | prose insert | 13,797..13,841 | 8,693..8,700 | 1 | 1 |
| `source-full` | Enter | 18,947..18,963 | 8,659..8,708 | 16 | 1 |
| `source-full` | resize step | 5,224..5,264 | 5,106..5,147 | 16 | 17 |
| `multiplier-full` | prose insert | 13,867..13,872 | 8,701..8,706 | 56 | 11 |
| `multiplier-full` | Enter | 13,846..13,874 | 8,665..8,713 | 56 | 10..11 |
| `multiplier-full` | resize step | 176 | 5,394..5,435 | 1 | 17 |

The `multiplier-full` width step is more expensive than before: one whole layout per step (176 allocations, 1 turn)
became 61 chunk layouts (about 5,400 allocations, 17 turns, the viewport published after the first). A width change
cannot reuse a chunk until `W6.1` reuses width-independent chunks.

## Remaining document-wide work per preparation

`BeginPreparation` walks every block's style runs once per preparation to find the document's largest run size and
script flag (the inputs of uniform line spacing, which every chunk must share to match one whole layout). It allocates
nothing and lays nothing, and has no counter: on `FX8` it is part of the indivisible projection step of each preparation
(one owner turn), whose per-insert allocation total above is unchanged by it. Its cost is proportional to the number of
projection blocks and runs (a few thousand on the full-size document), not to their text. Keeping that maximum per block
and combining it incrementally would remove it; it is not done in this item.
