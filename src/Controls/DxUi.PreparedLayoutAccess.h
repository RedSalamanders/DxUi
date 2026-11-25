#pragma once
#include <DxUi/Configuration.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <span>
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
    // W7.6. Whether every line's caret box (TryCaret at the line's first offset) is known to be stacked, its top and bottom
    // never decreasing from one line to the next: decided when the layout is prepared (argument at TryVisibleRange).
    static bool CaretBoxesStacked(const PreparedLayout& owner) noexcept;
    // The offset each line starts at, and the text size after them.
    static std::span<const std::size_t> LineStarts(const PreparedLayout& owner);
    // The line whose sum of line heights above it is the last at or above y: where to start looking for y.
    static std::size_t LineHint(const PreparedLayout& owner, float y) noexcept;
    // The first index in [0, count] where reached(index) holds, for a predicate that is false and then true (count when
    // it never holds): looks at `hint` first and widens from it by doubling steps before halving, so that the calls
    // grow with the distance from the hint, not with the index. Stops at the first failure written to `failure`.
    template <typename Reached> static std::size_t FirstLineWhere(std::size_t count, std::size_t hint, const Reached& reached, const HRESULT& failure)
    {
        hint            = std::min(hint, count);
        std::size_t low = 0u, high = count;
        if (hint < count && reached(hint))
        {
            high = hint;
            for (std::size_t step = 1u; ! FAILED(failure) && high > 0u; step *= 2u)
            {
                const auto probe = high > step ? high - step : 0u;
                if (! reached(probe))
                {
                    low = probe + 1u;
                    break;
                }
                high = probe;
            }
        }
        else if (hint < count)
        {
            low = hint + 1u;
            for (std::size_t step = 1u; ! FAILED(failure) && low < count; step *= 2u)
            {
                const auto probe = std::min(low + step - 1u, count - 1u);
                if (reached(probe))
                {
                    high = probe;
                    break;
                }
                low = probe + 1u;
            }
        }
        while (low < high && ! FAILED(failure))
        {
            const auto middle = low + (high - low) / 2u;
            if (reached(middle))
                high = middle;
            else
                low = middle + 1u;
        }
        return low;
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
#if DXUI_ENABLE_DIAGNOSTICS
// W7.6 negative control: while set, TryVisibleRange searches whatever the layout, as it must not for a layout whose caret
// boxes are not known to be stacked.
extern thread_local bool testRichForceVisibleRangeSearch;
#endif
} // namespace DxUi::RichText::Detail
