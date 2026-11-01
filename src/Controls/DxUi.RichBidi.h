#pragma once
#include "DxUi.MarkdownUnicodeData.h"
#include "DxUi.TextElements.h"
#include <algorithm>
#include <dwrite.h>
#include <string_view>

namespace DxUi::RichText::Detail
{
template <std::size_t N> bool BidiRangeContains(const MarkdownUnicodeData::Range (&ranges)[N], std::uint32_t scalar) noexcept
{
    const auto* found = std::lower_bound(std::begin(ranges), std::end(ranges), scalar, [](const auto& range, auto value) { return range.last < value; });
    return found != std::end(ranges) && found->first <= scalar;
}
// Unicode first-strong outside isolates. An unmatched isolate contains the
// rest of the paragraph; neutral-only text retains the supplied host direction.
inline DWRITE_READING_DIRECTION FirstStrongDirection(std::wstring_view text, DWRITE_READING_DIRECTION fallback) noexcept
{
    std::size_t depth = 0u;
    for (std::size_t at = 0u; at < text.size();)
    {
        const auto scalar = ReadCodePointAt(text, at);
        at = scalar.end;
        if (scalar.value >= 0x2066u && scalar.value <= 0x2068u)
        {
            ++depth;
            continue;
        }
        if (scalar.value == 0x2069u)
        {
            if (depth)
                --depth;
            continue;
        }
        if (depth)
            continue;
        if (BidiRangeContains(MarkdownUnicodeData::kBidiL, scalar.value))
            return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
        if (BidiRangeContains(MarkdownUnicodeData::kBidiR, scalar.value))
            return DWRITE_READING_DIRECTION_RIGHT_TO_LEFT;
    }
    return fallback;
}
} // namespace DxUi::RichText::Detail
