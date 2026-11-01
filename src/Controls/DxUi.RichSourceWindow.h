#pragma once
#include "DxUi.Internal.h"
#include "DxUi.TextElements.h"
#include <DxUi/RichDocument.h>

// A proven ASCII-to-ASCII break is independent of any preceding Unicode 17
// RI, Indic, prepend or emoji state. Long runs with no such break are expanded
// to a document edge rather than guessing a grapheme boundary.
namespace DxUi::RichText::Detail
{
inline bool StableSourceBreak(wchar_t previous, wchar_t current) noexcept
{
    return previous <= 0x7f && current <= 0x7f && ! (previous == L'\r' && current == L'\n');
}
inline size_t SourceWindowLeft(const SourceSnapshot& source, size_t limit)
{
    while (limit)
    {
        auto first = limit > 256u ? limit - 256u : 0u;
        auto last = limit;
        while (first && ! source.IsBoundary(first)) --first;
        while (last < source.Length() && ! source.IsBoundary(last)) ++last;
        const auto text = source.Slice(first, last);
        for (auto i = text.size(); i > 1u; --i)
            if (StableSourceBreak(text[i - 2u], text[i - 1u]) && first + i - 1u <= limit)
                return first + i - 1u;
        if (! first) return 0u;
        limit = first + 1u;
    }
    return 0u;
}
inline size_t SourceWindowRight(const SourceSnapshot& source, size_t limit)
{
    const auto length = source.Length();
    while (limit < length)
    {
        auto first = limit;
        auto last = (std::min)(length, limit + (std::min)(size_t{256u}, length - limit));
        while (first && ! source.IsBoundary(first)) --first;
        while (last < length && ! source.IsBoundary(last)) ++last;
        const auto text = source.Slice(first, last);
        for (size_t i = 1u; i < text.size(); ++i)
            if (StableSourceBreak(text[i - 1u], text[i]) && first + i >= limit)
                return first + i;
        if (last == length) return length;
        limit = last - 1u;
    }
    return length;
}
enum class SourceWindowOperation { Previous, Next, PreviousWord, NextWord, DeleteBackward, DeleteForward };
inline TextElementRange SourceWindowRange(const SourceSnapshot& source, size_t caret, SourceWindowOperation operation)
{
    const auto length = source.Length();
    caret = (std::min)(caret, length);
    auto first = SourceWindowLeft(source, caret > 128u ? caret - 128u : 0u);
    auto last = SourceWindowRight(source, (std::min)(length, caret + (std::min)(size_t{128u}, length - caret)));
    for (;;)
    {
        const auto text = source.Slice(first, last);
        const auto position = caret - first;
        TextElementRange range{};
        switch (operation)
        {
            case SourceWindowOperation::Previous: range = {StepToPreviousTextElement(text, position), position}; break;
            case SourceWindowOperation::Next: range = {position, StepToNextTextElement(text, position)}; break;
            case SourceWindowOperation::PreviousWord: range = {FindPreviousWordBoundary(text, position), position}; break;
            case SourceWindowOperation::NextWord: range = {position, FindNextWordBoundary(text, position)}; break;
            case SourceWindowOperation::DeleteBackward: range = GetTextElementDeletionRange(text, position, true); break;
            case SourceWindowOperation::DeleteForward: range = GetTextElementDeletionRange(text, position, false); break;
        }
        const bool previousWord = operation == SourceWindowOperation::PreviousWord;
        const bool nextWord = operation == SourceWindowOperation::NextWord;
        const bool rootDriveAmbiguous = previousWord && first && range.first == 2u && text.size() >= 2u && text[1] == L':';
        if (first && ((previousWord && range.first == 0u) || rootDriveAmbiguous))
        {
            const auto span = last - first;
            first = SourceWindowLeft(source, first > span ? first - span : 0u);
            continue;
        }
        if (last < length && nextWord && range.last == text.size())
        {
            const auto span = last - first;
            last = SourceWindowRight(source, last + (std::min)(span, length - last));
            continue;
        }
        return {first + range.first, first + range.last};
    }
}
} // namespace DxUi::RichText::Detail
