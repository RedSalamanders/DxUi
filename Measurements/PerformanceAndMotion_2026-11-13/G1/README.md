# Gallery tiles at four display scales (G1)

The first scaled gallery receipt of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md):
every one of the 84 gallery tiles in the five themes at 100, 125, 150 and 200% (1,680 tiles). Later items compare their
captures with it instead of with images; the images themselves are not retained.

- `receipt.json`: provenance. integration tip `c128bdb1` with this change applied as one commit (`sourceCommit`,
  `sourceDirty: false`; the final commit differs only by its message; the hashes of the four capture inputs are in
  `benchmarkInputs`), x64 Release, adapter WARP (forced), DPIs 96, 120, 144 and 192.
- `tiles.tsv`: one line per tile and scale: scale, theme, tile file name, pixel width and height, and the SHA-256 of the
  PNG decoded to 32-bit BGRA rows (`Get-GalleryPixelDigest` in [GalleryCapture.psm1](../../../Tools/GalleryCapture.psm1)).

Scenario: the Gallery suite of `DxUi.ControlTests.exe` builds each theme's sheet, gives every host and menu popup the
requested DPI through `WM_DPICHANGED`, captures the sheet offscreen without activation and crops each tile's frame
(DIP edges rounded to the nearest pixel). Only library-owned synthetic content is drawn. Command:

```powershell
./gallery.ps1 -Configuration Release -Scale 100,125,150,200 -Warp
```

The same capture on the intermediate tip `fda11e04` was pixel-identical in all 1,680 tiles. On the earlier base `ac6db736` the same capture was taken twice (the second with `-SkipBuild`, 24 s) and
`gallery.ps1 -CompareBefore <first> -CompareAfter <second>` found all 1,680 tiles identical, so the capture is
deterministic on WARP. On the default adapter, the 100% sheets this change renders without tile options were byte-identical to
that base's (all six PNGs, x64 Release, before and after this change on the same machine).

To compare a later capture with this receipt, capture the tiles the gate names and limit the comparison to them:

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles StatusStrip -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json `
    -CompareAfter .build/gallery/x64/Release/tiles -Scale 100,150,200 -Tiles StatusStrip
```

Pixels depend on the WARP rasterizer and the Windows fonts of the machine (here Windows 11, build 26300); a difference
against this receipt from a different machine is first checked by recapturing the base there.
