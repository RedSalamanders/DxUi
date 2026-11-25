#pragma once
#include <DxUi/Configuration.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <d2d1_3.h>
#include <dwrite_2.h>
#include <limits>
#include <string_view>
#include <wil/com.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
inline thread_local std::size_t testSourceInkRendererAttempts{};
#endif
enum class SourceInkDrawResult
{
    NotApplicable,
    Rejected,
    Drawn,
};

// A deliberately narrow renderer for source-only syntax ink. The layout,
// Direct2D context and brushes are borrowed for the synchronous Draw call; all
// run scratch is fixed-size stack storage and no drawing attribute is retained.
class SourceInkRenderer final : public IDWriteTextRenderer
{
public:
    SourceInkRenderer(ID2D1RenderTarget* target,
                      ID2D1Brush* baseBrush,
                      const LinkDrawing* drawing,
                      IDWriteInlineObject* positioning,
                      UINT32 textOffset,
                      std::wstring_view layoutText,
                      ID2D1DeviceContext7* colorContext = nullptr) noexcept
        : _target(target),
          _baseBrush(baseBrush),
          _drawing(drawing),
          _positioning(positioning),
          _textOffset(textOffset),
          _layoutText(layoutText),
          _colorContext(colorContext)
    {
        BuildInkSpans();
    }

