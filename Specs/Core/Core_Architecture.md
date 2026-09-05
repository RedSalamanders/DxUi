# Architecture and ownership

Status: normative intended contract
Last reviewed: 2026-09-05

Implemented capabilities are listed in [capabilities.json](../../capabilities.json); requirements for pending
targets are acceptance contracts, not claims of current support.

## Canonical source ownership

This repository is the root and home of DxUI. Shared implementation and tests evolve here; RedXe and RedSalamander
are consumers. Source is organized under `src`, tests under `Tests`, supported API under `include/DxUi`.
The owned control/host source currently lives in `src/Controls`, with tests/baselines in `Tests/Controls`.
These files are editable even while standalone dependency removal is pending. Build support remains explicit in
`capabilities.json`; a directory move or namespace change does not establish tested runtime support.

Historical source commit, original hashes and current ownership mappings live in `provenance/source-origin.json`.
Git history retains earlier bytes. No duplicate original source tree is retained or periodically synchronized.
`provenance/pending-dependencies.json` enumerates unresolved application includes until they are removed.
The frame runtime has one implementation under Foundation; obsolete application-bound project files are retired.

## Target boundaries

| Target | Responsibility | V1 consumer |
| --- | --- | --- |
| `DxUi.Controls.lib` | Shared retained controls, layout, state, theme/text, prepared D2D drawing and host abstraction. | AV Control and the library's two samples. |
| `DxUi.Embedded.lib` | Externally scheduled content preparation and texture composition using a supplied D3D11 device. | AV Control. |
| `DxUi.Win32Services.lib` | Reusable OS text-input/accessibility service implementation behind bounded transport contracts; no window renderer. | RedXe host adapter where needed, and WindowHost. |
| `DxUi.Win32Host.lib` | HWND/message integration, window presentation, popups and scheduling on top of shared controls/services. | WindowHost sample now; RedSalamander later. |

Controls depend on neutral host-service contracts, not the concrete embedded or window host. The services target must
not pull the control renderer into `RedXe.exe`. The embedded target must not link the window presentation target.
The public namespace is `DxUi`; application compatibility aliases, viewer-theme translation, logging IDs and settings
adapters live with the consumer. Platform libraries and WIL are allowed dependencies. A monolithic RedSalamander
`Helpers.h`, settings store, plugin ABI, animation dispatcher or global logger is not.

Library controls implement UI semantics, not AV operations. DxUi never enumerates audio/camera devices, switches
profiles, stores RedXe settings, or embeds XENEON-specific dimensions. The consumer supplies labels, application
state, style tokens and layout policy. Host chrome keeps its own icon source; AV uses the shared control/glyph system
with the agreed Fluent-font fallback, and does not copy browser Lucide assets into the native product.

Use one independent source repository and MSBuild static-library targets. Build the selected targets with the
consumer's supported toolchain and link them into the binary that owns the controls. For the first RedXe integration,
that binary is `AVControl.dll`. A small OS-services target may also be linked into `RedXe.exe` for host-owned text
and accessibility integration; it does not contain a second control tree or renderer.

| Integration | Benefit | Cost / constraint | Decision |
| --- | --- | --- | --- |
| Pinned source, compiled into static `.lib` targets | Shared implementation and tests; ordinary C++ internally; no additional DxUi runtime DLL to deploy. | Consumer rebuild on updates; code/state can be duplicated if many DLLs link the same targets. | **Use for V1.** Link only the necessary targets. |
| Shared `DxUi.dll` runtime | Can centralize code and resources across modules within one process. | Export/ownership ABI, version negotiation, loader/staging policy, and coordinated lifetime become a separate product surface. | Defer until measurements justify it. A future DLL needs its own reviewed interface contract. |
| Include the sibling `.cpp` files in each application project | Small initial project-file change. | Consumer flags and source inventories drift; no independent build boundary. | Reject. Consume a project/library target. |
| Copy DxUi source into each application | Each checkout is self-contained. | Permanent parallel implementations and fixes; defeats the requested shared ownership. | Reject as the ongoing integration model. A documented initial extraction is the only copy. |

Static linking does not by itself share runtime memory between executables or between independently linked DLLs.
V1 has one control runtime/cache owner inside `AVControl.dll`, shared by all its instances and staged pages. Before
a second RedXe DLL links the renderer, establish process-wide resource reuse or measure and explicitly justify
duplication under the performance contract. Do not silently introduce one device or atlas per widget.

The library is source-coordinated, with no promise of a stable exported C++ binary ABI. Public headers, static
libraries, compiler/STL, CRT mode, architecture, configuration, and feature switches must match. `/MDd` is the Debug
baseline and `/MD` the Release baseline. DxUi C++ objects, STL containers, exceptions, allocation ownership, and
callbacks containing C++ state never cross RedXe's plugin ABI. Use its size-pinned COM/POD contracts at that boundary.
This avoids cross-module ownership assumptions of the kind described in Microsoft's
[CRT boundary guidance](https://learn.microsoft.com/en-us/cpp/c-runtime-library/potential-errors-passing-crt-objects-across-dll-boundaries?view=msvc-170).
