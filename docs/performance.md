# Performance and testing

DxUi must remain fast and use little memory. The normative
[performance contract](../Specs/Core/Core_PerformanceAndResources.md) requires before/after evidence and developer
advice for a confirmed regression. A green functional suite alone does not establish performance acceptance.
The [Grid text-overflow investigation](../Measurements/GridTextOverflow/2026-09-21/README.md)
retains matched original/candidate reports and unresolved resource flags; it is not an acceptance record.
The [rejected associative-cache experiment](../Measurements/GridTextOverflow/2026-09-23/assoc-cache-rejected/README.md)
reduces layout churn but increases private memory; its prototype was restored, and invalid incremental
comparison attempts remain explicitly excluded from qualification. The
[review follow-ups](../Specs/Plans/Done/ReviewFollowUps_2026-09-29.md) later adopted set-associative Grid layout
tables under a frame-rate-first priority, which the performance contract records with their
[paired runs](../Measurements/ReviewFollowUps/2026-09-29/paired-local/README.md). The
[reliability follow-ups](../Specs/Plans/Done/ReliabilityAndFollowUps_2026-09-30.md) release a grid's layouts when it
stops painting; their [six-run paired set](../Measurements/ReliabilityAndFollowUps/2026-09-30/paired-local/README.md)
is the first judged by the rank-test verdict.

## Run and compare

Before changing implementation, measure the current revision on an otherwise quiet machine:

```powershell
.\performance.ps1 -Configuration Release -Platform x64 -OutputPath .build/reports/before.json
# Implement the change, then rebuild and measure using the same fixture and settings:
.\performance.ps1 -Configuration Release -Platform x64 -OutputPath .build/reports/after.json -Baseline .build/reports/before.json
.\test.ps1 -Configuration Release -Platform x64 -PerformanceBaseline .build/reports/before.json
```

Keep the baseline; never overwrite it with the candidate. Use separate files for each configuration/architecture.
When the benchmark itself changed, or the baseline predates it, measure both revisions with one harness:

```powershell
.\performance-paired.ps1 -BaselineRevision <commit> -Scenario Default,MultilineGrid,MultilineGridDistinct
```

It creates a detached baseline worktree under `.build/paired`, copies this checkout's `performance.ps1`, comparator
and benchmark inputs into it, builds both trees and runs the interleaved pass A1, B1, B2, A2 serially (A is the
baseline). `-Repetitions` (default 3, at most 10) repeats that pass as A3, B3, B4, A4 and so on, so each side ends
with six runs per scenario by default. `-SkipBuild` reuses the existing build of this checkout and of named
trees (below); detached worktrees always build. `-CandidateRevision` measures a second
historical revision instead of this checkout, still with this checkout's harness.

What a set establishes. A single pass gave each side two runs, and on 2026-09-30 all 14 same-binary controls on the
developer laptop drifted beyond their bands, so no set there established anything. The verdict is now the set's, not a
pass's. For every phase (clean, dirty) and metric, an exact two-sided Mann-Whitney U test compares the baseline's run
medians with the candidate's (each run's median over its five rounds; six against six by default), and the
candidate's median shift is compared with the metric's investigation band:

- A metric is `regressed` (or `improved`) only when p < 0.05 and the candidate's median lies beyond its band, 5%
  for timing and FPS and 2% for process memory, on the worse (better) side. Anything else is `within-noise`: a
  significant shift inside the band, and a shift beyond it that the runs cannot separate from noise, are both
  reported with their values and neither is a verdict.
- Exact budgets (surface bytes, replacement peak, C++ allocations) stay exact: any candidate run above the
  baseline runs' median is `regressed`, whatever the test says.
- The set is `advice-required` when any metric is `regressed`, otherwise `within-noise-budget`, which says that no
  change was established, not that none exists. Advice-required needs the developer's advice as before (optimize,
  reduce scope or defer, never a silent rebaseline), once the same set repeats on a quiet fixture: about 26 metric
  tests run per scenario, so a chance verdict is possible.
