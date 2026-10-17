# Unicode rich text

The additive v0 API lives in `DxUi::RichText` and links through the single
`DxUi.lib`. Include `DxUi/RichDocument.h`, `DxUi/RichMarkdown.h` and
`DxUi/RichTextLayout.h`. The catalog also contains the retained
`DxUi::RichTextEditor` preview described below. Native/embedded OS text services,
rich-text UIA and complete Markdown conformance remain separate
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

This is an explicit bounded v0 preview. It does **not** opt into OS text-input
sessions: native WM_CHAR and embedded DispatchCharacter exercise basic Unicode
entry, while TSF/IME composition, input-language services and rich UIA text/ranges
are D2d. Do not choose it for production accessible or IME editing yet.
There is no incremental large-document layout, complete CommonMark/GFM or
math/Mermaid/footnote/front-matter rendering. Unsupported syntax remains source.
No consumer pin is changed automatically.

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
Accepted BOM and mixed line endings round-trip unchanged.

`SourceSnapshot::IsBoundary` and `Slice(first,last)` inspect shared pieces without
flattening a revision. Slices and edits preserve scalar and CRLF boundaries,
including CR/LF split across two pieces. Edits copy descriptors and the inserted
buffer, not the complete old/new source text. This is a bounded piece vector,
not a large-document indexed tree; snapshot text materialization, projection
and layout still prepare the complete bounded document.

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

Geometry uses DirectWrite UTF-16/logical clusters; full Unicode grapheme and
visual bidi navigation are not qualified. This core supplies no TSF/IME/UIA,
clipboard, file persistence, retained theme/DPI invalidation or viewport scheduler.
The standalone prototype still owns those demonstration-host responsibilities.

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
