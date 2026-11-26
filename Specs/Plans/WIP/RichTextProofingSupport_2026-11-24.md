# RichText proofing and Markdown decoration infrastructure (`DX-SPELL-1`–`DX-SPELL-8`)

## Metadata

- **Status**: `HOLD` — queued for a future DxUi API/feature wave (`A8` / API revision bump); planning record only, no code implementation has started.
- **Created**: `2026-11-24` · **Updated**: `2026-11-24`
- **Owner**: DxUi maintainers
- **Planned-at commit**: `f51be47b812372a97a77c58ab74411bc8feacb0b` (`RedSalamanders/DxUi`)
- **Owning contracts**: [repository guidance](../../../AGENTS.md), [specification authority](../../README.md), [architecture](../../Core/Core_Architecture.md), [failure handling](../../Core/Core_FailureHandling.md), [performance and resources](../../Core/Core_PerformanceAndResources.md), [controls and layout](../../UI/UI_ControlsAndLayout.md), [input and accessibility](../../UI/UI_InputAndAccessibility.md), [validation](../../Testing/Testing_Validation.md), and [run evidence](../../Testing/Testing_RunEvidence.md)
- **Coordinated plans**: [Remaining work and qualification](RemainingWork_2026-11-04.md), [Performance and motion](PerformanceAndMotion_2026-11-13.md) (coordinated API revision bump in its `A8`), and [Explicit state machines and lifetime safety](ExplicitStateMachines_2026-11-10.md)
- **Consumer counterpart**: `RedTilde/Specs/Plans/WIP/DxUiProofingSimplification_2026-11-24.md` (currently `HOLD` behind this producer wave and a subsequent `Dependencies/DxUi.lock.json` pin update)

---

## 1. Architectural boundary and motivation

While implementing continuous Markdown-aware spell checking in RedTilde against the pinned DxUi API-5 baseline, RedTilde had to write and maintain ~650 lines of duplicated adapter logic around `SemanticDocument`, `MarkdownSyntax`, `SourceDisplayMap`, and `RichTextEditor`:

1. Walking `SemanticElement` / `StyleRun` / `DisplaySpan` and raw `MarkdownSyntax` blocks/inlines to mask non-prose spans (`Common/ProseExtractor`).
2. Manually scanning YAML/TOML `FrontMatter` text for top-level keys (`lang:` / `language:`) despite `libfyaml` and `toml++` already running inside `DxUi.lib`.
3. Slicing document decorations into viewport windows (`+/- 2,048` units), hooking scroll callbacks, and backing off `{256, 128, 64}` on `E_BOUNDS`.
4. Cloning and re-submitting filtered decorations on every caret move / active typing debounce expiry, retiring UIA annotation peers unnecessarily.
5. Converting screen `POINT` -> host DIPs -> `TryHitTestTextInputPoint` on right-click even though `RichTextEditor::OnContextMenu` already computes `HitSource(host, point)` internally.
6. Re-inventing boundary-checked disjoint `SourceEdit` builders across mid-word inline delimiters (e.g., `**re**commnd`).
7. Plumbing `_spell.immediatePending` through `WM_PAINT` and `ReconcilePostPreparePlacement` to discover when asynchronous semantic/syntax preparation publishes.

**Hard boundary invariant (permanent):** `DxUi` MUST remain pure text, Markdown, layout, decoration, input, and UIA infrastructure — **zero dictionaries, zero COM spellcheckers (`<spellcheck.h>`), zero user settings, and zero network I/O**.

---

## 2. Planned additions (`DX-SPELL-1`–`DX-SPELL-8`)

### `DX-SPELL-1` — Canonical prose-segment visitors (`TryVisitSemanticProseSegments` + `TryVisitSyntaxProseRanges`)

