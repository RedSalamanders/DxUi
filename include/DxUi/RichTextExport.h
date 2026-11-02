#pragma once
#include <DxUi/RichSemanticDocument.h>
#include <d2d1.h>
#include <string_view>

namespace DxUi::RichText
{
// Immutable owned export, prepared explicitly outside Paint. HTML contains
// only library-authored structure/styles: literal HTML is escaped, images use
// alternatives, and math/diagrams use readable source alternatives. It loads
// no resources and executes no document-authored HTML or script.
class ExportContent final
{
public:
    static constexpr std::size_t MaximumUnits      = 4u * 1024u * 1024u;
    ExportContent(const ExportContent&)            = default;
    ExportContent& operator=(const ExportContent&) = default;
    std::wstring_view Text() const noexcept
    {
        return text_;
    }
    std::wstring_view HtmlFragment() const noexcept
    {
        return html_;
    }

private:
    ExportContent(std::wstring&& text, std::wstring&& html) : text_(0u, L' '), html_(0u, L' ')
    {
        text_.swap(text);
        html_.swap(html);
    }
    std::wstring text_, html_;
    friend ExportContent ExportSemanticDocument(const SemanticDocument& document);
    friend ExportContent ExportSemanticDocument(const SemanticDocument& document, MarkdownRange displayRange);
};
// Exports one immutable semantic publication. Generated list/footnote text is
// retained in plain text; structural HTML uses semantic containers. Unsupported
// source remains escaped literal text. Each output is bounded to 4 Mi UTF-16
// units and failure publishes no partial ExportContent. No I/O or callbacks.
ExportContent ExportSemanticDocument(const SemanticDocument& document);
// Export a half-open display range from the same immutable publication. Bounds
// must preserve UTF-16 scalars and CRLF; surrounding structure is retained.
// A range cutting through a generated list prefix uses escaped preformatted
// selected text, preserving the partial prefix exactly once.
ExportContent ExportSemanticDocument(const SemanticDocument& document, MarkdownRange displayRange);
// Builds a semantic projection with default presentation from one immutable
// syntax publication and exports it. This is synchronous; run outside Paint.
ExportContent ExportMarkdownSyntax(std::shared_ptr<const MarkdownSyntax> syntax);

// Borrowed target-compatible brushes. The application owns the page surface,
// BeginDraw/EndDraw, transforms and any print/PDF transport. Null optional
// brushes inherit ordinary ink or omit fills; this adds no drawing resources.
struct ExportDrawing
{
    ID2D1Brush* text           = nullptr;
    ID2D1Brush* decoration     = nullptr;
    ID2D1Brush* fill           = nullptr;
    ID2D1Brush* quoteFill      = nullptr;
    ID2D1Brush* link           = nullptr;
    ID2D1Brush* markedTextFill = nullptr;
    bool underlineLinks        = true;
    bool outlineMarkedText     = false;
    // Opt in to prepared author diagram and math colors after contrast checks against
    // these page colors. Default is caller-supplied monochrome brushes.
    bool preserveAuthorColors = false;
    D2D1_COLOR_F pageText{}, pageStroke{}, pageFill{};
};
} // namespace DxUi::RichText
