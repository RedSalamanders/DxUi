- Record the `longbridge/gpui-fast` (`61dccec`) and `anomalyco/rune` (`07723732`) versus `DxUi` architectural studies
  and 11-wave runtime and resource efficiency plan in `Specs/Plans/WIP/RuntimeEfficiency_2026-11-07.md`, with Wave 0
  decisions D1–D4 settled (separate WIP plan coordinated post-D9 on shared files, preserved full `MenuPopup` UIA
  publication with `O(1)` incremental updates, clean API revision 5 evolution with no legacy compatibility shims for
  deadline-aware `EmbeddedHost` scheduling, zero-copy `IGridModel`/`ITreeModel` views and 64-slot slab
  `ControlLifetimeHandle` + `ColdControlMetadata` static-empty-singleton compaction from `296 B` to `~112–120 B`, and
  measurement-gated `ScrollPanel` bitmap strips), plus `anomalyco/rune` mechanisms across `poll_alone`/`LoopClock`,
  4-band hollow shadow culling, `KeyHasher`/`SupersedeTextShape`/`CachedElision`, `O(log L)` `VisibleRange`
  (`glyphs_in_band`), semantic `CodeNumbers` deletion, in-place `SetMaxWidth` re-break, and shared sRGB LUTs. Plan
  only; no library, test or tooling behavior changes yet.
