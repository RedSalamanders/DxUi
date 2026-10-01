# Performance and resources

Status: normative current contract
Last reviewed: 2026-10-01

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

## Mandatory performance policy

Speed and low memory consumption are primary library requirements for every design, implementation, review and test.
Among correct designs, minimize steady-state CPU/GPU time, private/resident memory, allocations and copies,
synchronization, thread/handle count, wake-ups and graphics submissions. Preserve correctness, accessibility,
security and visual quality; do not trade them away for an unmeasured optimization. All consumers share this policy.

Before changing implementation, measure the existing code with the same workload that will measure the candidate.
Retain the original baseline and measure again after the change. Record exact source/content and executable/fixture
fingerprints, compiler/dependencies, architecture/configuration, hardware/OS/driver, power policy, resolution/DPI,
visible controls/data size and repetitions. Run serially on the same quiet fixture. Cross-machine, changed-workload,
Debug/Release or x64/ARM64 comparisons cannot establish non-regression. If a new benchmark is required, run the same
harness against both implementations; `performance-paired.ps1` measures a baseline (a revision in a detached
worktree, or an existing tree named as it is) and a candidate (this checkout, a revision or a named tree) with this
checkout's harness inputs and runs the interleaved A/B/B/A pass serially, repeated as a [paired set](#paired-sets).
It refuses a pair with nothing to compare: two revisions that name one commit, or, when a tree is named, two
identical library source fingerprints; each side's revision or path, commit and fingerprint are recorded. A missing
baseline is explicitly unpaired and cannot close an implementation performance gate. Documentation/tool-only changes
that leave compiled library inputs unchanged record that fact.

