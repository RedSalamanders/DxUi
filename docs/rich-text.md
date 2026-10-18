# Unicode rich text

The additive v0 API lives in `DxUi::RichText` and links through the single
`DxUi.lib`. Include `DxUi/RichDocument.h`, `DxUi/RichMarkdown.h` and
`DxUi/RichTextLayout.h`. The catalog also contains the retained
`DxUi::RichTextEditor` preview described below. Its bounded source TSF/IME and
UIA adapters are implemented in both hosts. Real IME/assistive-technology
qualification and complete Markdown conformance remain separate
integration gates in the [active plan](../Specs/Plans/WIP/RichTextEditor_2026-10-17.md).

## Retained editor preview (D2c)

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
text attributes and annotations are unsupported; structured Markdown semantics,
font/style attributes and rendered-only accessible text remain future scope.
The bounded source range commands include scrolling, selection union/subtraction,
text search and uniform read-only attribute search. The rules below apply in both
hosts; rich structural/style attributes and semantic text anchors remain separate.

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
source text, enclosing element, visible bounds and empty embedded children are
covered in both hosts. `Line` movement uses the prepared wrapped rich projection
through `GetSourceVisualLineRanges`, including exact source CRLF and hidden
syntax. Unsupported Format units promote to Word; Paragraph/Page promote to
Document. Invalid units/endpoints/lengths are rejected. Character commands now
use shared Unicode 17 default extended grapheme rules. Visual bidi/word
navigation and rich style/structure semantics remain separate work.

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
buffers and can be handed to an application/shared preparation worker. The core
creates no worker, queue, HWND, graphics device, swap chain, timer or file. The
caller owns cancellation, request/revision keys, scheduling, stale publication
checks and view lifetime. `PreparedLayout` is also owner-thread mutable; do not
prepare or query it concurrently. Public operations can throw standard argument,
range, length/allocation exceptions, or WIL HRESULT exceptions for DirectWrite
failures. Catch at the caller's Windows/COM/callback boundary.

`Selection` stores anchor/caret UTF-16 offsets and preserves backwards direction.
Replacements are one source/selection transaction. Undo/redo restores snapshots
and selections while revisions increase. A same-content replacement only changes
selection and preserves history/revision. Invalid source, ranges or resulting
selection leave source, selection, revision and history unchanged. Reset clears
history and selection. The v0 bounds are 64 Ki UTF-16 units and 128 history items.
There is no ANSI API; UTF-8 conversion rejects malformed input and embedded NUL.
It rejects impossible byte lengths and decoded lengths above capacity before
allocating the UTF-16 result. Exact three-byte BMP and supplementary-scalar
capacity limits remain accepted. Accepted BOM and mixed line endings round-trip unchanged.

`SourceSnapshot::IsBoundary` and `Slice(first,last)` inspect shared pieces without
flattening a revision. Slices and edits preserve scalar and CRLF boundaries,
including CR/LF split across two pieces. Cumulative source ends reuse the existing
piece descriptors for binary-search offset lookup; a slice starts at its first
intersecting piece. This adds no separate prefix array or per-piece field.
Captured snapshots support concurrent immutable reads while the owner changes
its document. The Document itself remains owner-thread mutable. Edits copy
descriptors and the inserted buffer; snapshot text materialization, projection
and layout still prepare the complete bounded document. The capacity is unchanged;
incremental layout and large-document storage remain pending.

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
are retained. The performance hold, persistent storage/incremental preparation,
remaining engine and manual acceptance gates continue under the plan.
