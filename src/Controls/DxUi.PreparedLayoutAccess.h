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
    };
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
    static bool ScriptTargetSupported(const PreparedLayout& owner, ID2D1RenderTarget* target);
    static void PrepareSemantic(PreparedLayout& owner,
                                IDWriteFactory* factory,
                                SemanticInput& input,
                                const LayoutOptions& options,
                                bool marker                           = false,
                                IDWriteInlineObject* object           = nullptr,
                                UINT32 objectOffset                   = 0u,
                                std::span<const InlineObject> objects = {});
    static void Prepare(PreparedLayout& owner,
                        IDWriteFactory* factory,
                        Projection& projection,
                        const LayoutOptions& options,
                        float verticalOrigin                  = 0.0f,
                        bool semanticDisplay                  = false,
                        bool marker                           = false,
                        IDWriteInlineObject* object           = nullptr,
                        UINT32 objectOffset                   = 0u,
                        std::span<const InlineObject> objects = {});
};
} // namespace DxUi::RichText::Detail
