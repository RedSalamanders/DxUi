#pragma once
#include <DxUi/Configuration.h>
#include <d2d1.h>
#include <wil/com.h>
#include <wil/result.h>
#include <algorithm>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

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

public:
    NativeColorPalette(ID2D1RenderTarget* target, std::span<const std::uint32_t> colors) : target_(target), entries_(0u)
    {
        if (! target || colors.size() > 8192u)
            throw std::invalid_argument("Invalid native color palette");
        entries_.reserve(colors.size());
        for (const auto color : colors)
        {
            if (color > 0xffffffu)
                throw std::invalid_argument("Invalid native color");
            entries_.push_back({color, {}});
        }
        std::ranges::sort(entries_, {}, &Entry::rgb);
        const auto duplicate = std::ranges::unique(entries_, {}, &Entry::rgb);
        entries_.erase(duplicate.begin(), duplicate.end());
    }
    ID2D1SolidColorBrush* Stage(std::uint32_t rgb)
    {
        const auto entry = std::ranges::lower_bound(entries_, rgb, {}, &Entry::rgb);
        if (entry == entries_.end() || entry->rgb != rgb)
            throw std::invalid_argument("Color was not admitted to the native palette");
        if (! entry->brush)
        {
            const D2D1_COLOR_F color{static_cast<float>((rgb >> 16u) & 255u) / 255.f,
                                     static_cast<float>((rgb >> 8u) & 255u) / 255.f,
                                     static_cast<float>(rgb & 255u) / 255.f, 1.f};
#if DXUI_ENABLE_DIAGNOSTICS
            THROW_IF_FAILED(testNativeColorPaletteBrushFailure);
#endif
            THROW_IF_FAILED(target_->CreateSolidColorBrush(color, entry->brush.put()));
            ++created_;
        }
        return entry->brush.get();
    }
    bool Advance()
    {
        while (cursor_ < entries_.size() && entries_[cursor_].brush)
            ++cursor_;
        if (cursor_ < entries_.size())
        {
            static_cast<void>(Stage(entries_[cursor_].rgb));
            ++cursor_;
        }
        return Ready();
    }
    bool Ready() const noexcept { return created_ == entries_.size(); }
    bool Matches(ID2D1RenderTarget* target) const noexcept { return target_.get() == target; }
    ID2D1SolidColorBrush* Brush(std::uint32_t color) const noexcept
    {
        if (! Ready())
            return nullptr;
        const auto found = std::ranges::lower_bound(entries_, color, {}, &Entry::rgb);
        return found != entries_.end() && found->rgb == color ? found->brush.get() : nullptr;
    }
};
} // namespace DxUi::RichText::Detail
