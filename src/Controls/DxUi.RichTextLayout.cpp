#include "DxUi.TextElements.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <wil/result.h>
namespace DxUi::RichText
{
namespace
{
void Check(HRESULT result, const char* operation)
{
    if (FAILED(result))
        THROW_HR_MSG(result, "%hs", operation);
}
} // namespace
struct PreparedLayout::State
{
    State()
        : retirement{std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)},
          clusters(0u),
          lines(0u),
          lineMetrics(0u)
    {
    }
    Projection retirement;
    wil::com_ptr<IDWriteTextLayout> layout;
    std::vector<std::size_t> clusters, lines;
    std::vector<DWRITE_LINE_METRICS> lineMetrics;
    float height{};
    float drawingTop{}, drawingBottom{};
};
PreparedLayout::PreparedLayout()
    : projection_{std::wstring(0u, L'\0'), std::wstring(0u, L'\0'), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)}
{
    // Size constructors propagate Debug STL proxy failures. Retirement never
    // default-constructs containers inside a noexcept callback.
}
PreparedLayout::~PreparedLayout() noexcept = default;
PreparedLayout::PreparedLayout(const PreparedLayout& other) : PreparedLayout()
{
    *this = other;
}
PreparedLayout& PreparedLayout::operator=(const PreparedLayout& other)
{
    if (this != &other)
    {
        auto next = other.state_ ? std::make_unique<State>(*other.state_) : nullptr;
        Projection projection(other.projection_);
        projection_ = std::move(projection);
        state_      = std::move(next);
        generation_ = other.generation_;
    }
    return *this;
}
PreparedLayout::PreparedLayout(PreparedLayout&& other) : PreparedLayout()
{
    *this = std::move(other);
}
PreparedLayout& PreparedLayout::operator=(PreparedLayout&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        projection_.source.swap(other.projection_.source);
        projection_.text.swap(other.projection_.text);
        projection_.sourceToDisplay.swap(other.projection_.sourceToDisplay);
        projection_.units.swap(other.projection_.units);
        projection_.runs.swap(other.projection_.runs);
        std::swap(projection_.revision, other.projection_.revision);
        std::swap(projection_.mode, other.projection_.mode);
        state_      = std::move(other.state_);
        generation_ = other.generation_;
    }
    return *this;
}

