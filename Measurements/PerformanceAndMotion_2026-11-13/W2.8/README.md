# W2.8 Format and brush caches: before and after on FX12 and FX11

Plan item `W2.8` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); contract
"Format and brush caches" in [performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#format-and-brush-caches).

- **Scenarios**: fixture `FX12` ([definition](../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx12.h)), variants
  `default`, `typography-120` (2,000 controls, Labels and two-row Trees typed by 120 distinct size-and-weight
  typographies, Tree rows asking for theirs on every paint) and `form-10k`; fixture `FX11`
  ([definition](../../../Tests/Embedded/Embedded.Tests.PerformanceFixtureFx11.h)), the Tree expand and collapse fades
  being the transitions that animate a color today. Both hosts, WARP, x64 Release (MSVC 19.52.37025).
- **Before**: base `79b20d439` with only the W2.8 counters and tests added (the library behaves as the base), receipts
  taken with `DxUi.EmbeddedTests.exe --fixture-receipt <FX> Warp` and `DxUi.ControlTests.exe --fixture-receipt <FX>` into
  `.build` (not retained: the retained [FX11](../FX11/README.md) and [FX12](../FX12/README.md) receipts of earlier tips
  show the same counts); the before numbers below are read from them.
- **After**: [FX11/](FX11/README.md) and [FX12/](FX12/README.md), written by
  `performance-fixtures.ps1 -Fixture <FX> [-Variant <v>] -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W2.8`
  from the W2.8 commit rebased on the integration tip. Counts only; timing not measured.
- **Pixels**: [frame-hashes/](frame-hashes) holds one FNV-1a hash per fixture step, `before/` from the before build and
  `after/` from the W2.8 build: `DxUi.EmbeddedTests.exe --fixture-frame-hashes <FX>[@variant] <file>` (the prepared
  surface) and `DxUi.ControlTests.exe --fixture-frame-hashes <FX>[@variant] <file>` (a full frame of the window host,
  `DebugCaptureBitmap`). [gallery/](gallery) holds the one tile that changed.

## Pixels

Every frame hash is equal before and after: FX11 (2,191 steps) on both hosts, FX12 `default` and `typography-120`
(23 steps each) on both hosts and FX12 `form-10k` on the embedded host.

Gallery (`gallery.ps1 -Configuration Release -Scale 100,150,200 -Warp`, tiles of Tree, Button, Toggle, Checkbox,
RadioButton, Label, ComboBox, MenuBar, TextField, TabControl and Toolbar, compared with
[G1](../G1/receipt.json)): 660 of 675 tiles are identical. The 15 that differ are `button-selector-hover` at every scale
and theme but 100% dark, and only in the caption under the button. Before W2.8 the Selector button drew its chrome
with the host's keyed brush for its text color and recolored that brush (fill, border, text, chevron); its last
color, the hover-blended chevron, stayed on the keyed brush, so the caption asked for the text color and was drawn in
the chevron's gray (`before-100-light`: caption pixels 102,102,102). The Selector now draws with a transition brush, so
the keyed brush keeps its color and the caption is drawn in the text color (`after-100-light`: 20,20,20). This is the
fix of that defect, not a regression; no other pixel moved.

## Counts

Per measured step (each step includes its frame); "forced" is `frame.fullVisualPreparations`.

| Fixture, host | Phase | Forced before | after | Formats created before | after | Format trims before | after | C++ allocations before | after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FX12 typography-120, embedded | full, hover, value | 1 | 0 | 122 | 0 | 1 | 0 | 244-245 | 0-1 |
| FX12 typography-120, window | full, hover, value | 1 | 0 | 122 | 0 | 1 | 0 | not counted | not counted |
| FX12 default and form-10k, both hosts | measured | 0 | 0 | 0 | 0 | 0 | 0 | unchanged | unchanged |

Warm-up of typography-120: 366 formats created and 3 forced preparations before, 122 and 1 (the first frame) after on
the embedded host; 380 and 3 before, 136 and 1 after on the window host.

FX11, brushes created per Tree fade phase (embedded; the window host matches within one or two):

| Phase | 30 Hz before | after | 60 Hz before | after | 144 Hz before | after | 240 Hz before | after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| tree-expand | 9 | 6 (step 0) | 12 | 0 | 24 | 0 | 19 | 0 |
| tree-collapse | 8 | 0 | 12 | 0 | 25 | 0 | 17 | 0 |

After W2.8 a fade draws through transition brushes reused with `SetColor` (`frame.brushColorReuses`: 54, 114, 252-276
and 420-456 per phase); the six brushes of the first fade are the transition brushes themselves. Every other phase
creates what it created before (popup open 7, then 2; toggle and checkbox on 3 and 1 at their first window).

## Memory (G13)

Resident at the end of the run (embedded receipts, tracked live C++ bytes from the scene build; brush and format counts
from the retained-resource statistics):

| Fixture | Tracked bytes before | after | Brush objects before | after | Formats before | after | Registry cache bytes before | after |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| FX12 default | 1,411,086 | 1,411,598 | 17 | 17 | 3 | 3 | 1,856 | 2,320 |
| FX12 typography-120 | 1,585,125 | 1,586,685 | 21 | 21 | 122 | 122 | 13,856 | 15,368 |
| FX12 form-10k | 9,215,453 | 9,215,941 | 16 | 16 | 3 | 3 | 161,824 | 162,264 |
| FX11 (after the last phase) | 28,419 | 32,171 | 150 | 30 | 3 | 3 | 13,280 | 16,984 |

Each map entry grew by its last-use frame (and a brush entry by its key color, 16 bytes): +0.5 KB on the dense screens
and +1.5 KB with 120 formats, about 0.25 to 0.8 bytes per control, within `G13`'s 256 bytes per simple control. FX11
holds 120 fewer DirectWrite/Direct2D brush objects; its map keeps the first color of each faded color key without a
brush (the reason it grows by 3.7 KB), which is what keeps every fade frame identical to before. The format bound rose
from 96 to 256: a screen with more than 96 live formats now keeps them (each a DirectWrite text format object, not
tracked here) instead of recreating 122 a frame. A hidden view keeps both maps as before (the `hidden` samples equal
the last phase's), and a detached host keeps none (`TestHiddenAndDetachedCaches`).
