#pragma once
#include <DxUi/Failure.h>
#include <DxUi/RichTextLayout.h>
#include <algorithm>
#include <cmath>
#include <d2d1_3.h>
#include <dwrite_2.h>
#include <iterator>
#include <new>
#include <span>
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
struct UniformSourceLine final
{
    // DirectWrite sums float line heights in document order. Retain those
    // coordinates rather than reassociating a chunk-local total in Draw.
    std::size_t first{};
    float localTop{}, documentTop{}, localBaseline{}, documentBaseline{};
};
inline const UniformSourceLine& UniformLineAt(std::span<const UniformSourceLine> lines, std::size_t position) noexcept
{
    const auto after =
        std::upper_bound(lines.begin(), lines.end(), position, [](std::size_t value, const UniformSourceLine& line) { return value < line.first; });
    return after == lines.begin() ? lines.front() : *std::prev(after);
}
struct ScriptMetadata final
{
    ScriptMetadata() : fonts(0u), decorations(0u), uniformSourceLines(0u)
    {
    }
    std::vector<ScriptFont> fonts;
    std::vector<ScriptDecoration> decorations;
    std::vector<UniformSourceLine> uniformSourceLines;
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
        : _target(target),
          _brush(brush),
          _layout(layout),
          _projection(projection),
          _textOffset(textOffset),
          _origin(origin),
          _metadata(metadata),
          _staging(staging)
    {
        if (_target)
        {
            const auto result = _target->QueryInterface(IID_PPV_ARGS(_color.put()));
            if (FAILED(result) && result != E_NOINTERFACE)
                _result = result;
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
        return ++_references;
    }
    ULONG STDMETHODCALLTYPE Release() noexcept override
    {
        return _references ? --_references : 0u;
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
        if (_target)
            _target->GetTransform(&value);
        *transform = {value._11, value._12, value._21, value._22, value._31, value._32};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* pixels) noexcept override
    {
        if (! pixels)
            return E_POINTER;
        FLOAT x = 96.f, y = 96.f;
        if (_target)
            _target->GetDpi(&x, &y);
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
            _result = E_INVALIDARG;
            return S_OK;
        }
        if (! glyphs->glyphCount)
            return S_OK;
        if (! description || ! description->stringLength || description->textPosition < _textOffset ||
            static_cast<std::size_t>(description->textPosition - _textOffset) > _projection.text.size() ||
            description->stringLength > _projection.text.size() - static_cast<std::size_t>(description->textPosition - _textOffset))
        {
            if (SUCCEEDED(_result))
                _result = DWRITE_E_UNSUPPORTEDOPERATION;
            return S_OK;
        }
        const auto first  = static_cast<std::size_t>(description->textPosition - _textOffset);
        const auto last   = first + description->stringLength;
        const auto flags  = Flags(description->textPosition);
        const auto script = flags & (Superscript | Subscript);
        auto run          = std::lower_bound(_projection.runs.begin(), _projection.runs.end(), first, [](const StyleRun& value, std::size_t position) {
            return value.start + value.length <= position;
        });
        for (; run != _projection.runs.end() && run->start < last; ++run)
            if ((run->flags & (Superscript | Subscript)) != script)
            {
                if (SUCCEEDED(_result))
                    _result = DWRITE_E_UNSUPPORTEDOPERATION;
                return S_OK;
            }
        if (! _staging && ! _metadata.uniformSourceLines.empty())
            y = SnapY(_origin.y + UniformLineAt(_metadata.uniformSourceLines, first).documentBaseline, true);
        y += Offset(flags, glyphs->fontEmSize);
        if (! glyphs->fontFace)
        {
            _result = E_INVALIDARG;
            return S_OK;
        }
        if (_staging)
        {
            const auto found =
                std::find_if(_metadata.fonts.begin(), _metadata.fonts.end(), [&](const auto& entry) { return entry.face.get() == glyphs->fontFace; });
            if (found != _metadata.fonts.end())
                return S_OK;
            wil::com_ptr<IDWriteFontFace2> face;
            const auto faceResult = glyphs->fontFace->QueryInterface(IID_PPV_ARGS(face.put()));
            if (faceResult == E_NOINTERFACE)
            {
                if (SUCCEEDED(_result))
                    _result = DWRITE_E_UNSUPPORTEDOPERATION;
                return S_OK;
            }
            if (FAILED(faceResult))
            {
                _result = faceResult;
                return S_OK;
            }
            try
            {
                ScriptFont entry;
                entry.face  = glyphs->fontFace;
                entry.color = face->IsColorFont();
                _metadata.requiresColor |= entry.color;
                _metadata.fonts.push_back(std::move(entry));
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
            _result = E_INVALIDARG;
            return S_OK;
        }
        if (_color)
            _color->DrawGlyphRunWithColorSupport({x, y}, glyphs, description, ink, nullptr, 0u, mode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
        else
            _target->DrawGlyphRun({x, y}, glyphs, ink, mode);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT x, FLOAT y, const DWRITE_UNDERLINE* line, IUnknown* effect) noexcept override
    {
        if (! line)
            return E_INVALIDARG;
        if (_staging)
        {
            StageDecoration(x, y, line->width);
            return S_OK;
        }
        auto* ink = Brush(effect);
        if (! ink)
        {
            _result = E_INVALIDARG;
            return S_OK;
        }
        const auto decorationOffset = DecorationOffset(x, y);
        y                           = UniformBaseline(y) + decorationOffset;
        Stroke(x, y + line->offset, line->width, line->thickness, ink);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT x, FLOAT y, const DWRITE_STRIKETHROUGH* line, IUnknown* effect) noexcept override
    {
        if (! line)
            return E_INVALIDARG;
        if (_staging)
        {
            StageDecoration(x, y, line->width);
            return S_OK;
        }
        auto* ink = Brush(effect);
        if (! ink)
        {
            _result = E_INVALIDARG;
            return S_OK;
        }
        const auto decorationOffset = DecorationOffset(x, y);
        y                           = SnapY(UniformBaseline(y)) + decorationOffset;
        _target->FillRectangle(D2D1::RectF(x, y + line->offset, x + line->width, y + line->offset + line->thickness), ink);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE
    DrawInlineObject(void* context, FLOAT x, FLOAT y, IDWriteInlineObject* object, BOOL sideways, BOOL rtl, IUnknown* effect) noexcept override
    {
        if (! object)
            return E_INVALIDARG;
        if (_staging)
            return S_OK;
        const auto result = object->Draw(context, this, x, y, sideways, rtl, effect);
        if (FAILED(result))
            _result = result;
        return S_OK;
    }
    HRESULT Result() const noexcept
    {
        return _result;
    }

private:
    unsigned Flags(UINT32 position) const noexcept
    {
        if (position < _textOffset)
            return 0u;
        const auto index = static_cast<std::size_t>(position - _textOffset);
        const auto found =
            std::upper_bound(_projection.runs.begin(), _projection.runs.end(), index, [](std::size_t value, const StyleRun& run) { return value < run.start; });
        if (found == _projection.runs.begin())
            return 0u;
        const auto& run = *std::prev(found);
        return index - run.start < run.length ? run.flags : 0u;
    }
    void StageDecoration(float x, float y, float width) noexcept
    {
        BOOL trailing{}, inside{};
        DWRITE_HIT_TEST_METRICS metrics{};
        const auto hit = _layout->HitTestPoint(x - _origin.x + .01f, y - _origin.y, &trailing, &inside, &metrics);
        if (FAILED(hit))
        {
            _result = hit;
            return;
        }
        float size{};
        const auto sized = _layout->GetFontSize(metrics.textPosition, &size, nullptr);
        if (FAILED(sized))
        {
            _result = sized;
            return;
        }
        try
        {
            _metadata.decorations.push_back({x - _origin.x, x - _origin.x + width, y - _origin.y, Offset(Flags(metrics.textPosition), size)});
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }
    float DecorationOffset(float x, float y) const noexcept
    {
        x -= _origin.x;
        y -= _origin.y;
        auto line = std::lower_bound(
            _metadata.decorations.begin(), _metadata.decorations.end(), y - 2.f, [](const auto& span, float baseline) { return span.baseline < baseline; });
        const ScriptDecoration* best = nullptr;
        while (line != _metadata.decorations.end() && line->baseline < y + 2.f)
        {
            const auto end =
                std::upper_bound(line, _metadata.decorations.end(), line->baseline, [](float baseline, const auto& span) { return baseline < span.baseline; });
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
    float SnapY(float y, bool horizontalSkew = false) const noexcept
    {
        D2D1_MATRIX_3X2_F transform{};
        _target->GetTransform(&transform);
        FLOAT dpiX{}, dpiY{};
        _target->GetDpi(&dpiX, &dpiY);
        const auto scale     = transform._22 * dpiY / 96.f;
        const auto translate = transform._32 * dpiY / 96.f;
        return transform._12 == 0.f && (horizontalSkew || transform._21 == 0.f) && std::isfinite(scale) && scale > 0.f && std::isfinite(translate)
                   ? (std::round(y * scale + translate) - translate) / scale
                   : y;
    }
    void Stroke(float x, float y, float width, float thickness, ID2D1Brush* ink) noexcept
    {
        D2D1_MATRIX_3X2_F transform{};
        _target->GetTransform(&transform);
        FLOAT dpiX{}, dpiY{};
        _target->GetDpi(&dpiX, &dpiY);
        const auto scale     = transform._22 * dpiY / 96.f;
        const auto translate = transform._32 * dpiY / 96.f;
        if (transform._12 == 0.f && transform._21 == 0.f && std::isfinite(scale) && scale > 0.f && std::isfinite(translate))
        {
            y         = (std::round(y * scale + translate) - translate) / scale;
            thickness = std::max(1.f, std::round(thickness * scale)) / scale;
        }
        _target->FillRectangle(D2D1::RectF(x, y, x + width, y + thickness), ink);
    }
    ID2D1Brush* Brush(IUnknown* effect) noexcept
    {
        _effectBrush.reset();
        if (! effect)
            return _brush;
        return SUCCEEDED(effect->QueryInterface(IID_PPV_ARGS(_effectBrush.put()))) ? _effectBrush.get() : nullptr;
    }
    float UniformBaseline(float y) const noexcept
    {
        const auto& lines = _metadata.uniformSourceLines;
        if (lines.empty())
            return y;
        const auto local = y - _origin.y;
        auto line =
            std::lower_bound(lines.begin(), lines.end(), local, [](const UniformSourceLine& value, float baseline) { return value.localBaseline < baseline; });
        if (line == lines.end() || (line != lines.begin() && local - std::prev(line)->localBaseline < line->localBaseline - local))
            --line;
        return _origin.y + line->documentBaseline;
    }
    ID2D1RenderTarget* _target{};
    ID2D1Brush* _brush{};
    IDWriteTextLayout* _layout{};
    const Projection& _projection;
    UINT32 _textOffset{};
    D2D1_POINT_2F _origin{};
    wil::com_ptr<ID2D1DeviceContext7> _color;
    wil::com_ptr<ID2D1Brush> _effectBrush;
    ULONG _references{1u};
    HRESULT _result{S_OK};
    ScriptMetadata& _metadata;
    bool _staging{};
};
#pragma warning(pop)
} // namespace DxUi::RichText::Detail
