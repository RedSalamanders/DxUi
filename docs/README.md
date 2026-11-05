# Using DxUi

Builds require the default MSVC 14.52.x toolset. Release archives use function-level and whole-program compilation;
consumer EXE/DLL links use unused-function elimination and full LTCG with the exact matching tools. See
[toolchain and consumer integration](../Specs/Build/Build_ToolchainAndConsumption.md) for forwarding and cache identity.

DxUi is a Windows C++ retained-control library delivered as one static archive, **DxUi.lib**.
The API uses namespace `DxUi`. It supports a supplied-device embedded view and an HWND-based native host.

- [Getting started](getting-started.md): prerequisites, builds, exact-pin consumption and a minimal control tree.
- [Upgrade a consumer](upgrade.md): the 2 November delivery pin, API 3/4 migration, client changes, rebuild, validation and rollback.
- [Unicode rich text](rich-text.md): bounded source transactions, projections, Unicode 17 logical graphemes, retained editor and native/embedded source TSF/IME/UIA adapters.
- [Hosting and lifetime](hosting.md): graphics, input, DPI, animation, native windows and recovery.
- [Control guide](controls.md): all 35 controls, configuration, events and model ownership.
- [Intrinsic layout and forms](layout.md): opt-in sizing, bounded preparation, stable editors and accessibility.
- [Generated gallery](gallery/README.md): five themes and the runnable embedded example.
- [Design system](../Specs/DesignSystem/README.md): versioned DxUi palette tokens per theme, type styles, metrics,
  usage rules and a guideline and preview for every control. Rendered captures are in the [gallery](gallery/README.md).
- [Standalone samples](samples.md): minimal playground and the shared complex-UI benchmark scene.
- [Performance and testing](performance.md): complex-UI FPS, memory, comparisons and ARM64 evidence.

The [public example](../Samples/EmbeddedControls/EmbeddedScene.h) and its
[application host](../Samples/EmbeddedControls/Main.cpp) are compilable references.
The [capability manifest](../capabilities.json) distinguishes supported library behavior from consumer bridges
still to be implemented. Read the [normative specs](../Specs/README.md) when changing library behavior.

Licensed under [MIT](../LICENSE.txt). [Third-party notices](../THIRD-PARTY-NOTICES.md) cover the linked dependencies,
embedded Unicode/WHATWG data and retained source attribution. SDK ZIPs include their full licenses with working
local links and a file-hash inventory. No font or icon-font file is distributed with DxUi.
