# W7.8 image decode: before and after

Plan item: `W7.8` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md).
Contract: the rich image decode bound in [UI_ControlsAndLayout.md](../../../Specs/UI/UI_ControlsAndLayout.md) and
[Core_PerformanceAndResources.md](../../../Specs/Core/Core_PerformanceAndResources.md).

## Scenario and provenance

- Test: `TestRichImageDecodeMemory` in `Tests/Embedded/Embedded.Tests.RichTextResources.h`, run alone as
  `DxUi.EmbeddedTests.exe --test-rich-image-decode-memory`. Scenarios: `RichImageTests::DecodeScenarios` and
  `EncodedCapScenario` in `Tests/Support/Support.Tests.RichImage.h`, built in the test from library-owned synthetic
  pixels (the later `FX8` fixture can reuse them):
  - (a) `png-64x48`: a 64x48 opaque PNG.
  - (b) JPEG with an APP1 EXIF orientation: 96x64 orientation 6 (transposing, non-square), 96x64 orientation 3 and
    64x64 orientation 8 (transposing, square).
  - (c) a two-frame (animated) GIF, 64x48, once as written (first frame covers the canvas) and once with its logical
    screen widened to 80x60 and the first frame placed at (16, 12), touching the right and bottom edges.
  - (d) `png-2000x2000-at-encoded-cap`: a 2000x2000 opaque-noise PNG padded after IEND to exactly
    `MaximumEncodedBytes` (16,777,216 bytes), decoded from memory and loaded from a file.
- Instrument: the Embedded suite's counting `operator new` (`Embedded.Tests.BenchmarkInstrument.h`): peak live tracked
  bytes during one call, and every plain block classified as encoded-size or decoded-size (block size within 64 bytes
  above the buffer: the standard allocator's alignment padding) or other. Codecs warm first. WIC's own heaps are not
  tracked.
- Before: base `41398711` with this change's test files applied and the library unchanged. After: this change.
  Both x64 Debug, MSVC 19.52.37025, Windows 10.0.26300, same machine. Raw lines:
  [before-x64-debug.txt](before-x64-debug.txt), [after-x64-debug.txt](after-x64-debug.txt).

## Results (x64 Debug)

| Scenario | Encoded | Decoded | Peak before | Buffers before (encoded / decoded) | Peak after | Buffers after |
| --- | ---: | ---: | ---: | --- | ---: | --- |
| (a) PNG 64x48 | 221 | 12,288 | 12,700 | 1 / 1 | 12,463 | 0 / 1 |
| (b) JPEG 96x64 orientation 6 | 2,915 | 24,576 | 52,433 | 1 / 2 | 49,502 | 0 / 2 (permitted) |
| (b) JPEG 96x64 orientation 3 | 2,915 | 24,576 | 52,433 | 1 / 2 | 24,751 | 0 / 1 |
| (b) JPEG 64x64 orientation 8 | 2,165 | 16,384 | 35,299 | 1 / 2 | 16,559 | 0 / 1 |
| (c) GIF two frames, covering | 2,772 | 12,288 | 15,251 | 1 / 1 | 12,463 | 0 / 1 |
| (c) GIF two frames, 80x60 canvas | 2,772 | 19,200 | 34,514 | 1 / 2 | 19,375 | 0 / 1 |
| (d) 16 MiB PNG, `Decode` | 16,777,216 | 16,000,000 | 32,777,454 | 1 / 1 | 16,000,175 | 0 / 1 |
| (d) 16 MiB PNG, `LoadFile` | 16,777,216 | 16,000,000 | 49,555,165 | 2 / 1 | 32,777,886 | 1 / 1 |

`Decode` borrows the caller's encoded span, so its own encoded buffers fall from 1 to 0; `LoadFile` owns its read
buffer, the one encoded buffer left. Peak after is the decoded size (twice it for the transposing non-square JPEG)
plus 175 bytes (the image object, its control block and allocator padding); for `LoadFile` it adds the encoded size
and the path strings (1,430 other bytes requested). The decoded pixels' FNV-1a hashes are identical before and after
for every scenario, and the after run also checks each against an exact oracle.
