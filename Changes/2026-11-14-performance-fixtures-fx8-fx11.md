- Performance fixtures FX8 rich typing and FX11 motion (performance and motion plan W0.4) are built on the fixture
  framework, with WARP x64 Release receipts under `Measurements/PerformanceAndMotion_2026-11-13/FX8` and `FX11` as the
  "before" baseline.
  - FX8 edits a `RichTextEditor` document with math, diagrams, raster and SVG images, a table and a numbered code fence
    (100 inserts in prose, in the fence and next to the table, 10 Enter presses, 50 arrow keys, a 30-step resize); its
    `full` variant is the plan's 64 Ki-unit document and runs only for receipts. Variants `source` and `multiplier` (and their
    `-full` forms) lay the same document out through the v0 document layout: Source mode in chunks, and Rich mode without
    Markdown rendering at line height 1.5 as one whole layout.
  - FX11 records ticks, frames, layouts and allocations of a `PageHost` slide, a `Tree` expand and collapse, `Toggle`,
    `Checkbox` and `Slider` changes and a `ComboBox` popup at 30/60/144/240 Hz on the frozen clock, on both hosts.
  - The fixture model gains `Definition::settlePreparation` (the embedded runner repeats a step's frame until
    cooperative preparation finishes); the full-size FX8 variants are `receiptOnly`.
  - New private work counters in the rich-document group: `chunksLaid`, `diagramPreparations`,
    `displayUnitsWritten`, `elementsBuilt`, `imageUploadBytes`, `mathPreparations`, `preparationTurns`,
    `svgRasterizations` and `wholeLayoutsLaid`.
  - Library fix: `ControlHost::DebugAnimationTickForTest` now ends the dispatcher subscription of a tick that settles the
    host, as a dispatcher tick does; it used to clear only the host's id, leaving a dispatcher entry with a dangling host
    that the process failed on at exit. The production dispatcher path was not affected.
