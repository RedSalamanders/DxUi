# W3.2 gallery tiles: focused editable and read-only TextField

Harness gallery capture for [W3.2](../README.md): the new tiles `textfield-focused` and `textfield-focused-read-only` in
the five themes at 100, 150 and 200% (`<percent>/<theme>/<tile>.png`). Each is a focused `TextField` in a host of its own,
with the caret phase frozen by a steady blink time (`INFINITE` through the host's caret test seam), so the caret is drawn
in its shown phase. `receipt.json` holds the provenance (base `c030d60c` with this change uncommitted, x64 Release, forced
WARP, DPIs 96, 144 and 192, input hashes) and `tiles.tsv` the decoded-pixel SHA-256 of each tile. The gallery draws
through the native `HwndSwapChain` host.

```powershell
./gallery.ps1 -Configuration Release -Scale 100,150,200 -Tiles 'textfield-focused,textfield-focused-read-only' -Warp
```

Parity of every other tile against the [G1 receipt](../../G1/README.md) is in the [W3.2 README](../README.md#gallery).