- Code blocks (performance and motion plan W7.6): `PreparedLayout::TryVisibleRange` no longer reads a caret box for every
  line before the window where it can show that caret boxes are stacked (a box's top and bottom never decrease from line
  to line). Preparation decides that once per layout: one style, no inline object or script run, and in uniform spacing
  printable ASCII only. Such a layout starts from the sums of its line heights (a hint only: DirectWrite's caret tops are
  not those sums) and finds the first visible line and the first line at or below the window by doubling steps and halving
  over the real caret boxes; any other layout steps from the first line as before. Answers are the stepping scan's at every
  swept boundary (wrapped Latin at multipliers 0, 1.0, 1.15 and 1.65, right-to-left, mixed bidirectional, mixed run sizes,
  fallback fonts, empty lines, trailing newline, layouts of 65,536 and 131,072 units): at most 6 caret reads on layouts of up
  to 500 lines, 16 to 26 on the largest. `PreparedDocumentLayout::TryVisibleRange` does the same per chunk (on FX8's full text,
  at most 34 caret reads against 178 stepping). The plan's "0 `HitTestTextPosition` calls" is not met. The visible-range
  queries of the semantic geometry go through the paint index instead of visiting every box, with identical answers. The line
  numbers and language badges of numbered code fences are layouts shared across fences and edits through a bounded cache
  (4,096 numbers and 256 badges, registered as `RichTextEditor.codeLayoutCache`; an immutable layout shared by the published
  geometry and the one in preparation; the cache holds its one factory and drops everything when it changes): on `FX8` a prose
  insert, Enter, arrow key or character typed in the fence builds none (it built one layout per numbered line, 2,000 on the
  full document, each with a whole `RichText::Document`), a width step builds at most one badge per language, and a changed
  font, size, DPI, gutter width, reading direction or locale builds new layouts, never a stale one. A cache hit spends no
  layout of a turn's budget; a turn stays bounded by its block steps. New counters `caretHitTests`, `codeNumberLayouts` and
  `codeBadgeLayouts`. Not delivered: skipping the clip of a number that fits its gutter (the oracle finds the editor's frame
  one level different in one fence-edge row with the clips left out); every number is still clipped and no skip code ships.
  Validation: `Controls.Tests.RichCodeBlocks.cpp`, the `richCodeBlocks` incremental-versus-fresh script on both hosts,
  `Embedded.Tests.CodeBlocksFx8.h`; receipt `Measurements/PerformanceAndMotion_2026-11-13/W7.6`.
