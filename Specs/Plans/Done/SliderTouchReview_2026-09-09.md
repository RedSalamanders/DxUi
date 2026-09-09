# Slider touch and visual polish

Status: COMPLETE

Owners: `Specs/UI/UI_ControlsAndLayout.md`, `Specs/UI/UI_InputAndAccessibility.md`,
`Specs/Core/Core_PerformanceAndResources.md`, `Specs/Core/Core_Documentation.md`.

- [x] Retain pre-change Debug/Release complex-UI measurements.
- [x] Make the accent thumb visible at rest while retaining a slim track and independent 48 DIP touch band.
- [x] Fix acknowledgement of an already accepted value during animation and reject non-finite range settings.
- [x] Add meaningful slider regressions, including off-center touch grabs and cancellation.
- [x] Run all 18 suites in Debug/Release x64 and build Debug/Release ARM64.
- [x] Complete the matched Debug investigation; retain all Debug/Release A/B samples and the approved memory caveat.
- [x] Update controls/input contracts and user docs; regenerate and inspect all gallery themes.
- [x] Run formatting, skill/spec/dependency validators; move the completed plan to Done.

Paint uses the same two track capsules and two thumb discs, with no new resources, animations or allocations.
Synthetic tests do not establish physical touchscreen acceptance in a consumer.

Debug/Release runtime suites passed with 8/9 interactive-desktop Menu capability skips respectively.
Release A/B timing had no consistent slowdown; the user explicitly accepted the approximately 0.5 MiB (1.865%)
median clean private-byte increase. That narrow tradeoff is recorded in Core_PerformanceAndResources.md.
Alternating Debug original/candidate runs also showed no consistent candidate slowdown despite large desktop
variation. Their raw rounds and every comparison are retained in
[the measurement archive](../../../Measurements/ComplexUi/2026-09-09/README.md), including the larger Debug memory
variation. No benchmark threshold was changed. The Release relocated-consumer check passed, including five invalid
pin rejections. This closes the slider implementation; existing consumer hardware/IME/AT gates remain open.
