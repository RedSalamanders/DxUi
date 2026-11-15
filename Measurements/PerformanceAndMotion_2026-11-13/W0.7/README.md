# W0.7 hardware attribution receipt

Plan item `W0.7` of the [performance and motion plan](../../../Specs/Plans/WIP/PerformanceAndMotion_2026-11-13.md);
method: "Hardware attribution method" in [the performance contract](../../../Specs/Core/Core_PerformanceAndResources.md).

**Status: the receipt `attribution.json` is not recorded yet.** Two complete quiet-slot runs on 15 November 2026 were
rejected because other processes kept the host above 10% busy during a few passes (see below). The figures in this file come
from those rejected runs or from smoke runs, and say so.

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
- Per case three fresh processes of at least 10 s and 100 frames each: timing (uninstrumented), phases (time inside
  Direct2D and DirectWrite calls, `W0.1` counters), sampling (in-process module sampling).
- Kernel sampling: refused on this workstation. The unelevated `wpr.exe -start CPU` fails with `0xc5585011` ("Failed to
  enable the policy to profile system performance"); `logman start "NT Kernel Logger"` fails with access denied
  (`0x80070005`). The receipt records the refusal and the command an administrator would run; module shares come from the
  in-process sampler, which needs no rights.

## Commands

```powershell
./build.ps1 -Configuration Release -Platform x64       # an attested build of the committed tree
# The two cases the decision rule reads, then the rest, reusing their clean passes:
./performance-attribution.ps1 -Purpose Receipt -SkipBuild -CaseSet Decision -NoiseFloorReceipt Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json
./performance-attribution.ps1 -Purpose Receipt -SkipBuild -ResumeFrom <the first run's directory> -NoiseFloorReceipt Measurements/PerformanceAndMotion_2026-11-13/W0.2/calibration-FX5-dirty/receipt.json
```

The run reads the power scheme and refuses unless High performance is already active; it never switches it. A pass that the
frame timing method's admission rule does not admit (`interference-interim-v2`: on a measuring core isolated from the process's
other threads, the measuring thread's running share, its sibling and own processor, other processes' load and GPU busy time) or whose frames did no work is measured again once the
host is quiet, up to three attempts per pass and twelve retries in all. The receipt is published only when every pass of the whole
matrix is clean; otherwise `attribution.rejected.json` (or `attribution.partial.json` for part of the matrix) stays beside the
raw reports under `.build/reports/frame-attribution/`, with a checkpoint that a later run resumes from.

## Receipt

Filled from `attribution.json` once it is recorded; `…` marks a number still missing.

- Source commit `…`, x64 Release, attestation `…`; adapter `…`, driver `…`; power scheme High performance (read, never
  switched); admission rule `…`; idle preflight `…` busy; run `…` resumed from `…`.
- Kernel sampling: refused (`0xc5585011`); module shares from the in-process sampler.

| Case | Frame | UI-thread CPU median (ms) | GPU busy (ms) | Inside Direct2D | EndDraw | DxUi own | Draw calls | Leaf d2d1 / dwrite / driver / system / DxUi | Attempts |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `FX5@fhd-144dpi` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX5@uhd-192dpi` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX3@fhd` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX3@uhd-192dpi` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX12` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX12@uhd-192dpi` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX12@form-layout-10k` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |
| `FX12@form-layout-10k-uhd` | full / dirty | … / … | … / … | … / … | … / … | … / … | … | … | … |

Window host, full frames: `FX12` `Present` … ms median (render … ms); `FX12@uhd-192dpi` `Present` … ms (render … ms).

### Decision rule (`W0.7`)

| Case | UI-thread CPU (ms) | GPU busy (ms) | GPU below CPU | Inside Direct2D | Reading | Margin to 4 ms / 8.33 ms |
| --- | --- | --- | --- | --- | --- | --- |
| `FX12@uhd-192dpi` full | … | … | … | … | … | … / … |
| `FX12@form-layout-10k-uhd` full | … | … | … | … | … | … / … |

Calibration noise floors beside them (`FX5` dirty, W0.2): preparation cycles 0.55%, submission 0.55%, GPU frame 1.24%
(judged). The decision is the developer's; this is the receipt's reading of the rule.
## From the two rejected runs (15 November 2026, not a receipt)

Source `4d7208b55`, x64 Release, NVIDIA GeForce RTX 5080, driver 32.0.16.1714, High performance active, other workers
paused. Each figure is from a pass whose own other-process load is given; the runs were rejected because of other passes
(10.3% to 15.6%), and every pass quoted here was within 10%.

| Case, full frame | UI-thread CPU (median, ms) | GPU engine busy (ms/frame) | Inside Direct2D calls | Leaf samples d2d1 / dwrite / system / driver / DxUi |
| --- | --- | --- | --- | --- |
| `FX12@uhd-192dpi` | 16.94 (timing 8.7%), 16.93 (timing 6.8%) | 11.8 to 12.0 | 96.5% | 50 / 18 / 22 / 4 / 2.5% |
| `FX12@form-layout-10k-uhd` | 81.99 (9.8%), 81.63 (6.5%) | 16.1 to 16.5 | 88% | 35 / 26 / 28 / 0.5 / 6% |
| `FX5@fhd-144dpi` | 0.86, 0.85 | 0.63 to 0.76 | 95.5% | 52 / 16 / 20 / 5 / 3% |
| `FX5@uhd-192dpi` | 0.74, 0.74 | 0.08 | 95% | 52 / 18 / 21 / 2 / 3.5% |
| `FX3@fhd` | 0.37, 0.37 | 0.04 | 84% | 61 / 10 / 11 / 2 / 11% |
| `FX3@uhd-192dpi` | 0.40, 0.40 | 0.07 | 85% | 58 / 13 / 11 / 3 / 11% |

Read against `W0.7`'s rule, both `FX12` 4K full frames are far above 8.33 ms, with GPU busy time below CPU time and
88 to 97% of the frame inside Direct2D calls: the reading would be "W10.4's entry condition is evaluated". It is not a
receipt. Window-host `Present`: 0.05 ms median at both sizes. The per-pass table with load, pinned processor and cycles per
tick was retained in the build tree for the admission-rule work (`.build/reports/w07-rejected-passes.txt`).
