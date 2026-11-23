# W2.1 retained text layouts

Plan item `W2.1` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); the
behaviour and the chosen bound are stated in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#retained-text-layouts).

- Source: this item's commit on the integration branch (rebased on `b20c619c`, after `W2.7`, `W2.8`, `W2.3`, `W3.2`, `W3.3`, `W6.2`, `W7.2` and `W9.3`); the bound sweep and the gallery comparison ran on it rebased on `9574105b`.
  "Before" is base `7d39fef7` built unchanged (x64 Release); the bound sweep compares bounds within the item's build.
- Build: x64 Release, MSVC 14.52.37025; Windows 11 Pro 10.0.26300.
- Adapter: WARP (Microsoft Basic Render Driver) throughout. No hardware timing was run here.
- Fixture receipts regenerated with the item: [FX1](../FX1/README.md), [FX13](../FX13/README.md), [FX5](../FX5/README.md) WARP (FX5 gains a 20-step `hover` phase),
  [FX2](../FX2/README.md) (draw-call receipt) and every WARP receipt of [FX12](../FX12/README.md). The other fixtures'
  receipts still record their `DrawText` layouts as before.

## Counters per step (embedded host, WARP)

`implicit` is `resources.implicitTextLayout.created` (a `DrawText` call lays its string out); `layouts` is
`resources.textLayout.created`.