DxUi MUST NOT accept a confirmed performance or memory regression silently. Compare FPS, frame/input percentiles,
preparation and composition costs, allocations/bytes, surface/cache residency and peaks, private bytes/working set,
resource counts, idle activity and long-run retention. Keep all samples and investigate repeatable degradation,
including trends smaller than automated noise bands. `performance.ps1` compares five-round medians, with 5% timing/FPS
and 2% process-memory investigation bands; deterministic surface/allocation budgets allow no growth. A
[paired set](#paired-sets) applies the same bands to repeated runs. Those bands address measurement noise, not a
permitted regression budget. Repeat suspicious results against the retained baseline. Never relax a threshold, omit a
workload or replace the baseline merely to make a test pass.

On confirmed degradation, stop accepting/merging the affected development, present the measured deltas and suspected
cause, ask the developer for advice, and propose options with quantified costs: optimize the affected path/caches,
reduce optional scope, or defer/revert the change. Any explicitly approved tradeoff needs durable rationale and the
chosen resource budget in this contract or its owning domain; a WIP note alone cannot waive a requirement.

### Paired sets

A single receipt pair cannot tell a change from a machine that drifts between runs; on 2026-09-30 all 14 same-binary
controls of one developer laptop drifted beyond their bands. A paired set can: `performance-paired.ps1` runs the
interleaved pass A, B, B, A serially, repeats it `-Repetitions` times (default 3) and judges every scenario phase
(clean, dirty) and metric on all runs at once. Each side's sample is its run medians, the median of each run's five
rounds, so 2N runs per side after N repetitions.

- The baseline and candidate samples are compared by an exact two-sided Mann-Whitney U test whose null distribution is
  counted over the observed ranks, ties averaged, never approximated. A metric is `regressed` (`improved`) only when
  p < 0.05 and the candidate's median of run medians lies beyond the metric's investigation band (5% timing and FPS,
  2% process memory) on the worse (better) side; otherwise it is `within-noise`. A shift inside the band, or beyond it
  without separation from noise, is not a verdict and stays in the retained values, and trends inside the bands
  remain subject to the investigation above.
- Exact budgets (surface bytes, replacement peak, composition and C++ allocations) stay exact. Any candidate run above
  the median of the baseline runs is `regressed`, whatever the rank test says, and no band applies.
- Each side's spread across its own runs is reported with the verdicts for context. It is not a veto: it cannot
  waive a regressed metric or invalidate the set. Runs of one side must be of one binary and library source, and both
  sides of one fixture, or the set is `invalid-evidence`.
- A set is `advice-required` when any metric is `regressed` and `within-noise-budget` otherwise. `advice-required` is
  a measured finding, not an acceptance: it confirms on a repeat on the same quiet fixture and then follows the
  advice rule above, with the developer choosing among optimization, reduced scope and deferral. `within-noise-budget`
  states that no change was established, which is not evidence that none exists. A set never replaces a baseline or
  relaxes a band. A scenario makes about 26 metric tests, so a chance verdict is possible.
- What a set can establish is bounded by its runs: complete separation reaches p = 2 / C(4N, 2N), which is 0.0022 for
  the default six against six, 0.029 for four against four and 0.33 for two against two. A single pass therefore
  cannot reach p < 0.05 and can establish only a rise in an exact budget. Each set records its smallest attainable p.
- The retained `summary.json` keeps each side's revision or path, commit and library source fingerprint, every run's
  medians and each metric's run values, p-value, spread and verdict. Pairs are refused when there is nothing to
  compare: one commit for two revisions, or identical fingerprints when a working tree is named.

### Described-menu clean private memory

On 2026-09-27 the user directed a waiver for described native menu entries (#24, merged as `6f769ab`). In three
matched September 21 Release pairs against `78b3de3`
([common-scene investigation](../../Measurements/MenuDescriptions/2026-09-21/README.md)), clean-round median
private bytes had risen by 823,296, 1,175,552 and 1,175,552 bytes (+3.23%, +4.56%, +4.51%). The waiver accepted up to
1,175,552 bytes (+4.6%) on the default x64 Release WARP fixture while an
[optimization plan](../Plans/WIP/MenuDescriptionMemory_2026-09-27.md) investigated. The scene opens no menu, and
outside menu popups the change adds no allocation.

On 2026-09-30 the waiver is removed, because the plan's repeated paired runs of `e47c836` against `6f769ab` are within
the bands. A local set of six runs per side
([receipts](../../Measurements/MenuDescriptions/2026-09-30/paired-local/README.md)) measures clean private bytes
+0.36% (p = 0.70) and dirty private bytes -0.89% (p = 0.56). The earlier hosted set measured clean private bytes
-0.50% and -0.70%, and pair three's unaccepted dirty-round +8.67% did not repeat. Described menus now carry no memory
envelope, and the regular investigation bands and exact budgets apply to them.

### I26 accepted multiline Grid memory tradeoff

On 2026-09-23 the user explicitly accepted the measured V11 multiline Grid process-memory
cost to retain complete-line clipping, omission markers and readable Issues summaries.
The observed fixed-fixture Release median rises from 33,214,464 to 40,226,816 private bytes
(+7,012,352 bytes); paced retention rises from 29,743,104 to 36,831,232 bytes. The accepted
envelope is those recorded approximately 6–7 MiB costs, alongside the measured multiline
offscreen throughput improvement from 61.974 to 122.243 FPS. Preserve the
[original qualification](../../Measurements/GridTextOverflow/2026-09-21/qualification/Release/README.md)
and [heap attribution](../../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md).
Process private bytes are not equivalent to live cache storage; attribution includes native
heap capacity and does not establish a settled long-run bound.

This decision does not accept the rejected associative-cache experiment, separate common-path
timing/memory flags, menu benchmark differences, or additional growth. Existing investigation
bands and deterministic allocation/surface/hidden-work budgets remain unchanged. It neither
updates a consumer pin nor replaces the remaining functional and integrated qualification.

On 2026-09-29 the [review fixes](../Plans/Done/ReviewFixes_2026-09-29.md) stopped drawing multiline cells with
`D2D1_DRAW_TEXT_OPTIONS_CLIP` on their fractional layout box. A local paired set against `40c6c21`
([receipts](../../Measurements/ReviewFixes/2026-09-29/paired-local/README.md)) measured MultilineGrid dirty private
bytes 4.0–5.9 MB lower, dirty peaks about 9.7 MB lower and clean private bytes 5.0–6.9 MB lower, with unchanged
allocations. That removes most of the cost accepted above; the envelope stays until a developer tightens it after a
quiet-fixture repeat.

### Accepted Grid layout retention

On 2026-09-29 the developer set the priority for Grid text layouts: the best frame rate first, then the least memory
for it. That replaces the memory-first rejection of the associative-cache experiment for the Grid. The
[review follow-ups](../Plans/Done/ReviewFollowUps_2026-09-29.md) keep cell layouts in 32-way set-associative tables
(at most 16,384 entries; an entry the current or previous paint used is never evicted) and keep single-line captions'
layouts as well. Two local paired sets against the review fixes
([receipts](../../Measurements/ReviewFollowUps/2026-09-29/paired-local/README.md)) record equal or fewer dirty-round
allocations and no clean-round allocation in all three scenes, and a higher `Default` dirty rate in all four crossings
(+5.1% to +14.5%). B's median private bytes averaged -0.08 to +0.52 MB from A's per scene and phase. Every
same-binary control in both sets drifted beyond its band, so neither set alone establishes the gain or the cost.
Pooled (four runs per side) and judged by the paired-set rule, the `Default` dirty rate is 11.3% higher (p = 0.029),
and no memory change is established in any scene (the largest, `Default` dirty private bytes +2.4%, has p = 0.11).

What a painting grid keeps is one layout per cell its last paint drew, about 20 KB each, and the
[reliability follow-ups](../Plans/Done/ReliabilityAndFollowUps_2026-09-30.md) return all of it when the grid stops
painting: hidden (itself, under a hidden ancestor or in a hidden embedded view), detached or given another model. Their
local paired set against `main`, six runs per side in each scene
([receipts](../../Measurements/ReliabilityAndFollowUps/2026-09-30/paired-local/README.md)), keeps the painting path's
allocations identical and establishes no timing or memory change while painting. Where every cell holds its own
layout, hidden private bytes are 0.52 MB lower (p = 0.026); elsewhere the heap keeps the freed blocks committed, and
the Grid suite's layout counters establish the release.

On 2026-09-30 the developer accepted this retention for its frame rate. The accepted envelope is what the Grid
suite's counters and the Release heap walk of the reliability follow-ups record: while it paints, a Grid keeps at most
one text layout per cell its last paint drew, about 20 KB each (about 0.57 MB for a 6x4 multiline grid, 0.93 MB for
ten single-line rows, 1.3 MB for 10x6), with their key strings (6 to 12 KB) and tables (5 to 14 KB), which on the
complex-UI fixtures is the about 0.5 MB of private bytes the 2026-09-29 sets recorded; hidden, detached or given
another model, it keeps none. Layouts beyond the cells of the last paint, any layout kept by a grid that is not
painting, and any rise in the painting path's allocations remain regressions that need developer advice. The
accepted retention is bounded by the cells on screen, not by the model's size.

### I19 accepted resource trade-off

On 2026-09-13 the user accepted the measured static-library adoption trade-off and
requested closeout. The [matched library record](../../Measurements/SharedLibrary/2026-09-13-local/README.md)
retains all original comparisons, including investigation-band failures and baseline variation.
The accepted envelope is the observed I19 result on that fixed fixture: Release clean peak
private memory increases by a median 731,136 bytes, while surface storage, composition
allocations and hidden work remain unchanged. Release dirty completed throughput changes
by +0.02%; Debug dirty throughput changes by -1.65%. This is an explicit acceptance of
the recorded costs for canonical source ownership and the clipboard, module-lifetime and
font-refresh corrections, not a claim that every metric is unchanged.

The existing deterministic budgets and investigation bands remain in force. This decision
does not admit additional growth in later revisions or replace the retained baseline.
It covers the named offscreen fixture only; hardware presentation and long-run retention
still require their own evidence. Consumer measurements and their accepted costs remain
in the corresponding product repositories.

### Ongoing validation and resource budgets

Every `test.ps1` invocation MUST report complex-UI FPS and memory, including filtered suites, and each suite receipt
must include the measurement or its linked receipt. A failing/missing benchmark fails the test entrypoint. Unit-test
execution rate and CPU command-submission rate must never be labeled rendered FPS. Hidden/static idle views have no
requested frames (report zero work rather than an artificial FPS loop). The default executable fixture measures
1280x720/96-DPI completed offscreen WARP frames with 83 controls, 1,000-row Grid/Tree models, 20 warm-up frames and
five 40-frame rounds each for clean and changing content. Dirty work updates sliders/progress and scrolls the grid.
One reusable staging pixel synchronizes GPU completion outside production code; report its cost in total FPS.
Record p50/p95 frame time, p95 preparation/CPU composition, C++ allocations, exact surface bytes/replacement peak,
process private bytes/working set and sampled peaks/growth. Composition allocation and extra surface creation are
hard failures, and hidden preparation/composition must remain zero. Whole-frame C++ allocations are gated as well:
clean rounds must record zero, and dirty rounds may not exceed 64 allocations per frame in Release (2,560 per
40-frame round; 54 per frame measured on 2026-09-07) or 320 per frame in Debug (12,800 per round; 257 measured),
where the Debug STL allocates one container proxy per std::vector/std::wstring. The receipt records
`dirtyAllocationCeilingPerFrame`. A ceiling is never raised to pass; a failing gate reports the measured count for
advice. The benchmark does not establish displayed FPS.

Benchmark receipts also record process-memory phases at entry, device creation, scene
creation, warm-up, screenshot encoding and the hidden state. These untimed samples help
locate changes; they do not replace matched frame/retention comparisons. The opt-in
`MenuResources` control suite records 96 open/render/capture/close cycles for twelve
plain or described entries at matched 456-by-300-DIP viewport constraints, including
private/working-set bytes and process/GDI/USER handles. Report screenshot-buffer cost
separately from the open product menu; a short cycling run alone cannot establish a
long-run retention bound. The identical probe can characterize plain menus on the old
implementation and reports described mode as unsupported there.
The benchmark executable's opt-in `--benchmark-retention <output-prefix>` repeats
sixty complete create/render/hide/destroy cycles in one process, retaining each inner
report. Bind that diagnostic's executable/source/fixture hashes and keep its raw rounds;
it does not replace the default acceptance comparison or a controlled long-duration soak.
The executable's opt-in `--benchmark-grid-selection <report.json>` (fixture `dxui-grid-selection-v1`) measures Grid selection
with synthetic data: Grids of 1,000 to 200,000 rows, selected as Ctrl+A selects them and painted offscreen on WARP,
`IsSelected` per call over selections of 0 to 200,000 ids, and the time and C++ heap bytes of the selection model's mutators. It
also reports how many rows the default complex-UI scene's Grid holds selected. Its entry is dispatched outside `BenchmarkMain.h`,
so the complex-UI fixture's hashed inputs are unchanged. Compare it only between builds of one harness, as an interleaved
paired set; it supplements the default benchmark for a change to selection and does not replace it.

Shipping/consumer acceptance additionally requires a named hardware fixture and actual presented complex-UI FPS,
frame pacing and p50/p95/p99 latency at the target refresh rate (at least 60 FPS / 16.67 ms per frame for a 60 Hz
consumer while actively updating). Include 96/144/192 DPI, large data sets, typing/dragging, animation, scrolling,
multiple simultaneous views, resize/DPI transitions, allocation failure and device recovery. Record workload/driver
and CPU/GPU/total frame costs separately. WARP coverage and a clean composite alone cannot satisfy these gates.
Run retention/soak checks long enough to distinguish bounded warm caches from sustained growth; report samples,
duration, maximum views/data and start/steady/peak/end resources. Do not claim a hardware or long-run pass from
the short default benchmark. Match new development to a targeted measured workload as well as the common fixture.

Optional heap-attribution fixtures may enumerate and walk process heaps outside timed rounds,
with bounded storage and individual heap locks. Report per-heap failures and distinguish busy/free
heap blocks from process-private memory and working set; their totals are not interchangeable.
Do not purge heaps or trim working sets to hide retention. A separately identified fixture may
pace retention frames to compare wall-clock allocation rates, but must leave measured FPS rounds
unpaced and must not replace ordinary unpaced performance/resource acceptance. Diagnostic timing
includes sampling overhead. [The grid investigation](../../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md)
records the implemented opt-in diagnostics and their unresolved conclusions.

Steady-state hot paths use bounded reusable storage; cache derived state and batch compatible work. Never allocate,
shape text, create targets, traverse layout, do I/O, block or read back in clean composition. Coalesce dirty state and
prepare only changed visible content. Hidden, minimized, occluded, suspended, display-off and idle work is event-blocked,
with no embedded timer/worker/polling loop. Share immutable resources per device generation; explicitly bound queues,
caches, textures, replacement peaks and concurrent instances. Checked arithmetic/capacity failures must fail cleanly,
release ownership and retain coherent visual/input state. Destroy device-generation resources together on recovery.

## Independent measurement ownership

DxUi owns its samples, synthetic data, benchmark workloads and baseline receipts. They MUST build and run using
this repository, the Windows SDK and pinned library dependencies alone. No consumer checkout, plugin, settings,
AV endpoint or application service is required. Inspiration from application layouts is allowed; executable fixtures
and their acceptance criteria remain library-owned. The runnable complex sample and timed benchmark use the same
`Samples/ComplexUi/ComplexUiScene.h` scene. Every fixture change requires a new identity and matched fixture hashes.
A harness-only change (assertions or receipt fields) keeps the workload identity but changes the fixture hash, so the
matched baseline is measured with the final harness on the previous implementation before the candidate is compared.

Application-specific adoption reports, configurations, endpoint workloads and budgets belong in that application's
repository. Do not store them in DxUi docs or use them as a substitute for independent library evidence. Conversely,
DxUi offscreen throughput does not establish the complete application's presentation, input or service latency.
Reviewed independent raw receipts may be retained under `Measurements/`, with a README explaining the scenario,
source identity, all noisy runs and limits. Intermediate runs remain under `.build`; docs link to the retained evidence.

Measure the full sum of simultaneous views at 96/144/192 DPI, including shared-pool and replacement costs. One
1280x720 BGRA surface is 3,686,400 bytes (3.52 MiB), excluding driver overhead; physical extents determine residency.
A hidden or zero-extent view holds no surface and reports `surfaceBytes` 0: a consumer reclaims a collapsed view's
surface by hiding it, and the next visible sized preparation allocates exactly one replacement.
Applications must admit their aggregate view cost using their own instance bounds. Allocation counters distinguish
library-controlled work from OS/driver internals; both remain measured. A single composite draw does not make dirty
preparation free. Never impose one application's module topology, two-view layout or endpoint latency on all consumers.

### Single-library implementation budgets

EmbeddedHost has at most one cached surface (64 MiB maximum) while visible with a nonzero extent, and none while
hidden or zero-sized. A resize allocates the replacement before releasing the previous surface, so the single-surface
cap bounds the transactional replacement peak to 128 MiB per view by construction; `replacementPeakBytes` reports it.
Immutable composition state, the D2D device and the DWrite factory are shared through GraphicsDevice. A consumer
using tile/raised views admits their summed surface cost. Composite is a single triangle with shader constants
derived from SV_VertexID and needs no per-view vertex/index/dynamic constant buffer. Hidden state performs no timer
subscription; native WindowHost alone uses the event-driven animation dispatcher. Animation ticks add no raster work
by themselves: `AdvanceAnimation` never marks the view dirty, and every `Tick` that changes visual state invalidates.
Per-host caches are bounded to 256 solid brushes and 96 configured text formats; a cache beyond its bound is cleared
at the start of the next embedded preparation or native paint, never mid-paint, so steady-state residency stays
proportional to the painted working set. Diagnostics are borrowed and optional; composition does not emit them.
The private window-message payload registry is bounded to 128 windows and 128 queued payloads; saturation fails
immediately and releases transferred ownership. Teardown invalidates queued tokens and drains outside its lock.

A Grid's selection is a `GridSelectionModel`, which keeps each selected stable id twice: in selection order, and
ascending beside it. `IsSelected`, which a Grid asks once per visible row on every paint, is a binary search that
allocates nothing, so what a paint costs does not grow with the selection; it was a linear scan, 0.9 to 1.4 us a call at
20,000 selected rows and 47 to 83 us at 1,000,000. The ascending copy is the budget: 8 bytes per selected row beyond the
ordered ids' 8 (the model itself grows from 40 to 64 bytes), kept at the selection's high-water mark like the ordered ids,
and never a heap node per row. The mutators, which run on user gestures and data changes, keep both copies equal at O(n
log n) at most: a `SetRange` over ids that do not already ascend sorts them (1.0 ms for 20,000 ids, 10.9 ms for 200,000),
`PreserveOrdered` allocates at most twice instead of once per selected id, and `SetRange` and `PreserveOrdered`, the two
that may throw, leave the selection as it was when an allocation fails. A selection structure that costs a node per row, a
membership test that scans, or a paint that allocates for the selection regresses this budget. The
[paired record](../../Measurements/GridSelection/2026-10-01/README.md) holds the measurements.
