#pragma once
#include "DxUi.Internal.h"
#include "DxUi.RichProjectionWindow.h"
#include "DxUi.TextElements.h"
#include <DxUi/AccessibilityTextUnits.h>
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
// Uses the existing UIA word and Unicode grapheme policies over a local
// prepared-display window. Uncertain boundaries expand to a proven ASCII
// break or document edge; unusually long words/clusters may span the source.
template <typename UnitAt> TextElementRange PreparedDisplayWordRange(size_t length, size_t caret, UnitAt&& unitAt)
{
    caret                 = (std::min)(caret, length);
    const auto stableLeft = [&](size_t at)
    {
        while (at && ! StableSourceBreak(unitAt(at - 1u), unitAt(at)))
            --at;
        return at;
    };
    const auto stableRight = [&](size_t at)
    {
        while (at < length && ! StableSourceBreak(unitAt(at - 1u), unitAt(at)))
            ++at;
        return at;
    };
    auto first = stableLeft(caret > 128u ? caret - 128u : 0u);
    auto last  = stableRight(caret + (std::min)(size_t{128u}, length - caret));
    for (;;)
    {
        std::wstring text(last - first, L'\0');
        for (size_t index = first; index < last; ++index)
            text[index - first] = unitAt(index);
        const auto word = GetEnclosingAccessibilityTextUnitSpan(text, caret - first, TextUnit_Word);
        TextElementRange range{word.start, word.end};
        if (word.start < word.end)
            range = {GetEnclosingAccessibilityTextUnitSpan(text, word.start, TextUnit_Character).start,
                     GetEnclosingAccessibilityTextUnitSpan(text, word.end - 1u, TextUnit_Character).end};
        const bool rootDriveAmbiguous = first && range.first == 2u && text.size() >= 2u && text[1u] == L':';
        const auto span               = last - first;
        if (first && (range.first == 0u || rootDriveAmbiguous))
        {
            first = stableLeft(first > span ? first - span : 0u);
            continue;
        }
        if (last < length && range.last == text.size())
        {
            last = stableRight(last + (std::min)(span, length - last));
            continue;
        }
        return {first + range.first, first + range.last};
    }
}
inline size_t SourceWindowLeft(const SourceSnapshot& source, size_t limit)
{
    while (limit)
    {
        auto first = limit > 256u ? limit - 256u : 0u;
        auto last  = limit;
        while (first && ! source.IsBoundary(first))
            --first;
        while (last < source.Length() && ! source.IsBoundary(last))
            ++last;
        const auto text = source.Slice(first, last);
        for (auto i = text.size(); i > 1u; --i)
            if (StableSourceBreak(text[i - 2u], text[i - 1u]) && first + i - 1u <= limit)
                return first + i - 1u;
        if (! first)
            return 0u;
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
        auto last  = (std::min)(length, limit + (std::min)(size_t{256u}, length - limit));
        while (first && ! source.IsBoundary(first))
            --first;
        while (last < length && ! source.IsBoundary(last))
            ++last;
        const auto text = source.Slice(first, last);
        for (size_t i = 1u; i < text.size(); ++i)
            if (StableSourceBreak(text[i - 1u], text[i]) && first + i >= limit)
                return first + i;
        if (last == length)
            return length;
        limit = last - 1u;
    }
    return length;
}
enum class SourceWindowOperation
{
    Previous,
    Next,
    PreviousWord,
    NextWord,
    DeleteBackward,
    DeleteForward,
    Home,
    End
};
inline TextElementRange SourceWindowRange(const SourceSnapshot& source, size_t caret, SourceWindowOperation operation)
{
    const auto length = source.Length();
    caret             = (std::min)(caret, length);
    auto first        = SourceWindowLeft(source, caret > 128u ? caret - 128u : 0u);
    auto last         = SourceWindowRight(source, (std::min)(length, caret + (std::min)(size_t{128u}, length - caret)));
    for (;;)
    {
        const auto text     = source.Slice(first, last);
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
            case SourceWindowOperation::Home: return {0u, 0u};
            case SourceWindowOperation::End: return {length, length};
        }
        const bool previousWord       = operation == SourceWindowOperation::PreviousWord;
        const bool nextWord           = operation == SourceWindowOperation::NextWord;
        const bool rootDriveAmbiguous = previousWord && first && range.first == 2u && text.size() >= 2u && text[1] == L':';
        if (first && ((previousWord && range.first == 0u) || rootDriveAmbiguous))
        {
            const auto span = last - first;
            first           = SourceWindowLeft(source, first > span ? first - span : 0u);
            continue;
        }
        if (last < length && nextWord && range.last == text.size())
        {
            const auto span = last - first;
            last            = SourceWindowRight(source, last + (std::min)(span, length - last));
            continue;
        }
        return {first + range.first, first + range.last};
    }
}
// Physical-line boundaries are hard grapheme boundaries and v0 Markdown's
// grammar is line-local. Include the preceding line so trailing mapping at a
// hidden heading/marker boundary remains identical to a full projection.
inline size_t SourceLineStart(const SourceSnapshot& source, size_t position)
{
    auto last = (std::min)(position, source.Length());
    while (last && ! source.IsBoundary(last))
        --last;
    while (last)
    {
        auto first = last > 256u ? last - 256u : 0u;
        while (first && ! source.IsBoundary(first))
            --first;
        const auto text = source.Slice(first, last);
        for (auto i = text.size(); i > 0u; --i)
            if (text[i - 1u] == L'\r' || text[i - 1u] == L'\n')
                return first + i;
        last = first;
    }
    return 0u;
}
inline size_t SourceLineEnd(const SourceSnapshot& source, size_t position)
{
    const auto length = source.Length();
    auto first        = (std::min)(position, length);
    while (first && ! source.IsBoundary(first))
        --first;
    while (first < length)
    {
        auto last = first + (std::min)(size_t{256u}, length - first);
        while (last < length && ! source.IsBoundary(last))
            ++last;
        const auto text = source.Slice(first, last);
        for (size_t i = 0u; i < text.size(); ++i)
            if (text[i] == L'\r' || text[i] == L'\n')
                return first + i + (text[i] == L'\r' && i + 1u < text.size() && text[i + 1u] == L'\n' ? 2u : 1u);
        first = last;
    }
    return length;
}
inline TextElementRange RichSourceWindowRange(const SourceSnapshot& source, size_t caret, SourceWindowOperation operation, bool forDeletion = false)
{
    const auto length = source.Length();
    caret             = (std::min)(caret, length);
    const auto line   = SourceLineStart(source, caret);
    auto first        = line ? SourceLineStart(source, line - 1u) : 0u;
    auto last         = SourceLineEnd(source, caret);
    if (last < length)
        last = SourceLineEnd(source, last);
    if (operation == SourceWindowOperation::Home)
        first = 0u;
    if (operation == SourceWindowOperation::End)
        last = length;
    for (;;)
    {
        const auto projection = ProjectRichSourceWindow(source.Slice(first, last));
        const auto position   = projection->ToDisplay(caret - first);
        const auto& text      = projection->text;
        TextElementRange display{};
        switch (operation)
        {
            case SourceWindowOperation::Previous: display = {StepToPreviousTextElement(text, position), position}; break;
            case SourceWindowOperation::Next: display = {position, StepToNextTextElement(text, position)}; break;
            case SourceWindowOperation::PreviousWord: display = {FindPreviousWordBoundary(text, position), position}; break;
            case SourceWindowOperation::NextWord: display = {position, FindNextWordBoundary(text, position)}; break;
            case SourceWindowOperation::DeleteBackward: display = GetTextElementDeletionRange(text, position, true); break;
            case SourceWindowOperation::DeleteForward: display = GetTextElementDeletionRange(text, position, false); break;
            case SourceWindowOperation::Home: display = {0u, 0u}; break;
            case SourceWindowOperation::End: display = {text.size(), text.size()}; break;
        }
        const bool rootDriveAmbiguous =
            operation == SourceWindowOperation::PreviousWord && first && display.first == 2u && text.size() >= 2u && text[1] == L':';
        const auto span = last - first;
        if (first && (display.first == 0u || rootDriveAmbiguous))
        {
            first = SourceLineStart(source, first > span ? first - span : 0u);
            continue;
        }
        if (last < length && display.last == text.size())
        {
            last = SourceLineEnd(source, last + (std::min)(span, length - last));
            continue;
        }
        if (forDeletion && display.first == display.last)
            return {caret, caret};
        return {first + projection->ToSource(display.first), first + projection->ToSource(display.last, true)};
    }
}
} // namespace DxUi::RichText::Detail
