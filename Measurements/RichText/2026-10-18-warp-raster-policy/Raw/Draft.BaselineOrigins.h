// Private uncompiled investigation draft. Not installed production/test support.
#pragma once
#include <bit>
#include <span>
#include <wrl/implements.h>

namespace RichBaselineProbe
{
struct GlyphOrigin
{
    UINT32 position{};
    float x{}, y{};
};
class Recorder final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextRenderer>
{
public:
    Recorder(std::span<GlyphOrigin> storage, bool snapping) noexcept : storage_(storage), snapping_(snapping)
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
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT x, FLOAT y, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN*,
                                            const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown*) noexcept override
    {
        if (! description || count_ == storage_.size())
            return E_BOUNDS;
        // Storage is allocated before Draw. No exception/allocation crosses COM.
        storage_[count_++] = {description->textPosition, x, y};
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
inline void Run()
{
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
                    source += profile == 1u ? L"one visible line\n" : L"Arabic \u0627\u0644\u0639\u0631\u0628\u064a\u0629 \U0001f600 a\u0301 wrapped text and more words\r\n";
            wil::com_ptr<IDWriteTextFormat> format;
            THROW_IF_FAILED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                       DWRITE_FONT_STRETCH_NORMAL, fontSize, L"en-us", format.put()));
            THROW_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP));
            THROW_IF_FAILED(format->SetReadingDirection(profile == 2u ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT));
            wil::com_ptr<IDWriteTextLayout> layout;
            THROW_IF_FAILED(factory->CreateTextLayout(source.data(), static_cast<UINT32>(source.size()), format.get(), 321.25f, 1000000.0f, layout.put()));
            if (profile == 2u)
                for (std::size_t first = 0u; first < source.size();)
                {
                    THROW_IF_FAILED(layout->SetFontSize(fontSize * 1.9f, {static_cast<UINT32>(first), 7u}));
                    const auto code = source.find(L"wrapped", first);
                    THROW_IF_FAILED(layout->SetFontFamilyName(L"Consolas", {static_cast<UINT32>(code), 7u}));
                    const auto last = source.find(L'\n', first);
                    first = last == std::wstring::npos ? source.size() : last + 1u;
                }
            UINT32 count = 0u;
            layout->GetLineMetrics(nullptr, 0u, &count);
            std::vector<DWRITE_LINE_METRICS> metrics(count);
            THROW_IF_FAILED(layout->GetLineMetrics(metrics.data(), count, &count));
            std::vector<UINT32> starts(count + 1u, 0u);
            std::vector<float> singleTops(count), doubleTops(count);
            float singleTop = 0.0f;
            double doubleTop = 0.0;
            for (std::size_t line = 0u; line < count; ++line)
            {
                starts[line + 1u] = starts[line] + metrics[line].length;
                singleTops[line] = singleTop;
                doubleTops[line] = static_cast<float>(doubleTop);
                singleTop = Add(singleTop, metrics[line].height);
                doubleTop += metrics[line].height;
            }
            DWRITE_TEXT_METRICS text{};
            THROW_IF_FAILED(layout->GetMetrics(&text));
            std::size_t caretSamples = 0u, singleCaretMismatches = 0u, doubleCaretMismatches = 0u;
            for (std::size_t position = 0u;; position = std::min(source.size(), position + 53u))
            {
                float x = 0.0f, y = 0.0f;
                DWRITE_HIT_TEST_METRICS hit{};
                THROW_IF_FAILED(layout->HitTestTextPosition(static_cast<UINT32>(position), FALSE, &x, &y, &hit));
                const auto found = std::upper_bound(starts.begin(), starts.end() - 1, position);
                const auto line = static_cast<std::size_t>(found - starts.begin() - 1);
                ++caretSamples;
                singleCaretMismatches += y != singleTops[line];
                doubleCaretMismatches += y != doubleTops[line];
                if (position == source.size())
                    break;
            }
            std::vector<GlyphOrigin> storage(source.size() * 8u + 1u);
            for (const auto originY : {0.0f, -611.75f, -123456.25f})
                for (const bool snapping : {false, true})
                {
                    auto recorder = Microsoft::WRL::Make<Recorder>(std::span<GlyphOrigin>(storage), snapping);
                    THROW_HR_IF(E_OUTOFMEMORY, ! recorder);
                    THROW_IF_FAILED(layout->Draw(nullptr, recorder.Get(), 3.25f, originY));
                    std::size_t singleMismatches = 0u, doubleMismatches = 0u;
                    UINT32 firstPosition = 0u, firstActual = 0u, firstSingle = 0u, firstDouble = 0u;
                    for (const auto glyph : recorder->Recorded())
                    {
                        const auto found = std::upper_bound(starts.begin(), starts.end() - 1, glyph.position);
                        const auto line = static_cast<std::size_t>(found - starts.begin() - 1);
                        const auto single = Add(originY, Add(singleTops[line], metrics[line].baseline));
                        const auto precise = Add(originY, Add(doubleTops[line], metrics[line].baseline));
                        if (! singleMismatches && glyph.y != single)
                        {
                            firstPosition = glyph.position;
                            firstActual = std::bit_cast<UINT32>(glyph.y);
                            firstSingle = std::bit_cast<UINT32>(single);
                            firstDouble = std::bit_cast<UINT32>(precise);
                        }
                        singleMismatches += glyph.y != single;
                        doubleMismatches += glyph.y != precise;
                    }
                    std::cout << "rich raw baseline font=" << fontSize << " profile=" << profile << " lines=" << count
                              << " originY=" << originY << " snapping=" << snapping << " glyphRuns=" << recorder->Recorded().size()
                              << " singleMismatches=" << singleMismatches << " doubleMismatches=" << doubleMismatches
                              << " firstPosition=" << firstPosition << " actualBits=" << firstActual << " singleBaselineBits=" << firstSingle
                              << " doubleBaselineBits=" << firstDouble
                              << " heightBits=" << std::bit_cast<UINT32>(text.height) << " singleBits=" << std::bit_cast<UINT32>(singleTop)
                              << " doubleBits=" << std::bit_cast<UINT32>(static_cast<float>(doubleTop)) << '\n';
                    std::cout << "rich raw caret font=" << fontSize << " profile=" << profile << " samples=" << caretSamples
                              << " singleMismatches=" << singleCaretMismatches << " doubleMismatches=" << doubleCaretMismatches << '\n';
                    Require(! recorder->Recorded().empty(), "raw DirectWrite baseline probe emits actual glyph runs");
                }
        }
}
} // namespace RichBaselineProbe
