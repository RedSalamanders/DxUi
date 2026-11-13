- Plan evidence anchors are now checked (`W0.11`): `Tools/tests/Test-PlanAnchors.ps1`, run by
  `Tools/tests/Invoke-ToolingTests.ps1` and so by `validate.ps1` and `test.ps1`, resolves every source anchor in
  `Specs/Plans/WIP/*.md` (`path:line`, ranges, comma lists, a file link followed by `:lines`, and `:lines`
  continuations) against the checked-out tree: the file exists, every line lies within it, and a symbol written
  directly before an anchor occurs in its file, or the anchor is reported as drifted with file, line and symbol.
  Anchors in ticked items are skipped. The rule is stated in `Specs/Testing/Testing_Validation.md` and pointed to from
  the spec-workflow skill; seeded fixtures fail for a missing file, an out-of-range line, a drifted symbol and an
  unresolved continuation. The performance and motion plan's 556 anchors now resolve: 49 anchors on 43 lines were
  repaired to name the file they meant (bare `DxUi.h` is `include/DxUi/DxUi.h`; `AnimationDispatcher.h`, `FrameRuntime.cpp` and `tokens.json` gain
  their directories; continuations that followed the
  wrong file now name `DxUi.Controls.cpp`, `DxUi.ComboBox.cpp`, `DxUi.RichSemanticGeometry.cpp`,
  `DxUi.RichImage.cpp`, `DxUi.Accessibility.cpp` or `DxUi.WindowHost.cpp`); no plan wording changed. Tooling only.
