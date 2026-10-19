# Unicode rich text

The additive v0 API lives in `DxUi::RichText` and links through the single
`DxUi.lib`. Include `DxUi/RichDocument.h`, `DxUi/RichMarkdown.h` and
`DxUi/RichTextLayout.h`. The catalog also contains the retained
`DxUi::RichTextEditor` preview described below. Its bounded source TSF/IME and
UIA adapters are implemented in both hosts. Real IME/assistive-technology
qualification, complete semantic Markdown editing and accepted extensions remain
separate gates in the [active plan](../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).
The explicit bounded semantic rendering preview is available under M4d below.

## Retained editor preview (D2c)

For explicit semantic coordinate preparation, include `DxUi/RichSemantic.h`.
`SourceDisplayMap::Build` owns validated source/display spans and supports hidden,
decoded, generated, object, repeated and reordered content. Use `DisplayRanges`
for selection fragments and `SourceRanges` for edit fragments; mapped endpoints
alone cannot describe a reordered selection. Generated text edits nothing, while
decoded values and objects expand atomically to their complete source spelling.
Range queries accept normalized scalar/CRLF boundaries. Caret affinities are
explicit; grapheme movement remains the layout/TextElements responsibility.

`ApplySourceEdits(document, expectedSnapshot, expectedRevision, edits, selection)`
stages ascending disjoint source replacements as one undoable transaction. It
rejects stale or foreign snapshots before mutation. Retain the expected snapshot
when preparing commands, and apply only while the same document revision is
current. The primitive is independent of host editability and notifications;
control adapters must enforce those before calling it. The retained
`RichTextEditor::ApplySourceEdits` adapter supplies those guards, synchronizes the
source text store and notifies once after a committed source change. No-op plans
may silently update selection. These primitives do not change the retained
editor's v0 pixels or complete semantic Markdown editing.

`DxUi/RichSemanticDocument.h` adds the pure `SemanticBuilder` preview under M4
qualification. Begin with an immutable `MarkdownParser::Published()` result,
then advance a bounded number of block/inline events on each preparation turn.
`Published()` retains the previous complete result while work is pending;
consumers must check its syntax source/revision/profile before treating it as
current. Cancel preserves that publication; Clear releases builder ownership.
Held documents remain immutable. Queries and coalesced requests do not advance
work or start a worker.

The document supplies a source/display map, style runs, semantic roles and ordered
paragraph/table-cell metadata. Its source remains exact; code/escape/entity
normalization has token-level edit affinities. Tables include padded cells;
task markers and image alternatives are atomic objects. HTML uses the bounded
native subset below with whole-leaf literal fallback. Links/images retain inert
metadata without loading or executing anything. The retained rendering opt-in
consumes this data separately; source-exact structural commands follow below;
structured UIA is described under M4f below. Admission limits combined syntax
nodes to 32,768 and geometry paragraphs to 4,096, alongside the bounded model/map.

Include `DxUi/DxUi.h`. The same control works under native `ControlHost`
and supplied-device `EmbeddedHost::Controls()`:

```cpp
auto root = std::make_unique<DxUi::Panel>();
auto* editor = root->AddChild<DxUi::RichTextEditor>(
    L"# Notes\r\nSelect **Unicode** text.");
editor->SetBounds(D2D1::RectF(16, 16, 480, 280));
editor->SetOnTextChanged([editor] {
    // Persist editor->Snapshot() through application-owned storage.
});
host.SetRoot(std::move(root)); // ControlHost, or embedded.Controls()
```

All mutable operations belong to the host/UI owner thread. Source UTF-16
anchor/caret offsets include Markdown delimiters; invalid scalar/CRLF boundaries
throw without mutation. `SetText` silently resets history; `SetSelection`
and `SetViewMode(Source/Rich)` are silent and preserve source bytes.
`ReplaceSelection`, `ToggleFormatting`, `Undo` and `Redo` notify once
per actual source change. The four supported v0 formatting markers are described
below. Read-only/disabled controls reject source commands; read-only allows
selection and copy. `SetOnTextChanged` runs after publication and may replace/
destroy the sender; standard exceptions are contained and do not roll back its
committed edit. A scheduling callback that retires, rebinds or supersedes the
editor stops the old notification.

The preview implements pointer selection/word selection, Shift extension, host
capture and cancellation, logical DirectWrite cluster navigation, word navigation,
line/document Home/End, Up/Down/PageUp/PageDown, Backspace/Delete, Enter as CRLF,
Ctrl+A/C/X/V/Z/Y, Ctrl+Shift+Z and Ctrl+B/I. Copy/cut transfers exact selected
Markdown source and original EOLs through the host's bounded Unicode clipboard;
busy/failed clipboard writes never cut. Paste validates before committing and
rechecks owner/revision/selection after clipboard callbacks. Surrogate pairs
commit together, orphan halves are discarded, and AltGr Unicode input is accepted.
This does not qualify full visual bidi/UAX navigation.

The control borrows shared host fonts, DWrite/D2D, brushes, focus/capture, clipboard,
theme, DPI, invalidation and animation. It owns no HWND/device/swap chain/thread/
timer. Layout and clipped text/selection/caret rasterization happen during host
preparation; EmbeddedHost's unchanged composition path only draws its cached
surface. Layout is reused for selection/palette-only repaint, rebuilt for source/
mode/width/typography/flow/DPI changes, and retired on hiding or host rebinding.
A hidden control paints nothing even when installed directly as the host root.
Scrolling is bounded to the viewport and caret changes reveal the selection.
Wheel movement never changes source/history. The caret uses host timestamps,
including an embedded epoch of zero; read-only/reduced-motion/hidden controls
request no caret animation.

This is an explicit bounded v0 preview. The D2d source adapters described below
use the existing shared OS services; real IME/candidate-window, screen-reader and
physical-input qualification is still required before production acceptance.
There is no incremental large-document layout, complete CommonMark/GFM or
math/Mermaid/footnote/front-matter rendering. Unsupported syntax remains source.
No consumer pin is changed automatically.

## Source text services and accessibility (D2d)

Native ControlHost activates its existing text-input cache and TSF/IMM adapter
for RichTextEditor. `SetFocusControl(editor, false)` establishes logical focus
without taking HWND focus. The native host owns TSF activation, the system caret
and IME candidate placement only while its HWND owns keyboard focus. No editor
HWND or private OS service is introduced.

Embedded applications use the same `ReadTextInput`, `ApplyTextInput`,
`HitTestTextInput` and `GetTextInputRangeBounds` bridge as TextField. Keep one
TextInputServices client for the immutable focusId, reject stale revisions,
convert view DIPs to physical screen coordinates, and call NotifyLayoutChanged
after coherent preparation/placement changes. Clear the application-side client
on focus loss or view retirement. The view owns no HWND or TSF thread.

ACP offsets, service snapshots and UIA text all refer to exact Unicode Markdown
source, including hidden delimiters and original CRLF. Geometry maps source
ranges through the current prepared projection, wrapping, scroll, flow and DPI.
A hidden delimiter range has no pixel rectangles. Source mode displays its
rectangles again. Hidden/retired controls and incoherent embedded preparation do
not supply current geometry. `GetVisibleSourceRange(host)` includes complete
wrapped lines intersecting the viewport, including partial lines.

