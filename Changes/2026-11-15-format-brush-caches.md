- Host text-format and solid-brush caches evict by last use instead of clearing (plan item W2.8): each entry records the
  host frame (a native render pass or an embedded preparation) that last used it, and above its bound (now 256 formats
  and 256 brushes) a map releases at a frame's start only what neither of the last two frames used; entries in use stay,
  so a working set above the bound never thrashes. Format eviction no longer forces every visible control to prepare
  again: every retained key that kept a raw `IDWriteTextFormat*` (single-line text layouts, the `MenuBar` layout,
  `Tree` badge widths and tooltip overflow) now holds it by `com_ptr`, and the host's `_forceAllVisualPreparation` flag
  is gone (a theme change and a whole-tree cancellation mark every control pending instead). Colors of a transition in
  progress (a `Tree` row fade, `Button`, `Toggle`, `Checkbox` and `RadioButton` hover and focus blends) draw through
  per-frame transition brushes reused with `SetColor`, so the keyed map no longer grows by one brush per animated color;
  the `Selector` button no longer recolors a keyed brush. On `FX12` typography-120 a frame makes 0 forced full
  re-preparations and creates 0 text formats (122 before), and `FX11` Tree fades create 0 brushes after their first
  frame on both hosts, with WARP frame hashes equal before and after. New work counters: `frame.brushColorReuses`,
  `frame.brushEvictions`, `frame.fullVisualPreparations`, `text.formatCacheOverBound`, `text.formatEvictions`;
  `text.formatCacheTrims` now counts trims that evicted a format.
