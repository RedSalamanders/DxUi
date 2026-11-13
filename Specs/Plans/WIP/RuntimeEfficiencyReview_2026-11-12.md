# Runtime efficiency plan: challenge review, 12 November 2026

Status: REVIEW — findings and proposed dispositions for the
[runtime efficiency plan](RuntimeEfficiency_2026-11-07.md). `RD1` was confirmed on 12 November 2026 and the plan was then
corrected for fact; no item's disposition and no code was changed. Decisions `RD2`–`RD7` in
[§9](#9-decisions-for-the-developer) await the developer. Prepared at `DxUi` `315b7f7a`.

- **Reviewed text**: the plan as it stood at `315b7f7a` (1,426 lines). `git diff --stat 4acabe0e 315b7f7a -- src include`
  is empty, so every `file:line` the plan cites can be compared with the tree exactly; a citation that does not match is
  an error in the plan, not drift. `main` has since moved to `e9258f82` (the frontier plan and the removal of the BUI
  study), still without a change under `src` or `include`.
- **Goal the plan was judged against**: a best-in-class graphics library — high frame rate, small memory, advanced
  features and animation.
- **Coordinated plans**: [remaining work](RemainingWork_2026-11-04.md) (D-P1 timing policy),
  [explicit state machines](ExplicitStateMachines_2026-11-10.md), and the [frontier plan](Frontier_2026-11-11.md), which
  merged after this review was written and was withheld from the reviewers;
  [§8](#8-reconciliation-with-the-frontier-plan) reconciles the two.

## 1. Verdict

The plan found real hot spots and its two best ideas — deterministic work counters and the incremental-versus-fresh
pixel oracle — should be kept. As a plan for the stated goal it does not hold up, for seven reasons.

1. **It cannot succeed or fail at its purpose.** It states no frame-time, frame-rate, latency or memory target, and it
   "claims no timing verdict". Of 154 gate clauses, 121 are work counters and none is a frame-rate or process-memory
   budget ([§3.1](#31-no-target-and-no-way-to-prove-speed)).
2. **"Verified" is not reliable.** Of 269 ledger sub-claims re-read against source, 156 (58%) hold as written. The plan
   builds items on about twenty symbols and citations that do not exist in the tree
   ([§4](#4-ledger-scoreboard)).
3. **Its central rendering claim is backwards.** DXGI already keeps a partially presented flip-model frame coherent;
   DxUi's current partial-present code is correct; the plan's "asymmetric `Present1`" redesign is unnecessary and its
   own truth test would confirm the error ([§3.2](#32-the-present1-claim-is-backwards)).
4. **It optimizes what was audited, not what dominates.** Dirty regions help a caret blink and do nothing for scrolling
   or transitions. Text, the usual largest cost, gets a second glyph renderer instead of a layout cache that already
   exists in the tree. The memory phase targets the smallest line in every modelled scenario. Larger costs sitting next
   to the cited ones were missed ([§5](#5-what-the-plan-missed)).
5. **Several proposed fixes would break a contract or correctness** ([§3.6](#36-fixes-that-would-break-something)).
6. **It has no route to advanced animation.** There is no shared animation abstraction, native ticks come from an 8 ms
   timer and nothing can move without a UI-thread repaint ([§3.5](#35-animation-is-a-list-of-effects-not-a-system)).
7. **It is five plans in one**: 67 items holding about 300 changes, with nine unconfirmed checkboxes that bundle about
   65 separate decisions, and about twenty correctness bugs queued behind a measurement phase they do not need
   ([§3.7](#37-scope-and-bookkeeping)).

## 2. How the review was done, and its limits

- Twelve independent read-only review passes, each given one slice of the plan and no knowledge of the others: six
  re-verified every ledger row `E1`–`E84` against source by subsystem; six challenged rendering and platform facts,
  animation and features, measurement and scope, memory, the text and layout designs, and the external citations.
- Each report was then checked by reading the cited code. Confirmed directly in this tree: the `DrawTextW` path behind
  `DrawCenteredText` (`src/Controls/DxUi.Controls.cpp:1093`); the 8 ms dispatcher timer
  (`src/Support/AnimationDispatcher.h:182, 348`); per-item measurement in `ComboBox::ComputePopupWidthDip`
  (`src/Controls/DxUi.ComboBox.cpp:2689-2700`); the cross-edit reuse gate
  (`src/Controls/DxUi.RichSemanticGeometry.cpp:2437, 2447`); the per-thread host count and erase-at-zero
  (`src/Controls/DxUi.WindowHost.cpp:668-672, 716-721`); the outermost-only terminate handling
  (`src/Foundation/Failure.cpp:297-303`); `Checkbox : Toggle` (`include/DxUi/DxUi.h:2401`); `HeapSize` on allocation
  only (`src/Foundation/ControlAllocation.cpp:45, 54-62`); the row snap in `Grid` (`include/DxUi/DxUi.h:5525`,
  `src/Controls/DxUi.Grid.cpp:7311`); the trail-surrogate skip in `StatusStrip`
  (`src/Controls/DxUi.Controls.cpp:10429-10432`); the TSF lock queue (`src/Controls/DxUi.TextStoreACP.cpp:1235-1248`);
  the `WM_GETOBJECT` comparison (`src/Controls/DxUi.Accessibility.cpp:13954`); the definition of a benchmark "control"
  (`Tools/BenchmarkGate.psm1:305-324`); the read-only caret contract
  ([controls and layout](../../UI/UI_ControlsAndLayout.md), "editable or read-only"); and that the symbols listed in
  [§4](#4-ledger-scoreboard) have zero occurrences.
- Checked against primary sources: Microsoft's
  [flip model, dirty rectangles, scrolled areas](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-1-2-presentation-improvements)
  page; Chromium `gpu/command_buffer/service/shared_image/dxgi_swap_chain_image_backing.cc` at `301a3f2f`; the Makepad
  tree at `506ec051`.
- **Limits.** Nothing was built or run. Every time figure in this review is inference and is marked as such. `sizeof`
  values are hand counts under MSVC x64 rules and need a `static_assert` probe. 123 of about 230 external citations were
  fetched; the rest keep whatever status the plan gives them. Where two passes disagreed, the disagreement is
  adjudicated in [§4](#4-ledger-scoreboard). The detailed pass reports are untracked working files
  (`.build/reports/runtime-efficiency-review-20261112/` in the review worktree).

## 3. Foundational challenges

### 3.1 No target, and no way to prove speed

| Finding | Evidence |
| --- | --- |
| No quantitative target anywhere: frame time or rate at a stated resolution, input-to-present latency, idle CPU, private bytes, video memory, bytes per control, startup. | Whole plan. The nearest are wake counts (`P2.1`) and an unbounded "private-bytes drop" (`P8.1`). |
| 154 gate clauses: 121 counters, 5 timing, 6 memory (one process-level), 12 pixel parity, 10 human review. Seven items have no gate, including `P3.1`. | Gate inventory of `P0`–`P8`, `V1`–`V8`, `B1`–`B6`. |
| "71 of 72 controls unstable" does not mean timing is unusable. A "control" is a same-binary run pair, flagged unstable if **any** of 26 metrics moves more than 5% (timing) or 2% (memory) in **either** direction. | `Tools/BenchmarkGate.psm1:305-324`. |
| The instability is a sampling design problem: 40-frame rounds lasting about 15 ms (shorter than one scheduler quantum), "p95" taken as the 38th of 40 samples, clean prepare measured at 3–4 timer ticks, a shared desktop on a Balanced power plan, and a blocking readback inside every frame. Same-binary clean rate ranged from 9.9 to 2,626 frames per second. | `Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h:63-230`; `Measurements/Performance/2026-10-10-aa/README.md`. |
| All performance evidence is one fixture: 83 controls, 1280×720, 96 DPI, WARP, offscreen, absolute layout, motion disabled. No hardware GPU, present path, resize, popup, text entry or UI Automation client. Latest receipt: clean 2,620 frames per second, dirty 652.6, dirty p95 1.65 ms, 26.3 MiB private. | `Samples/ComplexUi/ComplexUiScene.h:69-116`; `Measurements/EmbeddedInteractionLayout/Default-B1.json`. |
| Two of the plan's own gates sit below the noise: `P0.1` "under 1% overhead" against a 2.6–4.0% spread inside one receipt; `P8.1` "private-bytes drop" of about 21 KB against 315 KB of same-binary drift. | Same receipts; `Measurements/GalleryFollowUps/2026-09-28/paired-hosted/README.md:22`. |
| The no-timing rule is broken by the plan itself: `P0.1` 1%, `P6.3` 1 ms, `P8.2` 2%, `B4` 50 ms, and µs budgets in `P2.1`, `P2.6` and `D5`. | The gates and change text of the named items. |

Counters are the right guard against allocation and resource churn. They cannot show that a frame got faster, and
several counter gates can pass while the user-visible result gets slower: one `DrawGlyphRun` per run instead of one
`DrawTextLayout` (`P4.1`), up to eight tree traversals for eight dirty rectangles (`P3.2`), cached effect bitmaps with
no video-memory gate (`P1.2`, `V4`). GPU time, Direct2D tessellation, `Present` stalls and non-`operator new`
allocations are invisible to them.

### 3.2 The `Present1` claim is backwards

The plan (verdict item 4, `E7`, `P0.4`, `P3.3`, `D12`, two risk rows) says a two-buffer flip-sequential swap chain
shows stale pixels unless Direct2D redraws `dirty_N ∪ dirty_{N-1}`, and that Windows Terminal and Chromium confirm it.

- Microsoft's page states the opposite: "The runtime copies only the differences between updated areas of the previous
  frame and updated areas of the current frame onto the current back buffer", and the copy happens "on the current
  frame's present". The application's duty is that every pixel inside its own dirty rectangles is current; it must
  copy from the previous buffer only when it does **not** fully re-render them.
- DxUi clips to the dirty rectangle, clears inside the clip and repaints the tree under it
  (`src/Controls/DxUi.WindowHost.cpp:5105-5137`), and forces a full first present after create and resize
  (`:4514-4516`, `:4591-4592`). That is complete. Only the comment at `:5071-5073` gives the wrong reason.
- Terminal at `5f7dc283` redraws every row each frame and passes one clamped dirty rectangle. Chromium's cited lines are
  capability setup; its swap-chain backing says `Present1` "will copy the contents outside the swap rect from the
  previous buffer". Neither implements a history union.
- **`P0.4` is designed to give the wrong answer.** It reads the back buffer before the next draw, which is before the
  runtime's copy, so it will see content two frames old and declare the union "mandatory". The test must observe the
  presented image.
- The path barely runs today: every DxUi invalidation is full-window (`:1912`), so a partial frame occurs only on
  partial invalidation from the system.
- The union is needed in exactly one case: code that **reads** the target before present — the back-buffer capture at
  `:5189-5194` and the planned blur of DxUi's own surface (`V5`, `D7`).

Other platform statements the plan builds on are also wrong or unsupported:

| Plan statement | What the primary source says |
| --- | --- |
| `DXGI_SCALING_STRETCH` is a defect (`E56`) | It is mandatory for `CreateSwapChainForComposition`. Valid only as "use `NONE` for the HWND chain". |
| ClearType on a premultiplied target fails `EndDraw` with `D2DERR_INVALID_CALL` (risk register) | Direct2D already falls back to grayscale on such targets; forcing ClearType gives "unpredictable results", not a failure. The documented failure is rendering parameters that do not match the antialias mode, which `P4.6` introduces. |
| `Present(0, DXGI_PRESENT_TEST)` wakes occluded swap chains | Flip-model chains never return `DXGI_STATUS_OCCLUDED`; this also contradicts [Win32 host](../../Rendering/Rendering_Win32Host.md). |
| Add `D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS` to stop "16–32 driver threads" | Documented as "not recommended for general use"; on WARP it moves all rasterization to the calling thread, slowing the fallback and every WARP test. The thread count has no source. |
| `DCompositionCreateDevice2(nullptr)` keeps the visual tree across device removal | Plausible but undocumented, and without effect alone: `DiscardDeviceResources` resets target, visual and device anyway (`:4660-4662`, `:612`). |
| Waitable swap chain at latency 1 (`D5`, `P2.7`) | The waitable flag must also be passed to `ResizeBuffers`; `:4571` passes 0, so every resize would fail. Latency 1 makes every unpaced `WM_PAINT` present block. A tick that skips `Present` needs a timer or it spins or stalls. None of the three is in the plan. |
| `RequestLock` returns an "illegal" `TS_E_SYNCHRONOUS` (`E77`) | The code queues asynchronous requests and returns `TS_S_ASYNC` (`src/Controls/DxUi.TextStoreACP.cpp:1235-1248`); the result called illegal is a documented outcome. The `P1.15` gate of zero such results cannot be met. |

### 3.3 Dirty regions are a local optimum; text gets the wrong tier

- **What `P3` buys.** A frame's cost today is, by inference, tree traversal, per-primitive Direct2D CPU work,
  per-frame resource creation and offscreen passes. Dirty regions cut all four for small changes. They cut nothing for
  full-viewport motion — scrolling, page slides, popup fades, inertia — which is what a user perceives as smooth, and
  the visual track adds exactly that kind of motion.
- **What the plan never uses.** `Present1`'s scroll rectangle (Terminal does), `ID2D1GeometryRealization`,
  `ID2D1SpriteBatch`, retained per-subtree surfaces or DirectComposition visuals. `D4`'s 16 MiB strip cap holds
  16,777,216 / (3,840 × 4) ≈ 1,092 rows at 4K, about half of one viewport.
- **`P3` design gaps.** `rcPaint` is the bounding box of the update region, so an eight-rectangle region must live in
  `ControlHost`, not in `WM_PAINT`. Painting once per dirty rectangle multiplies traversals unless `P3.4` culling lands
  first. A single overhang number per control is not enough: overhang depends on state and must be unioned old and new.
  Embedded `P3.2` is the cheap, safe, high-value part: one persistent texture, no buffer history.
- **Text.** A plain `Label` and every `Button` lay their text out again on each paint: `DrawCenteredText` calls
  `ID2D1DeviceContext::DrawTextW` and has 61 callers (`src/Controls/DxUi.Controls.cpp:1072-1095`). The cheapest fix is
  to retain one `IDWriteTextLayout` per string; the two-generation cache for exactly that already exists for `Grid`
  (`src/Controls/DxUi.Internal.h:328-388`). `P4.1` skips this tier and specifies glyph-run replay, a second text renderer
  that must stay pixel-identical across color fonts, fallback and trimming. A DxUi glyph atlas is a third tier and strains
  the embedded rule that the host borrows the application's device.
- **Layout.** `P5.1`'s "Rule M1" is unsound as a general rule (`FormField` and `FlexPanel` depend on the maximum they are
  given) and is not Taffy's: Taffy matches exact keys. The larger gaps are a whole-tree preparation walk on any dirty
  flag (`src/Controls/DxUi.Controls.cpp:2393-2529`), no cutoff when a re-measured size is unchanged, and a measure cache
  per parent instead of per control.

Two experiments decide what follows, and neither is in the plan:

1. **Full-frame cost breakdown on a hardware GPU at 1080p and 4K**: traversal and paint CPU, `EndDraw` CPU, GPU time
   from timestamp queries, `Present`, and Direct2D call count. If a full frame costs at most 4 ms at 4K, stay on
   Direct2D with dirty regions, the scroll rectangle and geometry realizations. If full frames exceed 8.33 ms and
   CPU-side submission dominates, dirty regions cannot fix animation and a layer tree or batch renderer is next.
2. **A scroll prototype three ways** at 120 Hz: full repaint; scroll-rectangle band repaint; a DirectComposition visual
   offset over a retained surface. Measure missed frames, UI-thread CPU and resident bytes.

### 3.4 The memory phase targets the smallest line

Estimates from the declarations and existing receipts; see the limits in §2.

| Scenario | What `P8` saves | Largest lines the plan does not address |
| --- | --- | --- |
| ComplexUi (83 controls) | 83 × 144 B + 16 × 584 B ≈ 21 KB | Free-but-committed heap 5–15 MB; first-frame caches ≈ 2.8 MB; surface 3.7 MB |
| 1,000-control form | ≈ 0.23 MB | Swap chain 2 × 8.29 MB at 1080p, 63.3 MiB at 4K; text undo up to 16 MiB **per** 64 Ki-unit `TextField` (64 full-text snapshots each way) |
| 200,000-row `Grid` | ≈ 144 B | Visible cell layouts at about 20 KB each: 6–15 MB; selection ids 3.2 MB |
| 128 KiB rich document | ≈ 0 | History ≤ 4 MiB; display-map anchors 1.0 MiB |

- The `P8.1` size table is miscounted. `Control` is about 296 B, not 328: the two callbacks are `shared_ptr`s (32 B), not
  inline functions (128 B). `Checkbox` is listed smaller than `Toggle` although it derives from it. The
  `ThroughputGraphSample` target of 160 B is unreachable with sixteen 16-byte weights (256 B). The proposed weight layout
  changes a public `double` to `float`.
- `HeapSize` is never called on delete, so "drop the `HeapSize` call on sized deletes" removes nothing. The header
  exists for an alignment back-pointer; `HeapAlloc` already returns 16-byte-aligned blocks.
- A `unique_ptr` cannot point at a static instance. The class already has the right pattern (null means defaults); a
  control that sets any cold field then costs about 80 B **more**.
- Splitting the toggles from `Button` breaks upcasts and inherited methods for consumers and eleven `dynamic_cast` sites
  in the library; it is a revision-6 break worth about 9 KB in ComplexUi.
- Half of `P8.3` targets `InMemoryGridModel::RebuildView` and `GridCellValue`, which do not exist.
- Nowhere in `src`: `SetMaximumTextureMemory`, `ClearResources`, `IDXGIDevice3::Trim`, `QueryVideoMemoryInfo`. Hiding a
  view frees a 3.69 MB surface and private bytes fall 0.64 MB.

### 3.5 Animation is a list of effects, not a system

- **Today.** At least six hand-rolled transition patterns; durations are constants, several in the public header;
  `MotionPolicy` has no caller in the library; the `Toggle` knob jumps (`src/Controls/DxUi.Controls.cpp:1274`) and the
  `Checkbox` mark appears without motion. Native ticks come from `SetTimer(8 ms)`, which the system raises to its 10 ms
  minimum; the effective rate of about 64 Hz is inferred from the default timer tick and is not aligned to the display. No control has a transform, opacity or clip property. There is
  no `WM_POINTER`, touch or DirectManipulation handling.
- **The plan.** `V1` adds closed-form springs per control. It has no timeline, keyframes, steps, sequencing, stagger or
  delay; no implicit transition on property change; no layout animation beyond the one `Tree` already has; and no path
  for transform or opacity to animate off the UI thread, although `WindowHost` already owns a DirectComposition device
  with one visual (`src/Controls/DxUi.WindowHost.cpp:835`).
- **Item defects.** `V3`'s second opacity tier folds group opacity into each brush, which shows fills through borders
  and backgrounds through text. `V4`'s "0 intermediate surfaces" is unproven for a custom effect and a cached nine-slice
  does the job. `V5`'s backdrop needs Windows 11 build 22621 while the baseline is Windows 10, and its per-frame blur
  pyramid would rasterize during composition in an embedded host. `V6` cites DirectManipulation in `P2.7`, which contains
  none. `V7` promises zero per-frame allocations while `PageHost` creates up to four layers per animated frame today
  (`:1803`, `:1936`). `P2.7`'s averaged frame delta drifts animation time from wall time; controls animate from absolute
  time, so sample the predicted present time instead. "Gallery review" is not an acceptance criterion.
- **Order that avoids doing the work twice.** Pacer → animation core with a deterministic clock and frame-rate-sweep
  tests → per-control transform, opacity and clip → migrate and add the missing transitions → layout and connected
  transitions → DirectComposition promotion in native hosts. An embedded host can only publish descriptions of what to
  animate.

### 3.6 Fixes that would break something

| Item | Proposed change | Problem | Instead |
| --- | --- | --- | --- |
| `P1.3`, `P2.2` | Suppress caret painting on read-only fields | Contradicts [controls and layout](../../UI/UI_ControlsAndLayout.md) ("editable or read-only") and the validation contract; removes keyboard-selection feedback | Steady, non-blinking caret: same wake saving |
| `P1.2` | Skip gradients on `OverlayMaterial::Solid` | Changes pixels (glaze and shade alphas are non-zero, `DxUi.ComboBox.cpp:252-254`), breaking the item's own parity gate | Retain the brushes |
| `P1.2` | Remove the menu's pre-show render | Risks an unpainted first frame | Drop the redundant `InvalidateRect` (`DxUi.Menu.cpp:4243`) |
| `P1.2` | Thread-scoped device lease kept across `EmbeddedHost` lifetimes | Pins a second D3D11 device for the application's lifetime | A popup opened from an embedded host borrows that host's device |
| `P1.6` | Skip posting `AccessibilityFlush` after a synchronous flush | The posted drain is the only place a `WindowHost` raises focus, property, structure, selection and text events (`DxUi.Accessibility.cpp:14786-14856`) | Delete the sub-item |
| `P1.6` | Update one snapshot node in place on a value change | Snapshots are read concurrently by UI Automation worker threads: a data race | Structural sharing (per-record sharing or a persistent array) |
| `P1.7`, `P4.3` | `SetMaxWidth` in place on `Grid` column resize | Layouts are often prefix-shaped, shared across cells by text and width, and trimmed per width (`DxUi.Grid.cpp:3773-3799, 3855-3876`) | Accept re-creation for the resized column |
| `P1.7` | Lightweight `Tree` expansion records | The collapse animation draws removed rows' text from the full records (`DxUi.Tree.cpp:1443-1479`) | Full records only for removed rows in the viewport |
| `P1.7` | Accumulate fractional wheel remainders in `Grid` at `:5891` | The delta is already a float; the quantization is the row snap at `:7311` | Fix the normalization |
| `P1.12` | Replace per-message terminate handling with a once-installed handler | Cost is three per-thread pointer operations per outermost message; the restore protects the application's own handler and the failure policy | Drop |
| `P1.15` | 45-second secret-clipboard clear | Needs a timer, which an embedded host must not own; it is product behavior | Application-scheduled, outside this plan |
| `P6.1` | In-place piece-tree mutation at `use_count() == 1` | Undo holds every previous root (`DxUi.RichDocument.cpp:1009-1012`), so it almost never fires; `use_count` is not a synchronization primitive | Drop |
| `P6.2` | "0-ULP" chunk rebase | The code relies on bit-exact float equality with a whole layout | Redefine the oracle; include Enter in the gate |
| `V3` | Fold group opacity into brushes for a single child | A control paints overlapping fill, border and text | Restrict to single-primitive paint or drop |

### 3.7 Scope and bookkeeping

- **Size.** 67 top-level items, about 300 distinct changes (`P1.12` alone has about 25 across ten ledger rows), 62–110
  pull requests under the plan's own rule, each needing three x64 test profiles and three ARM64 builds.
- **Status.** "SPEC READY" while nine of twelve decisions are unconfirmed. `D12` asks for approval of about twenty
  mechanisms from eleven codebases in one sentence. The [WIP index](README.md) says to add work to the current plan
  rather than open another queue.
- **Order.** `D9` says `P0` then `P1` first; the graph starts `P2`, `P3` and `V2` straight after `P0`. `P1.1`, `P1.7`
  and `P1.14` depend forward on `P4.1`.
- **Gates that name the wrong fixture.** `P2.6` and `P3.4` gate on `S12`, which has determinate progress bars, no
  `ScrollPanel` and absolute layout.
- **Ledger cross-references that point at the wrong row.** `P7.1` cites `E6`; the split-CRLF pad cites `E81`; the
  cross-ecosystem section cites `E83` and `E84` for unrelated mechanisms.
- **Undeclared conflicts with explicit state machines.** `P2.3` specifies a five-state tooltip machine against that
  plan's seven-state `TooltipLayer::Mode`; also `P2.1`, `P1.14`, `P1.7` and `P2.4`.
- **Pixel changes inside performance items**, against the plan's own rule: 0.7 px ink alpha (`P1.9`), cap-height
  centering and ligature suppression (`P4.1`), elision changes (`P1.14`).
- **`P0` itself.** The `->Create…(` source lint misses `CreateTextFormat` (16 of 19 sites are not written with `->`),
  `CreateDeviceContext`, and `D3D11CreateDevice`, which is called through a function pointer although `P1.2` gates on
  its count. About 17 of 20 scenarios mirror a ledger row one to one; none scrolls a large list, resizes a window, runs
  at 4K, holds thousands of controls or measures startup.
- **Correctness queued behind measurement.** `P0` "blocks every other phase", yet about twenty items are plain bugs with
  deterministic tests ([§6.1](#61-fix-now-no-measurement-phase-needed)).
- **Features filed as efficiency.** `B3`, `B5`, `B6`, `P5.4`, selection semantics in `P1.7`, the clipboard timer.

### 3.8 External citations add less rigor than they appear to

123 of about 230 citations were fetched at the pinned commits: 85 confirmed (69%), 13 real but at the wrong path or
lines, 18 misrepresented, 6 nonexistent, 1 unverifiable. All fifteen repositories and commits exist.

| Failure | Detail |
| --- | --- |
| Rule that the source does not have | Taffy's cache matches exact keys; "Rule M1" and "100% resize hits" (`P5.1`) are the plan's own. AccessKit uses a byte-index table, not a present mask with `popcount` (`P7.3`). ImGui has no `selectAllInverted` (`B6`). |
| Invented thresholds in gates | Makepad's `gpu_pressure.rs` has megabyte reserves and timers, not 80/70 and 92/85 percent (`P4.5`). gpui-fast has no three-wake threshold (`P2.7`). The 64–128 px resize blocks have no source (`P1.12`). |
| Paths that do not exist | About ten Makepad paths under `platform/src/os/windows/…`; the code is under `platform/hal/src/backend/windows/…`. Confirmed against the tree at `506ec051`. |
| Numbers absent from the cited document | rune `research/31` and `research/38` contain neither "220×", "50.9 ms" nor `wall_ms`. |
| Pull requests about something else | rune `#324` (Wayland), `#492` (wgpu), `#497` (visibility events); gpui-fast `#130` (macOS). |
| Renamed and presented as verbatim | `_frameLatencyCreditHeld` (Makepad: `credit`), `_pendingLockFlags`, tooltip states. |

Citations to ThorVG, Clay, ImGui, VS Code and xi-editor held almost without exception. The failures cluster where a
design or gate depends on them. The `BUI` section records GPL file and line ranges and constants, which documents that
its authors read the source: the opposite of a clean-room record. It has since been removed from the plan.

## 4. Ledger scoreboard

Sub-claims, as split and judged by the six ledger passes. Row-by-row corrections are in
[appendix A](#appendix-a-ledger-corrections).

| Area (ledger rows) | Sub-claims | Hold | Overstated | Wrong | Needs measurement | Mis-cited |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Hosts, rendering, scheduling (`E1`, `E4`–`E10`, `E14`, `E37`–`E39`, `E55`, `E56`, `E72`–`E74`, `E76`) | 49 | 24 | 11 | 9 | 5 | 0 |
| Text fields, combo, popups, menus (`E2`, `E3`, `E11`–`E13`, `E15`–`E18`, `E21`, `E34`, `E44`, `E51`, `E52`) | 44 | 27 | 7 | 5 | 5 | 0 |
| Grid, Tree, layout, specialised controls (`E22`–`E24`, `E46`–`E50`, `E53`, `E54`, `E82`–`E84`) | 59 | 37 | 12 | 5 | 5 | 0 |
| UI Automation, TSF, text services (`E19`, `E20`, `E59`–`E63`, `E77`, `E78`) | 39 | 22 | 9 | 6 | 0 | 2 |
| Semantic geometry, math, diagrams, images (`E25`–`E33`, `E45`, `E70`, `E71`, `E75`, `E81`) | 38 | 20 | 9 | 5 | 4 | 0 |
| Rich document core (`E64`–`E69`, `E79`, `E80`) | 40 | 26 | 6 | 4 | 4 | 0 |
| **Total** | **269** | **156 (58%)** | **54 (20%)** | **34 (13%)** | **23 (9%)** | **2 (1%)** |

The remaining rows were judged in the design passes: `E40` and `E42` hold, `E43` holds and is understated (the blur
also occurs at 100%), `E41` partly holds (wrong-speed restart only in `Slider`); of 22 memory figures in `E35`, `E36`,
`E46`, `E49`, `E58`, `E60` and `E66`, 8 are confirmed, 3 overstated, 8 wrong and 3 unverified; `E57` is overstated.

By whole row: 33 of the 84 rows hold as written, 15 hold with a corrected number, symbol or scope, 6 are overstated, 26
contain at least one false sub-claim, 3 were already refuted by the plan, and 1 needs a measurement. No row is wholly
false: the ledger points at real code almost everywhere, and is wrong about what that code does or costs in a third of
its rows.

**Symbols and citations the plan builds on that are not in the tree** (each has zero occurrences in `src`, `include`,
`Tests` and `Samples`, or points at unrelated code):

| Cited | Where | What is actually there |
| --- | --- | --- |
| `HandleDeviceLost`, `_deviceLossRetryCount` | `E56`, `P1.12`, risk register | No retry counter or backoff. If recreation fails, `Render` returns without scheduling a retry and the window can stay blank (`DxUi.WindowHost.cpp:5045-5050`). |
| `_lastMousePos` modality clobber | `E55`, `P1.12` | `WM_MOUSEMOVE` never sets input modality. |
| `UpdateTooltipHover` "5 copies, 2 queries" | `E56` | `UpdateSupplementalTooltipTarget`: 3 copies, 1 query. |
| `HBITMAP` cloning in `NativeMenuInterop` | `E14`, `P1.2` | No `HBITMAP` code in that file. |
| `PaintBadgeCell`, `ResolvedColumnLayout`, `ComputeViewportMetrics`, `GridVisibleRowMap` | `E53`, `P1.7` | Badges go through `DrawCenteredText`; the real cost is `CollectOrderedGroups` and `GetRowTopDip`. |
| `InMemoryGridModel::RebuildView`, `GridCellValue` | `E53`, `P8.3` | `Grid` does no row sort. |
| `vector<DWRITE_FONT_AXIS_VALUE>`, raw format pointers | `E57`, `P4.5` | The cache is in `include/DxUi/Typography.h`, owns its formats and pins the factory through `com_ptr`; the plan cites a different file. |
| A `"copy"` text layout per code fence | `E26`, `P1.11` | Drawn as rectangles and lines. |
| `state_->Clear()` on horizontal resize | `E68` | `Invalidate()`; images survive a width change. |
| Flatten at `DxUi.RichTextCommands.cpp:4238` | `E65` | No such call. |
| TSF deadlock at `DxUi.TextStoreACP.cpp:635, 650` | Cross-ecosystem §1 | Those lines are `ReadPolicy` and `ReadState`. |
| Grapheme functions at `TextInputServices.cpp:85-144` | `P1.15` | That range is `ClientTextStoreTarget`. |
| `DxUi.h:4714-4729` for the graph sample | `E46`, `P8.1` | That is `FlexPanel::LineRecord`; the struct is at `:2584-2602`. |
| Surrogate split in `StatusStrip` and in `SelectSingleLineWordAt` | `E47`, `E63`, `P1.14` | Both already keep pairs whole. |
| `ColorPicker` losing saturation at V = 0 | `E49`, `P1.14` | Already preserved (`DxUi.EditorControls.cpp:188-193`). |
| Undo pushed on every IME stroke | `E62`, `P1.15` | Not on the TSF path (`DxUi.TextStoreACP.cpp:349`). |
| Wang's formula for flowchart curves | `P1.9` | Edges are six-point polylines; there are no curves. |
| "Cache `ResolveDiagramLabelBackground`" | `E28`, `P1.9` | Already cached (`DxUi.RichTextEditor.cpp:4199-4207`). |

**Adjudicated disagreements between reviewers.**

- *`Grid` wheel (`E44`).* One pass called "quantized" wrong because the delta is a float (`DxUi.Grid.cpp:5891`); another
  showed the offset is then floored to a row boundary. Read in source: `ClampScrollOffsets` defaults to
  `normalizeVertical = true` and floors at `:7311`. `Grid` vertical scrolling is row-quantized; the ledger's mechanism and
  the plan's fix location are wrong.
- *History union (`P3.3`).* The citation pass called the union "required by DxUi's own two-buffer flip model" by
  inference; the rendering pass and Microsoft's page show the runtime performs that copy at present. The union is not
  required for presentation (§3.2).
- *Display-map size (`E66`).* One pass accepted "4.2 MiB" for a 262,144-unit document; the document cap is 65,536
  units (`include/DxUi/RichDocument.h:127`), so the anchors are 1.0 MiB and the figure is unreachable.
- *Format-cache leak (`E57`).* One pass repeated the plan's "leaked static format map"; the pass that read the file found
  at most 28 owned entries released at static destruction. No leak.
- *UI Automation gating (`E59`).* One pass repeated "no client gate"; the pass that read the code found native builds
  gated on a provider request or a listening client, and embedded builds run only when the application calls
  `UpdateAccessibility`.

## 5. What the plan missed

Costs and defects next to the cited ones, found while re-reading the same files. Time figures are inference.

| # | Finding | Evidence | Weight |
| --- | --- | --- | --- |
| 1 | `ComboBox` is quadratic when every row fits: (1 + 2M) × M DirectWrite layouts per paint (20,100 at M = 100), and three hit-tests per `WM_MOUSEMOVE` (1,500 layouts per mouse message at 500 items) | `DxUi.ComboBox.cpp:2616-2622, 2689-2700` | High |
| 2 | While a tooltip is pending, every mouse move over the control reports a change and repaints the whole window; `Tree` rows with tooltips do the same | `DxUi.Controls.cpp:12044-12050`, `DxUi.WindowHost.cpp:1788-1792` | High |
| 3 | Plain `Label`, `Button`, menu, `Tree` and `Grid` header text is laid out again on every paint: 61 callers | `DxUi.Controls.cpp:1072-1095` | High |
| 4 | After any edit, every raster image is re-uploaded to the GPU (up to 64 MiB) and every SVG re-rasterized; the reuse lookups are already keyed by pixel identity and only one condition blocks them | `DxUi.RichSemanticGeometry.cpp:289, 310, 2437` | High |
| 5 | The semantic document and display map are rebuilt from scratch on every revision (about 1 MiB written per keystroke at the size cap); the plan's fixes keep this | `DxUi.RichSemanticDocument.cpp:2596-2617` | Medium–High |
| 6 | Text-input sync flattens the document twice per character and per arrow key: about 640 KiB copied per keystroke at the cap | `DxUi.WindowHost.cpp:2335, 3908`, `DxUi.NativeTextInput.cpp:1063, 1076` | Medium |
| 7 | Each table cell's reuse check compares the full table source: about 44 MiB compared per keystroke for a 200×5 table | `DxUi.RichSemanticGeometry.cpp:1986-1993` | High if confirmed |
| 8 | Layout chunks bake their absolute vertical origin into the text, so Enter re-lays everything below | `DxUi.RichTextLayout.cpp:308-340` | High at the cap |
| 9 | A grouped `Grid` is O(G²) per frame, runs `CollectOrderedGroups` about ten times per frame, and once per cell in right-to-left layouts | `DxUi.Grid.cpp:7077-7114, 7393, 7424, 7866-7868` | High for grouped data |
| 10 | `Tree`'s default `FindVisibleItemById` is O(N) with four string copies per item on every key | `DxUi.Tree.cpp:279-295` | Medium |
| 11 | A multiline `TextField` builds two uncached full-document layouts on every paint, including caret blinks | `DxUi.TextInput.cpp:766, 811` | Medium–High |
| 12 | The brush cache is keyed by exact color: every animated color creates a brush and the cache clears at 256 | `DxUi.WindowHost.cpp:2853-2893, 4674-4677` | Medium |
| 13 | Hit-testing runs three times per `WM_MOUSEMOVE` (cursor, hover, target) | `DxUi.WindowHost.cpp:3581, 5833, 5868` | Medium |
| 14 | UI Automation: label lookup walks the whole tree for every unnamed control; an unnamed text field uses its full text as its Name; one `WM_GETOBJECT` turns on full rebuilds for the window's lifetime | `DxUi.Accessibility.cpp:4166, 4184, 6377` | High with a client attached |
| 15 | Any pending layout flag walks the whole tree, with no cutoff; `Label` rebuilds its layout on a position-only move, which [theme and typography](../../UI/UI_ThemeAndTypography.md) forbids | `DxUi.Controls.cpp:2393-2529, 2822-2826` | Medium |
| 16 | Text undo is capped by count, not bytes; no video-memory budget or trim exists; a hidden view returns a sixth of its surface bytes | `DxUi.h:4025`, `DxUi.TextInput.cpp:3362-3366` | Memory |
| 17 | Per-popup cost with no device churn: about fifteen text formats, a device context, a shadow-padded composition swap chain and a screen capture, for every popup including submenus | `DxUi.WindowHost.cpp:4162-4307`, `DxUi.Menu.cpp:4097, 4152` | Medium (open latency) |

## 6. Proposed restructure

### 6.1 Fix now, no measurement phase needed

Each has a deterministic functional test.

- **Hosts.** Schedule a retry when device or swap-chain recreation fails (blank window). Embedded chord capture released
  on the first button-up; `MiddleUp` ignored in both hosts; `ScrollPanel` swallowing the wheel at its bounds;
  `CardPanel::SetCornerRadius` not repainting; brushes cleared on every resize; the dead `#else` render body.
- **UI Automation.** Cast `lParam` to `DWORD` in `WM_GETOBJECT`; a toggle `Button` exposing both Invoke and Toggle; raise
  `CompositionFinalized` with the committed text instead of an empty `Composition` event; provider lookup fallback when
  the window property is gone; read the caret index without copying the text state.
- **TSF.** `GetACPFromPoint` flags and error code; input scopes through `RetrieveRequestedAttrs`; a prefix and suffix
  `TS_TEXTCHANGE` instead of whole-document.
- **Text.** Soft-wrap caret affinity; code coloring discarded above 4,096 tokens (keep the prefix); honor
  `GetCaretBlinkTime`, caret timeout and reduced motion; `Grid` row-snap loss of small wheel deltas.
- **Tooltips.** Do not invalidate while only the pending origin moved (finding 2).

### 6.2 Say what "best" means, then measure it

Proposed targets; each is a proposal until a receipt confirms it is attainable.

| Budget | Proposed target | Proved by |
| --- | --- | --- |
| Micro-change frame (caret, hover, one value) at 4K, 192 DPI, hardware GPU | UI-thread CPU ≤ 1 ms; work proportional to the changed controls | Cycle-time and counter receipt |
| Full repaint of a demanding screen (2,000 controls) at 4K | ≤ 4 ms CPU, GPU time below CPU time | Experiment 1 |
| Continuous scroll of a 100,000-row `Grid`, 10 s at 120 Hz | 0 missed frames | Presented-frame capture |
| Idle | 0 wakes with nothing armed; ≤ 20 wakes in 10 s with a focused text field | Wake counter |
| Idle native window, 1080p | ≤ 8 MB private above process entry; when minimized, video memory ≤ 8 MiB | Private bytes, `QueryVideoMemoryInfo` |
| Per control | ≤ 256 B all-in for a simple control; ≤ 1 KB per visible single-line `Grid` cell | `static_assert`, live-byte counter |
| Text history | ≤ 4 MiB per editor, capped by bytes | Tracked bytes |

Minimum method: at least 1,000 frames or 2 s per run; about six judged metrics instead of 26; thread cycle time for
CPU; D3D11 timestamp queries read one frame late for GPU; presented-frame capture on a `WindowHost` fixture; a hardware
GPU for every timing verdict, WARP only for pixels and counters; equivalence testing against a declared margin; the
noise floor calibrated per metric. Six fixtures: large-list scroll, live resize to 4K, motion-on transitions, a 64 Ki
rich document, 4K with 2,000 controls, and whole-application idle; plus cold start. This is input to D-P1, which keeps
ownership of the policy.

### 6.3 Cheap changes with the largest effect

In descending order of expected effect per unit of risk. Each needs a counter or timing receipt before and after.

1. Retain text layouts inside `DrawCenteredText` with the existing two-generation cache (finding 3).
2. Remove the `document ==` condition from image and SVG reuse; add content keys for math and diagrams (finding 4).
3. `ComboBox`: measure each item width once per items and typography revision; make `GetPopupItemRect` pure (finding 1).
4. Embedded partial preparation with clip culling first (`P3.2` after `P3.4`).
5. Deadline scheduling, the shared caret clock and tooltip deadlines (`P2.1`–`P2.3`), merged with the tooltip state
   machine in explicit state machines.
6. `Grid`: cache groups and a prefix array of row offsets per data change; cache column descriptions (finding 9, `E24`).
7. Remove the whole-document flatten sites including text-input sync (finding 6); position-independent chunks
   (finding 8).
8. Format cache: stamp entries by frame and hold formats by `com_ptr` in every retained key, then delete the forced
   re-preparation; reusable brushes with `SetColor` (finding 12).
9. UI Automation: a label index built in the existing count pass, a kind tag in place of the cast chains, one text
   copy, and gating on advise counts (finding 14).
10. Popups: blur the static capture once at open, pre-render shadows per popup size, borrow the embedded host's device.

### 6.4 Decide the architecture by experiment

Run the two experiments in §3.3 before `P3.1`, `P3.3`, `P4.1`, `D4` or any layer or batching work is designed.

### 6.5 Split the plan

| New home | Content |
| --- | --- |
| Correctness and conformance | §6.1, the real parts of `P1.6`, `P1.12`, `P1.15`, `P6.2` affinity |
| Measurement | `P0` redesigned (§3.7), §6.2 |
| Performance (this plan, reduced) | §6.3, `P2`, `P3`, `P4.2`–`P4.5`, `P5`, the rich-document items that survive, `P7` re-scoped around incremental publication |
| Motion and visual | `V1`, `V2`, `V4`, `V6` physics, `V7`, `V8`, bevel rims, under the design-system decision |
| Features and input | `B3`, `B5`, `B6`, `P5.4`, selection semantics, pointer, touch and DirectManipulation |

## 7. Item dispositions

`Keep` = as written. `Rewrite` = right target, wrong text. `Shrink` = keep the named part. Reasons are in §3–§5.

| Item | Disposition |
| --- | --- |
| `P0.1` | Rewrite: keep counters; replace the source lint with counting wrappers or receiver-independent matching; drop the 1% gate |
| `P0.2` | Shrink to about eight scenarios; add the macro fixtures in §6.2 |
| `P0.3` | Keep; read the presented image or the embedded texture, freeze the clock, pin or exempt effects, prove the drop-state hook clears every new cache |
| `P0.4` | Rewrite: observe the presented image; expected result reversed |
| `P1.1` | Rewrite: per-item width cache; add the all-rows-fit case; gate on width-measurement layouts only |
| `P1.2` | Shrink: device borrowing, retained brushes, blur once at open, pre-rendered shadows. Drop the Solid gradient skip, the pre-show render removal, the `HBITMAP` item and the threading flag |
| `P1.3` | Shrink: remove the copies; steady caret instead of none on read-only fields |
| `P1.4` | Keep; choose one wrap width as a visual change; include the two paint-path layouts |
| `P1.5` | Rewrite: nothing structural during preview, one build on commit |
| `P1.6` | Shrink to the conformance fixes and an advise-count gate; drop the skipped flush; value updates need structural sharing |
| `P1.7` | Rewrite against real symbols: group and offset cache, row-snap fix, `Tree` id index, selection epoch. Drop in-place `SetMaxWidth` and lightweight expansion records; move selection semantics to features |
| `P1.8` | Lower: C ≤ 20 children, F ≤ 256 fields, per layout pass; revision-based revalidation is a use-after-free risk for little gain |
| `P1.9` | Shrink to prefix sums in `TryVisibleRange` and one path geometry per flowchart edge. Drop the lookup table, capacity retention, GIF fast path, Wang's formula, single-pass SVG and the already-cached background; move the ink alpha to visual |
| `P1.10` | Keep prefix retention; defer the lexical-state cache |
| `P1.11` | Rewrite: gate on text layouts per edit; share number layouts across fences and edits |
| `P1.12` | Shrink to the bugs in §6.1 and a bounded device-loss backoff. Drop the terminate-handler rewrite, the modality item and the retry-counter wording; probe before touching `WM_ACTIVATE` |
| `P1.13` | Keep; the cap is 16 MiB, not 32; relax the peak gate for GIF canvas and orientation |
| `P1.14` | Shrink to setter guards, `CardPanel`, one-layout elision with cluster metrics, `PushLayer(nullptr)`. Drop the surrogate fix, the saturation fix and the `pow` table |
| `P1.15` | Shrink to the conformance fixes and a shared immutable text buffer. Drop the lock-result gate, undo coalescing and the clipboard timer |
| `P2.1` | Keep (native only); name its fixture; coordinate with explicit state machines |
| `P2.2` | Keep without read-only suppression |
| `P2.3` | Merge into the explicit-state-machines tooltip machine; add finding 2 |
| `P2.4` | Keep; it can ship as API revision 6 alone |
| `P2.5` | Measure first: no ledger evidence |
| `P2.6` | Keep; fix the gate fixture |
| `P2.7` | Shrink to the waitable with the flag on `ResizeBuffers`, credit tracking, a timer for ticks without a present, and wait-before-render on every path. Replace the averaged delta with predicted present time. Cut the compositor clock, DWM sampling, VRR policy and power broadcast |
| `P3.1` | Keep after experiment 1; region inside `ControlHost`; state-dependent overhang; give it a gate |
| `P3.2` | Keep, after `P3.4` |
| `P3.3` | Shrink to clamp, skip-empty and full-present fallback; add the scroll rectangle; no history union |
| `P3.4` | Keep; it precedes `P3.2` |
| `P4.1` | Replace with retained layouts (§6.3 item 1); glyph-run replay only if a profile shows what is left |
| `P4.2` | Keep; hand the measured layout to paint; gate on DxUi-owned allocations |
| `P4.3` | Keep for unshared, fully shaped layouts; exclude `Grid`; gate with a timing comparison |
| `P4.4` | Drop |
| `P4.5` | Rewrite as §6.3 item 8; leave `Typography.h` alone |
| `P4.6` | Shrink: the default already does what `D8` describes; drop linear-gamma mask parameters; an embedded host's monitor comes from the application |
| `P5.1` | Rewrite: one measure cache in `Control::Measure`; monotonic reuse only as an opt-in per type |
| `P5.2` | Keep, generalized: an unchanged measured size stops propagation |
| `P5.3` | Keep; add the position-only relayout fix |
| `P5.4` | Move to features |
| `P6.1` | Split: reuse across edits first (§6.3 item 2); flatten sites; table carry and the per-cell compare. Drop the slab, per-piece monoid, in-place mutation and the CRLF pad unless a fuzz test shows drift. Add block-incremental semantic projection, which is missing |
| `P6.2` | Keep: position-independent chunks with Enter in the gate; affinity now |
| `P6.3` | Keep, extended to the semantic document |
| `P6.4` | Keep |
| `P6.5` | Rewrite: keep eager parsing, cache font data per font across generations |
| `P7.1` | Drop: duplicate of `P1.5` and `P1.6` |
| `P7.2` | Keep; prerequisite of `P1.5` |
| `P7.3` | Rewrite around incremental publication; drop the popcount slab and hash sizing |
| `P7.4`, `P7.5` | Drop |
| `P8.1` | Shrink to `static_assert` probes, the allocator header and a null-means-default cold block if measured. Fix the table |
| `P8.2` | Drop: already decided |
| `P8.3` | Rewrite: cache column descriptions; delete the nonexistent sort |
| `V1` | Move to the motion plan as an animation core, after the pacer |
| `V2` | Defer; fix premultiplied interpolation instead |
| `V3` | Keep hairlines (add 100%); drop the second opacity tier |
| `V4` | Lower: cached nine-slice first |
| `V5` | Keep the capture retirement; solid fill is the tested default below build 22621; embedded blur only in preparation |
| `V6` | Physics to the motion plan; DirectManipulation to the input plan |
| `V7` | Rewrite with a bitmap snapshot; fix the per-frame layers first |
| `V8` | Keep; snap the whole subtree |
| `B1` | Rewrite or drop: the library has no animation clock and failed sources are already not retried |
| `B2` | Corner mask keep; rims to the design-system decision |
| `B3`, `B5`, `B6` | Move to features |
| `B4` | Drop |

## 8. Reconciliation with the frontier plan

The [frontier plan](Frontier_2026-11-11.md)
([RedSalamanders/DxUi#254](https://github.com/RedSalamanders/DxUi/pull/254)) challenged this plan from a 16-row sample
and four external checks. This review covered all 84 rows and 123 citations without seeing it.

| Topic | Frontier plan | This review |
| --- | --- | --- |
| `Present1` (`E7`) | Backwards | Confirmed independently, with one addition: its own `F0.4` must also observe the presented image, not the back buffer |
| Timing abandoned | Fix it rather than accept it | Agrees; adds why the "71 of 72" figure is a misreading and a concrete method |
| Makepad paths, BUI, features filed as efficiency, plan size | Found | Confirmed, with counts |
| Ledger reliability | 10 of 16 hold | 156 of 269 sub-claims hold |
| `E51`, `E35`, `E14`, `E59`, terminate handling | Overstated | Agrees |
| `E62` IME copies | Overstated: 6–10 | **Disagrees**: 21 copies traced line by line in one lock |
| Device flags | "Flags are right" | **Disagrees** on `PREVENT_INTERNAL_THREADING_OPTIMIZATIONS` (§3.2) |
| `P1.8` O(N²), `P8.1`–`P8.3` | Wave 1; keep | **Disagrees**: immaterial at C ≤ 20; `P8` is the smallest line and half of `P8.3` does not exist |
| `P1.15` | Wave 1 correctness | Partly: the lock-result claim is a misreading; the undo item is already done |
| Shared device lease | Wave 1 | A popup borrowing the embedded host's device avoids the second device |
| Retained command lists (`F1`) | Adopt subject to a spike | Not re-opened. One input: retained text layouts (§6.3 item 1) and the reuse fix (item 2) should land **before** the spike is measured, or the spike will be credited with gains a small cache also delivers |
| Missing from both | — | The findings in §5, the memory model in §3.4, the fixes in §3.6 |

## 9. Decisions for the developer

- [x] **RD1. Where this review lands.** Options: (a) keep it as this record and correct the plan's ledger and items after
  the frontier pull request merges; (b) apply the corrections on top of that branch now; (c) fold it into the frontier
  plan's challenge ledger. Confirmed on 12 November 2026: (a). The frontier pull request merged the same day and the
  corrections were applied on top of it, for fact only: every ledger row carries its re-read verdict and materiality;
  the verdict section, risk register, item text and external-citation sections state what the code and the cited
  sources do; `D3`–`D12` and each item whose premise was refuted carry a `Challenged (12 November 2026)` note. No item
  was removed, re-ordered or re-gated; that waits for `RD4`–`RD7`. Line numbers this review cites for the plan refer
  to the text at `315b7f7a`.
- [ ] **RD2. Targets.** Adopt the budgets in §6.2 as proposals to be confirmed by receipts, and name the hardware.
- [ ] **RD3. Experiments first.** Run the two experiments in §3.3 before any dirty-region, glyph, layer or batching
  design work.
- [ ] **RD4. Split.** Split the plan as in §6.5.
- [ ] **RD5. Fix-now list.** Authorize §6.1 as independent changes that do not wait for `P0`.
- [ ] **RD6. Withdraw or rewrite.** Withdraw the asymmetric `Present1` design and rewrite `P0.4`; withdraw the items in
  §3.6 as written; remove unverified figures from gates (§3.8).
- [ ] **RD7. Memory.** Replace the `P8` gate with size probes and live-byte counters, add a video-memory budget and trim
  for owned devices, cap text undo by bytes, and treat the toggle split as a revision-6 question.

## Appendix A: ledger corrections

One row per ledger row. Verdicts: `Holds`; `Holds, corrected` (true in substance; a number, symbol, citation or scope is
wrong); `Overstated`; `Partly wrong` (at least one sub-claim is false); `Agreed refuted` (the plan already refuted it);
`Needs measurement`. `(AT only)` marks a cost paid only while a UI Automation client is attached. Paths without a
directory are under `src/Controls/`. `Source` names the review pass: 01 hosts, rendering and scheduling; 02 text fields,
combo, popups and menus; 03 Grid, Tree and layout; 04 UI Automation and TSF; 05 semantic geometry and media; 06 rich
document core; 07 rendering and platform facts; 08 animation and features; 09 measurement and scope; 10 memory; 11 text
and layout design.

| Row | Ledger says | Review verdict | What is actually true | Materiality | Source |
| --- | --- | --- | --- | --- | --- |
| E1 | Verified (effective ~64 Hz) | Holds, corrected | SetTimer clamps 8 ms up to 10 ms (`USER_TIMER_MINIMUM`). ~64 Hz assumes a 15.625 ms tick, which nobody verified. It is not an idle poll: the timer stops when no `Tick` returns true (`src/Support/AnimationDispatcher.h:331-391`). It runs continuously only while some control ticks forever (E2). | High for motion quality, Low CPU (per frame) | 01, 08 |
| E2 | Verified | Holds, corrected | TextField and ComboBox return true only while focused (ComboBox only if editable). The tooltip show delay is `SPI_GETMOUSEHOVERTIME` clamped to 100–2,500 ms; 500 ms is only the fallback (`DxUi.WindowHost.cpp:499-507`). The read-only caret is required by `UI_ControlsAndLayout.md:491`, so it is not a bug. | Medium idle power; None FPS | 02 |
| E3 | Verified (RichTextEditor honors reduced motion; `SPI_GETCARETTIMEOUT` appears nowhere) | Holds | Zero hits for `GetCaretBlinkTime` and `SPI_GETCARETTIMEOUT` in `src`. The TextField and ComboBox `Tick` functions never read `reducedMotion` (`DxUi.TextInput.cpp:1787-1813`). | Medium (accessibility); efficiency gain only through E2 | 02 |
| E4 | Verified | Holds | `Panel::Tick` visits every visible child (`DxUi.Controls.cpp:1571-1585`). Both native `OnAnimationTick` and embedded `AdvanceAnimation` call it. | Medium on large trees with a blinking caret (per frame) | 01, 08 |
| E5 | Verified | Holds | `dc->Clear` is followed by an unclipped `_root->Paint` (`src/Rendering/Embedded.cpp:631, 641`). The texture persists while the size is unchanged, so partial repaint is feasible. | High (per frame) | 01, 07 |
| E6 | Verified | Holds | `InvalidateRect(_hwnd, nullptr, FALSE)` (`DxUi.WindowHost.cpp:1912`); the other `InvalidateRect` calls are full-window too (`DxUi.Menu.cpp:1631, 4243`). As a result the partial-present path almost never runs. | High (per change) | 01, 07 |
| E7 | Unproven and probably false | Partly wrong | Before present, the back buffer outside `dirty_N` holds frame N−2. `Present1` then copies `dirty_{N−1} \ dirty_N` forward, so the displayed frame is correct. The comment at `DxUi.WindowHost.cpp:5071-5073` gives the wrong reason. Only pre-present readers (capture, V5 blur) see stale pixels. P0.4 reads at the wrong time. | None for FPS; correctness risk for pre-present readers only | 01, 07 |
| E8 | Verified | Holds | `Panel::Paint` tests only `IsVisible` (`DxUi.Controls.cpp:1535-1545`). `ScrollPanel` clips, then paints without culling (`:11311, 11326`). D2D clips the GPU work, but the CPU paint still runs for every child. | Medium–High for long scroll content (per frame) | 01, 07 |
| E9 | Verified, and cleared twice | Holds, corrected | The cache is cleared at `DxUi.WindowHost.cpp:4612` and again at `:4552`/`:4687`. The second clear runs on an already-empty map and costs nothing. Brushes are device-context resources, so neither clear is needed. | Low–Medium (per live-resize frame) | 01, 07 |
| E10 | Refuted | Agreed refuted | The caches are `unordered_map`s (`include/DxUi/DxUi.h:6721, 6746`). The 256/96 limits and the clear-on-overflow behavior hold (`DxUi.h:6364-6365`, `DxUi.WindowHost.cpp:4671-4683`). | None for the scan; for the cliff see E72 | 01 |
| E11 | Overstated | Holds, corrected | There are 7 raw `DrawTextW` sites. `DrawCenteredText` has 61 callers, not 63 (the count included the declaration and the definition). `CreateTextLayout` sites: 28 lines (02), or 30 when GDI-compatible calls and `Typography.h` are included (11). The MS docs say `DrawText` lays out the whole string on every call. | Medium–High (per frame) | 02, 11 |
| E12 | Verified | Holds | Each `UpdatePopupLayout` call creates M DirectWrite layouts (`DxUi.ComboBox.cpp:2647, 2689-2700`), and 8,500 holds (≈8,508). The ledger understates: when all M > 8 rows fit, V = M, giving (1+2M)·M layouts per paint (20,100 at M = 100). Each uncaptured `WM_MOUSEMOVE` makes 3 calls. | High for M of ~30 or more (per frame, per input event) | 02, 09 |
| E13 | Verified | Partly wrong | Per-paint blur, `CreateLayer` (ComboBox and Menu), geometry, gradients, and 2 shadow command lists with effects all hold. `PaintTransientSurfaceBackdrop` creates no layer (`DxUi.cpp:198` passes `nullptr`). The blur runs only with a capture and a non-Solid material. Solid gradients are visible pixels, so skipping them changes output. | Medium responsiveness; None steady FPS (per popup paint) | 02, 08 |
| E14 | Refuted per-icon; lifecycle & device churn verified | Partly wrong | Device churn happens once per menu session, and only when no other native host is attached on that thread (`DxUi.WindowHost.cpp:716-722`). The omitted flags hold; "16–32 threads" has no source. `NativeMenuInterop.cpp` contains no `HBITMAP` cloning. The WIC factory is released after `CoUninitialize` (holds); no crash is verified. | Medium (per open) latency; None steady FPS | 01, 07, 09 |
| E15 | Verified | Holds, corrected | The synchronous `BitBlt(CAPTUREBLT)`, the heap vector (plus an extra full DIB copy) and the alpha loop all hold (`DxUi.cpp:305-342`). `PREMULTIPLIED` explains only the loop. The vector exists because the D2D bitmap is created lazily at paint. A 300×400 popup peaks at ≈1.44 MB transient. | Medium (per open) latency; Low memory | 02, 10 |
| E16 | Verified | Holds | `SecureClear` plus a copy runs on both hit and miss (`DxUi.SingleLineTextEditing.cpp:527-540`). Every field takes the secure path. The `NativeTextInputState` is deep-copied twice per paint, even with no composition. That is ≈6–8 heap allocations per focused paint. | Low CPU (per frame); breaks the zero-allocation rule | 02 |
| E17 | Verified | Holds | There are 15 uncached layout sites; "10" counts paths. The 2 layouts before `wheelStepCount == 0` and the 5 per PageDown hold (`DxUi.TextInput.cpp:2180-2203`). Missed: every focused paint builds 2 more uncached full-document layouts (`:811, 766`). | Medium–High for large multiline text (per frame, per input event) | 02 |
| E18 | Refuted | Agreed refuted | `TagPicker::Paint` does no concatenation (`DxUi.Controls.cpp:7542-7576`). Side cost: `LayoutParts` measures each tag twice per bounds change. | Low | 02 |
| E19 | Verified | Holds | `Splitter::NotifyChange` triggers a full `General` refresh (`DxUi.EditorControls.cpp:730-750`), about 1 build per dispatched `WM_MOUSEMOVE`. The build runs only when `providerRequested` or `UiaClientsAreListening()` (`DxUi.Accessibility.cpp:13785-13794`). | High (AT only) during drag | 04 |
| E20 | Verified | Holds, corrected | Each value change runs `RequestInvalidate` plus a gated `General` rebuild (`DxUi.Controls.cpp:5186-5198, 6467-6474`). "4-pass" holds: 3 tree walks plus an index. "Reserves 4× maps" is the `bit_ceil(4n)` hash load factor (`DxUi.Accessibility.cpp:864`), not a pass. | Medium (AT only) (per input event) | 04 |
| E21 | Verified | Holds | `Label::Label` enables preparation (`DxUi.Controls.cpp:2810`). The O(T) `IsControlInTree` DFS runs before the cheap guard (`:2970`). For Button this applies only to the IconOnly, Hyperlink and shortcut variants. | Medium during resize; None steady | 02 |
| E22 | Verified | Holds | Each Grid key press calls `CollectVisibleRowIndices`, which reserves and fills all rows (`DxUi.Grid.cpp:576-607`): 1.6 MB per key at 200k rows. The Tree double lookup holds. The real cost is the default `FindVisibleItemById`: O(N) with 4 string copies per item (`DxUi.Tree.cpp:279-295`). | Medium (per input event) | 03 |
| E23 | Verified | Holds | All five O(N²) shapes exist (`DxUi.Forms.cpp:747-759`, `DxUi.Layout.cpp:1053-1059`). FlexPanel `BuildLayout` is quadratic only on uncached passes. N is ≤ 20 children or ≤ 256 fields, and the cost is per layout pass, not per frame. | None–Low | 03, 11 |
| E24 | Verified | Holds | `GetColumn` runs once per visible header and once per visible cell (`DxUi.Grid.cpp:4155, 4435`); C is the visible column count. Each call returns `GridColumnDesc` by value with 2 `wstring`s: 40×8 gives 320 calls and up to 640 heap allocations per frame. | Medium (per frame) | 03 |
| E25 | Verified | Holds | Image and native-diagram reuse are gated on `document ==` (`DxUi.RichSemanticGeometry.cpp:2437, 2447`); text-box reuse is not. Stronger than stated: every raster image re-uploads (up to 64 MiB) and every SVG re-rasterizes per keystroke. Tables not re-verified. | High (per edit) | 05 |
| E26 | Verified | Partly wrong | Per-line code-number layouts hold (capped at 4,096; `DxUi.RichSemanticGeometry.cpp:2924-2944`), and so does one badge layout per fence. `RichSourceGuides` is windowed to the viewport, at most 4 per Advance. There is no `"copy"` layout: the affordance is drawn with rectangles and lines (`:4051-4057`). | Medium–High (per edit) in code-heavy documents | 05 |
| E27 | Verified | Holds | `output.clear(); return false;` at `DxUi.RichCodeInk.h:103-107`, with `MaximumCodeTokens = 4096`. Only keywords, numbers, strings and comments count as tokens. | None FPS (correctness) | 05 |
| E28 | Verified | Partly wrong | The 32 KiB zeroing (colored formulas only) and 6 `pow` calls per colored op hold. `ResolveDiagramLabelBackground` is cached and runs only on theme change (`DxUi.RichTextEditor.cpp:4199-4207`). It uses double `pow`: 192 calls, 222 at worst. `GetFactory` per draw holds. | Low / None | 05 |
| E29 | Verified | Overstated | TextField has no contrast call (0 hits). TagPicker calls it once per paint, not per tag (`DxUi.Controls.cpp:7555`). The per-edge flowchart contrast is real. ComboBox not re-verified. | None | 05 |
| E30 | Verified | Holds | The loop starts at line 0 and calls `HitTestTextPosition` per line (`DxUi.RichTextLayout.cpp:768-781`). It runs up to 4× per prepare in Source view. The semantic box scans hold; each box test is cheap. | Medium for large source documents scrolled deep (per prepare) | 05 |
| E31 | Verified | Holds | A fresh vector is swapped in (`DxUi.PaintIndex.h:30-52`). The index lives in a per-generation `State`, so capacity reuse is impossible without pooling. | None | 05 |
| E32 | Verified | Holds | `DxUi.RichImage.cpp:158-166`. It runs once per decode, off the UI thread, and only when frame 0 is smaller than the logical screen. | None | 05 |
| E33 | Verified | Holds | `LoadFile` reads into `bytes` (`DxUi.RichImage.cpp:506-518`) and `Decode` copies them again (`:251-255`). The cap is 16 MiB, so peak encoded memory is 32 MiB. | Low (per open) | 05 |
| E34 | Not found | Agreed refuted | Only `CHANGELOG.md:11` and the plan mention 6.10 ms. No fixture or receipt exists. | None (unmeasured) | 02 |
| E35 | Verified static layout; no test probe yet | Partly wrong | Hand count: `Control` is 296 B in Release (336 B Debug), not ~328. The header is 24 B plus 15 B of slack (`ControlAllocation.cpp:17-22`). Button is 816 B, Toggle 904, Checkbox 912, RadioButton 864. The ledger's Checkbox < Toggle is impossible because `Checkbox : Toggle` (`DxUi.h:2401`). | Low memory (~21 KB in ComplexUi, below the 315 KB noise) | 10, 09 |
| E36 | Verified, 460 call sites | Holds | `make_shared<int>` at `DxUi.h:1853`: about a 32 B block plus heap header per control (estimate). The 460 call sites were not re-verified. | Low memory | 10 |
| E37 | Verified dead | Holds | `DXUI_ENABLE_DIAGNOSTICS 1` (`include/DxUi/Configuration.h:4`), so ~240 lines at `DxUi.WindowHost.cpp:4708-4952` are dead. | None (maintenance) | 01 |
| E38 | Verified open | Holds | HOST-03: a Ctrl edge causes 2 dirty prepares on a clean view (`Embedded.cpp:1055-1060`). HOST-09: text input is activated for any focused control with no `SupportsTextInput` check (`:664-666`). | Low (per input event) | 01 |
| E39 | Verified; embedded forces grayscale | Holds | The only `SetTextAntialiasMode` is at `Embedded.cpp:283`, and there is no `SetTextRenderingParams`. The consequence is nil: D2D's defaults already give ClearType on IGNORE HWND chains and grayscale on premultiplied composition (`DxUi.WindowHost.cpp:4448, 4541`). | None (quality) | 01, 07, 11 |
| E40 | Verified | Holds | `BlendColor` lerps gamma-encoded components with straight alpha (`DxUi.Theme.cpp:198-202`). Today's animated colours are near pairs, where OKLab looks the same. The real fix is premultiplied lerp. | None (visual) | 08 |
| E41 | Verified | Partly wrong | "No springs" holds. Wrong speed holds only for `Slider`, which restarts the full 100 ms (`DxUi.Controls.cpp:6619-6641`). Button, Toggle, Checkbox and the scrollbar are constant-rate linear: they keep speed but reverse velocity instantly. | None (visual) | 08 |
| E42 | Verified | Holds | An accent rounded rect is drawn over `LerpRect(source, target)` (`DxUi.Controls.cpp:1968-1980`). | None (visual) | 08 |
| E43 | Verified | Holds | A 1 DIP stroke is centred on snapped integer edges (`DxUi.Controls.cpp:554-562, 648-656`). Understated: it also blurs at 100% (inference; no capture taken). | None FPS (visual correctness) | 08 |
| E44 | Verified | Partly wrong | Grid floors float deltas to a row boundary (`ClampScrollOffsets` with `normalizeVertical = true`, `DxUi.h:5525`; floor at `DxUi.Grid.cpp:7311`), so it is row-quantised. The ledger's mechanism and fix location are wrong. Tree already accumulates `_wheelDeltaRemainder`. "No inertia" holds. Edge fades not re-verified. Both ignore `SPI_GETWHEELSCROLLLINES`. | None FPS (correctness, UX) | 02, 03 |
| E45 | Verified; Windows 10 is in the embedded baseline | Overstated | This applies only to `ScriptTextRenderer` super/subscript runs (`DxUi.RichTextScriptRenderer.h:217-220`). Ordinary text uses `DrawTextLayout(..., ENABLE_COLOR_FONT)` (`DxUi.RichTextLayout.cpp:834`). The `ID2D1DeviceContext7` OS minimum is not verified. | Low | 05, 11 |
| E46 | Verified | Holds, corrected | Every number recomputes: 46,080 B bitmap, 70,696 B of stack, 408 B sample, 73,440 B per graph, HueWeight 24 → 16 B by reordering. The brush is unused only on the band path, and the raster is not HSV. The cited `DxUi.h:4714-4729` is `FlexPanel::LineRecord`. The struct is at `DxUi.h:2584-2602` with layout `{float, double, uint8_t}`. | Low (niche control) | 03, 09, 10 |
| E47 | Verified | Partly wrong | The binary-search layouts hold: 12 layouts and 10 strings at n = 260 (`DxUi.Controls.cpp:10398-10426`). Surrogate splitting is wrong: the final cut skips trail surrogates (`:10429-10432`). Grapheme splitting holds. It runs only for `leadingEllipsis` sections. | Low (per frame) | 03 |
| E48 | Verified | Holds | `CreateLayer` at `DxUi.Controls.cpp:1803, 1936`, up to 4 per transition frame; `PushLayer(nullptr)` is valid from Windows 8. Missed: two full-bounds overlay layers are pushed even when there is no overlay content (`:1953-1959`). | Low (per frame, transitions only) | 03, 08 |
| E49 | Verified | Partly wrong | The picker draws only 3 texts itself. UIA rebuilds are gated; "4 per move" is unverified. Saturation at V = 0 is preserved (`DxUi.EditorControls.cpp:188-193`). There are 6 heap label strings, not 7, and `Labels` is 460–464 B. `NumericStepper` lacks a guard and calls `pow` 3× (holds). The `std::format` result fits SSO. | None (niche control) | 03, 10 |
| E50 | Verified | Holds | 28 unguarded invalidating setters were counted (`DxUi.cpp` not enumerated). StackPanel invalidates unconditionally (`DxUi.Controls.cpp:10660-10701`). `CardPanel::SetCornerRadius` never invalidates (`:2736-2739`). Whether redundant calls happen at steady state is unverified. | Low; CardPanel (correctness) | 03 |
| E51 | Verified | Partly wrong | There is no `weak_ptr::lock`. Each child costs 2–3 `expired()` loads plus one `weak_ptr` copy (`DxUi.Controls.cpp:76-127`). That is 2N+2P RMW ops per traversal and ~6N per frame, not 4N: ≈0.17 ms at 5k controls (inference). | Low (per frame) | 02, 09 |
| E52 | Verified | Partly wrong | `Debug::Info` does no formatting when no sink is installed (`src/Support/Diagnostics.h:27-28`). The MenuBar resize re-measure and the MenuPopup double render (`DxUi.Menu.cpp:4224, 4243`) hold. Each row makes one `ParseMenuLabel` call, because `:3224` and `:3244` are exclusive branches. | Low–Medium (per open) | 02 |
| E53 | Verified | Partly wrong | `PaintBadgeCell`, `ResolvedColumnLayout`, `ComputeViewportMetrics`, `InMemoryGridModel::RebuildView` and `GridCellValue` do not exist. Badges go through `DrawCenteredText` every frame. A column resize re-creates ~R layouts (overstated). About 10 `CollectOrderedGroups` calls per frame; the real group cost is O(G²) via `GetRowTopDip` (`DxUi.Grid.cpp:7077-7114, 7393`). | Medium; High for grouped Grids (per frame) | 03, 10 |
| E54 | Verified | Holds, corrected | Expansion copies ≈3N+3N' records, not 2×: 24,576 at 4,096 items (`DxUi.Tree.cpp:3599-3602, 3660`). The hover repaint holds, but the trigger is the tooltip origin following the pointer (`DxUi.Controls.cpp:11891`), not sub-zones. | Low–Medium (per click); hover Medium (per input event) | 03, 08 |
| E55 | Verified | Partly wrong | The ScrollPanel boundary repaint and swallow, the embedded chord capture drop and the ignored `MiddleUp` (native too) hold. `_lastMousePos` does not exist, and `WM_MOUSEMOVE` never calls `SetInputModality` (`DxUi.WindowHost.cpp:3543-3569`). | Low–Medium wheel; None (correctness) | 01 |
| E56 | Verified | Partly wrong | `HandleDeviceLost` and `_deviceLossRetryCount` do not exist, and there is no retry counter. A failed recreation leaves the window blank (`DxUi.WindowHost.cpp:5045-5050`). STRETCH holds and is mandatory for composition chains. Each outermost message makes 2 `set_terminate` calls. The tooltip function is `UpdateSupplementalTooltipTarget`: 3 copies, 1 SPI call. The HWND is kept while any host is attached. The +8 ms frame-1 dt holds. | Low (per input event) | 01, 07 |
| E57 | Verified | Partly wrong | The mutex, heap `wstring` and `GetSystemFontCollection(TRUE)` hold, but only on format-cache misses and at init, not on the paint path (`include/DxUi/Typography.h:194-294`). There is no `vector<DWRITE_FONT_AXIS_VALUE>`. Entries own `com_ptr`s and pin the factory: no raw pointers, no use-after-free, at most 28 entries. | Low | 11, 10 |
| E58 | Verified | Partly wrong | The +39 B is a 24 B header plus 15 B of alignment slack. `HeapSize` is called only in `TryAllocate` (`src/Foundation/ControlAllocation.cpp:45`), never in `Free`. The Oklab ramp derivations were not re-verified. | Low | 10 |
| E59 | Verified | Partly wrong | The embedded rebuild runs once per application `UpdateAccessibility` call after a changed preparation, placement or focus, by opt-in contract; it does not run per mutation. The `providerRequested` latch holds (`DxUi.Accessibility.cpp:6377`). The "rebuilds twice" claim is wrong: the posted flush returns `S_FALSE` when revisions match (`:14758`). | Medium (latch; animated embedded scenes); otherwise None | 04 |
| E60 | Verified | Holds, corrected | The label DFS is also the fall-through for every unnamed control (`DxUi.Accessibility.cpp:4184`). About 45 casts per Label and 20 per Button. The record is 936 B (±8) by hand count, 1,088 B in Debug. Each build makes 2–3 text copies of up to 128 KiB. Tree rows are eager only when the model offers no prepared rows. | High (AT only) on large forms | 04, 10 |
| E61 | Verified | Holds, corrected | The word span from 0 and the by-value `ResolveText` hold (AT only); the size is 128 KiB, not 131 KiB. `expectedText` is moved, not copied. The un-gated per-keystroke work holds, and for every user it adds a full state copy per caret move (`DxUi.Accessibility.cpp:13657-13662`). | High (AT only) for long docs; Low for all users (per input event) | 04 |
| E62 | Verified | Partly wrong | About 19–21 copies per lock are provable, so "15–25" holds. The double import on commit holds and is deliberate (one undo step). The `GetTextExt` fallback is O(range), not O(N). Intermediate TSF strokes record no undo (`DxUi.TextStoreACP.cpp:349`; `DxUi.TextInput.cpp:3166-3167`). IMM32 path not re-verified. | Low for short fields; Medium for ≥64 Ki docs (per input event) | 04 |
| E63 | Verified | Partly wrong | The ASCII step has a fast path (`DxUi.SingleLineTextEditing.cpp:81-84`); the snap from 0 holds. Surrogate pairs are not split, but astral letters are misclassed as punctuation. The Ctrl+C double read holds but lives at `TextInputServices.cpp:893, 1083`. The `substr` and the uncached `RegisterClipboardFormatW` hold. | Low (per input event); None for clipboard | 04 |
| E64 | Verified | Holds | Each insert creates a new buffer (`DxUi.RichDocument.cpp:1000-1005`), so `AppendCoalesced` never fires. 100 typed chars give 102 pieces, ~2.3 KiB per keystroke; undo stays ≤ 283 KiB. | Low (per edit) | 06 |
| E65 | Verified | Holds, corrected | There are 14 editor flatten sites plus `TryFindInSource`, plus a missed host sync that runs 2× per keystroke. At the cap that copies 640 KiB per keystroke (`DxUi.NativeTextInput.cpp:1063, 1076-1077`), ≈65–130 µs. The Ctrl+A and highlight claims hold. | Medium memory churn; Low latency (per edit) | 06 |
| E66 | Verified | Overstated | The `source_` copy holds. The anchors are 1.00 MiB (1.05 MB) at the 64 Ki cap, and the whole map is 3.875 MiB. The edit envelope spans first to last edit, not the whole document. v0 waste is 3.5 MiB total, of which 1.75 MiB is duplicate. The map is rebuilt every keystroke (`DxUi.RichSemanticDocument.cpp:2608`). | Medium (per edit) | 06, 10 |
| E67 | Verified | Partly wrong | The 2–3 name copies and the math re-parse hold, and YAML is revalidated on every parse (`DxUi.MarkdownSyntax.cpp:446-449`). The `multimap` is the old-node identity map (`:368, 417`), not part of YAML validation. Mermaid, the libfyaml DOM and `MarkdownInlines` were not re-verified. | Medium (per edit) | 06 |
| E68 | Verified | Partly wrong | `MeasureTableOne` runs only in Subtle presentation and makes 1 layout per cell: ≈R·C per edit, ≈2·R·C−C on resize. No `state_->Clear()` call exists. Images survive width changes (`DxUi.RichSemanticGeometry.cpp:2436-2444`). Markers are dropped; "not viewport-first" holds. | High with Subtle tables (per edit); None in Default | 06 |
| E69 | Verified | Holds, corrected | The chunk skip holds only with the non-default `lineHeightMultiplier > 0` (`DxUi.RichDocumentLayout.cpp:338-344`). The `:445` claim holds and goes further: chunks bake in the absolute origin (`DxUi.RichTextLayout.cpp:308-340`). The cited `:5397` was not located; full-viewport invalidation holds via `:3663`. The Draw, QI and transform claims hold. | High in that config (per edit); Low for the raster parts | 06 |
| E70 | Verified | Overstated | The ~22,500 calls are in-memory MATH-table lookups, not DirectWrite calls; the real cost is the font face plus the table copy. Layouts are made per atom run, not per character (`E=mc^2` gives 4). Missed: one `DrawTextLayout` per leaf per paint (`DxUi.PreparedMath.h:3117`). | Medium (per edit, per frame) | 05 |
| E71 | Verified | Holds, corrected | Four of five segments are zero-length only on straight routes. `Preparation` is ~155 KiB. The SVG strip re-walks hold. Accessibility shares the document and copies only the full text (`DxUi.Accessibility.cpp:2410, 2423`); the trigger frequency is not verified. | Low–Medium (per edit) | 05 |
| E72 | Verified | Overstated | The mechanism holds (`DxUi.WindowHost.cpp:4678-4682`), and once triggered it repeats every frame. An ordinary screen uses ~10–40 keys; reaching 97 takes many typography overrides. No fixture has measured it. | Low normally; High when hit (per frame) | 01, 10, 11 |
| E73 | Verified | Needs measurement | The DComp/D3D coupling holds (`DxUi.WindowHost.cpp:834-835`). Passing `nullptr` alone changes nothing, because `:4660-4662` and `:612` still reset the target, visual and device. `WM_PAINT` starvation during size-move cannot be checked statically: there is no `WM_ENTERSIZEMOVE` handling. Probe: `WM_SIZE` count vs render count. | Low (device loss); Low–Medium (live resize) | 01, 07 |
| E74 | Verified | Partly wrong | The clamp applies only above 50 ms (`src/Foundation/FrameRuntime.cpp:125-135`), as stated. Native ticks come from `WM_TIMER`, not DXGI/DWM, and the 13/20 ms figures have no source. Nested layout scopes do no `set_terminate` (`Failure.cpp:282-290`): 0 per pass, not 6N–9N. | Medium motion quality; Low for the scopes | 01 |
| E75 | Verified | Partly wrong | The copy holds and is unneeded, but it is at most 16 MiB (`MaximumEncodedBytes`, `include/DxUi/RichImage.h:66`), not 32. Failed decodes are already embargoed while location and binding are unchanged (`DxUi.RichImageResources.h:250, 327`). | Low (per open) | 05 |
| E76 | Verified | Partly wrong | The rect is `rcPaint`, clipped to the client area, so the `E_INVALIDARG` risk is overstated; the missing fallback holds. At the cited `DxUi.Menu.cpp:4117-4120` the popup sets `usesSystemBackdrop = false`, so it uses no DWM backdrop today (`DWMSBT` needs Windows 11 22621). The duplicate IMM context on `WM_ACTIVATE` is unverifiable and likely wrong. | Low; None FPS | 01, 07 |
| E77 | Verified | Partly wrong | `TS_E_SYNCHRONOUS` for an async request that cannot be scheduled is a documented outcome. Async requests are queued and get `TS_S_ASYNC` (`DxUi.TextStoreACP.cpp:1235-1248`). The whole-document `TS_TEXTCHANGE`, the missing input scopes (password is moot), the dropped clauses and the `GetACPFromPoint` errors hold. | Low (correctness) | 04 |
| E78 | Verified | Holds, corrected | The `GetPropW`-only lookup leaks only when `WM_NCDESTROY` does not reach the host. The throw inside `noexcept` is a gap only where no caller `Scope` is active. The Win64 cast (`DxUi.Accessibility.cpp:13954`), Invoke+Toggle, the empty Composition payload and the dead code hold. Narrator double-speak is unverified. | Low (correctness, AT only) | 04 |
| E79 | Verified | Holds | `widthDip` in `SameLayoutOptions` (`src/Controls/DxUi.RichLayoutOptions.h:15`) forces a whole-document reshape on every resize step. `SemanticDisplayInput` copies every paragraph's display text before the reuse check (`DxUi.RichSemanticGeometry.cpp:1913-1916` vs `:1969`); report 11 did not re-verify this part. | High on resize; Medium (per edit) | 06, 11 |
| E80 | Verified | Holds | Each paragraph or chunk gets an uncached format (`DxUi.RichTextLayout.cpp:289-297`). The image scans hold but stay small (I ≤ 128). `TryAdjacentSourceLine` runs only when spacing options are set. There is no affinity parameter (`include/DxUi/RichTextLayout.h:259`). | Low; affinity (correctness) | 06, 11 |
| E81 | Verified | Holds | One `ID2D1DeviceContext` per distinct SVG raster; identical placements share one (`DxUi.RichSemanticGeometry.cpp:448-455, 475`). The Document+Project per code line holds. Pie makes 2N+1 `SetOpacity` calls. The plan miscites E81 for the split-CRLF pad. | Low–Medium (per edit); None for Pie | 05, 06, 09 |
| E82 | Verified | Overstated | The 1-slot caches exist; thrashing is unverified (probe with `DebugGetMeasureCount`). The second `MeasureParts` call hits the cache. Zeroing `_lines` is trivial. The grow loop is O(C²), not O(L·C²) (`DxUi.Layout.cpp:841-871`). | Low | 03, 11 |
| E83 | Verified | Holds, corrected | All mechanisms hold (`DxUi.Grid.cpp:6728, 1080, 6756`). 1,600,000 B is 1.53 MiB, not 1.6 MiB. Missed: `SetSelectedRowIds` builds a second `unordered_set` (`:7027`). The Ctrl+Shift wipe is a behaviour defect, not a cost. | Medium (per input event) | 03, 09, 10 |
| E84 | Verified | Holds | `Slider::OnKeyDown` ignores modifiers (`DxUi.Controls.cpp:7101`). WinUI and trackbars use PageUp/PageDown for the large step, so this is a preference (inference). WindowHost dual-Shift polling was not re-verified. The plan miscites E84 for the arena and drag remainder. | None | 03, 09 |
