#pragma once
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
    wil::com_ptr<ID2D1RenderTarget> target_;
    std::vector<Entry> entries_;
    std::size_t cursor_{};
    std::size_t created_{};
    HRESULT result_ = E_INVALIDARG;
    bool valid_     = false;

public:
    NativeColorPalette(ID2D1RenderTarget* target, std::span<const std::uint32_t> colors) : target_(target), entries_(0u)
    {
        const Failure::Scope failureScope;
        if (! target || colors.size() > 8192u)
            return;
        entries_.reserve(colors.size());
        for (const auto color : colors)
        {
            if (color > 0xffffffu)
                return;
            entries_.push_back({color, {}});
        }
        std::ranges::sort(entries_, {}, &Entry::rgb);
        const auto duplicate = std::ranges::unique(entries_, {}, &Entry::rgb);
        entries_.erase(duplicate.begin(), duplicate.end());
        valid_  = true;
        result_ = S_OK;
    }
    ID2D1SolidColorBrush* Stage(std::uint32_t rgb)
    {
        const Failure::Scope failureScope;
        if (! valid_)
            return nullptr;
        result_          = S_OK;
        const auto entry = std::ranges::lower_bound(entries_, rgb, {}, &Entry::rgb);
        if (entry == entries_.end() || entry->rgb != rgb)
        {
            result_ = E_INVALIDARG;
            return nullptr;
        }
        if (! entry->brush)
        {
            const D2D1_COLOR_F color{
                static_cast<float>((rgb >> 16u) & 255u) / 255.f, static_cast<float>((rgb >> 8u) & 255u) / 255.f, static_cast<float>(rgb & 255u) / 255.f, 1.f};
#if DXUI_ENABLE_DIAGNOSTICS
            if (FAILED(result_ = testNativeColorPaletteBrushFailure))
                return nullptr;
#endif
            if (FAILED(result_ = target_->CreateSolidColorBrush(color, entry->brush.put())))
                return nullptr;
            ++created_;
        }
        return entry->brush.get();
    }
    bool Advance()
    {
        if (! valid_)
            return false;
        while (cursor_ < entries_.size() && entries_[cursor_].brush)
            ++cursor_;
        if (cursor_ < entries_.size())
        {
            if (! Stage(entries_[cursor_].rgb))
                return false;
            ++cursor_;
        }
        return Ready();
    }
    bool Ready() const noexcept
    {
        return valid_ && created_ == entries_.size();
    }
    HRESULT Result() const noexcept
    {
        return result_;
    }
    bool Matches(ID2D1RenderTarget* target) const noexcept
    {
        return target_.get() == target;
    }
    ID2D1SolidColorBrush* Brush(std::uint32_t color) const noexcept
    {
        if (! Ready())
            return nullptr;
        const auto found = std::ranges::lower_bound(entries_, color, {}, &Entry::rgb);
        return found != entries_.end() && found->rgb == color ? found->brush.get() : nullptr;
    }
};
} // namespace DxUi::RichText::Detail
