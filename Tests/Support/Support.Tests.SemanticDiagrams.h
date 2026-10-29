#pragma once
#include "Controls/DxUi.RichSemanticGeometry.h"
#include "Controls/DxUi.SemanticExtensions.h"
#include "Support.Tests.PreparedFlowchart.h"
#include "Support.Tests.RichSemanticDocument.h"
#include <DxUi/DxUi.h>

namespace SemanticDiagramTests
{
using namespace DxUi;
using namespace DxUi::RichText;
using namespace DxUi::RichText::Detail;
struct Scenario
{
    std::wstring_view source;
    bool diagram{}, diagnostic{};
    SemanticDiagnosticKind issue{};
};
inline constexpr Scenario scenarios[] = {
    {L"```mermaid\ngraph TD;A-->B\n```\n", true},
    {L"~~~mermaid\ngraph LR;A-->|yes|B\n~~~", true},
    {L"> ```mermaid\r\n> graph LR;A-->|yes|B\r\n> ```\r\n", true},
    {L"- ```mermaid\n  graph TD;A-->B\n  ```\n", true},
    {L"ref[^n]\n\n[^n]: body\n    ```mermaid\n    graph TD;A-->B\n    ```\n", true},
    {L"```mermaid\ngraph BT;A[\"Unicode \u03b2 \U0001f600\"]-->B;B-->A;A-->A\n```", true},
    {L"```Mermaid\ngraph TD;A-->B\n```\n"},
    {L"```mermaidx\ngraph TD;A-->B\n```\n"},
    {L"```mermaid {theme:default}\ngraph TD;A-->B\n```\n", false, true, SemanticDiagnosticKind::UnsupportedDiagram},
    {L"```mermaid\nsequenceDiagram\nA->>B: hello\n```\n", false, true, SemanticDiagnosticKind::UnsupportedDiagram},
    {L"```mermaid\ngraph TD;A-->\n```\n", false, true, SemanticDiagnosticKind::MalformedDiagram},
    {L"```mermaid\ngraph TD;\U0001f600-->A\n```\n", false, true, SemanticDiagnosticKind::MalformedDiagram},
    {L"```mermaid\ngraph TD\n```\n", false, true, SemanticDiagnosticKind::EmptyDiagram},
    {L"```mermaid\ngraph TD;A-->B\n", false, true, SemanticDiagnosticKind::UnclosedDiagram},
    {L"```mermaid\n%%{init: {}}%%\ngraph TD;A\n```\n", false, true, SemanticDiagnosticKind::UnsupportedDiagram},
};
template <typename Check> inline std::shared_ptr<const SemanticDocument> Complete(SemanticBuilder& builder, Check&& check)
{
    std::size_t turns{};
    while (builder.HasPendingPreparation())
    {
        const auto step = builder.Advance(7u);
        check(step.workUnits > 0u && step.workUnits <= 7u, "native semantic work respects each owner event allowance");
        check(++turns < 200000u, "native semantic preparation remains finite");
    }
    return builder.Published();
}
template <typename Check>
inline std::shared_ptr<const SemanticDocument> Build(std::wstring_view source, Check&& check, MarkdownProfile profile = MarkdownProfile::RichMarkdown)
{
    const auto syntax = RichSemanticDocumentTests::Parse(source, profile);
    SemanticBuilder builder;
    builder.Begin(syntax);
    return Complete(builder, check);
}
template <typename Check> inline void Core(Check&& check)
{
    for (const auto& scenario : scenarios)
    {
        const auto doc = Build(scenario.source, check);
        check(doc->Map().Source() == scenario.source, "native semantics retain the exact original source and EOLs");
        const auto diagrams = SemanticExtensionAccess::Diagrams(*doc);
        check(diagrams.size() == (scenario.diagram ? 1u : 0u) && doc->Diagnostics().size() == (scenario.diagnostic ? 1u : 0u),
              "native object/fallback classification matches independent expectations");
        if (scenario.diagram)
        {
            const auto& native  = diagrams.front();
            const auto& element = doc->Elements()[native.element];
            check(element.role == SemanticRole::Diagram && native.display.last - native.display.first == 1u &&
                      doc->Map().Display()[native.display.first] == L'\ufffc',
                  "prepared diagrams have one source-owning replacement character");
            check(element.name.find(L"A") != std::wstring::npos && element.name.find(L"Connections:") != std::wstring::npos,
                  "diagram alternative exposes node identities and graph relationships");
            check(native.bodyUnits.size() == native.graph->source.size(), "every normalized body unit retains original-source provenance");
            for (const auto& node : native.graph->nodes)
            {
                const auto raw = native.SourceRange(node.identifier);
                check(raw && doc->Map().Source().substr(raw->first, raw->last - raw->first) ==
                                 std::wstring_view(native.graph->source).substr(node.identifier.first, node.identifier.last - node.identifier.first),
                      "node IDs map through container prefixes and CRLF to their exact raw spelling");
            }
            for (auto source = element.source.first; source < element.source.last; ++source)
                if (IsEditBoundary(doc->Map().Source(), source))
                    check(doc->Map().ToDisplay(source, MappingAffinity::Leading) == native.display.first,
                          "hidden raw fence positions retain atomic primary editing affinity");
        }
        if (scenario.diagnostic)
        {
            const auto& diagnostic = doc->Diagnostics().front();
            check(diagnostic.kind == scenario.issue && doc->Elements()[diagnostic.element].role == SemanticRole::Code &&
                      ! doc->Elements()[diagnostic.element].title.empty(),
                  "literal fallback retains its explicit typed diagnostic and accessible explanation");
            check(IsEditBoundary(doc->Map().Source(), diagnostic.source.first) && IsEditBoundary(doc->Map().Source(), diagnostic.source.last),
                  "native diagnostics never split Unicode scalars or original CRLF pairs");
            if (scenario.source.find(L"\U0001f600") != std::wstring_view::npos)
                check(doc->Map().Source().substr(diagnostic.source.first, diagnostic.source.last - diagnostic.source.first) == L"\U0001f600",
                      "invalid non-ASCII identifier selects the complete supplementary scalar");
        }
    }
    for (const auto profile : {MarkdownProfile::CommonMark0312, MarkdownProfile::Gfm029})
    {
        const auto doc = Build(scenarios[0u].source, check, profile);
        check(SemanticExtensionAccess::Diagrams(*doc).empty() && doc->Diagnostics().empty() && doc->Elements()[1u].role == SemanticRole::Code,
              "pinned base profiles do not acquire Mermaid semantics");
    }
    std::wstring many;
    for (std::size_t index = 0u; index <= SemanticExtensions::MaximumObjects; ++index)
        many += scenarios[0u].source;
    const auto limited = Build(many, check);
    check(SemanticExtensionAccess::Diagrams(*limited).size() == SemanticExtensions::MaximumObjects && limited->Diagnostics().size() == 1u &&
              limited->Diagnostics().front().kind == SemanticDiagnosticKind::DiagramCapacity,
          "document object admission occurs before preparing the next graph");
    const auto large = Build(L"```mermaid\n" + std::wstring(NativeFlowchartParser::MaximumSourceUnits + 1u, L' ') + L"\n```\n", check);
    check(SemanticExtensionAccess::Diagrams(*large).empty() && large->Diagnostics().size() == 1u &&
              large->Diagnostics().front().kind == SemanticDiagnosticKind::DiagramCapacity,
          "oversized graph bodies retain source without copying a parser model");
    SemanticBuilder builder;
    const auto oldSyntax = RichSemanticDocumentTests::Parse(L"old", MarkdownProfile::RichMarkdown);
    builder.Begin(oldSyntax);
    const auto old  = Complete(builder, check);
    const auto next = RichSemanticDocumentTests::Parse(L"```mermaid\ngraph TD;A[\"" + std::wstring(1000u, L'x') + L"\"]\n```\n", MarkdownProfile::RichMarkdown);
    builder.Begin(next);
    builder.Advance(8u);
    check(builder.HasPendingPreparation() && builder.Published() == old, "partial native parsing cannot publish a half-prepared semantic graph");
    builder.Cancel();
    check(builder.Published() == old && old->Map().Source() == L"old", "native cancellation preserves held immutable semantics");
    builder.Begin(next);
    builder.Advance(4u);
    builder.Begin(oldSyntax);
    check(! builder.HasPendingPreparation() && builder.Published() == old, "replacement with the current publication rejects stale native completion");
}
template <typename Check> inline void Geometry(Check&& check)
{
    const auto real    = PreparedFlowchartTests::WriteFactory();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    const auto calls   = Microsoft::WRL::Make<NativeTextTests::CountingWriteFactory>(real.get());
    if (! calls)
        throw std::bad_alloc();
    for (const auto& scenario : scenarios)
    {
        if (! scenario.diagram)
            continue;
        const auto doc = Build(scenario.source, check);
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            LayoutOptions options;
            options.widthDip         = 1024.0f;
            options.readingDirection = direction;
            PreparedSemanticGeometry geometry;
            geometry.Begin(calls.Get(), doc, options, drawing.get(), {1u, 1u});
            std::size_t turns{};
            while (geometry.Pending())
            {
                const auto before = calls->layouts;
                const auto step   = geometry.Advance({1u, 3u});
                check(step.layoutsPrepared <= 1u && step.blockSteps <= 3u && calls->layouts - before == step.layoutsPrepared,
                      "object labels and the positioning paragraph share the same real SDK budget");
                check(++turns < 20000u, "native paragraph object preparation makes finite progress");
            }
            check(geometry.Ready() && geometry.HasNativeObjects(), "complete native objects acknowledge source-matched geometry");
            for (const auto& native : SemanticExtensionAccess::Diagrams(*doc))
            {
                const auto& element = doc->Elements()[native.element];
                const auto bounds   = geometry.ElementBounds(native.element);
                check(bounds && bounds->right > bounds->left && bounds->bottom > bounds->top && ! geometry.SourceRange(element.source).empty(),
                      "an exact raw fence range exposes the measured native object rectangle");
                for (auto at = element.source.first; at < element.source.last; ++at)
                    if (IsEditBoundary(doc->Map().Source(), at))
                        check(geometry.SourceCaret(at).height <= options.fontSizeDip + 0.01f,
                              "opaque object caret height follows text instead of the full graphic");
            }
            std::size_t source{};
            for (const auto& line : geometry.SourceLines())
            {
                check(line.first == source && line.second >= line.first, "native visual lines remain an ordered source partition");
                source = line.second;
            }
            check(source == doc->Map().Source().size(), "native visual line units cover every original source unit");
        }
    }
}
template <typename Check> inline void PositionedRaster(Check&& check)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    check(SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE, "native paragraph raster imaging apartment");
    const auto apartment = wil::scope_exit([&]() noexcept
    {
        if (SUCCEEDED(initialized))
            CoUninitialize();
    });
    wil::com_ptr<IWICImagingFactory> imaging;
    THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imaging.put())));
    const auto write   = PreparedFlowchartTests::WriteFactory();
    const auto drawing = PreparedFlowchartTests::DrawingFactory();
    for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
    {
        const auto doc = Build(L"```mermaid\ngraph TD;A[\"Unicode \\n\u03b2\"]-->|yes|B;B-->A\n```\n", check);
        LayoutOptions options;
        options.widthDip         = 480.0f;
        options.readingDirection = direction;
        PreparedSemanticGeometry positioned;
        positioned.Begin(write.get(), doc, options, drawing.get(), {1u, 1u});
        while (positioned.Pending())
            positioned.Advance({1u, 3u});
        const auto& object = SemanticExtensionAccess::Diagrams(*doc).front();
        const auto bounds  = positioned.ElementBounds(object.element);
        check(bounds.has_value(), "native source object bounds");
        PreparedFlowchart reference;
        reference.Begin(write.get(), drawing.get(), object.graph, options, {1u, 1u});
        PreparedFlowchartTests::Complete(reference, check);
        for (const auto dpi : {96.0f, 144.0f, 192.0f})
        {
            const auto render = [&](bool direct)
            {
                constexpr UINT width = 1024u, height = 700u, stride = width * 4u;
                wil::com_ptr<IWICBitmap> bitmap;
                THROW_IF_FAILED(imaging->CreateBitmap(width, height, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, bitmap.put()));
                wil::com_ptr<ID2D1RenderTarget> target;
                THROW_IF_FAILED(drawing->CreateWicBitmapRenderTarget(
                    bitmap.get(),
                    D2D1::RenderTargetProperties(
                        D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), dpi, dpi),
                    target.put()));
                target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                wil::com_ptr<ID2D1SolidColorBrush> text, stroke, fill;
                THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.1f, .1f, .1f), text.put()));
                THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.2f, .4f, .7f), stroke.put()));
                THROW_IF_FAILED(target->CreateSolidColorBrush(D2D1::ColorF(.9f, .95f, 1.f), fill.put()));
                target->BeginDraw();
                target->Clear(D2D1::ColorF(D2D1::ColorF::White));
                if (direct)
                    check(reference.Draw(target.get(), text.get(), stroke.get(), fill.get(), D2D1::Point2F(7.f + bounds->left, 5.f + bounds->top)),
                          "direct native reference raster");
                else
                    positioned.Draw(target.get(), text.get(), stroke.get(), {7.f, 5.f}, 0.f, 600.f, fill.get());
                THROW_IF_FAILED(target->EndDraw());
                std::vector<BYTE> bytes(stride * height);
                THROW_IF_FAILED(bitmap->CopyPixels(nullptr, stride, static_cast<UINT>(bytes.size()), bytes.data()));
                return std::vector<BYTE>(bytes);
            };
            // The direct service already has independent SDK shape/label oracles.
            // This comparison isolates paragraph positioning and proves that the
            // reserved inline object paints neither a glyph nor an extra frame.
            check(render(false) == render(true), "source-backed object pixels exactly match direct native painting at each DPI and text direction");
        }
    }
}

