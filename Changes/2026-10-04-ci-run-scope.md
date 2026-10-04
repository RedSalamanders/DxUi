- The validation workflow runs once per change. A push to a branch other than main starts nothing, since its pull
  request validates the same code on its merge ref: in the four days to 4 October, 43 of 73 commits ran every job twice,
  and 45 of 71 push runs were on such branches. A pull request whose every changed path is documentation (Markdown,
  `Specs`, `Changes`, `Measurements`, `docs`, `.agents`, `LICENSE` and the formatting and gallery workflows) skips the
  six native jobs, about 130 runner minutes; 5 of the last 40 pull requests were such. Pushes to main and manual runs
  still run every native job, because consumers adopt the main commit whose latest push run succeeded, and so does a pull
  request whose scope cannot be read. The formatting workflow checks pushes to main alone too.
  - **Tools.** The new `native-scope` job runs `Tools/Get-NativeScope.ps1` (rules in `Tools/NativeScope.psm1`) on the
    pull request's commits and paths from `BenchmarkGate.psm1`. `Tools/tests/Test-NativeScope.ps1` covers the rules,
    every tracked file, the step on fixture merge commits and the workflows' wiring.
  - Specified in `Testing_Validation.md`, with `Tools/README.md` and `CONTRIBUTING.md`. No native job, suite, consumer
    build or benchmark changed what it runs.
