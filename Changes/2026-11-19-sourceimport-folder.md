- Moved the RedSalamander source-import records from the stray `Specs/Done/` folder to `Specs/SourceImport/`, where
  RedMonitor keeps its own (convergence CV-6-03, B-05). The records are unchanged; `Tools/Validation.psm1`, the scope
  planner's compiled-only exclusion, four tooling tests and every guide and spec that cites them follow. Validated with
  `validate-specs.ps1`, `validate-dependencies.ps1`, `validate-test-port.ps1`, `Test-Validation.ps1`,
  `Test-TestPort.ps1`, `Test-NativeScope.ps1`, `Test-ScopedTesting.ps1` and `Tools/Test-Changelog.ps1`.
