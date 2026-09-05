#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <windows.h>
namespace DxUi::Detail
{
inline uint32_t StableVisualHash32Utf16V1(std::wstring_view text) noexcept
{
    uint32_t hash = 2166136261u;
    for (wchar_t ch : text)
    {
        hash ^= static_cast<uint32_t>(ch);
        hash *= 16777619u;
    }
    return hash;
}
inline double SrgbChannelToLinear(double c) noexcept
{
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
inline double RelativeLuminanceFromSrgb(double r, double g, double b) noexcept
{
    return .2126 * SrgbChannelToLinear(r) + .7152 * SrgbChannelToLinear(g) + .0722 * SrgbChannelToLinear(b);
}
inline double ContrastRatioFromRelativeLuminance(double a, double b) noexcept
{
    return (std::max(a, b) + .05) / (std::min(a, b) + .05);
}
inline float PixelToDip(float px, float dpi) noexcept
{
    return static_cast<float>(px) * 96.0f / static_cast<float>(dpi ? dpi : 96u);
}
} // namespace DxUi::Detail
