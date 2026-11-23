#pragma once
#include <DxUi/RichTextLayout.h>
namespace DxUi::RichText::Detail
{
struct PreparedLayoutAccess
{
    struct InlineObject
    {
        IDWriteInlineObject* object{};
        UINT32 first{}, length{};
    };
    struct SemanticInput
    {
        SemanticInput() : text(0u, L' '), runs(0u)
        {
        }
        std::wstring text;
        std::vector<StyleRun> runs;
        std::uint64_t revision{};
        bool padInlineCode{};
    };
    // The inputs of uniform line spacing (an explicit line height, or any superscript/subscript run) taken over a whole
    // document: the largest run font size and whether any script run exists. A v0 document chunk passes the document's
    // value so that every chunk spaces its lines exactly as one whole layout of the document would.
    struct DocumentLineSpacing
    {
        float size{};
        bool scripts{};
        bool operator==(const DocumentLineSpacing&) const noexcept = default;
    };
    static void AccumulateLineSpacing(DocumentLineSpacing& spacing, std::span<const StyleRun> runs, const LayoutOptions& options) noexcept;
    static std::span<const DWRITE_LINE_METRICS> LineMetrics(const PreparedLayout& owner)
    {
        return owner.LineMetrics();
    }
    static std::pair<float, float> HorizontalTextBounds(const PreparedLayout& owner)
    {
        return owner.HorizontalTextBounds();
    }
    static std::pair<float, float> DrawingVerticalBounds(const PreparedLayout& owner)
    {
        return owner.DrawingVerticalBounds();
    }
    static float MinimumTextWidth(const PreparedLayout& owner);
    static bool ScriptTargetSupported(const PreparedLayout& owner, bool colorTargetAvailable) noexcept;
    static HRESULT PrepareSemantic(PreparedLayout& owner,
                                   IDWriteFactory* factory,
                                   SemanticInput& input,
                                   const LayoutOptions& options,
                                   bool marker                           = false,
                                   IDWriteInlineObject* object           = nullptr,
                                   UINT32 objectOffset                   = 0u,
                                   std::span<const InlineObject> objects = {});
    static HRESULT Prepare(PreparedLayout& owner,
                           IDWriteFactory* factory,
                           Projection& projection,
                           const LayoutOptions& options,
                           bool semanticDisplay                       = false,
                           bool marker                                = false,
                           IDWriteInlineObject* object                = nullptr,
                           UINT32 objectOffset                        = 0u,
                           std::span<const InlineObject> objects      = {},
                           bool padInlineCode                         = false,
                           const DocumentLineSpacing* documentSpacing = nullptr);
};
} // namespace DxUi::RichText::Detail
