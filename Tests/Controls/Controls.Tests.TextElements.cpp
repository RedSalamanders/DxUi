#include "Controls.Tests.TextElements.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.UnicodeGraphemeCorpus.h"
#include "Controls/DxUi.TextElements.h"

namespace
{
void TestUnicode17DefaultGraphemeCorpus()
{
    std::size_t ordinal = 0;
    for (const auto& test : UnicodeGraphemeCorpus::kCases)
    {
        const auto text       = test.text;
        const auto boundaries = test.boundaries;
        Require(! boundaries.empty() && boundaries.front() == 0 && boundaries.back() == text.size(), "complete original Unicode vector boundaries");
        for (std::size_t offset = 0; offset <= text.size(); ++offset)
        {
            const auto next             = std::upper_bound(boundaries.begin(), boundaries.end(), offset);
            const auto previous         = std::lower_bound(boundaries.begin(), boundaries.end(), offset);
            const auto expectedNext     = next == boundaries.end() ? text.size() : *next;
            const auto expectedPrevious = previous == boundaries.begin() ? 0u : *std::prev(previous);
            const auto actualNext       = DxUi::StepToNextTextElement(text, offset);
            const auto actualPrevious   = DxUi::StepToPreviousTextElement(text, offset);
            if (actualNext != expectedNext || actualPrevious != expectedPrevious)
            {
                std::cerr << "Unicode17 vector=" << ordinal << " offset=" << offset << " next=" << actualNext << '/' << expectedNext
                          << " previous=" << actualPrevious << '/' << expectedPrevious << '\n';
                Require(false, "shared character navigation matches the full independent Unicode 17 corpus at every UTF-16 offset");
            }
            const auto snapped = DxUi::SnapCaretIndexToTextElementBoundary(text, offset);
            Require(std::binary_search(boundaries.begin(), boundaries.end(), snapped), "caret snapping always yields an original grapheme boundary");
            const auto right = std::lower_bound(boundaries.begin(), boundaries.end(), offset);
            const auto left  = right == boundaries.begin() ? 0u : *std::prev(right);
            const auto expectedSnap =
                right != boundaries.end() && *right == offset ? offset : (right == boundaries.end() || offset - left < *right - offset ? left : *right);
            Require(snapped == expectedSnap, "caret snapping chooses the nearest boundary and keeps the existing forward tie policy");
        }
        Require(DxUi::CountTextElements(text) + 1u == boundaries.size(), "shared element count matches each independent Unicode vector");
        ++ordinal;
    }
    Require(ordinal == 766u, "every pinned Unicode 17 vector runs without exclusions");
}
void TestTextElementBoundsMalformedAndLongContext()
{
    for (const auto& text : {std::wstring{}, std::wstring(1, wchar_t(0xd800)), std::wstring(1, wchar_t(0xdc00)), std::wstring(L"x\0y", 3)})
    {
        Require(DxUi::StepToPreviousTextElement(text, 0) == 0 && DxUi::StepToNextTextElement(text, text.size() + 8) == text.size(),
                "empty, malformed and out-of-range legacy input is bounded");
        size_t cursor = 0;
        while (cursor < text.size())
        {
            const auto next = DxUi::StepToNextTextElement(text, cursor);
            Require(next > cursor && next <= text.size(), "every legacy text element makes bounded progress");
            cursor = next;
        }
    }
    const std::wstring longAscii(100000u, L'a');
    const auto middle = longAscii.size() / 2u;
    Require(DxUi::StepToPreviousTextElement(longAscii, middle) == middle - 1u && DxUi::StepToNextTextElement(longAscii, middle) == middle + 1u,
            "long ASCII movement preserves the adjacent boundaries");
    const auto backward = DxUi::GetTextElementDeletionRange(longAscii, middle, true);
    const auto forward  = DxUi::GetTextElementDeletionRange(longAscii, middle, false);
    Require(backward.first == middle - 1u && backward.last == middle && forward.first == middle && forward.last == middle + 1u,
            "long ASCII deletion uses the same independent neighboring boundaries");
    const std::wstring longElement = L"a" + std::wstring(8192, L'\u0301');
    Require(DxUi::CountTextElements(longElement) == 1 && DxUi::StepToPreviousTextElement(longElement, longElement.size()) == 0,
            "long extender context uses a bounded state machine without recursive lookbehind");
    Require(DxUi::CountTextElements(L"a\u200db") == 2, "ZWJ does not join arbitrary non-pictographic letters");
    Require(DxUi::CountTextElements(L"\U0001f1e6\u0301\U0001f1e7") == 2, "an extender interrupts a regional-indicator run");
}
void TestReverseCjkTraversalAndContext()
{
    constexpr size_t length = 20000u;
    const std::wstring text(length, L'\u4e2d');
#if DXUI_ENABLE_DIAGNOSTICS
    const auto before = DxUi::testTextElementPropertyLookups;
#endif
    for (size_t caret = text.size(); caret > 0u; --caret)
    {
        Require(DxUi::StepToPreviousTextElement(text, caret) == caret - 1u, "reverse CJK movement visits each independent element");
        const auto deletion = DxUi::GetTextElementDeletionRange(text, caret, true);
        Require(deletion.first == caret - 1u && deletion.last == caret, "CJK backspace selects exactly the preceding element");
    }
#if DXUI_ENABLE_DIAGNOSTICS
    Require(DxUi::testTextElementPropertyLookups - before <= 12u * length, "reverse CJK traversal performs linear Unicode property work");
#endif
    // Independent forward boundaries are the oracle at every UTF-16 ACP,
    // including RI parity, Indic linkers, ZWJ sequences and surrogate interiors.
    const std::wstring mixed = L"\u4e2d\U0001f1e6\U0001f1e7\U0001f1e8\u0915\u094d\u0915\U0001f469\u200d\U0001f4bb\u4e2d";
    std::vector<size_t> boundaries(1u, 0u);
    while (boundaries.back() < mixed.size())
        boundaries.push_back(DxUi::FindNextTextElementBoundary(mixed, boundaries.back()));
    for (size_t caret = 0u; caret <= mixed.size(); ++caret)
    {
        const auto it       = std::lower_bound(boundaries.begin(), boundaries.end(), caret);
        const auto expected = it == boundaries.begin() ? 0u : *std::prev(it);
        Require(DxUi::FindPreviousTextElementBoundary(mixed, caret) == expected,
                "reverse boundary retains contextual Unicode behavior at every UTF-16 position");
    }
}
} // namespace

void RunTextElementTests()
{
    DXUI_RUN_TEST(TestUnicode17DefaultGraphemeCorpus);
    DXUI_RUN_TEST(TestTextElementBoundsMalformedAndLongContext);
    DXUI_RUN_TEST(TestReverseCjkTraversalAndContext);
}
