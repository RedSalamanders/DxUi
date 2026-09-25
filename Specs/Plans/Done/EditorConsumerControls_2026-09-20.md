# Editor-consumer controls for RedPrism (splitter, color picker, numeric stepper)

Status: **DONE** (2026-09-21). Planned 2026-09-20 against DxUi `c8354a422e21d9c599993354ee44506e3ca8497e`;
implemented on `codex/editor-consumer-controls` from `4099dde` after `LocalizedAdaptiveLayout_2026-09-19.md` closed.
RedPrism C2 evidence is attached below. The durable contracts live in `UI_ControlsAndLayout.md`.

This plan is non-normative. Durable control contracts remain in
[`UI_ControlsAndLayout.md`](../../UI/UI_ControlsAndLayout.md),
[`UI_InputAndAccessibility.md`](../../UI/UI_InputAndAccessibility.md),
[`Core_PerformanceAndResources.md`](../../Core/Core_PerformanceAndResources.md),
and [`Core_Architecture.md`](../../Core/Core_Architecture.md).

Sibling consumer pin currently used by RedXe/RedSalamander:
`f5c7fc7403e352425b8e82b7a4da1c042d942ede` (API revision 2, target `DxUi`).
Library completion never implicitly changes that pin. RedPrism must adopt a new pin only after this
plan’s library gates pass.

## C2 evidence (RedPrism, 2026-09-20)

RedPrism C2 (`88e7e5b`) pinned DxUi `f5c7fc7403e352425b8e82b7a4da1c042d942ede` (API revision 2) and
composited an EmbeddedHost Button (`Hello DxUi`) into the one D3D11 back buffer. Harness captures
(not Computer Use): `.build/screenshots/hello-dxui-x64-Debug.png`, `hello-dxui-x64-Release.png`,
`hello-dxui-x64-ASanDebug.png`. Hidden surface bytes are 0; visible surface stays within the 64 MiB
cap. The 27-control catalog is therefore **reachable** from a compositor host.

It is still **insufficient** for the chrome the legacy editor actually uses:

| Gap | Legacy evidence | Why not an existing control |
| --- | --- | --- |
| Splitter | `ContentView` `HStack`: tool rail \| canvas \| `PanelResizeEdge` (8 pt hit, column-resize, Escape-less drag, persistable `layersPanelWidth` 252) \| Layers | StackPanel/ScrollPanel have no draggable separator, keyboard resize, min-size, or preview/commit/cancel. App DIP rectangles are a C7 fallback, not a second toolkit. |
| ColorPicker | `ColorPickerSheet`: 256² saturation/brightness field, vertical hue strip, new/current preview, RGB/hex, OK/Enter commit, Escape/capture-loss cancel, canvas sample | `ColorSwatch` is display + `SetOnClick` (“launch your picker”). Slider+TextField cannot author a 2D HSV field. Eyedropper outside the picker stays a host job. |
| NumericStepper | `TransformInspector` / `ArrowStepper`: labeled X/Y/W/H/angle/% fields, Up/Down nudge, Shift×10, unit suffix, clamp | TextField has no step buttons or nudge-while-focused contract. Grid spinner cells are table-only. |

**Promotion block:** `LocalizedAdaptiveLayout_2026-09-19.md` is still ACTIVE and owns `src/Controls/DxUi.cpp`,
`DxUi.Controls.cpp`, catalog/gallery, and related tests. Dual-editing those files from this plan is a STOP.
After that plan releases the files, promote Splitter first, then ColorPicker, then NumericStepper.

RedPrism C7 fallback (recorded in `Z:\src\RedPrism\Specs\UI\UI_ApplicationChrome.md`): app-owned DIP split
rectangles; ColorSwatch click opens a dialog composed from existing controls (saturation field MAY be a
RedPrism EmbeddedHost mesh); numeric inspectors use TextField plus host arrow-key nudge. Pin bump waits
for a green library commit.

## Why this exists

RedPrism is a Photoshop-class raster compositor. Its chrome needs a few **generic** retained controls
that the catalog does not have. Product-specific canvas, layers policy, blend modes, documents, and
brushes stay in RedPrism. This plan owns only shared controls that a second consumer could reuse
without compositor knowledge.

## Drift check

