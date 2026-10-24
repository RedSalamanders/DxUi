# Architecture and ownership

Status: normative intended contract
Last reviewed: 2026-10-18

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

The bounded Unicode rich-text model, Markdown projection and prepared DirectWrite
layout are canonical in src/Controls with public RichDocument/RichMarkdown/
RichTextLayout headers. The standalone prototype is a consumer of DxUi.lib; no
second reusable implementation is maintained. These pure preparation services
own no HWND/device/worker. Immutable source snapshots use a persistent piece tree
with cumulative subtree/piece ends. Edits copy boundary paths and share untouched
subtrees and source buffers; snapshot reads retain no mutable cache. Node layout
is private behind the snapshot's existing shared ownership boundary.
The v0 projection reuses physical-line blocks; retained geometry stages bounded
chunks through shared owner preparation. Projection/index construction remains
indivisible, and geometry has explicit whole SDK fallbacks.
The opt-in lossless parser separately publishes block/inline forests, resolved
global references and configurable GFM metadata. It owns Unicode 17 folding and
character-class tables generated offline from pinned licensed data. Its semantic
strings/raw source enclosures are metadata; editing coordinates use the explicit
SourceDisplayMap contract. The v0 projection remains the default renderer.
RichSemantic adds an independent immutable span map with nonmonotonic primary
coordinates and fragment queries, and snapshot-guarded source edit staging.
It owns no host/device/worker. It does not relax v0 Projection's ordered-unit
invariants or activate semantic rendering. Structural adapters must commit its
edit plans through their existing editable/lifetime/notification guards.
RichSemanticDocument stages immutable semantic roles, style runs, paragraph/cell
metadata and exact value-token maps from completed MarkdownSyntax. Its builder
is an owner-serialized CPU preparation service with event budgets, cancellation,
coalescing and failure-atomic publication. It owns no graphics or scheduling
resource. This CPU service alone does not activate rendering or change source
text-store coordinates. RichTextEditor's explicit semantic preview consumes its
immutable publication. Semantic preparation also stages a complete source-format
partition and process-local publication identity. Rich UIA snapshots share this
immutable graph and retain optional prepared bounds/nominal typography only for
semantic editors; unrelated controls add no rich strings or rectangle vectors.
Semantic child providers retain weak graph identity plus the control's host
binding epoch. Geometry reflow can reuse CPU semantics; detach/rebind cannot
resurrect old children. Source text stores retain raw UTF-16 coordinates.
The semantic builder validates the bounded native HTML policy before hiding any
tag in a leaf. Its character-reference adapter calls the syntax lexer's existing
pinned decoder. Markdown style state and active HTML anchor ownership remain
separate, including when their delimiters cross. Unsupported markup publishes
literal normalized leaf contents while retaining exact raw source.
Native geometry measures marker prefixes without wrapping, computes shared list
gutters, then stages marker/body layouts on one first baseline. Both are counted
against native admission and SDK budgets. Ancestor list gutters apply to loose
continuations, nested blocks and tables. Marker/caret sites are geometry identity,
never source ACPs; all source movement preserves logical direction across boxes.
The retained RichTextEditor v0 preview uses ControlHost
services for native/embedded source TSF/IME and UIA, including bounded source range
commands. Explicit semantic rendering adds bounded style/structure providers;
real IME/AT acceptance remains pending.

MarkdownSyntax is a separate owned Unicode preparation service. Its immutable
source and exact root-block/lexeme partitions retain original spelling independently
of the v0 Projection. MarkdownParser stages resumable scanning and atomically
publishes a complete revision/profile; callers may retain older immutable results.
It owns no graphics, host, thread or timer. Parser/control calls are serialized on
their owner; retained immutable syntax can be read on other threads. Atomic
publication denotes failure-atomic replacement, not concurrent mutation of one
parser. Optional retained-control analysis uses
the same native/embedded owner preparation traversal before geometry. Completed
source-only analysis survives width/DPI changes; pending work is cancelled when
its control preparation is retired. This first stage does not replace the v0
renderer or expose structured Markdown through source TSF/UIA offsets.

DxUi.TextElements is the single pure UTF-16 decoder and Unicode 17 default
extended-grapheme implementation used by the existing text controls, rich
preparation and UIA character commands. It has no host/font/device, heap storage,
worker or timer. Its generated table is pinned to licensed Unicode data in
ThirdParty/Unicode; the generator and complete independent conformance corpus
belong here. Do not add a parallel rich-only segmentation utility or depend on
the installed font/OS glyph-cluster rules for logical character navigation.

## Single library and hosting modes

The single public target is `src/DxUi.vcxproj`, producing **DxUi.lib**. Foundation, controls, embedded rendering,
native text/accessibility and Win32 hosting are implementation areas of that target, not separately shipped libraries.
Consumers reference this project once. There is no DxUi runtime DLL and no consumer-maintained source list.

