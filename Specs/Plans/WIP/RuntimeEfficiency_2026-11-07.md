# Dirty-frame, layout, text-shaping and rich-document efficiency, 7 November 2026

Status: SPEC READY — architectural studies against `longbridge/gpui-fast` (`61dccec2536ecbb6845a2c0cdf5fb17651b10bc5`)
and `anomalyco/rune` (`077237326b59b6afa85b4329231fe6320a187c74`) plus a corner-by-corner static audit of `DxUi`
completed; developer confirmed Wave 0 decisions D1–D4 on 7 November 2026 (including clean API revision 5 evolution with
no legacy compatibility shims). Plan only; code implementation has not started.

- **Date**: 2026-11-07.
- **Reference studies**:
  1. `longbridge/gpui-fast` (`crates/gpui/src/fast/` and `crates/gpui_windows/src/fast/` at
     `61dccec2536ecbb6845a2c0cdf5fb17651b10bc5`) compared against `DxUi` at `main`.
  2. `anomalyco/rune` (`crates/rune`, `crates/rune-kit`, `crates/rune-motion`, `crates/procwatch`, and empirical studies
     `research/01` through `research/38` at `077237326b59b6afa85b4329231fe6320a187c74`) compared against `DxUi` at `main`.
- **Owning contracts**: [performance and resources](../../Core/Core_PerformanceAndResources.md),
  [architecture](../../Core/Core_Architecture.md),
  [controls and layout](../../UI/UI_ControlsAndLayout.md),
  [theme and typography](../../UI/UI_ThemeAndTypography.md),
  [input and accessibility](../../UI/UI_InputAndAccessibility.md),
  [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
  [Win32 host](../../Rendering/Rendering_Win32Host.md),
  [toolchain and consumption](../../Build/Build_ToolchainAndConsumption.md),
  [validation](../../Testing/Testing_Validation.md) and
  [documentation](../../Core/Core_Documentation.md).
- **Coordinated active & archived plans**:
  [DxUi remaining correctness and qualification](RemainingWork_2026-11-04.md) (consolidated active correctness and
  qualification queue; coordinates D9 history rewrite and shared rich-text/control files), plus archived records
  [Production review of the five-day delivery](../Done/ProductionReview_2026-11-02.md) (original review inventory for
  `EDIT-01`, `RENDER-01`, `RENDER-02`, `RENDER-05`, `RENDER-09`, `RENDER-14`, `SEM-02`, `MD-05`..`MD-07`, `CTRL-02`,
  `CTRL-04`, `CTRL-13`, `HOST-03`, `HOST-08`, `HOST-09`, `HOST-13`, `HOST-14`),
  [Complete Update-stage metric accounting](../Done/FrameUpdateAccounting_2026-10-26.md) and
  [Library qualification and consumer adoption](../Done/ConsumerUiQualification_2026-10-11.md).

## Architectural polarity: `longbridge/gpui-fast` versus `DxUi`

`longbridge/gpui-fast` starts from an immediate-mode Rust closure framework (Zed's GPUI) that previously rebuilt its
element tree, Taffy layout nodes, shaped text runs, paths, hitboxes and accessibility nodes every frame, and retrofits
cross-frame retention onto it (`LayoutRetention`, `MeasureLog`, `settle_measured_leaves`, `DrawLog`/`HitboxLog`/`A11yLog`
scene splicing, `text.rs`/`glyphs.rs`, `number_shaping.rs`, `path_cache.rs`, `bounds_tree.rs`, `hover_freeze.rs`,
`interactive_text.rs`, `frame_schedule.rs`, `draw_state.rs`, and the lockstep `oracle.rs` verifier).

`DxUi` starts from the opposite pole: it is already a retained-mode C++ control library whose clean frames cost
`0.0003 ms` in `EmbeddedHost::Prepare` and `0.0104 ms` p95 in `EmbeddedHost::Composite` with `0` steady-state heap
allocations ([Default-B1.json](../../../Measurements/EmbeddedInteractionLayout/Default-B1.json)). Where `DxUi` spends
avoidable CPU, GPU, COM and heap work is on **dirty frames**, **focused idle timers**, **popup overlays**, **virtualized
table/tree scrolling**, and **rich-document editing**.

### Summary comparison matrix across all 11 corners

| Corner | `longbridge/gpui-fast` (`61dccec`) | `DxUi` (`main`) | Borrow / adapt verdict |
| --- | --- | --- | --- |
| **1. Idle timers, animation scheduling & `HoverFreeze`** | Event-driven `frame_schedule.rs` + `device_wake.rs`; `hover_freeze.rs` suppresses hover hit-tests for 100 ms while wheel-scrolling under a stationary cursor | `TextField::Tick` (530 ms), `ComboBox::Tick` (530 ms), `RichTextEditor::Tick` (500 ms), and `TooltipLayer::Tick` (500 ms show / 5,000 ms hide) return `true` on every 8 ms `AnimationDispatcher` tick (~125 Hz full-tree walk) and keep `EmbeddedHost::NeedsAnimation()` continuously `true`; `TextField`/`ComboBox` ignore reduced motion; wheel scroll re-runs hover every notch | **High leverage**: Add deadline scheduling (`RequestWakeAt`) + `_subtreeHasTickables` pruning + `EmbeddedHost::GetNextAnimationDelayMs()` + 100 ms stationary-cursor `HoverFreeze`. |
| **2. Dirty-rect preparation, clip culling & resize** | Tile dirty rects + `ChangedBounds` 64×64 mask; `draw_state.rs` redundant D3D11 state filter | `WindowHost::Render` already has `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` + `PushAxisAlignedClip` + `Present1`, but `ControlHost::Invalidate()` passes `nullptr` rect; `EmbeddedHost` has a single `bool dirty`; `Panel::Paint` / `ScrollPanel::Paint` do not cull against the active clip; `PrepareForSwapChainResize` clears `_brushCache` on every `WM_SIZE` (`50+` brush allocations/frame) | **High leverage**: 4-rect outward-pixel-snapped `DirtyRegion` for `WindowHost` & `EmbeddedHost`; clip-rect subtree culling across containers; keep `_brushCache` across `WM_SIZE`; optional `CompositeBatch` for multi-view hosts. |
| **3. Incremental layout, `MeasureLog` & width settle** | `layout.rs`: 4-inline-slot `MeasureLog` (`replays` cutoff), unwrapped-fit carry, post-layout `settle_measured_leaves` width-match guard | `InvalidateMeasure()` bubbles unconditionally to `_root`; `FlexPanel::ChildRecord` and `FormField::PartMeasure` keep only 1 width slot (thrashing on min/max/final probes); `FlexPanel::ApplyLayout` `stillValid()` is `O(N²)`; `Label::PrepareVisualContent` runs an `O(N)` `IsControlInTree` DFS (`CTRL-02`) | **High leverage**: Fix `CTRL-02` and `O(N)` `FlexPanel::ApplyLayout`; 4-slot `MeasureLog` + unwrapped-fit carry on container records + leaf size-stability cutoff + post-arrange width settle. |
| **4. Text shaping, `NumberShaping` & brush/format caches** | `text.rs` / `glyphs.rs` two-generation glyph-run cache (color-independent, unwrapped shortcut) + `number_shaping.rs` (`0-9.,+-%$KMBT `, 1/256 oracle, no `:`) | `DrawCenteredText` calls raw `dc->DrawTextW` on 90+ call sites (`Label`, `Button`, `TabControl`, `Menu`, `ComboBox`, `Tree` 3×/row, `Grid` headers/badges/icons), allocating a throwaway `IDWriteTextLayout` every frame; `MeasureText` builds another throwaway layout + `vector`; `_brushCache`/`_configuredTextFormats` use linear scan and `.clear()` on overflow | **Highest CPU/RAM win**: Host 2-generation `ShapedTextCache` (`IDWriteTextRenderer` -> `DrawGlyphRun`, ~280 B/entry vs ~20 KB `IDWriteTextLayout`) + context-safe `NumberGlyphTable` + PUA icon table + Windows 10/11 color-glyph support (`RENDER-05`) + `O(1)` flat clock-LRU brush/format tables. |
| **5. `ComboBox` & `MenuPopup` overlays** | Retained scene slices + cached blur/surface primitives | `ComboBox::PaintOverlay` calls `UpdatePopupLayout(&host)` `1 + 2 * V` times per frame, each running `N` × `CreateTextLayout` (`1,275` layouts/frame for 25 items, `8,500` for 500 items!); both `ComboBox` and `MenuPopup` recreate `ID2D1Layer` + `CLSID_D2D1GaussianBlur` + 2 linear gradient brushes every frame; `GetMenuIconWicFactory` calls `CoCreateInstance` per icon | **Critical hotspot fix**: Cache `ComboBox` popup layout across paint queries (`8,500 -> 0` `CreateTextLayout`/frame); retain popup blur/gradient/clip resources per open/bounds/theme; cache WIC factory; prefix-sum `MenuPopup` item Y-offsets (`O(1)` rect lookup). |
| **6. `TextField`, `SingleLineTextEditing`, `TagPicker` & `Splitter`** | Retained text runs, zero-alloc hit-testing | `TextField::GetOrCreateSingleLineLayout` passes `secureCacheText = true` for all fields, running `SecureWipe::SecureClear` + `wstring` heap alloc on every `cacheHit == true` (2×/frame); `GetDisplayText()` copies `wstring` by value; `BuildNativeCompositionUnderlineRects` deep-copies `NativeTextInputState` 2×/frame when not composing; multiline selection/caret ignore `_cachedMultilineLayout` (2 `CreateTextLayout`s/frame); `TagPicker::ComputeLayout` reallocates strings/layouts every paint; `Splitter::NotifyChange` rebuilds full window UIA snapshot on every drag pixel | **High leverage**: Guard `SecureClear`/copy behind `!cacheHit` (and `IsPasswordMasked()`); zero-copy `GetDisplayTextView()`; `HasNativeTextCompositionOrTarget` fast guard; reuse `_cachedMultilineLayout`; stack-buffer `HitTestTextRange`; cache `TagPicker` layout; coalesce `Splitter` preview UIA refresh. |
| **7. `Grid` & `Tree` virtualization & scrolling** | Virtual lists stay zero-VRAM and replay cached glyph runs; `hover_freeze` + `WorkBudget` | `Grid::Paint` calls `paintModel->GetColumn` `4 + 4 * visibleRows` times/frame (`52` `wstring` allocs + `2` `vector`s = `54 allocs/frame` in `ComplexUi`); `Grid` `_cellTextLayouts` keys on rounded width (100% miss storm on column resize) and thrashes 100% on 2,048-row scroll (`28` `CreateTextLayout`s/frame); `Grid` key nav is `O(rows)` (`CTRL-04`); `Tree::SetFocusedItemId` scans `O(N)` twice and `DrawTreeRow` has zero text cache | **High leverage**: Hoist/cache `GetColumn` + scratch vectors (`54 -> 0` allocs/frame in `ComplexUi`); route `Tree` rows and single-line `Grid` cells/headers/badges through `ShapedTextCache` / `NumberGlyphTable` (keeping `IDWriteTextLayout` strictly bounded to on-screen complex cells); cache `_visibleBodyItems` for `Grid` key nav. |
| **8. `RichTextEditor`, `Markdown`, `SemanticGeometry`, `Math` & `Flowchart`** | `interactive_text.rs` clickable-span hover filter; `em_metrics.rs`; `path_cache.rs` `(0,0)` geometry + `heavy` slot; keyed block `Carry` | `RichSourceGuides` builds up to 65,537 line-number `IDWriteTextLayout`s per keystroke and withholds frames (`EDIT-01`/`RENDER-01`); `lineHeightMultiplier > 0` disables 127-chunk reuse (`RENDER-02`); `PreparedSemanticGeometry::Begin` gates `previousImages` and `previousNative` (`PreparedMath`, `PreparedFlowchart`) on `state_->document == document`, so **every keystroke destroys 100% of math, Mermaid diagrams, bitmaps and SVGs** (extending `SEM-02`); `PreparedMath::DrawColored` and `PreparedFlowchart::Draw` zero 9–60 KB stack arrays and recompute `std::pow(..., 2.4)` contrast every paint; `PreparedFlowchart` allocates 1 `ID2D1PathGeometry` per arrowhead; `PaintIndex::Build` reallocates `nodes` every rebuild | **Critical rich-doc win**: Viewport-scoped guides rendered via `NumberGlyphTable` (`0` `IDWriteTextLayout`s); preserve 127-chunk reuse with `lineHeightMultiplier`; decouple `previousImages` & `previousNative` from `state_->document == document` + carry unchanged semantic blocks/math/diagrams across edits; cache resolved WCAG colors & shared unit arrowhead geometry; `PaintIndex` clickable-only hover + buffer reuse; origin-relative `(0,0)` path cache. |
| **9. `Control` base-class footprint & lifetime tokens** | Compact arena/pooled state | `sizeof(Control)` is `296 B` (Release) / `336 B` (Debug) due to 4 cold `std::wstring`s (`128–160 B`), 3 cold callback/form pointers (`40 B`), 4 `std::optional`s (`8 B`), and an eager `std::shared_ptr<int> _lifetimeToken` (`~40 B` control-block heap alloc per control; 6 heap allocs/control in Debug) | **High memory win**: Extract `unique_ptr<ColdControlMetadata>` + pack state/inheritance bits (`sizeof(Control)` `296 B -> 128 B`, saving `~168 B/control` and 4 Debug proxy allocs/control); optional non-throwing 64-slot slab `ControlLifetimeHandle` (never lazy `make_shared` in `noexcept` `GetLifetimeToken()`). |
| **10. Accessibility (`UIA`) splicing & spatial hit-testing** | `a11y.rs` (`A11yLog` subtree slice reuse + `-scroll` offset translation) + `bounds_tree.rs` (64 px grid + `rest_max`) | `PublishWindowHostAccessibilitySnapshot` walks the full tree 3× on focus changes (`Grid`/`Tree`/`Menu` arrow keys), coalesced wheel flushes, and `EmbeddedHost` updates; `MenuPopup` keyboard navigation/scroll rebuilds all 4,096 UIA nodes (`6.10 ms/step`) and runs `O(N)` `GetVisibleItemRect`; `Panel::HitTest` is linear | **High leverage**: `_a11ySubtreeRevision` slice splicing + `-scrollDeltaPx` bounds translation; incremental `O(1)` focus/scroll update on `MenuPopup`'s retained 4,096-row snapshot (preserving `Core_PerformanceAndResources.md` popup publication); reuse `PaintIndex` / 64 DIP grid in `ScrollPanel`/`Panel` (`N >= 32`). |
| **11. Lockstep differential oracle** | `oracle.rs` / `--verify` comparing incremental vs fresh tree/scene | Deterministic WARP goldens, no multi-step incremental-vs-fresh oracle | **Mandatory safety gate**: `VerifyIncrementalParity` / `Controls.Tests.LayoutAndPaintOracle.cpp` asserting `0` BGRA pixel diff, bit-identical `GetBounds()`, identical UIA snapshots, and positive cache hits. |

### Explicit anti-patterns (what `DxUi` must not borrow)

1. **Immediate-mode closure replay logs (`retained.rs`, `splice.rs`, `DrawLog`)**: `DxUi` already retains live C++
   `Control` trees; adding closure logs would only increase memory and CPU overhead.
2. **GPU texture tiles for virtualized `Grid` or `Tree`**: Even `gpui-fast` (`docs/scroll-layers.md`) forbids GPU tiles
   on virtual lists; `Grid` and `Tree` stay zero-VRAM and replay cached glyph runs.
3. **Offscreen `IDWriteTextLayout` retention in `Grid`**: Forbidden by
   [Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md) (~20 KB/cell); warm scroll-back uses
   ~280 B `ShapedTextCache` glyph runs and `NumberGlyphTable` instead.
4. **Lazy `std::make_shared<int>` inside `Control::GetLifetimeToken() const noexcept`**: Calling `make_shared` inside a
   `noexcept` getter invokes `std::terminate()` under `DxUi`'s exhaustive OOM fault-injection tests and shifts heap
   allocations into measured `Prepare`/`Paint` frames.
5. **Contextual punctuation (`:`, `/`, `x`) in `NumberGlyphTable`**: `Segoe UI` applies OpenType `calt` vertical
   shifting (`colon.case`) to `:` between digits (`12:34`), which would mismatch DirectWrite and permanently trip the
   1-in-256 verification oracle (`number_shaping.rs`). Keep `:`/`/`/`x` in Tier 1 `ShapedTextCache`.
6. **Mid-paint cache eviction**: [Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md) forbids
   evicting cached resources mid-paint; all `ShapedTextCache`, brush, format, and geometry trimming runs strictly at
   frame/preparation start inside `ControlHost::TrimCaches()`.
7. **Editing `Embedded.Tests.ComplexUiBenchmark.h` ceilings inside feature PRs**: Invalidates paired fixture input
   hashes in `Publish-BenchmarkVerdict.ps1`.

## Architectural polarity & empirical study 2: `anomalyco/rune` (`07723732`) versus `DxUi`

`anomalyco/rune` (`crates/rune`, `crates/rune-kit`, `crates/rune-motion`, `crates/procwatch` at
`077237326b59b6afa85b4329231fe6320a187c74`) is a native Rust UI framework (`winit` + `wgpu`/Metal + `taffy` + `parley` +
`swash` + `pulldown-cmark`). Where `longbridge/gpui-fast` retrofits cross-frame retention onto an immediate-mode closure
engine, `anomalyco/rune` pairs a retained `Node`/`Scene` pipeline with **38 empirical performance studies (`research/01`
through `research/38`)** that measured, spiked, and either landed or explicitly rejected CPU, GPU, and memory
optimizations across 5,283-node trees, 50 KB–200 KB Markdown/editor notes, live resize, live values, and idle windows:

| Empirical study (`anomalyco/rune`) | Measured before → after | Primary mechanism in `anomalyco/rune` (`07723732`) | Direct `DxUi` implication |
| --- | --- | --- | --- |
| **200 KB scrolled text paint** (`research/27-long-note-editing.md:78-95`) | `5.45 ms/frame` → **`0.09 ms/frame`** (`60×` faster; click `5.78 → 0.39 ms`) | `glyphs_in_band` (`crates/rune/src/text/glyph_emit.rs:54-68`) binary-searches `block_max_coord < band.start` (`lo`) and stops at `block_min_coord <= band.end` (`hi`) with `2 em` ink slack | `PreparedLayout::VisibleRange` (`src/Controls/DxUi.RichTextLayout.cpp:590-609`) loops linearly from line `0` calling `Caret()` (`HitTestTextPosition` COM call/line); replace with binary search over precomputed cumulative line Y-tops |
| **50 KB Markdown live window resize** (`research/28-resize-rewrap.md:38-88`, `research/32:168-176`) | `16.1 ms/frame` (`401` shapings) → **`4.6 ms` → `1.74 ms/frame`** (`0` shapings, `9.2×` faster) | Split width-independent `shape_key` (`text/hash.rs:21-97`) from `layout_key`; re-break `Layout` in place (`Rc::make_mut` when `strong_count == 1`, `~0.88–2 µs/paragraph` vs `~30 µs` to shape); skip view on `resize_only` (`relayout.rs:58-181`) | Reuse `IDWriteTextLayout` via in-place `SetMaxWidth(newWidth)` + retained `clusters` in `PreparedLayoutAccess::Apply` (`DxUi.RichTextLayout.cpp:300-454`) and `Label::PrepareVisualContent` (`DxUi.Controls.cpp:2958-3006`) when only width changed |
| **50 KB multi-paragraph editing & stale eviction** (`research/32-engine-plan.md:389-442`, `layout_cache.rs:38-106`) | `15.2 ms/key` → **`0.10 ms/key`** (`150×` faster, `384/384` bit-exact runs); fixes `3.18 MB` stale leak | Per-`\n` paragraph layouts with 3 exact stacking rules (document base direction, running unrounded line-top sum, widest-paragraph alignment) + `TextSystem::supersede(shape)` deferred eviction + byte-cost cap (`LAYOUT_PER_BYTE = 64`, `STALE_BYTES = 3 MiB`) | Add `SupersedeTextShape(prevShapeKey)` drained in `ControlHost::TrimCaches()` + `3 MiB` byte-weighted stale cap to Wave 5 (`W5.1`), and enforce the 3 paragraph/chunk stacking invariants in Wave 9 |
| **5,283-node complex tree heap & commit** (`research/32` Ranks #2, #7; `research/38:72-96`) | `Node = 2.4 KB`, `AnimationStore = 3.5 KB/node`, `md-same = 0.851 ms` (`5,687` allocs) → **`Node = ~0.6 KB`, `md-same = 0.167 ms` (`10` allocs)** | `Cold<T: Empty>(Option<Rc<T>>)` rare-field boxing (`cold.rs:16-67`), `CachedElision` ellipsis cut cache (`layout_cache.rs:21-25`), `Node::moved_stub` zero-clone relayout/memo move (`relayout.rs:120-138`) | Validates Wave 10 (`ColdControlMetadata` + `_packedStateBits`) with static `Empty` singleton deref (`Control::Cold()`), plus `CachedElision` in Wave 5 (`W5.1`) |
| **High-frequency live presentation values** (`crates/rune/src/live.rs:69-382`) | Full commit (`13.7 ms`, `42k` allocs) → **lock-free atomic/seqlock update** (`0` layout passes, `0` wakes when hidden) | `Live<T>` (`AtomicU128` + seqlock `seq`) & `LiveText` update paint state in place; `Wake` (`pending`, `visible`) suppresses host wakes while hidden and coalesces visible updates | Stop 3-pass full-window UIA rebuilds (`AccessibilityMutationKind::Full`) and whole-window `Invalidate(nullptr)` in `ProgressBar::SetValue` (`DxUi.Controls.cpp:5137-5149`) and `Slider::PublishRangeState` (`lines 6415-6422`); skip `InvalidateMeasure()` in `Label::SetText` (`lines 2817-2836`) when glyph bounds are invariant |
| **8192×8192 RGBA mip generation & sRGB math** (`crates/rune/src/image/mips.rs:30-152`) | `~1.5 GiB` peak + per-pixel `powf` → **`341 MiB` peak, zero `powf`** | Static 4096-step `Srgb` LUT (`decode: [f32; 256]`, `up: [f32; 256]`, `coarse: [u8; 4097]`), `p[3] == 255` opaque skip, row-budgeted `MipSteps::step(budget)` | Replace all 4 `std::pow(..., 2.4)` helpers (`Utilities.h:21`, `DxUi.Theme.cpp:53`, `DxUi.PreparedMath.h:2855`, `DxUi.PreparedFlowchart.cpp:21`) with a shared `constexpr` LUT; fast-skip `alpha == 255u`/`0u` in `ComposeGifCanvas` (`DxUi.RichImage.cpp:152-165`) |

### Corner-by-corner comparison: `anomalyco/rune` (`07723732`) versus `DxUi`

| Corner | `anomalyco/rune` (`07723732`) | `DxUi` (`main`) | Borrow / adapt verdict for `DxUi` |
| --- | --- | --- | --- |
| **1. Idle timers, resting polls & `LoopClock`** | `TimerWake` (`crates/rune/src/runtime/motion/timer_wake.rs:16-127`) runs `poll_hidden` / `resting_wake` (`poll_alone`) **without** view, layout, or GPU draw unless the poll dirties visible state (`stale`), and drains due `wake_deadlines` immediately. `LoopClock` (`loop_clock.rs:24-91`) tracks `owed` app time dropped while hidden/occluded (`JITTER = 1/30 s`) so periodic loops stay in phase on resume, while gating loops on clip visibility. | `WindowHost` and `EmbeddedHost` tick `AnimationDispatcher` at ~125 Hz when any discrete caret/tooltip timer is active (`W2.1`–`W2.3`), and periodic/indeterminate animations (`ProgressBar`) do not gate tick registration on ancestor scroll clip visibility or effective opacity. | **Enhance Wave 2 (`W2.1`–`W2.3`)**: Run due discrete timer callbacks alone (`poll_alone`) before scheduling `Prepare`/`Render`, drain spent deadlines immediately, gate periodic animations on ancestor clip intersection and `opacity > 0`, and phase-align resumed animations via `LoopClock`. |
| **2. Dirty-rect preparation, 4-band hollow shadows & border inset** | `bands::push_shadow` and `bands::push_ring` (`crates/rune/src/runtime/paint/bands.rs:17-53`) split any shadow or unfilled border ring whose opaque interior hole `[hx0, hy0, hx1, hy1]` (inset by `corner_extent(r) + 1.0`) is `>= MIN_HOLE` (`96.0 px`) into **4 perimeter clip bands** (`top`, `bottom`, `left`, `right`), eliminating interior fragment overdraw. `border::fill_under` (`border.rs:17-41`) insets opaque fills by `bw * 0.5` under opaque rounded borders. `El::dim` multiplies leaf alpha (`paint_alpha`) instead of allocating a blend layer. | `DrawDropShadow` (`src/Controls/DxUi.Controls.cpp:797-810`, called by `ComboBox::PaintOverlay` at `DxUi.ComboBox.cpp:403` and `MenuPopup::Paint` at `DxUi.Menu.cpp:761`) calls `DrawShadowEffectPass` (`DxUi.Controls.cpp:246-296`) **twice per frame**, creating **2 `ID2D1CommandList`s + 2 `CLSID_D2D1Shadow` `ID2D1Effect`s every frame** and shading the full interior behind the popup body; rounded fills beneath opaque borders use identical outer rects. | **Enhance Wave 3 (`W3.2`) & Wave 6 (`W6.2`)**: Retain the dual `DrawDropShadow` `ID2D1CommandList`/`ID2D1Effect` resources alongside the popup backdrop blur in `W6.2`, emit 4 perimeter `PushAxisAlignedClip` bands when the opaque interior hole is `>= 96 DIP` on both axes, and inset opaque rounded fills by `0.5f * strokeWidth` under opaque strokes. |
| **3. Incremental layout, `resize_only` & `Live<T>` bypass** | `Runtime::resize_only` (`crates/rune/src/runtime/commit/relayout.rs:58-181`) skips view/flatten when only viewport size changed, moving `Node`s via `Node::moved_stub` (`0` clones). `Live<T>` (`crates/rune/src/live.rs:69-382`) updates presentation values via `AtomicU128` + seqlock and coalesces wakes via `Wake` (`pending`, `visible`) without dirtying layout. | `ProgressBar::SetValue` (`DxUi.Controls.cpp:5137-5149`) and `Slider::PublishRangeState` (`lines 6415-6422`) call whole-window `RequestInvalidate()` and `RefreshWindowHostAccessibilitySnapshot` (`Full` 3-pass tree walk) on every scalar step; `Label::SetText` (`lines 2817-2836`) unconditionally calls `InvalidateMeasure()`. | **Enhance Wave 3 (`W3.1`), Wave 4 (`W4.3`) & Wave 11 (`W11.1`)**: Route scalar updates (`ProgressBar`, `Slider`, `NumericStepper`) through control-bounds `DirtyRegion` + `O(1)` in-place UIA range-value updates; short-circuit `InvalidateMeasure()` in `Label::SetText` when single-line `DesiredSize` is unchanged. |
| **4. Text hashing (`KeyHasher`), `shape_key` vs `layout_key`, `CachedElision` & `supersede`** | `KeyHasher` (`crates/rune/src/text/hash.rs:21-97`) hashes bytes with `FxHasher` and mixes words via `((self.0 ^ i ^ 0x243f6a8885a308d3) as u128).wrapping_mul(0x9e3779b97f4a7c15)` xor-folded to 64 bits. `shape_key` excludes width; `layout_key` adds width + alignment. `TextSystem::supersede(shape)` (`layout_cache.rs:86-106`) records superseded shape keys on self-edit and evicts them at `end_commit()`, bounding older stale entries by byte cost (`LAYOUT_PER_BYTE = 64`, `STALE_BYTES = 3 MiB`). `CachedElision` (`layout_cache.rs:21-25`) caches `(layout, head, tail)` ellipsis cuts. | `DxUi` hashes text repeatedly during `MeasureText`/`Paint`, recreates `IDWriteTextLayout` on width changes in `Label::PrepareVisualContent` (`DxUi.Controls.cpp:2958-3006`) and `PreparedLayoutAccess::Apply` (`DxUi.RichTextLayout.cpp:300-454`), and does not explicitly evict the prior text key when a control or paragraph mutates its own string. | **Enhance Wave 5 (`W5.1`–`W5.4`)**: Precompute `_textShapeKey` on `Control::SetText`/typography change using Rune's folded 128-bit multiply mixer; add `SupersedeTextShape(prevShapeKey)` (drained in `ControlHost::TrimCaches()`) + `3 MiB` byte-cost stale budget + `CachedElision` to `ShapedTextCache`; reuse `IDWriteTextLayout` via `SetMaxWidth` when only width changes. |
| **5. Glyph/surface atlas 1px apron zeroing & row-band dirty upload** | `Atlas::put` (`crates/rune/src/text/atlas.rs:49-398`) uses 2 steady `2048×2048` shelf pages (`MAX_STEADY_PAGES = 2`, `MAX_OVERFLOW_PAGES = 8`, `COLOR_GLYPH_BUDGET = 16 MiB`), zeroes **only** the 1px border apron around each newly placed glyph (`lines 374-391`) instead of clearing 4 MiB on page reset, and tracks `dirty: Option<(u32, u32)>` (`y0..y1` row band) for GPU upload. | `DxUi` uses Direct2D's glyph cache for `DrawGlyphRun` (`W5.1`), and optionally caches non-virtual `ScrollPanel` bitmap strips (`D4`). | **Apply to `D4` (if strip cache is enabled)**: Track `(dirtyYMin, dirtyYMax)` row bands on strip updates and never full-clear recycled strip surfaces when sub-rect overwriting suffices. |
| **6. Intra-block visible-line binary search (`glyphs_in_band`)** | `glyphs_in_band` (`crates/rune/src/text/glyph_emit.rs:54-68`) binary-searches `layout.get(mid).is_some_and(|l| l.metrics().block_max_coord < band.start)` (`lo`) and stops at `block_min_coord <= band.end` (`hi`), reducing 200 KB scroll frame CPU from `5.45 ms` to `0.09 ms` (`60×`). | `PreparedLayout::VisibleRange` (`src/Controls/DxUi.RichTextLayout.cpp:590-609`) loops linearly from `index = 0` calling `Caret(state_->lines[index])` (`HitTestTextPosition` COM call/line!) until `bottom`; `PreparedSemanticGeometry::VisibleRange` (`DxUi.RichSemanticGeometry.cpp:3243-3259`) and `VisibleSourceRange` (`lines 3120-3140`) scan `state.boxes` linearly instead of using `state.paintBoxes`. | **Critical Wave 9 (`W9.5`) addition**: Store cumulative `lineTops` (`vector<float>`) in `PreparedLayout::State` during `Apply` (`DxUi.RichTextLayout.cpp:419-443`) so `PreparedLayout::VisibleRange` runs an `O(log L)` `std::lower_bound`/`std::upper_bound` with **zero** `HitTestTextPosition` calls; route `PreparedSemanticGeometry::VisibleRange` and `VisibleSourceRange` through `state.paintBoxes.ForEach(top, bottom, ...)`. |
| **7. Bounded linear-time code fence scanner & Markdown block-cut (`cuts`) tail reparse** | `syntax::scan` (`crates/rune-kit/src/syntax/scan.rs:84-137`) caps coloring at `MAX_SOURCE = 32 KiB` (**cut cleanly at the last `\n` via `coloured_len`**) and `MAX_TOKENS = 8192` (keeping the first 8,192 tokens and leaving the tail plain), and emits **color-only** spans so glyph advances never shift. `markdown::cache` (`crates/rune-kit/src/markdown/cache.rs:64-125`) stores top-level block byte cuts (`cuts`) so append edits re-parse only from the last finished line (`extend`) and evicts the superseded prefix entry (`c.docs.remove(&*last)`). | `DxUi::RichText::Detail::TokenizeCode` (`src/Controls/DxUi.RichCodeInk.h:27-36, 103-107`) clears **all** tokens (`output.clear(); return false;`) when a code fence exceeds `MaximumCodeTokens = 4096u`, flashing the entire code block from colored to uncolored on the 4,097th token! Worse, `Preparation::Phase::CodeNumbers` (`src/Controls/DxUi.RichSemanticGeometry.cpp:2560-2607, 2808, 3339-3348`) allocates up to **4,096 separate `PreparedLayout` (`IDWriteTextLayout`) objects** for fenced code line numbers in Semantic Markdown view (`Subtle`) and sets `clip = box.frame` so all 1,000+ line numbers draw every frame! | **Critical Wave 9 (`W9.1`, `W9.3`) additions**: (1) In `DxUi.RichCodeInk.h:27-36`, prefix-color up to a `32 KiB` newline boundary and retain the first `MaximumCodeTokens` tokens instead of `output.clear()`; (2) Delete `Phase::CodeNumbers` per-line `PreparedLayout` allocations in `DxUi.RichSemanticGeometry.cpp:2560-2607` and draw only visible code fence line numbers `[firstVisibleLine..lastVisibleLine]` via `NumberGlyphTable` (`0` `IDWriteTextLayout`s); (3) Add block-cut (`cuts`) tail re-parsing on append edits. |
| **8. `Cold<T: Empty>` rare-field boxing & static default singleton** | `Cold<T: Empty>(Option<Rc<T>>)` (`crates/rune/src/cold.rs:16-67`) defines `trait Empty { const EMPTY: &'static Self; }` so `Deref` returns `self.0.as_deref().unwrap_or(T::EMPTY)` (branchless static singleton read, zero heap allocation) and allocates only on `DerefMut`, shrinking `Node` from `2.4 KB` to `~0.6 KB`. | `DxUi::Control` (`include/DxUi/DxUi.h:1794-1834`) embeds 4 cold `std::wstring`s, 3 cold callback/form pointers, and 4 `std::optional`s inline (`sizeof(Control) = 296 B` Release / `336 B` Debug, addressed in `W10.1`). | **Enhance Wave 10 (`W10.1`)**: Pair `std::unique_ptr<ColdControlMetadata> _cold` with a `static const ColdControlMetadata kEmptyColdMetadata{}` singleton returned by `Control::Cold() const noexcept` (`return _cold ? *_cold : kEmptyColdMetadata;`). |
| **9. sRGB LUT vs `std::pow(..., 2.4)` & GIF opaque fast-skip** | `Srgb` (`crates/rune/src/image/mips.rs:125-152`) precomputes `decode: [f32; 256]`, `up: [f32; 256]`, and `coarse: [u8; 4097]`, eliminating `powf` across image premultiplication and OKLab color comparisons (`research/32` Rank #2), and skips opaque pixels (`if p[3] == 255 { continue; }`). | `DxUi` has 4 separate `std::pow((c + 0.055) / 1.055, 2.4)` implementations (`Utilities.h:21`, `DxUi.Theme.cpp:53`, `DxUi.PreparedMath.h:2855`, `DxUi.PreparedFlowchart.cpp:21`), and `ComposeGifCanvas` (`DxUi.RichImage.cpp:152-165`) runs 4 integer multiplies/divides per pixel even when `alpha == 255u` or `alpha == 0u`. | **Enhance Wave 9 (`W9.4`) & `Utilities.h`**: Consolidate all 4 `std::pow(..., 2.4)` helpers onto a single `constexpr float kSrgbByteToLinear[256]` lookup table in `src/Support/Utilities.h`, and fast-skip `alpha == 255u` / `alpha == 0u` in `ComposeGifCanvas` (`DxUi.RichImage.cpp:152-165`). |
| **10. Thread-local allocation counters & debug perf-warn guardrails** | `procwatch::alloc` (`crates/procwatch/src/lib.rs:30-115`) tracks thread-local allocation counts/bytes and `Snapshot::unaccounted()`. `perf_warn` warns once per callsite on UI-thread blocking stalls (`wall > 8 ms && wall > 4 * cpu`), virtual-list overbuild, and idle redraw loops. | `DxUi` tracks process-wide allocations in `Embedded.Tests.ComplexUiBenchmark.h` and has `FrameRuntime.h` / `Diagnostics.h`, but lacks a per-thread zero-allocation scope guard for unit tests. | **Enhance Wave 1 (`W1.2`)**: Add a thread-local `ScopedZeroAllocationCheck` guard for `Tests/Controls` and `Tests/Embedded` hot-path assertions, plus optional Debug diagnostics for UI-thread wall-vs-CPU stalls. |

### Verified `anomalyco/rune` (`07723732`) code mechanisms

```rust
// 1. Cold<T: Empty> with static EMPTY singleton deref (crates/rune/src/cold.rs:16-67)
pub(crate) trait Empty: Clone + 'static { const EMPTY: &'static Self; }
pub(crate) struct Cold<T: Empty>(Option<Rc<T>>);
impl<T: Empty> Deref for Cold<T> {
    type Target = T;
    fn deref(&self) -> &T { self.0.as_deref().unwrap_or(T::EMPTY) }
}
impl<T: Empty> DerefMut for Cold<T> {
    fn deref_mut(&mut self) -> &mut T {
        Rc::make_mut(self.0.get_or_insert_with(|| Rc::new(T::EMPTY.clone())))
    }
}

// 2. KeyHasher & deferred supersede + byte-cost stale eviction (text/hash.rs:21-97, layout_cache.rs:14-106)
const SEED: u64 = 0x243f_6a88_85a3_08d3;
const MUL: u64 = 0x9e37_79b9_7f4a_7c15;
impl Hasher for KeyHasher {
    fn finish(&self) -> u64 { self.0 }
    fn write(&mut self, bytes: &[u8]) {
        let mut h = rustc_hash::FxHasher::default();
        h.write(bytes);
        self.write_u64(h.finish());
    }
    fn write_u64(&mut self, i: u64) {
        let p = ((self.0 ^ i ^ SEED) as u128).wrapping_mul(MUL as u128);
        self.0 = (p as u64) ^ ((p >> 64) as u64);
    }
}
const LAYOUT_PER_BYTE: usize = 64;
const LAYOUT_BASE: usize = 2048;
const STALE_BYTES: usize = 3 << 20;
pub(crate) fn supersede(&mut self, shape: u64) {
    if shape != 0 { self.superseded.insert(shape); }
}

// 3. Visible-line binary search glyphs_in_band (crates/rune/src/text/glyph_emit.rs:54-68)
let (mut lo, mut hi) = (0usize, layout.len());
while lo < hi {
    let mid = (lo + hi) / 2;
    if layout.get(mid).is_some_and(|l| l.metrics().block_max_coord < band.start) { lo = mid + 1; }
    else { hi = mid; }
}
let lines = (lo..layout.len()).map_while(|i| layout.get(i).filter(|l| l.metrics().block_min_coord <= band.end));

// 4. Four-band hollow shadow culling (crates/rune/src/runtime/paint/bands.rs:17-47)
const MIN_HOLE: f32 = 96.0;
pub(super) fn push_shadow(out: &mut Vec<Instance>, inst: Instance, quad: [f32; 4], hole: [f32; 4], radii: [f32; 4]) {
    let e = radii.map(corner_extent).into_iter().fold(0.0, f32::max) + 1.0;
    let [hx0, hy0, hx1, hy1] = [hole[0] + e, hole[1] + e, hole[2] - e, hole[3] - e];
    if hx1 - hx0 < MIN_HOLE || hy1 - hy0 < MIN_HOLE { out.push(inst); return; }
    let [qx0, qy0, qx1, qy1] = quad;
    for [x0, y0, x1, y1] in [
        [qx0, qy0, qx1, hy0.clamp(qy0, qy1)], [qx0, hy1.clamp(qy0, qy1), qx1, qy1],
        [qx0, hy0.clamp(qy0, qy1), hx0.clamp(qx0, qx1), hy1.clamp(qy0, qy1)],
        [hx1.clamp(qx0, qx1), hy0.clamp(qy0, qy1), qx1, hy1.clamp(qy0, qy1)],
    ] {
        if x1 > x0 && y1 > y0 { out.push(Instance { uv: [x0, y0, -x1, y1], ..inst }); }
    }
}
```

### Documented negative results in `anomalyco/rune` (what Rune spiked and rejected)

1. **Per-word text shaper cache (`research/32-engine-plan.md:443-468` §6 Q2 & `research/37:185` — REJECTED)**: On real
   50 KB files (`PLAN.md`, `CHANGELOG.md`, `runtime.rs`, `57.4%–58.8%` word repeat rate) with mandatory per-paragraph
   BiDi/script analysis included, a word-level shaper cache yielded only **`1.46×–1.57×`** cold speedup (missing the
   `2.0×` bar) while breaking cross-space kerning/ligatures and UAX #14 line breaking. Paragraph-level caching
   (`shape_key` + in-place `break_key` + `supersede`) achieved **`150×` faster typing (`0.10 ms`)** and **`384/384`
   bit-exact runs**.
2. **Per-frame bump arena for retained control/layout nodes (`research/37-what-to-own-next.md:183` — REJECTED)**:
   Allocation was `< 1%` of busy time once `Cold<T>` and in-place node moves landed; an arena only reaches
   frame-lifetime objects while complicating ownership.
3. **Pooling node path strings across commits (`research/37-what-to-own-next.md:223-243` — REJECTED)**: Saved `3.5%`
   of commit allocations on the long card workload, but instructions moved `-0.02%` and commit-frame medians regressed
   in half the paired rounds (`7.94 → 8.11 ms`).
4. **Keeping node references to cached `Layout` across `relayout::solve` (`research/38-view-rerun-and-commit-cost.md:105-110`
   — REJECTED)**: Keeping `n.text_layout` (`Rc<Layout<Ink>>`) attached during `solve` left `Rc::strong_count == 2`
   alongside the layout cache, forcing a deep clone on width re-break for fixed-size wrapped boxes.

## Implementation sequence

Work on focused feature branches from `main`. Every performance-affecting wave captures a retained baseline before code
edits and compares the candidate on the identical machine and fixture (`test.ps1`, `performance-paired.ps1`,
`Publish-BenchmarkVerdict.ps1`). Tick a box only with its commit and passing test/receipt.

### Wave 0 — developer decisions that unblock implementation

Decided on 7 November 2026:

- [x] **D1. Sequencing with [RemainingWork_2026-11-04](RemainingWork_2026-11-04.md) and
  [ProductionReview_2026-11-02](../Done/ProductionReview_2026-11-02.md) (D9 history rewrite and shared Wave 1/2 items)**.
  **Decided 7 November: separate WIP plan, coordinated post-D9 on shared files.** Keep this plan as its own indexed WIP
  plan and coordinate the overlapping rich-text/control items (`EDIT-01`/`RENDER-01`/`RENDER-14`, `RENDER-02`,
  `RENDER-05`, `RENDER-09`, `SEM-02`, `MD-05`..`MD-07`, `CTRL-02`, `CTRL-04`, `CTRL-13`, `HOST-03`, `HOST-08`,
  `HOST-09`, `HOST-13`, `HOST-14`) after D9's history rewrite so each file is modified once.
- [x] **D2. `MenuPopup` 4,096-row UIA contract ([Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md))**.
  **Decided 7 November: preserve full popup row publication with `O(1)` incremental updates.** Keep the normative
  requirement (*"A popup publishes its rows' elements whether or not a client listens"*) and eliminate the `6.10 ms/step`
  keyboard/scroll spike via in-place `O(1)` focus/scroll updates on the popup's retained `AccessibilitySnapshot` plus an
  `O(1)` prefix-sum item Y-offset table.
- [x] **D3. Clean API evolution — no backwards-compatibility shims (`apiRevision` `4 -> 5`)**. **Decided 7 November: no
  need for compatibility.** All consumers (`RedXe`, `RedPrism`, `RedTilde`, `RedSalamander`) are pinned first-party
  consumers upgraded explicitly; do not retain legacy API shims when a clean API revision 5 contract eliminates
  allocations, memory or polling:
  1. **Base `Control` compaction & lifetime handle (Wave 10)**: Extract `std::unique_ptr<ColdControlMetadata>`, pack
     boolean/tri-state inheritance flags into `_packedStateBits`, **and** replace `std::shared_ptr<int> _lifetimeToken`
     with a non-throwing 64-slot slab `ControlLifetimeHandle` (`sizeof(Control)` `296 B -> ~112–120 B`, cutting
     per-control heap control-block allocations by 64× while keeping `GetLifetimeToken() const noexcept` 100%
     allocation-free and `std::terminate`-proof).
  2. **Direct deadline-aware `EmbeddedHost` scheduling (Wave 2)**: Replace boolean VBlank polling for discrete timers
     with a clean deadline-aware scheduling contract on `EmbeddedHost` (`GetNextWakeDelayMs()` / continuous-vs-deadline
     wake semantics) rather than carrying legacy compatibility wrappers.
  3. **Zero-copy `IGridModel` / `ITreeModel` text & column views (Wave 8)**: Replace by-value `std::wstring` /
     `GridColumn` model getters on hot paths with zero-copy `std::wstring_view` / caller-scratch formatting in API
     revision 5 (`capabilities.json` and `Tools/ConsumerApi/` updated together with `docs/upgrade.md`).
- [x] **D4. Non-virtual `ScrollPanel` GPU strip cache scope**. **Decided 7 November: gate on post-Wave-5 measurement.**
  Only implement D2D bitmap strip caching (`<= 16 MiB` within the 64 MiB per-view cap, with strict ClearType/subpixel/animation
  `Bypass` and `WorkBudget` auto-demotion) if dense non-virtual `ScrollPanel` wheel benchmarks still exceed `0.5 ms/step`
  on WARP after clip culling (Wave 3), `ShapedTextCache` (Wave 5) and `HoverFreeze` (Wave 2).

### Wave 1 — lockstep `IncrementalVsFresh` WARP oracle & baseline hygiene

- [ ] **W1.1. Paired baseline capture**: Record pre-change baseline receipts via `performance-paired.ps1` and `test.ps1`
  on `ComplexUi`, `Menu-Popup4096-Keyboard`, `ComboBox` 500-item overlay paint, idle focused caret wakeups, and 64 Ki-unit
  `RichTextEditor` typing.
- [ ] **W1.2. Lockstep differential oracle & thread-local zero-alloc guard (`Tests/Controls`, `Tests/Embedded`)**: Add
  `VerifyIncrementalParity` and `Controls.Tests.LayoutAndPaintOracle.cpp` (modeled on `gpui-fast`'s `oracle.rs` /
  `--verify` and `anomalyco/rune`'s 3-rule paragraph stacking matrix + `procwatch::alloc` thread-local allocation
  counters) running multi-step mutation scripts against an incremental host/view and a freshly invalidated WARP
  reference, asserting `0` BGRA pixel diff, bit-identical `GetBounds()` across all controls, identical
  `WindowHostAccessibilitySnapshot` records, positive cache-hit counters, and `0` thread-local heap allocations via
  `ScopedZeroAllocationCheck` on warm scroll, settled layout, and scalar `ProgressBar`/`Slider` updates.
- [ ] **W1.3. Host render & wake hygiene (`HOST-03`, `HOST-09`, `HOST-14`)**: Remove the dead `#else` duplicate of
  `ControlHost::Render` in [DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) (`line 4601`); stop bare
  `Ctrl` key transitions over non-editor controls from waking clean `EmbeddedHost` views (`HOST-03`); skip redundant
  text-input activation when focus is on a non-text control in `EmbeddedHost::Prepare` (`HOST-09`).

### Wave 2 — deadline-driven `AnimationDispatcher`, `Tick` subtree pruning & `HoverFreeze`

- [ ] **W2.1. Deadline timer scheduling & `poll_alone` (`AnimationDispatcher` & `ControlHost`)**: Add
  `RequestWakeAt(uint64_t wakeTickMs)` to [AnimationDispatcher.h](../../../src/Support/AnimationDispatcher.h) and
  `ControlHost` so discrete timers arm `SetTimer` for the exact next deadline (`minWakeTickMs - now`) instead of polling
  at `kFrameIntervalMs = 8u` (125 Hz) whenever no continuous interpolation animation is active. Following
  `anomalyco/rune`'s `TimerWake::poll_alone` (`timer_wake.rs:36-127`), drain due deadlines immediately and run discrete
  callbacks before scheduling `Prepare`/`Render`, skipping GPU presentation when no visible state changed.
- [ ] **W2.2. Caret & tooltip deadline migration**: Migrate `TextField::Tick` (530 ms,
  [DxUi.TextInput.cpp](../../../src/Controls/DxUi.TextInput.cpp)), `ComboBox::Tick` (530 ms,
  [DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp)), `RichTextEditor::Tick` (500 ms,
  [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp)), and `TooltipLayer::Tick` (500 ms show /
  5,000 ms auto-hide, [DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp)) from returning continuous `true` to
  scheduling their exact next flip/hide deadline, and honor `host.AreAnimationsEnabled()` / `GetCaretBlinkTime()` in
  `TextField::Tick` and `ComboBox::Tick`.
- [ ] **W2.3. `_subtreeHasTickables` pruning, clip-gated `LoopClock` & clean `EmbeddedHost` deadline contract (D3.2)**:
  Propagate a `_subtreeHasTickables` bit up `Panel` ancestors so `Panel::Tick` skips inert subtrees without touching
  lifetime handles; gate periodic animations (`ProgressBar` indeterminate pulse, `ThroughputGraph`) on ancestor
  scroll-clip intersection and `effectiveOpacity > 0.0f` while phase-aligning resumed loops via a monotonic `LoopClock`
  (`loop_clock.rs:24-91`); upgrade `EmbeddedHost` animation queries directly to expose exact next-deadline delay
  (`GetNextWakeDelayMs()`, API revision 5, no legacy compatibility wrapper) so embedded hosts sleep between 500–530 ms
  caret flips.
- [ ] **W2.4. Stationary-cursor `HoverFreeze` during wheel scroll (`hover_freeze.rs`)**: Suppress hover re-hit-testing
  and tooltip churn during wheel scrolling (`ScrollPanel`, `Grid`, `Tree`, `MenuPopup`) while the physical cursor screen
  point is unchanged, arming a single 100 ms one-shot settle wake when scrolling stops (zero idle timers).

### Wave 3 — pixel-snapped `DirtyRegion`, clip-rect subtree culling, live-resize brush retention & `CompositeBatch`

- [ ] **W3.1. Outward-pixel-snapped 4-rect `DirtyRegion` & scalar `Live<T>` bounds updates**: Add fixed-capacity
  `DirtyRegion` (`std::array<D2D1_RECT_U, 4>` snapped outward by 1 device pixel via `floor`/`ceil` to cover ClearType/AA
  fringes; coalescing to bounding box on the 5th rect or falling back to `full = true` above 65% surface area or on
  resize/DPI/theme/device/root/smoke-overlay change). Record both old and new visual bounds on `SetBounds`, `SetVisible`,
  hover/focus/press transitions, caret blink, `_tooltipLayer`, and scalar value updates (`ProgressBar::SetValue` and
  `Slider::SetValueInternal` in [DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 5137-5149, 6635-6680`).
- [ ] **W3.2. `WindowHost` & `EmbeddedHost` partial dirty preparation & border fill inset**: Feed `DirtyRegion` into
  `WindowHost::Render`'s existing `PushAxisAlignedClip` + `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` `Present1` path
  ([DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp)) and into `EmbeddedHost::Prepare`
  ([Embedded.cpp](../../../src/Rendering/Embedded.cpp)) when `s.bitmapNeedsFullPaint == false`; inset opaque rounded-rect
  fills by `0.5f * strokeWidth` beneath opaque border strokes (`anomalyco/rune` `border::fill_under`).
- [ ] **W3.3. Container clip-rect subtree culling**: Maintain a `currentClipDip` stack on `ControlHost` and skip
  non-intersecting children (`!HasClipEscapeDescendant()`) in `Panel::Paint`, `Panel::PaintOverlay`,
  `ScrollPanel::Paint`, `FlexPanel`, and `FormPanel`.
- [ ] **W3.4. Preserve `_brushCache` across `WM_SIZE`**: Remove `_brushCache.clear()` from
  `ControlHost::PrepareForSwapChainResize` ([DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp), `line 4504`),
  retaining `ID2D1SolidColorBrush` instances across swap-chain buffer resizes (clearing only in `DiscardDeviceResources` /
  device loss) to eliminate 30–64 `CreateSolidColorBrush` COM allocations per resize frame.
- [ ] **W3.5. Optional `EmbeddedGraphicsContext::CompositeBatch` (`draw_state.rs`)**: Add a batch composition entry
  point that binds shared D3D11 shaders, blend, rasterizer, sampler and vertex/index state once across `N` embedded views
  and queries `context->GetDevice()` once per batch.

### Wave 4 — container-owned 4-slot `MeasureLog`, unwrapped-fit carry & post-arrange width settle

- [ ] **W4.1. Fix `CTRL-02` & `O(N²)` `FlexPanel::ApplyLayout`**: In `Label::PrepareVisualContent`
  ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `line 2944`), check `_typographyOverride.has_value() || GetMnemonic() != L'\0'`
  before calling `IsControlInTree`, and enable visual preparation on `Label` only when an override or mnemonic is set.
  Replace the `O(N²)` per-child `stillValid()` child loop in `FlexPanel::ApplyLayout`
  ([DxUi.Layout.cpp](../../../src/Controls/DxUi.Layout.cpp), `lines 1023-1053`) with an `O(1)`
  `_childrenTopologyRevision` + `_interactionRevision` delta check (matching `FormField::ApplyLayout`).
- [ ] **W4.2. 4-slot `MeasureLog` & unwrapped-fit carry on `FlexPanel::ChildRecord` and `FormField::PartMeasure`**:
  Replace the 1-slot width cache on `FlexPanel::ChildRecord` and `FormField::PartMeasure`
  ([DxUi.h](../../../include/DxUi/DxUi.h)) with a 4-slot `MeasureLog` (`std::array<MeasureLogEntry, 4>`) keyed by
  existing `Control::_measureRevision`, plus unwrapped-fit carry (`unwrappedWidthDip <= queryMaxWidthDip` returns the
  cached single-line measurement immediately). Costs `0 B` on manual-layout controls.
- [ ] **W4.3. Pre-layout leaf size-stability cutoff (`LiveText`) & post-arrange `settle_measured_leaves`**: In
  `Label::SetText` ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 2817-2836`) and during
  `PreparePendingPageLayout`, check if a modified leaf's single-line `DesiredSize` (via `NumberGlyphTable`/`ShapedTextCache`
  or fixed constraints) matches its prior `DesiredSize`; when unchanged, skip `InvalidateMeasure()` and only mark the
  control's `DirtyRegion` (`anomalyco/rune` `LiveText`). After container `ApplyLayout` finishes, verify that any wrapped
  text leaf's arranged content width matches its last shaped width (`settle_measured_leaves`), re-shaping at the final
  arranged width in `PrepareVisualContent` when different.

### Wave 5 — three-tier `ShapedTextCache` (`DrawGlyphRun`), context-safe `NumberGlyphTable` & `O(1)` brush/format caches

- [ ] **W5.1. Tier 0 — PUA icon micro-table & context-safe `NumberGlyphTable` (`number_shaping.rs`)**:
  - Cache single-codepoint Segoe Fluent Icons (`U+E000..U+F8FF`) glyph index + advance in a 128-entry flat table drawn
    via `dc->DrawGlyphRun`.
  - Add per-`(IDWriteFontFace*, fontEmSize)` `NumberGlyphTable` covering **strictly context-free ASCII `0123456789.,+-%$KMBT`
    and `' '`** (never `:`, `/`, or `x` so `Segoe UI` OpenType `calt` `colon.case` never diverges), using cached glyph
    indices, design metrics and pairwise kerning adjustments, guarded by a **1-in-256 DirectWrite verification oracle**
    (plus first 32 calls in Debug) that sets `font.disagreed = true` on any mismatch.
- [ ] **W5.2. Tier 1 — width-independent unwrapped `CapturedGlyphRuns` via `IDWriteTextRenderer`, `KeyHasher` & `CachedElision`**:
  - Precompute `_textShapeKey` on `SetText`/typography changes using `anomalyco/rune`'s folded 128-bit multiply mixer
    (`SEED = 0x243f6a8885a308d3ULL`, `MUL = 0x9e3779b97f4a7c15ULL` in `text/hash.rs:21-97`).
  - Capture `DWRITE_GLYPH_RUN` slices (`wil::com_ptr_nothrow<IDWriteFontFace3>`, `fontEmSize`, `bidiLevel`, offsets,
    arena `glyphIndices`/`glyphAdvances`/`glyphOffsets`, underline/strikethrough, `unwrappedWidthDip`, `heightDip`,
    `baselineDip`, `hasColorGlyphs`) from `IDWriteTextLayout::Draw` once per distinct `(IDWriteTextFormat*, readingDirection, text)`
    up to 256 UTF-16 code units (~280 B/entry vs ~20 KB `IDWriteTextLayout`), excluding color/hover/selection from the key,
    plus `CachedElision` (`layout_cache.rs:21-25`) caching `(glyphRuns, headLen, tailLen)` for ellipsis-truncated strings.
  - Unify `MeasureText` ([DxUi.Layout.cpp](../../../src/Controls/DxUi.Layout.cpp)) and `DrawCenteredText`
    ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp)): `MeasureText` populates Tier 1 and eliminates its
    `std::vector<DWRITE_LINE_METRICS>` heap allocation; `DrawCenteredText` replays via `dc->DrawGlyphRun` with
    alignment computed from `unwrappedWidthDip` and device-pixel baseline snapping (`quantize_origin`), satisfying
    wrapped/ellipsis queries directly whenever `unwrappedWidthDip <= rectWidth`.
- [ ] **W5.3. Windows 10 + Windows 11 color-glyph support (`RENDER-05`)**: When `hasColorGlyphs` is set, use
  `ID2D1DeviceContext7::DrawGlyphRunWithColorSupport` when available (Win11 22H2+) and fall back to
  `IDWriteFactory4::TranslateColorGlyphRun` + `ID2D1DeviceContext4` (`DrawGlyphRun` / `DrawSvgGlyphRun` /
  `DrawColorBitmapGlyphRun`) on Windows 10 / WARP. Share this helper with `DxUi.RichTextScriptRenderer.h` to close
  `RENDER-05`.
- [ ] **W5.4. Tier 2 bounded wrapped/ellipsis/mnemonic sub-cache, in-place `SetMaxWidth` re-break & `SupersedeTextShape`**:
  Keep a 128-entry width-keyed `IDWriteTextLayout` sub-cache only for genuinely multi-line wrapped text, overflowing
  ellipsis strings, and `DrawTextWithMnemonic`. In `Label::PrepareVisualContent`
  ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 2958-3006`), when only `width`/`height` changed
  on resize, call `_typographyLayout->SetMaxWidth(width)` / `SetMaxHeight(height)` in place (`anomalyco/rune`
  `research/28`). Record `SupersedeTextShape(prevShapeKey)` on self-edits (`layout_cache.rs:86-106`) and bound stale
  layouts by byte cost (`LAYOUT_BASE = 2048 + 64 * textBytes`, `STALE_BYTES = 3 MiB`), trimming **only at
  frame/preparation start** in `ControlHost::TrimCaches()` and releasing all entries on `OnHidden`, `Detach`,
  `DiscardDeviceResources`, and DPI/theme changes.
- [ ] **W5.5. `O(1)` flat clock-LRU `_brushCache` & `_configuredTextFormats` + zero-alloc `CreateTextFormatForSpec`**:
  Replace the 64-entry linear scans and `.clear()` overflow cliffs in `ControlHost::GetSolidBrush`,
  `ControlHost::GetTextFormat`, and `ControlHost::TrimCaches` with open-addressed flat clock-LRU hash tables, and avoid
  allocating `std::wstring resolvedFamily` in `CreateTextFormatForSpec` ([Typography.h](../../../include/DxUi/Typography.h)).

### Wave 6 — `ComboBox` & `MenuPopup` `O(1)` overlay paint, cached material surfaces & prefix-sum geometry

- [ ] **W6.1. `ComboBox` `PopupLayoutCache` (`8,500 -> 0` `CreateTextLayout`/frame)**: Cache the computed `PopupLayout`
  on `ComboBox` ([DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp), `lines 836-918, 2528-2668, 3051-3073`),
  invalidated only when `_items`, typography/DPI, control bounds, window size, or containing `ScrollPanel` scroll offset
  changes, and route `ComputePopupWidthDip` and item drawing through `ShapedTextCache`.
- [ ] **W6.2. Retained popup backdrop/material/shadow D2D resources & 4-band hollow shadow culling**: Cache unit-space
  linear gradient brushes (matching `ColorPicker::EnsureBrushes` in
  [DxUi.EditorControls.cpp](../../../src/Controls/DxUi.EditorControls.cpp)), rounded clip geometry,
  `CLSID_D2D1GaussianBlur` effect, and `DrawDropShadow`'s 2 `ID2D1CommandList`s + 2 `CLSID_D2D1Shadow` effects
  ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 246-296, 797-810`) across frames in
  `PaintComboBoxBackdropSurface` / `PaintComboBoxPopupMaterialSurface` and `PaintMenuBackdropSurface` /
  `PaintMenuMaterialSurface` ([DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp)); when the popup's opaque interior
  hole (inset by `cornerRadiusDip + 1.0f`) is `>= 96.0f × 96.0f` DIPs (`MIN_HOLE`), emit shadow passes through 4
  perimeter `PushAxisAlignedClip` bands (`anomalyco/rune` `bands::push_shadow`, `bands.rs:17-47`).
- [ ] **W6.3. `MenuPopup` open/paint fast path & cached WIC factory**: Cache the WIC imaging factory in
  `GetMenuIconWicFactory` ([DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp), `line 805`); build a prefix-sum item
  Y-offset table at popup open for `O(1)`/`O(log N)` item-rect and visible-range queries; route menu labels, accelerators,
  checkmarks and icons through `ShapedTextCache`.

### Wave 7 — `TextField`, `SingleLineTextEditing`, `NativeTextInput`, `TagPicker` & `Splitter` de-churn

- [ ] **W7.1. Single-line `TextField` cache-hit & display-text zero-alloc path (`CTRL-13`)**: In
  `GetOrCreateSingleLineTextLayout` ([DxUi.SingleLineTextEditing.cpp](../../../src/Controls/DxUi.SingleLineTextEditing.cpp),
  `lines 486-542`) and `TextField::GetOrCreateSingleLineLayout`
  ([DxUi.TextInput.cpp](../../../src/Controls/DxUi.TextInput.cpp), `line 3546`), guard `cache->text` copy and
  `SetMaxWidth`/`SetMaxHeight` behind `!cacheHit`, only run `SecureWipe::SecureClear` when `IsPasswordMasked()` (or when
  clearing a previously masked cache), and add a zero-copy `GetDisplayTextView()` (`std::wstring_view`) for unmasked
  fields.
- [ ] **W7.2. Zero-copy IME composition guard & stack-buffered `HitTestTextRange`**: Add
  `ControlHost::HasNativeTextCompositionOrTarget(const Control*) const noexcept`
  ([DxUi.NativeTextInput.cpp](../../../src/Controls/DxUi.NativeTextInput.cpp)) so `TextField::Paint` and
  `ComboBox::Paint` skip copying `NativeTextInputState` when no composition is active; probe `HitTestTextRange` in
  `ComputeSingleLineSelectionPaintRectFromLayout` with a stack `std::array<DWRITE_HIT_TEST_METRICS, 8>` before heap
  fallback.
- [ ] **W7.3. Multiline `TextField` layout reuse**: Pass `_cachedMultilineLayout` into `DrawMultilineSelection` and
  `MeasureMultilineCaretRectDip` ([DxUi.TextInput.cpp](../../../src/Controls/DxUi.TextInput.cpp), `lines 543-619, 753-819`)
  and cache line metrics on the multiline cache entry to eliminate 2 `CreateTextLayout` COM allocations and 2 `std::vector`
  allocations per focused multiline paint frame.
- [ ] **W7.4. `TagPicker` layout cache & `Splitter` preview UIA coalescing**: Cache `TagPicker::ComputeLayout`
  ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 7475-7575`) across `Measure`/`Paint`/`HitTest`
  when tags, width and font are unchanged (drawing the `×` glyph without `tag + L" \x00D7"` heap concatenation); coalesce
  `RefreshWindowHostAccessibilitySnapshot` during `Splitter::NotifyChange` (`SplitterChangePhase::Preview`,
  [DxUi.EditorControls.cpp](../../../src/Controls/DxUi.EditorControls.cpp), `line 745`) to once per frame/commit; remove
  redundant tooltip `std::wstring` copy (`HOST-13`).

### Wave 8 — zero-allocation `Grid::Paint`, `Grid`/`Tree` row text caching & `O(1)` key navigation

- [ ] **W8.1. Zero-allocation `Grid::Paint` & zero-copy `IGridModel` / `ITreeModel` views (`54 -> 0` dirty-frame
  allocations in `ComplexUi`, D3.3)**: Hoist column metadata out of the per-row × per-column loop in `Grid::Paint`
  ([DxUi.Grid.cpp](../../../src/Controls/DxUi.Grid.cpp), `lines 4115, 4395`), reuse member `_visibleBodyItemsScratch`
  and `_orderedGroupsScratch` vectors, and upgrade `IGridModel` / `ITreeModel` in API revision 5 to expose zero-copy
  `std::wstring_view` / caller-scratch cell and column queries so even live-mutating cells allocate `0` heap strings.
- [ ] **W8.2. `Grid` & `Tree` `ShapedTextCache` / `NumberGlyphTable` routing**: Route `Tree` rows (`DrawTreeRow` in
  [DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp)) and `Grid` headers, spinners, icons, badges, and unwrapped
  single-line cells through `ShapedTextCache` (Tier 0 `NumberGlyphTable` for numeric/size columns + Tier 1 width-independent
  `CapturedGlyphRuns`, eliminating column-resize `CreateTextLayout` storms and 2,048-row scroll thrashing), while keeping
  `_cellTextLayouts` `IDWriteTextLayout` retention strictly bounded to on-screen multiline/clamped/complex cells per
  [Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md).
- [ ] **W8.3. `O(1)`/cached `Grid` & `Tree` keyboard navigation (`CTRL-04`)**: Cache `_visibleBodyItems` per
  `(_modelRevision, _groupStateRevision)` across `Grid` arrow/page/home/end keys
  ([DxUi.Grid.cpp](../../../src/Controls/DxUi.Grid.cpp), `line 5886`) and deduplicate the double `FindVisibleItemById`
  scan in `Tree::SetFocusedItemId` ([DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp), `lines 881-890`).

### Wave 9 — `RichTextEditor`, `Markdown`, cross-edit `RichSemanticGeometry` retention, diagrams/math & path cache

- [ ] **W9.1. Viewport-scoped `RichSourceGuides` & semantic `CodeNumbers` via `NumberGlyphTable` (`EDIT-01`, `RENDER-01`, `RENDER-14`)**:
  Move source guides and code ink to `PrepareVisualContent` (never withholding frames via `S_FALSE` from
  `PrepareContent`), scope preparation to `[scrollTop - viewport, scrollBottom + 2 * viewport]`, delete
  `Preparation::Phase::CodeNumbers` per-line `PreparedLayout` allocations in
  [DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp) (`lines 2560-2607, 2808, 3339-3348`),
  and draw both source gutter and `Subtle` fenced code block line numbers directly from `NumberGlyphTable` (`0`
  `IDWriteTextLayout` COM objects on 64K-line documents and 4,096-line code fences).
- [ ] **W9.2. `lineHeightMultiplier` 127-chunk positioned reuse (`RENDER-02`) & in-place `SetMaxWidth` resize re-break**:
  Enable 127-chunk positioned layout reuse in [DxUi.RichDocumentLayout.cpp](../../../src/Controls/DxUi.RichDocumentLayout.cpp)
  (`lines 307-313`) when `lineHeightMultiplier > 0` by applying uniform line-height scaling per chunk (preserving
  `anomalyco/rune`'s 3 paragraph stacking invariants from `research/32:389-419`); in `PreparedLayoutAccess::Apply`
  ([DxUi.RichTextLayout.cpp](../../../src/Controls/DxUi.RichTextLayout.cpp), `lines 280-454`), when shape inputs
  (`projection.text`, `projection.runs`, `options`, inline objects) are unchanged and only `width` changed during resize,
  call `state_->layout->SetMaxWidth(width)` in place and retain `state_->clusters` (`research/28`).
- [ ] **W9.3. Cross-edit retention of `PreparedMath`, `PreparedFlowchart`, bitmaps & SVGs (extends `SEM-02`) + bounded code fence scanner**:
  - In `PreparedSemanticGeometry::Begin`
    ([DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp), `lines 2179-2197`), remove the
    `state_->document == document` gate on `previousImages` and `previousNative`: retain uploaded `ID2D1Bitmap`s and
    `ID2D1SvgDocument`s keyed by `(RichImageId, frameRevision)` and retain `PreparedMath` / `PreparedFlowchart` across
    document edits keyed by `(blockKind, sourceSpanHash, quantizedWidthDip, fontSize, zoom, themeRevision)`.
  - In `SemanticBuilder` ([DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp),
    `line 1628`), carry parsed `MathExpression`, `FlowchartDiagram`, and unchanged semantic block layouts across localized
    Markdown edits, translating `topDip` by `deltaY`, and record top-level block byte cuts (`cuts`, `markdown/cache.rs:64-125`)
    for append tail re-parsing.
  - In `TokenizeCode` ([DxUi.RichCodeInk.h](../../../src/Controls/DxUi.RichCodeInk.h), `lines 27-36, 103-107`), prefix-color
    up to a `32 KiB` newline cut (`coloured_len`, `syntax/scan.rs:84-137`) and retain the first `MaximumCodeTokens = 4096u`
    tokens instead of clearing `output.clear()` on overflow.
  - Eliminate the duplicate encoded byte buffer in `RichImage::LoadFile` (`RENDER-09`,
    [DxUi.RichImage.cpp](../../../src/Controls/DxUi.RichImage.cpp), `line 249`) and fix the quadratic `<!--`/`<?`/`<![CDATA[`
    inline scans (`MD-05`..`MD-07`, [DxUi.MarkdownInlines.h](../../../src/Controls/DxUi.MarkdownInlines.h), `line 1044`).
- [ ] **W9.4. `PreparedMath` & `PreparedFlowchart` stack/contrast/arrowhead de-bloat + shared `kSrgbByteToLinear[256]` LUT**:
  Replace all 4 `std::pow((c + 0.055) / 1.055, 2.4)` helpers ([Utilities.h](../../../src/Support/Utilities.h) `line 21`,
  [DxUi.Theme.cpp](../../../src/Controls/DxUi.Theme.cpp) `line 53`,
  [DxUi.PreparedMath.h](../../../src/Controls/DxUi.PreparedMath.h) `line 2855`,
  [DxUi.PreparedFlowchart.cpp](../../../src/Controls/DxUi.PreparedFlowchart.cpp) `line 21`) with a shared `constexpr float kSrgbByteToLinear[256]`
  lookup table (`anomalyco/rune` `image/mips.rs:125-152`) and fast-skip `alpha == 255u` / `0u` in `ComposeGifCanvas`
  ([DxUi.RichImage.cpp](../../../src/Controls/DxUi.RichImage.cpp), `lines 152-165`); cache WCAG-resolved node/ink colors
  keyed by `(backgroundArgb, defaultInkArgb, dark, highContrast)` on `PreparedMath` and `PreparedFlowchart` instead of
  zeroing 9–60 KB stack arrays every paint; replace per-edge arrowhead `ID2D1PathGeometry` COM objects with 1 shared
  unit-arrowhead geometry positioned via `SetTransform`; size `FlowchartGraph`/`LayoutScratch` to actual node/edge counts.
- [ ] **W9.5. `O(log L)` `VisibleRange` (`glyphs_in_band`), `PaintIndex` capacity reuse & clickable-only hover (`interactive_text.rs`, `HOST-08`)**:
  Store cumulative `lineTops` in `PreparedLayout::State` during `Apply` so `PreparedLayout::VisibleRange`
  ([DxUi.RichTextLayout.cpp](../../../src/Controls/DxUi.RichTextLayout.cpp), `lines 590-609`) uses `std::lower_bound` /
  `std::upper_bound` (`O(log L)`, `0` `HitTestTextPosition` COM calls, matching `anomalyco/rune`'s `glyphs_in_band` in
  `text/glyph_emit.rs:54-68`); route `PreparedSemanticGeometry::VisibleRange` and `VisibleSourceRange`
  ([DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp), `lines 3120-3140, 3243-3259`)
  through `state.paintBoxes.ForEach(top, bottom, ...)`; reuse `this->nodes` vector capacity in `PaintIndex::Build`
  ([DxUi.PaintIndex.h](../../../src/Controls/DxUi.PaintIndex.h), `lines 30-52`); index clickable link/image/footnote
  spans so pointer motion over plain rich text skips hover invalidation and `O(N)` title scans (`HOST-08`).
- [ ] **W9.6. Origin-relative `(0,0)` `ID2D1PathGeometry` cache + `heavy` slot (`path_cache.rs`)**: Cache `(0,0)`-origin
  geometries (`<= 512` entries / `<= 4 MiB` + 1 `heavy` slot) for `TabControl` attached tabs
  (`PrepareTopRoundedAttachedRect`), `ThroughputGraph` traces, and `Flowchart` edges, translated at draw time via
  `dc->SetTransform`.

### Wave 10 — `Control` base-class memory diet (`296 B -> ~112–120 B`) & slab lifetime handles (API revision 5)

- [ ] **W10.1. `ColdControlMetadata` sidecar with static `Empty` singleton & packed state bits (D3.1)**: Move cold strings
  (`_connectedAnimationKey`, `_tooltipText`, `_accessibleName`, `_accessibleHelpText`, `_accessibleAutomationId`),
  `_accessibleFormProperties`, `_onAccessibleInvoke`, and `_onContextMenu` on `Control`
  ([DxUi.h](../../../include/DxUi/DxUi.h), `lines 1794-1834`) into on-demand `std::unique_ptr<ColdControlMetadata>`
  paired with a branchless `static const ColdControlMetadata kEmptyColdMetadata{}` singleton returned by
  `Control::Cold() const noexcept` (`anomalyco/rune` `cold.rs:16-67`), and pack the boolean flags +
  `FlowDirection`/`Density` tri-states into `uint32_t _packedStateBits`.
- [ ] **W10.2. Non-throwing 64-slot slab `ControlLifetimeHandle` (D3.1)**: Replace `std::shared_ptr<int> _lifetimeToken`
  directly in API revision 5 (no `std::weak_ptr<int>` compatibility wrapper) with a non-throwing 64-slot slab generation
  handle, shrinking `sizeof(Control)` from `296 B` (Release) / `336 B` (Debug) to `~112–120 B`, cutting per-control heap
  control-block allocations by 64×, and keeping `GetLifetimeToken() const noexcept` 100% allocation-free and
  `std::terminate`-proof under OOM fault injection. Update `capabilities.json` (`apiRevision: 5`),
  `Tools/ConsumerApi/`, and `docs/upgrade.md` together.

### Wave 11 — subtree-versioned UIA snapshot splicing, `MenuPopup` `O(1)` UIA & spatial hit-testing

- [ ] **W11.1. Subtree-versioned UIA snapshot splicing, `O(1)` range-value updates & scroll-delta fast path (`a11y.rs`, `live.rs`)**:
  Track `_a11ySubtreeRevision` across `Control`/`Panel` mutations; splice contiguous `AccessibleElementSnapshot` and
  `PointHitEntry` slices from the previous `WindowHostAccessibilitySnapshot` in `PublishWindowHostAccessibilitySnapshot`
  ([DxUi.Accessibility.cpp](../../../src/Controls/DxUi.Accessibility.cpp), `lines 1343-1386`), update `ProgressBar::SetValue`
  and `Slider::PublishRangeState` ([DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp), `lines 5147, 6420, 6677`)
  in place in `O(1)` without a 3-pass full-window UIA rebuild, and translate cached descendant `boundsScreenPx` by
  `-scrollDeltaPx` in one flat array pass on pure scroll flushes.
- [ ] **W11.2. `MenuPopup` `O(1)` focus/scroll UIA update (per D2)**: Update focused row state and scrolled item rects
  in-place inside `MenuPopup`'s retained 4,096-row `AccessibilitySnapshot` (`SynchronizeMenuAccessibility` in
  [DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp), `lines 3602-3635`), preserving full popup row publication per
  [Core_PerformanceAndResources.md](../../Core/Core_PerformanceAndResources.md) while cutting `Menu-Popup4096-Keyboard`
  from `6.10 ms/step` to `< 0.5 ms/step`.
- [ ] **W11.3. Spatial hit-testing (`bounds_tree.rs`)**: Reuse `PaintIndex` / 64 DIP uniform grid with `rest_max`
  z-order early exit in `ScrollPanel::HitTest` and `Panel::HitTest` when visible child count `>= 32`.

## Validation & closeout gates

1. `.\validate.ps1` (all five `validate-*.ps1` validators and `Tools/tests/Invoke-ToolingTests.ps1`),
   `.\Tools\Test-Changelog.ps1`, and `.\format.ps1 -Check` green after every wave.
2. Wave 1 lockstep `IncrementalVsFresh` WARP parity tests green across `Tests/Controls` and `Tests/Embedded` (covering
   DPI 100%/150%/200%, LTR/RTL, light/dark/High Contrast themes, Windows 10/11 color glyphs, WARP device-loss recovery,
   and exhaustive OOM fault injection).
3. Paired `test.ps1`, `performance-paired.ps1`, and `Publish-BenchmarkVerdict.ps1` receipts in x64 `Debug`, `Release`,
   and `ASan Debug`, plus builds in all three `ARM64` configurations (`Debug`, `Release`, `ASan Debug`), demonstrating:
   - `ComplexUi` dirty-frame C++ allocations: `54/frame -> 0/frame` (`Release`).
   - `ComboBox` 500-item overlay paint `CreateTextLayout` allocations: `8,500/frame -> 0/frame`.
   - Idle focused caret wakeups: `~125 Hz -> ~1.9 Hz` (`0%` idle CPU between blinks).
   - `Menu-Popup4096-Keyboard`: `6.10 ms/step -> < 0.5 ms/step`.
   - Measurable dirty-frame prepare/render p50/p95 latency reduction on `ComplexUi` and 64 Ki-unit `RichTextEditor`
     typing with math/Mermaid/images, with zero clean-frame or hidden-memory regressions.
4. Interactive focus-taking suites (`Menu`, `NativeTextInput`, and the two menu resource fixtures via
   `.\test.ps1 -Interactive`) executed with the developer's explicit desktop-time agreement.
5. Update normative domain contracts, `docs/performance.md`, `Measurements/` receipts, and one dated `Changes/` fragment
   per PR; move this plan to `Specs/Plans/Done/` only when every accepted wave box is ticked with its commit and test.