| Fixture, phase | Before: implicit / layouts | After: implicit / layouts | Retained draws |
| --- | --- | --- | --- |
| FX5 `dirty` | 49 / 7 | 0 / 7 (the Grid's 7 new rows) | 49 |
| FX5 `hover` (new phase) | no phase before; every paint laid its 49 strings out in `DrawText` | **0 / 0** (`G11`) | 49 |
| FX12 `default` `full`, `hover` | 1,333 / 0 | 0 / 0 | 1,333 |
| FX12 `typography-120` `hover` | 1,083 / 0 | 250 / 0 | 833 |
| FX12 window host `hover` | 2,666 / 0 | 0 / 0 | |
| FX2 `hover` (open popup) | 10 / 0 | 0 / 0 | 10 |
| FX2 `fit-hover` (100 rows) | 102 / 0 | 0 / 0 | 102 |

With `W2.8` on the base, `typography-120` keeps its formats and creates no layout per frame. Its 250 `DrawText` strings are
Tree leaf rows whose text box is not positive in the narrow cells (`text.textLayoutCacheBypasses`).

## Bound: memory against layout work

`DxUi.EmbeddedTests.exe --measure-text-layout-bound FX<N>[@variant] <bound> <frames>` plays each fixture's phases, then
200 unchanged frames (`ControlHost::Invalidate`, a full repaint of the same strings). It runs one process per fixture and
bound, and two runs of the whole set. Bound 0 draws every string through `DrawText`. The table takes power-of-two sizes,
so 1,536 is not a table size; 16,384 (the Grid's ceiling) shows what an unbounded table would hold.

Columns:

- **Strings**: strings drawn per frame.
- **Entries**: resident layouts after the frames.
- **Created**: layouts created per unchanged frame (0 means the table holds every string).
- **Prepare cycles**: median UI-thread `QueryThreadCycleTime` of `Prepare` (preparation and paint) over 200 frames, run 1 /
  run 2. Shaping is CPU work, so WARP is acceptable here; its rasterization is CPU work too and dominates the dense frames.
- **Private Δ**: private bytes against bound 0 in the same run, in MB. Process-level, between processes, and noisy:
  WARP's threads allocate in the same heaps; FX10 moves by 35 to 90 MB with 21 entries.

| Fixture | Bound | Strings | Entries | Created | Prepare cycles (M) | Private Δ (MB) |
| --- | --- | --- | --- | --- | --- | --- |
| FX12 `default` (1080p) | 0 | 1,333 | 0 | 1,333 | 243.0 / 233.9 | 0 / 0 |
| | 512 | 1,333 | 512 | 502 | 228.4 / 232.5 | +12.5 / +19.4 |
| | 1,024 | 1,333 | 947 | 67 | 224.1 / 217.3 | +50.3 / +38.0 |
| | **2,048** | 1,333 | 1,014 | **0** | 206.6 / 230.2 | +18.5 / +13.8 |
| | 16,384 | 1,333 | 1,014 | 0 | 225.7 / 223.9 | +42.1 / +17.7 |
| FX12 `uhd-192dpi` (4K) | 0 | 1,333 | 0 | 1,333 | 267.4 / 295.1 | 0 / 0 |
| | 512 | 1,333 | 512 | 502 | 251.7 / 287.0 | -7.8 / +55.1 |
| | 1,024 | 1,333 | 947 | 67 | 262.2 / 270.1 | +30.8 / +29.7 |
| | **2,048** | 1,333 | 1,014 | **0** | 289.6 / 257.2 | +367.3 (outlier) / +29.6 |
| | 16,384 | 1,333 | 1,014 | 0 | 253.1 / 263.7 | +46.1 / +32.6 |
| FX12 `form-layout` | 0 | 900 | 0 | 900 | 77.0 / 75.4 | 0 / 0 |
| | 512, 1,024, 2,048, 16,384 | 900 | 204 | 0 | 49.3 to 57.6 | -3.4 to +18.7 |
| FX12 `form-layout-10k` | 0 | 9,000 | 0 | 9,000 | 647.4 / 654.1 | 0 / 0 |
| | 512 | 9,000 | 512 | 4,859 | 526.3 / 524.5 | +4.3 / +15.1 |
| | 1,024 | 9,000 | 1,024 | 2,589 | 449.7 / 466.5 | +25.1 / +28.1 |
| | **2,048** | 9,000 | 1,878 | 374 | 376.7 / 444.7 | +28.8 / +74.6 |
| | 16,384 | 9,000 | 2,011 | 0 | 363.4 / 429.6 | +37.5 / +66.5 |
| FX5 | 0 | 49 | 0 | 49 | 5.6 / 5.6 | 0 / 0 |
| | 512 to 16,384 | 49 | 34 | 0 | 3.8 to 4.6 | -2.0 to +16.8 |
| FX9 `default` | 0 | 35 | 0 | 35 | 5.1 / 5.0 | 0 / 0 |
| | 512 to 16,384 | 35 | 31 | 0 | 3.3 to 4.1 | +2.1 to +22.0 |
| FX10 `default` | 0 | 22 | 0 | 22 | 5.6 / 5.3 | 0 / 0 |
| | 512 to 16,384 | 21 | 21 | 0 | 4.4 to 4.6 | +35.0 to +90.4 (WARP noise) |
| FX13 `default` (sparse) | 0 | 21 | 0 | 21 | 3.2 / 3.4 | 0 / 0 |
| | 512 to 16,384 | 21 | 21 | 0 | 2.3 to 2.9 | -0.4 to +1.3 |
| FX1 `default` (sparse) | any | 0 | 0 | 2 (the field's own) | 113.5 to 134.1 | -1.8 to +0.1 |

**DirectWrite bytes per layout, measured in-process.** Live heap bytes returned when the table is released, divided by
its entries:

- FX12 (every variant, both runs): 17,756 to 18,212 bytes.
- FX5, FX9, FX10 and FX13: 10.5 KB to 30 KB, from fewer entries and with more noise.

So a retained short string costs about 18 KB of heap. Process private bytes show about 27 KB with the heap's own retention
(FX12 `default`: +27.7 MB for 1,014 entries in the regenerated receipt pair).

**The registry row is not the cost.** Its bytes are the table and key strings only: FX12 `default` reports 1,014 entries
and 147,456 bytes, while those layouts hold about 18 MB of heap.

**Choice: 2,048 entries** (`kTextLayoutCacheMaxEntries`). It is the smallest bound at which every fixture that fits a real
screen (FX12 `default` at 1080p, `uhd-192dpi` at 4K, `form-layout`, FX5, FX9, FX10, FX13) creates 0 layouts per unchanged
frame:

- At 1,024, FX12's 1,014 distinct strings do not fit. The table has 32 sets of 32 ways, so it keeps about 947 and lays 67
  out again every frame.
- Worst-case resident memory at 2,048: 2,048 × 18 KB ≈ 37 MB of heap (about 55 MB private), against the accepted envelope
  of about 50 MB.
- A screen beyond the bound pays only its overflow. `form-layout-10k`, 10,000 controls in a scroll panel that paints all
  of them, lays out 374 of its 9,000 strings per frame at 2,048.
- 16,384 would have bought that fixture its last 374 layouts per frame (2,011 entries, about 36 MB). It would allow about
  290 MB on a larger screen, outside the envelope.
- The CPU gain is clearest where text dominates the frame: `form-layout` -33%, `form-layout-10k` -32 to -42%, FX5, FX9
  and FX13 -15 to -30%. On the dense FX12 screen WARP's rasterization dominates, so the gain sits inside the run-to-run
  spread.

**Proportional, not standing.** The table holds only the strings of the last paint:

- FX13: 21 entries, about 0.4 MB.
- FX5: 34 entries, about 0.65 MB.
- FX1: none.
- Hidden, every table is released (FX12 hidden private bytes 42.6 MB).

## Glyph-run alternative (`W6.5`)

Retained layouts that a captured glyph run could replace (one line, no trimming, basic-plane text only, so no
supplementary-plane colour emoji), as `DebugCountGlyphRunEligibleTextLayouts` counts them:

| Fixture | Eligible / retained |
| --- | --- |
| FX12 (every variant) | 1,014 / 1,014; 204 / 204 (`form-layout`); 1,878 / 1,878 (`form-layout-10k` at 2,048) |
| FX5 | 34 / 34 |
| FX9 | 31 / 31 |
| FX13 | 21 / 21 |
| FX10 | 20 / 21 |

At about 280 bytes per glyph run against about 18 KB per layout, nearly every retained string would qualify.

## Pixels

- Equivalence oracle `TestRetainedTextLayoutsDrawWhatDrawTextDraws`: 1,782 cases (54 of them pairs of boxes sharing only their width or only their height) at 96, 120, 144, 168 and 192 DPI
  (embedded, grayscale) and at the system DPI on a native window (ClearType). Every frame through the table, on the frame
  that creates the layouts and on the next, is byte-identical to the frame through `DrawText`.
- FX5 captures with the item, identical to [W6.4](../W6.4/README.md): `hwnd-dark-96dpi` `0xAEDE74BE5EBED69C`,
  `composition-dark-96dpi` `0xB7476CE1D8BEC3D6`, `embedded-dark-96dpi` `0x867A2DAE567318B2`, `hwnd-light-96dpi`
  `0xAE6ADB4413AF706C`, `composition-light-96dpi` `0x2E128D34E73A48E7`, `embedded-light-96dpi` `0x9B297D359290BC47`.
- Gallery, compared with the [G1 receipt](../G1/README.md): every harness tile in five themes at 100, 150 and 200% on WARP,
  x64 Release. On the rebased tip:
  - 1,245 of the 1,290 tiles are identical.
  - 30 tiles are new since G1 (`textfield-focused`, `textfield-focused-read-only`).
  - The 15 `button-selector-hover` tiles differ. That tile captured with the table off is byte-identical to the one with
    the table on, so the difference comes from the base, not from this item.
  - Before the rebase, all 1,260 of 1,260 tiles were identical ([gallery-parity.tsv](gallery-parity.tsv), one row per
    tile).

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json -CompareAfter .build/gallery/x64/Release/tiles -Scale 100,150,200
```
