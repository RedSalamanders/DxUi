#include "Scope.Tests.TextElements.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.UnicodeGraphemeCorpus.h"

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
    const std::wstring longElement = L"a" + std::wstring(8192, L'\u0301');
    Require(DxUi::CountTextElements(longElement) == 1 && DxUi::StepToPreviousTextElement(longElement, longElement.size()) == 0,
            "long extender context uses a bounded state machine without recursive lookbehind");
    Require(DxUi::CountTextElements(L"a\u200db") == 2, "ZWJ does not join arbitrary non-pictographic letters");
    Require(DxUi::CountTextElements(L"\U0001f1e6\u0301\U0001f1e7") == 2, "an extender interrupts a regional-indicator run");
}
} // namespace

void RunTextElementTests()
{
    DXUI_RUN_TEST(TestUnicode17DefaultGraphemeCorpus);
    DXUI_RUN_TEST(TestTextElementBoundsMalformedAndLongContext);
}
