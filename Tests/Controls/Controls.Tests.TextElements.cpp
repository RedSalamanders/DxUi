#include "Controls.Tests.TextElements.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.UnicodeGraphemeCorpus.h"
#include "Controls/DxUi.TextElements.h"
#include <DxUi/AccessibilityTextUnits.h>
#include <random>

namespace
{
// The pre-W8.6 arbitrary-caret wrappers, kept verbatim as the differential
// oracle and as the before figure: both segment forward from offset 0.
size_t FromZeroStepToNextTextElement(std::wstring_view text, size_t caretIndex)
{
    const size_t caret = std::min(caretIndex, text.size());
    if (caret >= text.size())
        return text.size();
    if (text[caret] <= 0x7fu && (! caret || (text[caret - 1u] <= 0x7fu && ! (text[caret - 1u] == L'\r' && text[caret] == L'\n'))))
        return DxUi::FindNextTextElementBoundary(text, caret);
    for (size_t cursor = 0u; cursor < text.size();)
    {
        const size_t next = DxUi::FindNextTextElementBoundary(text, cursor);
        if (caret < next)
            return next;
        cursor = next;
    }
    return text.size();
}
size_t FromZeroSnapCaretIndexToTextElementBoundary(std::wstring_view text, size_t caretIndex)
{
    const size_t caret = std::min(caretIndex, text.size());
    if (caret == 0u || caret == text.size())
        return caret;
    for (size_t cursor = 0u; cursor < text.size();)
    {
        const size_t next = DxUi::FindNextTextElementBoundary(text, cursor);
        if (caret == cursor || caret == next)
            return caret;
        if (caret < next)
            return caret - cursor < next - caret ? cursor : next;
        cursor = next;
    }
    return text.size();
}
std::vector<size_t> FromZeroBoundaries(std::wstring_view text)
{
    std::vector<size_t> boundaries(1u, 0u);
    while (boundaries.back() < text.size())
        boundaries.push_back(DxUi::FindNextTextElementBoundary(text, boundaries.back()));
    return boundaries;
}
void RequireLocalSteppingMatchesFromZero(std::wstring_view text, const char* source, size_t ordinal)
{
    const auto boundaries = FromZeroBoundaries(text);
    for (size_t offset = 0u; offset <= text.size() + 1u; ++offset)
    {
        const auto previous         = std::lower_bound(boundaries.begin(), boundaries.end(), std::min(offset, text.size()));
        const auto expectedPrevious = previous == boundaries.begin() ? 0u : *std::prev(previous);
        const auto next             = DxUi::StepToNextTextElement(text, offset);
        const auto snapped          = DxUi::SnapCaretIndexToTextElementBoundary(text, offset);
        const auto stepPrevious     = DxUi::StepToPreviousTextElement(text, offset);
        const auto findPrevious     = DxUi::FindPreviousTextElementBoundary(text, offset);
        if (next != FromZeroStepToNextTextElement(text, offset) || snapped != FromZeroSnapCaretIndexToTextElementBoundary(text, offset) ||
            stepPrevious != expectedPrevious || findPrevious != expectedPrevious)
        {
            std::cerr << source << " case=" << ordinal << " offset=" << offset << " units=" << text.size() << '\n';
            Require(false, "anchored stepping equals the from-zero segmentation for next, previous and nearest snapping");
        }
    }
}
void TestTextElementAnchoredSteppingMatchesFromZero()
{
    size_t ordinal = 0u;
    for (const auto& test : UnicodeGraphemeCorpus::kCases)
        RequireLocalSteppingMatchesFromZero(test.text, "corpus", ordinal++);
    // Seeded mixes cover every pair class the anchor reasons about, including
    // lone and reversed surrogate halves that legacy callers still accept.
    const std::wstring_view tokens[] = {L"中",         L"文",         L"\U0001f600", L"\U0001f44d", L"\U0001f3fd", L"‍", L"♀",  L"️",     L"\U0001f1e6",
                                        L"\U0001f1e7", L"\U0001f1e8", L"ᄀ",         L"ᅡ",          L"ᆨ",          L"가",  L"각", L"́",     L"̈",
                                        L"ः",          L"؀",         L"क",          L"्",           L"़",           L"\r",  L"\n", L"\x01", L"a",
                                        L" ",          L"\xd800",     L"\xdc00",     L"\U0001f469", L"\U0001f4bb", L"ำ",   L"⃣",   L"1"};
    std::mt19937 random(0x5786u);
    std::uniform_int_distribution<size_t> pick(0u, std::size(tokens) - 1u);
    std::uniform_int_distribution<size_t> length(0u, 40u);
    for (size_t trial = 0u; trial < 4096u; ++trial)
    {
        std::wstring text;
        for (size_t count = length(random); count > 0u; --count)
            text += tokens[pick(random)];
        RequireLocalSteppingMatchesFromZero(text, "seeded", trial);
    }
}

// FX7-shaped fixture: exactly 65,536 UTF-16 units of CJK and emoji, built from
// a fixed token cycle so a later fixture set can reuse the same text.
std::wstring BuildCjkEmojiFieldText()
{
    const std::wstring_view tokens[] = {L"中文字",
                                        L"\U0001f600",
                                        L"日本語",
                                        L"\U0001f44d\U0001f3fd",
                                        L"漢字",
                                        L"\U0001f469‍\U0001f4bb",
                                        L"한국어",
                                        L"\U0001f468‍\U0001f469‍\U0001f467‍\U0001f466",
                                        L"測試",
                                        L"\U0001f1ef\U0001f1f5",
                                        L"表示",
                                        L"1️⃣",
                                        L"文字列",
                                        L"\U0001f1ef\U0001f1f5\U0001f1fa\U0001f1f8",
                                        L"あいう",
                                        L"❤️"};
    constexpr size_t kUnits          = 65536u;
    std::wstring text;
    text.reserve(kUnits);
    for (size_t index = 0u;; ++index)
    {
        const auto token = tokens[index % std::size(tokens)];
        if (text.size() + token.size() > kUnits)
            break;
        text += token;
    }
    text.append(kUnits - text.size(), L'中');
    return text;
}
struct VisitedUnits
{
    size_t maximum{}, total{}, steps{};
    void Add(size_t units) noexcept
    {
        maximum = std::max(maximum, units);
        total += units;
        ++steps;
    }
};
template <class Step> size_t CountVisitedUnits(Step&& step)
{
    const auto before = DxUi::testTextElementCodeUnitsVisited;
    step();
    return DxUi::testTextElementCodeUnitsVisited - before;
}
void TestTextElementSteppingVisitsBoundedUnits()
{
    constexpr size_t kStepBudget = 64u;
    const std::wstring text      = BuildCjkEmojiFieldText();
    Require(text.size() == 65536u, "the CJK and emoji field holds 64 Ki UTF-16 units");
    const auto boundaries = FromZeroBoundaries(text);

    // Every UTF-16 offset, including surrogate and cluster interiors.
    VisitedUnits next, previous, snap;
    for (size_t offset = 0u; offset <= text.size(); ++offset)
    {
        size_t result{};
        next.Add(CountVisitedUnits([&] { result = DxUi::StepToNextTextElement(text, offset); }));
        Require(result == *std::upper_bound(boundaries.begin(), boundaries.end(), std::min(offset, text.size() - 1u)) || offset == text.size(),
                "next step at every offset reaches the from-zero boundary");
        previous.Add(CountVisitedUnits([&] { result = DxUi::StepToPreviousTextElement(text, offset); }));
        snap.Add(CountVisitedUnits([&] { result = DxUi::SnapCaretIndexToTextElementBoundary(text, offset); }));
        Require(std::binary_search(boundaries.begin(), boundaries.end(), result), "snapping at every offset yields a from-zero boundary");
    }
    std::cout << "W8.6 every-offset units=" << text.size() << " elements=" << boundaries.size() - 1u << " nextMax=" << next.maximum
              << " previousMax=" << previous.maximum << " snapMax=" << snap.maximum << " nextMean=" << double(next.total) / double(next.steps)
              << " snapMean=" << double(snap.total) / double(snap.steps) << '\n';
    Require(next.maximum <= kStepBudget && previous.maximum <= kStepBudget && snap.maximum <= kStepBudget,
            "next, previous and nearest snapping visit at most 64 code units at any caret offset");

    // 200 arrow keys through a real TextField from the middle of the field.
    DxUi::WindowHost host;
    ExposedTextField field(text);
    field.SetBounds(D2D1::RectF(0, 0, 480, 32));
    const size_t middle = boundaries[boundaries.size() / 2u];
    field.SetSelectionRange(middle, middle);
    VisitedUnits arrows, fromZero;
    for (size_t key = 0u; key < 200u; ++key)
    {
        DxUi::TextInputState state;
        Require(field.ExportTextInputState(state), "field exports its caret");
        const auto caret = state.caretIndex;
        const bool right = key < 100u;
        if (right)
            fromZero.Add(CountVisitedUnits([&] { (void)FromZeroStepToNextTextElement(text, caret); }));
        arrows.Add(CountVisitedUnits([&] { Require(field.OnKeyDown(host, static_cast<UINT>(right ? VK_RIGHT : VK_LEFT), 0), "field handles the arrow key"); }));
        Require(field.ExportTextInputState(state), "field exports its moved caret");
        const auto expected =
            right ? *std::upper_bound(boundaries.begin(), boundaries.end(), caret) : *std::prev(std::lower_bound(boundaries.begin(), boundaries.end(), caret));
        Require(state.caretIndex == expected, "each arrow key moves exactly one from-zero element");
    }
    std::cout << "W8.6 TextField arrows=" << arrows.steps << " startCaret=" << middle << " maxUnitsPerKey=" << arrows.maximum
              << " meanUnitsPerKey=" << double(arrows.total) / double(arrows.steps) << " fromZeroRightMax=" << fromZero.maximum
              << " fromZeroRightMean=" << double(fromZero.total) / double(fromZero.steps) << '\n';
    Require(arrows.maximum <= kStepBudget, "each TextField arrow key visits at most 64 code units");

    // UI Automation character walk: expansion and single moves at every element.
    VisitedUnits expand, moveForward, moveBackward, fromZeroExpand;
    for (size_t index = 0u; index + 1u < boundaries.size(); ++index)
    {
        const auto start  = boundaries[index];
        const auto inside = boundaries[index + 1u] - 1u;
        DxUi::AccessibilityTextUnitSpan span{};
        expand.Add(CountVisitedUnits([&] { span = DxUi::GetEnclosingAccessibilityTextUnitSpan(text, inside, TextUnit_Character); }));
        Require(span.start == start && span.end == boundaries[index + 1u], "character expansion encloses the from-zero element");
        DxUi::TextRangeUnitMoveResult moved{};
        moveForward.Add(CountVisitedUnits([&] { moved = DxUi::MoveAccessibilityTextPositionByUnit(text, start, TextUnit_Character, 1); }));
        Require(moved.position == boundaries[index + 1u] && moved.moved == 1, "character Move(+1) reaches the next element");
        moveBackward.Add(CountVisitedUnits([&] { moved = DxUi::MoveAccessibilityTextPositionByUnit(text, boundaries[index + 1u], TextUnit_Character, -1); }));
        Require(moved.position == start && moved.moved == -1, "character Move(-1) reaches the previous element");
        if (index % 1024u == 0u)
            fromZeroExpand.Add(CountVisitedUnits([&] { (void)FromZeroSnapCaretIndexToTextElementBoundary(text, inside); }));
    }
    std::cout << "W8.6 UIA character walk elements=" << expand.steps << " expandMax=" << expand.maximum << " moveForwardMax=" << moveForward.maximum
              << " moveBackwardMax=" << moveBackward.maximum << " expandMean=" << double(expand.total) / double(expand.steps)
              << " fromZeroSnapSampledMax=" << fromZeroExpand.maximum
              << " fromZeroSnapSampledMean=" << double(fromZeroExpand.total) / double(fromZeroExpand.steps) << '\n';
    Require(expand.maximum <= kStepBudget && moveForward.maximum <= kStepBudget && moveBackward.maximum <= kStepBudget,
            "each UI Automation character expansion and single move visits at most 64 code units");
}
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
void TestWordSelectionClassesSupplementaryScalars()
{
    const auto select = [](std::wstring_view text, size_t hit)
    {
        size_t caret = 0u;
        std::optional<size_t> anchor;
        DxUi::SelectSingleLineWordAt(text, hit, caret, anchor);
        return std::pair{anchor.value_or(caret), caret};
    };
    // U+20000 and U+20001 are CJK Extension B letters encoded as surrogate pairs.
    const std::wstring_view bracketed = L"(\U00020000\U00020001)";
    for (size_t hit = 1u; hit <= 4u; ++hit)
        Require(select(bracketed, hit) == std::pair<size_t, size_t>{1u, 5u}, "double-click inside U+20000 U+20001 selects both letters and no punctuation");
    const std::wstring_view joined = L"ab\U00020000\U00020001cd";
    for (size_t hit = 0u; hit < joined.size(); ++hit)
        Require(select(joined, hit) == std::pair<size_t, size_t>{0u, joined.size()}, "supplementary letters join adjacent ASCII letters in one word");
    const std::wstring_view spaced = L"x \U00020000\U00020001 y";
    Require(select(spaced, 3u) == std::pair<size_t, size_t>{2u, 6u}, "a trail-surrogate hit selects the complete supplementary word");
    Require(select(spaced, 1u) == std::pair<size_t, size_t>{1u, 2u}, "whitespace selection stops before a supplementary letter");
    const std::wstring_view emoji = L"a\U0001f600\U0001f600b";
    Require(select(emoji, 2u) == std::pair<size_t, size_t>{1u, 5u}, "supplementary symbols remain one punctuation run without splitting a pair");
}
} // namespace

void RunTextElementTests()
{
    DXUI_RUN_TEST(TestWordSelectionClassesSupplementaryScalars);
    DXUI_RUN_TEST(TestUnicode17DefaultGraphemeCorpus);
    DXUI_RUN_TEST(TestTextElementBoundsMalformedAndLongContext);
    DXUI_RUN_TEST(TestReverseCjkTraversalAndContext);
    DXUI_RUN_TEST(TestTextElementAnchoredSteppingMatchesFromZero);
    DXUI_RUN_TEST(TestTextElementSteppingVisitsBoundedUnits);
}
