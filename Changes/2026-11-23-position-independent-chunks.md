- Position-independent v0 document chunks and viewport-first preparation (performance and motion plan W7.3):
  `PreparedDocumentLayout` lays every chunk out at its own origin 0 (the private `U+FFFC` origin line and the
  translated uniform-Source baselines are gone) and places it at the double-precision sum of the line heights before
  it, rounded once; carets, hit tests, selection rectangles, visible ranges and drawing add that placement. An unchanged
  chunk is reused wherever it moves, so a height-changing edit no longer rebuilds the document's suffix: on `FX8` Source
  mode an insert or Enter re-lays one chunk (an Enter re-laid 62 to 64 at 65,280 units before; an edited chunk grows to
  2,048 units up to the next unchanged chunk instead of splitting, while chunks keep the 1,024-unit target when nothing
  is reusable). An explicit line-height multiplier outside Source no longer builds one whole-document layout per
  keystroke: it uses the same chunks, spaced with the whole document's largest run size and script flag. Line tops stay
  within 1/64 DIP of the exact sum of line heights below 2^17 DIP; one whole layout's own single-precision accumulation
  drifts from that sum on long documents (1,120 DIP at 65,536 lines), which the old private origin line reproduced and
  chunks no longer do. Pixel change: where that drift moved a glyph baseline across a device-pixel rounding boundary,
  pixels differ from the previous release (6 of 36 viewport pairs of the raster profiles); line pitch and heights are
  unchanged and short documents draw identically. An incremental preparation equals a fresh preparation with the same
  chunking bit for bit and in exact WARP pixels, with a wrong-offset negative control, and each chunk's ink is checked
  at its exact line top. The editor gives its viewport to every preparation advance: chunks meeting it (with one
  viewport of overscan) are laid and published before any off-screen chunk, on a first load, a reflow, a live resize or
  an edit during one. No host frame shows that partial publication yet: both hosts withhold the frame until content
  preparation completes. Every chunk not laid yet stands in as its previous or stale geometry or an estimated
  placeholder (its source units times the known height per unit; on a first load away from the top, one sample layout
  measures line height and wrapping first). A partial publication is made only when a window chunk was laid since the
  last one, and stepping back to the last complete publication's width during a live resize restores it without laying
  a chunk. Until the rest is laid the layout is not current: input that needs geometry, caret and selection, find,
  scrolling, UI Automation and text-service rectangles wait or answer pending, and a reflow keeps the first visible line
  in place. A visible range at a chunk seam follows the placed caret boxes, as the geometry oracle states. A width step
  with the multiplier now lays every chunk (61 at full size) instead of one whole layout. New work counters
  `rich.offscreenChunksLaidFirst`, `rich.viewportPublications` and `rich.estimateSampleLayouts`.