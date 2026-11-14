# W6.4 gallery text tiles at 96, 144 and 192 DPI

Harness gallery capture for [W6.4](../README.md): the tiles `label-body`, `textfield-single-line`, `grid-cell-variants`
and `tree-hierarchy` in the light (dark-on-light text) and dark (light-on-dark text) themes at 100, 150 and 200%
(`<percent>/<theme>/<tile>.png`), with the user's unchanged ClearType setting. `receipt.json` holds the provenance (source
`d0d07184`, x64 Release, forced WARP, DPIs 96, 144 and 192, input hashes) and `tiles.tsv` the decoded-pixel SHA-256 of
each tile. The gallery draws through the native `HwndSwapChain` host.

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'label-body,textfield-single-line,grid-cell-variants,tree-hierarchy' -Theme light,dark -Warp
./gallery.ps1 -CompareBefore Measurements/PerformanceAndMotion_2026-11-13/G1/receipt.json -CompareAfter .build/gallery/x64/Release/tiles `
    -Scale 100,150,200 -Tiles 'label-body,textfield-single-line,grid-cell-variants,tree-hierarchy' -Theme light,dark
```

Parity against the [G1 receipt](../../G1/README.md): 24 tiles compared, 24 identical (8 of 8 at each scale), 0 differing.
