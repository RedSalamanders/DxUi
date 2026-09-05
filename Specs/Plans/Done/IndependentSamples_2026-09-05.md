# Independent samples and measurement ownership

Status: COMPLETE (2026-09-05)

Owning contracts: [architecture](../../Core/Core_Architecture.md),
[performance](../../Core/Core_PerformanceAndResources.md), [documentation](../../Core/Core_Documentation.md).

- [x] Move the AV evidence to RedXe; preserve all eleven JSON receipts and add a historical-scope explanation.
- [x] Use an independent minimal playground and share one synthetic complex scene between the runnable sample and benchmark.
- [x] Record dedicated DxUi measurements with full fixture identity; do not compare changed fixtures as implementation regressions.
- [x] Keep application budgets and adoption evidence in consumer repositories; document standalone sample/measurement commands.
- [x] Validate all x64 suites, ARM64 cross-builds, sample output, gallery, tooling and docs; record native execution limits.

Public API and library algorithms are unchanged; the user's canonical-name edits also update comments and diagnostic
labels to DxUi. The scene and fixture identity change, so new measurements are a new baseline plus same-code
repeatability, not a claimed speedup or proof of before/after library performance.

Local x64 Debug/Release runs each pass all 18 suites with nine explicit interactive Menu capability skips. Both ARM64
configurations cross-build; native ARM64 has not been rerun for this working tree. The independent Release pair is
retained under Measurements/ComplexUi/2026-09-05, with a within-noise-budget comparison and zero composition allocations.
All six gallery sheets and the complex scene were reviewed, all 34 tooling tests pass, and both repositories' skill
checks pass. The relocated consumer proof passed from clean disposable snapshot
`663e91c63aef8e2fec45c6bc5347a978fc42ada2`: both sample modes build/render and all five invalid pins are rejected.
The deeply nested initial fixture exceeded vcpkg's Windows path limits; the final run used the documented shorter
`-FixtureRoot` option. No global Git setting changed. Source/build/sample validation is complete; no new native CI
run or hardware presentation claim is made. Shared working changes remain uncommitted.
