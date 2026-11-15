#pragma once
#include "Support.Tests.CallTicks.h"
#include <dwrite.h>
#include <limits>
#include <string_view>
#include <wil/com.h>
// Windows SDK WRL owns its interlocked/volatile implementation and target semantics.
// Scope is this system header only. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4746)
#include <wrl/implements.h>
#pragma warning(pop)
namespace NativeTextTests
{
// Counts the actual COM entry points, independently of a preparation service's
// returned statistics. All successful work reaches the real installed SDK.
// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class CountingWriteFactory final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFactory>
{
public:
    explicit CountingWriteFactory(IDWriteFactory* factory) noexcept : real(factory)
    {
    }
    wil::com_ptr<IDWriteFactory> real;
    HRESULT (*wrapLayout)(void*, IDWriteTextLayout*, IDWriteTextLayout**) noexcept = nullptr;
    void* wrapContext                                                              = nullptr;
    std::size_t layouts{}, formats{}, matchingLayouts{}, failedLayouts{}, failLayoutAt{std::numeric_limits<std::size_t>::max()}, failMatchingLayoutCount{};
    // Objects the real factory returned, by kind (work counters' cross-check, Support.Tests.CountingDirect2D.h): text formats,
    // text layouts (GDI-compatible ones included) and every other created object.
    std::size_t createdFormats{}, createdLayouts{}, otherCreations{};
    // With forwardUnknownInterfaces, an interface other than IDWriteFactory is answered by the real factory and counted here;
    // without it (the default) such a query is refused, as before.
    bool forwardUnknownInterfaces{};
    std::size_t escapedInterfaces{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        const HRESULT hr = RuntimeClass::QueryInterface(iid, object);
        if (hr != E_NOINTERFACE || ! forwardUnknownInterfaces || ! object)
            return hr;
        const HRESULT forwarded = real->QueryInterface(iid, object);
        if (SUCCEEDED(forwarded))
            ++escapedInterfaces;
        return forwarded;
    }
    HRESULT formatFailure{S_OK};
    HRESULT layoutFailure{E_OUTOFMEMORY};
    const wchar_t* failLayoutText{};
    HRESULT STDMETHODCALLTYPE GetSystemFontCollection(IDWriteFontCollection** fontCollection, BOOL checkForUpdates) noexcept override
    {
        return real->GetSystemFontCollection(fontCollection, checkForUpdates);
    }
    HRESULT STDMETHODCALLTYPE CreateCustomFontCollection(IDWriteFontCollectionLoader* collectionLoader,
                                                         void const* collectionKey,
                                                         UINT32 collectionKeySize,
                                                         IDWriteFontCollection** fontCollection) noexcept override
    {
        return Other(real->CreateCustomFontCollection(collectionLoader, collectionKey, collectionKeySize, fontCollection), fontCollection);
    }
    HRESULT STDMETHODCALLTYPE RegisterFontCollectionLoader(IDWriteFontCollectionLoader* fontCollectionLoader) noexcept override
    {
        return real->RegisterFontCollectionLoader(fontCollectionLoader);
    }
    HRESULT STDMETHODCALLTYPE UnregisterFontCollectionLoader(IDWriteFontCollectionLoader* fontCollectionLoader) noexcept override
    {
        return real->UnregisterFontCollectionLoader(fontCollectionLoader);
    }
    HRESULT STDMETHODCALLTYPE CreateFontFileReference(WCHAR const* filePath, FILETIME const* lastWriteTime, IDWriteFontFile** fontFile) noexcept override
    {
        return Other(real->CreateFontFileReference(filePath, lastWriteTime, fontFile), fontFile);
    }
    HRESULT STDMETHODCALLTYPE CreateCustomFontFileReference(void const* fontFileReferenceKey,
                                                            UINT32 fontFileReferenceKeySize,
                                                            IDWriteFontFileLoader* fontFileLoader,
                                                            IDWriteFontFile** fontFile) noexcept override
    {
        return Other(real->CreateCustomFontFileReference(fontFileReferenceKey, fontFileReferenceKeySize, fontFileLoader, fontFile), fontFile);
    }
    HRESULT STDMETHODCALLTYPE CreateFontFace(DWRITE_FONT_FACE_TYPE fontFaceType,
                                             UINT32 numberOfFiles,
                                             IDWriteFontFile* const* fontFiles,
                                             UINT32 faceIndex,
                                             DWRITE_FONT_SIMULATIONS fontFaceSimulationFlags,
                                             IDWriteFontFace** fontFace) noexcept override
    {
        return Other(real->CreateFontFace(fontFaceType, numberOfFiles, fontFiles, faceIndex, fontFaceSimulationFlags, fontFace), fontFace);
    }
    HRESULT STDMETHODCALLTYPE CreateRenderingParams(IDWriteRenderingParams** renderingParams) noexcept override
    {
        return Other(real->CreateRenderingParams(renderingParams), renderingParams);
    }
    HRESULT STDMETHODCALLTYPE CreateMonitorRenderingParams(HMONITOR monitor, IDWriteRenderingParams** renderingParams) noexcept override
    {
        return Other(real->CreateMonitorRenderingParams(monitor, renderingParams), renderingParams);
    }
    HRESULT STDMETHODCALLTYPE CreateCustomRenderingParams(FLOAT gamma,
                                                          FLOAT enhancedContrast,
                                                          FLOAT clearTypeLevel,
                                                          DWRITE_PIXEL_GEOMETRY pixelGeometry,
                                                          DWRITE_RENDERING_MODE renderingMode,
                                                          IDWriteRenderingParams** renderingParams) noexcept override
    {
        return Other(real->CreateCustomRenderingParams(gamma, enhancedContrast, clearTypeLevel, pixelGeometry, renderingMode, renderingParams),
                     renderingParams);
    }
    HRESULT STDMETHODCALLTYPE RegisterFontFileLoader(IDWriteFontFileLoader* fontFileLoader) noexcept override
    {
        return real->RegisterFontFileLoader(fontFileLoader);
    }
    HRESULT STDMETHODCALLTYPE UnregisterFontFileLoader(IDWriteFontFileLoader* fontFileLoader) noexcept override
    {
        return real->UnregisterFontFileLoader(fontFileLoader);
    }
    HRESULT STDMETHODCALLTYPE CreateTextFormat(WCHAR const* fontFamilyName,
                                               IDWriteFontCollection* fontCollection,
                                               DWRITE_FONT_WEIGHT fontWeight,
                                               DWRITE_FONT_STYLE fontStyle,
                                               DWRITE_FONT_STRETCH fontStretch,
                                               FLOAT fontSize,
                                               WCHAR const* localeName,
                                               IDWriteTextFormat** textFormat) noexcept override
    {
        const NativeWorkTests::TimedCall<&NativeWorkTests::CallTicks::textFormats> timed;
        ++formats;
        if (FAILED(formatFailure))
        {
            if (textFormat)
                *textFormat = nullptr;
            return formatFailure;
        }
        return Counted(real->CreateTextFormat(fontFamilyName, fontCollection, fontWeight, fontStyle, fontStretch, fontSize, localeName, textFormat),
                       textFormat,
                       createdFormats);
    }
    HRESULT STDMETHODCALLTYPE CreateTypography(IDWriteTypography** typography) noexcept override
    {
        return Other(real->CreateTypography(typography), typography);
    }
    HRESULT STDMETHODCALLTYPE GetGdiInterop(IDWriteGdiInterop** gdiInterop) noexcept override
    {
        return real->GetGdiInterop(gdiInterop);
    }
    HRESULT STDMETHODCALLTYPE CreateTextLayout(WCHAR const* string,
                                               UINT32 stringLength,
                                               IDWriteTextFormat* textFormat,
                                               FLOAT maxWidth,
                                               FLOAT maxHeight,
                                               IDWriteTextLayout** textLayout) noexcept override
    {
        const NativeWorkTests::TimedCall<&NativeWorkTests::CallTicks::textLayouts> timed;
        const auto call         = layouts++;
        const bool matchingText = failLayoutText && string && std::wstring_view(string, stringLength) == failLayoutText;
        matchingLayouts += matchingText;
        if (call == failLayoutAt || (matchingText && failMatchingLayoutCount != 0u))
        {
            if (matchingText && failMatchingLayoutCount != 0u)
                --failMatchingLayoutCount;
            ++failedLayouts;
            if (textLayout)
                *textLayout = nullptr;
            return layoutFailure;
        }
        if (! wrapLayout)
            return Counted(real->CreateTextLayout(string, stringLength, textFormat, maxWidth, maxHeight, textLayout), textLayout, createdLayouts);
        if (! textLayout)
            return E_INVALIDARG;
        *textLayout = nullptr;
        wil::com_ptr<IDWriteTextLayout> created;
        const auto result = real->CreateTextLayout(string, stringLength, textFormat, maxWidth, maxHeight, created.put());
        if (FAILED(result))
            return result;
        ++createdLayouts;
        return wrapLayout(wrapContext, created.get(), textLayout);
    }
    HRESULT STDMETHODCALLTYPE CreateGdiCompatibleTextLayout(WCHAR const* string,
                                                            UINT32 stringLength,
                                                            IDWriteTextFormat* textFormat,
                                                            FLOAT layoutWidth,
                                                            FLOAT layoutHeight,
                                                            FLOAT pixelsPerDip,
                                                            DWRITE_MATRIX const* transform,
                                                            BOOL useGdiNatural,
                                                            IDWriteTextLayout** textLayout) noexcept override
    {
        const NativeWorkTests::TimedCall<&NativeWorkTests::CallTicks::textLayouts> timed;
        return Counted(real->CreateGdiCompatibleTextLayout(
                           string, stringLength, textFormat, layoutWidth, layoutHeight, pixelsPerDip, transform, useGdiNatural, textLayout),
                       textLayout,
                       createdLayouts);
    }
    HRESULT STDMETHODCALLTYPE CreateEllipsisTrimmingSign(IDWriteTextFormat* textFormat, IDWriteInlineObject** trimmingSign) noexcept override
    {
        return Other(real->CreateEllipsisTrimmingSign(textFormat, trimmingSign), trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE CreateTextAnalyzer(IDWriteTextAnalyzer** textAnalyzer) noexcept override
    {
        return Other(real->CreateTextAnalyzer(textAnalyzer), textAnalyzer);
    }
    HRESULT STDMETHODCALLTYPE CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD substitutionMethod,
                                                       WCHAR const* localeName,
                                                       BOOL ignoreUserOverride,
                                                       IDWriteNumberSubstitution** numberSubstitution) noexcept override
    {
        return Other(real->CreateNumberSubstitution(substitutionMethod, localeName, ignoreUserOverride, numberSubstitution), numberSubstitution);
    }
    HRESULT STDMETHODCALLTYPE CreateGlyphRunAnalysis(DWRITE_GLYPH_RUN const* glyphRun,
                                                     FLOAT pixelsPerDip,
                                                     DWRITE_MATRIX const* transform,
                                                     DWRITE_RENDERING_MODE renderingMode,
                                                     DWRITE_MEASURING_MODE measuringMode,
                                                     FLOAT baselineOriginX,
                                                     FLOAT baselineOriginY,
                                                     IDWriteGlyphRunAnalysis** glyphRunAnalysis) noexcept override
    {
        return Other(
            real->CreateGlyphRunAnalysis(glyphRun, pixelsPerDip, transform, renderingMode, measuringMode, baselineOriginX, baselineOriginY, glyphRunAnalysis),
            glyphRunAnalysis);
    }

private:
    template <typename Interface> static HRESULT Counted(HRESULT hr, Interface** object, std::size_t& tally) noexcept
    {
        if (SUCCEEDED(hr) && object && *object)
            ++tally;
        return hr;
    }
    template <typename Interface> HRESULT Other(HRESULT hr, Interface** object) noexcept
    {
        return Counted(hr, object, otherCreations);
    }
};
#pragma warning(pop)
} // namespace NativeTextTests
