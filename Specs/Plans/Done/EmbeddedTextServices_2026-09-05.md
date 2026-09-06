# Embedded text-service adoption

Status: COMPLETE (2026-09-06), library APIs and synthetic tests only

Owners: `Specs/UI/UI_InputAndAccessibility.md`, `Specs/Rendering/Rendering_EmbeddedD3D11.md`

This plan delivered the DxUi library text and embedded-accessibility surface. It does **not** close matched
performance, real IME/touch/screen-reader acceptance, or RedXe consumer-bridge adoption. Those remain on
RedXe `Specs/Plans/WIP/RFC_Plugins_AVControl.md` (`embedded-host-text-uia-bridge`).
Library extraction and the synthetic pin are historical in
[Bootstrap and RedXe adoption](BootstrapAndRedXeAdoption_2026-09-05.md).

- [x] Public revision-checked embedded text snapshot, composition preview/commit/cancel and safe lifetime tests.
- [x] Shared application-side OS services in DxUi.lib with no additional renderer: host-owned TSF/clipboard adapters,
  focus identity, coalesced locks and a public `--text-input` sample. Synthetic TSF attachment is covered; a real
  IME session is not.
- [x] Library embedded accessibility attach/update/disconnect with synthetic pattern, lifetime, geometry and
  marshaling tests. RedXe COM/POD transport and real assistive-technology acceptance remain consumer work.
- [x] Native synthetic matrix for this library surface: x64/ARM64 Debug/Release suites and
  [CI 33991645881](https://github.com/RedSalamanders/DxUi/actions/runs/33991645881) at `3208083` on `main`.
- [x] Reconcile capabilities and owning contracts for implemented library APIs; move remaining gates off this plan.

## Delivered library surface

`EmbeddedHost` exposes revision-checked `ReadTextInput` / `ApplyTextInput` / geometry queries / `CancelTextInput`.
`TextInputServices` borrows an application HWND on the UI STA and owns no renderer, swap chain, worker or timer.
`EmbeddedAccessibility.h` is the module-local UIA site; `AttachAccessibility` reuses native providers without an
embedded HWND. C++ records stay in-module; consumers define their own plugin ABI.

`capabilities.json` records `embedded-text-state`, `application-text-services` and `embedded-accessibility` as
supported. `embedded-host-text-uia-bridge` stays pending for the RedXe tree/event routing and real AT gate.

## Intentionally not closed here

| Gate | Evidence / owner |
| --- | --- |
| Matched text/UIA performance | OPEN. Passing v2 text-state pairs are in [TextInput/2026-09-05](../../../Measurements/TextInput/2026-09-05/README.md). [NativeStore](../../../Measurements/TextInput/NativeStore-2026-09-05/README.md) and [HostServices](../../../Measurements/TextInput/HostServices-2026-09-05/README.md) flagged bands or were inconclusive. EmbeddedUIA ABBA receipts were not committed under Measurements. RedXe AV owns the next comparison; do not waive failures. |
| Real IME, touch, screen reader | Not established. Synthetic NativeTextInput/UIA tests and a hidden TSF sample do not count. |
| RedXe generic transport acceptance | Synthetic adapters exist; real IME/AT release acceptance stays on RedXe `Specs/Plugins/Plugins_AVControl.md`. |

Earlier isolated-branch pause notes (`codex/av-input-services`, `codex/av-text-bridge`) are historical. The APIs are
on `main`.
