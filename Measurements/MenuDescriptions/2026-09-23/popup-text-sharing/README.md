# Popup-local native text layout sharing

The candidate shares equal text/font-role/width layouts during described-menu
preparation. The lookup dies before paint. Commands, accessible names, selection
and row geometry stay independent. Absolute scrollbar widths prevent cumulative
narrowing when several rows share a layout. Distinct strings still have distinct
native layouts; this optimization does not promise savings for unique captions.

The retained pre-change production is `fb857d4`. Its v4 menu fixture is unchanged
from the previously measured implementation. Both native Release executables run
serially on the same computer through the authorized warning/desktop-lease wrapper.
The fixture paints but does not capture a bitmap. Raw logs retain every open/close,
heap counter, native extent and restoration result. Source and executable hashes
are archived with each variant. Both runs pass all 320 openings at 476 by 322
pixels / 96 DPI, with zero heap errors and restored focus/cursor.

Last-sixteen-cycle median open-minus-before, bytes:

| Entries / descriptions | Baseline live heap | Candidate live heap | Baseline private | Candidate private |
| --- | ---: | ---: | ---: | ---: |
| 12 / 0 | 86,725 | 86,789 | 4,096 | 4,096 |
| 12 / 1 | 150,900 | 150,900 | 4,096 | 4,096 |
| 12 / 2 | 189,736.5 | 170,685 | 4,096 | 4,096 |
| 12 / 4 | 266,506 | 209,938 | 4,096 | 4,096 |
| 12 / 8 | 420,707 | 288,651 | 4,096 | 4,096 |
| 12 / 12 | 575,310 | 367,894 | 4,096 | 16,384 |
| 24 / 0 | 95,231.5 | 95,036 | 4,096 | 12,288 |
| 24 / 24 | 1,073,124 | 639,436 | 270,336 | 28,672 |
| 48 / 0 | 112,607.5 | 112,412 | 4,096 | 8,192 |
| 48 / 48 | 2,072,214.5 | 1,185,982 | 884,736 | 575,488 |

This is about 36% less live heap for twelve fully described repeated-caption
entries, 40% for 24 and 43% for 48. Private-memory deltas are allocator-dependent
and do not establish a monotonic saving. Small signed closed-heap residuals and
all min/max values remain in the raw summary. `analyze.py <variant>/scaling.log`
reproduces each summary and rejects missing samples or unequal native extents.

## Isolated design experiment

The standalone probe runs 32 rotated cycles of twelve French primary/path pairs,
using Body/Small typography at 396 DIP. It has no popup, semantic tree, painting,
bitmap readback or UI Automation. Last-sixteen-cycle median live bytes:

| Layout strategy | After creation | After metrics | After release |
| --- | ---: | ---: | ---: |
| Separate primary and secondary per row | 13,272 | 453,754 | 0 |
| One formatted layout per row | 12,480 | 250,346 | 0 |
| Shared identical primary, distinct secondary | 7,200 | 246,338 | 0 |

The selected sharing approach cuts native text storage about 46% in this repeated
caption fixture while retaining the existing two-field drawing contract. These
are native process-heap totals, not C++ row sizes, whole-process private memory,
allocation call stacks, pixel equivalence or timing acceptance. It does not
explain the earlier menu-free common-scene memory difference.

`text-only/analyze.py text-only/run.log` reproduces the isolated summary. The
retained source and compiler driver describe the experiment; the driver uses
the original external paths. Production correctness is covered by the library's
French repeated-name, row-width, DPI-roundtrip, input and accessibility tests.

## Qualification boundaries

All seventeen non-activating Release suites pass with zero skips. Directed Menu
and NativeTextInput functional cases also pass. NativeTextInput restores focus
and cursor; the Menu wrapper reports cursorRestored=0 despite restored focus,
so that run does not establish cursor-restoration acceptance. A diagnostic-only
follow-up records each fixture's restore/preserve-external-movement outcome. All
three pointer moves restore exactly, and the complete Menu run restores child
focus, foreground and physical cursor. The first mismatch remains unclassified;
this follow-up is not claimed as a production repair. A process exit of zero
must not hide the first wrapper outcome.

The first common-scene comparison and a subsequent serial ABBA pair both retain
timing flags. The candidate used incremental linking while the baseline was a
fresh build. Clean rebuilding the same candidate tests that concrete build-history
hypothesis: the rebuilt `.text` bytes are identical (SHA-256
`ce6a5692431e810e42621c90319f1e25af4bbb24f67b9dec96c892c7780e07c2`).
The hypothesis is not supported, and another unchanged timing retry is not justified.
The retained PE section hashes record this result. The gallery was regenerated
with `gallery.ps1 -PublishDocs` and reviewed: all described-menu pixels are
unchanged. Four sheets match byte-for-byte; the other two differ only in their
indeterminate progress bar animation (450 and 642 pixels, exact rectangles in
the receipt). No text, geometry or interaction changed. Skills, specs, dependency
and format validators pass. Final native configuration qualification is pending.
Earlier flagged comparisons remain
valid unresolved findings; this experiment grants no resource waiver and changes
no consumer pin. Full external logs and failed attempts remain under
`C:/RedSalamander.Perf/evidence/i26-ui/menu-combined-layout-probe/`.
