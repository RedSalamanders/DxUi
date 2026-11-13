- UI Automation conformance (performance and motion plan W1.3): a window host answers `WM_GETOBJECT` with a
  zero-extended `UiaRootObjectId`; a `Button` with a toggle state exposes Toggle and no longer Invoke; composition
  changes raise `TextEditChangeType_Composition` with the changed part of the composition string and a commit raises
  one `TextEditChangeType_CompositionFinalized` with the committed text instead of an empty `Composition`; `Detach`
  after the window was destroyed without `WM_NCDESTROY` reaching the host releases the accessibility target and
  disconnects its root provider; the empty snapshot published on detach or root swap is made once at registration,
  so retirement allocates nothing. Behaviour change: an unnamed `TextField` or `ComboBox` now has an empty UIA
  `Name` (its content stays in `Value` and `Text`); give it a label or an accessible name. Covered by new cases in
  the Accessibility and Embedded suites; physical screen-reader and IME acceptance remain separate.