inline constexpr std::wstring_view SelectionSource = L"```mermaid\ngraph TD;A[\"\"]\n```\n";
inline ThemePalette SelectionPalette()
{
    auto palette         = MakeDefaultThemePalette(false);
    palette.highContrast = palette.reducedMotion = true;
    palette.inputFill                            = D2D1::ColorF(1.f, 1.f, 1.f, 1.f);
    palette.selectionFill = palette.selectionInactiveFill = D2D1::ColorF(0.f, .5f, 0.f, 1.f);
    palette.selectionText                                 = D2D1::ColorF(1.f, 1.f, 1.f, 1.f);
    return palette;
}
template <typename Check>
inline void SelectionPixels(RichTextEditor& editor, ControlHost& host, std::span<const uint8_t> pixels, UINT width, UINT height, float scale, Check&& check)
{
    const auto doc = editor.GetAcknowledgedSemanticDocument(host);
    check(doc && ! SemanticExtensionAccess::Diagrams(*doc).empty(), "selected native object remains acknowledged");
    const auto bounds = editor.GetSemanticElementBounds(host, SemanticExtensionAccess::Diagrams(*doc).front().element);
    check(bounds.has_value(), "selected native object retains measured bounds");
    // Empty-label rectangle: 8 DIP graph margin, then a point four DIP inside
    // the first node. Neither glyph ink nor antialiased borders cross it.
    const auto x = static_cast<UINT>((bounds->left + 12.f) * scale), y = static_cast<UINT>((bounds->top + 12.f) * scale);
    check(x < width && y < height && pixels.size() == static_cast<std::size_t>(width) * height * 4u, "native selection pixel is inside its actual capture");
    const auto at = (static_cast<std::size_t>(y) * width + x) * 4u;
    check(pixels[at] == 0u && pixels[at + 1u] >= 127u && pixels[at + 1u] <= 128u && pixels[at + 2u] == 0u && pixels[at + 3u] == 255u,
          "selected native node interiors preserve the opaque high-contrast selection fill instead of covering it with the input surface");
    check(editor.GetText() == SelectionSource && editor.GetSelection() == Selection{0u, SelectionSource.size()} && ! editor.CanUndo(),
          "diagram selection changes no source or history");
}

