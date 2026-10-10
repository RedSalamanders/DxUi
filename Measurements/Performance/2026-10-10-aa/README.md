# Original A/A calibration, 10 October 2026

This packet retains the original three-scene A/A study at
`60e1cbff8f7a75757e9447073965d09101d9833c`. Both labels used the same clean source and executable.
Both original judges reported `within-noise-budget` for each scene, and the migrated judge labelled all 78 metric
cells `within-noise`. **All 24 same-binary controls in each scene were unstable (72/72 overall).** The retained
acceptance status is `policy-review-required`, with `trusted: false`. This packet does not establish a gate pass,
qualify a policy or measure a library change.

All 433 original reports retain their exact JSON bytes: the original aggregate `summary.json` is named
[summary.receipt.txt](summary.receipt.txt), alongside 144 raw receipt `.json` files, 144 per-run comparisons,
72 cross-label comparisons and 72 same-label controls. The aggregate uses the repository's report filename
convention so validators distinguish it from a native receipt. [SHA256SUMS](SHA256SUMS) covers these files and
the analytical companions. The local [.gitattributes](.gitattributes) disables text conversion so the original
CRLF bytes survive checkout. [analysis.txt](analysis.txt) contains newly derived JSON statistics; no retained
judge result was rewritten.

Original reports came from
`C:/Users/eric/.codex/tmp/dpf-aa/20261010T115238Z-60e1cbff8f7a/reports`.
The successful invocation, run from `C:/Users/eric/.codex/worktrees/performance-forms/DxUi`, was:

```powershell
./performance-paired.ps1 -CalibrationAA -BaselineRevision 60e1cbff8f7a75757e9447073965d09101d9833c -CandidateRevision 60e1cbff8f7a75757e9447073965d09101d9833c -Scenario Default,MultilineGrid,MultilineGridDistinct -Blocks 12 -Seed 20261010 -OutputDirectory C:/Users/eric/.codex/tmp/dpf-aa
```

Release/x64 were the defaults. Receipt completion times span `2026-10-10T11:57:04.5085108Z` to
`2026-10-10T12:11:19.0719575Z`. The earlier long-output-path attempt collected no receipts and is not part of this
packet. The host reports an AMD Ryzen 9 9950X3D, Windows `10.0.26300` and the Balanced power plan. There is no
dedicated-host or quiet-host claim. Frames are completed offscreen WARP work, including clear and a blocking
one-pixel readback, at 1280×720/96 DPI; they do not measure swap-chain presentation or vsync.

All 144 receipts share executable SHA256 `0DA932EA7A2971BF942C1EFB7DAE00B3312482E7979AC99BAFD014E25BFE640F`
and source fingerprint `E6DB772710D1C2050CC2A7B6F3D38AC07F1A21EB84EAB046A209AEFB70591570`.
The original clean harness is the same revision, before the subsequent gate integrity repair. Its recorded
`Tools/BenchmarkGate.psm1` hash remains `02DD68E2A79E2D06C7369DEFAB8917500CB1320F59BB0BDA3BC8377B1DFE155D`;
the original judges remain `dxui-paired-block-sign-flip-holm-v2`. Full fixture, dependency, compiler, environment
and harness identities remain in the receipts and summary. The summary SHA256 is
`71C7D726AF0FB4A6E296A317C84BE1BDBFE3CA5C49128C718AA6A4944368E928`.

## Complete control reading

The analysis covers 144 receipts, 1,440 rounds and 57,600 timed frames; 72 controls contribute 1,872 metric
observations. It verifies all 3,744 retained control before/after values against the raw five-round medians and
checks all 78 set baseline/candidate medians against the raw receipts. It preserves all 78 original migrated
metric records alongside descriptive statistics. These statistics do not rerun an inferential acceptance test.

The counts below are controls exceeding the original investigation band for that metric: 5% timing/FPS and
2% process memory. Every scene/phase has 24 controls. FPS is the median of the 48 run medians, with the full
range in brackets.

| Scene / phase | FPS median [min, max] | FPS drift | Prepare p95 drift | Private bytes drift | Working set drift |
| --- | ---: | ---: | ---: | ---: | ---: |
| Default / clean | 1,925.48 [9.90, 2,626.27] | 21 | 17 | 15 | 8 |
| Default / dirty | 506.15 [35.38, 651.21] | 16 | 22 | 12 | 6 |
| MultilineGrid / clean | 2,323.20 [1,496.20, 2,694.09] | 18 | 14 | 17 | 8 |
| MultilineGrid / dirty | 120.77 [90.18, 132.68] | 16 | 18 | 4 | 0 |
| MultilineGridDistinct / clean | 2,389.37 [1,768.39, 2,833.32] | 18 | 11 | 17 | 9 |
| MultilineGridDistinct / dirty | 587.20 [382.68, 652.26] | 14 | 20 | 11 | 5 |

At least one control drifted in 52/78 metric cells. All 24 exact-budget cells stayed constant: surface bytes and
replacement peak were 3,686,400 bytes; composition allocations were zero; C++ allocations were zero in clean
rounds and 2,160/1,080/1,080 per dirty round for Default/MultilineGrid/MultilineGridDistinct. The two remaining
non-exact cells with no drift were MultilineGrid dirty working set and its peak. Exact-budget stability does
not close the unstable timing/process-memory controls.

## Memory stages

