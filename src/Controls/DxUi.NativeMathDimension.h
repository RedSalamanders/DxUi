#pragma once
#include <optional>
#include <string_view>

namespace DxUi::RichText::Detail
{
enum class MathDimensionUnit
{
    Em,
    Ex,
    Point
};
struct MathDimension
{
    float value{};
    MathDimensionUnit unit{};
};
// Bounded, locale-independent nonnegative TeX dimensions. Unsupported units,
// signs, excessive precision and oversized strokes fail admission.
inline std::optional<MathDimension> ParseMathDimension(std::wstring_view text) noexcept
{
    if (text == L"0")
        return MathDimension{};
    if (text.size() < 3u || text.size() > 16u)
        return std::nullopt;
    const auto unitText = text.substr(text.size() - 2u);
    MathDimensionUnit unit;
    if (unitText == L"em")
        unit = MathDimensionUnit::Em;
    else if (unitText == L"ex")
        unit = MathDimensionUnit::Ex;
    else if (unitText == L"pt")
        unit = MathDimensionUnit::Point;
    else
        return std::nullopt;
    const auto number = text.substr(0u, text.size() - 2u);
    double value{}, fraction = .1;
    bool dot{}, digit{};
    for (const auto c : number)
    {
        if (c == L'.' && ! dot)
        {
            dot = true;
            continue;
        }
        if (c < L'0' || c > L'9')
            return std::nullopt;
        digit = true;
        if (dot)
        {
            value += static_cast<unsigned>(c - L'0') * fraction;
            fraction *= .1;
        }
        else
            value = value * 10.0 + static_cast<unsigned>(c - L'0');
        if (value > (unit == MathDimensionUnit::Point ? 64.0 : 4.0))
            return std::nullopt;
    }
    if (! digit)
        return std::nullopt;
    return MathDimension{static_cast<float>(value), unit};
}
} // namespace DxUi::RichText::Detail