template <typename Prepare, typename Check> inline void Editor(RichTextEditor& editor, ControlHost& host, Prepare&& prepare, Check&& check)
{
    const std::wstring source(scenarios[0u].source);
    editor.SetMarkdownAnalysisProfile(MarkdownProfile::RichMarkdown);
    editor.SetMarkdownRenderingEnabled(true);
    editor.SetText(source);
    prepare(480u);
    check(editor.GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor.GetAcknowledgedSemanticDocument(host),
          "retained native diagram geometry is acknowledged in both hosts");
    editor.SetSelection({1u, 2u});
    prepare(50u);
    const auto localFallback = [&]()
    {
        const auto doc = editor.GetAcknowledgedSemanticDocument(host);
        return editor.GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && doc && doc->Map().Source() == source &&
               SemanticExtensionAccess::Diagrams(*doc).empty() && doc->Diagnostics().size() == 1u &&
               doc->Diagnostics()[0u].kind == SemanticDiagnosticKind::DiagramCapacity && doc->Diagnostics()[0u].element < doc->Elements().size() &&
               doc->Elements()[doc->Diagnostics()[0u].element].role == SemanticRole::Code && doc->Map().Display() == source;
    };
    check(localFallback() && editor.GetText() == source && editor.GetSelection() == Selection{1u, 2u} && ! editor.CanUndo(),
          "view-capacity fallback is local to its exact diagram source and preserves selection and history");
    check(editor.TryGetTextInputCaretRect(host, 1u).has_value(), "local source fallback has usable raw-source caret geometry");
    const auto idle = editor.DebugGetState();
    for (unsigned turn = 0u; turn < 16u; ++turn)
        prepare(50u);
    const auto after = editor.DebugGetState();
    check(after.preparationAdvances == idle.preparationAdvances && after.markdownPreparationAdvances == idle.markdownPreparationAdvances &&
              after.semanticPreparationAdvances == idle.semanticPreparationAdvances,
          "identical rejected native view requests add no idle parse or layout work");
    const auto raw = source.find(L'A');
    auto erased    = source;
    erased.erase(raw, 1u);
    for (const auto command : {0u, 1u, 2u})
    {
        editor.SetSelection(command == 0u ? Selection{raw, raw} : command == 1u ? Selection{raw + 1u, raw + 1u} : Selection{raw, raw + 1u});
        check(editor.OnKeyDown(host, command == 1u ? VK_BACK : VK_DELETE, 0u) && editor.GetText() == erased,
              "source fallback Delete/Backspace/selection deletion removes only the visible raw source instead of the complete native object");
        check(editor.Undo() && editor.GetText() == source, "fallback deletion is one reversible exact-source edit");
        prepare(50u);
        check(localFallback(), "undo retries exact-view capacity without losing local source fallback");
    }
    prepare(480u);
    check(editor.GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic && editor.GetText() == source,
          "width-key change recovers a valid graph without editing its syntax");
    const auto first = source.find(L'A');
    editor.SetSelection({first, first + 1u});
    check(editor.ReplaceSelection(L"Node") && editor.Undo() && editor.GetText() == source && editor.Redo(),
          "diagram source edits use the same Unicode transaction and undo/redo history");
    prepare(480u);
    editor.SetViewMode(ViewMode::Source);
    prepare(480u);
    check(editor.GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Disabled, "explicit source view retains its normal presentation status");
    editor.SetViewMode(ViewMode::Rich);
    prepare(480u);
    check(editor.GetMarkdownRenderingStatus() == MarkdownRenderingStatus::Semantic, "returning to rich view prepares the edited graph");
}
} // namespace SemanticDiagramTests
