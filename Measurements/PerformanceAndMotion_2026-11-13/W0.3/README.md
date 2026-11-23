# W0.3 memory metrics: first receipts

Plan item: `W0.3` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: [memory metrics](../../../Specs/Core/Core_PerformanceAndResources.md#memory-metrics).

## Scenario and provenance

- Fixture: `dxui-complex-ui-memory-v1`, the FX5 scene (83-control ComplexUi, 1280x720, 96 DPI, offscreen, reduced
  motion): device, scene build, 20 warm frames, 200 scrolling frames, shown, hidden; each sample after a context flush.
- Source: the W0.3 commit on integration tip `fda11e04` (`baseCommit` in each receipt; the commit that adds this file).
  `executableSha256` and `benchmarkInputs` identify the measured executable and fixture sources.
- Build: x64 Release, MSVC 19.52.37025, `build.ps1 -Configuration Release`.
- Machine: AMD Ryzen 9 9950X3D, Windows 10.0.26300, shared with other builds while measured.
- [`complex-ui-memory-warp-x64-release.json`](complex-ui-memory-warp-x64-release.json):
  `DxUi.EmbeddedTests.exe --memory-receipt <path>`, WARP (Microsoft Basic Render Driver).
- [`complex-ui-memory-hardware-x64-release.json`](complex-ui-memory-hardware-x64-release.json):
  `DxUi.EmbeddedTests.exe --memory-receipt-hardware <path>`, NVIDIA GeForce RTX 5080 (16 GiB dedicated).
- Provenance fields (`platform`, `configuration`, `baseCommit`, `executableSha256`, `benchmarkInputs`, `machine`,
  `cpu`, `os`, `recordedUtc`) were added to the native output by the recording PowerShell session.

Private bytes and working set are advisory. Nothing here is timed.

## Memory fields

| Field | WARP | Hardware |
| --- | --- | --- |
| Tracked live C++ bytes: scene / warm / scroll | 53,125 / 58,149 / 58,149 | 53,125 / 58,149 / 58,149 |
| Retained text layouts / UTF-16 units (warm, scroll, shown) | 12 / 96 | 12 / 96 |
| Retained bitmaps / bytes | 0 / 0 | 0 / 0 |
| Cached brushes / configured text formats | 20 / 2 | 20 / 2 |
| Surface bytes (shown) | 3,686,400 | 3,686,400 |
| Local video usage: device / scene / warm / scroll / shown / hidden | not applicable | 8,855,552 / 8,855,552 / 17,076,224 / 17,076,224 / 17,076,224 / 17,076,224 |
| Non-local video usage: same phases | not applicable | 909,312 / 909,312 / 9,596,928 / 10,825,728 / 10,825,728 / 10,825,728 |
| Hidden delta: surface bytes / retained layouts | -3,686,400 / -12 | -3,686,400 / -12 |
| Hidden delta: private bytes (advisory) | 0 (+8,192 and +12,288 in earlier runs) | 0 |
| Hidden delta: local / non-local video | not applicable | 0 / 0 |
| `sizeof` Control / Label / Button / Toggle / Checkbox / RadioButton | 296 / 448 / 816 / 904 / 912 / 864 | same |

Debug and ASan Debug sizes (Debug STL proxies): 336 / 496 / 880 / 984 / 992 / 928 bytes.

## Observations

- Scene build holds 53,125 tracked bytes for 83 controls, about 640 bytes per control before painting, and
  `sizeof(Label)` alone is 448 bytes in Release: above the `G13` target of 256 bytes per simple control all-in. Under
  `A7` this receipt contradicts that target; `W9.6` must recover it or the plan must restate it.
- Warm-up adds 5,024 tracked bytes and 200 scrolling frames add none: no C++ growth while scrolling.
- FX5's only retained layouts are 12 single-line Grid cell layouts; plain Labels, Toggles, Sliders, ProgressBars and
  the Tree keep none. Hiding returns the Grid's layouts and the surface.
- On the hardware adapter, hiding releases the 3,686,400-byte surface (the view reports 0) yet neither local nor
  non-local video usage falls after a flush: the released allocation stays resident. Private bytes do not fall on WARP
  either. This is the residency `W9.2` addresses with trimming.
- Local video usage rises 8.2 MB and non-local 8.7 MB (9.9 MB by the end of scrolling) from scene to warm-up, more
  than the surface: Direct2D's own device resources make up the rest and are not attributed here.
- Not measured here: native-window minimized residency (an embedded view is minimized by hiding it; native windows need
  the `W0.4` fixtures), RichTextEditor text layouts, and per-object DirectWrite and Direct2D bytes, which those APIs do
  not expose.

Teardown accounting is a functional test, not a receipt: `Embedded.Tests.MemoryMetrics.h` proves that a second FX5
lifetime leaves 0 live tracked bytes in Debug, Release and ASan Debug, and 0 DxUi-created live device objects on a
D3D11 debug-layer WARP device in Debug and ASan Debug (the fixture's own 3 objects remain).
