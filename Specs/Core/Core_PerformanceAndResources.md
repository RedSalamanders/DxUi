# Performance and resources

Status: normative current contract
Last reviewed: 2026-09-05

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
harness against both implementations. A missing baseline is explicitly unpaired and cannot close an implementation
performance gate. Documentation/tool-only changes that leave compiled library inputs unchanged record that fact.

DxUi MUST NOT accept a confirmed performance or memory regression silently. Compare FPS, frame/input percentiles,
preparation and composition costs, allocations/bytes, surface/cache residency and peaks, private bytes/working set,
resource counts, idle activity and long-run retention. Keep all samples and investigate repeatable degradation,
including trends smaller than automated noise bands. `performance.ps1` compares five-round medians, with 5% timing/FPS
and 2% process-memory investigation bands; deterministic surface/allocation budgets allow no growth. Those bands
address measurement noise, not a permitted regression budget. Repeat suspicious results against the retained baseline.
Never relax a threshold, omit a workload or replace the baseline merely to make a test pass.

On confirmed degradation, stop accepting/merging the affected development, present the measured deltas and suspected
cause, ask the developer for advice, and propose options with quantified costs: optimize the affected path/caches,
reduce optional scope, or defer/revert the change. Any explicitly approved tradeoff needs durable rationale and the
chosen resource budget in this contract or its owning domain; a WIP note alone cannot waive a requirement.

Every `test.ps1` invocation MUST report complex-UI FPS and memory, including filtered suites, and each suite receipt
must include the measurement or its linked receipt. A failing/missing benchmark fails the test entrypoint. Unit-test
execution rate and CPU command-submission rate must never be labeled rendered FPS. Hidden/static idle views have no
requested frames (report zero work rather than an artificial FPS loop). The default executable fixture measures
1280x720/96-DPI completed offscreen WARP frames with 83 controls, 1,000-row Grid/Tree models, 20 warm-up frames and
five 40-frame rounds each for clean and changing content. Dirty work updates sliders/progress and scrolls the grid.
One reusable staging pixel synchronizes GPU completion outside production code; report its cost in total FPS.
Record p50/p95 frame time, p95 preparation/CPU composition, C++ allocations, exact surface bytes/replacement peak,
process private bytes/working set and sampled peaks/growth. Composition allocation and extra surface creation are
hard failures, and hidden preparation/composition must remain zero. The benchmark does not establish displayed FPS.

Shipping/consumer acceptance additionally requires a named hardware fixture and actual presented complex-UI FPS,
frame pacing and p50/p95/p99 latency at the target refresh rate (at least 60 FPS / 16.67 ms per frame for a 60 Hz
consumer while actively updating). Include 96/144/192 DPI, large data sets, typing/dragging, animation, scrolling,
multiple simultaneous views, resize/DPI transitions, allocation failure and device recovery. Record workload/driver
and CPU/GPU/total frame costs separately. WARP coverage and a clean composite alone cannot satisfy these gates.
Run retention/soak checks long enough to distinguish bounded warm caches from sustained growth; report samples,
duration, maximum views/data and start/steady/peak/end resources. Do not claim a hardware or long-run pass from
the short default benchmark. Match new development to a targeted measured workload as well as the common fixture.

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

Application-specific adoption reports, configurations, endpoint workloads and budgets belong in that application's
repository. Do not store them in DxUi docs or use them as a substitute for independent library evidence. Conversely,
DxUi offscreen throughput does not establish the complete application's presentation, input or service latency.
Reviewed independent raw receipts may be retained under `Measurements/`, with a README explaining the scenario,
source identity, all noisy runs and limits. Intermediate runs remain under `.build`; docs link to the retained evidence.

Measure the full sum of simultaneous views at 96/144/192 DPI, including shared-pool and replacement costs. One
1280x720 BGRA surface is 3,686,400 bytes (3.52 MiB), excluding driver overhead; physical extents determine residency.
Applications must admit their aggregate view cost using their own instance bounds. Allocation counters distinguish
library-controlled work from OS/driver internals; both remain measured. A single composite draw does not make dirty
preparation free. Never impose one application's module topology, two-view layout or endpoint latency on all consumers.

### Single-library implementation budgets

EmbeddedHost has one cached surface (64 MiB maximum, 128 MiB transactional replacement peak) and shares immutable
composition state, D2D device and DWrite factory through GraphicsDevice. A consumer using tile/raised views admits
their summed surface cost. Composite is a single triangle with shader constants derived from SV_VertexID and needs
no per-view vertex/index/dynamic constant buffer. Hidden state performs no timer subscription; native WindowHost
alone uses the event-driven animation dispatcher. Diagnostics are borrowed and optional; composition does not emit
them. The private window-message payload registry is bounded to 128 windows and 128 queued payloads; saturation
fails immediately and releases transferred ownership. Teardown invalidates queued tokens and drains outside its lock.
