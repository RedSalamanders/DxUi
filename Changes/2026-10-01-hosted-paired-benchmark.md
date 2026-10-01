- The paired benchmark runs on GitHub for every pull request to `main` that changes something it measures, so a merge waits
  on a hosted runner and not on a quiet developer machine, where the same-binary controls drifted and the set proved
  nothing. The job's first step matches the pull request's changed paths against `Get-BenchmarkScopeRules`: the library
  inputs and harness every receipt hashes (`src`, `include`, `Build`, the build props and vcpkg manifests,
  `performance.ps1` and its comparator and benchmark inputs), the sources of the benchmark executable
  (`Tests/Embedded`, `Tests/Support`) and the fixtures and samples compiled into it, the build and restore scripts, the
  paired measurement and gate tooling and `ci.yml`; Markdown never counts. The steps after it run only when one matched,
  so a pull request that changes none of them ends the job within a couple of minutes, passed, and a step that cannot
  decide fails the job instead of passing a pull request unmeasured. The job always starts rather than being skipped,
  because GitHub reports a skipped job under its unevaluated name expression, which a required check of that name would
  never see.
  - The `paired-benchmark` job compares the pull request's merge ref with that merge commit's first parent, the base as
    the merge ref was made, so the two differ by exactly this pull request (the merge base with the branch would also hold
    what `main` gained since the branch was cut), in the gating scenarios `Default`, `MultilineGrid` and
    `MultilineGridDistinct`, three repetitions each: six runs per side, whose smallest attainable p is 0.0022. A run
    takes 10 to 15 minutes (a pull request's job is bounded at 45, a manual run's at the 90 it always had), a newer push
    to the pull request cancels the older run, the token stays `contents: read`, and the pinned actions are the ones the
    other jobs use. A pull request's check is named `paired-benchmark (pull request)`, apart from the push and manual
    runs of the same job, so that a required check names exactly one check run.
  - The job summary lists each scenario's set verdict and every metric's medians, change, p-value, band, same-binary
    controls and outcome, flagged metrics first; `paired-benchmark-x64-Release` keeps every receipt, comparison,
    `summary.json` and the conclusion as `verdict.json`. The check fails for a confirmed degradation, a regressed metric
    whose own same-binary controls stayed within its band (an exact budget: stayed equal), and for an inconclusive run, a
    regressed metric whose controls drifted beyond it. GitHub has no neutral job conclusion and a green check would read
    as a pass, so both fail; an inconclusive run is re-run, and a flagged metric is listed whatever the conclusion.
    Nothing is rebaselined: the gate measures the base afresh. A run with no regressed metric passes as no regression
    established, and its summary counts the metrics whose controls could not hold their band.
  - The controls of the flagged metric decide, not those of all twenty-six, because a hosted runner drifts beyond some
    band in nearly every control: in the retained A/A set (`Measurements/HostedPairedGate/2026-10-01`, the same library
    code built twice) all 18 controls drifted, in 1 to 13 of 26 metrics, mostly p95 timings (frame, preparation and
    composition), while the eight exact budgets never drifted and nothing was flagged in 78 metric tests. A second
    hosted A/A run (`aa-2` beside it) flagged five clean-phase timings (p 0.004 to 0.026) with their controls drifted,
    which is inconclusive per metric and so can need a re-run. The strict reading, any unstable control making its
    scenario inconclusive, is `-StrictControls` of `Tools/Publish-BenchmarkVerdict.ps1` and was off because it would have
    made every hosted run inconclusive. When both sides have one library fingerprint (a change to the benchmark or its
    tooling only), a timing or memory flag is listed as noise and a rise in a deterministic budget still fails. A set too
    small to reach p < 0.05 cannot pass.
  - A manual dispatch with `benchmark_baseline` keeps its inputs, its default scenarios, its artifact and its three
    repetitions, publishes the same summary and stays green for a finding. Its concurrency group is now its own
    (`github.run_id`), so a push to the dispatched ref, or another dispatch of it, no longer cancels a measurement
    someone asked for (a merge to `main` would have cancelled one dispatched on `main`).
  - The decisions live in `Tools/BenchmarkGate.psm1` with the two step scripts `Tools/Get-BenchmarkScope.ps1` and
    `Tools/Publish-BenchmarkVerdict.ps1` (which also judges a local run: `-Reports <reports directory>`), and
    `Tools/tests/Test-BenchmarkGate.ps1` covers them: the scope rules against the receipt's inputs, the include closure
    of the benchmark executable and every module the scripts import; the pair on fixture merge commits; the conclusion
    on synthetic summaries, on the retained local and hosted sets and on every summary this repository retains, whatever
    its age; the summary, annotations and exit codes; and the workflow's wiring. `Get-LibraryInputPaths` is the
    comparator's one additive export. The performance contract, `docs/performance.md`, `Tools/README.md`,
    `CONTRIBUTING.md`, the testing contract and the performance skill describe it. Nothing visual changed, so the gallery
    is unchanged; the library, its API and every receipt format are unchanged.
