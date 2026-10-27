#pragma once
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
        : target_(target),
          baseBrush_(baseBrush),
          drawing_(drawing),
          positioning_(positioning),
          textOffset_(textOffset),
          layoutText_(layoutText),
          colorContext_(colorContext)
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
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        if (references_ != 0u)
            --references_;
        return references_;
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
        target_->GetTransform(&current);
        *transform = {current._11, current._12, current._21, current._22, current._31, current._32};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixelsPerDip) noexcept override
    {
        if (! pixelsPerDip)
            return E_POINTER;
        FLOAT dpiX{}, dpiY{};
        target_->GetDpi(&dpiX, &dpiY);
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
        if (! painting_)
        {
            requiresColorContext_ = requiresColorContext_ || useColor;
            return S_OK;
        }

        if (useColor)
        {
            if (! colorContext_)
                return Reject();
            colorContext_->DrawGlyphRunWithColorSupport(
                {x, y}, run, description, BrushForRole(glyphRoles[0]), nullptr, 0u, measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
            return S_OK;
        }

        if (! monospaced)
        {
            target_->DrawGlyphRun({x, y}, run, BrushForRole(glyphRoles[0]), measuringMode);
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
            target_->DrawGlyphRun({partX, y}, &part, drawBrush, measuringMode);
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
        return object == positioning_ && positioning_ && ! sideways && ! effect ? S_OK : Reject();
    }

    bool Accepted() const noexcept
    {
        return accepted_;
    }
    void BeginPaint() noexcept
    {
        painting_ = true;
    }
    bool RequiresColorContext() const noexcept
    {
        return requiresColorContext_;
    }
    void SetColorContext(ID2D1DeviceContext7* context) noexcept
    {
        colorContext_ = context;
    }

private:
    struct InkSpan
    {
        std::size_t first{}, last{};
        int role{};
    };
    HRESULT Reject() noexcept
    {
        accepted_ = false;
        return E_NOTIMPL;
    }
    bool IsPositioningPrefixLineFeed(const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown* effect) const noexcept
    {
        return positioning_ && textOffset_ == 2u && run && ! run->glyphCount && ! run->isSideways && ! effect && description && description->string &&
               description->textPosition == 1u && description->stringLength == 1u && description->string[0] == L'\n';
    }
    bool IsNonpaintingSourceLineBreak(const DWRITE_GLYPH_RUN* run, const DWRITE_GLYPH_RUN_DESCRIPTION* description, IUnknown* effect) const noexcept
    {
        // DirectWrite emits source CR/LF callbacks without glyphs or a cluster
        // map. They advance no ink; verify the exact source interval before
        // accepting them so other empty or malformed runs still fall back.
        if (! run || run->glyphCount || run->isSideways || (run->bidiLevel & 1u) || effect || ! description || ! description->string ||
            description->textPosition < textOffset_)
            return false;
        const auto first  = static_cast<std::size_t>(description->textPosition - textOffset_);
        const auto length = static_cast<std::size_t>(description->stringLength);
        if (first > layoutText_.size() || length > layoutText_.size() - first)
            return false;
        const std::wstring_view text(description->string, length);
        return (text == L"\n" || text == L"\r" || text == L"\r\n") && text == layoutText_.substr(first, length);
    }
    void BuildInkSpans() noexcept
    {
        if (! drawing_ || drawing_->inkExclusion || drawing_->displayOrigin > (std::numeric_limits<std::size_t>::max)() - layoutText_.size())
        {
            accepted_ = false;
            return;
        }
        const std::size_t displayEnd = drawing_->displayOrigin + layoutText_.size();
        auto ink =
            std::lower_bound(drawing_->inkRanges.begin(), drawing_->inkRanges.end(), drawing_->displayOrigin, [](const TextInkRange& range, std::size_t first) {
            return range.display.last <= first;
        });
        std::size_t previousEnd = 0u;
        bool havePrevious       = false;
        for (; ink != drawing_->inkRanges.end() && ink->display.first < displayEnd; ++ink)
        {
            if (ink->display.first >= ink->display.last || (havePrevious && ink->display.first < previousEnd))
            {
                accepted_ = false;
                return;
            }
            previousEnd     = ink->display.last;
            havePrevious    = true;
            const auto role = static_cast<std::size_t>(ink->role);
            if (role >= drawing_->inkBrushes.size() || ! drawing_->inkBrushes[role])
                continue;
            const auto first = (std::max)(ink->display.first, drawing_->displayOrigin);
            const auto last  = (std::min)(ink->display.last, displayEnd);
            if (first >= last)
                continue;
            if (inkSpanCount_ == inkSpans_.size())
            {
                accepted_ = false;
                return;
            }
            inkSpans_[inkSpanCount_++] = {first - drawing_->displayOrigin, last - drawing_->displayOrigin, static_cast<int>(role)};
        }
        if (! inkSpanCount_)
            accepted_ = false;
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
            ! description->clusterMap || description->textPosition < textOffset_ ||
            static_cast<std::size_t>(description->textPosition - textOffset_) + description->stringLength > layoutText_.size())
            return false;
        const auto textOffset = static_cast<std::size_t>(description->textPosition - textOffset_);
        if (std::wstring_view(description->string, description->stringLength) != layoutText_.substr(textOffset, description->stringLength))
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
        const std::size_t displayPosition = absoluteTextPosition - textOffset_;
        for (std::size_t index = 0; index < inkSpanCount_; ++index)
        {
            const auto& span = inkSpans_[index];
            if (span.first > displayPosition)
                break;
            if (span.first <= displayPosition && displayPosition < span.last)
                return span.role;
        }
        return -1;
    }
    ID2D1Brush* BrushForRole(int role) const noexcept
    {
        return role >= 0 ? drawing_->inkBrushes[static_cast<std::size_t>(role)] : baseBrush_;
    }

    ID2D1RenderTarget* target_{};
    ID2D1Brush* baseBrush_{};
    const LinkDrawing* drawing_{};
    IDWriteInlineObject* positioning_{};
    UINT32 textOffset_{};
    std::wstring_view layoutText_;
    ID2D1DeviceContext7* colorContext_{};
    std::array<InkSpan, 32> inkSpans_{};
    std::size_t inkSpanCount_{};
    ULONG references_{1u};
    bool painting_{};
    bool requiresColorContext_{};
    bool accepted_{true};
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
                                                    SourceInkColorContextLookup colorContextLookup = SourceInkColorContextLookup::QueryTarget)
{
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
        return SourceInkDrawResult::Rejected;

    wil::com_ptr<ID2D1DeviceContext7> colorContext;
    if (renderer.RequiresColorContext())
    {
        if (colorContextLookup == SourceInkColorContextLookup::Unavailable || FAILED(target->QueryInterface(IID_PPV_ARGS(colorContext.put()))))
            return SourceInkDrawResult::Rejected;
    }
    renderer.SetColorContext(colorContext.get());
    renderer.BeginPaint();
    THROW_IF_FAILED(layout->Draw(nullptr, &renderer, origin.x, origin.y));
    if (! renderer.Accepted())
        THROW_HR_MSG(E_UNEXPECTED, "Preflighted source syntax renderer rejected a paint callback");
    return SourceInkDrawResult::Drawn;
}
} // namespace DxUi::RichText::Detail
