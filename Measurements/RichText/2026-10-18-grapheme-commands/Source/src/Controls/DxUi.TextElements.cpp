#include "DxUi.TextElements.h"
#include "DxUi.TextElementsData.h"
#include <algorithm>
#include <iterator>

namespace DxUi
{
bool IsUtf16LeadSurrogate(wchar_t value) noexcept
{
    return value >= 0xd800 && value <= 0xdbff;
}
bool IsUtf16TrailSurrogate(wchar_t value) noexcept
{
    return value >= 0xdc00 && value <= 0xdfff;
}

std::size_t StepToPreviousCodePoint(std::wstring_view text, std::size_t caretIndex) noexcept
{
    const auto caret = std::min(caretIndex, text.size());
    if (! caret)
        return 0u;
    if (caret < text.size() && IsUtf16TrailSurrogate(text[caret]) && IsUtf16LeadSurrogate(text[caret - 1u]))
        return caret - 1u;
    const auto previous = caret - 1u;
    return previous && IsUtf16TrailSurrogate(text[previous]) && IsUtf16LeadSurrogate(text[previous - 1u]) ? previous - 1u : previous;
}
std::size_t StepToNextCodePoint(std::wstring_view text, std::size_t caretIndex) noexcept
{
    const auto caret = std::min(caretIndex, text.size());
    if (caret >= text.size())
        return text.size();
    if (caret && IsUtf16TrailSurrogate(text[caret]) && IsUtf16LeadSurrogate(text[caret - 1u]))
        return caret + 1u;
    return caret + 1u < text.size() && IsUtf16LeadSurrogate(text[caret]) && IsUtf16TrailSurrogate(text[caret + 1u]) ? caret + 2u : caret + 1u;
}
Utf16CodePoint ReadCodePointAt(std::wstring_view text, std::size_t index) noexcept
{
    const auto begin = std::min(index, text.size());
    if (begin >= text.size())
        return {};
    const auto first = text[begin];
    if (begin + 1u < text.size() && IsUtf16LeadSurrogate(first) && IsUtf16TrailSurrogate(text[begin + 1u]))
        return {0x10000u + ((static_cast<std::uint32_t>(first) - 0xd800u) << 10u) + (static_cast<std::uint32_t>(text[begin + 1u]) - 0xdc00u), begin + 2u};
    return {static_cast<std::uint32_t>(first), begin + 1u};
}
namespace
{
// Encoding matches the pinned generator: low four bits are GCB, bit four is
// Extended_Pictographic, and bits five/six are Indic_Conjunct_Break.
enum class BreakClass : std::uint8_t
{
    Other,
    CR,
    LF,
    Control,
    Extend,
    ZWJ,
    RegionalIndicator,
    Prepend,
    SpacingMark,
    L,
    V,
    T,
    LV,
    LVT
};
enum class IndicClass : std::uint8_t
{
    None,
    Consonant,
    Extend,
    Linker
};
std::uint8_t Properties(std::uint32_t codePoint) noexcept
{
    if (codePoint >= 0xac00u && codePoint <= 0xd7a3u)
        return static_cast<std::uint8_t>((codePoint - 0xac00u) % 28u ? BreakClass::LVT : BreakClass::LV);
    const auto& ranges = UnicodeGraphemeData::kProperties;
    std::size_t first = 0u, last = std::size(ranges);
    while (first < last)
    {
        const auto middle = first + (last - first) / 2u;
        if (ranges[middle].last < codePoint)
            first = middle + 1u;
        else
            last = middle;
    }
    return static_cast<std::uint8_t>(first < std::size(ranges) && ranges[first].first <= codePoint ? ranges[first].properties : 0u);
}
bool HardBoundary(BreakClass value) noexcept
{
    return value == BreakClass::CR || value == BreakClass::LF || value == BreakClass::Control;
}
} // namespace

std::size_t FindNextTextElementBoundary(std::wstring_view text, std::size_t elementStart) noexcept
{
    auto cursor = std::min(elementStart, text.size());
    bool seen = false, pictographicExtendPrefix = false, previousZwjFromPictographic = false, oddRegionalRun = false;
    BreakClass previous = BreakClass::Other;
    unsigned indicState = 0u; // 1: consonant prefix; 2: that prefix has a linker (GB9c).
    while (cursor < text.size())
    {
        const auto point        = ReadCodePointAt(text, cursor);
        const auto properties   = Properties(point.value);
        const auto current      = static_cast<BreakClass>(properties & 15u);
        const auto indic        = static_cast<IndicClass>((properties >> 5u) & 3u);
        const bool pictographic = (properties & 16u) != 0u;
        if (seen)
        {
            bool boundary = true; // GB999; rules below keep their normative priority.
            if (previous == BreakClass::CR && current == BreakClass::LF)
                boundary = false; // GB3
            else if (HardBoundary(previous) || HardBoundary(current))
                boundary = true; // GB4/GB5 override extenders.
            else if (previous == BreakClass::L &&
                     (current == BreakClass::L || current == BreakClass::V || current == BreakClass::LV || current == BreakClass::LVT))
                boundary = false; // GB6
            else if ((previous == BreakClass::LV || previous == BreakClass::V) && (current == BreakClass::V || current == BreakClass::T))
                boundary = false; // GB7
            else if ((previous == BreakClass::LVT || previous == BreakClass::T) && current == BreakClass::T)
                boundary = false; // GB8
            else if (current == BreakClass::Extend || current == BreakClass::ZWJ || current == BreakClass::SpacingMark || previous == BreakClass::Prepend)
                boundary = false; // GB9/9a/9b
            else if (indicState == 2u && indic == IndicClass::Consonant)
                boundary = false; // GB9c
            else if (previous == BreakClass::ZWJ && previousZwjFromPictographic && pictographic)
                boundary = false; // GB11
            else if (previous == BreakClass::RegionalIndicator && current == BreakClass::RegionalIndicator && oddRegionalRun)
                boundary = false; // GB12/13
            if (boundary)
                return cursor;
        }
        previousZwjFromPictographic = current == BreakClass::ZWJ && pictographicExtendPrefix;
        pictographicExtendPrefix    = pictographic || (current == BreakClass::Extend && pictographicExtendPrefix);
        oddRegionalRun              = current == BreakClass::RegionalIndicator && ! oddRegionalRun;
        if (indic == IndicClass::Consonant)
            indicState = 1u;
        else if (indic == IndicClass::Linker && indicState)
            indicState = 2u;
        else if (indic != IndicClass::Extend && indic != IndicClass::Linker)
            indicState = 0u;
        previous = current;
        seen     = true;
        cursor   = point.end;
    }
    return text.size();
}
TextElementRange GetTextElementDeletionRange(std::wstring_view text,
                                             std::size_t position,
                                             bool backwards,
                                             std::span<const std::size_t> preparedBoundaries) noexcept
{
    const auto caret = std::min(position, text.size());
    if ((backwards && ! caret) || (! backwards && caret == text.size()))
        return {caret, caret};
    if (! preparedBoundaries.empty())
    {
        const auto next = std::lower_bound(preparedBoundaries.begin(), preparedBoundaries.end(), caret);
        // The owned index's complete endpoints and the edge checks above make
        // both neighbors valid. An interior caret encloses the same span for
        // Backspace and Delete, including an ACP between surrogate halves.
        if (*next == caret)
            return backwards ? TextElementRange{*std::prev(next), caret} : TextElementRange{caret, *std::next(next)};
        return {*std::prev(next), *next};
    }
    for (std::size_t first = 0u; first < text.size();)
    {
        const auto last = FindNextTextElementBoundary(text, first);
        if (caret < last)
            return {first, last};
        if (caret == last)
            return backwards ? TextElementRange{first, last} : TextElementRange{last, FindNextTextElementBoundary(text, last)};
        first = last;
    }
    return {caret, caret};
}
} // namespace DxUi
