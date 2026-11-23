# W6.2: measure cache in `Control::Measure`

Plan item `W6.2` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contract: [measure cache](../../../Specs/UI/UI_ControlsAndLayout.md#measure-cache). The counter receipts this item
regenerated are under [FX10](../FX10/README.md) and [FX12](../FX12/README.md); this page explains what moved.

- **Source**: the W6.2 commit on the integration branch `ej/dxui-performance-motion-impl-63dac3` (base `f95f6db8`);
  "before" is that base plus a counting-only patch (a `layout.measureOverrideExecutions` counter and temporary hit
  counters on the two retired per-parent caches), never committed.
- **Build profile**: x64 Release (compiler 19.52.37025), `performance-fixtures.ps1`; slot experiments x64 Debug.
- **Adapter**: WARP (Microsoft Basic Render Driver 10.0.26100.9711); both hosts.

## Probe distribution and slot count

With eight slots kept in most-recently-used order (a temporary build), every `FX10` hit on both hosts was answered by
the most recently used slot: 0 hits from the second slot or later, in all 32 measured steps (embedded sweep: 3,180
hits, of which 1,830 reused a maximum-independent size at another maximum). One slot therefore suffices for `FX10`.
It does not suffice for a form with labels beside their editors: `FormPanel` measures each label at the shared cap and
then at its resolved width in every pass, two keys that alternate. With one slot the existing Forms test
`TestFormsMeasureIsPureAndSharedLabelCacheIsStable` failed ("the first arrangement reuses contextual label and editor
measures"); with two it passes. Two slots are the smallest count that keeps every existing reuse guarantee.

The window-host gate test records every probe by its complete key. `FX10` default (suite variant, x64 Release): 29
measured controls, 864 unique and 1,200 repeated probes, 384 `MeasureOverride` executions, 0 executions for a repeated
key (Debug counts 1,280 repeated probes: its verification re-runs measure children inside hits). The `FX12` form-10k
window run in that test takes 23.3 s in x64 Release (0 probes); Debug builds skip it, over the 300 s test timeout.

## Counters per fixture (x64 Release, WARP)

| Receipt | Phase | `measureOverrideExecutions` before -> after | `textLayout.created` before -> after | C++ allocations before -> after |
| --- | --- | --- | --- | --- |
| FX10 default, embedded | warm (2) | 54 -> 39 | 80 -> 65 | 160 -> 174 |
| FX10 default, embedded | sweep (30) | 1,620 -> 690 | 1,634 -> 704 | 2,477 -> 1,547 |
| FX10 default, embedded | rest (3) | 0 -> 0 | 0 -> 0 | 39 -> 39 |
| FX10 default, window | sweep (30) | 810 -> 345 | 824 -> 359 | not counted |
| FX10 uhd, embedded | sweep (30) | 1,620 -> 690 | 1,738 -> 808 | 4,235 -> 3,305 |
| FX12 every variant, both hosts | every phase | 0 -> 0 | unchanged | unchanged |

Text layouts created during measurement fell by exactly the executions saved (each library measurement shapes one
`IDWriteTextLayout`, two for a Toggle with state labels or a Button with a shortcut). The warm phase allocates 14 more
blocks once: the out-of-line caches of the 29 measured controls (the first measurement of each). The embedded host lays
out FX10 twice per sweep step and the window host once, hence the factor of two. `FX12` lays out absolutely (no
`FlexPanel`, `FormPanel` or `FormField`), so nothing measures before or after; its gate holds with 0 executions.

The per-parent caches this item removed hit as follows on `FX10` (before): `FlexPanel::_records` 0 times in all
phases (the constraints a panel offers a child differ between its measure and its arrangement, and every resize step
changes them), `FormField::_measureCache` 75 (warm) and 2,250 (sweep) on the embedded host, 1,125 on the window host.
After: every probe of those parents goes through the shared cache, which answered 3,180 (embedded) and 1,590
(window) sweep probes; executions fell 57%.

## Footprint

| | x64 Release before | after | ARM64 |
| --- | --- | --- | --- |
| `sizeof(Control)` | 296 | 304 | 296 -> 304 (compiled for ARM64) |
| `Label` / `Button` / `Toggle` / `Checkbox` / `RadioButton` | 448 / 816 / 904 / 912 / 864 | +8 each | same |
| Measure cache, allocated on the first successful measurement | none | 96 bytes (32-byte context, two 32-byte slots) | same |
| `FlexPanel` record per child (`ChildRecord`) | 192 | 144 | same |
| `sizeof(FormField)` (five inline `PartMeasure` records replaced by one `float`) | 856 | 512 | same |

`FX12` tracked live bytes at build: form-10k 9,053,869 -> 9,133,893 (+80,024 for 10,002 controls: 8.0 bytes each; per
control 905.2 -> 913.2); default 1,409,454 -> 1,425,470 (+8 x 2,002). `FX10` (embedded default): built 56,628 ->
54,316, after warm-up 67,132 -> 67,172, after the sweep 70,883 -> 70,923: the removed records pay for the caches.

## Layout equality

- Every type that declares `MeasureDependsOnMaximum() == false` matched an uncached measurement at every probe of a
  1/64 DIP sweep around its natural width, maxima of 0 and of infinity and both flow directions; every other measured
  type matched at every probed maximum; every measured input listed in the contract changes the next measurement as an
  uncached one would (`Controls.Tests.IntrinsicLayout.cpp`, Debug verification of every hit on, 0 mismatches).
- The incremental-versus-fresh oracle (W0.5) passes its layout, interaction and rich scripts with the new
  `Control._measureCache` registry row, and replays `FX12` default (23 steps): 0 differences in bounds, pixels or UIA.
- `FX10` replayed through the same oracle (window host, the adapter in `Controls.Tests.IncrementalParity.cpp`) differs
  at step 2: the incremental scene leaves the body row's `RichTextEditor` at empty bounds after the first resize while
  the fresh scene arranges it. The difference is identical with every measure cache disabled (a temporary build that
  never reads the cache), so it does not come from this item; it is reported to the plan and the FX10 replay is not
  registered as a test until the arrangement it shows is fixed.

## Gallery

`./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'FlexPanel','FormField','FormPanel' -Warp`, then
`-CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json`: 20 of 20 tiles identical at 100%, 150%
and 200% (the parity report stays under `.build/gallery/parity`; no tile changed, so none is retained).

The before/after pairs above were taken on the same base (`f95f6db8`). The receipts committed under FX10 and FX12 were
retaken after rebasing onto the integration tip `67f4761a`; their counters equal the pairs above, and their tracked
bytes include the other items merged since (FX10 embedded default: built 54,524, after the sweep 71,563; FX12 form-10k
built 9,133,973).