- **Current anchor**: `TryVisitSemanticWords` in [src/Controls/DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp#L116) ([include/DxUi/RichSemanticDocument.h](../../../include/DxUi/RichSemanticDocument.h)).
- **Current friction**: `TryVisitSemanticWords` already filters excluded semantic intervals (`Code`, `Html`, `Metadata`, `Math`, `Diagram`, `Image`, `Style::Code`, `Generated`/`Object` spans, literal URLs), then breaks text with `IsWordCharacter`, which excludes `'`, `’`, and `-` — splitting English contractions (`don't`, `won't`) and French elisions (`l'arbre`, `aujourd'hui`, `peut-être`). No comparable prose-interval visitor exists for Source mode over `MarkdownSyntax`.
- **Target API & behavior**:
  - `SourceResult TryVisitSemanticProseSegments(const SemanticDocument& document, MarkdownRange display, SemanticProseVisitor visitor, void* context, std::size_t& visitedChars)` yielding each contiguous unexcluded prose slice `(std::wstring_view displaySlice, MarkdownRange displayRange)` without altering word-counting APIs (`TryVisitSemanticWords` / `CountWords`).
  - `SourceResult TryVisitSyntaxProseRanges(const MarkdownSyntax& syntax, MarkdownRange source, SyntaxProseVisitor visitor, void* context)` yielding raw source prose intervals for `Paragraph`, `AtxHeading`, `SetextHeading`, and `TableCell` blocks while skipping fences, inline code, inline/display math, HTML, metadata, footnote references, and link/image URL tails (retaining visible link labels).
- **Ownership & failure contract**: Synchronous, pure, allocation-minimal callbacks adhering to `SourceResult` bounds validation and zero exceptions across visitor boundaries.

### `DX-SPELL-2` — Layered decoration channels (`RichTextDecorationLayer`) + viewport-clipped geometry preparation

- **Current anchor**: `TrySetDecorations` and `GetDecorationStatus` in [include/DxUi/DxUi.h](../../../include/DxUi/DxUi.h#L3764) and [src/Controls/DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp#L454) (`_state->decorationRectangleLimit = 4096u`, hard `decorations.size() > 256u` cap at `L1811`).
- **Current friction**: `RichTextEditor` owns a single flat `std::vector<RichTextDecoration>` capped at `256` items across the whole document and eagerly maps D2D rectangles for every off-screen item. Independent consumers (e.g., Find vs Spelling) would overwrite each other unless coordinated externally.
- **Target API & behavior**:
  - Add `enum class RichTextDecorationLayer : uint8_t { Consumer, Spelling, Grammar, Diagnostic }` and an overload `HRESULT TrySetDecorations(const RichText::SourceSnapshot& expectedSource, RichTextDecorationLayer layer, std::span<const RichTextDecoration> decorations)` (preserving existing single-layer overload mapped to `Consumer`).
  - Raise logical stored entries per layer to `4,096` (bounded to document scale) while **lazily mapping D2D geometry rectangles only for decorations overlapping the current visible viewport plus bounded overscan** during `PrepareContent`, maintaining the `4,096`-rectangle paint/resource ceiling and zero steady-state allocations on settled frames.
  - Diff UIA annotation peers (`SpellingError`, `GrammarError`) per layer so Find/diagnostic updates never retire unchanged spelling peers.

### `DX-SPELL-3` — Rich context-menu request payload (`SetOnRichContextMenu`)

- **Current anchor**: `RichTextEditor::OnContextMenu` in [src/Controls/DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp#L7104-L7150).
- **Current friction**: `RichTextEditor::OnContextMenu` already calls `HitSource(host, point)` internally (`L7119`), yet `Control::OnContextMenu` forwards only `(POINT screenPoint, bool keyboardInvocation)`, forcing callers to recompute DPI conversion and `TryHitTestTextInputPoint`.
- **Target API & behavior**:
  - Add `struct RichTextContextMenuRequest { POINT screenPoint; bool keyboardInvocation; RichText::SourceSnapshot source; RichText::Selection selection; std::optional<std::size_t> hitSourceOffset; };` and an additive callback setter `SetOnRichContextMenu(std::function<bool(const RichTextContextMenuRequest&)> callback)` (retaining existing `SetOnContextMenu` intact).
  - For pointer invocations, `hitSourceOffset` carries the exact resolved source offset; for keyboard invocations (`Shift+F10` / Menu key), `hitSourceOffset` defaults to `selection.caret` (or `std::nullopt` with documented semantics).

### `DX-SPELL-4` — Lightweight `O(1)` per-layer decoration suppression (`SetDecorationSuppressionRange`)

- **Current anchor**: `RichTextEditor::TrySetDecorations` (`RetireSemanticAccessibility()`) in [src/Controls/DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp#L1863-L1872).
- **Current friction**: Suppressing a squiggle under the single word touched by the caret during active typing requires re-filtering and re-calling `TrySetDecorations`, which dirty-marks overlay highlights and retires semantic accessibility peers on every keystroke/timer tick.
- **Target API & behavior**:
  - Add `void SetDecorationSuppressionRange(RichTextDecorationLayer layer, std::optional<RichText::MarkdownRange> sourceRange) noexcept` in `RichTextEditor`.
  - Paint/hit-test/UIA annotation visibility skips decorations intersecting `sourceRange` in `O(1)` without mutating stored decorations, invalidating layout geometry, or rebuilding D2D resources except repainting the affected line.

### `DX-SPELL-5` — Semantic display-range replacement planner (`TryPlanDisplayRangeReplacement`)

- **Current anchor**: `SourceEdit` and `TryApplySourceEdits` in [include/DxUi/RichSemantic.h](../../../include/DxUi/RichSemantic.h#L93-L97).
- **Current friction**: Words spanning inline formatting markers (e.g., `**re**commnd`) map via `SourceDisplayMap::TrySourceRanges` to disjoint source fragments `[r0, r1]`. Constructing safe multi-fragment `SourceEdit` vectors requires delicate lifetime and boundary logic (note `SourceEdit::replacement` is a borrowed `std::wstring_view`).
- **Target API & behavior**:
  - Add `SourceResult TryPlanDisplayRangeReplacement(const SemanticDocument& document, MarkdownRange displayRange, std::wstring_view replacementText, std::vector<SourceEdit>& outEdits)` ensuring `outEdits` are sorted, non-overlapping, boundary-valid, and borrow directly from `replacementText` (preserving outer delimiters on `fragments[0]` and clearing trailing fragments).

### `DX-SPELL-6` — Read-only front-matter scalar query (`MarkdownSyntax::TryGetFrontMatterScalar`)

- **Current anchor**: `MarkdownSyntax` ([include/DxUi/MarkdownSyntax.h](../../../include/DxUi/MarkdownSyntax.h)) and front-matter diagnostics in [src/Controls/DxUi.RichSemanticDocument.cpp](../../../src/Controls/DxUi.RichSemanticDocument.cpp#L3050-L3095).
- **Current friction**: `MarkdownParser` already parses leading YAML 1.2.2 (`libfyaml`) and TOML 1.0.0 (`toml++`) envelopes (`MarkdownBlockKind::FrontMatter`) during syntax analysis, but exposes no read-only key lookup.
- **Target API & behavior**:
  - Add `[[nodiscard]] std::optional<std::wstring> TryGetFrontMatterScalar(std::wstring_view key) const` returning a validated top-level UTF-16 string scalar (e.g., `lang`, `language`) only when the envelope is valid and non-diagnostic, keeping parser ownership internal.

### `DX-SPELL-7` — Semantic and syntax publication notifications (`SetOnSemanticPublished` / `SetOnSyntaxPrepared`)

- **Current anchor**: `RichTextEditor` preparation lifecycle (`GetSemanticDocument()`, `GetMarkdownSyntax()`).
- **Current friction**: Right after `TrySetText` or a view change, `SemanticDocument` is not yet built until preparation advances. Consumers initiating background analysis (spell checking, outline refresh) must either check inside `WM_PAINT`/post-prepare messages or poll preparation state.
- **Target API & behavior**:
  - Add additive lightweight notification callbacks (matching existing `SetOnTextChanged` / `SetOnSelectionChanged` lifetime semantics with weak/token safety) invoked once immutable `SemanticDocument` or `MarkdownSyntax` preparation publishes a new revision on the owner thread.

### `DX-SPELL-8` — Familiar smooth wavy squiggle underline rendering (`RichTextDecorationKind::Squiggle`)

- **Current anchor**: `RichTextDecorationKind::Squiggle` branch in [src/Controls/DxUi.RichTextEditor.cpp](../../../src/Controls/DxUi.RichTextEditor.cpp#L5509-L5520).
- **Current defect**: The loop renders disconnected upward diagonal slash segments (`DrawLine({x, bounds.bottom - 1.0f}, {std::min(last, x + step), bounds.bottom - 3.0f})`), which visually reads as diagonal hatching rather than a familiar continuous proofing wave.
- **Target rendering contract**:
  - Render `RichTextDecorationKind::Squiggle` as a continuous, symmetric sine-like wave (`~1.5–2.0` DIP peak-to-peak amplitude, `~4.0–5.0` DIP full period/wavelength, `~1.1–1.25` DIP stroke width with rounded caps/joins) positioned cleanly beneath the run's baseline/descenders and clipped symmetrically to `[first, last]` along the viewport.
  - Use connected periodic quadratic/cubic Bezier segments or a cached/pooled `ID2D1PathGeometry` / stroke style (`ID2D1StrokeStyle` with `D2D1_CAP_STYLE_ROUND`, `D2D1_LINE_JOIN_ROUND`) prepared cleanly with **zero steady-state heap or D2D factory allocations during composition/paint** and crisp pixel snapping across 100%–200% DPI scales and Light/Dark/Paper/High-Contrast themes.

---

## 3. Phased execution plan (future `DxUi` feature wave)

| Slice | Scope | Primary Files | Qualification Required |
| --- | --- | --- | --- |
| **Slice P1 — Pure RichText & Markdown helpers (`DX-SPELL-1`, `DX-SPELL-5`, `DX-SPELL-6`)** | Implement semantic/syntax prose-segment visitors, display-range multi-fragment `SourceEdit` planner, and `TryGetFrontMatterScalar` (reusing `libfyaml`/`toml++` parse artifacts). | `include/DxUi/RichSemanticDocument.h`, `include/DxUi/RichSemantic.h`, `include/DxUi/MarkdownSyntax.h`, `src/Controls/DxUi.RichSemanticDocument.cpp`, `src/Controls/DxUi.RichSemantic.cpp`, `Tests/Controls/` | Unit tests for contractions, elisions, surrogate boundaries, mid-word `**re**commnd` round-trip, YAML/TOML scalar extraction, malformed front-matter refusal; `validate.ps1`, zero warnings across 6 profiles. |
| **Slice P2 — Layered viewport decorations, smooth squiggle & suppression (`DX-SPELL-2`, `DX-SPELL-4`, `DX-SPELL-8`)** | Add `RichTextDecorationLayer`, `4,096`-entry per-layer storage with viewport-clipped rectangle mapping, per-layer UIA annotation diffing, `SetDecorationSuppressionRange`, and smooth continuous Bezier wave rendering for `RichTextDecorationKind::Squiggle`. | `include/DxUi/DxUi.h`, `src/Controls/DxUi.RichTextEditor.cpp`, `Tests/Controls/` | `4,096`-decoration scaling + viewport-scroll tests, `E_BOUNDS` rectangle-budget tests, UIA `SpellingError` vs `GrammarError` retention tests, WARP smooth-wave pixel/stroke verification across DPI/themes, zero-alloc paint check, and clean/dirty/hidden resource & perf measurements. |
| **Slice P3 — Rich context menu, publication hook & API bump (`DX-SPELL-3`, `DX-SPELL-7`)** | Add `SetOnRichContextMenu`, `SetOnSemanticPublished`/`SetOnSyntaxPrepared`, bump `DXUI_API_REVISION` cleanly (coordinate with `A8`), update `docs/` and `Specs/UI/UI_ControlsAndLayout.md`, `Specs/UI/UI_InputAndAccessibility.md`. | `include/DxUi/DxUi.h`, `src/Controls/DxUi.RichTextEditor.cpp`, `Specs/`, `docs/`, `Tests/Controls/` | Pointer & `Shift+F10` context-menu offset tests, callback reentrancy/lifetime teardown tests, gallery/docs review (`gallery.ps1 -PublishDocs`), full 6-profile build + x64 Debug/Release/ASan runtime suites. |

---

## 4. Consumer handoff criteria

When `P1`–`P3` land and pass DxUi qualification on `main`:

1. Record the new `DXUI_API_REVISION` and qualified commit SHA.
2. Consumer adoption (`RedTilde/Specs/Plans/WIP/DxUiProofingSimplification_2026-11-24.md`) updates `Dependencies/DxUi.lock.json` cleanly in its own PR while deleting RedTilde's superseded `ProseExtractor`, viewport decoration slicing, scroll re-projection, `immediatePending`, and coordinate hit-test adapter code in the same change.
