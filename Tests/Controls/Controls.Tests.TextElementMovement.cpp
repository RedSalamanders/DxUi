#include "Controls.Tests.TextElementMovement.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.UnicodeGraphemeCorpus.h"
#include <DxUi/AccessibilityTextUnits.h>
#include <climits>

namespace
{
void TestUnicode17BulkCharacterMovementOracle()
{
    // The independent standard supplies every boundary; do not derive expected
    // endpoints by asking the same production walker used by the command.
    for (const auto& test : UnicodeGraphemeCorpus::kCases)
    {
        const auto boundaries = test.boundaries;
        for (size_t position = 0; position <= test.text.size() + 1u; ++position)
        {
            const auto clamped = std::min(position, test.text.size());
            const auto right   = std::lower_bound(boundaries.begin(), boundaries.end(), clamped);
            auto ordinal       = static_cast<std::int64_t>(right - boundaries.begin());
            if (right != boundaries.begin() && (right == boundaries.end() || (*right != clamped && clamped - *std::prev(right) < *right - clamped)))
                --ordinal;
            for (const auto count : {INT_MIN, -3, -1, 0, 1, 3, INT_MAX})
            {
                const auto target = std::clamp(ordinal + static_cast<std::int64_t>(count), std::int64_t{0}, static_cast<std::int64_t>(boundaries.size() - 1u));
                const auto expectedPosition = count ? static_cast<size_t>(boundaries[static_cast<size_t>(target)]) : clamped;
                const auto expectedMoved    = count ? static_cast<int>(target - ordinal) : 0;
                const auto result           = DxUi::MoveAccessibilityTextPositionByUnit(test.text, position, TextUnit_Character, count);
                Require(result.position == expectedPosition && result.moved == expectedMoved,
                        "bulk character movement preserves original Unicode boundaries, nearest snapping, zero counts and actual signed movement");
            }
        }
    }
}

void TestTextElementBulkMovementFixedTrials()
{
    std::wstring source;
    constexpr size_t repetitions = 1024u;
    for (size_t index = 0; index < repetitions; ++index)
        source += L"a\u0301\u1100\u1161\u11a8x";
    // Exactly three graphemes per six-unit pattern, independent of the walker.
    constexpr int expectedElements = static_cast<int>(repetitions * 3u);
    for (size_t trial = 0; trial < 5u; ++trial)
    {
        const auto started  = std::chrono::steady_clock::now();
        const auto forward  = DxUi::MoveAccessibilityTextPositionByUnit(source, 0, TextUnit_Character, INT_MAX);
        const auto middle   = std::chrono::steady_clock::now();
        const auto backward = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
        const auto ended    = std::chrono::steady_clock::now();
        Require(forward.position == source.size() && forward.moved == expectedElements && backward.position == 0 && backward.moved == -expectedElements,
                "fixed bulk traversal trials reach independent endpoints and exact signed counts");
        std::cout << "UIA bulk trial=" << trial << " sourceUnits=" << source.size() << " elements=" << expectedElements
                  << " forwardMs=" << std::chrono::duration<double, std::milli>(middle - started).count()
                  << " backwardMs=" << std::chrono::duration<double, std::milli>(ended - middle).count() << '\n';
    }
}

void TextFieldInteriorDeletion(bool multiline)
{
    DxUi::WindowHost host;
    // Include an interior surrogate ACP accepted by legacy APIs, in addition
    // to valid scalar ACPs inside combining, Hangul, Indic and CRLF elements.
    for (const auto& element : {std::wstring(L"\U0001f600"),
                                std::wstring(L"a\u0301"),
                                std::wstring(L"\u1100\u1161\u11a8"),
                                std::wstring(L"\u0915\u094d\u0915"),
                                std::wstring(L"\r\n")})
    {
        for (const auto key : {VK_BACK, VK_DELETE})
        {
            const auto source = L"A" + element + L"B";
            ExposedTextField field(source);
            field.SetMultiline(multiline);
            field.SetBounds(D2D1::RectF(0, 0, 480, 120));
            field.SetSelectionRange(2, 2);
            Require(field.OnKeyDown(host, static_cast<UINT>(key), 0), "legacy field handles collapsed deletion at an interior ACP");
            std::cout << "interior field multiline=" << multiline << " key=" << key << " resultUnits=" << field.GetText().size()
                      << " validUtf16=" << DxUi::RichText::IsValidUtf16(field.GetText()) << '\n';
            Require(field.GetText() == L"AB", "collapsed legacy deletion removes the containing grapheme without leaving a surrogate half or mark");
            Require(field.OnKeyDown(host, 'Z', MK_CONTROL) && field.GetText() == source, "legacy interior deletion is one reversible edit");
            DxUi::TextInputState restored;
            Require(field.ExportTextInputState(restored) && restored.caretIndex == 2u, "undo restores the original explicit ACP");
        }
    }
}
void TestTextFieldInteriorAcpDeletion()
{
    TextFieldInteriorDeletion(false);
}
void TestMultilineFieldInteriorAcpDeletion()
{
    TextFieldInteriorDeletion(true);
}
void TestComboBoxInteriorAcpDeletion()
{
    DxUi::WindowHost host;
    for (const auto& element :
         {std::wstring(L"\U0001f600"), std::wstring(L"a\u0301"), std::wstring(L"\u1100\u1161\u11a8"), std::wstring(L"\u0915\u094d\u0915")})
    {
        for (const auto key : {VK_BACK, VK_DELETE})
        {
            const auto source = L"A" + element + L"B";
            DxUi::ComboBox combo;
            combo.SetEditable(true);
            combo.SetBounds(D2D1::RectF(0, 0, 480, 32));
            combo.SetText(source);
            combo.SetEditableSelectionRange(2, 2);
            Require(combo.OnKeyDown(host, static_cast<UINT>(key), 0), "editable combo handles deletion at an explicit interior ACP");
            Require(combo.GetText() == L"AB", "editable combo removes the whole containing grapheme");
            Require(combo.OnKeyDown(host, 'Z', MK_CONTROL) && combo.GetText() == source, "combo interior deletion is one reversible edit");
        }
    }
}

void TestUnicode17CollapsedDeletionCorpus()
{
    for (const auto& test : UnicodeGraphemeCorpus::kCases)
    {
        const std::vector<size_t> index(test.boundaries.begin(), test.boundaries.end());
        for (size_t position = 0; position <= test.text.size() + 1u; ++position)
        {
            const auto caret = std::min(position, test.text.size());
            const auto right = std::lower_bound(index.begin(), index.end(), caret);
            for (const bool backwards : {false, true})
            {
                size_t first = caret, last = caret;
                if (*right != caret)
                {
                    first = *std::prev(right);
                    last  = *right;
                }
                else if (backwards && right != index.begin())
                    first = *std::prev(right);
                else if (! backwards && std::next(right) != index.end())
                    last = *std::next(right);
                for (const auto supplied : {std::span<const size_t>{}, std::span<const size_t>(index)})
                {
                    const auto actual = DxUi::GetTextElementDeletionRange(test.text, position, backwards, supplied);
                    Require(actual.first == first && actual.last == last,
                            "indexed and forward-only collapsed deletion match every original Unicode boundary, interior ACP and edge");
                }
            }
        }
    }
    for (const bool backwards : {false, true})
    {
        const auto empty = DxUi::GetTextElementDeletionRange({}, SIZE_MAX, backwards);
        Require(empty.first == 0 && empty.last == 0, "empty deletion is bounded in either direction");
    }
}

void TestTextElementBulkMovementMaximumSourceAndMalformedBounds()
{
    std::wstring source;
    constexpr int expectedElements = 32768;
    for (int index = 0; index < expectedElements; ++index)
        source += L"a\u0301";
    const auto forward  = DxUi::MoveAccessibilityTextPositionByUnit(source, 0, TextUnit_Character, INT_MAX);
    const auto backward = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
    Require(source.size() == 65536u && forward.position == source.size() && forward.moved == expectedElements && backward.position == 0 &&
                backward.moved == -expectedElements,
            "extreme signed counts are bounded by the maximum rich source rather than repeated requested steps");
    const std::wstring malformed{wchar_t(0xd800), L'\0', wchar_t(0xdc00)};
    const auto first = DxUi::MoveAccessibilityTextPositionByUnit(malformed, SIZE_MAX, TextUnit_Character, INT_MIN);
    const auto last  = DxUi::MoveAccessibilityTextPositionByUnit(malformed, 0, TextUnit_Character, INT_MAX);
    Require(first.position == 0u && first.moved == -3 && last.position == malformed.size() && last.moved == 3,
            "legacy malformed code units and out-of-range positions make bounded progress in both directions");
}
void TestTextElementSingleBackwardFixedTrials()
{
    std::wstring source;
    for (size_t index = 0u; index < 10240u; ++index)
        source += L"a\u0301\u1100\u1161\u11a8x";
    for (size_t trial = 0u; trial < 5u; ++trial)
    {
        bool valid         = true;
        const auto started = std::chrono::steady_clock::now();
        for (size_t query = 0u; query < 32u; ++query)
        {
            const auto result = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, -1);
            valid             = valid && result.position == source.size() - 1u && result.moved == -1;
        }
        const auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        Require(valid && source.size() == 61440u, "single backward commands retain the independent final ASCII boundary");
        std::cout << "UIA single-backward trial=" << trial << " sourceUnits=" << source.size() << " queries=32 elapsedMs=" << milliseconds << '\n';
    }
}
} // namespace
void RunTextElementMovementTests()
{
    DXUI_RUN_TEST(TestUnicode17BulkCharacterMovementOracle);
    DXUI_RUN_TEST(TestTextElementBulkMovementFixedTrials);
    DXUI_RUN_TEST(TestTextElementBulkMovementMaximumSourceAndMalformedBounds);
    DXUI_RUN_TEST(TestTextFieldInteriorAcpDeletion);
    DXUI_RUN_TEST(TestMultilineFieldInteriorAcpDeletion);
    DXUI_RUN_TEST(TestComboBoxInteriorAcpDeletion);
    DXUI_RUN_TEST(TestUnicode17CollapsedDeletionCorpus);
    DXUI_RUN_TEST(TestTextElementSingleBackwardFixedTrials);
}
