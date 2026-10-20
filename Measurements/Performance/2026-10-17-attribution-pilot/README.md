# Bounded timing and memory attribution pilot

Status: diagnostic pilot complete; every remaining qualification gate stays **OPEN/HOLD**. Reviewed 17 October 2026.
This follows the [71/72-control investigation](../2026-10-17-pr80-controls/README.md), not a new calibration or
accepted non-regression study. The [qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#bounded-timing-and-memory-attribution-pilot-17-october-2026)
owns the next steps; the [performance contract](../../../Specs/Core/Core_PerformanceAndResources.md) owns acceptance.

## Identity, schedule and validation

The unchanged reference source is merge `9f848b6464f166aa89229b80cd0e2ac22dbed42b`. A fresh x64 Release build and
one canonical reference run per Default, MultilineGrid and MultilineGridDistinct pass their before/after v2
provenance checks. Their dirty run-median FPS are 541.77, 102.75 and 589.93 respectively. These are three unpaired
references on this host, not replayed PR #80 binaries or a comparison against its hosted numbers.

The separately frozen diagnostic source is private overlay `60f9756f623b11fa2230452502aa4ea8744000a2`, based on that
merge. It adds one opt-in fixture and its dispatcher entry, reusing the existing profiling/heap/scene helpers;
the library implementation, canonical benchmark closure and acceptance tools are unchanged. The prototype is
retained privately, not installed as a supported repository test workflow. Its Release build passes with warnings
as errors. The canonical harness, compiler and dependency identities match the reference build, but the executables
have different hashes. This does not make the two fixtures equivalent acceptance evidence.

Before native measurement, retain the fresh executable/library/symbol outputs, build attestations and 22 resolved
compiler/SDK/dependency roots. Copies are checked against installed bytes; their packet seals 8,573 files. Keep the
diagnostic working source tree, generated/build inputs and its 10,539-file working-source hash inventory too.
Git source archives additionally preserve the source revisions; they are not substituted for actual working-file bytes.
Original PR #80 binaries remain unavailable from its upload; this new retention does not repair that historical gap.

The predeclared pilot uses one frozen executable, native x64 Release/WARP, 1280x720 at 96 DPI, all three synthetic
scenarios and four modes. Every process performs 20 dirty warm-up frames, an optional populated harness capture,
five 40-frame clean rounds and five 40-frame dirty rounds. Update indices match the canonical schedule. Additional
stage observations, CPU queries and clock probes make all four modes diagnostic workloads.

| Mode | Frame timing | Heap snapshots | Capture |
| --- | --- | --- | --- |
| A | Total/Prepare/Composite | Off | On |
| B | Also update, bind/clear, copy, blocking Map and Unmap; per-frame CPU counters | Off | On |
| C | Same detailed timing as B | On | On |
| D | Same detailed timing as B | Off | Off |

Run Default, MultilineGrid and Distinct in that order, A/B/C/D per scenario; then reverse both scenario and mode
order. Thus there are exactly 24 fresh processes, two per scenario/mode. No seed or passing repeat is selected.
Each owned child is limited to 120 seconds, also capped by the remaining 1,800-second overall scheduling budget.
Collection finishes in 808.049 seconds: 24/24 diagnostic-valid processes, 10,080 frames, 240 measured rounds and
21 stage snapshots per process. All stage telemetry is complete; kernel scheduling telemetry is separately unavailable.
All raw hashes, source/controller closures and complete before/after v2 identities pass; captures pass PNG structure/CRC checks.
The sampler exits normally. No foreground-taking suite, priority/affinity change, heap purge or working-set trim runs.

Three private helper preflight failures occurred before their affected native stages: output-root separator
validation, nested-module import ordering, and a CRLF dispatcher patch anchor. Their records remain retained.
They are not native benchmark failures or discarded measurements. No scheduled pilot process fails or is rerun.

## Clock resolution and observer costs

Each process retains 256 empty clock pairs and 256 observations each of compiler-barrier loop controls and
1/64/256-call clean-Prepare batches, rotating batch order after all frame rounds. The declared QPC grid is 100 ns.
In the six B-mode processes the empty pair and one-call batch both have p95 100 ns and median zero ticks; zero
ticks do not mean zero cost. The following are ranges across those two B processes per scenario:

| Scenario | 64-call batch p95 divided by 64 | 256-call batch p95 divided by 256 |
| --- | ---: | ---: |
| Default | 4.6875 ns | 3.90625 ns |
| MultilineGrid | 9.375 ns | 8.984375 ns |
| Distinct | 4.6875 ns | 3.90625-4.296875 ns |

Optimized-object disassembly retains a real `EmbeddedHost::Prepare` call inside the decrement/back-edge loop
(function-relative offsets 0xB25/0xB3D). Its instruction pattern also matches the linked executable uniquely,
at file offset 428,960. The call was not eliminated. Barrier-loop raw controls remain retained; no clock/loop
overhead is subtracted from an acceptance metric.

These normalized batch means describe a repeatedly hot call pattern. They are not per-frame p95 values and cannot
replace canonical clean preparation: the pilot's run-median clean frame-Prepare p95 ranges from 300 to 800 ns,
where a 100 ns step alone is 12.5-33.3%. Resolution and call context matter; this does not explain the original
67 controls with non-preparation timing/FPS drift, nor does it make their crossings harmless.

The fixed telemetry storage is 743,632 bytes, included from entry. Total stage sampling takes 2,336.52-3,522.73 ms
per process. In heap mode, the separately timed heap block totals only 9.35-13.98 ms. The derived non-heap residual
includes the other queries and enumeration; it is not an independently timed causal attribution to one helper.
Dirty-window time outside individual frame spans is 1.15-2.46 microseconds/frame in A and 50.05-72.97 in B/C/D.
It includes CPU/memory queries, checks and loop overhead. This is material observer work even outside frame timing:
the gaps can change later backend/cache readiness. The modes must not establish an isolated heap or tracing effect.

## Timing witnesses and shared-host admission

All observations below are retained. Each cell gives the first and second scheduled process; FPS and frame-p95
are medians of five dirty rounds. A round's p95 uses sorted sample index 37 of 40, matching the canonical projection.

| Scenario | Mode | Process slots | Dirty FPS | Dirty frame-p95, ms |
| --- | --- | --- | --- | --- |
| Default | A | 1/24 | 575.68 / 645.61 | 2.0844 / 1.8073 |
| Default | B | 2/23 | 515.19 / 639.15 | 2.2801 / 1.8232 |
| Default | C | 3/22 | 577.38 / 619.81 | 1.9982 / 1.8822 |
| Default | D | 4/21 | 567.35 / 644.56 | 2.2186 / 1.7726 |
| MultilineGrid | A | 5/20 | 108.12 / 134.36 | 9.8182 / 8.2874 |
| MultilineGrid | B | 6/19 | 124.50 / 129.77 | 9.0734 / 8.3303 |
| MultilineGrid | C | 7/18 | 98.66 / 133.11 | 11.1699 / 8.0359 |
| MultilineGrid | D | 8/17 | 96.75 / 113.74 | 11.4839 / 9.4248 |
| MultilineGridDistinct | A | 9/16 | 502.31 / 593.70 | 2.4353 / 1.9965 |
| MultilineGridDistinct | B | 10/15 | 548.30 / 598.51 | 2.1693 / 2.2479 |
| MultilineGridDistinct | C | 11/14 | 573.85 / 551.60 | 2.1794 / 2.1278 |
| MultilineGridDistinct | D | 12/13 | 566.07 / 530.21 | 2.3004 / 2.5240 |

Identical A-mode dirty FPS rises 12.1481%, 24.2674% and 18.1935% in Default, MultilineGrid and Distinct. These are
descriptive same-mode differences, not a new randomized control count or judge verdict. Order/environment effects
remain unresolved; B/C/D comparisons cannot establish a causal tracing, heap or capture timing effect from two observations.

The longest measured frame is slot 7, MultilineGrid C, dirty round 2, zero-based frame 25: 12.9317 ms total,
12.4262 ms Prepare, 0.4236 ms blocking Map and 0.0206 ms Composite. Another region can dominate: slot 15, Distinct B,
dirty round 4, frame 11 records 4.1910 ms total, 3.0655 ms Map, 1.1016 ms Prepare and 0.0075 ms Composite.
Its main/process CPU times both advance zero coarse ticks, while cycles advance 9,199,043/51,832,765. Retain the
raw consumption counters; zero coarse CPU time is not zero work, and cycles are not fabricated scheduler delay.
Map includes completion waiting, driver/worker work and possible preemption, not isolated GPU execution time.
No pilot frame reproduces Default B07's historical 584.4771 ms round-p95 or 2,619.8089 ms individual frame.
That non-reproduction does not attribute or dismiss either original witness.

The external sampler retains 3,530 aggregate CPU/memory observations, zero query errors, maximum interval
263.06 ms and at least 13 samples per native child. Available physical memory stays above 17.29 GiB. Aggregate
CPU busy reaches 66.67% during children and 88.62% across the entire collection, including provenance work.
In 399 consecutive samples with the same owned child identity, aggregate busy minus that child's CPU-consumption
fraction leaves an approximate 10.60-65.40 percentage-point residual (median 25.73). Reads are sequential and CPU
times coarse; this includes observer/other-process work and does not name a competing process or explain a frame.
This is a shared-host diagnostic, not evidence of an admitted quiet or dedicated calibration environment.

The named `CPU.light` WPR attempt around slot 2 is refused: `0xc5585011`, "Failed to enable the policy to profile
system performance." The exact refusal, not an inferred missing privilege, is retained. No kernel recording starts;
the final named status is not recording. Consequently there are no CPU-ready/context-switch, allocation call-stack
or actual WARP-thread-placement traces. Ordered frame spans, sampled thread identities and CPU counters do not fill that gap.

## Memory attribution and its limits

At the capture boundary, all 18 capture-on processes add 3,816-4,188 KiB private bytes; all six capture-off processes
add zero. The [harness capture](../../../Samples/EmbeddedControls/GraphicsFixture.h) reads a full-size image and
encodes PNG through WIC. This supports a capture-associated footprint in this diagnostic; it does not identify
every retained allocation or attribute the original pre-capture witness. Capture remains in the canonical fixture.

MultilineGrid C slots 7 and 18 expose a pre-capture difference with unchanged controlled resource counts:

| After warm20 | Slot 7 | Slot 18 | Difference |
| --- | ---: | ---: | ---: |
| Process private bytes | 33,550,336 | 34,836,480 | +1,286,144 (1,256 KiB) |
| Sampled heap busy bytes | 15,212,484 | 16,183,584 | +971,100 |
| Sampled heap committed bytes | 19,906,560 | 20,770,816 | +864,256 |
| Owned threads sampled | 32 | 36 | +4 |
| View surface bytes | 3,686,400 | 3,686,400 | 0 |
| Retained Grid layouts / table bytes | 5 / 5,120 | 5 / 5,120 | 0 |

The retained prior heap-JSON capacity is identical, 2,349 bytes, at that boundary. Heap usage and worker population
vary before capture while these library counters hold. The samples are sequential, not simultaneous; this is not
a decomposition of the private-byte difference or proof that those four threads caused it. It neither assigns a
heap owner nor resolves the historical 628 KiB control or earlier 1.28/1.31/1.54 MiB local flags.

After root clearing, detach, scene destruction and GPU-fixture destruction, slot 7 reports 19,492,864 process-private
bytes, with 1,879,254 busy, 14,514,864 free and 17,276,928 committed bytes in the sampled heaps; heap errors are zero.
Slot 18 similarly reports 20,594,688 private, 1,899,837 busy, 15,074,240 free and 17,764,352 committed bytes.
This establishes substantial sampled allocator capacity retained alongside much smaller busy totals. Committed,
free, busy, heap metadata and process-private bytes have different scopes and are not an exact additive accounting.
It does not establish object ownership, unreachable memory, a leak or a long-run retention bound. No heaps are purged.

Every process reports a 3,686,400-byte visible surface and zero after hiding. Composition/clean C++ allocations
remain zero; the maximum dirty-round count is 54/frame, below the unchanged Release ceiling of 64/frame.
These controlled resources do not bound native allocator/driver residency.

## Retention and decision

Keep the complete private packet: predeclaration/order, both build outputs/attestations, working/generated source
inputs, retained toolchain/dependencies, three reference runs, all 24 reports/child results, 18 captures, host samples,
disassembly/proof, analysis and failure/refusal records. Rehashing all 24 raw reports after analysis succeeds.
The pilot sub-packet seals 92 files. This publication contains sanitized Markdown only.

| Retained identity | SHA-256 |
| --- | --- |
| Reference executable | `02324234DDB81DAF54D28BA7F93307A033ECFA8A7BFADD7B8B615456C3D47771` |
| Diagnostic executable | `026421EC9847EAADC8E85CE5D1C26CF2246F9194B992D4592FE4C257EFD4F1FE` |
| Diagnostic source overlay | `76326FC33643995F31A9C67211510FB8055464F9D3F5E953C9D459D21C7ED6B3` |
| Diagnostic input manifest | `3199B94FBCBE6732462D0EC030DEE0942FFAC859CC66312A972E6716CF7FCC8D` |
| Controller/order/predeclaration manifest | `5D4E1C8ED7D9D6F673A0CCAFF658071ECAB04273EE6D0DB00647755390101274` |
| Pilot 92-file seal | `C94FD7C7476E4A9FB53444CD22C1229E98970B765581773D0B4BF913A06CB47E` |

Reproduction requires the retained private overlay and exact build packet, not an ordinary checkout with an
unversioned replacement fixture. Use the retained controller to replay the declared order and owned-child limits;
verify every input/output hash and before/after state, then recompute per-round sorted-index percentiles, five-round
medians and named stage differences. A fresh rebuild is a new build/run identity. Never overwrite the original reports.

The ledger keeps timing/allocator attribution, trusted policy, fresh canonical twelve-block/all-scenario A/A,
independent judge-migration review, native ARM64 opt-in resources, physical/device-removal/IME/AT/touch and consumer
acceptance open. Original `verdictAgreement=false` and `qualified=false` are unchanged. Bands remain 5% timing/FPS
and 2% memory, with identical exact budgets and allocation ceilings. No baseline is replaced and no numeric A/A
pass rule is invented. This evidence-only repository update changes no public runtime, test workflow or visuals;
gallery regeneration is unnecessary.
