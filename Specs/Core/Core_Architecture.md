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
exposes supplied-device graphics and scheduling; `ControlCatalog.h` enumerates/constructs all 30 concrete controls;
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

Revision 4 adds ownership-safe extraction. `Panel::TakeChild` cancels branch capture before releasing focus with lifetime checks,
leaves an empty logical slot, clears host/parent links, and preserves effective inherited flow/density without making
them explicit overrides. Adoption into a new owner resumes inheritance from that owner. `TabControl::TakeTab`
and its virtual `TakeChild` also remove matching tab metadata and reconcile selection; virtual `ClearChildren`
clears tab metadata together with pages. Legacy mutable owning spans remain a compatibility surface; arbitrary
slot rearrangement and tab extraction through those slots are unsupported. Application callbacks may retire or
replace a control tree, so callers revalidate both owner and borrowed-model lifetimes after every external callback
and callable-capture cleanup. Physical retirement at a dispatch boundary remains an evaluation, not a promise that
callbacks or accessibility publication cannot reenter. Whole-host destruction within a callback remains separately
unqualified.

Borrowed Tree/Grid queries capture the control lifetime, model pointer and private mutation revision. Replacing a
model, assigning a delegate or notifying model changes invalidates an older query, including replacing and restoring
the same pointer. Parent paint, tick and hit-test traversal reacquires child storage after virtual callbacks and
rejects retired or detached hit targets. Failed model collections must not be committed as an empty selection.
An action distinguishes a delegate's completed `NotifyDataChanged` edit from replacing the model/delegate binding:
it can acknowledge a freshly verified completed edit while discarding the old query/animation transaction, but
binding replacement or physical retirement still ends the old request.
Queries also capture host identity and interaction geometry revision, so a flow, density or metric change inside a
getter or focus callback ends the old read before it can commit a newly resolved action against earlier pixels.
Model and page mutations also invalidate host geometry. A page replacement during painting retains a private pending
layout request; the next preparation synchronizes current and outgoing page bounds before paint. The host scans the
current owned tree only when such a request is pending, revalidating each owner after virtual callbacks, and allocates
no task queue. This supports callback replacement without doing layout in the render stage.
Geometry, visibility, enabled-state and supported ownership changes advance the same host revision in native and
embedded hosting. Detaching or attaching a branch schedules invalidation for both affected hosts. Native painting
therefore rejects a partial frame even when the root itself survives a child mutation. Visual-only invalidation
does not advance this revision, preserving embedded dispatch against still-current prepared hit geometry.
Flow/density overrides, density-changing themes and Tree/Grid row/header/indent metrics invalidate interaction
geometry. A color-only theme change outside painting remains visual-only. A theme assigned during painting aborts
the mixed-palette frame. Virtual hit and hover queries revalidate geometry before proceeding to action dispatch.

Library controls implement UI semantics rather than application operations. DxUi does not enumerate audio/camera
devices, switch application profiles, store consumer settings or embed product-specific dimensions. Consumers supply
labels, model state, style tokens and layout policy. Samples and measurements use synthetic library-owned data;
application adoption records and budgets stay in their owning repositories.

Use one independent source repository and compile its single static library with the consumer's supported toolchain.
A module shares GraphicsDevice pools by device generation and uses an EmbeddedHost for each independent view.
Application text/UIA bridges and cross-module services remain consumer integration decisions.
`redxe-adapter` means a consumer restores a pinned commit, links `DxUi.lib` once per module, and keeps DxUi C++
inside that module. It does not include real IME/assistive-technology (`embedded-host-text-uia-bridge`), AV
product backends (`av-control`), or another consumer's qualification. `redsalamander-migration` records
implemented replacement of the in-tree library on the I19 consumer branch, qualified locally in x64 Debug/Release.
It does not claim consumer master deployment, deferred ARM64/ASan passes or unavailable hardware coverage.
[The completed routing record](../Plans/Done/RedSalamanderMigration.md) points to the consumer evidence.

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
