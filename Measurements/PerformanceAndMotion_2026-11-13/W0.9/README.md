# W0.9 presentation-mode comparison

Plan item `W0.9` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); assumption `A5`
(the default native mode stays `HwndSwapChain` until this receipt has been reviewed). The receipt is
[presentation-mode.json](presentation-mode.json). This page reports differences. It recommends no default: that is the developer's decision.

## What was run

- Scenario: `FX5` (the 83-control ComplexUi scene, default variant 1280x720 at 96 DPI, after its 20 warm-up frames) and, as the
  text-heavy page, `FX1`'s multiline page (a 64 Ki-unit page of prose in a `TextField`, 800x600 at 96 DPI; the plan's other text fixture,
  `FX8`, is embedded-only because its preparation settles over many owner turns). Each runs on a `WindowHost` in `HwndSwapChain` and in
  `CompositionSwapChain` (both flip-sequential, two buffers, `W3.6` pacing; `HwndSwapChain`: alpha ignored, opaque target cleared to the window
  background; `CompositionSwapChain`: premultiplied, cleared to transparent, content of one DirectComposition visual).
- Adapters: WARP (Microsoft Basic Render Driver 10.0.26100.9818) and the default hardware adapter (NVIDIA GeForce RTX 5080, driver 32.0.16.1714).
- Runner: `DxUi.ControlTests.exe --presentation-modes` (`Tests/Controls/Controls.Tests.PresentationModes.cpp`), driven by
  [performance-presentation.ps1](../../../performance-presentation.ps1), one process per fixture, adapter and order of the two modes (both orders
  are run; the second order is used for memory and for determinism). The fixture is the framework's one definition (`OpenWindowFixtureSession` over the
  runner's `WindowInstance`); `FX5`'s window form builds `ComplexUiScene::BuildRoot` under the host. The definition still declares the embedded host only.
- Build: x64 Release, source `a1b8cd0b6` plus the uncommitted W0.9 change (the receipt's `sourceDirty` is true because it was taken before the commit
  that adds it). The window is non-activating and off-screen; the run blocked 0 activations and 0 focus moves.
- Pixels are read from the presented image: `PrintWindow` of the composed window over a black and over a white backdrop (and over the window
  background colour), which recovers the alpha each pixel presents exactly (`channelsDisagree` is 0). Each read is calibrated against the back buffer
  composed the way the mode composes it: 0 differing pixels in all eight runs.
- Parts complete on both adapters: pixels, cost, behaviour (six cases run or simulated) and scope. **Not run: a monitor move that changes DPI
  and a remote session** (below). **Simulated: the DPI change** (`WM_DPICHANGED` delivered to the host). **Not judged: all timing.**

## 1. Pixels

| Measured (presented image) | `HwndSwapChain` | `CompositionSwapChain` |
| --- | --- | --- |
| Text antialiasing, Direct2D mode on the context | `DEFAULT` (resolves to ClearType on an opaque target; the system setting is ClearType) | `DEFAULT` (resolves to grayscale on a premultiplied target, whatever the setting) |
| Text pixels with subpixel colour fringes, `FX1` text region | 49,242 of 53,548 | 0 of 41,008 |
| Same, `FX5` first card's label | 250 of 275 | 0 of 205 |
| Alpha presented, `FX5` (921,600 px) | 921,600 opaque | 819,552 opaque, 85,636 fully transparent, 16,412 partial |
| Alpha presented, `FX1` (480,000 px) | 480,000 opaque | 379,104 opaque, 96,976 fully transparent, 3,920 partial |
| Back buffer alpha byte | 255 everywhere, ignored by the presentation | meaningful, read by the compositor |

Same on WARP and hardware (alpha and fringe counts are identical to the pixel). The transparent area of `FX5` is the gutters between the cards
and around the Tree and Grid (46 fully transparent rows, 14 columns); what shows through it is whatever is behind the visual: the window's own GDI
layer, which DxUi does not paint, or the desktop for a window without a redirection bitmap. `HwndSwapChain` shows the theme's window background there.

Presented image of one mode against the other, over the window background colour (`comparison.pixels`):

| | `FX5` WARP | `FX5` hardware | `FX1` WARP | `FX1` hardware |
| --- | --- | --- | --- | --- |
| Pixels that differ | 24,324 of 921,600 (2.6%) | 110,334 | 54,462 of 480,000 (11.3%) | 151,525 |
| where the composition pixel is opaque | 24,086 | 24,086 | 54,429 | 54,429 |
| where it is partially transparent (rounded and antialiased edges) | 238 | 612 | 33 | 120 |
| where it is fully transparent | 0 | 85,636 | 0 | 96,976 |
| differ by 1 level | 238 | 86,248 | 33 | 97,096 |
| differ by 2-4 | 342 | 342 | 636 | 636 |
| differ by 5-16 | 2,199 | 2,199 | 4,581 | 4,581 |
| differ by 17-64 | 11,658 | 11,658 | 26,327 | 26,327 |
| differ by 65-255 (largest 142 and 158) | 9,887 | 9,887 | 22,885 | 22,885 |

