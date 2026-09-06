# Architecture and ownership

Status: normative intended contract
Last reviewed: 2026-09-06

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

## Canonical source ownership

This repository is the root and home of DxUi. Shared implementation and tests evolve here; RedXe and RedSalamander
are consumers. Source is organized under `src`, tests under `Tests`, supported API under `include/DxUi`.
The owned control/host source currently lives in `src/Controls`, with tests/baselines in `Tests/Controls`.
The controls and native tests compile independently of either application. Build support and pending consumer bridges
remain explicit in `capabilities.json`.

Historical source commit, original hashes and current ownership mappings live in `Specs/Done/SourceImport/source-origin.json`.
Git history retains earlier bytes. No duplicate original source tree is retained or periodically synchronized.
`Specs/Done/SourceImport/pending-dependencies.json` preserves the empty final inventory; validators reject new
application dependencies. The [archive explanation](../Done/SourceImport/README.md) describes why these records remain.
The frame runtime has one implementation under Foundation; obsolete application-bound project files are retired.

## Single library and hosting modes

The single public target is `src/DxUi.vcxproj`, producing **DxUi.lib**. Foundation, controls, embedded rendering,
native text/accessibility and Win32 hosting are implementation areas of that target, not separately shipped libraries.
Consumers reference this project once. There is no DxUi runtime DLL and no consumer-maintained source list.

Public headers are under `include/DxUi`: `DxUi.h` exposes all retained controls and `ControlHost`; `Embedded.h`
exposes supplied-device graphics and scheduling; `ControlCatalog.h` enumerates/constructs all 26 concrete controls;
`TextInputServices.h` and `EmbeddedAccessibility.h` expose application-side TSF/clipboard and lazy embedded UIA
attach; `ThemeColors.h`, `Diagnostics.h`, `FrameRuntime.h` and `Configuration.h` complete the supporting API.

`ControlHost` supplies the shared tree, text, theme, focus and drawing services. Its native mode attaches to a
caller-owned HWND (`WindowHost` is a source compatibility alias). `EmbeddedHost` contains a ControlHost configured
for a supplied device and callbacks; attempts to attach that host to an HWND fail. Embedded code does not run the
native presentation/scheduling path. The archive includes both modes; static linking alone does not guarantee that
no native object code or Windows system import is linked into an embedded consumer.

The library has no application include dependencies. It owns a small diagnostics adapter, fixed-capacity posted
payload registry, neutral ThemeColors record and native animation dispatcher. Consumers inject synchronous borrowed
diagnostic sinks; DxUi opens no application log file. Platform libraries and pinned WIL are permitted dependencies.
Public configuration is fixed by API revision: diagnostic hooks are compiled but dormant until used. Consumer flags
must not change class definitions. Control C++ objects remain within their owning module.

Library controls implement UI semantics rather than application operations. DxUi does not enumerate audio/camera
devices, switch application profiles, store consumer settings or embed product-specific dimensions. Consumers supply
labels, model state, style tokens and layout policy. Samples and measurements use synthetic library-owned data;
application adoption records and budgets stay in their owning repositories.

Use one independent source repository and compile its single static library with the consumer's supported toolchain.
A module shares GraphicsDevice pools by device generation and uses an EmbeddedHost for each independent view.
Application text/UIA bridges and cross-module services remain consumer integration decisions.
`redxe-adapter` means a consumer restores a pinned commit, links `DxUi.lib` once per module, and keeps DxUi C++
inside that module. It does not include real IME/assistive-technology (`embedded-host-text-uia-bridge`), AV
product backends (`av-control`), or RedSalamander in-tree replacement (`redsalamander-migration`).

Pinned source plus a static archive provides an ordinary C++ interface and no extra runtime deployment. A shared DLL
would introduce a second versioned ABI and loader/lifetime policy; defer it until measurements justify that cost.
Copying controls or enumerating sibling .cpp files in consumers is prohibited: shared fixes are made here.

Static linking does not by itself share runtime memory between executables or between independently linked DLLs.
Consumers must establish their process/module resource ownership and measure duplication before linking the renderer
into additional modules. Do not silently introduce one device or atlas per control instance.

The library is source-coordinated, with no promise of a stable exported C++ binary ABI. Public headers, static
libraries, compiler/STL, CRT mode, architecture, configuration, and feature switches must match. `/MDd` is the Debug
baseline and `/MD` the Release baseline. DxUi C++ objects, STL containers, exceptions, allocation ownership, and
callbacks containing C++ state must not cross an unrelated module ABI. Consumers define their own COM/POD contracts at that boundary.
This avoids cross-module ownership assumptions of the kind described in Microsoft's
[CRT boundary guidance](https://learn.microsoft.com/en-us/cpp/c-runtime-library/potential-errors-passing-crt-objects-across-dll-boundaries?view=msvc-170).