Composition Preview updates are silent and preserve undo and redo. Cancel
restores entry source and selection; Commit restores entry source before recording
one ordinary transaction and one callback, including when the result equals its
last preview. Revisions remain monotonic. The reusable Document::ReplaceTransient
performs the history-preserving update; callers must restore composition-entry
source before recording the final replacement. Invalid UTF-16, NUL, oversized
source and scalar/CRLF-splitting selections are rejected atomically. An application
edit supersedes an older service preview. Retained services cannot write through
replacement focus or a retired owner. Callback-driven removal stops old publication.
Composition uses the shared caret ink for a 1 DIP underline and a 2 DIP conversion
target underline inside the text viewport.

Both UIA hosts publish Document semantics, Text, Text2 and TextEdit patterns through
the same immutable snapshots and stable control identity. Text2 reports the actual
caret of backward selections. TextEdit reports composition/conversion ranges;
Text exposes exact source, selection, visible ranges, point hits and projected
rectangles. Read-only documents permit selection and expose IsReadOnly; disabled
controls reject actions. Multiline documents do not duplicate their content in
ValuePattern. Endpoint-to-range movement validates a common live owner and
collapses crossing endpoints. Retained rich ranges read current published source
at bounded source offsets, clamping scalar/CRLF boundaries after replacement;
they do not track the semantic identity of text shifted by an edit. Reacquire
DocumentRange for the complete grown document. Bounded GetText never returns
half a supplementary scalar. Retained providers/ranges disconnect on owner replacement. Unknown
text attributes and annotations are unsupported in source/v0 presentation.
Explicit semantic presentation adds the bounded M4f attributes/children below;
rendered-only TextPattern text and annotations remain future scope.
The bounded source range commands include scrolling, selection union/subtraction,
text search and uniform read-only attribute search. The rules below apply in both
hosts; the M4f opt-in extends them with prepared style and structure.

`ScrollIntoView(true)` aligns the range's first painted edge to the viewport top;
`false` aligns its last edge to the bottom. Document extents clamp both offsets,
and ranges taller than the viewport reveal the requested edge. A degenerate range
reveals its caret. Hidden-only markers have no painted extent and do not jump to
an offscreen anchor. Scrolling preserves source, selection and undo/redo, takes no
keyboard focus, and uses existing preparation/invalidation services. The public
owner-thread `ScrollSourceRangeIntoView` validates source boundaries; UIA clamps
retained offsets to current source before calling it.

