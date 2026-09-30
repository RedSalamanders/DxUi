# PowerShell tooling: one scripting language for every repository tool

Status: COMPLETE (2026-09-30). Tooling only; no library, consumer or public API change.
Base: the review work, merged to `main` as `5561b61`.
Owning contracts: [testing and validation](../../Testing/Testing_Validation.md),
[build and consumption](../../Build/Build_ToolchainAndConsumption.md) and
[performance](../../Core/Core_PerformanceAndResources.md).

The developer decided on 2026-09-30 that repository tooling is PowerShell only. Python entered with the bootstrap and
nothing in the tools needs it, while it costs an interpreter, pip downloads (PyYAML, the clang-format wheel), five CI
setup steps, and every `test.ps1` run depends on it through the performance comparator.

## Execution

- [x] Performance comparator in PowerShell (`Tools/PerformanceComparison.psm1`, `Tools/Compare-Performance.ps1`): the
  same statuses, and output byte-identical to the Python comparator's, checked against all 52 stored paired
  comparisons under `Measurements`; every stored unpaired receipt still validates (the 16 long-fixture receipts of
  2026-09-13 fail the standard five-rounds-of-forty rule, as they did with the Python comparator).
- [x] Validators in PowerShell (`Tools/Validation.psm1`): specs, skills, dependencies, inherited tests and the build
  matrix report exactly what the Python validators report on this repository. Python's semantics are kept where they
  decide results: universal newlines, UTF-8 with or without a byte-order mark, hidden entries, ordinal sorting and
  case-sensitive comparison. Skill front matter is read as a strict `key: value` subset instead of with PyYAML.
- [x] Tooling tests as plain `Test-*.ps1` scripts with a small shared module (no Pester): every Python test case, plus
  the front-matter subset and the stored-evidence parity above; `Tools/tests/Invoke-ToolingTests.ps1` runs them.
- [x] Callers: root `validate-*.ps1` scripts (adding test-port and build-matrix entry points), `performance.ps1`, and
  `performance-paired.ps1`, whose harness also copies the comparator into historical worktrees.
- [x] Formatter without pip: `Tools/Install-ClangFormat.ps1` fetches the pinned clang-format 22.1.3 wheel, checks its
  SHA-256 and extracts only the executable; `format.ps1` finds that install.
- [x] CI without Python: the validation job runs the validators and tooling tests under `pwsh`; the native and paired
  jobs drop `setup-python`; the formatting workflow uses the installer.
- [x] Remove `Tools/*.py`, `Tools/tests/*.py`, `Invoke-Python.ps1` and both requirements files. The historical
  `Measurements/**/*.py` analysis scripts stay: their READMEs cite them as how that evidence was produced.
- [x] Docs and contracts: README, getting started, CONTRIBUTING, Tools/README, the testing and build contracts, the
  performance guide, third-party notices and CHANGELOG.
- [x] Local validation: every validator and all 52 tooling tests pass; `test.ps1` writes an unpaired comparison, and
  with a same-binary baseline the laptop's noise is flagged `advice-required` and fails the run, as before; a paired run
  against the pre-port `d6eab14` measured all four runs, the baseline worktree comparing with the copied PowerShell
  comparator; `format.ps1 -Check` passes; the installer rejects a wheel that does not match its pin.
- [x] CI on the pull request (PR 30): the validation job passes under `pwsh` on Ubuntu (validators and tooling
  tests), and the formatting workflow's check job installed the pinned clang-format 22.1.3 with
  `Tools/Install-ClangFormat.ps1` (fetched by its SHA-256, verified, extracted) and passed.