Reading: the large differences (over 16 levels, 21,545 pixels on `FX5` and 49,212 on `FX1`) are text, spread over every card label, the Tree, the Grid
and the page text (`differenceGrid` in the receipt). Rounded-edge pixels: of the 16,412 partially transparent edge pixels of `FX5` only 238 (WARP) or
612 (hardware) differ at all, and only by 1 level; the edges of rounded shapes composed over the window background match the opaque target to
rounding. On hardware the 85,636 and 96,976 one-level differences are the fully transparent pixels: the GPU's 8-bit value of the float window background
differs by one level from the integer colour used as the backdrop of the read, which is not a difference a consumer sees through a real backdrop.
Nothing differs in the interiors of shapes.

## 2. Cost

- Counters, a full frame (`FX5`, `FX1`; WARP, hardware): every work counter is identical between the modes (`counterDifferencesPerFullFrame` is empty
  in all four cases), including renders, presents, text layouts, resource creations and `text.setTextRenderingParams`. One swap chain per host in each mode;
  a composition host also creates the DirectComposition device, target and visual, which no counter covers.
- Frame time, **not judged, machine busy** (host busy 31.2%, 9.8 of 32 logical processors against a cap of 8; 200 frames per mode; host frame
  metrics and UI-thread cycles around the paint, `DwmFlush` after each frame). Median total, render and Present in microseconds:

  | | `HwndSwapChain` | `CompositionSwapChain` |
  | --- | --- | --- |
  | `FX5` WARP | 1,156 (1,059 and 85) | 1,140 (1,046 and 81) |
  | `FX1` WARP | 1,578 (1,492 and 75) | 1,633 (1,536 and 84) |
  | `FX5` hardware | 779 (717 and 50) | 977 (901 and 66) |
  | `FX1` hardware | 1,409 (1,336 and 56) | 1,535 (1,459 and 64) |

  Composition is 0.99 to 1.25 times the HWND mode, hardware slightly more costly in both fixtures. No module sampling (kernel tracing is refused here,
  `A6`) and no GPU timestamps exist for the window host; the `W0.7` module split belongs to the embedded host. The command that judges this on a quiet machine
  is below.
- Input to present (`W0.8` trace provider; 40 wheel notches per mode, each sent with a frame credit held): exactly one input-received event and one present
  event per input in every run (40 of 40, 0 inputs without a present). Median, not judged: `FX5` hardware 1,136 us (HWND) and 1,256 us (composition),
  `FX1` hardware 1,739 and 1,854, WARP 1,465 and 1,545 (`FX5`), 1,872 and 2,047 (`FX1`); the refresh period was 8,333 us.
- Private bytes and video memory: **the growth belongs to the order, not the mode.** Whichever mode runs first in a process pays the one-time growth
  (WARP `FX5`: +23.4 MB for the first mode, +12.7 MB for the second in the same run; hardware `FX5`: +77 MB and +49 MB; swapping the order swaps the numbers). Video
  memory of the process on the hardware adapter is the same in both modes: 17,014,784 bytes local and 3.0 to 3.1 MB non-local on `FX5` (13,344,768 local on
  `FX1`; non-local 2.1 MB for HWND and 2.6 MB for composition, a 0.5 MB difference on `FX1` only). WARP has no video memory to query (it is in private bytes).

## 3. Behaviour (each case on both adapters, both fixtures; `behavior.cases` in the receipt)

