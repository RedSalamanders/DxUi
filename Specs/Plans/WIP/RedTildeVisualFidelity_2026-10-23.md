# Shared presentation for RedTilde visual fidelity

Status: implementation in progress. The owner requested that RedTilde match its image-2 proposal and that reusable control improvements live in DxUi.

- [ ] Add opt-in Filled/Subtle/Outline/Tab presentation independently of button invocation and TogglePattern.
- [ ] Add leading icons and keyboard hints to text buttons with intrinsic measurement and RTL support.
- [ ] Add transparent/borderless Tree presentation and optional expander reservation, preserving navigation/model/UIA semantics.
- [ ] Allow a borderless document editor to omit decorative focus chrome; retain the caret and high-contrast indicator.
- [ ] Add behavioral/visual regressions, update usage/design guidance and regenerate the five-theme gallery.
- [ ] Run affected validation, x64 native suites and six builds; retain paired baseline/candidate performance records and resource limits.
- [ ] Commit the exact SDK revision for isolated consumer restore and verify actual RedTilde captures in its own repository.

Application layout, paths, report fixtures, theme policy and caption actions stay in RedTilde. Existing performance/policy, physical input/AT, ARM64 runtime and release qualification gates are not closed by this visual change. A screenshot cannot establish those claims.
