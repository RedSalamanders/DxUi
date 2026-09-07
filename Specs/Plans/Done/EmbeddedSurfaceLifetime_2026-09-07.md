# Embedded surface lifetime, tick-driven dirtying and bounded caches

Status: COMPLETE (2026-09-07)

Owning contracts: [embedded hosting](../../Rendering/Rendering_EmbeddedD3D11.md),
[performance](../../Core/Core_PerformanceAndResources.md), [validation](../../Testing/Testing_Validation.md).

Goal: a collapsed or hidden RedXe AV widget kept its ~3.5 MiB raised-overlay surface alive for the widget's
lifetime, every host tick re-rasterized the whole surface even when no control changed, and the per-host brush and
text-format caches grew without bound. This round makes hidden/zero-extent views hold no surface, makes ticks dirty a
view only through control invalidation, bounds both caches, and gates whole-frame C++ allocations in the benchmark.

- [x] `EmbeddedHost::ReleaseSurface` (`src/Rendering/Embedded.cpp`): `SetVisible(false)`, a zero-extent `Prepare`
  and `ReplaceDevice` drop the D2D target, bitmap, shader view and texture, report `surfaceBytes` 0 and cancel
  pointer/text input like the previous hide path. The next visible sized preparation recreates the surface through
  the existing allocation branch and counts exactly one `surfaceAllocations` increment.
- [x] `AdvanceAnimation` no longer calls `MarkDirty()`. Every `Tick` that changes visual state invalidates its host:
  `TextField::Tick` / `ComboBox::Tick` (caret blink flips only), `Button::Tick` (interaction progress moved),
  `PageHost::Tick` (every transition tick including the settling one), `TooltipLayer::Tick` (show/hide),
  `Grid::Tick` (spinners, busy header, sort glyph including its settling tick, scrollbars), `Tree::Tick` (expander,
  expansion and scrollbar transitions) and the menu popup content control (slider animation). `ProgressBar::Tick`
  and `ThroughputGraph::Tick` already invalidated; `Panel::Tick` and the default `Control::Tick` change nothing.
  `ControlHost::RequestAnimation` (`src/Controls/DxUi.WindowHost.cpp`) invalidates an embedded view only on the
  false-to-true transition: previously an indeterminate progress bar requesting animation from `Paint` re-dirtied
  the view inside every preparation, which the unconditional tick dirtying had masked.
- [x] `ControlHost::TrimCaches` (`src/Controls/DxUi.WindowHost.cpp`) clears the solid-brush cache above
  `kSolidBrushCacheLimit` (256) and the configured-text-format cache above `kConfiguredTextFormatCacheLimit` (96)
  at the start of `EmbeddedHost::Prepare` (before `BeginDraw`) and of `ControlHost::Render`, never mid-paint.
  `EmbeddedStatistics` gains `cachedBrushes` and `cachedTextFormats` (`include/DxUi/Embedded.h`).
- [x] `Tests/Embedded/ComplexUiBenchmark.h` fails a clean round with any C++ allocation and a dirty round above
  64 allocations per frame in Release or 320 in Debug (`dirtyAllocationCeilingPerFrame` in the receipt).
- [x] `Tests/Embedded/EmbeddedTests.cpp`: surface release on hide and zero extent with exactly one reallocation and
  pixel-identical restoration, device replacement while hidden, idle-tick / caret-phase / indeterminate-progress
  dirtying, brush and text-format cache bounds; the existing zero-sized deferral and hidden-animation checks stay.
- [x] Normative text, `docs/hosting.md`, `docs/performance.md` and the changelog updated; `capabilities.json` stays
  at API revision 2 with additive changes only.

## Validation receipts

`format.ps1 -Check`, `validate-specs.ps1`, `validate-skills.ps1` and `validate-dependencies.ps1` pass. x64 Debug and
Release `test.ps1` runs pass all 18 suites (Embedded: 2,161 checks). Menu passed with eight interactive-desktop
capability skips (native menu-bar focus dismissal/restoration, owner-message-flood routing, split-button routing,
destruction proof); earlier fully interactive Menu runs failed rotating real-window cases (light-dismiss pointer-up,
stationary-mouse hover, popup capture readiness, debug-state timeout, large-menu cached offsets) for the baseline
binary as well as the candidate while the desktop was in use, so those cases need a quiet interactive desktop and are
not evidence against this change. ARM64 Debug and Release cross-build; native ARM64 execution stays with CI.

