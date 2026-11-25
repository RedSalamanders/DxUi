# W2.4 Popup paint and devices

Plan item `W2.4` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
contracts "Popup surfaces" and "Retained native device stack" in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#popup-surfaces), "Frame pacing" and
"Shared device stack" in [the Win32 host](../../../Specs/Rendering/Rendering_Win32Host.md#shared-device-stack), and
"Graphics and lifetime" in [embedded D3D11](../../../Specs/Rendering/Rendering_EmbeddedD3D11.md#graphics-and-lifetime).

- **Source**: the W2.4 commit on branch `pm/w2-4-popup-paint`, rebased on the integration tip before it was committed.
- **Build profiles**: x64 Debug for the numbers below (the tests also run in Release and ASan Debug); x64 Release for the
  gallery; the device-stack probe is a standalone x64 `/O2` program.
- **Adapters**: WARP for every test and gallery capture. The probe's hardware rows ran on the default adapter, an NVIDIA
  GeForce RTX 5080 (15,977 MiB dedicated; the machine also has an AMD Radeon iGPU).
- **Machine**: Windows 11 Pro 10.0.26300, shared with other workers' builds while every number was taken: times are rough.
- **Developer decisions (25 November 2026)**: keep the thread's native device stack while an `EmbeddedHost` is attached on
  the thread; keep a popup's backdrop blur as a bitmap ("Yes, keep bitmap"), accepting a one-level change in blurred popups.

## Why the blur is kept: the per-paint blur depends on the paint region

With W4.10's paint regions, a frame that repaints only a region over a blurred popup must equal the whole frame. The blur as
it was drawn before W2.4 (an effect made and drawn for each paint) does not: Direct2D's blur result depends on the paint
clip. `TestPerPaintBlurRegionEvidence` (popup at (40,32)-(280,232) DIP over striped content, a declared 40 by 40 DIP control
at (230,20)-(270,60) invalidated alone) records it:

| Per-paint blur | Region frame against the whole frame |
| --- | --- |
| Mica dark, 96 DPI | 12 pixels, up to 1 level, x 251 to 253, y 27 to 48 (W4.10's verifier reports the same frame) |
| Acrylic dark, 144 DPI | 214 pixels, up to 1 level, x 376 to 404, y 43 to 68 |

A `Solid` popup's shadow passes over content (the `ComboBox` list) show the same kind of dependence, before and after W2.4
alike, so it is a W4.10 finding and not W2.4's change. The same test, with a declared region on the popup's bottom-right
corner where the shadow spreads, records:

| `Solid` shadow passes, dark | Region (230,20)-(270,60) DIP | Region (260,210)-(300,250) DIP |
| --- | --- | --- |
| 96 DPI, per paint (before W2.4) | 0 pixels | 6 pixels, up to 1 level, x 277 to 297, y 221 to 233 |
| 96 DPI, retained (W2.4) | 0 pixels | the same 6 pixels |
| 144 DPI, per paint and retained | 0 pixels | 0 pixels |

The two passes cannot be kept as a bitmap over content without changing `Solid` pixels (see below), so this stays open for
W4.10.

Kept as a bitmap, the blur draws the same pixels in every frame: `TestPopupBlurRegionFramesEqualWholeFrames` (Mica and
Acrylic, 96 and 144 DPI, with the verifier on, whose reference paint draws from what the popup kept) finds 0 differences and
0 verifier mismatches.

## Which retained form is exact over which target

Measured first with a standalone Direct2D probe on WARP at 96, 144 and 192 DPI (shadow passes as `DrawDropShadow` draws
them; a 24 DIP Gaussian blur in a rounded layer of opacity 0.62), each retained form against drawing the effect directly:

| Retained form | Over a cleared, transparent target | Over opaque content |
| --- | --- | --- |
| Both shadow passes rendered into one 8-bit bitmap, drawn back | exact | differs by 1 level (25,713 to 104,561 bytes) |
| One 8-bit bitmap per shadow pass | exact | differs by 1 level (103 to 588 bytes) |
| `D2D1_PROPERTY_CACHED` on the shadow effect | not measured | differs by 1 level (99 to 449 bytes) |
| One 16-bit or 32-bit float bitmap per pass | exact | differs by up to 2 levels (4,268 to 28,454 bytes) |
| The blur rendered into a bitmap, drawn inside the layer | exact in the probe | exact in the probe |

A direct pass blends the effect's float output into the target once; a retained form stores that output first. Over
transparent pixels the blend is a copy, so the stored value is the result. Inside DxUi every retained form of the blur
differed from the whole-frame per-paint blur by one level in the column the region evidence shows; the reason is the clip
dependence above, not the retained form. Direct2D's batching also moves a level on a few pixels: a retained shadow pass is
preceded by a flush (the per-paint pass's mask recording flushed the batch), and a `Solid` popup's paint that has to make a
retained object draws the per-paint way.

What ships:

| Popup | Blur | Shadow | Pixels against the build before W2.4 |
| --- | --- | --- | --- |
| `Solid` menu popup (cleared composition surface) | none | bitmap | identical |
| `Solid` `ComboBox` list (over content) | none | two passes, retained objects | identical |
| Blurred menu popup (`Mica`, `MicaAlt`, `Acrylic`) | bitmap | bitmap | within 1 level |
| Blurred `ComboBox` list | bitmap | bitmap | within 1 level |
| `PaintTransientSurface` with a backdrop | bitmap | solid rings, not an effect | within 1 level (0 measured) |

## Pixel change of blurred popups

`DxUi.EmbeddedTests.exe --test-popup-surfaces` ([output](popup-surfaces-debug.txt)), x64 Debug, WARP. Every number is the
pixels that differ from the code before W2.4 and the largest level difference; nothing differs by more than one level.

- Probe over content (the `ComboBox` case), popup 240 by 200 DIP: Mica 3,022 to 28,453 pixels, MicaAlt 4,002 to 20,619,
  Acrylic 3,368 to 18,026 (96 to 192 DPI, dark and light), all within the popup and its shadow, all at most 1 level.
- Probe over a cleared surface (the menu case): 0 pixels everywhere except MicaAlt at 96 DPI (178 dark, 152 light, 1 level).
- A real `ComboBox` list in an embedded view: Mica 27 / 419 / 97, MicaAlt 9 / 26 / 12, Acrylic 85 / 527 / 1,307 pixels at
  96 / 144 / 192 DPI, all 1 level.
- `PaintTransientSurface`: 0 pixels.
- `Solid`: bit-identical everywhere (the probe at three scales, dark and light, and the `ComboBox` at three scales).

Gallery: no gallery tile shows a blurred popup (every gallery theme uses the default `Solid` overlay material), so the
`ComboBox`, `MenuBar` and `Menu` tiles must stay bit-identical and no baseline changes. Captured with
`./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles ComboBox,Menu,MenuBar -Warp` on the integration tip
`4be928d9b` (the base this commit was rebased on) and with this commit, `gallery.ps1 -CompareBefore <base> -CompareAfter
<this commit>` finds all 135 tiles identical (45 at each of 100, 150 and 200%). The [G1 receipt](../G1/receipt.json) is no
base for these tiles any more: items merged since G1 changed 35 of the 45, closed ones included, before W2.4.

## Steady popup frames

"Before" is read from the code before W2.4; "after" is measured on FX2's 80 steady popup frames and the probe.

| Steady frame, per frame | Before W2.4 | After |
| --- | --- | --- |
| `Solid` `ComboBox` (FX2): effects, command lists, layers, geometries, stop collections made | 2, 2, 0, 0, 2 | 0 |
| `Solid` `ComboBox` (FX2): shadow passes | 2 | 2 (not met: no exact retained form over content) |
| `Acrylic` `ComboBox` (FX2): effects, command lists, layers, geometries, stop collections made | 3, 2, 1, 1, 2 | 0 |
| `Acrylic` `ComboBox` (FX2): blur passes, shadow passes | 1, 2 | 0, 0 |
| Probe, every material over a cleared surface (the menu): shadow passes | 2 | 0 |

Native, `DxUi.ControlTests.exe --no-activate --suite=WindowHost`: a host whose first frame `RenderInitialFrameForShow`
presented renders once per open (1 render, 1 present, the show's WM_PAINT validated); a change before the show gives exactly
2 renders; a window hidden and shown again renders once; no present without a credit and no pacer invariant violation; the
pacer holds no credit, armed wait or deferred region after the open.

## Native device stack (G10)

Method: [device-stack-probe.cpp](device-stack-probe.cpp) (output: [device-stack-probe.txt](device-stack-probe.txt)), a
standalone probe of `EnsureSharedWindowHostGraphicsResources` and `EnsureSharedWindowHostCompositionDevice` (D3D11 device with
BGRA support and no debug layer, DXGI factory, Direct2D factory and device, shared DirectWrite factory,
`DCompositionCreateDevice2`), then one popup session as `MenuPopup` attaches: a hidden, never shown 420 by 560
`WS_EX_NOREDIRECTIONBITMAP` window, a flip-sequential premultiplied composition swap chain with a latency-1 waitable object,
a DirectComposition target and visual, a Direct2D context, one frame, `Present` and `Commit`. "No stack" creates the stack
and the session; "kept stack" makes only the session. 11 sessions per row, medians, three runs. Idle cost: process private
bytes and the adapter's video memory use after creating a stack and one session and releasing the session.

| Adapter | Attach to first present, no stack | With a kept stack | Idle stack: private bytes | Dedicated video | Non-local video |
| --- | --- | --- | --- | --- | --- |
| NVIDIA GeForce RTX 5080 | 103.9 to 105.7 ms (min 99.2, max 120.0) | 1.5 to 1.6 ms | +14.7 to +17.6 MiB | +7.5 to +8.5 MiB | +1.0 MiB |
| WARP | 3.8 to 3.9 ms | 0.5 to 0.7 ms | +1.2 to +1.8 MiB | 0 | 0 |

DxUi's Debug build adds `D3D11_CREATE_DEVICE_DEBUG`, so a Debug creation costs more.
`TestSharedDeviceStackKeptAcrossEmbeddedOnlyMenuSessions` (WindowHost suite, WARP, a hidden popup-like window attached as
`MenuPopup` attaches, never shown): the first of eleven sessions creates 1 D3D11, 1 Direct2D and 1 DirectComposition device,
sessions 2 to 11 create none. The real FX4 ten-session count runs only in the interactive Menu suite and is not part of this
receipt.
