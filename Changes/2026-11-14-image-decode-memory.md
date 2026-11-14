- Rich image decoding no longer copies its encoded input (performance and motion plan W7.8): `RichImage::Decode`
  streams the caller's bytes in place for the duration of the call and `LoadFile` decodes from its single read
  buffer, so a 16 MiB file peaks at one encoded buffer instead of two. A GIF first frame is copied into its canvas at
  its offset and composed in place, and EXIF orientation is applied in place for square images and the
  non-transposing orientations; only a transposing orientation of a non-square image keeps one extra decoded-size
  buffer. Results, caps, cancellation points and decoded pixels are unchanged. The new Embedded
  `TestRichImageDecodeMemory` (`--test-rich-image-decode-memory`) checks the bound on a PNG, oriented JPEGs, a
  two-frame GIF on and off its canvas, and a PNG at the 16 MiB encoded cap; before and after numbers are in
  `Measurements/PerformanceAndMotion_2026-11-13/W7.8`.