Each cell is median [minimum, maximum] across all 48 receipts, in KiB (1,024 bytes). These are process snapshots,
not allocation attribution. The
[original snapshot source](https://github.com/RedSalamanders/DxUi/blob/60e1cbff8f7a75757e9447073965d09101d9833c/Tests/Embedded/Embedded.Tests.ComplexUiBenchmark.h)
takes `entry` before device creation, `device` after WARP device creation, `scene` after model/scene setup, `warm`
after completion-texture creation and 20 warm frames, `capture` after PNG capture, and `hidden` after all timed
rounds and the hidden no-work checks. `hidden` is before teardown; capture-to-hidden includes timed rendering
as well as the visibility change.

| Scene | Stage | Private KiB | Working-set KiB |
| --- | --- | ---: | ---: |
| Default | entry | 2,064 [2,036, 2,096] | 10,888 [10,860, 10,916] |
| Default | device | 6,530 [6,508, 6,556] | 15,416 [15,396, 15,444] |
| Default | scene | 8,178 [7,996, 8,628] | 19,168 [19,124, 19,324] |
| Default | warm | 22,306 [19,540, 23,652] | 36,572 [34,296, 37,528] |
| Default | capture | 26,470 [23,632, 27,836] | 42,242 [39,936, 43,220] |
| Default | hidden | 28,396 [21,648, 29,516] | 44,142 [37,608, 45,200] |
| MultilineGrid | entry | 2,072 [2,036, 2,104] | 10,888 [10,856, 10,924] |
| MultilineGrid | device | 6,536 [6,516, 6,564] | 15,420 [15,396, 15,444] |
| MultilineGrid | scene | 8,484 [8,320, 8,844] | 19,540 [19,464, 19,644] |
| MultilineGrid | warm | 30,948 [27,908, 35,420] | 41,954 [40,432, 43,480] |
| MultilineGrid | capture | 35,082 [31,928, 39,572] | 47,570 [46,128, 49,112] |
| MultilineGrid | hidden | 33,566 [32,388, 34,964] | 49,162 [48,304, 50,084] |
| MultilineGridDistinct | entry | 2,068 [2,032, 2,096] | 10,888 [10,856, 10,908] |
| MultilineGridDistinct | device | 6,532 [6,508, 6,552] | 15,416 [15,400, 15,432] |
| MultilineGridDistinct | scene | 9,026 [8,924, 9,392] | 20,160 [20,120, 20,408] |
| MultilineGridDistinct | warm | 24,804 [23,880, 26,280] | 38,850 [37,688, 40,072] |
| MultilineGridDistinct | capture | 28,942 [27,828, 30,504] | 44,524 [43,364, 45,800] |
| MultilineGridDistinct | hidden | 30,682 [29,016, 31,824] | 46,780 [45,500, 48,008] |

The private-byte span grows from 632/524/468 KiB at `scene` to 4,112/7,512/2,400 KiB at `warm`. The wider
spread is therefore already present before capture. Per-receipt warm-to-capture increments are comparatively
consistent: private medians 4,154/4,148/4,156 KiB, ranges 3,984–4,300 / 3,948–4,296 / 3,820–4,368 KiB;
working-set medians 5,668/5,672/5,688 KiB, ranges 5,544–5,788 / 5,500–5,756 / 5,472–5,824 KiB.
Capture-to-hidden private changes include decreases: ranges −2,992–3,840 / −6,112–1,576 / 172–2,908 KiB.
Those changes do not isolate hiding, identify an allocator/component, or establish a leak. All adjacent-stage
increments and their A/B/block statistics are retained in the analysis.

## Labels, order and clocks

The seeded schedule contains seven ABBA and five BAAB blocks. For each block the analysis compares the median
of its two B-labelled runs with the median of its two A-labelled runs. The table shows medians of those paired
effects within each order. These are effects of labels assigned to the same binary; they do not estimate an
implementation change or prove that order causes the observed difference.

| Scene | Clean FPS B/A %, ABBA / BAAB | Warm private B−A KiB, ABBA / BAAB | Warm working set B−A KiB, ABBA / BAAB |
| --- | ---: | ---: | ---: |
| Default | +9.81 / −11.57 | +186 / +562 | +142 / +512 |
| MultilineGrid | −4.16 / −1.61 | +110 / −534 | +374 / −30 |
| MultilineGridDistinct | +3.35 / −1.15 | +338 / +16 | +288 / −102 |

For Default clean FPS the individual block effects range from −4.47% to +99.83% in ABBA and −27.04% to
+651.99% in BAAB. The complete 78-cell analysis retains the two role distributions and every block effect;
the descriptive ratios above differ from the frozen judge's mean log-effect statistic.

All 144 receipts report `std::chrono::steady_clock` backed by QueryPerformanceCounter at 10,000,000 ticks/s:
100 ns per tick. The nominal 1 ns C++ duration period is not the measured clock resolution. Across 240 clean
rounds per scene, prepare-p95 tick medians [ranges] are 4 [2, 158], 4 [2, 11] and 3 [2, 8]. Of 9,600 clean
prepare samples per scene, 997/1,291/2,274 are at most one tick, including 1/3/5 zero samples. Clean preparation
is near clock quantization scale. Dirty preparation is much larger (round-p95 medians 17,014/87,605/13,888.5
ticks); clock resolution alone cannot explain the study's broad timing/process-memory variability.

Recompute only the analytical companions and manifest with
`./Measurements/Performance/2026-10-10-aa/Analyze.ps1` from the repository root. The script validates and reads
the retained data using the original judge source recovered from Git; it builds or executes no native program.
The [performance contract](../../../Specs/Core/Core_PerformanceAndResources.md) owns qualification. These
original receipts remain historical evidence under their original harness and judges.
