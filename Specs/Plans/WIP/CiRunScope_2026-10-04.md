# CI run scope

- **Status**: ACTIVE (4 October 2026). The rules, the scope step and the workflow wiring are implemented and pass the
  tooling tests; the hosted behavior remains to be observed on this change's pull request and the next
  documentation-only one.
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
- [ ] Observe on hosted CI: this change's pull request runs the native jobs (it changes `ci.yml` and tools), a
  documentation-only pull request skips them with the scope summary, and its merge to main runs all six.
- [ ] Move this plan to Done.

## Validation

`validate.ps1` (the validators and the tooling tests, `Test-NativeScope.ps1` and the unchanged workflow checks of
`Test-BenchmarkGate.ps1` included) and `format.ps1 -Check` pass. Applied to the changed files of the last 40 pull
requests, the rules skip the native jobs for 5 (#26, #34, #49, #52 and #58) and run them for the other 35.

## Not in this plan

The ARM64 jobs take the longest (25 to 34 minutes), a third to a half of it in their consumer builds, and the ASan jobs
build the consumer twice. Running those builds as jobs of their own would shorten the wall time at more runner minutes,
a trade the developer decides. Making a native job a required check would first need an aggregate job, since a
documentation-only pull request reports them skipped.
