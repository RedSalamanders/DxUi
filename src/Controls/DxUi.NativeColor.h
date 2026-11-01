#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace DxUi::RichText::Detail
{
// Opaque CSS named colors; values mirror the Windows SDK D2D color constants,
// including CSS grey aliases and rebeccapurple. No locale-dependent folding.
struct NativeNamedColor { std::wstring_view name; std::uint32_t rgb; };
inline constexpr NativeNamedColor NativeNamedColors[] = {
    {L"aliceblue", 0xF0F8FFu},
    {L"antiquewhite", 0xFAEBD7u},
    {L"aqua", 0x00FFFFu},
    {L"aquamarine", 0x7FFFD4u},
    {L"azure", 0xF0FFFFu},
    {L"beige", 0xF5F5DCu},
    {L"bisque", 0xFFE4C4u},
    {L"black", 0x000000u},
    {L"blanchedalmond", 0xFFEBCDu},
    {L"blue", 0x0000FFu},
    {L"blueviolet", 0x8A2BE2u},
    {L"brown", 0xA52A2Au},
    {L"burlywood", 0xDEB887u},
    {L"cadetblue", 0x5F9EA0u},
    {L"chartreuse", 0x7FFF00u},
    {L"chocolate", 0xD2691Eu},
    {L"coral", 0xFF7F50u},
    {L"cornflowerblue", 0x6495EDu},
    {L"cornsilk", 0xFFF8DCu},
    {L"crimson", 0xDC143Cu},
    {L"cyan", 0x00FFFFu},
    {L"darkblue", 0x00008Bu},
    {L"darkcyan", 0x008B8Bu},
    {L"darkgoldenrod", 0xB8860Bu},
    {L"darkgray", 0xA9A9A9u},
    {L"darkgreen", 0x006400u},
    {L"darkgrey", 0xA9A9A9u},
    {L"darkkhaki", 0xBDB76Bu},
    {L"darkmagenta", 0x8B008Bu},
    {L"darkolivegreen", 0x556B2Fu},
    {L"darkorange", 0xFF8C00u},
    {L"darkorchid", 0x9932CCu},
    {L"darkred", 0x8B0000u},
    {L"darksalmon", 0xE9967Au},
    {L"darkseagreen", 0x8FBC8Fu},
    {L"darkslateblue", 0x483D8Bu},
    {L"darkslategray", 0x2F4F4Fu},
    {L"darkslategrey", 0x2F4F4Fu},
    {L"darkturquoise", 0x00CED1u},
    {L"darkviolet", 0x9400D3u},
    {L"deeppink", 0xFF1493u},
    {L"deepskyblue", 0x00BFFFu},
    {L"dimgray", 0x696969u},
    {L"dimgrey", 0x696969u},
    {L"dodgerblue", 0x1E90FFu},
    {L"firebrick", 0xB22222u},
    {L"floralwhite", 0xFFFAF0u},
    {L"forestgreen", 0x228B22u},
    {L"fuchsia", 0xFF00FFu},
    {L"gainsboro", 0xDCDCDCu},
    {L"ghostwhite", 0xF8F8FFu},
    {L"gold", 0xFFD700u},
    {L"goldenrod", 0xDAA520u},
    {L"gray", 0x808080u},
    {L"green", 0x008000u},
    {L"greenyellow", 0xADFF2Fu},
    {L"grey", 0x808080u},
    {L"honeydew", 0xF0FFF0u},
    {L"hotpink", 0xFF69B4u},
    {L"indianred", 0xCD5C5Cu},
    {L"indigo", 0x4B0082u},
    {L"ivory", 0xFFFFF0u},
    {L"khaki", 0xF0E68Cu},
    {L"lavender", 0xE6E6FAu},
    {L"lavenderblush", 0xFFF0F5u},
    {L"lawngreen", 0x7CFC00u},
    {L"lemonchiffon", 0xFFFACDu},
    {L"lightblue", 0xADD8E6u},
    {L"lightcoral", 0xF08080u},
    {L"lightcyan", 0xE0FFFFu},
    {L"lightgoldenrodyellow", 0xFAFAD2u},
    {L"lightgray", 0xD3D3D3u},
    {L"lightgreen", 0x90EE90u},
    {L"lightgrey", 0xD3D3D3u},
    {L"lightpink", 0xFFB6C1u},
    {L"lightsalmon", 0xFFA07Au},
    {L"lightseagreen", 0x20B2AAu},
    {L"lightskyblue", 0x87CEFAu},
    {L"lightslategray", 0x778899u},
    {L"lightslategrey", 0x778899u},
    {L"lightsteelblue", 0xB0C4DEu},
    {L"lightyellow", 0xFFFFE0u},
    {L"lime", 0x00FF00u},
    {L"limegreen", 0x32CD32u},
    {L"linen", 0xFAF0E6u},
    {L"magenta", 0xFF00FFu},
    {L"maroon", 0x800000u},
    {L"mediumaquamarine", 0x66CDAAu},
    {L"mediumblue", 0x0000CDu},
    {L"mediumorchid", 0xBA55D3u},
    {L"mediumpurple", 0x9370DBu},
    {L"mediumseagreen", 0x3CB371u},
    {L"mediumslateblue", 0x7B68EEu},
    {L"mediumspringgreen", 0x00FA9Au},
    {L"mediumturquoise", 0x48D1CCu},
    {L"mediumvioletred", 0xC71585u},
    {L"midnightblue", 0x191970u},
    {L"mintcream", 0xF5FFFAu},
    {L"mistyrose", 0xFFE4E1u},
    {L"moccasin", 0xFFE4B5u},
    {L"navajowhite", 0xFFDEADu},
    {L"navy", 0x000080u},
    {L"oldlace", 0xFDF5E6u},
    {L"olive", 0x808000u},
    {L"olivedrab", 0x6B8E23u},
    {L"orange", 0xFFA500u},
    {L"orangered", 0xFF4500u},
    {L"orchid", 0xDA70D6u},
    {L"palegoldenrod", 0xEEE8AAu},
    {L"palegreen", 0x98FB98u},
    {L"paleturquoise", 0xAFEEEEu},
    {L"palevioletred", 0xDB7093u},
    {L"papayawhip", 0xFFEFD5u},
    {L"peachpuff", 0xFFDAB9u},
    {L"peru", 0xCD853Fu},
    {L"pink", 0xFFC0CBu},
    {L"plum", 0xDDA0DDu},
    {L"powderblue", 0xB0E0E6u},
    {L"purple", 0x800080u},
    {L"rebeccapurple", 0x663399u},
    {L"red", 0xFF0000u},
    {L"rosybrown", 0xBC8F8Fu},
    {L"royalblue", 0x4169E1u},
    {L"saddlebrown", 0x8B4513u},
    {L"salmon", 0xFA8072u},
    {L"sandybrown", 0xF4A460u},
    {L"seagreen", 0x2E8B57u},
    {L"seashell", 0xFFF5EEu},
    {L"sienna", 0xA0522Du},
    {L"silver", 0xC0C0C0u},
    {L"skyblue", 0x87CEEBu},
    {L"slateblue", 0x6A5ACDu},
    {L"slategray", 0x708090u},
    {L"slategrey", 0x708090u},
    {L"snow", 0xFFFAFAu},
    {L"springgreen", 0x00FF7Fu},
    {L"steelblue", 0x4682B4u},
    {L"tan", 0xD2B48Cu},
    {L"teal", 0x008080u},
    {L"thistle", 0xD8BFD8u},
    {L"tomato", 0xFF6347u},
    {L"turquoise", 0x40E0D0u},
    {L"violet", 0xEE82EEu},
    {L"wheat", 0xF5DEB3u},
    {L"white", 0xFFFFFFu},
    {L"whitesmoke", 0xF5F5F5u},
    {L"yellow", 0xFFFF00u},
    {L"yellowgreen", 0x9ACD32u},
};
inline std::optional<std::uint32_t> ParseNativeColor(std::wstring_view text) noexcept
{
    if (text.empty() || text.size() > 32u)
        return std::nullopt;
    if (text.front() != L'#')
        for (const auto& candidate : NativeNamedColors)
        {
            if (candidate.name.size() != text.size())
                continue;
            bool equal = true;
            for (std::size_t i = 0; i < text.size(); ++i)
            {
                const auto unit = text[i] >= L'A' && text[i] <= L'Z' ? text[i] + (L'a' - L'A') : text[i];
                if (unit != candidate.name[i]) { equal = false; break; }
            }
            if (equal)
                return candidate.rgb;
        }
    if (text.front() == L'#')
        text.remove_prefix(1u);
    if (text.size() != 3u && text.size() != 6u)
        return std::nullopt;
    std::uint32_t value{};
    for (const auto unit : text)
    {
        const auto digit = unit >= L'0' && unit <= L'9' ? unit - L'0'
                         : unit >= L'a' && unit <= L'f' ? unit - L'a' + 10
                         : unit >= L'A' && unit <= L'F' ? unit - L'A' + 10 : -1;
        if (digit < 0)
            return std::nullopt;
        value = (value << 4u) | static_cast<std::uint32_t>(digit);
    }
    if (text.size() == 3u)
        value = ((value & 0xf00u) << 12u) | ((value & 0xf00u) << 8u) | ((value & 0x0f0u) << 8u) |
                ((value & 0x0f0u) << 4u) | ((value & 0x00fu) << 4u) | (value & 0x00fu);
    return value;
}
inline std::wstring EncodeNativeColor(std::uint32_t rgb)
{
    constexpr std::wstring_view digits = L"0123456789abcdef";
    std::wstring result(6u, L'0');
    for (std::size_t i = 0u; i < 6u; ++i)
    {
        result[5u - i] = digits[rgb & 15u];
        rgb >>= 4u;
    }
    return std::wstring(result);
}
} // namespace DxUi::RichText::Detail
