#pragma once
#include "DxUi.MarkdownUnicodeData.h"
#include "DxUi.TextElements.h"
#include <DxUi/MarkdownSyntax.h>
#include <algorithm>
#include <iterator>

namespace DxUi::RichText::Detail
{
inline bool ValidFootnoteLabel(std::wstring_view text) noexcept
{
    bool nonblank{}, escaped{};
    std::size_t scalars{};
    for (std::size_t i = 0u; i < text.size();)
    {
        const auto unit = text[i++];
        if (++scalars > 999u || unit == L'\r' || unit == L'\n')
            return false;
        if (! escaped && (unit == L'[' || unit == L']'))
            return false;
        nonblank = nonblank || (unit != L' ' && unit != L'\t');
        if (IsUtf16LeadSurrogate(unit))
        {
            if (i == text.size() || ! IsUtf16TrailSurrogate(text[i++]))
                return false;
        }
        else if (IsUtf16TrailSurrogate(unit))
            return false;
        if (escaped)
            escaped = false;
        else
            escaped = unit == L'\\' && i < text.size() && (text[i] == L'[' || text[i] == L']' || text[i] == L'\\');
    }
    return nonblank;
}
inline void AppendLabelScalar(std::wstring& result, std::uint32_t scalar)
{
    if (scalar < 0x10000u)
        result.push_back(static_cast<wchar_t>(scalar));
    else
    {
        scalar -= 0x10000u;
        result.push_back(static_cast<wchar_t>(0xd800u + (scalar >> 10u)));
        result.push_back(static_cast<wchar_t>(0xdc00u + (scalar & 1023u)));
    }
}
// Shared identity for ordinary references and the named footnote profile. No
// NFC/NFKC, entity decoding or source rewriting is performed.
inline std::wstring NormalizeMarkdownLabel(std::wstring_view text, MarkdownProfile profile)
{
    std::wstring result(0u, L' ');
    bool space{};
    for (std::size_t i = 0u; i < text.size(); i = StepToNextCodePoint(text, i))
    {
        auto scalar = static_cast<std::uint32_t>(text[i]);
        if (IsUtf16LeadSurrogate(text[i]) && i + 1u < text.size())
            scalar = 0x10000u + ((scalar - 0xd800u) << 10u) + static_cast<std::uint32_t>(text[i + 1u]) - 0xdc00u;
        if (! scalar)
            scalar = 0xfffdu;
        const bool whitespace = scalar == L' ' || scalar == L'\t' || scalar == L'\n' || scalar == L'\r' ||
                                (profile == MarkdownProfile::Gfm029 && (scalar == L'\v' || scalar == L'\f'));
        if (whitespace)
        {
            space = ! result.empty();
            continue;
        }
        if (space)
            result.push_back(L' ');
        space = false;
        const auto* entry =
            std::lower_bound(std::begin(MarkdownUnicodeData::kFold), std::end(MarkdownUnicodeData::kFold), scalar, [](const auto& candidate, auto wanted) {
            return candidate.scalar < wanted;
        });
        if (entry != std::end(MarkdownUnicodeData::kFold) && entry->scalar == scalar)
            for (unsigned j = 0u; j < entry->count; ++j)
                AppendLabelScalar(result, entry->mapped[j]);
        else
            AppendLabelScalar(result, scalar);
    }
    // Guaranteed fallible copy rather than an allocating noexcept string move.
    return std::wstring(result);
}
} // namespace DxUi::RichText::Detail
