# Independent DxUi measurements

This directory retains reviewed evidence from DxUi's own samples and benchmarks. Scenarios may be inspired by real
interfaces, but use synthetic data and require no RedXe or RedSalamander checkout, plugin, settings or services.
Application adoption measurements belong in their application's repository.

- [Complex UI, 2026-09-05](ComplexUi/2026-09-05/README.md): Release baseline and matched repeat, complete raw rounds,
  comparison and rendered scene.

The runnable [complex sample](../Samples/ComplexUi/README.md) and benchmark share one scene. Its `dxui-complex-ui-v2`
identity distinguishes it from older fixtures; changed fixtures cannot establish before/after implementation results.
Each reviewed run must retain raw rounds, environment/source/fixture hashes, comparisons and a README explaining
limitations and noisy results. Temporary output remains in `.build`; docs link here instead of containing archives.

Run and compare using [the performance guide](../docs/performance.md). Do not replace an earlier baseline to hide
a regression or claim application-level acceptance from these library measurements.

- [Primary high contrast, 2026-09-05](https://github.com/RedSalamanders/DxUi/blob/1947a5b91beb029e9b99d71e0893c6075bbb29ca/docs/measurements/primary-high-contrast-2026-09-05/README.md): retained v1 fixture evidence and native validation for the focused color-pair correction. This is historical v1 evidence, not a comparison against the new v2 scene.

- [Embedded text state, 2026-09-05](TextInput/2026-09-05/README.md): matched final-driver measurements, earlier investigation runs and complete x64 suite receipts.

- [Native text-store investigation](TextInput/NativeStore-2026-09-05/README.md): callback safety; performance acceptance open.
