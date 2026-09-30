# Multiline frame-cost investigation, 29 September 2026

Where the multiline benchmark scenes spend their frames, measured before the grid cache was finished, for the
[review follow-ups](../../../../Specs/Plans/Done/ReviewFollowUps_2026-09-29.md). The question was whether a better text
layout cache could raise their dirty frame rate. It cannot by itself: the frame is dominated by drawing color-font
text, which no layout cache avoids. The distinct scene has since kept the Tree's short names, so its frame measures
the grid; the [paired runs](../paired-local/README.md) use it.

- Each row is one `DxUi.EmbeddedTests.exe --benchmark-multiline-grid-distinct` run (five 40-frame rounds each for
  clean and dirty content, 1280x720, 96 DPI, WARP; medians of the rounds) of an x64 Release build carrying a
  temporary experiment switch read from an environment variable. The switches were reverted and are not in the
  source, so the runs cannot be reproduced from it: their raw outputs are not kept as receipts (the measurement rules
  admit only `performance.ps1` receipts of library builds), and the rows record what each switch measured, not a
  shippable configuration. `Y-default` runs the `Default` scene for comparison.
- Machine: AMD Ryzen AI 7 PRO 350, Windows 10.0.26200, WARP, compiler 195136257, on AC power. A developer laptop,
  not a quiet fixture: the X, Y and Z series were measured at different times, so compare within a series (each has
  its own unmodified `none` run), never across series.

## Scene switches (`DXUI_EXPERIMENT`, in the benchmark scene)

| Run | Change | Dirty FPS | Dirty p50 (ms) | Dirty private (MB) | Dirty allocations per round |
|---|---|---:|---:|---:|---:|
| `X-none` | none | 91.5 | 10.84 | 31.88 | 1,148 |
| `X-hidegrid` | grid hidden | 92.4 | 10.60 | 31.71 | 40 |
| `X-singleline` | grid cells single-line | 58.9 | 16.25 | 29.97 | 1,120 |
| `X-clamp1` | grid line clamp 1 | 93.2 | 10.61 | 32.43 | 1,178 |
| `X-short` | short grid values | 84.9 | 11.52 | 31.54 | 1,083 |
| `X-shortnames` | tree items with short names | 227.2 | 4.24 | 28.23 | 1,131 |
| `X-shortnameshidegrid` | both | 264.4 | 3.66 | 26.42 | 0 |
| `Y-none` | none | 107.0 | 9.28 | 32.52 | 1,220 |
| `Y-shortnames` | tree items with short names | 369.9 | 2.53 | 29.20 | 1,099 |
| `Y-default` | the `Default` scene | 383.5 | 2.56 | 27.31 | 2,167 |

Hiding the multiline grid changes nothing (92.4 against 91.5 FPS), and neither does clamping it to one line; shorter
values are within noise. Giving the tree's items short names instead of the long French ones multiplies the rate by
2.5 to 3.5. The multiline grid's cost is its allocations (1,108 of the round's 1,148), not its time. Single-line cells
are the slowest variant (58.9 FPS): they take the lightweight draw path, which lays their text out on every frame
instead of keeping it.

## Draw switches (`DXUI_DRAW_EXPERIMENT`, in the host's text draw)

| Run | Change to every host text draw | Dirty FPS | Dirty p50 (ms) | Dirty private (MB) |
|---|---|---:|---:|---:|
| `Z-none` | none | 103.7 | 9.54 | 33.01 |
| `Z-nocolor` | without `D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT` | 371.4 | 2.69 | 31.39 |
| `Z-noclip` | without `D2D1_DRAW_TEXT_OPTIONS_CLIP` | 85.2 | 11.55 | 33.90 |
| `Z-firstline` | only the text's first line | 352.4 | 2.74 | 30.08 |
| `Z-prefix40` | only the text's first 40 units | 363.7 | 2.62 | 30.02 |

Dropping the color-font option alone raises the dirty rate 3.6-fold (103.7 to 371.4 FPS), about the factor short
tree names gave in the Y series: the tree's long names end with a camera emoji, and drawing that color glyph through
WARP costs about 7 ms of the 10 ms frame. Shortening the drawn text does the same because it drops the emoji.
Clipping is not the cost.

## Rejected: a host-wide retained text layout

A retained layout for every host text draw (keyed like the grid's cells) hit 99.7% of lookups on the dirty frames,
yet did not change the frame rate and added about 0.8 MB of private bytes, so it was reverted. Its receipts were not
kept. The color-font cost is the cost of drawing the emoji glyph on WARP. The library draws text with color fonts
enabled (`kTextDrawOptions`), and the tree's names are the scene's content, so the cost stays with the scene; only a
hardware-adapter measurement, not made here, would show how much of it WARP adds.
