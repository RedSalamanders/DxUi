# W8.2 Incremental publication — FX6 before and after

Plan item W8.2 of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md); behavior in
[input and accessibility](../../../Specs/UI/UI_InputAndAccessibility.md#incremental-publication).

- **Scenario**: the `FX6` fixture (`Tests/Embedded/Embedded.Tests.PerformanceFixtureFx6.h`), embedded host, 640x480 at
  96 DPI, WARP, reduced motion; "one client" is the client gate's test override. Step k of a phase publishes what step
  k - 1 did: the `Slider` keys are published by steps 1-99 of `slider-one-client` (and step 0 of the next phase); the
  drag's press by step 1 of `splitter-one-client`, its 200 moves by steps 2-201, its release by step 0 of
  `wordwalk-one-client`, which also shows the hidden document.
- **Command**: `performance-fixtures.ps1 -Fixture FX6` (Release, x64). The gate: `DxUi.EmbeddedTests.exe
  --test-incremental-publication` (also run by the Embedded suite), which verifies every incremental publication field by
  field against a whole build of the same tree.
- **Source**: "before" is the integration base `25318917b` (after W2.10), "after" this item's commit on `2b09df9e6`.
  **Build**: x64 Release, MSVC 19.52. **Adapter**: WARP (Microsoft Basic Render Driver). **OS**: Windows 11 build 26300.
- **Files**: [before](fx6-before-release.json) (the fixture's receipt format, every counter per step); after: the
  regenerated [FX6 receipt](../FX6/fx6-default-embedded-warp-x64-release.json).

| Release, FX6 with one client | Before | After |
| --- | --- | --- |
| `Slider` key (99 publications): whole builds | 1 per step | **0** |
| `Slider` key: records re-read / chunks cloned / bytes copied | 4 records, 3,952 B of records | **1 record, 1 chunk, 528 B** |
| `Slider` key: C++ allocations per step (frame included) | 17 | **9** |
| `Splitter` press (focus moves from the slider): whole builds / records re-read | 1 / 4 | **0 / 2** |
| `Splitter` move (200 publications): whole builds | 1 per step | **0** |
| `Splitter` move: records re-read / bounding-rectangle rebuilds | 4 records | **1 record, 1 rebuild** |
| `Splitter` move: C++ allocations per step | 17-18 | 17-18 |
| `Splitter` release | 1 whole build | 1 whole build |
| Mutations with one client (label, field, slider; 99 publications): whole builds | 1 per step | **0** |
| Mutations: C++ allocations per step | 15-20 | **9-13** |
| Incremental publications verified against a whole build (gate run) | — | 402, 0 differences |

A `Splitter` move rebuilds every bounding rectangle once (the separator moved), which allocates the rectangle table and its
index: its allocations equal the whole build's on four records. On larger trees the whole build's cost grows with every
record and string while the move's grows with the rectangles only.

Per value change the successor copies the record spine (one 16-byte pointer per 32 records) and clones one chunk of 32
record pointers (512 bytes): 528 bytes on FX6. On the 10,000-control form of `FX12` the same change copies 313 spine
pointers (5,008 bytes) and one chunk (512 bytes) and allocates the snapshot header, the spine, the chunk and the record:
computed from the layout, not measured (that variant is receipt-only and was not run for this item). A flat spine of one
pointer per record would copy 160,000 bytes and touch 10,000 reference counts per change; chunks of records by value would
copy 32 records (about 30 KB plus their strings).

## Layout cost and coalescing (review F3)

- **Command**: `DxUi.EmbeddedTests.exe --measure-incremental-publication` (Release, x64, WARP, one client through the client
  gate's override). **Scenario**: `FX12`'s form shape: one `ScrollPanel` of rows of a `Label`, a `TextField`, a `Checkbox`, a
  `Toggle` and a `Button` (400 rows: 2,000 controls; 2,000 rows: 10,000), 1920x1080 at 96 DPI, on a stopped application
  clock. Each figure is the `UpdateAccessibility` call alone, the median of its frames, with the C++ allocations it made.
  Two runs on a shared desktop; the 10,000-control figures varied between them (other builds ran beside them).

| Release, one client | 2,000 controls | 10,000 controls |
| --- | --- | --- |
| Whole build (a label's text, every publication whole) | 31-33 ms, 1,291 allocations | 809-1,454 ms, 6,341 allocations |
| Frame moving every control, whole | 31 ms, 1,290 allocations | 801-1,457 ms, 6,340 allocations |
| Frame moving every control, incremental (marks collapsed onto the `ScrollPanel`) | 11.3-11.5 ms, 1,292 allocations | 283-538 ms, 6,346 allocations |
| Records re-read per incremental frame (the text fields, whose ranges follow bounds) | 400 | 2,000 |
| Deferred frame (coalesced, within 100 ms of the last geometry publication) | 0.002 ms, 0 allocations | 0.003 ms, 0 allocations |

Where an incremental frame's allocations come from: it allocates about as often as a whole build (1,292 against 1,290 at
2,000 controls) because it re-reads the 400 text fields (each record and its strings: name, value, text, selection
rectangles) and rebuilds the rectangle table and its index, while a whole build allocates its records in one shared block
and its rectangles once. The re-read is needed only because a text control's selection rectangles and visible range are
held in absolute coordinates; no cheap fix fits this item. Open, for W8.3 (container-relative bounds): with relative
geometry a move re-reads no text record and rebuilds only the moved scope's rectangles.

The incremental frame stays above 1 ms with collapsing alone: it rebuilds every bounding rectangle and re-reads every
text-capable record, and both grow faster than linearly with the tree here (the whole build too). Geometry-only
publications are therefore coalesced: at most one per 100 ms of the host's clock plus one when the layout settles, the others
deferred at the cost above (`TestGeometryPublicationsCoalesce`: 20 frames 10 ms apart publish at two of them, defer 18, and
the settle and a name change publish at once).
