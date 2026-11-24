# W4.10 paint regions and culling

Plan item `W4.10` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md), the
mechanism of wave W4 after the [W4.0](../W4.0/README.md) no-go. The contract is
[performance and resources, "Paint region"](../../../Specs/Core/Core_PerformanceAndResources.md#paint-region) and
[controls and layout, "Painted extent"](../../../Specs/UI/UI_ControlsAndLayout.md#painted-extent).

- Source: the `(W4.10)` commit on branch `pm/w4-10-regions-culling`, first built on integration tip `e2d33425c`; the gates and the
  review section below were rerun on `02063cb85` after the review's fixes.
- Builds: x64 Debug (counters, pixels, gates), x64 Release (the culling benchmark); MSVC 14.52.37025; Windows 11 Pro
  10.0.26300.
- Adapters: WARP (Microsoft Basic Render Driver) for every gate. The benchmark also ran on the NVIDIA GeForce RTX 5080 as a
  **smoke** run while other workers built on the machine: its numbers are shown, never judged.
- Files: `gates-warp-debug.txt` (the output of `DxUi.EmbeddedTests.exe --test-paint-regions` inside the whole embedded
  suite, Debug, WARP: every gate's counters and the reach each library control kind drew in each state at 96, 120, 144, 168
  and 192 DPI), `review/` (the independent review's confirmed defects and deliberate product breaks: the new tests failing
  before the fix), `cull-bench-warp.jsonl` and `cull-bench-hardware.jsonl` (`--paint-region-cull-bench 400 Warp|Hardware`,
  Release).

## Gates

| Clause | Result | Evidence |
| --- | --- | --- |
| `FX5` hover: `Paint` executions <= 2 (`G2`, literal) | **Not met**: 8 per hover frame (4 in the first, which changes one toggle). Repainting a region clears it, so the card under each toggle and the root must paint there, and the toggle's 3 DIP reach (its focus ring, drawn 2 DIP below it) meets the slider just below it. The restated clause (changed controls, their ancestors and the neighbours their reach meets, under each rectangle) is met exactly: per rectangle the root, the card, the toggle and the slider, nothing else; 19 children culled per rectangle. | `TestFx5HoverRegionGate`; `gates-warp-debug.txt` |
| `FX12` scrolled: child paints <= visible children + one row | **Met**: form-10k paints at most 175 children for 175 in the viewport, form-layout-10k at most 182 for 182 (of 10,000 controls) | `TestFx12ScrollPaintsVisibleChildren` |
| Enclosed clips emit 0 inner `PushAxisAlignedClip` | **Met**: a region inside a `ScrollPanel`'s viewport skips the viewport clip (`clipPushesSkipped` 1, the only clip a library panel pushes) | `TestEnclosedClipIsNotPushed` |
| `W0.5` oracle 0 differences after every partial frame | **Met** on the embedded host (every incremental frame after the first repaints its region; 118 rectangles, 8,307 culled children and 10 ink escapes compared in the regions scripts) and on the window host (whole frames with culling); the shared layout, interaction and rich scripts now also run partial embedded frames | `TestRegionParityScripts`, `TestNativeIncrementalParityPaintRegions`, `TestEmbeddedIncrementalParityScripts` |
| `G4` micro-change frame CPU <= 1 ms at 4K, hardware | **Open**: not judged here (hardware, Release, quiet machine) | |

Hover-frame C++ allocations on FX5 (`G3`, 54 in the plan's baseline): 2 per frame with regions, 25 with regions off, in the
same binary (Debug).

## Culling cost

`--paint-region-cull-bench`: a 1920x1080 view whose one changing Label is repainted every frame, with 10,000 other Labels
outside the region (mode `cull`) or none (mode `empty`); median UI-thread cycles of `Prepare` over 400 frames.

| Adapter | cull | empty | Per culled child |
| --- | --- | --- | --- |
| WARP | 1,369,148 | 221,467 | 115 cycles |
| Hardware (smoke) | 1,190,601 | 129,618 | 106 cycles |

The W4.0 spike's skip cost 453 to 549 cycles per control with its bookkeeping; a culled child here costs its visit, its
extent (the virtual reach) and one rectangle test. A flat panel's per-rectangle traversal is linear in its children; no
spatial index is built.

## Reach of the library controls

`TestDeclaredReachCoversEveryPixel` paints every library control class alone on a transparent surface (45 cases of 35
classes: each class at its usual size and, where its reach depends on it, at a size that maximises it: a 16-page
`PageIndicator` in 120 and in 60 DIP, a 12 DIP `Slider` with ticks at value 100, a 10 DIP vertical one, short `Button`,
`Toggle`, `Checkbox`, `TextField`, a 200 x 160 `ColorPicker`), normal, hovered, pressed, focused with focus visible,
after the focus ring left, and disabled, at 96, 120, 144, 168 and 192 DPI, and requires every inked pixel inside the
extent a panel culls it by (bounds plus reach, with its children for a panel), snapped outward: 1,160 cases checked, 190
skipped because the extent is unbounded in them (an open `ComboBox` popup, text that escaped its box). Not covered:
`TooltipLayer` (the host's own layer, never in a tree; every tooltip change repaints the whole view) and `PageHost`
during a transition (unbounded by declaration).

Largest reach drawn (DIP beyond the bounds): `Button` and `Splitter` 3.7 (focus ring), `Slider` 3.5, at 12 DIP tall 4.6
and at 10 DIP wide 5.7 (hovered thumb and ticks), `Toggle`, `Checkbox`, `RadioButton` 2.7, `PageIndicator` with 16 pages
55.7 in 120 DIP and 85.4 in 60 DIP; the others 1.4 or less. Every library control declares 3 DIP except `PageIndicator`,
`Slider` and `ColorPicker`, which declare what they draw past their bounds.

A `ColorPicker` smaller than its parts (200 x 160 DIP): its field, strip and swatches are declared; the text of its R, G, B
and hex rows overflows their boxes and is caught by ink-escape detection, so every change repaints the whole view: in the
oracle runs at 96 and 168 DPI (2 seeds, 8 steps each) 70 whole repaints and 0 region rectangles. A known cost, not a
stale pixel.

## Review (round 2)

An independent review of the first commit confirmed three defects that left stale pixels and a hole in the tests. Each
fix has a test that fails without it (files in `review/`):

| Finding | Cause | Fix | Test (fails before) |
| --- | --- | --- | --- |
| F1 `PageIndicator` overflow | the dots are centred on the bounds without clamping; 3 DIP were declared | `PageIndicator::GetPaintOverhang` declares the row as drawn (today's pixels kept); `SetPageCount` adds the old extent before the change | `TestDeclaredReachCoversEveryPixel` (`prefix-verifier.log`: ink 49.7 DIP past the extent), `overfull-page-indicator` oracle (`prefix-edges.log`: 106 pixels) |
| F2 `Slider` thumb | the hovered (20 DIP) and pressed thumb and the ticks are not bounded by the cross size | `Slider::GetPaintOverhang` declares what passes a short slider's edges; `SetOrientation` adds the old extent | `thin-slider` oracle (`prefix-slider.log`: 20 pixels) |
| F3 undeclared panels | `Panel::Paint` culled the children of an undeclared panel against the frame's clip, though it may paint them elsewhere | the painting scope of an undeclared control makes the clip unbounded below it | `TestUndeclaredComposingPanelsCullNothing` (`prefix-composing.log`: 1 culled, 4,476 pixels) |
| F4 break 2 (extent short by one pixel) | not caught | `region-edges` oracle (an EdgeInk's last pixel column under a one-column region, 96, 120, 168 DPI) and `TestClipMissAtTheExtentEdge` | `break2-run.log` |
| F4 break 1 (reach dropped from the region) | caught only at one random seed | `region-edges` (a declared reach taken away) | `break1-run.log` |
| F4 break 3 (old extent not added on a move) | caught only at one random seed | `region-edges` (a Button moves 40 DIP) | `break3-run.log` |
## Commands

```powershell
.\.build\x64\Debug\DxUi.EmbeddedTests.exe --test-paint-regions
.\.build\x64\Debug\DxUi.ControlTests.exe --suite=Rendering --test=TestNativeIncrementalParityPaintRegions
.\.build\x64\Release\DxUi.EmbeddedTests.exe --paint-region-cull-bench 400 Warp
# Same-binary comparison for G4 (hardware, quiet machine; not run here):
./performance-timing.ps1 -Purpose Comparison -Fixture FX5 -Phase Dirty -BaselineExecutable .build/x64/Release/DxUi.EmbeddedTests.exe `
    -CandidateEnvironment DXUI_PAINT_REGIONS=0 -SkipBuild
```
