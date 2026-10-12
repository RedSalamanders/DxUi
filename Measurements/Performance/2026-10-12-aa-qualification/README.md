# Same-binary A/A qualification attempt, 12 October 2026

This fresh x64 Release/WARP calibration completed all three canonical scenes with one physical executable for both labels. The migrated aggregate judge returns `within-noise-budget` for each scene, but **all 72 same-binary controls are unstable**. The legacy and migrated judges disagree on three timing slots. The accepted policy remains `policy-review-required`; this study does not establish paired non-regression or qualify a trusted base.

## Source, schedule and retained evidence

The clean source and harness are commit `0bd0e99c482ccd230dce949c65686791df682769`. The invocation from `Z:/src/DxUi` was:

```powershell
./performance-paired.ps1 -CalibrationAA -BaselineRevision 0bd0e99c482ccd230dce949c65686791df682769 -CandidateRevision 0bd0e99c482ccd230dce949c65686791df682769 -Configuration Release -Platform x64 -Scenario @('Default','MultilineGrid','MultilineGridDistinct') -Blocks 12 -Seed 20261012 -OutputDirectory .build/paired/qualification-aa-20261012
```

The run started at 17:38:10 UTC and completed at 19:13:14 UTC. The 12 independently drawn blocks comprise seven ABBA and five BAAB orders. The person at this machine agreed to a quiet window; this task ran no other builds or tests during measurement. There is no continuous ambient-load trace, so the host cannot be certified as dedicated. Each synthetic scene uses an offscreen WARP renderer; completed frames include blocking readback and are not display cadence.

The original 433 reports are byte-preserved here: [summary.receipt.txt](summary.receipt.txt), 144 native receipts, 144 per-run comparisons, 72 cross-label comparisons and 72 same-label controls. The original summary SHA256 is `D6980895604AB6CBECCBE7D968CF2697EF27FDD9A3A4E85A2762E146474591F1`. The one executable SHA256 is `419349DAFAAC1DB44026DA22ABB8A7C7E5748F7D7052E9447D8EAECD6E504037`; library source fingerprint is `D1C3380BFDDAA3A1BD37534C666FF24A29C5936A27185DBFA4D26EE8B0D11A46`. Build artifact identity is `8c0926e58b2894bfa8ecd0d63c1279d1db00642aebf16c817dfe23c5bca2a624` and build attestation identity is `68448430befbf3872317b2d6f57978cb2186b82d77cf55e1c7e92acc6fbc9467`. The retained build and incremental receipts, build log and [driver log](driver.log) explain the build and execution. [.gitattributes](.gitattributes) prevents checkout conversion of the original bytes; [SHA256SUMS](SHA256SUMS) seals the packet.

## Replayed result

[Audit.ps1](Audit.ps1) verifies the original summary, raw hashes, full scheduled roles, both frozen judges, all controls and the actual retained executable/build attestation at the original run root. Its output is [audit.receipt.txt](audit.receipt.txt). [Analyze.ps1](Analyze.ps1) reconstructs descriptive statistics from all 144 raw receipts and independently checks all 3,744 control before/after medians; the resulting [analysis.txt](analysis.txt) records the calculations without executing a native program.

The run contains 1,440 raw rounds and 57,600 timed frames. All 72 controls have at least one drifting metric. Of 78 timing/memory metric cells, 54 drift at least once; 883 of 1,872 control metric observations exceed their unchanged investigation bands. The migrated judge reports zero regressed slots. The legacy judge reports Default clean `composeCpuP95Ms`, Default dirty `frameP95Ms`, and Default dirty `prepareP95Ms` as regressed while the migrated judge classifies each within noise. Neither decision is an accepted regression attribution because the same executable was measured on both sides and the controls are unstable.

FPS values below are descriptive medians of 48 run medians, with the full range in brackets. Drift counts are out of 24 same-side controls per scene and phase.

| Scene / phase | FPS median [minimum, maximum] | FPS drift | Prepare p95 drift | Private bytes drift | Working set drift |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default / clean | 2,167.31 [186.82, 2,601.93] | 17 | 16 | 19 | 15 |
| Default / dirty | 551.72 [278.67, 639.01] | 19 | 20 | 12 | 9 |
| MultilineGrid / clean | 1,679.47 [466.60, 2,437.36] | 20 | 21 | 21 | 7 |
| MultilineGrid / dirty | 99.37 [64.84, 126.05] | 17 | 17 | 9 | 6 |
| MultilineGridDistinct / clean | 1,504.90 [52.74, 2,481.28] | 21 | 24 | 18 | 12 |
| MultilineGridDistinct / dirty | 381.85 [66.47, 593.02] | 24 | 17 | 15 | 9 |

All 720 clean rounds have zero tracked C++ allocations. The maximum dirty round is 2,160 allocations against the unchanged Release ceiling of 64 per frame × 40 frames = 2,560; composition allocations are zero. These absolute limits hold, but do not resolve timing or memory spread. The clock reports 10 MHz QueryPerformanceCounter ticks (100 ns) despite the nominal 1 ns C++ duration period. Tick quantization can affect tiny clean timing values; it does not explain large process-level FPS or memory outliers by itself.

## Decision and next measurement

Keep the policy unqualified and preserve the existing bands and exact budgets. A bounded controlled-host pilot should trace initialization, WARP/backend warm-up and scheduling around slow processes before another full calibration. Preserve outliers and test a causal explanation against same-binary controls. A change to timing granularity or metric scope needs independent contract/policy review before it can enter the required CI rule. The separate hosted A/A run and ARM64 foreground results must be evaluated at their own exact source and environment identities. Native ARM64, physical touch, real IME, screen-reader and consumer acceptance remain separate gates.

The separate [hosted A/A packet](hosted/README.md) comes from [workflow run 38288233925](https://github.com/RedSalamanders/DxUi/actions/runs/38288233925). Its reports-only replay finds 71/72 unstable controls and 39/78 drifting timing/memory cells. Both judges agree on the aggregate, but the strict and required-policy results are inconclusive. The upload lacks the executable and build inventory for independent artifact rehashing.

The read-only replay commands are:

```powershell
./Measurements/Performance/2026-10-12-aa-qualification/Audit.ps1
./Measurements/Performance/2026-10-12-aa-qualification/Analyze.ps1
```

The artifact audit needs the original ignored `.build` run root or `-ArtifactRoot` pointing to a byte-preserved copy of its baseline tree. This packet makes no dedicated-host, hardware, consumer or release qualification claim.
