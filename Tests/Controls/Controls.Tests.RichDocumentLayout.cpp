#include "Controls.Tests.RichDocumentLayout.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls/DxUi.PreparedLayoutAccess.h"
#include "Controls/DxUi.RichLayoutOptions.h"
#include "GraphicsFixture.h"
#include "Support.Tests.UniformSourceChunks.h"
#include "Support/RetainedCaches.Tests.Hooks.h"
#include "Support/WorkCounters.h"
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <d2d1_3.h>
#include <dwrite_3.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <vector>
#include <wil/result.h>
// Windows SDK WRL owns its interlocked/volatile implementation and target semantics.
// Scope is this system header only. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)

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
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)
// Stack-only rounding/observation barrier; no inter-thread synchronization or memory-mapped I/O.
// Both target volatile models preserve these accesses. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
inline float Add(float first, float second) noexcept
{
    volatile float result = first + second;
    return result;
}
#pragma warning(pop)
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
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
#pragma warning(pop)
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
// W7.3 contract (Core_PerformanceAndResources.md, rich document geometry): a chunk is placed at the exact (double) sum
// of the line heights before it, rounded once. The chunked geometry is checked against those exact sums, computed here
// from one whole layout's line heights (which do not drift), not against the whole layout's own single-precision line
// tops (which do). Tolerance: 1/64 DIP below 2^17 DIP, beyond that one single-precision unit of the coordinate plus the
// same 1/64 DIP.
constexpr double kLineTopTolerance = 1.0 / 64.0;
// Largest |chunked coordinate - exact sum|: over all checked coordinates, and over chunks' first lines only (where the
// placement alone decides the coordinate and the contract's 1/64 DIP applies without an in-chunk term).
thread_local double g_largestLineTopDifference{}, g_largestWholeLayoutDrift{}, g_largestChunkStartDifference{};
[[nodiscard]] double LineTopTolerance(double exact)
{
    const auto magnitude = static_cast<float>(std::abs(exact));
    return magnitude < 131072.0f ? kLineTopTolerance : kLineTopTolerance + static_cast<double>(std::nextafter(magnitude, INFINITY) - magnitude);
}
// The whole layout's lines: display starts and the exact tops (one more than lines: the document bottom).
struct ExactLineTops
{
    std::vector<std::size_t> starts;
    std::vector<double> exact;
    std::size_t textSize{};
    explicit ExactLineTops(const PreparedLayout& flat) : textSize(flat.Content().text.size())
    {
        double sum        = 0.0;
        std::size_t start = 0u;
        for (const auto& line : Detail::PreparedLayoutAccess::LineMetrics(flat))
        {
            starts.push_back(start);
            exact.push_back(sum);
            sum += line.height;
            start += line.length;
        }
        exact.push_back(sum);
    }
    [[nodiscard]] std::size_t LineAt(std::size_t position) const
    {
        const auto found = std::upper_bound(starts.begin(), starts.end(), position);
        return found == starts.begin() ? 0u : static_cast<std::size_t>(found - starts.begin()) - 1u;
    }
    // The line whose exact box contains y (clamped to the first and last line).
    [[nodiscard]] std::size_t LineAtY(double y) const
    {
        const auto found = std::upper_bound(exact.begin(), exact.end() - 1, y);
        return found == exact.begin() ? 0u : std::min(starts.size() - 1u, static_cast<std::size_t>(found - exact.begin()) - 1u);
    }
    // PreparedLayout::VisibleRange's rule on the exact line boxes.
    [[nodiscard]] std::pair<std::size_t, std::size_t> Visible(double top, double bottom) const
    {
        std::optional<std::size_t> first;
        std::size_t last = 0u;
        for (std::size_t line = 0u; line < starts.size(); ++line)
        {
            if (exact[line] >= bottom)
                break;
            if (exact[line + 1u] > top)
            {
                if (! first)
                    first = starts[line];
                last = line + 1u < starts.size() ? starts[line + 1u] : textSize;
            }
        }
        return {first.value_or(0u), last};
    }
    // The nearest exact line top to y.
    [[nodiscard]] double NearestTop(double y) const
    {
        const auto found = std::lower_bound(exact.begin(), exact.end(), y);
        double best      = found == exact.end() ? exact.back() : *found;
        if (found != exact.begin() && std::abs(*std::prev(found) - y) < std::abs(best - y))
            best = *std::prev(found);
        return best;
    }
};
// `inChunk`: DirectWrite's own single-precision accumulation of the lines before this one inside its chunk (half a
// unit of the chunk-local coordinate per line), the only term the placement does not make exact.
void NearExact(float actual, double exact, const char* message, double inChunk = 0.0)
{
    const auto difference      = std::abs(static_cast<double>(actual) - exact);
    g_largestLineTopDifference = std::max(g_largestLineTopDifference, difference);
    if (inChunk == 0.0)
        g_largestChunkStartDifference = std::max(g_largestChunkStartDifference, difference);
    const auto allowed = LineTopTolerance(exact) + inChunk;
    if (difference > allowed)
        std::cerr << message << " actual=" << actual << " exact=" << exact << " inChunk=" << inChunk << '\n';
    Require(difference <= allowed, message);
}
// The in-chunk accumulation bound of each whole-layout line: lines before it in its chunk times half a single-precision
// unit at its chunk-local top.
std::vector<double> InChunkAllowances(const PreparedDocumentLayout& prepared, const ExactLineTops& tops)
{
    const auto starts = Detail::PreparedDocumentLayoutAccess::ChunkDisplayStarts(prepared);
    std::vector<double> allowance(tops.starts.size() + 1u, 0.0);
    std::size_t chunk = 0u, chunkLine = 0u;
    for (std::size_t line = 0u; line < tops.starts.size(); ++line)
    {
        while (chunk + 1u < starts.size() && starts[chunk + 1u] <= tops.starts[line])
        {
            ++chunk;
            chunkLine = line;
        }
        const auto local = static_cast<float>(tops.exact[line] - tops.exact[chunkLine]);
        allowance[line]  = 0.5 * static_cast<double>(line - chunkLine) * static_cast<double>(std::nextafter(local, INFINITY) - local);
    }
    allowance.back() = allowance[tops.starts.size() - 1u] * 2.0;
    return allowance;
}
// `longChunks`: the document has chunks of about a thousand lines (empty-line runs), where DirectWrite's own in-chunk
// accumulation can exceed 1/64 DIP; only then is that term allowed (and bounded). Every other document must meet
// 1/64 DIP against the exact sums at every line, as the contract promises.
void CompareDocumentGeometry(PreparedDocumentLayout& prepared, PreparedLayout& flat, std::size_t stride = 1u, bool longChunks = false)
{
    const ExactLineTops tops(flat);
    auto inChunk = InChunkAllowances(prepared, tops);
    if (! longChunks)
        std::fill(inChunk.begin(), inChunk.end(), 0.0);
    const auto& projection = flat.Content();
    Require(prepared.SourceLength() == projection.source.size() && prepared.SourceMatches(projection.source),
            "chunked source comparison matches every source unit");
    Require(prepared.DisplayText() == projection.text && prepared.Revision() == projection.revision, "chunked text and published revision match flat v0");
    // The whole-layout fallback is that layout: compared exactly. Chunked geometry is compared with the exact sums.
    const bool whole = prepared.UsesWholeGeometry();
    if (whole)
        Require(prepared.Height() == flat.Height(), "whole-layout fallback height is the whole layout's");
    else
        NearExact(prepared.Height(), tops.exact.back(), "complete chunked height is the exact sum of line heights", inChunk.back());
    // The whole layout's own drift from the exact sums, recorded for the receipts (measured at its line-start carets).
    for (std::size_t line = 0u; line < tops.starts.size(); line += std::max<std::size_t>(1u, tops.starts.size() / 64u))
        g_largestWholeLayoutDrift = std::max(g_largestWholeLayoutDrift, std::abs(static_cast<double>(flat.Caret(tops.starts[line]).y) - tops.exact[line]));
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
        Near(actual.height, expected.height, "chunked caret height matches whole layout");
        if (whole)
            Require(actual.y == expected.y, "whole-layout fallback caret is the whole layout's");
        else
        {
            // The caret box meets its line's exact box (a caret may reach above or below a uniformly spaced line).
            const auto line  = tops.LineAt(position);
            const auto slack = LineTopTolerance(tops.exact[line + 1u]) + inChunk[line];
            Require(static_cast<double>(actual.y) < tops.exact[line + 1u] + slack &&
                        static_cast<double>(actual.y) + static_cast<double>(actual.height) > tops.exact[line] - slack,
                    "chunked caret box meets its line's exact box");
        }
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
    // Every line's top (a selection rectangle over its first unit spans the line box) at its exact sum.
    if (! whole)
        for (std::size_t line = 0u; line < tops.starts.size(); line += std::max<std::size_t>(1u, stride / 4u))
        {
            if (tops.starts[line] >= projection.text.size())
                continue;
            const auto rectangles = prepared.Range(tops.starts[line], tops.starts[line] + 1u);
            Require(! rectangles.empty(), "every line's first unit has a selection rectangle");
            NearExact(rectangles.front().top, tops.exact[line], "chunked line top is the exact sum of the line heights before it", inChunk[line]);
            NearExact(rectangles.front().bottom, tops.exact[line + 1u], "chunked line bottom is the exact sum through that line", inChunk[line]);
        }
    for (float top = -20.0f; top < flat.Height() + 20.0f; top += std::max(7.0f, flat.Height() / 41.0f))
    {
        const auto actualRange = prepared.VisibleRange(top, top + 57.0f);
        if (whole)
        {
            Require(actualRange == flat.VisibleRange(top, top + 57.0f), "whole-layout fallback visible lines are the whole layout's");
            continue;
        }
        // The visible lines follow PreparedLayout::VisibleRange's rule (a line's caret box meets the viewport) on the
        // chunked carets, whose placement the line-top checks above bind to the exact sums.
        std::optional<std::size_t> firstVisible;
        std::size_t lastVisible = 0u;
        for (std::size_t line = 0u; line < tops.starts.size(); ++line)
        {
            const auto caret = prepared.Caret(tops.starts[line]);
            if (caret.y >= top + 57.0f)
                break;
            if (caret.y + caret.height > top)
            {
                if (! firstVisible)
                    firstVisible = tops.starts[line];
                lastVisible = line + 1u < tops.starts.size() ? tops.starts[line + 1u] : projection.text.size();
            }
        }
        const std::pair<std::size_t, std::size_t> expectedRange{firstVisible.value_or(0u), lastVisible};
        if (actualRange != expectedRange)
            std::cerr << "visible top=" << top << " actual=" << actualRange.first << ',' << actualRange.second << " expected=" << expectedRange.first << ','
                      << expectedRange.second << '\n';
        Require(actualRange == expectedRange, "chunked visible lines are the lines whose placed caret boxes meet the viewport");
        // A hit lands in the line whose exact box contains the point.
        for (float x : {-20.0f, 0.0f, 17.0f, 103.0f, 1000.0f})
        {
            const auto hit      = prepared.HitTest(x, top);
            const auto expected = tops.LineAtY(top);
            auto line           = tops.LineAt(hit.position);
            if (hit.trailing && line > expected && hit.position == tops.starts[line])
                line = expected; // a trailing hit at a line's end reports the next line's start
            const auto nearBoundary = std::abs(static_cast<double>(top) - tops.NearestTop(top)) <= LineTopTolerance(top) + inChunk[expected];
            Require(line == expected || (nearBoundary && (line + 1u == expected || line == expected + 1u)),
                    "chunked hit tests land in the line whose exact box contains the point");
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
            if (whole)
                Require(actual[index].top == expected[index].top && actual[index].bottom == expected[index].bottom,
                        "whole-layout fallback selection is the whole layout's");
            else
            {
                const auto line = tops.LineAtY(actual[index].top + 0.5f);
                NearExact(actual[index].top, tops.NearestTop(actual[index].top), "selection top is an exact line top", inChunk[line]);
                NearExact(actual[index].bottom, tops.NearestTop(actual[index].bottom), "selection bottom is an exact line bottom", inChunk[line]);
            }
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
void TestRichDocumentLayoutWholeReferenceReuseAndStyleChanges()
{
    const auto factory          = DocumentFactory();
    const std::wstring original = L"plain\nline\n*tail*\nend";
    LayoutOptions options;
    options.widthDip             = 310.0f;
    options.lineHeightMultiplier = 1.25f;
    Document initial(original);
    PreparedDocumentLayout prepared;
    prepared.Prepare(factory.get(), initial.Snapshot(), initial.Revision(), ViewMode::Rich, options);
    // W7.3: an explicit line height outside Source is laid in chunks spaced with the whole document's inputs.
    Require(! prepared.UsesWholeGeometry() && prepared.LastPreparation().wholeLayoutsPrepared == 0u, "explicit line height lays chunks, not a whole layout");
    for (const auto replacement :
         {std::wstring_view(original), std::wstring_view(L"plain\nline\n**tail**\nend"), std::wstring_view(L"plain\nline\n**tall**\nend")})
    {
        // A different document prevents the unchanged-snapshot fast path.
        Document candidate(replacement);
        Require(candidate.Snapshot().ChangedRangeFrom(initial.Snapshot()).kind == SourceChangeKind::FullReplacement,
                "whole-reference comparison fixture has independent source ancestry");
        prepared.Prepare(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
        Require(prepared.LastPreparation().wholeLayoutsReused == 0u && prepared.LastPreparation().wholeLayoutsPrepared == 0u && ! prepared.UsesWholeGeometry(),
                "styled multiplier documents keep chunk geometry");
        PreparedLayout flat;
        flat.Prepare(factory.get(), Project(candidate.Snapshot(), candidate.Revision(), ViewMode::Rich), options);
        CompareDocumentGeometry(prepared, flat);
    }
}
void TestRichDocumentLayoutUniformSourceReuse()
{
    const auto factory = DocumentFactory();
    UniformSourceTests::EditReuse(factory.get(), Require);
    std::wstring seed;
    for (unsigned line = 0u; line < 80u; ++line)
        seed += L"source a\u0301 \U0001f600 and \u05d0\u05d1\u05d2\r\n";
    for (const auto multiplier : {1.0f, 1.6f, 4.0f})
        for (const auto width : {80.f, 417.f})
            for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
                for (const auto ending : {L"end", L"\n"})
                {
                    Document document(seed + ending);
                    LayoutOptions options;
                    options.widthDip             = width;
                    options.lineHeightMultiplier = multiplier;
                    options.readingDirection     = direction;
                    PreparedDocumentLayout prepared;
                    PreparedLayout flat;
                    prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, options);
                    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), options);
                    Require(! prepared.UsesWholeGeometry() && prepared.ChunkCount() > 1u, "uniform Source oracle retains actual independent chunks");
                    CompareDocumentGeometry(prepared, flat, 17u);
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
    rejected              = prepared.TryPrepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, invalid) == E_INVALIDARG;
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
    CaretGeometry unavailable{};
    rejected = prepared.TryCaret(0u, unavailable) == E_UNEXPECTED;
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
        // W7.3: chunks carry no origin, so a height change re-lays only the edited chunk and moves the rest.
        Require(prepared.Height() > oldHeight && work.reusedChunks + 2u >= prepared.ChunkCount() && work.preparedChunks <= 2u &&
                    work.preparedSourceUnits <= 2048u && work.wholeLayoutsPrepared == 0u && ! prepared.UsesWholeGeometry(),
                "changed first-line height re-lays at most the edited chunks and repositions every later chunk");
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
        CompareDocumentGeometry(prepared, flat, 257u, true); // 65,536 one-unit lines: chunks of 1,024 lines
    }
}
// W7.3 oracle: an incrementally prepared document (an Enter near its top and a character in its middle, so later chunks
// move without being laid again) draws exactly the WARP BGRA pixels of a fresh preparation with the same chunking, and
// every caret is bit-identical, for fractional fonts, right-to-left text, scaled and skewed drawing and large origins.
// Its geometry stays within the 1/64 DIP line-top contract of one whole layout. With a deliberately wrong placement
// offset (Detail::testRichChunkOffsetError) the same comparison must fail.
struct RasterTarget
{
    GraphicsFixture graphics;
    wil::com_ptr<ID2D1DeviceContext> context;
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    RasterTarget()
    {
        THROW_IF_FAILED(graphics.Create());
        wil::com_ptr<ID2D1Factory1> d2dFactory;
        THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(d2dFactory.put())));
        wil::com_ptr<IDXGIDevice> dxgiDevice;
        THROW_IF_FAILED(graphics.device.query_to(dxgiDevice.put()));
        wil::com_ptr<ID2D1Device> device;
        THROW_IF_FAILED(d2dFactory->CreateDevice(dxgiDevice.get(), device.put()));
        THROW_IF_FAILED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context.put()));
        wil::com_ptr<IDXGISurface> surface;
        THROW_IF_FAILED(graphics.target.query_to(surface.put()));
        const auto properties = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        wil::com_ptr<ID2D1Bitmap1> bitmap;
        THROW_IF_FAILED(context->CreateBitmapFromDxgiSurface(surface.get(), &properties, bitmap.put()));
        context->SetTarget(bitmap.get());
        THROW_IF_FAILED(context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush.put()));
    }
    std::vector<std::uint8_t> Draw(const PreparedDocumentLayout& layout, float top, float viewportHeight)
    {
        context->BeginDraw();
        context->Clear(D2D1::ColorF(0, 0));
        layout.Draw(context.get(), brush.get(), D2D1::Point2F(8.0f, -top), top, top + viewportHeight);
        THROW_IF_FAILED(context->EndDraw());
        std::vector<std::uint8_t> pixels;
        THROW_IF_FAILED(graphics.Read(pixels));
        return pixels;
    }
};
std::wstring RasterProfileSource(unsigned profile)
{
    std::wstring seed;
    for (std::size_t line = 0u; line < 130u; ++line)
        seed +=
            L"# Heading\n**bold** a\u0301\u0301\u0301\u0301\u0301\u0301\u0301\u0301 *italic* ~~strike~~ \U0001f600 `code`\n\u05d0\u05d1\u05d2 mixed text\r\n";
    if (profile == 2u)
    {
        for (auto position = seed.find(L"# "); position != std::wstring::npos; position = seed.find(L"# ", position))
            seed.erase(position, 2u);
    }
    if (profile == 5u)
        seed.insert(0u, 50000u, L'\n'); // Exercise large document coordinates with populated final viewports.
    if (profile == 8u)
        seed.insert(0u, 1000u, L'\n');
    return seed;
}
LayoutOptions RasterProfileOptions(unsigned profile, DWRITE_READING_DIRECTION direction)
{
    LayoutOptions options;
    options.widthDip             = 417.0f;
    options.fontSizeDip          = profile == 0u || profile == 6u ? 18.0f : profile <= 2u ? 16.0f : profile == 3u ? 13.0f : 18.1f;
    options.lineHeightMultiplier = profile >= 6u ? 1.6f : 0.f;
    options.readingDirection     = direction;
    return options;
}
ViewMode RasterProfileMode(unsigned profile)
{
    return profile == 1u || profile >= 6u ? ViewMode::Source : ViewMode::Rich;
}
// Prepares `incremental` through two edits from the profile's source and `fresh` with the incremental's chunk starts.
void PrepareIncrementalAndFresh(IDWriteFactory* factory,
                                unsigned profile,
                                DWRITE_READING_DIRECTION direction,
                                PreparedDocumentLayout& incremental,
                                PreparedDocumentLayout& fresh,
                                Document& document)
{
    const auto options              = RasterProfileOptions(profile, direction);
    const auto mode                 = RasterProfileMode(profile);
    Detail::testRichRecordChunkPlan = true;
    const auto stopRecording        = wil::scope_exit([] { Detail::testRichRecordChunkPlan = false; });
    incremental.Prepare(factory, document.Snapshot(), document.Revision(), mode, options);
    const auto firstLine = document.Text().find(L'\n');
    document.Replace(firstLine, firstLine, L"\n", {0u, 0u}); // an Enter above every later chunk
    incremental.Prepare(factory, document.Snapshot(), document.Revision(), mode, options);
    Require(incremental.LastPreparation().preparedChunks <= 2u && incremental.LastPreparation().reusedChunks + 2u >= incremental.ChunkCount(),
            "an Enter above later chunks re-lays at most two chunks");
    const auto middle = document.Text().find(L' ', document.Length() / 2u); // a boundary inside no surrogate pair or CRLF
    document.Replace(middle, middle, L"x", {0u, 0u});
    incremental.Prepare(factory, document.Snapshot(), document.Revision(), mode, options);
    Require(incremental.LastPreparation().reusedChunks > 0u && ! incremental.UsesWholeGeometry(), "the edited preparation reuses chunks");
    DxUi::Detail::SetFreshPreparationForTest(true);
    const auto notFresh = wil::scope_exit([] { DxUi::Detail::SetFreshPreparationForTest(false); });
    fresh.Prepare(factory, document.Snapshot(), document.Revision(), mode, options);
    Require(fresh.LastPreparation().reusedChunks == 0u && fresh.ChunkCount() == incremental.ChunkCount(),
            "the fresh preparation lays every chunk again with the incremental chunking");
}
void TestRichDocumentLayoutWarpRasterOracle()
{
    const auto factory = DocumentFactory();
    RasterTarget raster;
    std::size_t comparedPairs  = 0u;
    g_largestLineTopDifference = g_largestWholeLayoutDrift = 0.0;
    for (unsigned profile = 0u; profile < 9u; ++profile)
    {
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            Document document(RasterProfileSource(profile));
            PreparedDocumentLayout incremental, fresh;
            PrepareIncrementalAndFresh(factory.get(), profile, direction, incremental, fresh, document);
            // Every chunk equals a fresh layout of it at origin 0, placed at the same offset: carets are bit-identical.
            const auto length = incremental.DisplayLength();
            for (std::size_t position = 0u; position <= length; position += std::max<std::size_t>(1u, length / 997u))
            {
                const auto a = incremental.Caret(position), b = fresh.Caret(position);
                Require(std::bit_cast<std::uint32_t>(a.x) == std::bit_cast<std::uint32_t>(b.x) &&
                            std::bit_cast<std::uint32_t>(a.y) == std::bit_cast<std::uint32_t>(b.y) &&
                            std::bit_cast<std::uint32_t>(a.height) == std::bit_cast<std::uint32_t>(b.height),
                        "incremental chunk geometry is bit-identical with a fresh preparation of the same chunking");
            }
            PreparedLayout flat;
            flat.Prepare(
                factory.get(), Project(document.Snapshot(), document.Revision(), RasterProfileMode(profile)), RasterProfileOptions(profile, direction));
            g_largestLineTopDifference = g_largestChunkStartDifference = 0.0;
            // Profiles 5 and 8 begin with 50,000 and 1,000 empty lines: chunks of about 1,000 lines.
            CompareDocumentGeometry(incremental, flat, profile == 5u ? 4099u : 61u, profile == 5u || profile == 8u);
            std::cout << "  profile=" << profile << " direction=" << direction << " height=" << flat.Height()
                      << " largestLineTopDifference=" << g_largestLineTopDifference << " atChunkStarts=" << g_largestChunkStartDifference << '\n';
            // Placement: each chunk's first line lies at the exact (double) sum of every line height before it, rounded
            // once to single precision, independent of how the document was chunked.
            {
                std::vector<std::size_t> lineStarts;
                std::vector<double> exactTops;
                double sum        = 0.0;
                std::size_t start = 0u;
                for (const auto& line : Detail::PreparedLayoutAccess::LineMetrics(flat))
                {
                    lineStarts.push_back(start);
                    exactTops.push_back(sum);
                    sum += line.height;
                    start += line.length;
                }
                std::size_t placed = 0u;
                for (const auto chunkStart : Detail::testRichPublishedChunkStarts)
                {
                    const auto display = incremental.ToDisplay(chunkStart);
                    const auto line    = std::lower_bound(lineStarts.begin(), lineStarts.end(), display);
                    if (display >= incremental.DisplayLength() || line == lineStarts.end() || *line != display)
                        continue;
                    const auto rectangles = incremental.Range(display, display + 1u);
                    Require(! rectangles.empty(), "a chunk's first unit has a selection rectangle");
                    const auto exactTop = exactTops[static_cast<std::size_t>(line - lineStarts.begin())];
                    const auto top      = rectangles.front().top;
                    Require(std::abs(static_cast<double>(top) - exactTop) <= static_cast<double>(std::nextafter(std::abs(top), INFINITY) - std::abs(top)),
                            "a chunk is placed at the exact sum of the line heights before it");
                    ++placed;
                }
                Require(placed > 1u, "the placement check sees several chunks");
            }
            for (unsigned variant = 0u; variant < 3u; ++variant)
            {
                raster.context->SetDpi(variant == 1u ? 144.0f : 96.0f, variant == 1u ? 144.0f : 96.0f);
                raster.context->SetTransform(variant == 1u   ? D2D1::Matrix3x2F::Scale(0.75f, 0.75f)
                                             : variant == 2u ? D2D1::Matrix3x2F::Skew(6.0f, 0.0f)
                                                             : D2D1::Matrix3x2F::Identity());
                const auto viewportHeight = static_cast<float>(raster.graphics.height) / (variant == 1u ? 1.125f : 1.0f);
                size_t viewportIndex      = 0u;
                for (const float top : {0.0f, incremental.Height() * 0.5f, std::max(0.0f, incremental.Height() - viewportHeight)})
                {
                    const auto expected = raster.Draw(fresh, top, viewportHeight), actual = raster.Draw(incremental, top, viewportHeight);
                    const auto capturePath = std::filesystem::path(L".build/reports/rich-document-raster") /
                                             (L"profile-" + std::to_wstring(profile) + L"-direction-" + std::to_wstring(static_cast<unsigned>(direction)) +
                                              L"-variant-" + std::to_wstring(variant) + L"-viewport-" + std::to_wstring(viewportIndex++));
                    Require(SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-expected.png",
                                                                    {raster.graphics.width, raster.graphics.height, expected}) &&
                                SaveWindowHostBitmapCaptureAsPngForTest(capturePath.wstring() + L"-actual.png",
                                                                        {raster.graphics.width, raster.graphics.height, actual}),
                            "retain actual and reference WARP pixels with this native test receipt");
                    if (actual != expected)
                    {
                        std::size_t differences = 0u;
                        for (std::size_t index = 0u; index < actual.size(); ++index)
                            differences += actual[index] != expected[index];
                        std::cerr << "chunk raster profile=" << profile << " direction=" << direction << " variant=" << variant << " top=" << top
                                  << " differingBytes=" << differences << '\n';
                    }
                    Require(actual == expected, "incremental chunk raster matches a fresh preparation of the same chunking exactly on WARP");
                    ++comparedPairs;
                    if (viewportIndex == 3u)
                        Require(std::any_of(expected.begin(), expected.end(), [](std::uint8_t value) { return value != 0u; }),
                                "every final viewport contains actual reference glyph ink, including the large-origin profile");
                }
            }
        }
    }
    raster.context->SetDpi(96.0f, 96.0f);
    raster.context->SetTransform(D2D1::Matrix3x2F::Identity());
    Require(comparedPairs == 162u, "all font/large-origin/uniform Source/RTL/transform WARP pixel pairs execute");
    std::cout << "rich chunk raster exactPairs=" << comparedPairs
              << " fontProfiles=9 directions=2 transforms=3 viewports=3 largestLineTopDifference=" << g_largestLineTopDifference
              << " largestWholeLayoutDrift=" << g_largestWholeLayoutDrift << '\n';
}
void TestRichDocumentLayoutWarpRasterOracleDetectsAWrongOffset()
{
    const auto factory = DocumentFactory();
    RasterTarget raster;
    Document document(RasterProfileSource(4u));
    PreparedDocumentLayout incremental, fresh;
    Detail::testRichChunkOffsetError = 1.0f;
    {
        const auto restore = wil::scope_exit([] { Detail::testRichChunkOffsetError = 0.0f; });
        PrepareIncrementalAndFresh(factory.get(), 4u, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, incremental, fresh, document);
    }
    const auto viewportHeight = static_cast<float>(raster.graphics.height);
    const auto top            = incremental.Height() * 0.5f;
    Require(raster.Draw(incremental, top, viewportHeight) != raster.Draw(fresh, top, viewportHeight),
            "a chunk placed one DIP off differs from a fresh preparation of the same chunking");
    const auto caret = std::max<std::size_t>(1u, incremental.DisplayLength() / 2u);
    Require(incremental.Caret(caret).y != fresh.Caret(caret).y, "the wrong offset reaches caret geometry");
}
// Review finding H1: an oracle that a chunk drawn away from its placement cannot pass. For short documents, where one
// whole layout's single-precision line tops agree with the exact sums far below a device pixel, chunk pixels equal the
// whole layout's pixels exactly.
void TestRichDocumentLayoutChunkPixelsMatchShortWholeLayout()
{
    const auto factory = DocumentFactory();
    RasterTarget raster;
    const std::wstring hebrew{static_cast<wchar_t>(0x05d0), static_cast<wchar_t>(0x05d1), static_cast<wchar_t>(0x05d2)};
    std::wstring source;
    for (std::size_t line = 0u; line < 70u; ++line)
        source += L"Line " + std::to_wstring(line) + L" carries **ordinary** words that wrap at narrow widths, " + hebrew + L" and `code`.\n";
    std::size_t pairs = 0u;
    // Review finding E2 (round 2): absolute placement under a non-identity transform and a DPI other than 96 too (a scale
    // at 144 DPI, a skew), where the incremental-versus-fresh comparison cannot see a placement both share.
    for (unsigned variant = 0u; variant < 3u; ++variant)
        for (const auto mode : {ViewMode::Source, ViewMode::Rich})
            for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
                for (const float multiplier : {0.0f, 1.6f})
                {
                    raster.context->SetDpi(variant == 1u ? 144.0f : 96.0f, variant == 1u ? 144.0f : 96.0f);
                    raster.context->SetTransform(variant == 1u   ? D2D1::Matrix3x2F::Scale(0.75f, 0.75f)
                                                 : variant == 2u ? D2D1::Matrix3x2F::Skew(6.0f, 0.0f)
                                                                 : D2D1::Matrix3x2F::Identity());
                    Document document(source);
                    auto options                 = RasterProfileOptions(0u, direction);
                    options.fontSizeDip          = 16.5f;
                    options.lineHeightMultiplier = multiplier;
                    PreparedDocumentLayout chunked;
                    chunked.Prepare(factory.get(), document.Snapshot(), document.Revision(), mode, options);
                    PreparedLayout flat;
                    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), mode), options);
                    Require(chunked.ChunkCount() > 2u && ! chunked.UsesWholeGeometry(), "the short document spans several chunks");
                    const auto viewportHeight = static_cast<float>(raster.graphics.height) / (variant == 1u ? 1.125f : 1.0f);
                    for (const float top : {0.0f, flat.Height() * 0.5f, std::max(0.0f, flat.Height() - viewportHeight)})
                    {
                        const auto actual = raster.Draw(chunked, top, viewportHeight);
                        raster.context->BeginDraw();
                        raster.context->Clear(D2D1::ColorF(0, 0));
                        flat.Draw(raster.context.get(), raster.brush.get(), D2D1::Point2F(8.0f, -top));
                        THROW_IF_FAILED(raster.context->EndDraw());
                        std::vector<std::uint8_t> expected;
                        THROW_IF_FAILED(raster.graphics.Read(expected));
                        std::size_t differing = 0u;
                        for (std::size_t index = 0u; index < actual.size(); ++index)
                            differing += actual[index] != expected[index] ? 1u : 0u;
                        if (differing)
                            std::cerr << "short whole-layout pixels variant=" << variant << " mode=" << static_cast<unsigned>(mode)
                                      << " direction=" << direction << " multiplier=" << multiplier << " top=" << top << " differingBytes=" << differing
                                      << '\n';
                        Require(differing == 0u, "chunk pixels of a short document equal one whole layout's pixels exactly");
                        ++pairs;
                    }
                }
    raster.context->SetDpi(96.0f, 96.0f);
    raster.context->SetTransform(D2D1::Matrix3x2F::Identity());
    std::cout << "short whole-layout pixel pairs=" << pairs << '\n';
    Require(pairs == 72u, "every transform, mode, direction, spacing and viewport pair executes");
}
// Review finding H1, every profile: each chunk's ink, drawn by the document at its placement, equals that chunk's own text
// laid out alone and drawn at the chunk's line top (its first line's selection rectangle, from the exact sums). Source
// mode profiles, so the chunk alone projects exactly as inside the document.
void TestRichDocumentLayoutChunkInkAtPlacement()
{
    const auto factory = DocumentFactory();
    RasterTarget raster;
    raster.context->SetDpi(96.0f, 96.0f);
    raster.context->SetTransform(D2D1::Matrix3x2F::Identity());
    std::size_t bands = 0u;
    for (const unsigned profile : {1u, 6u, 7u, 8u})
        for (const auto direction : {DWRITE_READING_DIRECTION_LEFT_TO_RIGHT, DWRITE_READING_DIRECTION_RIGHT_TO_LEFT})
        {
            Document document(RasterProfileSource(profile));
            PreparedDocumentLayout incremental, fresh;
            PrepareIncrementalAndFresh(factory.get(), profile, direction, incremental, fresh, document);
            const auto starts  = Detail::testRichPublishedChunkStarts;
            const auto text    = document.Text();
            const auto options = RasterProfileOptions(profile, direction);
            const auto height  = static_cast<float>(raster.graphics.height);
            for (const float top : {0.0f, incremental.Height() * 0.5f, std::max(0.0f, incremental.Height() - height)})
            {
                const auto whole = raster.Draw(incremental, top, height);
                for (std::size_t index = 0u; index < starts.size(); ++index)
                {
                    const auto display = incremental.ToDisplay(starts[index]);
                    const auto end     = index + 1u < starts.size() ? starts[index + 1u] : text.size();
                    if (display >= incremental.DisplayLength())
                        continue;
                    const auto chunkTop    = incremental.Range(display, display + 1u).front().top;
                    const auto chunkBottom = index + 1u < starts.size()
                                                 ? incremental.Range(incremental.ToDisplay(end), incremental.ToDisplay(end) + 1u).front().top
                                                 : incremental.Height();
                    // Device rows strictly inside the chunk's band, away from neighbours' ascenders and descenders.
                    const auto first = static_cast<long>(std::ceil(chunkTop - top)) + 6, last = static_cast<long>(std::floor(chunkBottom - top)) - 6;
                    const auto rowFirst = std::max(0L, first), rowLast = std::min(static_cast<long>(raster.graphics.height), last);
                    if (rowLast - rowFirst < 8)
                        continue;
                    Document alone(text.substr(starts[index], end - starts[index]));
                    PreparedLayout chunk;
                    chunk.Prepare(factory.get(), Project(alone.Snapshot(), alone.Revision(), ViewMode::Source), options);
                    raster.context->BeginDraw();
                    raster.context->Clear(D2D1::ColorF(0, 0));
                    chunk.Draw(raster.context.get(), raster.brush.get(), D2D1::Point2F(8.0f, chunkTop - top));
                    THROW_IF_FAILED(raster.context->EndDraw());
                    std::vector<std::uint8_t> own;
                    THROW_IF_FAILED(raster.graphics.Read(own));
                    const auto stride     = static_cast<std::size_t>(raster.graphics.width) * 4u;
                    std::size_t differing = 0u;
                    for (auto row = rowFirst; row < rowLast; ++row)
                        for (std::size_t column = 0u; column < stride; ++column)
                        {
                            const auto offset = static_cast<std::size_t>(row) * stride + column;
                            differing += whole[offset] != own[offset] ? 1u : 0u;
                        }
                    if (differing)
                        std::cerr << "chunk ink profile=" << profile << " direction=" << direction << " top=" << top << " chunk=" << index
                                  << " differingBytes=" << differing << '\n';
                    Require(differing == 0u, "a chunk's ink in the document is its own layout's ink at its exact line top");
                    ++bands;
                }
            }
        }
    Require(bands > 8u, "the ink check compares several chunk bands");
    std::cout << "chunk ink bands=" << bands << '\n';
}
HRESULT DrainDocumentPreparation(PreparedDocumentLayout& layout, const LayoutWorkBudget& budget);
// Chunks a scrolled first load of the viewport fixture lays before its first publication (the window's chunks).
constexpr std::size_t kScrolledFirstLoadChunks = 4u;
// Chunks a first load at the top lays before its first publication.
constexpr std::size_t kTopFirstLoadChunks = 2u;
// The viewport-first fixture: about 40,000 units of wrapping Source lines (39 chunks at 420 DIP).
std::wstring ViewportFixtureSource()
{
    const std::wstring hebrew{static_cast<wchar_t>(0x05d0), static_cast<wchar_t>(0x05d1), static_cast<wchar_t>(0x05d2)};
    std::wstring source;
    for (std::size_t line = 0u; source.size() < 40000u; ++line)
        source += L"Line " + std::to_wstring(line) + L" carries enough ordinary words to wrap at narrow widths, " + hebrew + L" too.\n";
    return source;
}
LayoutOptions ViewportFixtureOptions(float width)
{
    LayoutOptions options;
    options.widthDip    = width;
    options.fontSizeDip = 15.5f;
    return options;
}
// Advances with the owner's viewport until the layout publishes (partially or completely) or finishes.
void AdvanceUntilPublication(PreparedDocumentLayout& layout, float top, float bottom)
{
    const auto generation = layout.Generation();
    for (std::size_t advance = 0u; advance < 100000u && layout.Generation() == generation && layout.HasPendingPreparation(); ++advance)
        Require(Detail::PreparedDocumentLayoutAccess::Advance(layout, {1u, 4096u}, Detail::DocumentViewport::Of(top, bottom)).status !=
                    LayoutPreparationStatus::Failed,
                "viewport-first advance succeeds");
}
// A partial publication answers no query: it is not Ready, and a position outside its laid chunks is pending.
void RequirePartialRefusesQueries(PreparedDocumentLayout& layout)
{
    CaretGeometry caret{};
    Hit hit{};
    std::vector<D2D1_RECT_F> rectangles;
    std::pair<std::size_t, std::size_t> range{};
    Require(Detail::PreparedDocumentLayoutAccess::Partial(layout) && ! layout.Ready() && layout.TryCaret(0u, caret) == E_UNEXPECTED &&
                layout.TryHitTest(0.0f, 0.0f, hit) == E_UNEXPECTED && layout.TryRange(0u, 1u, rectangles) == E_UNEXPECTED &&
                layout.TryVisibleRange(0.0f, 10.0f, range) == E_UNEXPECTED,
            "a partial publication answers no geometry query (D3)");
}
// W7.3 gate: after a width change, the chunks meeting the viewport (and one viewport of overscan) are laid and published
// before any other chunk is laid; the rest keep stand-ins until the preparation completes.
void TestRichDocumentLayoutViewportFirstPublication()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const auto chunks = layout.ChunkCount();
    Require(chunks > 20u, "the viewport-first fixture spans many chunks");
    const float top = layout.Height() * 0.6f, bottom = top + 500.0f;
    const auto first = layout.ToSource(layout.HitTest(10.0f, top + 50.0f).position), second = layout.ToSource(layout.HitTest(10.0f, top + 400.0f).position);
    const auto narrower = ViewportFixtureOptions(383.0f);
    PreparedDocumentLayout reference;
    reference.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, narrower);
    const auto generation = layout.Generation();
    const DxUi::Detail::WorkCounterScope counters;
    Require(layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, narrower) == LayoutPreparationStatus::Pending,
            "a width change stages a preparation");
    AdvanceUntilPublication(layout, top, bottom);
    const auto atPublication = counters.Delta().rich;
    std::cout << "viewport-first chunks=" << chunks << " laidBeforePublication=" << atPublication.chunksLaid
              << " viewportPublications=" << atPublication.viewportPublications << " offscreenFirst=" << atPublication.offscreenChunksLaidFirst << '\n';
    Require(layout.HasPendingPreparation() && layout.Generation() == generation + 1u && atPublication.viewportPublications == 1u,
            "the viewport's chunks are published while the rest of the preparation is still pending");
    Require(atPublication.offscreenChunksLaidFirst == 0u && atPublication.chunksLaid == 4u && atPublication.chunksLaid < chunks,
            "only chunks meeting the viewport and its overscan were laid before that publication");
    RequirePartialRefusesQueries(layout);
    // The published viewport already follows the new width: line spacing between two viewport positions is the reference's.
    float firstTop{}, secondTop{};
    Require(Detail::PreparedDocumentLayoutAccess::PartialSourceTop(layout, first, firstTop) == S_OK &&
                Detail::PreparedDocumentLayoutAccess::PartialSourceTop(layout, second, secondTop) == S_OK,
            "viewport positions lie in laid chunks");
    const auto expected = reference.Caret(reference.ToDisplay(second)).y - reference.Caret(reference.ToDisplay(first)).y;
    Require(std::abs((secondTop - firstTop) - expected) <= 1.0f / 64.0f, "viewport lines are already wrapped at the new width");
    Require(DrainDocumentPreparation(layout, {4u, 4096u}) == S_OK && layout.Ready() && layout.Generation() == generation + 2u,
            "the rest completes in one more publication");
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), narrower);
    CompareDocumentGeometry(layout, flat, 211u);
    Require(counters.Delta().rich.chunksLaid == layout.ChunkCount() && layout.ChunkCount() == chunks,
            "every chunk was laid once for the new width, and the chunking did not coarsen");
}
// Review finding D1: consecutive width steps, each arriving while the previous one's off-screen chunks are still pending
// (a live resize), each publish their viewport before any off-screen chunk; stale stand-ins stand in again.
void TestRichDocumentLayoutViewportFirstAcrossWidthSteps()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const auto chunks = layout.ChunkCount();
    const float top = layout.Height() * 0.6f, bottom = top + 500.0f;
    auto width = 420.0f;
    for (unsigned step = 0u; step < 6u; ++step)
    {
        width -= 11.0f;
        const DxUi::Detail::WorkCounterScope counters;
        Require(layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(width)) ==
                    LayoutPreparationStatus::Pending,
                "each width step stages a preparation");
        AdvanceUntilPublication(layout, top, bottom);
        const auto delta = counters.Delta().rich;
        std::cout << "width step " << step << ": viewportPublications=" << delta.viewportPublications << " chunksLaid=" << delta.chunksLaid
                  << " offscreenFirst=" << delta.offscreenChunksLaidFirst << " chunks=" << layout.ChunkCount() << '\n';
        Require(delta.viewportPublications == 1u && delta.offscreenChunksLaidFirst == 0u && delta.chunksLaid == 4u && layout.HasPendingPreparation(),
                "every width step of a live resize publishes its viewport before any off-screen chunk");
        Require(layout.ChunkCount() == chunks, "the plan keeps the chunking: nothing coarsens while nothing is reusable");
        RequirePartialRefusesQueries(layout);
    }
    Require(DrainDocumentPreparation(layout, {4u, 4096u}) == S_OK && layout.Ready() && layout.ChunkCount() == chunks, "the last width step completes");
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), ViewportFixtureOptions(width));
    CompareDocumentGeometry(layout, flat, 211u);
}
// Review finding D2: an insert, an Enter and a paste during a viewport-first window each lay exactly one chunk before
// their publication, and the result is the same geometry as a complete preparation.
void TestRichDocumentLayoutEditsDuringViewportFirst()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const float top = layout.Height() * 0.6f, bottom = top + 500.0f;
    auto at             = layout.ToSource(layout.HitTest(10.0f, top + 100.0f).position);
    const auto narrower = ViewportFixtureOptions(383.0f);
    std::wstring pasted;
    while (pasted.size() < 300u)
        pasted += L"pasted ordinary words\n";
    layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, narrower);
    AdvanceUntilPublication(layout, top, bottom);
    Require(Detail::PreparedDocumentLayoutAccess::Partial(layout), "the width step leaves a partial publication");
    for (const auto* text : {L"x", L"\n", pasted.c_str()})
    {
        document.Replace(at, at, text, {0u, 0u});
        at += std::wstring_view(text).size();
        const DxUi::Detail::WorkCounterScope counters;
        layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, narrower);
        AdvanceUntilPublication(layout, top, bottom);
        const auto delta = counters.Delta().rich;
        std::cout << "edit during viewport-first (" << std::wstring_view(text).size() << " units): chunksLaid=" << delta.chunksLaid
                  << " viewportPublications=" << delta.viewportPublications << " offscreenFirst=" << delta.offscreenChunksLaidFirst << '\n';
        Require(delta.chunksLaid == 1u && delta.viewportPublications == 1u && delta.offscreenChunksLaidFirst == 0u,
                "an insert, an Enter or a paste during a viewport-first window lays exactly the one chunk it edits before it is shown");
    }
    Require(DrainDocumentPreparation(layout, {4u, 4096u}) == S_OK && layout.Ready(), "the edits complete");
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), narrower);
    CompareDocumentGeometry(layout, flat, 211u);
}
// A document's first preparation also publishes its viewport first: chunks without any previous geometry are estimated
// placeholders until laid.
void TestRichDocumentLayoutFirstLoadViewportFirst()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    const DxUi::Detail::WorkCounterScope counters;
    Require(layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f)) ==
                LayoutPreparationStatus::Pending,
            "a first preparation stages");
    AdvanceUntilPublication(layout, 0.0f, 500.0f);
    const auto delta = counters.Delta().rich;
    std::cout << "first load: viewportPublications=" << delta.viewportPublications << " chunksLaid=" << delta.chunksLaid
              << " offscreenFirst=" << delta.offscreenChunksLaidFirst << '\n';
    Require(delta.viewportPublications == 1u && delta.offscreenChunksLaidFirst == 0u && delta.chunksLaid == kTopFirstLoadChunks &&
                delta.estimateSampleLayouts == 0u && layout.HasPendingPreparation(),
            "a first load publishes its viewport before any off-screen chunk (at the top, no sample is needed)");
    RequirePartialRefusesQueries(layout);
    Require(DrainDocumentPreparation(layout, {4u, 4096u}) == S_OK && layout.Ready(), "the first load completes");
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), ViewportFixtureOptions(420.0f));
    CompareDocumentGeometry(layout, flat, 211u);
    {
        // One advance with budget left after the window is published goes on laying in the same advance: the window is
        // still published before any off-screen chunk, and only once per advance.
        PreparedDocumentLayout wide;
        const DxUi::Detail::WorkCounterScope wideCounters;
        wide.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
        const auto advance   = Detail::PreparedDocumentLayoutAccess::Advance(wide, {24u, 4096u}, Detail::DocumentViewport::Of(0.0f, 500.0f));
        const auto wideDelta = wideCounters.Delta().rich;
        std::cout << "first load, one wide advance: viewportPublications=" << wideDelta.viewportPublications << " chunksLaid=" << wideDelta.chunksLaid
                  << " offscreenFirst=" << wideDelta.offscreenChunksLaidFirst << '\n';
        Require(advance.status == LayoutPreparationStatus::Pending && wideDelta.viewportPublications == 1u && wideDelta.offscreenChunksLaidFirst == 0u &&
                    wideDelta.chunksLaid == 24u && Detail::PreparedDocumentLayoutAccess::Partial(wide),
                "an advance continues after its viewport publication without laying an off-screen chunk first");
    }
    {
        // A first load whose whole document fits one advance completes in that advance (one turn, one frame), even when
        // it has chunks outside the window.
        std::wstring shortSource;
        while (shortSource.size() < 2600u)
            shortSource += L"A short document line of ordinary words that wraps at narrow widths.\n";
        Document shortDocument(shortSource);
        PreparedDocumentLayout once;
        const DxUi::Detail::WorkCounterScope onceCounters;
        once.BeginPreparation(factory.get(), shortDocument.Snapshot(), shortDocument.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
        const auto advance = Detail::PreparedDocumentLayoutAccess::Advance(once, {}, Detail::DocumentViewport::Of(0.0f, 60.0f));
        std::cout << "short first load: chunks=" << once.ChunkCount() << " viewportPublications=" << onceCounters.Delta().rich.viewportPublications << '\n';
        Require(advance.status == LayoutPreparationStatus::Complete && once.Ready() && once.ChunkCount() == 3u,
                "a first load that fits one advance completes in it, although chunks lie outside the window");
    }
    {
        // Review finding C1 (round 2): a first load scrolled away from the top, so no height is known and every estimate
        // used to be 0 (every chunk at 0, none meeting the window: an empty publication, then the document's first chunk
        // laid first). A sample layout measures line height and wrapping: the first publication holds the viewport, and
        // no chunk outside the window is laid before it.
        PreparedDocumentLayout reference;
        reference.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
        const float top = reference.Height() * 0.6f, bottom = top + 500.0f;
        const auto inView = reference.ToSource(reference.HitTest(10.0f, top + 250.0f).position);
        PreparedDocumentLayout scrolled;
        const DxUi::Detail::WorkCounterScope scrolledCounters;
        Require(scrolled.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f)) ==
                    LayoutPreparationStatus::Pending,
                "a scrolled first preparation stages");
        AdvanceUntilPublication(scrolled, top, bottom);
        float y{}, startY{};
        const auto shown         = Detail::PreparedDocumentLayoutAccess::PartialSourceTop(scrolled, inView, y);
        const auto startLaid     = Detail::PreparedDocumentLayoutAccess::PartialSourceTop(scrolled, 0u, startY) == S_OK;
        const auto scrolledDelta = scrolledCounters.Delta().rich;
        std::cout << "scrolled first load: viewportPublications=" << scrolledDelta.viewportPublications << " chunksLaid=" << scrolledDelta.chunksLaid
                  << " samples=" << scrolledDelta.estimateSampleLayouts << " offscreenFirst=" << scrolledDelta.offscreenChunksLaidFirst << " shown=0x"
                  << std::hex << static_cast<unsigned long>(shown) << std::dec << " documentStartLaid=" << startLaid << '\n';
        Require(shown == S_OK && ! startLaid, "C1: the first publication of a scrolled first load shows the viewport, not the document's start");
        Require(scrolledDelta.viewportPublications == 1u && scrolledDelta.offscreenChunksLaidFirst == 0u && scrolledDelta.estimateSampleLayouts == 1u &&
                    scrolledDelta.chunksLaid == kScrolledFirstLoadChunks,
                "C1: one sample, then only the window's chunks before the first publication");
        RequirePartialRefusesQueries(scrolled);
        Require(DrainDocumentPreparation(scrolled, {4u, 4096u}) == S_OK && scrolled.Ready() && scrolledCounters.Delta().rich.viewportPublications == 1u,
                "the scrolled first load completes with no further partial publication");
        PreparedLayout scrolledFlat;
        scrolledFlat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), ViewportFixtureOptions(420.0f));
        CompareDocumentGeometry(scrolled, scrolledFlat, 211u);
    }
}
// Review finding C3 (round 2): an edit outside the viewport window, with every window chunk reused, makes no partial
// publication (it would show nothing new); the edited chunk is laid and the preparation completes.
void TestRichDocumentLayoutEditOutsideWindowPublishesNothingNew()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const float top = layout.Height() * 0.8f, bottom = top + 500.0f;
    document.Replace(100u, 100u, L"\n\n\n\n\n\n\n\n\n\n", {0u, 0u}); // ten new lines near the top
    const DxUi::Detail::WorkCounterScope counters;
    layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const auto advance = Detail::PreparedDocumentLayoutAccess::Advance(layout, {1u, 4096u}, Detail::DocumentViewport::Of(top, bottom));
    const auto delta   = counters.Delta().rich;
    std::cout << "edit outside window: viewportPublications=" << delta.viewportPublications << " chunksLaid=" << delta.chunksLaid
              << " status=" << static_cast<int>(advance.status) << '\n';
    Require(delta.viewportPublications == 0u && delta.chunksLaid == 1u && advance.status == LayoutPreparationStatus::Complete && layout.Ready(),
            "C3: an edit outside the window publishes nothing partial and completes in one advance");
    PreparedLayout flat;
    flat.Prepare(factory.get(), Project(document.Snapshot(), document.Revision(), ViewMode::Source), ViewportFixtureOptions(420.0f));
    CompareDocumentGeometry(layout, flat, 211u);
}
// Review finding C2 (round 2): a width step, its partial publication, and a step back to the published width (a live
// resize that returns) restores the last complete publication: no chunk is laid again.
void TestRichDocumentLayoutWidthReturnDuringPartial()
{
    const auto factory = DocumentFactory();
    Document document(ViewportFixtureSource());
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    const auto chunks = layout.ChunkCount();
    const auto height = layout.Height();
    const float top = height * 0.6f, bottom = top + 500.0f;
    const auto probe = layout.ToDisplay(layout.ToSource(layout.HitTest(10.0f, top + 250.0f).position));
    const auto caret = layout.Caret(probe);
    layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(383.0f));
    AdvanceUntilPublication(layout, top, bottom);
    Require(Detail::PreparedDocumentLayoutAccess::Partial(layout), "the width step leaves a partial publication");
    const auto generation = layout.Generation();
    const DxUi::Detail::WorkCounterScope counters;
    const auto status = layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(420.0f));
    std::cout << "width back to the published width: status=" << static_cast<int>(status) << " chunksLaid=" << counters.Delta().rich.chunksLaid << " of "
              << layout.ChunkCount() << '\n';
    Require(status == LayoutPreparationStatus::Complete && layout.Ready() && ! layout.HasPendingPreparation() && layout.Generation() == generation + 1u,
            "C2: returning to the last complete publication's inputs restores it at once");
    Require(counters.Delta().rich.chunksLaid == 0u && layout.ChunkCount() == chunks && layout.Height() == height &&
                layout.LastPreparation().preparedChunks == 0u,
            "C2: returning to the published width lays no chunk");
    const auto restored = layout.Caret(probe);
    Require(restored.y == caret.y && restored.x == caret.x, "the restored publication answers as before");
}
// Review finding F1 (round 2): the bound the layout keeps while it prepares. Each graph holds at most 127 chunks plus one
// whole layout; at most four graphs are alive (the publication; the last complete publication a partial one keeps; the
// partial publication a preparation began from; the staged graph, whose laid chunks an interim publication shares), so
// at most 3 x 127 distinct chunk layouts and 2 whole layouts. Reached with a document edited up to many chunks, then
// width steps during a live resize.
void TestRichDocumentLayoutRetentionBound()
{
    const auto factory = DocumentFactory();
    std::wstring source;
    while (source.size() < 64000u)
        source += L"short line of ordinary words\n";
    Document document(source);
    const auto options = ViewportFixtureOptions(420.0f);
    PreparedDocumentLayout layout;
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, options);
    // Shrink every chunk by deleting most of it, one edit inside each chunk (from the last chunk to the first), then
    // append: edits keep the chunk starts, so the document reaches many chunks.
    auto starts = Detail::PreparedDocumentLayoutAccess::ChunkDisplayStarts(layout);
    for (std::size_t index = starts.size(); index-- > 1u;)
    {
        const auto first = starts[index - 1u] + 29u * 2u, last = starts[index] - 29u * 2u;
        if (last <= first)
            continue;
        document.Replace(first, last, L"", {0u, 0u});
        layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, options);
    }
    std::wstring tail;
    while (document.Length() + tail.size() < 64000u)
        tail += L"appended line of ordinary words\n";
    document.Replace(document.Length(), document.Length(), tail, {0u, 0u});
    layout.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, options);
    const auto many = layout.ChunkCount();
    Detail::PreparedDocumentLayoutAccess::Retention largest;
    const auto check = [&]
    {
        const auto retained        = Detail::PreparedDocumentLayoutAccess::Retained(layout);
        largest.graphs             = std::max(largest.graphs, retained.graphs);
        largest.chunkLayouts       = std::max(largest.chunkLayouts, retained.chunkLayouts);
        largest.wholeLayouts       = std::max(largest.wholeLayouts, retained.wholeLayouts);
        largest.largestGraphChunks = std::max(largest.largestGraphChunks, retained.largestGraphChunks);
        Require(retained.graphs <= 4u && retained.largestGraphChunks <= 127u && retained.chunkLayouts <= 3u * 127u && retained.wholeLayouts <= 2u,
                "F1: at most four graphs of at most 127 chunks each, 3 x 127 chunk layouts and 2 whole layouts are alive");
    };
    // Three width steps, each arriving after the previous one's viewport publication (a live resize), the last drained.
    float width = 420.0f;
    for (unsigned step = 0u; step < 3u; ++step)
    {
        width -= 17.0f;
        layout.BeginPreparation(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Source, ViewportFixtureOptions(width));
        check();
        const auto generation = layout.Generation();
        for (std::size_t advance = 0u; advance < 100000u && layout.HasPendingPreparation() && (step == 2u || layout.Generation() == generation); ++advance)
        {
            Detail::PreparedDocumentLayoutAccess::Advance(layout, {1u, 4096u}, Detail::DocumentViewport::Of(0.0f, 500.0f));
            check();
        }
    }
    std::cout << "retention: chunks=" << many << " largest graphs=" << largest.graphs << " chunkLayouts=" << largest.chunkLayouts
              << " wholeLayouts=" << largest.wholeLayouts << " graphChunks=" << largest.largestGraphChunks << '\n';
    Require(many > 64u && largest.graphs == 4u && largest.chunkLayouts > 127u, "the bound is reached beyond one graph's chunks");
    Require(layout.Ready(), "the last width step completes");
}
// Review finding A1 (round 2): at each chunk seam of the large-coordinate profiles, the visible range follows the placed
// caret boxes exactly: for the 24 viewport tops just above the seam's next line, the previous chunk's last line is visible
// exactly when its placed caret box reaches below that top.
void TestRichDocumentLayoutSeamVisibleRange()
{
    const auto factory  = DocumentFactory();
    std::size_t checked = 0u, mismatches = 0u;
    for (const unsigned profile : {5u, 8u})
    {
        Document document(RasterProfileSource(profile));
        const auto options = RasterProfileOptions(profile, DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
        PreparedDocumentLayout prepared;
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), RasterProfileMode(profile), options);
        const auto starts = Detail::PreparedDocumentLayoutAccess::ChunkDisplayStarts(prepared);
        for (std::size_t index = 1u; index < starts.size(); ++index)
        {
            const auto seam      = starts[index];
            const auto lastStart = prepared.LineBoundary(seam - 1u, false);
            const auto last = prepared.Caret(lastStart), next = prepared.Caret(seam);
            const float lastBottom = last.y + last.height;
            auto t                 = next.y;
            for (int step = 0; step < 24; ++step)
            {
                t                        = std::nextafter(t, -INFINITY);
                const auto range         = prepared.VisibleRange(t, t + 57.0f);
                const auto expectedFirst = lastBottom > t ? lastStart : seam;
                ++checked;
                if (range.first != expectedFirst)
                {
                    if (mismatches < 8u)
                        std::cout << std::setprecision(10) << "seam profile=" << profile << " chunk=" << index << " t=" << t << " lastTop=" << last.y
                                  << " lastBottom=" << lastBottom << " nextTop=" << next.y << " actualFirst=" << range.first
                                  << " expectedFirst=" << expectedFirst << '\n';
                    ++mismatches;
                }
            }
        }
    }
    std::cout << "seam visible ranges checked=" << checked << " mismatches=" << mismatches << '\n';
    Require(checked == 1632u && mismatches == 0u, "A1: a line whose placed caret box meets the viewport is visible at every chunk seam");
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
    for (size_t edit = 0u; edit < 512u; ++edit)
    {
        const auto position = (edit * 997u + 512u) % document.Length();
        document.Replace(position, position, L"x\n", {0u, 0u});
        prepared.Prepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options);
        Require(prepared.ChunkCount() <= 128u && prepared.SourceMatches(document.Text()),
                "fragmenting edits keep the published COM graph and exact source bounded");
    }
    {
        // W7.3: an edit re-lays the chunk that contains it up to the next unchanged chunk, so edits no longer fragment
        // a document towards 127 chunks. Compaction is exercised directly: a forced plan of more than 127 cuts (the
        // oracle's same-chunking seam) must compact to the default chunking without failing.
        Detail::testRichRecordChunkPlan       = true;
        Detail::testRichPublishedSourceLength = document.Length();
        Detail::testRichPublishedChunkStarts.clear();
        const auto text = document.Text();
        for (std::size_t start = 0u; start < text.size(); start = text.find(L'\n', start) + 1u)
        {
            Detail::testRichPublishedChunkStarts.push_back(start);
            if (text.find(L'\n', start) == std::wstring::npos)
                break;
        }
        Require(Detail::testRichPublishedChunkStarts.size() > 200u, "the forced plan asks for more than 127 chunks");
        DxUi::Detail::SetFreshPreparationForTest(true);
        const auto restore = wil::scope_exit([]() noexcept
        {
            DxUi::Detail::SetFreshPreparationForTest(false);
            Detail::testRichRecordChunkPlan = false;
            Detail::testRichPublishedChunkStarts.clear();
        });
        PreparedDocumentLayout compacted;
        Require(SUCCEEDED(compacted.TryPrepare(factory.get(), document.Snapshot(), document.Revision(), ViewMode::Rich, options)) &&
                    compacted.ChunkCount() <= 127u && compacted.ChunkCount() > 1u && compacted.LastPreparation().preparedChunks == compacted.ChunkCount(),
                "a plan of more than 127 chunks compacts before laying any chunk");
    }
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
HRESULT DrainDocumentPreparation(PreparedDocumentLayout& layout, const LayoutWorkBudget& budget)
{
    for (size_t call = 0u; call < 200000u; ++call)
    {
        const auto advance = layout.AdvancePreparation(budget);
        Require(advance.layoutsPrepared <= budget.maximumLayouts && advance.blockSteps <= budget.maximumBlockSteps,
                "each cooperative advance obeys both independent work budgets");
        if (advance.status == LayoutPreparationStatus::Complete)
            return S_OK;
        if (advance.status == LayoutPreparationStatus::Failed)
            return advance.result;
        Require(advance.status == LayoutPreparationStatus::Pending && layout.HasPendingPreparation(),
                "incomplete preparation retains an explicit continuation");
    }
    Require(false, "finite document preparation must terminate within its independent test bound");
    return E_FAIL;
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
    // W7.3: chunks are planned and ordered before they are laid; advance until the first chunk is laid.
    auto first = layout.AdvancePreparation({1u, 1u});
    for (std::size_t advance = 0u; advance < 64u && first.status == LayoutPreparationStatus::Pending && layout.PendingPreparation().preparedChunks == 0u;
         ++advance)
        first = layout.AdvancePreparation({1u, 1u});
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
    rejected      = DrainDocumentPreparation(layout, {1u, 1u}) == E_INVALIDARG;
    Require(rejected && ! layout.HasPendingPreparation() && layout.Generation() == publishedGeneration && layout.SourceMatches(replacement.Text()),
            "failed advance discards staging and preserves the published pair");
    layout.BeginPreparation(factory.get(), candidate.Snapshot(), candidate.Revision(), ViewMode::Rich, options);
    rejected                 = false;
    const auto refusedBudget = layout.AdvancePreparation({0u, 1u});
    rejected                 = refusedBudget.status == LayoutPreparationStatus::Failed && refusedBudget.result == E_INVALIDARG;
    Require(rejected && layout.HasPendingPreparation(), "refused zero work budget leaves a valid pending request intact");
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
    DXUI_RUN_TEST(TestRichDocumentLayoutWholeReferenceReuseAndStyleChanges);
    DXUI_RUN_TEST(TestRichDocumentLayoutUniformSourceReuse);
    DXUI_RUN_TEST(TestRichDocumentLayoutEditsAndFailurePublication);
    DXUI_RUN_TEST(TestRichDocumentLayoutHeightChangesRepositionSuffix);
    DXUI_RUN_TEST(TestRichDocumentLayoutPrivateOriginsDoNotEnterProjection);
    DXUI_RUN_TEST(TestRichDocumentLayoutLongHeightAndChunkBudget);
    DXUI_RUN_TEST(TestRichDocumentLayoutWarpRasterOracle);
    DXUI_RUN_TEST(TestRichDocumentLayoutWarpRasterOracleDetectsAWrongOffset);
    DXUI_RUN_TEST(TestRichDocumentLayoutViewportFirstPublication);
    DXUI_RUN_TEST(TestRichDocumentLayoutViewportFirstAcrossWidthSteps);
    DXUI_RUN_TEST(TestRichDocumentLayoutEditsDuringViewportFirst);
    DXUI_RUN_TEST(TestRichDocumentLayoutFirstLoadViewportFirst);
    DXUI_RUN_TEST(TestRichDocumentLayoutEditOutsideWindowPublishesNothingNew);
    DXUI_RUN_TEST(TestRichDocumentLayoutWidthReturnDuringPartial);
    DXUI_RUN_TEST(TestRichDocumentLayoutRetentionBound);
    DXUI_RUN_TEST(TestRichDocumentLayoutSeamVisibleRange);
    DXUI_RUN_TEST(TestRichDocumentLayoutChunkPixelsMatchShortWholeLayout);
    DXUI_RUN_TEST(TestRichDocumentLayoutChunkInkAtPlacement);
    DXUI_RUN_TEST(TestRichDocumentLayoutBranchesOptionsAndCompaction);
    DXUI_RUN_TEST(TestRichDocumentLayoutCooperativeBudgetsAndGeometry);
    DXUI_RUN_TEST(TestRichDocumentLayoutCoalescingCancellationAndFailure);
}
