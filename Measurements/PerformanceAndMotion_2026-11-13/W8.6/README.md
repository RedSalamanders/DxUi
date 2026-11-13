# W8.6 Grapheme stepping: code units visited per step

Item: `W8.6` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: [UI_InputAndAccessibility.md](../../../Specs/UI/UI_InputAndAccessibility.md), shared text-element segmentation.

## Scenario

`TestTextElementSteppingVisitsBoundedUnits` (`Tests/Controls/Controls.Tests.TextElements.cpp`, suite `EditorControls`)
builds exactly 65,536 UTF-16 units (32,772 grapheme elements) from a fixed cycle of CJK, kana and Hangul words and
emoji: a single emoji, a skin-tone modifier, a two-person ZWJ sequence, the four-person family ZWJ sequence
(11 units), a keycap, a heart with VS16, one flag and two adjacent flags. `BuildCjkEmojiFieldText` is shaped for
reuse by the later `FX7` fixture set. It measures:

- every UTF-16 offset (0 to 65,536, including surrogate and cluster interiors): `StepToNextTextElement`,
  `StepToPreviousTextElement`, `SnapCaretIndexToTextElementBoundary`;
- a `TextField` holding the text, caret at the middle element (32,767): 100 Right then 100 Left arrow keys,
  measured around each `OnKeyDown`;
- the UI Automation character walk (`FX6`-shaped): at every element, `GetEnclosingAccessibilityTextUnitSpan`
  (Character) from the element's last unit, and `MoveAccessibilityTextPositionByUnit` (Character) +1 and -1.

The counter is `DxUi::testTextElementCodeUnitsVisited` (diagnostics hook in `DxUi.TextElements.cpp`): UTF-16 units
of every code point the grapheme walkers read, forward and backward. It is a local hook, to be folded into the
`W0.1` work counters.

The "before" figures run the pre-change wrappers, kept verbatim in the test as `FromZeroStepToNextTextElement` and
`FromZeroSnapCaretIndexToTextElementBoundary`, on the same text, carets and counter in the same binary. Base
`2f7e8af8` has the same forward walker; the counter only adds the count.

## Result (x64 Debug, Release and ASan Debug; source = this item's commit on integration tip `693b1981`; no adapter used: pure CPU)

| Step | Before (from 0) | After |
| --- | --- | --- |
| Right arrow at the 100 carets the field visited | max 53,307, mean 53,143.6 units | max 24, mean 6.5 units per key (Right and Left) |
| Nearest snap (before: last unit of every 1,024th element; after: every offset) | max 105,946, mean 52,975.5 | max 24, mean 8.3 |
| Next step, every offset | from 0 off the ASCII path | max 24, mean 9.9 |
| Previous step, every offset | already anchored (narrower pair rule) | max 24 |
| UIA character expansion | snap + previous + next, each from 0 | max 63, mean 14.7 |
| UIA Move(+1) / Move(-1) | snap from 0 | max 18 / max 27 |

Gate: at most 64 units per step; every row meets it. The expansion's maximum (63) is the family ZWJ sequence read by
three calls (snap, previous, next); the item leaves `DxUi.Accessibility.cpp` unchanged.

x64 Debug, Release and ASan Debug runs of the same test printed identical counts (they do not depend on timing).

Bounds that remain open by the Unicode rules: a run of regional indicators (GB12/GB13), a chain of pictographic
ZWJ pairs (GB11) and an Indic linker chain (GB9c) are read back to their start, because their pairs depend on
earlier context. The fixture's longest such run is two adjacent flags (8 units).

Evidence (local, not committed), each with `payload/logs/test-EditorControls-x64-<Configuration>*.log`:
Debug `.build/evidence/test-runs/20261113T193719802Z-53684-dc532d99fc5d460c8f4864a4c21263da`, Release
`.build/evidence/test-runs/20261113T194930914Z-53684-2efc867c0c9740a69fdc96d3c36633c9`, ASan Debug
`.build/evidence/test-runs/20261113T195804711Z-53684-effae13c2dfc41a59ea545f50e02f594`.