    // DirectWrite borrows this renderer only for IDWriteTextLayout::Draw. The
    // stack owner remains authoritative; Release never deletes stack storage.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        if (! object)
            return E_POINTER;
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteTextRenderer))
        {
            *object = static_cast<IDWriteTextRenderer*>(this);
            AddRef();
            return S_OK;
        }
        if (iid == __uuidof(IDWritePixelSnapping))
        {
            *object = static_cast<IDWritePixelSnapping*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        if (_references != 0u)
            --_references;
        return _references;
    }

    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* disabled) noexcept override
    {
        if (! disabled)
            return E_POINTER;
        *disabled = FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* transform) noexcept override
    {
        if (! transform)
            return E_POINTER;
        D2D1_MATRIX_3X2_F current{};
        _target->GetTransform(&current);
        *transform = {current._11, current._12, current._21, current._22, current._31, current._32};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) noexcept override
    {
        if (! pixelsPerDip)
            return E_POINTER;
        FLOAT dpiX{}, dpiY{};
        _target->GetDpi(&dpiX, &dpiY);
        if (! std::isfinite(dpiX) || ! std::isfinite(dpiY) || dpiX <= 0.0f || dpiY <= 0.0f || dpiX != dpiY)
            return E_NOTIMPL;
        *pixelsPerDip = dpiX / 96.0f;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,
                                           FLOAT x,
                                           FLOAT y,
                                           DWRITE_MEASURING_MODE measuringMode,
                                           const DWRITE_GLYPH_RUN* run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                                           IUnknown* effect) noexcept override
    {
        if (IsPositioningPrefixLineFeed(run, description, effect) || IsNonpaintingSourceLineBreak(run, description, effect))
            return S_OK;
        std::array<int, 256> glyphRoles{};
        bool useColor{};
        bool monospaced{};
        if (effect || ! Eligible(run, description, glyphRoles, monospaced, useColor))
            return Reject();
        if (! _painting)
        {
            _requiresColorContext = _requiresColorContext || useColor;
            return S_OK;
        }

        if (useColor)
        {
            if (! _colorContext)
                return Reject();
            _colorContext->DrawGlyphRunWithColorSupport(
                {x, y}, run, description, BrushForRole(glyphRoles[0]), nullptr, 0u, measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
            return S_OK;
        }

        if (! monospaced)
        {
            _target->DrawGlyphRun({x, y}, run, BrushForRole(glyphRoles[0]), measuringMode);
            return S_OK;
        }

        const auto* ink = run;

        UINT32 first = 0;
        FLOAT partX  = x;
        while (first < run->glyphCount)
        {
            const int role        = glyphRoles[first];
            ID2D1Brush* drawBrush = BrushForRole(role);
            UINT32 last           = first + 1;
            while (last < run->glyphCount && BrushForRole(glyphRoles[last]) == drawBrush)
                ++last;
            DWRITE_GLYPH_RUN part = *ink;
            part.glyphCount       = last - first;
            part.glyphIndices     = ink->glyphIndices + first;
            part.glyphAdvances    = ink->glyphAdvances + first;
            part.glyphOffsets     = ink->glyphOffsets ? ink->glyphOffsets + first : nullptr;
            _target->DrawGlyphRun({partX, y}, &part, drawBrush, measuringMode);
            for (UINT32 index = first; index < last; ++index)
                partX += ink->glyphAdvances[index];
            first = last;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) noexcept override
    {
        return Reject();
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) noexcept override
    {
        return Reject();
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject* object, BOOL sideways, BOOL, IUnknown* effect) noexcept override
    {
        return object == _positioning && _positioning && ! sideways && ! effect ? S_OK : Reject();
    }

    bool Accepted() const noexcept
    {
        return _accepted;
    }
    void BeginPaint() noexcept
    {
        _painting = true;
    }
    bool RequiresColorContext() const noexcept
    {
        return _requiresColorContext;
    }
    void SetColorContext(ID2D1DeviceContext7* context) noexcept
    {
        _colorContext = context;
    }

private:
    struct InkSpan
    {
        std::size_t first{}, last{};
        int role{};
    };
    HRESULT Reject() noexcept
    {
        _accepted = false;
        return E_NOTIMPL;
    }
    bool IsPositioningPrefixLineFeed(const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown* effect) const noexcept
    {
        return _positioning && _textOffset == 2u && run && ! run->glyphCount && ! run->isSideways && ! effect && description && description->string &&
               description->textPosition == 1u && description->stringLength == 1u && description->string[0] == L'\n';
    }
    bool IsNonpaintingSourceLineBreak(const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown* effect) const noexcept
    {
        // DirectWrite emits source CR/LF callbacks without glyphs or a cluster
        // map. They advance no ink; verify the exact source interval before
        // accepting them so other empty or malformed runs still fall back.
        if (! run || run->glyphCount || run->isSideways || (run->bidiLevel & 1u) || effect || ! description || ! description->string ||
            description->textPosition < _textOffset)
            return false;
        const auto first  = static_cast<std::size_t>(description->textPosition - _textOffset);
        const auto length = static_cast<std::size_t>(description->stringLength);
        if (first > _layoutText.size() || length > _layoutText.size() - first)
            return false;
        const std::wstring_view text(description->string, length);
        return (text == L"\n" || text == L"\r" || text == L"\r\n") && text == _layoutText.substr(first, length);
    }
    void BuildInkSpans() noexcept
    {
        if (! _drawing || _drawing->inkExclusion || _drawing->displayOrigin > (std::numeric_limits<std::size_t>::max)() - _layoutText.size())
        {
            _accepted = false;
            return;
        }
        const std::size_t displayEnd = _drawing->displayOrigin + _layoutText.size();
        auto ink =
            std::lower_bound(_drawing->inkRanges.begin(), _drawing->inkRanges.end(), _drawing->displayOrigin, [](const TextInkRange& range, std::size_t first) {
            return range.display.last <= first;
        });
        std::size_t previousEnd = 0u;
        bool havePrevious       = false;
        for (; ink != _drawing->inkRanges.end() && ink->display.first < displayEnd; ++ink)
        {
            if (ink->display.first >= ink->display.last || (havePrevious && ink->display.first < previousEnd))
            {
                _accepted = false;
                return;
            }
            previousEnd     = ink->display.last;
            havePrevious    = true;
            const auto role = static_cast<std::size_t>(ink->role);
            if (role >= _drawing->inkBrushes.size() || ! _drawing->inkBrushes[role])
                continue;
            const auto first = (std::max)(ink->display.first, _drawing->displayOrigin);
            const auto last  = (std::min)(ink->display.last, displayEnd);
            if (first >= last)
                continue;
            if (_inkSpanCount == _inkSpans.size())
            {
                _accepted = false;
                return;
            }
            _inkSpans[_inkSpanCount++] = {first - _drawing->displayOrigin, last - _drawing->displayOrigin, static_cast<int>(role)};
        }
        if (! _inkSpanCount)
            _accepted = false;
    }
    bool Eligible(const DWRITE_GLYPH_RUN* run,
                  const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                  std::array<int, 256>& glyphRoles,
                  bool& monospaced,
                  bool& useColor) noexcept
    {
        monospaced = false;
        useColor   = false;
        if (! run || ! description || ! run->fontFace || run->isSideways || (run->bidiLevel & 1u) || ! run->glyphCount || run->glyphCount > 256u ||
            ! run->glyphIndices || ! run->glyphAdvances || ! description->string || ! description->stringLength || description->stringLength > 512u ||
            ! description->clusterMap || description->textPosition < _textOffset ||
            static_cast<std::size_t>(description->textPosition - _textOffset) + description->stringLength > _layoutText.size())
            return false;
        const auto textOffset = static_cast<std::size_t>(description->textPosition - _textOffset);
        if (std::wstring_view(description->string, description->stringLength) != _layoutText.substr(textOffset, description->stringLength))
            return false;
        wil::com_ptr<IDWriteFontFace1> face1;
        wil::com_ptr<IDWriteFontFace2> face2;
        if (FAILED(run->fontFace->QueryInterface(IID_PPV_ARGS(face1.put()))) || FAILED(run->fontFace->QueryInterface(IID_PPV_ARGS(face2.put()))))
            return false;
        monospaced = face1->IsMonospacedFont();
        useColor   = face2->IsColorFont();
        for (UINT32 index = 0; index < run->glyphCount; ++index)
            if (! std::isfinite(run->glyphAdvances[index]) || run->glyphAdvances[index] < 0.0f)
                return false;
        if (! BuildGlyphRoles(run, description, glyphRoles))
            return false;
        if (monospaced && ! useColor)
            return true;
        return HasUniformBrush(run, glyphRoles);
    }
    bool HasUniformBrush(const DWRITE_GLYPH_RUN* run, const std::array<int, 256>& glyphRoles) const noexcept
    {
        if (! run || ! run->glyphCount)
            return false;
        const ID2D1Brush* const firstBrush = BrushForRole(glyphRoles[0]);
        if (! firstBrush)
            return false;
        for (UINT32 index = 1; index < run->glyphCount; ++index)
            if (BrushForRole(glyphRoles[index]) != firstBrush)
                return false;
        return true;
    }
    bool BuildGlyphRoles(const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, std::array<int, 256>& glyphRoles) const noexcept
    {
        std::array<int, 256> clusterRoles{};
        clusterRoles.fill(-2);
        glyphRoles.fill(-2);
        for (UINT32 unit = 0; unit < description->stringLength; ++unit)
        {
            const auto glyph = description->clusterMap[unit];
            const auto role  = RoleFor(description->textPosition + unit);
            if (glyph >= run->glyphCount || (clusterRoles[glyph] != -2 && clusterRoles[glyph] != role))
                return false;
            clusterRoles[glyph] = role;
        }
        UINT32 inkSegments = 0;
        int previous       = -1;
        UINT32 glyph       = 0;
        while (glyph < run->glyphCount)
        {
            if (clusterRoles[glyph] == -2)
                return false;
            const auto role = clusterRoles[glyph];
            if (role >= 0 && role != previous)
                ++inkSegments;
            UINT32 next = glyph + 1;
            while (next < run->glyphCount && clusterRoles[next] == -2)
                ++next;
            std::fill(glyphRoles.begin() + glyph, glyphRoles.begin() + next, role);
            previous = role;
            glyph    = next;
        }
        return inkSegments <= 8u;
    }
    int RoleFor(UINT32 absoluteTextPosition) const noexcept
    {
        const std::size_t displayPosition = absoluteTextPosition - _textOffset;
        for (std::size_t index = 0; index < _inkSpanCount; ++index)
        {
            const auto& span = _inkSpans[index];
            if (span.first > displayPosition)
                break;
            if (span.first <= displayPosition && displayPosition < span.last)
                return span.role;
        }
        return -1;
    }
    ID2D1Brush* BrushForRole(int role) const noexcept
    {
        return role >= 0 ? _drawing->inkBrushes[static_cast<std::size_t>(role)] : _baseBrush;
    }

    ID2D1RenderTarget* _target{};
    ID2D1Brush* _baseBrush{};
    const LinkDrawing* _drawing{};
    IDWriteInlineObject* _positioning{};
    UINT32 _textOffset{};
    std::wstring_view _layoutText;
    ID2D1DeviceContext7* _colorContext{};
    std::array<InkSpan, 32> _inkSpans{};
    std::size_t _inkSpanCount{};
    ULONG _references{1u};
    bool _painting{};
    bool _requiresColorContext{};
    bool _accepted{true};
};

enum class SourceInkColorContextLookup : uint8_t
{
    QueryTarget,
    Unavailable,
};

inline SourceInkDrawResult DrawSourceInkIfSupported(ID2D1RenderTarget* target,
                                                    ID2D1Brush* baseBrush,
                                                    IDWriteTextLayout* layout,
                                                    IDWriteInlineObject* positioning,
                                                    UINT32 textOffset,
                                                    const Projection& projection,
                                                    const LinkDrawing& drawing,
                                                    D2D1_POINT_2F origin,
                                                    SourceInkColorContextLookup colorContextLookup = SourceInkColorContextLookup::QueryTarget,
                                                    HRESULT* paintFailure                          = nullptr)
{
    if (paintFailure)
        *paintFailure = S_OK;
#if DXUI_ENABLE_DIAGNOSTICS
    ++testSourceInkRendererAttempts;
#endif
    if (! target || ! baseBrush || ! layout || projection.mode != ViewMode::Source || drawing.inkExclusion || drawing.emphasisBrush ||
        drawing.inactiveLinkBrush || drawing.emphasis || drawing.hover || drawing.caret ||
        std::ranges::any_of(projection.runs, [](const StyleRun& run) { return run.flags != 0u; }))
        return SourceInkDrawResult::NotApplicable;
    FLOAT dpiX{}, dpiY{};
    target->GetDpi(&dpiX, &dpiY);
    if (! std::isfinite(dpiX) || ! std::isfinite(dpiY) || dpiX <= 0.0f || dpiY <= 0.0f || dpiX != dpiY)
        return SourceInkDrawResult::Rejected;

    SourceInkRenderer renderer(target, baseBrush, &drawing, positioning, textOffset, projection.text);
    if (! renderer.Accepted())
        return SourceInkDrawResult::Rejected;
    const HRESULT preflight = layout->Draw(nullptr, &renderer, origin.x, origin.y);
    if (FAILED(preflight) || ! renderer.Accepted())
    {
        if (paintFailure && FAILED(preflight) && renderer.Accepted() && preflight != E_NOTIMPL)
            *paintFailure = preflight;
        return SourceInkDrawResult::Rejected;
    }

    wil::com_ptr<ID2D1DeviceContext7> colorContext;
    if (renderer.RequiresColorContext())
    {
        if (colorContextLookup == SourceInkColorContextLookup::Unavailable)
            return SourceInkDrawResult::Rejected;
        const auto hr = target->QueryInterface(IID_PPV_ARGS(colorContext.put()));
        if (FAILED(hr))
        {
            if (paintFailure && hr != E_NOINTERFACE)
                *paintFailure = hr;
            return SourceInkDrawResult::Rejected;
        }
    }
    renderer.SetColorContext(colorContext.get());
    renderer.BeginPaint();
    const auto paintResult = layout->Draw(nullptr, &renderer, origin.x, origin.y);
    if (FAILED(paintResult) || ! renderer.Accepted())
    {
        if (paintFailure)
            *paintFailure = FAILED(paintResult) ? paintResult : E_UNEXPECTED;
        return SourceInkDrawResult::Rejected;
    }
    return SourceInkDrawResult::Drawn;
}
} // namespace DxUi::RichText::Detail