Public headers are under `include/DxUi`: `DxUi.h` exposes all retained controls and `ControlHost`; `Embedded.h`
exposes supplied-device graphics and scheduling; `ControlCatalog.h` enumerates/constructs all 35 concrete controls;
`TextInputServices.h` and `EmbeddedAccessibility.h` expose application-side TSF/clipboard and lazy embedded UIA
attach; RichDocument/RichMarkdown/RichTextLayout expose the bounded preparation core; `ThemeColors.h`, `Diagnostics.h`, `FrameRuntime.h` and `Configuration.h` complete the supporting API.

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
clears tab metadata together with pages. Extraction/removal stops without changing ownership or tab metadata when
a capture/focus callback moves its live owner to another host or restores focus/capture inside the requested branch.
The newer callback state is preserved; extraction returns empty in that case. Legacy mutable owning spans remain a compatibility surface; arbitrary
slot rearrangement and tab extraction through those slots are unsupported. Application callbacks may retire or
replace a control tree, so callers revalidate both owner and borrowed-model lifetimes after every external callback
and callable-capture cleanup. Physical retirement at a dispatch boundary remains an evaluation, not a promise that
callbacks or accessibility publication cannot reenter. Whole-host destruction within a callback remains separately
unqualified.

Before that cancellation, `TakeChild` and a TabControl's tab removal call the protected `Panel::OnDescendantDetaching`
on the panel and on every enclosing panel, walking the parent chain across non-panel containers such as PageHost, while
the subtree is still attached. The new virtual changes the class layout, so consumers rebuild against it. A container that
tracks its own hover or capture inside the subtree ends them there: `ScrollPanel` delivers the inner control's capture
loss and hover end, and releases the host capture it held for that control. It decides both before the first callback
can retire the subtree and asks nothing of a subtree that callback destroyed. Its forwarded gesture also ends when the
host's capture no longer carries it (a child released capture without a capture-loss callback), and neither it nor the
host captures for a press whose handler already ended the press (a blocking menu loop read the release) or gave the
pointer to another window. Containers also revalidate such observers
by lifetime and branch at every use, so a descendant removed by any other path is forgotten without being
dereferenced; a pressed descendant that disappears keeps its gesture until the release, which reaches no other child.

Borrowed Tree/Grid queries capture the control lifetime, model pointer and private mutation revision. Replacing a
model, assigning a delegate or notifying model changes invalidates an older query, including replacing and restoring
the same pointer. Parent paint, tick and hit-test traversal reacquires child storage after virtual callbacks and
rejects retired or detached hit targets. Failed model collections must not be committed as an empty selection.
An action distinguishes a delegate's completed `NotifyDataChanged` edit from replacing the model/delegate binding:
it can acknowledge a freshly verified completed edit while discarding the old query/animation transaction, but
binding replacement or physical retirement still ends the old request.
Queries also capture host identity and interaction geometry revision, so a flow, density or metric change inside a
getter ends the old read before it can commit a newly resolved action against earlier pixels.
Delegate callbacks, focus handlers and UI Automation event delivery are application code that may legitimately change the
model or any control's layout. After one, a Tree or Grid operation continues while the control lives in the same host
with the same model and delegate (`ResumeAfterCallback`): the change becomes the new baseline, rows are re-resolved by
stable id and geometry is recomputed. Its later reports, reveal, context menu, drag arming, checkbox toggle, animation and
UI Automation result still happen, so a callback that only enables a toolbar button changes nothing about the gesture.
A pointer press or double-click whose focus handlers moved, scrolled, mirrored or refreshed the control itself is
consumed when the target under the pointer is no longer the one aimed at: the same part, place and stable row, group
or item id. A right-click opens the menu for the pressed row found again by id after the selection callbacks (none
when they removed it), a checkbox toggles only while its column id is still in the pressed place, and a Tree expansion
request that the delegate answered by publishing succeeds only when the item now has the requested state.
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
Intrinsic measurement adds a non-pure control extension point and opt-in FlexPanel/FormField/FormPanel ownership;
manual Panel/StackPanel/Toolbar layout remains explicit. The added methods, vtable entries and numeric state require
consumers to rebuild against the matching pinned headers and static library. The library remains in the ongoing API
revision 4 adoption scope; earlier consumer pins are not changed by this implementation.
This avoids cross-module ownership assumptions of the kind described in Microsoft's
[CRT boundary guidance](https://learn.microsoft.com/en-us/cpp/c-runtime-library/potential-errors-passing-crt-objects-across-dll-boundaries?view=msvc-170).

The M4d RichTextEditor semantic preview adds an explicit rendering option and an
immutable semantic document accessor within API revision 4. Its private retained
layout adapter stages either existing v0/source geometry or semantic native
geometry, and switches only after a complete publication. The semantic backend
retains paragraph/cell DirectWrite layouts and a factory; it borrows raster targets
and brushes synchronously. The CPU builder remains independent of graphics.
Both paths use ControlHost owner turns, resource/typography/binding identities,
retirement and scheduling. No new worker, HWND, device or timer is introduced.
Consumers must rebuild matching headers/library; their pinned source is unchanged.

M4d.2 adds parser-owned `MarkdownEmptyLine` source trivia and a semantic element's
revision-local syntax-node index within this same source-coordinated API scope.
The native geometry backend maintains separate source-order caret boxes and a
display-order glyph index: generated markers and hidden source can make those
orders differ. Site affinity is private native state, never a document ACP or
history coordinate. Both indexes publish together after bounded preparation.
