#include "Scope.Tests.TextElementMovement.h"
#include "Controls.Tests.DxUiTestHelpers.h"
#include "Scope.Tests.UnicodeGraphemeCorpus.h"
#include <DxUi/AccessibilityTextUnits.h>

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
            const auto right = std::lower_bound(boundaries.begin(), boundaries.end(), clamped);
            auto ordinal = static_cast<std::int64_t>(right - boundaries.begin());
            if (right != boundaries.begin() && (right == boundaries.end() || (*right != clamped && clamped - *std::prev(right) < *right - clamped)))
                --ordinal;
            for (const auto count : {INT_MIN, -3, -1, 0, 1, 3, INT_MAX})
            {
                const auto target = std::clamp(ordinal + static_cast<std::int64_t>(count), std::int64_t{0}, static_cast<std::int64_t>(boundaries.size() - 1u));
                const auto expectedPosition = count ? static_cast<size_t>(boundaries[static_cast<size_t>(target)]) : clamped;
                const auto expectedMoved = count ? static_cast<int>(target - ordinal) : 0;
                const auto result = DxUi::MoveAccessibilityTextPositionByUnit(test.text, position, TextUnit_Character, count);
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
        const auto started = std::chrono::steady_clock::now();
        const auto forward = DxUi::MoveAccessibilityTextPositionByUnit(source, 0, TextUnit_Character, INT_MAX);
        const auto middle = std::chrono::steady_clock::now();
        const auto backward = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
        const auto ended = std::chrono::steady_clock::now();
        Require(forward.position == source.size() && forward.moved == expectedElements && backward.position == 0 && backward.moved == -expectedElements,
                "fixed bulk traversal trials reach independent endpoints and exact signed counts");
        std::cout << "UIA bulk trial=" << trial << " sourceUnits=" << source.size() << " elements=" << expectedElements
                  << " forwardMs=" << std::chrono::duration<double, std::milli>(middle - started).count()
                  << " backwardMs=" << std::chrono::duration<double, std::milli>(ended - middle).count() << '\n';
    }
}

void TestTextElementBulkMovementMaximumSourceAndMalformedBounds()
{
    std::wstring source;
    constexpr int expectedElements = 32768;
    for (int index = 0; index < expectedElements; ++index)
        source += L"a\u0301";
    const auto forward = DxUi::MoveAccessibilityTextPositionByUnit(source, 0, TextUnit_Character, INT_MAX);
    const auto backward = DxUi::MoveAccessibilityTextPositionByUnit(source, source.size(), TextUnit_Character, INT_MIN);
    Require(source.size() == 65536u && forward.position == source.size() && forward.moved == expectedElements && backward.position == 0 && backward.moved == -expectedElements,
            "extreme signed counts are bounded by the maximum rich source rather than repeated requested steps");
    const std::wstring malformed{wchar_t(0xd800), L'\0', wchar_t(0xdc00)};
    const auto first = DxUi::MoveAccessibilityTextPositionByUnit(malformed, SIZE_MAX, TextUnit_Character, INT_MIN);
    const auto last = DxUi::MoveAccessibilityTextPositionByUnit(malformed, 0, TextUnit_Character, INT_MAX);
    Require(first.position == 0u && first.moved == -3 && last.position == malformed.size() && last.moved == 3,
            "legacy malformed code units and out-of-range positions make bounded progress in both directions");
}
} // namespace
void RunTextElementMovementTests()
{
    DXUI_RUN_TEST(TestUnicode17BulkCharacterMovementOracle);
    DXUI_RUN_TEST(TestTextElementBulkMovementFixedTrials);
    DXUI_RUN_TEST(TestTextElementBulkMovementMaximumSourceAndMalformedBounds);
}
