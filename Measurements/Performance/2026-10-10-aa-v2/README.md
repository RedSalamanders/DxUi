# Guarded same-binary A/A calibration, 10 October 2026

The repaired v2 build/provenance harness completed all 144 reports and 72 same-side controls. Both declared
judges reproduce `within-noise-budget` for all three aggregate sets, with all 78 migrated metric cells
`within-noise`. **Every control is unstable in at least one metric (72/72); all 54 timing/process-memory cells
drift at least once.** The policy remains `policy-review-required`, `trusted: false`. Strict controls are
inconclusive for every scenario, and required-policy qualification explicitly refuses an A/A study.
No required CI rule is enabled and no ordinary implementation non-regression pass is established.

This is a new packet. The [earlier A/A packet](../2026-10-10-aa/README.md) keeps its original pre-v2 reports,
harness and analysis unchanged. The [layout/forms and Grid packet](../../Layout/2026-10-10/README.md) records
the candidate's separate functional and resource diagnostics; this A/A study measures the old library twice.

## Source, execution and retained bytes

The invocation from `C:/Users/eric/.codex/worktrees/performance-forms/DxUi` was:

```powershell
./performance-paired.ps1 -CalibrationAA -BaselineRevision 60e1cbff8f7a75757e9447073965d09101d9833c -CandidateRevision 60e1cbff8f7a75757e9447073965d09101d9833c -Configuration Release -Platform x64 -Scenario Default,MultilineGrid,MultilineGridDistinct -Blocks 12 -Seed 20261010 -OutputDirectory C:/Users/eric/.codex/tmp/dpf-aa-final
```

Original output is under `C:/Users/eric/.codex/tmp/dpf-aa-final/20261010T174833Z-60e1cbff8f7a`.
One physical baseline tree was built once and its exact executable used for both labels. Library source is
`60e1cbff8f7a75757e9447073965d09101d9833c`; the clean frozen harness is
`e0a57ece2965c848d71c4f94cfef4fae9a196aa9`. Five verified tooling overlays were applied to the baseline and
restored afterward. The receipts correctly record `sourceDirty: true` for that overlay, while the library
fingerprint remains the original baseline. The complete 17-path harness inventory is retained in the summary.

The driver started at 17:48:33 UTC and finished at `2026-10-10T18:52:36.3387316Z`, about 64 minutes including
restore/build and before/after byte guards. Report completion times span `17:52:59.7775875Z` to
`18:52:18.8423033Z`. The person at this machine agreed to a quiet window of up to 2½ hours; this task's builds
and tests were stopped during measurement. No continuous ambient-load trace establishes that other activity
was absent. The receipts identify AMD Ryzen 9 9950X3D, Windows `10.0.26300`, Balanced power policy and WARP.
The three unchanged synthetic fixtures use 1280×720 at 96 DPI. Completed offscreen frames include clearing
and blocking one-pixel readback; they do not measure swap-chain presentation.

All 433 original reports are byte-preserved: [summary.receipt.txt](summary.receipt.txt), 144 raw native receipts,
144 per-run comparisons, 72 cross-label comparisons and 72 same-label controls. The build and incremental
receipts, build log and driver log are retained alongside them. [SHA256SUMS](SHA256SUMS) seals the reports and
analytical companions; [.gitattributes](.gitattributes) prevents checkout conversion.

All native receipts carry one full, verifiable v2 build binding. The retained executable SHA256 is
`5C6E40CA84636C870F220B5091E91381772089841669D896961FF8B5052AA8A7`, and the library fingerprint is
`E6DB772710D1C2050CC2A7B6F3D38AC07F1A21EB84EAB046A209AEFB70591570`.
The complete output artifact identity is
`e5fc0d4dd14a0728a6113679e298d45646cb810d7c4f3c7ac798cc8189109e9f`;
build attestation identity is `abbd43c974254c0f125916d8e1666064b55bab3597fa54e12304de802533b380`.
The shared build-receipt envelope is version 1 with `outcome: PASSED`; its contained identity and native
`buildIdentityVersion: 2` bind the `dxui-performance-build-v2` algorithm. Envelope and identity versions are
different concepts. Summary SHA256 is `9C6980663ACD92DC6AD217A6C98E1F5A9162683320C17DCADBECE14A337671D9`.

## Reproduced control and timing results

