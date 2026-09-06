# Single library, public controls and supplied-device consumption

Status: COMPLETE (2026-09-06)

Owning contracts: [architecture](../../Core/Core_Architecture.md), [embedded rendering](../../Rendering/Rendering_EmbeddedD3D11.md),
[controls](../../UI/UI_ControlsAndLayout.md), [validation](../../Testing/Testing_Validation.md),
[build](../../Build/Build_ToolchainAndConsumption.md), [performance](../../Core/Core_PerformanceAndResources.md),
[theme](../../UI/UI_ThemeAndTypography.md).

The user approved one DxUi.lib, a complete public control API, embedded rendering with a caller-supplied D3D11
device, a standalone toggle/slider consumer, all meaningful inherited control tests, and an all-control gallery.
This plan owned library delivery. RedXe AV device backends, remaining host text/UIA adoption, and the later
RedSalamander application port remain on their own plans.

- [x] Consolidate Foundation and controls into one DxUi.lib and expose independent public headers.
- [x] Remove application dependencies; preserve shared controls, native text, accessibility and window-host behavior.
- [x] Supply externally owned graphics/resources and input through an embedded host with separate prepare/composite phases.
- [x] Add a standalone supplied-device toggle/slider consumer using only public headers and the single library.
- [x] Provide a catalog/factory and gallery covering every concrete public control, with generated visual artifacts.
- [x] Classify every inherited test, port reusable behavior tests, remove application/source-text-only cases with reasons.
- [x] Validate WARP rendering/input/recovery, public consumption, idle/clean costs and the four native configurations.
- [x] Reconcile all specs/skills/build contracts and AV dependency references; publish tested main and close this plan.

## Delivered library

`src/DxUi.vcxproj` produces DxUi.lib, API revision 2, lock target `["DxUi"]`. Foundation, Controls, Embedded and
Win32 are internal implementation areas, not separate libraries. Public headers are DxUi.h, Embedded.h,
ControlCatalog.h, ThemeColors.h, Diagnostics.h, FrameRuntime.h and Configuration.h. Later text/UIA headers are
owned by [Embedded text services](EmbeddedTextServices_2026-09-05.md).

GraphicsDevice retains a caller-created BGRA D3D11 device. EmbeddedHost separates dirty Prepare from allocation-free
Composite. Shared device pool, independent views, logical capture, DPI, hidden/zero-size suspension, device
replacement and coherent prepared input are implemented. No embedded HWND, swap chain, worker or timer.

All 26 concrete controls have catalog/factory entries and populated five-theme gallery tiles. The supplied-device
toggle/slider example is under Samples/EmbeddedControls and uses public headers only. 941 inherited named cases are
inventoried in test-port.json: 853 retained runtime cases; 88 individually reasoned exclusions. Eight original PNG
baselines are unchanged.

## Validation

[Native CI 33991645881](https://github.com/RedSalamanders/DxUi/actions/runs/33991645881) at
`3208083836a89d2c3348e4389b105cf3c2b453fc` on `main` passed all five jobs: Linux validation and native x64/ARM64
Debug/Release. The x64 Debug job also ran the relocated exact-pin consumer, which rendered the public sample and
rejected invalid pins. Later banner-only commits on `main` are outside this library-delivery scope.

This closeout does not waive intermittent Menu-fixture failures on other commits, matched performance acceptance
on later text/UIA work, real IME/touch/screen-reader checks, RedXe AV backends, or RedSalamander migration.

## Closeout notes

A 2026-09-05 pause left this plan ACTIVE while CI on `26ac85f` was still failing. Subsequent `main` merges delivered
the remaining native matrix. Normative leftovers from bootstrap wording were corrected on closeout:
UI_ThemeAndTypography no longer treats control/text rendering as an unextracted target; Build_ToolchainAndConsumption
references the single `src/DxUi.vcxproj`; Testing_Validation states current WARP fixtures versus remaining adoption
gates; the source-import ledger matches the empty pending-dependency inventory.
