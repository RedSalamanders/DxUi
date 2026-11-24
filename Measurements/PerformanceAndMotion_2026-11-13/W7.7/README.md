# W7.7: math and diagrams

Receipt for plan item `W7.7` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
The counters are the ones `Embedded.Tests.MathDiagramsFx8.h` prints on the `FX8` fixture (embedded host, WARP, default
variant); the pixel evidence is the gallery's RichTextEditor tiles on WARP. Counters only: no timing.

- **Scenario**: the `FX8` definition (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx8.h`) as this item extends it:
  the first formula carries an authored color (`\textcolor{red}`), the first diagram is styled (`style A fill:#204080,color:#ffffff`,
  `linkStyle 0 stroke:#c04040,stroke-width:3px` for a 3 px colored edge, `linkStyle 2 stroke-dasharray:5 5` for the dashed
  back edge, which bends), the document holds a second SVG picture, and two phases are added after `resize`:
  `seek-formula` (the caret moves into the first formula) and `formula-insert` (ten one-character inserts inside it, each
  of which prepares that formula again). Each step is one action with every owner turn of preparation it causes.
- **Before**: the base of this item (`db139338d`) with the four new work counters, the fixture additions and the test
  applied, x64 Debug, WARP, recorded on 24 November 2026: [gate-before-x64-debug.txt](gate-before-x64-debug.txt).
- **After**: the item on the integration tip `9c442ed25`, measured at `22a182cf2` (the tree this item's commit has, less
  these receipts and its changelog fragment's final wording), x64 Release, WARP: [gate-after-x64-release.txt](gate-after-x64-release.txt).
  The same test in x64 Debug prints the same counters. Command:
  `DxUi.EmbeddedTests.exe --test-math-diagrams` (also part of the Embedded suite and of `--test-performance-fixtures`).
- **Not regenerated**: the receipts under [FX8](../FX8/README.md), which are re-recorded once at the end for every fixture.

## Gate on `FX8`

| Clause | Phase | Before | After |
| --- | --- | --- | --- |
| math font reads per prose insert (`rich.mathFontReads`) | `prose-insert` (40 steps) | 0 | 0 |
| math font reads per formula edit | `formula-insert` (10 steps) | 10 (1 per step) | 0 |
| math font reads, warm-up (3 formulas) | `warm` | 1 | 1 (the one read the cache keeps) |
| relative-luminance evaluations (`rich.luminanceEvaluations`, up to three `std::pow` each) | `prose-insert` | 1,440 (36 per paint) | 0 |
| | `enter` (10), `arrow` (50), `seek-formula` | 210, 476, 36 | 0, 0, 0 |
| | `formula-insert` (10 steps, each re-prepares the colored formula) | 360 | 220 (22 per step: its one decision per page colors) |
| | `warm` | 74 | 38 |
| edge connector draw calls for edges drawn (`rich.edgeConnectorDraws` for `rich.edgesDrawn`) | `prose-insert` | 600 for 120 (`DrawLine` per segment) | 120 for 120 (one `DrawGeometry` per edge) |
| | `enter`, `arrow`, `table-insert`, `resize`, `seek-formula`, `formula-insert` | 60 for 12, 180 for 36, 300 for 60, 300 for 60, 15 for 3, 150 for 30 | 12 for 12, 36 for 36, 60 for 60, 60 for 60, 3 for 3, 30 for 30 |
| Direct2D device contexts created per preparation (`resources.deviceContext.created`, per step, 2 SVG pictures) | `warm` | 2 | 1 |
| | every other phase | 0 | 0 |

`fence-insert` paints no diagram (the caret is far below it): 0 everywhere, before and after. The plan's claim that a prose
insert rebuilds math font data per geometry generation is not true of `FX8` at the base: the formulas a previous generation
carries (W2.3) lend their data to the one that is prepared again, so a prose insert read no font before either. The font is read
again when the first formula of a generation is the one prepared again, which is what `formula-insert` measures. The clause
holds trivially for the prose insert and is proved by the formula edit.

## Pixels

The gallery's ten RichTextEditor tiles (math, colored math, flowchart edges, SVG picture, state and class diagrams, sequence,
and four document tiles), five themes at 100, 150 and 200% (150 tiles), WARP, x64 Release,
`gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles RichTextEditor -Warp`, re-captured at `d87e67983` (this item with its round-2 changes) for the numbers below. The colored math, flowchart edges and SVG
tiles are new in this item; the base was captured with the same three tiles added:

- **Parts 1, 2 and 4 change no pixel.** The base (`9c442ed25` with this item's gallery tiles) and this item with its
  connectors drawn one `DrawLine` per segment (a private seam, `DXUI_GALLERY_EDGE_CONNECTORS=1`, the old drawing) are
  identical in all 150 tiles, `gallery.ps1 -CompareBefore ... -CompareAfter` exit 0: the math and colored math tiles, the
  SVG tile, and every flowchart tile with its authored-color decisions, which now run once per page colors.
- **Part 3 changes edge pixels on purpose.** With production drawing (one path per edge) 30 of the 50 tiles at each scale are
  identical to the base; the four diagram tiles (flowchart edges, state and class diagrams, sequence, the composed document's
  one-edge graph) differ in every theme. [connector-pixels.json](connector-pixels.json) compares three captures (production,
  one `DrawLine` per segment, and no connector at all, `DXUI_GALLERY_EDGE_CONNECTORS=2`): every differing pixel lies on a pixel
  a connector paints (`differingOutsideConnectors` is 0 in all 60 rows). An edge of one segment changes 4 to 12 pixels of its
  tile (anti-aliasing at its ends); the flowchart edges tile changes 723 to 755 of 226,720 pixels at 100% and 2,290 to 2,391 of
  907,920 at 200%, where the dashed and 3 px edges bend.
- **Retained tiles** (the dashed and 3 px edges the gate names): `after-100-light-flowchart-edges.png`,
  `after-200-light-flowchart-edges.png`, `after-200-dark-flowchart-edges.png` and, for comparison,
  `before-200-light-flowchart-edges.png` (the base). A dashed edge's pattern now runs on across a bend instead of restarting
  at each segment, and a thick edge is joined at a bend with a bevel join (flat caps), whose ink lies inside the ink the segments draw apart (the round-2 capture below used it). [tiles-after.tsv](tiles-after.tsv) lists every captured tile's pixel
  digest.
- The incremental-versus-fresh oracle (the `richObjects` script and the `FX8` replay, both hosts) and the Embedded, EditorControls
  and Rendering suites pass in x64 Debug.