- The runs a set can separate limit what it can establish. Complete separation of six runs against six reaches
  p = 2/924 = 0.0022, four against four 0.029, and two against two only 0.33, which can never reach 0.05: a single
  pass (`-Repetitions 1`) can establish only a rise in an exact budget. Each set records this smallest attainable p.
- Each side's own spread across its runs is printed with every verdict as context and never vetoes one. The
  per-pass comparisons (B1/A1 and B2/A2 cross the change; A2/A1 and B2/B1 are same-source controls, marked
  `unstable-control` when they drift beyond a band in either direction) are kept for continuity and are context too.

`summary.json` keeps every run's medians and, for each scenario, the set's per-metric run values, medians, shift,
spread, p-value and verdict; `Compare-PerformanceSet` in `Tools/PerformanceComparison.psm1` judges a set again from its
retained receipts.

To measure work that is not a commit, name the tree instead: `-BaselinePath` and `-CandidatePath` take the top of an
existing DxUi working tree with its dependencies restored (`vcpkg-install.ps1`), such as one feature worktree against
another, or a revision against this checkout's uncommitted edits
(`-BaselineRevision <commit> -CandidatePath .`). A named tree is measured as it is. The harness files that differ
from this checkout's are written into it for the run, with the originals saved under the run directory, and put back
when the run ends; its build output stays, and its receipts report `sourceDirty` when the overlay changed anything. Two
revisions, or a revision and this checkout, are refused when both name one commit. A pair with a named tree is
refused when both trees have the same library source fingerprint (`src`, `include`, `Build`, the build props and the
vcpkg manifests), because nothing differs to measure; that is how uncommitted work on a revision's own commit is
compared with it. `-SkipBuild` on a named tree needs its existing build, and refuses a tree whose overlay changed a
compiled benchmark input, since that build predates the harness its receipts would name. `summary.json` records each
side's revision or path, commit and source fingerprint. Every receipt, comparison and
`summary.json` is retained; an `advice-required` set still needs developer advice.