[Analyze.ps1](Analyze.ps1) derives descriptive statistics from all raw reports and verifies all 3,744 control
before/after values against their five-round medians. [Audit.ps1](Audit.ps1) independently replays the declared
schedule, both full judges, consumed sets and all 26-slot controls. Both judges have normalized SHA256
`EB1B3047ED8B031383AEBA4D9A72A37D3719B1320A46381EFF042897CCB72E33` and version
`dxui-paired-block-sign-flip-holm-v2`. Seed `20261010` reproduces seven ABBA and five BAAB blocks under
`independent-ABBA-BAAB-v1`; order counts were not forced. No retained decision or study purpose was rewritten.

The inventory is 144 reports, 1,440 rounds, 57,600 completed frames and 1,872 control metric observations.
611 observations exceed the unchanged bands: 390/720 timing observations and 221/576 process-memory
observations. All 576 deterministic-budget observations hold. FPS values below are medians of 48 run medians
with their full range; each drift column counts out of 24 controls, using 5% timing/FPS and 2% memory bands.

| Scene / phase | FPS median [minimum, maximum] | FPS drift | Prepare p95 drift | Private bytes drift | Working set drift |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default / clean | 2,576.13 [86.20, 2,851.26] | 14 | 11 | 18 | 14 |
| Default / dirty | 632.76 [45.89, 675.08] | 7 | 12 | 10 | 4 |
| MultilineGrid / clean | 2,621.60 [709.91, 2,904.04] | 9 | 7 | 18 | 10 |
| MultilineGrid / dirty | 127.03 [68.19, 139.68] | 11 | 14 | 10 | 5 |
| MultilineGridDistinct / clean | 2,553.85 [1,934.11, 2,851.70] | 16 | 4 | 10 | 3 |
| MultilineGridDistinct / dirty | 598.26 [458.54, 659.67] | 12 | 18 | 11 | 3 |

All 24 exact metric cells hold under the unchanged median and raw-peak rules. Surfaces and replacement peaks
are 3,686,400 bytes, and clean/composition allocation calls are zero. Default dirty rounds contain 2,145 or
2,160 C++ allocation calls, with every run median and the common raw peak at 2,160; both Grid fixtures have
1,080 calls per dirty round. All hidden-work counters are zero. These facts do not close timing/memory instability.

All 172,800 timing samples lie on the recorded 100 ns QPC grid (10 MHz), although the C++ duration period is
nominally 1 ns. Across 9,600 clean preparation samples per scene, the sample median is two ticks; 3,044/4,089/3,444
are at most one tick, including 7/13/7 zero samples. Clean round-p95 medians are three ticks, with ranges
2–33 / 2–11 / 2–9. Dirty preparation is much larger. Default A07 and A19 have clean run medians of 126.16 and
86.20 FPS; the A19→A20 control changes clean FPS by +2,980.87% and dirty FPS by +1,072.00%. Quantization cannot
explain those process-level outliers. Their labels and block position do not establish an order effect or cause.

## Memory observations and developer advice

The analysis retains all six stage snapshots, adjacent-stage deltas, role distributions and block/order
statistics. Private-byte span grows from `scene` to `warm`: 332→4,872 KiB for Default, 708→6,380 KiB for
MultilineGrid and 428→2,760 KiB for MultilineGridDistinct. Per-report warm-to-capture private-byte increments
have medians 4,152/4,152/4,156 KiB, with ranges 3,948–4,292 / 3,948–4,416 / 3,820–4,232 KiB.
Substantial spread therefore precedes PNG capture. `hidden` is taken after timed rounds and visibility change,
before teardown; capture-to-hidden differences include all that work and cannot isolate the cost of hiding.
Stage snapshots do not identify an allocator, a component or a leak. The earlier Grid preparation and roughly
1.05 MB resident-memory flags remain unresolved by this same-binary study or the candidate-only retention runs.

Keep the existing judge, bands, exact budgets and required-rule hold. Before another complete calibration,
run a bounded diagnostic pilot on a controlled host to trace initialization, WARP/backend warm-up, scheduling
and the unusually slow processes. Preserve every outlier. A separately reviewed batched clean-preparation
diagnostic could quantify work below one QPC tick, but would change metric scope and cannot replace the
canonical metric without contract/policy review. Stable same-binary controls and independent seed-policy
review must precede an ordinary candidate comparison against a qualified trusted base. Consumer performance,
native ARM64 input and physical IME/AT/touch qualification remain separately owned.

Recompute without building or running native programs from a checkout with the declared harness bytes:

```powershell
./Measurements/Performance/2026-10-10-aa-v2/Audit.ps1
./Measurements/Performance/2026-10-10-aa-v2/Analyze.ps1
```

The artifact check additionally needs the original run directory (or an explicit retained artifact root).
Analytical outputs are new companions, not modifications of the original reports. There is no library-change,
policy, dedicated-host or product-wide production qualification claim in this packet.
