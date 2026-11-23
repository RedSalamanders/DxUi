# W4.0 retained command list spike: no-go

Plan item `W4.0` of [the performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md): record
`Label`, `Button` and `CardPanel` `Paint` into an `ID2D1CommandList` during preparation, replay it, and judge four go
criteria. **Decision (the developer's, 23 November 2026): no-go; "proceed with W4.10".** Criterion 3 is not met: on
hardware, replaying a retained control costs 0.85 to 1.07 of painting it, against at most 0.5. `W4.1` to `W4.9` are not
built as command-list retention; `W4.10` reuses the parts of them it names (region invalidation as in `W4.6` with per-state
overhang declared by each control, raster tiles as in `W4.3` for effect-heavy controls only, then `W4.7` and `W4.9` on that
region). The contract states the outcome in
[performance and resources](../../../Specs/Core/Core_PerformanceAndResources.md#retained-drawing).

- Prototype: branch `pm/w4-0-spike-code`, commit `e5816e537` (on integration tip `55401884b`, which has `W2.1`, `W2.3`,
  `W2.8` and `W0.2` with its interim admission rule), behind a private switch that is off by default. It does not land.
- Build: x64 Debug for counters and pixels, x64 Release for timing; MSVC 14.52.37025; Windows 11 Pro 10.0.26300.
- Adapters: WARP (Microsoft Basic Render Driver) for counters and pixels. Hardware: NVIDIA GeForce RTX 5080, driver
  32.0.16.1714, High performance scheme. Hardware results are labelled **quiet slot** (23 November 2026, no builds on the
  machine, run by the conductor from the attested Release build of `e5816e537`) or **smoke** (15 November, short runs while
  other builds ran; shown only for comparison, never judged).
- Files here: `spike-warp.jsonl` (`DxUi.EmbeddedTests.exe --test-retained-paint-spike <dir>`: the Direct2D experiments, list
  shapes, FX5 counters and memory), `bench-hardware.jsonl` (quiet slot), `bench-hardware-smoke.jsonl` and `bench-warp.jsonl`
  (per-control benchmark, `--retained-paint-spike-bench`), and `timing-FX5-*/` (frame timing comparisons, quiet slot; each
  with the README `performance-timing.ps1` wrote).

## The four criteria

| Criterion | Result | Evidence |
| --- | --- | --- |
| 1. `Paint` executions per dirty frame equal the changed controls | **Met** for every retained kind on FX5 (0 of the retained kinds paint on an unchanged frame; with every leaf kind retained, a dirty frame paints only the 32 changed controls plus the 3 kinds outside the spike) | [Criterion 1](#criterion-1-paint-executions-on-fx5-warp-counters) |
| 2. `W0.5` oracle 0 BGRA differences at 96, 144 and 192 DPI | **Met** on the embedded host with `DrawImage` replay; on the window host only with stream replay, because `DrawImage` replay draws ClearType text grayscale (first frame fails). One shared window-host script fails identically with the spike off (harness, see below) | [Criteria 2 and 4](#criteria-2-and-4-the-oracle-warp) |
| 3. Replay costs at most half the CPU of painting it, hardware, after `W2.1` and `W2.3` | **Not met**: 0.859 (`Label`), 0.853 (`Button`), 1.073 (`CardPanel`); FX5 frame CPU is not lower (0x7: inconclusive, +1.8%) or is slower (0x3F: +8.0% at 1080p, +7.8% at 4K) | [Criterion 3](#criterion-3-cpu-of-replay-against-paint) |
| 4. Text position identical after a whole-pixel translation | **Met** on both hosts at the three DPIs, and in a direct `DrawTextLayout` experiment | [Criteria 2 and 4](#criteria-2-and-4-the-oracle-warp) |

## The mechanism prototyped

- **Where it records.** `Panel::Paint` offers each visible child to the spike before running its `Paint`. A child whose
  exact type is `Label`, `Button` or `CardPanel` (never a derived class) is recorded into an `ID2D1CommandList` on a
  second device context the host owns on the same Direct2D device, set to the drawing context's world transform, DPI,
  antialias mode, text antialias mode, text rendering parameters, primitive blend and unit mode; while it records,
  `ControlHost::GetDeviceContext()` returns that context, so `Paint` runs unchanged. The list is drawn at once, and
  kept. Recording therefore happens inside the paint pass: on the embedded host that is `Prepare` (preparation), on the
  window host it is the render pass. Composition is untouched: it still only draws the prepared surface.
- **What makes a list stale (the spike's rule).** A list is replayed only while (a) no invalidation was attributed to its
  control since it was recorded (`Control::RequestInvalidate`, `Control::Invalidate(host)` and `InvalidateMeasure` mark
  it; a `SetBounds` that keeps the size does not), (b) its key matches: size, the transform's linear part, hover,
  pressed (buttons), focus, enabled, keyboard-focus-visible, default button, density and flow direction, (c) the host
  epoch matches, read once per host frame: a theme epoch bumped by `SetTheme`, the resource generation (device loss and
  replacement), the typography revision, the DPI, the text rendering parameters and both antialias modes, and the icon
  font's presence, (d) the control holds no frame-schedule request (an animation in progress re-records every frame), and
  (e) its origin moved by whole device pixels only, which replays it translated. Anything else records again. This is a
  spike's rule: a key of observed state cannot see private state a control changes without invalidating (the oracle found
  none in the spike's kinds, but nothing guarantees it), so a real design would need the paint revision the plan's `W4.1`
  named, advanced by the control itself.
- **Nesting, clips, layers, transforms, opacity, antialiasing.** A container records only its own drawing: while
  `CardPanel` records, `Panel::Paint` suppresses its children; after its list replays, its children are painted (each
  offered to the spike again), so one changed child never re-records its card. Clips and layers a control pushes are
  recorded in its list; the parent's clip is the drawing context's, which applies to the replay as to a paint. Transforms
  recorded in the list compose with the replay transform (a translation, so whole-pixel moves need no re-recording).
  Antialias modes are recorded as state commands. `DrawButtonChrome` sets one scratch brush's colour several times within a
  paint; a command list keeps **brushes by value** (the experiment below), so that is safe, and `W2.8`'s rule that retained
  drawing uses keyed brushes is not needed for command lists (it still holds for any object kept by reference).
- **Host state a `Paint` reads.** Hover, pressed, focus, focus visibility, default button, theme, DPI, text rendering
  parameters and typography are in the key or the epoch above; animation progress is covered by (d).
- **Device loss.** Lists and the recording context are device resources: a new resource generation or a different
  `ID2D1Device` drops every list of the host. `DebugDropRetainedStateForTest` drops them too, and the oracle's fresh scene
  paints with the spike bypassed (`IsFreshPreparationForTest`), so every comparison is retained replay against plain
  `Paint`.
- **Two replays.** *DrawImage replay*: `DrawImage(list)` under a translation. *Stream replay*: `ID2D1CommandList::Stream`
  into a forwarding `ID2D1CommandSink` that issues each recorded command on the drawing context (`DrawGlyphRun`,
  `FillGeometry`, `DrawGeometry`, `PushAxisAlignedClip`, ...), composing recorded transforms with the replay translation,
  forwarding a state command only when it changes the context's state, and restoring the context's state afterwards. The
  spike uses stream replay for lists recorded for ClearType text (the window host's opaque swap chain) and `DrawImage`
  otherwise.

## Direct2D facts the spike established (WARP, test-local objects)

| Question | Answer |
| --- | --- |
| What does a list hold for text? | Glyph runs (`DrawGlyphRun` with font face, indices, advances, offsets), not the layout: a layout's reference count is the same before and after recording (1 and 1), and the list replayed **after the layout is released** draws 0 differing pixels against `DrawTextLayout`. So `W9.4`'s assumption holds. |
| Whole-pixel translation of a recorded text list | 0 differing pixels against `DrawTextLayout` at the translated origin (144 DPI, fractional origin, (3, -2) px): DirectWrite snaps the baseline when the list records, a whole-pixel move keeps the snapped phase. |
| Brushes | Kept **by value**: a brush recolored after recording streams back as a different object with its recorded colour. |
| ClearType | `DrawImage` of a list replays its text **grayscale** on an opaque (`D2D1_ALPHA_MODE_IGNORE`) target, whether the list recorded `CLEARTYPE` or `DEFAULT`: 1,602 pixels differ from `DrawTextLayout` in the experiment, and the window-host oracle failed at the first frame (`B1B1B1` against `BCD0E8`). Stream replay keeps ClearType. |
| Rounded rectangles | Recorded as geometries (`FillGeometry`, `DrawGeometry`): one `CardPanel` chrome is 2 geometries and 10 state commands. |
| What a list contains | Card label 15 commands (2 glyph runs); card chrome 12; button 16 (1 geometry, 1 glyph run, clip push and pop); shortcut button 20; multiline label 17 (3 glyph runs, 71 glyphs). About two thirds of every list is state commands the recording context emits. |

## Criterion 1: Paint executions on FX5 (WARP, counters)

FX5 is 16 cards (`CardPanel` with `Label`, `Toggle`, `Slider`, `ProgressBar`), a `Tree` and a `Grid`: it has no `Button`.
Per frame, averaged over the fixture's phases (warm 20, clean 40, dirty 40, hover 20 frames):

| Mask | Retained kinds | clean | dirty | hover |
| --- | --- | --- | --- | --- |
| 0 | none (today) | 83 | 83 | 83 |
| 0x7 | the item's three (FX5 has `Label`, `CardPanel`) | 51 (32 replayed) | 51 (0 recorded, 32 replayed) | 51 |
| 0x3F | plus `Toggle`, `Slider`, `ProgressBar` | **3** (80 replayed) | **35** (32 recorded: the 16 sliders and 16 progress bars that changed) | **4.95** (1.95 recorded: the toggle that lost and the one that gained hover) |

Every `Paint` of a retained kind on a dirty or hover frame is a control that changed. The remaining 3 executions per
frame are the controls outside the spike's kinds (`Tree`, `Grid` and one more control `Panel::Paint` walks); the
self-painting controls are `W4.2`'s. On FX5 `G2` (≤ 2 per hover frame) is therefore reachable only with every leaf kind
retained and parts for `Tree` and `Grid`.

## Criteria 2 and 4: the oracle (WARP)

`RunParityScript`, incremental (spike on) against fresh (`DebugDropRetainedStateForTest` and plain `Paint`), BGRA, bounds and
UI Automation:

| Host | Scripts | Result |
| --- | --- | --- |
| Embedded | spike script (cards, labels, buttons, toggles; hover, press, focus, text, whole-pixel and fractional moves of controls and whole cards, theme, clock), 3 seeds × 40 steps at 96, 144 and 192 DPI; translation script (only whole-pixel moves) at the three DPIs; shared `layout` and `interaction` scripts; masks 0x7 and 0x3F | 0 differences; 433 and 599 translated replays |
| Embedded, FX5 | 80 frames (warm, dirty, hover) against FX5 painted plainly, at 96, 144, 192 DPI; masks 0x7 and 0x3F; also with stream replay | 0 differences |
| Window host | spike and translation scripts at the three DPIs and the `layout` script, mask 0x7, ClearType lists replayed by streaming | 0 differences (994 translated replays) |
| Window host | same with `DrawImage` replay | fails at the first frame: ClearType text drawn grayscale |
| Window host | shared `interaction` script | fails at step 2 with a focus-state difference (no pixel difference reported first), **identically with the spike off** on this machine; a press in the spike script fails the same way with the spike off, so the window-host spike script does not press. Treated as a desktop-focus dependency of the native harness; not verified against an unmodified build. |

Translation: whole-pixel moves replay translated and match `Paint` at the new position exactly at 96, 144 and 192 DPI on
both hosts (criterion 4). A fractional move records again (the spike's rule (e)); the snapped text would otherwise move.

## Criterion 3: CPU of replay against paint

`--retained-paint-spike-bench`: one embedded view, 800 controls of one kind at 3840×2160, 192 DPI, every frame dirty, on
`FrameTiming::Measure` (UI-thread cycles of `Prepare`, which includes `EndDraw`, GPU timestamps). Modes: *empty* (no
controls), *paint* (spike off), *replay* (`DrawImage`), *stream*, *tile* (the list rasterized once into a bitmap,
`DrawBitmap`), *skip* (an unchanged control not drawn, what region invalidation does). Per control = (mode − empty) / 800.
Direct2D API calls per frame are counted by the counting wrappers (WARP pass).

**Quiet slot** (`bench-hardware.jsonl`; 4 rounds, every round measures each kind in each mode for at least 1,000 frames and
2 s, alternating the mode order; values are the median over rounds of each run's median, per control):

| Kind | Paint, cycles | Replay (`DrawImage`) | Stream replay | Tile (`DrawBitmap`) | Skip (not drawn) |
| --- | --- | --- | --- | --- | --- |
| `Label` | 5,306 | 4,560 (0.859) | 6,324 (1.192) | 2,785 (0.525) | 455 (0.086) |
| `Button` | 11,792 | 10,053 (0.853) | 12,581 (1.067) | 2,790 (0.237) | 453 (0.038) |
| `CardPanel` | 10,624 | 11,398 (1.073) | 12,571 (1.183) | 2,911 (0.274) | 549 (0.052) |

Submission time and GPU preparation time per control follow the same ratios (for example `Button` submit 2,757 ns paint,
2,351 replay, 653 tile, 106 skip; GPU 2,801, 2,134, 690, 115). The 15 November smoke runs under load gave the same picture
(replay 0.87, 0.76, 1.04; tile 0.47, 0.21, 0.28). On WARP (counters, `bench-warp.jsonl`) replay is 0.93, 0.85 and 1.17.

Direct2D API calls per frame for 800 controls (counting wrappers, WARP; 1 is the frame's `Clear`):

| Kind | Paint | Replay | Stream | Tile | Skip |
| --- | --- | --- | --- | --- | --- |
| `Label` | 801 | 801 | 801 | 801 | 1 |
| `Button` | 1,601 | 801 | 1,601 | 801 | 1 |
| `CardPanel` | 1,601 | 801 | 1,601 | 801 | 1 |

### FX5 frame timing (quiet slot)

`performance-timing.ps1 -Purpose Comparison -Phase Dirty -Sessions 6`, one executable compared with itself: A with the
spike off, B with `DXUI_RETAINED_PAINT_SPIKE` set (`-CandidateEnvironment`, a spike-only switch of the prototype's
`performance-timing.ps1`), default `interference-interim-v1` admission. Change is B against A; positive is slower.

| Comparison | Prepare cycles (median) | Submit (median) | GPU frame (median) | Rejected session attempts |
| --- | --- | --- | --- | --- |
| [FX5 1080p, mask 0x7](timing-FX5-dirty-0x7/README.md) (`Label`, `CardPanel` replayed; FX5 has no `Button`) | +1.84%, inconclusive (MDE 2.21%) | +1.97%, inconclusive | +1.79%, equivalent | 0 |
| [FX5 1080p, mask 0x3F](timing-FX5-dirty-0x3F/README.md) (every leaf kind retained) | +8.03%, slower | +7.85%, slower | +6.97%, slower | 3 |
| [FX5 4K 192 DPI, mask 0x3F](timing-FX5-uhd-dirty-0x3F/README.md) (no calibration: medians only) | +7.84%, slower | +7.75%, slower | +6.14%, slower | 2 |

With 0x3F a dirty frame records 32 lists (the changed sliders and progress bars: `Paint` into the list, then `DrawImage` of
it, which costs more than painting once) and replays 48 at 0.85 to 1.07 of a paint each, so the frame is slower; with
0x7 the 32 replays save about as much as the bookkeeping costs.

## Memory

Heap bytes returned when a view's 2,000 lists are dropped (`HeapSummary` over every process heap, same scene with the
spike off subtracted; WARP): `Label` about 2.4 KB, `Button` about 2.6 KB, `CardPanel` chrome about 2.7 KB per list. A
4 MiB budget (`A16`) holds about 1,600 lists; FX12's 2,000 controls would need about 5 MB. A list does not hold its text
layout (above), so a layout could be released once recorded (`W9.4`'s assumption holds for command lists). A tile at
192 DPI costs its ink bounds in device pixels × 4 bytes: a 90×24 DIP control is about 182×50 px, about 36 KB.

## Why replay is not cheaper, and the ClearType finding

**`DrawImage` of a command list re-executes the list.** Direct2D keeps no rasterized form of a command list: drawing it
plays every recorded command (glyph runs, geometries, clip pushes and pops, about 10 state commands) into the context's
batch again, so Direct2D's per-primitive cost, which the preliminary `W0.7` attribution puts at about 96% of frame CPU,
is paid on every replay. The wrappers see one API call per replayed control (801 per frame against 1,601 when painting
`Button` and `CardPanel`), yet the CPU per control is 0.85 to 1.07 of painting; stream replay, which issues the recorded
commands one by one through the counted context, counts exactly the painting's calls and costs 1.07 to 1.19 of painting.
What replay saves is only DxUi's own work before the Direct2D calls (style resolution, brush and retained-layout lookup),
which `W2.1` and `W2.8` had already made small; for `CardPanel` the recorded geometries cost more than its paint, which
draws a rounded rectangle directly. A bitmap tile replaces all of a control's primitives with one textured quad, and
skipping draws nothing; they are the only two forms that remove the Direct2D cost.

**`DrawImage` of a command list draws ClearType text grayscale.** On an opaque target (`D2D1_ALPHA_MODE_IGNORE`, the
window host's swap chain, where Direct2D's default text mode is ClearType), a list replayed with `DrawImage` draws its
text with grayscale antialiasing whether it recorded `D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE` explicitly or `DEFAULT`
(experiment: 1,602 pixels of the text differ from `DrawTextLayout`; window-host oracle: the first frame fails, `B1B1B1`
gray against `BCD0E8` coloured fringes). The embedded surface is premultiplied and always grayscale, so it is unaffected.
**Stream replay** keeps ClearType exactly: `ID2D1CommandList::Stream` hands each recorded command to an
`ID2D1CommandSink` that issues the same call on the drawing context (`DrawGlyphRun` with the recorded glyph run and
description, `FillGeometry`, `DrawGeometry`, `PushAxisAlignedClip`, `PushLayer`, ...), composes each recorded transform with
the replay translation, forwards a state command only when it changes the context's state, and restores the context's
transform, antialias modes, blend, unit mode and rendering parameters afterwards. With it the window-host spike,
translation and layout scripts give 0 differences at 96, 144 and 192 DPI, and FX5 with stream replay gives 0 differences on
the embedded host. It costs 1.07 to 1.19 of painting (the first version, which rebound the rendering parameters and modes
for every list, cost 15 to 40 times painting on hardware: rebinding text rendering parameters defeats Direct2D's glyph
batching). Any command-list design on the opaque native target therefore either replays by streaming, at more than the
cost of painting, or gives up ClearType; neither makes retention pay.

## What `W4.10` can take from this evidence

- **Skipping is cheap and is the win.** An unchanged control that is not drawn costs 0.04 to 0.09 of painting it (453 to
  549 cycles at 4K). That figure is everything that remains when a control is not drawn: `Panel::Paint`'s visit
  (`IsVisible`, the work counters) plus the spike's bookkeeping, which is heavier than `W4.10` needs: an exact-type check, a
  hash lookup of the host and of the control, two `weak_ptr` locks, a key of nine observed states (`IsCompactDensity` and
  `GetFlowDirection` walk to the root), `GetTransform` on the context and the whole-pixel test. A culling test against the
  paint clip in DIPs needs only the bounds and the clip, so `W4.10`'s skip should cost less; it should be measured the
  same way (the bench's `skip` against `empty`).
- **Where tiles pay.** A tile costs 0.24 (`Button`), 0.27 (`CardPanel`) and 0.53 (`Label`) of painting: about 4 times
  cheaper for chrome, about 2 times for text alone. A tile holds the control's ink bounds in device pixels at 4 bytes per
  pixel: a 90×24 DIP control at 192 DPI is about 182×50 px, about 36 KB, and 800 of them about 29 MB, beyond `A16`'s 16 MiB
  of tiles per view; text rendered into a transparent tile is grayscale, so tiles are for the embedded host or for content
  without ClearType text. A tile is invalidated by every change of its control (a hover recreates it), and creating one
  costs a record plus a rasterization, so it pays only for content that stays unchanged for many frames and is expensive
  to draw. `CardPanel` has no shadow (it fills and strokes a rounded rectangle), so the plan's "shadowed panel" was not
  tested; the effect-heavy content in DxUi is the popup and transient-surface elevation (`DrawDropShadow`'s shadow
  effect passes and backdrop blur of `ComboBox`, `Menu` and transient surfaces), which is where the developer's
  "effect-heavy controls only" scope applies.
- **Per-control invalidation.** Region invalidation needs to know which control changed. The spike attributed
  `Control::RequestInvalidate`, `Control::Invalidate(host)` and `Control::InvalidateMeasure` to their control, treated a
  `SetBounds` that keeps the size as a move (old and new bounds both dirty, nothing else), and found every other state a
  retained kind paints (hover, pressed, focus, focus visibility, default button, theme, DPI, text rendering parameters,
  typography, animation requests) reachable from the control or the host: in the oracle runs no list was replayed stale.
  Hover changes are not attributed today (`Control::OnHoverChanged` invalidates nothing itself; the spike read
  `IsHovered()` in its key), so `W4.10`'s region must add the old and new hovered controls explicitly.
- **Test infrastructure worth landing with `W4.10`** (on `pm/w4-0-spike-code`, not landed here): the spike oracle scripts
  (`Tests/Support/Support.Tests.RetainedPaintSpike.h`: cards with labels, buttons and toggles; hover, press, focus, text,
  whole-pixel and fractional moves of controls and whole cards, theme; at 96, 144 and 192 DPI on both hosts) as the
  region-invalidation scripts, the FX5 counters by phase (Paint executions, tree visits), the per-control benchmark
  (`--retained-paint-spike-bench`, a `FrameTiming` fixture per kind and mode, `skip` being exactly what culling achieves),
  and `performance-timing.ps1 -CandidateEnvironment` for same-binary A/B of a switch. The `W0.7` attribution entry points
  (`FrameAttribution::RunPass`) can take the bench's fixture unchanged.

## Window-host harness finding

The shared `interaction` parity script fails on the window host on this machine at step 2 (captured slider drag) with a
focus-state difference and no pixel difference, identically with the spike off (`TestNativeIncrementalParityInteraction`
run alone on the prototype's build). A press in the spike script failed the same way with the spike off, so the
window-host spike script hovers without pressing. It looks like a dependency on which of the two hidden windows holds
Win32 focus; it was not checked against an unmodified build.

## Commands

```powershell
# WARP evidence (Debug or Release): experiments, shapes, FX5 counters and parity, oracle scripts, memory
.\.build\x64\Debug\DxUi.EmbeddedTests.exe --test-retained-paint-spike .build\spike\warp-debug
.\.build\x64\Debug\DxUi.ControlTests.exe --suite=Rendering --test=TestNativeRetainedPaintSpikeParity
# Per-control benchmark (quiet slot: Hardware 1000 2000 4)
.\.build\x64\Release\DxUi.EmbeddedTests.exe --retained-paint-spike-bench .build\spike\bench-hardware Hardware 1000 2000 4
# FX5 comparisons (quiet slot); 0x7 and 0x3F, and -Fixture FX5@uhd-192dpi without -CalibrationPath
./performance-timing.ps1 -Purpose Comparison -Fixture FX5 -Phase Dirty -Sessions 6 -BaselineExecutable .build/x64/Release/DxUi.EmbeddedTests.exe `
    -CandidateEnvironment DXUI_RETAINED_PAINT_SPIKE=0x3F -CalibrationPath Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json -SkipBuild
```
