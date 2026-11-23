# W9.1 size probes and G13 budget lines

Plan item `W9.1` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: [memory metrics and G13 budget lines](../../../Specs/Core/Core_PerformanceAndResources.md#g13-budget-lines).
This README summarizes; the measured values are in the fixture receipts it links, which the fixture framework writes.

## Size probes

Measured on 2026-11-14 by compiling the library in all six configurations with a deliberately incomplete template
instantiated at `sizeof(T)` (the compiler error names each size); the probes then pin each type at that size. x64 and
ARM64 agree in every configuration; Debug and ASan Debug agree (Debug STL proxies).

| Type | Release x64 / ARM64 | Debug, ASan Debug x64 / ARM64 | Probe | G13 line served | Lowered by |
| --- | --- | --- | --- | --- | --- |
| `Control` | 296 / 296 | 336 / 336 | `Embedded.Tests.MemoryMetrics.h` | per simple control | `W9.6` |
| `Label` | 448 / 448 | 496 / 496 | same | per simple control | `W9.6` |
| `Button` | 816 / 816 | 880 / 880 | same | per simple control | `W9.6` |
| `Toggle` | 904 / 904 | 984 / 984 | same | per simple control | `W9.6` |
| `Checkbox` | 912 / 912 | 992 / 992 | same | per simple control | `W9.6` |
| `RadioButton` | 864 / 864 | 928 / 928 | same | per simple control | `W9.6` |
| `ThroughputGraphSample` | 408 / 408 | 408 / 408 | same | per control (180 samples per graph) | no item yet |
| `AccessibilityControlNavigationSnapshot` | 936 / 936 | 1,088 / 1,088 | `src/Controls/DxUi.Accessibility.cpp` | snapshot residency | `W9.7` |

Every plan hand count in W9.1's "Why" matches the measured Release size.

## G13 lines

Each receipt carries `budgetLines.G13` and the per-cache rows (`memory.retainedCaches`). Receipts, written by
`performance-fixtures.ps1` in x64 Release (MSVC 19.52.37025) on 2026-11-14, source `9f6fdc74` (the W9.1 commit rebased on integration
tip `f95f6db8`, before these receipts were added to it; its code is the commit that adds this file), Windows build 26300, WARP
10.0.26100.9711 and an NVIDIA GeForce RTX 5080:

- FX5: [WARP](../FX5/fx5-default-embedded-warp-x64-release.json),
  [hardware 1280x720](../FX5/fx5-default-embedded-hardware-x64-release.json),
  [hardware 3840x2160 at 192 DPI](../FX5/fx5-uhd-192dpi-embedded-hardware-x64-release.json).
- FX13: [WARP](../FX13/fx13-default-embedded-warp-x64-release.json),
  [hardware 1920x1080](../FX13/fx13-minimized-hardware-embedded-hardware-x64-release.json).

| Line | Budget | FX5 (WARP / hardware / hardware 4K) | FX13 (WARP / hardware) | Status, closing item |
| --- | --- | --- | --- | --- |
| `idle-private` | 8,000,000 B | 45,862,912 / 88,322,048 / 145,514,496 | 56,848,384 / 89,587,712 | advisory, never judged |
| `minimized-video` | 8,388,608 B | not measured / 17,076,224 / 76,058,624 | not measured / 26,906,624 | recorded exception, `W9.2` |
| `per-control` | 256 B | 640 (53,149 tracked bytes / 83 controls) | 973 (34,085 / 35) | recorded exception, `W9.6` |
| `per-grid-cell` | 1,000 B | not measured (WARP) / 20,965 / 20,878 (12 cells) | not measured: no Grid | recorded exception, `W9.4` (FX5) |
| `text-history` | 4,194,304 B | not measured: no editor | 0 (focused field, no edits) | met (FX13) |

Two Debug and two Release in-process runs of the gate tests gave per-control 633-798 B (FX5) and 954-1,161 B (FX13): the
tracked bytes include the host attachment and differ with the configuration and with first-use state already created by
earlier tests in the process. The private-bytes line is the whole test process above its entry, not a 1080p native
window; it is reported as the advisory band the item names.

## Findings

- The `G13` lines `per-control`, `per-grid-cell` and `minimized-video` do not hold today; each is a recorded exception in
  `kG13Exceptions` (`Tests/Embedded/Embedded.Tests.PerformanceFixture.h`) naming its closing item. Deleting the row makes
  the gate enforce the budget; a row on a line that meets its budget fails the gate.
- A single-line Grid cell holds about 21 KB on hardware (rebuild of 12 cells: 250,542-251,580 heap bytes), in line
  with the plan's 20 KB figure and the 17 KB-per-layout measurement of the
  [described-menu study](../../MenuDescriptions/2026-09-30/row-layout/README.md). Hiding returns far less than the
  rebuild costs (3,424 bytes on hardware): released DirectWrite storage stays busy in the process heaps, which is the
  heap retention `W9.5` addresses.
- On WARP the live-heap probe is not stable: WARP's own threads allocate in the same heaps while the cells are
  rebuilt. Five Release runs before the line was restricted to hardware measured 1,549 to 14,291 bytes a cell while
  hiding returned 47,160 to 71,624 (12 cells), so a WARP run could fall under the budget by chance. The line is
  therefore "not measured" on a software adapter and judged on hardware receipts, which agree to 0.5% (the WARP
  receipts still record the raw `gridCellRelease`). Under AddressSanitizer the probe sees no heap growth and the line
  also reports "not measured".
- The probe runs only after the last phase and touches nothing when a scene has no visible Grid. Receipts of FX3, FX8,
  FX9, FX10 and FX12 taken with it (default variants, WARP, kept under `.build`, not committed) show per-step series
  identical to the committed receipts in every phase.
- FX8's only editor is a `RichTextEditor`. `largestTextHistoryBytes` covers TextField and ComboBox, whose history entries
  hold whole texts; a rich document's history holds piece-tree snapshots that share storage with each other and the
  current text, and nothing in DxUi totals their exclusive bytes. FX8's `text-history` line therefore reads "not
  measured" with that reason, which is W9.3's to close.

## Private bytes above process entry: who holds them

From the samples of the FX13 and FX5 receipts above (`device` is taken after the fixture's Direct3D device exists and
before DxUi is built; private bytes are advisory and vary between runs by several MB on WARP):

| Interval | FX13 WARP | FX13 hardware | FX5 WARP | FX5 hardware |
| --- | --- | --- | --- | --- |
| entry to device (Direct3D runtime and driver or WARP, before any DxUi object) | 13.5 MB | 69.5 MB | 8.9 MB | 63.4 MB |
| device to built (host attach, Direct2D device context, DirectWrite factory, the tree) | 1.7 MB | 5.2 MB | 1.8 MB | 5.6 MB |
| built to first phase end (first frames: surface, Direct2D and DirectWrite first use) | 11.5 MB | 11.7 MB | 15.5 MB | 17.9 MB |
| later phases (idle; FX5 clean and dirty frames) | 30.2 MB | 3.2 MB | 19.7 MB | 1.3 MB |
| total above entry, after the last phase | 56.8 MB | 89.6 MB | 45.9 MB | 88.3 MB |

DxUi-owned, after the last phase: tracked C++ bytes 35,461 (FX13) and 58,173 (FX5), retained-cache bytes 1,696 and
5,344, and the surface, 8,294,400 (FX13) and 3,686,400 (FX5). On WARP the surface lives in system memory and is inside
private bytes; on hardware it is in local video memory, which rose 18.1 MB (FX13) and 8.2 MB (FX5) at the first frames.
So on WARP about 8.3 MB of FX13's 56.8 MB is DxUi's own (surface plus 37 KB), and on hardware about 37 KB of 89.6 MB;
the rest is held by the Direct3D runtime and driver or WARP, Direct2D and DirectWrite, much of it created on DxUi's
behalf (device context, glyph and geometry caches, command buffers) but not observable per owner from inside the
process. On WARP, FX13's idle phases add 30.2 MB while tracked bytes stay flat (35,429 to 35,461), all outside DxUi's
C++ allocations; hardware adds 3.2 MB over the same phases. Attributing the runtimes' share by heap is W9.5's heap walk.
- Hiding the view keeps the process's local video memory unchanged (17,076,224 bytes at 720p, 76,058,624 at 4K, 26,906,624
  for FX13 at 1080p), as W0.3 found.
- Registry resident bytes in FX5 after the dirty phase: 5,344 bytes over 34 entries (brush cache 1,696; Grid cell table
  3,424 for 12 entries; text formats 224) besides the 3,686,400-byte surface. `RichTextEditor.layout` has no byte count.
- Every `sizeof` hand count in W9.1's "Why" matched the Release measurement; x64 and ARM64 layouts are identical.
