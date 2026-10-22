#include "DxUi.TextElements.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <new>
#include <optional>
#include <stdexcept>
#include <wil/resource.h>
#include <wil/result.h>
#include <wrl/implements.h>
namespace DxUi::RichText
{
namespace
{
void Check(HRESULT result, const char* operation)
{
    if (FAILED(result))
        THROW_HR_MSG(result, "%hs", operation);
}
// A positioning-only first line lets DirectWrite perform its own sequential
// document-coordinate rounding, mixed-font caret math and color-font drawing.
// Its two private text units never enter source/display ACP mappings.
class InvisibleLayoutOrigin final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
public:
    explicit InvisibleLayoutOrigin(float height) noexcept : height_(height)
    {
    }
    HRESULT STDMETHODCALLTYPE Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_INLINE_OBJECT_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        *metrics = {0.0f, height_, height_ * 0.5f, FALSE};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* metrics) noexcept override
    {
        if (! metrics)
            return E_POINTER;
        // No ink belongs to this zero-width object. Collapse its ink box at the
        // synthetic line's end so it cannot expand viewport drawing bounds.
        *metrics = {0.0f, -height_, 0.0f, 0.0f};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBreakConditions(DWRITE_BREAK_CONDITION* before, DWRITE_BREAK_CONDITION* after) noexcept override
    {
        if (! before || ! after)
            return E_POINTER;
        *before = *after = DWRITE_BREAK_CONDITION_NEUTRAL;
        return S_OK;
    }

private:
    float height_{};
};
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
    wil::com_ptr<IDWriteInlineObject> positioning;
    std::vector<std::size_t> clusters, lines;
    std::vector<DWRITE_LINE_METRICS> lineMetrics;
    float height{};
    float drawingTop{}, drawingBottom{};
    float textLeft{}, textWidth{};
    float verticalOrigin{};
    UINT32 textOffset{};
    bool positionedExactly = true;
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
void PreparedLayout::PrepareSemanticOwned(IDWriteFactory* factory,
                                          Projection& projection,
                                          const LayoutOptions& options,
                                          bool marker,
                                          IDWriteInlineObject* object,
                                          UINT32 objectOffset,
                                          std::span<const SemanticInlineObject> objects)
{
    PrepareOwned(factory, projection, options, 0.0f, true, marker, object, objectOffset, objects);
}
void PreparedLayout::PrepareOwned(IDWriteFactory* factory,
                                  Projection& projection,
                                  const LayoutOptions& options,
                                  float verticalOrigin,
                                  bool semanticDisplay,
                                  bool marker,
                                  IDWriteInlineObject* object,
                                  UINT32 objectOffset,
                                  std::span<const SemanticInlineObject> objects)
{
    const auto width = options.widthDip, fontSize = options.fontSizeDip;
    const auto validName          = [](const std::wstring& name) { return ! name.empty() && name.size() <= 255 && IsValidUtf16(name); };
    const auto validHeadingSize   = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 512.0f)); };
    const auto validHeadingHeight = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 4.0f)); };
    if (! factory || ! std::isfinite(width) || width <= 0 || ! std::isfinite(fontSize) || fontSize <= 0 || ! std::isfinite(options.codeFontSizeDip) ||
        options.codeFontSizeDip < 0.0f || options.codeFontSizeDip > 512.0f || ! std::isfinite(options.lineHeightMultiplier) ||
        (options.lineHeightMultiplier != 0.0f && (options.lineHeightMultiplier < 1.0f || options.lineHeightMultiplier > 4.0f)) ||
        ! std::isfinite(options.headingScaleStep) || options.headingScaleStep < 0.0f || options.headingScaleStep > 1.0f || ! validName(options.fontFamily) ||
        ! validName(options.codeFontFamily) || options.locale.size() > 255 || ! IsValidUtf16(options.locale) ||
        (options.readingDirection != DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && options.readingDirection != DWRITE_READING_DIRECTION_RIGHT_TO_LEFT) ||
        (options.textAlignment != DWRITE_TEXT_ALIGNMENT_LEADING && options.textAlignment != DWRITE_TEXT_ALIGNMENT_TRAILING &&
         options.textAlignment != DWRITE_TEXT_ALIGNMENT_CENTER) ||
        ! std::isfinite(verticalOrigin) || verticalOrigin < 0.0f || ! std::ranges::all_of(options.headingFontSizeDip, validHeadingSize) ||
        ! std::ranges::all_of(options.headingLineHeightMultiplier, validHeadingHeight))
        throw std::invalid_argument("Invalid rich-text layout options");
    const auto maximum = semanticDisplay ? Document::MaximumUnits * 2u : Document::MaximumUnits;
    if (projection.text.size() > maximum || ! IsValidUtf16(projection.text) || projection.source.size() > maximum || ! IsValidUtf16(projection.source) ||
        projection.units.size() != projection.text.size() || projection.sourceToDisplay.size() != projection.source.size() + 1 ||
        (projection.mode != ViewMode::Source && projection.mode != ViewMode::Rich))
        throw std::invalid_argument("Invalid rich-text display text");
    if (object && (! semanticDisplay || objectOffset >= projection.text.size() || projection.text[objectOffset] != L'\ufffc'))
        throw std::invalid_argument("Invalid semantic object placement");
    std::size_t objectEnd{};
    for (const auto& inlineObject : objects)
    {
        if (! semanticDisplay || ! inlineObject.object || ! inlineObject.length || inlineObject.first < objectEnd ||
            inlineObject.first >= projection.text.size() || inlineObject.length > projection.text.size() - inlineObject.first ||
            ! IsEditBoundary(projection.text, inlineObject.first) ||
            ! IsEditBoundary(projection.text, static_cast<std::size_t>(inlineObject.first) + inlineObject.length) ||
            (object && objectOffset >= inlineObject.first && objectOffset < static_cast<std::size_t>(inlineObject.first) + inlineObject.length))
            throw std::invalid_argument("Invalid semantic inline object range");
        objectEnd = static_cast<std::size_t>(inlineObject.first) + inlineObject.length;
    }
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
            (run.flags & ~(Bold | Italic | Code | Strike | Underline | Link)) || ! IsEditBoundary(projection.text, run.start) ||
            ! IsEditBoundary(projection.text, run.start + run.length))
            throw std::invalid_argument("Invalid rich-text style range");
    }
    // Create the retained text format through the supplied DirectWrite factory.
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
    Check(format->SetWordWrapping(marker || object ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP), "SetWordWrapping");
    Check(format->SetReadingDirection(options.readingDirection), "SetReadingDirection");
    Check(format->SetTextAlignment(options.textAlignment), "SetTextAlignment");
    wil::com_ptr<IDWriteTextLayout> layout;
    std::wstring positionedText(0u, L'\0');
    const UINT32 textOffset = verticalOrigin > 0.0f ? 2u : 0u;
    if (textOffset)
    {
        positionedText.assign(L"\ufffc\n");
        positionedText.append(projection.text);
    }
    if (! objects.empty())
    {
        if (! textOffset)
            positionedText.assign(projection.text);
        // The public display/source map retains the Unicode alternative. Only
        // the private SDK buffer uses neutral object code units of equal
        // length: otherwise script/font fallback can split a Han/emoji alt
        // into several inline-object runs and repeat the image metrics.
        for (const auto& inlineObject : objects)
            std::fill_n(positionedText.begin() + inlineObject.first + textOffset, inlineObject.length, L'\ufffc');
    }
    const auto text = textOffset || ! objects.empty() ? std::wstring_view(positionedText) : std::wstring_view(projection.text);
    Check(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.get(), width, 1000000.0f, layout.put()), "CreateTextLayout");
    wil::com_ptr<IDWriteInlineObject> positioning;
    if (textOffset)
    {
        // WRL allocates through Make's nothrow new. Translate only its null
        // allocation result to the existing fallible staging contract.
        auto origin = Microsoft::WRL::Make<InvisibleLayoutOrigin>(verticalOrigin);
        if (origin.Get() == nullptr)
            throw std::bad_alloc();
        positioning = origin.Get();
        Check(layout->SetFontSize(0.01f, {0u, textOffset}), "Set private origin font size");
        Check(layout->SetInlineObject(positioning.get(), {0u, 1u}), "Set private layout origin");
    }
    for (const auto& run : projection.runs)
    {
        DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + textOffset, static_cast<UINT32>(run.length)};
        if (run.flags & Bold)
            Check(layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range), "SetFontWeight");
        if (run.flags & Italic)
            Check(layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range), "SetFontStyle");
        if (run.flags & Strike)
            Check(layout->SetStrikethrough(TRUE, range), "SetStrikethrough");
        if (run.flags & (Underline | Link))
            Check(layout->SetUnderline(TRUE, range), "SetUnderline");
        if (run.flags & Code)
        {
            Check(layout->SetFontFamilyName(options.codeFontFamily.c_str(), range), "SetFontFamilyName");
            if (options.codeFontSizeDip > 0.0f)
                Check(layout->SetFontSize(options.codeFontSizeDip, range), "Set code font size");
        }
        if (run.heading)
        {
            const auto size         = (run.flags & Code) && options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : fontSize;
            const auto overrideSize = options.headingFontSizeDip[run.heading - 1u];
            Check(
                layout->SetFontSize(overrideSize > 0.0f ? overrideSize : size * (1.0f + options.headingScaleStep * static_cast<float>(7 - run.heading)), range),
                "SetFontSize");
            Check(layout->SetFontWeight(semanticDisplay && (run.flags & Bold) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_SEMI_BOLD, range),
                  "SetFontWeight");
        }
    }
    if (marker)
    {
        // Only semantic marker layouts contain these owned task objects.
        // Segoe UI's checked box falls back to a wider color emoji; using the
        // same monochrome symbol face for both states keeps the bullet, task
        // hit box and body text fixed when a checkbox is toggled. Body text
        // and generated list ordinals retain the caller's requested font.
        for (std::size_t i = 0u; i < projection.text.size(); ++i)
            if (projection.text[i] == L'\u2610' || projection.text[i] == L'\u2611')
                Check(layout->SetFontFamilyName(L"Segoe UI Symbol", {static_cast<UINT32>(i) + textOffset, 1u}), "Set task checkbox font");
    }
    auto lineHeight       = options.lineHeightMultiplier;
    const auto headingRun = std::ranges::find_if(projection.runs, [](const auto& run) { return run.heading != 0u; });
    if (semanticDisplay && headingRun != projection.runs.end() && options.headingLineHeightMultiplier[headingRun->heading - 1u] > 0.0f)
        lineHeight = options.headingLineHeightMultiplier[headingRun->heading - 1u];
    if (lineHeight > 0.0f && ! textOffset)
    {
        auto size = fontSize;
        for (const auto& run : projection.runs)
        {
            auto runSize = (run.flags & Code) && options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : fontSize;
            if (run.heading)
                runSize = options.headingFontSizeDip[run.heading - 1u] > 0.0f
                              ? options.headingFontSizeDip[run.heading - 1u]
                              : runSize * (1.0f + options.headingScaleStep * static_cast<float>(7u - run.heading));
            size = std::max(size, runSize);
        }
        // Font-relative line height matches a document's 16/1.65 typography.
        // Inline images/math must still reserve their complete ascent/descent.
        auto ascent = size * 0.8f, descent = size * 0.2f;
        const auto accommodate = [&](IDWriteInlineObject* value)
        {
            DWRITE_INLINE_OBJECT_METRICS extent{};
            Check(value->GetMetrics(&extent), "Get inline metrics for line spacing");
            ascent  = std::max(ascent, extent.baseline);
            descent = std::max(descent, extent.height - extent.baseline);
        };
        if (object)
            accommodate(object);
        for (const auto& inlineObject : objects)
            accommodate(inlineObject.object);
        const auto height = std::max(size * lineHeight, ascent + descent);
        Check(layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, height, ascent + (height - ascent - descent) * 0.5f),
              "Set font-relative line spacing");
    }
    DWRITE_TEXT_METRICS metrics{};
    // The layout retains this COM property. Its no-op Draw reserves metrics;
    // native graphics paint explicitly on the owner's known target later.
    if (object)
        Check(layout->SetInlineObject(object, {objectOffset + textOffset, 1u}), "Set semantic object metrics");
    for (const auto& inlineObject : objects)
        Check(layout->SetInlineObject(inlineObject.object, {inlineObject.first + textOffset, inlineObject.length}), "Set prepared image metrics");
    Check(layout->GetMetrics(&metrics), "GetMetrics");
    DWRITE_OVERHANG_METRICS overhang{};
    Check(layout->GetOverhangMetrics(&overhang), "GetOverhangMetrics");
    const auto drawingTop    = std::min(verticalOrigin, -overhang.top);
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
    // An empty suffix would inherit the tiny prefix LF's terminal font. Keep
    // that uncommon case on canonical whole geometry instead.
    const bool positionedExactly = ! textOffset || (! projection.text.empty() && ! lineMetrics.empty() && lineMetrics.front().length == textOffset &&
                                                    lineMetrics.front().height == verticalOrigin);
    if (textOffset && ! lineMetrics.empty())
        lineMetrics.erase(lineMetrics.begin());
    std::vector<std::size_t> lines{0};
    for (const auto& line : lineMetrics)
        lines.push_back(lines.back() + line.length);
    auto next               = std::make_unique<State>();
    projection_             = std::move(projection);
    next->layout            = std::move(layout);
    next->positioning       = std::move(positioning);
    next->clusters          = std::move(clusters);
    next->lines             = std::move(lines);
    next->lineMetrics       = std::move(lineMetrics);
    next->height            = metrics.height;
    next->drawingTop        = drawingTop;
    next->drawingBottom     = drawingBottom;
    next->textLeft          = metrics.left;
    next->textWidth         = metrics.widthIncludingTrailingWhitespace;
    next->verticalOrigin    = verticalOrigin;
    next->textOffset        = textOffset;
    next->positionedExactly = positionedExactly;
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
std::pair<float, float> PreparedLayout::HorizontalTextBounds() const
{
    if (! state_)
        throw std::logic_error("Unprepared rich-text horizontal metrics");
    return {state_->textLeft, state_->textWidth};
}
float PreparedLayout::VerticalOrigin() const noexcept
{
    return state_ ? state_->verticalOrigin : 0.0f;
}
bool PreparedLayout::PositionedExactly() const noexcept
{
    return state_ && state_->positionedExactly;
}
Hit PreparedLayout::HitTest(float x, float y) const
{
    if (! state_ || ! std::isfinite(x) || ! std::isfinite(y))
        throw std::invalid_argument("Invalid hit test");
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(state_->layout->HitTestPoint(x, y, &trailing, &inside, &metrics), "HitTestPoint");
    const auto actualPosition = static_cast<std::size_t>(metrics.textPosition) + (trailing ? metrics.length : 0u);
    auto position             = std::min<std::size_t>(actualPosition > state_->textOffset ? actualPosition - state_->textOffset : 0u, Content().text.size());
    // A font/OS can shape one Unicode grapheme into several glyph clusters.
    // Mouse navigation uses the same logical stops as deletion and arrows;
    // explicit ACP caret/range geometry still goes directly to DirectWrite.
    const auto stop = std::lower_bound(state_->clusters.begin(), state_->clusters.end(), position);
    if (stop != state_->clusters.end() && *stop != position)
        position = trailing ? *stop : (stop == state_->clusters.begin() ? 0u : *std::prev(stop));
    return {position, trailing != FALSE, inside != FALSE};
}
CaretGeometry PreparedLayout::Caret(std::size_t position) const
{
    if (! state_ || position > Content().text.size())
        throw std::out_of_range("Caret position");
    float x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    Check(state_->layout->HitTestTextPosition(static_cast<UINT32>(position) + state_->textOffset, FALSE, &x, &y, &metrics), "HitTestTextPosition");
    return {x, y, metrics.height};
}
std::vector<D2D1_RECT_F> PreparedLayout::Range(std::size_t first, std::size_t last) const
{
    if (! state_ || first > last || last > Content().text.size())
        throw std::out_of_range("Selection range");
    if (first == last)
        return std::vector<D2D1_RECT_F>(0u);
    UINT32 count = 0;
    state_->layout->HitTestTextRange(static_cast<UINT32>(first) + state_->textOffset, static_cast<UINT32>(last - first), 0, 0, nullptr, 0, &count);
    std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
    if (count)
        Check(state_->layout->HitTestTextRange(
                  static_cast<UINT32>(first) + state_->textOffset, static_cast<UINT32>(last - first), 0, 0, metrics.data(), count, &count),
              "HitTestTextRange");
    std::vector<D2D1_RECT_F> rectangles(0u);
    rectangles.reserve(count);
    for (const auto& item : metrics)
        rectangles.push_back({item.left, item.top, item.left + item.width, item.top + item.height});
    // Do not rely on optional NRVO: MSVC Debug's noexcept vector move can
    // allocate an iterator proxy. A fallible copy/prvalue keeps query/staging
    // allocation failures inside the caller's exception boundary.
    return std::vector<D2D1_RECT_F>(rectangles);
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
void PreparedLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, const LinkDrawing* links) const
{
    if (! target || ! brush || ! state_)
        throw std::invalid_argument("Unprepared drawing");
    if (! links)
    {
        target->DrawTextLayout(origin, state_->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        return;
    }
    // SDK drawing attributes are borrowed for this synchronous owner-thread
    // raster call. Copies may share the layout; restore its canonical style
    // even when setting a later range fails. No brush survives this call.
    const auto restore    = wil::scope_exit([&]() noexcept
    {
        static_cast<void>(state_->layout->SetDrawingEffect(nullptr, {state_->textOffset, static_cast<UINT32>(projection_.text.size())}));
        for (const auto& run : projection_.runs)
            if (run.flags & Link)
            {
                const DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + state_->textOffset, static_cast<UINT32>(run.length)};
                static_cast<void>(state_->layout->SetUnderline(TRUE, range));
            }
    });
    const auto intersects = [](const std::optional<MarkdownRange>& active, std::size_t first, std::size_t last) noexcept
    { return active && active->first < last && first < active->last; };
    if (links->emphasis && links->emphasisBrush)
    {
        const auto first = std::max(links->emphasis->first, links->displayOrigin);
        const auto last  = std::min(links->emphasis->last, links->displayOrigin + projection_.text.size());
        if (first < last)
            Check(state_->layout->SetDrawingEffect(links->emphasisBrush,
                                                   {static_cast<UINT32>(first - links->displayOrigin) + state_->textOffset, static_cast<UINT32>(last - first)}),
                  "Set paragraph emphasis ink");
    }
    for (auto ink = std::lower_bound(links->inkRanges.begin(),
                                     links->inkRanges.end(),
                                     links->displayOrigin,
                                     [](const TextInkRange& range, size_t first) { return range.display.last <= first; });
         ink != links->inkRanges.end() && ink->display.first < links->displayOrigin + projection_.text.size();
         ++ink)
    {
        const auto role = static_cast<size_t>(ink->role);
        if (role >= links->inkBrushes.size() || ! links->inkBrushes[role])
            continue;
        const auto first = std::max(ink->display.first, links->displayOrigin);
        const auto last  = std::min(ink->display.last, links->displayOrigin + projection_.text.size());
        const auto apply = [&](size_t begin, size_t end)
        {
            if (begin < end)
                Check(state_->layout->SetDrawingEffect(
                          links->inkBrushes[role], {static_cast<UINT32>(begin - links->displayOrigin) + state_->textOffset, static_cast<UINT32>(end - begin)}),
                      "Set prepared source syntax ink");
        };
        if (links->inkExclusion && links->inkExclusion->first < last && first < links->inkExclusion->last)
        {
            apply(first, std::min(last, links->inkExclusion->first));
            apply(std::max(first, links->inkExclusion->last), last);
        }
        else
            apply(first, last);
    }
    for (const auto& run : projection_.runs)
        if (run.flags & Link)
        {
            const DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + state_->textOffset, static_cast<UINT32>(run.length)};
            const auto first = links->displayOrigin + run.start, last = first + run.length;
            const bool underline =
                (run.flags & Underline) || links->underline || intersects(links->hover, first, last) || intersects(links->caret, first, last);
            Check(state_->layout->SetUnderline(underline ? TRUE : FALSE, range), "Set transient link underline");
            auto* linkBrush =
                links->emphasis && ! intersects(links->emphasis, first, last) && links->inactiveLinkBrush ? links->inactiveLinkBrush : links->brush;
            if (linkBrush)
                Check(state_->layout->SetDrawingEffect(linkBrush, range), "Set transient link ink");
        }
    target->DrawTextLayout(origin, state_->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}
} // namespace DxUi::RichText
