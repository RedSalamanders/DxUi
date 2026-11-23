# W0.2 frame timing method: receipts

Method: [Frame timing method](../../../Specs/Core/Core_PerformanceAndResources.md#frame-timing-method), run by
`performance-timing.ps1`. Plan item: `W0.2` in `Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md`; an input to the
D-P1 timing policy, not a replacement for it.

## Calibration: FX5 dirty (15 November 2026)

Run by the conductor on a quiet machine, the developer having set the High Performance scheme:

```powershell
./performance-timing.ps1 -Purpose Calibration -Fixture FX5 -Phase Dirty -SkipBuild
```

Source commit `9574105be` (integration tip, clean), x64 Release; NVIDIA GeForce RTX 5080, driver 32.0.16.1714; five
A/B/B/A sessions, idle preflight 8.0, 6.7, 7.6, 5.7 and 8.6% host busy (threshold 10%); UI thread pinned (`auto`). Each
run held 2,503 to 2,613 frames in 2 s, no disjoint GPU frame. Run medians: preparation 2.93 to 2.97 million cycles,
composition 5,580 to 6,009 cycles, submission 689 to 699 µs, GPU frame 741 to 756 µs (p95 863 to 1,035 µs). The UI
thread's cycle clock advances in steps of about 427 cycles on this machine, the submission clock in 0.1 µs, the GPU
timestamps in 1 ns. Receipt: [`calibration-FX5-dirty`](calibration-FX5-dirty/README.md).

| Metric | Margin | Noise floor | Minimum detectable effect | Resolution | Status |
| --- | --- | --- | --- | --- | --- |
| Median preparation cycles | 3% | 0.55% | 0.35% | 0.015% | judged |
| Median composition cycles | 3% | 7.69% | 3.42% | 7.65% | advisory: the floor is one clock step |
| p95 UI-thread cycles | 3% | 4.81% | 2.09% | 0.014% | advisory |
| Median submission time | 3% | 0.55% | 0.39% | 0.015% | judged |
| Median GPU frame time | 5% | 1.24% | 0.63% | 0.0001% | judged |
| p95 GPU frame time | 5% | 15.14% | 4.95% | 0.0001% | advisory |

Gate for FX5: met. Every judged metric's same-binary spread is below its margin, and the three others are recorded as
advisory with their noise floors.

## Calibration: FX3 dirty (15 November 2026)

Rerun by the conductor on a quiet machine with the High Performance scheme set, after the timing-adapter fix below:

```powershell
./performance-timing.ps1 -Purpose Calibration -Fixture FX3 -Phase Dirty -OutputDirectory Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX3-dirty
```

Source commit `19e970047` (clean), x64 Release; NVIDIA GeForce RTX 5080, driver 32.0.16.1714; five A/B/B/A sessions,
idle preflight 9.5, 7.7, 7.9, 9.9 and 8.5% host busy (threshold 10%). Each run held 5,514 to 5,911 frames in 2 s, every
one preparing work, with 55 to 59 rebuilds (one per 100 declared notches) and no disjoint GPU frame. Run medians:
preparation 1.13 to 1.16 million cycles, composition 4,292 cycles in every run, submission 269 to 276 µs, GPU frame
294 to 319 µs (p95 398 to 458 µs). Receipt: [`calibration-FX3-dirty`](calibration-FX3-dirty/README.md).

| Metric | Margin | Noise floor | Minimum detectable effect | Resolution | Status |
| --- | --- | --- | --- | --- | --- |
| Median preparation cycles | 3% | 1.85% | 0.87% | 0.038% | judged |
| Median composition cycles | 3% | 0.00% | 0.00% | 9.95% | advisory: a constant ten clock steps, below the clock's resolution for this margin |
| p95 UI-thread cycles | 3% | 15.04% | 11.89% | 0.034% | advisory |
| Median submission time | 3% | 1.69% | 0.83% | 0.037% | judged |
| Median GPU frame time | 5% | 5.64% | 2.66% | 0.0003% | advisory: floor above the margin |
| p95 GPU frame time | 5% | 9.05% | 6.90% | 0.0002% | advisory |

Gate for FX3: met. The two judged metrics' same-binary spreads (1.85% and 1.69%) are below their 3% margin; the other
four are recorded as advisory with their noise floors, the GPU median because its 5.64% floor exceeds its 5% margin.
FX3 GPU time therefore cannot be judged on this machine.

The p95 spread does not come from the rebuilds. The first five frames after each rebuild are 5% of a run's frames;
the first one costs about 1.47 million cycles against about 1.14 million for a settled frame, and these frames are 41
to 88 of the 276 to 296 frames at or above each run's p95. Leaving them out (a settled p95) keeps the same-binary floor
at 15.04% for UI-thread cycles and moves the GPU p95 floor only from 9.05% to 8.84%: the spread is a whole-run shift of
the upper tail (one run's settled p95 is 1.49 million cycles against about 1.25 million for its pair). A settled p95
would not be a better advisory metric, and the method is unchanged.

### The first FX3 calibration (not retained)

The first FX3 calibration (same day and machine, commit `9574105be`) timed the wrong frames. FX3's timed phase is 100
wheel notches of 3 lines down a 2,048-row Grid, which reaches its end after about 675 notches. The timing adapter played
step `n` for every frame `n`, so after 60 warm-up and about 615 timed frames each run was at the end of the list and
timed about 100,000 frames in which the wheel changed nothing: preparation stayed at 858 cycles (two clock steps) and
submission at 2.4 µs in every run, which gave 0.00% floors, and the 101,000 frames per run made a 71 MB receipt. The
adapter now plays only the declared steps: after step 99 the scene is rebuilt and its warm-up phase replayed outside
the timed frames (`Definition::timedPhaseRepeats`; FX5 declares that its phase repeats). A judged dirty run also
requires every timed frame to prepare work, and a metric whose clock step exceeds a tenth of its margin is advisory,
so neither failure can again be reported as judged.

## Admission: interim rule and the load-sensitivity run

On this 32-thread machine the original preflight (at most 10% of all processors busy, about 3.2 cores) refused or
rejected four hardware runs on 15 November while the measuring thread was pinned to one core and most cores were idle.
The runner now admits runs on what disturbs the measurement (`interference-interim-v1`, the default; the contract's
"Frame timing method" states it) and keeps the original rule selectable (`-Admission Legacy10`). The two calibrations
above were taken under the original, stricter whole-host rule and stay valid; their receipts carry no admission field,
which means `host-busy-10pct-v1`.

Evidence from data already taken, without a new run:

- The W0.7 worker's two rejected attribution runs (100 passes; other-process load 5.9 to 15.6% of the processors,
  6 passes above 10%): of 16 timing-pass pairs (one case in both runs), the 13 pairs whose cycles-per-tick ratio stayed
  at or above 0.99 differed by 0.82% at the median and 3.79% at most; the two of them whose higher load was 9% or more
  differed by at most 0.74%, the eleven below 9% by up to 3.79%. The largest difference (FX12 full, 7.3%) came from a
  pass at 0.976 cycles per tick; the other two low-ratio pairs differed by 0.40% and 0.07%. Whole-host load does not
  predict the difference; a low cycles-per-tick ratio marks the one clearly disturbed pass. (Single pairs, without a
  noise floor per case.)
- FX5 calibration, 20 runs at 9.5 to 10.8% session host load (the observer's figure, which includes the benchmark's own
  core): each run's deviation from the 20-run median correlates with the session's host load by 0.19 to 0.34 for the
  judged metrics. Runs 2 to 4 of session 5 had a 5th-percentile cycle rate (thread cycles per microsecond of
  submission) about 6% below the other seventeen runs (3,972 to 3,980 against 4,226 to 4,239); they carried the three
  largest GPU p95 deviations (+13.3 to +17.8%; correlation -0.95), while their session's host load (10.8%) was ordinary.
  The p95 GPU floor (15.14%) comes from them.
- FX3 calibration, 20 runs at 10.3 to 12.5%: correlations 0.01 to 0.20; the cycle rate stayed flat (4,212 to 4,221),
  and nothing recorded explains its p95 and GPU-median spreads, which therefore look intrinsic to the fixture.

Interim limits (to be replaced by the load-sensitivity run): running share at least 0.99 at the median and 0.97 at the
5th percentile; sibling at most 10% busy; other work on the measuring processor at most 5%; other processes at most a
quarter of the logical processors (8 here) in the preflight and during each run; other processes at most 15% of the
measured GPU's busiest engine (an idle desktop measured about 8% on the 3D engine in a 7.6 s sample). Only the
running-share limits rest on the evidence above; the others are conservative and untested.

The load-sensitivity run for a quiet slot, with the High Performance scheme set, about ten minutes after the build
(30 sessions: six arms of five, four runs each):

```powershell
./performance-timing.ps1 -Purpose LoadSensitivity -Fixture FX5 -Phase Dirty -PreflightSeconds 1 -CalibrationPath Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json -OutputDirectory Measurements/PerformanceAndMotion_2026-11-13/W0.2/load-sensitivity-FX5-dirty
```

It pins the measuring thread to the quietest core and, in each arm, runs A quiet and B under 0, 2, 4, 8 or 12 busy
logical processors away from that core or one on its sibling (`DxUi.EmbeddedTests.exe --busy-load`, its own child,
ended after each B run). The receipt names the largest load level at which FX5's three judged metrics stay equivalent
within their margins, and whether a busy sibling is admissible; every run also records the interference indicators, so
the sibling and own-processor limits can be set from the same data.

## Admission, 23 November: core isolation, rule v2 and the partial load-sensitivity study

The hardware slot of 23 November (tip `2b09df9e6`, Release, High Performance, RTX 5080, no builds running) showed two
problems, fixed by `interference-interim-v2` and core isolation.

**The sibling was our own process.** The W0.7 attribution tool, which judges its passes with
`Test-FrameTimingRunAdmission`, had every FX12 4K pass refused with the measuring processor's sibling 0.46 to 0.82 busy
while other processes kept only 3.3 to 4.8 logical processors busy and the measuring thread's median running share stayed
at 0.996 to 0.998 in the timing and phase passes (rejected records of runs 074153Z and 080630Z). FX12's Direct2D, DirectWrite and driver worker threads
landed on the measuring core's second hardware thread. They do disturb the measurement: in the load-sensitivity study
one busy sibling thread made FX5 46 to 52% slower. The harness now isolates the measuring core: the measured process's
default CPU set excludes the measuring core and the measuring thread selects its own processor (no elevation). The
Embedded self-test starts helper threads after isolation and checks that none ever runs on the measuring core. A WARP
smoke run of FX12 on this machine while other workers were building (other processes 13 logical processors, the
machine about 68% busy) read sibling 0.60 shared and 0.61 to 0.66 isolated, close to the machine's average processor load,
so it cannot show the difference; on an idle machine the scheduler leaves an idle sibling idle (quiet FX5 runs read 0.00
to 0.03), and the hardware smoke below shows FX12 before and after. FX12 now declares its timed "full" phase
repeatable, so the harness no longer rebuilds 2,000 controls every four frames.

**The load-sensitivity study stopped at session 27 of 30** (5.5 minutes): the preflight saw 9.4 busy logical
processors. It was the desktop, not the study's load: no `DxUi.EmbeddedTests` process was alive in that preflight's
host observations (the previous arm's load child had exited), and the busiest processes were git, Zoom, OneDrive and
the observer itself, with the host at 25.9% against 8.5 to 19.8% in the 26 earlier preflights. A refused preflight now
waits 30 s and retries up to 10 times, the study's own processes (observer, runner, load children) are left out of the
preflight figure, every admitted session is checkpointed, and running the same command resumes the study.

**Partial result** (not a receipt: 26 of 30 sessions, read from the raw reports of the aborted study; measuring
processor 28; the loaded runs against the quiet runs of the same sessions; the three metrics FX5's calibration judges):

| Extra busy logical processors | Sessions | Preparation cycles | Submission time | GPU frame time | Equivalent within margin |
| --- | --- | --- | --- | --- | --- |
| 0 (control) | 5 | +0.04% (-0.65 to +0.50) | +0.04% | +0.11% | yes |
| 2, away from the core | 5 | +1.59% (+0.88 to +2.35) | +1.60% | +1.58% | yes |
| 4, away | 4 | +3.23% (+2.34 to +4.40) | +3.26% | +3.41% | no (above 3%; 4 sessions cannot show equivalence) |
| 8, away | 4 | +4.21% | +4.22% | +4.45% | no |
| 12, away | 4 | +5.66% | +5.76% | +7.42% | no |
| 1, on the sibling | 4 | +51.6% | +51.4% | +46.1% | no |

The measuring thread was not preempted under load elsewhere (its 5th-percentile running share stayed at 0.991 to
0.995), so the running share cannot catch this slowdown: the total-load cap and a new session steadiness clause do.
Two extra busy logical processors stay within the margin, four do not, so v2 rejects a session whose four runs' other
load differs by more than 2 logical processors and keeps the cap at 8 (the 2-processor arm's loaded runs reached 8.46).
Other work on the measuring processor read 0.00 to 0.06 in every arm, and the W4.0 comparisons were rejected at 0.050
and 0.058 under the 5% limit and completed on repeat; v2 allows 10%. Running shares of 0.93 at the 5th percentile (two
rejections in those comparisons) stay rejected, as do 8.11 busy logical processors (one).

**Next quiet slot** (High Performance set; after the build):

```powershell
# FX12 before and after core isolation, hardware smoke runs (one session each, about a minute):
./performance-timing.ps1 -Purpose Smoke -Fixture FX12@uhd-192dpi -Phase Dirty -Sessions 1 -CoreIsolation Off -OutputDirectory .build/reports/frame-timing/fx12-shared
./performance-timing.ps1 -Purpose Smoke -Fixture FX12@uhd-192dpi -Phase Dirty -Sessions 1 -SkipBuild -OutputDirectory .build/reports/frame-timing/fx12-isolated
# The load-sensitivity study, bracketing the boundary between 2 and 4 (25 sessions, about 9 minutes; resumable):
./performance-timing.ps1 -Purpose LoadSensitivity -Fixture FX5 -Phase Dirty -SkipBuild -PreflightSeconds 1 -LoadLevels 0,1,2,3,4 -NoSiblingArm -CalibrationPath Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json -OutputDirectory Measurements/PerformanceAndMotion_2026-11-13/W0.2/load-sensitivity-FX5-dirty
```

If it stops, the same command resumes it. The two retained calibrations were taken under the stricter original rule
without isolation; their judged statuses stand.

## Retained form

A committed receipt holds per-run summaries (the six statistics, the clock steps and resolution, frame counts), each
raw run report's path, SHA-256 and size, and the judged statistics: 43 KB for the FX5 calibration, against 2.3 MB with
the per-frame samples. The runner writes the full receipt (`receipt-full.json`), the raw reports and the host logs to
`.build/reports/frame-timing/raw/`; `Tools/Convert-FrameTimingReceipt.ps1` turns an existing full receipt into the
retained form and refuses if any summary, floor, minimum detectable effect, equivalence test or status would change.
## Smoke run under load: `smoke-under-load-FX5-dirty`

Not a calibration. One session, run to prove the command works end to end while other workers were building
(idle preflight recorded 34.3% host busy, `-IdlePreflight Record`), with the power scheme unchanged
(`-PowerPlan Keep`, Balanced) and `-Purpose Smoke`. Its same-binary deviations (0.9–12.5%) describe a loaded machine,
not the method's noise floor, and must not be used as one. That directory's README is generated by the runner.

- Source: base `693b1981` plus the uncommitted W0.2 change (`sourceDirty: true`); x64 Release, v2 build attestation
  checked before and after; compiler `195237025`.
- Fixture: `FX5`, ComplexUi 83 controls, 1280×720, 96 DPI, dirty frames, 60 warm-up frames per process.
- Adapter: NVIDIA GeForce RTX 5080, driver 32.0.16.1714 (UMD), vendor `0x10DE`, device `0x2C02`. The D3D11 timestamp
  disjoint query reported a 1 GHz frequency and no disjoint frame in 9,388 frames.
- Pinning: measuring UI thread on logical processor 31 (all processors report efficiency class 0).
- Runs (A, B, B, A, one executable): 2,332 / 2,379 / 2,310 / 2,367 frames in 2.00 s each. Median preparation 3.07–3.10
  million cycles, median composition 6,009–6,867 cycles, median submission 723–730 µs, median GPU frame 793–805 µs,
  p95 GPU frame 1,047–1,162 µs, median wait for the one-frame-late result 67–69 µs.
- Retained: `receipt.json`, converted to the retained form (15 KB); the raw files and the host observation logs
  stayed under `.build/reports/frame-timing/raw/`.