The editor reports `SupportedTextSelection_Single`. `AddToSelection` selects a
range when there is only an insertion point, or unions overlapping/touching
ranges. `RemoveFromSelection` trims a selected prefix/suffix; removing the entire
selection leaves its active insertion point. Both preserve backward direction.
Disjoint addition or middle subtraction returns `UIA_E_INVALIDOPERATION` atomically.
A degenerate add moves the insertion point; degenerate/nonoverlapping removal is
a no-op. Selection commands preserve history/revision and emit selection events,
without text-change callbacks. Read-only permits these operations and scrolling;
disabled controls reject them. Retired/hidden providers cannot act on successors.
Native foreign-thread actions use bounded owner dispatch. Embedded mutation is
owner-thread-only (`RPC_E_WRONG_THREAD` otherwise); snapshot searches remain reads.
Other text controls without these optional range actions report
`UIA_E_NOTSUPPORTED` instead of a success-only no-op.
The contiguous union/subtraction and rejection policy follows Microsoft's
[single-selection TextRange contract](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingtextandtextrange#selecting-text-in-a-text-range).

`FindText` searches exact source within the range, forward or backward, with
ordinal Unicode case-sensitive/insensitive comparison. Hidden syntax is searchable;
matches cannot bisect a surrogate or CRLF pair. Empty/malformed search strings
return `E_INVALIDARG`, and no match returns a null range with `S_OK`.
`FindAttribute` supports `UIA_IsReadOnlyAttributeId` with `VT_BOOL`: a matching
uniform attribute returns the bounded range, and a mismatch returns null.
Unimplemented rich attributes return `UIA_E_NOTSUPPORTED` for searches.

Clone, comparison, endpoint movement/collapse, word/character/document movement,
source text, enclosing element, visible bounds and source-only empty children are
covered in both hosts. `Line` movement uses the prepared wrapped rich projection
through `GetSourceVisualLineRanges`, including exact source CRLF and hidden
syntax. In source/v0 presentation unsupported Format units promote to Word; Paragraph/Page promote to
Document. Invalid units/endpoints/lengths are rejected. Character commands now
use shared Unicode 17 default extended grapheme rules. Visual bidi/word
navigation remains separate work; rich style/structure semantics are described below.

Caret, visible, composition and conversion ranges retain source offsets and
identity while reading the published document. They do not copy the full source
per query. The supplied-device regression bounds warm caret plus visible-range
C++ allocation requests to 4 KiB for both 32-unit and maximum 65,536-unit sources;
snapshot/layout preparation and Windows/driver allocations are outside that budget.

The non-activating native IMM/TSF protocol tests, embedded preview/revision tests,
actual application-bridged UIA client navigation/event tests and WARP captures
qualify these bounded adapters. They do not substitute for real input languages,
Narrator/NVDA or native ARM64 execution. See the [input contract](../Specs/UI/UI_InputAndAccessibility.md)
and [active qualification plan](../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).

## Reusable preparation core

```cpp
#include <DxUi/RichTextLayout.h>

DxUi::RichText::Document document(L"# Heading\nSelect **Unicode** text.");
document.SetSelection({2, 9});
document.ReplaceSelection(L"Title");
const auto snapshot = document.Snapshot();
auto projection = DxUi::RichText::Project(snapshot, document.Revision(),
                                          DxUi::RichText::ViewMode::Rich);
DxUi::RichText::PreparedLayout prepared;
DxUi::RichText::LayoutOptions options;
options.widthDip = 420;
options.fontFamily = L"Segoe UI";
options.fontSizeDip = 18;
prepared.Prepare(borrowedDWriteFactory, std::move(projection), options);
// During raster preparation only: caller owns BeginDraw/EndDraw, clip and target.
prepared.Draw(borrowedD2DTarget, borrowedBrush, D2D1::Point2F(8, 8));
```

`Document` is mutable on its owner's thread. `SourceSnapshot` shares immutable
buffers and can be handed to an application/shared preparation worker. Document's
source-taking methods borrow `std::wstring_view` only for the call and copy changed
source into immutable owned storage before returning. No-op admission precedes
owning-string/history construction, including Debug iterator proxies. Caller
buffers and temporary strings need no lifetime beyond the call. The WIP core's
source signatures changed from owning strings to views; rebuild against matching
headers and DxUi.lib. This does not provide compatibility with an older archive.
The core creates no worker, queue, HWND, graphics device, swap chain, timer or file. The
caller owns cancellation, request/revision keys, scheduling, stale publication
checks and view lifetime. `PreparedLayout` is also owner-thread mutable; do not
prepare or query it concurrently. Public operations can throw standard argument,
range, length/allocation exceptions, or WIL HRESULT exceptions for DirectWrite
failures. Catch at the caller's Windows/COM/callback boundary.

`Selection` stores anchor/caret UTF-16 offsets and preserves backwards direction.
Replacements are one source/selection transaction. Undo/redo restores snapshots
and selections while revisions increase. A same-content replacement only changes
selection without allocating source/history and preserves history/revision.
Invalid source, ranges or resulting
selection leave source, selection, revision and history unchanged. Reset clears
history and selection. The v0 bounds are 64 Ki UTF-16 units and 128 history items.
There is no ANSI API; UTF-8 conversion rejects malformed input and embedded NUL.
It rejects impossible byte lengths and decoded lengths above capacity before
allocating the UTF-16 result. Exact three-byte BMP and supplementary-scalar
capacity limits remain accepted. Accepted BOM and mixed line endings round-trip unchanged.

`SourceSnapshot::IsBoundary` and `Slice(first,last)` inspect shared pieces without
flattening a revision. Slices and edits preserve scalar and CRLF boundaries,
including CR/LF split across two pieces. A persistent B+ tree stores cumulative
piece/subtree ends, with 16..32 entries in each non-root node. Reads descend by
binary search and slices visit only intersecting entries. Edits copy boundary
paths, reuse untouched subtrees and rebalance split/merged nodes before publication.
Captured snapshots support concurrent immutable reads while the owner changes
its document. The Document itself remains owner-thread mutable. Changed edits
allocate the inserted buffer and copied paths; snapshot text materialization, projection
and layout still prepare the complete bounded document. The capacity is unchanged;
incremental layout and qualification beyond the bounded source remain pending.
Future selection checks run against the virtual replacement before storage/history
allocation. Reset, replacement, transient replacement, undo and redo stage all
required allocations before publishing; allocation failure preserves the full
transaction state. Default, moved-from and fully deleted snapshots are empty
queryable values. The node representation stays private behind shared ownership.

`SourceSnapshot::ChangedRangeFrom(previous)` returns a conservative replacement
interval with bounds in both immutable snapshots. Recent edits, transient
previews, undo/redo and copied-document branches use bounded numeric metadata;
the latest 96 edits are always tracked, with at most 128 retained. Missing
history or reset falls back to the complete source. Returned boundaries are
safe for `Slice`, including newly joined CRLF. Queries allocate nothing and change
no shared cache. Numeric pages keep no old source buffers alive. This source
service supports the planned incremental preparation; whole-document projection/
layout and shared scheduling are still separate work.
Whole-text IME previews are narrowed to their changed prefix/suffix interval
before replacing source. Shared fragments are compared without materializing old
text; scalar and CRLF edges remain intact and unchanged buffers stay shared.

The v0 grammar handles ATX headings, paired strong/emphasis/strike/code, nesting
and paragraphs. Unknown/incomplete syntax remains editable source. Source and
rich projections carry source/display mapping and explicit revision/mode. Hidden
markers have leading/trailing affinities. ToggleMarker supports `**`, `*`,
backtick and `~~` source transactions, including backwards selections. This is
not complete CommonMark/GFM. Math, Mermaid, footnotes, front matter, tables,
links, images and extended rich editing are unimplemented.

Preparation borrows a DirectWrite factory synchronously and owns its resulting
layout with WIL. Font/monospace families, locale (including DirectWrite
default empty locale), reading direction, size and width are caller settings; brush/color/transform/clip/target remain caller owned. Invalid public
projection/style/font input or DirectWrite failure retains the previous matching
projection and geometry. `Clear()` retires geometry, increments generation and
preserves the separate document/history. Draw accepts a replacement device's
caller-owned target without retaining it. Draw performs raster work: never call
it from allocation-free embedded composition. The retained preview uses the
host cached-surface preparation/composition boundary.

Rich arrows, deletion, UIA character commands and mouse hit stops use the shared
Unicode 17.0.0 default extended grapheme rules. Hangul, Indic conjuncts, contextual
emoji ZWJ and regional-indicator sequences are covered by all 766 original
conformance vectors at every UTF-16 offset. Prepared stops and password mask
indices use a forward walker with no C++ heap requests. Font glyph clusters
still belong to DirectWrite; explicit ACP caret/range geometry remains available
inside an element for text services. [Pinned data and regeneration](../ThirdParty/Unicode/README.md)
are kept in the repository. Logical grapheme support does not establish visual
bidi/word navigation or physical IME/AT behavior. This core supplies no TSF/IME/UIA,
clipboard, file persistence, retained theme/DPI invalidation or viewport scheduler.
The standalone prototype still owns those demonstration-host responsibilities.

`PreparedLayout::DeletionRange(position, backwards)` uses the prepared grapheme
stops to return a display interval. Both deletion directions enclose a complete
grapheme when an explicit ACP lies inside it. Exact-boundary deletion keeps the
usual preceding/following behavior; empty edges yield empty intervals. Invalid
positions or retired layout throw. Rich keyboard deletion maps this interval
back to source. Explicit noncollapsed source selections retain their exact
scalar/CRLF meaning, and undo restores the original source and caret ACP.

Bulk UIA character movement uses bounded forward traversal, with overflow-safe
INT_MIN/INT_MAX handling and actual signed counts. It adds no C++ heap work or
mutable index. This command improvement is separate from Default frame timing
and does not clear the performance hold.

Tests live in Scope.Tests.RichDocument, RichMarkdown and RichTextLayout under
[Tests/Controls](../Tests/Controls), registered in EditorControls. They retain the
prototype's randomized/history/codec/projection/real-DirectWrite scenarios and
add cross-piece Unicode/CRLF oracles, slice/failure/no-op contracts, font changes,
invalid projection retirement and actual drawing on two separate WARP devices.
This does not replace native/embedded retained-editor, IME, AT or large-file tests.

The migration is source-coordinated within one module; no C++ ownership crosses
plugin ABIs. It adds headers/functions without changing existing declarations,
so API revision stays 4. Consume an exact clean pin via the normal
[props/targets](../Build/DxUi.Consumer.targets); no sibling cpp enumeration.

The standalone prototype consumes exact code pin
302f3783a5d2ef60f201ca3abf508c1158fde039 through an immutable consumer checkout.
[Core and broader qualification](../Measurements/RichText/2026-10-17-core-d2/README.md)
records 57 normal suite runs and all six builds. Consumer linkage/ownership and
application smoke are separate receipts. Performance comparisons remain
advice-required; the original baseline and both warning reports are retained.

The [D2c qualification](../Measurements/RichText/2026-10-17-retained-d2c/README.md)
retains the native/embedded, sanitizer, build, gallery and timing evidence for
this bounded preview. Full OS input/accessibility and conformance are separate
from these passing v0 gates.

The [D2d qualification](../Measurements/RichText/2026-10-17-input-d2d/README.md)
retains the source-adapter protocol/client, history, retirement, geometry and
constant-size UIA range allocation evidence. It includes 57 ordinary x64 suite
passes and six builds. Physical IME/AT, rich structural UIA semantics, native
ARM64 execution and controlled performance acceptance remain open; both diagnostic
memory-warning reports are retained.

The [source range-command qualification](../Measurements/RichText/2026-10-17-uia-ranges/README.md)
records 57 ordinary x64 suite passes, six builds and 41 named rich-text scenarios
per x64 profile. Native/embedded command semantics and real-client actions/events
pass. Performance acceptance is on hold after repeated Default clean-frame p95
warnings; both failed comparisons and the original baseline are retained.

The [controlled Default investigation](../Measurements/RichText/2026-10-17-uia-profiling/README.md)
retains 48 interleaved reports and eight CPU/cache profiles. Its paired clean-p95
effect is -1.31%, with no judged regression; 13 of 24 same-binary controls cross
the clean-p95 band and all 24 flag some drift. A consistent slowdown and
non-regression acceptance are both unestablished. Policy qualification,
rich-editor workload performance and the original historical gates remain open;
no library source, threshold, baseline or consumer pin changed.

The [clear/readback follow-up](../Measurements/RichText/2026-10-18-uia-boundaries/README.md)
retains eight fixed processes with timers-off/on and omitted-command controls.
Blocking Map accounts for about 96.5% of each side's same-p95 full-frame tail;
controls remain variable and Windows refused scheduler tracing. This identifies
a wait boundary, without proving GPU duration, scheduler delay or non-regression.
The canonical workload and all original acceptance gates remain unchanged.

The [indexed-source qualification](../Measurements/RichText/2026-10-18-indexed-source/README.md)
retains its 57 ordinary x64 suite runs, six builds and both boundary-timer
modes in each x64 profile. The fragmented Unicode oracle and concurrent snapshot
reads pass; the fixed lookup-trial median moves from 6.5310 to 0.7234 ms in Release.
That observation is descriptive, with no paired performance acceptance. ARM64
runtime, full Markdown/extensions, incremental preparation and physical IME/AT
remain open. No consumer pin or gallery content changed.

The [Unicode 17 qualification](../Measurements/RichText/2026-10-18-unicode-graphemes/README.md)
retains the original corpus/allocation failures, 57 passing ordinary x64 suite
runs, six attested builds and all pinned generation inputs. Every profile passes
the 766-vector corpus and native/embedded behavior cases. The 49,152-unit forward
walk requests no C++ heap storage; oversized UTF-8 rejection also requests zero
C++ bytes in both tested cases. The refused ARM64 build-input mutation and its
two successful recovery builds remain separately recorded.

The [character-command qualification](../Measurements/RichText/2026-10-18-grapheme-commands/README.md)
adds linear, allocation-free bulk movement and complete-grapheme deletion from
collapsed interior ACP carets. Explicit noncollapsed source selections retain
their exact semantics. All 57 ordinary x64 suite runs and six builds pass,
including native/embedded maximal commands, undo/redo and the original Unicode
corpus at every offset. Embedded records 5,167 checks per profile; maximal command
queries request zero C++ bytes. Original failures and descriptive timing trials
are retained. The performance hold, incremental preparation,
remaining engine and manual acceptance gates continue under the plan.

The [persistent-storage qualification](../Measurements/RichText/2026-10-18-piece-tree/README.md)
retains all original resource failures, the intermediate Debug proxy failure and
the successful six-profile rerun. All 57 ordinary x64 suite runs pass, with zero
native skips. The fragmented source and capped history retain about 1.21 MB of
C++ storage in Release and 1.31 MB in Debug/ASan, below the fixed 4 MiB fixture
budget; the original vector implementation retained 40.2 MB. Same-content edits
request zero C++ allocations in every profile. All five failure sweeps preserve
complete source/selection/revision/history. Borrowed-input tests destroy or mutate
caller buffers after return and verify that document content survives. Exact observations and descriptive
edit/read/backward-query timings are in the packet. Incremental/shared
preparation, full Markdown/extensions and the performance/manual gates stay open.

The [bounded source-change qualification](../Measurements/RichText/2026-10-18-source-changes/README.md)
retains all 57 ordinary x64 suite successes, six cross/native build profiles,
original failures and unchanged fixed operation methods. Source-change queries
allocate zero C++ bytes; localized transient previews retain about 23 KB in the
fixed 4,096-preview fixture. Numeric metadata has an explicit 188,664-byte history
fixture cost. These are functional/resource observations. Incremental geometry,
shared preparation and frame-performance acceptance remain open in the plan.
`PreparedProjection` is the supporting v0 line-block preparation service. Call
`Prepare(snapshot, revision, mode)` on the preparation owner, then use `ToDisplay`,
`ToSource` or immutable `Blocks()` with document-wide source/display prefixes.
Block `content` uses local offsets and revision zero; the service's `Revision()`
and `Mode()` describe the published document. Unchanged outer blocks stay shared;
adjacent equal physical lines may share one local payload. `DisplayText()` is an
explicit flat copy. `LastPreparation()` counts rebuilt/reused block slots and
widened source units, not parser calls or elapsed-time acceptance.

Updates use tracked source intervals widened to physical lines and neighbors.
Mode/reset/missing ancestry falls back to complete bounded preparation. The
prefix index remains linear in line count. All new state is staged before one
publication; failure leaves the prior revision, payload identities and mapping
intact. `Clear()` releases its state and rejects content/mapping queries until
prepared again. This helper still uses v0 syntax. Its original qualification
preceded the guarded geometry integration below and provides no shared host
scheduling. No complete Markdown claim follows.
The [projection-block qualification](../Measurements/RichText/2026-10-18-projection-blocks/README.md)
retains the original reuse and Debug noexcept failures, 57 final native suite
successes, six builds, allocation/retention observations and unchanged operation
methods. Snapshot slicing now propagates allocation failure. Valid block mapping
and unchanged preparation allocate zero C++ bytes. Exact universal retained
rendering, shared scheduling and complete syntax profiles remain plan gates; the
descriptive operation improvements do not resolve frame-performance acceptance.

The retained editor's bounded geometry update is functionally qualified.
`PreparedDocumentLayout` joins source-line projections into chunks targeting
1,024 source units and caps the published document at 128 chunks. Unchanged
chunks reuse immutable DirectWrite layouts after source edits; source and geometry
publish together only after preparation succeeds. `Invalidate()` rejects stale
queries while retaining bounded reuse state. `Clear()` releases that graph.
Drawing remains owner-thread raster preparation and visits only visible chunks;
embedded composition keeps using its cached surface. `Clone()` creates an
independent projection service while sharing immutable block payloads; clearing
or preparing the clone does not change the original. Full shared scheduling,
complete Markdown/extensions and platform/performance acceptance remain pending.

The implementation reserves one of its 128 geometry slots for a canonical whole-layout
compatibility cache. The qualified positioned-line implementation prepends an invisible
origin line inside noninitial DirectWrite layouts. Its private UTF-16 units never
enter source/display mappings or text-service content. DirectWrite calculates
real lines in document coordinates and draws every chunk at the same origin.
Reuse requires the same origin: height-changing edits can rebuild the dependent
suffix. Inexact origin lines and empty displayed suffixes retain whole geometry
to preserve terminal-font behavior.
`UsesWholeGeometry()`, `UsesWholeRaster()` and the separate whole-layout preparation,
reuse and source-unit statistics make that work visible. Prefix indexing remains
linear in line count. Positioned lines pass all three x64 profiles and ARM64
cross-builds. The original height/pixel failures stay preserved; no oracle
or performance threshold is relaxed to accept the new cache.

The [geometry packet](../Measurements/RichText/2026-10-18-retained-geometry/README.md)
retains 57 passing ordinary x64 suite runs, six builds, complete observed
copy/geometry allocation sweeps, 162 exact WARP image pairs and fixed storage
bounds. The original Debug point-173 noexcept move failure is preserved. The
earlier native 13-DIP fixture needed whole-layout compatibility: its descriptive
128-edit median was 53,381.5 ms versus 51,102.6 ms before, with no controlled timing
acceptance. New affected Release checks avoid whole work in native and embedded
13-DIP editors. The [positioned-line packet](../Measurements/RichText/2026-10-18-positioned-lines/README.md)
adds 324 exact pixel pairs across default/fractional/large-origin profiles, 57
ordinary x64 suite successes and all six builds. The unchanged five-trial method
records medians of 145.909 ms initial preparation and 2,166.98 ms for 128 edits,
versus the original 522.474/51,102.6 ms. These descriptive shared-host observations
provide no controlled causal speedup, large-document latency or frame acceptance.
Universal incremental work and shared preparation remain active work.

The [thread-state follow-up](../Measurements/RichText/2026-10-18-thread-state-resolution/README.md)
preserves four same-binary observer controls and 1,280 clear/readback frames.
All captures match, but 17–25 ms query p95 cannot resolve 0.45–0.57 ms frame p95;
none of 35 acquisition brackets fits inside a frame. There is no Map wait-reason
or scheduler-delay conclusion. The Windows trace-policy refusal and performance
HOLD remain open. The separate WARP threading diagnostic is explicit:

```powershell
./performance-profile.ps1 -Configuration Release -SkipBuild -BoundaryTimers On -WarpRasterMode CallingThread -OutputPath .build/reports/warp-calling.json
```

Use `-WarpRasterMode Default` for a fresh comparison. The v2 report binds requested
and actual device flags. Omitting this option retains v1. The option requires
boundary diagnostics; it changes no library/canonical device default. CallingThread
names the requested SDK raster policy. Device-reported flags do not prove actual
application-main-thread execution or isolate scheduler latency.

The five repeated native preparation timing trials are now opt-in through
`test.ps1 -Suites EditorControls -Tests TestRichTextEditorRetainedPreparationFixedTrials`.
Ordinary reuse/retirement/geometry/input tests remain enabled; the timing body is
unchanged. Its explicit Release run passes all five trials against the preserved
binary. The first collector used the ordinary receipt name and failed after the
native run passed; the filtered receipt/log and all samples were recovered without
a native rerun. Both the collector failure and recovery remain retained.

The [WARP policy packet](../Measurements/RichText/2026-10-18-warp-raster-policy/README.md)
retains all 57 ordinary x64 successes, six build profiles, six isolated Debug/ASan
probes, full tooling/format checks, four native CLI refusals and legacy v1 smoke.
All 11 changed inputs match their frozen start/end hashes. Flat Project passes
21 allocation points in Release and 29 in Debug/ASan; no flat-helper defect was
reproduced under this compiler, and its value API remains unchanged.

The fixed Default/CallingThread/CallingThread/Default study preserves 1,280 frames
and four byte-identical populated captures. First full-work-window p95 is
0.7756/4.5538/4.6896/0.4622 ms, respectively. Map still accounts for about 99.6%
of the p95 frame in both CallingThread runs. Requested/actual flags are 32/32
and 40/40, but the intervention does not isolate application-main-thread work or
scheduler delay. Selected-image counters also record compiler activity during
the study; they cannot establish a quiet host or attribute individual frames.
Keep all windows/outliers and the original HOLD. This is a complex-UI diagnostic,
not a rich-editor edit-throughput or baseline/candidate acceptance result.

The earlier explicit native preparation trial recorded initial/edit medians of
683.7/66,413.6 ms on this shared host. Those observations retain the same library
fingerprint as the prior guarded geometry; no causal speed/regression conclusion
follows. The later positioned-line record qualifies exact retained placement;
cooperative preparation and the full Markdown/extension engine remain active work.

Cooperative preparation is implemented and configuration-qualified. The core exposes `BeginPreparation`,
`AdvancePreparation` and `CancelPreparation`; synchronous `Prepare` drains those
phases for direct core users. The control uses shared host turns, never an input/
geometry callback drain. An embedded pending turn returns S_FALSE with
`NeedsPreparation()` true; process the next wakeup before composing or publishing
geometry. Source typing, history and IME remain available while glyph work waits.

Hiding a native window or making either client dimension zero cancels retained
content, even if the shaping width stays unchanged. Show/restore prepares again
without losing source, selection or undo history. Native swap-chain ownership
and WM_SETREDRAW handling stay with the host.

The default owner advance permits four SDK layouts and 4,096 block/placement
steps. A physical paragraph/whole SDK call and the bounded v0 projection pass
remain indivisible. Logical commands without current geometry may project source
without DirectWrite. No hard input latency, large-document or resumable full
parser claim follows. All ordinary x64 Debug/Release/ASan suites and all six build profiles pass; native ARM64 runtime and physical IME/AT remain separate.

Parser design references requested for the complete engine:

- [MD4C](https://github.com/mity/md4c/tree/60e8065cfccd5d7ac561115f9a9bfbdf811f5cc9)
  uses block/span/text callbacks and offers a Windows UTF-16 build. Borrow its
  parsing/rendering separation and explicit callback handling. Callback abort
  is not resumable pause; complete-document parsing still needs scheduling and
  cancellation. Current-head claims are separate from release-0.5.2 and our
  pinned conformance corpora.
- [Discount](https://github.com/Orc/discount/tree/0cc4fef6b7ccdc19a8e64ef73aa21596b47c335b)
  separates input, compilation and document generation, with explicit dialect/
  feature flags. Use that separation to inform selectable profiles; its legacy
  Markdown compatibility is not a strict CommonMark/GFM oracle.
- [cmark-gfm](https://github.com/github/cmark-gfm/tree/27d942c8b0a62d192f616e5bf3578f4b6a89e180)
  exposes an AST, source positions, input feed/finish and extension interfaces.
  Use these to inform semantic nodes, extension boundaries and independent
  tests. Feed/finish alone does not establish resumable edit reflow or bounded
  CPU time.

These are design inferences from pinned public README/API/license snapshots.
No parser implementation/dependency is adopted. The owned editor still needs
lossless anchors, explicit UTF-8/UTF-16 mappings where applicable, the accepted
extensions and full corpus qualification. An HTML output renderer or private
Microsoft engine does not replace the owned Unicode control.

The [cooperative preparation record](../Measurements/RichText/2026-10-18-cooperative/README.md) retains native/build/pixel/fault/resource evidence, all original failed/interrupted attempts and pinned Ghidra/parser references. This closes the scheduling component; the full engine and original frame-performance HOLD remain open.

## Owned lossless Markdown preparation

`DxUi/MarkdownSyntax.h` starts the owned parser independently of the v0 renderer.
It retains exact Unicode source, root block ranges, a container/leaf tree and an
independent complete lexical partition. Block recognition covers nested/lazy
quotes and lists, tight/loose and empty items, headings, thematic breaks,
indented/fenced code, seven HTML forms and multiline reference definitions. Original
delimiters, trivia, unknown constructs, mixed line endings and entity spellings
are preserved. The parser accepts raw NUL and supplies a replacement lexical
value; the existing editor Document/codecs/text services still reject NUL.

The three grammar targets are CommonMark0312, Gfm029 and RichMarkdown. They are
versioned grammar keys. The bounded base parser and specified GFM extensions pass
the original semantic corpora; RichMarkdown's additional extensions remain pending. Lexical
candidates include all 2,125 semicolon-terminated named references, bounded numeric
references and ASCII punctuation escapes. A candidate inside code/HTML cannot be
rendered by decoding it in isolation; use the resolved inline forest described
below. `LexemeSourceAnchor` expresses leading/trailing source affinity when one
reference expands to several UTF-16 units. It does not replace model edit-boundary
checks or provide generated-object/footnote semantic anchors.

Use opt-in analysis in either retained host:

```cpp
editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::CommonMark0312);
// Continue the host's ordinary preparation turns. This query never drains them.
if (auto syntax = editor->GetMarkdownSyntax())
{
    auto exactSource = syntax->Source(); // Borrow while retaining syntax.
    const auto& rootBlocks = syntax->Blocks();
    const auto& nodes = syntax->Nodes(); // nodes[0] is the document root.
    auto documentIdentity = syntax->DocumentIdentity();
}
// Optional analysis can be disabled without editing source or history.
editor->SetMarkdownAnalysisProfile(std::nullopt);
```

`MarkdownParser` is also usable without a control. Begin owns/validates bounded
source; Advance resumes stored scanning cursors with an operation budget (default
4,096). Zero budget does nothing. Cancellation and failed staging retain a prior
published result; a current control getter rejects obsolete revision/profile
results immediately. Held revisions remain immutable after edits or detachment.
No worker, timer, graphics resource or additional HWND is created. Begin's copy,
validation and same-source comparisons remain indivisible, and operation budgets
do not guarantee a wall-clock deadline. The 65,536-unit limit remains.

Prefer the `Begin(SourceSnapshot, revision, profile)` overload when maintaining
an edited document. The retained editor does so: tracked edit ranges distinguish
which identical paragraph was removed. Node identities are scoped to one parser
until Clear and reconciled under compatible parent identities. Generated missing
table cells may share empty ranges; each consumes one prior identity in traversal
order, retaining distinct IDs across unrelated edits and undo/redo. String Begin uses
common-prefix/suffix mapping; deleted/reclassified/reparented nodes may acquire
new IDs, and no unbounded syntax history is retained. Blocks are reparsed across
the bounded source before reconciliation. Per-line transitions, paragraph
finalization and tree compaction remain indivisible; step budgets are not hard
latency guarantees.
`BlockStatistics()` reports deterministic prefix inspection and container closure
counts. Prefix lookahead uses bounded thresholds and line metadata; lazy lines
share a deferred accepted container end. These counters support regression tests
and do not measure complete CPU time. Node identity is a logical handle, not a
cache-validity or geometry-generation key; also check the source revision/profile.

HTML blocks remain exact opaque source. A later semantic stage will map an
explicit safe subset to our native renderer. No browser DOM, script execution,
event handler execution or automatic remote loading is introduced. Unsupported
HTML remains editable source. Analysis alone retains the v0 preview; the explicit
semantic rendering option uses the bounded native HTML policy below.

The native tests preserve all 652 CommonMark and 672 GFM original example inputs
and check all named entity values against independently transcribed standard data.
The independent M3 semantic checks described below additionally compare every
original expected HTML output. Semantic rendering/editing, math, Mermaid,
footnotes/front matter and end-to-end NUL admission remain future work.
Pinned inputs, attribution and offline generators are described in
[ThirdParty/Markdown](../ThirdParty/Markdown/README.md). The control catalog,
styles and existing gallery example remain unchanged; embedded exact-pixel tests
compare the same preview with optional analysis disabled and enabled.

The [parser-stage qualification](../Measurements/RichText/2026-10-18-markdown-parser/README.md) retains all 57 ordinary x64 suite receipts, six root builds, 324 exact rendering pairs, complete parser failure traces/storage measurements and the original failed fixture. Full grammar/HTML conformance, semantic rendering/editing, extensions and the original platform/performance gates remain open.

The [M2 block-tree qualification](../Measurements/RichText/2026-10-18-markdown-blocks/README.md)
adds 568 independent pinned block-structure/code-content/ordered-start expectations,
conservative snapshot-based node IDs and native/embedded grammar/Unicode edge tests.
All 57 ordinary x64 scope obligations, six root builds, eight isolated failure
probes and 324 exact raster pairs pass. It retains the original failures and the
MSVC Debug proxy-allocation fix. That historical packet does not qualify later
inline/GFM work, semantic rendering/editing or physical/platform/performance gates.

## Resolved inlines and GFM extensions (M3)

The owned parser now resolves emphasis/strong, code spans, escapes/entities,
inline and global reference links, images, URI/email autolinks, raw HTML and
line breaks. GFM options add tables with cell alignment, task metadata, two-tilde
strike, extended autolinks and tagfilter metadata. The source always stays exact.
Analysis alone retains the v0 native formatted renderer. The explicit semantic
rendering preview described below consumes the resolved document separately.

```cpp
using namespace DxUi::RichText;
MarkdownParser parser;
parser.Begin(L"~~text~~ and [reference]\n\n[reference]: /local\n", 1,
             MarkdownProfile::Gfm029); // All specified GFM extensions by default.
// Call again on a later owner turn if preparation is still pending.
parser.Advance(4096);
if (auto syntax = parser.Published(); !parser.HasPendingPreparation() && syntax)
{
    const auto& inlines = syntax->Inlines();
    // Each leaf's firstInline roots a dense firstChild/nextSibling forest.
    // Semantic values and raw enclosures are not source/display editing maps.
}
```

An optional fourth Begin argument selects `MarkdownExtensions` explicitly.
CommonMark defaults to None and rejects GFM options; Gfm029 and RichMarkdown
default to All. Request identity includes the options. RichMarkdown still awaits
its math/Mermaid/footnote/front-matter stage. Published inline indices belong to
one revision; stable block IDs do not validate reference-dependent semantics.
Reference edits re-resolve links even when the referring paragraph is unchanged.

Values decode ordinary escapes/entities. Code stays literal; semantic HTML uses
the explicit validated native subset below with whole-leaf literal fallback.
Extended autolinks retain their own literal entity/backslash spelling. Unicode
17 full default case folding resolves labels without normalization or OS locale
dependence. ASCII label whitespace, P versus P/S punctuation and inline declaration
rules preserve the requested dialect. Table cells unescape pipes before inlines;
tasks expose state without an interactive checkbox command. Opaque Html values
carry sorted `filteredHtmlOpeners`, pointing to '<' values filtered by GFM.
This metadata is not sanitization or a browser preview API.

The independent conformance test compares all 652 CommonMark and 672 original
GFM expected HTML vectors, including each example's extension flags and both
task vectors marked disabled by the original harness. Nine printed GFM examples
flatten directly nested redundant strong tags; the test serializer preserves
that output convention while the syntax retains every wrapper/source delimiter.
Original standards and expected values are unchanged. Directed core/native/
embedded tests also cover Unicode and profile boundaries, global reference
edit/undo/redo, immutable old trees, option identity and deep iterative forests.
The [sealed M3 qualification](../Measurements/RichText/2026-10-18-markdown-inlines/README.md)
retains 57 fresh complete ordinary x64 scope/profile runs, six root builds,
12 isolated probes, 324 exact raster pairs and full validation/format checks.
Both hosts also verify padded-cell identity across edits/undo/redo. Parser-only
raw NUL label replacement is tested under all three profiles; Document/codecs/
text-service NUL admission stays deferred. ARM64 builds are compile-only.

M4 still owns transformed/generated/object source maps, semantic native layout,
structural editing and structured accessibility. M5 owns the accepted extensions;
M6 and the existing gates own real capacity, physical/platform and performance
acceptance. The current source limit stays 65,536 UTF-16 units. Advance budgets
count operations; construct lookahead, stack resolution, sorting and allocations
can inspect more than one character per step and are not wall-clock guarantees. Generated table padding can amplify columns times body rows; a general tree/storage capacity bound remains M6 work.

## Semantic rendering preview (M4d)

```cpp
auto editor = std::make_unique<DxUi::RichTextEditor>(
    L"# Heading\n\n| left | right |\n| :--- | ---: |\n| **bold** | &amp; |\n");
editor->SetMarkdownAnalysisProfile(DxUi::RichText::MarkdownProfile::Gfm029);
editor->SetMarkdownRenderingEnabled(true);
// Give ownership to the native or embedded ControlHost as usual.
// Its preparation turns parse, build semantics and prepare native layouts.
```

The option requires an analysis profile and defaults off. Source mode always
displays raw text; clearing the profile disables semantic rendering.
`GetMarkdownRenderingStatus()` reports Disabled, Pending, Semantic or
SourceFallback. `GetSemanticDocument()` returns completed current CPU data, which
can be ready before native geometry; neither getter advances preparation.

The preview displays resolved Unicode values, styles, headings, quote/list
indentation, rules, code/image outlines and equal-width table cells with declared
alignment and RTL columns. Images show their alternative; destinations remain
inert. Links are underlined in inherited theme text ink. Validated native HTML
uses the policy below. Selection, composition rectangles and scrolling
use mapped source fragments. Text services and UIA still expose exact source
ACPs, including hidden Markdown and original line endings.

Admission overflow uses exact source geometry once and reports SourceFallback;
it preserves source, requested view, selection and history. A new source/profile
request can retry. The existing source limit remains 65,536 UTF-16 units;
semantic admission adds 4,096 native layouts (including separate marker/body layouts,
cells and caret boxes) and 32,768 elements. These are
preview bounds, not large-document latency or performance qualification.

Empty, trailing and container-only source lines have distinct prepared caret
sites. `MarkdownSyntax::EmptyLines()` exposes their exact raw source/EOL ranges,
insertion ACPs and revision-local container owners. Code-body blank lines reuse
the code layout. Empty/padded table cells retain visual affinity even when they
share the same real row-end ACP. Clicking, navigation and same-source reflow do
not modify source or add undo entries. Hidden/generated source ranges still
have no selection glyph rectangles. Empty physical-line deletion uses raw
Unicode graphemes/whole CRLF; a cell scaffold owns no token to delete.

The [source-caret qualification packet](../Measurements/RichText/2026-10-19-source-caret-sites/README.md)
records the native/embedded scenarios, nine x64 scope passes, three ARM64 builds,
fault/resource observations and actual sample capture. The separate performance
HOLD remains open, including the retained clean-frame diagnostic warning.

List markers are measured with native no-wrap layouts. A shared maximum gutter
aligns 9./10. siblings, wrapped text and loose continuations; nested lists/tables
inherit ancestor gutters. RTL mirrors placement. Marker and body share their first
baseline; block-only items retain their existing standalone marker rows. Marker
ink stays outside source selection ownership and adds no synthetic arrow stop.
Task checkboxes use `Segoe UI Symbol` for both states, avoiding color-emoji
fallback and keeping their bullet column, hit box and text stable when toggled.
This affects owned task glyphs only; body text and list numbers keep their fonts.
The [task-alignment evidence](../Measurements/RichText/2026-10-19-task-alignment/README.md)
includes source/interaction regressions, native captures, profile receipts and
the unresolved performance diagnostic.

The native HTML subset accepts balanced attribute-free b/strong/i/em/u/s/del/code/
span tags, br/br / breaks, and anchors with exactly one quoted href plus optional
quoted title. One outer p/div/h1-h6/pre wrapper is allowed in an HTML block.
References use the parser's pinned decoder; decoded tag-looking text stays text.
Inline HTML preserves Markdown inline semantics. HTML-block text preserves
normalized whitespace and treats Markdown spelling literally. Empty balanced
wrappers have a real interior raw insertion, so typing retains both tag pairs.

Unknown/active tags or attributes, CSS/class/events, malformed/unbalanced/crossed
tags, nested/sibling block wrappers, overlapping Markdown/HTML anchors, more than
32 open tags or 4,096 total tags retain the complete normalized leaf contents in
literal code ink. No valid prefix is partly hidden. This local policy can still
report Semantic; whole-document capacity failures report SourceFallback.
HTML href values allow http/https/mailto and relative/fragment/empty values after
reference decoding, with ASCII spaces/C0/C1 controls, quotes/angle brackets,
backslashes and other schemes rejected. Destinations stay inert: clicks place a
caret, double-clicks select source text, and the control opens no URL/resource.
HTML lists/tables/images and DOM/CSS recovery/layout remain outside this subset.
Their supported Markdown counterparts retain native rendering.

The [native presentation qualification packet](../Measurements/RichText/2026-10-19-native-markup/README.md)
records the M4d.3 native/embedded tests, profile and resource checks, actual sample,
five-theme review and retained diagnostic comparison. The D4c HOLD remains open.

Source-exact structural editing and semantic UIA are described below. Math, Mermaid,
footnotes/front matter and physical IME/AT/platform/performance gates stay open.

Run `.build/x64/Release/DxUi.EmbeddedControls.exe --rich-text` for the current
branch's interactive semantic preview. It uses the sample application's existing
device, host loop and text-service bridge. `--rich-text --output image.png`
captures the same scene without desktop takeover. Consumer pins are unchanged.

## Source-exact structural commands (M4e)

Include `DxUi/RichTextCommands.h` for the reusable Unicode command planner. It
consumes an existing current `MarkdownSyntax` and owns its replacement strings;
it never reparses, advances preparation or calls a host. A plan binds the exact
snapshot, revision, syntax and directional source selection. Apply it once
through the existing atomic source transaction. Untouched spelling and mixed
line endings remain exact; one undo/redo restores the complete original/result,
including backward selections. A stale revision stays stale after undo returns
to the same spelling. Planning, capacity and allocation failures change no source,
selection or history. Identical and navigation-only operations add no history.

```cpp
using namespace DxUi::RichText;
MarkdownCommandRequest request;
request.command = MarkdownCommand::SetHeading;
request.headingLevel = 2;
auto status = editor->ExecuteMarkdownCommand(request);
// Changed, Unchanged, or Unavailable for an incompatible/currently pending target.
```

The retained API requires explicit semantic rendering in Rich view, an editable
control and current published analysis. `ApplyMarkdownCommand(plan)` also checks
the current syntax owner; it does not revive a plan from a replaced publication.
Composition blocks structural commands. Pending analysis never triggers an inline
parse: explicit commands report Unavailable; hardware Enter can still insert a
raw CRLF. Source view, TSF imports and programmatic replacements retain exact raw
replacement semantics. Pure plans have no host binding and are also usable with
`BuildMarkdownCommand` / `ApplyMarkdownCommand(Document&, plan)`.

| Command family | Source behavior |
| --- | --- |
| Lists | Enter continues the owned bullet/ordered marker and padding, increments its ordinal and resets a continued task to `[ ]`. Empty items exit one level. SetList changes only selected sibling markers/tasks; None removes their own list prefixes. Existing ordered numbering is preserved unless listStart is supplied. Indent needs a previous sibling; Outdent removes one proven ancestor indentation. Continuations and child indentation move with changed marker width, including 9 to 10. Untouched siblings are never renumbered. |
| Joins | Backspace/Delete at compatible paragraph boundaries joins plain paragraphs or sibling items with one paragraph each. It removes the owned boundary/prefix and inserts one space. At the first item, Backspace exits a list level. Incompatible nested/opaque/table subtrees are rejected. |
| Headings | SetHeading accepts levels 0 through 6; zero removes heading syntax. Existing ATX marker spelling/closing trivia and setext underlines are edited locally. Converting a multi-line body to ATX adds a heading marker to each affected physical content line. Empty source/container sites can start a heading. A task paragraph cannot simultaneously become an ATX heading. |
| Styles | ToggleStyle supports strong, emphasis, GFM strike and code. It unwraps an enclosing Markdown style or splits an existing style around a partial selection. New non-code styles retain existing syntax and outside whitespace; literal punctuation is escaped when the changed delimiter context would reinterpret it. Code conversion uses resolved Unicode text, chooses a sufficient backtick fence, and escapes punctuation on removal so literal code stays literal. This deliberately reserializes the affected body; untouched text and exact undo remain preserved. Cross-line code wrapping, partial multiline-code removal, images/HTML conversion, clipped nested/atomic tokens and ambiguous neighboring literal delimiters are unavailable. |
| Links | SetLink creates/updates an inert inline link with encoded Unicode metadata; RemoveLink unwraps its label without changing reference definitions. Literal URI/label punctuation is escaped where needed to preserve Unicode values and prevent automatic relinking; entity-looking autolink bytes stay literal when retargeted. A validated native HTML anchor updates only raw href/title values, preserving tag case, quote choice, attribute order and spacing. Removing it deletes only its opening/closing tags; an opening HTML-block anchor is unavailable because unlinking it would reinterpret a literal block body. The existing URI policy applies. No link activation or resource fetch is added. |
| Tasks | ToggleTask changes the checkbox state character, retaining the original marker, spaces and label. Multiple selected existing tasks invert independently; converting mixed items to tasks preserves existing states. CommonMark without TaskLists rejects the command. Pointer press stages a plan; matching release inside the checkbox commits once. Capture/focus loss, leaving the checkbox, source/layout replacement and stale selection cancel the gesture. |
| Tables | InsertTable creates a bounded GFM scaffold at an empty site or over a selected single-line paragraph. Insert/DeleteTableRow and Insert/DeleteTableColumn preserve unaffected cell bytes, escapes, edge pipes, alignment markers and EOLs. Deleting a header promotes the next body row while retaining the separator; deleting the last row/column removes the table. SetTableCell accepts raw single-line Markdown with escaped pipes. Missing padded cells materialize only the necessary delimiters/cells. Next/PreviousTableCell retain separate native affinity at equal ACPs; Tab from the final cell appends one row. |

Table commands admit at most 64 columns, 256 body rows and 4,096 declared cells,
within the existing 64 Ki UTF-16 source cap. `targetCell` is an optional index in
the exact plan syntax, required to distinguish padded cells sharing an ACP.
Creating cells/rows uses the local EOL (then the document's first EOL, then CRLF).
Marker widths use parser tab columns. Transformations that would relocate owned marker tabs across stops are unavailable. Ambiguous partially consumed tabs, missing lazy container prefixes, mixed opaque
selections, incompatible/mixed list targets and invalid metadata are rejected
as a whole. A command is not a general Markdown document reformatter.
Removing a task prefix also rejects a literal label that would become a block
marker. Metadata and deletion fragment requests are bounded before staging.

In semantic Rich view, Enter continues a list or inserts a table row; Shift+Enter
inserts a raw newline. Tab/Shift+Tab first navigate cells or indent/outdent an
eligible list, otherwise the host retains ordinary focus traversal. Ctrl+B/I
toggle styles, Ctrl+Enter toggles tasks, and Ctrl+Z/Y retain transaction history.
Typing a literal pipe/backslash in an empty/padded cell escapes it and uses the
focused cell's real source insertion. Visual deletion removes empty owned
Markdown/validated inline HTML wrappers only when their complete body is deleted;
raw TSF/programmatic replacement does not run cleanup.

The sample toolbar exercises all these families with the same retained API.
At the M4e checkpoint, structured UIA was still pending; M4f is described below.
Extended Markdown, physical IME/AT,
native ARM64 execution, consumer adoption and the D4c performance HOLD remain
separate gates. The [M4e evidence packet](../Measurements/RichText/2026-10-19-structural-commands/README.md)
records 57 ordinary x64 scope/profile passes, all three ARM64 build-only results,
allocation-failure and actual transport/client tests, the reviewed toolbar sample
and gallery, preserved failures and the unresolved performance diagnostic.

## Semantic accessibility (M4f)

The semantic opt-in also supplies native and embedded UIA structure/style. There
is no application-side replacement provider to implement. TSF and TextPattern
continue to expose exact Markdown source, including hidden delimiters and CRLF.
Heading/list/quote/link/image/task/table children provide rendered names and
source-backed structure beside that editable source interface.

Text ranges report typed weight, italic, underline, strike, heading StyleId,
nominal body/monospace font name, point size and read-only attributes. Mixed and
unsupported values use UIA's reserved values. FindAttribute searches exact raw
source in either direction and validates VARIANT types. Format units include
unformatted hidden syntax: expanding inside `**bold &fjlig;**` returns
`bold &fjlig;`, preserving the entity's original spelling. Source/v0 mode keeps
the original unsupported-Format-to-Word behavior; Paragraph/Page still promote
to Document.

RangeFromChild returns a current child's exact source interval. DocumentRange
encloses the editor and enumerates its immediate semantic children; inner ranges
enclose their innermost semantic owner. Padded cells can share an empty raw ACP
while their explicit child ranges/clones retain distinct cell identity. Table
patterns provide rows, columns, cells and column headers. Bounds come from native
prepared geometry with viewport clipping, including RTL/reflow. Links remain
inert and advertise no navigation action. Images expose their Unicode alternative.

Tasks advertise Toggle. Its action changes only the source state character,
preserves directional selection and creates one undo entry. Read-only, disabled,
active composition and pending/stale owners reject the action. Queries never
prepare content or take focus. Native foreign-thread actions use the existing
bounded host dispatch; embedded clients route through the owner's COM apartment.
Committed callbacks may retire the editor safely.

`SemanticDocument::SourceRuns()` is a prepared complete source-format partition;
`Identity()` distinguishes process-local semantic publications. The retained
`GetAcknowledgedSemanticDocument(host)` and `GetSemanticElementBounds(host, index)`
read only current prepared state. `ToggleSemanticTask(host, expected, index)`
uses the same guarded source transaction as UIA. Child providers retain weak
publication identity and the owner binding epoch, so old children cannot act on
an edited, replaced or rebound successor. Reacquire children after a semantic
structure invalidation. Pure CPU semantics may survive geometry-only changes.

The bounded M4f implementation and M4 component closeout are
[functionally qualified](../Measurements/RichText/2026-10-20-semantic-uia/README.md):
all 57 ordinary x64 scope/profile runs, three ARM64 builds, failure/resource
probes, validation and formatting pass. Native/embedded client tests are distinct
from physical Narrator/NVDA/IME and native ARM64 acceptance. This accessibility
change alters no drawing inputs; fresh reviewed rich captures are pixel-identical
to published regions, so existing gallery assets are retained. D4c remains HOLD.

M5 follows the owner's C++-only decision: an explicit limited Mermaid grammar
will be implemented natively. Unsupported diagrams remain exact editable source;
no JavaScript or headless-browser helper is deployed.
