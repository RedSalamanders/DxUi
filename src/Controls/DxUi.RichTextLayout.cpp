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
void PreparedLayout::Prepare(IDWriteFactory* factory, Projection projection, float width, float fontSize)
{
    LayoutOptions options;
    options.widthDip    = width;
    options.fontSizeDip = fontSize;
    Prepare(factory, std::move(projection), options);
}
void PreparedLayout::Prepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options)
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
    UINT32 count = 0;
    layout->GetClusterMetrics(nullptr, 0, &count);
    std::vector<DWRITE_CLUSTER_METRICS> clusterMetrics(count);
    if (count)
        Check(layout->GetClusterMetrics(clusterMetrics.data(), count, &count), "GetClusterMetrics");
    std::vector<std::size_t> clusters{0};
    for (const auto& cluster : clusterMetrics)
        clusters.push_back(clusters.back() + cluster.length);
    count = 0;
    layout->GetLineMetrics(nullptr, 0, &count);
    std::vector<DWRITE_LINE_METRICS> lineMetrics(count);
    if (count)
        Check(layout->GetLineMetrics(lineMetrics.data(), count, &count), "GetLineMetrics");
    std::vector<std::size_t> lines{0};
    for (const auto& line : lineMetrics)
        lines.push_back(lines.back() + line.length);
    projection_ = std::move(projection);
    layout_     = std::move(layout);
    clusters_   = std::move(clusters);
    lines_      = std::move(lines);
    height_     = metrics.height;
    ++generation_;
}
void PreparedLayout::Clear() noexcept
{
    layout_.reset();
    projection_ = {};
    clusters_.clear();
    lines_.clear();
    height_ = 0;
    ++generation_;
}
Hit PreparedLayout::HitTest(float x, float y) const
{
    if (! layout_ || ! std::isfinite(x) || ! std::isfinite(y))
        throw std::invalid_argument("Invalid hit test");
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(layout_->HitTestPoint(x, y, &trailing, &inside, &metrics), "HitTestPoint");
    return {std::min<std::size_t>(metrics.textPosition + (trailing ? metrics.length : 0), projection_.text.size()), trailing != FALSE};
}
CaretGeometry PreparedLayout::Caret(std::size_t position) const
{
    if (! layout_ || position > projection_.text.size())
        throw std::out_of_range("Caret position");
    float x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(layout_->HitTestTextPosition(static_cast<UINT32>(position), FALSE, &x, &y, &metrics), "HitTestTextPosition");
    return {x, y, metrics.height};
}
std::vector<D2D1_RECT_F> PreparedLayout::Range(std::size_t first, std::size_t last) const
{
    if (! layout_ || first > last || last > projection_.text.size())
        throw std::out_of_range("Selection range");
    if (first == last)
        return {};
    UINT32 count = 0;
    layout_->HitTestTextRange(static_cast<UINT32>(first), static_cast<UINT32>(last - first), 0, 0, nullptr, 0, &count);
    std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
    if (count)
        Check(layout_->HitTestTextRange(static_cast<UINT32>(first), static_cast<UINT32>(last - first), 0, 0, metrics.data(), count, &count),
              "HitTestTextRange");
    std::vector<D2D1_RECT_F> rectangles;
    rectangles.reserve(count);
    for (const auto& item : metrics)
        rectangles.push_back({item.left, item.top, item.left + item.width, item.top + item.height});
    return rectangles;
}
std::size_t PreparedLayout::PreviousCluster(std::size_t position) const
{
    if (! layout_ || position > projection_.text.size())
        throw std::out_of_range("Cluster position");
    auto found = std::lower_bound(clusters_.begin(), clusters_.end(), position);
    return found == clusters_.begin() ? 0 : *std::prev(found);
}
std::size_t PreparedLayout::NextCluster(std::size_t position) const
{
    if (! layout_ || position > projection_.text.size())
        throw std::out_of_range("Cluster position");
    const auto found = std::upper_bound(clusters_.begin(), clusters_.end(), position);
    return found == clusters_.end() ? projection_.text.size() : *found;
}
std::size_t PreparedLayout::LineBoundary(std::size_t position, bool end) const
{
    if (! layout_ || position > projection_.text.size())
        throw std::out_of_range("Line position");
    auto found = std::upper_bound(lines_.begin(), lines_.end(), position);
    if (! end)
        return found == lines_.begin() ? 0 : *std::prev(found);
    auto result = found == lines_.end() ? projection_.text.size() : *found;
    if (result && projection_.text[result - 1] == L'\n')
        --result;
    return result;
}
std::pair<std::size_t, std::size_t> PreparedLayout::VisibleRange(float top, float bottom) const
{
    if (! layout_ || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        throw std::invalid_argument("Invalid visible text viewport");
    std::optional<std::size_t> first;
    std::size_t last = 0;
    for (std::size_t index = 0; index < lines_.size(); ++index)
    {
        const auto caret = Caret(lines_[index]);
        if (caret.y >= bottom)
            break;
        if (caret.y + caret.height > top)
        {
            if (! first)
                first = lines_[index];
            last = index + 1 < lines_.size() ? lines_[index + 1] : projection_.text.size();
        }
    }
    return {first.value_or(0), last};
}
void PreparedLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin) const
{
    if (! target || ! brush || ! layout_)
        throw std::invalid_argument("Unprepared drawing");
    target->DrawTextLayout(origin, layout_.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}
} // namespace DxUi::RichText
