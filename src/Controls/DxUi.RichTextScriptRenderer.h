#pragma once
#include <DxUi/Failure.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <d2d1_3.h>
#include <dwrite_2.h>
#include <iterator>
#include <new>
#include <wil/com.h>

namespace DxUi::RichText::Detail
{
struct ScriptFont final
{
    wil::com_ptr<IDWriteFontFace> face;
    bool color{};
};
struct ScriptDecoration final
{
    float left{}, right{}, baseline{}, offset{};
};
struct ScriptMetadata final
{
    ScriptMetadata() : fonts(0u), decorations(0u)
    {
    }
    std::vector<ScriptFont> fonts;
    std::vector<ScriptDecoration> decorations;
    bool requiresColor{}, unsupported{};
};
// The layout and every effect brush are borrowed for one synchronous draw.
// No Direct2D resource or glyph buffer is created here.
// Existing identity/RAII/reference traits intentionally delete implicit operations.
// Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4626 5027)
class ScriptTextRenderer final : public IDWriteTextRenderer
{
public:
    ScriptTextRenderer(ID2D1RenderTarget* target,
                       ID2D1Brush* brush,
                       IDWriteTextLayout* layout,
                       const Projection& projection,
                       UINT32 textOffset,
                       D2D1_POINT_2F origin,
                       ScriptMetadata& metadata,
                       bool staging) noexcept
        : target_(target),
          brush_(brush),
          layout_(layout),
          projection_(projection),
          textOffset_(textOffset),
          origin_(origin),
          metadata_(metadata),
          staging_(staging)
    {
        if (target_)
        {
            const auto result = target_->QueryInterface(IID_PPV_ARGS(color_.put()));
            if (FAILED(result) && result != E_NOINTERFACE)
                result_ = result;
        }
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) noexcept override
    {
        if (! object)
            return E_POINTER;
        *object = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWriteTextRenderer))
            *object = static_cast<IDWriteTextRenderer*>(this);
        else if (iid == __uuidof(IDWritePixelSnapping))
            *object = static_cast<IDWritePixelSnapping*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() noexcept override
    {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return references_ ? --references_ : 0u;
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
        D2D1_MATRIX_3X2_F value = D2D1::Matrix3x2F::Identity();
        if (target_)
            target_->GetTransform(&value);
        *transform = {value._11, value._12, value._21, value._22, value._31, value._32};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) noexcept override
    {
        if (! pixels)
            return E_POINTER;
        FLOAT x = 96.f, y = 96.f;
        if (target_)
            target_->GetDpi(&x, &y);
        if (x <= 0.f || y <= 0.f)
            return E_INVALIDARG;
        *pixels = x / 96.f;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*,
                                           FLOAT x,
                                           FLOAT y,
                                           DWRITE_MEASURING_MODE mode,
                                           const DWRITE_GLYPH_RUN* glyphs,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION* description,
                                           IUnknown* effect) noexcept override
    {
        if (! glyphs)
        {
            result_ = E_INVALIDARG;
            return S_OK;
        }
        if (! glyphs->glyphCount)
            return S_OK;
        if (! description || ! description->stringLength || description->textPosition < textOffset_ ||
            static_cast<std::size_t>(description->textPosition - textOffset_) > projection_.text.size() ||
            description->stringLength > projection_.text.size() - static_cast<std::size_t>(description->textPosition - textOffset_))
        {
            if (SUCCEEDED(result_))
                result_ = DWRITE_E_UNSUPPORTEDOPERATION;
            return S_OK;
        }
        const auto first  = static_cast<std::size_t>(description->textPosition - textOffset_);
        const auto last   = first + description->stringLength;
        const auto flags  = Flags(description->textPosition);
        const auto script = flags & (Superscript | Subscript);
        auto run          = std::lower_bound(projection_.runs.begin(), projection_.runs.end(), first, [](const StyleRun& value, std::size_t position) {
            return value.start + value.length <= position;
        });
        for (; run != projection_.runs.end() && run->start < last; ++run)
            if ((run->flags & (Superscript | Subscript)) != script)
            {
                if (SUCCEEDED(result_))
                    result_ = DWRITE_E_UNSUPPORTEDOPERATION;
                return S_OK;
            }
        y += Offset(flags, glyphs->fontEmSize);
        if (! glyphs->fontFace)
        {
            result_ = E_INVALIDARG;
            return S_OK;
        }
        if (staging_)
        {
            const auto found =
                std::find_if(metadata_.fonts.begin(), metadata_.fonts.end(), [&](const auto& entry) { return entry.face.get() == glyphs->fontFace; });
            if (found != metadata_.fonts.end())
                return S_OK;
            wil::com_ptr<IDWriteFontFace2> face;
            const auto faceResult = glyphs->fontFace->QueryInterface(IID_PPV_ARGS(face.put()));
            if (faceResult == E_NOINTERFACE)
            {
                if (SUCCEEDED(result_))
                    result_ = DWRITE_E_UNSUPPORTEDOPERATION;
                return S_OK;
            }
            if (FAILED(faceResult))
            {
                result_ = faceResult;
                return S_OK;
            }
            try
            {
                ScriptFont entry;
                entry.face  = glyphs->fontFace;
                entry.color = face->IsColorFont();
                metadata_.requiresColor |= entry.color;
                metadata_.fonts.push_back(std::move(entry));
            }
            catch (const std::bad_alloc&)
            {
                Failure::FailAllocation();
            }
            return S_OK;
        }
        if (flags & (Superscript | Subscript))
            y = SnapY(y);
        auto* ink = Brush(effect);
        if (! ink)
        {
            result_ = E_INVALIDARG;
            return S_OK;
        }
        if (color_)
            color_->DrawGlyphRunWithColorSupport({x, y}, glyphs, description, ink, nullptr, 0u, mode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
        else
            target_->DrawGlyphRun({x, y}, glyphs, ink, mode);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT x, FLOAT y, const DWRITE_UNDERLINE* line, IUnknown* effect) noexcept override
    {
        if (! line)
            return E_INVALIDARG;
        if (staging_)
        {
            StageDecoration(x, y, line->width);
            return S_OK;
        }
        auto* ink = Brush(effect);
        if (! ink)
        {
            result_ = E_INVALIDARG;
            return S_OK;
        }
        y += DecorationOffset(x, y);
        Stroke(x, y + line->offset, line->width, line->thickness, ink);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH* line, IUnknown* effect) noexcept override
    {
        if (! line)
            return E_INVALIDARG;
        if (staging_)
        {
            StageDecoration(x, y, line->width);
            return S_OK;
        }
        auto* ink = Brush(effect);
        if (! ink)
        {
            result_ = E_INVALIDARG;
            return S_OK;
        }
        y = SnapY(y) + DecorationOffset(x, y);
        target_->FillRectangle(D2D1::RectF(x, y + line->offset, x + line->width, y + line->offset + line->thickness), ink);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    DrawInlineObject(void* context, FLOAT x, FLOAT y, IDWriteInlineObject* object, BOOL sideways, BOOL rtl, IUnknown* effect) noexcept override
    {
        if (! object)
            return E_INVALIDARG;
        if (staging_)
            return S_OK;
        const auto result = object->Draw(context, this, x, y, sideways, rtl, effect);
        if (FAILED(result))
            result_ = result;
        return S_OK;
    }
    HRESULT Result() const noexcept
    {
        return result_;
    }

private:
    unsigned Flags(UINT32 position) const noexcept
    {
        if (position < textOffset_)
            return 0u;
        const auto index = static_cast<std::size_t>(position - textOffset_);
        const auto found =
            std::upper_bound(projection_.runs.begin(), projection_.runs.end(), index, [](std::size_t value, const StyleRun& run) { return value < run.start; });
        if (found == projection_.runs.begin())
            return 0u;
        const auto& run = *std::prev(found);
        return index - run.start < run.length ? run.flags : 0u;
    }
    void StageDecoration(float x, float y, float width) noexcept
    {
        BOOL trailing{}, inside{};
        DWRITE_HIT_TEST_METRICS metrics{};
        const auto hit = layout_->HitTestPoint(x - origin_.x + .01f, y - origin_.y, &trailing, &inside, &metrics);
        if (FAILED(hit))
        {
            result_ = hit;
            return;
        }
        float size{};
        const auto sized = layout_->GetFontSize(metrics.textPosition, &size, nullptr);
        if (FAILED(sized))
        {
            result_ = sized;
            return;
        }
        try
        {
            metadata_.decorations.push_back({x - origin_.x, x - origin_.x + width, y - origin_.y, Offset(Flags(metrics.textPosition), size)});
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }
    float DecorationOffset(float x, float y) const noexcept
    {
        x -= origin_.x;
        y -= origin_.y;
        auto line = std::lower_bound(
            metadata_.decorations.begin(), metadata_.decorations.end(), y - 2.f, [](const auto& span, float baseline) { return span.baseline < baseline; });
        const ScriptDecoration* best = nullptr;
        while (line != metadata_.decorations.end() && line->baseline < y + 2.f)
        {
            const auto end =
                std::upper_bound(line, metadata_.decorations.end(), line->baseline, [](float baseline, const auto& span) { return baseline < span.baseline; });
            const auto candidate = std::upper_bound(line, end, x + .02f, [](float position, const auto& span) { return position < span.left; });
            if (candidate != line)
            {
                const auto& span = *std::prev(candidate);
                if (x < span.right - .01f && (! best || std::abs(span.baseline - y) < std::abs(best->baseline - y)))
                    best = &span;
            }
            line = end;
        }
        if (best)
            return best->offset;
        return 0.f;
    }
    static float Offset(unsigned flags, float scriptSize) noexcept
    {
        const auto parentSize = scriptSize / .75f;
        return flags & Superscript ? -.30f * parentSize : flags & Subscript ? .20f * parentSize : 0.f;
    }
    float SnapY(float y) const noexcept
    {
        D2D1_MATRIX_3X2_F transform{};
        target_->GetTransform(&transform);
        FLOAT dpiX{}, dpiY{};
        target_->GetDpi(&dpiX, &dpiY);
        const auto scale     = transform._22 * dpiY / 96.f;
        const auto translate = transform._32 * dpiY / 96.f;
        return transform._12 == 0.f && transform._21 == 0.f && std::isfinite(scale) && scale > 0.f && std::isfinite(translate)
                   ? (std::round(y * scale + translate) - translate) / scale
                   : y;
    }
    void Stroke(float x, float y, float width, float thickness, ID2D1Brush* ink) noexcept
    {
        D2D1_MATRIX_3X2_F transform{};
        target_->GetTransform(&transform);
        FLOAT dpiX{}, dpiY{};
        target_->GetDpi(&dpiX, &dpiY);
        const auto scale     = transform._22 * dpiY / 96.f;
        const auto translate = transform._32 * dpiY / 96.f;
        if (transform._12 == 0.f && transform._21 == 0.f && std::isfinite(scale) && scale > 0.f && std::isfinite(translate))
        {
            y         = (std::round(y * scale + translate) - translate) / scale;
            thickness = std::max(1.f, std::round(thickness * scale)) / scale;
        }
        target_->FillRectangle(D2D1::RectF(x, y, x + width, y + thickness), ink);
    }
    ID2D1Brush* Brush(IUnknown* effect) noexcept
    {
        effectBrush_.reset();
        if (! effect)
            return brush_;
        return SUCCEEDED(effect->QueryInterface(IID_PPV_ARGS(effectBrush_.put()))) ? effectBrush_.get() : nullptr;
    }
    ID2D1RenderTarget* target_{};
    ID2D1Brush* brush_{};
    IDWriteTextLayout* layout_{};
    const Projection& projection_;
    UINT32 textOffset_{};
    D2D1_POINT_2F origin_{};
    wil::com_ptr<ID2D1DeviceContext7> color_;
    wil::com_ptr<ID2D1Brush> effectBrush_;
    ULONG references_{1u};
    HRESULT result_{S_OK};
    ScriptMetadata& metadata_;
    bool staging_{};
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
