#pragma once
#include "Support/CountedCreation.h"
#include <DxUi/Configuration.h>
#include <DxUi/Failure.h>
#include <algorithm>
#include <cstdint>
#include <d2d1.h>
#include <span>
#include <stdexcept>
#include <vector>
#include <wil/com.h>
#include <wil/result.h>

namespace DxUi::RichText::Detail
{
#if DXUI_ENABLE_DIAGNOSTICS
inline thread_local HRESULT testNativeColorPaletteBrushFailure{S_OK};
#endif
// Raster resources are separate from width-independent math shapes. A palette
// is private to one target and is published only after every brush is staged.
class NativeColorPalette final
{
    struct Entry
    {
        std::uint32_t rgb{};
        wil::com_ptr<ID2D1SolidColorBrush> brush;
    };
    wil::com_ptr<ID2D1RenderTarget> _target;
    std::vector<Entry> _entries;
    std::size_t _cursor{};
    std::size_t _created{};
    HRESULT _result = E_INVALIDARG;
    bool _valid     = false;

public:
    NativeColorPalette(ID2D1RenderTarget* target, std::span<const std::uint32_t> colors) : _target(target), _entries(0u)
    {
        const Failure::Scope failureScope;
        if (! target || colors.size() > 8192u)
            return;
        _entries.reserve(colors.size());
        for (const auto color : colors)
        {
            if (color > 0xffffffu)
                return;
            _entries.push_back({color, {}});
        }
        std::ranges::sort(_entries, {}, &Entry::rgb);
        const auto duplicate = std::ranges::unique(_entries, {}, &Entry::rgb);
        _entries.erase(duplicate.begin(), duplicate.end());
        _valid  = true;
        _result = S_OK;
    }
    ID2D1SolidColorBrush* Stage(std::uint32_t rgb)
    {
        const Failure::Scope failureScope;
        if (! _valid)
            return nullptr;
        _result          = S_OK;
        const auto entry = std::ranges::lower_bound(_entries, rgb, {}, &Entry::rgb);
        if (entry == _entries.end() || entry->rgb != rgb)
        {
            _result = E_INVALIDARG;
            return nullptr;
        }
        if (! entry->brush)
        {
            const D2D1_COLOR_F color{
                static_cast<float>((rgb >> 16u) & 255u) / 255.f, static_cast<float>((rgb >> 8u) & 255u) / 255.f, static_cast<float>(rgb & 255u) / 255.f, 1.f};
#if DXUI_ENABLE_DIAGNOSTICS
            if (FAILED(_result = testNativeColorPaletteBrushFailure))
                return nullptr;
#endif
            if (FAILED(_result = CountedCreateSolidColorBrush(_target, color, entry->brush.put())))
                return nullptr;
            ++_created;
        }
        return entry->brush.get();
    }
    bool Advance()
    {
        if (! _valid)
            return false;
        while (_cursor < _entries.size() && _entries[_cursor].brush)
            ++_cursor;
        if (_cursor < _entries.size())
        {
            if (! Stage(_entries[_cursor].rgb))
                return false;
            ++_cursor;
        }
        return Ready();
    }
    bool Ready() const noexcept
    {
        return _valid && _created == _entries.size();
    }
    HRESULT Result() const noexcept
    {
        return _result;
    }
    bool Matches(ID2D1RenderTarget* target) const noexcept
    {
        return _target.get() == target;
    }
    ID2D1SolidColorBrush* Brush(std::uint32_t color) const noexcept
    {
        if (! Ready())
            return nullptr;
        const auto found = std::ranges::lower_bound(_entries, color, {}, &Entry::rgb);
        return found != _entries.end() && found->rgb == color ? found->brush.get() : nullptr;
    }
};
} // namespace DxUi::RichText::Detail
