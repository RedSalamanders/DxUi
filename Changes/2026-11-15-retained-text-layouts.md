- Hosts retain the text layouts of the plain text their controls draw (`DrawCenteredText`, `Button`'s selector chrome,
  `ThroughputGraph` labels, `Tree` rows with a typography override, single-line fields without their own layout) instead
  of laying each string out on every `DrawText` call. One set-associative table per host, keyed by the whole string, its
  format and the exact box, holds the strings of the last paint up to a measured bound of 2,048 entries (DirectWrite keeps
  about 18 KB of heap per retained string); hidden, minimized, detached or device-replaced hosts keep none, and embedded
  views create layouts only in `Prepare`. A hover frame of FX5 now creates no text layout (was 49 per frame inside
  `DrawText`). An equivalence oracle of 1,728 cases at 96 to 192 DPI and the FX5 captures of every host are byte-identical
  to drawing through `DrawText`. Text measurement reads line metrics into a stack buffer, so a `Label` measure makes no C++
  allocation. New work counters `text.textLayoutCacheHits`, `textLayoutCacheMisses`, `textLayoutCacheBypasses`,
  `textLayoutCacheOverflows` and `lineMetricsHeapFallbacks`; registry row `ControlHost._textLayoutCache`; FX5 gains a
  20-frame hover phase.
