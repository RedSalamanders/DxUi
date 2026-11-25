# W4.10 paint regions and culling

Plan item `W4.10` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md), the
mechanism of wave W4 after the [W4.0](../W4.0/README.md) no-go. The contract is
[performance and resources, "Paint region"](../../../Specs/Core/Core_PerformanceAndResources.md#paint-region) and
[controls and layout, "Painted extent"](../../../Specs/UI/UI_ControlsAndLayout.md#painted-extent).

- Source: the `(W4.10)` commit on branch `pm/w4-10-regions-culling`, first built on integration tip `e2d33425c`; the gates and the
  review section below were rerun on `02063cb85` after the review's fixes. Round 3 (the frame verifier and the second
  review's fixes, branch `pm/w4-10-round3`) was built and run on integration tip `9c442ed25`; the round 2 receipts
  (`gates-warp-debug.txt`, the culling benchmark) were not regenerated.
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
`Slider` and `ColorPicker`, which declare what they draw past their bounds, and (round 3) a `Button` of variant `Selector`,
which declares 4.5 DIP (its ring drawn 5.4 DIP out in whole pixels at 96 DPI, 4.6 at 168).

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

## Review (round 3)

A second review confirmed a new list of class-specific defects with probes, which showed that the tests did not find what
exists. This round first built a frame verifier, then fixed what the review and the verifier found. Correct means: a
frame painted with regions and culling equals, pixel for pixel on WARP, the same scene painted whole with both off.

The frame verifier ([Testing_Validation.md, "Frame verifier"](../../../Specs/Testing/Testing_Validation.md#frame-verifier)) is
on in every suite of the Debug and ASan Debug test programs. Every embedded `Prepare` that painted a region, culled a
child or skipped a viewport clip is followed by a whole repaint of the same scene, at the same time, with regions and
culling off, into a scratch surface. The two are compared exactly. The incremental-versus-fresh oracle now paints its fresh
scene with regions and culling off on both hosts (finding H1). Its first version swapped the host's brush and format caches
out for the reference paint. The verifier then reported its own artifacts, which were not defects:
- a brush key draws the color it was first asked for, so a fresh brush differed by one level in a channel;
- a fresh format made controls lay out again (12 text layouts in `TestFx5RegionFramesKeepCaches`).

Both caches are now shared, and only their ages are restored.

Cost: the whole Embedded program (x64 Debug, WARP, `DxUi.EmbeddedTests.exe`, runs alternated on the same build and
machine, other workers building) took 655 s and 818 s with `DXUI_FRAME_VERIFY=0` and 962 s and 1,035 s with the verifier,
which checked 12,753 frames: about +36 % (+262 s), about 20 ms per verified frame. In the window-host suites the verifier
checks nothing, so they cost nothing more; there, culling is checked by the oracle's fresh scene, which paints unculled, in
every native oracle script.

| Finding | Cause | Fix | Test (fails before) |
| --- | --- | --- | --- |
| B1 `TagPicker` inner hover | `TagPicker::Paint` draws its field's frame from `_combo->IsHovered()`; a hover change added only the old and new hovered control | a library control whose `Paint` reads a hover below it is flagged (`kPaintsInnerHover`, `TagPicker` only); every hover change, the host's and a `ScrollPanel`'s inner one, adds such ancestors (`AddHoverChange`) | `tagpicker-hover` and `scrolled-tagpicker-hover` scripts in `TestReviewScripts` (probe: 18,156 pixels) |
| B3 overlay content in a `ScrollPanel` | a `ScrollPanel`'s extent left out its children, and its overlay pass moves a `PopupLayer`'s children by the offset with no clip | a panel's cached extent records whether its subtree holds overlay content; a `ScrollPanel`'s extent covers its children that hold some, moved by the offset; an offset change adds that content at the old and the new offset (`ScrollMoved`); a panel holding overlay content maps to the view unclipped by viewports | `scrolled-popup-layer` (wheel, `SetScrollOffset`, hide and show; probe: 9,549 pixels) |
| E1 `Selector` focus ring | `DrawRawFocusRing` scales offset and stroke by radius/4; a Selector's radius is 6, so its ring reaches 4.5 DIP against a declared 3 | `GetPaintOverhang` declares 4.5 DIP for a Selector; `SetVariant` adds the old extent first | `TestButtonVariantReach` (8 variants x 6 appearances x 4 states x 3 DPIs), `selector-neighbour`, `selector-off-view` (probe: 12 of 288 cases, 56 pixels) |
| E2 `TabControl` ring outside the control | the ring of the selected tab is drawn after the header clip is popped, and the tab can lie far outside the control | the ring is clipped (aliased) to the control's bounds plus 3 DIP, snapped outward to whole device pixels: a **pixel change**, named in the changelog | `TestTabControlFocusReach` (every selection of 8 tabs in 160 DIP, of 3, one long title), `tabcontrol-ring` (probe: ink 140 DIP left, 280 pixels) |
| A1 fractional DPI | the embedded host stored `static_cast<UINT>(dpi)`, which the region and the clip test used, while `Prepare` painted with the float | `ControlHost::_paintDpi`, the float DPI of the view, for every DIP-to-pixel conversion of the region, the clip test and the snapping (`PixelScale`) | `TestFractionalDpiRegions` at 120.75, 150.5 and 144 DPI (probe: 414 and 376 pixels) |
| D1 nested fractional viewports | `ClipEnclosedBy` rounded the paint clip to pixels, assuming it whole; after an outer antialiased viewport clip it is not, and skipping the inner clip doubled the edge coverage, **in whole frames too** | the inner clip is skipped only while the paint clip lies on whole device pixels; today's regions-off pixels are the pre-`W4.10` pixels (both clips pushed), so nothing changes against either | `TestWholeFramesEqualRegionsOff` (96, 168, 150.5 DPI; probe: 290 pixels) |
| F1 undeclared overlay composer | the painting scope that lifts culling below an undeclared control covered `Paint` only; the overlay pass culled a `PopupLayer`'s children against the view | `OverlayScope` around every `PaintOverlay` (both hosts' roots, `Panel`, `PageHost`) | `TestWholeFramesEqualRegionsOff`, "F1 undeclared overlay composer" (probe: 5,756 pixels) |
| H1 oracle blind to whole-frame culling | both sides of `RunParityScript` culled | the fresh side paints with regions and culling off (both hosts), and the frame verifier covers every embedded test | `selector-off-view` (probe: 0 differences before the oracle change, 30 pixels in a whole frame) |
| V1 content preparation (verifier) | a control's prepared content (an adopted parsed rich document) is painted next, but `PreparePendingPageLayout` added nothing to the region; the preparation painted an empty region and kept the old pixels | a control whose `PrepareContent` returns `S_OK`, or whose `PrepareVisualContent` ran, adds its extent before the region is taken | the frame verifier in `TestRichTextAccessibilityEmbeddedReadOnlyParsedSnapshotAdoption` (Accessibility suite: 3 frames, 1,344 pixels) |
| X1 undeclared `PageHost` page (found while extending F1) | `PageHost` painted its pages with no painting scope, so a page that is an undeclared consumer panel had its children culled | the pages paint under `PaintingScope` and `OverlayScope` | script `page-host-culling` on both hosts, `TestPageHostCulling` (embedded) and `TestNativeIncrementalParityPageHostCulling` (window host): an undeclared page drawing its children 200 DIP lower, a declared page's child whose reach meets the view, a transition frozen half way (`review/round3-x1-before.log`: 4,476 pixels on each host with the fix removed) |
| O1 native oracle hover race (the conductor's verification of `68a66f3bb`, Debug, about 87 % processor load: `interaction` seed `0x5EED00B1` step 28, a `Tree` row hovered in the incremental scene only) | an oracle defect, not a product one: a synthesized `WM_MOUSEMOVE` makes the host call `TrackMouseEvent(TME_LEAVE)`; the real cursor is never over the off-screen parity window, so Windows posts `WM_MOUSELEAVE` to the thread's queue, where it was still queued when the step returned. A capture's pump stops at its 50 ms bound, so under load the incremental scene could be captured before its leave was dispatched, while the fresh scene's later pump dispatched both. Window-host paint is whole frames here; neither the region nor the pacer is involved, and the frame verifier, which checks embedded preparations only, has nothing to see | the native parity scene (and the fixture port) dispatches its window's queued `WM_MOUSELEAVE` before the pointer step returns, so each scene ends every step in the same state whatever the load. The comparison itself is unchanged | `TestNativeParityPointerInputCompletesInItsStep` (`review/round3-oracle-leave-before.log`: the leave still queued and the hover still set when `Pointer` returned). The original difference did not reappear in replays of the seed under load: 20 on `bbaa14933`, and 30 on `aaad7e45e` (W3.6 round 2) with the fix taken out. The capture (`DebugCaptureBitmap`) renders the current state itself, without a credit or a paced frame, so a difference in its pixels is a difference in state, here the hover, and only the order of the queued leave and the capture decides that |
| Leftover | `TestEnclosedClipIsNotPushed` bounded `paintExecutions <= 5` | the exact count | itself |

The B1 sweep read every library `Paint`, `PaintOverlay` and `PrepareVisualContent`:
- `TagPicker` reads its inner `ComboBox`'s hover (fixed above), its focus and whether its popup is open. A focus change
  repaints the whole view (`SetFocusControl`). An open popup makes the extent unbounded, and closing it marks the region
  full.
- `NumericStepper` places its unit by its field's bounds, which only its own layout moves after its own invalidation.
- `ComboBox::PaintOverlay` places its popup by an ancestor `ScrollPanel`, which happens only while the popup is open and
  the extent is unbounded.
- `ColorPicker` mirrors its child fields' values; each child change invalidates the picker.
- `Button` compares itself with the host's default button; changing that repaints the whole view.

No library paint reads another control's pressed or capture state.

States covered beyond the review:
- `TestReachMidAnimation` samples every library control kind with motion on, 16, 48, 96, 160 and 400 ms after each
  transition (hover in and out, press, release, focus visible, focus leaving). It runs at 96 and 144 DPI, in left-to-right
  and right-to-left flow, and again in high contrast: 6,870 samples, every one within its extent.
- `uncovered-states` runs as region frames under the verifier, with and without high contrast. It covers a right-to-left
  wrapping `FlexPanel`, a `FormPanel`, an emoji label, a hyperlink's underline, typing into a field and clock advances
  mid-animation.
- The IME composition underline is drawn in the embedded text-input tests, which run under the verifier.

The breaks of round 2 against this commit, one at a time, `--test-paint-regions` with the verifier on and then with
`DXUI_FRAME_VERIFY=0`:
- break 1 (reach dropped from the region): with the verifier, it fails the first test, `TestDeclaredReachCoversEveryPixel`,
  at its first region frame (124 pixels). Without the verifier, `TestEnclosedClipIsNotPushed` fails on its exact paint count.
- break 2 (cull with the extent one pixel short): with or without the verifier, `TestRegionEdgeScripts` fails, script
  `region-edges` step 2 (40 pixels).
- break 3 (a moved control's old extent not added): with or without the verifier, `TestRegionEdgeScripts` fails, step 6
  (1,182 pixels).

The logs are in `review/round3-break*.log`. The review's probes on the base commit are in `review/round3-probes-before.log`,
and the verifier's first finding is in `review/round3-verifier-accessibility.log`.

## Commands

```powershell
.\.build\x64\Debug\DxUi.EmbeddedTests.exe --test-paint-regions
.\.build\x64\Debug\DxUi.ControlTests.exe --suite=Rendering --test=TestNativeIncrementalParityPaintRegions
.\.build\x64\Release\DxUi.EmbeddedTests.exe --paint-region-cull-bench 400 Warp
# Same-binary comparison for G4 (hardware, quiet machine; not run here):
./performance-timing.ps1 -Purpose Comparison -Fixture FX5 -Phase Dirty -BaselineExecutable .build/x64/Release/DxUi.EmbeddedTests.exe `
    -CandidateEnvironment DXUI_PAINT_REGIONS=0 -SkipBuild
```
