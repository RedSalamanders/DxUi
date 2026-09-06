# Bootstrap and RedXe-first adoption

Status: COMPLETE (2026-09-06) — library extraction, first-consumer pin, synthetic adapters

The user approved the independent private DxUi project. This plan owned library extraction and the first RedXe pin.
Owning library contracts remain under `Specs/` in this repository. RedXe consumer behavior is
`Specs/Core/Core_DxUiIntegration.md` in the RedXe checkout (sibling path
`Z:/src/RedXe/Specs/Core/Core_DxUiIntegration.md`). AV release gates stay on RedXe
`Specs/Plans/WIP/RFC_Plugins_AVControl.md`. RedSalamander remains [HOLD](../WIP/RedSalamanderMigration.md).

- [x] Create private `RedSalamanders/DxUi` and sibling checkout.
- [x] Bootstrap AGENTS, nine skills, normative contracts, validators, build/test scripts, source provenance and CI.
- [x] Extract the first independent Foundation static target from the existing frame runtime.
- [x] Build/test Foundation on all four native configurations, locally for x64 and in GitHub for ARM64; publish on main.
- [x] Complete hosted bootstrap validation after the import-manifest correction.
- [x] Make DxUi the canonical source home; move controls/tests, remove duplicate runtime and application projects.
- [x] Validate owned-source layout, pending dependency inventory, formatting and local Foundation regressions.
- [x] D1: decouple retained controls and tests; public ControlHost replaces application-specific host naming.
- [x] D1: one DxUi.lib containing controls and both hosting modes; native gallery fixture retained.
- [x] D2: embedded host, coherent preparation/composition, dirty surfaces and resource pool.
- [x] Library text/TSF/clipboard and embedded UIA attach with synthetic tests:
  [EmbeddedTextServices](EmbeddedTextServices_2026-09-05.md).
- [x] D3: RedXe pins `3208083836a89d2c3348e4389b105cf3c2b453fc`; host and bundled plugins rebuild against that pin.
  Generic preparation/input/text/UIA adapters exist as COM/POD transport. Synthetic tests pass.
- [x] D4: AV Control uses shared DxUi controls and the reviewed responsive layout. Audio/camera backends stay in
  RedXe's AV RFC.
- [x] Close this extraction/adoption plan. Do not treat RedSalamander HOLD, real IME/AT, or matched performance as
  unfinished library bootstrap.

## Intentionally not closed here

| Gate | Owner |
| --- | --- |
| Real IME, touch, screen-reader | RedXe AV RFC |
| Matched text/UIA performance (NativeStore/HostServices open; EmbeddedUIA receipts uncommitted) | RedXe AV RFC; do not waive failures |
| Intermittent Menu flood / split-button CI on non-pin commits | Preserve failures; pin `3208083` had a full five-job pass ([33991645881](https://github.com/RedSalamanders/DxUi/actions/runs/33991645881)) |
| AV G1 audio policy and G2 camera | RedXe AV RFC |
| RedSalamander in-tree replacement | [RedSalamanderMigration](../WIP/RedSalamanderMigration.md) |

`capabilities.json` records `redxe-adapter` as supported for the pin and synthetic adapters.
`embedded-host-text-uia-bridge` stays pending for real AT/IME. `av-control` stays pending for product backends.
`redsalamander-migration` stays pending.
