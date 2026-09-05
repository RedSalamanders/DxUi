# Single library, public controls and supplied-device consumption

Status: ACTIVE
Date: 2026-09-05

The user approved one DxUi.lib, a complete public control API, embedded rendering with a caller-supplied D3D11
device, a standalone toggle/slider consumer, all meaningful inherited control tests, and an all-control gallery.
This plan owns library delivery; RedXe AV device backends and the later RedSalamander application port remain separate.
Owning contracts: [architecture](../../Core/Core_Architecture.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[controls](../../UI/UI_ControlsAndLayout.md), [validation](../../Testing/Testing_Validation.md),
[build](../../Build/Build_ToolchainAndConsumption.md), [performance](../../Core/Core_PerformanceAndResources.md).

- [x] Consolidate Foundation and controls into one DxUi.lib and expose independent public headers.
- [x] Remove application dependencies; preserve shared controls, native text, accessibility and window-host behavior.
- [x] Supply externally owned graphics/resources and input through an embedded host with separate prepare/composite phases.
- [x] Add a standalone supplied-device toggle/slider consumer using only public headers and the single library.
- [x] Provide a catalog/factory and gallery covering every concrete public control, with generated visual artifacts.
- [x] Classify every inherited test, port reusable behavior tests, remove application/source-text-only cases with reasons.
- [ ] Validate WARP rendering/input/recovery, public consumption, idle/clean costs and the four native configurations.
- [ ] Reconcile all specs/skills/build contracts and AV dependency references; publish tested main and close this plan.
