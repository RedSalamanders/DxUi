#pragma once
#include <DxUi/MarkdownSyntax.h>

namespace DxUi::RichText::Detail
{
// The block parser and source commands share the exact escape/edge-pipe rules.
// Raw slots retain cell whitespace so structural edits need not serialize rows.
inline std::vector<MarkdownRange> MarkdownTableCells(std::wstring_view source, MarkdownRange line, bool trimCells = true)
{
    const auto space = [](wchar_t c) noexcept { return c == L' ' || c == L'\t'; };
    while (line.last > line.first && (source[line.last - 1u] == L'\n' || source[line.last - 1u] == L'\r'))
        --line.last;
    while (line.first < line.last && space(source[line.first]))
        ++line.first;
    while (line.last > line.first && space(source[line.last - 1u]))
        --line.last;
    if (line.first < line.last && source[line.first] == L'|')
        ++line.first;
    if (line.last > line.first && source[line.last - 1u] == L'|')
    {
        auto back = line.last - 1u;
        while (back > line.first && source[back - 1u] == L'\\')
            --back;
        if ((line.last - 1u - back) % 2u == 0u)
            --line.last;
    }
    std::vector<MarkdownRange> cells(0u);
    auto first = line.first;
    for (auto i = first; i <= line.last; ++i)
    {
        if (i < line.last && source[i] == L'\\' && i + 1u < line.last)
        {
            ++i;
            continue;
        }
        if (i != line.last && source[i] != L'|')
            continue;
        auto begin = first, end = i;
        if (trimCells)
        {
            while (begin < end && space(source[begin]))
                ++begin;
            while (end > begin && space(source[end - 1u]))
                --end;
        }
        cells.push_back({begin, end});
        first = i + 1u;
    }
    return cells;
}
} // namespace DxUi::RichText::Detail
