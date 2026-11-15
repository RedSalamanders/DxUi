# W2.7 Text field paint: before and after on FX1

Plan item `W2.7` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); contract
"Text field layouts" in [performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#text-field-layouts).

- **Scenario**: fixture `FX1` ([definition](../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx1.h)), variant
  `default` (800 by 600, 96 DPI): a focused editable and a focused read-only single-line `TextField`, then the focused
  64 Ki-unit multiline `TextField` idle (caret blinking), wheeled down a quarter notch (30 units) per step and paged down
  (`PageDown`) once per step. W2.7 added the two last phases (`multiline-wheel`, `multiline-page`) to the definition.
  Embedded host, WARP, x64 Release (MSVC 19.52.37025).
- **Before**: base `37c476b17` with only the W2.7 tests, the two FX1 phases and the `text.nativeTextInputStateCopies`
  counter and wipe observer added (the library behaves as the base); receipt written by
  `DxUi.EmbeddedTests.exe --fixture-receipt FX1 Warp <file>` into `.build` (not retained; its counts are below).
- **After**: the W2.7 change on the same base, same command and the same treatment. Counts only; timing not measured.
- **Pixels**: [frame-hashes/](frame-hashes) holds one FNV-1a hash per FX1 step (246 steps), `before/` and `after/`:
  `DxUi.EmbeddedTests.exe --fixture-frame-hashes FX1 <file>` (the prepared surface) and
  `DxUi.ControlTests.exe --fixture-frame-hashes FX1 <file>` (a full frame of the window host). Every hash is equal before
  and after on both hosts.

## Counts

Per phase, summed over its steps; a step is one action and its frame. "Layouts" is `resources.textLayout.created`,
"copies" `text.nativeTextInputStateCopies`, "allocations" the C++ allocations of the step (action and frame).

| Phase | Steps (painted) | Layouts before | after | Copies before | after | Allocations before | after |
| --- | --- | --- | --- | --- | --- | --- | --- |
| warm (build, focus) | 2 (1) | 4 | 3 | 2 | 0 | 20 | 16 |
| editable | 100 (10) | 10 | 0 | 20 | 0 | 31 | 1 |
| readonly | 100 (1) | 1 | 0 | 2 | 0 | 3 | 0 |
| multiline (idle, focused) | 20 (9) | 16 | 0 | 18 | 0 | 35 | 1 |
| multiline-wheel (quarter notches) | 16 (5) | 42 | 0 | 10 | 0 | 54 | 1 |
| multiline-page (`PageDown`) | 8 (8) | 56 | 0 | 16 | 0 | 72 | 2 |

Before, each focused multiline paint built two full-document layouts (selection and caret), each wheel step two (the
document and the "Ag" fallback) whether or not it completed a notch, and each `PageDown` seven (five for the key, two for
its paint); every paint of an unfocused multiline field built one, and every paint of a focused field copied the
text-input state twice. After, the multiline field is laid out once (the warm-up build) and reused by every path while
its text and width stay.

The single-line paint gate counts the frame alone (`TestFx1TextFieldPaintGate`): 0 C++ allocations in each of the 10
painted frames of the editable phase and the painted frame of the read-only phase. The one remaining allocation of the
editable phase is the first caret tick's (the action, not the paint). The 1, 1 and 2 left in the multiline phases are not
attributed here (the gate does not cover multiline allocations). The ten implicit layouts per editable phase (`resources.implicitTextLayout.created`) are the
clear button's glyph drawn by `DrawCenteredText`, plan item `W2.1`.

Composition (`TestTextFieldCompositionStateCopies`, an embedded composition preview through `ApplyTextInput`, never a
real IME): a paint without a composition copies the state 0 times and allocates nothing; a paint during the composition
copies it once (before: twice per paint, composing or not).

## Gallery

`gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'TextField' -Warp`, compared with [G1](../G1/receipt.json)
(`gallery.ps1 -CompareBefore ... -CompareAfter .build/gallery/x64/Release/tiles`): all 75 TextField tiles that G1 holds are
identical at 100%, 150% and 200% in every theme (the parity report stays under `.build/gallery/parity`). The 30 others,
`textfield-focused` and `textfield-focused-read-only` in five themes at three scales, were added to the gallery after G1
and have no G1 capture (`MissingBefore`); their states (a focused editable and a focused read-only field) are the FX1
`editable` and `readonly` phases, whose frames are identical before and after on both hosts.
