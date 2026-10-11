# Bounded ComplexUi profiling pilot

This descriptive investigation follows the unstable guarded A/A study. It does not qualify the policy, approve
the original Grid flags or change any acceptance threshold. The accepted execution is recorded in the
[production-review plan](../../../Specs/Plans/WIP/ProductionReview_2026-10-05.md).

## Frozen baseline

All six serial fresh-process reports ran before implementation on `01f1aed1017e5c83dfdbf85a226444bbb1c63419`.
The Release/x64 embedded executable SHA-256 was
`198D5A32F3EC51631AAA4A7C20120997A344D0CBB6ED2749E474291EA44E61AF`. The sequence alternated Default and
MultilineGrid three times, and every report passed the canonical before/after v2
byte guards. The baseline here is the current layout candidate, distinct from the earlier A/A binary on `60e1cbf`.
All reports, logs and unpaired comparison companions are retained exactly under `baseline/`.
All six record `sourceDirty=true` and the same source fingerprint; the commit alone is not their source identity.

Baseline Grid report 4 includes a clean round at 989.49 completed FPS between rounds near 2,667 FPS, confirming
that large within-process variability can reproduce in this small fixed inventory. Baseline Default report 1
also ranges from 1,356.87 to 2,729.02 clean FPS. These are observations, not an attribution or a candidate/base
regression comparison. No ambient-load trace or dedicated-host guarantee accompanies the reports.

## Tracing availability

WPR and xperf are installed. The task tried only its uniquely named WPR instance, with a custom memory-mode
profile requesting 64 buffers of 64 KiB and a two-second permission preflight. Start was refused with
`0xc5585011`, "Failed to enable the policy to profile system performance." The subsequent owned-instance status
reported no recording. The profile, error and metadata remain under `wpr-preflight/`. No other recording was
canceled, no elevation or machine policy was changed, and no workload trace was produced.

## Diagnostic pilot and limits

The [first driver attempt](attempt-80c34c9f-stopped/README.md) stopped after one successful native report because
the caller lost the policy guard during module imports. Its original one-attempt/eleven-unattempted completion
and exact report remain intact. `Test-DriverImports.ps1` reproduces that defect in all three affected batch/analysis
drivers, then checks both equal-identity acceptance and changed-identity rejection after the import correction.
The corrected driver uses a separately declared fixed inventory; no failed attempt or outlier is silently replaced.

The corrected pilot completed all twelve declared Release/x64 reports on clean `cc77bec2` in 357.405 seconds,
within its 1,200-second start budget. Its fixed forward/reverse/forward schedule contains three fresh processes
per scenario/heap combination. All twelve match the initial full v2 anchor and profiling closure; all 48
raw/child/log/capture bindings match. The exact 74 original files are copied under `diagnostic/`, with original
raw/child/plan/completion JSON bytes renamed to `.receipt.txt`. The stopped attempt is excluded.

Descriptive completed FPS ranges, calculated from summed completed-frame durations:

| Scenario / heap | First 20 warm frames | Frames 101-200 | Clean probe | Dirty probe |
| --- | ---: | ---: | ---: | ---: |
| Default / off | 511-536 | 562-643 | 1,711-2,370 | 412-614 |
| Default / on | 456-543 | 540-651 | 1,716-2,734 | 624-648 |
| MultilineGrid / off | 115-117 | 123-133 | 1,424-2,421 | 125-137 |
| MultilineGrid / on | 109-115 | 125-136 | 2,412-2,530 | 126-133 |

Every report has complete telemetry: 280 frames, five windows and twelve stages (3,360 frames total). Longer warm-up
does not establish convergence: Default heap report 6 goes 543.44, 630.96 and 539.71 FPS through the three warm windows.
Total sampling costs 63.126-215.739 ms per stage and 0.840-1.521 seconds per report, versus 0.406-1.963
seconds in timed windows. This total includes the optional heap sampling block, which costs 0.107-0.746 ms
including enumeration, traversal and JSON construction/parsing. Both heap profiles still run the common
thread/process sampler; its interleaved gaps allow backend work to continue. Subtracting the recorded heap block
gives a non-heap residual of 0.834-1.521 seconds per report, not an independently timed causal effect. Heap-off/on spread does not isolate
the heap observer effect or establish a warm-up policy.

