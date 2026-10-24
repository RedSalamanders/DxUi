# Shared presentation for RedTilde visual fidelity

Status: scoped implementation, native tests and gallery complete. The owner requested that RedTilde match its image-2 proposal and that reusable control improvements live in DxUi. Shared performance acceptance remains with the active library qualification plan; this completion does not accept a new cost or qualify release.

- [x] Add opt-in Filled/Subtle/Outline/Tab presentation independently of button invocation and TogglePattern.
- [x] Add leading icons and keyboard hints to text buttons with intrinsic measurement and RTL support.
- [x] Add transparent/borderless Tree presentation and optional expander reservation, preserving navigation/model/UIA semantics.
- [x] Allow a borderless document editor to omit decorative focus chrome; retain the caret and high-contrast indicator.
- [x] Add behavioral/visual regressions, update usage/design guidance and regenerate the five-theme gallery.
- [x] Run affected validation, x64 native suites and six builds; retain baseline/candidate diagnostics and exact resource observations.
- [x] Commit the exact SDK revision for isolated consumer restore and verify actual RedTilde captures in its own repository.

The originally planned paired performance acceptance is not completed by these unpaired diagnostics. It remains
under [library qualification](../WIP/ConsumerUiQualification_2026-10-11.md). Both comparisons are advice-required:
the first dirty private-memory peak flag (26,439,680 to 26,992,640 bytes) did not repeat; the repeat flagged timing
differences instead. Preserve both records, the original baseline and the existing thresholds. No cause, accepted
degradation or trusted non-regression verdict is inferred.

Code revision `903643b4a2bc2f6d75250ff5e6834513c96432c4` passes six builds and 57 noninteractive x64 suite
executions with zero capability skips, plus real ASan detection. RedTilde's exact-pin adoption passes six builds,
53 behavioral cases and 23 captures in each x64 profile. Its image-2 report is reviewed at 1440x900 pixels / 96 DPI,
with additional 144/192-DPI renders and truthful fixture totals. The [SDK packet](../../../Measurements/VisualFidelity_2026-10-23/README.md)
retains this scope and the separate performance hold.

Application layout, paths, report fixtures, theme policy and caption actions stay in RedTilde. Existing performance/policy, physical input/AT, ARM64 runtime and release qualification gates are not closed by this visual change. A screenshot cannot establish those claims.

First matched app capture exposed a second Tree paint-layout calculation that bypassed the new presentation;
both paths now share one canonical helper. The continuation also adds neutral tree decoration/state dots,
subtle splitter seams and optional per-heading margins/container indentation. Real source blank-line caret sites
remain full native lines. The first focus-chrome regression fixture lacked keyboard modality; its failing record
is retained and the fixture now sends owned-host logical Shift messages without OS input or native focus transfer.
Three simultaneous filtered attempts refused the runner's artifact lock; final suites run serially.
