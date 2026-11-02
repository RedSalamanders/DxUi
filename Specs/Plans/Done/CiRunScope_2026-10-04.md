# CI run scope

- **Status**: DONE as historical implementation, with the former hosted-observation requirement superseded on
  1 November 2026. The current CI contract is [Build_UnifiedCI](../../Build/Build_UnifiedCI.md). This closeout
  does not claim the old hosted observations passed or qualify the unified pipeline.
- **Owner**: Repository tooling and CI.
- **Scope**: Run the validation workflow once per change, and leave the native jobs out of a documentation-only pull
  request, without weakening the push runs of main, which consumers adopt. The native matrix itself (configurations,
  suites, consumer builds, gallery) and the paired benchmark are unchanged.

## Why

A review of the last 200 workflow runs (30 September to 4 October) found:

- `ci.yml` ran on every push to every branch and on every pull request event. A push to a pull request's branch ran the
  six native jobs twice, once for the branch and once for the merge ref, and the two runs are different concurrency
  groups, so neither cancelled the other: 43 of 73 commits were validated twice, and 45 of 71 push runs were on branches
  other than main.
- One run of the native jobs costs about 130 runner minutes (x64 Debug 13, Release 11 and ASan Debug 18; ARM64 Debug
  25, Release 28 and ASan Debug 34, a third to a half of it in the consumer builds) and about 34 minutes of wall time,
  whatever changed. A pull request that changed only plans, changelog fragments, measurements or the gallery paid all
  of it.
- `format.yml` had the same duplication (49 push and 29 pull request runs).

## Checklist

- [x] Measure the runs, the job and step times and the commits validated twice.
- [x] Pushes validate main alone (`ci.yml`, `format.yml`); pull requests and manual runs as before.
- [x] `Tools/NativeScope.psm1` and `Tools/Get-NativeScope.ps1`: a documentation-only pull request skips the native
  jobs; a push to main and a manual run never do; any other path, a path the rules do not name or a failed scope runs
  them.
- [x] Tooling tests (`Tools/tests/Test-NativeScope.ps1`), the testing contract, `Tools/README.md` and `CONTRIBUTING.md`.
- [x] Retire the former hosted-observation requirement as superseded: PRs now use compiler-free checks;
  main native qualification follows the quiet-period and two-architecture Release gate in the current contract.
  The old code-PR native matrix and documentation-only skip are not current acceptance criteria.
- [x] Archive this implementation record; current hosted acceptance remains with the unified CI rollout.

## Validation

`validate.ps1` (the validators and the tooling tests, `Test-NativeScope.ps1` and the unchanged workflow checks of
`Test-BenchmarkGate.ps1` included) and `format.ps1 -Check` pass. The original retrospective claimed five of 40 PRs would skip native jobs. The later
[production review](ProductionReview_2026-10-05.md) found that its inclusion of #34 did not reproduce;
treat that count as disputed historical analysis, not verified evidence.

## Not in this plan

The ARM64 jobs take the longest (25 to 34 minutes), a third to a half of it in their consumer builds, and the ASan jobs
build the consumer twice. Running those builds as jobs of their own would shorten the wall time at more runner minutes,
a trade the developer decides. Making a native job a required check would first need an aggregate job, since a
documentation-only pull request reports them skipped.

## Current evidence boundary (1 November 2026)

[CI run 41301128985](https://github.com/RedSalamanders/DxUi/actions/runs/41301128985) at `4423074dee2410acb28e62b41072e6c561a79eb8` and [run 41228969822](https://github.com/RedSalamanders/DxUi/actions/runs/41228969822) at `5f31f2b3fcee2528c5a32c83285ee1d656019a26` completed successfully with only the coordinator job executed. Neither is a native qualification result. Current rollout qualification and platform, consumer and performance obligations remain separate.

## Current-source disposition — 1 November 2026

This is a historical closeout: the former PR-native-matrix observation was superseded by Build_UnifiedCI. It does not establish the current hosted workflow or current candidate qualification.
