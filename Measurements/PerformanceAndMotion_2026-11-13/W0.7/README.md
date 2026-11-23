# W0.7 hardware attribution receipt

Plan item `W0.7` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
method: "Hardware attribution method" in [the performance contract](../../../Specs/Core/Core_PerformanceAndResources.md).
The receipt is [attribution.json](attribution.json).

## Receipt

- Recorded 23 November 2026, 20:03 to 20:21 UTC, in two runs: the decision cases (run `Receipt-Hardware-20261123T200342Z`,
  `-CaseSet Decision`), then the rest (run `Receipt-Hardware-20261123T201148Z`, `-ResumeFrom` the first). Every pass was
  admitted at its first attempt (0 retries used).
- Source commit `8f3b34ee3` (clean), x64 Release, attested build; NVIDIA GeForce RTX 5080, driver 32.0.16.1714, LUID
  0x00000000_0x0001adfe; AMD Ryzen 9 9950X3D; High performance power scheme (read, never switched).
- Admission rule `interference-interim-v2`: every embedded pass on an isolated measuring core (isolation took effect in
  every pass), sibling at most 0.02 busy, other processes 2.3 to 4.1 logical processors busy; idle preflight 10.9% (3.5 of 32
  logical processors).
- Kernel sampling refused (`0xc5585011`); module shares come from the in-process sampler.
- The receipt's library identity (`workloadOwner`, `fixture`) and the SHA-256 of its 31 input files (`benchmarkInputs`) were
  added after the measurement by `./performance-attribution.ps1 -StampReceipt`, from the clean source commit the receipt
  records (`8f3b34ee3`, files as a checkout writes them); no recorded figure changed. Receipts the tool publishes now carry
  them from the start.
- Each case: three passes of at least 10 s and 100 frames (timing, phases, sampling). "UI-thread CPU" is the timing pass's
  median preparation plus composition; "GPU busy" the GPU engines' running time per frame; "Inside Direct2D" the phases
  pass's share of the frame inside Direct2D calls (`EndDraw`, draw calls, `BeginDraw`, `Flush`); "DxUi own" the rest of
  preparation; the leaf shares are the sampling pass's.

The tables below are generated from the receipt by
`Import-Module ./Tools/FrameAttribution.psm1; Format-FrameAttributionReceipt -Receipt (Get-Content -Raw Measurements/PerformanceAndMotion_2026-11-13/W0.7/attribution.json | ConvertFrom-Json -AsHashtable -Depth 60)`
(`Tools/tests/Test-FrameAttribution.ps1` checks that they match the receipt).

