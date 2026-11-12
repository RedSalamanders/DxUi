#include "DxUi.PreparedLayoutAccess.h"
#include "DxUi.RichTextScriptRenderer.h"
#include "DxUi.RichTextSourceInkRenderer.h"
#include "DxUi.TextElements.h"
#include <DxUi/Failure.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <dwrite_1.h>
#include <new>
#include <optional>
#include <stdexcept>
#include <wil/resource.h>
#include <wil/result.h>
// Windows SDK WRL owns its interlocked/volatile implementation and target semantics.
// Scope is this system header only. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)
namespace DxUi::RichText
{
namespace
{
// Match DirectWrite's sequential single-precision document line accumulation.
// Reassociating chunk-local sums changes geometry for pitches such as 16 * 1.65.
// Stack-only rounding barrier. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
float AddLineCoordinate(float left, float right) noexcept
{
    volatile float result = left + right;
    return result;
}
#pragma warning(pop)
void Check(HRESULT result, const char*)
{
    if (FAILED(result))
        Failure::FailContract(result);
}
// DirectWrite may catch an internal C++ exception while lazily formatting a
// layout and still return an HRESULT. Keep only that opaque COM call inside
// the native allowance; following SDK allocations retain ordinary observation.
HRESULT NativeTextMetrics(IDWriteTextLayout* layout, DWRITE_TEXT_METRICS* metrics)
{
    return layout->GetMetrics(metrics);
}
HRESULT NativeOverhangMetrics(IDWriteTextLayout* layout, DWRITE_OVERHANG_METRICS* metrics)
{
    return layout->GetOverhangMetrics(metrics);
}
// A positioning-only first line lets DirectWrite perform its own sequential
// document-coordinate rounding, mixed-font caret math and color-font drawing.
// Its two private text units never enter source/display ACP mappings.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)

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
    Detail::ScriptMetadata scripts;
    bool hasScripts{};
    wil::com_ptr<IDWriteTextLayout> layout;
    wil::com_ptr<IDWriteInlineObject> positioning;
    std::vector<std::size_t> clusters, lines;
    std::vector<DWRITE_LINE_METRICS> lineMetrics;
    float height{};
    float drawingTop{}, drawingBottom{};
    float textLeft{}, textWidth{};
    float verticalOrigin{};
    float verticalTranslation{};
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
    if (const auto status = PrepareOwned(factory, projection, options); FAILED(status))
        Failure::FailContract(status);
}
void PreparedLayout::Prepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options)
{
    if (const auto status = PrepareOwned(factory, projection, options); FAILED(status))
        Failure::FailContract(status);
}
HRESULT PreparedLayout::TryPrepare(IDWriteFactory* factory, Projection projection, const LayoutOptions& options)
{
    return PrepareOwned(factory, projection, options);
}
HRESULT PreparedLayout::PrepareOwned(IDWriteFactory* factory, Projection& projection, const LayoutOptions& options, float verticalOrigin)
{
    return Detail::PreparedLayoutAccess::Prepare(*this, factory, projection, options, verticalOrigin);
}
HRESULT Detail::PreparedLayoutAccess::PrepareSemantic(PreparedLayout& owner,
                                                      IDWriteFactory* factory,
                                                      SemanticInput& input,
                                                      const LayoutOptions& options,
                                                      bool marker,
                                                      IDWriteInlineObject* object,
                                                      UINT32 objectOffset,
                                                      std::span<const InlineObject> objects)
{
    Projection projection{std::wstring(0u, L' '), std::wstring(0u, L' '), std::vector<std::size_t>(0u), std::vector<SourceUnit>(0u), std::vector<StyleRun>(0u)};
    projection.text.swap(input.text);
    projection.runs.swap(input.runs);
    projection.revision = input.revision;
    return Prepare(owner, factory, projection, options, 0.0f, true, marker, object, objectOffset, objects, input.padInlineCode);
}
HRESULT Detail::PreparedLayoutAccess::Prepare(PreparedLayout& owner,
                                              IDWriteFactory* factory,
                                              Projection& projection,
                                              const LayoutOptions& options,
                                              float verticalOrigin,
                                              bool semanticDisplay,
                                              bool marker,
                                              IDWriteInlineObject* object,
                                              UINT32 objectOffset,
                                              std::span<const InlineObject> objects,
                                              bool padInlineCode)
{
    const Failure::Scope failureScope;
    const auto width = options.widthDip, fontSize = options.fontSizeDip;
    const auto validName          = [](const std::wstring& name) { return ! name.empty() && name.size() <= 255 && IsValidUtf16(name); };
    const auto validHeadingSize   = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 512.0f)); };
    const auto validHeadingHeight = [](float value) noexcept { return std::isfinite(value) && (value == 0.0f || (value >= 1.0f && value <= 4.0f)); };
    const auto validSpacing       = [](const std::optional<float>& value, float maximum) noexcept
    { return ! value || (std::isfinite(value.value()) && value.value() >= 0.0f && value.value() <= maximum); };
    if (! factory || ! std::isfinite(width) || width <= 0 || ! std::isfinite(fontSize) || fontSize <= 0 || ! std::isfinite(options.dpi) ||
        options.dpi <= 0.0f || options.dpi > 4096.0f || ! std::isfinite(options.codeFontSizeDip) || options.codeFontSizeDip < 0.0f ||
        options.codeFontSizeDip > 512.0f || ! std::isfinite(options.lineHeightMultiplier) || ! std::isfinite(options.tabStopDip) || options.tabStopDip < 0.0f ||
        options.tabStopDip > 4096.0f || (options.tabStopDip > 0.0f && options.tabStopDip < 1.0f) ||
        (options.lineHeightMultiplier != 0.0f && (options.lineHeightMultiplier < 1.0f || options.lineHeightMultiplier > 4.0f)) ||
        ! std::isfinite(options.headingScaleStep) || options.headingScaleStep < 0.0f || options.headingScaleStep > 1.0f || ! validName(options.fontFamily) ||
        ! validName(options.codeFontFamily) ||
        (semanticDisplay && (! validName(options.mathFontFamily) || ! std::isfinite(options.mathAbsoluteUnitScale) || options.mathAbsoluteUnitScale <= 0.f ||
                             options.mathAbsoluteUnitScale > 16.f)) ||
        options.locale.size() > 255 || ! IsValidUtf16(options.locale) ||
        (options.readingDirection != DWRITE_READING_DIRECTION_LEFT_TO_RIGHT && options.readingDirection != DWRITE_READING_DIRECTION_RIGHT_TO_LEFT) ||
        (options.textAlignment != DWRITE_TEXT_ALIGNMENT_LEADING && options.textAlignment != DWRITE_TEXT_ALIGNMENT_TRAILING &&
         options.textAlignment != DWRITE_TEXT_ALIGNMENT_CENTER) ||
        ! std::isfinite(verticalOrigin) || verticalOrigin < 0.0f || ! std::ranges::all_of(options.headingFontSizeDip, validHeadingSize) ||
        ! std::ranges::all_of(options.headingLineHeightMultiplier, validHeadingHeight) || ! validSpacing(options.paragraphSpacingAfterLines, 4.0f) ||
        ! validSpacing(options.listItemSpacingDip, 256.0f) || (options.semanticSpacing && ! options.semanticSpacing->IsValid()) ||
        (options.markdownPresentation != MarkdownPresentationStyle::Default && options.markdownPresentation != MarkdownPresentationStyle::Subtle))
        return E_INVALIDARG;
    const auto maximum = semanticDisplay ? Document::MaximumUnits * 2u : Document::MaximumUnits;
    if (projection.text.size() > maximum || ! IsValidSourceUtf16(projection.text) ||
        (! semanticDisplay &&
         (projection.source.size() > maximum || ! IsValidSourceUtf16(projection.source) || projection.units.size() != projection.text.size() ||
          projection.sourceToDisplay.size() != projection.source.size() + 1 || (projection.mode != ViewMode::Source && projection.mode != ViewMode::Rich))))
        return E_INVALIDARG;
    if (object && (! semanticDisplay || objectOffset >= projection.text.size() || projection.text[objectOffset] != L'\ufffc'))
        return E_INVALIDARG;
    std::size_t objectEnd{};
    for (const auto& inlineObject : objects)
    {
        if (! semanticDisplay || ! inlineObject.object || ! inlineObject.length || inlineObject.first < objectEnd ||
            inlineObject.first >= projection.text.size() || inlineObject.length > projection.text.size() - inlineObject.first ||
            ! IsEditBoundary(projection.text, inlineObject.first) ||
            ! IsEditBoundary(projection.text, static_cast<std::size_t>(inlineObject.first) + inlineObject.length) ||
            (object && objectOffset >= inlineObject.first && objectOffset < static_cast<std::size_t>(inlineObject.first) + inlineObject.length))
            return E_INVALIDARG;
        objectEnd = static_cast<std::size_t>(inlineObject.first) + inlineObject.length;
    }
    if (! semanticDisplay)
    {
        std::size_t previousDisplay = 0, previousSource = 0;
        for (const auto offset : projection.sourceToDisplay)
        {
            if (offset < previousDisplay || offset > projection.text.size())
                return E_INVALIDARG;
            previousDisplay = offset;
        }
        for (const auto& unit : projection.units)
        {
            if (unit.start < previousSource || unit.start >= unit.end || unit.end > projection.source.size())
                return E_INVALIDARG;
            previousSource = unit.end;
        }
    }
    for (const auto& run : projection.runs)
    {
        if (run.start > projection.text.size() || run.length > projection.text.size() - run.start || run.heading > 6 ||
            (run.flags & ~(Bold | Italic | Code | Strike | Underline | Link | Marked | Superscript | Subscript)) ||
            (run.flags & (Superscript | Subscript)) == (Superscript | Subscript) || ! IsEditBoundary(projection.text, run.start) ||
            ! IsEditBoundary(projection.text, run.start + run.length))
            return E_INVALIDARG;
    }
    // Create the retained text format through the supplied DirectWrite factory.
    wil::com_ptr<IDWriteTextFormat> format;
    RETURN_IF_FAILED(factory->CreateTextFormat(options.fontFamily.c_str(),
                                               nullptr,
                                               DWRITE_FONT_WEIGHT_NORMAL,
                                               DWRITE_FONT_STYLE_NORMAL,
                                               DWRITE_FONT_STRETCH_NORMAL,
                                               fontSize,
                                               options.locale.c_str(),
                                               format.put()));
    RETURN_IF_FAILED(format->SetWordWrapping(marker || object || ! options.wrap ? DWRITE_WORD_WRAPPING_NO_WRAP : DWRITE_WORD_WRAPPING_WRAP));
    RETURN_IF_FAILED(format->SetReadingDirection(options.readingDirection));
    RETURN_IF_FAILED(format->SetTextAlignment(options.textAlignment));
    if (options.tabStopDip > 0.0f)
        RETURN_IF_FAILED(format->SetIncrementalTabStop(options.tabStopDip));
    wil::com_ptr<IDWriteTextLayout> layout;
    std::wstring positionedText(0u, L'\0');
    // Uniform Source spacing applies to every SDK line, including a private
    // prefix. Position these unstyled chunks through their drawing/query origin
    // instead; retain the exact-prefix path for natural and rich typography.
    const bool translatedOrigin    = verticalOrigin > 0.0f && options.lineHeightMultiplier > 0.0f && projection.mode == ViewMode::Source;
    const auto verticalTranslation = translatedOrigin ? verticalOrigin : 0.0f;
    const UINT32 textOffset        = verticalOrigin > 0.0f && ! translatedOrigin ? 2u : 0u;
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
    RETURN_IF_FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format.get(), width, 1000000.0f, layout.put()));
    wil::com_ptr<IDWriteInlineObject> positioning;
    if (textOffset)
    {
        // WRL allocates through Make's nothrow new. Translate only its null
        // allocation result to the existing fallible staging contract.
        auto origin = Microsoft::WRL::Make<InvisibleLayoutOrigin>(verticalOrigin);
        if (origin.Get() == nullptr)
            return E_OUTOFMEMORY;
        positioning = origin.Get();
        RETURN_IF_FAILED(layout->SetFontSize(0.01f, {0u, textOffset}));
        RETURN_IF_FAILED(layout->SetInlineObject(positioning.get(), {0u, 1u}));
    }
    float scriptTopOutset{}, scriptBottomOutset{};
    for (const auto& run : projection.runs)
    {
        DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + textOffset, static_cast<UINT32>(run.length)};
        if (run.flags & Bold)
            RETURN_IF_FAILED(layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range));
        if (run.flags & Italic)
            RETURN_IF_FAILED(layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range));
        if (run.flags & Strike)
            RETURN_IF_FAILED(layout->SetStrikethrough(TRUE, range));
        if (run.flags & (Underline | Link))
            RETURN_IF_FAILED(layout->SetUnderline(TRUE, range));
        if (run.flags & Code)
        {
            RETURN_IF_FAILED(layout->SetFontFamilyName(options.codeFontFamily.c_str(), range));
            if (options.codeFontSizeDip > 0.0f)
                RETURN_IF_FAILED(layout->SetFontSize(options.codeFontSizeDip, range));
            if (padInlineCode && run.length)
            {
                // Character spacing contributes to native metrics, wrapping,
                // hit testing and carets. Painting a wider chip alone would
                // cover neighboring text. Only the two scalar endpoints get
                // an inset; fenced-code contents retain their original metrics.
                wil::com_ptr<IDWriteTextLayout1> text1;
                RETURN_IF_FAILED(wil::com_query_to_nothrow(layout.get(), text1.put()));
                const auto firstEnd    = StepToNextCodePoint(projection.text, run.start);
                const auto lastStart   = StepToPreviousCodePoint(projection.text, run.start + run.length);
                const auto firstLength = static_cast<UINT32>(firstEnd - run.start);
                RETURN_IF_FAILED(text1->SetCharacterSpacing(3.0f, firstEnd == run.start + run.length ? 3.0f : 0.0f, 0.0f, {range.startPosition, firstLength}));
                if (firstEnd != run.start + run.length)
                    RETURN_IF_FAILED(text1->SetCharacterSpacing(
                        0.0f, 3.0f, 0.0f, {static_cast<UINT32>(lastStart) + textOffset, static_cast<UINT32>(run.start + run.length - lastStart)}));
            }
        }
        if (run.heading)
        {
            const auto size         = (run.flags & Code) && options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : fontSize;
            const auto overrideSize = options.headingFontSizeDip[run.heading - 1u];
            RETURN_IF_FAILED(layout->SetFontSize(
                overrideSize > 0.0f ? overrideSize : size * (1.0f + options.headingScaleStep * static_cast<float>(7 - run.heading)), range));
            RETURN_IF_FAILED(layout->SetFontWeight(semanticDisplay && (run.flags & Bold) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_SEMI_BOLD, range));
        }
        if (run.flags & (Superscript | Subscript))
        {
            auto size = (run.flags & Code) && options.codeFontSizeDip > 0.0f ? options.codeFontSizeDip : fontSize;
            if (run.heading)
                size = options.headingFontSizeDip[run.heading - 1u] > 0.0f ? options.headingFontSizeDip[run.heading - 1u]
                                                                           : size * (1.0f + options.headingScaleStep * static_cast<float>(7u - run.heading));
            RETURN_IF_FAILED(layout->SetFontSize(size * .75f, range));
            if (run.flags & Superscript)
                scriptTopOutset = std::max(scriptTopOutset, size * .30f);
            if (run.flags & Subscript)
                scriptBottomOutset = std::max(scriptBottomOutset, size * .20f);
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
                RETURN_IF_FAILED(layout->SetFontFamilyName(L"Segoe UI Symbol", {static_cast<UINT32>(i) + textOffset, 1u}));
    }
    auto lineHeight       = options.lineHeightMultiplier;
    const auto headingRun = std::ranges::find_if(projection.runs, [](const auto& run) { return run.heading != 0u; });
    if (semanticDisplay && headingRun != projection.runs.end() && options.headingLineHeightMultiplier[headingRun->heading - 1u] > 0.0f)
        lineHeight = options.headingLineHeightMultiplier[headingRun->heading - 1u];
    const bool scripts = std::ranges::any_of(projection.runs, [](const auto& run) { return (run.flags & (Superscript | Subscript)) != 0u; });
    if (lineHeight > 0.0f || scripts)
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
        auto ascent = size * (scripts ? 1.05f : .8f), descent = size * (scripts ? .4f : .2f);
        const auto accommodate = [&](IDWriteInlineObject* value) -> HRESULT
        {
            DWRITE_INLINE_OBJECT_METRICS extent{};
            RETURN_IF_FAILED(value->GetMetrics(&extent));
            ascent  = std::max(ascent, extent.baseline);
            descent = std::max(descent, extent.height - extent.baseline);
            return S_OK;
        };
        if (object)
            RETURN_IF_FAILED(accommodate(object));
        for (const auto& inlineObject : objects)
            RETURN_IF_FAILED(accommodate(inlineObject.object));
        const auto height = std::max(size * lineHeight, ascent + descent);
        RETURN_IF_FAILED(layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, height, ascent + (height - ascent - descent) * 0.5f));
    }
    DWRITE_TEXT_METRICS metrics{};
    // The layout retains this COM property. Its no-op Draw reserves metrics;
    // native graphics paint explicitly on the owner's known target later.
    if (object)
        RETURN_IF_FAILED(layout->SetInlineObject(object, {objectOffset + textOffset, 1u}));
    for (const auto& inlineObject : objects)
        RETURN_IF_FAILED(layout->SetInlineObject(inlineObject.object, {inlineObject.first + textOffset, inlineObject.length}));
    RETURN_IF_FAILED(Failure::InvokeNative(&NativeTextMetrics, layout.get(), &metrics));
    DWRITE_OVERHANG_METRICS overhang{};
    RETURN_IF_FAILED(Failure::InvokeNative(&NativeOverhangMetrics, layout.get(), &overhang));
    const auto drawingTop    = std::min(verticalOrigin, verticalTranslation - overhang.top) - scriptTopOutset;
    const auto drawingBottom = verticalTranslation + std::max(metrics.height, metrics.layoutHeight + overhang.bottom) + scriptBottomOutset;
    if (! std::isfinite(metrics.height) || ! std::isfinite(drawingTop) || ! std::isfinite(drawingBottom))
        return E_UNEXPECTED;
    std::vector<std::size_t> clusters{0};
    while (clusters.back() < projection.text.size())
        clusters.push_back(::DxUi::FindNextTextElementBoundary(projection.text, clusters.back()));
    UINT32 count                 = 0;
    const auto lineMetricsResult = layout->GetLineMetrics(nullptr, 0, &count);
    if (lineMetricsResult != E_NOT_SUFFICIENT_BUFFER)
        RETURN_IF_FAILED(lineMetricsResult);
    std::vector<DWRITE_LINE_METRICS> lineMetrics(count);
    if (count)
        RETURN_IF_FAILED(layout->GetLineMetrics(lineMetrics.data(), count, &count));
    // An empty suffix would inherit the tiny prefix LF's terminal font. Keep
    // that uncommon case on canonical whole geometry instead.
    const bool positionedExactly = ! textOffset || (! projection.text.empty() && ! lineMetrics.empty() && lineMetrics.front().length == textOffset &&
                                                    lineMetrics.front().height == verticalOrigin);
    if (textOffset && ! lineMetrics.empty())
        lineMetrics.erase(lineMetrics.begin());
    std::vector<std::size_t> lines{0};
    for (const auto& line : lineMetrics)
        lines.push_back(lines.back() + line.length);
    auto next            = std::make_unique<PreparedLayout::State>();
    next->hasScripts     = scripts;
    float documentHeight = verticalTranslation + metrics.height;
    if (translatedOrigin)
    {
        auto localTop = 0.0f, documentTop = verticalOrigin;
        std::size_t first = 0u;
        next->scripts.uniformSourceLines.reserve(lineMetrics.size());
        for (const auto& line : lineMetrics)
        {
            next->scripts.uniformSourceLines.push_back(
                {first, localTop, documentTop, AddLineCoordinate(localTop, line.baseline), AddLineCoordinate(documentTop, line.baseline)});
            first += line.length;
            localTop    = AddLineCoordinate(localTop, line.height);
            documentTop = AddLineCoordinate(documentTop, line.height);
        }
        documentHeight = documentTop;
    }
    if (scripts || translatedOrigin)
    {
        Detail::ScriptTextRenderer collector(nullptr, nullptr, layout.get(), projection, textOffset, {}, next->scripts, true);
        RETURN_IF_FAILED(layout->Draw(nullptr, &collector, 0.f, 0.f));
        if (collector.Result() == E_OUTOFMEMORY)
            return E_OUTOFMEMORY;
        if (collector.Result() == DWRITE_E_UNSUPPORTEDOPERATION)
            next->scripts.unsupported = true;
        else
            RETURN_IF_FAILED(collector.Result());
        std::sort(next->scripts.decorations.begin(), next->scripts.decorations.end(), [](const auto& a, const auto& b) {
            return a.baseline < b.baseline || (a.baseline == b.baseline && a.left < b.left);
        });
    }
    owner.projection_         = std::move(projection);
    next->layout              = std::move(layout);
    next->positioning         = std::move(positioning);
    next->clusters            = std::move(clusters);
    next->lines               = std::move(lines);
    next->lineMetrics         = std::move(lineMetrics);
    next->height              = documentHeight;
    next->drawingTop          = drawingTop;
    next->drawingBottom       = std::max(drawingBottom, documentHeight + std::max(0.0f, overhang.bottom) + scriptBottomOutset);
    next->textLeft            = metrics.left;
    next->textWidth           = metrics.widthIncludingTrailingWhitespace;
    next->verticalOrigin      = verticalOrigin;
    next->verticalTranslation = verticalTranslation;
    next->textOffset          = textOffset;
    next->positionedExactly   = positionedExactly;
    owner.state_.swap(next);
    ++owner.generation_;
    return S_OK;
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
        Failure::FailContract(E_INVALIDARG);
    return {state_->drawingTop, state_->drawingBottom};
}
std::span<const DWRITE_LINE_METRICS> PreparedLayout::LineMetrics() const
{
    if (! state_)
        Failure::FailContract(E_INVALIDARG);
    return state_->lineMetrics;
}
std::pair<float, float> PreparedLayout::HorizontalTextBounds() const
{
    if (! state_)
        Failure::FailContract(E_INVALIDARG);
    return {state_->textLeft, state_->textWidth};
}
float Detail::PreparedLayoutAccess::MinimumTextWidth(const PreparedLayout& owner)
{
    if (! owner.state_)
        Failure::FailContract(E_INVALIDARG);
    float minimum{};
    Check(owner.state_->layout->DetermineMinWidth(&minimum), "DetermineMinWidth");
    if (! std::isfinite(minimum) || minimum < 0.0f)
        Failure::FailContract(E_INVALIDARG);
    return minimum;
}
float PreparedLayout::VerticalOrigin() const noexcept
{
    return state_ ? state_->verticalOrigin : 0.0f;
}
bool PreparedLayout::PositionedExactly() const noexcept
{
    return state_ && state_->positionedExactly;
}
HRESULT PreparedLayout::TryHitTest(float x, float y, Hit& output) const
{
    if (! state_ || ! std::isfinite(x) || ! std::isfinite(y))
        return E_INVALIDARG;
    BOOL trailing = FALSE, inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    auto localY       = y - state_->verticalTranslation;
    const auto& lines = state_->scripts.uniformSourceLines;
    if (! lines.empty())
    {
        const auto after =
            std::upper_bound(lines.begin(), lines.end(), y, [](float value, const Detail::UniformSourceLine& line) { return value < line.documentTop; });
        const auto& line = after == lines.begin() ? lines.front() : *std::prev(after);
        localY           = line.localTop + (y - line.documentTop);
    }
    const auto hr = state_->layout->HitTestPoint(x, localY, &trailing, &inside, &metrics);
    if (FAILED(hr))
        return hr;
    const auto actual = static_cast<std::size_t>(metrics.textPosition) + (trailing ? metrics.length : 0u);
    auto position     = std::min<std::size_t>(actual > state_->textOffset ? actual - state_->textOffset : 0u, Content().text.size());
    const auto stop   = std::lower_bound(state_->clusters.begin(), state_->clusters.end(), position);
    if (stop != state_->clusters.end() && *stop != position)
        position = trailing ? *stop : (stop == state_->clusters.begin() ? 0u : *std::prev(stop));
    output = {position, trailing != FALSE, inside != FALSE};
    return S_OK;
}
Hit PreparedLayout::HitTest(float x, float y) const
{
    Hit output{};
    const auto hr = TryHitTest(x, y, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
HRESULT PreparedLayout::TryCaret(std::size_t position, CaretGeometry& output) const
{
    if (! state_ || position > Content().text.size())
        return E_INVALIDARG;
    float x = 0, y = 0;
    DWRITE_HIT_TEST_METRICS metrics{};
    const auto hr = state_->layout->HitTestTextPosition(static_cast<UINT32>(position) + state_->textOffset, FALSE, &x, &y, &metrics);
    if (FAILED(hr))
        return hr;
    if (! state_->scripts.uniformSourceLines.empty())
    {
        const auto& line = Detail::UniformLineAt(state_->scripts.uniformSourceLines, position);
        y                = line.documentBaseline - (line.localBaseline - y);
    }
    else
        y += state_->verticalTranslation;
    output = {x, y, metrics.height};
    return S_OK;
}
CaretGeometry PreparedLayout::Caret(std::size_t position) const
{
    CaretGeometry output{};
    const auto hr = TryCaret(position, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
HRESULT PreparedLayout::TryRange(std::size_t first, std::size_t last, std::vector<D2D1_RECT_F>& output) const
{
    Failure::Scope failureScope;
    if (! state_ || first > last || last > Content().text.size())
        return E_INVALIDARG;
    std::vector<D2D1_RECT_F> rectangles(0u);
    if (first != last)
    {
        const auto origin = state_->scripts.uniformSourceLines.empty() ? state_->verticalTranslation : 0.0f;
        UINT32 count      = 0;
        const auto query =
            state_->layout->HitTestTextRange(static_cast<UINT32>(first) + state_->textOffset, static_cast<UINT32>(last - first), 0, origin, nullptr, 0, &count);
        if (FAILED(query) && query != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
            return query;
        std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
        if (count)
        {
            const auto hr = state_->layout->HitTestTextRange(
                static_cast<UINT32>(first) + state_->textOffset, static_cast<UINT32>(last - first), 0, origin, metrics.data(), count, &count);
            if (FAILED(hr))
                return hr;
            if (count > metrics.size())
                return E_UNEXPECTED;
        }
        rectangles.reserve(count);
        for (UINT32 index = 0u; index < count; ++index)
        {
            const auto& item = metrics[index];
            auto top         = item.top;
            if (! state_->scripts.uniformSourceLines.empty())
            {
                const auto& line = Detail::UniformLineAt(state_->scripts.uniformSourceLines, item.textPosition);
                top              = line.documentBaseline - (line.localBaseline - top);
            }
            rectangles.push_back({item.left, top, item.left + item.width, top + item.height});
        }
    }
    output.swap(rectangles);
    return S_OK;
}
std::vector<D2D1_RECT_F> PreparedLayout::Range(std::size_t first, std::size_t last) const
{
    Failure::Scope failureScope;
    std::vector<D2D1_RECT_F> output(0u);
    const auto hr = TryRange(first, last, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return std::vector<D2D1_RECT_F>(output);
}
HRESULT PreparedLayout::TryPreviousCluster(std::size_t position, std::size_t& output) const
{
    if (! state_ || position > Content().text.size())
        return E_INVALIDARG;
    const auto found = std::lower_bound(state_->clusters.begin(), state_->clusters.end(), position);
    output           = found == state_->clusters.begin() ? 0u : *std::prev(found);
    return S_OK;
}
HRESULT PreparedLayout::TryNextCluster(std::size_t position, std::size_t& output) const
{
    if (! state_ || position > Content().text.size())
        return E_INVALIDARG;
    const auto found = std::upper_bound(state_->clusters.begin(), state_->clusters.end(), position);
    output           = found == state_->clusters.end() ? Content().text.size() : *found;
    return S_OK;
}
std::size_t PreparedLayout::PreviousCluster(std::size_t position) const
{
    std::size_t output{};
    const auto hr = TryPreviousCluster(position, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
std::size_t PreparedLayout::NextCluster(std::size_t position) const
{
    std::size_t output{};
    const auto hr = TryNextCluster(position, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
HRESULT PreparedLayout::TryDeletionRange(std::size_t position, bool backwards, std::pair<std::size_t, std::size_t>& output) const
{
    if (! state_ || position > Content().text.size())
        return E_INVALIDARG;
    const auto range = ::DxUi::GetTextElementDeletionRange(Content().text, position, backwards, state_->clusters);
    output           = {range.first, range.last};
    return S_OK;
}
std::pair<std::size_t, std::size_t> PreparedLayout::DeletionRange(std::size_t position, bool backwards) const
{
    std::pair<std::size_t, std::size_t> output{};
    const auto hr = TryDeletionRange(position, backwards, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
HRESULT PreparedLayout::TryLineBoundary(std::size_t position, bool end, std::size_t& output) const
{
    if (! state_ || position > Content().text.size())
        return E_INVALIDARG;
    const auto found = std::upper_bound(state_->lines.begin(), state_->lines.end(), position);
    if (! end)
        output = found == state_->lines.begin() ? 0u : *std::prev(found);
    else
    {
        auto result = found == state_->lines.end() ? Content().text.size() : *found;
        if (result > position && Content().text[result - 1u] == L'\n')
            --result;
        output = result;
    }
    return S_OK;
}
std::size_t PreparedLayout::LineBoundary(std::size_t position, bool end) const
{
    std::size_t output{};
    const auto hr = TryLineBoundary(position, end, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
HRESULT PreparedLayout::TryVisibleRange(float top, float bottom, std::pair<std::size_t, std::size_t>& output) const
{
    if (! state_ || ! std::isfinite(top) || ! std::isfinite(bottom) || bottom < top)
        return E_INVALIDARG;
    std::optional<std::size_t> first;
    std::size_t last = 0u;
    for (std::size_t index = 0; index < state_->lines.size(); ++index)
    {
        CaretGeometry caret{};
        const auto hr = TryCaret(state_->lines[index], caret);
        if (FAILED(hr))
            return hr;
        if (caret.y >= bottom)
            break;
        if (caret.y + caret.height > top)
        {
            if (! first)
                first = state_->lines[index];
            last = index + 1u < state_->lines.size() ? state_->lines[index + 1u] : Content().text.size();
        }
    }
    output = {first.value_or(0u), last};
    return S_OK;
}
std::pair<std::size_t, std::size_t> PreparedLayout::VisibleRange(float top, float bottom) const
{
    std::pair<std::size_t, std::size_t> output{};
    const auto hr = TryVisibleRange(top, bottom, output);
    if (FAILED(hr))
        Failure::FailContract(hr);
    return output;
}
bool Detail::PreparedLayoutAccess::ScriptTargetSupported(const PreparedLayout& owner, bool colorTargetAvailable) noexcept
{
    if (! owner.state_)
        return false;
    if (! owner.state_->hasScripts && owner.state_->scripts.uniformSourceLines.empty())
        return true;
    if (owner.state_->scripts.unsupported)
        return false;
    if (! owner.state_->scripts.requiresColor)
        return true;
    return colorTargetAvailable;
}
void PreparedLayout::Draw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, const LinkDrawing* links) const
{
    const auto hr = TryDraw(target, brush, origin, links);
    if (FAILED(hr))
        Failure::FailContract(hr);
}
HRESULT PreparedLayout::TryDraw(ID2D1RenderTarget* target, ID2D1Brush* brush, D2D1_POINT_2F origin, const LinkDrawing* links) const
{
    Failure::Scope failureScope;
    if (! target || ! brush || ! state_)
        return E_INVALIDARG;
    const bool positionedSource = ! state_->scripts.uniformSourceLines.empty();
    if (! positionedSource)
        origin.y += state_->verticalTranslation;
    const bool scripts = state_->hasScripts || positionedSource;
    if (scripts && state_->scripts.unsupported)
        return DWRITE_E_UNSUPPORTEDOPERATION;
    if (scripts && state_->scripts.requiresColor)
    {
        wil::com_ptr<ID2D1DeviceContext7> color;
        const auto hr = target->QueryInterface(IID_PPV_ARGS(color.put()));
        if (hr == E_NOINTERFACE)
            return DWRITE_E_UNSUPPORTEDOPERATION;
        if (FAILED(hr))
            return hr;
    }
    if (! links && ! scripts)
    {
        target->DrawTextLayout(origin, state_->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        return S_OK;
    }
    const LinkDrawing defaultDrawing{};
    if (! links)
        links = &defaultDrawing;
    if (! scripts && links->directSourceInkPreview)
    {
        HRESULT paintResult = S_OK;
        const auto drawn    = Detail::DrawSourceInkIfSupported(target,
                                                               brush,
                                                               state_->layout.get(),
                                                               state_->positioning.get(),
                                                               state_->textOffset,
                                                               projection_,
                                                               *links,
                                                               origin,
                                                               Detail::SourceInkColorContextLookup::QueryTarget,
                                                               &paintResult);
        if (FAILED(paintResult))
            return paintResult;
        if (drawn == Detail::SourceInkDrawResult::Drawn)
            return S_OK;
    }
    // SDK drawing attributes are borrowed for this synchronous owner-thread
    // raster call. Copies may share the layout; restore its canonical style
    // even when setting a later range fails. No brush survives a returning call.
    // Arm rollback before each fallible effect setter, including a failed one.
    bool drawingEffectAttempted = false;
    const auto restore          = wil::scope_exit([&]() noexcept
    {
        if (drawingEffectAttempted)
        {
            const auto hr = state_->layout->SetDrawingEffect(nullptr, {state_->textOffset, static_cast<UINT32>(projection_.text.size())});
            if (FAILED(hr))
                Failure::FailContract(hr); // Shared retained layout cannot safely keep a borrowed raster effect.
        }
        for (const auto& run : projection_.runs)
            if (run.flags & Link)
            {
                const DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + state_->textOffset, static_cast<UINT32>(run.length)};
                const auto hr = state_->layout->SetUnderline(TRUE, range);
                if (FAILED(hr))
                    Failure::FailContract(hr); // Every sharing owner requires the canonical underline state.
            }
    });
    const auto intersects       = [](const std::optional<MarkdownRange>& active, std::size_t first, std::size_t last) noexcept
    { return active && active->first < last && first < active->last; };
    if (links->emphasis && links->emphasisBrush)
    {
        const auto first = std::max(links->emphasis->first, links->displayOrigin);
        const auto last  = std::min(links->emphasis->last, links->displayOrigin + projection_.text.size());
        if (first < last)
        {
            drawingEffectAttempted = true;
            const auto hr          = state_->layout->SetDrawingEffect(
                links->emphasisBrush, {static_cast<UINT32>(first - links->displayOrigin) + state_->textOffset, static_cast<UINT32>(last - first)});
            if (FAILED(hr))
                return hr;
        }
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
        const auto apply = [&](size_t begin, size_t end) -> HRESULT
        {
            if (begin < end)
            {
                drawingEffectAttempted = true;
                return state_->layout->SetDrawingEffect(
                    links->inkBrushes[role], {static_cast<UINT32>(begin - links->displayOrigin) + state_->textOffset, static_cast<UINT32>(end - begin)});
            }
            return S_OK;
        };
        if (links->inkExclusion && links->inkExclusion->first < last && first < links->inkExclusion->last)
        {
            auto hr = apply(first, std::min(last, links->inkExclusion->first));
            if (FAILED(hr))
                return hr;
            hr = apply(std::max(first, links->inkExclusion->last), last);
            if (FAILED(hr))
                return hr;
        }
        else
        {
            const auto hr = apply(first, last);
            if (FAILED(hr))
                return hr;
        }
    }
    for (const auto& run : projection_.runs)
        if (run.flags & Link)
        {
            const DWRITE_TEXT_RANGE range{static_cast<UINT32>(run.start) + state_->textOffset, static_cast<UINT32>(run.length)};
            const auto first = links->displayOrigin + run.start, last = first + run.length;
            const bool underline =
                (run.flags & Underline) || links->underline || intersects(links->hover, first, last) || intersects(links->caret, first, last);
            const auto underlineResult = state_->layout->SetUnderline(underline ? TRUE : FALSE, range);
            if (FAILED(underlineResult))
                return underlineResult;
            auto* linkBrush =
                links->emphasis && ! intersects(links->emphasis, first, last) && links->inactiveLinkBrush ? links->inactiveLinkBrush : links->brush;
            if (linkBrush)
            {
                drawingEffectAttempted = true;
                const auto hr          = state_->layout->SetDrawingEffect(linkBrush, range);
                if (FAILED(hr))
                    return hr;
            }
        }
    if (scripts)
    {
        Detail::ScriptTextRenderer renderer(target, brush, state_->layout.get(), projection_, state_->textOffset, origin, state_->scripts, false);
        const auto hr = state_->layout->Draw(nullptr, &renderer, origin.x, origin.y);
        if (FAILED(hr))
            return hr;
        if (FAILED(renderer.Result()))
            return renderer.Result();
    }
    else
        target->DrawTextLayout(origin, state_->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    return S_OK;
}
} // namespace DxUi::RichText
