# Selection blocks and embedded input lifetime

Status: DONE — SDK implementation and scoped native qualification.
Implementation: `f765aeca5e059dbe768841b605b1be6a5f6ea942`, API 4.

- [x] Add `WrapSelectionInMarkdownBlock` for a nonempty selection, with exact-source/EOL preservation,
  growing code fences, retained direction, one transaction and atomic refusals.
- [x] Defer embedded preparation notifications until the outer mutating input entry point exits, including
  nested calls and detach/reattach, without delaying another view's direct setter.
- [x] Guard direct TextField, ComboBox and FormField continuations against callback-driven retirement.
- [x] Retain original Release and three ASan counterexamples plus intermediate build/fixture failures.
- [x] Complete six zero-warning builds, all 19 noninteractive suites in each x64 profile and real sanitizer
  detection; member-verify native programs/symbols and the original working compiled-input archive.
- [x] Merge durable behavior into [controls](../../UI/UI_ControlsAndLayout.md),
  [input](../../UI/UI_InputAndAccessibility.md), [embedded hosting](../../Rendering/Rendering_EmbeddedD3D11.md),
  [resource rules](../../Core/Core_PerformanceAndResources.md) and public rich-text/hosting usage docs.

The [qualification packet](../../../Measurements/SelectionBlocksAndInputLifetime_2026-11-05/README.md)
binds the exact source and native artifacts. This closes D-L1 and the SDK portion of selected-block
insertion. RedTilde's actual Insert commands still require its own exact-pin build/tests. Native ARM64,
physical input, paired performance and unrelated remaining work retain their existing owners in
[the active plan](../WIP/RemainingWork_2026-11-04.md).