A merge does not have to wait for a quiet developer machine: the [validation workflow](../.github/workflows/ci.yml) runs
the set on one hosted x64 Release runner (about 14 minutes). Its
[contract](../Specs/Core/Core_PerformanceAndResources.md#hosted-paired-gate) has the rules; in short:

- **Pull requests.** Every pull request to `main` that changes something the benchmark measures (library sources and
  headers, the build props and vcpkg manifests, the sources of the benchmark executable, its fixtures and samples, the
  build and measurement scripts and the workflow; never Markdown) runs the `paired-benchmark` job. It compares the pull
  request's merge ref with that merge commit's first parent, the base as the merge ref was made, in `Default`,
  `MultilineGrid` and `MultilineGridDistinct`, three repetitions each: six runs per side. The `benchmark-scope` job decides
  on Linux and its summary names the files that did; a pull request that changes none of them skips the Windows job. A
  skipped job satisfies a required check, so the check `paired-benchmark (pull request)` (named apart from the push and
  manual runs of the same job, so that a required check names one check run) can be made required in branch protection
  without holding documentation changes.
- **Manual runs.** A manual run with `benchmark_baseline` (and optionally `benchmark_candidate` and `benchmark_scenarios`)
  measures the revisions it names, with three repetitions. It publishes the verdict the same way and stays green for a
  finding; only a pull request is gated. A manual run is its own concurrency group, so a later push does not cancel it,
  while a newer push to a pull request cancels that pull request's older run.
- **What is published.** The job summary lists each scenario's set verdict and every metric's medians, change, p-value,
  band, same-binary controls and outcome, flagged metrics first. `paired-benchmark-x64-Release` keeps every receipt,
  comparison, `summary.json` and `verdict.json` for 14 days; copy a run worth keeping into `Measurements`.
- **What fails a pull request.** A *confirmed degradation*: a metric the set flagged whose own same-binary controls stayed
  within its band (an exact budget: stayed equal). It follows the regression rule above, and nothing is rebaselined: the
  gate measures the base afresh. An *inconclusive* run fails too: a metric was flagged but its controls drifted beyond the
  band, so the flag cannot be told from the runner. GitHub has no neutral job conclusion and a green check would read as a
  pass, so the job fails; re-run it (a new runner), repeat the set on a quiet machine, and treat the listed metrics as
  findings, not as noise. A run with no flagged metric passes as *no regression established*, which is not evidence that
  none exists. Hosted controls drift in several timings in almost every run (the
  [calibration set](../Measurements/HostedPairedGate/2026-10-01/README.md) records 18 of 18), which is why the controls of
  the flagged metric decide and not all twenty-six; exact budgets never drift, so a rise in one always confirms.
- **Unchanged library code.** When both sides have one library fingerprint (a change to the benchmark or its tooling
  only) the same code was measured twice: a timing or memory flag is listed as noise and a rise in a deterministic budget
  still fails.
- **No verdict.** A run that cannot finish fails with the reason in its summary. The usual cause is a pull request that
  changes the harness and the library's interfaces together: its base cannot be built with the merge ref's harness, so
  measure that comparison by hand with `performance-paired.ps1`, as before.
- **Approved tradeoffs.** The gate has no waiver list. A tradeoff the developer approves is recorded in the
  [contract](../Specs/Core/Core_PerformanceAndResources.md) with its measured budget, as the accepted ones are, and a
  maintainer merges over the failed check.

To judge a local or retained run the same way, point the verdict script at its reports directory:
`./Tools/Publish-BenchmarkVerdict.ps1 -Reports .build/paired/<run>/reports [-Gate]` prints the summary, writes `verdict.md`
and `verdict.json` beside `summary.json` and, with `-Gate`, exits 1 for anything but a pass. A laptop in use drifts in
every control, so a flagged metric there reads as inconclusive.

Hosted runs are serial on one machine but not a controlled quiet desktop; record that limitation.
`-Scenario MultilineGridRetention` extends the French multiline fixture with six complete passes through
its 1,000 rows. It records process memory, handles and retained surface bytes every 200 frames, after
clearing the Grid model, and after destroying the control tree/detaching the host. Compare identical
harness hashes and configurations. This bounded retention experiment is separate from the common
short benchmark and does not establish hours-long, multiple-view or hardware presentation acceptance.
`-Scenario MultilineGridHeap` adds process-local heap walks outside the timed rounds.
It reports busy/free blocks, overhead, region commitment and per-heap errors, using bounded
enumeration and one heap lock at a time. Partial/failed walks are not total memory accounting.
`MultilineGridHeapPaced` additionally targets 50 retention frames/second to distinguish
frame-count effects from wall-clock cleanup. These opt-in diagnostics never pace production;
their retention timing is not performance acceptance. The five FPS rounds remain unpaced.
[Matched heap attribution and rejected experiments](../Measurements/GridTextOverflow/2026-09-21/heap-attribution/README.md)
retain the observations, limits and unresolved resource gate.
Every `test.ps1` invocation runs the complex-UI benchmark, including filtered suite runs, and embeds its scenarios
and report path in every suite receipt. Without `-PerformanceBaseline`, the result is explicitly **unpaired**.
This records throughput but cannot claim absence of regression. CI artifacts retain those measurements; a developer
must supply a matched baseline comparison before accepting an implementation change.

The fixture is 1280×720 at 96 DPI, reduced motion, 83 controls: a root, 16 cards each containing a label, toggle,
slider and progress bar, plus a Tree and four-column Grid backed by 1,000 rows. After 20 warm-up frames it runs
five rounds of 40 frames for each scenario. Clean frames reuse the prepared surface; dirty frames update 32 values,
scroll the grid and repaint. A screenshot is recorded outside timing at `.build/test-artifacts/complex-ui.png`.

For changes to multiline grid rendering, also run `performance.ps1 -Scenario MultilineGrid` on
both implementations. Its distinct fixture, `dxui-complex-ui-multiline-grid-v1`, uses long French
sentences, explicit paragraphs, combining accents and an emoji, 64-DIP rows and a two-line clamp.
The grid advances one row per dirty frame to exercise reuse across viewport changes. Other controls,
round counts, completion readback, resource gates and measurement methods remain the same.
All four columns of a row share one text, so a frame exercises only about six distinct layout-cache keys; the
fixture understates cache misses for grids with distinct per-column text.
The harness records `complex-ui-multiline-grid.png` outside timing. This optional fixture does not
replace the default benchmark; never compare reports from the two different scenarios.
`-Scenario MultilineGridDistinct` (`dxui-complex-ui-multiline-grid-distinct-v1`) keeps that geometry, clamp and
scrolling but gives every cell its own French text (four column-specific sentences per row, with accents and an
emoji), about 24 distinct layouts per frame, and keeps the Tree's short names; it records
`complex-ui-multiline-grid-distinct.png`. Run it with `MultilineGrid` for any change to grid text layout or its
cache: the repeated-text fixture cannot show conflict misses, and this one cannot show sharing between identical
values. In `MultilineGrid`, drawing the Tree's long names with their color emoji on WARP takes most of a dirty frame
and the multiline grid almost none of its time
([frame-cost investigation](../Measurements/ReviewFollowUps/2026-09-29/frame-cost/README.md)), so a grid text-layout
change shows there in allocations and memory long before it shows in FPS; the distinct scene's short Tree names
leave its frame to the grid.

Receipts record completed offscreen WARP FPS, p50/p95 total frame milliseconds, p95 preparation and CPU composition
times, C++ allocation counts with the gated dirty per-frame ceiling (`dirtyAllocationCeilingPerFrame`: 64 in
Release, 320 in Debug where the debug STL allocates container proxies; clean rounds must allocate nothing), exact
surface payload and replacement peak, process private bytes and working set
with sampled peaks and private-byte growth. Source commit/content fingerprint, executable/fixture hashes, compiler,
machine, CPU, OS, WARP binary version, active power policy, native architecture and configuration make the comparison
auditable. The fixture fingerprint covers the minimal benchmark entry (`BenchmarkMain.h`), benchmark, shared
sample scene and graphics helper; receipts declare `workloadOwner: DxUi`. Functional tests run in a separate
non-inlined function so their stack frame is not part of benchmark entry. Changes to those inputs invalidate earlier fixture comparisons, so measure a fresh baseline with the final harness on the previous implementation before comparing a candidate. `-SkipBuild` is recorded; the caller is responsible for matching existing binaries to the recorded sources.

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

The [no-capture menu scaling record](../Measurements/MenuDescriptions/2026-09-21/scaling-v1/README.md)
retains the first 320-cycle probe and its non-monotonic private-memory changes.
The current v4 diagnostic retains live/free process-heap counters; partial heap
errors must be reported, and heap totals are not total process memory or native
call-stack attribution. No per-entry cost or resource acceptance is inferred.

Use `./test.ps1 -Configuration Release -Suites MenuResourceScaling` to investigate
the incremental cost of menu descriptions. The diagnostic keeps the window extent
constant, rotates described-row counts and samples before opening, after ordinary
paint and after closing. It performs no menu bitmap capture. Its raw JSON lines
are retained in the suite log, including DPI, entry counts and process/handle
counters. Release x64 CI runs it alongside the existing native matrix.
This suite and `MenuResources` require foreground interaction. Local runs use the
authorized warning and desktop lease with focus/cursor restoration; `--no-activate`
is rejected. V4 records all 320 native extents from a visible owned parent on one
monitor. Earlier locally failed activation-blocked runs remain invalid evidence.
The first description enables a whole-menu semantic tree; later descriptions add
text layouts. A process-memory delta divided by the number of entries is therefore
not a measurement of one entry's allocation. Treat this as attribution evidence,
not a substitute for the required matched-source performance comparison.

`./test.ps1 -Configuration Release -Suites MenuTextLayoutResources` isolates the
native text-layout path: twelve Body labels, twelve Small descriptions and their
pairs, sampled before creation, before/after metrics and after release. It uses
32 rotated cycles without popup, semantic tree or paint and does not take focus.
Compare its live-heap deltas with the whole-menu probe; private-memory changes
still include allocator retention and cannot identify an individual allocation.

The [popup-local sharing experiment](../Measurements/MenuDescriptions/2026-09-23/popup-text-sharing/README.md)
compares separate, combined and shared native layouts. Sharing equal text/font/width
reduces duplicate shaping storage; unique captions do not have the same saving.
Its text-only evidence is separate from whole-menu and common-scene acceptance.
The same packet retains a matched whole-menu comparison: twelve repeated-caption
rows use 575,310 versus 367,894 live heap bytes. The
[six-profile native qualification](../Measurements/MenuDescriptions/2026-09-23/native-ci-sharing/README.md)
passes functionally with explicit ARM64 desktop skips. Its common-scene timing flags compare two
described-menu builds, not the feature against unchanged main; the matched pairs against main never
flag clean frame p95. Their clean private-memory increase, once accepted under a waiver, does not
reproduce in a [six-run local paired set](../Measurements/MenuDescriptions/2026-09-30/paired-local/README.md)
(+0.36%, p = 0.70), so the waiver is removed; consumer adoption remains separate.

`DxUi.EmbeddedTests.exe --benchmark-grid-selection <report.json> [parts]` is an opt-in measurement of Grid selection
membership, run in Release. It uses synthetic data and only the public `Grid` and `GridSelectionModel` interfaces, so
one source measures any revision; `parts` names some of paint, selectionCost, membership, retention, mutators, preserve and
complexUiScene. It paints Grids of 1,000 to 1,000,000 rows, selected as Ctrl+A selects them, offscreen
on WARP (the time and the UI thread's cycles of Prepare, the time of the whole frame, and the C++ allocations and
bytes of each round), isolates what `IsSelected` costs inside such a paint (one Grid painted alternately with a small and
a full selection that draw alike), times `IsSelected` per call over selections of 0 to 1,000,000 ids with the questions in
an order a processor learns and in a random one it cannot, counts the C++ heap bytes a selection model holds after Ctrl+A
and after each way back from it, times the selection model's mutators with their C++ heap bytes and `PreserveOrdered` over
a long list, and reports how many rows the default complex-UI scene's Grid
holds selected (none). Its entry is dispatched outside `BenchmarkMain.h`, so the complex-UI fixture's hashed inputs do not change.
Compare builds of the one harness as an interleaved A, B, B, A set (A, B, C, C, B, A for three). The
[record of the sorted selection copy](../Measurements/GridSelection/2026-10-01/README.md) does so, ten runs per side
twice and then for three builds: `IsSelected` costs 9 to 16 ns from 1,500 to 1,000,000 selected ids when a processor can
predict the questions (48 to 149 ns when it cannot), where a scan cost up to 94 us, and a paint no
longer grows with its selection (27% to 30% less Prepare time at 200,000 selected rows and 70% to 73% less at
1,000,000; the 20,000-row difference of a few percent is below what the runs separate). The record also shows what
it costs: 8 bytes per selected row, and a sort in `SetRange` over ids that do not already ascend. Its second set shows that
a binary search is dearer than a scan for 2 to 512 selected ids when the questions cannot be predicted (the ids of the
rows on screen against hashed ids), by 5 to 20 ns a call at 8 to 128 ids, which is why the model scans up to 1,024 ids and
searches above; that a
model gives back the room of a large selection (3.2 MB after Ctrl+A over 200,000 ids and `Clear`, 0 now); and that
`PreserveOrdered` over a long list with a modest selection needs a table of bits to stay under a hash set's cost.

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

The [grid candidate's native receipts](../Measurements/GridTextOverflow/2026-09-21/native-ci/README.md)
cover all six x64/ARM64 profiles, preserve an intermittent accessibility setup failure
and identify ARM64 Menu desktop-capability skips. These functional receipts do not
close the candidate's separately measured resource regression. A
[hosted paired benchmark](../Measurements/GridTextOverflow/2026-09-27/paired-hosted/README.md) of the
merged grid stays within the accepted V11 memory envelope.

## Other checks and formatting

Run `validate.ps1` and `format.ps1 -Check`. `validate.ps1` is the one validation entry point: it runs
`validate-skills.ps1`, `validate-specs.ps1`, `validate-dependencies.ps1`, `validate-test-port.ps1`,
`validate-build-matrix.ps1` and `Tools/tests/Invoke-ToolingTests.ps1`, each in its own process, reports every failure
before it fails, and is what CI's validation job runs (each validator also runs alone, and `test.ps1` runs the tooling
tests beside its native suites). No validator scan enters a nested git checkout, such as an agent's worktree under
`.claude/worktrees`, so a half-edited copy cannot fail this tree. The comparator behind
`performance.ps1` is `Tools/Compare-Performance.ps1`; its tests reproduce every stored paired comparison under
Measurements exactly. Run x64 Debug/Release suites and build ARM64 Debug/Release for code
changes; native ARM64 CI must also pass. To iterate on one control test instead of its whole suite, run
`./test.ps1 -Configuration Debug -Platform x64 -SkipBuild -Suites Menu -Tests TestDescribedMenuReleasesItsMemoryWhenItCloses`
(or `DxUi.ControlTests.exe --suite=Menu --test=<Name>[,<Name>]`): only the named tests run, an unknown name fails the run,
and the receipt is a separate `*.filtered.json` file. It supports development; a change is validated by the whole suites.
A test that never returns must not hold a CI job: the runner gives every control test (and every fixture suite without named
tests) a deadline, 300 s by default, and a test that outlives it ends the run with `TIMEOUT: <TestName> after <N> s` and exit
code 124. `DxUi.ControlTests.exe --test-timeout=<seconds>` (`test.ps1 -TestTimeout <seconds>`) changes it and 0 turns it off,
which debugging a test needs; every run prints its deadline on a `[WATCHDOG]` line and every `[DONE]` marker carries the
test's duration. `test.ps1` prints a failing suite's exit code, its `TIMEOUT:` line and the last lines of its log.
Use `gallery.ps1 -PublishDocs` after visual/control changes and review all
generated sheets. CI's x64 Release job runs the same command and uploads its `docs/gallery` output as
`docs-gallery-x64-Release` for review. To publish it after a merge, run the manual
[Publish docs gallery workflow](../.github/workflows/gallery.yml) on the branch with its `publish_docs` input enabled:
it regenerates the gallery on a native x64 Release build and commits it with an ordinary push, never forced, only when
a sheet, the index or the README changed, so nothing is copied by hand. Review the sheets in that commit's diff. Full
IME, touch and screen-reader adoption checks remain explicit manual gates.

[Formatting CI](../.github/workflows/format.yml) checks pushes/PRs and uploads a ready-to-apply patch. To reformat a
branch remotely, run its manual workflow with `apply_changes` enabled. It commits formatting on the selected
branch without a force push; branch protection still applies. GitHub-token commits do not trigger another push
workflow, so validation must run again explicitly or on the next user push. The gallery workflow works the same way.
