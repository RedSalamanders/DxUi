# Independent DxUi measurements

This directory retains reviewed evidence from DxUi's own samples and benchmarks. Scenarios may be inspired by real
interfaces, but use synthetic data and require no RedXe or RedSalamander checkout, plugin, settings or services.
Application adoption measurements belong in their application's repository.

- [Grid native CI, 2026-09-21](GridTextOverflow/2026-09-21/native-ci/README.md): six native profiles,
  retained accessibility classification retry and explicit ARM64 Menu capability skips; recorded memory and timing
  costs are accepted. Later combined qualification and explicit pin handoff complete at 73ba below.
- [Described native menus, 2026-09-21](MenuDescriptions/2026-09-21/README.md): wrapped French
  entries, menu/UIA lifecycle coverage, stage and open/close memory attribution; recorded timing costs
  are accepted; the later exact-source packet completes library qualification and pin handoff.
- [Ordinary-menu exact-head native CI, 2026-09-27](MenuDescriptions/2026-09-27/ordinary-menu-ci-73ba/README.md):
  six passing native profiles at 73ba936; all architecture/consumer/gallery receipts and the
  ARM64 interactive-desktop skips retained. RedSalamander explicitly selects 73ba; its I26/H4 product gates remain separate.
- [Final local navigation profiles, 2026-09-27](MenuDescriptions/2026-09-27/navigation-final/README.md):
  54 passing nonactivating suite receipts, all resource observations and unchanged comparison flags;
  local pre-commit source identities remain explicit, separate from exact-head native CI.
- [Ordinary-menu accessibility, 2026-09-27](MenuDescriptions/2026-09-27/ordinary-menu-accessibility/README.md):
  accepted measured semantic-row heap cost, full ABBA/correction evidence, d45's failed native ASan
  latency gate and forced-rebuild snapshot optimization comparison; the later exact73 packet qualifies the correction.

- [Complex UI, 2026-09-05](ComplexUi/2026-09-05/README.md): Release baseline and matched repeat, complete raw rounds,
  comparison and rendered scene.
- [Complex UI, 2026-09-07](ComplexUi/2026-09-07/README.md): surface-lifetime round; Debug/Release baselines measured
  with the final harness before the implementation, candidate receipts, an environmental probe and alternating
  same-state runs. The automated comparison stayed `advice-required` on a non-quiet desktop; see its README.

The runnable [complex sample](../Samples/ComplexUi/README.md) and benchmark share one scene. Its `dxui-complex-ui-v2`
identity distinguishes it from older fixtures; changed fixtures cannot establish before/after implementation results.
Each reviewed run must retain raw rounds, environment/source/fixture hashes, comparisons and a README explaining
limitations and noisy results. Temporary output remains in `.build`; docs link here instead of containing archives.

Run and compare using [the performance guide](../docs/performance.md). Do not replace an earlier baseline to hide
a regression or claim application-level acceptance from these library measurements.

- [Native provider lifetime, 2026-09-28](NativeProviderLifetime/2026-09-28/README.md): original stale-provider/focus failures,
  54 local suite passes, three ARM64 cross-builds, matched native-menu resource samples and every common-UI flag.
  [September 30 qualification review](NativeProviderLifetime/Qualification_2026-09-30.md) adds exact49 native CI,
  the longer matched crossover, retained flags and explicit attribution limits; consumer adoption remains separate.

- [Primary high contrast, 2026-09-05](https://github.com/RedSalamanders/DxUi/blob/1947a5b91beb029e9b99d71e0893c6075bbb29ca/docs/measurements/primary-high-contrast-2026-09-05/README.md): retained v1 fixture evidence and native validation for the focused color-pair correction. This is historical v1 evidence, not a comparison against the new v2 scene.

- [Embedded text state, 2026-09-05](TextInput/2026-09-05/README.md): matched final-driver measurements, earlier investigation runs and complete x64 suite receipts.

- [Native text-store investigation](TextInput/NativeStore-2026-09-05/README.md): callback safety; performance acceptance open, owned by RedXe AV.
- [Application-side text services](TextInput/HostServices-2026-09-05/README.md): TSF/clipboard component; performance acceptance open, owned by RedXe AV.

- [Native clipboard capacity, 2026-09-09](Clipboard/2026-09-09/README.md): retained pre-fix failure, candidate Debug regression pass and raw benchmark rounds; performance acceptance in progress.

- [Shared-library I19, 2026-09-09](SharedLibrary/2026-09-09/README.md): native CI qualification and all paired resource comparisons; resource acceptance remains open.
- [Font availability refresh, 2026-09-12](SharedLibrary/2026-09-12-font-refresh/README.md): local library validation, the retained Release Menu failure/retry, gallery review and unpaired resource reports; native and consumer qualification remain open.
- [Local I19 follow-up, 2026-09-13](SharedLibrary/2026-09-13-local/README.md): all 18 Release suite exits, nine explicit Menu capability skips, and 16 longer matched benchmark receipts; paired performance acceptance remains open.