| Case | `HwndSwapChain` | `CompositionSwapChain` |
| --- | --- | --- |
| Live resize, 12-step sweep through `SetWindowPos` | 12 renders, 12 presents, 0 swap chains created, 0 `ResizeBuffers` failures, swap chain equals client size every step, presented image equals a full render at the end | Identical counts and result |
| Between the resize and the host's next present (frame the compositor shows) | The old frame is **stretched** to the new client area (`FX5` grow 1280x720 to 1440x810: 430,552 pixels equal to the old image at the same coordinates; shrink: the larger frame scaled down); no gap | The old frame stays 1:1 at the top-left; growing leaves a **gap showing the backdrop** (`FX5`: 244,800 pixels outside the old client area plus the scene's own transparent pixels, 330,436 in all; `FX1`: 182,400 outside, 279,376); shrinking clips the old frame |
| Minimize and restore | 0 renders while minimized, 3 renders and 3 presents after restore, presented image equals the image before | Identical counts. After a restore the **first read of the presented image was stale in 2 of 4 runs** (`FX1` WARP 2,644 pixels, `FX5` hardware 13,961 pixels; right on the second read); 0 of 4 for `HwndSwapChain` |
| Covering window (a no-activation popup placed over the host's window) | 5 renders and 5 presents, every present `S_OK`, none with another status | Identical. The off-screen window is never reported occluded here: **no `DXGI_STATUS_OCCLUDED` was observed in either mode**, so occlusion handling is not exercised |
| Device loss (`DebugSimulateDeviceLoss`) | Recovers in 1 round: 1 swap chain and 59 resources (`FX5`) or 12 (`FX1`) created, presented image equals a full render | Identical counts and result |
| DPI change, simulated: `WM_DPICHANGED` 96 to 144 and back | 0 swap chains, 2 or 5 resources created, text antialias mode unchanged, presented image equals a full render at 144 and at 96 | Identical |
| Child `HWND` host (a `WS_CHILD` window, as `NativeMenuBar` attaches its host) | Attaches; 4 renders, 4 presents, 1 swap chain; the child presents 31,200 of 31,200 pixels opaque and the parent's print shows them | Attaches; same counts; the child presents 4,419 fully transparent and about 390 partial pixels, and the parent's print shows the parent's backdrop through the 4,419 (the child composes over its parent) |
| Monitor move that changes DPI | **Not run** | **Not run** |
| Remote session | **Not run** (`SM_REMOTESESSION` read 0) | **Not run** |

Manual steps for the two cases not run are in `notRun` in the receipt: for the monitor move, two monitors of different scale (or a scale change in
Settings with the window open), a small program that attaches in each mode, and a slow drag with a drop that straddles both; for the remote session, a
Remote Desktop session with `SM_REMOTESESSION` 1, both modes at several scales, looking at the adapter, the present results, text antialiasing and the transparent area.

## 4. Scope: hosts a consumer creates without naming a mode

Today the one default is `AttachOptions::presentationMode = HwndSwapChain` (`include/DxUi/DxUi.h`, the `AttachOptions` member initializer),
reached through `ControlHost::Attach(HWND)`, which forwards `AttachOptions{}` (`src/Controls/DxUi.WindowHost.cpp`). A consumer-style `Attach(hwnd)` run in this
process reports `HwndSwapChain`. Changing the default would reach:

1. every consumer window attached with `Attach(hwnd)` or with `AttachOptions` that set only `forceWarp` or `batchInitialRootChildren`;
2. the `NativeMenuBar` host the library creates as a `WS_CHILD` window under the owner (`src/Controls/NativeMenuInterop.cpp`, `Attach(hwnd)`), the child-`HWND` case measured above;
3. a never-attached `ControlHost` (no swap chain, member initializer only), and the test window of every `WindowHost` test (`AttachedHostWindow`).

It would not reach the context-menu and flyout popup windows (`MenuPopup`, `src/Controls/DxUi.Menu.cpp`), which already name `CompositionSwapChain`, or the
`EmbeddedHost`, which has no swap chain. The receipt lists each with the file and the line found at this commit.

## What a consumer relying on the default would see change

Text on opaque areas loses its ClearType colour fringes and becomes grayscale (11% of the pixels of the text page differ, 2.6% of the scene
`FX5`); whatever the scene leaves unpainted becomes transparent and shows what is behind the window instead of the theme's window background; while a
window is being resized the compositor shows a gap instead of a stretched frame; after a restore the first presented frame can lag by a compositor frame.
Work counters, swap-chain creation, device-loss recovery, minimize, DPI and child-window behaviour matched, and rounded edges matched to rounding. Cost differences
are within the busy-machine noise and not judged.

## Judged timing (to run in a hardware slot, quiet machine)

```powershell
./performance-presentation.ps1 -Adapter Hardware -Judged -TimingFrames 2000 -OutputRoot Measurements/PerformanceAndMotion_2026-11-13/W0.9/judged -Replace
```

It stops before measuring unless the W0.2 idle preflight admits the host, takes 2,000 frames per mode, and never runs `performance-timing.ps1` or switches the power plan
(run it from the High performance plan). `Evidence images` (presented images over each backdrop and the alpha plane) are written under
`.build/test-artifacts/presentation-modes/` by every run and are not retained here.

## Limits

One display, one machine, one hardware adapter (RTX 5080); the input and frame-time numbers come from a busy machine; the window is off-screen, so it
is never occluded and has no monitor of its own scale; the text-region fringe measure is a blend-line test, not a rasterizer readback.
