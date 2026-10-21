#include "Scope.Tests.RichDocumentLayout.h"
#include "../../Samples/EmbeddedControls/GraphicsFixture.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <vector>
#include <wil/result.h>
#include <wrl/implements.h>

namespace
{
namespace RichLineOriginDiagnostics
{
struct GlyphOrigin
{
    UINT32 position{}, glyphCount{};
    float x{}, y{}, fontSize{};
    DWRITE_FONT_METRICS fontMetrics{};
};
class GlyphOriginRecorder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
public:
    GlyphOriginRecorder(std::span<GlyphOrigin> storage, bool snapping) noexcept : storage_(storage), snapping_(snapping)
    {
    }
    std::span<const GlyphOrigin> Recorded() const noexcept
    {
        return storage_.first(count_);
    }
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) noexcept override
    {
        if (! disabled)
            return E_POINTER;
        *disabled = ! snapping_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) noexcept override
    {
        if (! transform)
            return E_POINTER;
        *transform = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) noexcept override
    {
        if (! pixels)
            return E_POINTER;
        *pixels = 1.0f;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,
                                           FLOAT x,
                                           FLOAT y,
                                           DWRITE_MEASURING_MODE,
                                           const DWRITE_GLYPH_RUN* run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                                           IUnknown*) noexcept override
    {
        if (! run || ! description || count_ == storage_.size())
            return E_BOUNDS;
        // Storage is allocated before Draw. No exception/allocation crosses COM.
        auto& event       = storage_[count_++];
        event.position    = description->textPosition;
        event.glyphCount  = run->glyphCount;
        event.x           = x;
        event.y           = y;
        event.fontSize    = run->fontEmSize;
        event.fontMetrics = {};
        if (run->fontFace)
            run->fontFace->GetMetrics(&event.fontMetrics);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) noexcept override
    {
        return E_NOTIMPL; // No inline objects are installed by this raw probe.
    }

private:
    std::span<GlyphOrigin> storage_;
    std::size_t count_{};
    bool snapping_{};
};
inline float Add(float first, float second) noexcept
{
    volatile float result = first + second;
    return result;
}
class InvisibleLinePrefix final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteInlineObject>
{
public:
    explicit InvisibleLinePrefix(float height) noexcept : height_(height)
    {
    }
    HRESULT STDMETHODCALLTYPE Draw(void*, IDWriteTextRenderer*, FLOAT, FLOAT, BOOL, BOOL, IUnknown*) noexcept override
    {
        return S_OK; // Positioning-only object has no visible content.
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
        *metrics = {};
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
void RunRawLineOrigins()
{
    const auto oldPrecision     = std::cout.precision();
    const auto restorePrecision = wil::scope_exit([=] { std::cout.precision(oldPrecision); });
    std::cout << std::setprecision(std::numeric_limits<float>::max_digits10);
    GraphicsFixture graphics;
    RequireSucceeded(graphics.Create(), "raw line diagnostic WARP device");
    wil::com_ptr<ID2D1Factory1> d2dFactory;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(d2dFactory.put())));
    wil::com_ptr<IDXGIDevice> dxgi;
    THROW_IF_FAILED(graphics.device.query_to(dxgi.put()));
    wil::com_ptr<ID2D1Device> d2dDevice;
    THROW_IF_FAILED(d2dFactory->CreateDevice(dxgi.get(), d2dDevice.put()));
    wil::com_ptr<ID2D1DeviceContext> context;
    THROW_IF_FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
    wil::com_ptr<ID2D1DeviceContext7> colorContext;
    const auto colorSupport = context->QueryInterface(IID_PPV_ARGS(colorContext.put()));
    std::cout << "rich raw color capability result=" << static_cast<ULONG>(colorSupport) << " available=" << static_cast<bool>(colorContext) << '\n';
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    for (const auto fontSize : {13.0f, 18.0f, 18.1f})
        for (unsigned profile = 0u; profile < 3u; ++profile)
        {
            std::wstring source;
            if (profile == 0u)
                source = std::wstring(65535u, L'\n') + L"x";
            else
                for (unsigned line = 0u; line < 1024u; ++line)
                    source += profile == 1u ? L"one visible line\n"
                                            : L"Arabic \u0627\u0644\u0639\u0631\u0628\u064a\u0629 \U0001f600 a\u0301 wrapped text and more words\r\n";
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(factory->CreateTextFormat(
                L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", format.put()));
            THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
            THROW_IF_FAILED(format->SetReadingDirection(profile == 2u ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
            wil::com_ptr<IDWriteTextLayout> layout;
            THROW_IF_FAILED(factory->CreateTextLayout(source.data(), static_cast<UINT32>(source.size()), format.get(), 321.25f, 1000000.0f, layout.put()));
            if (profile == 2u)
                for (std::size_t first = 0u; first < source.size();)
                {
                    THROW_IF_FAILED(layout->SetFontSize(fontSize * 1.9f, {static_cast<UINT32>(first), 7u}));
                    const auto code = source.find(L"wrapped", first);
                    Require(code != std::wstring::npos, "raw diagnostic retains its explicit code-font run");
                    THROW_IF_FAILED(layout->SetFontFamilyName(L"Consolas", {static_cast<UINT32>(code), 7u}));
                    const auto last = source.find(L'\n', first);
                    first           = last == std::wstring::npos ? source.size() : last + 1u;
                }
            UINT32 count = 0u;
            layout->GetLineMetrics(nullptr, 0u, &count);
            Require(count > 0u && count <= source.size() + 1u && source.size() <= DxUi::RichText::Document::MaximumUnits,
                    "raw line diagnostics bound source, metrics and preallocated callback storage");
            std::vector<DWRITE_LINE_METRICS> metrics(count);
            THROW_IF_FAILED(layout->GetLineMetrics(metrics.data(), count, &count));
            std::vector<UINT32> starts(count + 1u, 0u);
            std::vector<float> singleTops(count), doubleTops(count);
            float singleTop  = 0.0f;
            double doubleTop = 0.0;
            for (std::size_t line = 0u; line < count; ++line)
            {
                starts[line + 1u] = starts[line] + metrics[line].length;
                singleTops[line]  = singleTop;
                doubleTops[line]  = static_cast<float>(doubleTop);
                singleTop         = Add(singleTop, metrics[line].height);
                doubleTop += metrics[line].height;
            }
            DWRITE_TEXT_METRICS text{};
            THROW_IF_FAILED(layout->GetMetrics(&text));
            std::size_t caretSamples = 0u, singleCaretMismatches = 0u, doubleCaretMismatches = 0u, baselineCaretMismatches = 0u;
            for (std::size_t position = 0u;; position = std::min(source.size(), position + 53u))
            {
                float x = 0.0f, y = 0.0f;
                DWRITE_HIT_TEST_METRICS hit{};
                THROW_IF_FAILED(layout->HitTestTextPosition(static_cast<UINT32>(position), FALSE, &x, &y, &hit));
                const auto found = std::upper_bound(starts.begin(), starts.end() - 1, position);
                const auto line  = static_cast<std::size_t>(found - starts.begin() - 1);
                ++caretSamples;
                singleCaretMismatches += y != singleTops[line];
                doubleCaretMismatches += y != doubleTops[line];
                baselineCaretMismatches += y != Add(Add(singleTops[line], metrics[line].baseline), -metrics[line].baseline);
                if (y != singleTops[line] && singleCaretMismatches <= 8u)
                    std::cout << "rich raw caret detail font=" << fontSize << " profile=" << profile << " position=" << position << " line=" << line
                              << " actual=" << y << " metricTop=" << hit.top << " metricHeight=" << hit.height << " lineTop=" << singleTops[line]
                              << " lineHeight=" << metrics[line].height << " lineBaseline=" << metrics[line].baseline << '\n';
                if (position == source.size())
                    break;
            }
            UINT32 rangeCount = 0u;
            layout->HitTestTextRange(0u, static_cast<UINT32>(source.size()), 0.0f, 0.0f, nullptr, 0u, &rangeCount);
            Require(rangeCount <= source.size() + 1u, "raw range observation count remains bounded by source");
            std::vector<DWRITE_HIT_TEST_METRICS> ranges(rangeCount);
            THROW_IF_FAILED(layout->HitTestTextRange(0u, static_cast<UINT32>(source.size()), 0.0f, 0.0f, ranges.data(), rangeCount, &rangeCount));
            std::size_t rangeTopMismatches = 0u, rangeHeightMismatches = 0u;
            for (const auto range : ranges)
            {
                const auto found = std::upper_bound(starts.begin(), starts.end() - 1, range.textPosition);
                const auto line  = static_cast<std::size_t>(found - starts.begin() - 1);
                rangeTopMismatches += range.top != singleTops[line];
                rangeHeightMismatches += range.height != metrics[line].height;
                if ((range.top != singleTops[line] || range.height != metrics[line].height) && rangeTopMismatches + rangeHeightMismatches <= 16u)
                    std::cout << "rich raw range detail font=" << fontSize << " profile=" << profile << " position=" << range.textPosition << " line=" << line
                              << " actualTop=" << range.top << " actualHeight=" << range.height << " lineTop=" << singleTops[line]
                              << " lineHeight=" << metrics[line].height << " lineBaseline=" << metrics[line].baseline << '\n';
            }
            std::cout << "rich raw range font=" << fontSize << " profile=" << profile << " count=" << rangeCount << " topMismatches=" << rangeTopMismatches
                      << " heightMismatches=" << rangeHeightMismatches << '\n';
            const auto split            = profile == 0u ? std::size_t(1007u) : source.find(L'\n', source.size() / 2u) + 1u;
            const auto splitLine        = static_cast<std::size_t>(std::upper_bound(starts.begin(), starts.end() - 1, split) - starts.begin() - 1);
            const auto seed             = singleTops[splitLine];
            std::wstring positionedText = L"\ufffc\n" + source.substr(split);
            wil::com_ptr<IDWriteTextLayout> positioned;
            THROW_IF_FAILED(factory->CreateTextLayout(
                positionedText.data(), static_cast<UINT32>(positionedText.size()), format.get(), 321.25f, 1000000.0f, positioned.put()));
            THROW_IF_FAILED(positioned->SetFontSize(0.01f, {0u, 2u}));
            auto spacer = Microsoft::WRL::Make<InvisibleLinePrefix>(seed);
            THROW_HR_IF(E_OUTOFMEMORY, spacer.Get() == nullptr);
            THROW_IF_FAILED(positioned->SetInlineObject(spacer.Get(), {0u, 1u}));
            if (profile == 2u)
                for (std::size_t first = 2u; first < positionedText.size();)
                {
                    THROW_IF_FAILED(positioned->SetFontSize(fontSize * 1.9f, {static_cast<UINT32>(first), 7u}));
                    const auto code = positionedText.find(L"wrapped", first);
                    Require(code != std::wstring::npos, "positioned raw fixture retains code-font run");
                    THROW_IF_FAILED(positioned->SetFontFamilyName(L"Consolas", {static_cast<UINT32>(code), 7u}));
                    const auto last = positionedText.find(L'\n', first);
                    first           = last == std::wstring::npos ? positionedText.size() : last + 1u;
                }
            UINT32 positionedLineCount = 0u;
            positioned->GetLineMetrics(nullptr, 0u, &positionedLineCount);
            std::vector<DWRITE_LINE_METRICS> positionedLines(positionedLineCount);
            THROW_IF_FAILED(positioned->GetLineMetrics(positionedLines.data(), positionedLineCount, &positionedLineCount));
            DWRITE_TEXT_METRICS positionedMetrics{};
            DWRITE_OVERHANG_METRICS positionedOverhang{};
            THROW_IF_FAILED(positioned->GetMetrics(&positionedMetrics));
            THROW_IF_FAILED(positioned->GetOverhangMetrics(&positionedOverhang));
            std::size_t positionedCaretMismatches = 0u;
            for (std::size_t position = split;; position = std::min(source.size(), position + 53u))
            {
                float expectedX = 0.0f, expectedY = 0.0f, actualX = 0.0f, actualY = 0.0f;
                DWRITE_HIT_TEST_METRICS expected{}, actual{};
                THROW_IF_FAILED(layout->HitTestTextPosition(static_cast<UINT32>(position), FALSE, &expectedX, &expectedY, &expected));
                THROW_IF_FAILED(positioned->HitTestTextPosition(static_cast<UINT32>(position - split + 2u), FALSE, &actualX, &actualY, &actual));
                positionedCaretMismatches += expectedX != actualX || expectedY != actualY || expected.height != actual.height;
                if (position == source.size())
                    break;
            }
            std::cout << "rich positioned geometry font=" << fontSize << " profile=" << profile << " split=" << split << " seed=" << seed
                      << " prefixHeight=" << positionedLines.front().height << " prefixUnits=" << positionedLines.front().length
                      << " heightEqual=" << (positionedMetrics.height == text.height) << " caretMismatches=" << positionedCaretMismatches
                      << " overhangTop=" << positionedOverhang.top << " overhangBottom=" << positionedOverhang.bottom << '\n';
            std::vector<GlyphOrigin> positionedStorage(positionedText.size() * 8u + 1u);
            std::vector<GlyphOrigin> storage(source.size() * 8u + 1u);
            for (const auto originY : {0.0f, -611.75f, -123456.25f})
                for (const bool snapping : {false, true})
                {
                    auto recorder = Microsoft::WRL::Make<GlyphOriginRecorder>(std::span<GlyphOrigin>(storage), snapping);
                    THROW_HR_IF(E_OUTOFMEMORY, recorder.Get() == nullptr);
                    THROW_IF_FAILED(layout->Draw(nullptr, recorder.Get(), 3.25f, originY));
                    auto positionedRecorder = Microsoft::WRL::Make<GlyphOriginRecorder>(std::span<GlyphOrigin>(positionedStorage), snapping);
                    THROW_HR_IF(E_OUTOFMEMORY, positionedRecorder.Get() == nullptr);
                    THROW_IF_FAILED(positioned->Draw(nullptr, positionedRecorder.Get(), 3.25f, originY));
                    std::size_t comparedRuns = 0u, positionedRunMismatches = 0u, positionedIndex = 0u;
                    const auto positionedEvents = positionedRecorder->Recorded();
                    while (positionedIndex < positionedEvents.size() && positionedEvents[positionedIndex].position < 2u)
                        ++positionedIndex;
                    for (const auto glyph : recorder->Recorded())
                    {
                        if (glyph.position < split)
                            continue;
                        ++comparedRuns;
                        if (positionedIndex == positionedEvents.size())
                        {
                            ++positionedRunMismatches;
                            continue;
                        }
                        const auto actual = positionedEvents[positionedIndex++];
                        positionedRunMismatches += actual.position - 2u + split != glyph.position || actual.x != glyph.x || actual.y != glyph.y ||
                                                   actual.glyphCount != glyph.glyphCount || actual.fontSize != glyph.fontSize;
                    }
                    positionedRunMismatches += positionedEvents.size() - positionedIndex;
                    std::cout << "rich positioned glyphs font=" << fontSize << " profile=" << profile << " originY=" << originY << " snapping=" << snapping
                              << " compared=" << comparedRuns << " mismatches=" << positionedRunMismatches << '\n';
                    std::size_t singleMismatches = 0u, doubleMismatches = 0u, bottomBaselineMismatches = 0u, ascentLeadingMismatches = 0u;
                    UINT32 firstPosition = 0u, firstActual = 0u, firstSingle = 0u, firstDouble = 0u;
                    for (const auto glyph : recorder->Recorded())
                    {
                        const auto found   = std::upper_bound(starts.begin(), starts.end() - 1, glyph.position);
                        const auto line    = static_cast<std::size_t>(found - starts.begin() - 1);
                        const auto single  = Add(originY, Add(singleTops[line], metrics[line].baseline));
                        const auto precise = Add(originY, Add(doubleTops[line], metrics[line].baseline));
                        const auto fromBottom =
                            Add(originY, Add(Add(singleTops[line], metrics[line].height), -Add(metrics[line].height, -metrics[line].baseline)));
                        const auto ascent      = glyph.fontMetrics.designUnitsPerEm
                                                     ? glyph.fontSize * (static_cast<float>(glyph.fontMetrics.ascent) / glyph.fontMetrics.designUnitsPerEm)
                                                     : 0.0f;
                        const auto fromLeading = Add(originY, Add(Add(singleTops[line], Add(metrics[line].baseline, -ascent)), ascent));
                        bottomBaselineMismatches += glyph.y != fromBottom;
                        ascentLeadingMismatches += glyph.y != fromLeading;
                        if (! singleMismatches && glyph.y != single)
                        {
                            firstPosition = glyph.position;
                            firstActual   = std::bit_cast<UINT32>(glyph.y);
                            firstSingle   = std::bit_cast<UINT32>(single);
                            firstDouble   = std::bit_cast<UINT32>(precise);
                        }
                        singleMismatches += glyph.y != single;
                        doubleMismatches += glyph.y != precise;
                        if (originY == 0.0f && ! snapping && glyph.y != single && singleMismatches <= 8u)
                            std::cout << "rich raw glyph detail font=" << fontSize << " profile=" << profile << " position=" << glyph.position
                                      << " line=" << line << " glyphs=" << glyph.glyphCount << " actual=" << glyph.y << " expected=" << single
                                      << " fromBottom=" << fromBottom << " fromLeading=" << fromLeading << " top=" << singleTops[line]
                                      << " height=" << metrics[line].height << " baseline=" << metrics[line].baseline << " runFontSize=" << glyph.fontSize
                                      << " designUnits=" << glyph.fontMetrics.designUnitsPerEm << " ascent=" << glyph.fontMetrics.ascent
                                      << " descent=" << glyph.fontMetrics.descent << " gap=" << glyph.fontMetrics.lineGap << '\n';
                    }
                    std::cout << "rich raw baseline font=" << fontSize << " profile=" << profile << " lines=" << count << " originY=" << originY
                              << " snapping=" << snapping << " glyphRuns=" << recorder->Recorded().size() << " singleMismatches=" << singleMismatches
                              << " doubleMismatches=" << doubleMismatches << " firstPosition=" << firstPosition << " actualBits=" << firstActual
                              << " bottomMismatches=" << bottomBaselineMismatches << " leadingMismatches=" << ascentLeadingMismatches
                              << " singleBaselineBits=" << firstSingle << " doubleBaselineBits=" << firstDouble
                              << " heightBits=" << std::bit_cast<UINT32>(text.height) << " singleBits=" << std::bit_cast<UINT32>(singleTop)
                              << " doubleBits=" << std::bit_cast<UINT32>(static_cast<float>(doubleTop)) << '\n';
                    std::cout << "rich raw caret font=" << fontSize << " profile=" << profile << " samples=" << caretSamples
                              << " singleMismatches=" << singleCaretMismatches << " doubleMismatches=" << doubleCaretMismatches
                              << " fromBaselineMismatches=" << baselineCaretMismatches << '\n';
                    Require(! recorder->Recorded().empty(), "raw DirectWrite baseline probe emits actual glyph runs");
                }
        }
}
} // namespace RichLineOriginDiagnostics

void TestRichDocumentLayoutRawLineOriginDiagnostics()
{
    RichLineOriginDiagnostics::RunRawLineOrigins();
}
using namespace DxUi::RichText;
wil::com_ptr<IDWriteFactory> DocumentFactory()
{
    wil::com_ptr<IDWriteFactory> factory;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(factory.put())));
    return factory;
}
void Near(float actual, float expected, const char* message)
{
    if (std::abs(actual - expected) > 0.125f)
        std::cerr << message << " actual=" << actual << " expected=" << expected << '\n';
    Require(std::abs(actual - expected) <= 0.125f, message);
}
void CompareDocumentGeometry(PreparedDocumentLayout& prepared, PreparedLayout& flat, std::size_t stride = 1u)
{
    const auto& projection = flat.Content();
    Require(prepared.SourceLength() == projection.source.size() && prepared.SourceMatches(projection.source),
            "chunked source comparison matches every source unit");
    Require(prepared.DisplayText() == projection.text && prepared.Revision() == projection.revision, "chunked text and published revision match flat v0");
    Near(prepared.Height(), flat.Height(), "complete chunked height matches DirectWrite whole-document metrics");
    for (std::size_t position = 0u; position <= projection.source.size(); ++position)
        Require(prepared.ToDisplay(position) == projection.ToDisplay(position), "chunked source mapping covers every ACP");
    for (std::size_t position = 0u;; position = std::min(projection.text.size(), position + stride))
    {
        const auto boundary = static_cast<std::size_t>(std::lower_bound(projection.sourceToDisplay.begin(), projection.sourceToDisplay.end(), position) -
                                                       projection.sourceToDisplay.begin());
        Require(prepared.SourceBoundary(position) == boundary, "visual-line source boundary preserves the earliest hidden-marker affinity");
        if (position < projection.text.size())
            Require(prepared.DisplayUnit(position) == projection.text[position], "indexed display unit matches flat UTF16 including interior ACPs");
        const auto actual = prepared.Caret(position), expected = flat.Caret(position);
        Near(actual.x, expected.x, "chunked caret x matches whole layout");
        Near(actual.y, expected.y, "chunked caret y matches whole layout");
        Near(actual.height, expected.height, "chunked caret height matches whole layout");
        for (const bool trailing : {false, true})
            Require(prepared.ToSource(position, trailing) == projection.ToSource(position, trailing),
                    "chunked display mapping preserves both source affinities");
        Require(prepared.PreviousCluster(position) == flat.PreviousCluster(position) && prepared.NextCluster(position) == flat.NextCluster(position),
                "chunked Unicode navigation matches whole logical elements at all ACPs");
        for (const bool backwards : {false, true})
            Require(prepared.DeletionRange(position, backwards) == flat.DeletionRange(position, backwards), "chunked deletion preserves whole grapheme policy");
        Require(prepared.LineBoundary(position, false) == flat.LineBoundary(position, false) &&
                    prepared.LineBoundary(position, true) == flat.LineBoundary(position, true),
                "chunked wrapped-line navigation matches whole layout");
        if (position == projection.text.size())
            break;
    }
    for (float top = -20.0f; top < flat.Height() + 20.0f; top += std::max(7.0f, flat.Height() / 41.0f))
    {
        const auto actualRange = prepared.VisibleRange(top, top + 57.0f), expectedRange = flat.VisibleRange(top, top + 57.0f);
        if (actualRange != expectedRange)
            std::cerr << "visible top=" << top << " bottom=" << top + 57.0f << " actual=" << actualRange.first << ',' << actualRange.second
                      << " expected=" << expectedRange.first << ',' << expectedRange.second << '\n';
        Require(actualRange == expectedRange, "chunked visible lines match the complete layout");
        for (float x : {-20.0f, 0.0f, 17.0f, 103.0f, 1000.0f})
        {
            const auto actual = prepared.HitTest(x, top), expected = flat.HitTest(x, top);
            Require(actual.position == expected.position && actual.trailing == expected.trailing,
                    "chunked hit tests match whole layout inside/outside viewport");
        }
    }
    for (std::size_t first = 0u; first < projection.text.size(); first += std::max(std::size_t(1u), projection.text.size() / 23u))
    {
        const auto last   = std::min(projection.text.size(), first + 1131u);
        const auto actual = prepared.Range(first, last), expected = flat.Range(first, last);
        Require(actual.size() == expected.size(), "cross-chunk selection has the same rectangle count");
        for (std::size_t index = 0u; index < actual.size(); ++index)
        {
            Near(actual[index].left, expected[index].left, "selection left matches whole layout");
            Near(actual[index].right, expected[index].right, "selection right matches whole layout");
            Near(actual[index].top, expected[index].top, "selection top matches whole layout");
            Near(actual[index].bottom, expected[index].bottom, "selection bottom matches whole layout");
        }
    }
}
void TestRichDocumentLayoutWholeGeometryOracle()
{
    const auto factory         = DocumentFactory();
    const std::wstring pattern = L"# Heading\r\n**bold** a\u0301 \U0001f469\u200d\U0001f4bb and `code`\n"
                                 L"\u05d0\u05d1\u05d2 mixed\rline\u2028separator\u2029paragraph\n\n";
    std::wstring seed;
    for (std::size_t repeat = 0u; repeat < 29u; ++repeat)
        seed += pattern;
    for (const auto ending : {L"end", L"\n", L"\n# ", L"\n**last**\r\n"})
        for (const auto mode : {ViewMode::Source, ViewMode::Rich})
            for (const auto width : {80.0f, 417.0f})
                for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
                {
                    Document document(seed + ending);
                    LayoutOptions options;
                    options.widthDip         = width;
                    options.readingDirection = direction;
                    PreparedDocumentLayout prepared;
                    PreparedLayout flat;
                    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), mode, options);
                    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), mode), options);
                    std::cout << "rich geometry oracle mode=" << static_cast<unsigned>(mode) << " width=" << width << " direction=" << direction
                              << " sourceUnits=" << document.Length() << " endingUnits=" << std::wstring_view(ending).size() << '\n';
                    Require(prepared.LastPreparation().preparedChunks > 1u,
                            "whole-geometry oracle actually prepares multiple chunks before compatibility checks");
                    CompareDocumentGeometry(prepared, flat);
                }
}
void TestRichDocumentLayoutEditsAndFailurePublication()
{
    const auto factory = DocumentFactory();
    std::wstring seed;
    for (std::size_t line = 0u; line < 512u; ++line)
        seed += L"line **bold** \U0001f600\r\n";
    Document document(seed);
    LayoutOptions options;
    options.widthDip = 310;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    const auto originalHeight = prepared.Height();
    for (std::size_t edit = 0u; edit < 160u; ++edit)
    {
        document.Replace(seed.size() / 2u, seed.size() / 2u + 1u, edit % 2u ? L"x" : L"y", {0u, 0u});
        prepared.Invalidate();
        Require(! prepared.Ready() && prepared.Height() == 0.0f, "invalidation hides stale geometry while retaining reusable resources");
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        Require(prepared.LastPreparation().reusedChunks > 0u && prepared.LastPreparation().preparedSourceUnits <= 2048u && prepared.ChunkCount() <= 128u,
                "single-line edit reuses unchanged DirectWrite geometry and bounds rebuilt source");
        if (edit % 31u == 0u)
        {
            PreparedLayout flat;
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
            CompareDocumentGeometry(prepared, flat, 11u);
        }
    }
    Near(prepared.Height(), originalHeight, "nonwrapping source edit retains height");
    const auto generation = prepared.Generation();
    auto invalid          = options;
    invalid.widthDip      = std::numeric_limits<float>::quiet_NaN();
    bool rejected         = false;
    try
    {
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, invalid);
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && prepared.Ready() && prepared.Generation() == generation && prepared.Revision() == document.Revision(),
            "invalid geometry options leave the published source/geometry pair unchanged");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().preparedChunks == 0u && prepared.LastPreparation().reusedChunks == prepared.ChunkCount(),
            "unchanged source/options reuse the complete prepared graph");
    Require(document.Undo(), "chunked document undo fixture");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
    CompareDocumentGeometry(prepared, flat, 7u);
    prepared.Clear();
    Require(! prepared.Ready() && prepared.ChunkCount() == 0u && prepared.Height() == 0.0f, "full retirement discards the complete chunk graph");
    rejected = false;
    try
    {
        static_cast<void>(prepared.Caret(0u));
    }
    catch (const std::logic_error&)
    {
        rejected = true;
    }
    Require(rejected, "retired geometry is unavailable");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    Require(prepared.Ready() && prepared.LastPreparation().reusedChunks == 0u, "retired resources are rebuilt on resumed preparation");
}
void TestRichDocumentLayoutHeightChangesRepositionSuffix()
{
    const auto factory = DocumentFactory();
    std::wstring seed;
    for (std::size_t line = 0u; line < 256u; ++line)
        seed += L"plain **bold** a\u0301 \U0001f600 text\n";
    for (const auto insertion : {L"extra line\n", L"# ", L"a long wrapped first line with many words and more words and still more words "})
    {
        Document document(seed);
        LayoutOptions options;
        options.widthDip    = 180.0f;
        options.fontSizeDip = 13.0f;
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        const auto oldHeight = prepared.Height();
        Require(prepared.ChunkCount() > 2u && ! prepared.UsesWholeGeometry(), "height-change fixture starts with positioned retained chunks");
        document.Replace(0u, 0u, insertion, {0u, 0u});
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        const auto work = prepared.LastPreparation();
        Require(prepared.Height() > oldHeight && work.reusedChunks == 0u && work.preparedSourceUnits == document.Length() && work.wholeLayoutsPrepared == 0u &&
                    ! prepared.UsesWholeGeometry(),
                "changed first-line height rebuilds every dependent document origin and reports its complete source work");
        PreparedLayout flat;
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        CompareDocumentGeometry(prepared, flat, 7u);
        const auto last = document.Length() - 2u;
        document.Replace(last, last + 1u, L"x", {0u, 0u});
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        Require(prepared.LastPreparation().reusedChunks > 0u && prepared.LastPreparation().preparedSourceUnits <= 2048u,
                "a later height-neutral edit retains the correctly repositioned prefix");
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        CompareDocumentGeometry(prepared, flat, 7u);
        Require(document.Undo() && document.Undo(), "height-change undo fixture restores original source");
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        CompareDocumentGeometry(prepared, flat, 7u);
        Near(prepared.Height(), oldHeight, "undo restores the original document height and all suffix coordinates");
    }
}
void TestRichDocumentLayoutPrivateOriginsDoNotEnterProjection()
{
    const auto factory = DocumentFactory();
    const std::wstring prefix(1024u, L'\n');
    Document document(prefix + L"# ");
    LayoutOptions options;
    options.widthDip    = 310.0f;
    options.fontSizeDip = 13.0f;
    PreparedDocumentLayout prepared;
    PreparedLayout flat;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
    Require(prepared.UsesWholeGeometry() && prepared.LastPreparation().preparedChunks == 2u &&
                prepared.LastPreparation().wholeLayoutSourceUnits == document.Length(),
            "an empty displayed suffix at a real chunk boundary retains canonical terminal-font geometry with reported fallback work");
    CompareDocumentGeometry(prepared, flat);
    document.Replace(prefix.size(), document.Length(), L"\ufffc user object marker\n", {0u, 0u});
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
    Require(! prepared.UsesWholeGeometry() && ! prepared.UsesWholeRaster() && prepared.LastPreparation().wholeLayoutsPrepared == 0u &&
                prepared.DisplayText() == prefix + L"\ufffc user object marker\n",
            "a real user FFFC stays in the projection while private prefix units remain absent and obsolete whole resources retire");
    CompareDocumentGeometry(prepared, flat);
}
void TestRichDocumentLayoutLongHeightAndChunkBudget()
{
    const auto factory = DocumentFactory();
    for (const std::wstring seed : {std::wstring(Document::MaximumUnits, L'\n'), std::wstring(Document::MaximumUnits, L'a')})
    {
        Document document(seed);
        LayoutOptions options;
        options.widthDip = 321.25f;
        PreparedDocumentLayout prepared;
        PreparedLayout flat;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        Require(prepared.ChunkCount() <= 128u, "maximum source bounds the entire live document geometry graph");
        if (seed.front() == L'\n')
            Require(! prepared.UsesWholeGeometry() && prepared.LastPreparation().wholeLayoutsPrepared == 0u,
                    "maximum newline geometry actually uses positioned retained chunks without a hidden whole layout");
        CompareDocumentGeometry(prepared, flat, 257u);
    }
}
void TestRichDocumentLayoutWarpRasterOracle()
{
    const auto factory = DocumentFactory();
    std::wstring seed;
    for (std::size_t line = 0u; line < 130u; ++line)
        seed +=
            L"# Heading\n**bold** a\u0301\u0301\u0301\u0301\u0301\u0301\u0301\u0301 *italic* ~~strike~~ \U0001f600 `code`\n\u05d0\u05d1\u05d2 mixed text\r\n";
    GraphicsFixture graphics;
    THROW_IF_FAILED(graphics.Create());
    wil::com_ptr<ID2D1Factory1> d2dFactory;
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(d2dFactory.put())));
    wil::com_ptr<IDXGIDevice> dxgiDevice;
    THROW_IF_FAILED(graphics.device.query_to(dxgiDevice.put()));
    wil::com_ptr<ID2D1Device> device;
    THROW_IF_FAILED(d2dFactory->CreateDevice(dxgiDevice.get(), device.put()));
    wil::com_ptr<ID2D1DeviceContext> context;
    THROW_IF_FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
    wil::com_ptr<IDXGISurface> surface;
    THROW_IF_FAILED(graphics.target.query_to(surface.put()));
    const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    THROW_IF_FAILED(context->CreateBitmapFromDxgiSurface(surface.get(), &properties, bitmap.put()));
    context->SetTarget(bitmap.get());
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush.put()));
    std::size_t comparedPairs = 0u;
    for (unsigned profile = 0u; profile < 6u; ++profile)
    {
        auto input = seed;
        if (profile == 2u)
        {
            for (auto position = input.find(L"# "); position != std::wstring::npos; position = input.find(L"# ", position))
                input.erase(position, 2u);
        }
        if (profile == 5u)
            input.insert(0u, 50000u, L'\n'); // Exercise large document coordinates with populated final viewports.
        Document document(input);
        const auto mode = profile == 1u ? ViewMode::Source : ViewMode::Rich;
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            LayoutOptions options;
            options.widthDip         = 417.0f;
            options.fontSizeDip      = profile == 0u ? 18.0f : profile <= 2u ? 16.0f : profile == 3u ? 13.0f : 18.1f;
            options.readingDirection = direction;
            PreparedDocumentLayout prepared;
            PreparedLayout flat;
            prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), mode, options);
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), mode), options);
            Require(! prepared.UsesWholeGeometry() && prepared.ChunkCount() > 1u && prepared.LastPreparation().wholeLayoutsPrepared == 0u,
                    "every raster profile actually retains positioned chunks without whole geometry or preparation");
            for (unsigned variant = 0u; variant < 3u; ++variant)
            {
                context->SetDpi(variant == 1u ? 144.0f : 96.0f, variant == 1u ? 144.0f : 96.0f);
                context->SetTransform(variant == 1u   ? D2D1::Matrix3x2F::Scale(0.75f, 0.75f)
                                      : variant == 2u ? D2D1::Matrix3x2F::Skew(6.0f, 0.0f)
                                                      : D2D1::Matrix3x2F::Identity());
                const auto viewportHeight = static_cast<float>(graphics.height) / (variant == 1u ? 1.125f : 1.0f);
                size_t viewportIndex      = 0u;
                for (const float top : {0.0f, flat.Height() * 0.5f, std::max(0.0f, flat.Height() - viewportHeight)})
                {
                    const auto raster = [&](bool chunked)
                    {
                        context->BeginDraw();
                        context->Clear(D2D1::ColorF(0, 0));
                        const auto origin = D2D1::Point2F(8.0f, -top);
                        if (chunked)
                            prepared.Draw(context.get(), brush.get(), origin, top, top + viewportHeight);
                        else
                            flat.Draw(context.get(), brush.get(), origin);
                        THROW_IF_FAILED(context->EndDraw());
                        std::vector<std::uint8_t> pixels;
                        THROW_IF_FAILED(graphics.Read(pixels));
                        return pixels;
                    };
                    const auto expected = raster(false), actual = raster(true);
                    const auto capturePath = std::filesystem::path(L".build/reports/rich-document-raster") /
                                             (L"profile-" + std::to_wstring(profile) + L"-direction-" + std::to_wstring(static_cast<unsigned>(direction)) +
                                              L"-variant-" + std::to_wstring(variant) + L"-viewport-" + std::to_wstring(viewportIndex++));
                    Require(SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-expected.png", {graphics.width, graphics.height, expected}) &&
                                SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-actual.png", {graphics.width, graphics.height, actual}),
                            "retain actual and reference WARP pixels with this native test receipt");
                    if (actual != expected)
                    {
                        std::size_t differences = 0u;
                        for (std::size_t index = 0u; index < actual.size(); ++index)
                            differences += actual[index] != expected[index];
                        std::cerr << "chunk raster profile=" << profile << " direction=" << direction << " variant=" << variant << " top=" << top
                                  << " differingBytes=" << differences << '\n';
                    }
                    Require(actual == expected, "chunked visible raster matches whole-document DirectWrite pixels exactly on WARP");
                    ++comparedPairs;
                    Require(! prepared.UsesWholeRaster(), "axis-aligned and skewed raster profiles actually draw positioned chunks without whole fallback");
                    if (viewportIndex == 3u)
                        Require(std::any_of(expected.begin(), expected.end(), [](std::uint8_t value) { return value != 0u; }),
                                "every final viewport contains actual reference glyph ink, including the large-origin profile");
                }
            }
        }
    }
    Require(comparedPairs == 108u, "all original and added font/large-origin WARP pixel pairs execute");
    std::cout << "rich positioned raster exactPairs=" << comparedPairs << " fontProfiles=6 directions=2 transforms=3 viewports=3\n";
}
void TestRichDocumentLayoutBranchesOptionsAndCompaction()
{
    const auto factory = DocumentFactory();
    // Integer font metrics under 2^24 keep this fragmentation fixture in the
    // exact-placement path. Rounding-sensitive fallback is tested separately.
    Document document(std::wstring(4096u, L'\n'));
    LayoutOptions options;
    options.widthDip    = 321.25f;
    options.fontSizeDip = 2048.0f;
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    bool compacted = false;
    for (size_t edit = 0u; edit < 512u; ++edit)
    {
        const auto position = (edit * 997u + 512u) % document.Length();
        document.Replace(position, position, L"x\n", {0u, 0u});
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        compacted = compacted || prepared.LastPreparation().preparedChunks > prepared.ChunkCount();
        Require(prepared.ChunkCount() <= 128u && prepared.SourceMatches(document.Text()),
                "fragmenting edits keep the published COM graph and exact source bounded");
    }
    Require(compacted, "fragmentation fixture actually exercises bounded compaction rather than only its count predicate");
    Document sibling = document;
    document.Replace(0u, 1u, L"a", {0u, 0u});
    sibling.Replace(2u, 3u, L"b", {0u, 0u});
    Require(document.Revision() == sibling.Revision(), "geometry sibling fixture has equal numeric revisions");
    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
    prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.SourceMatches(sibling.Text()) && ! prepared.SourceMatches(document.Text()),
            "geometry compatibility checks source identity despite equal caller revision");
    for (size_t setting = 0u; setting < 6u; ++setting)
    {
        auto changed = options;
        switch (setting)
        {
            case 0u: changed.widthDip += 1.0f; break;
            case 1u: changed.fontSizeDip += 1.0f; break;
            case 2u: changed.fontFamily = L"Consolas"; break;
            case 3u: changed.codeFontFamily = L"Segoe UI"; break;
            case 4u: changed.locale = L"fr-FR"; break;
            default: changed.readingDirection = DWRITE_READING_DIRECTION_RIGHT_TO_LEFT; break;
        }
        prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, changed);
        Require(prepared.LastPreparation().reusedChunks == 0u && prepared.LastPreparation().preparedSourceUnits == sibling.Length(),
                "all typography/width/locale/direction changes invalidate incompatible geometry");
        prepared.Prepare(factory.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    }
    wil::com_ptr<IDWriteFactory> isolated;
    THROW_IF_FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(isolated.put())));
    prepared.Prepare(isolated.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().reusedChunks == 0u, "different retained factory identity cannot reuse previous geometry");
    for (size_t edit = 0u; edit < 160u; ++edit)
        sibling.Replace(0u, 1u, edit % 2u ? L"c" : L"d", {0u, 0u});
    prepared.Prepare(isolated.get(), sibling.Snapshot(), sibling.Revision(), ViewMode::Rich, options);
    Require(prepared.LastPreparation().reusedChunks == 0u && prepared.LastPreparation().preparedSourceUnits == sibling.Length(),
            "expired bounded source ancestry falls back to complete preparation");
}
void DrainDocumentPreparation(PreparedDocumentLayout& layout, const LayoutWorkBudget& budget)
{
    for (size_t call = 0u; call < 200000u; ++call)
    {
        const auto advance = layout.AdvancePreparation(budget);
        Require(advance.layoutsPrepared <= budget.maximumLayouts && advance.blockSteps <= budget.maximumBlockSteps,
                "each cooperative advance obeys both independent work budgets");
        if (advance.status == LayoutPreparationStatus::Complete)
            return;
        Require(advance.status == LayoutPreparationStatus::Pending && layout.HasPendingPreparation(),
                "incomplete preparation retains an explicit continuation");
    }
    Require(false, "finite document preparation must terminate within its independent test bound");
}
void TestRichDocumentLayoutCooperativeBudgetsAndGeometry()
{
    const auto factory = DocumentFactory();
    std::wstring source;
    for (size_t line = 0u; line < 160u; ++line)
        source += line % 7u ? L"mixed **bold** a\u0301 \U0001f600 \u05d0\u05d1\r\n" : L"# Heading\n";
    Document document(source);
    for (const auto font : {13.0f, 18.1f, 0.0001f})
    {
        LayoutOptions options;
        options.widthDip    = 273.25f;
        options.fontSizeDip = font;
        PreparedLayout flat;
        flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        for (const auto budget : {LayoutWorkBudget{1u, 1u}, LayoutWorkBudget{2u, 17u}, LayoutWorkBudget{128u, 100000u}})
        {
            PreparedDocumentLayout layout;
            const auto generation = layout.Generation();
            Require(layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options) ==
                            LayoutPreparationStatus::Pending &&
                        ! layout.Ready() && layout.Generation() == generation,
                    "begin stages source/projection without publishing incomplete geometry");
            DrainDocumentPreparation(layout, budget);
            Require(layout.Generation() == generation + 1u && ! layout.HasPendingPreparation(), "one coherent publication increments generation once");
            CompareDocumentGeometry(layout, flat, 17u);
            Require(layout.AdvancePreparation(budget).status == LayoutPreparationStatus::Idle, "advancing completed work does not republish it");
            document.Replace(0u, 0u, L"inserted\n", {0u, 0u});
            layout.Invalidate();
            layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
            DrainDocumentPreparation(layout, budget);
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
            CompareDocumentGeometry(layout, flat, 19u);
            Require(document.Undo(), "cooperative geometry fixture restores the original source");
            flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Rich), options);
        }
    }
}
void TestRichDocumentLayoutCoalescingCancellationAndFailure()
{
    const auto factory = DocumentFactory();
    LayoutOptions options;
    options.widthDip = 310.0f;
    Document original(L"published **source**"), candidate(std::wstring(5120u, L'x'));
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), original.Snapshot(), original.Revision(), ViewMode::Rich, options);
    const auto generation = layout.Generation();
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    const auto first = layout.AdvancePreparation({1u, 1u});
    Require(first.status == LayoutPreparationStatus::Pending && layout.SourceMatches(original.Text()) && layout.Generation() == generation,
            "pending work leaves the previous published source/geometry pair intact");
    Require(layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options) == LayoutPreparationStatus::Pending,
            "identical request coalesces without resetting continuation");
    Require(layout.PendingPreparation().preparedChunks == 1u, "coalescing preserves work already completed by the previous advance");
    layout.CancelPreparation();
    Require(! layout.HasPendingPreparation() && layout.Ready() && layout.SourceMatches(original.Text()) && layout.Generation() == generation,
            "cancellation retires only staging and does not fabricate a publication");
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    Document replacement(L"different identity, equal caller revision");
    layout.BeginPreparation(factory.get(), replacement.Snapshot(), candidate.Revision(), ViewMode::Source, options);
    DrainDocumentPreparation(layout, {1u, 1u});
    Require(layout.SourceMatches(replacement.Text()) && ! layout.SourceMatches(candidate.Text()), "latest source/mode wins despite equal numeric revision");
    const auto publishedGeneration = layout.Generation();
    auto invalid                   = options;
    invalid.widthDip               = std::numeric_limits<float>::quiet_NaN();
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, invalid);
    bool rejected = false;
    try
    {
        DrainDocumentPreparation(layout, {1u, 1u});
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && ! layout.HasPendingPreparation() && layout.Generation() == publishedGeneration && layout.SourceMatches(replacement.Text()),
            "failed advance discards staging and preserves the published pair");
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    rejected = false;
    try
    {
        static_cast<void>(layout.AdvancePreparation({0u, 1u}));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected && layout.HasPendingPreparation(), "invalid work budget leaves a valid pending request intact");
    layout.Invalidate();
    Require(! layout.Ready() && ! layout.HasPendingPreparation(), "invalidation cancels an obsolete request and rejects old geometry");
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    layout.Clear();
    Require(! layout.Ready() && ! layout.HasPendingPreparation() && layout.ChunkCount() == 0u, "clear retires published and staged graphs");
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    DrainDocumentPreparation(layout, {1u, 1u});
    Require(layout.SourceMatches(candidate.Text()), "retired or failed preparation can be explicitly retried");
}
} // namespace
void RunRichDocumentLayoutTests()
{
    // Raw DirectWrite observations guide the retained renderer; repeated large
    // diagnostic cases run only under an explicit named selection.
    if (GetDxUiTestFilter().active)
        DXUI_RUN_TEST(TestRichDocumentLayoutRawLineOriginDiagnostics);

    DXUI_RUN_TEST(TestRichDocumentLayoutWholeGeometryOracle);
    DXUI_RUN_TEST(TestRichDocumentLayoutEditsAndFailurePublication);
    DXUI_RUN_TEST(TestRichDocumentLayoutHeightChangesRepositionSuffix);
    DXUI_RUN_TEST(TestRichDocumentLayoutPrivateOriginsDoNotEnterProjection);
    DXUI_RUN_TEST(TestRichDocumentLayoutLongHeightAndChunkBudget);
    DXUI_RUN_TEST(TestRichDocumentLayoutWarpRasterOracle);
    DXUI_RUN_TEST(TestRichDocumentLayoutBranchesOptionsAndCompaction);
    DXUI_RUN_TEST(TestRichDocumentLayoutCooperativeBudgetsAndGeometry);
    DXUI_RUN_TEST(TestRichDocumentLayoutCoalescingCancellationAndFailure);
}
