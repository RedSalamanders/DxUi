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

The separate opt-in fixture uses the library-owned synthetic scene, fixed warm-up checkpoints, bounded phase
snapshots and heap-off/on profiles. Its native report and stamped provenance must remain distinct from canonical
acceptance reports. Heap totals cannot identify exact component ownership; process/thread counters cannot prove
processor placement or scheduler delay. Query failures, overflow, sampler cost and every outlier are evidence,
including when a small pilot does not reproduce the earlier extreme process slowdown.

This change has no control visuals or public product behavior. The documentation/gallery contract was reviewed;
the existing design-system guidance and published gallery remain applicable without regeneration. Native ARM64,
physical input, assistive-technology and consumer qualification remain separately owned.