CPU times show 15.625 ms quantization; ten clean windows record zero main-thread time delta. The analysis also
retains available raw cycle deltas, without converting them into durations. Sequential process-minus-main time
is a read-order estimate, not scheduler attribution. Observed threads rise from four at entry to 33-36 at warm20;
those worker identities remain at release. Their presence does not identify a delayed thread or owning component.
After release, private bytes remain 20.45-24.61 MiB while sampled heap busy bytes fall to about 1.43 MiB. Free
committed heap space and non-heap virtual memory prevent component/leak attribution from that difference.

The measured resource invariants hold: one 3,686,400-byte visible surface, zero composition allocations, zero
clean allocations, stable clean preparation/cache counters and zero hidden surface/cache residency. The twelve
harness captures form two byte-identical groups of six, one per scene. The lead decoded both distinct captures
and checked their populated 1280-by-720 content; structural PNG validation alone is a separate check.

The lead independently checked inventory, anchors, native/stamped data, all companion/copy hashes, FPS arithmetic
and images. Independent review reproduced 1,752 diagnostic numeric fields, sixty canonical baseline FPS cells and
48 summary FPS cells. [analysis.txt](analysis.txt) retains the calculations; [fps-ranges.receipt.txt](fps-ranges.receipt.txt)
retains the lead's independently computed ranges. Quantifying the common sampler's effect is the next bounded
attribution step. The original Grid flags, canonical workload, frozen judges and required-CI hold remain unchanged.

## Implementation validation

The `validation/` inventory retains all four declared Debug/ASan native profiling cases, 57 passing noninteractive
x64 suite/log pairs (nineteen per configuration, zero skips), their scoped receipts and three canonical unpaired
benchmark reports. The sanitizer detection probe diagnoses its deliberate isolated use-after-free. All three
ARM64 configurations build; the lead independently verifies all six current v2 build attestations and executable
bytes. These are cross-compilation results for ARM64, not native execution.

The runner summary regressions pass 29/29 and profiling tooling passes 102/102. The original third-ASan lease
receipt replays as failed foreground, skipped focus and failed pointer, with "Desktop restoration failed";
its bytes remain unchanged. The fresh Debug confirmation timed out before any of the four focus children launched.
Its four suite exits are 24 and lease exit is 21; all three recorded desktop states are unchanged. This canceled
attempt is retained under `validation/focus-Debug-attempt1/`. [Four later confirmed attempts](validation/focus/README.md)
remain separately retained on `5763d57e`: Debug passes all four suites with verified restoration, Release fails Menu
popup discovery, and ASan repeats an earlier Debug NativeTextInput initial setup failure followed by grant/restoration
failures. Three direct full post-focus states match the recorded pre-focus state; the earlier Debug failure's recorded
identity was compared with the later successful Debug retry. The new bounded setup diagnostics change test bytes;
[their corrected builds and noninteractive runs](validation/focus/setup-runs/README.md) pass all six builds and 57
suite executions with zero skips and complete actual provenance. Their ASan confirmation times out before any
foreground child runs, with verified unchanged desktop state. Fresh Debug/Release/ASan foreground validation remains
open; the earlier passes qualify their archived binaries only. Causes remain unproved.

Replay the packet with `Analyze.ps1`, which requires the retained `SHA256SUMS` and checks imported helper bytes
against the recorded profiling closure. If those helpers have changed, use their recorded `cc77bec2` versions.
`Test-DriverImports.ps1` checks the three actual import sequences and guard
availability without running native work. The source/closure fields remain authoritative: the Debug feature cases
record dirty `01f1aed1`, the ASan cases and fixed twelve-report pilot record clean `cc77bec2`. Equal library source
fingerprints alone do not substitute for their complete build/profile/helper/artifact identities.

The separate opt-in fixture uses the library-owned synthetic scene, fixed warm-up checkpoints, bounded phase
snapshots and heap-off/on profiles. Its native report and stamped provenance must remain distinct from canonical
acceptance reports. Heap totals cannot identify exact component ownership; process/thread counters cannot prove
processor placement or scheduler delay. Query failures, overflow, sampler cost and every outlier are evidence,
including when a small pilot does not reproduce the earlier extreme process slowdown.

This change has no control visuals or public product behavior. The documentation/gallery contract was reviewed;
the existing design-system guidance and published gallery remain applicable without regeneration. Native ARM64,
physical input, assistive-technology and consumer qualification remain separately owned.
