- The Grid's bounded multiline cells are verified by 16 new tests in the Grid, Rendering, Accessibility and Embedded suites
  (plan `GridTextOverflow_2026-09-21`, the verification item), each comparing a scenario with a fresh twin painted on the
  same device and never with stored pixels. Ctrl+C and `OnCopy` of trimmed cells holding CR LF, U+2028 and U+2029, a
  zero-width-joiner emoji, a 5,000-unit word, decomposed accents and Arabic are exact across rows, columns and reordered
  columns; UI Automation's Name, Value and ValuePattern carry the same values, a 100,000-unit one included, and the bounding
  rectangle of a cell the viewport cuts is the clipped cell, at five scroll offsets and under a dragged scrollbar thumb.
  Decomposed accents paint the pixels of precomposed ones. A 100,000-unit value whose shaped prefix ends inside a surrogate
  pair, a zero-width-joiner sequence or a letter and its marks paints like a short twin. In a right-to-left flow the marker
  stays on the side its text reads to and each cell's ink in its cell. A cell narrower than a word keeps its ink inside, its
  ellipsis and its tooltip at every clamp. A cell the viewport cuts paints a shifted crop of its whole self at whole-row,
  scrollbar-dragged fractional vertical and fractional horizontal offsets. A new dpi, theme (light, dark, high contrast), font
  or density repaints like a fresh attach. Values that share a start but lay out differently never share a layout. The layout
  tables hold at a lowered ceiling and paint right after eviction, and the real tables stop at 16,384 entries, evict a least
  recently used way and halve when use drops. Device loss, a move between hosts and, on WARP in an embedded view, long French
  cells (marker, hide and show, device replacement, dpi) reproduce a fresh grid. Every test fails under at least one temporary
  mutation of the library (27 mutants, 33 pairs of a mutant and a test; the archived table names the assertion that failed,
  and the one pair that passes is an eviction order the pixel test cannot see, which the Grid test catches). Nothing in the
  library was wrong: the only library change is the diagnostics hook `Grid::DebugSetTextLayoutEntryLimit` (and
  `DebugGetTextLayoutEntryLimit`), which lowers the layout tables' ceiling for the test; the production ceiling is unchanged.
  One contract sentence was imprecise and is corrected: an entry the current or previous paint used is not evicted while a
  table can grow, but at the ceiling a full set gives up its least recently used way, which the Grid test pins on a 64-entry
  table running the same code as at 16,384.
  The fixtures that move a control between hosts moved from `DxUiTests.EditorControls.cpp` to
  `Tests/Controls/DxUiTestMovedControls.h`, and the tests share `Tests/Controls/GridMultilineFixtures.h`. The Grid reads no
  `FlowDirection`, so a right-to-left grid is not mirrored and paints the pixels of a left-to-right one (a finding, logged by
  the right-to-left test, not a change). x64 Debug, Release and ASan Debug runs of the Grid, Rendering, Accessibility,
  Embedded and MultilineText suites are archived under `Measurements/GridTextOverflow/2026-09-30/verification`. The API
  revision does not change (additive diagnostics accessors; a private member added).
