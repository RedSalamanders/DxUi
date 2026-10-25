#pragma once
#include <dwrite.h>
#include <limits>
#include <string_view>
#include <wil/com.h>
#include <wrl/implements.h>
namespace NativeTextTests
{
// Counts the actual COM entry points, independently of a preparation service's
// returned statistics. All successful work reaches the real installed SDK.
class CountingWriteFactory final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IDWriteFactory>
{
public:
    explicit CountingWriteFactory(IDWriteFactory* factory) noexcept : real(factory)
    {
    }
    wil::com_ptr<IDWriteFactory> real;
    std::size_t layouts{}, formats{}, matchingLayouts{}, failedLayouts{}, failLayoutAt{std::numeric_limits<std::size_t>::max()}, failMatchingLayoutCount{};
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
        return real->CreateCustomFontCollection(collectionLoader, collectionKey, collectionKeySize, fontCollection);
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
        return real->CreateFontFileReference(filePath, lastWriteTime, fontFile);
    }
    HRESULT STDMETHODCALLTYPE CreateCustomFontFileReference(void const* fontFileReferenceKey,
                                                            UINT32 fontFileReferenceKeySize,
                                                            IDWriteFontFileLoader* fontFileLoader,
                                                            IDWriteFontFile** fontFile) noexcept override
    {
        return real->CreateCustomFontFileReference(fontFileReferenceKey, fontFileReferenceKeySize, fontFileLoader, fontFile);
    }
    HRESULT STDMETHODCALLTYPE CreateFontFace(DWRITE_FONT_FACE_TYPE fontFaceType,
                                             UINT32 numberOfFiles,
                                             IDWriteFontFile* const* fontFiles,
                                             UINT32 faceIndex,
                                             DWRITE_FONT_SIMULATIONS fontFaceSimulationFlags,
                                             IDWriteFontFace** fontFace) noexcept override
    {
        return real->CreateFontFace(fontFaceType, numberOfFiles, fontFiles, faceIndex, fontFaceSimulationFlags, fontFace);
    }
    HRESULT STDMETHODCALLTYPE CreateRenderingParams(IDWriteRenderingParams** renderingParams) noexcept override
    {
        return real->CreateRenderingParams(renderingParams);
    }
    HRESULT STDMETHODCALLTYPE CreateMonitorRenderingParams(HMONITOR monitor, IDWriteRenderingParams** renderingParams) noexcept override
    {
        return real->CreateMonitorRenderingParams(monitor, renderingParams);
    }
    HRESULT STDMETHODCALLTYPE CreateCustomRenderingParams(FLOAT gamma,
                                                          FLOAT enhancedContrast,
                                                          FLOAT clearTypeLevel,
                                                          DWRITE_PIXEL_GEOMETRY pixelGeometry,
                                                          DWRITE_RENDERING_MODE renderingMode,
                                                          IDWriteRenderingParams** renderingParams) noexcept override
    {
        return real->CreateCustomRenderingParams(gamma, enhancedContrast, clearTypeLevel, pixelGeometry, renderingMode, renderingParams);
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
        ++formats;
        if (FAILED(formatFailure))
        {
            if (textFormat)
                *textFormat = nullptr;
            return formatFailure;
        }
        return real->CreateTextFormat(fontFamilyName, fontCollection, fontWeight, fontStyle, fontStretch, fontSize, localeName, textFormat);
    }
    HRESULT STDMETHODCALLTYPE CreateTypography(IDWriteTypography** typography) noexcept override
    {
        return real->CreateTypography(typography);
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
        return real->CreateTextLayout(string, stringLength, textFormat, maxWidth, maxHeight, textLayout);
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
        return real->CreateGdiCompatibleTextLayout(
            string, stringLength, textFormat, layoutWidth, layoutHeight, pixelsPerDip, transform, useGdiNatural, textLayout);
    }
    HRESULT STDMETHODCALLTYPE CreateEllipsisTrimmingSign(IDWriteTextFormat* textFormat, IDWriteInlineObject** trimmingSign) noexcept override
    {
        return real->CreateEllipsisTrimmingSign(textFormat, trimmingSign);
    }
    HRESULT STDMETHODCALLTYPE CreateTextAnalyzer(IDWriteTextAnalyzer** textAnalyzer) noexcept override
    {
        return real->CreateTextAnalyzer(textAnalyzer);
    }
    HRESULT STDMETHODCALLTYPE CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD substitutionMethod,
                                                       WCHAR const* localeName,
                                                       BOOL ignoreUserOverride,
                                                       IDWriteNumberSubstitution** numberSubstitution) noexcept override
    {
        return real->CreateNumberSubstitution(substitutionMethod, localeName, ignoreUserOverride, numberSubstitution);
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
        return real->CreateGlyphRunAnalysis(
            glyphRun, pixelsPerDip, transform, renderingMode, measuringMode, baselineOriginX, baselineOriginY, glyphRunAnalysis);
    }
};
} // namespace NativeTextTests