```powershell
git -C Z:\src\DxUi diff --stat c8354a422e21d9c599993354ee44506e3ca8497e..HEAD -- include/DxUi src/Controls Tests/Controls Specs/UI/UI_ControlsAndLayout.md capabilities.json
```

If `LocalizedAdaptiveLayout_2026-09-19.md` still owns layout/control files in the same slice, STOP and
coordinate; do not dual-edit `DxUi.cpp` / catalog without a single owner.

## Authority and boundary

Read [control-development](../../../.agents/skills/control-development/SKILL.md),
[consumer-integration](../../../.agents/skills/consumer-integration/SKILL.md),
[modern-cpp-wil](../../../.agents/skills/modern-cpp-wil/SKILL.md),
and [AGENTS.md](../../../AGENTS.md) before any edit.

Quoted non-negotiables:

- New controls require a catalog/factory entry, meaningful interaction tests, and a populated gallery tile.
- Edit `src/Controls` in place; never a second source tree.
- Composition does no heap, layout, raster, I/O, or blocking waits.
- Do not add file-operation policy, consumer resource IDs, settings persistence, or compositor domain types.
- `ColorSwatch` already displays a color and “click launches your picker.” A ColorPicker is a new control,
  not a rewrite of ColorSwatch; ColorSwatch remains the compact display that can open the picker.
- Do **not** add canvas/viewport, docking, ribbon, property-grid, layers, rulers, or infinite scroll as
  library controls in this plan.

## Proven catalog (do not duplicate)

Existing `ControlKind` values in `include/DxUi/ControlCatalog.h`: Panel, PageHost, CardPanel, Label,
Button, Toggle, Checkbox, RadioButton, RadioButtons, ProgressBar, ThroughputGraph, Slider, Toolbar,
MenuBar, TabControl, ColorSwatch, TextField, ComboBox, TagPicker, StatusStrip, PopupLayer, StackPanel,
ScrollPanel, TooltipLayer, Tree, Grid, PageIndicator.

Reuse first:

| RedPrism need | Existing control |
| --- | --- |
| Tool rail | Toolbar + Toggle / RadioButtons |
| Document tabs | TabControl |
| Menus | MenuBar + ContextMenu + NativeMenuInterop |
| Layers list | Tree or Grid + consumer `IDxTreeModel` / `IDxGridModel` |
| Opacity | Slider (preview/commit/cancel) |
| Blend mode | ComboBox |
| Color well | ColorSwatch |
| Vertical panels | ScrollPanel |
| Status | StatusStrip |

## Proposed catalog additions (generic only)

Promote from HOLD to ACTIVE one control at a time after RedPrism C2 evidence.

### 1. Splitter

Two (later N) panes with a draggable separator, explicit bounds, keyboard resize, and a minimum size
per pane. Both orientations. Preview/commit/cancel: dragging is preview; capture loss / Escape restores
the previous split; mouse-up commits. Persistable ratio is consumer state, not library settings.

Why not app-owned layout only: RedSalamander dual-pane and RedPrism canvas|layers share the same
interaction contract. A consumer-local splitter would fork hit-testing, DPI, and UIA.

### 2. ColorPicker

HSV field + hue strip + RGB/hex fields + new/current preview, matching the interaction in
`Z:\src\RedPrism\legacy\Compositor\UI\ColorPickerSheet.swift` **without** importing Swift types.
Eyedropper **outside** the picker is the host’s job (canvas click); the control exposes a sample-color
API the host can call. Preview while dragging; commit on OK / Enter; cancel on Escape / capture loss.

ColorSwatch click remains “open picker” via consumer callback, or the picker can be parented in a
PopupLayer / floating `ControlHost` window. Do not put document sampling policy in DxUi.

### 3. NumericStepper

Standalone labeled numeric field with step buttons, arrow-key nudge, optional unit, clamp, and
preview/commit/cancel. Grid already has a spinner **cell kind**; this is the dialog/inspector control
for transform X/Y/W/H/angle. Prefer extracting shared numeric editing from Grid rather than a second
parser.

## Explicitly out of scope

- Zoomable/pan canvas, rulers, guides, pixel grid
- Docking / MDI / floating tool windows as a framework
- Ribbon
- PropertyGrid (compose Label+TextField+Slider+Combo+NumericStepper first)
- Horizontal+vertical infinite widget canvas
- Any RedPrism document, blend-mode, or brush type in public headers

