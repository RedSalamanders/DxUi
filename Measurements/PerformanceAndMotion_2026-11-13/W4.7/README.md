# W4.7 embedded partial preparation

Plan item `W4.7` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md). On the
embedded host `EmbeddedHost::Prepare` already repaints only the paint region (`W4.10`); this item makes a caret blink flip add
only the caret's ink to it. The contract is [performance and resources, "Paint region"](../../../Specs/Core/Core_PerformanceAndResources.md#paint-region).

- Source: the `(W4.7)` commit on branch `pm/w4-7-embedded-partial`, built on base `59e4b81bb` and rebased onto integration tip
  `4be928d9b`. The "before" numbers were measured on the base's behaviour (the commit's test, with only unused private members
  added to the editors).
- Build: x64 Debug (counters, pixels, the frame verifier), Windows 11 Pro 10.0.26300, MSVC 14.52.37025.
- Adapter: WARP (Microsoft Basic Render Driver).
- Files: `before-fx1.txt` (FX1 flip frames before the change, `DXUI_CARET_FLIP_REPORT=1 DxUi.EmbeddedTests.exe --test-caret-flip-regions`),
  `after-caret-flips-debug.txt` (the same command after: FX1, 400 flips in 25 editor states at 4 scales, the oracle, the
  repaint-as-before and fallback checks), `fx5-and-oracle-paint-regions-debug.txt` (`DxUi.EmbeddedTests.exe --test-paint-regions`:
  the `FX5` hover frames and the regions oracle).

## Gate

| Clause | Result | Evidence |
| --- | --- | --- |
| Embedded `FX1`: pixels written per caret flip <= caret ink bounds + 2 px | **Met** (the bound is `(w + 2) * (h + 2)` for the ink's bounding box `w x h`, the ink being the box of pixels that differ between the frame before and after the flip). Editable field: 12,388 pixels per flip before (the field's extent, 326 x 38) and 40 after (ink 2 x 20, bound 88), 10 flips, 0 whole-view repaints. Multiline field of 64 Ki units: 480,000 pixels per flip before (the whole 800 x 600 view, 1 whole repaint per flip, 4 `Paint` executions) and 17 after (ink 1 x 17, bound 57; 1 region rectangle, 2 `Paint` executions), 8 flips. Read-only field: no flips. | `TestFx1CaretFlipPaintsOnlyTheCaret` |
| Every editor and state | **Met**: `TextField` (single line at the end, start, middle, empty, with a placeholder, with a selection, scrolled, Hebrew and mixed text, right-to-left flow, an IME composition, masked), multiline `TextField` (line end, empty line, scrolled, right-to-left flow), editable `ComboBox` (end, placeholder, Hebrew), `RichTextEditor` (source: end, empty, after typing; Markdown: start, Hebrew), at 96, 144, 192 and 150.5 DPI: 400 flips, each one region rectangle of at most the ink + 2 px (for example 40 px at 96 DPI, 60 at 144, 80 at 192 for a single line field), and the frame verifier (Debug) found 0 differences against the whole frame. No editor has an overwrite caret, so there is no second caret shape to cover. | `TestCaretFlipsInEveryEditorAndState` |
| Embedded `FX5`: `Paint` executions per hover frame <= 2 | **NOT MET**, as for `W4.10`: 8 per hover frame (2 region rectangles of 4: the root, the toggle's card, the toggle and the slider its reach meets; 4 in the first hover frame, which changes one toggle). Judged against `G2` as restated in the plan's section 1 (changed controls, their ancestors and the neighbours their reach meets, per repainted rectangle), it is met exactly: 4 per rectangle, 83 before `W4.10`. Not reduced here. | `TestFx5HoverRegionGate` (`fx5-and-oracle-paint-regions-debug.txt`) |
| `W0.5` oracle: 0 differences reading the embedded texture | **Met**: script `caret-flips` (a `TextField`, a multiline one, an editable `ComboBox` and a `RichTextEditor`, flips between typed characters and focus moves) at 96, 150 and 192 DPI, 24 and 25 steps each, 0 differences; 48 region rectangles in those runs. The window host runs the same script with whole frames. | `TestCaretFlipOracleScripts`, `TestNativeIncrementalParityPaintRegions` |
| A flip that cannot trust the ink repaints the control | **Met**: an open `ComboBox` popup (unbounded extent) repaints the whole view; typing, selection, caret movement, a bounds change and focus changes still repaint the control's extent or more. | `TestCaretFlipFallsBackWhenTheInkIsUnknown`, `TestEverythingThatMovesTheCaretStillRepaintsTheControl` |

## Configurations

Rebased onto integration tip `155622462`. x64 Debug: `Test-Changes.ps1 -Configuration Debug` passed in full (all suites,
including the whole Embedded suite with the frame verifier and `TestNativeIncrementalParityPaintRegions` with the new
`caret-flips` script on the window host) before the last rebase. After it, on the rebased commit:

| Configuration | Embedded suite (includes every test above) | `TestNativeIncrementalParityPaintRegions` |
| --- | --- | --- |
| x64 Release | passed | passed |
| x64 ASan Debug | passed | passed |

## What changes in the fixture receipts

`FX1` is regenerated at closeout. Its embedded `editable` and `multiline` phases change: `frame.pixelsPainted` per blink flip
frame (12,388 to 40 and 480,000 to 17 on WARP at 96 DPI), `frame.paintRegionRects` (1 for both; the multiline field had 0
with 1 whole repaint, `frame.paintRegionFull`) and `frame.paintExecutions` (the multiline field's flip frames 4 to 2). Counters
that do not follow the flip frames (tick calls, wakes, layouts) are unchanged. The window receipts are unchanged: the window
host paints whole frames until `W4.9`.
