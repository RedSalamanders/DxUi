# Performance and testing

DxUi must remain fast and use little memory. The normative
[performance contract](../Specs/Core/Core_PerformanceAndResources.md) requires before/after evidence and developer
advice for a confirmed regression. A green functional suite alone does not establish performance acceptance.

## Run and compare

Before changing implementation, measure the current revision on an otherwise quiet machine:

```powershell
.\performance.ps1 -Configuration Release -Platform x64 -OutputPath .build/reports/before.json
# Implement the change, then rebuild and measure using the same fixture and settings:
.\performance.ps1 -Configuration Release -Platform x64 -OutputPath .build/reports/after.json -Baseline .build/reports/before.json
.\test.ps1 -Configuration Release -Platform x64 -PerformanceBaseline .build/reports/before.json
```

Keep the baseline; never overwrite it with the candidate. Use separate files for each configuration/architecture.
Every `test.ps1` invocation runs the complex-UI benchmark, including filtered suite runs, and embeds its scenarios
and report path in every suite receipt. Without `-PerformanceBaseline`, the result is explicitly **unpaired**.
This records throughput but cannot claim absence of regression. CI artifacts retain those measurements; a developer
must supply a matched baseline comparison before accepting an implementation change.

The fixture is 1280×720 at 96 DPI, reduced motion, 83 controls: a root, 16 cards each containing a label, toggle,
slider and progress bar, plus a Tree and four-column Grid backed by 1,000 rows. After 20 warm-up frames it runs
five rounds of 40 frames for each scenario. Clean frames reuse the prepared surface; dirty frames update 32 values,
scroll the grid and repaint. A screenshot is recorded outside timing at `.build/test-artifacts/complex-ui.png`.

Receipts record completed offscreen WARP FPS, p50/p95 total frame milliseconds, p95 preparation and CPU composition
times, C++ allocation counts, exact surface payload and replacement peak, process private bytes and working set
with sampled peaks and private-byte growth. Source commit/content fingerprint, executable/fixture hashes, compiler,
machine, CPU, OS, WARP binary version, active power policy, native architecture and configuration make the comparison
auditable. The fixture fingerprint covers the benchmark, shared sample scene and graphics helper; receipts declare
`workloadOwner: DxUi`. Changes to those inputs invalidate earlier fixture comparisons. `-SkipBuild` is recorded; the caller is responsible for matching existing binaries to the recorded sources.

FPS includes target clear and a blocking readback of one pixel into a reusable staging texture, ensuring submitted
work has completed. It excludes PNG encoding, statistics serialization and process-memory sampling. This readback
exists only in the benchmark. It is **not displayed FPS**, GPU timestamp duration or a hardware-GPU performance claim.
Private bytes/working set include the fixture and OS/driver allocations; C++ counters do not intercept all driver
allocations. Surface payload excludes driver overhead. A fixed-size fixture does not establish every consumer's budget.

The comparison uses the median of five rounds: 5% timing/FPS and 2% process-memory investigation bands; deterministic
surface and allocation budgets allow no growth. These bands identify noise, not acceptable slowdowns. Retain all
rounds, repeat suspicious results with the original baseline on the same quiet fixture, and investigate trends even
inside the bands. A mismatch or regression returns nonzero and writes a comparison receipt. Do not increase a
tolerance or replace a baseline to hide a slowdown. Present measured options: optimize, reduce optional work, or
defer/revert the new development; ask the developer for advice before accepting a confirmed degradation.

For shipping decisions also measure hardware rendering and actual presented FPS/frame pacing at the consumer's
target refresh rate, 96/144/192 DPI, large lists, text entry, animations, multiple views, resize and device recovery.
Record GPU/driver, power policy, resolution, controls/data size, texture/cache totals, p50/p95/p99 frame/input latency,
CPU time, allocations and bytes, handles, threads, wake-ups and long-run retained memory. Hidden/idle work must stay
zero. WARP numbers cannot stand in for native graphics hardware, physical input or screen-reader checks.

## Dedicated library evidence

[Retained independent measurements](../Measurements/README.md) include raw rounds and comparison receipts with a
scenario explanation. They measure the library's synthetic workload; AV adoption receipts live in RedXe.
The `dxui-complex-ui-v2` scene is a new fixture, so its baseline/repeat pair demonstrates the measurement procedure
on unchanged library code, not an implementation speedup. Never compare it to `complex-ui-v1` as if the workload
were identical. Presentation, hardware-GPU, input latency and long-duration acceptance require additional evidence.

## ARM64 evidence

On the current development computer, Windows and the PowerShell process both report **X64**. Local ARM64 validation
uses the installed cross-compiler:

```powershell
.\build.ps1 -Platform ARM64 -Configuration Debug
.\build.ps1 -Platform ARM64 -Configuration Release
```

These commands compile/link; they do not execute ARM64 code. `test.ps1 -Platform ARM64` rejects an x64 host.
`performance.ps1` requires target/native architecture to match, including rejecting x64 emulation as native evidence.
Native ARM64 runs are configured in [CI](../.github/workflows/ci.yml) on `windows-11-vs2026-arm`, in Debug and Release.
That label identifies [GitHub's native ARM64 runner](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
Only an actual successful job and its receipts establish a runtime pass; configuration and cross-builds do not.

## Other checks and formatting

Run `validate-skills.ps1`, `validate-specs.ps1`, `validate-dependencies.ps1`, `format.ps1 -Check` and
`python -m unittest discover -s Tools/tests -v`. Run x64 Debug/Release suites and build ARM64 Debug/Release for code
changes; native ARM64 CI must also pass. Use `gallery.ps1 -PublishDocs` after visual/control changes and review all
generated sheets. Full IME, touch and screen-reader adoption checks remain explicit manual gates.

[Formatting CI](../.github/workflows/format.yml) checks pushes/PRs and uploads a ready-to-apply patch. To reformat a
branch remotely, run its manual workflow with `apply_changes` enabled. It commits formatting on the selected
branch without a force push; branch protection still applies. GitHub-token commits do not trigger another push
workflow, so validation must run again explicitly or on the next user push.
