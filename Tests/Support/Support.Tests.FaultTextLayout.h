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
    FaultTextLayout(IDWriteTextLayout* real, LayoutFaultState* state) noexcept : real_(real), state_(state)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** result) noexcept override
    {
        if (riid == __uuidof(IDWriteTextLayout1))
        {
            ++state_->layout1Queries;
            if (state_->failLayout1Query)
            {
                if (result)
                    *result = nullptr;
                return state_->layout1Failure;
            }
            wil::com_ptr<IDWriteTextLayout1> native;
            const HRESULT hr = wil::com_query_to_nothrow(real_.get(), native.put());
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
        const HRESULT hr = wil::com_query_to_nothrow(real_.get(), native.put());
        return FAILED(hr) ? hr : native->SetPairKerning(enabled, range);
    }
    HRESULT STDMETHODCALLTYPE GetPairKerning(UINT32 position, BOOL* enabled, DWRITE_TEXT_RANGE* range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(real_.get(), native.put());
        return FAILED(hr) ? hr : native->GetPairKerning(position, enabled, range);
    }
    HRESULT STDMETHODCALLTYPE SetCharacterSpacing(FLOAT leading, FLOAT trailing, FLOAT minimum, DWRITE_TEXT_RANGE range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(real_.get(), native.put());
        return FAILED(hr) ? hr : native->SetCharacterSpacing(leading, trailing, minimum, range);
    }
    HRESULT STDMETHODCALLTYPE GetCharacterSpacing(UINT32 position, FLOAT* leading, FLOAT* trailing, FLOAT* minimum, DWRITE_TEXT_RANGE* range) noexcept override
    {
        wil::com_ptr<IDWriteTextLayout1> native;
        const HRESULT hr = wil::com_query_to_nothrow(real_.get(), native.put());
        return FAILED(hr) ? hr : native->GetCharacterSpacing(position, leading, trailing, minimum, range);
    }
    HRESULT STDMETHODCALLTYPE SetTextAlignment(DWRITE_TEXT_ALIGNMENT textAlignment) noexcept override
    {
        return real_->SetTextAlignment(textAlignment);
    }
    HRESULT STDMETHODCALLTYPE SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT paragraphAlignment) noexcept override
    {
        return real_->SetParagraphAlignment(paragraphAlignment);
    }
    HRESULT STDMETHODCALLTYPE SetWordWrapping(DWRITE_WORD_WRAPPING wordWrapping) noexcept override
    {
        return real_->SetWordWrapping(wordWrapping);
    }
    HRESULT STDMETHODCALLTYPE SetReadingDirection(DWRITE_READING_DIRECTION readingDirection) noexcept override
    {
        return real_->SetReadingDirection(readingDirection);
    }
    HRESULT STDMETHODCALLTYPE SetFlowDirection(DWRITE_FLOW_DIRECTION flowDirection) noexcept override
    {
        return real_->SetFlowDirection(flowDirection);
    }
    HRESULT STDMETHODCALLTYPE SetIncrementalTabStop(FLOAT incrementalTabStop) noexcept override
    {
        return real_->SetIncrementalTabStop(incrementalTabStop);
    }
    HRESULT STDMETHODCALLTYPE SetTrimming(DWRITE_TRIMMING const* trimmingOptions, IDWriteInlineObject* trimmingSign) noexcept override
    {
        return real_->SetTrimming(trimmingOptions, trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE SetLineSpacing(DWRITE_LINE_SPACING_METHOD lineSpacingMethod, FLOAT lineSpacing, FLOAT baseline) noexcept override
    {
        return real_->SetLineSpacing(lineSpacingMethod, lineSpacing, baseline);
    }
    DWRITE_TEXT_ALIGNMENT STDMETHODCALLTYPE GetTextAlignment() noexcept override
    {
        return real_->GetTextAlignment();
    }
    DWRITE_PARAGRAPH_ALIGNMENT STDMETHODCALLTYPE GetParagraphAlignment() noexcept override
    {
        return real_->GetParagraphAlignment();
    }
    DWRITE_WORD_WRAPPING STDMETHODCALLTYPE GetWordWrapping() noexcept override
    {
        return real_->GetWordWrapping();
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetReadingDirection() noexcept override
    {
        return real_->GetReadingDirection();
    }
    DWRITE_FLOW_DIRECTION STDMETHODCALLTYPE GetFlowDirection() noexcept override
    {
        return real_->GetFlowDirection();
    }
    FLOAT STDMETHODCALLTYPE GetIncrementalTabStop() noexcept override
    {
        return real_->GetIncrementalTabStop();
    }
    HRESULT STDMETHODCALLTYPE GetTrimming(DWRITE_TRIMMING* trimmingOptions, IDWriteInlineObject** trimmingSign) noexcept override
    {
        return real_->GetTrimming(trimmingOptions, trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE GetLineSpacing(DWRITE_LINE_SPACING_METHOD* lineSpacingMethod, FLOAT* lineSpacing, FLOAT* baseline) noexcept override
    {
        return real_->GetLineSpacing(lineSpacingMethod, lineSpacing, baseline);
    }
    HRESULT STDMETHODCALLTYPE GetFontCollection(IDWriteFontCollection** fontCollection) noexcept override
    {
        return real_->GetFontCollection(fontCollection);
    }
    UINT32 STDMETHODCALLTYPE GetFontFamilyNameLength() noexcept override
    {
        return real_->GetFontFamilyNameLength();
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyName(WCHAR* fontFamilyName, UINT32 nameSize) noexcept override
    {
        return real_->GetFontFamilyName(fontFamilyName, nameSize);
    }
    DWRITE_FONT_WEIGHT STDMETHODCALLTYPE GetFontWeight() noexcept override
    {
        return real_->GetFontWeight();
    }
    DWRITE_FONT_STYLE STDMETHODCALLTYPE GetFontStyle() noexcept override
    {
        return real_->GetFontStyle();
    }
    DWRITE_FONT_STRETCH STDMETHODCALLTYPE GetFontStretch() noexcept override
    {
        return real_->GetFontStretch();
    }
    FLOAT STDMETHODCALLTYPE GetFontSize() noexcept override
    {
        return real_->GetFontSize();
    }
    UINT32 STDMETHODCALLTYPE GetLocaleNameLength() noexcept override
    {
        return real_->GetLocaleNameLength();
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(WCHAR* localeName, UINT32 nameSize) noexcept override
    {
        return real_->GetLocaleName(localeName, nameSize);
    }
    HRESULT STDMETHODCALLTYPE SetMaxWidth(FLOAT maxWidth) noexcept override
    {
        return real_->SetMaxWidth(maxWidth);
    }
    HRESULT STDMETHODCALLTYPE SetMaxHeight(FLOAT maxHeight) noexcept override
    {
        return real_->SetMaxHeight(maxHeight);
    }
    HRESULT STDMETHODCALLTYPE SetFontCollection(IDWriteFontCollection* fontCollection, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontCollection(fontCollection, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontFamilyName(WCHAR const* fontFamilyName, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontFamilyName(fontFamilyName, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontWeight(DWRITE_FONT_WEIGHT fontWeight, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontWeight(fontWeight, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontStyle(DWRITE_FONT_STYLE fontStyle, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontStyle(fontStyle, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontStretch(DWRITE_FONT_STRETCH fontStretch, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontStretch(fontStretch, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetFontSize(FLOAT fontSize, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetFontSize(fontSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetUnderline(BOOL hasUnderline, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        const auto call = state_->underlineCalls++;
        if (call == state_->failUnderlineAt || call == state_->failUnderlineAt2)
        {
            if (state_->mutateBeforeFailure && call == state_->failUnderlineAt)
                (void)real_->SetUnderline(hasUnderline, textRange);
            return state_->failure;
        }
        return real_->SetUnderline(hasUnderline, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetStrikethrough(BOOL hasStrikethrough, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetStrikethrough(hasStrikethrough, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetDrawingEffect(IUnknown* drawingEffect, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        const auto call = state_->effectCalls++;
        if (call == state_->failEffectAt || call == state_->failEffectAt2)
        {
            if (state_->mutateBeforeFailure && call == state_->failEffectAt)
                (void)real_->SetDrawingEffect(drawingEffect, textRange);
            return state_->failure;
        }
        return real_->SetDrawingEffect(drawingEffect, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetInlineObject(IDWriteInlineObject* inlineObject, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetInlineObject(inlineObject, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetTypography(IDWriteTypography* typography, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetTypography(typography, textRange);
    }
    HRESULT STDMETHODCALLTYPE SetLocaleName(WCHAR const* localeName, DWRITE_TEXT_RANGE textRange) noexcept override
    {
        return real_->SetLocaleName(localeName, textRange);
    }
    FLOAT STDMETHODCALLTYPE GetMaxWidth() noexcept override
    {
        return real_->GetMaxWidth();
    }
    FLOAT STDMETHODCALLTYPE GetMaxHeight() noexcept override
    {
        return real_->GetMaxHeight();
    }
    HRESULT STDMETHODCALLTYPE GetFontCollection(UINT32 currentPosition, IDWriteFontCollection** fontCollection, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontCollection(currentPosition, fontCollection, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyNameLength(UINT32 currentPosition, UINT32* nameLength, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontFamilyNameLength(currentPosition, nameLength, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontFamilyName(UINT32 currentPosition, WCHAR* fontFamilyName, UINT32 nameSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontFamilyName(currentPosition, fontFamilyName, nameSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontWeight(UINT32 currentPosition, DWRITE_FONT_WEIGHT* fontWeight, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontWeight(currentPosition, fontWeight, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontStyle(UINT32 currentPosition, DWRITE_FONT_STYLE* fontStyle, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontStyle(currentPosition, fontStyle, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontStretch(UINT32 currentPosition, DWRITE_FONT_STRETCH* fontStretch, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontStretch(currentPosition, fontStretch, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetFontSize(UINT32 currentPosition, FLOAT* fontSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetFontSize(currentPosition, fontSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetUnderline(UINT32 currentPosition, BOOL* hasUnderline, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetUnderline(currentPosition, hasUnderline, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetStrikethrough(UINT32 currentPosition, BOOL* hasStrikethrough, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetStrikethrough(currentPosition, hasStrikethrough, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetDrawingEffect(UINT32 currentPosition, IUnknown** drawingEffect, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetDrawingEffect(currentPosition, drawingEffect, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetInlineObject(UINT32 currentPosition, IDWriteInlineObject** inlineObject, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetInlineObject(currentPosition, inlineObject, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetTypography(UINT32 currentPosition, IDWriteTypography** typography, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetTypography(currentPosition, typography, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetLocaleNameLength(UINT32 currentPosition, UINT32* nameLength, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetLocaleNameLength(currentPosition, nameLength, textRange);
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 currentPosition, WCHAR* localeName, UINT32 nameSize, DWRITE_TEXT_RANGE* textRange) noexcept override
    {
        return real_->GetLocaleName(currentPosition, localeName, nameSize, textRange);
    }
    HRESULT STDMETHODCALLTYPE Draw(void* clientDrawingContext, IDWriteTextRenderer* renderer, FLOAT originX, FLOAT originY) noexcept override
    {
        return real_->Draw(clientDrawingContext, renderer, originX, originY);
    }
    HRESULT STDMETHODCALLTYPE GetLineMetrics(DWRITE_LINE_METRICS* lineMetrics, UINT32 maxLineCount, UINT32* actualLineCount) noexcept override
    {
        if (! lineMetrics && state_->failLineMetrics)
        {
            ++state_->lineFailures;
            return state_->failure;
        }
        return real_->GetLineMetrics(lineMetrics, maxLineCount, actualLineCount);
    }
    HRESULT STDMETHODCALLTYPE GetMetrics(DWRITE_TEXT_METRICS* textMetrics) noexcept override
    {
        return real_->GetMetrics(textMetrics);
    }
    HRESULT STDMETHODCALLTYPE GetOverhangMetrics(DWRITE_OVERHANG_METRICS* overhangs) noexcept override
    {
        return real_->GetOverhangMetrics(overhangs);
    }
    HRESULT STDMETHODCALLTYPE GetClusterMetrics(DWRITE_CLUSTER_METRICS* clusterMetrics, UINT32 maxClusterCount, UINT32* actualClusterCount) noexcept override
    {
        return real_->GetClusterMetrics(clusterMetrics, maxClusterCount, actualClusterCount);
    }
    HRESULT STDMETHODCALLTYPE DetermineMinWidth(FLOAT* minWidth) noexcept override
    {
        return real_->DetermineMinWidth(minWidth);
    }
    HRESULT STDMETHODCALLTYPE
    HitTestPoint(FLOAT pointX, FLOAT pointY, BOOL* isTrailingHit, BOOL* isInside, DWRITE_HIT_TEST_METRICS* hitTestMetrics) noexcept override
    {
        return real_->HitTestPoint(pointX, pointY, isTrailingHit, isInside, hitTestMetrics);
    }
    HRESULT STDMETHODCALLTYPE
    HitTestTextPosition(UINT32 textPosition, BOOL isTrailingHit, FLOAT* pointX, FLOAT* pointY, DWRITE_HIT_TEST_METRICS* hitTestMetrics) noexcept override
    {
        return real_->HitTestTextPosition(textPosition, isTrailingHit, pointX, pointY, hitTestMetrics);
    }
    HRESULT STDMETHODCALLTYPE HitTestTextRange(UINT32 textPosition,
                                               UINT32 textLength,
                                               FLOAT originX,
                                               FLOAT originY,
                                               DWRITE_HIT_TEST_METRICS* hitTestMetrics,
                                               UINT32 maxHitTestMetricsCount,
                                               UINT32* actualHitTestMetricsCount) noexcept override
    {
        return real_->HitTestTextRange(textPosition, textLength, originX, originY, hitTestMetrics, maxHitTestMetricsCount, actualHitTestMetricsCount);
    }

private:
    wil::com_ptr<IDWriteTextLayout> real_;
    LayoutFaultState* state_; // Borrowed; the fixture outlives every layout.
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
