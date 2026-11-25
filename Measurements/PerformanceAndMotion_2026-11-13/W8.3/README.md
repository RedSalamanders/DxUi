# W8.3 Build cost — before and after

Plan item W8.3 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior in
[input and accessibility](../../../Specs/UI/UI_InputAndAccessibility.md#record-build-cost).

- **Source**: "before" is the integration base `db139338d` (after W8.2, W8.5 and W9.1); "after" is this item's commit, rebased
  on `9c442ed25`. **Build**: x64 Release, MSVC 14.52. **Adapter**: WARP (Microsoft Basic Render Driver). **OS**: Windows 11
  build 26300. Shared desktop: other workers' builds ran beside every figure, so the timings are indicative, not paired.
- **Gate**: `DxUi.EmbeddedTests.exe --test-accessibility-build-cost` (`Tests/Embedded/Embedded.Tests.AccessibilityBuildCost.h`,
  also run by the Embedded suite) on the `FX12` (`form-10k`) and `FX3` (`default`) definitions and on FX12's form shape at
  2,000 and 10,000 controls, with one client (the client gate's override), every incremental publication verified against a
  whole build of the same tree. The old path is the test seam `DebugSetAccessibilityLegacyRecordReadsForTest`, which reads
  records as before W8.3, counted the same way.
- **Layout cost**: `DxUi.EmbeddedTests.exe --measure-incremental-publication`, the scenario of the
  [W8.2 receipt](../W8.2/README.md#layout-cost-and-coalescing-review-f3) (FX12's form shape: one `ScrollPanel` of rows of a
  `Label`, `TextField`, `Checkbox`, `Toggle` and `Button`; 1920x1080 at 96 DPI; stopped application clock; the
  `UpdateAccessibility` call alone, median of its frames, with its C++ allocations). "Before" ran the base's binary.

## Gate (x64 Release; Debug and ASan Debug give the same counts)

| `FX12` 10,000-control form, one client, one whole build | Before (legacy reads) | After |
| --- | --- | --- |
| Label-lookup walks (`accessibility.labelLookupWalks`) | 6,000 (one per field, check box and toggle) | **0** |
| `dynamic_cast` control type tests (`accessibility.dynamicCasts`) | 60,624,094 | **0** |
| Text control documents copied into records (`accessibility.textCopies`) | 4,000 (each field's value and text) | **0** |

| `FX3` 100 wheel notches over the Grid, one client | Before | After |
| --- | --- | --- |
| Whole (structural) builds | 100 | **0** |
| Records re-read per publication (one scroll scope: the Grid) | — (whole) | **1** |
| Incremental publications verified against a whole build | — | 100, 0 differences |

`FX12`'s wheel phase over its `ScrollPanel`: after the first notch, which also moves the pointer off the button the warm
phase hovered (that button's hover repaint declares nothing, so that publication is whole under the W8.2 rules), each
notch publishes incrementally, re-reads no record, rebuilds no rectangle and recomputes the one scroll scope.

| Layout moving every control, one incremental publication | 2,000 controls | 10,000 controls |
| --- | --- | --- |
| Records re-read (the text fields) | 400 | 2,000 |
| Children read to resolve them (`accessibility.pathChildReads`), upward search (legacy) | 430,911 | 10,154,911 |
| Same, down each record's path (after) | **831** | **4,031** |
| Controls a whole build reads | 2,002 | 10,002 |

## Layout cost (x64 Release, WARP, one client)

| | 2,000 controls before | 2,000 after | 10,000 before | 10,000 after |
| --- | --- | --- | --- | --- |
| Whole build | 38.7 ms, 1,291 allocations | **1.3 ms, 901** | 1,453 ms, 6,341 | **7.9 ms, 4,355** |
| Frame moving every control, whole | 41.2 ms, 1,290 | **1.5 ms, 900** | 1,428 ms, 6,340 | **8.0 ms, 4,354** |
| Frame moving every control, incremental | 12.1 ms, 1,292 | **0.58 ms, 901** | 518 ms, 6,346 | **4.2 ms, 4,359** |
| Records re-read per incremental frame | 400 | 400 | 2,000 | 2,000 |

The whole build was dominated by the label walks (6,000 walks of 10,000 controls with a `dynamic_cast` per control). A frame
that moves every control is still incremental: its marks collapse onto the `ScrollPanel`, whose geometry re-reads each text
field's record (its selection rectangles follow its own bounds) and rebuilds the rectangle table. Two other runs of the same
binary on the shared desktop gave 10,000-control figures of 10.4 ms whole and 4.9 ms incremental, and 25.8 ms and 8.3 ms
beside an ASan build: in every run the incremental frame costs about half of the whole one.

### Where an incremental frame's time went (10,000 controls, median of 20 publications)

A temporary timed breakdown of the publication (removed before the commit), first with the successor finding each re-read
control's path upward (the reads of this item before its last change, at 26.8 ms per frame against 9.4 ms whole):

| Successor step | Upward path search | Down each record's path |
| --- | --- | --- |
| Marks: each mark's path and the records it covers | 0.54 ms | 0.24 ms |
| Resolving and checking each re-read control | **19.5 ms** | 0.25 ms |
| Reading the 2,000 records | 1.59 ms | 1.16 ms |
| Sharing the record spine | 0.17 ms | 0.13 ms |
| Rectangles, scroll scopes and focus | 1.45 ms | 0.98 ms |
| Successor in all | 23.7 ms | **2.9 ms** (whole build: 5.5 ms) |
| Event raise | 0.79 ms | 0.68 ms (whole: 1.6 ms) |

The upward search read each parent's children to find a control's index: one parent of 10,000 children made about 10^7
child reads per publication. A successor now reaches each record's control down its stored path, as a whole build does,
and finds a marked control by its address in the previous generation before following its path, with no field added to any
control.

## Record size

| x64 | Release | Debug, ASan Debug |
| --- | --- | --- |
| Hot core (`AccessibilityControlNavigationSnapshot`; was 936 B / 1,088 B) | **376 B** | 408 B |
| Text part (a text field's text, selection, rich text, reveal button) | 208 B | 224 B |
| Range part (slider, progress bar, splitter, page indicator) | 40 B | 40 B |
| Grid part | 232 B | 296 B |
| Tree part | 104 B | 120 B |
| Tab part | 32 B | 40 B |

A record of a label, button, check box, toggle or radio button is the core alone (376 B, below W9.7's 512 B); a part adds its
size plus one shared-pointer control block. The size probe beside the definition is lowered to 376 B (Release) and 408 B
(Debug).

## Logs

Under `.build/w83` of the worker's tree (not archived): `gate-{Debug,Release,ASanDebug}.log`, `sweep-*.log`,
`measure-before-release.log`, `measure-after-release.log`, `measure-after-release-final.log`, `timing-before-fix.log`,
`timing-after-fix2.log`, `parity-seed-*-final.log`.
