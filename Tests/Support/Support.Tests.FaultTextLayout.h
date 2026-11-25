#pragma once
#include <dwrite.h>
#include <dwrite_1.h>
#include <limits>
#include <wil/com.h>
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)
namespace NativeTextTests
{
struct LayoutFaultState
{
    HRESULT failure{E_ACCESSDENIED};
    wil::com_ptr<IDWriteTextLayout> lastLayout;
    bool failLineMetrics{};
    bool failLayout1Query{};
    HRESULT layout1Failure{E_NOINTERFACE};
    unsigned layout1Queries{};
    bool mutateBeforeFailure{true};
    unsigned lineFailures{};
    size_t effectCalls{}, underlineCalls{};
    size_t failEffectAt{(std::numeric_limits<size_t>::max)()}, failEffectAt2{(std::numeric_limits<size_t>::max)()};
    size_t failUnderlineAt{(std::numeric_limits<size_t>::max)()}, failUnderlineAt2{(std::numeric_limits<size_t>::max)()};
};
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class FaultTextLayout final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextLayout1>
{
public:
    using Base = Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteTextLayout1>;
    FaultTextLayout(IDWriteTextLayout* real, LayoutFaultState* state) noexcept : _real(real), _state(state)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** result) noexcept override
    {
        if (riid == __uuidof(IDWriteTextLayout1))
        {
            ++_state->layout1Queries;
            if (_state->failLayout1Query)
            {
                if (result)
                    *result = nullptr;
                return _state->layout1Failure;
            }
            wil::com_ptr<IDWriteTextLayout1> native;
            const HRESULT hr = wil::com_query_to_nothrow(_real.get(), native.put());
            if (FAILED(hr))
            {
                if (result)
                    *result = nullptr;
                return hr;
            }
        }
        if (riid == __uuidof(IDWriteTextLayout))
        {
            if (! result)
                return E_POINTER;
            *result = static_cast<IDWriteTextLayout*>(static_cast<IDWriteTextLayout1*>(this));
            AddRef();
            return S_OK;
        }
        return Base::QueryInterface(riid, result);
    }
    HRESULT STDMETHODCALLTYPE SetPairKerning(BOOL enabled, DWRITE_TEXT_RANGE range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(_real.get(), native.put());
        return FAILED(hr) ? hr : native->SetPairKerning(enabled, range);
    }
    HRESULT STDMETHODCALLTYPE GetPairKerning(UINT32 position, BOOL* enabled, DWRITE_TEXT_RANGE* range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(_real.get(), native.put());
        return FAILED(hr) ? hr : native->GetPairKerning(position, enabled, range);
    }
    HRESULT STDMETHODCALLTYPE SetCharacterSpacing(FLOAT leading, FLOAT trailing, FLOAT minimum, DWRITE_TEXT_RANGE range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(_real.get(), native.put());
        return FAILED(hr) ? hr : native->SetCharacterSpacing(leading, trailing, minimum, range);
    }
    HRESULT STDMETHODCALLTYPE GetCharacterSpacing(UINT32 position, FLOAT* leading, FLOAT* trailing, FLOAT* minimum, DWRITE_TEXT_RANGE* range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(_real.get(), native.put());
        return FAILED(hr) ? hr : native->GetCharacterSpacing(position, leading, trailing, minimum, range);
    }
    HRESULT STDMETHODCALLTYPE SetTextAlignment(DWRITE_TEXT_ALIGNMENT textAlignment) noexcept override
    {
        return _real->SetTextAlignment(textAlignment);
    }
    HRESULT STDMETHODCALLTYPE SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment) noexcept override
    {
        return _real->SetParagraphAlignment(paragraphAlignment);
    }
    HRESULT STDMETHODCALLTYPE SetWordWrapping(DWRITE_WORD_WRAPPING wordWrapping) noexcept override
    {
        return _real->SetWordWrapping(wordWrapping);
    }
    HRESULT STDMETHODCALLTYPE SetReadingDirection(DWRITE_READING_DIRECTION readingDirection) noexcept override
    {
        return _real->SetReadingDirection(readingDirection);
    }
    HRESULT STDMETHODCALLTYPE SetFlowDirection(DWRITE_FLOW_DIRECTION flowDirection) noexcept override
    {
        return _real->SetFlowDirection(flowDirection);
    }
    HRESULT STDMETHODCALLTYPE SetIncrementalTabStop(FLOAT incrementalTabStop) noexcept override
    {
        return _real->SetIncrementalTabStop(incrementalTabStop);
    }
    HRESULT STDMETHODCALLTYPE SetTrimming(DWRITE_TRIMMING const* trimmingOptions, IDWriteInlineObject* trimmingSign) noexcept override
    {
        return _real->SetTrimming(trimmingOptions, trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE SetLineSpacing(DWRITE_LINE_SPACING_METHOD lineSpacingMethod, FLOAT lineSpacing, FLOAT baseline) noexcept override
    {
        return _real->SetLineSpacing(lineSpacingMethod, lineSpacing, baseline);
    }
    DWRITE_TEXT_ALIGNMENT STDMETHODCALLTYPE GetTextAlignment() noexcept override
    {
        return _real->GetTextAlignment();
    }
    DWRITE_PARAGRAPH_ALIGNMENT STDMETHODCALLTYPE GetParagraphAlignment() noexcept override
    {
        return _real->GetParagraphAlignment();
    }
    DWRITE_WORD_WRAPPING STDMETHODCALLTYPE GetWordWrapping() noexcept override
    {
        return _real->GetWordWrapping();
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetReadingDirection() noexcept override
    {
        return _real->GetReadingDirection();
    }
    DWRITE_FLOW_DIRECTION STDMETHODCALLTYPE GetFlowDirection() noexcept override
    {
        return _real->GetFlowDirection();
    }
    FLOAT STDMETHODCALLTYPE GetIncrementalTabStop() noexcept override
    {
        return _real->GetIncrementalTabStop();
    }
    HRESULT STDMETHODCALLTYPE GetTrimming(DWRITE_TRIMMING* trimmingOptions, IDWriteInlineObject** trimmingSign) noexcept override
    {
        return _real->GetTrimming(trimmingOptions, trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE GetLineSpacing(DWRITE_LINE_SPACING_METHOD* lineSpacingMethod, FLOAT* lineSpacing, FLOAT* baseline) noexcept override
    {
        return _real->GetLineSpacing(lineSpacingMethod, lineSpacing, baseline);
    }
    HRESULT STDMETHODCALLTYPE GetFontCollection(IDWriteFontCollection** fontCollection) noexcept override
    {
        return _real->GetFontCollection(fontCollection);
    }
    UINT32 STDMETHODCALLTYPE GetFontFamilyNameLength() noexcept override
    {
        return _real->GetFontFamilyNameLength();
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyName(WCHAR* fontFamilyName, UINT32 nameSize) noexcept override
    {
        return _real->GetFontFamilyName(fontFamilyName, nameSize);
    }
    DWRITE_FONT_WEIGHT STDMETHODCALLTYPE GetFontWeight() noexcept override
    {
        return _real->GetFontWeight();
    }
    DWRITE_FONT_STYLE STDMETHODCALLTYPE GetFontStyle() noexcept override
    {
        return _real->GetFontStyle();
    }
    DWRITE_FONT_STRETCH STDMETHODCALLTYPE GetFontStretch() noexcept override
    {
        return _real->GetFontStretch();
    }
    FLOAT STDMETHODCALLTYPE GetFontSize() noexcept override
    {
        return _real->GetFontSize();
    }
    UINT32 STDMETHODCALLTYPE GetLocaleNameLength() noexcept override
    {
        return _real->GetLocaleNameLength();
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(WCHAR* localeName, UINT32 nameSize) noexcept override
    {
        return _real->GetLocaleName(localeName, nameSize);
    }
    HRESULT STDMETHODCALLTYPE SetMaxWidth(FLOAT maxWidth) noexcept override
    {
        return _real->SetMaxWidth(maxWidth);
    }
    HRESULT STDMETHODCALLTYPE SetMaxHeight(FLOAT maxHeight) noexcept override
    {
        return _real->SetMaxHeight(maxHeight);
    }
    HRESULT STDMETHODCALLTYPE SetFontCollection(IDWriteFontCollection* fontCollection, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontCollection(fontCollection, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontFamilyName(WCHAR const* fontFamilyName, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontFamilyName(fontFamilyName, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontWeight(DWRITE_FONT_WEIGHT fontWeight, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontWeight(fontWeight, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontStyle(DWRITE_FONT_STYLE fontStyle, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontStyle(fontStyle, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontStretch(DWRITE_FONT_STRETCH fontStretch, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontStretch(fontStretch, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontSize(FLOAT fontSize, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetFontSize(fontSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetUnderline(BOOL hasUnderline, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        const auto call = _state->underlineCalls++;
        if (call == _state->failUnderlineAt || call == _state->failUnderlineAt2)
        {
            if (_state->mutateBeforeFailure && call == _state->failUnderlineAt)
                (void)_real->SetUnderline(hasUnderline, textRange);
            return _state->failure;
        }
        return _real->SetUnderline(hasUnderline, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetStrikethrough(BOOL hasStrikethrough, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetStrikethrough(hasStrikethrough, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetDrawingEffect(IUnknown* drawingEffect, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        const auto call = _state->effectCalls++;
        if (call == _state->failEffectAt || call == _state->failEffectAt2)
        {
            if (_state->mutateBeforeFailure && call == _state->failEffectAt)
                (void)_real->SetDrawingEffect(drawingEffect, textRange);
            return _state->failure;
        }
        return _real->SetDrawingEffect(drawingEffect, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetInlineObject(IDWriteInlineObject* inlineObject, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetInlineObject(inlineObject, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetTypography(IDWriteTypography* typography, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetTypography(typography, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetLocaleName(WCHAR const* localeName, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return _real->SetLocaleName(localeName, textRange);
    }
    FLOAT STDMETHODCALLTYPE GetMaxWidth() noexcept override
    {
        return _real->GetMaxWidth();
    }
    FLOAT STDMETHODCALLTYPE GetMaxHeight() noexcept override
    {
        return _real->GetMaxHeight();
    }
    HRESULT STDMETHODCALLTYPE GetFontCollection(UINT32 currentPosition, IDWriteFontCollection** fontCollection, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontCollection(currentPosition, fontCollection, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyNameLength(UINT32 currentPosition, UINT32* nameLength, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontFamilyNameLength(currentPosition, nameLength, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyName(UINT32 currentPosition, WCHAR* fontFamilyName, UINT32 nameSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontFamilyName(currentPosition, fontFamilyName, nameSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontWeight(UINT32 currentPosition, DWRITE_FONT_WEIGHT* fontWeight, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontWeight(currentPosition, fontWeight, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontStyle(UINT32 currentPosition, DWRITE_FONT_STYLE* fontStyle, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontStyle(currentPosition, fontStyle, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontStretch(UINT32 currentPosition, DWRITE_FONT_STRETCH* fontStretch, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontStretch(currentPosition, fontStretch, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontSize(UINT32 currentPosition, FLOAT* fontSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetFontSize(currentPosition, fontSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetUnderline(UINT32 currentPosition, BOOL* hasUnderline, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetUnderline(currentPosition, hasUnderline, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetStrikethrough(UINT32 currentPosition, BOOL* hasStrikethrough, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetStrikethrough(currentPosition, hasStrikethrough, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetDrawingEffect(UINT32 currentPosition, IUnknown** drawingEffect, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetDrawingEffect(currentPosition, drawingEffect, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetInlineObject(UINT32 currentPosition, IDWriteInlineObject** inlineObject, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetInlineObject(currentPosition, inlineObject, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetTypography(UINT32 currentPosition, IDWriteTypography** typography, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetTypography(currentPosition, typography, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetLocaleNameLength(UINT32 currentPosition, UINT32* nameLength, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetLocaleNameLength(currentPosition, nameLength, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 currentPosition, WCHAR* localeName, UINT32 nameSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return _real->GetLocaleName(currentPosition, localeName, nameSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE Draw(void* clientDrawingContext, IDWriteTextRenderer* renderer, FLOAT originX, FLOAT originY) noexcept override
    {
        return _real->Draw(clientDrawingContext, renderer, originX, originY);
    }
    HRESULT STDMETHODCALLTYPE GetLineMetrics(DWRITE_LINE_METRICS* lineMetrics, UINT32 maxLineCount, UINT32* actualLineCount) noexcept override
    {
        if (! lineMetrics && _state->failLineMetrics)
        {
            ++_state->lineFailures;
            return _state->failure;
        }
        return _real->GetLineMetrics(lineMetrics, maxLineCount, actualLineCount);
    }
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_TEXT_METRICS* textMetrics) noexcept override
    {
        return _real->GetMetrics(textMetrics);
    }
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhangs) noexcept override
    {
        return _real->GetOverhangMetrics(overhangs);
    }
    HRESULT STDMETHODCALLTYPE GetClusterMetrics(DWRITE_CLUSTER_METRICS* clusterMetrics, UINT32 maxClusterCount, UINT32* actualClusterCount) noexcept override
    {
        return _real->GetClusterMetrics(clusterMetrics, maxClusterCount, actualClusterCount);
    }
    HRESULT STDMETHODCALLTYPE DetermineMinWidth(FLOAT* minWidth) noexcept override
    {
        return _real->DetermineMinWidth(minWidth);
    }
    HRESULT STDMETHODCALLTYPE
    HitTestPoint(FLOAT pointX, FLOAT pointY, BOOL* isTrailingHit, BOOL* isInside, DWRITE_HIT_TEST_METRICS* hitTestMetrics) noexcept override
    {
        return _real->HitTestPoint(pointX, pointY, isTrailingHit, isInside, hitTestMetrics);
    }
    HRESULT STDMETHODCALLTYPE
    HitTestTextPosition(UINT32 textPosition, BOOL isTrailingHit, FLOAT* pointX, FLOAT* pointY, DWRITE_HIT_TEST_METRICS* hitTestMetrics) noexcept override
    {
        return _real->HitTestTextPosition(textPosition, isTrailingHit, pointX, pointY, hitTestMetrics);
    }
    HRESULT STDMETHODCALLTYPE HitTestTextRange(UINT32 textPosition,
                                               UINT32 textLength,
                                               FLOAT originX,
                                               FLOAT originY,
                                               DWRITE_HIT_TEST_METRICS* hitTestMetrics,
                                               UINT32 maxHitTestMetricsCount,
                                               UINT32* actualHitTestMetricsCount) noexcept override
    {
        return _real->HitTestTextRange(textPosition, textLength, originX, originY, hitTestMetrics, maxHitTestMetricsCount, actualHitTestMetricsCount);
    }

private:
    wil::com_ptr<IDWriteTextLayout> _real;
    LayoutFaultState* _state; // Borrowed; the fixture outlives every layout.
};
#pragma warning(pop)
inline HRESULT WrapFaultLayout(void* context, IDWriteTextLayout* original, IDWriteTextLayout** output) noexcept
{
    auto& state      = *static_cast<LayoutFaultState*>(context);
    state.lastLayout = original;
    auto wrapped     = Microsoft::WRL::Make<FaultTextLayout>(original, &state);
    if (! wrapped)
        return E_OUTOFMEMORY;
    return wrapped.CopyTo(output);
}
} // namespace NativeTextTests
