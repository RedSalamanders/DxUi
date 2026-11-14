# W1.5 gallery tiles at 100, 125 and 150%

The harness gallery captures that the `W1.5` gate of the
[performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md) names and that could not be
produced when `W1.5` landed: the `StatusStrip / Sections` and `TextField / Multiline` tiles at 100, 125 and 150% in the
five gallery themes (30 tiles), after `W1.5`'s cluster-safe status elision and multiline selection width.

- `<percent>/<theme>/<tile>.png`: the captured tiles.
- `receipt.json`: provenance. Integration tip `c128bdb1` (which contains `W1.5`) with the gallery-scales change (G1) applied
  as one commit (`sourceCommit`, `sourceDirty: false`; the final commit differs only by its message; the hashes of the
  capture inputs are in `benchmarkInputs`), x64 Release, adapter WARP (forced), DPIs 96, 120 and 144.
- `tiles.tsv`: pixel size and SHA-256 of the decoded pixels of each tile.

| Tile | 100% | 125% | 150% |
| --- | --- | --- | --- |
| `statusstrip-sections` | 436 x 96 | 545 x 120 | 654 x 144 |
| `textfield-multiline` | 214 x 96 | 266 x 120 | 320 x 144 |

Scenario: the Gallery suite of `DxUi.ControlTests.exe` renders each theme's sheet offscreen, without activation, after
giving every host the requested DPI through `WM_DPICHANGED`, and crops the two tiles. Only library-owned synthetic
content is drawn. Command:

```powershell
./gallery.ps1 -Configuration Release -Scale 100,125,150 -Tiles 'StatusStrip','TextField / Multiline' -Warp
```

The same 30 tiles are pixel-identical to those of the all-tiles receipt taken from the same build
([G1](../G1/README.md)):
`gallery.ps1 -CompareBefore ../G1/receipt.json -CompareAfter . -Scale 100,125,150 -Tiles StatusStrip,textfield-multiline`
reports 30 of 30 identical.
