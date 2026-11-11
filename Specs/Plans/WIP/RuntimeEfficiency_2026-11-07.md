# Runtime efficiency and rendering quality, 7 November 2026 (revised 11 November 2026)

Status: SPEC READY — the 7 November draft (`rune` `0772373`, `gpui-fast` `61dccec`) was red-team reviewed on
8 November 2026 (`DxUi` `c0ead00a`, `rune` `a7f276c`, `gpui-fast` `fb8cf94`, `BUI` `e8126ca`) and then re-audited from
the ground up on 11 November 2026 across the full `DxUi` tree at `4acabe0e` and the current upstream heads of
[`longbridge/gpui-fast`](https://github.com/longbridge/gpui-fast/tree/af019de83a26f0d94c65ef408fdecebca1c25e9c) at
`af019de`, [`anomalyco/rune`](https://github.com/anomalyco/rune/tree/0ea75abef0a6caa014067a88f89518b9a04cce2b) at
`0ea75abe` (including `research/01..38`, `gpu-floor` and `engine-spike`),
[`makepad/makepad`](https://github.com/makepad/makepad/tree/506ec05105b9adb08d1d21f9879df181b5244ea7) at `506ec051`
(MIT; see [Makepad study](#makepad-study-makepadmakepad-at-506ec051)), and
[`ppmpreetham/BUI`](https://github.com/ppmpreetham/BUI/tree/ae284781c2ae9b030aa795c7c0c1816678aafcf5) at `ae284781`
(see [BUI review](#bui-review-ppmpreethambui-at-ae284781)). Decisions D1, D2 and D4 stand; revised D3 and D5–D11 await
the developer's confirmation (see [Decisions](#decisions)). Plan only; no code has been implemented.

- **Owning contracts**: [performance and resources](../../Core/Core_PerformanceAndResources.md),
  [architecture](../../Core/Core_Architecture.md), [failure handling](../../Core/Core_FailureHandling.md),
  [controls and layout](../../UI/UI_ControlsAndLayout.md), [theme and typography](../../UI/UI_ThemeAndTypography.md),
  [input and accessibility](../../UI/UI_InputAndAccessibility.md), [design system](../../UI/UI_DesignSystem.md),
  [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md), [Win32 host](../../Rendering/Rendering_Win32Host.md),
  [toolchain and consumption](../../Build/Build_ToolchainAndConsumption.md),
  [validation](../../Testing/Testing_Validation.md) and [documentation](../../Core/Core_Documentation.md).
- **Coordinated plans**: [remaining work](RemainingWork_2026-11-04.md) owns D-P1 (the timing policy), D-Q1
  (platform/consumer acceptance) and every closed item cited below; [explicit state machines](ExplicitStateMachines_2026-11-10.md)
  owns private non-breaking state-machine refactors and lifetime fixes (`DXUI-SM-01`..`DXUI-SM-08`) coordinated with
  `P2.1`/`P2.3`/`P1.15`. The archived [production review](../Done/ProductionReview_2026-11-02.md) holds the original
  `CTRL-*`/`HOST-*` findings. This plan takes over only the open IDs it names. It never reopens a closed one or weakens
  an accepted bound in [performance and resources](../../Core/Core_PerformanceAndResources.md) (including `Grid`'s
  last-paint cell text-layout retention limit and application-supplied native occlusion visibility).

## Verdict on the 7 November draft and 11 November full reanalysis

The original draft studied the right projects and identified genuine hotspots, and the 8 November pass separated verified
claims from refuted ones. Full subsystem auditing on 11 November closed the remaining blind spots:

1. **About a fifth of the 7 November draft was already done.** `EDIT-01`/`RENDER-01`/`RENDER-14` were closed in
   [EditorCosmeticPreparation](../Done/EditorCosmeticPreparation_2026-11-06.md): source guides are viewport-scoped,
   capped at 4 layouts per turn and never withhold a frame (though retaining `PreparedLayout` entries outside the
   viewport remains open in `E26`/`P1.11`). `RENDER-02` for Source mode was closed in
   [SourceSpacingAndRadioNaming](../Done/SourceSpacingAndRadioNaming_2026-11-07.md). `RENDER-05` (color-glyph
   capability) was closed in [ScriptColorPreparation](../Done/ScriptColorPreparation_2026-11-06.md). `SEM-02` was
   closed in [SemanticAndDiagramResources](../Done/SemanticAndDiagramResources_2026-11-07.md), and `MD-05`..`07` in
   [MarkdownScanBounds](../Done/MarkdownScanBounds_2026-11-06.md).
2. **Several 7 November "facts" were wrong or overstated.** There is no `6.10 ms/step` measurement and no
   `Menu-Popup4096-Keyboard` fixture anywhere. The host brush and format caches are hash maps capped at 256 and 96, not
   64-entry linear scans. `GetMenuIconWicFactory` is already a cached static (though its CRT `atexit` lifetime after
   `CoUninitialize` is a teardown hazard; see `E14`). `TagPicker::ComputeLayout` and the `tag + L" \x00D7"`
   concatenation do not exist. `PreparedFlowchart::Draw` zeroes no stack arrays and creates no per-draw geometry (its
   actual waste is 4 of 5 zero-length `DrawLine` segments per orthogonal edge and a `>120 KiB` preparation buffer; see
   `E71`). `GetVisibleItemRect` is constant time. `Control` has 5 cold strings, not 4. The 8 ms `SetTimer` is not
   "125 Hz": `USER_TIMER_MINIMUM` raises it to 10 ms and the system tick quantizes it to about 64 Hz, with jitter. The
   API revision is already 5, so a breaking change makes it 6.
3. **The reference projects were partly misread.** `device_wake.rs` handles zero-poll D3D11 device removal, not widget
   deadline timers. `ChangedBounds` is draw-order batching, not dirty rects. `em_metrics.rs` is a font-fallback fix.
   gpui-fast's number set excludes space. rune's 4-band shadow split saves fragment overdraw in its own shader; with a
   `CLSID_D2D1Shadow` effect it would multiply effect passes by four, an anti-pattern.
4. **One rendering assumption looks wrong.** The partial-present path assumes `FLIP_SEQUENTIAL` with `BufferCount = 2`
   keeps the previous frame outside the dirty rect. DXGI documents that the reused back buffer holds the frame from
   `BufferCount` presents earlier; the source comment at `DxUi.WindowHost.cpp` around line 5071 assumes the opposite.
   Extending control-driven dirty rects on this assumption would show stale pixels on alternating frames. `P0.4` proves
   or disproves the behavior before anything builds on it.
5. **No usable timing acceptance signal.** D-P1 in [remaining work](RemainingWork_2026-11-04.md) records that paired
   timing is inconclusive: 71 of 72 controls are unstable and the base policy is unqualified. Every item here gates on
   **deterministic work counters** (layouts created, effects/layers/bitmaps created, tree visits, timer wakes, UIA
   passes, heap allocations and bytes). Wall/CPU timing stays advisory until D-P1 qualifies its policy.
6. **Beauty and motion belong beside CPU efficiency.** `V1`–`V8` cover closed-form springs, perceptual OKLab color
   transitions, crisp fractional-DPI hairlines, single-pass analytic Gaussian shadows, real popup materials, smooth and
   fractional scrolling, command-list connected animations and subpixel-stable text glide.
7. **Whole `DxUi` subsystems and upstream commits after 8 November were missing (`E46`–`E75`).** The 11 November audit
   uncovered major steady-state churn in `ThroughputGraph`, `StatusStrip`, `PageHost`, `ColorPicker`, `NumericStepper`,
   ~30 unguarded control setters, `Panel` `weak_ptr` atomic locking, `Grid` badge/resize/sort/selection paths, `Tree`
   expansion snapshots, `WindowHost` device-loss retry spin, `TrimCaches` `_forceAllVisualPreparation = true` format-cliff
   storms, `DCompositionCreateDevice2(dxgiDevice)` TDR visual teardown, modal `WM_SIZING` `WM_PAINT` starvation,
   `FrameClock::SmoothDeltaUs` unfiltered vblank jitter and per-child `Failure::Scope` `set_terminate` calls,
   `DxUi.Typography.h` global mutex/leak/ALPC probes, `EmbeddedHost` un-gated 4-pass UIA rebuilds and `ITextRangeProvider`
   `O(N²)` scans, TSF `TextStoreACP` `15–25×` full-document IME copies, `RichDocument` piece-tree non-coalescing
   1-character buffers, `SourceDisplayMap` `1.05–4.2 MiB` keystroke vectors, `libfyaml` frontmatter reparses, uncached
   `2·R·C` semantic table layouts, `RichImageDecoder::Decode` synchronous `32 MiB` copies, and `MathFontData` `22,500`-call
   rebuilds + per-atom `IDWriteTextLayout`s, alongside upstream advances across `gpui-fast` (`#104`–`#133`), `rune`
   (`#248`–`#570`, `research/01..38`, `gpu-floor`), `makepad` (`506ec051`, including its waitable-swap-chain credit
   deadlock guard and empirical `Slug` driver findings) and clean-room `BUI` (`5cea7a6`–`ae284781`).

## Evidence ledger

Static findings carry exact file references at `DxUi` `4acabe0e`. "Counter" means `P0.1` instrumentation must confirm
the baseline before the item closes; ledger numbers are hypotheses to measure, never receipts.

| # | Claim | Verdict | Evidence (`4acabe0e`) | Consequence |
| --- | --- | --- | --- | --- |
| E1 | Animation dispatcher polls at 8 ms whenever any subscriber exists | Verified (effective ~64 Hz) | `kFrameIntervalMs = 8u`, `SetTimer` in [AnimationDispatcher.h](../../../src/Support/AnimationDispatcher.h) | P2.1 |
| E2 | `TextField`/`ComboBox` caret (530 ms), `RichTextEditor` caret (500 ms) and `TooltipLayer` (500 ms show, 5,000 ms hide) return `true` every tick; `TextField` also ticks/draws caret when `_readOnly` | Verified | [DxUi.TextInput.cpp](../../../src/Controls/DxUi.TextInput.cpp) `:1707, 1789, 1804`, [DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp) `:1901`, [DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp) `:5505-5511`, [DxUi.Controls.cpp](../../../src/Controls/DxUi.Controls.cpp) `:12152` | P1.3, P2.2, P2.3 |
| E3 | `TextField`/`ComboBox` ignore `GetCaretBlinkTime`, reduced motion and `SPI_GETCARETTIMEOUT` | Verified (`RichTextEditor` honors reduced motion; `SPI_GETCARETTIMEOUT` appears nowhere) | same files | P2.2 |
| E4 | Every animation tick walks the whole tree | Verified | `Panel::Tick` visits every visible child; `EmbeddedHost::AdvanceAnimation` calls `_root->Tick` | P2.1 |
| E5 | `EmbeddedHost::Prepare` clears and repaints the whole surface for any change | Verified | `dc->Clear(...)` then `_root->Paint` in [Embedded.cpp](../../../src/Rendering/Embedded.cpp) | P3.2 |
| E6 | Control invalidation is always full-window | Verified | `ControlHost::Invalidate()` calls `InvalidateRect(_hwnd, nullptr, FALSE)` | P3.1 |
| E7 | Partial present keeps previous-frame content | **Unproven and probably false** | `BufferCount = 2`, `FLIP_SEQUENTIAL`, comment near [DxUi.WindowHost.cpp](../../../src/Controls/DxUi.WindowHost.cpp) `:5071` | P0.4 decides; P1.12/P3.3 fix |
| E8 | `Panel`/`ScrollPanel` paint every visible child regardless of clip | Verified | `DxUi.Controls.cpp:1533, 11311` | P3.4 |
| E9 | The host brush cache is cleared on every swap-chain resize | Verified, and cleared **twice** | `PrepareForSwapChainResize` and `RecreateBrushCache` in `DxUi.WindowHost.cpp` | P1.12 |
| E10 | Host brush/format caches use 64-entry linear scans | **Refuted** | `std::unordered_map`, limits 256/96 in [DxUi.h](../../../include/DxUi/DxUi.h); clear-on-overflow cliff is real | P4.5 |
| E11 | `DrawTextW` at "90+ call sites" | **Overstated** | 7 raw `DrawTextW` sites; `DrawCenteredText` has 63 callers; 28+ `CreateTextLayout` sites | P4.1 |
| E12 | `ComboBox` popup runs `1 + 2V` `UpdatePopupLayout` calls per paint (`8,500` layouts at `N = 500, V = 8`), plus `2–3×` (`2N–3N`) per `WM_MOUSEMOVE` pixel while open and `2–3×` per typed char even while closed | Verified | `DxUi.ComboBox.cpp:1127, 1845, 2114, 2213, 2311, 2322, 2577-2665` | P1.1 |
| E13 | Popup blur, layer, gradients (even on `OverlayMaterial::Solid`), both shadow passes and `PaintTransientSurfaceBackdrop` blur/geometry/layer are recreated every paint | Verified | `DxUi.ComboBox.cpp:217-245, 348/359, 414-428`, [DxUi.Menu.cpp](../../../src/Controls/DxUi.Menu.cpp) `:590-619, 717/728, 778-793`, `DxUi.Controls.cpp:258/283`, [DxUi.cpp](../../../src/Controls/DxUi.cpp) `:152-201` | P1.2, V4 |
| E14 | WIC factory created per icon (refuted), **but** static `wil::com_ptr_nothrow<IWICImagingFactory>` releases post-`CoUninitialize`, `MenuPopup` creates a fresh `D3D11CreateDevice` per open, and `NativeMenuInterop` clones `HBITMAP`s | Refuted per-icon; **lifecycle & device churn verified** | `DxUi.Menu.cpp:814, 1244`, [NativeMenuInterop.cpp](../../../src/Controls/NativeMenuInterop.cpp) `:56-109, 746` | P1.2, P1.12 |
| E15 | Popup backdrop does synchronous GDI `BitBlt(..., SRCCOPY \| CAPTUREBLT)` plus heap `vector<uint8_t>` + scalar alpha loop because bitmap uses `PREMULTIPLIED` instead of `IGNORE` | Verified | `DxUi.cpp:139, 278-345` | V5 (and P1.2 until V5) |
| E16 | `TextField` cache hit runs `SecureClear` plus a string copy; masking applies to every field; composition state deep copy | Verified | [DxUi.SingleLineTextEditing.cpp](../../../src/Controls/DxUi.SingleLineTextEditing.cpp) `:527`, `DxUi.TextInput.cpp:3620`, [DxUi.NativeTextInput.cpp](../../../src/Controls/DxUi.NativeTextInput.cpp) `:2094` | P1.3 |
| E17 | Multiline `TextField` bypasses `_cachedMultilineLayout` across **10 hot paths**, including 2 layouts before `wheelStepCount == 0` and 5 per `PageDown` | Verified | `DxUi.TextInput.cpp:352, 482, 585, 713, 728, 765, 811, 1958, 2088, 2180-2203, 2955, 3063, 3215, 3679` | P1.4 |
| E18 | `TagPicker` per-paint string concatenation | **Refuted** | `LayoutParts()` runs on mutation only | dropped |
| E19 | `Splitter` preview rebuilds the whole-window UIA snapshot every pointer move | Verified | [DxUi.EditorControls.cpp](../../../src/Controls/DxUi.EditorControls.cpp) `:747` | P1.5 |
| E20 | `ProgressBar`/`Slider` value changes run full invalidation plus a `General` **4-pass** UIA rebuild (`IndexAccessibilitySnapshot` reserves `4×` maps) | Verified | `DxUi.Controls.cpp:5186, 6467`, [DxUi.Accessibility.cpp](../../../src/Controls/DxUi.Accessibility.cpp) `:1408-1483, 2199` | P1.6, P7.3 |
| E21 | `Label::Label` eagerly sets `NeedsVisualPreparation(true)` on plain labels, and both `Label` and `Button::PrepareVisualContent` run `IsControlInTree` DFS before cheap guards (`CTRL-02`) | Verified | `DxUi.Controls.cpp:2808-2811, 2970, 3180-3192` | P1.7 |
| E22 | `Grid` key navigation allocates and walks all rows per key (`CTRL-04`); `Tree::SetFocusedItemId` double lookup | Verified | [DxUi.Grid.cpp](../../../src/Controls/DxUi.Grid.cpp) `:5937`, [DxUi.Tree.cpp](../../../src/Controls/DxUi.Tree.cpp) `:892/901` | P1.7 |
| E23 | `O(N²)` child/field revalidations in `FlexPanel` (`BuildLayout`, `ApplyLayout`), `FormPanel` (`BuildLayout`, `ApplyLayout`, `AddField`) and `TabControl::UpdateVisiblePageBounds` | Verified | [DxUi.Layout.cpp](../../../src/Controls/DxUi.Layout.cpp) `:670-682, 726, 1030`, [DxUi.Forms.cpp](../../../src/Controls/DxUi.Forms.cpp) `:651-653, 747-759, 974-991`, `DxUi.Controls.cpp:9376-9447` | P1.8 |
| E24 | `Grid::Paint` calls `GetColumn` (by-value strings) `C + C·rows` times per frame | Verified | `DxUi.Grid.cpp:4155, 4435` | P8.3 |
| E25 | Semantic geometry reuse is gated on `state_->document == document` and every edit creates a new `SemanticDocument` | Verified (the real cross-edit loss of math, diagrams, tables and images) | [DxUi.RichSemanticGeometry.cpp](../../../src/Controls/DxUi.RichSemanticGeometry.cpp) `:2437/2447`, [DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp) `:2848-2855` | P6.1 |
| E26 | Semantic code-fence line numbers & `RichSourceGuides` allocate per-line `PreparedLayout`s across all lines; `CodeLabels` recreates identical `"copy"`/language badge layouts per fence | Verified | `DxUi.RichSemanticGeometry.cpp:2849-2864, 2932/2943, 4073-4082`, [DxUi.RichSourceGuides.h](../../../src/Controls/DxUi.RichSourceGuides.h) `:148-155, 219-228` | P1.11 |
| E27 | `TokenizeCode` discards all coloring above 4,096 tokens | Verified | `output.clear(); return false;` in [DxUi.RichCodeInk.h](../../../src/Controls/DxUi.RichCodeInk.h) | P1.10 |
| E28 | `PreparedMath::DrawColored` zeroes a 32 KiB (`4096 × 8 B`) stack array and calls `pow` per paint; `ResolveDiagramLabelBackground` runs a 32-step `Contrast` binary search (`~192` `powf` calls) + `target->GetFactory` per draw | Verified | [RichMath.h](../../../include/DxUi/RichMath.h) `:11`, [DxUi.PreparedMath.h](../../../src/Controls/DxUi.PreparedMath.h) `:3015-3030`, [DxUi.PreparedFlowchart.cpp](../../../src/Controls/DxUi.PreparedFlowchart.cpp) `:21, 148-191, 812-814` | P1.9 |
| E29 | `pow`-based sRGB decode runs on paint/mutation paths in `Utilities.h` (`TextField`/`ComboBox`), math, flowchart **and** `ChooseContrastingTextColor` (`TagPicker::Paint`, `NumericStepper::SetValue`) | Verified | [Utilities.h](../../../src/Support/Utilities.h), `DxUi.Controls.cpp:7555`, `DxUi.EditorControls.cpp:1734` | P1.9 |
| E30 | `PreparedLayout::TryVisibleRange` is linear with a `HitTestTextPosition` per line; semantic `TryVisibleRange` and `TryVisibleSourceRange` scan all `boxes` and ignore `paintBoxes` | Verified | [DxUi.RichTextLayout.cpp](../../../src/Controls/DxUi.RichTextLayout.cpp) `:762-785`, `DxUi.RichSemanticGeometry.cpp:3651-3682, 3966` | P1.9 |
| E31 | `PaintIndex::Build` discards vector capacity on every rebuild | Verified | [DxUi.PaintIndex.h](../../../src/Controls/DxUi.PaintIndex.h) | P1.9 |
| E32 | `ComposeGifCanvas` has no fast path for alpha 0/255 | Verified | [DxUi.RichImage.cpp](../../../src/Controls/DxUi.RichImage.cpp) `:158` | P1.9 |
| E33 | `RichImage::LoadFile` duplicates the encoded buffer (`RENDER-09`) | Verified | `DxUi.RichImage.cpp:251, 506` | P1.13 |
| E34 | `MenuPopup` keyboard step costs 6.10 ms | **Not found** | no fixture or receipt | P0.2 measures; P7.4 conditional |
| E35 | Unrecorded `sizeof(Control)` (`~328 B` inline + `39 B` header) and bloated `Button`-derived toggles (`Checkbox` `~896 B`, `Toggle` `~928 B`, `RadioButton` `~880 B`) | Verified static layout; no test probe yet | `DxUi.h:1624-1870, 2979-3306`, [ControlAllocation.cpp](../../../src/Foundation/ControlAllocation.cpp) `:25-62` | P8.1 |
| E36 | Lifetime token is an eager `make_shared<int>` | Verified, 460 call sites | `DxUi.h:1853` | D3 revision: slab handle withdrawn |
| E37 | `HOST-14` dead `#else` `Render` body | Verified dead (`DXUI_ENABLE_DIAGNOSTICS == 1` in [Configuration.h](../../../include/DxUi/Configuration.h)) | `DxUi.WindowHost.cpp:4708-4952` | P1.12 |
| E38 | `HOST-03`, `HOST-09` | Verified open | `Embedded.cpp:1055, 664` | P1.12 |
| E39 | Native host never sets text antialias mode or rendering params | Verified; embedded forces grayscale | `Embedded.cpp:283` | P4.6 |
| E40 | Color transitions are sRGB-space lerps | Verified | `BlendColor` in [DxUi.Theme.cpp](../../../src/Controls/DxUi.Theme.cpp) | V2 |
| E41 | No springs; interrupted lerp transitions restart at the wrong speed | Verified | `Slider::BeginVisualTransition` re-anchors and restarts its clock | V1 |
| E42 | Connected animation draws a placeholder outline, not content | Verified | `PageHost::PaintOverlay` in `DxUi.Controls.cpp` | V7 |
| E43 | 1 DIP strokes at fractional DPI produce blurred 1.25/1.5 px borders | Verified | shared rounded-rect stroke helpers in `DxUi.Controls.cpp` | V3 |
| E44 | Wheel scrolling is quantized and immediate, with no inertia, fractional touchpad accumulation or edge fades | Verified | `DxUi.Grid.cpp:5891-5896`, `DxUi.Tree.cpp:2155` | P1.7, V6 |
| E45 | Windows 10 color-glyph path (`TranslateColorGlyphRun`) is absent; only `ID2D1DeviceContext7` is used | Verified; Windows 10 is in the embedded baseline ([toolchain](../../Build/Build_ToolchainAndConsumption.md)) | [DxUi.RichTextScriptRenderer.h](../../../src/Controls/DxUi.RichTextScriptRenderer.h) `:218` | P4.1 |
| E46 | `ThroughputGraph::Paint` creates per-frame `ID2D1PathGeometry`, unused `ID2D1SolidColorBrush`, `CreateLayer`, CPU-rasters `180×64` HSV pixels + `46 KB` `CreateBitmap`, zeroes `70.7 KB` stack arrays, draws `358` lines and `3` raw `DrawTextW`s; `ThroughputGraphSample` is `408 B` (`73.4 KB`/graph) and `ThroughputGraphHueWeight` pads `8 B` (`24 B` vs `16 B`) | Verified | `DxUi.Controls.cpp:5747-6319`, `DxUi.h:4714-4729` | P1.14, P8.1 |
| E47 | `StatusStrip::Paint` / `ElideLeadingForWidth` runs binary-search `CreateTextLayout` + `wstring` allocs (~12 layouts + ~11 strings/frame) and slices raw `wchar_t` offsets, splitting UTF-16 surrogate pairs and grapheme clusters | Verified | `DxUi.Controls.cpp:10386-10511` | P1.14 |
| E48 | `PageHost::PaintPage`/`PaintOverlay` calls `dc->CreateLayer` up to 4 times per transition frame instead of `PushLayer(..., nullptr)` | Verified | `DxUi.Controls.cpp:1802-1815, 1935-1948` | P1.14 |
| E49 | `ColorPicker` queries `dc->GetDevice` every paint, draws 6+ uncached texts, triggers **4 full 4-pass UIA rebuilds per HSV drag move**, loses saturation at `V = 0`, and allocates 7 SSO-exceeding default `Labels` strings; `NumericStepper::SetValue` lacks an unchanged-value guard and calls `3× pow` | Verified | `DxUi.EditorControls.cpp:977-1017, 1229-1280, 1691-1904`, `DxUi.h:6082-6099` | P1.14, P8.1 |
| E50 | ~30 control setters lack unchanged-value guards; `StackPanel::SetOrientation`/`SetGap`/`SetPadding` unconditionally call `RequestInvalidate()`; `CardPanel::SetCornerRadius` mutates `_cornerRadius` without invalidating (`0` repaint bug) | Verified | `DxUi.cpp`, `DxUi.Controls.cpp:2736-2739, 10660-10701`, `DxUi.EditorControls.cpp` | P1.14 |
| E51 | `Panel::Paint`/`PaintOverlay`/`Tick` run `4N` atomic control-block operations per frame via `AcquireControlLifetime` (`weak_ptr::lock` before and after each child) | Verified | `DxUi.Controls.cpp:77-127, 1528-1595` | P1.8 |
| E52 | `MenuBar` formats unguarded `Debug::Info` strings in Release on hover/key/open and invalidates all item text widths on pure horizontal `MenuBar` resize; `MenuPopup` renders twice on open (`RenderPopup` + `WM_PAINT`) and allocates 2 `ParseMenuLabel` strings per row per paint | Verified | `DxUi.Controls.cpp:199-217, 8052-8270, 8287`, `DxUi.Menu.cpp:3224, 3244, 4224, 4243` | P1.2, P1.12 |
| E53 | `Grid` (within its last-paint cell layout retention contract) creates uncached `PaintBadgeCell` layouts per frame, destroys visible cell layouts on column drag-resize instead of `SetMaxWidth`, recomputes `ResolvedColumnLayout` & `CollectOrderedGroups` per frame, scans all groups linearly in `ComputeViewportMetrics`, materializes `O(R)` vectors/sets in `GridSelectionModel`, and copies `GridCellValue` strings during `InMemoryGridModel::RebuildView` sort | Verified | `DxUi.Grid.cpp:609-735, 1041-1095, 1395-1455, 4977, 5042-5080, 6756, 6955, 7393` | P1.7, P4.3, P8.3 |
| E54 | `Tree::BeginTreeExpansionAnimation` deep-copies up to `4,096` `TreeItemData` records (`4 × wstring` each) twice (`emplace(animation)` avoids `std::move` because MSVC `_ITERATOR_DEBUG_LEVEL == 2` allocates a vector proxy inside `noexcept`); `Tree::OnMouseMove` invalidates on within-row sub-zone moves with identical visuals | Verified | `DxUi.Tree.cpp:2077-2122, 3611-3665` | P1.7 |
| E55 | `ScrollPanel::OnMouseWheel` repaints and swallows wheel at clamped boundaries; `EmbeddedHost::DispatchPointer` drops `_capturedControl` on first button-up during multi-button chords and ignores `MiddleUp`; `WindowHost::OnMouseMove` switches modality to `Pointer` even when client pos equals `_lastMousePos` | Verified | `DxUi.Controls.cpp:11820-11825`, `Embedded.cpp:953-990`, `DxUi.WindowHost.cpp:3556-3568` | P1.12 |
| E56 | `WindowHost::HandleDeviceLost` resets `_deviceLossRetryCount = 0` inside `EnsureDeviceResources` before `Present` succeeds (unbounded retry spin on persistent `DEVICE_REMOVED`); swap chain uses `DXGI_SCALING_STRETCH`; `Failure::Scope` calls `3 × std::set_terminate` per Win32 message; `UpdateTooltipHover` copies `_tooltipText` 5×, calls `SPI_GETMOUSEHOVERTIME` 2× and runs `IsControlInTree` DFS 6–8× per move; `AnimationDispatcher` destroys/recreates `HWND_MESSAGE` across idle and double-counts frame-1 `dt` | Verified | `DxUi.WindowHost.cpp:4408, 4446, 4579, 5240, 5857-6008`, [Failure.cpp](../../../src/Foundation/Failure.cpp) `:103-114, 303-312`, `AnimationDispatcher.h:308-448` | P1.12, P2.1 |
| E57 | `DxUi.Typography.h` acquires a global `std::mutex`, allocates a heap `wstring` key + `vector<DWRITE_FONT_AXIS_VALUE>`, calls `GetSystemFontCollection(..., TRUE)` (FontCache ALPC check on miss), and stores raw `IDWriteTextFormat*` pointers in a never-evicted static map across `IDWriteFactory` lifetimes | Verified | [DxUi.Typography.h](../../../src/Controls/DxUi.Typography.h) `:194-433` | P4.5 |
| E58 | `MakeThemePalette` runs 4 redundant Oklab perceptual ramp derivations per theme build; `Control::operator new` adds `+39 B` header padding and a `HeapSize` Win32 call on allocation/deallocation | Verified | `DxUi.Theme.cpp:1068-1215`, `ControlAllocation.cpp:25-62` | P4.5, P8.1 |
| E59 | `EmbeddedHost::UpdateAccessibility` runs a full 4-pass UIA rebuild on every mutation with **no** `providerRequested` or `AreUiaClientsListening()` gate; `WindowHost` latches `providerRequested = true` forever and calls `AccessibilityFlush` twice per dirty frame | Verified | `DxUi.Accessibility.cpp:6377, 13839, 13904, 14897-14942` | P1.6, P7.1 |
| E60 | `FindAssociatedLabelText` runs an unindexed `O(K·N)` DFS; `PopulateControlSnapshot`/`CollectSnapshotSubtree` run `45–60` `dynamic_cast`s per control; `AccessibilityNodeSnapshot` (`~600+ B`) copies entire `128 KB` editor buffers twice (`value` + `documentText`) and materializes `10,000` `Tree` row nodes eagerly | Verified | `DxUi.Accessibility.cpp:2349-2908, 4092-4150, 5187-5325` | P7.3 |
| E61 | `ITextRangeProvider` `StepWordBoundary` scans from index `0` (`O(offset)`, `O(N²)` walk) and calls `CaptureSnapshot()` twice per COM method; `RaiseWindowHostTextInputAutomationEvent` copies the full document `3×` per keystroke even with zero UIA clients | Verified | `DxUi.Accessibility.cpp:4604-4651, 8677-9250, 13616-13651` | P7.1, P7.3 |
| E62 | TSF `TextStoreACP` copies the entire document `std::wstring` **15–25 times** per CJK/Korean IME syllable and imports state twice on commit; clipped `GetTextExt` probes character bounds backwards to `0` (`O(M·N)`); `TextField` and `ComboBox` push uncoalesced full-doc undo snapshots on every intermediate IME stroke | Verified | [DxUi.TextStoreACP.cpp](../../../src/Controls/DxUi.TextStoreACP.cpp) `:124-168, 908-941`, `DxUi.NativeTextInput.cpp:1924-1958`, `DxUi.TextInput.cpp:3166`, `DxUi.ComboBox.cpp:1829` | P1.15 |
| E63 | `StepToNextTextElement` / `SnapCaretIndexToTextElementBoundary` scan graphemes from `0` (`O(N)`) on non-ASCII moves; `SelectSingleLineWordAt` splits UTF-16 astral surrogate pairs; embedded `Ctrl+C` calls `ReadSnapshot` twice; `OnCopy` allocates `_text.substr`; `RegisterClipboardFormatW(L"HTML Format")` is uncached | Verified | [TextInputServices.cpp](../../../src/Controls/TextInputServices.cpp) `:85-144`, `DxUi.SingleLineTextEditing.cpp:836-865`, `DxUi.NativeTextInput.cpp:893, 1083`, `DxUi.TextInput.cpp:2896`, [TextClipboard.cpp](../../../src/Controls/TextClipboard.cpp) `:133, 205` | P1.15 |
| E64 | `RichDocument::ReplaceText` allocates a fresh `Buffer` per 1-char insert (`bufferIndex` increments each keystroke), so `Piece::AppendCoalesced` **never coalesces** consecutive typing | Verified | [DxUi.RichDocument.cpp](../../../src/Controls/DxUi.RichDocument.cpp) `:283-291, 1000-1008` | P6.1 |
| E65 | `RichTextEditor` calls full-doc `document_.GetText()` on arrow/click/drag/IME; Rich `Backspace`/`Delete` walks all piece-tree leaves via `BuildRichSourceProjection`; `Ctrl+A` tessellates offscreen selection rects and offscreen highlight `HitTestTextRange` | Verified | `DxUi.RichTextEditor.cpp:2910, 3657, 3775-3871, 3806, 5065, 5920`, [DxUi.RichTextCommands.cpp](../../../src/Controls/DxUi.RichTextCommands.cpp) `:4238-4269` | P6.1 |
| E66 | `BuildSourceDisplayMap` allocates dense `displayToSource` + `sourceToDisplay` (`1.05–4.2 MiB` per Rich keystroke on 64 Ki–256 Ki docs) | Verified | [DxUi.RichSemantic.cpp](../../../src/Controls/DxUi.RichSemantic.cpp) `:146-212` | P6.1 |
| E67 | `CloseElement` stores display text 2–3×; prose edits re-parse unchanged math/Mermaid ASTs and spin up `libfyaml` DOM + `std::multimap` (`ValidateYamlMetadata`); `MarkdownInlines` allocates per-run vectors/strings | Verified | `DxUi.RichSemanticDocument.cpp:940, 1286, 1756, 2182`, [DxUi.MarkdownSyntax.cpp](../../../src/Controls/DxUi.MarkdownSyntax.cpp) `:417, 436-458, 520-533, 1079` | P6.1 |
| E68 | `MeasureTableOne` creates `2·R·C` uncached `PreparedLayout`s on every 1-char prose edit and resize; horizontal resize calls `state_->Clear()`, dropping width-independent markers/line-numbers/images and reflowing top-to-bottom instead of viewport-first | Verified | `DxUi.RichSemanticGeometry.cpp:1531-1625, 2445` | P6.1 |
| E69 | `BuildChunkLayout` builds a throwaway `Phase::Reference` `IDWriteTextLayout` even for uniform-font chunks; chunks lack `translatedOrigin` delta rebase; multiline selection invalidates full viewport; `DrawSourceInkIfSupported` calls `layout.Draw` twice per chunk | Verified | [DxUi.RichDocumentLayout.cpp](../../../src/Controls/DxUi.RichDocumentLayout.cpp) `:338-344, 445`, `DxUi.RichTextEditor.cpp:5397`, [DxUi.RichTextSourceInkRenderer.h](../../../src/Controls/DxUi.RichTextSourceInkRenderer.h) `:411, 434` | P6.2 |
| E70 | `MathFontData::Build` calls `3 × ~7,500` Cambria Math glyph methods and is discarded whenever `SemanticLayout` rebuilds; `BuildTextAtom` creates a full `IDWriteTextLayout` per single-character math variable/digit/operator (`~60–120`/formula) | Verified | `DxUi.PreparedMath.h:421-471, 950-1005, 3117` | P6.5 |
| E71 | `PreparedFlowchart::Draw` emits 5 `DrawLine`s per edge when 4/5 segments are zero-length and `Preparation` reserves `>120 KiB` fixed arrays; `RasterizeSvg` renders every SVG twice; `PopulateRichTextEditorAccessibility` copies full doc and transforms every block/link/cell per cursor/scroll move | Verified | `DxUi.PreparedFlowchart.cpp:910-917`, `DxUi.RichSemanticGeometry.cpp:508-538`, `DxUi.Accessibility.cpp:2423, 2559` | P1.9, P7.2 |
| E72 | `ControlHost::TrimCaches` clears `_configuredTextFormats` **and** sets `_forceAllVisualPreparation = true` whenever `_configuredTextFormats.size() > 96`, forcing 100% full-tree visual re-preparation every frame on dense views (`>= 97` format combinations) | Verified | `DxUi.WindowHost.cpp:4678-4682` | P4.5 |
| E73 | `DCompositionCreateDevice2(dxgiDevice.get(), ...)` couples `_dcompDevice` to the D3D11 device so TDR tears down `_dcompTarget`/`_dcompVisual`, and `ControlHost::OnSize` only calls `Invalidate()` so rapid modal `WM_ENTERSIZEMOVE`..`WM_EXITSIZEMOVE` mouse traffic starves `WM_PAINT` | Verified | `DxUi.WindowHost.cpp:835, 4660-4662, 5291-5333` | P1.12 |
| E74 | `FrameClock::SmoothDeltaUs` clamps only `> hitchClampUs` with zero vblank jitter filtering (feeding raw `13 ms / 20 ms` DXGI/DWM variance into motion/scroll integrators), and `DxUi.Layout.cpp` constructs nested per-child `Failure::Scope`s (`6N–9N` `std::set_terminate` calls/pass) | Verified | [FrameRuntime.cpp](../../../src/Foundation/FrameRuntime.cpp) `:125-135`, `DxUi.Layout.cpp:183, 245, 535, 624, 973` | P1.8, P1.12, P2.7 |
| E75 | `RichImageDecoder::Decode` copies up to `32 MiB` of `encoded` into `std::vector<BYTE> owned` before `IWICStream::InitializeFromMemory` even though `CopyPixels` finishes synchronously before `Decode` returns, and broken image sources lack a failed-decode retry embargo | Verified | `DxUi.RichImage.cpp:250-256, 292-306` | P1.13, B1 |

## What DxUi borrows, and what it does not

| Mechanism (source) | Transfer | DxUi item |
| --- | --- | --- |
| Lockstep incremental-versus-fresh verifier (`gpui-fast` `tests/oracle.rs`, `layers/verify.rs`) | As-is | P0.3 |
| Per-thread allocation counters, debug stall warnings & `wall_ms - cpu_ms` driver-stall split (`rune` `procwatch`, `tooling/perf_warn.rs`, `research/38`) | Adapted to test-only `operator new` hooks plus deterministic counters | P0.1 |
| Deadline wakes run without view, layout or GPU work unless visible state changed (`rune` `motion/timer_wake.rs`, `BUI` `kit_custom.c`); covered/minimized timer latch (`rune` `#324`) | Adapted | P2.1 |
| Multi-HWND `16 ms` `WM_PAINT` starvation guard (`rune` `#392`) and unified `Backoff` reset only on `Presented`/`Uncovered` (`rune` `627c2b3e`, `#492`) | Adapted | P1.12, P2.1, P2.7 |
| Hover held still for 100 ms after wheel under stationary pointer, immediate `release_unhovered` on leave, draw-only typing suppression (`gpui-fast` `#124`, `#105`, `hover_freeze.rs`) | As-is | P2.5 |
| 5-state tooltip machine (`Idle`, `Armed`, `Open`, `Grace`, `Suppressed`, `rune` `#383`) | Adapted | P2.3 |
| Unseen motion lands instantly (`rune` `87caf2a`, `motion/unseen_presence.rs`); loop phase kept across hiding (`rune` `motion/loop_clock.rs`) | As-is | P2.6 |
| Sample animations at predicted presentation time (`rune` `research/15` §1); guard `< 1 ms` `qpcRefreshPeriod` and 3-wake sleep fallback (`gpui-fast` `vsync.rs:62-79`); focused-only VRR `Steady` (`rune` `#497`) | Adapted to waitable swap chain plus `DwmGetCompositionTimingInfo` | P2.7 |
| Zero-poll TDR device-loss event (`gpui-fast` `device_wake.rs` `ID3D11Device4::RegisterDeviceRemovedEvent`) and borrow-safe FIFO deferral of reentrant Win32 display/power messages (`gpui-fast` `#125`) | Adapted | P1.12 |
| Boundary-clamped wheel no-op (`gpui-fast` `#104`), multi-button chord capture refcount & stationary `WM_MOUSEMOVE` filter (`gpui-fast` `#130`, `events.rs:369`), drag-out press disarm (`rune` `0ea75abe`) | As-is | P1.12 |
| Unchanged-value setter guards & old+new bounds dirty union (`BUI` `3ec5a5e`), premultiplied `(0,0,0,0)` dirty-rect clear (`BUI` `5cea7a6`), and central detach/close pointer scrubbing (`BUI` `9080f51`) | Clean-room architectural invariants | P1.14, P2.1, P3.1, P3.2 |
| Two-generation color-independent shaped-run cache (`gpui-fast` `text.rs` `RecentShapes`, 16 Ki glyphs/generation) + single `heavy` outlier slot (`gpui-fast` `path_cache.rs`) | Adapted: capture `DWRITE_GLYPH_RUN` via private `IDWriteTextRenderer`, replay via `DrawGlyphRun` | P4.1, P4.5 |
| Grapheme-safe shaped-width truncation (`gpui-fast` `#132` `truncation.rs`) | Adapted via DirectWrite cluster hit-testing | P1.14 |
| Width-independent shaping, re-breaking in place on resize (`rune` `research/28`: `16.1 → 4.6 ms`) and `hold_landing_width` during collapse (`rune` `#248`) | Adapted: `IDWriteTextLayout::SetMaxWidth` in place | P4.3, P5.3 |
| Context-free numeric shaping table (`gpui-fast` `number_shaping.rs`: `0123456789.,+-%$KMBT`, no `:`, no space) | Redesigned: tabular figures, all 15×15 pairs **proved once per font** | P4.4 |
| Binary-searched visible lines (`rune` `text/glyph_emit.rs`: 200 KB note `5.45 → 0.09 ms`) | As-is over cached line tops | P1.9 |
| Byte-cut fence coloring keeping complete prefix lines (`rune-kit` `syntax/scan.rs`) and `LRI..PDI` inline-code BiDi isolates (`rune` `#496`) | As-is | P1.10, P6.5 |
| Viewport-first resize reflow + `[0.25, 4.0]` sample height rescaling (`gpui-fast` `#129` `lists.rs`: `3.78 → 0.18 ms`) | Adapted for semantic blocks & variable-height lists | P6.1 |
| First-character undo boundary (`rune` `#385`) and fenced secret-clipboard auto-clear (`rune` `secret.rs` `CLIPBOARD_SEQUENCE` + `ZeroMemory`) | Adapted | P1.15, P6.1 |
| Cold rare-field sidecar with static empty instance (`rune` `cold.rs`) | As-is | P8.1 |
| Clickable-span hover filter (`gpui-fast` `interactive_text.rs`) | As-is | P6.4 |
| A11y scroll carry translating cached bounds (`gpui-fast` `a11y.rs`) | Adapted | P7.2 |
| Lazy tab-stop order (`gpui-fast` `tab_stop.rs`, `6b9c204`, `fb8cf94`) | Already lazy on `VK_TAB` in `WindowHost`; verify embedded/modal | P7.5 |
| Opaque fill inset by half stroke under opaque border (`rune` `paint/border.rs` `fill_under`) & `PushAxisAlignedClip` when content clears rounded corners (`rune` `17abbcb`: 512 cards `~82 → 3 ms`) | Adapted | V3 |
| Closed-form spring with velocity-preserving retarget; effect channels never overshoot (`rune` `spring.rs`, `research/15` §7) | As-is | V1 |
| Separable 1D-`erf` × 4-step-Y analytic Gaussian rounded-rect shadow (`gpui-fast` `shaders.hlsl:465-512`) | Adapted as custom D2D effect | V4 |
| Scroll edge fades (`rune` `ScrollFade`), analytic fling (`rune` `research/15` §8`), `1/64` px reversal hold & `double` tall-scroll delta (`rune` `#401`, `b45bfd66`), DirectManipulation (`gpui-fast` `direct_manipulation.rs`) | Adapted | V6 |
| Text glide dead-zone: snap Y to whole px, snap X within `1/64` px of rest (`rune` `e1bd315`) | Adapted | V8 |
| Sub-rect `UpdateSubresource` uploads (`gpui-fast` `#131`) & scroll-layer promotion/demotion (`gpui-fast` `layers/policy.rs`, `REPAINT_COST = 2`) | Sub-rect upload in `P1.14`; scroll strip cache **only if** D4 triggers | P1.14, D4 |
| Unspent waitable-credit deadlock guard (`_frameLatencyCreditHeld`), `32`-handle `DCompositionWaitForCompositorClock` limit & `0xC01E0006` occluded sleep (`makepad` `os/windows/win32/mod.rs:8-11, 194-195, 500-505`) | As-is: never re-wait on latency-1 waitable object when a woken tick skipped `Present` | P2.7 |
| `DCompositionCreateDevice2(nullptr)` TDR-surviving HWND target + `CheckDeviceState` & modal `WM_ENTERSIZEMOVE`..`WM_EXITSIZEMOVE` coalesced render (`makepad` `dcomp.rs:60-68, 224-228`, `win32_window.rs:766-925`) | As-is | P1.12 |
| `0.15` EMA vblank `dt` filter (`[0.5, 1.5] * dt_ema`) + `LagSmoothing` (`> 500 ms -> 33 ms`) (`makepad` `scroll_motion.rs:702-741`, `tween/src/ticker.rs:14-45`) | As-is | P2.7, V1, V6 |
| `NextFrameGate` 2-bit hidden-subtree settle + single re-show sync & `500 µs`/`200 µs` cooperative prep/retire slices (`makepad` `next_frame_gate.rs:1-88`, `async_labels.rs:252-318`, `draw_list.rs:942`) | Adapted | P2.1, P2.6 |
| Working-set frame-generation cache protection + `"Source Library wall"` producer epoch invalidation & DXGI 1.4 hysteretic VRAM trim (`makepad` `layouter.rs:392-401, 514-531`, `slug_atlas.rs:101-125`, `gpu_pressure.rs:14-181`) | Adapted | P4.1, P4.5, P6.1 |
| Beyond-viewport wheel backlog jump (`find_position` before row draw), Fenwick `HeightTree`, `DataGrid` sparse `AxisSizes` (`O(K)` memory, `O(log K)` hit) & ink-only `ReorderList` slide (`makepad` `portal_list.rs:360-726, 2508-2526`, `data_grid.rs:124-258`, `reorder_list.rs:164-307`) | Adapted | P1.7, P6.1 |
| 1-pass post-loop `StackPanel` shift, active-flex index list & `ConcedingRow` priority toolbar overflow with `6 DIP` hysteresis (`makepad` `turtle.rs:3683-3742, 4885-5086`, `conceding_row.rs:222-335`) | Adapted | P1.8, P5.4 |
| Per-line `CodeLexState` `(start_state, end_state)` lexical continuation & pixel-row deduplicated scrollbar marks (`makepad` `code_editor/src/tokenizer.rs:66-227`, `scroll_marks.rs:189-248`) | Adapted | P1.10, P6.2 |
| Optical cap-height `ink_centered` offset, `letter_spacing` `liga`/`calt` suppression, `< U+0590` fast LTR Bidi check & lazy per-glyph `MathFontTable` (`makepad` `draw_text.rs:314-327`, `shaper.rs:38-62, 220-236`, `latex_math/src/layout.rs:395-473`) | Adapted | P1.9, P4.1, P6.5 |
| `box_all` negative interior SDF, `smoothstep(-3*px, 0, d)` shadow reach, `+ 1e20` callout union, shared disjoint-pane blur pyramid + `r²` variance lerp & `MIN_PX = 0.7` ink-conserving hairlines (`makepad` `sdf.rs:436-616`, `surface.rs:101-261`, `backdrop.rs:123-131`, `gauss_chain.rs:518-526`, `ink.rs:17-46`) | Adapted | P1.9, V3, V4, V5, B2 |
| `64 B` cache-line `Hot` tween record + `QuickTo` `Retime::ByDistance`, invertible rubber-band `stretch_displayed`/`stretch_raw` & `lsq2_release_velocity` (`makepad` `tween/src/{engine.rs:89-117, quick.rs:51-150}`, `scroll_motion.rs:233-338, 569-615`) | Adapted | V1, V6 |
| `15 s` failed-image retry embargo + alpha-weighted mip generation, `1×1` `ClipCursor` + `WM_INPUT` infinite scrubbing + direction-splitting select/scrub, and `RadialMenu` per-sector AABB + resting-radius hit-test (`makepad` `image_tiles/src/store.rs:85-112`, `image_cache.rs:76-134`, `rawinput.rs:132-222`, `number_field.rs:584-631`, `radial_menu.rs:464-704`) | Adapted | P1.13, B1, B3, B5 |
| Per-frame `churn = min(creates, destroys)` resource counter & gallery unlabeled-control UIA lint (`makepad` `ledger.rs:118-241`, `a11y/src/render.rs:255-330`) | Adapted | P0.1 |

### External repository synthesis (11 November 2026 heads)

- **`longbridge/gpui-fast` at `af019de`**: Recent PRs `#104`–`#133` hardened the exact edges a retained D3D11 UI hits
  after basic caching lands: `#129` cut 100k-item resize reflow `21×` (`3.78 ms → 0.18 ms`) by reflowing the visible
  viewport first and clamping height-estimate ratios to `[0.25, 4.0]`; `#131` switched D3D11 atlas uploads from staging
  buffers to `ID3D11DeviceContext::UpdateSubresource` on `D3D11_USAGE_DEFAULT` sub-rects and collapsed scroll-tile
  invalidation keys to a single `u64` version; `#132` added grapheme-safe shaped-width truncation; `#124`/`#105` split
  pointer-vs-wheel hover freeze (`release_unhovered` still clears exited hover immediately, while typing hides hover at
  draw time without invalidating hit-test state); `#130` fixed multi-button chord capture and stationary `WM_MOUSEMOVE`
  events; `#104` eliminated boundary-clamped scroll repaints; `#125` queued reentrant Win32 power/display callbacks in a
  FIFO instead of panicking or mutating borrowed host state; and `lists.rs:78, 3129` (`LIST_LAYERS = true`) confirmed
  that virtualized lists should replay compact row-draw operations rather than caching GPU scroll tiles.
- **`anomalyco/rune` at `0ea75abe` (`research/01..38`, `gpu-floor`, `engine-spike`)**:
  - *Empirical baselines and falsified ideas*: `gpu-floor` established a `< 90 µs/frame` native DirectX submission floor
    when per-frame resource creation is eliminated; `research/28` measured `3.5×` faster resize reflow (`16.1 → 4.6 ms`)
    from width-independent paragraph shaping; `research/31` proved that deferring retained-tree invalidation onto timer
    phases catastrophically regressed rapid edits `220×` (`0.23 → 50.9 ms`, never borrow); `research/32` proved per-word
    shaping caches yield only `1.46–1.57×` while breaking kerning and line breaks (never borrow); `research/37` showed
    heap allocation was `2–11%` of fresh-build busy time while `clear_old_nodes` was `< 1%` (justifying `P8` inline size
    reductions and cold sidecars without a custom node bump arena); and `research/38` separated `wall_ms - cpu_ms` to
    catch driver/Present stalls.
  - *State machines and rendering*: `#371`–`0ea75abe` replaced ad-hoc booleans with table-tested `#[derive(Machine)]`
    automata (`Tip` `#383`, `ScrollMode` `#401`, unified `Backoff` `627c2b3e`, first-char undo `#385`, fenced 45 s
    secret clipboard `be24e577`..`14e3c5d5`, drag-out disarm `0ea75abe`), rescued multi-window `WM_PAINT` starvation at
    `16 ms` (`#392`), latched expired timers while covered (`#324`), reset surface retries only after `Present` succeeds
    (`#492`), added `3×1` ClearType-to-A8 grayscale averaging (`#472`) and hue-preserving dual-source LCD blending
    (`#491`), switched to `Windfoil` analytical glyph outlines only during active scale motion (`#390`/`#425`/`#482`),
    isolated inline code in RTL paragraphs with `LRI..PDI` (`#496`), and documented Win32 `ISpellChecker` TLS COM
    teardown deadlocks (`#256`: never hold COM spellcheck objects in thread-local destructors).
- **`makepad/makepad` at `506ec051` (MIT; full audit in [Makepad study](#makepad-study-makepadmakepad-at-506ec051))**:
  - *Win32/DXGI/DComp & cache survival*: uncovered the **latency-1 waitable swap-chain unspent-credit deadlock** when a
    woken tick skips `Present` (`win32/mod.rs:8-11, 500-505`), decoupled `IDCompositionDesktopDevice` from `IDXGIDevice`
    (`DCompositionCreateDevice2(nullptr)`) so HWND visual trees survive D3D11 TDR (`dcomp.rs:60-68`), coalesced modal
    `WM_SIZING` paints (`win32_window.rs:766-925`), replaced `0.1–1.5 ms` byte hashing with pointer-epoch upload identity
    (`d3d11.rs:144-169`), and protected current/prev-frame cache entries from overflow cliffs (`layouter.rs:514-531`,
    exposing `DxUi`'s `E72` `_forceAllVisualPreparation = true` storm).
  - *Virtualization, shaders, math & empirical `Slug` limits*: contributed beyond-viewport wheel backlog jumps (`0`
    intermediate offscreen row instantiations on `8,000 px` flicks, `portal_list.rs:2508-2526`), `DataGrid` `O(K)`
    `AxisSizes` (`data_grid.rs:124-258`), `ConcedingRow` `6 DIP`-hysteresis toolbar overflow (`conceding_row.rs`),
    per-line `CodeLexState` continuation (`tokenizer.rs`), pixel-row deduplicated `ScrollMarks` (`scroll_marks.rs`), lazy
    per-glyph `MathFontTable` (`latex_math/src/layout.rs:395-473`), shared disjoint-pane backdrop blur pyramids + `r²`
    variance interpolation (`backdrop.rs`, `gauss_chain.rs`), `1×1` `ClipCursor` + `WM_INPUT` infinite scrubbing
    (`rawinput.rs`), tight per-sector `RadialMenu` AABBs (`radial_menu.rs`), and crucial production evidence that `Slug`
    GPU curve text hit shader-compiler banding bugs (`band_count = 0` in `slug_atlas.rs:334-336`) and `3.5 s` unroll
    compile stalls (`draw_text.rs:197-205`).

**Not borrowed**, with reasons:

- **Closure replay logs** (`gpui-fast` `retained.rs`/`splice.rs`): `DxUi` already retains its control tree.
- **GPU tiles for virtual lists**: `gpui-fast` `docs/scroll-layers.md` and `lists.rs:78` reject them in favor of
  tile-less row replay.
- **Time-phased retainer** (`rune` `research/31`): regressed rapid edits `0.23 → 50.9 ms` (`220×`).
- **4-band shadow clip split** (`rune` `paint/bands.rs`): with D2D effects it multiplies effect passes by four.
- **Background `SetWinEventHook` + `EnumWindows` GDI occlusion thread** (`rune` `#570`, `platform/win32/occlusion.rs`):
  [performance and resources](../../Core/Core_PerformanceAndResources.md) explicitly forbids inferring arbitrary window
  coverage via polling or global WinEvent hooks; `SetNativePresentationVisible` stays application-supplied.
- **Expanding `Grid` text-layout retention beyond the last-painted cells**: `EndTextLayoutPaint` in
  [DxUi.Internal.h](../../../src/Controls/DxUi.Internal.h) `:371-382` enforces the accepted memory bound in
  [performance and resources](../../Core/Core_PerformanceAndResources.md) (`Grid` retains at most one text layout per
  cell drawn by the last paint).
- **rune's `KeyHasher` constants**: any good 64-bit hash works, and a cache must compare the full key exactly.
- **Per-word shaping cache**: `rune` `research/32` rejected it at `1.46–1.57×` while breaking kerning and line breaks.
- **Bump arena for retained nodes**: `rune` `research/37` showed node teardown under `1%`; `P8.1` cold sidecars and
  inline struct shrinking capture the `2–11%` allocator share safely under WIL/RAII.
- **`em_metrics.rs`**: DirectWrite font fallback already covers it.
- **`ChangedBounds`/`BoundsTree`**: a draw-order structure `DxUi` does not have.
- **`CompositeBatch`/`draw_state.rs`**: embedded composite is already `0.0104 ms` p95
  ([Default-B1](../../../Measurements/EmbeddedInteractionLayout/Default-B1.json)); revisit only if a multi-view
  consumer measures otherwise.
- **HDR/advanced color**, **continuous `opsz` axis values**, **replacing the WinUI double focus ring**, and **retired
  Fluent reveal/noise textures**: outside product design or already handled by DWM/Segoe UI Variable instances.
- **Runtime `D3DCompile`, Immediate-mode `Turtle` re-walks, `Vec<String>` monospace code buffers, unbanded `Slug`
  (`band_count = 0`) for 1:1 UI chrome, and in-process hang-polling threads** (`makepad`): rejected for the reasons
  recorded in [Makepad study](#makepad-study-makepadmakepad-at-506ec051).

## Definition of done for every item

1. **Fail-before counter.** The item names its `P0.2` scenario and counter. The baseline value is recorded before
   the code change. The candidate meets the stated bound on the same fixture and build profile. Counter bounds
   are exact or upper bounds and live in the test, so a regression fails the build. Timing (`cpu_ms` and `wall_ms`) is
   reported as advisory evidence until D-P1 qualifies its policy.
2. **Parity.** Any item that adds retention, culling or partial painting passes the `P0.3` incremental-versus-fresh
   oracle with **0** BGRA differences on WARP (including premultiplied alpha channels). It also keeps bit-identical
   bounds and an identical UIA snapshot for its scripts.
3. **Visual changes are separate.** A change meant to alter pixels (`V`-items, `P4.1` replay snapping, `P4.6`, `B2`) is
   reviewed through `gallery.ps1 -PublishDocs` harness captures, with scenario/build/DPI provenance and a
   before/after carousel. A design-system update is required. `P0.3` parity never stands in for this review.
4. **Resource and failure-policy rules.** Steady state allocates nothing, composition does no heap/layout/raster/blocking
   work, idle blocks on events, caches trim only at frame or preparation start, and every `noexcept` boundary preserves
   [failure handling](../../Core/Core_FailureHandling.md) (including MSVC Debug `_ITERATOR_DEBUG_LEVEL == 2` container
   proxy allocations). New caches report their size in `EmbeddedStatistics` or the diagnostics sink.
5. **Repository workflow.** One focused branch per item group, a `Changes/` fragment, `Test-Changes.ps1 -Explain`
   plus affected suites while editing, and `-Mode PrePush` before push. Normative text moves into the owning
   contract when the item closes. Behavior or API changes update `docs/` and `capabilities.json`. Focus-taking
   suites run only with the developer's agreement to the time.
6. **Platform.** No unconditional import beyond the Windows 10 embedded baseline. Windows 11-only facilities are
   detected at run time with a tested fallback.

## Decisions

Confirmed on 7 November 2026 and retained:

- [x] **D1.** This plan stays separate from [remaining work](RemainingWork_2026-11-04.md) and is sequenced after
  D9's history rewrite on shared files.
- [x] **D2.** A popup keeps publishing every row to UI Automation. The draft's "6.10 ms/step" was unsubstantiated,
  so `P7.4` runs only if `P0.2` (`S10`) measures a real cost.
- [x] **D4.** A non-virtual `ScrollPanel` bitmap strip cache waits for measurement after `P3`/`P4`/`P2.5`. If it is
  built, it uses gpui-fast's promotion/demotion policy and `REPAINT_COST` work accounting, a 16 MiB cap inside the
  64 MiB per-view cap, and only row-band `UpdateSubresource` uploads.

Proposed (revised 11 November 2026); each needs the developer's confirmation before its phase starts:

- [ ] **D3 (revised). API revision 6, not 5; one bundled break.** Revision 5 already exists. Bundle every public
  break into a single revision 6 so consumers migrate once:
  1. The embedded animation request replaces `NeedsAnimation()`/`AdvanceAnimation()` (`P2.4`). This is a
     deliberate compile break: a silent change in meaning would stop carets blinking in unported consumers.
  2. `Control::GetVisualOverhangDip()` and region invalidation (`P3.1`).
  3. Zero-copy `IGridModel::GetColumn` only if the `P8.3` hoist leaves allocations.
  4. **Withdrawn:** the 64-slot slab `ControlLifetimeHandle`. It would touch 460 call sites and every consumer to
     save one small allocation per control; `P8.2` keeps `std::weak_ptr<int>` while `P1.8` eliminates its per-paint
     atomic lock traffic.
  The `Control` and `ToggleableControlBase` layout changes in `P8.1` need no revision bump beyond rebuilding pinned
  consumers, provided source compatibility of `Checkbox`/`Toggle`/`RadioButton` public methods is preserved (or bundle
  into revision 6 if any consumer downcasts them to `Button*`).
- [ ] **D5. Native frame pacer.** `WindowHost` continuous animation is paced by a DXGI frame-latency waitable
  object (`MaximumFrameLatency = 1`), guarded by `_frameLatencyCreditHeld` so a woken tick that skips `Present` never
  re-waits on an unspent semaphore credit (`makepad` `win32/mod.rs:8-11, 500-505`). One pacer thread per UI thread
  blocks on `{waitable, arm/stop event}` and posts one frame message only while a continuous animation is armed. When
  disarmed it blocks indefinitely, so this is event-driven with no polling. Fallback where unavailable, occluded
  (`0xC01E0006`) or when `< 500 µs`–`1 ms` wakes indicate monitor sleep is a one-shot timer at the display refresh
  period. Embedded hosts are unchanged and stay owner-scheduled.
- [ ] **D6. Visual direction.** Adopt springs (`V1`), OKLab color transitions (`V2`), device-pixel hairlines (`V3`),
  analytic elevation shadows (`V4`) and smooth fractional scrolling (`V6`) as design-system changes, with gallery
  review per item. Reduced motion and High Contrast keep their current semantics.
- [ ] **D7. Popup materials.** Retire the GDI `CAPTUREBLT` screen capture. HWND popups use the DWM transient
  system backdrop where available. In-window and embedded overlays blur `DxUi`'s own retained surface. Everything
  else falls back to the solid themed fill. This also stops reading other applications' pixels.
- [ ] **D8. Text rendering params.** Pick antialias mode by presentation mode: grayscale on premultiplied or
  composition targets, the system setting on opaque HWNDs. Use per-monitor rendering params. Any change to dark-theme
  grayscale contrast requires harness A/B captures.
- [ ] **D9. Order.** `P0`, then `P1`, are mandatory first. `P2` through `P8` and `V1` through `V8` follow the
  [dependency graph](#sequencing). `V`-items may run in parallel with `P`-items when they touch different files.
- [ ] **D10. Clean-room BUI additions.** Adopt visibility-latched animated image clocks, two-buffer ping-pong and
  hysteresis-bounded target-size WIC decode scaling (`B1`), per-corner radius masks and 1-DIP analytical bevel/emboss
  rims for joined controls (`B2`), and evaluate optional Blender-style numeric scrubbing and menu typeahead (`B3`),
  zoomed curve text (`B4`), 8-slot radial `PieMenu` (`B5`), and `2 DIP` segment-swept multi-control boolean drag-toggle
  (`B6`). Zero GPL-2.0/3.0 code, shader source, comments or tables may be copied from `BUI`.
- [ ] **D11. MIT-licensed Makepad additions.** Fold Makepad's verified Win32/DXGI/cache/virtualization/text/math fixes
  directly into `P0`–`P8` (`_frameLatencyCreditHeld` deadlock guard, `DCompositionCreateDevice2(nullptr)` TDR survival,
  modal `WM_SIZING` synchronous coalesce, `FrameClock::SmoothDeltaUs` EMA jitter filter, working-set-protected
  `TrimCaches` without `_forceAllVisualPreparation = true`, zero-copy `RichImageDecoder::Decode` span stream + `15 s`
  failure embargo, `AxisSizes` / `HeightTree` / beyond-viewport wheel jump, per-line `CodeLexState` continuation,
  pixel-row `ScrollMarks`, lazy `MathFontTable`, and hysteretic DXGI 1.4 VRAM trim), and evaluate opt-in optical
  cap-height label centering (`P4.1`), priority-ranked `ConcedingRow` toolbar overflow (`P5.4`), `1×1` `ClipCursor` +
  `WM_INPUT` infinite scrubbing + direction-splitting select/scrub (`B3`), driver-banded HLSL curve-text qualification
  (`B4`), and tight per-sector `PieMenu` AABBs + resting-radius hit-testing (`B5`).

## P0 — Measurement foundation (blocks every other phase)

- [ ] **P0.1 Deterministic work counters.**
  - **Change**: Add a private `Detail::WorkCounters` thread-local record of plain increments, dormant until a test sink
    reads it. Route creation through counted helpers in [DxUi.Internal.h](../../../src/Controls/DxUi.Internal.h):
    `CreateTextLayout`, implicit `DrawTextW` layouts, `CreateEffect`, `CreateCommandList`, `CreateLayer`, gradient and
    solid brushes, path/rounded-rect geometries, `CreateBitmap*`, GDI captures, `set_terminate` calls, `Typography`
    format misses, and UIA snapshot builds (with pass counts, node counts and copied text bytes). Track per-frame
    resource `churn = min(creates, destroys)` (`makepad` `ledger.rs:118-241`) so same-frame create+destroy pairs fail
    steady-state gates even when net resident count is `0`, and add a deterministic gallery/test UIA lint asserting every
    interactive control exposes a non-empty accessible name (`makepad` `libs/a11y/src/render.rs:255-330`). Also count tree
    visits in `Tick`/`Paint`/`HitTest`, timer and pacer wakes, `Prepare`/`Render`/`Present`, pixels painted, and record
    advisory `cpu_ms` versus `wall_ms` (`wall_ms - cpu_ms` driver/Present stall metric from `rune` `research/38`).
  - **Allocation counting**: test executables replace `operator new`/`delete` to count per-thread allocations and bytes
    inside a `ScopedAllocationCounter`. The shipped library stays unhooked.
  - **Enforcement**: a tooling test (`Tools/tests`) fails if `->CreateTextLayout(`, `->CreateEffect(`,
    `->DrawTextW(`, `->CreatePathGeometry(`, `->CreateRoundedRectangleGeometry(`, `->CreateLayer(`, `->CreateBitmap(`
    or `->CreateLinearGradientBrush(` appears outside the counted helpers.
  - **Gate**: overhead under 1% on ComplexUi clean frames. Clean frames stay at **0** allocations.
- [ ] **P0.2 Scenario fixtures and baseline receipt.** Add `Embedded.Tests.WorkCounterScenarios.h` and
  `Controls.Tests.WorkCounterScenarios.cpp`. Record every counter for `S1`–`S20` in
  `Measurements/RuntimeEfficiency_2026-11-11/baseline-counters.json` with source commit and build profile:
  - `S1`: focused idle single-line `TextField` (editable and `_readOnly`) for 10 s.
  - `S2`: tooltip shown, hovered and dismissed across 6 s.
  - `S3`: 500-item `ComboBox` open (60 hover moves) and closed (20 typed characters).
  - `S4`: popup shadow/blur/backdrop steady frame for `ComboBox`, `MenuPopup` (`Acrylic` and `Solid`) and transient flyouts.
  - `S5`: 2,048-row `Grid`, 100 wheel notches, 60 fractional touchpad deltas, and 30 column-resize drag steps.
  - `S6`: 10,000-row `Tree` (including expand/collapse animation) and grouped `Grid`, 200 arrow/page keys.
  - `S7`: `Slider`/`ProgressBar`/`NumericStepper`, 100 value steps with 1 UIA client listening.
  - `S8`: `Splitter` dragged 200 px.
  - `S9`: semantic document with 3 math blocks, 2 Mermaid diagrams, 2 images and 1 off-viewport animated GIF; 50 prose keystrokes.
  - `S10`: 4,096-row `MenuPopup`, 100 keyboard steps and open/close cycle.
  - `S11`: 2,000-line fenced code block in semantic view and 20,000-line Source view with `RichSourceGuides`, scrolled.
  - `S12`: the existing ComplexUi and the 64 Ki-unit Source/Rich typing benchmark.
  - `S13`: swap-chain and semantic horizontal resize sweep, 30 sizes.
  - `S14`: wrapped-label resize, 30 widths.
  - `S15`: `ThroughputGraph` (180 hue-weighted samples) + 4-segment elided `StatusStrip` + `PageHost` slide, 60 frames.
  - `S16`: `ColorPicker` HSV drag (100 pointer moves) + `NumericStepper` unchanged/changed setter sweep.
  - `S17`: `EmbeddedHost` 100 mutations with **0** UIA clients vs **1** UIA client (`ITextRangeProvider` word walk on 64 Ki doc).
  - `S18`: TSF IME 10-syllable CJK composition + commit + non-ASCII grapheme arrow walk + rich `Ctrl+C` on 64 Ki `TextField` and `RichTextEditor`.
  - `S19`: 64 Ki Rich-mode typing (100 1-char inserts) + arrow/click/Backspace + YAML frontmatter + `200×5` semantic table + prose edit outside table.
  - `S20`: semantic document with 8 multi-atom `PreparedMath` formulas, 4 `PreparedFlowchart` diagrams and 2 inline SVGs across prose edits.
- [ ] **P0.3 Incremental-versus-fresh oracle.**
  - **Change**: Add `Controls.Tests.IncrementalParity.cpp` and `Embedded.Tests.IncrementalParity.h`. Seeded mutation
    scripts cover text, bounds, visibility, hover/focus/press, chord capture, scroll, theme, DPI 96/144/192, LTR/RTL,
    caret ticks, popup open/close, control detach during active hover/timers, and rich edits. Each step renders the
    incremental host and a reference rendered after `ControlHost::DebugDropRetainedStateForTest()` clears every cache
    and invalidates fully.
  - **Comparison**: exact 32-bit premultiplied BGRA output on WARP, all bounds and UIA snapshot records. Failures
    retain seed and step.
- [ ] **P0.4 Swap-chain content truth test.**
  - **Change**: Add a WARP test in [DxUi.Tests.WindowHost.cpp](../../../Tests/Controls/DxUi.Tests.WindowHost.cpp)
    presenting a full frame, then partial A, then partial B, reading back buffer 0 before the next draw to prove
    whether non-dirty pixels come from N−1 or N−2.
  - **Outcome**: Record the result in [Win32 host](../../Rendering/Rendering_Win32Host.md). If it is N−2, today's
    `WM_PAINT` partial path is a latent stale-pixel bug and `P3.3`'s history-union design is mandatory; fix `WM_PAINT`
    immediately in `P1.12`.

## P1 — Verified hotspots and open review IDs (independent, low risk, one PR per bullet group)

- [ ] **P1.1 `ComboBox` popup layout (`E12`).**
  - **Change**:
    - Cache widest item width keyed by `{itemsRevision, filterText, typography/DPI/themeRevision, fontGeneration}`.
    - Dirty-flag `PopupLayout` instead of recomputing on `WM_MOUSEMOVE` hover changes or on keystrokes while `_open == false`.
    - Compute `PopupLayout` at most once per open paint and derive row text rects arithmetically from the row rect.
  - **Files**: [DxUi.ComboBox.cpp](../../../src/Controls/DxUi.ComboBox.cpp).
  - **Gate (`S3`)**: width-measurement layouts per open hover frame drop from `8,500+` to **0**; layouts per typed
    character while closed are **0**; total open paint layouts stay at most `V` until `P4.1` makes them **0**.
- [ ] **P1.2 Retained popup, backdrop and shadow resources (`E13`, `E14`, `E15`, `E52`; Phase A of `V4`).**
  - **Change**:
    - Retain per popup and per `PaintTransientSurfaceBackdrop` (`DxUi.cpp:152-201`) the blur effect, clip geometry and
      unit-space gradient brushes; pass `nullptr` to `PushLayer` for D2D-pooled layers; skip gradient creation entirely
      when `OverlayMaterial::Solid`.
    - Until `V5` retires `CaptureBackdropScreenRegion`, upload the temporary GDI capture with `D2D1_ALPHA_MODE_IGNORE`
      to delete the heap `std::vector<uint8_t>` and scalar alpha loop (`DxUi.cpp:139, 278-345`).
    - Replace per-call `DrawShadowEffectPass` objects with an 8-entry host `ShadowCache` keyed by
      `{radius, blur, color, quantizedDeviceSize}` holding a retained command list + `D2D1_PROPERTY_CACHED` shadow.
    - Tie `IWICImagingFactory` to COM-initialized host state (never a CRT `atexit` static), share/retain menu popup D3D
      resources across opens where compatible, avoid redundant `NativeMenuInterop` `HBITMAP` cloning on unchanged icons,
      remove `MenuPopup`'s duplicate initial `RenderPopup()` before `ShowWindow` (`4224, 4243`), and pre-parse
      `MenuItem` accelerator labels on mutation (`3224, 3244`).
  - **Files**: `DxUi.cpp`, `DxUi.Controls.cpp`, `DxUi.ComboBox.cpp`, `DxUi.Menu.cpp`, `NativeMenuInterop.cpp`.
  - **Gate (`S4`, `S10`)**: effect, command-list, layer, geometry and gradient creations per steady popup frame **0**;
    `ParseMenuLabel` allocations per paint **0**; popup open renders **1** frame, not 2; exact WARP pixel parity.
- [ ] **P1.3 `TextField`/`ComboBox` paint churn (`CTRL-13`, `E2`, `E16`).**
  - **Change**:
    - Copy or `SecureClear` cached display text only on a cache miss and only when masked (or clearing prior mask).
    - Add `GetDisplayTextView()` for unmasked fields and `HasNativeTextComposition(const Control*)` to skip composition
      state copies when idle.
    - Suppress caret blink registration and caret painting when `TextField::_readOnly == true` (`DxUi.TextInput.cpp:1707, 1789`).
    - Probe a stack array of 8 hit-test metrics before any heap fallback.
  - **Gate (`S1`)**: **0** heap allocations per focused paint frame; **0** caret wakes on `_readOnly` fields.
- [ ] **P1.4 Multiline `TextField` layout reuse (`E17`).** Route all 10 caret, selection, hit-test, scroll and wheel
  paths (`DxUi.TextInput.cpp:352..3679`) through `_cachedMultilineLayout` (with cached line metrics), and check
  `wheelStepCount == 0` before touching layout (`2180-2203`). **Gate**: focused multiline paint, sub-notch wheel and
  `PageDown` create **0** `IDWriteTextLayout`s when text/width are unchanged.
- [ ] **P1.5 `Splitter` preview accessibility (`E19`).** Publish only a bounds update once per frame during drag; run
  the structural refresh on commit. **Gate (`S8`)**: at most **1** UIA bounds update/frame and **1** structural build/drag.
- [ ] **P1.6 Value-only accessibility mutation and client-presence gating (`E20`, `E59`).**
  - **Change**:
    - Gate `EmbeddedHost::UpdateAccessibility` (`DxUi.Accessibility.cpp:14897`) on
      `providerRequested_ || AreUiaClientsListening()`, matching `WindowHost`'s lazy contract; deduplicate
      `WindowHost`'s double `AccessibilityFlush` per dirty frame (`6377, 13839`) and decay `providerRequested` when all
      issued COM providers release and `AreUiaClientsListening()` is false.
    - Add `AccessibilityMutationKind::Value` updating one node's range/text value in place and raising the property
      event for `ProgressBar`, `Slider` and `NumericStepper`.
  - **Gate (`S7`, `S17`)**: **0** UIA passes in `EmbeddedHost` with 0 clients; **0** structural snapshot builds per
    value step with 1 listening UIA client.
- [ ] **P1.7 Label/Button preparation, `Grid` within-contract hot paths, and `Tree` (`E21`, `E22`, `E44`, `E53`, `E54`).**
  - **Change**:
    - Do not set `NeedsVisualPreparation(true)` in `Label::Label` unless typography override or mnemonics are enabled;
      check cheap flags before `IsControlInTree` in both `Label` and `Button::PrepareVisualContent`.
    - `Grid` (preserving `EndTextLayoutPaint`'s last-paint cell layout cap): route `PaintBadgeCell` (`4977`) through
      `P4.1`, call `SetMaxWidth` in place on retained visible cell layouts during column drag-resize (`5042-5080`),
      cache `GridVisibleRowMap`, `ResolvedColumnLayout` and `CollectOrderedGroups` + sparse `O(K)` custom-extent
      `AxisSizes` (`O(log K)` group/custom-row offset & hit-test instead of `O(R)` dense prefix arrays, `makepad`
      `data_grid.rs:124-258`) per `(modelRevision, groupStateRevision, width)`, resolve beyond-viewport wheel deltas via
      prefix lookup *before* row instantiation (`makepad` `portal_list.rs:2508-2526`, instantiating **0** scrolled-past
      intermediate rows on multi-page flicks), accumulate fractional touchpad wheel remainders (`5891-5896`), and
      remove `O(R)` `GridSelectionModel` temporary vectors/sets (`609-669, 1041-1095`).
    - `Tree` & reorderable lists: single lookup in `SetFocusedItemId` (`892/901`), suppress within-row sub-zone
      `OnMouseMove` invalidation (`2077-2122`), slide displaced drag-reorder neighbors via ink-only Y offsets without
      relayout until drop (`makepad` `reorder_list.rs:164-307`), and replace `BeginTreeExpansionAnimation`'s `4,096`-row
      `TreeItemData` (`4×wstring`) double copy (`3611-3665`) with a pre-allocated slice of lightweight
      `{itemId, startY, targetY, depth}` records swapped in outside any MSVC `_ITERATOR_DEBUG_LEVEL == 2` `noexcept`
      vector-proxy hazard.
  - **Gate (`S5`, `S6`)**: **0** allocations per navigation key or steady `Grid` wheel frame; **0** cell layout
    recreations on `Grid` column resize for unchanged text; **0** `wstring` copies on `Tree` expand/collapse.
- [ ] **P1.8 `O(N)` panel/form/tab validation, scope hoisting and lock-free child traversal (`E23`, `E51`, `E74`).**
  - **Change**:
    - Check O(1) topology/interaction revisions inside loops and run one `O(N)` validation pass across
      `FlexPanel::BuildLayout`/`ApplyLayout`, `FormPanel::BuildLayout`/`ApplyLayout`/`AddField`, and
      `TabControl::UpdateVisiblePageBounds`. Reentrant mutation keeps its `E_ABORT` semantics.
    - Hoist `Failure::Scope` to outer host entry points instead of constructing nested per-child scopes inside
      `Control::Measure`, `Label::MeasureOverride`, `WrapPanel`, `StackPanel` and `FlexPanel` (`DxUi.Layout.cpp:183, 245,
      535, 624, 973`), maintain a compact active-flex index list during `FlexPanel` freeze iterations instead of
      re-scanning all children (`1053-1059`, `makepad` `turtle.rs:3683-3742`), and shift `StackPanel` `Center`/`End`
      children in one post-loop rect pass (`turtle.rs:4885-5086`).
    - In `Panel::Paint`/`PaintOverlay`/`Tick`, pin the panel itself once and check host topology revision between
      children instead of `4N` atomic `weak_ptr::lock` calls when topology is untouched (`77-127`).
  - **Gate**: child validations per layout pass **≤ 2N**; per-child `Failure::Scope` entries **0**; atomic refcount ops
    in clean `Panel::Paint` **≤ 2** per panel.
- [ ] **P1.9 Rich-document, math, flowchart and SVG quick fixes (`E28`–`E32`, `E71`).**
  - **Change**:
    - Cache line tops in `PreparedLayout` for `O(log L)` `TryVisibleRange` with **0** `HitTestTextPosition` calls;
      route semantic `TryVisibleRange` and `TryVisibleSourceRange` (`3651, 3966`) through `paintBoxes`; reuse
      `PaintIndex` capacity; add alpha 0/255 fast paths in `ComposeGifCanvas`; add fast `< U+0590` ASCII/Latin/CJK LTR
      range checks before `std::lower_bound` in `FirstStrongDirection` ([DxUi.RichBidi.h](../../../src/Controls/DxUi.RichBidi.h)
      `:21-45`, `makepad` `shaper.rs:220-236`).
    - Cache `PreparedMath` node-sized resolved colors and `PreparedFlowchart` `ResolveDiagramLabelBackground` contrast
      results + D2D factory pointer per palette key; replace paint-path sRGB `pow` decode with one shared 256-entry LUT
      in `Utilities.h` and `ChooseContrastingTextColor`.
    - Skip zero-length segment `DrawLine` calls in `PreparedFlowchart::Draw` (`910-917`), conserve visual ink on
      zoomed-out diagram edges below `0.7` device px (`widthPx = max(rawPx, 0.7f)`, `alpha *= rawPx / 0.7f`, `makepad`
      `render_lines/src/ink.rs:17-46`), size `Flowchart::Preparation` dynamically to graph size instead of fixed
      `>120 KiB` arrays, and rasterize SVGs in a single pass (`508-538`).
  - **Gates (`S9`, `S20`)**: line hit-tests per visible-range query **0**; `pow` calls and palette resolutions per
    steady paint **0**; `PaintIndex` rebuild allocations after warm-up **0**; SVG raster passes per image **1**.
- [ ] **P1.10 Fence coloring prefix retention and per-line lexical state cache (`E27`).** Above `MaximumCodeTokens`,
  keep tokens up to the last complete line inside the budget and leave the remainder plain (`rune-kit` `scan.rs`), and
  cache per-line `(start_state, end_state)` lexical continuation tags in [DxUi.RichCodeInk.h](../../../src/Controls/DxUi.RichCodeInk.h)
  (`makepad` `code_editor/src/tokenizer.rs:66-227`) so mid-fence edits re-lex only until `new_end_state == old_end_state`.
  **Gate**: a 4,097-token fence keeps every line before the cut colored; single-line edit in a 2,000-line fence re-lexes
  **≤ 2** lines when delimiter state is unchanged; usage docs updated.
- [ ] **P1.11 Viewport-scoped semantic code line numbers, source guides and fence badges (`E26`).**
  - **Change**: Draw only visible semantic fence line numbers with per-line clip; ring-buffer `RichSourceGuides`
    `PreparedLayout` entries to the visible window (`DxUi.RichSourceGuides.h:148-228`) instead of retaining 20,000
    layouts; cache shared `"copy"`/language `CodeLabels` layouts (`2849-2864`). (`P4.4` later reduces line-number
    layouts to **0**.)
  - **Gate (`S11`)**: retained guide/number layouts bounded by `visibleLines + margin`; badge layouts per language **1**.
- [ ] **P1.12 Host, DirectComposition TDR survival, failure-scope and pointer/wheel hygiene (`E9`, `E37`, `E38`, `E52`, `E55`, `E56`, `E73`, `E74`).**
  - **Change**:
    - `HOST-14` (delete dead `#if !DXUI_ENABLE_DIAGNOSTICS` code), `HOST-03` (scope `Ctrl` rich-title resolve),
      `HOST-09` (skip text-input activation on non-text focus), `HOST-13` (eliminate 5× tooltip copies, cache
      `SPI_GETMOUSEHOVERTIME` on `WM_SETTINGCHANGE`, check bounds containment before `IsControlInTree` in
      `UpdateTooltipHover`), retain solid brushes across `ResizeBuffers`, and guard `MenuBar` `Debug::Info` formatting
      behind an active sink while keeping `MenuBar` text widths on horizontal resize (`8287`).
    - Create `_dcompDevice` via `DCompositionCreateDevice2(nullptr, ...)` (`DxUi.WindowHost.cpp:835`, `makepad`
      `dcomp.rs:60-68`) so `_dcompTarget` and root `_dcompVisual` survive D3D11 TDR without tearing down the HWND visual
      tree (`4660-4662`), verify `IDCompositionDesktopDevice::CheckDeviceState` on `WM_PAINT` (`dcomp.rs:224-228`),
      coalesce and render synchronously during modal `WM_ENTERSIZEMOVE`..`WM_EXITSIZEMOVE` (`5291-5333`,
      `win32_window.rs:766-925`) so high-rate drag messages never starve `WM_PAINT`, and support opt-in first-`Present`
      window reveal to eliminate pre-first-frame flashes.
    - Reset `_deviceLossRetryCount = 0` only **after** `Present` succeeds (`DxUi.WindowHost.cpp:4408, 5240`, `rune`
      `#492`), register `ID3D11Device4::RegisterDeviceRemovedEvent` where available (`gpui-fast` `device_wake.rs`),
      evaluate `DXGI_SCALING_NONE` (`4446`), and defer reentrant Win32 display/power/DPI notifications via a host FIFO
      (`gpui-fast` `#125`).
    - Replace `Failure::Scope`'s `3 × std::set_terminate` per message (`Failure.cpp:103, 303`) with a once-installed
      terminate handler reading a `thread_local` scope stack, preserving every [failure handling](../../Core/Core_FailureHandling.md)
      diagnostic and dump guarantee.
    - Fix `ScrollPanel::OnMouseWheel` boundary no-op (`11820-11825`, `gpui-fast` `#104`), `EmbeddedHost::DispatchPointer`
      button-mask chord capture + `MiddleUp` release (`Embedded.cpp:953-990`, `gpui-fast` `#130`), stationary
      `WM_MOUSEMOVE` modality clobber (`3556-3568`), and fix `WM_PAINT` partial history if `P0.4` proves N−2 content.
  - **Gates (`S13`)**: brush creations on resize **0**; `set_terminate` calls per message **0**; boundary wheel
    invalidations **0**; `_dcompTarget` recreations across simulated D3D11 TDR **0**; persistent simulated
    `DEVICE_REMOVED` backs off deterministically instead of spinning.
- [ ] **P1.13 `RENDER-09` and zero-copy synchronous `IWICStream` decode (`E33`, `E75`).** `RichImage::LoadFile` moves or
  file-streams its encoded input into `Decode`, and synchronous `RichImageDecoder::Decode` (`DxUi.RichImage.cpp:250-256`)
  initializes `IWICStream` directly over the `encoded` span instead of copying up to `32 MiB` into `std::vector<BYTE>
  owned` (since `CopyPixels` at `:292` finishes before `Decode` returns at `:306`). **Gate**: **0** duplicate encoded
  buffers; peak synchronous memory at most `encodedSize + decodedSize`.
- [ ] **P1.14 Control rendering and setter guards (`E46`–`E50`).**
  - **Change**:
    - `ThroughputGraph::Paint`: retain grid geometry/brushes, pass `nullptr` to `PushLayer`, update the `180×64`
      spectrum bitmap incrementally (`CopyFromMemory` / `UpdateSubresource` 1-column strip per new sample instead of
      `11,520`-pixel CPU HSV raster + `CreateBitmap` + `70.7 KB` stack zeroing per frame), and route labels to `P4.1`.
    - `StatusStrip`: cache elided text per `(rawText, quantizedWidthDip, formatRevision)` and snap leading truncation to
      grapheme cluster boundaries via DirectWrite cluster metrics (`gpui-fast` `#132`), fixing the UTF-16 surrogate split.
    - `PageHost`: replace `CreateLayer` (`1802, 1935`) with `PushLayer(..., nullptr)`.
    - `ColorPicker` & `NumericStepper`: cache device/spectrum bitmap across paints, coalesce HSV drag child R/G/B/Hex
      updates into 1 `Value` UIA event (`P1.6`) while preserving hue/saturation across `V = 0`, guard
      `NumericStepper::SetValue`, and store default `ColorPicker::Labels` as static views or SSO strings.
    - Add unchanged-value guards across all ~30 unguarded setters (`DxUi.cpp`, `DxUi.Controls.cpp`,
      `DxUi.EditorControls.cpp`), fix `StackPanel` unconditional invalidation (`10660-10701`), and add missing
      invalidation to `CardPanel::SetCornerRadius` (`2736-2739`).
  - **Gate (`S15`, `S16`)**: `CreateBitmap`, `CreateLayer`, `CreatePathGeometry` and `CreateTextLayout` per steady
    `ThroughputGraph`/`StatusStrip`/`PageHost` frame **0**; structural UIA builds during `ColorPicker` drag **0**;
    no-op setter invalidations **0**.
- [ ] **P1.15 TSF IME, grapheme navigation and clipboard hot paths (`E62`, `E63`).**
  - **Change**:
    - `TextStoreACP` & `NativeTextInput`: serve `GetText` from `wstring_view` spans without whole-document copies,
      import committed text once (not twice in `InsertTextAtSelection` + `OnCompositionTerminated`), coalesce
      intermediate IME updates into a single undo entry in `TextField`/`ComboBox` (matching `RichTextEditor`, `rune`
      `#385`), and compute clipped `GetTextExt` via `HitTestTextPosition` instead of `O(M·N)` backwards probing (`124-168`).
    - Bound `StepToNextTextElement` / `SnapCaretIndexToTextElementBoundary` (`TextInputServices.cpp:85-144`) to a
      bounded window around `index` (`O(1)` instead of `O(N)` from 0), and decode full UTF-32 code points in
      `SelectSingleLineWordAt` (`836-865`) so astral emoji/CJK surrogate pairs never split.
    - Read snapshot once on embedded `Ctrl+C` (`893, 1083`), copy selection spans directly into `GlobalLock` without
      `_text.substr`, cache `RegisterClipboardFormatW(L"HTML Format")` once (`133, 205`), and support fenced 45 s
      `ZeroMemory` clipboard clearing (`GetClipboardSequenceNumber`) when copying from a secret/masked field (`rune`
      `secret.rs`).
  - **Gate (`S18`)**: full-document `wstring` copies per IME syllable drop from `15–25` to **≤ 1**; non-ASCII arrow
    grapheme step visits **O(1)** code units; surrogate-pair double-click selects the whole code point.

## P2 — Scheduling and frame pacing

- [ ] **P2.1 Deadline-aware dispatcher and lifecycle scrubbing (`E1`, `E4`, `E56`).**
  - **Change**:
    - `Tick` keeps its meaning (`true` requests continuous next frame); add `ControlHost::RequestWakeAt(uint64_t tickMs)`
      arming one `SetCoalescableTimer` for `min(deadline) - now` with 10% tolerance when only deadlines exist.
    - Keep `AnimationDispatcher`'s `HWND_MESSAGE` alive across idle transitions (`AnimationDispatcher.h:308-448`), fix
      frame-1 double-`dt` after idle, latch expired timers without firing ticks while the host is hidden/minimized/unvisible
      (`rune` `#324`), and rescue starved `WM_PAINT` messages after `16 ms` during multi-HWND animation (`rune` `#392`).
    - A deadline wake runs `Tick` once and renders only if something invalidated (`rune` `timer_wake.rs`); collapsed
      `TabControl`/`PageHost`/`Expander` subtrees use a 2-bit `NextFrameGate` (`(scheduled, pendingWhileHidden)`,
      `makepad` `next_frame_gate.rs:1-88`) so hidden subtrees schedule **0** ticks and run **1** catch-up sync on re-show.
    - Enforce central `ControlHost::ForgetControl` scrubbing of pending deadlines, hover/freeze targets, dirty-region
      owners and pointer captures on detach, hide and popup close (`BUI` `9080f51`), reclaiming large retired resource
      batches within a bounded `200 µs` per-frame budget (`makepad` `draw_list.rs:942`).
  - **Gate (`S1`)**: wakes in 10 s drop from ~640 to **≤ 20**; wakes while minimized/hidden **0**; detach fuzz passes
    ASan with **0** stale pointers.
- [ ] **P2.2 Shared caret policy (`E2`, `E3`).** One `CaretBlinkClock` across `TextField`, `ComboBox` and
  `RichTextEditor` honoring `GetCaretBlinkTime()` (`INFINITE` = steady), `SPI_GETCARETTIMEOUT`, reduced motion,
  `_readOnly` suppression, and `WM_SETTINGCHANGE`. **Gate**: **0** wakes after timeout; input restarts blink.
- [ ] **P2.3 Tooltip deadlines (`E2`).** Explicit 5-state `Tip` machine (`Idle`, `Armed`, `Open`, `Grace`, `Suppressed`,
  `rune` `#383`) driven by show/hide deadlines. **Gate (`S2`)**: wakes while shown drop from ~320 to **≤ 2**.
- [ ] **P2.4 Embedded animation request (`D3.1`).**
  - **Change**: Replace `NeedsAnimation()`/`AdvanceAnimation()` with
    `EmbeddedAnimationRequest GetAnimationRequest() const noexcept` (`{None | Continuous | Deadline, dueTickMs}`) and
    `EmbeddedAnimationRequest AdvanceAnimation(uint64_t nowTickMs) noexcept`. Update [hosting](../../../docs/hosting.md),
    [upgrade](../../../docs/upgrade.md), samples and `Tools/ConsumerApi`.
  - **Gate**: scheduler simulation in `Embedded.Tests` sees **≤ 2** host wakes per blink period and **0** when idle.
- [ ] **P2.5 Hover freeze and post-scroll hover.** Hold hover for 100 ms after wheel under an unmoved pointer, re-resolve
  once on a deadline, release exited hover immediately (`release_unhovered`), and suppress hover visuals at draw time
  while typing without invalidating hit-test state (`gpui-fast` `#124`, `#105`). **Gate (`S5`)**: hover re-hit-tests
  during notch burst **0**, **1** after settle, exited hover clears in **0 ms**.
- [ ] **P2.6 Unseen motion, loop gating and cooperative prep slice.** Transitions on controls invisible before and after
  land immediately; periodic loops (`ProgressBar`, `ThroughputGraph`, animated images in `B1`) suspend while unseen and
  resume in phase; background-prepared rich/label batches commit on the UI thread within a `500 µs` cooperative slice
  (`makepad` `async_labels.rs:252-318`). **Gate (`S12`)**: scrolled-away indeterminate progress bar adds **0** wakes.
- [ ] **P2.7 Native frame pacer, waitable-credit safety and EMA `dt` filter (`D5`, `E74`).**
  - **Change**: `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` (`MaximumFrameLatency = 1`), event-blocked pacer
    thread active only while continuous animation is armed, **strictly tracking `_frameLatencyCreditHeld`** (`makepad`
    `os/windows/win32/mod.rs:8-11, 194-195, 500-505`: latched `true` on waitable wake, cleared `false` **only** when
    `Present`/`Present1` executes or `ResizeBuffers`/swap-chain recreate runs, **never** waiting on the waitable handle
    while `_frameLatencyCreditHeld == true` so clean/occluded/zero-size ticks that skip `Present` cannot deadlock);
    cap `DCompositionWaitForCompositorClock` handle arrays at `32`; `DwmGetCompositionTimingInfo` presentation-time
    sampling with `< 1 ms` `qpcRefreshPeriod` DWM bug guard, `STATUS_GRAPHICS_PRESENT_OCCLUDED` (`0xC01E0006`) sleep, and
    3-wake `< 500 µs`–`1 ms` monitor-sleep timer fallback (`gpui-fast` `vsync.rs:62-79`, `makepad` `win32/mod.rs`);
    filter `FrameClock::SmoothDeltaUs` ([FrameRuntime.cpp](../../../src/Foundation/FrameRuntime.cpp) `:125-135`) with a
    `0.15` EMA clamped to `[0.5, 1.5] * dt_ema` plus `LagSmoothing` (`> 500 ms -> 33 ms`, `makepad`
    `scroll_motion.rs:702-741`, `tween/src/ticker.rs:14-45`); unified `Backoff` reset only on `Presented`/visible (`rune`
    `627c2b3e`); and focused-only VRR `Steady` policy (`rune` `#497`).
  - **Gates**: **0** pacer wakes without continuous animation; 10 consecutive woken ticks skipping `Present` complete
    with **0** deadlocks; steady 60 Hz `SmoothDeltaUs` variance stays within `[0.5, 1.5] * dt_ema`; local hardware
    frame-interval receipt recorded.

## P3 — Dirty regions and culling

- [ ] **P3.1 Region invalidation API (`D3.2`).** `Control::InvalidateVisual()` unions **both previous and current**
  bounds plus `GetVisualOverhangDip()` (`BUI` `3ec5a5e`) into an 8-rect device-snapped `DirtyRegion` (merging under 25%
  area overhead, falling back to full above 50% coverage or on topology/theme/DPI/size/device/smoke-overlay change).
  `RequestInvalidate()` stays full. Migrate caret, hover/press/focus, scalar values, scroll thumb and tooltip first.
- [ ] **P3.2 Embedded partial preparation (`E5`).** Push aliased axis-aligned clip per dirty rect, explicitly clear the
  clipped premultiplied rect to `D2D1::ColorF(0, 0, 0, 0)` (`BUI` `5cea7a6`) so translucent pixels never accumulate,
  paint with `P3.4` culling, and pop. **Gates**: `P0.3` parity (**0** BGRA diffs); caret blink paints at most caret rect + 2 px.
- [ ] **P3.3 Native partial present (`E7`).** Paint the union of this frame's region and the previous `BufferCount − 1`
  regions (if `P0.4` proves N−2 content), clear clipped rects on composition targets to `(0, 0, 0, 0)`, and present
  dirty rects via `Present1`. Force full frame on any failed/occluded/skipped present, resize or reset. **Gate**: `P0.3`
  parity with `P0.4` readback after every present.
- [ ] **P3.4 Clip culling (`E8`).** Track current paint clip in DIPs on `ControlHost`; `Panel`, `ScrollPanel`,
  `FlexPanel` and `FormPanel` skip children whose bounds + overhang miss the clip. **Gate (`S12` scrolled)**: child
  paints per frame bounded by visible children + 1 row margin.

## P4 — Text pipeline and typography

- [ ] **P4.1 Shaped-run cache (`E11`, `E45`; `gpui-fast` `RecentShapes`, adapted).**
  - **Key**: exact text (stored and compared), `IDWriteTextFormat` identity + revision, reading direction, locale,
    font-collection generation and measuring mode (plus pixels-per-DIP for non-natural modes).
  - **Value**: glyph runs captured via private `IDWriteTextRenderer` (pixel snapping disabled), advance width, height,
    baseline and overhang. Excludes wrapped, trimmed, inline-object, underline/strikethrough and mnemonic text.
  - **Storage & replay**: two arena generations (16 Ki glyphs or 1 MiB cap per generation) plus one `heavy` outlier slot
    (`gpui-fast` `path_cache.rs`), reported in `EmbeddedStatistics`, enforcing the `"Source Library wall"` epoch
    invalidation invariant (`makepad` `slug_atlas.rs:101-125`: any glyph/GPU cache eviction increments a cache epoch so
    upstream control caches never reference evicted runs). Suppress `liga`/`calt` OpenType features when
    `letterSpacing != 0` (`makepad` `shaper.rs:38-62`), and support an opt-in optical cap-height `ink_centered` vertical
    offset `0.5f * ((ascent - descent) - capHeight)` (`makepad` `draw_text.rs:314-327`) for single-line
    `Button`/`TabControl`/`ComboBox` chrome without mutating layout bounds. Replay via axis-aligned baseline-snapped
    `DrawGlyphRun`, sharing one color-glyph helper across `ID2D1DeviceContext7` and Windows 10
    `IDWriteFactory4::TranslateColorGlyphRun` + `ID2D1DeviceContext4`.
  - **Callers**: `DrawCenteredText` (63 callers), `DrawRawCenteredText`, `DrawTreeRow`, `ThroughputGraph` labels, `Grid`
    headers/badges/single-line cells, `ColorPicker` labels, `Menu`/`ComboBox` rows, and `MeasureSingleLineTextWidthDip`.
  - **Gates**: **0** layouts and **0** allocations per steady ComplexUi dirty frame; pixel-identical WARP replay across
    Latin, Arabic/Hebrew RTL, CJK, color emoji, combining marks and fallback fonts at DPI 96/144/192.
- [ ] **P4.2 `MeasureText` without heap.** Pass a single stack `DWRITE_LINE_METRICS` to extract line-0 `baseline`
  (`DxUi.Layout.cpp:93-107, 285-296`; `GetMetrics` already supplies `lineCount`), with single-line width/height served
  from `P4.1`. **Gate (`S12`)**: measure heap allocations **0**.
- [ ] **P4.3 Re-break in place (`rune` `research/28`).** Call `SetMaxWidth`/`SetMaxHeight` on `Label::_typographyLayout`,
  `PreparedLayout` and visible `Grid` cell layouts when only width/height changes. **Gate (`S5`, `S14`)**: layouts
  created **0**, with `P0.3` parity.
- [ ] **P4.4 Proved tabular numeric table.** Per `(font face, em size)`, shape `0`–`9 . , + - %` with `tnum` singly and
  across all 15×15 pairs once; enable direct glyph lookup only when every pair advance equals the sum of singles and
  maps 1:1 (excluding `:`, `/` and space). **Gate (`S11`)**: **0** layouts for line numbers and source gutters.
- [ ] **P4.5 Cache cliffs, `DxUi.Typography.h` overhaul, working-set protection and VRAM pressure (`E10`, `E57`, `E58`, `E72`).**
  - **Change**:
    - Convert host brush and format caches into frame-stamped two-generation maps (`makepad` `layouter.rs:392-401,
      514-531`): **never** evict entries touched in `currentFrame` or `currentFrame - 1`, and delete
      `ControlHost::TrimCaches`'s `_forceAllVisualPreparation = true` cliff (`DxUi.WindowHost.cpp:4678-4682`) so dense
      views using `>= 97` text formats never re-prepare the entire control tree every frame.
    - Replace `DxUi.Typography.h`'s global mutex, per-call heap `wstring` + `vector<DWRITE_FONT_AXIS_VALUE>`,
      `GetSystemFontCollection(..., TRUE)` ALPC probe and leaked static raw-pointer map (`194-433`) with a
      factory-scoped `wil::com_ptr_nothrow<IDWriteTextFormat>` two-generation cache keyed by a fixed POD key (inline
      2-axis array, font generation refreshed only on `WM_FONTCHANGE`/font-set event).
    - Add optional DXGI 1.4 `IDXGIAdapter3::QueryVideoMemoryInfo` hysteretic soft-cache trimming (`Normal < 80%`,
      `Elevated >= 80% / < 70%`, `Critical >= 92% / < 85%`, `makepad` `gpu_pressure.rs:14-181`) shrinking non-working-set
      glyph/shadow/image caches to 50%/0% only on state transitions, and deduplicate `MakeThemePalette`'s 4 Oklab scale
      passes (`DxUi.Theme.cpp:1068-1215`).
  - **Gate**: **0** heap allocations and **0** mutex/ALPC calls on format cache hit; **0** `_forceAllVisualPreparation`
    storms on 120-format screens; **0** leaked COM formats at exit.
- [ ] **P4.6 Text antialias mode and rendering params (`D8`, `E39`).** Grayscale on premultiplied/composition targets
  (with dark-theme contrast A/B captures informed by `rune` `#472`), system ClearType/grayscale setting on opaque HWNDs
  (`rune` `#491`), and per-monitor `CreateMonitorRenderingParams` refreshed on monitor change. **Gate**: **0** per-frame
  cost; gallery review.

## P5 — Layout (measure first)

- [ ] **P5.1 Measure-probe evidence, then `MeasureLog`.** Count repeated vs unique measure probes in `S12` and
  form/localized fixtures; add gpui-fast's 4-entry inline `MeasureLog` (~96 B/record) only if repeats exceed `1.5×`.
- [ ] **P5.2 Live label cutoff.** Skip `InvalidateMeasure()` in `Label::SetText` when single-line desired size (`P4.1`)
  is unchanged; invalidate visually (`P3.1`). **Gate**: ticking label causes **0** layout passes.
- [ ] **P5.3 Width settle and collapse landing hold.** Build width settle only if `P0.3` finds a wrapped leaf drawn at
  non-arranged width; adopt `rune` `#248` `hold_landing_width` when animating panel collapse so wrapped descendants do
  not reflow every collapse frame.
- [ ] **P5.4 Priority-ranked `ConcedingRow` bar overflow (`D11`).** Add an optional priority-ranked concession policy for
  `Toolbar` / command bars (`makepad` `conceding_row.rs:222-335`) that sheds lowest-priority slots into an overflow
  flyout in `O(N)` (`N <= 16`) and requires a `6 DIP` upward hysteresis margin before restoring conceded items during
  live window resize. **Gate**: continuous 1 px boundary oscillation triggers **0** overflow-flap cycles.

## P6 — Rich documents, piece tree, math and diagrams

- [ ] **P6.1 Piece-tree append slab, zero-copy editor slices and cross-edit semantic carry (`E25`, `E64`–`E68`).**
  - **Change**:
    - *Piece tree & editor*: replace per-keystroke 1-char `Buffer` creation (`DxUi.RichDocument.cpp:283, 1000`) with an
      append-only immutable `4 KiB` `AddBuffer` slab so `Piece::AppendCoalesced` coalesces consecutive typing into **1**
      leaf; add direct `PieceTree` slice/line readers so arrow, click, drag, IME and Rich `Backspace`/`Delete`
      (`DxUi.RichTextEditor.cpp:3657..5920`, `DxUi.RichTextCommands.cpp:4238`) never call full-doc `GetText()` or walk
      all leaves; replace `SourceDisplayMap`'s `1.05–4.2 MiB` dense UTF-16 vectors (`DxUi.RichSemantic.cpp:146-212`)
      with binary-searched sparse `SpanEntry` runs (`< 8 KiB`); clip `Ctrl+A` `GetSelectionRectangles` (`2910`) and
      highlight `HitTestTextRange` (`3775`) to visible chunks.
    - *Semantic carry & tables*: replace the `state_->document == document` gate (`2437`) with content-keyed carry for
      images (`{imageIdentity, frameEpoch, rasterTarget}`), math/flowcharts/SVGs (`{kind, exactSourceText + hash,
      widthBucket, typography, zoom, themeRevision}`), and `SemanticTable` column intrinsic widths + cell layouts
      (`MeasureTableOne` `1531-1625`); deduplicate `CloseElement`'s 2–3× text copies (`940`); skip `libfyaml`
      `ValidateYamlMetadata` (`417`) when edits occur past the frontmatter fence; keep width-independent markers,
      line-numbers and images on horizontal resize (`2445`) while reflowing viewport-first (`gpui-fast` `#129`) backed by
      a Fenwick `HeightTree` (`0.001 px` jitter gate, `0.5 px` default-height hysteresis, `N/8` in-place `O(N)` rebuild,
      `makepad` `portal_list.rs:360-726`).
  - **Gates (`S9`, `S19`, `S20`)**: 100 consecutive 1-char inserts add **≤ 2** piece-tree leaves; full-doc `GetText()`
    allocations on arrow/click/Backspace **0**; `SourceDisplayMap` bytes on 64 Ki doc **< 16 KiB**; prose keystroke
    outside math/diagrams/tables/frontmatter causes **0** math/diagram/table/YAML rebuilds and **0** image uploads.
- [ ] **P6.2 Source and Rich chunk translation and deduplicated scrollbar marks (`E69`).** Add `float translatedOrigin`
  to `Chunk` (`0`-ULP delta rebase for chunks after an edit), pre-scan chunk runs to skip throwaway `Phase::Reference`
  layouts when font/size are uniform (`DxUi.RichDocumentLayout.cpp:338`), narrow multiline selection invalidation to
  `union(oldSel, newSel)` (`5397`), merge `DrawSourceInkIfSupported` into **1** `layout.Draw` pass per chunk (`411,
  434`), and quantize/deduplicate scrollbar search/diagnostic/diff marks to integer track pixel rows via a reusable
  bitset (`makepad` `scroll_marks.rs:189-248`). **Gate (`S12`, `S19`)**: chunks reshaped per mid-doc keystroke in 64 Ki
  **≤ 2**; `Phase::Reference` layouts on uniform paragraphs **0**; `10,000` search hits emit at most `trackHeightPx`
  scrollbar tick quads.
- [ ] **P6.3 Incremental Markdown block reparse.** Measure full Markdown reparse in `Embedded.Tests.RichPreparationBenchmark`;
  if above 1 ms, reparse from enclosing block cut to next stable cut (`rune-kit` `markdown/cache.rs`) gated by a seeded
  AST-equality oracle (including lazy continuations and link reference definitions).
- [ ] **P6.4 Clickable-span hover (`HOST-08`).** Index link, footnote and image spans at preparation so pointer moves
  over plain text cause **0** invalidations and **0** title scans.
- [ ] **P6.5 Lazy `MathFontData` memoization, glyph-run atoms and BiDi inline-code isolates (`E70`, `rune` `#496`).**
  - **Change**: Retain `MathFontData` per `(IDWriteFontFace*, fontGeneration)` across semantic rebuilds and query
    OpenType `MATH` per-glyph metrics (`italicsCorrection`, `topAccentAttachment`, `GlyphVariants`, `GlyphAssembly`)
    lazily on first encounter per `glyphId` (`makepad` `latex_math/src/layout.rs:395-473`) instead of sweeping all `7,500`
    Cambria Math glyphs upfront (`DxUi.PreparedMath.h:421-471`); replace single-character `BuildTextAtom`
    `IDWriteTextLayout`s (`950-1005, 3117`) with direct `GetGlyphIndices` + design metrics + `DrawGlyphRun`; wrap
    Markdown inline-code runs in `LRI` (`U+2066`)..`PDI` (`U+2069`) isolates during RTL shaping.
  - **Gate (`S20`)**: upfront 7,500-glyph sweeps **0**; each distinct math `glyphId` queried **≤ 1** time per font face;
    single-glyph math atoms create **0** `IDWriteTextLayout`s with WARP visual review; RTL inline-code punctuation order
    matches reference expectations.

## P7 — Accessibility

- [ ] **P7.1 Client gating and value-only updates.** Delivered by `P1.5` and `P1.6`, plus gating
  `RaiseWindowHostTextInputAutomationEvent` (`DxUi.Accessibility.cpp:13616`) so typing with **0** UIA clients copies
  **0** document strings.
- [ ] **P7.2 Scroll carry (`E71`).** Translate cached descendant screen bounds by scroll delta in one flat pass and
  compute `RichTextEditor` semantic block/link/cell screen bounds lazily on UIA query (`2559`). **Gate (`S5`)**:
  structural builds per scroll notch **0**.
- [ ] **P7.3 Snapshot slimming, `O(N)` label index and `O(1)` `ITextRangeProvider` (`E20`, `E60`, `E61`).**
  - **Change**:
    - Build one `O(N)` label map per snapshot instead of `O(K·N)` `FindAssociatedLabelText` DFS (`4092`), size snapshot
      hash maps to `1.3×` instead of `4×` (`2199`), and replace the `45–60` `dynamic_cast` cascade (`2349-2908,
      5187-5325`) with a virtual `Control::PopulateAccessibility` hook.
    - Split `AccessibilityNodeSnapshot` (`~600+ B`) into a `≤ 128 B` core record plus rare role payloads, share
      immutable `PieceTree` / text snapshots instead of copying `128 KB` buffers into `value` + `documentText` (`2410,
      2423`) and twice per `ITextRangeProvider` call (`8677+`), bound `StepWordBoundary` (`4604`) locally around
      `offset` (`O(1)` per step), and materialize off-viewport `Tree` rows lazily when queried by UIA.
  - **Gate (`S17`)**: `ITextRangeProvider` 1,000-word walk on 64 Ki doc copies **0** full-document strings and runs in
    `O(words)` steps; `sizeof(AccessibilityNodeSnapshot)` **≤ 128 B**.
- [ ] **P7.4 `MenuPopup` keyboard step (`D2`).** Conditional on `S10`: update only old/new focused row records in place.
- [ ] **P7.5 Lazy tab order.** `WindowHost::FindAdjacentFocusable` (`DxUi.WindowHost.cpp:987-1035`) is already lazy on
  `VK_TAB`; verify `EmbeddedHost` and modal scopes do **0** eager tab-order work outside `VK_TAB`.

## P8 — Memory

- [ ] **P8.1 Cold control metadata and struct diet (`E35`, `E46`, `E49`, `E58`).**
  - **Verified `4acabe0e` x64 Release inline baseline vs target**:

    | Type | Current inline (`4acabe0e`) | Main bloat source | Target |
    | --- | --- | --- | --- |
    | `Control` | `~328 B` (`+ 39 B` alloc header) | 5 `wstring`s (`160 B`), 2 `function`s (`128 B`), 4 `optional`s | **≤ 176 B** (`+ 16 B` header) |
    | `Checkbox` / `Toggle` / `RadioButton` | `~896 B` / `~928 B` / `~880 B` | Inherit all `~832 B` of `Button` (icons, badge, menu, repeat) | **≤ 320 B** via shared `ToggleableControlBase` |
    | `ThroughputGraphHueWeight` | `24 B` | `{bool, pad[3], float, double}` alignment holes | **16 B** (`{float, float, bool}`) |
    | `ThroughputGraphSample` | `408 B` (`73.4 KB` × 180 ring) | `16 × 24 B` padded hue array | **≤ 160 B** (`< 29 KB`/graph) |
    | `ColorPicker::Labels` | `224 B` + 7 heap strings | 7 default strings exceed 7-`wchar_t` UTF-16 SSO | **0** default heap allocs |

  - **Change**: Move `Control`'s 5 cold strings, 2 callbacks and 4 optionals into an on-demand
    `std::unique_ptr<ControlCold>` backed by a static empty instance (`rune` `cold.rs`); extract `ToggleableControlBase`
    so `Checkbox`/`Toggle`/`RadioButton` stop carrying `Button`'s icon/badge/split-menu fields; pack
    `ThroughputGraphHueWeight`/`ThroughputGraphSample`; use sized C++17 `operator delete(void*, std::size_t)` in
    `ControlAllocation.cpp` to shrink the `+39 B` header to `16 B` and drop the `HeapSize` call on sized deletes; add a
    recorded `sizeof` test in `Foundation.Tests`.
  - **Gate**: all table bounds enforced by `static_assert` in x64 Release; measured ComplexUi private-bytes drop.
- [ ] **P8.2 Lifetime token (`E36`).** Keep `std::weak_ptr<int>` (`P1.8` removes per-paint lock traffic). Allow pooled
  `allocate_shared` only if profiling still attributes ≥ 2% of construction time to control blocks.
- [ ] **P8.3 `Grid` column and sort strings (`E24`, `E53`).** Hoist `GetColumn` out of the row paint loop into a
  reused member scratch vector, and sort row indices in `InMemoryGridModel::RebuildView` (`1395-1455`) using `GetCell`
  views/projections instead of copying `GridCellValue` strings `O(R log R)` times. Change `IGridModel` (`D3.3`) only if
  paint allocations remain. **Gate (`S5`)**: **0** paint allocations; **0** `wstring` copies inside `std::stable_sort`.

## Visual track — beauty with measured cost

- [ ] **V1 Motion foundation (`D6`, `E41`).** Closed-form `MotionValue` spring (`duration`, `bounce`, velocity-preserving
  retarget as in `rune` `spring.rs`; critical damping on opacity/color/blur, optional bounce on spatial channels;
  instant under reduced motion) packed into a `64 B` cache-line-aligned `Hot` state record (`makepad`
  `tween/src/engine.rs:89-117`) with `QuickTo` `Retime::ByDistance` duration scaling for cursor-chasing indicators
  (`tween/src/quick.rs:51-150`) + motion tokens in [tokens.json](../../DesignSystem/tokens.json). Migrate
  `Button`/`Toggle`/`Checkbox`, `Slider`, scrollbar, `Tree` expander/rows, then `PageHost`. **Gates**: continuity and
  bounded-settle unit tests, **0** allocations, **0** frames after settle, gallery review.
- [ ] **V2 Perceptual color transitions (`E40`).** Interpolate animated colors in OKLab using LUT-backed sRGB
  decode/encode (`P1.9`); compositing math unchanged. **Gate**: reference unit tests and gallery review.
- [ ] **V3 Crisp geometry (`E43`).** Hairline stroke width `max(1, round(1 DIP × scale))` px centered on pixel centers,
  opaque fills inset by half stroke under opaque borders (`rune` `fill_under`), negative interior SDF preservation
  (`min(max(q.x, q.y), 0.0f)` so `r == 0` corners never stroke solid, `makepad` `sdf.rs:436-468`), cached per-corner
  radius geometries, and `PushAxisAlignedClip` instead of `PushLayer` when content clears rounded corners (`rune`
  `17abbcb`). **Gate**: exact 1-device-pixel full-alpha border at 125/150/175% DPI; `PushLayer` count drops in `S12`.
- [ ] **V4 Analytic elevation shadows (`E13`, Phase B after `P1.2`).** Custom `DxUi.RoundedShadow` D2D effect
  (`ID2D1EffectImpl` + `ID2D1DrawTransform` pixel shader evaluating Evan Wallace's separable 1D-`erf` on X × 4-step
  Gaussian integral on Y, as in `gpui-fast` `shaders.hlsl:465-512`), extending shadow coverage `3 px` inside the border
  (`smoothstep(-3*px, 0, d)` to prevent 1 px bright border halos, `makepad` `surface.rs:188-193`) and supporting seamless
  callout-beak SDF union (`+ 1e20f` off-screen disable rather than `lerp`, `sdf.rs:574-616`), needing **0** intermediate
  surfaces and supporting animated card/flyout elevation, with `P1.2` `ShadowCache` as fallback. **Gate**: **0**
  intermediate surfaces per shadow, WARP pixel tests against reference integral, gallery review.
- [ ] **V5 Real popup materials (`D7`, `E15`).** HWND popups use `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_TRANSIENTWINDOW`
  where supported; in-window/embedded overlays share one downsampled `CopyFromBitmap` blur pyramid per frame across
  disjoint glass panes (`makepad` `backdrop.rs:123-131` `BackdropCompositor`) and interpolate mip levels linearly in
  Gaussian variance `r²` (`gauss_chain.rs:518-526`) in a retained scratch bitmap; High Contrast and fallback use solid
  themed fill. **Gates**: **0** GDI captures, **≤ 1** pyramid build per frame across disjoint panes, gallery review
  across themes.
- [ ] **V6 Scrolling feel (`E44`).** Critically damped wheel target (`rune` `ScrollMode`), fractional precision-touchpad
  deltas without 3-row quantization (`P1.7`), `1/64` px direction-reversal hold and `double` tall-document delta math
  (`> 16M px`, `rune` `#401`, `b45bfd66`), invertible rational rubber-band overscroll (`stretch_displayed`/`stretch_raw`
  for `0 px` reversal dead-travel, `makepad` `scroll_motion.rs:233-338`), degree-2 least-squares parabola release flick
  estimation (`lsq2_release_velocity` with first-sample `Option` startup guard, `scroll_motion.rs:569-615`), `WindowHost`
  DirectManipulation (`P2.7`), and 16 DIP edge fades. **Gates**: **0** frames at rest, `P0.3` parity at rest, gallery
  review.
- [ ] **V7 Real connected animation (`E42`).** Record source subtree into an `ID2D1CommandList` once at transition
  start, then draw transformed and cross-faded with target. **Gate**: **0** per-frame allocations.
- [ ] **V8 Text glide stability.** Snap animated Y translation of text-bearing subtrees to whole device pixels; snap X
  within a `1/64` px dead-zone at rest (`rune` `e1bd315`). **Gate**: zero baseline jitter across slow `PageHost` slide.

## BUI review (`ppmpreetham/BUI` at `ae284781`)

`BUI` adapts Blender's immediate-mode widget architecture into a C11 library with a tile-diffing CPU rasterizer
(`gpu/intern/gpu_raster.c`), a Direct3D 11 instanced backend (`gpu/intern/gpu_d3d11.c`), a single uber-shader
(`gpu/shaders/bui_ui.hlsl`), radial/grouped chrome, and a worker-pooled media subsystem (`kit/intern/image/`).

### License boundary (mandatory)

- `BUI` is licensed under **GPL-3.0-or-later** (`LICENSE`), with individual source and shader files under
  **GPL-2.0-or-later**. `DxUi` is **MIT** ([LICENSE.txt](../../../LICENSE.txt)).
- **Rule**: Zero source code, HLSL statements, struct definitions, verbatim comments or lookup tables from `BUI` may
  ever be copied into `DxUi` or its specifications. Only clean-room architectural observations, standard Win32/DXGI
  platform mechanics, general UX interaction models and independently MIT-licensed algorithms (such as Eric Lengyel's
  reference Slug paper/code or `rune`'s clean-room `Windfoil`) may inform `DxUi`.

### Where BUI independently confirms this plan's findings

1. **Flip-model partial-present hazard (`E7`, `P0.4`, `P3.3`).** `BUI`'s D3D11 backend (`gpu_d3d11.c:1618-1652`) uses a
   two-buffer `DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL` swap chain with `Present1` dirty rects and has to copy its entire
   persistent offscreen render target onto the back buffer before every partial present because the rotated back buffer
   holds two-frames-old pixels. `DxUi`'s `P3.3` avoids that full-window GPU copy by unioning the last `BufferCount − 1`
   dirty regions (or copying only dirty sub-rectangles).
2. **Single earliest-deadline host timer (`P2.1`–`P2.3`).** `BUI`'s context timer (`kit_custom.c:249-300`) computes the
   minimum due timestamp across media playback, text-caret blink and custom widget timers and arms one Win32 `SetTimer`
   only when a deadline exists and moves earlier, dropping idle CPU to `0.0%`.
3. **Transparent composition swap chain and premultiplied `(0, 0, 0, 0)` partial clears (`P3.2`, `P3.3`).** Commits
   `5cea7a6` and `f660180` show `BUI` abandoning layered-HWND hacks in favor of `IDComposition` +
   `CreateSwapChainForComposition` (`DXGI_ALPHA_MODE_PREMULTIPLIED`) with scissor-clipped transparent `(0, 0, 0, 0)`
   clears before redrawing dirty regions — confirming `DxUi`'s existing `CompositionSwapChain` architecture and proving
   `P3.2`/`P3.3` must clear dirty clips to transparent zero before painting so semi-transparent pixels never accumulate.
4. **Unchanged-value suppression + old/new bounds dirty union (`P1.14`, `P3.1`).** Commit `3ec5a5e` measured a `6.3×`
   D3D11 frame-rate gain from suppressing unchanged widget state and unioning both previous and current widget bounds on
   geometry moves.
5. **Central pointer/timer scrubbing on detach and popup close (`P2.1`).** Commit `9080f51` fixed use-after-free crashes
   when cached hover, active or timer pointers outlived a closed popup or removed widget, reinforcing `P2.1`'s central
   `ControlHost::ForgetControl` invariant.

### Clean-room items proposed for review (D10)

- [ ] **B1 Visibility-latched media clock, two-buffer ping-pong, retry embargo and hysteresis decode (`E75`; extends `P1.13`, `P2.6`, `P6.1`).**
  - **Change**: Latch a `drawn` bit on animated images in [DxUi.RichImage.cpp](../../../src/Controls/DxUi.RichImage.cpp)
    only when emitted inside the visible clip; arm `dueTickMs` only while `drawn && !paused && !finished`; rebase
    deadlines > 250 ms behind `now` to `now + frameDuration` instead of bursting catch-up frames; compose frames into a
    two-buffer ping-pong (`current`, `spare`) with **0** steady-state allocations; decode oversized static markdown
    images on the worker lane via `IWICBitmapScaler` with a `[0.5×, 1.25×]` size-hysteresis bucket (avoiding re-decode
    thrash during smooth resize/zoom), alpha-weighted RGBA mip downsampling (`makepad` `image_cache.rs:76-134` so
    transparent edges never bleed dark fringes), LIFO-within-priority scheduling plus pre-`CopyPixels` cancellation for
    scrolled-past thumbnails (`makepad` `image_tiles/src/grid.rs:1046-1062`), a `15 s` failed-decode retry embargo on
    corrupt/missing sources (`image_tiles/src/store.rs:85-112`), and frame-end eviction of unreferenced entries.
  - **Gates (`S9`)**: off-viewport animated GIF causes **0** wakes and **0** decodes; broken image source retries **0**
    times within 15 s; visible GIF allocates **0** buffers after frame 2; decoded bytes in `EmbeddedStatistics` stay
    bounded by target box × hysteresis ceiling.
- [ ] **B2 Per-corner radius mask and 1-DIP analytical bevel/inner-border chrome (extends `V3`).**
  - **Change**: Add a 4-bit `CornerMask` (`TopLeft = 1`, `TopRight = 2`, `BottomRight = 4`, `BottomLeft = 8`) to shared
    rounded-box fill/stroke helpers in `DxUi.Controls.cpp` so joined shipped controls (split `Button`, `NumericStepper`
    increment/decrement buttons, docked `TabControl` tabs and grouped `Toolbar` items) render crisp `0 px` inner seams
    and rounded outer corners with **0** extra `PushLayer` clips, paired with an optional 1-DIP top specular / bottom
    emboss rim from theme tokens.
  - **Gates**: **0** extra `PushLayer` calls on joined controls; WARP pixel tests at 100/125/150/200% DPI; gallery and
    design-system review.
- [ ] **B3 Infinite `ClipCursor` + `WM_INPUT` numeric scrubbing and menu typeahead (`D10`, `D11`).**
  - **Change**: Drag-scrubbing on `NumericStepper` using a `1×1` `ClipCursor` lock + `WM_INPUT` `RAWMOUSE` high-DPI
    relative deltas (`makepad` `rawinput.rs:132-222` — **never** `SetCursorPos` pointer warping, releasing unconditionally
    on `WM_KILLFOCUS`/`WM_CANCELMODE`), direction-splitting co-capture (`|dx| > |dy|` selects text, `|dy| >= |dx|` scrubs
    value, `number_field.rs:584-631`), Shift = `0.1×`, Ctrl = step snap, vertical cross-sibling sweep, **1** coalesced
    undo entry per drag gesture, and optional multi-word prefix/substring typeahead filtering in `MenuPopup` preserving
    arrow and UIA navigation.
  - **Gates**: deterministic tests in `Controls.Tests`, `ClipCursor` restoration on focus-loss/cancel, UIA parity,
    updated `docs/controls.md` and gallery tiles.
- [ ] **B4 Resolution-independent GPU curve text for zoomed vector surfaces (research option after `P4.1` and `P6.1`).**
  - **Change**: Keep DirectWrite (`P4.1`) at 1:1 UI scale; evaluate a clean-room `ID2D1DrawTransform` quadratic-Bezier
    outline shader (from Eric Lengyel's MIT-licensed Slug formulation and `rune` `#390`/`#482` `Windfoil` scale-motion
    switch, fed by `IDWriteFontFace::GetGlyphRunOutline`) only for actively zoomed `PreparedFlowchart`/Mermaid/math
    surfaces if post-`P6.1` pinch-zoom counters still show text rebuilds dominating frame work.
  - **Makepad empirical gate (`D11`)**: `makepad` had to set `band_count = 0` on `DrawText` (`slug_atlas.rs:334-336`) due
    to GPU shader-compiler banding bugs (degrading fragment work to `O(N_curves)` per pixel) and hit a `3.5 s` Adreno
    shader-compile stall from unrolled supersampling (`draw_text.rs:197-205`). `B4` ships **only** if horizontal/vertical
    band culling and `< 50 ms` HLSL compile times pass on WARP, Intel, AMD, NVIDIA and Qualcomm D3D11 drivers.
- [ ] **B5 Radial 8-slot `PieMenu` control (optional new catalog control; `D10`, `D11`).**
  - **Change**: Clean-room retained radial menu (`ControlCatalog`, factory, `docs/controls.md`, design-system guide and
    preview, gallery tile) supporting hold-drag-release directional flick (≥ dead-zone DIP radius, unbounded outer radial
    pick while captured), tight per-sector AABB bounds (`sector_bounds` cutting fragment overdraw ~`4.5×`, `makepad`
    `radial_menu.rs:464-530`), hit-testing against **resting** ring radii rather than spring-overshot animated radii
    (`:636-638`), asymmetric submenu ring dwell (`50 ms` child preview vs `90 ms` parent return, `:664-704`), and
    click-then-key mnemonic selection across up to 8 cardinal/diagonal slots in **≤ 1** overlay pass with full
    `Menu`/`MenuItem` UIA and keyboard parity.
- [ ] **B6 Segment-swept multi-control boolean drag-toggle (optional interaction polish).**
  - **Change**: Optional `Panel`/list gesture allowing a press-and-drag across stacked `Checkbox` or `Toggle` rows
    (sampled at `2 DIP` segment intervals so fast pointer motion never skips rows) to apply the initial toggled state
    across swept siblings in a single undo/UIA batch, disarmed cleanly on `Escape`.

### BUI mechanisms explicitly rejected for DxUi

- **Cold-start CPU rasterizer vs GPUI startup framing (`gpu_raster.c:267-306`)**: `EmbeddedHost` borrows the host's
  existing D3D11 device (`0 ms` init) and already prepares clean frames in `0.0003 ms`.
- **Full-frame `CopyResource` on every `Present1` (`gpu_d3d11.c:1632`) and UI-thread `DwmFlush()`**: wasteful bandwidth
  and UI-thread stalls compared with `P3.3` history-union partial present and `P2.7` waitable swap chains.
- **UI-thread `WaitForSingleObject(..., INFINITE)` on worker pool during resize** and **`Sleep(1)` busy-wait fallback**:
  violate `DxUi`'s non-blocking UI/composition and zero-polling contracts.
- **24× per-frame `Map(WRITE_DISCARD)` + `DrawInstanced` flushes per z-layer** and **per-frame `CreateTexture2D` uploads
  without a staging/sub-rect pool**: Direct2D batching + `P1.14` `UpdateSubresource` avoid this driver churn.
- **64×32 CPU pixel-diff tile filtering (`gpu_raster.c:405-425`)**: redundant on a GPU D2D target once `P1.14`/`P3.1`
  suppress unchanged invalidations before rasterization.
- **`1,256 B` fixed inline immediate-mode widget struct** and **single-page clear-all glyph atlas cliff**: opposite of
  `P8.1` (`≤ 176 B` `Control`) and `P4.1`/`P4.5` two-generation caches.
- **Single-font `stb_truetype` text, hardcoded English strings, no RTL and no UIA/TSF IME/High Contrast**: incompatible
  with `DxUi` accessibility, localization and Unicode requirements.

## Makepad study (`makepad/makepad` at `506ec051`)

`makepad/makepad` (`506ec05105b9adb08d1d21f9879df181b5244ea7`, **MIT License** — directly compatible with `DxUi`'s MIT
license in [LICENSE.txt](../../../LICENSE.txt)) is a Rust/DSL studio UI engine combining retained `WidgetRef` trees,
immediate-style `Turtle` layout passes, instanced D3D11/HLSL quad shaders (`platform/src/os/windows/d3d11.rs`,
`draw/src/shader/`), DirectComposition (`dcomp.rs`), hybrid `Slug` + raster glyph atlases (`draw/src/text/`), virtualized
`PortalList`/`DataGrid`/`ImageTiles` (`widgets/src/`), OpenType `latex_math`, `flowgraph` and `tween`.

### Source-verified crate disambiguations (do not misread crate names)

1. **`libs/relax` (`CONTRIBUTING.md:1-4`, `libs/relax/crates/relax_api/src/lib.rs:72-98`)**: an in-tree Rust JIT/AOT live
   hot-reload compiler consuming `.relaxapi` interface manifests — **not** a constraint or UI layout engine.
2. **`libs/skyline` (`libs/skyline/src/lib.rs:1-38`)**: a procedural 360° equirectangular night-skyline HDRI environment
   generator for 3D reflections — **not** a 2D skyline bin packer. Makepad's actual glyph/SDF atlas packer lives in
   `libs/font_core/src/raster.rs:436-762` (`MultiPlaneAllocator` + Guillotine `RectPacker` with a 64-probe scan budget).
3. **`libs/alloc_epoch` (`libs/alloc_epoch/src/lib.rs:1-102`)**: a macOS `makepad-cef` `dlopen` thread-parking barrier;
   on Windows (`:63-66`) it compiles to no-ops.
4. **`draw_sploded_hairline.rs` (`draw/src/shader/draw_sploded_hairline.rs:10-195`, `platform/src/sploded.rs:1-250`)**: an
   interactive 2.5D isometric exploded z-layer and overdraw inspector emitting 8-triangle hollow `OutlineGeom` wireframe
   rings (zero interior fragment fill so overdraw heatmaps stay exact) — **not** ordinary widget border hairlines.
5. **`libs/flow` vs `libs/flowgraph`**: `libs/flow` (`src/lib.rs:1-108`) is a headless async task runner with manual
   `(x, y)` node coordinates; interactive corridor-filtered orthogonal wire routing (`wire_route.rs:76-799`) lives in
   `libs/flowgraph`.
6. **`libs/source_slice` (`libs/source_slice/src/lib.rs:1-45`)**: a build-time Git CLI dependency-closure extractor —
   **not** a runtime zero-copy text slice representation.
7. **`widgets/src/code_editor/src/text.rs` (`:10-12`, `layout.rs:414-421`)**: stores lines as `Vec<String>` and hardcodes
   a strict monospace character grid — **not** a rope or piece tree (`DxUi`'s `RichDocument` piece tree + `P6.1`
   `AddBuffer` append slab is strictly stronger).
8. **`gpu_pressure.rs`, `gpu_prof.rs`, `memory_watchdog.rs`, `threadcpu.rs` and `main_lag.rs`**: unwired or no-op stubs
   on Windows at `506ec051`; only `gpu_pressure.rs`'s hysteretic threshold state machine (`80%/70%`, `92%/85%`) is
   adapted onto DXGI 1.4 `IDXGIAdapter3::QueryVideoMemoryInfo` in `P4.5`.

### High-value empirical and architectural findings transferred to DxUi

1. **Latency-1 waitable swap-chain unspent-credit deadlock (`platform/src/os/windows/win32/mod.rs:8-11, 194-195,
   500-505` -> `P2.7`).** Waiting on `IDXGISwapChain2::GetFrameLatencyWaitableObject()` consumes **one** semaphore credit
   that DXGI returns **only** when `Present` retires. When a woken tick skips `Present` (clean deadline in `P2.1`,
   occluded window or zero-size client), calling `WaitForMultipleObjects` on the waitable handle again at
   `MaximumFrameLatency = 1` **deadlocks forever**. `P2.7` adopts Makepad's `_frameLatencyCreditHeld` state bit verbatim,
   alongside its 32-handle `DCompositionWaitForCompositorClock` limit and `0xC01E0006` / `< 500 µs` 3-strike sleep guards.
2. **Device-independent DirectComposition HWND target & modal `WM_SIZING` coalesce (`dcomp.rs:60-68, 224-228`,
   `d3d11.rs:58-73`, `win32_window.rs:766-925` -> `E73`, `P1.12`).** Passing `nullptr` to `DCompositionCreateDevice2`
   decouples `_dcompDevice` and `_dcompTarget` from `IDXGIDevice` so D3D11 TDR only swaps a fresh `IDCompositionSurface` /
   swap chain onto the surviving visual tree instead of tearing down the HWND target (`DxUi.WindowHost.cpp:835,
   4660-4662`), while synchronous coalesced rendering inside `WM_ENTERSIZEMOVE`..`WM_EXITSIZEMOVE` prevents high-rate
   resize pointer traffic from starving `WM_PAINT`.
3. **Working-set frame-generation cache protection & upload pointer epochs (`layouter.rs:392-401, 514-531`,
   `d3d11.rs:144-169`, `slug_atlas.rs:101-125` -> `E72`, `P4.1`, `P4.5`).** Makepad's `Layouter` stamps entries with
   `last_used_frame` and refuses to evict `current_frame` or `current_frame - 1` entries (directly fixing `DxUi`'s `E72`
   `TrimCaches` `_forceAllVisualPreparation = true` storm), pairs GPU cache eviction with `"Source Library wall"` epoch
   invalidation, and replaced `0.1–1.5 ms` per-frame `SipHash` byte hashing with `O(1)` pointer/epoch identity checks.
4. **Two-lane background/text instanced batching (`platform/src/draw_list.rs:479-504, 689-721`).** Separating opaque/SDF
   control backgrounds from glyph draw calls by `append_group_id` collapses `N` alternating button-background + label
   pairs from `2N` draw submissions to `2` without `O(N²)` overlap checks — a useful reference if `DxUi` ever batches
   custom `ID2D1DrawTransform` / `ID2D1Mesh` primitives outside standard D2D command batching.
5. **Empirical `Slug` GPU curve-text pitfalls (`draw/src/text/{slug_atlas.rs:334-336, draw_text.rs:197-230}` -> `B4`).**
   In production, Makepad had to disable curve banding (`band_count = 0`) on `DrawText` because GPU shader compilers
   miscompiled the banded loop, turning fragment evaluation into `O(N_curves)` per pixel, and hit a `3.5 s` Adreno shader
   compilation freeze when `#[unroll]` duplicated the winding kernel across 4 supersamples. This confirms `P4.1`
   DirectWrite shaped-run replay must remain `DxUi`'s 1:1 UI path and sets strict multi-IHV driver gates on `B4`.
6. **Lock-safe external hang stack capture (`libs/ui_hang/src/windows.rs:80-161`).** Capturing a hung UI thread via
   `SuspendThread` -> `GetThreadContext` -> bounded `ReadProcessMemory` -> `ResumeThread` in `< 50 µs` — resolving
   `SymFromAddrW` only *after* `ResumeThread` so symbol allocation never deadlocks on the target thread's Windows heap
   lock — is recorded as a clean pattern for out-of-process `test.ps1` timeout forensics (never inside `DxUi.lib`).

### Makepad mechanisms explicitly rejected for DxUi

- **Runtime `D3DCompile` HLSL string compilation (`d3d11.rs:1808-1834`)**: `DxUi` compiles shaders offline at build time
  (`0 ms` cold startup, zero `d3dcompiler_47.dll` runtime cost or driver-JIT variance).
- **Full-buffer `D3D11_USAGE_DEFAULT` `UpdateSubresource(..., nullptr, ...)` instance re-uploads, unpooled `CreateBuffer`
  churn and per-draw `OMSetRenderTargets`/`RSSetViewports`/`RSSetScissorRects` rebinding (`d3d11.rs:774-797,
  1632-1677`)**: Direct2D batching + `P1.14` sub-rect `UpdateSubresource` avoid this driver overhead.
- **`DXGI_SWAP_EFFECT_FLIP_DISCARD` `ResizeTarget` without `Present1` dirty rects (`d3d11.rs:1505-1531`)**: incompatible
  with `P3.3` history-union partial present and `EmbeddedHost` borrowed targets.
- **Immediate-mode `Turtle` per-frame full-walk layout + `defer_walks` 2-vec replay (`turtle.rs`)**: `DxUi` retains
  measured/arranged bounds on controls and skips clean subtrees.
- **`code_editor` `Vec<String>` document storage and monospace-only layout (`text.rs:10-12`, `layout.rs:414-421`)**:
  inferior to `DxUi`'s UTF-16 piece tree (`P6.1`) and proportional/RTL/complex-script DirectWrite editor (`P6.2`).
- **Unbanded `Slug` (`band_count = 0`) on 1:1 UI chrome (`slug_atlas.rs:334-336`)**: wastes fragment ALU and loses
  ClearType/DirectWrite hinting at standard desktop sizes (`P4.1`, `P4.6`).
- **In-process `ui_hang` background polling thread (`libs/ui_hang/src/lib.rs`)**: violates `DxUi`'s zero-polling and
  zero-worker embedded hosting contracts ([embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md)).

## Sequencing

```mermaid
flowchart LR
    P0["P0 counters, S1-S20, oracle, swap-chain truth"] --> P1["P1 hotspots, setters, UIA gate, IME"]
    P0 --> P2["P2 deadline scheduling & pacer"]
    P0 --> P3["P3 dirty regions & culling"]
    P1 --> P4["P4 text pipeline & Typography.h"]
    P3 --> P4
    P2 --> V1["V1 springs"]
    P2 --> V6["V6 smooth & fractional scrolling"]
    P1 --> V4["V4 analytic shadows"]
    P4 --> P5["P5 layout"]
    P4 --> P6["P6 piece tree, semantic carry & math"]
    P3 --> P7["P7 accessibility scaling"]
    P1 --> P8["P8 struct diet & cold metadata"]
    V1 --> V7["V7 connected animation"]
    P1 --> V5["V5 popup materials"]
    P4 --> V8["V8 text glide"]
    P3 --> V3["V3 crisp geometry & B2"]
    P0 --> V2["V2 OKLab color"]
    P4 --> D4["D4 strip cache decision"]
    P2 --> D4
    P6 --> B1["B1 media clock & hysteresis decode"]
```

API revision 6 (`D3`) lands as one PR carrying `P2.4`, `P3.1`'s public surface and any `P8.3` interface change, together
with `capabilities.json`, `Tools/ConsumerApi`, `docs/upgrade.md` and samples.

## Risk register

| Risk | Mitigation |
| --- | --- |
| Glyph-run replay differs from `DrawTextLayout` (snapping, color fonts, fallback) | Pixel-identical WARP corpus gate; any failing class keeps using a layout; cache lives behind one helper |
| Partial present or partial premultiplied clear shows stale/accumulated pixels | `P0.4` truth test first; `BufferCount − 1` history union; explicit `(0,0,0,0)` clip clear; force full on uncertain present; parity readback |
| Stale control pointer in pending deadlines, hover freeze or dirty regions after detach/close | Central `ControlHost::ForgetControl` scrub (`P2.1`) + ASan detach/popup-close fuzz steps in `P0.3` |
| Culling or missing old-bounds union leaves trails outside bounds | Union old + new bounds + `GetVisualOverhangDip()` (`P3.1`); `P0.3` oracle; `RequestInvalidate()` stays full |
| Device-loss retry loop or reentrant Win32 display/power message corrupts host state | Reset retry count only after `Present` succeeds (`P1.12`); `DCompositionCreateDevice2(nullptr)` so HWND visual target survives TDR; FIFO message deferral; event-blocked pacer joined before detach |
| Latency-1 waitable swap chain deadlocks when a woken tick skips `Present` | Track `_frameLatencyCreditHeld` (`P2.7`): never wait on waitable object while credit is unspent; clear only on `Present`/`Present1` or `ResizeBuffers`/recreate; 10-skip regression test |
| Format/glyph cache overflow causes `_forceAllVisualPreparation = true` storm or stale GPU run pointers | Frame-stamped working-set protection (`P4.5`, never evicting `currentFrame`/`currentFrame - 1` entries and deleting `_forceAllVisualPreparation = true` from `TrimCaches`) + `"Source Library wall"` cache-epoch invalidation (`P4.1`) |
| MSVC Debug `_ITERATOR_DEBUG_LEVEL == 2` allocates inside `noexcept` moves | Construct containers outside `noexcept` or use pre-allocated storage + `swap`; verify x64 Debug + ASan Debug (`P1.7`, `P8.1`) |
| Cross-edit carry shows stale math, diagrams or tables | Exact source-text compare (never hash-only); theme/zoom/width in key; `P0.3` parity oracle against fresh preparation |
| `EmbeddedHost` UIA client gate misses late-attaching AT | `WM_GETOBJECT` / `GetNativeAutomationObject` sets `providerRequested_ = true` and builds synchronously on first query (`P1.6`) |
| Visual changes surprise consumers | `D6` sign-off; one harness gallery carousel per `V`/`B2` item; upgrade notes; reduced-motion and High Contrast unchanged |
| API 6 churn across consumers | One bundled compile-breaking revision; each consumer migrates explicitly in its own repository |
| Counters themselves cost time | Thread-local plain integer increments only; `< 1%` overhead gate in `P0.1` |

## Closeout gates

1. Every accepted item is ticked with its commit, its deterministic counter receipt under
   `Measurements/RuntimeEfficiency_2026-11-11/` and its tests.
2. `validate.ps1`, `format.ps1 -Check` and `Tools/Test-Changelog.ps1` are green. `test.ps1` passes in x64
   Debug/Release/ASan Debug, and all three ARM64 configurations build. Native ARM64 runtime is claimed only after
   native execution.
3. `Menu`, `NativeTextInput` and the menu resource fixtures run through `test.ps1 -Interactive` with the
   developer's agreement to the time.
4. Timing results (`cpu_ms` and `wall_ms`) go to D-P1 as advisory evidence. This plan claims no timing verdict until
   D-P1 qualifies its policy.
5. Normative behavior moves into the owning contracts. `docs/performance.md`, the design system, `docs/controls.md`
   and the gallery are updated. The plan then moves to `Specs/Plans/Done/`. Consumer adoption of API 6 is never
   closed here.
