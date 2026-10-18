# DxUi

Shared Windows DirectX controls, owned here and consumed through **one static library: DxUi.lib**.

Read the [documentation](docs/README.md), [control guide](docs/controls.md), and
[generated gallery](docs/gallery/README.md). [Performance and testing](docs/performance.md) explains measured
complex-UI FPS, memory budgets, before/after comparisons and the distinction between ARM64 builds and native tests.

## Build, test and generate every control

Requires Visual Studio 2026/v145, Windows SDK 10.0.26100, x64 or ARM64, and Git for pinned WIL/vcpkg restore.

```powershell
.\vcpkg-install.ps1 -Platform All
.\test.ps1 -Configuration Debug -Platform x64
.\test.ps1 -Configuration Release -Platform x64
.\build.ps1 -Platform ARM64
.\build.ps1 -Platform ARM64 -Configuration Release
.\gallery.ps1 -SkipBuild
.\.build\x64\Debug\DxUi.EmbeddedControls.exe
```

The last command opens a generic effect toggle and intensity slider. Add `--complex-ui` for the independent 83-control workload, or `--output image.png` for a headless PNG. See [samples](docs/samples.md) and [library measurements](Measurements/README.md).
`gallery.ps1` generates all 34 public controls and variants in five themes plus an HTML index under `.build/gallery`.
Read [the example](Samples/EmbeddedControls/EmbeddedScene.h) for public-API construction and event handling.

## Public API and ownership

- [DxUi.h](include/DxUi/DxUi.h): retained controls, layout, theme and ControlHost services.
- [Embedded.h](include/DxUi/Embedded.h): supplied-device GraphicsDevice pool and EmbeddedHost preparation/composition.
- [RichDocument.h](include/DxUi/RichDocument.h), [RichMarkdown.h](include/DxUi/RichMarkdown.h),
  [RichTextLayout.h](include/DxUi/RichTextLayout.h): [bounded Unicode preparation core](docs/rich-text.md); retained RichTextEditor v0 preview with shared Unicode 17 grapheme navigation and native/embedded source TSF/IME and UIA adapters. Real IME/AT and complete Markdown conformance remain pending.
- [MarkdownSyntax.h](include/DxUi/MarkdownSyntax.h): owned lossless root-block and
  lexical analysis with cooperative preparation and opt-in RichTextEditor hosting;
  complete CommonMark/GFM parsing and rendering remain in progress.
- [ControlCatalog.h](include/DxUi/ControlCatalog.h): discover and construct all 34 concrete controls.
- [ThemeColors.h](include/DxUi/ThemeColors.h), [Diagnostics.h](include/DxUi/Diagnostics.h),
  [FrameRuntime.h](include/DxUi/FrameRuntime.h): neutral theme, borrowed diagnostics, timing and motion.

An application creates the D3D11 device with BGRA support, then supplies it to GraphicsDevice. Create an EmbeddedHost,
set its root tree, prepare changed content before drawing, bind the application render target and call Composite.
The application owns presentation and scheduling. Use one shared pool and a separate view for each simultaneous
layout; controls contain no AV device operations or application settings. Native ControlHost/WindowHost mode retains
Win32 text/accessibility and presentation support inside the same archive.

[Intrinsic layout and forms](docs/layout.md) describe `Control::Measure`, opt-in `FlexPanel`, and stable
`FormPanel`/`FormField` editors with labels, help, validation and UI Automation metadata. Manual containers keep
their explicit bounds. Applications own values, validation decisions, scrolling and fixed footer actions.

Consumers import [Build/DxUi.Consumer.props](Build/DxUi.Consumer.props) and
[Build/DxUi.Consumer.targets](Build/DxUi.Consumer.targets), pin an exact commit/API revision 4 with target `["DxUi"]`,
and use an isolated restore/build output root. See [the consumption contract](Specs/Build/Build_ToolchainAndConsumption.md).
No DxUi DLL or copied/enumerated library sources are required.

## Validation and remaining application work

Foundation, inherited control suites and supplied-device WARP tests all link the same library. The
[test-port inventory](Specs/Done/SourceImport/test-port.json) records every inherited case and exclusions. Native x64/ARM64
Debug/Release/ASan Debug CI produces per-suite receipts; cross-compilation is not reported as native execution. Eight original
visual baselines remain unchanged. Runtime logs and measurements stay in `.build`; `gallery.ps1 -PublishDocs`
publishes reviewed gallery images to `docs/gallery`.

RedXe pins a tested DxUi commit and ships synthetic text/UIA adapters (`redxe-adapter`). Real IME/touch/screen-reader
checks and AV backends remain application gates (`embedded-host-text-uia-bridge`, `av-control`). The library does
not change either application's runtime. RedSalamander's I19 consumer branch has completed local x64
Debug/Release adoption with the duplicate implementation removed; ARM64/ASan qualification and publication/merge
remain separate. See [the adoption record](Specs/Plans/Done/RedSalamanderMigration.md).
[Capabilities](capabilities.json) lists supported library mechanisms separately from those pending integrations.
It distinguishes implemented behavior from open physical-touch, native-platform, interactive-suite and API-revision-4
consumer qualification; the consumer-owned pin, adapter, publication and product-test actions are tracked in the
[API revision 4 handoff register](Specs/Plans/WIP/ProductionReview_2026-10-05/api4-consumer-handoffs.md).

Start with [AGENTS.md](AGENTS.md), [spec authority](Specs/README.md), and [active plans](Specs/Plans/WIP/README.md).
Every repository tool is a PowerShell 7 script: build, test, formatting, the validators and the tooling tests.
Historical source attribution is in [the import archive](Specs/Done/SourceImport/README.md); this repository is the root and home
of DxUi, with no upstream source tree and no dependency on either application checkout.

Affected iteration, full local coverage, exact reuse and pending PR obligations are described in [the testing guide](Tests/README.md).
