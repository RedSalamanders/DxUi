# PR #80 paired-control investigation

Status: diagnostic evidence; performance/policy qualification remains **HOLD**. Reviewed 17 October 2026.
This follow-up replays retained bytes and inspects the frozen source; it executes no new native benchmark.

The [qualification ledger](../../../Specs/Plans/WIP/ConsumerUiQualification_2026-10-11.md#pr-80-paired-control-investigation-17-october-2026)
owns concrete next steps and every remaining gate. The [performance contract](../../../Specs/Core/Core_PerformanceAndResources.md)
owns thresholds and acceptance. No threshold, baseline, original receipt or policy decision is changed here.

## Study identity and actual outcomes

[PR #80 workflow](https://github.com/RedSalamanders/DxUi/actions/runs/39017319370) measures base
`99e8b313b6ae68cc52949b113b9867001d605b24` against tested merge candidate
`652c1ecbca4ec5c48668cadfdce3d81d6ec54b90`, using a clean x64 Release/WARP harness and seed `425707354`.
The candidate, implementation head `b1de5280422d16cb80a3669119eda9b1b5010432` and actual merge
`02b59a617665f897730ce269c3764ea88943338d` share tree `eb667e342329ad0e3bb33b39b4106d8f544d5c6f`.
Tree equality does not equate separately built binaries. The assignment is twelve independent ABBA/BAAB blocks,
48 invocations per scenario, 144 raw receipts and 1,440 raw clean/dirty rounds (five per phase per receipt).

All 144 receipt hashes, the seeded schedule, both complete frozen judge decisions and the 72 full same-side
comparisons reproduce. Declared v2 identity, scheduled role/source/profile and absolute allocation checks hold.
Both judge source hashes are `EB1B3047ED8B031383AEBA4D9A72A37D3719B1320A46381EFF042897CCB72E33`.
This hash is a review target in the unqualified policy, not independent approval.

The measurement step succeeds. The verdict publisher exits 1 because the base policy remains `qualified=false`,
`qualificationStatus=policy-review-required`; the PR aggregate fails on that upstream job. This refusal precedes
the qualified path's control/judge review. Control instability and method disagreement are additional unresolved
qualification evidence, not the error actually emitted by that first refusal.

The separate [post-merge run](https://github.com/RedSalamanders/DxUi/actions/runs/39020397326) passes all six native
jobs and its aggregate: 128 suite receipts, zero skips and eight relocated consumers at the actual merge.
All ten new regression functions complete in each native profile. Push deliberately skips paired measurement.
The developer's implementation-only merge exception does not qualify performance or any remaining gate.

## What the unstable-control count means

A control compares the two runs of one unchanged executable within a block. It is unstable if any of its 26
phase/metric observations changes in either direction beyond the existing 5% timing/FPS or 2% memory band, or
changes an exact budget. These checks use the complete comparisons replayed from raw rounds, not just saved
labels. They are band crossings, not additional significance tests or causal regression findings.

| Scenario | Unstable controls | Timing/FPS controls | Memory controls | Drifting timing/memory slots | Migrated regression flags |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default | 24/24 | 24 | 5 | 18/18 | 0/26 |
| MultilineGrid | 23/24 | 22 | 13 | 18/18 | 0/26 |
| MultilineGridDistinct | 24/24 | 24 | 1 | 14/18 | 0/26 |

Timing and memory counts overlap: 52 controls have timing-only drift, one memory-only drift, 18 both and one
neither. Fifty of 54 timing/memory scenario-phase slots drift at least once; including the 24 held exact-budget
slots, that is 50/78. There are 310 band crossings among 1,872 control-metric observations. Baseline controls
are unstable in 35/36 comparisons and candidate controls in 36/36. Inner adjacent-run controls are unstable in
35/36 comparisons; outer controls in 36/36. The longer outer separation alone cannot explain the pattern.
The only completely stable control is `MultilineGrid-block-03-baseline-control`.

The following counts are out of 24 controls per scenario. Every omitted exact-budget row has zero crossings.

| Phase | Metric | Default | MultilineGrid | MultilineGridDistinct |
| --- | --- | ---: | ---: | ---: |
| clean | fps | 7 | 2 | 1 |
| clean | frameP50Ms | 7 | 2 | 0 |
| clean | frameP95Ms | 7 | 4 | 2 |
| clean | prepareP95Ms | 16 | 16 | 15 |
| clean | composeCpuP95Ms | 16 | 14 | 17 |
| clean | privateBytes | 1 | 2 | 1 |
| clean | privatePeakBytes | 1 | 2 | 1 |
| clean | workingSetBytes | 1 | 1 | 0 |
| clean | workingSetPeakBytes | 1 | 1 | 0 |
| dirty | fps | 9 | 7 | 4 |
| dirty | frameP50Ms | 7 | 2 | 0 |
| dirty | frameP95Ms | 18 | 7 | 13 |
| dirty | prepareP95Ms | 11 | 9 | 13 |
| dirty | composeCpuP95Ms | 15 | 11 | 11 |
| dirty | privateBytes | 4 | 7 | 1 |
| dirty | privatePeakBytes | 4 | 7 | 1 |
| dirty | workingSetBytes | 1 | 5 | 1 |
| dirty | workingSetPeakBytes | 2 | 1 | 1 |

## Timing witnesses and attribution limits

All 28,800 clean preparation samples lie on the declared 100 ns QPC grid. The 240 round-p95 observations per
scenario span 4-9 ticks in Default, 4-17 in MultilineGrid and 4-16 in Distinct; their medians are 5/6/5 ticks.
A single tick changes these small measurements by more than the 5% band. This demonstrates sensitivity to
resolution; it does not establish that every observed difference is quantization or harmless noise.
Clean preparation drifts in 47 controls, while 67 controls also drift in timing/FPS metrics other than preparation.
All original metrics remain in acceptance.

`MultilineGridDistinct-block-03-baseline-control` changes clean preparation from 0.0005 to 0.0014 ms (+180%).
That is a 0.9 microsecond difference. Larger-scale observations also remain:

- `Default-B07.json`, dirty round 4, records 5.25024 completed offscreen FPS, frame-p95 584.4771 ms,
  prepare-p95 199.6355 ms and composition-p95 0.0451 ms. Six preparation samples exceed 50 ms.
  Its frame 18 lasts 2,619.8089 ms, including preparation 199.6355 ms and composition 0.0196 ms.
- The remaining 2,420.1538 ms in that frame is outside the separately timed Prepare/Composite calls.
  The [frozen fixture](../../../Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h) also includes model update,
  target binding and a blocking staging-copy/Map completion in the total frame. Preemption can affect elapsed
  spans too. This remainder is not an isolated GPU-time measurement and cannot identify scheduling, WARP,
  model work or allocator behavior without a trace.
- The same-candidate B07/B08 dirty control compares run-median FPS 70.13371519 to 141.1536701 (+101.26364%),
  and frame-p95 29.5279 to 7.7628 ms (-73.71029%). The 584 ms round remains retained even though the
  producer's median-of-five projection and final paired set do not flag an aggregate regression.

No retained continuous host-load, CPU-ready or completion trace proves the cause of these stalls. Neither
matching executable hashes nor the later B08 measurements dismiss B07. The workload measures completed
offscreen throughput with blocking completion, not display refresh or hardware qualification.

## Memory witness and exact resources

`MultilineGrid-block-04-baseline-control` (A07/A08, inner BAAB control) is the memory-only unstable control.
Dirty private bytes rise from 29,036,544 to 29,679,616 (+643,072 bytes, 628 KiB, +2.21470%); private peak rises
from 30,142,464 to 30,781,440 (+638,976 bytes, 624 KiB, +2.11985%). Its timing/FPS metrics stay inside their bands.

Between these two processes, the entry/scene private-byte differences are only 8/32 KiB, but the difference is
620 KiB after the canonical 20-frame warm-up, before capture, and 668 KiB after capture. Thus capture alone
cannot explain this witness's footprint difference. These snapshots do not identify an allocator, native object,
leak or cause; the final hidden snapshot also precedes teardown. This source-specific observation does not
attribute or erase the earlier local MultilineGrid 1.28/1.31/1.54 MiB flags.

All 24 exact resource slots satisfy both median and raw peak checks. Across 1,440 raw rounds, composition and
clean C++ allocations are zero; dirty C++ allocations peak at 2,160 per 40 frames (54/frame), below the fixed
Release limit of 64/frame. Surface/replacement peaks are 3,686,400 bytes on both sides, and hidden preparation/
composition counters are zero. These facts do not bound every native allocation or explain process-memory drift.

## Why the judges disagree

Both methods reproduce `within-noise-budget` overall for all three scenarios, but Distinct dirty `frameP95Ms`
has one different metric label. The [legacy calculation](../../../Tools/PerformanceComparison.psm1) pools
24 five-round run medians per side; the migrated method forms twelve independent block log-ratios and applies
the exact sign-flip test and Holm correction across all 26 predeclared slots.

| Method | Effect | Probability evidence | Metric verdict |
| --- | --- | --- | --- |
| Legacy pooled runs | medians 6.76695 to 6.35555 ms; -6.079548% | Mann-Whitney p=0.03193627519 | improved |
| Migrated paired blocks | mean log effect -0.03993941995; -3.915235% | 264/4,096 assignments; raw p=0.064453125; Holm p=1 | within-noise |

An independent enumeration from the raw per-run medians reproduces the 264 extreme assignments. The raw
paired p-value ranks fourth among 26 slots; its Holm multiplier is 23 and its term clamps to 1. The disagreement
already exists before correction: the paired effect is inside 5% and raw p exceeds 0.05. Different estimators,
independent units and multiplicity account for the labels; no arithmetic/replay mismatch was found.

The legacy pooled-run result cannot qualify the migrated independent-block policy. The
[equivalence check](../../../Tools/BenchmarkGate.psm1) intentionally compares every phase/metric label, including
improvements. Retain `verdictAgreement=false`; matching headline statuses does not pass that check. Policy
approval requires independent review and fresh calibration, not a special case for this favorable label.

## Retention, reproduction and limits

The original `paired-benchmark-x64-Release` artifact is ID `12097910143`; at collection its expiry is
31 October 2026, 09:59 UTC. All 435 original report/comparison/log/verdict files and a complete SHA-256 seal
are retained privately. This publication adds Markdown only and excludes raw machine/workspace/foreground data.
Selected immutable inputs for reproducing the calculations are:

| Input | SHA-256 |
| --- | --- |
| summary.json | `23201420670D37D2D9A1DF5CF531091EED2E476F1BF2C54BA6D97CEAC6D21C88` |
| Default-B07.json | `FA5F17BBDFF1F29B285D1743D6D2343F489DBB803029BB421CE3CBBE4B6851EA` |
| MultilineGrid-A07.json | `CF4875C8BC2A94E4E24BF48B9590899D111A0D728AFC9019568A4FD694C2CA9F` |
| MultilineGrid-A08.json | `22A85D6E02C7D9D1245A439AFDF6CD6AF94D53D3D3C89F7D0D0E5357EF56AF89` |

Recover the named artifact from the linked run or the retained private original, verify the summary and all
144 `sameReceiptsSha256` bindings, replay the seed with `Get-RandomizedPairedBlockSchedule`, and load the immutable
base/candidate judges with `Get-VersionedPerformanceJudge`. Recompute both complete sets and each scheduled
same-side comparison with `Compare-PerformanceSet`, `Compare-PairedBlockSet` and `Compare-PerformanceReceipt`.
Derive drift in both directions using the unchanged `Add-ControlMetricChanges` rule, and verify the complete
decisions with `Test-EquivalentGateDecision` in the gate module. These are report-level checks; no native run
or change to original metadata is needed.

The upload omits actual executable/library/symbol and compiler/SDK/dependency bytes. Declared full-v2 identities
and their consistency were checked; independent binary/toolchain rehashing remains unavailable from that upload.
A rebuild cannot restore the missing original bytes; the next fresh study must retain its own binary/toolchain
evidence privately from the start.
Strict-control replay is inconclusive in every scenario, and required-policy replay stops at the unqualified
base policy. Zero migrated regression flags establish neither accepted non-regression nor resolution of the
earlier local memory excursion. All remaining qualification gates stay open in the owning ledger.

This evidence-only change affects no runtime, public usage, test workflow or visuals; gallery regeneration is
unnecessary. Diagnostics, prospective fixture experiments and policy approval remain separately scoped work.