Performance evidence is retained in [Measurements/ComplexUi/2026-09-07](../../../Measurements/ComplexUi/2026-09-07/README.md).
The baseline was measured with the final harness on the previous implementation (commit 7b571b73, fixture
`9DCFBC39…`). Five-round medians:

| Run | Scenario | FPS | Frame p95 | Prepare p95 | C++ allocations | Surface / peak bytes | Private bytes |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline Release | clean | 2,288.8 | 0.481 ms | 0.0003 ms | 0 | 3,686,400 / 3,686,400 | 26,316,800 |
| Baseline Release | dirty | 487.0 | 2.257 ms | 1.617 ms | 2,160 | 3,686,400 / 3,686,400 | 27,930,624 |
| Candidate Release | clean | 2,230.7 | 0.566 ms | 0.0003 ms | 0 | 3,686,400 / 3,686,400 | 26,779,648 |
| Candidate Release | dirty | 498.6 | 2.294 ms | 1.602 ms | 2,160 | 3,686,400 / 3,686,400 | 27,545,600 |
| Baseline Debug | clean | 2,302.0 | 0.539 ms | 0.001 ms | 0 | 3,686,400 / 3,686,400 | 29,908,992 |
| Baseline Debug | dirty | 407.1 | 2.896 ms | 2.037 ms | 10,280 | 3,686,400 / 3,686,400 | 31,522,816 |
| Candidate Debug | clean | 2,173.4 | 0.562 ms | 0.0009 ms | 0 | 3,686,400 / 3,686,400 | 28,786,688 |
| Candidate Debug | dirty | 403.8 | 2.831 ms | 2.074 ms | 10,280 | 3,686,400 / 3,686,400 | 29,761,536 |

The automated paired comparison flagged `advice-required` in every attempt on rotating sub-band metrics (1-2 µs of
composition CPU, 0.03 ms of frame p50/p95, 1-1.2 MB of private bytes) while the desktop was actively in use; one
attempt showed a 3x slowdown that reproduced with the baseline binary. Alternating same-state runs of both binaries
(four Release pairs, four Debug pairs plus three reversed-order Debug pairs) show no direction: Release clean
2,190 vs 2,228 FPS and dirty 486 vs 497 FPS means, Debug clean 2,200 vs 2,253 and dirty 412 vs 399 then 401 vs 401
reversed, with identical allocation counts and surface bytes in every run. The suite receipts are therefore
`unpaired`; a `within-noise-budget` receipt requires re-running
`test.ps1 -PerformanceBaseline` against the retained baseline on a quiet fixture. No threshold was relaxed and no
baseline replaced.

## Remaining gates

| Gate | Owner / note |
| --- | --- |
| Paired performance receipt on a quiet fixture | The retained baseline and candidate receipts pair by fixture hash; the automated 5%/2% bands need an idle interactive desktop. Re-run `test.ps1 -Configuration <c> -Platform x64 -PerformanceBaseline Measurements/ComplexUi/2026-09-07/baseline-<c>.json` when the machine is quiet. |
| Per-tick delay hint | Hosts still tick at their own cadence while `NeedsAnimation()` is true; a caret-only view could tell the host to wait until the next blink flip. A `NextTickDelayMs` style hint is a later additive API, not part of this round. |
| Consumer scheduling | RedXe should check `NeedsPreparation()` after `AdvanceAnimation` instead of preparing unconditionally, hide collapsed views to reclaim their surface, and read `cachedBrushes`/`cachedTextFormats` in its adoption receipts. Its matched AV measurements remain consumer evidence. |
| Native host ticks | `ControlHost::OnAnimationTick` still invalidates its HWND on every ticking frame; control-level invalidation now makes that redundant rather than wrong. Removing it is a separate native change with its own baselines. |
| Pipeline references | `ReleaseSurface` drops every host-owned reference and the D2D target. A binding left on the application's immediate context is replaced by the application's next bind; the release is not deferred by the library. |