## Implementation pattern (when promoted)

For each control, in one closeable slice:

1. Public type in `include/DxUi/DxUi.h`.
2. Implementation under `src/Controls`.
3. `ControlKind` + `GetControlCatalog` / `CreateControl` in `ControlCatalog.h/.cpp`.
4. Interaction tests in `Tests/Controls` covering pointer, keyboard, cancel, disabled, hidden, UIA.
5. Gallery tile; `gallery.ps1 -PublishDocs`.
6. Normative paragraphs in `UI_ControlsAndLayout.md` and input/a11y notes if gestures are new.
7. Paired perf receipts if the control can appear in a dirty frame (no silent rebaseline).
8. `capabilities.json` remains truthful.

## Verification (library, not consumer)

```powershell
.\validate-skills.ps1
.\validate-specs.ps1
.\format.ps1 -Check
.\build.ps1 -Platform x64 -Configuration Debug
.\test.ps1 -Platform x64 -Configuration Debug
.\test.ps1 -Platform x64 -Configuration Release
.\test.ps1 -Platform x64 -Configuration "ASan Debug"
.\build.ps1 -Platform ARM64 -Configuration Debug
.\build.ps1 -Platform ARM64 -Configuration Release
.\build.ps1 -Platform ARM64 -Configuration "ASan Debug"
.\gallery.ps1 -PublishDocs
```

Library tests do not qualify RedPrism. After merge, RedPrism updates `Dependencies/DxUi.lock.json`
to the landing commit and runs its own product suite.

## STOP conditions

- LocalizedAdaptiveLayout still has an exclusive lock on the same files and no coordination note.
- A proposed control is actually compositor-specific (layers blend UI, brush HUD, curves graph).
- Composition would allocate, layout, or rasterize.
- Adding the control would require a second D3D device, HWND, or swap chain inside the library.
- Consumer pin/docs would be silently edited from this repository.

## Done criteria

- Each promoted control is catalogued, tested, documented, and gallery-covered.
- Durable contracts live in domain specs, not only this plan.
- This file moves to `Specs/Plans/Done/` and leaves the WIP index.
- No consumer lock file in RedXe, RedSalamander, or RedPrism was modified by the library change.

## Implementation record (2026-09-21)

All three controls landed in one slice because the file owner had released the control sources:

| Control | Public type | Source | Tests | Gallery |
| --- | --- | --- | --- | --- |
| Splitter | `DxUi::Splitter`, `SplitterChange{Preview, Commit, Cancel}` | `src/Controls/DxUi.EditorControls.cpp` | `Tests/Controls/DxUiTests.EditorControls.cpp` (9 cases) | `Splitter / Vertical`, `Splitter / Horizontal` |
| NumericStepper | `DxUi::NumericStepper`, `NumericStepperChange` | same | 7 cases | `NumericStepper / Inspector` |
| ColorPicker | `DxUi::ColorPicker`, `ColorPickerChange`, `HsvColor`, `HsvFromArgb`, `ArgbFromHsv`, `ParseHexColor`, `FormatHexColor` | same | 10 cases incl. helpers and catalog | `ColorPicker / Sheet` |

Also: `ControlKind::Splitter / NumericStepper / ColorPicker` (catalog 30), `WindowHostCursorKind::VerticalResize`
mapped to `IDC_SIZENS`, the `EditorControls` suite in `test.ps1`, `capabilities.json` `editor-controls`,
`docs/controls.md` rows, `CHANGELOG.md`, and the normative sections in `UI_ControlsAndLayout.md`. API revision
stays 2 (additive). Deviations from the proposal: the hex field previews only once six digits are typed (the
three-digit short form applies on Enter or focus loss); a `Splitter` position set before the first layout is kept
until bounds arrive so a persisted consumer value survives construction order.

Gates run on 2026-09-21 (x64): `format.ps1`, `validate-specs.ps1`, `validate-skills.ps1`, `test.ps1` Debug,
Release and ASan Debug (19 suites each), `build.ps1 -Platform ARM64 -Configuration Debug`,
`gallery.ps1 -Configuration Release -PublishDocs`. No consumer lock file was modified from this repository;
RedPrism adopts the landing commit through its own `Dependencies/DxUi.lock.json` and product suite.
