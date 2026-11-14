# W7.9 Hover and hit index: element scans and title resolutions per pointer move and tick

Item: `W7.9` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: [UI_ControlsAndLayout.md](../../../Specs/UI/UI_ControlsAndLayout.md), rich title tooltips and hover lookup cost.

## Scenario

`RichHoverIndexTests::HoverGate` (`Tests/Support/Support.Tests.RichHoverIndex.h`), run on the native host by
`TestRichHoverIndexNativeGate` (suite `EditorControls`) and on the embedded host by `TestEmbeddedRichHoverIndex`
(Embedded suite; alone with `DxUi.EmbeddedTests.exe --test-rich-hover-index`). The document has 60 sections, each a
paragraph with plain text, a titled link, an abbreviation, a titled image and a footnote reference, plus a footnote
whose body holds another titled link: 601 semantic elements. The editor is read-only (titles need no Ctrl) with
`HoverOrCaret` link underlining, so a pointer move resolves both the underlined link and the hover title. The scenario
stands in for the plan's `FX14`-over-`FX8` fixtures, which a later item creates.

- Over a link: the pointer enters the first link, then makes 8 further moves inside it.
- Unchanged pointer: 10 animation ticks (native `DebugAnimationTickForTest`; embedded `AdvanceAnimation`, the path
  that revalidates the supplemental tooltip).
- Over plain text: the pointer enters the first paragraph's plain words, then makes 8 further moves inside them.

Counters: `rich.hoverScanElements` and `rich.hoveredTitleCalls` (`src/Support/WorkCounters.h`); invalidations are the
native host's invalidate count and the embedded view's `NeedsPreparation`. The "before" run is base `41398711` with
only the two counters and the tests added (the scans counted where they ran); the "after" run is this item's commit.

## Result (x64 Debug; counts do not depend on timing or adapter; embedded on WARP)

| Measure | Before, native | Before, embedded | After, both hosts |
| --- | --- | --- | --- |
| Element scans per pointer move over a link | 1,205 | 1,208 | 0 |
| `HoveredTitle` calls per pointer move over a link | 2 | 2 | 1 |
| `HoveredTitle` calls per tick, unchanged pointer | 2 | 2 | 0 |
| Element scans per tick, unchanged pointer | 1,202 | 1,202 | 0 |
| Invalidations per move over plain text | 0 | 0 | 0 |
| Element scans per move over plain text | 1,202 | 1,803 | 0 |

Gate: over a link, element scans per pointer move 0; `HoveredTitle` calls per tick with an unchanged pointer 0;
invalidations per move over plain text 0. Every row meets it. Plain-text moves already invalidated nothing on the base;
the test keeps that as a guard.

Correctness: `TestRichHoverIndexMatchesFullScan` compares the index with the former full scans at every display offset
of 48 prepared seeded documents (37,060 offsets; 1,822 titled spans, 180 nested, 118 empty, 24 expanded source owners;
nested links in footnotes, adjacent, wrapped and right-to-left spans) and the index alone with brute force over random
overlapping spans. `HoverStaleness` (both hosts) shows that a document edit, scroll, zoom, width change, density theme
and re-preparation under an unmoved pointer each make the host resolve again and agree with a resolution from nothing;
a colour-only theme keeps the remembered title. Removing the scroll offset from the hover key makes the scroll case
fail (checked once by mutation).