float PreparedLayout::Height() const noexcept
{
    return state_ ? state_->height : 0.0f;
}
void PreparedLayout::Prepare(IDWriteFactory* factory, Projection projection, float width, float fontSize)
{
    LayoutOptions options;
    options.widthDip    = width;
    options.fontSizeDip = fontSize;
    PrepareOwned(factory, projection, options);
}
void PreparedLayout::Prepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options)
{
    PrepareOwned(factory, projection, options);
}
void PreparedLayout::PrepareOwned(IDWriteFactory* factory, Projection& projection, const LayoutOptions& options)
{
    const auto width = options.widthDip, fontSize = options.fontSizeDip;
    const auto validName = [](const std::wstring& name) { return ! name.empty() && name.size() <= 255 && IsValidUtf16(name); };
    if (! factory || ! std::isfinite(width) || width <= 0 || ! std::isfinite(fontSize) || fontSize <= 0 || ! validName(options.fontFamily) ||
        ! validName(options.codeFontFamily) || options.locale.size() > 255 || ! IsValidUtf16(options.locale) ||
        (options.readingDirection != DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && options.readingDirection != DWRITE_READING_DIRECTION_RIGHT_TO_LEFT))
        throw std::invalid_argument("Invalid rich-text layout options");
    if (projection.text.size() > Document::MaximumUnits || ! IsValidUtf16(projection.text) || projection.source.size() > Document::MaximumUnits ||
        ! IsValidUtf16(projection.source) || projection.units.size() != projection.text.size() ||
        projection.sourceToDisplay.size() != projection.source.size() + 1 || (projection.mode != ViewMode::Source && projection.mode != ViewMode::Rich))
        throw std::invalid_argument("Invalid rich-text display text");
    std::size_t previousDisplay = 0, previousSource = 0;
    for (const auto offset : projection.sourceToDisplay)
    {
        if (offset < previousDisplay || offset > projection.text.size())
            throw std::invalid_argument("Invalid rich-text source mapping");
        previousDisplay = offset;
    }
    for (const auto& unit : projection.units)
    {
        if (unit.start < previousSource || unit.start >= unit.end || unit.end > projection.source.size())
            throw std::invalid_argument("Invalid rich-text display mapping");
        previousSource = unit.end;
    }
    for (const auto& run : projection.runs)
    {
        if (run.start > projection.text.size() || run.length > projection.text.size() - run.start || run.heading > 6 ||
            (run.flags & ~(Bold | Italic | Code | Strike)) || ! IsEditBoundary(projection.text, run.start) ||
            ! IsEditBoundary(projection.text, run.start + run.length))
            throw std::invalid_argument("Invalid rich-text style range");
    }
    // Office TxDrawD2D (RVA 0x15db90) adapts a supplied target to a renderer.
    // This owned implementation uses public DirectWrite styles and metrics;
    // it does not call the private Office/LineServices/PTS rendering interfaces.
    wil::com_ptr<IDWriteTextFormat> format;
    Check(factory->CreateTextFormat(options.fontFamily.c_str(),
                                    nullptr,
                                    DWRITE_FONT_WEIGHT_NORMAL,
                                    DWRITE_FONT_STYLE_NORMAL,
                                    DWRITE_FONT_STRETCH_NORMAL,
                                    fontSize,
                                    options.locale.c_str(),
                                    format.put()),
          "CreateTextFormat");
    Check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP), "SetWordWrapping");
    Check(format->SetReadingDirection(options.readingDirection), "SetReadingDirection");
    wil::com_ptr<IDWriteTextLayout> layout;
    Check(factory->CreateTextLayout(projection.text.data(), static_cast<UINT32>(projection.text.size()), format.get(), width, 1000000.0f, layout.put()),
          "CreateTextLayout");
    for (const auto& run : projection.runs)
    {
        DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start), static_cast<UINT32>(run.length)};
        if (run.flags & Bold)
            Check(layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range), "SetFontWeight");
        if (run.flags & Italic)
            Check(layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range), "SetFontStyle");
        if (run.flags & Strike)
            Check(layout->SetStrikethrough(TRUE, range), "SetStrikethrough");
        if (run.flags & Code)
            Check(layout->SetFontFamilyName(options.codeFontFamily.c_str(), range), "SetFontFamilyName");
        if (run.heading)
        {
            Check(layout->SetFontSize(fontSize * (1.0f + 0.15f * static_cast<float>(7 - run.heading)), range), "SetFontSize");
            Check(layout->SetFontWeight(DWRITE_FONT_WEIGHT_SEMI_BOLD, range), "SetFontWeight");
        }
    }
    DWRITE_TEXT_METRICS metrics{};
    Check(layout->GetMetrics(&metrics), "GetMetrics");
    DWRITE_OVERHANG_METRICS overhang{};
    Check(layout->GetOverhangMetrics(&overhang), "GetOverhangMetrics");
    const auto drawingTop    = std::min(0.0f, -overhang.top);
    const auto drawingBottom = std::max(metrics.height, metrics.layoutHeight + overhang.bottom);
    if (! std::isfinite(metrics.height) || ! std::isfinite(drawingTop) || ! std::isfinite(drawingBottom))
        throw std::overflow_error("Nonfinite rich-text drawing metrics");
    std::vector<std::size_t> clusters{0};
    while (clusters.back() < projection.text.size())
        clusters.push_back(::DxUi::FindNextTextElementBoundary(projection.text, clusters.back()));
    UINT32 count = 0;
    layout->GetLineMetrics(nullptr, 0, &count);
    std::vector<DWRITE_LINE_METRICS> lineMetrics(count);
    if (count)
        Check(layout->GetLineMetrics(lineMetrics.data(), count, &count), "GetLineMetrics");
    std::vector<std::size_t> lines{0};
    for (const auto& line : lineMetrics)
        lines.push_back(lines.back() + line.length);
    auto next           = std::make_unique<State>();
    projection_         = std::move(projection);
    next->layout        = std::move(layout);
    next->clusters      = std::move(clusters);
    next->lines         = std::move(lines);
    next->lineMetrics   = std::move(lineMetrics);
    next->height        = metrics.height;
    next->drawingTop    = drawingTop;
    next->drawingBottom = drawingBottom;
    state_.swap(next);
    ++generation_;
}
void PreparedLayout::Clear() noexcept
{
    if (state_)
    {
        // Prepare constructs the empty retirement containers fallibly. Swapping
        // individual buffers here neither allocates nor moves a Debug proxy
        // through a noexcept default/move constructor. Destroy the old graph.
        projection_.source.swap(state_->retirement.source);
        projection_.text.swap(state_->retirement.text);
        projection_.sourceToDisplay.swap(state_->retirement.sourceToDisplay);
        projection_.units.swap(state_->retirement.units);
        projection_.runs.swap(state_->retirement.runs);
    }
    projection_.revision = 0u;
    projection_.mode     = ViewMode::Source;
    state_.reset();
    ++generation_;
}
std::pair<float, float> PreparedLayout::DrawingVerticalBounds() const
{
    if (! state_)
        throw std::logic_error("Unprepared rich-text drawing bounds");
    return {state_->drawingTop, state_->drawingBottom};
}
std::span<const DWRITE_LINE_METRICS> PreparedLayout::LineMetrics() const
{
    if (! state_)
        throw std::logic_error("Unprepared rich-text line metrics");
    return state_->lineMetrics;
}
Hit PreparedLayout::HitTest(float x, float y) const
{
    if (! state_ || ! std::isfinite(x) || ! std::isfinite(y))
        throw std::invalid_argument("Invalid hit test");
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(state_->layout->HitTestPoint(x, y, &trailing, &inside, &metrics), "HitTestPoint");
    auto position = std::min<std::size_t>(metrics.textPosition + (trailing ? metrics.length : 0), Content().text.size());
    // A font/OS can shape one Unicode grapheme into several glyph clusters.
    // Mouse navigation uses the same logical stops as deletion and arrows;
    // explicit ACP caret/range geometry still goes directly to DirectWrite.
    const auto stop = std::lower_bound(state_->clusters.begin(), state_->clusters.end(), position);
    if (stop != state_->clusters.end() && *stop != position)
        position = trailing ? *stop : (stop == state_->clusters.begin() ? 0u : *std::prev(stop));
    return {position, trailing != FALSE};
}
CaretGeometry PreparedLayout::Caret(std::size_t position) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Caret position");
    float x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(state_->layout->HitTestTextPosition(static_cast<UINT32>(position), FALSE, &x, &y, &metrics), "HitTestTextPosition");
    return {x, y, metrics.height};
}
std::vector<D2D1_RECT_F> PreparedLayout::Range(std::size_t first, std::size_t last) const
{
    if (! state_ || first > last || last > Content().text.size())
        throw std::out_of_range("Selection range");
    if (first == last)
        return std::vector<D2D1_RECT_F>(0u);
    UINT32 count = 0;
    state_->layout->HitTestTextRange(static_cast<UINT32>(first), static_cast<UINT32>(last - first), 0, 0, nullptr, 0, &count);
    std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
    if (count)
        Check(state_->layout->HitTestTextRange(static_cast<UINT32>(first), static_cast<UINT32>(last - first), 0, 0, metrics.data(), count, &count),
              "HitTestTextRange");
    std::vector<D2D1_RECT_F> rectangles(0u);
    rectangles.reserve(count);
    for (const auto& item : metrics)
        rectangles.push_back({item.left, item.top, item.left + item.width, item.top + item.height});
    return rectangles;
}
std::size_t PreparedLayout::PreviousCluster(std::size_t position) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Cluster position");
    auto found = std::lower_bound(state_->clusters.begin(), state_->clusters.end(), position);
    return found == state_->clusters.begin() ? 0 : *std::prev(found);
}
std::size_t PreparedLayout::NextCluster(std::size_t position) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Cluster position");
    const auto found = std::upper_bound(state_->clusters.begin(), state_->clusters.end(), position);
    return found == state_->clusters.end() ? Content().text.size() : *found;
}
std::pair<std::size_t, std::size_t> PreparedLayout::DeletionRange(std::size_t position, bool backwards) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Deletion position");
    const auto range = ::DxUi::GetTextElementDeletionRange(Content().text, position, backwards, state_->clusters);
    return {range.first, range.last};
}
std::size_t PreparedLayout::LineBoundary(std::size_t position, bool end) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Line position");
    auto found = std::upper_bound(state_->lines.begin(), state_->lines.end(), position);
    if (! end)
        return found == state_->lines.begin() ? 0 : *std::prev(found);
    auto result = found == state_->lines.end() ? Content().text.size() : *found;
    if (result > position && Content().text[result - 1] == L'\n')
        --result;
    return result;
}
std::pair<std::size_t, std::size_t> PreparedLayout::VisibleRange(float top, float bottom) const
{
    if (! state_ || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        throw std::invalid_argument("Invalid visible text viewport");
    std::optional<std::size_t> first;
    std::size_t last = 0;
    for (std::size_t index = 0; index < state_->lines.size(); ++index)
    {
        const auto caret = Caret(state_->lines[index]);
        if (caret.y >= bottom)
            break;
        if (caret.y + caret.height > top)
        {
            if (! first)
                first = state_->lines[index];
            last = index + 1 < state_->lines.size() ? state_->lines[index + 1] : Content().text.size();
        }
    }
    return {first.value_or(0), last};
}
void PreparedLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin) const
{
    if (! target || ! brush || ! state_)
        throw std::invalid_argument("Unprepared drawing");
    target->DrawTextLayout(origin, state_->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}
} // namespace DxUi::RichText
