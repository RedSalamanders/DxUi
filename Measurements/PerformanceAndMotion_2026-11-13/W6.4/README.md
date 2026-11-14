# W6.4 text rendering parameters

Plan item `W6.4` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); the
rules and observations are recorded in [theme and typography](../../../Specs/UI/UI_ThemeAndTypography.md#text-antialiasing-and-rendering-parameters).

- Source: the item's commit, first on base `c128bdb1` (the "before" build is `c128bdb1` with only the new test file
  added), then rebased on `9df9b1b3`, where the FX5 hashes below were reproduced.
- Build: x64 Debug, MSVC 14.52.37025; Windows 11 Pro 10.0.26300.
- Adapter: WARP (Microsoft Basic Render Driver) for every test below; the `gallery.ps1` comparison ran on the default
  hardware adapter of the same machine (AMD Radeon / NVIDIA GeForce RTX 5080 present).
- The user's real setting during every run: `SPI_GETFONTSMOOTHING` = 1, `SPI_GETFONTSMOOTHINGTYPE` = 2 (ClearType). No
  test changed it; ClearType, Standard and off were supplied through `DebugSetFontSmoothingForTest`.

## Default antialias mode per target (`TestTextAntialiasDefaultsFollowTheTargetAlphaMode`)

A fresh device context reports `D2D1_TEXT_ANTIALIAS_MODE_DEFAULT` and no rendering parameters. The probe string drawn
with that default into a B8G8R8A8 target is byte-identical to explicit `CLEARTYPE` on an `IGNORE` target (820 chromatic
pixels) and to explicit `GRAYSCALE` on a `PREMULTIPLIED` target (0 chromatic pixels). `IDWriteFactory::CreateRenderingParams`
and `CreateMonitorRenderingParams(primary)` both report gamma 1.8, enhanced contrast 0.5, ClearType level 1, RGB pixel
geometry, rendering mode `DEFAULT`.

## Parameter and antialias compatibility (`TestTextRenderingParamsCompatibilityWithAntialiasModes`)

Custom parameters (gamma 1.8, contrast 0.5, ClearType level 1 and 0, RGB) for each rendering mode, drawn under each
antialias mode on `IGNORE` and `PREMULTIPLIED` targets; `EndDraw` results (identical for both targets and both levels):

| Rendering mode | DEFAULT | CLEARTYPE | GRAYSCALE | ALIASED |
| --- | --- | --- | --- | --- |
| DEFAULT | S_OK | S_OK | S_OK | S_OK |
| ALIASED | S_OK | E_INVALIDARG | E_INVALIDARG | S_OK |
| GDI_CLASSIC, GDI_NATURAL, NATURAL, NATURAL_SYMMETRIC | S_OK | S_OK | S_OK | E_INVALIDARG |
| OUTLINE | S_OK | E_INVALIDARG | S_OK | S_OK |

The hosts' predicate `TextAntialiasModeAcceptsRenderingMode` matches every cell, and a rejected set replaced through
`MakeTextRenderingParamsFor` draws in every cell.

## Pixels with the default setting: before and after

FNV-1a 64 of each FX5 capture (`TestTextRenderingComplexUiCapturesForEachHost`, 1280 x 720, 96 DPI), identical on the
before and after builds:

| Capture | Before = after |
| --- | --- |
| `hwnd-dark-96dpi` | `0xAEDE74BE5EBED69C` |
| `composition-dark-96dpi` | `0xB7476CE1D8BEC3D6` |
| `embedded-dark-96dpi` | `0x867A2DAE567318B2` |
| `hwnd-light-96dpi` | `0xAE6ADB4413AF706C` |
| `composition-light-96dpi` | `0x2E128D34E73A48E7` |
| `embedded-light-96dpi` | `0x9B297D359290BC47` |

`gallery.ps1 -SkipBuild` on both builds produced six byte-identical sheets (SHA-256 of `theme-controls-dark.png`
`6690834F...F778`, `theme-controls-light.png` `470AB3BA...C3A0`, `embedded-controls.png` `86D2FE80...1240`; all six
compared). `TestTextRenderingDefaultSettingKeepsTodaysPixels` repeats the parity in one process for both presentation
modes and both themes.

## Settings through the seam

`hwnd-dark-96dpi-standard.png` is the opaque chain with ClearType off (Standard and off draw the same grayscale frame,
`0x726F86875C34BD30`); with ClearType it equals `hwnd-dark-96dpi`. The composition chain draws `0xB7476CE1D8BEC3D6` under
all three settings. Every FX5 frame in both presentation modes and embedded presented or prepared without failure, and
each steady frame made 0 `SetTextRenderingParams` calls and created 0 parameter sets.

## Captures

The PNGs here are harness captures (`DebugCaptureBitmap` for the native modes, embedded composition readback) at 96 DPI:
light-on-dark (`*-dark-*`) and dark-on-light (`*-light-*`). The composition captures are the premultiplied back buffer
before Present.

`gallery-tiles/` holds the gallery's own scaled capture of four text tiles (`label-body`, `textfield-single-line`,
`grid-cell-variants`, `tree-hierarchy`) in the light and dark themes at 100, 150 and 200% (96, 144 and 192 DPI), x64
Release, forced WARP, source `d0d07184` (this item rebased on `9df9b1b3`; `receipt.json`, `tiles.tsv`):

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'label-body,textfield-single-line,grid-cell-variants,tree-hierarchy' -Theme light,dark -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json -CompareAfter .build/gallery/x64/Release/tiles `
    -Scale 100,150,200 -Tiles 'label-body,textfield-single-line,grid-cell-variants,tree-hierarchy' -Theme light,dark
```

All 24 tiles are identical to the [G1 receipt](../G1/README.md) ([gallery-tiles/README.md](gallery-tiles/README.md)): with the default setting this item
changes no pixel at 96, 144 or 192 DPI. The gallery renders through the native `HwndSwapChain` host only; the
composition chain and the embedded view are captured at 96 DPI by the tests above.