| Case | UI-thread CPU median (ms) | GPU busy (ms) | GPU below CPU | Inside Direct2D | EndDraw | DxUi own | Draw calls | Leaf d2d1 / dwrite / driver / system / DxUi | Attempts |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FX5@fhd-144dpi full` | 0.501 | 0.711 | no | 93.6% | 32.7% | 6.4% | 399 | 70 / 3 / 9 / 10 / 5% | 3 |
| `FX5@fhd-144dpi dirty` | 0.541 | 0.663 | no | 92.8% | 29.9% | 6.8% | 399 | 68 / 6 / 5 / 13 / 5% | 3 |
| `FX5@uhd-192dpi full` | 0.464 | 0.085 | yes | 92.8% | 27.5% | 7.2% | 399 | 74 / 3 / 3 / 11 / 5% | 3 |
| `FX5@uhd-192dpi dirty` | 0.507 | 0.085 | yes | 92.0% | 25.1% | 7.5% | 399 | 68 / 7 / 2 / 14 / 5% | 3 |
| `FX3@fhd full` | 0.313 | 0.035 | yes | 81.2% | 21.0% | 18.8% | 414 | 60 / 8 / 3 / 9 / 15% | 3 |
| `FX3@fhd dirty` | 0.390 | 0.036 | yes | 82.1% | 18.3% | 17.0% | 414 | 52 / 13 / 3 / 18 / 11% | 3 |
| `FX3@uhd-192dpi full` | 0.336 | 0.073 | yes | 82.8% | 21.0% | 17.1% | 414 | 62 / 9 / 5 / 8 / 12% | 3 |
| `FX3@uhd-192dpi dirty` | 0.416 | 0.075 | yes | 83.5% | 20.7% | 15.7% | 414 | 52 / 14 / 4 / 15 / 11% | 3 |
| `FX12 full` | 8.502 | 11.973 | no | 93.4% | 1.6% | 6.6% | 5085 | 72 / 3 / 8 / 11 / 3% | 3 |
| `FX12 dirty` | 8.327 | 11.914 | no | 95.6% | 1.7% | 4.4% | 5085 | 70 / 5 / 6 / 10 / 6% | 3 |
| `FX12@uhd-192dpi full` | 9.465 | 11.608 | no | 94.4% | 1.6% | 5.6% | 5085 | 72 / 4 / 5 / 11 / 5% | 3 |
| `FX12@uhd-192dpi dirty` | 9.287 | 11.895 | no | 96.0% | 1.7% | 3.9% | 5085 | 73 / 4 / 5 / 10 / 4% | 3 |
| `FX12@form-layout-10k full` | 34.332 | 3.763 | yes | 84.5% | 0.2% | 15.5% | 29004 | 59 / 10 / 1 / 12 / 15% | 3 |
| `FX12@form-layout-10k dirty` | 38.986 | 4.347 | yes | 78.0% | 0.2% | 22.0% | 29004 | 68 / 8 / 1 / 14 / 7% | 3 |
| `FX12@form-layout-10k-uhd full` | 38.715 | 4.380 | yes | 78.3% | 0.2% | 21.7% | 29004 | 59 / 9 / 1 / 13 / 14% | 3 |
| `FX12@form-layout-10k-uhd dirty` | 42.620 | 4.656 | yes | 78.9% | 0.2% | 21.1% | 29004 | 61 / 9 / 1 / 12 / 13% | 3 |

Window host, full frames: `FX12` `Present` 0.057 ms median (p95 0.083; render 11.577 ms); `FX12@uhd-192dpi` `Present` 0.041 ms median (p95 0.069; render 10.785 ms).

| Case | UI-thread CPU (ms) | GPU busy (ms) | GPU below CPU | Inside Direct2D | Reading | 4 ms minus CPU / 8.33 ms minus CPU |
| --- | --- | --- | --- | --- | --- | --- |
| `FX12@uhd-192dpi full` | 9.465 | 11.608 | no | 94.4% | W10.4 entry condition is evaluated | -5.465 / -1.135 ms |
| `FX12@form-layout-10k-uhd full` | 38.715 | 4.380 | yes | 78.3% | W10.4 entry condition is evaluated | -34.715 / -30.385 ms |

### Decision rule (`W0.7`)

The rule, as the plan states it: if a full frame of `FX12` costs at most 4 ms at 4K with GPU below CPU, `W4` proceeds and
`W10.4` stays closed. If it exceeds 8.33 ms and `d2d1`, `d3d11` and driver submission dominate, `W10.4`'s entry condition is
evaluated. Between the two, `W4` proceeds and the receipt is repeated after `W4` before `W10.4` is judged.

- `FX12@uhd-192dpi` full frame (the 2,000-control screen at 4K of budget `G5`): 9.465 ms of UI-thread CPU, 1.135 ms above
  8.33 ms and 5.465 ms above 4 ms; 94.4% of the frame inside Direct2D calls (inclusive), 77% of leaf samples in `d2d1` and
  `d3d11` with the driver. The receipt reads: `W10.4`'s entry condition is evaluated.
  **GPU busy time is above UI-thread CPU time here: 11.608 ms against 9.465 ms** (GPU timestamp timeline 11.786 ms median).
  The rule's 8.33 ms branch names no condition on GPU time; its "GPU below CPU" condition belongs to the 4 ms branch, which
  this frame misses on CPU alone. A GPU-bound frame means that a lower CPU cost alone would not bring the frame under 4 ms.
- `FX12@form-layout-10k-uhd` full frame (10,000 library-laid-out controls at 4K): 38.715 ms of CPU, 30.385 ms above 8.33 ms;
  GPU busy 4.380 ms, below CPU; 78.3% inside Direct2D calls. The receipt reads: `W10.4`'s entry condition is evaluated.
- Calibration noise floors beside them (`FX5` dirty, W0.2): preparation cycles 0.55%, submission 0.55%, GPU frame 1.24%
  (judged). Both distances from the thresholds are far beyond them.
- The decision is the developer's; this is the receipt's reading of the rule.

### Budget `G5`

`G5` (full repaint of a 2,000-control screen at 4K on hardware: at most 4 ms of CPU, GPU time below CPU time), measured on
`FX12@uhd-192dpi`: 9.465 ms of UI-thread CPU and 11.608 ms of GPU busy time. **Not met** on either condition.

## Scenario

- Fixtures, each at 1920x1080 and 3840x2160, a full frame (`ControlHost::Invalidate` with nothing changed) and a typical
  dirty frame, on the embedded host:
  - `FX5` (ComplexUi, 83 controls): `FX5@fhd-144dpi` (the 1280x720 DIP scene filling 1920x1080) and `FX5@uhd-192dpi`
    (the scene in the top-left 2560x1440 of the 4K surface); dirty frame: its timed phase (16 values and a Grid row).
  - `FX3` (2,048-row Grid): `FX3@fhd` and `FX3@uhd-192dpi`; dirty frame: its timed phase, one wheel notch of the 100 it
    declares, the scene rebuilt (untimed) before the phase repeats.
  - `FX12` (dense screen, 2,000 absolutely positioned controls): default (1920x1080, 96 DPI) and `FX12@uhd-192dpi`; and the
    10,000-control form laid out by the library, `FX12@form-layout-10k` and `FX12@form-layout-10k-uhd`; dirty frame: one
    hover change.
- Window host: `FX12` and `FX12@uhd-192dpi` full frames on a hardware `WindowHost` of the non-activating test window at
  (-32000, -32000), off every monitor, for `Present`.

## Commands

```powershell
./build.ps1 -Configuration Release -Platform x64       # an attested build of the committed tree
./performance-attribution.ps1 -Purpose Receipt -SkipBuild -CaseSet Decision -NoiseFloorReceipt Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json
./performance-attribution.ps1 -Purpose Receipt -SkipBuild -ResumeFrom <the first run's directory> -NoiseFloorReceipt Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json
```

The run reads the power scheme and refuses unless High performance is already active; it never switches it. A pass that the
frame timing method's admission rule does not admit, or whose frames did no work, is measured again once the host is quiet
(up to three attempts per pass, twelve retries in all). The receipt is published only when every pass of the whole matrix
is admitted. Raw per-frame reports stay under `.build/reports/frame-attribution/`, with their SHA-256 in the receipt.

## Earlier, rejected runs (15 November 2026, older commit, not this receipt)

Two complete runs on commit `4d7208b55` were rejected under the original 10% whole-host rule, before core isolation and
before the items that landed between that commit and `8f3b34ee3`. Their passes measured `FX12@uhd-192dpi` full at 16.9 ms of
UI-thread CPU (GPU busy about 12 ms) and `FX12@form-layout-10k-uhd` full at 81.6 to 82.0 ms (GPU busy about 16 ms), against
9.5 ms and 38.7 ms in this receipt. They are kept only as that comparison: the receipt above supersedes them.