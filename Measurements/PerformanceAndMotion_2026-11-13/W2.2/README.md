# W2.2 ComboBox popup layout — width measurements and placements on FX2

Plan item W2.2 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior in
[controls and layout](../../../Specs/UI/UI_ControlsAndLayout.md) ("ComboBox popup layout"), counters in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md).

- **Scenario**: fixture `FX2` ([definition](../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx2.h), default variant,
  800x3600 at 96 DPI, embedded host, WARP): a 500-item editable ComboBox opened with F4, 60 hover moves, Escape, 20
  characters typed while closed; then a 100-item ComboBox whose popup shows every row, opened, 20 hover moves, Escape.
- **Command**: `DxUi.EmbeddedTests.exe --fixture-receipt FX2 Warp <receipt>` run directly, three times per side, in fresh
  processes. "Before" is base `2f9001a3` (the integration tip the item started from), "after" is this item's change
  on `89eba619` (the FX2 receipt was taken again after rebasing on `79b20d43`, with the same counters and a 168 MB peak). **Build**: x64 Release, MSVC 19.52. **Adapter**: WARP (Microsoft Basic Render Driver).
  **OS**: Windows 11 build 26300. The fixture's own receipt (`performance-fixtures.ps1 -Fixture FX2 -CountDrawCalls`, with
  its provenance and input hashes) is regenerated under [FX2](../FX2/README.md) for "after"; the "before" receipt is the one
  it replaces (Git history of that file). The direct runs' raw receipts carry no input hashes and are not retained.
- **Gate test**: `TestFx2PopupLayoutGate` (`Tests/Embedded/Embedded.Tests.ComboBoxPopupLayout.h`) reads
  `testSingleLineMeasureLayoutCreations` around every FX2 step; the Embedded suite runs it, and
  `DxUi.EmbeddedTests.exe --test-combobox-popup-layout` runs it alone with the legacy comparison, the typography test and
  the `combo-popup` parity script.

| Phase (per step) | Text layouts before | Text layouts after | Single-line measuring layouts after | Popup placements after |
| --- | --- | --- | --- | --- |
| `open` (1) | 9,500 | **500** (one per item) | 500 | 2 |
| `hover` (60) | 11,000 | **0** | 0 | 0 |
| `close` (1) | 0 | 0 | 0 | 0 |
| `type`, popup closed (20) | 1,001 max, 5,290 total | **1** max, 20 total (the field's own text layout) | 0 | 4 total |
| `fit-open` (1) | 20,300 | **100** | 100 | 2 |
| `fit-hover` (20) | 20,600 | **0** | 0 | 0 |
| `fit-close` (1) | 0 | 0 | 0 | 0 |
| second open of unchanged items (gate test) | — | 0 | 0 | — |
| typing into the open popup (gate test, 6 characters) | — | 0 | 0 | — |

Text layouts are `resources.textLayout.created`; single-line measuring layouts are `testSingleLineMeasureLayoutCreations`
(the gate counter); placements are the new `popups.comboBoxPopupLayouts`, and the new `popups.comboBoxItemWidthMeasurements`
equals the measuring layouts in every phase. The remaining per-frame work of an open popup is outside this item:
`DrawCenteredText` per visible row (`resources.implicitTextLayout.created`, 10 per hover frame and 102 per all-fit frame:
`W2.1`) and the popup material (2 effects and 2 command lists per frame: `W2.4`).

**Peak private bytes** (largest `privateBytes` sample of a run; three fresh processes each, Release, WARP):

| | Run 1 | Run 2 | Run 3 |
| --- | --- | --- | --- |
| Before | 444,186,624 | 455,213,056 | 443,666,432 |
| After | 166,359,040 | 164,536,320 | 166,739,968 |

The peak falls by about 63% but is not gone: private bytes still grow during `hover` (49 MB to 101 MB) and `fit-hover`
(128 MB to 166 MB) while no text layout is created by the popup layout any more, so the rest follows the per-frame
drawing (implicit `DrawText` layouts and popup effects on WARP) and is not explained here; tracked C++ bytes stay flat.

**Run times** of the FX2 determinism gate (two runs, `Performance fixture FX2@default ... (N ms)`): before about 27 s in
Debug and 152 s in ASan Debug (the item brief's figures for base `2f9001a3`); after 3.6 s in Debug (inside the full Embedded
suite) and 2.6 s in ASan Debug (`--test-performance-fixtures`). A single Release receipt process took 11 to 21 s before and
under 1 s after.

**Pixels**: the `ComboBox` gallery tiles at 100%, 150% and 200% on WARP are identical to the
[G1](../G1/README.md) captures (30 of 30 tiles at each scale; `gallery.ps1 -CompareBefore` exit 0). The
incremental-versus-fresh `combo-popup` script (3 seeds, 40 steps each) shows no difference on the embedded and the native
host, and the retained widths and placement equal the per-layout measurement bit for bit in 240 comparisons over eight
seeded item sets (empty, one item, a very long item, CJK, emoji, right-to-left, mixed scripts, 500 items, with filters typed
in seeded orders).
