- `gallery.ps1` captures individual gallery tiles at chosen display scales as review evidence. `-Scale 100,125,150,200`
  (100 to 300% in steps of 25) renders the same sheets through the same harness capture, non-activating, after giving
  every host, menu popups included, the requested DPI through `WM_DPICHANGED`; `-Tiles` (a control, a caption or a tile
  file name), `-Theme` (light, dark, rainbow-light, rainbow-dark, high-contrast) and `-Warp` select what is written and
  where it renders. Tiles go to `.build/gallery/<Platform>/<Configuration>/tiles/<percent>/<theme>/<tile>.png` with
  `receipt.json` (provenance) and `tiles.tsv` (pixel size and SHA-256 of the decoded pixels per tile and scale).
  `-CompareBefore`/`-CompareAfter` compares two captures, or a capture with a retained receipt, per tile and scale,
  lists the differing tiles and exits 1 on any difference. Without these options the published 100% sheets are
  unchanged and `docs/gallery` stays at one scale. Tooling tests cover the arguments, refusals, pixel digest, receipt
  and parity, including a seeded one-pixel difference; two Rendering-suite tests prove a 100% tile is today's sheet
  pixel for pixel and that 125% and 150% tiles have their frames' scaled pixel sizes. The first receipt, every tile of
  the five themes at 100, 125, 150 and 200% on WARP, is in `Measurements/PerformanceAndMotion_2026-11-13/G1`, and the `StatusStrip` and multiline
  `TextField` tiles at 100, 125 and 150% that `W1.5`'s gate names are in `W1.5` there.
