#include "DxUi.Internal.h"
#include "DxUi.TextEditHistory.h"
#include <DxUi/Failure.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <limits>
#include <typeinfo>

#include "Support/Diagnostics.h"

namespace DxUi
{
namespace
{
constexpr float kMultilineLayoutHeightDip = 32768.0f;

[[nodiscard]] std::wstring NormalizeMultilineLineEndings(std::wstring_view text, bool multiline);

struct ConcealedMaskBucket final
{
    size_t start       = 0u;
    size_t end         = 0u;
    size_t displayBase = 0u;
    size_t displaySpan = 1u;
};

[[nodiscard]] ConcealedMaskBucket GetConcealedMaskBucket(size_t exactCount) noexcept
{
    if (exactCount == 0u)
    {
        return {};
    }
    if (exactCount <= 4u)
    {
        return {.start = 1u, .end = 4u, .displayBase = 4u, .displaySpan = 4u};
    }
    if (exactCount <= 8u)
    {
        return {.start = 5u, .end = 8u, .displayBase = 8u, .displaySpan = 4u};
    }
    if (exactCount <= 12u)
    {
        return {.start = 9u, .end = 12u, .displayBase = 12u, .displaySpan = 4u};
    }

    const size_t bucketEnd = ((exactCount + 7u) / 8u) * 8u;
    return {.start = bucketEnd - 7u, .end = bucketEnd, .displayBase = bucketEnd, .displaySpan = 8u};
}

[[nodiscard]] size_t ScaleMaskedTextIndex(size_t index, size_t sourceLength, size_t targetLength) noexcept
{
    if (index == 0u || sourceLength == 0u || targetLength == 0u)
    {
        return 0u;
    }
    if (index >= sourceLength)
    {
        return targetLength;
    }

    const long double scaled = (static_cast<long double>(index) * static_cast<long double>(targetLength)) / static_cast<long double>(sourceLength);
    return (std::min)(targetLength, static_cast<size_t>(scaled + 0.5L));
}

} // namespace

[[nodiscard]] std::optional<std::pair<size_t, size_t>> ResolveNativeTextInputOptionalRange(const NativeTextInputState& state,
                                                                                           std::optional<size_t> startIndex,
                                                                                           std::optional<size_t> endIndex) noexcept
{
    if (! startIndex.has_value() || ! endIndex.has_value())
    {
        return std::nullopt;
    }

    const size_t start = (std::min)(startIndex.value(), state.text.size());
    const size_t end   = (std::min)(endIndex.value(), state.text.size());
    if (end <= start)
    {
        return std::nullopt;
    }

    return std::pair<size_t, size_t>{start, end};
}

[[nodiscard]] std::vector<D2D1_RECT_F> BuildTextInputUnderlineRects(
    const ControlHost& host, const Control& control, std::pair<size_t, size_t> range, float bottomInsetDip, float thicknessDip)
{
    std::vector<D2D1_RECT_F> underlineRects;
    const std::optional<std::vector<D2D1_RECT_F>> rangeRects = control.TryGetTextInputRangeRects(host, range.first, range.second);
    if (! rangeRects.has_value())
    {
        return underlineRects;
    }

    for (const D2D1_RECT_F& rangeRect : rangeRects.value())
    {
        const D2D1_RECT_F snappedRange = SnapRectToPixel(host, rangeRect);
        if (snappedRange.right <= snappedRange.left || snappedRange.bottom <= snappedRange.top)
        {
            continue;
        }

        const float thickness       = std::max(1.0f, thicknessDip);
        const float bottom          = std::min(snappedRange.bottom, std::max(snappedRange.top + thickness, snappedRange.bottom - bottomInsetDip));
        const D2D1_RECT_F underline = SnapRectToPixel(host, D2D1::RectF(snappedRange.left, bottom - thickness, snappedRange.right, bottom));
        if (underline.right > underline.left && underline.bottom > underline.top)
        {
            underlineRects.push_back(underline);
        }
    }

    return underlineRects;
}

[[nodiscard]] std::vector<D2D1_RECT_F> BuildNativeCompositionUnderlineRects(const ControlHost& host, const Control& control, const bool conversionTarget)
{
    // Composition and conversion-target ranges exist only while a composition is active: without one nothing is read.
    if (host.GetNativeTextCompositionActive(&control) != std::optional<bool>(true))
        return {};
    NativeTextInputState state{};
    if (! host.TryReadNativeTextInputState(&control, state))
    {
        return {};
    }

    const std::optional<std::pair<size_t, size_t>> range =
        conversionTarget ? ResolveNativeTextInputOptionalRange(state, state.conversionTargetStartIndex, state.conversionTargetEndIndex)
                         : ResolveNativeTextInputOptionalRange(state, state.compositionStartIndex, state.compositionEndIndex);
    if (! range.has_value())
    {
        return {};
    }

    return BuildTextInputUnderlineRects(host, control, range.value(), conversionTarget ? 1.0f : 2.0f, conversionTarget ? 2.0f : 1.0f);
}

namespace
{
// Both underlines of BuildNativeCompositionUnderlineRects from one read of the text-input state, made only while composing.
void DrawNativeCompositionUnderlines(ControlHost& host, const Control& control, const D2D1_COLOR_F& compositionColor, const D2D1_COLOR_F& targetColor)
{
    if (host.GetNativeTextCompositionActive(&control) != std::optional<bool>(true))
        return;
    NativeTextInputState state{};
    if (! host.TryReadNativeTextInputState(&control, state))
        return;
    if (const auto range = ResolveNativeTextInputOptionalRange(state, state.compositionStartIndex, state.compositionEndIndex))
        DrawTextInputUnderlineRects(host, BuildTextInputUnderlineRects(host, control, range.value(), 2.0f, 1.0f), compositionColor);
    if (const auto range = ResolveNativeTextInputOptionalRange(state, state.conversionTargetStartIndex, state.conversionTargetEndIndex))
        DrawTextInputUnderlineRects(host, BuildTextInputUnderlineRects(host, control, range.value(), 1.0f, 2.0f), targetColor);
}
} // namespace

void DrawTextInputUnderlineRects(ControlHost& host, const std::vector<D2D1_RECT_F>& underlineRects, const D2D1_COLOR_F& color) noexcept
{
    auto* dc    = host.GetDeviceContext();
    auto* brush = host.GetSolidBrush(color);
    if (! dc || ! brush)
    {
        return;
    }

    for (const D2D1_RECT_F& rect : underlineRects)
    {
        dc->FillRectangle(rect, brush);
    }
}

namespace
{
// Not a value SPI_GETWHEELSCROLLLINES reports (WHEEL_PAGESCROLL is UINT_MAX): the setting has not been read yet.
constexpr UINT kWheelScrollLinesUnread = std::numeric_limits<UINT>::max() - 1u;
std::atomic<UINT> wheelScrollLines{kWheelScrollLinesUnread};
#if DXUI_ENABLE_DIAGNOSTICS
std::atomic<bool> wheelScrollLinesOverridden{false};
std::atomic<UINT> wheelScrollLinesOverride{0u};
#endif

[[nodiscard]] UINT ReadWheelScrollLinesSetting() noexcept
{
#if DXUI_ENABLE_DIAGNOSTICS
    if (wheelScrollLinesOverridden.load(std::memory_order_acquire))
        return wheelScrollLinesOverride.load(std::memory_order_relaxed);
#endif
    UINT systemLines = 0u;
    if (SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &systemLines, 0) == FALSE)
        systemLines = 3u;
    return systemLines;
}
} // namespace

void RefreshWheelScrollLines() noexcept
{
    wheelScrollLines.store(ReadWheelScrollLinesSetting(), std::memory_order_relaxed);
}

size_t ResolveWheelScrollLineCount(size_t pageLineCount) noexcept
{
    UINT lines = wheelScrollLines.load(std::memory_order_relaxed);
    if (lines == kWheelScrollLinesUnread)
    {
        lines = ReadWheelScrollLinesSetting();
        wheelScrollLines.store(lines, std::memory_order_relaxed);
    }
    if (lines == WHEEL_PAGESCROLL)
        return std::max<size_t>(1u, pageLineCount);
    return static_cast<size_t>(lines);
}

#if DXUI_ENABLE_DIAGNOSTICS
void DebugSetWheelScrollLinesForTest(std::optional<UINT> lines) noexcept
{
    wheelScrollLinesOverride.store(lines.value_or(0u), std::memory_order_relaxed);
    wheelScrollLinesOverridden.store(lines.has_value(), std::memory_order_release);
}
#endif

namespace
{

[[nodiscard]] std::wstring NormalizeMultilineLineEndings(std::wstring_view text, bool multiline)
{
    if (! multiline || text.empty())
    {
        return std::wstring(text);
    }

    std::wstring normalized;
    normalized.reserve(text.size());
    for (size_t index = 0u; index < text.size(); ++index)
    {
        const wchar_t ch = text[index];
        if (ch == L'\r')
        {
            if (index + 1u < text.size() && text[index + 1u] == L'\n')
            {
                ++index;
            }

            normalized.push_back(L'\n');
            continue;
        }

        normalized.push_back(ch);
    }

    return normalized;
}

[[nodiscard]] std::wstring NormalizePastedControlText(std::wstring_view text, bool multiline)
{
    if (text.empty())
    {
        return std::wstring(text);
    }

    if (multiline)
    {
        return NormalizeMultilineLineEndings(text, true);
    }

    std::wstring normalized;
    normalized.reserve(text.size());
    for (wchar_t ch : text)
    {
        const auto codeUnit = static_cast<unsigned int>(ch);
        if (codeUnit < 0x20u || (codeUnit >= 0x7Fu && codeUnit <= 0x9Fu))
        {
            continue;
        }

        normalized.push_back(ch);
    }

    return normalized;
}

[[nodiscard]] size_t FindLineStart(std::wstring_view text, size_t caretIndex) noexcept
{
    size_t index = std::min(caretIndex, text.size());
    while (index > 0u && text[index - 1u] != L'\n')
    {
        --index;
    }
    return index;
}

[[nodiscard]] size_t FindLineEnd(std::wstring_view text, size_t caretIndex) noexcept
{
    size_t index = std::min(caretIndex, text.size());
    while (index < text.size() && text[index] != L'\n')
    {
        ++index;
    }
    return index;
}

[[nodiscard]] wil::com_ptr<IDWriteTextLayout> CreateMultilineTextLayout(
    const ControlHost* host, std::wstring_view text, FontRole role, float widthDip, float heightDip) noexcept;
[[nodiscard]] std::vector<DWRITE_LINE_METRICS> GetMultilineLineMetrics(IDWriteTextLayout* layout) noexcept;
[[nodiscard]] std::optional<size_t> TryGetMultilineCaretLineIndex(IDWriteTextLayout* layout,
                                                                  const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                  size_t caretIndex,
                                                                  size_t textLength) noexcept;
struct WrappedLineTextRange
{
    size_t start = 0u;
    size_t end   = 0u;
};

[[nodiscard]] size_t MoveCaretVerticallyByLogicalLine(std::wstring_view text,
                                                      size_t caretIndex,
                                                      bool moveDown,
                                                      std::optional<float>& preferredXOffsetDip) noexcept
{
    const size_t caret            = std::min(caretIndex, text.size());
    const size_t currentLineStart = FindLineStart(text, caret);
    const size_t currentLineEnd   = FindLineEnd(text, caret);
    const size_t currentColumn    = std::min(caret - currentLineStart, currentLineEnd - currentLineStart);
    const float targetXOffsetDip  = preferredXOffsetDip.value_or(static_cast<float>(currentColumn) * 7.0f);
    preferredXOffsetDip           = targetXOffsetDip;
    const size_t targetColumn     = static_cast<size_t>(std::max(0.0, std::floor(static_cast<double>(targetXOffsetDip) / 7.0)));

    if (moveDown)
    {
        if (currentLineEnd >= text.size())
        {
            return caret;
        }

        const size_t nextLineStart = currentLineEnd + 1u;
        const size_t nextLineEnd   = FindLineEnd(text, nextLineStart);
        return nextLineStart + std::min(targetColumn, nextLineEnd - nextLineStart);
    }

    if (currentLineStart == 0u)
    {
        return caret;
    }

    const size_t previousLineEnd   = currentLineStart - 1u;
    const size_t previousLineStart = FindLineStart(text, previousLineEnd);
    return previousLineStart + std::min(targetColumn, previousLineEnd - previousLineStart);
}

struct MultilineViewportMetrics
{
    size_t totalLineCount   = 1u;
    size_t visibleLineCount = 1u;
};

[[nodiscard]] size_t CountLogicalTextLines(std::wstring_view text) noexcept
{
    return 1u + static_cast<size_t>(std::count(text.begin(), text.end(), L'\n'));
}

[[nodiscard]] size_t ComputeMultilinePageLineCount(float viewportHeightDip, float lineHeightDip) noexcept
{
    const float effectiveLineHeightDip = std::max(1.0f, lineHeightDip);
    return std::max<size_t>(1u, static_cast<size_t>(std::floor(std::max(1.0f, viewportHeightDip) / effectiveLineHeightDip)));
}

[[nodiscard]] size_t MoveCaretByPage(
    std::wstring_view text, size_t caretIndex, bool moveDown, std::optional<float>& preferredXOffsetDip, size_t lineCount) noexcept
{
    size_t nextCaretIndex = caretIndex;
    for (size_t step = 0u; step < lineCount; ++step)
    {
        const size_t movedIndex = MoveCaretVerticallyByLogicalLine(text, nextCaretIndex, moveDown, preferredXOffsetDip);
        if (movedIndex == nextCaretIndex)
        {
            break;
        }

        nextCaretIndex = movedIndex;
    }

    return nextCaretIndex;
}

// `layout` and `metrics` are the multiline layout of `text` at the width of `textRect` and its line metrics.
[[nodiscard]] std::optional<size_t> TryMoveCaretVerticallyByWrappedLines(IDWriteTextLayout* layout,
                                                                         const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                         std::wstring_view text,
                                                                         const D2D1_RECT_F& textRect,
                                                                         size_t caretIndex,
                                                                         int visualLineDelta,
                                                                         std::optional<float>& preferredXOffsetDip) noexcept
{
    if (! layout || text.empty() || visualLineDelta == 0 || (textRect.right - textRect.left) <= 1.0f || (textRect.bottom - textRect.top) <= 1.0f)
    {
        return std::nullopt;
    }

    if (metrics.empty())
    {
        return std::nullopt;
    }

    float currentX = 0.0f;
    float currentY = 0.0f;
    DWRITE_HIT_TEST_METRICS currentHitMetrics{};
    if (FAILED(layout->HitTestTextPosition(static_cast<UINT32>(std::min(caretIndex, text.size())), FALSE, &currentX, &currentY, &currentHitMetrics)))
    {
        return std::nullopt;
    }

    const std::optional<size_t> currentLineIndex = TryGetMultilineCaretLineIndex(layout, metrics, caretIndex, text.size());
    if (! currentLineIndex)
    {
        return std::nullopt;
    }

    const int maxLineIndex    = static_cast<int>(metrics.size()) - 1;
    const int targetLineIndex = std::clamp(static_cast<int>(currentLineIndex.value()) + visualLineDelta, 0, maxLineIndex);
    if (targetLineIndex == static_cast<int>(currentLineIndex.value()))
    {
        return caretIndex;
    }

    const float targetX = preferredXOffsetDip.value_or(currentX);
    preferredXOffsetDip = targetX;

    float targetTopDip = 0.0f;
    for (int index = 0; index < targetLineIndex; ++index)
    {
        targetTopDip += std::max(1.0f, metrics[static_cast<size_t>(index)].height);
    }

    const float targetHeightDip = std::max(1.0f, metrics[static_cast<size_t>(targetLineIndex)].height);
    const float targetY         = targetTopDip + std::min(targetHeightDip - 1.0f, targetHeightDip * 0.5f);

    BOOL isTrailingHit = FALSE;
    BOOL isInside      = FALSE;
    DWRITE_HIT_TEST_METRICS targetHitMetrics{};
    if (FAILED(layout->HitTestPoint(targetX, targetY, &isTrailingHit, &isInside, &targetHitMetrics)))
    {
        return std::nullopt;
    }

    const size_t textPosition    = static_cast<size_t>(targetHitMetrics.textPosition);
    const size_t trailingAdvance = isTrailingHit ? static_cast<size_t>(targetHitMetrics.length) : 0u;
    return std::min(text.size(), textPosition + trailingAdvance);
}

[[nodiscard]] size_t MoveMultilineCaretVertically(IDWriteTextLayout* layout,
                                                  const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                  std::wstring_view text,
                                                  const D2D1_RECT_F& textRect,
                                                  size_t caretIndex,
                                                  bool moveDown,
                                                  std::optional<float>& preferredXOffsetDip) noexcept
{
    if (const std::optional<size_t> wrappedCaretIndex =
            TryMoveCaretVerticallyByWrappedLines(layout, metrics, text, textRect, caretIndex, moveDown ? 1 : -1, preferredXOffsetDip))
    {
        return wrappedCaretIndex.value();
    }

    return MoveCaretVerticallyByLogicalLine(text, caretIndex, moveDown, preferredXOffsetDip);
}

[[nodiscard]] size_t MoveMultilineCaretByPage(IDWriteTextLayout* layout,
                                              const std::vector<DWRITE_LINE_METRICS>& metrics,
                                              std::wstring_view text,
                                              const D2D1_RECT_F& textRect,
                                              size_t caretIndex,
                                              bool moveDown,
                                              std::optional<float>& preferredXOffsetDip,
                                              size_t lineCount) noexcept
{
    if (lineCount == 0u)
    {
        return caretIndex;
    }

    if (const std::optional<size_t> wrappedCaretIndex = TryMoveCaretVerticallyByWrappedLines(
            layout, metrics, text, textRect, caretIndex, moveDown ? static_cast<int>(lineCount) : -static_cast<int>(lineCount), preferredXOffsetDip))
    {
        return wrappedCaretIndex.value();
    }

    return MoveCaretByPage(text, caretIndex, moveDown, preferredXOffsetDip, lineCount);
}

// The text range of wrapped line `lineIndex` (without its line break), walking the metrics up to it; nullopt past the last.
[[nodiscard]] std::optional<WrappedLineTextRange> WrappedLineTextRangeAt(const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                         size_t textLength,
                                                                         size_t lineIndex) noexcept
{
    size_t lineStart = 0u;
    for (size_t index = 0u; index < metrics.size(); ++index)
    {
        const DWRITE_LINE_METRICS& metric = metrics[index];
        const size_t rawLineEnd           = std::min(textLength, lineStart + static_cast<size_t>(metric.length));
        if (index == lineIndex)
        {
            size_t visibleLineEnd = rawLineEnd;
            if (metric.newlineLength > 0u)
            {
                const size_t newlineLength = static_cast<size_t>(metric.newlineLength);
                visibleLineEnd             = (visibleLineEnd >= newlineLength) ? visibleLineEnd - newlineLength : lineStart;
            }
            return WrappedLineTextRange{lineStart, std::max(lineStart, visibleLineEnd)};
        }
        lineStart = rawLineEnd;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<size_t> TryMoveCaretToWrappedLineBoundary(IDWriteTextLayout* layout,
                                                                      const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                      std::wstring_view text,
                                                                      const D2D1_RECT_F& textRect,
                                                                      size_t caretIndex,
                                                                      bool moveToLineEnd) noexcept
{
    if (! layout || text.empty() || (textRect.right - textRect.left) <= 1.0f || (textRect.bottom - textRect.top) <= 1.0f)
    {
        return std::nullopt;
    }

    if (metrics.empty() || metrics.size() <= CountLogicalTextLines(text))
    {
        return std::nullopt;
    }

    const std::optional<size_t> currentLineIndex = TryGetMultilineCaretLineIndex(layout, metrics, caretIndex, text.size());
    if (! currentLineIndex)
    {
        return std::nullopt;
    }

    const std::optional<WrappedLineTextRange> range = WrappedLineTextRangeAt(metrics, text.size(), currentLineIndex.value());
    if (! range)
    {
        return std::nullopt;
    }

    return moveToLineEnd ? range->end : range->start;
}

[[nodiscard]] wil::com_ptr<IDWriteTextLayout> CreateMultilineTextLayout(
    const ControlHost* host, std::wstring_view text, FontRole role, float widthDip, float heightDip) noexcept
{
    if (! host)
    {
        return {};
    }

    auto* factory = host->GetWriteFactory();
    auto* format  = host->GetTextFormat(role, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
    if (! factory || ! format)
    {
        return {};
    }

    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(CountedCreateTextLayout(factory,
                                       text.data(),
                                       static_cast<UINT32>(text.size()),
                                       format,
                                       std::max(1.0f, widthDip),
                                       std::max(kMultilineLayoutHeightDip, heightDip),
                                       layout.addressof())) ||
        ! layout)
    {
        return {};
    }
    return layout;
}

[[nodiscard]] std::vector<DWRITE_LINE_METRICS> GetMultilineLineMetrics(IDWriteTextLayout* layout) noexcept
{
    if (! layout)
    {
        return {};
    }

    UINT32 actualLineCount = 0u;
    HRESULT hr             = layout->GetLineMetrics(nullptr, 0u, &actualLineCount);
    if (FAILED(hr) && actualLineCount == 0u)
    {
        return {};
    }

    std::vector<DWRITE_LINE_METRICS> metrics(actualLineCount);
    if (actualLineCount == 0u)
    {
        return metrics;
    }

    if (FAILED(layout->GetLineMetrics(metrics.data(), actualLineCount, &actualLineCount)))
    {
        return {};
    }

    metrics.resize(actualLineCount);
    return metrics;
}

// GetMultilineLineMetrics into retained storage: a relayout of text with no more lines than before allocates nothing.
void FillMultilineLineMetrics(IDWriteTextLayout* layout, std::vector<DWRITE_LINE_METRICS>& metrics) noexcept
{
    metrics.clear();
    if (! layout)
        return;
    UINT32 actualLineCount = 0u;
    const HRESULT hr       = layout->GetLineMetrics(nullptr, 0u, &actualLineCount);
    if ((FAILED(hr) && actualLineCount == 0u) || actualLineCount == 0u)
        return;
    try
    {
        metrics.resize(actualLineCount);
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
    if (FAILED(layout->GetLineMetrics(metrics.data(), actualLineCount, &actualLineCount)))
    {
        metrics.clear();
        return;
    }
    metrics.resize(actualLineCount);
}

[[nodiscard]] float EstimateMultilineFallbackLineHeightDip(const ControlHost* host, FontRole role) noexcept
{
    if (! host)
    {
        return 20.0f;
    }

    auto* factory = host->GetWriteFactory();
    auto* format  = host->GetTextFormat(role, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
    if (! factory || ! format)
    {
        return 20.0f;
    }

    static constexpr wchar_t kSampleText[] = L"Ag";
    wil::com_ptr<IDWriteTextLayout> layout;
    if (FAILED(CountedCreateTextLayout(
            factory, kSampleText, static_cast<UINT32>(std::size(kSampleText) - 1u), format, 1024.0f, kMultilineLayoutHeightDip, layout.addressof())) ||
        ! layout)
    {
        return 20.0f;
    }

    const std::vector<DWRITE_LINE_METRICS> metrics = GetMultilineLineMetrics(layout.get());
    if (! metrics.empty())
    {
        return std::max(1.0f, metrics.front().height);
    }

    DWRITE_TEXT_METRICS textMetrics{};
    if (SUCCEEDED(layout->GetMetrics(&textMetrics)) && textMetrics.height > 0.0f)
    {
        return textMetrics.height;
    }

    return 20.0f;
}

[[nodiscard]] float MeasureWrappedLineOffsetDip(const std::vector<DWRITE_LINE_METRICS>& metrics, size_t firstVisibleLine) noexcept
{
    if (metrics.empty() || firstVisibleLine == 0u)
    {
        return 0.0f;
    }

    const size_t clampedFirstVisibleLine = std::min(firstVisibleLine, metrics.size());
    float offsetDip                      = 0.0f;
    for (size_t index = 0u; index < clampedFirstVisibleLine; ++index)
    {
        offsetDip += metrics[index].height;
    }
    return offsetDip;
}

[[nodiscard]] size_t CountVisibleWrappedLines(const std::vector<DWRITE_LINE_METRICS>& metrics, float viewportHeightDip, float fallbackLineHeightDip) noexcept
{
    if (metrics.empty())
    {
        return ComputeMultilinePageLineCount(viewportHeightDip, fallbackLineHeightDip);
    }

    const float clampedViewportHeightDip = std::max(1.0f, viewportHeightDip);
    float consumedHeightDip              = 0.0f;
    size_t visibleLineCount              = 0u;
    for (const DWRITE_LINE_METRICS& metric : metrics)
    {
        ++visibleLineCount;
        consumedHeightDip += std::max(1.0f, metric.height);
        if (consumedHeightDip >= clampedViewportHeightDip)
        {
            break;
        }
    }

    return std::max<size_t>(1u, visibleLineCount);
}

[[nodiscard]] MultilineViewportMetrics BuildMultilineViewportMetrics(std::wstring_view text,
                                                                     float viewportHeightDip,
                                                                     const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                     float fallbackLineHeightDip) noexcept
{
    MultilineViewportMetrics viewportMetrics;
    if (metrics.empty())
    {
        viewportMetrics.totalLineCount   = CountLogicalTextLines(text);
        viewportMetrics.visibleLineCount = ComputeMultilinePageLineCount(viewportHeightDip, fallbackLineHeightDip);
        return viewportMetrics;
    }

    viewportMetrics.totalLineCount   = std::max<size_t>(1u, metrics.size());
    viewportMetrics.visibleLineCount = CountVisibleWrappedLines(metrics, viewportHeightDip, fallbackLineHeightDip);
    return viewportMetrics;
}

[[nodiscard]] size_t ComputeMultilineMaxFirstVisibleLine(const MultilineViewportMetrics& viewportMetrics) noexcept
{
    // Preserve imported/view-driven top-line state up to the last content line.
    // The DxUi surface can legitimately show trailing blank space below the last line.
    return viewportMetrics.totalLineCount > 0u ? viewportMetrics.totalLineCount - 1u : 0u;
}

[[nodiscard]] size_t ClampMultilineFirstVisibleLine(size_t firstVisibleLine, const MultilineViewportMetrics& viewportMetrics) noexcept
{
    return (std::min)(firstVisibleLine, ComputeMultilineMaxFirstVisibleLine(viewportMetrics));
}

[[nodiscard]] std::optional<size_t> TryGetMultilineCaretLineIndex(IDWriteTextLayout* layout,
                                                                  const std::vector<DWRITE_LINE_METRICS>& metrics,
                                                                  size_t caretIndex,
                                                                  size_t textLength) noexcept
{
    if (! layout || metrics.empty())
    {
        return std::nullopt;
    }

    float x = 0.0f;
    float y = 0.0f;
    DWRITE_HIT_TEST_METRICS hitMetrics{};
    if (FAILED(layout->HitTestTextPosition(static_cast<UINT32>(std::min(caretIndex, textLength)), FALSE, &x, &y, &hitMetrics)))
    {
        return std::nullopt;
    }

    float lineTopDip = 0.0f;
    for (size_t index = 0u; index < metrics.size(); ++index)
    {
        lineTopDip += std::max(1.0f, metrics[index].height);
        if (y < lineTopDip || index + 1u == metrics.size())
        {
            return index;
        }
    }

    return std::nullopt;
}

// `layout` is the multiline layout of `text` at the width of `textRect`, or null (an estimate from the text alone).
[[nodiscard]] size_t HitTestMultilineCaretIndexDip(
    IDWriteTextLayout* layout, std::wstring_view text, const D2D1_RECT_F& textRect, float scrollDip, D2D1_POINT_2F point) noexcept
{
    if (text.empty())
    {
        return 0u;
    }

    const float localX = std::max(0.0f, point.x - textRect.left);
    const float localY = std::max(0.0f, point.y - textRect.top + scrollDip);
    if (layout)
    {
        BOOL isTrailingHit = FALSE;
        BOOL isInside      = FALSE;
        DWRITE_HIT_TEST_METRICS metrics{};
        if (SUCCEEDED(layout->HitTestPoint(localX, localY, &isTrailingHit, &isInside, &metrics)))
        {
            const size_t textPosition    = static_cast<size_t>(metrics.textPosition);
            const size_t trailingAdvance = isTrailingHit ? static_cast<size_t>(metrics.length) : 0u;
            return std::min(text.size(), textPosition + trailingAdvance);
        }
    }

    size_t lineStart = 0u;
    size_t lineIndex = static_cast<size_t>(std::max(0.0, std::floor(static_cast<double>(localY) / 18.0)));
    while (lineIndex > 0u && lineStart < text.size())
    {
        const size_t nextBreak = text.find(L'\n', lineStart);
        if (nextBreak == std::wstring_view::npos)
        {
            return text.size();
        }
        lineStart = nextBreak + 1u;
        --lineIndex;
    }

    const size_t lineEnd        = text.find(L'\n', lineStart);
    const size_t clampedLineEnd = lineEnd == std::wstring_view::npos ? text.size() : lineEnd;
    const size_t column = static_cast<size_t>(std::clamp(std::floor(static_cast<double>(localX) / 7.0), 0.0, static_cast<double>(clampedLineEnd - lineStart)));
    return std::min(text.size(), lineStart + column);
}

[[nodiscard]] D2D1_RECT_F MeasureMultilineCaretRectDip(
    IDWriteTextLayout* layout, std::wstring_view text, const D2D1_RECT_F& textRect, float scrollDip, size_t caretIndex) noexcept
{
    const D2D1_RECT_F fallbackRect = D2D1::RectF(textRect.left, textRect.top + 2.0f, textRect.left + 1.0f, textRect.top + 18.0f);
    if (layout)
    {
        float x = 0.0f;
        float y = 0.0f;
        DWRITE_HIT_TEST_METRICS metrics{};
        if (SUCCEEDED(layout->HitTestTextPosition(static_cast<UINT32>(std::min(caretIndex, text.size())), FALSE, &x, &y, &metrics)))
        {
            const float caretTop    = textRect.top + y - scrollDip;
            const float caretHeight = std::max(12.0f, metrics.height);
            return D2D1::RectF(textRect.left + x, caretTop, textRect.left + x + 1.0f, caretTop + caretHeight);
        }
    }

    return fallbackRect;
}

[[nodiscard]] D2D1_RECT_F ClipTextInputRectToBounds(D2D1_RECT_F rect, const D2D1_RECT_F& bounds) noexcept
{
    rect.left   = std::clamp(rect.left, bounds.left, bounds.right);
    rect.right  = std::clamp(rect.right, rect.left, bounds.right);
    rect.top    = std::clamp(rect.top, bounds.top, bounds.bottom);
    rect.bottom = std::clamp(rect.bottom, rect.top, bounds.bottom);
    return rect;
}

// The width the painted multiline text and its selection wrap at: the text rectangle's own unsnapped width, which
// the caret, hit-testing and line metrics use too. Snapping may move the drawing origin, never the wrap width.
[[nodiscard]] float MultilinePaintLayoutWidthDip(const D2D1_RECT_F& rect) noexcept
{
    return std::max(1.0f, rect.right - rect.left);
}

// `layout` is the field's retained multiline layout of `text`, made at MultilinePaintLayoutWidthDip(rect), or null.
void DrawMultilineSelection(ControlHost& host,
                            IDWriteTextLayout* layout,
                            std::wstring_view text,
                            const D2D1_RECT_F& rect,
                            FontRole role,
                            const D2D1_COLOR_F& textColor,
                            const D2D1_COLOR_F& selectionFill,
                            const D2D1_COLOR_F& selectionText,
                            float scrollDip,
                            std::optional<std::pair<size_t, size_t>> selectionRange) noexcept
{
    auto* dc                = host.GetDeviceContext();
    auto* fillBrush         = host.GetSolidBrush(selectionFill);
    auto* textBrush         = host.GetSolidBrush(textColor);
    auto* selectedTextBrush = host.GetSolidBrush(selectionText);
    if (! dc || ! textBrush)
    {
        return;
    }

    const D2D1_RECT_F snappedRect = SnapRectToPixel(host, rect);
    if (! layout)
    {
        DrawCenteredText(host, text, snappedRect, role, textColor, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);
        return;
    }

    dc->PushAxisAlignedClip(snappedRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    dc->DrawTextLayout(D2D1::Point2F(snappedRect.left, snappedRect.top - scrollDip), layout, textBrush, kTextDrawOptions);

    if (selectionRange && fillBrush && selectedTextBrush)
    {
        const auto [selectionStart, selectionEnd] = selectionRange.value();
        if (selectionStart < selectionEnd)
        {
            UINT32 actualCount = 0u;
            std::vector<DWRITE_HIT_TEST_METRICS> metrics(64u);
            HRESULT hr = layout->HitTestTextRange(static_cast<UINT32>(selectionStart),
                                                  static_cast<UINT32>(selectionEnd - selectionStart),
                                                  snappedRect.left,
                                                  snappedRect.top - scrollDip,
                                                  metrics.data(),
                                                  static_cast<UINT32>(metrics.size()),
                                                  &actualCount);
            if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
            {
                metrics.resize(actualCount);
                hr = layout->HitTestTextRange(static_cast<UINT32>(selectionStart),
                                              static_cast<UINT32>(selectionEnd - selectionStart),
                                              snappedRect.left,
                                              snappedRect.top - scrollDip,
                                              metrics.data(),
                                              static_cast<UINT32>(metrics.size()),
                                              &actualCount);
            }

            if (SUCCEEDED(hr))
            {
                for (UINT32 index = 0u; index < actualCount; ++index)
                {
                    const auto& hit           = metrics[index];
                    const D2D1_RECT_F hitRect = SnapRectToPixel(host, D2D1::RectF(hit.left, hit.top + 1.0f, hit.left + hit.width, hit.top + hit.height - 1.0f));
                    if (hitRect.right > hitRect.left)
                    {
                        dc->FillRectangle(hitRect, fillBrush);
                    }
                }

                for (UINT32 index = 0u; index < actualCount; ++index)
                {
                    const auto& hit           = metrics[index];
                    const D2D1_RECT_F hitRect = SnapRectToPixel(host, D2D1::RectF(hit.left, hit.top, hit.left + hit.width, hit.top + hit.height));
                    if (hitRect.right <= hitRect.left)
                    {
                        continue;
                    }

                    dc->PushAxisAlignedClip(hitRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                    dc->DrawTextLayout(D2D1::Point2F(snappedRect.left, snappedRect.top - scrollDip), layout, selectedTextBrush, kTextDrawOptions);
                    dc->PopAxisAlignedClip();
                }
            }
        }
    }

    dc->PopAxisAlignedClip();
}

} // namespace

void ControlHost::CommitFocusedTextInput() noexcept
{
    SyncNativeTextInputSession(_focusedControl);
}

bool ControlHost::TryReadTextInputState(const Control* control, TextInputState& outState) const noexcept
{
    if (! control || control != _nativeTextInputControl || ! _nativeTextInputStateCacheValid)
    {
        return false;
    }

    // The public state carries its own copy of the document; a rich editor's revision is materialized once for it.
    outState.text.assign(GetNativeTextInputCacheText());
    if (_nativeTextInputCacheSource)
        NoteRichDocumentCopy(outState.text.size());
    outState.selectionAnchorIndex = _nativeTextInputStateCache.selectionAnchorIndex;
    outState.caretIndex           = _nativeTextInputStateCache.caretIndex;
    outState.firstVisibleLine     = _nativeTextInputStateCache.firstVisibleLine;
    outState.readOnly             = _nativeTextInputStateCache.readOnly;
    outState.masked               = _nativeTextInputStateCache.masked;
    outState.multiline            = _nativeTextInputStateCache.multiline;
    return true;
}

void ControlHost::SyncTextInput(Control* control) noexcept
{
    // Embedded composition imports synchronize privately. A public control edit is newer application/input
    // state and must not be undone by cancellation of the previous OS-service preview.
    if (_embedded && _nativeTextInputImeComposing && control && control == _nativeTextInputControl)
    {
        try
        {
            // Shares the document where the control allows it; the comparison reads in place.
            SharedTextInputState actual;
            if (SharedTextInputAccess::Export(*control, actual) &&
                ! SameTextInputContent(actual.text,
                                       actual.source,
                                       _nativeTextInputStateCacheValid ? _nativeTextInputCacheText : SharedText(),
                                       _nativeTextInputStateCacheValid ? _nativeTextInputCacheSource : std::nullopt))
                ClearNativeTextInputCompositionState();
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }
    SyncNativeTextInputSession(control);
}

bool ControlHost::HasActiveTextInput() const noexcept
{
    return HasActiveNativeTextInputSession();
}

HWND ControlHost::GetTextInputHwnd() const noexcept
{
    return HasActiveNativeTextInputSession() ? _hwnd : nullptr;
}

namespace
{
// Securely wipes a shared text buffer when its last owner releases it: every unit up to capacity(), so text that shrank in
// place leaves nothing behind either. Its presence on a buffer (std::get_deleter) also proves MakeSharedText created the
// buffer as a non-const std::wstring.
struct SharedTextDeleter final
{
    void operator()(const std::wstring* text) const noexcept
    {
        auto* const owned     = const_cast<std::wstring*>(text);
        const size_t sizeUnit = owned->size();
        // Growing to the capacity never reallocates; it makes every unit of the allocation part of the string, so the wipe
        // below writes only within it.
        owned->resize(owned->capacity());
        SecureZeroMemory(owned->data(), owned->size() * sizeof(wchar_t));
        if (const auto observer = Detail::sharedTextReleaseObserver)
            observer(owned->data(), owned->size(), sizeUnit);
        delete owned;
    }
};
} // namespace

SharedText MakeSharedText(std::wstring text)
{
    // If the control block cannot be allocated, shared_ptr runs the deleter on the new string before rethrowing.
    return SharedText(new std::wstring(std::move(text)), SharedTextDeleter{});
}

const SharedText& EmptySharedText() noexcept
{
    // Made once on first use (before any control can need it: a control's constructor asks for it) and never released before
    // process exit, so it always has an owner besides any control holding it.
    static const SharedText empty = []() noexcept
    {
        try
        {
            return MakeSharedText(std::wstring());
        }
        catch (const std::bad_alloc&)
        {
            Failure::FailAllocation();
        }
    }();
    return empty;
}

std::wstring& MutableSharedText(SharedText& text, size_t requiredUnits)
{
    if (text && text.use_count() == 1 && std::get_deleter<SharedTextDeleter>(text) && text->capacity() >= requiredUnits)
    {
        // Sole owner of a buffer made non-const by MakeSharedText: no other holder can observe this write. The fence pairs
        // with the release of the last other owner, whose reads therefore happen before the write. The capacity check keeps
        // std::wstring from reallocating and freeing the old block without the wipe.
        std::atomic_thread_fence(std::memory_order_acquire);
        return const_cast<std::wstring&>(*text);
    }
    const std::wstring_view current = SharedTextView(text);
    const size_t target             = (std::max)(current.size(), requiredUnits);
    std::wstring copy;
    // Room for the edit that follows and a little typing after it, so neither reallocates the copy.
    copy.reserve(target + target / 16u + 16u);
    copy.assign(current);
    // The old buffer leaves through its deleter once its last owner releases it.
    text = MakeSharedText(std::move(copy));
    return const_cast<std::wstring&>(*text);
}

namespace
{
// Two revisions with equal lengths: equal outside their tracked changed range by construction, so only that range is read.
[[nodiscard]] bool SourcesHaveSameText(const RichText::SourceSnapshot& left, const RichText::SourceSnapshot& right) noexcept
{
    if (left.IsSameRevision(right))
        return true;
    if (left.Length() != right.Length())
        return false;
    const auto range = right.ChangedRangeFrom(left);
    if (range.kind == RichText::SourceChangeKind::Unchanged)
        return true;
    if (range.previousEnd - range.begin != range.currentEnd - range.begin)
        return false;
    for (size_t offset = range.begin; offset < range.previousEnd; ++offset)
        if (left.UnitAt(offset) != right.UnitAt(offset))
            return false;
    return true;
}
} // namespace

bool SameTextInputContent(const SharedText& leftText,
                          const std::optional<RichText::SourceSnapshot>& leftSource,
                          const SharedText& rightText,
                          const std::optional<RichText::SourceSnapshot>& rightSource) noexcept
{
    if (leftText && leftText == rightText)
        return true;
    if (leftSource && rightSource)
        return SourcesHaveSameText(*leftSource, *rightSource);
    if (leftText && rightText)
        return SharedTextEquals(leftText, rightText);
    if (leftSource && rightText)
        return leftSource->MatchesText(*rightText);
    if (rightSource && leftText)
        return rightSource->MatchesText(*leftText);
    // A side without any document reads as empty text, as SharedTextView does.
    const size_t leftLength  = leftText ? leftText->size() : leftSource ? leftSource->Length() : 0u;
    const size_t rightLength = rightText ? rightText->size() : rightSource ? rightSource->Length() : 0u;
    return leftLength == 0u && rightLength == 0u;
}

bool TextInputContentEquals(const SharedTextInputState& state, std::wstring_view text) noexcept
{
    if (state.text)
        return SharedTextEquals(state.text, text);
    if (state.source)
        return state.source->MatchesText(text);
    return text.empty();
}

SharedText MaterializeSharedText(const RichText::SourceSnapshot& source)
{
    std::wstring text(0u, L'\0');
    if (source.TrySlice(0u, source.Length(), text) != RichText::SourceResult::Success)
        Failure::FailContract(E_UNEXPECTED); // A revision's own extent is always a valid slice.
    return MakeSharedText(std::move(text));
}

bool SharedTextInputAccess::Shares(const Control& control) noexcept
{
    if (const auto* field = dynamic_cast<const TextField*>(&control))
        return field->KeepsDefaultTextInputState();
    if (const auto* comboBox = dynamic_cast<const ComboBox*>(&control))
        return comboBox->KeepsDefaultTextInputState();
    if (const auto* editor = dynamic_cast<const RichTextEditor*>(&control))
        return editor->KeepsDefaultTextInputState();
    return false;
}

void SharedTextInputAccess::Materialize(const Control* control, SharedTextInputState& state)
{
    if (state.text || ! state.source)
        return;
    if (const auto* editor = dynamic_cast<const RichTextEditor*>(control); editor && editor->KeepsDefaultTextInputState())
    {
        state.text = editor->TextInputText(*state.source);
        return;
    }
    state.text = MaterializeSharedText(*state.source);
}

bool SharedTextInputAccess::ShowsText(const Control& control, const SharedText& text) noexcept
{
    if (const auto* textField = dynamic_cast<const TextField*>(&control))
        return SharedTextEquals(text, textField->GetText());
    if (const auto* comboBox = dynamic_cast<const ComboBox*>(&control))
        return SharedTextEquals(text, comboBox->GetText());
    if (const auto* editor = dynamic_cast<const RichTextEditor*>(&control))
        return editor->ShowsTextInputText(text);
    return false;
}

bool SharedTextInputAccess::Export(const Control& control, SharedTextInputState& state)
{
    // The fields are read directly only for a control that keeps the default ExportTextInputState; an override is called.
    if (const auto* field = dynamic_cast<const TextField*>(&control); field && field->KeepsDefaultTextInputState())
    {
        state = {field->_text,
                 field->_selectionAnchorIndex,
                 field->_caretIndex,
                 field->_multilineFirstVisibleLine,
                 field->_readOnly,
                 field->_masked,
                 field->_multiline};
        return true;
    }
    if (const auto* comboBox = dynamic_cast<const ComboBox*>(&control); comboBox && comboBox->KeepsDefaultTextInputState())
    {
        if (! comboBox->SupportsTextInput())
            return false;
        state = {comboBox->_text, comboBox->_selectionAnchorIndex, comboBox->_caretIndex, 0u, false, false, false};
        return true;
    }
    if (const auto* editor = dynamic_cast<const RichTextEditor*>(&control); editor && editor->KeepsDefaultTextInputState())
    {
        editor->ExportTextInputSource(state);
        return true;
    }
    TextInputState copy;
    if (! control.ExportTextInputState(copy))
        return false;
    state = {
        MakeSharedText(std::move(copy.text)), copy.selectionAnchorIndex, copy.caretIndex, copy.firstVisibleLine, copy.readOnly, copy.masked, copy.multiline};
    return true;
}

bool SharedTextInputAccess::Import(Control& control, ControlHost& host, const SharedTextInputState& state, bool notifyChange)
{
    // The non-text fields travel in a TextInputState whose text stays empty; the document is the shared buffer.
    const TextInputState fields{
        std::wstring(), state.selectionAnchorIndex, state.caretIndex, state.firstVisibleLine, state.readOnly, state.masked, state.multiline};
    if (auto* editor = dynamic_cast<RichTextEditor*>(&control); editor && editor->KeepsDefaultTextInputState())
    {
        // Imports read the document in place: the editor applies only its changed range and keeps the buffer as its copy.
        if (state.text)
            return editor->ImportTextInputText(host, state.text, fields, notifyChange);
        if (! state.source)
            return false;
        SharedTextInputState materialized = state;
        Materialize(&control, materialized);
        return editor->ImportTextInputText(host, materialized.text, fields, notifyChange);
    }
    if (! state.text && state.source)
    {
        SharedTextInputState materialized = state;
        Materialize(nullptr, materialized);
        return Import(control, host, materialized, notifyChange);
    }
    if (! state.text)
        return false;
    // The buffer is adopted only by a control that keeps the default ImportTextInputState; an override is called.
    if (auto* field = dynamic_cast<TextField*>(&control); field && field->KeepsDefaultTextInputState())
        return field->ImportTextInputText(host, state.text, fields, notifyChange);
    if (auto* comboBox = dynamic_cast<ComboBox*>(&control); comboBox && comboBox->KeepsDefaultTextInputState())
        return comboBox->ImportTextInputText(host, state.text, fields, notifyChange);
    TextInputState copy = fields;
    copy.text.assign(state.View());
    return control.ImportTextInputState(host, copy, notifyChange);
}

void SharedTextInputAccess::SettlePreview(Control& control, ControlHost& host) noexcept
{
    if (auto* comboBox = dynamic_cast<ComboBox*>(&control))
        comboBox->SettlePreview(&host);
}

bool TextField::KeepsDefaultTextInputState() const noexcept
{
    return typeid(*this) == typeid(TextField);
}
TextField::TextField(std::wstring text) : _text(text.empty() ? EmptySharedText() : MakeSharedText(std::move(text)))
{
    _caretIndex = _text->size();
    SetFocusable(true);
}

const std::wstring& TextField::Text() const noexcept
{
    return *_text;
}

std::wstring& TextField::MutableText(size_t requiredUnits)
{
    return MutableSharedText(_text, requiredUnits);
}

TextField::~TextField() noexcept
{
    SecureClearStorage();
}

void TextField::SecureClearStorage() noexcept
{
    // Only the destructor clears storage. Each shared buffer is wiped when its last owner (this field, its history, the host
    // cache or a TSF store) releases it. The field keeps the shared empty buffer, so code that still reads it while members are
    // destroyed (a callback capture's destructor) reads empty text.
    _text = EmptySharedText();
    for (EditHistoryState& state : _undoHistory)
    {
        state.text.reset();
    }
    for (EditHistoryState& state : _redoHistory)
    {
        state.text.reset();
    }
    // The multiline layout cache keeps no copy of the text; the single-line one is wiped on destruction whatever the mask.
    ClearSingleLineTextLayoutCache(_singleLineLayoutCache, true);
    _maskedSourceTextElementBoundaries.clear();
    _undoHistory.clear();
    _redoHistory.clear();
}

void TextField::RefreshAccessibilitySnapshot() const noexcept
{
    if (ControlHost* const host = GetHost())
    {
        RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
    }
}

void TextField::SetText(std::wstring text)
{
    const auto lifetime   = GetLifetimeToken();
    _pendingLeadSurrogate = 0;
    _text                 = MakeSharedText(std::move(text));
    if (_multiline)
        InvalidateMeasure();
    if (lifetime.expired())
        return;
    _caretIndex = std::min(_caretIndex, _text->size());
    _selectionAnchorIndex.reset();
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    _preferredMultilineXOffsetDip.reset();
    // A focused field restarts its blink now, with the caret shown, and sleeps until the first flip.
    if (ControlHost* const caretHost = GetHost(); caretHost && HasFocus())
        ResetCaretBlink(*caretHost);
    _horizontalScrollDip          = 0.0f;
    _multilineFirstVisibleLine    = 0u;
    _multilineWheelDeltaRemainder = 0.0f;
    _dragSelecting                = false;
    _undoHistory.clear();
    _redoHistory.clear();
    BreakDirectEditMerge();
    RegenerateConcealedMaskEpoch();
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        host->SyncTextInput(this);
        if (lifetime.expired())
            return;
    }
    RefreshAccessibilitySnapshot();
    if (lifetime.expired())
        return;
    RequestInvalidate();
    if (lifetime.expired())
        return;
}

std::wstring_view TextField::GetText() const noexcept
{
    return *_text;
}

void TextField::SetFontRole(const FontRole fontRole) noexcept
{
    if (static_cast<uint8_t>(fontRole) > static_cast<uint8_t>(FontRole::IconLarge))
        return;
    // The font sets the text's geometry, which UI Automation's text ranges report.
    Assign(_fontRole,
           fontRole,
           Affects::Layout | Affects::Paint | Affects::SemanticValue,
           [this]() noexcept
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
        _preferredMultilineXOffsetDip.reset();
        if (ControlHost* const host = GetHost())
        {
            if (_multiline)
                EnsureMultilineCaretVisible(host);
            else
                EnsureCaretVisible(host, std::max(1.0f, GetTextRect().right - GetTextRect().left));
            if (HasFocus())
                host->SyncTextInput(this);
            if (lifetime.expired() || GetHost() != host)
                return;
            host->NotifyTextInputLayoutChanged(this);
        }
    });
}

FontRole TextField::GetFontRole() const noexcept
{
    return _fontRole;
}

size_t TextField::GetCaretIndex() const noexcept
{
    return _caretIndex;
}

void TextField::SetSelectionRange(const size_t selectionStart, const size_t selectionEnd) noexcept
{
    const auto lifetime   = GetLifetimeToken();
    _pendingLeadSurrogate = 0;
    BreakDirectEditMerge();
    const size_t clampedStart = std::min(selectionStart, _text->size());
    const size_t clampedEnd   = std::min(selectionEnd, _text->size());
    _preferredMultilineXOffsetDip.reset();
    _dragSelecting = false;
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    if (clampedStart == clampedEnd)
    {
        _selectionAnchorIndex.reset();
        _caretIndex = clampedEnd;
    }
    else
    {
        _selectionAnchorIndex = clampedStart;
        _caretIndex           = clampedEnd;
    }
    // A focused field restarts its blink now, with the caret shown, and sleeps until the first flip.
    if (ControlHost* const caretHost = GetHost(); caretHost && HasFocus())
        ResetCaretBlink(*caretHost);
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        host->SyncTextInput(this);
        if (lifetime.expired())
            return;
    }
    RefreshAccessibilitySnapshot();
    if (lifetime.expired())
        return;
    RequestInvalidate();
    if (lifetime.expired())
        return;
}

void TextField::ReplaceSelectionAndNotify(std::wstring_view replacement)
{
    const Failure::Scope failureScope;
    const auto lifetime   = GetLifetimeToken();
    const bool willMutate = HasSelection() || ! replacement.empty();
    if (! willMutate)
    {
        return;
    }

    RecordUndoStateForDirectEdit();
    _preferredMultilineXOffsetDip.reset();
    static_cast<void>(DeleteSelection());
    MutableText(_text->size() + replacement.size()).insert(_caretIndex, replacement.data(), replacement.size());
    _caretIndex += replacement.size();
    _selectionAnchorIndex.reset();
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    // A focused field restarts its blink now, with the caret shown, and sleeps until the first flip.
    if (ControlHost* const caretHost = GetHost(); caretHost && HasFocus())
        ResetCaretBlink(*caretHost);
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    if (auto* host = GetHost(); host && host->GetFocusControl() == this)
    {
        host->SyncTextInput(this);
        if (lifetime.expired())
            return;
    }
    RefreshAccessibilitySnapshot();
    if (lifetime.expired())
        return;
    RequestInvalidate();
    if (lifetime.expired())
        return;
    static_cast<void>(NotifyChanged());
}

void TextField::SetTextAndNotify(std::wstring text)
{
    const Failure::Scope failureScope;
    const auto lifetime = GetLifetimeToken();
    SetText(std::move(text));
    if (lifetime.expired())
        return;
    static_cast<void>(NotifyChanged());
}

void TextField::SetMasked(bool masked) noexcept
{
    // A masked field is a password field without a text pattern or value (UI Automation).
    Assign(_masked,
           masked,
           Affects::Layout | Affects::Paint | Affects::SemanticState,
           [this]() noexcept
    {
        RegenerateConcealedMaskEpoch();
        // Masking or unmasking changes the shown text: the layouts of the old one, plaintext included, are released now.
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
        if (ControlHost* const host = GetHost(); host && HasFocus())
            host->SyncTextInput(this);
    });
}

bool TextField::IsMasked() const noexcept
{
    return _masked;
}

void TextField::SetInputScope(TextInputScope scope) noexcept
{
    // Read by the text services when the field next takes or synchronizes input.
    Assign(_inputScope, scope, Affects::None);
}

TextInputScope TextField::GetInputScope() const noexcept
{
    return _inputScope;
}

void TextField::SetPasswordRevealMode(PasswordRevealMode mode) noexcept
{
    // The mode reserves the reveal button (measure) and shows it to UI Automation.
    Assign(_passwordRevealMode,
           mode,
           Affects::Layout | Affects::Paint | Affects::SemanticState,
           [this]() noexcept
    {
        const auto lifetime            = GetLifetimeToken();
        _passwordRevealButtonHovered   = false;
        _passwordRevealButtonPressed   = false;
        _passwordRevealKeyboardFocused = false;

        if (_passwordRevealMode == PasswordRevealMode::Visible)
        {
            SetPasswordRevealState(PasswordRevealState::Visible);
        }
        else if (_passwordRevealMode == PasswordRevealMode::Hidden)
        {
            SetPasswordRevealState(PasswordRevealState::Hidden);
        }
        else if (_passwordRevealState == PasswordRevealState::Visible)
        {
            SetPasswordRevealState(PasswordRevealState::Hidden);
        }

        if (! lifetime.expired())
            InvalidateSingleLineLayoutCache();
    });
}

PasswordRevealMode TextField::GetPasswordRevealMode() const noexcept
{
    return _passwordRevealMode;
}

void TextField::SetPasswordRevealState(PasswordRevealState state) noexcept
{
    if (_passwordRevealMode == PasswordRevealMode::Visible)
    {
        state = PasswordRevealState::Visible;
    }
    else if (_passwordRevealMode == PasswordRevealMode::Hidden)
    {
        state = PasswordRevealState::Hidden;
    }

    const auto startedAt = std::chrono::steady_clock::now();
    const bool masked    = _masked;
    if (! Assign(_passwordRevealState,
                 state,
                 Affects::Paint | Affects::SemanticState,
                 [this]() noexcept
    {
        // A masked multiline field measures its displayed text: the mask or the revealed value.
        if (_multiline && _masked)
            InvalidateMeasure();
        // Revealing or concealing changes the shown text: a layout of the plaintext is released the moment it is hidden again.
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
    }))
        return;
    if (Debug::Perf::IsCaptureEnabled())
    {
        Debug::Perf::Emit(
            L"dxui.textinput.reveal_toggle_us", L"textfield", Debug::Perf::ElapsedUs(startedAt), static_cast<uint64_t>(state), masked ? 1u : 0u, S_OK);
    }
}

PasswordRevealState TextField::GetPasswordRevealState() const noexcept
{
    return _passwordRevealState;
}

void TextField::SetPasswordMaskLengthPolicy(PasswordMaskLengthPolicy policy) noexcept
{
    Assign(_maskLengthPolicy,
           policy,
           Affects::Paint,
           [this]() noexcept
    {
        // The policy sets how many dots a masked multiline field displays, and so measures.
        if (_multiline && _masked)
            InvalidateMeasure();
        RegenerateConcealedMaskEpoch();
        InvalidateSingleLineLayoutCache();
    });
}

PasswordMaskLengthPolicy TextField::GetPasswordMaskLengthPolicy() const noexcept
{
    return _maskLengthPolicy;
}

size_t TextField::GetSecretVisibleDotCount() const noexcept
{
    const auto startedAt = std::chrono::steady_clock::now();
    size_t result        = 0u;
    size_t exactCount    = 0u;
    if (! _masked || _text->empty() || _passwordRevealMode == PasswordRevealMode::Visible || _passwordRevealState == PasswordRevealState::Visible)
    {
        return result;
    }

    if (_maskLengthPolicy == PasswordMaskLengthPolicy::Exact)
    {
        exactCount = GetMaskedSourceTextElementBoundaries().size() - 1u;
        result     = exactCount;
    }
    else
    {
        exactCount = _text->size();
        result     = GetConcealedMaskVisibleDotCount(exactCount);
    }

    if (Debug::Perf::IsCaptureEnabled())
    {
        const std::wstring_view policyDetail = _maskLengthPolicy == PasswordMaskLengthPolicy::Exact ? L"exact" : L"concealed";
        Debug::Perf::Emit(L"dxui.textinput.secret_render_us", policyDetail, Debug::Perf::ElapsedUs(startedAt), _text->size(), result, S_OK);
        Debug::Perf::Emit(L"dxui.textinput.secret_display_dot_count", policyDetail, 0u, result, exactCount, S_OK);
    }
    return result;
}

void TextField::SetPasswordRevealAccessibleName(std::wstring name)
{
    // The name UI Automation reports for the reveal button while it is shown.
    Assign(_passwordRevealAccessibleName, std::move(name), Affects::SemanticName);
}

std::wstring_view TextField::GetPasswordRevealAccessibleName() const noexcept
{
    return _passwordRevealAccessibleName;
}

bool TextField::IsPasswordRevealButtonVisibleForAccessibility() const noexcept
{
    return IsPasswordRevealButtonVisible();
}

D2D1_RECT_F TextField::GetPasswordRevealButtonAccessibilityRect() const noexcept
{
    return GetPasswordRevealButtonRect();
}

bool TextField::InvokePasswordRevealButton(ControlHost& host)
{
    if (! IsPasswordRevealButtonVisible())
    {
        return false;
    }

    _passwordRevealButtonHovered   = false;
    _passwordRevealButtonPressed   = false;
    _passwordRevealKeyboardFocused = false;
    if (_passwordRevealState == PasswordRevealState::Visible)
    {
        RemaskPasswordReveal();
    }
    else
    {
        SetPasswordRevealState(PasswordRevealState::Visible);
    }
    ResetCaretBlink(host);
    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    host.SyncTextInput(this);
    Invalidate(host);
    return true;
}

void TextField::SetPlaceholder(std::wstring text)
{
    // Painted while the field is empty.
    Assign(_placeholder, std::move(text), Affects::Paint);
}

void TextField::SetMultiline(bool multiline) noexcept
{
    // A multiline field measures its text, paints lines and has no Value pattern (UI Automation).
    Assign(_multiline,
           multiline,
           Affects::Layout | Affects::Paint | Affects::SemanticState,
           [this]() noexcept
    {
        _preferredMultilineXOffsetDip.reset();
        _multilineFirstVisibleLine    = 0u;
        _multilineWheelDeltaRemainder = 0.0f;
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
    });
}

bool TextField::IsMultiline() const noexcept
{
    return _multiline;
}

void TextField::SetClearButtonEnabled(bool enabled) noexcept
{
    // The measure reserves the button; paint shows it.
    Assign(_clearButtonEnabled,
           enabled,
           Affects::Layout | Affects::Paint,
           [this, enabled]() noexcept
    {
        if (! enabled)
        {
            _clearButtonHovered = false;
        }
    });
}

bool TextField::IsClearButtonEnabled() const noexcept
{
    return _clearButtonEnabled;
}

void TextField::SetCaretColor(std::optional<D2D1_COLOR_F> caretColor) noexcept
{
    Assign(_caretColorOverride, caretColor, Affects::Paint);
}

void TextField::SetHorizontalTextPadding(float leftDip, float rightDip) noexcept
{
    Assign(std::tie(_hasExplicitHorizontalTextPadding, _textPaddingLeftDip, _textPaddingRightDip),
           std::tuple<bool, float, float>(true, std::max(0.0f, leftDip), std::max(0.0f, rightDip)),
           Affects::Layout | Affects::Paint,
           [this]() noexcept
    {
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
        if (auto* host = GetHost(); host && host->GetFocusControl() == this)
            host->SyncTextInput(this);
    });
}

void TextField::SetVerticalTextPadding(float topDip, float bottomDip) noexcept
{
    Assign(std::tie(_hasExplicitVerticalTextPadding, _textPaddingTopDip, _textPaddingBottomDip),
           std::tuple<bool, float, float>(true, std::max(0.0f, topDip), std::max(0.0f, bottomDip)),
           Affects::Layout | Affects::Paint,
           [this]() noexcept
    {
        InvalidateSingleLineLayoutCache();
        InvalidateMultilineLayoutCache();
        if (auto* host = GetHost(); host && host->GetFocusControl() == this)
            host->SyncTextInput(this);
    });
}

void TextField::SetReadOnly(bool readOnly) noexcept
{
    _pendingLeadSurrogate = 0;
    // UI Automation reports a read-only value.
    Assign(_readOnly,
           readOnly,
           Affects::Paint | Affects::SemanticState,
           [this]() noexcept
    {
        const auto lifetime = GetLifetimeToken();
        if (_readOnly)
        {
            _passwordRevealButtonHovered   = false;
            _passwordRevealButtonPressed   = false;
            _passwordRevealKeyboardFocused = false;
            RemaskPasswordReveal();
            if (lifetime.expired())
                return;
        }
        if (ControlHost* const host = GetHost(); host && HasFocus())
        {
            // An editable caret blinks and a read-only one is steady: the caret restarts in its new mode.
            ResetCaretBlink(*host);
            host->SyncTextInput(this);
        }
    });
}

bool TextField::IsReadOnly() const noexcept
{
    return _readOnly;
}

void TextField::SetOnTextChanged(std::function<void(std::wstring_view)> onTextChanged)
{
    ReplaceControlCallback(_onTextChanged, std::move(onTextChanged));
}

void TextField::SetOnSubmitted(std::function<void()> onSubmitted)
{
    ReplaceControlCallback(_onSubmitted, std::move(onSubmitted));
}

void TextField::SetOnPreviewKeyDown(std::function<bool(ControlHost& host, UINT virtualKey, UINT modifiers)> onPreviewKeyDown)
{
    ReplaceControlCallback(_onPreviewKeyDown, std::move(onPreviewKeyDown));
}

void TextField::SetOnBlur(std::function<void()> onBlur)
{
    ReplaceControlCallback(_onBlur, std::move(onBlur));
}

bool TextField::DebugGetMultilineState(const ControlHost& host, TextFieldDebugMultilineState& out) const noexcept
{
    out = {};
    if (! _multiline)
    {
        return false;
    }

    const D2D1_RECT_F textRect         = GetTextRect();
    const float viewportHeightDip      = std::max(1.0f, textRect.bottom - textRect.top);
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    const auto multilineLayout = CreateMultilineTextLayout(&host, displayText, _fontRole, std::max(1.0f, textRect.right - textRect.left), viewportHeightDip);
    const std::vector<DWRITE_LINE_METRICS> lineMetrics = GetMultilineLineMetrics(multilineLayout.get());
    const float fallbackLineHeightDip                  = EstimateMultilineFallbackLineHeightDip(&host, _fontRole);
    const MultilineViewportMetrics viewportMetrics     = BuildMultilineViewportMetrics(displayText, viewportHeightDip, lineMetrics, fallbackLineHeightDip);
    const size_t clampedFirstVisibleLine               = ClampMultilineFirstVisibleLine(_multilineFirstVisibleLine, viewportMetrics);

    out.firstVisibleLine                                          = clampedFirstVisibleLine;
    out.visibleLineCount                                          = viewportMetrics.visibleLineCount;
    out.totalLineCount                                            = viewportMetrics.totalLineCount;
    out.canScrollVertically                                       = viewportMetrics.totalLineCount > viewportMetrics.visibleLineCount;
    out.cachedLayoutPresent                                       = static_cast<bool>(_cachedMultilineLayout);
    out.layoutDirty                                               = _multilineLayoutDirty;
    out.cachedLayoutWidthDip                                      = _cachedLayoutSize.width;
    out.cachedLayoutHeightDip                                     = _cachedLayoutSize.height;
    out.selectionLayoutWidthDip                                   = MultilinePaintLayoutWidthDip(textRect);
    const std::optional<std::pair<size_t, size_t>> selectionRange = GetSelectionRange();
    const std::optional<std::pair<size_t, size_t>> displayRange =
        selectionRange.has_value() ? ControlTextRangeToDisplayTextRange(selectionRange->first, selectionRange->second) : std::nullopt;
    if (displayRange && displayRange->first < displayRange->second)
    {
        const auto selectionLayout = CreateMultilineTextLayout(&host, displayText, _fontRole, out.selectionLayoutWidthDip, viewportHeightDip);
        UINT32 count               = 0u;
        const UINT32 first = static_cast<UINT32>(displayRange->first), length = static_cast<UINT32>(displayRange->second - displayRange->first);
        if (selectionLayout &&
            selectionLayout->HitTestTextRange(first, length, 0.0f, 0.0f, nullptr, 0u, &count) == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
        {
            std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
            if (SUCCEEDED(selectionLayout->HitTestTextRange(first, length, 0.0f, 0.0f, metrics.data(), count, &count)))
                for (UINT32 index = 0u; index < count; ++index)
                    out.selectionRects.push_back(D2D1::RectF(
                        metrics[index].left, metrics[index].top, metrics[index].left + metrics[index].width, metrics[index].top + metrics[index].height));
        }
    }
    return true;
}

bool TextField::DebugGetSingleLinePaintState(const ControlHost& host, TextFieldDebugSingleLinePaintState& out) const noexcept
{
    out = {};
    if (_multiline)
    {
        return false;
    }

    const D2D1_RECT_F textRect = GetTextRect();
    EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));

    out.textRect            = SnapRectToPixel(host, textRect);
    out.horizontalScrollDip = _horizontalScrollDip;
    if (IsClearButtonVisible())
    {
        out.trailingButtonRect    = SnapRectToPixel(host, GetClearButtonRect());
        out.hasTrailingButtonRect = true;
    }
    else if (IsPasswordRevealButtonVisible())
    {
        out.trailingButtonRect    = SnapRectToPixel(host, GetPasswordRevealButtonRect());
        out.hasTrailingButtonRect = true;
    }
    const DWRITE_READING_DIRECTION readingDirection               = ResolveReadingDirection(GetFlowDirection());
    const std::optional<std::pair<size_t, size_t>> selectionRange = GetSelectionRange();
    const std::optional<std::pair<size_t, size_t>> displaySelectionRange =
        selectionRange.has_value() ? ControlTextRangeToDisplayTextRange(selectionRange.value().first, selectionRange.value().second) : std::nullopt;
    if (const std::optional<D2D1_RECT_F> selectionPaintRect =
            ComputeSingleLineSelectionPaintRect(host, *GetDisplayText(), textRect, _fontRole, _horizontalScrollDip, displaySelectionRange, readingDirection);
        selectionPaintRect.has_value())
    {
        out.selectionPaintRect    = selectionPaintRect.value();
        out.hasSelectionPaintRect = true;
    }
    out.compositionUnderlineRects      = BuildNativeCompositionUnderlineRects(host, *this, false);
    out.conversionTargetUnderlineRects = BuildNativeCompositionUnderlineRects(host, *this, true);
    return true;
}

bool TextField::DebugIsCaretVisible() const noexcept
{
    const ControlHost* const host = GetHost();
    return ! host || CaretClockAccess::Shown(*host, *this);
}

bool TextField::DebugGetCaretRect(const ControlHost& host, size_t controlTextIndex, D2D1_RECT_F& outRect) const noexcept
{
    outRect                                    = {};
    const std::optional<D2D1_RECT_F> caretRect = GetTextInputCaretRect(host, controlTextIndex);
    if (! caretRect.has_value())
    {
        return false;
    }

    outRect = caretRect.value();
    return true;
}

void TextField::OnBoundsChanged() noexcept
{
    InvalidateSingleLineLayoutCache();
    {
        // A move keeps the multiline layout: only the width and the layout height its key holds change what it lays out.
        const D2D1_RECT_F movedTextRect = GetTextRect();
        const float widthDip            = MultilinePaintLayoutWidthDip(movedTextRect);
        const float heightDip           = std::max(kMultilineLayoutHeightDip, std::max(1.0f, movedTextRect.bottom - movedTextRect.top));
        if (_cachedLayoutSize.width != widthDip || _cachedLayoutSize.height != heightDip)
            InvalidateMultilineLayoutCache();
    }
    if (! _multiline)
    {
        _horizontalScrollDip = 0.0f;
        if (ControlHost* const host = GetHost(); host && HasFocus())
        {
            host->SyncTextInput(this);
        }
        return;
    }

    ControlHost* const host = GetHost();
    if (! host)
    {
        _multilineFirstVisibleLine    = 0u;
        _multilineWheelDeltaRemainder = 0.0f;
        return;
    }

    const D2D1_RECT_F textRect         = GetTextRect();
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    if (displayText.empty())
    {
        _multilineFirstVisibleLine    = 0u;
        _multilineWheelDeltaRemainder = 0.0f;
        return;
    }

    const float viewportHeightDip = std::max(1.0f, textRect.bottom - textRect.top);
    static_cast<void>(GetOrCreateMultilineLayout(host, displayTextBuffer, textRect));
    const std::vector<DWRITE_LINE_METRICS>& lineMetrics = _cachedMultilineLineMetrics;
    const float fallbackLineHeightDip                   = lineMetrics.empty() ? GetMultilineFallbackLineHeightDip(host) : 0.0f;
    const MultilineViewportMetrics viewportMetrics      = BuildMultilineViewportMetrics(displayText, viewportHeightDip, lineMetrics, fallbackLineHeightDip);
    _multilineFirstVisibleLine                          = ClampMultilineFirstVisibleLine(_multilineFirstVisibleLine, viewportMetrics);
    if (HasFocus() && ! _readOnly)
    {
        EnsureMultilineCaretVisible(host);
        host->SyncTextInput(this);
    }
}

void TextField::Paint(ControlHost& host) const
{
    const TextFieldVisualStyle style =
        ResolveTextFieldVisualStyle(host.GetTheme(), IsEnabled(), IsHovered(), HasFocus(), HasFocus() && host.IsKeyboardFocusVisible(), _caretColorOverride);
    constexpr float kTextFieldCornerRadiusDip = 4.0f;
    DrawRoundedRect(host, GetBounds(), style.fill, style.border, kTextFieldCornerRadiusDip);
    if (style.showFocus)
    {
        // WinUI accent bottom border: 2px accent line at bottom of control when focused
        // Inset horizontally by corner radius to avoid clipping into rounded corners
        auto* dc = host.GetDeviceContext();
        if (auto* brush = dc ? host.GetSolidBrush(host.GetTheme().accent) : nullptr)
        {
            const D2D1_RECT_F bounds = GetBounds();
            const D2D1_RECT_F accentBar =
                D2D1::RectF(bounds.left + kTextFieldCornerRadiusDip, bounds.bottom - 2.0f, bounds.right - kTextFieldCornerRadiusDip, bounds.bottom);
            dc->FillRectangle(&accentBar, brush);
        }
    }

    const D2D1_RECT_F textRect                                    = GetTextRect();
    const SharedText displayTextBuffer                            = GetDisplayText();
    const std::wstring& displayText                               = *displayTextBuffer;
    const bool usePlaceholder                                     = _text->empty() && ! _placeholder.empty();
    const DWRITE_READING_DIRECTION readingDirection               = ResolveReadingDirection(GetFlowDirection());
    const std::optional<std::pair<size_t, size_t>> selectionRange = GetSelectionRange();
    const std::optional<std::pair<size_t, size_t>> displaySelectionRange =
        selectionRange.has_value() ? ControlTextRangeToDisplayTextRange(selectionRange.value().first, selectionRange.value().second) : std::nullopt;
    wil::com_ptr<IDWriteTextLayout> paintSingleLineLayout;
    if (_multiline)
    {
        const auto multilineLayout     = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const float multilineScrollDip = MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine);
        DrawMultilineSelection(host,
                               multilineLayout.get(),
                               displayText,
                               textRect,
                               _fontRole,
                               style.text,
                               style.selectionFill,
                               style.selectionText,
                               multilineScrollDip,
                               displaySelectionRange);
    }
    else if (usePlaceholder)
    {
        DrawSingleLineTextClipped(host, std::wstring_view(_placeholder), textRect, _fontRole, style.placeholderText, 0.0f, readingDirection);
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
        paintSingleLineLayout = GetOrCreateSingleLineLayout(&host,
                                                            displayText,
                                                            std::max(1.0f, textRect.right - textRect.left + _horizontalScrollDip),
                                                            std::max(1.0f, textRect.bottom - textRect.top),
                                                            readingDirection);
        DrawSingleLineSelectionWithLayout(host,
                                          displayText,
                                          textRect,
                                          _fontRole,
                                          style.text,
                                          style.selectionFill,
                                          style.selectionText,
                                          _horizontalScrollDip,
                                          displaySelectionRange,
                                          readingDirection,
                                          paintSingleLineLayout.get());
    }

    if (! usePlaceholder)
    {
        DrawNativeCompositionUnderlines(host, *this, style.text, host.GetTheme().accent);
    }

    if (HasFocus() && CaretClockAccess::Shown(host, *this))
    {
        D2D1_RECT_F caretRect = D2D1::RectF();
        if (_multiline)
        {
            const auto multilineLayout     = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
            const float multilineScrollDip = MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine);
            caretRect =
                MeasureMultilineCaretRectDip(multilineLayout.get(), displayText, textRect, multilineScrollDip, ControlTextIndexToDisplayTextIndex(_caretIndex));
        }
        else
        {
            if (! paintSingleLineLayout)
            {
                paintSingleLineLayout = GetOrCreateSingleLineLayout(&host,
                                                                    displayText,
                                                                    std::max(1.0f, textRect.right - textRect.left + _horizontalScrollDip),
                                                                    std::max(1.0f, textRect.bottom - textRect.top),
                                                                    readingDirection);
            }
            const float caretOffset = MeasureCaretOffsetDip(paintSingleLineLayout.get(), displayText, ControlTextIndexToDisplayTextIndex(_caretIndex));
            const float caretMaxX   = std::max(textRect.left, textRect.right - 1.0f);
            const float caretX      = std::clamp(textRect.left + caretOffset - _horizontalScrollDip, textRect.left, caretMaxX);
            caretRect               = D2D1::RectF(caretX, textRect.top + 2.0f, caretX + 1.0f, textRect.bottom - 2.0f);
        }
        if (auto* dc = host.GetDeviceContext())
        {
            if (auto* brush = host.GetSolidBrush(style.caret))
            {
                const D2D1_RECT_F snappedCaretRect = SnapRectToPixel(host, caretRect);
                // A multiline caret on a partly visible line, or scrolled out by the wheel, reaches past the text viewport;
                // only its part inside the viewport is drawn.
                if (_multiline)
                    dc->PushAxisAlignedClip(SnapRectToPixel(host, textRect), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                dc->DrawLine(
                    D2D1::Point2F(snappedCaretRect.left, snappedCaretRect.top), D2D1::Point2F(snappedCaretRect.left, snappedCaretRect.bottom), brush, 1.0f);
                if (_multiline)
                    dc->PopAxisAlignedClip();
            }
        }
    }

    if (IsPasswordRevealButtonVisible())
    {
        const D2D1_RECT_F revealRect = GetPasswordRevealButtonRect();
        if (_passwordRevealButtonHovered || _passwordRevealButtonPressed || _passwordRevealKeyboardFocused)
        {
            const D2D1_ROUNDED_RECT hoverBg =
                D2D1::RoundedRect(D2D1::RectF(revealRect.left + 4.0f, revealRect.top + 4.0f, revealRect.right - 4.0f, revealRect.bottom - 4.0f), 4.0f, 4.0f);
            auto* dc = host.GetDeviceContext();
            if (auto* brush = dc ? host.GetSolidBrush(_passwordRevealButtonPressed ? host.GetTheme().pressedFill : host.GetTheme().hoverFill) : nullptr)
            {
                dc->FillRoundedRectangle(&hoverBg, brush);
            }
        }
        DrawCenteredText(host, L"\xE7B3", revealRect, FontRole::Icon, style.text);
        if (_passwordRevealKeyboardFocused)
        {
            PaintFocusRing(host, D2D1::RectF(revealRect.left + 3.0f, revealRect.top + 3.0f, revealRect.right - 3.0f, revealRect.bottom - 3.0f), 4.0f);
        }
    }
    if (IsClearButtonVisible())
    {
        const D2D1_RECT_F clearRect = GetClearButtonRect();
        if (_clearButtonHovered)
        {
            const D2D1_ROUNDED_RECT hoverBg =
                D2D1::RoundedRect(D2D1::RectF(clearRect.left + 4.0f, clearRect.top + 4.0f, clearRect.right - 4.0f, clearRect.bottom - 4.0f), 4.0f, 4.0f);
            auto* dc = host.GetDeviceContext();
            if (auto* brush = dc ? host.GetSolidBrush(host.GetTheme().hoverFill) : nullptr)
            {
                dc->FillRoundedRectangle(&hoverBg, brush);
            }
        }
        DrawCenteredText(host, L"\xE711", clearRect, FontRole::Icon, style.text);
    }
}

bool TextField::Tick(ControlHost& host, uint64_t nowTickMs)
{
    // The caret is the field's only tick: the host's caret clock flips its phase and arms the next flip.
    if (HasFocus() && CaretClockAccess::Tick(host, *this, nowTickMs))
    {
        // Only a blink-phase flip changes pixels; a tick within one phase leaves the surface clean.
        Invalidate(host);
    }
    return false;
}

void TextField::OnFocusChanged(ControlHost& host, bool focused)
{
    const Failure::Scope failureScope;
    _pendingLeadSurrogate = 0;
    Control::OnFocusChanged(host, focused);
    if (focused)
    {
        RegenerateConcealedMaskEpoch();
        // A new epoch can change how many dots a concealed multiline mask displays, which its measurement reads.
        if (_multiline && UsesMaskedDisplayText() && _maskLengthPolicy != PasswordMaskLengthPolicy::Exact)
        {
            const std::weak_ptr<int> lifetime = GetLifetimeToken();
            InvalidateMeasure();
            if (lifetime.expired())
                return;
        }
        if (_multiline)
        {
            if (! _readOnly)
            {
                EnsureMultilineCaretVisible(&host);
            }
        }
        ResetCaretBlink(host);
    }
    else
    {
        RemaskPasswordReveal();
        BreakDirectEditMerge();
        // The caret is the field's only tick: an unfocused field waits for nothing.
        CaretClockAccess::Stop(host, *this);
        _dragSelecting = false;
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        _preferredMultilineXOffsetDip.reset();
        _multilineWheelDeltaRemainder  = 0.0f;
        _passwordRevealButtonHovered   = false;
        _passwordRevealButtonPressed   = false;
        _passwordRevealKeyboardFocused = false;
        if (_onBlur)
        {
            const std::weak_ptr<int> lifetime = GetLifetimeToken();
            auto onBlur                       = _onBlur;
            (*onBlur)();
            onBlur.reset();
            if (lifetime.expired())
            {
                return;
            }
        }
        Invalidate(host);
    }
}

void TextField::OnEnabledChanged(bool enabled) noexcept
{
    if (! enabled)
    {
        _passwordRevealButtonHovered   = false;
        _passwordRevealButtonPressed   = false;
        _passwordRevealKeyboardFocused = false;
        RemaskPasswordReveal();
    }
    // A focused field disabled or enabled again: its caret stops blinking, or blinks again from now.
    if (ControlHost* const host = GetHost(); host && HasFocus())
        ResetCaretBlink(*host);
}

WindowHostCursorKind TextField::ResolveCursorKind(ControlHost&, D2D1_POINT_2F point) const noexcept
{
    if (! IsEnabled() || ! IsVisible() || (IsClearButtonVisible() && PointInRect(GetClearButtonRect(), point)) ||
        (IsPasswordRevealButtonVisible() && PointInRect(GetPasswordRevealButtonRect(), point)))
        return WindowHostCursorKind::Default;
    return PointInRect(GetTextRect(), point) ? WindowHostCursorKind::IBeam : WindowHostCursorKind::Default;
}

bool TextField::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const Failure::Scope failureScope;
    _pendingLeadSurrogate = 0;
    if (rightButton)
    {
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        return OnContextMenu(host, false, point);
    }

    if (IsPasswordRevealButtonVisible() && PointInRect(GetPasswordRevealButtonRect(), point))
    {
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        _dragSelecting                 = false;
        _passwordRevealKeyboardFocused = false;
        _passwordRevealButtonHovered   = true;
        _passwordRevealButtonPressed   = true;
        SetPasswordRevealState(PasswordRevealState::Visible);
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
        return true;
    }

    if (IsClearButtonVisible() && PointInRect(GetClearButtonRect(), point))
    {
        const auto lifetime = GetLifetimeToken();
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        _passwordRevealKeyboardFocused = false;
        SetText({});
        if (lifetime.expired())
            return true;
        if (! NotifyChanged())
        {
            return true;
        }
        ResetCaretBlink(host);
        if (lifetime.expired())
            return true;
        Invalidate(host);
        return true;
    }

    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    _passwordRevealKeyboardFocused = false;
    if (_masked && _maskLengthPolicy == PasswordMaskLengthPolicy::Concealed && _passwordRevealState != PasswordRevealState::Visible && ! _multiline)
    {
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        SetCaretIndex(_text->size(), false);
        _dragSelecting             = false;
        const D2D1_RECT_F textRect = GetTextRect();
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
        return true;
    }

    if (! _multiline && ! ModifiersContainShift(modifiers) && ShouldPromoteSingleLineClickToSelectAll(host, _selectionClickSequence, point))
    {
        ResetSingleLineSelectionClickSequence(_selectionClickSequence);
        SelectAllText();
        _dragSelecting             = false;
        const D2D1_RECT_F textRect = GetTextRect();
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
        return true;
    }

    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    if (_multiline)
    {
        const D2D1_RECT_F textRect         = GetTextRect();
        const SharedText displayTextBuffer = GetDisplayText();
        const std::wstring& displayText    = *displayTextBuffer;
        const auto multilineLayout         = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const size_t displayHitIndex       = HitTestMultilineCaretIndexDip(
            multilineLayout.get(), displayText, textRect, MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine), point);
        const size_t hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
        _preferredMultilineXOffsetDip.reset();
        if (ModifiersContainShift(modifiers))
        {
            SetCaretIndex(hitIndex, true);
        }
        else
        {
            BreakDirectEditMerge();
            _selectionAnchorIndex = hitIndex;
            _caretIndex           = hitIndex;
        }
        _dragSelecting = true;
        EnsureMultilineCaretVisible(&host);
    }
    else
    {
        const D2D1_RECT_F textRect         = GetTextRect();
        const SharedText displayTextBuffer = GetDisplayText();
        const std::wstring& displayText    = *displayTextBuffer;
        const size_t displayHitIndex =
            HitTestCaretIndexDip(&host, displayText, _fontRole, textRect, _horizontalScrollDip, point, ResolveReadingDirection(GetFlowDirection()));
        const size_t hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
        _preferredMultilineXOffsetDip.reset();
        if (ModifiersContainShift(modifiers))
        {
            SetCaretIndex(hitIndex, true);
        }
        else
        {
            BreakDirectEditMerge();
            _selectionAnchorIndex = hitIndex;
            _caretIndex           = hitIndex;
        }
        _dragSelecting = true;
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
    }
    ResetCaretBlink(host);
    host.SyncTextInput(this);
    Invalidate(host);
    return true;
}

bool TextField::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const Failure::Scope failureScope;
    _pendingLeadSurrogate = 0;
    if (rightButton)
    {
        return OnMouseDown(host, point, rightButton, modifiers);
    }

    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    const D2D1_RECT_F textRect         = GetTextRect();
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    size_t hitIndex                    = 0u;
    if (_multiline)
    {
        const auto multilineLayout   = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const size_t displayHitIndex = HitTestMultilineCaretIndexDip(
            multilineLayout.get(), displayText, textRect, MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine), point);
        hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
    }
    else
    {
        const size_t displayHitIndex =
            HitTestCaretIndexDip(&host, displayText, _fontRole, textRect, _horizontalScrollDip, point, ResolveReadingDirection(GetFlowDirection()));
        hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
    }
    _preferredMultilineXOffsetDip.reset();
    SelectWordAt(hitIndex);
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    if (! _multiline)
    {
        ArmSingleLineSelectionClickSequence(_selectionClickSequence, point);
    }
    _dragSelecting = false;
    if (_multiline)
    {
        EnsureMultilineCaretVisible(&host);
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
    }
    ResetCaretBlink(host);
    host.SyncTextInput(this);
    Invalidate(host);
    return true;
}

bool TextField::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    const Failure::Scope failureScope;
    const bool wasClearHovered   = _clearButtonHovered;
    const bool wasRevealHovered  = _passwordRevealButtonHovered;
    _clearButtonHovered          = IsClearButtonVisible() && PointInRect(GetClearButtonRect(), point);
    _passwordRevealButtonHovered = IsPasswordRevealButtonVisible() && PointInRect(GetPasswordRevealButtonRect(), point);
    if (_clearButtonHovered != wasClearHovered || _passwordRevealButtonHovered != wasRevealHovered)
    {
        Invalidate(host);
    }

    if (! _dragSelecting)
    {
        return _clearButtonHovered || _passwordRevealButtonHovered || _passwordRevealButtonPressed;
    }

    const D2D1_RECT_F textRect         = GetTextRect();
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    size_t hitIndex                    = 0u;
    if (_multiline)
    {
        const auto multilineLayout   = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const size_t displayHitIndex = HitTestMultilineCaretIndexDip(
            multilineLayout.get(), displayText, textRect, MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine), point);
        hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
    }
    else
    {
        const size_t displayHitIndex =
            HitTestCaretIndexDip(&host, displayText, _fontRole, textRect, _horizontalScrollDip, point, ResolveReadingDirection(GetFlowDirection()));
        hitIndex = DisplayTextIndexToControlTextIndex(displayHitIndex);
    }
    _preferredMultilineXOffsetDip.reset();
    SetCaretIndex(hitIndex, true);
    if (_multiline)
    {
        EnsureMultilineCaretVisible(&host);
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
    }
    ResetCaretBlink(host);
    host.SyncTextInput(this);
    Invalidate(host);
    return true;
}

bool TextField::OnMouseUp(ControlHost& host, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/)
{
    const Failure::Scope failureScope;
    if (rightButton)
    {
        return false;
    }

    if (_passwordRevealButtonPressed)
    {
        _passwordRevealButtonPressed = false;
        RemaskPasswordReveal();
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
        return true;
    }

    const bool wasDragging = _dragSelecting;
    _dragSelecting         = false;
    if (wasDragging)
    {
        Invalidate(host);
    }
    return wasDragging;
}

void TextField::OnCaptureLost(ControlHost& host)
{
    const Failure::Scope failureScope;
    const bool hadRevealPress    = _passwordRevealButtonPressed;
    const bool wasDragging       = _dragSelecting;
    _dragSelecting               = false;
    _passwordRevealButtonPressed = false;
    _passwordRevealButtonHovered = false;

    if (hadRevealPress)
    {
        RemaskPasswordReveal();
        ResetCaretBlink(host);
        host.SyncTextInput(this);
    }
    if (hadRevealPress || wasDragging)
    {
        Invalidate(host);
    }
}

bool TextField::OnMouseWheel(ControlHost& host, D2D1_POINT_2F /*point*/, float wheelDelta, UINT /*modifiers*/)
{
    const Failure::Scope failureScope;
    if (! _multiline)
    {
        return false;
    }

    const D2D1_RECT_F textRect         = GetTextRect();
    const float viewportHeightDip      = std::max(1.0f, textRect.bottom - textRect.top);
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    // The retained layout and metrics: a wheel step on unchanged text and width, a partial notch included, lays out nothing.
    static_cast<void>(GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect));
    const std::vector<DWRITE_LINE_METRICS>& lineMetrics = _cachedMultilineLineMetrics;
    const float fallbackLineHeightDip                   = lineMetrics.empty() ? GetMultilineFallbackLineHeightDip(&host) : 0.0f;
    const MultilineViewportMetrics viewportMetrics      = BuildMultilineViewportMetrics(displayText, viewportHeightDip, lineMetrics, fallbackLineHeightDip);
    const size_t wheelLineCount                         = ResolveWheelScrollLineCount(viewportMetrics.visibleLineCount);
    if (wheelLineCount == 0u)
    {
        return true;
    }

    if (viewportMetrics.totalLineCount <= viewportMetrics.visibleLineCount)
    {
        _multilineFirstVisibleLine    = 0u;
        _multilineWheelDeltaRemainder = 0.0f;
        if (host.GetFocusControl() == this)
        {
            host.SyncTextInput(this);
        }
        return true;
    }

    _multilineWheelDeltaRemainder += wheelDelta;
    const int wheelStepCount = static_cast<int>(_multilineWheelDeltaRemainder / static_cast<float>(WHEEL_DELTA));
    if (wheelStepCount == 0)
    {
        return true;
    }
    _multilineWheelDeltaRemainder -= static_cast<float>(wheelStepCount * WHEEL_DELTA);

    const int direction              = wheelStepCount > 0 ? -1 : 1;
    const size_t deltaLines          = static_cast<size_t>(std::abs(wheelStepCount)) * (std::max)(static_cast<size_t>(1u), wheelLineCount);
    const size_t maxFirstVisibleLine = ComputeMultilineMaxFirstVisibleLine(viewportMetrics);
    if (_multilineFirstVisibleLine > maxFirstVisibleLine)
    {
        _multilineFirstVisibleLine = maxFirstVisibleLine;
    }
    const int64_t nextFirstVisibleLine =
        std::clamp<int64_t>(static_cast<int64_t>(_multilineFirstVisibleLine) + (static_cast<int64_t>(direction) * static_cast<int64_t>(deltaLines)),
                            0ll,
                            static_cast<int64_t>(maxFirstVisibleLine));

    if (_multilineFirstVisibleLine == static_cast<size_t>(nextFirstVisibleLine))
    {
        return true;
    }

    _multilineFirstVisibleLine = static_cast<size_t>(nextFirstVisibleLine);
    if (host.GetFocusControl() == this)
    {
        host.SyncTextInput(this);
    }
    Invalidate(host);
    return true;
}

bool TextField::OnKeyDown(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    const Failure::Scope failureScope;
    const auto inputLifetime = GetLifetimeToken();
    if (! KeyKeepsPendingSurrogate(virtualKey))
        _pendingLeadSurrogate = 0;
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    const size_t caretBeforeKey                          = _caretIndex;
    const std::optional<size_t> selectionAnchorBeforeKey = _selectionAnchorIndex;
    const auto textRect                                  = GetTextRect();
    const auto refreshCaret                              = [this, &host, &textRect, caretBeforeKey, selectionAnchorBeforeKey, inputLifetime]() noexcept
    {
        ResetCaretBlink(host);
        if (inputLifetime.expired())
            return false;
        if (_multiline)
        {
            EnsureMultilineCaretVisible(&host);
        }
        else
        {
            EnsureCaretVisible(&host, std::max(1.0f, textRect.right - textRect.left));
        }
        if (_caretIndex != caretBeforeKey || _selectionAnchorIndex != selectionAnchorBeforeKey)
        {
            // Selection and caret positions are part of the retained text snapshot. Publish them
            // through the same dirty/coalesced path as committed text, after the key's final state.
            RefreshAccessibilitySnapshot();
            if (inputLifetime.expired())
                return false;
        }
        Invalidate(host);
        return ! inputLifetime.expired();
    };

    if (virtualKey == VK_ESCAPE && _masked && _passwordRevealState != PasswordRevealState::Hidden)
    {
        _passwordRevealButtonPressed   = false;
        _passwordRevealKeyboardFocused = false;
        RemaskPasswordReveal();
        host.SyncTextInput(this);
        Invalidate(host);
        return false;
    }

    if (virtualKey == VK_TAB && ! ModifiersContainAlt(modifiers) && ! ModifiersContainCtrl(modifiers))
    {
        if (_passwordRevealKeyboardFocused)
        {
            _passwordRevealKeyboardFocused = false;
            _passwordRevealButtonPressed   = false;
            RemaskPasswordReveal();
            host.SyncTextInput(this);
            Invalidate(host);
            return ModifiersContainShift(modifiers);
        }

        if (! ModifiersContainShift(modifiers) && IsPasswordRevealButtonVisible())
        {
            _passwordRevealKeyboardFocused = true;
            _passwordRevealButtonPressed   = false;
            _passwordRevealButtonHovered   = false;
            Invalidate(host);
            return true;
        }

        return false;
    }

    if (_passwordRevealKeyboardFocused && (virtualKey == VK_SPACE || virtualKey == VK_RETURN) && ! ModifiersContainAlt(modifiers) &&
        ! ModifiersContainCtrl(modifiers))
    {
        _passwordRevealButtonPressed = true;
        SetPasswordRevealState(PasswordRevealState::Visible);
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
        return true;
    }

    if (_onPreviewKeyDown)
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        auto onPreviewKeyDown             = _onPreviewKeyDown;
        const bool handled                = (*onPreviewKeyDown)(host, virtualKey, modifiers);
        onPreviewKeyDown.reset();
        if (handled || lifetime.expired())
        {
            return true;
        }
    }

    // AltGr arrives as Ctrl+Alt and types a character (Polish AltGr+A is ą, Czech AltGr+V is @): Ctrl with Alt is never
    // an editing shortcut, as in the Win32 Edit control.
    if (ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
    {
        if (virtualKey == 'Z' && ! ModifiersContainShift(modifiers) && ! _readOnly)
        {
            if (TryUndoDirectEdit())
            {
                if (inputLifetime.expired())
                    return true;
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                host.SyncTextInput(this);
                return true;
            }
            return false;
        }
        if (virtualKey == 'Y' && ! ModifiersContainShift(modifiers) && ! _readOnly)
        {
            if (TryRedoDirectEdit())
            {
                if (inputLifetime.expired())
                    return true;
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                host.SyncTextInput(this);
                return true;
            }
            return false;
        }
        if (virtualKey == 'A')
        {
            SelectAllText();
            if (! refreshCaret())
                return true;
            return true;
        }
        if (virtualKey == 'C')
        {
            return OnCopy(host);
        }
        if (virtualKey == 'X' && ! _readOnly)
        {
            if (OnCopy(host))
            {
                RecordUndoStateForDirectEdit();
                _preferredMultilineXOffsetDip.reset();
                if (DeleteSelection())
                {
                    if (! NotifyChanged())
                    {
                        return true;
                    }
                    if (! refreshCaret())
                        return true;
                }
                return true;
            }
            return false;
        }
        if (virtualKey == VK_INSERT)
        {
            return OnCopy(host);
        }
        if (virtualKey == 'V' && ! _readOnly)
        {
            const auto clipboardText = host.ReadTextFromClipboard();
            if (clipboardText)
            {
                const std::wstring normalizedClipboardText = NormalizePastedControlText(clipboardText.value(), _multiline);
                const bool willMutate                      = HasSelection() || ! normalizedClipboardText.empty();
                if (willMutate)
                {
                    RecordUndoStateForDirectEdit();
                    static_cast<void>(DeleteSelection());
                    MutableText(_text->size() + normalizedClipboardText.size()).insert(_caretIndex, normalizedClipboardText);
                    _caretIndex += normalizedClipboardText.size();
                    _selectionAnchorIndex.reset();
                    if (! NotifyChanged())
                    {
                        return true;
                    }
                    if (! refreshCaret())
                        return true;
                }
                return true;
            }
        }
        if (virtualKey == VK_BACK && ! _readOnly)
        {
            const size_t eraseFrom = FindPreviousWordBoundary(Text(), _caretIndex);
            if (HasSelection() || eraseFrom < _caretIndex)
            {
                RecordUndoStateForDirectEdit();
                _preferredMultilineXOffsetDip.reset();
            }
            if (DeleteSelection())
            {
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                return true;
            }
            if (eraseFrom < _caretIndex)
            {
                MutableText(_text->size()).erase(eraseFrom, _caretIndex - eraseFrom);
                _caretIndex = eraseFrom;
                _selectionAnchorIndex.reset();
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                return true;
            }
            return true;
        }
        if (virtualKey == VK_DELETE && ! _readOnly)
        {
            const size_t eraseTo = FindNextWordBoundary(Text(), _caretIndex);
            if (HasSelection() || eraseTo > _caretIndex)
            {
                RecordUndoStateForDirectEdit();
                _preferredMultilineXOffsetDip.reset();
            }
            if (DeleteSelection())
            {
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                return true;
            }
            if (eraseTo > _caretIndex)
            {
                MutableText(_text->size()).erase(_caretIndex, eraseTo - _caretIndex);
                _selectionAnchorIndex.reset();
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
                return true;
            }
            return true;
        }
    }

    if (virtualKey == VK_INSERT && ModifiersContainShift(modifiers) && ! _readOnly)
    {
        const auto clipboardText = host.ReadTextFromClipboard();
        if (clipboardText)
        {
            const std::wstring normalizedClipboardText = NormalizePastedControlText(clipboardText.value(), _multiline);
            const bool willMutate                      = HasSelection() || ! normalizedClipboardText.empty();
            if (willMutate)
            {
                RecordUndoStateForDirectEdit();
                static_cast<void>(DeleteSelection());
                MutableText(_text->size() + normalizedClipboardText.size()).insert(_caretIndex, normalizedClipboardText);
                _caretIndex += normalizedClipboardText.size();
                _selectionAnchorIndex.reset();
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
            }
            return true;
        }
    }
    if (virtualKey == VK_DELETE && ModifiersContainShift(modifiers) && ! _readOnly)
    {
        if (OnCopy(host))
        {
            RecordUndoStateForDirectEdit();
            _preferredMultilineXOffsetDip.reset();
            if (DeleteSelection())
            {
                if (! NotifyChanged())
                {
                    return true;
                }
                if (! refreshCaret())
                    return true;
            }
            return true;
        }
        return false;
    }

    if (virtualKey == VK_LEFT)
    {
        _preferredMultilineXOffsetDip.reset();
        const bool extendSelection = ModifiersContainShift(modifiers);
        if (ModifiersContainCtrl(modifiers))
        {
            SetCaretIndex(FindPreviousWordBoundary(Text(), _caretIndex), extendSelection);
            if (! refreshCaret())
                return true;
            return true;
        }
        if (! extendSelection && HasSelection())
        {
            BreakDirectEditMerge();
            _caretIndex = GetSelectionRange().value().first;
            _selectionAnchorIndex.reset();
            if (! refreshCaret())
                return true;
            return true;
        }
        if (_caretIndex > 0u)
        {
            SetCaretIndex(StepToPreviousTextElement(Text(), _caretIndex), extendSelection);
            if (! refreshCaret())
                return true;
        }
        return true;
    }
    if (virtualKey == VK_RIGHT)
    {
        _preferredMultilineXOffsetDip.reset();
        const bool extendSelection = ModifiersContainShift(modifiers);
        if (ModifiersContainCtrl(modifiers))
        {
            SetCaretIndex(FindNextWordBoundary(Text(), _caretIndex), extendSelection);
            if (! refreshCaret())
                return true;
            return true;
        }
        if (! extendSelection && HasSelection())
        {
            BreakDirectEditMerge();
            _caretIndex = GetSelectionRange().value().second;
            _selectionAnchorIndex.reset();
            if (! refreshCaret())
                return true;
            return true;
        }
        SetCaretIndex(StepToNextTextElement(Text(), _caretIndex), extendSelection);
        if (! refreshCaret())
            return true;
        return true;
    }
    if (! _multiline && virtualKey == VK_UP && ! ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
    {
        _preferredMultilineXOffsetDip.reset();
        SetCaretIndex(0u, ModifiersContainShift(modifiers));
        if (! refreshCaret())
            return true;
        return true;
    }
    if (! _multiline && virtualKey == VK_DOWN && ! ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
    {
        _preferredMultilineXOffsetDip.reset();
        SetCaretIndex(_text->size(), ModifiersContainShift(modifiers));
        if (! refreshCaret())
            return true;
        return true;
    }
    // The navigation keys move through the layout of the field's own text: the retained one, which shows that text, unless
    // the field shows a mask; then its text is laid out for the key alone, as always, and not kept.
    std::vector<DWRITE_LINE_METRICS> maskedNavigationMetrics;
    const auto navigationLayout = [&]() -> std::pair<wil::com_ptr<IDWriteTextLayout>, const std::vector<DWRITE_LINE_METRICS>*>
    {
        const D2D1_RECT_F navigationRect = GetTextRect();
        if (! UsesMaskedDisplayText())
        {
            auto layout = GetOrCreateMultilineLayout(&host, _text, navigationRect);
            return {std::move(layout), &_cachedMultilineLineMetrics};
        }
        auto layout = CreateMultilineTextLayout(
            &host, Text(), _fontRole, std::max(1.0f, navigationRect.right - navigationRect.left), std::max(1.0f, navigationRect.bottom - navigationRect.top));
        maskedNavigationMetrics = GetMultilineLineMetrics(layout.get());
        return {std::move(layout), &maskedNavigationMetrics};
    };
    if (_multiline && virtualKey == VK_UP)
    {
        const bool extendSelection   = ModifiersContainShift(modifiers);
        const auto [layout, metrics] = navigationLayout();
        const size_t nextCaretIndex =
            MoveMultilineCaretVertically(layout.get(), *metrics, Text(), GetTextRect(), _caretIndex, false, _preferredMultilineXOffsetDip);
        BreakDirectEditMerge();
        SetSingleLineCaretIndex(_caretIndex, _selectionAnchorIndex, nextCaretIndex, extendSelection);
        if (! refreshCaret())
            return true;
        return true;
    }
    if (_multiline && virtualKey == VK_DOWN)
    {
        const bool extendSelection   = ModifiersContainShift(modifiers);
        const auto [layout, metrics] = navigationLayout();
        const size_t nextCaretIndex =
            MoveMultilineCaretVertically(layout.get(), *metrics, Text(), GetTextRect(), _caretIndex, true, _preferredMultilineXOffsetDip);
        BreakDirectEditMerge();
        SetSingleLineCaretIndex(_caretIndex, _selectionAnchorIndex, nextCaretIndex, extendSelection);
        if (! refreshCaret())
            return true;
        return true;
    }
    if (_multiline && virtualKey == VK_PRIOR)
    {
        const bool extendSelection     = ModifiersContainShift(modifiers);
        const D2D1_RECT_F pageTextRect = GetTextRect();
        const auto [layout, metrics]   = navigationLayout();
        const float pageHeightDip      = std::max(1.0f, pageTextRect.bottom - pageTextRect.top);
        const MultilineViewportMetrics viewportMetrics =
            BuildMultilineViewportMetrics(Text(), pageHeightDip, *metrics, metrics->empty() ? GetMultilineFallbackLineHeightDip(&host) : 0.0f);
        const size_t nextCaretIndex = MoveMultilineCaretByPage(
            layout.get(), *metrics, Text(), pageTextRect, _caretIndex, false, _preferredMultilineXOffsetDip, viewportMetrics.visibleLineCount);
        BreakDirectEditMerge();
        SetSingleLineCaretIndex(_caretIndex, _selectionAnchorIndex, nextCaretIndex, extendSelection);
        if (! refreshCaret())
            return true;
        return true;
    }
    if (_multiline && virtualKey == VK_NEXT)
    {
        const bool extendSelection     = ModifiersContainShift(modifiers);
        const D2D1_RECT_F pageTextRect = GetTextRect();
        const auto [layout, metrics]   = navigationLayout();
        const float pageHeightDip      = std::max(1.0f, pageTextRect.bottom - pageTextRect.top);
        const MultilineViewportMetrics viewportMetrics =
            BuildMultilineViewportMetrics(Text(), pageHeightDip, *metrics, metrics->empty() ? GetMultilineFallbackLineHeightDip(&host) : 0.0f);
        const size_t nextCaretIndex = MoveMultilineCaretByPage(
            layout.get(), *metrics, Text(), pageTextRect, _caretIndex, true, _preferredMultilineXOffsetDip, viewportMetrics.visibleLineCount);
        BreakDirectEditMerge();
        SetSingleLineCaretIndex(_caretIndex, _selectionAnchorIndex, nextCaretIndex, extendSelection);
        if (! refreshCaret())
            return true;
        return true;
    }
    if (virtualKey == VK_HOME)
    {
        _preferredMultilineXOffsetDip.reset();
        if (_multiline && ! ModifiersContainCtrl(modifiers))
        {
            const auto [layout, metrics] = navigationLayout();
            const size_t nextCaretIndex  = TryMoveCaretToWrappedLineBoundary(layout.get(), *metrics, Text(), GetTextRect(), _caretIndex, false)
                                               .value_or(FindLineStart(Text(), _caretIndex));
            SetCaretIndex(nextCaretIndex, ModifiersContainShift(modifiers));
        }
        else
        {
            SetCaretIndex(0u, ModifiersContainShift(modifiers));
        }
        if (! refreshCaret())
            return true;
        return true;
    }
    if (virtualKey == VK_END)
    {
        _preferredMultilineXOffsetDip.reset();
        if (_multiline && ! ModifiersContainCtrl(modifiers))
        {
            const auto [layout, metrics] = navigationLayout();
            const size_t nextCaretIndex =
                TryMoveCaretToWrappedLineBoundary(layout.get(), *metrics, Text(), GetTextRect(), _caretIndex, true).value_or(FindLineEnd(Text(), _caretIndex));
            SetCaretIndex(nextCaretIndex, ModifiersContainShift(modifiers));
        }
        else
        {
            SetCaretIndex(_text->size(), ModifiersContainShift(modifiers));
        }
        if (! refreshCaret())
            return true;
        return true;
    }
    if (virtualKey == VK_RETURN && _multiline)
    {
        host.AllowTranslatedCharacter();
        return true;
    }
    if (virtualKey == VK_RETURN && ! _multiline)
    {
        if (_onSubmitted)
        {
            const auto onSubmitted = _onSubmitted;
            (*onSubmitted)();
            return true;
        }
        return false;
    }
    if (virtualKey == VK_BACK && ! _readOnly)
    {
        if (HasSelection() || (_caretIndex > 0 && ! _text->empty()))
        {
            RecordUndoStateForDirectEdit(HasSelection() ? DirectEditMergeKind::None : DirectEditMergeKind::BackspaceRun);
            _preferredMultilineXOffsetDip.reset();
        }
        if (DeleteSelection())
        {
            if (! NotifyChanged())
            {
                return true;
            }
            if (! refreshCaret())
                return true;
            return true;
        }
        if (_caretIndex > 0 && ! _text->empty())
        {
            const auto range = GetTextElementDeletionRange(Text(), _caretIndex, true);
            MutableText(_text->size()).erase(range.first, range.last - range.first);
            _caretIndex = range.first;
            _selectionAnchorIndex.reset();
            if (! NotifyChanged())
            {
                return true;
            }
            if (! refreshCaret())
                return true;
        }
        return true;
    }
    if (virtualKey == VK_DELETE && ! _readOnly)
    {
        if (HasSelection() || _caretIndex < _text->size())
        {
            RecordUndoStateForDirectEdit(HasSelection() ? DirectEditMergeKind::None : DirectEditMergeKind::DeleteRun);
            _preferredMultilineXOffsetDip.reset();
        }
        if (DeleteSelection())
        {
            if (! NotifyChanged())
            {
                return true;
            }
            if (! refreshCaret())
                return true;
            return true;
        }
        if (_caretIndex < _text->size())
        {
            const auto range = GetTextElementDeletionRange(Text(), _caretIndex, false);
            MutableText(_text->size()).erase(range.first, range.last - range.first);
            _caretIndex = range.first;
            _selectionAnchorIndex.reset();
            if (! NotifyChanged())
            {
                return true;
            }
            if (! refreshCaret())
                return true;
        }
        return true;
    }
    return false;
}

bool TextField::OnKeyUp(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    const Failure::Scope failureScope;
    if (! _passwordRevealKeyboardFocused || (virtualKey != VK_SPACE && virtualKey != VK_RETURN) || ModifiersContainAlt(modifiers) ||
        ModifiersContainCtrl(modifiers))
    {
        return false;
    }

    if (_passwordRevealButtonPressed)
    {
        _passwordRevealButtonPressed = false;
        RemaskPasswordReveal();
        ResetCaretBlink(host);
        host.SyncTextInput(this);
        Invalidate(host);
    }
    return true;
}

bool TextField::OnChar(ControlHost& host, wchar_t ch, UINT modifiers)
{
    const Failure::Scope failureScope;
    const auto inputLifetime = GetLifetimeToken();
    // A pair remains private until both UTF-16 units are available.
    // Alt alone makes the system's characters (menu access keys, Alt+Space); AltGr arrives as Ctrl+Alt and types.
    if (ModifiersContainAlt(modifiers) && ! ModifiersContainCtrl(modifiers))
    {
        _pendingLeadSurrogate = 0;
        return false;
    }
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    if (_passwordRevealKeyboardFocused && (ch == L' ' || ch == L'\r'))
    {
        _pendingLeadSurrogate = 0;
        return true;
    }
    if (_readOnly)
    {
        _pendingLeadSurrogate = 0;
        return false;
    }
    if (IsUtf16LeadSurrogate(ch))
    {
        _pendingLeadSurrogate = ch;
        return true;
    }
    const wchar_t lead = std::exchange(_pendingLeadSurrogate, wchar_t{});
    if (IsUtf16TrailSurrogate(ch) && ! lead)
        return true;

    if (ch == L'\r')
    {
        if (! _multiline)
        {
            return true;
        }
        RecordUndoStateForDirectEdit(DirectEditMergeKind::InsertRun);
        _preferredMultilineXOffsetDip.reset();
        static_cast<void>(DeleteSelection());
        MutableText(_text->size() + 1u).insert(_caretIndex, 1u, L'\n');
        _caretIndex += 1u;
        _selectionAnchorIndex.reset();
    }
    else if (std::iswcntrl(static_cast<wint_t>(ch)) == 0 && (_multiline || ch != L'\t'))
    {
        RecordUndoStateForDirectEdit(DirectEditMergeKind::InsertRun);
        _preferredMultilineXOffsetDip.reset();
        static_cast<void>(DeleteSelection());
        if (IsUtf16TrailSurrogate(ch))
        {
            const wchar_t pair[] = {lead, ch};
            MutableText(_text->size() + 2u).insert(_caretIndex, pair, 2u);
            _caretIndex += 2u;
        }
        else
        {
            MutableText(_text->size() + 1u).insert(_caretIndex, 1u, ch);
            ++_caretIndex;
        }
        _selectionAnchorIndex.reset();
    }
    else
    {
        return false;
    }

    if (! NotifyChanged())
    {
        return true;
    }
    ResetCaretBlink(host);
    if (inputLifetime.expired())
        return true;
    if (_multiline)
    {
        EnsureMultilineCaretVisible(&host);
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, GetTextRect().right - GetTextRect().left));
    }
    Invalidate(host);
    return true;
}

bool TextField::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip)
{
    const Failure::Scope failureScope;
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    host.SyncTextInput(this);
    ResetCaretBlink(host);
    Invalidate(host);
    return Control::OnContextMenu(host, keyboardInvocation, pointDip);
}

bool TextField::OnCopy(ControlHost& host)
{
    const Failure::Scope failureScope;
    if (_masked && _passwordRevealMode != PasswordRevealMode::Visible && _passwordRevealState != PasswordRevealState::Visible)
    {
        return false;
    }

    if (const std::optional<std::pair<size_t, size_t>> selectionRange = GetSelectionRange())
    {
        const auto [selectionStart, selectionEnd] = selectionRange.value();
        return host.CopyTextToClipboard(_text->substr(selectionStart, selectionEnd - selectionStart));
    }

    return false;
}

bool TextField::OnSelectAll(ControlHost& host)
{
    const Failure::Scope failureScope;
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    _preferredMultilineXOffsetDip.reset();
    SelectAllText();
    ResetCaretBlink(host);
    if (_multiline)
    {
        EnsureMultilineCaretVisible(&host);
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, GetTextRect().right - GetTextRect().left));
    }
    host.SyncTextInput(this);
    Invalidate(host);
    return true;
}

bool TextField::SupportsTextInput() const noexcept
{
    return true;
}

bool TextField::AcceptsTypedText() const noexcept
{
    return ! IsReadOnly() && IsEnabled();
}

std::optional<D2D1_RECT_F> TextField::GetTextInputViewportRect() const noexcept
{
    const D2D1_RECT_F textRect = GetTextRect();
    if (textRect.right <= textRect.left || textRect.bottom <= textRect.top)
    {
        return std::nullopt;
    }
    return textRect;
}

std::optional<D2D1_RECT_F> TextField::GetTextInputCaretRect(const ControlHost& host, size_t controlTextIndex) const noexcept
{
    const D2D1_RECT_F textRect         = GetTextRect();
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    const size_t displayCaretIndex     = ControlTextIndexToDisplayTextIndex(controlTextIndex);
    if (textRect.right <= textRect.left || textRect.bottom <= textRect.top)
    {
        return std::nullopt;
    }

    if (_multiline)
    {
        const auto multilineLayout     = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const float multilineScrollDip = MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine);
        return MeasureMultilineCaretRectDip(multilineLayout.get(), displayText, textRect, multilineScrollDip, displayCaretIndex);
    }

    const bool perfEnabled                          = Debug::Perf::IsCaptureEnabled();
    const auto startedAt                            = perfEnabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const DWRITE_READING_DIRECTION readingDirection = ResolveReadingDirection(GetFlowDirection());
    const wil::com_ptr<IDWriteTextLayout> layout    = GetOrCreateSingleLineLayout(&host,
                                                                                  displayText,
                                                                                  std::max(1.0f, textRect.right - textRect.left + _horizontalScrollDip),
                                                                                  std::max(1.0f, textRect.bottom - textRect.top),
                                                                                  readingDirection);
    const float caretOffset                         = MeasureCaretOffsetDip(layout.get(), displayText, displayCaretIndex);
    const float caretMaxX                           = std::max(textRect.left, textRect.right - 1.0f);
    const float caretX                              = std::clamp(textRect.left + caretOffset - _horizontalScrollDip, textRect.left, caretMaxX);
    const D2D1_RECT_F result                        = D2D1::RectF(caretX, textRect.top + 2.0f, caretX + 1.0f, textRect.bottom - 2.0f);
    if (perfEnabled && ShouldEmitSingleLineBiDiTextMetric(displayText, readingDirection))
    {
        const std::wstring_view detail = readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? L"text-field-rtl" : L"text-field-ltr";
        Debug::Perf::Emit(L"dxui.textinput.bidi_caret_rect_us", detail, Debug::Perf::ElapsedUs(startedAt), displayCaretIndex, displayText.size(), S_OK);
    }
    return result;
}

std::optional<std::vector<D2D1_RECT_F>> TextField::GetTextInputRangeRects(const ControlHost& host,
                                                                          size_t controlTextStartIndex,
                                                                          size_t controlTextEndIndex) const
{
    const SharedText displayTextBuffer                          = GetDisplayText();
    const std::wstring& displayText                             = *displayTextBuffer;
    const std::optional<std::pair<size_t, size_t>> displayRange = ControlTextRangeToDisplayTextRange(controlTextStartIndex, controlTextEndIndex);
    if (! displayRange.has_value())
    {
        return std::nullopt;
    }
    const size_t rangeStart = displayRange.value().first;
    const size_t rangeEnd   = displayRange.value().second;
    if (rangeStart >= rangeEnd || rangeStart > static_cast<size_t>(std::numeric_limits<UINT32>::max()) ||
        rangeEnd - rangeStart > static_cast<size_t>(std::numeric_limits<UINT32>::max()))
    {
        return std::nullopt;
    }

    const D2D1_RECT_F textRect = GetTextRect();
    if (textRect.right <= textRect.left || textRect.bottom <= textRect.top)
    {
        return std::nullopt;
    }

    if (! _multiline)
    {
        const DWRITE_READING_DIRECTION readingDirection = ResolveReadingDirection(GetFlowDirection());
        const float heightDip                           = std::max(1.0f, textRect.bottom - textRect.top);
        const wil::com_ptr<IDWriteTextLayout> layout =
            GetOrCreateSingleLineLayout(&host, displayText, std::max(1.0f, textRect.right - textRect.left + _horizontalScrollDip), heightDip, readingDirection);
        if (! layout)
        {
            return std::nullopt;
        }

        UINT32 actualCount = 0u;
        std::vector<DWRITE_HIT_TEST_METRICS> metrics(64u);
        HRESULT hr = layout->HitTestTextRange(static_cast<UINT32>(rangeStart),
                                              static_cast<UINT32>(rangeEnd - rangeStart),
                                              textRect.left - _horizontalScrollDip,
                                              textRect.top,
                                              metrics.data(),
                                              static_cast<UINT32>(metrics.size()),
                                              &actualCount);
        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && actualCount > 0u)
        {
            metrics.resize(actualCount);
            hr = layout->HitTestTextRange(static_cast<UINT32>(rangeStart),
                                          static_cast<UINT32>(rangeEnd - rangeStart),
                                          textRect.left - _horizontalScrollDip,
                                          textRect.top,
                                          metrics.data(),
                                          static_cast<UINT32>(metrics.size()),
                                          &actualCount);
        }
        if (FAILED(hr) || actualCount == 0u)
        {
            return std::nullopt;
        }

        std::vector<D2D1_RECT_F> rects;
        rects.reserve(actualCount);
        for (UINT32 index = 0u; index < actualCount; ++index)
        {
            const DWRITE_HIT_TEST_METRICS& hit = metrics[index];
            D2D1_RECT_F rect                   = D2D1::RectF(hit.left, hit.top, hit.left + hit.width, hit.top + hit.height);
            rect                               = ClipTextInputRectToBounds(rect, textRect);
            if (rect.right > rect.left && rect.bottom > rect.top)
            {
                rects.push_back(rect);
            }
        }

        if (rects.empty())
        {
            return std::nullopt;
        }
        return rects;
    }

    const auto multilineLayout = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
    if (! multilineLayout)
    {
        return std::nullopt;
    }

    const float multilineScrollDip = MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine);

    UINT32 actualCount = 0u;
    std::vector<DWRITE_HIT_TEST_METRICS> metrics(64u);
    HRESULT hr = multilineLayout->HitTestTextRange(static_cast<UINT32>(rangeStart),
                                                   static_cast<UINT32>(rangeEnd - rangeStart),
                                                   textRect.left,
                                                   textRect.top - multilineScrollDip,
                                                   metrics.data(),
                                                   static_cast<UINT32>(metrics.size()),
                                                   &actualCount);
    if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && actualCount > 0u)
    {
        metrics.resize(actualCount);
        hr = multilineLayout->HitTestTextRange(static_cast<UINT32>(rangeStart),
                                               static_cast<UINT32>(rangeEnd - rangeStart),
                                               textRect.left,
                                               textRect.top - multilineScrollDip,
                                               metrics.data(),
                                               static_cast<UINT32>(metrics.size()),
                                               &actualCount);
    }
    if (FAILED(hr) || actualCount == 0u)
    {
        return std::nullopt;
    }

    std::vector<D2D1_RECT_F> rects;
    rects.reserve(actualCount);
    for (UINT32 index = 0u; index < actualCount; ++index)
    {
        const DWRITE_HIT_TEST_METRICS& hit = metrics[index];
        D2D1_RECT_F rect                   = D2D1::RectF(hit.left, hit.top, hit.left + hit.width, hit.top + hit.height);
        rect                               = ClipTextInputRectToBounds(rect, textRect);
        if (rect.right > rect.left && rect.bottom > rect.top)
        {
            rects.push_back(rect);
        }
    }

    if (rects.empty())
    {
        return std::nullopt;
    }
    return rects;
}

std::optional<size_t> TextField::HitTestTextInputPoint(const ControlHost& host, D2D1_POINT_2F point) const noexcept
{
    const D2D1_RECT_F textRect = GetTextRect();
    if (textRect.right <= textRect.left || textRect.bottom <= textRect.top)
    {
        return std::nullopt;
    }

    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    if (_multiline)
    {
        const auto multilineLayout   = GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect);
        const size_t displayHitIndex = HitTestMultilineCaretIndexDip(
            multilineLayout.get(), displayText, textRect, MeasureWrappedLineOffsetDip(_cachedMultilineLineMetrics, _multilineFirstVisibleLine), point);
        return DisplayTextIndexToControlTextIndex(displayHitIndex);
    }

    const size_t displayHitIndex =
        HitTestCaretIndexDip(&host, displayText, _fontRole, textRect, _horizontalScrollDip, point, ResolveReadingDirection(GetFlowDirection()));
    return DisplayTextIndexToControlTextIndex(displayHitIndex);
}

bool TextField::ExportTextInputState(TextInputState& outState) const
{
    outState.text.assign(Text());
    outState.selectionAnchorIndex = _selectionAnchorIndex;
    outState.caretIndex           = _caretIndex;
    outState.firstVisibleLine     = _multilineFirstVisibleLine;
    outState.readOnly             = _readOnly;
    outState.masked               = _masked;
    outState.multiline            = _multiline;
    return true;
}

bool TextField::ImportTextInputState(ControlHost& host, const TextInputState& state, bool notifyChange)
{
    // Unchanged text keeps the field's buffer; changed text is the one copy of this import.
    return ImportTextInputText(host, SharedTextEquals(_text, state.text) ? _text : MakeSharedText(state.text), state, notifyChange);
}

bool TextField::ImportTextInputText(ControlHost& host, std::shared_ptr<const std::wstring> text, const TextInputState& state, bool notifyChange)
{
    const Failure::Scope failureScope;
    const auto lifetime   = GetLifetimeToken();
    _pendingLeadSurrogate = 0;
    // Pinned for the comparisons below: the notification re-entry can replace _text.
    const SharedText previousText         = _text;
    const bool changed                    = ! SharedTextEquals(previousText, text);
    const size_t previousFirstVisibleLine = _multilineFirstVisibleLine;
    // Imported previews, cancellation and selection changes preserve existing history on every host.
    // A changed notifying import is one committed edit, regardless of how many previews preceded it.
    if (notifyChange && changed)
        RecordUndoStateForDirectEdit();
    else if (changed)
        TextEditHistory::NoteUnrecordedChange(_undoHistory, _redoHistory, *previousText);
    BreakDirectEditMerge();
    ResetSingleLineSelectionClickSequence(_selectionClickSequence);
    // Adopts the buffer: the store, the host cache and this field now share it, and none of them writes to it.
    _text = std::move(text);
    if (_multiline && changed)
        InvalidateMeasure();
    if (lifetime.expired())
        return false;
    _caretIndex = std::min(state.caretIndex, _text->size());
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    if (state.selectionAnchorIndex)
    {
        _selectionAnchorIndex = std::min(state.selectionAnchorIndex.value(), _text->size());
        if (_selectionAnchorIndex.value() == _caretIndex)
        {
            _selectionAnchorIndex.reset();
        }
    }
    else
    {
        _selectionAnchorIndex.reset();
    }

    const bool preserveReadOnlyMultilineViewport = _multiline && _readOnly && state.multiline;
    _multilineFirstVisibleLine = state.multiline ? (preserveReadOnlyMultilineViewport ? previousFirstVisibleLine : state.firstVisibleLine) : 0u;
    _preferredMultilineXOffsetDip.reset();
    _multilineWheelDeltaRemainder = 0.0f;
    _dragSelecting                = false;
    ResetCaretBlink(host);
    if (lifetime.expired())
        return false;
    if (_multiline)
    {
        if (preserveReadOnlyMultilineViewport)
        {
            if (! SharedTextEquals(previousText, _text))
            {
                const D2D1_RECT_F textRect         = GetTextRect();
                const SharedText displayTextBuffer = GetDisplayText();
                const std::wstring& displayText    = *displayTextBuffer;
                if (displayText.empty())
                {
                    _multilineFirstVisibleLine = 0u;
                }
                else
                {
                    const float viewportHeightDip = std::max(1.0f, textRect.bottom - textRect.top);
                    static_cast<void>(GetOrCreateMultilineLayout(&host, displayTextBuffer, textRect));
                    const std::vector<DWRITE_LINE_METRICS>& lineMetrics = _cachedMultilineLineMetrics;
                    const float fallbackLineHeightDip                   = lineMetrics.empty() ? GetMultilineFallbackLineHeightDip(&host) : 0.0f;
                    const MultilineViewportMetrics viewportMetrics =
                        BuildMultilineViewportMetrics(displayText, viewportHeightDip, lineMetrics, fallbackLineHeightDip);
                    _multilineFirstVisibleLine = ClampMultilineFirstVisibleLine(_multilineFirstVisibleLine, viewportMetrics);
                }
            }
        }
        else
        {
            EnsureMultilineCaretVisible(&host);
        }
    }
    else
    {
        EnsureCaretVisible(&host, std::max(1.0f, GetTextRect().right - GetTextRect().left));
    }
    Invalidate(host);
    if (lifetime.expired())
        return false;
    if (notifyChange && changed)
    {
        return NotifyChanged();
    }
    return true;
}

TextField::EditHistoryState TextField::CaptureEditHistoryState() const
{
    // Shares the current buffer: history costs no copy, and a later edit makes its own buffer.
    return EditHistoryState{
        .text = _text, .caretIndex = _caretIndex, .selectionAnchorIndex = _selectionAnchorIndex, .firstVisibleLine = _multilineFirstVisibleLine};
}

void TextField::RestoreEditHistoryState(const EditHistoryState& state, bool textChanged) noexcept
{
    const bool remeasure = _multiline && textChanged;
    _caretIndex          = std::min(state.caretIndex, _text->size());
    if (state.selectionAnchorIndex)
    {
        _selectionAnchorIndex = std::min(state.selectionAnchorIndex.value(), _text->size());
        if (_selectionAnchorIndex.value() == _caretIndex)
        {
            _selectionAnchorIndex.reset();
        }
    }
    else
    {
        _selectionAnchorIndex.reset();
    }
    _multilineFirstVisibleLine = _multiline ? state.firstVisibleLine : 0u;
    _preferredMultilineXOffsetDip.reset();
    _multilineWheelDeltaRemainder = 0.0f;
    _dragSelecting                = false;
    _horizontalScrollDip          = 0.0f;
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    if (remeasure)
        InvalidateMeasure();
}

void TextField::BeginEditTransactionMetric(const wchar_t* detail) noexcept
{
    if (! Debug::Perf::IsCaptureEnabled())
    {
        return;
    }

    _pendingEditTransactionStartedAt = std::chrono::steady_clock::now();
    _pendingEditTransactionDetail    = detail ? detail : L"";
}

void TextField::FinishEditTransactionMetric() noexcept
{
    if (! _pendingEditTransactionStartedAt.has_value())
    {
        return;
    }

    const auto startedAt        = _pendingEditTransactionStartedAt.value();
    const wchar_t* const detail = _pendingEditTransactionDetail ? _pendingEditTransactionDetail : L"";
    _pendingEditTransactionStartedAt.reset();
    _pendingEditTransactionDetail = L"";

    Debug::Perf::Emit(L"dxui.textinput.edit_transaction_us",
                      detail,
                      Debug::Perf::ElapsedUs(startedAt),
                      static_cast<uint64_t>(_text->size()),
                      static_cast<uint64_t>(_caretIndex),
                      S_OK);
    Debug::Perf::Emit(L"dxui.textinput.undo_depth", detail, 0u, static_cast<uint64_t>(_undoHistory.size()), static_cast<uint64_t>(_redoHistory.size()), S_OK);
}

void TextField::RegenerateConcealedMaskEpoch() const noexcept
{
    _concealedMaskVisibleDotCountValid = false;
}

size_t TextField::GetConcealedMaskVisibleDotCount(size_t exactCount) const noexcept
{
    const ConcealedMaskBucket bucket = GetConcealedMaskBucket(exactCount);
    if (bucket.displayBase == 0u)
    {
        _concealedMaskVisibleDotCountValid = false;
        _concealedMaskBucketStart          = 0u;
        _concealedMaskBucketEnd            = 0u;
        _concealedMaskVisibleDotCount      = 0u;
        return 0u;
    }

    if (_concealedMaskVisibleDotCountValid && exactCount >= _concealedMaskBucketStart && exactCount <= _concealedMaskBucketEnd)
    {
        return _concealedMaskVisibleDotCount;
    }

    const size_t previousDotCount = _concealedMaskVisibleDotCount;
    const size_t displaySpan      = std::max<size_t>(1u, bucket.displaySpan);
    const uint64_t mixedEpoch     = (_concealedMaskEpoch * 1103515245ull) + 12345ull + (static_cast<uint64_t>(bucket.displayBase) * 2654435761ull);
    size_t displayOffset          = static_cast<size_t>(mixedEpoch % displaySpan);
    size_t dotCount               = bucket.displayBase + displayOffset;
    if (displaySpan > 1u && dotCount == previousDotCount)
    {
        displayOffset = (displayOffset + 1u) % displaySpan;
        dotCount      = bucket.displayBase + displayOffset;
    }

    ++_concealedMaskEpoch;
    _concealedMaskBucketStart          = bucket.start;
    _concealedMaskBucketEnd            = bucket.end;
    _concealedMaskVisibleDotCount      = dotCount;
    _concealedMaskVisibleDotCountValid = true;
    return _concealedMaskVisibleDotCount;
}

void TextField::BreakDirectEditMerge() noexcept
{
    _directEditMergeKind = DirectEditMergeKind::None;
}

void TextField::RecordUndoStateForDirectEdit(DirectEditMergeKind mergeKind)
{
    BeginEditTransactionMetric(L"direct-edit");
    const bool mergeable = mergeKind != DirectEditMergeKind::None;
    // A run adds to its open step, which still restores the text the run started from.
    const bool coalesced = mergeable && _directEditMergeKind == mergeKind && ! _undoHistory.empty() && ! _undoHistory.back().sealed && ! HasSelection();
    if (! coalesced)
        TextEditHistory::Record(_undoHistory, _redoHistory, *_text, CaptureEditHistoryState(), kMaxEditHistoryEntries);
    _directEditMergeKind = mergeable ? mergeKind : DirectEditMergeKind::None;
    _redoHistory.clear();
}

bool TextField::TryUndoDirectEdit() noexcept
{
    if (_undoHistory.empty())
    {
        return false;
    }

    BeginEditTransactionMetric(L"undo");
    BreakDirectEditMerge();
    try
    {
        EditHistoryState applied;
        bool textChanged = false;
        if (! TextEditHistory::Undo(_undoHistory, _redoHistory, _text, CaptureEditHistoryState(), kMaxEditHistoryEntries, applied, textChanged))
            return false;
        RestoreEditHistoryState(applied, textChanged);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
}

bool TextField::TryRedoDirectEdit() noexcept
{
    if (_redoHistory.empty())
    {
        return false;
    }

    BeginEditTransactionMetric(L"redo");
    BreakDirectEditMerge();
    try
    {
        EditHistoryState applied;
        bool textChanged = false;
        if (! TextEditHistory::Redo(_undoHistory, _redoHistory, _text, CaptureEditHistoryState(), kMaxEditHistoryEntries, applied, textChanged))
            return false;
        RestoreEditHistoryState(applied, textChanged);
        return true;
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
}

void TextField::RemaskPasswordReveal() noexcept
{
    if (_masked && _passwordRevealMode != PasswordRevealMode::Visible && _passwordRevealState != PasswordRevealState::Hidden)
    {
        SetPasswordRevealState(PasswordRevealState::Hidden);
    }
}

void TextField::SetCaretIndex(size_t caretIndex, bool extendSelection) noexcept
{
    _preferredMultilineXOffsetDip.reset();
    BreakDirectEditMerge();
    SetSingleLineCaretIndex(_caretIndex, _selectionAnchorIndex, std::min(caretIndex, _text->size()), extendSelection);
}

bool TextField::HasSelection() const noexcept
{
    return GetSelectionRange().has_value();
}

std::optional<std::pair<size_t, size_t>> TextField::GetSelectionRange() const noexcept
{
    return GetSingleLineSelectionRange(_selectionAnchorIndex, _caretIndex);
}

bool TextField::DeleteSelection() noexcept
{
    if (! GetSingleLineSelectionRange(_selectionAnchorIndex, _caretIndex))
        return false;
    try
    {
        return DeleteSingleLineSelection(MutableText(_text->size()), _caretIndex, _selectionAnchorIndex);
    }
    catch (const std::bad_alloc&)
    {
        Failure::FailAllocation();
    }
}

void TextField::SelectAllText() noexcept
{
    BreakDirectEditMerge();
    SelectAllSingleLineText(_text->size(), _caretIndex, _selectionAnchorIndex);
}

void TextField::SelectWordAt(size_t hitIndex) noexcept
{
    BreakDirectEditMerge();
    SelectSingleLineWordAt(Text(), hitIndex, _caretIndex, _selectionAnchorIndex);
}

D2D1_RECT_F TextField::GetTextRect() const noexcept
{
    const bool hasTrailingButton = IsClearButtonVisible() || IsPasswordRevealButtonVisible();
    const float rightInset       = hasTrailingButton ? std::max(38.0f, _textPaddingRightDip) : _textPaddingRightDip; // 30 DIP button + padding
    const float verticalPadding  = IsCompactDensity() ? 2.0f : 4.0f;
    const float topInset         = _hasExplicitVerticalTextPadding ? _textPaddingTopDip : verticalPadding;
    const float bottomInset      = _hasExplicitVerticalTextPadding ? _textPaddingBottomDip : verticalPadding;
    return D2D1::RectF(GetBounds().left + _textPaddingLeftDip, GetBounds().top + topInset, GetBounds().right - rightInset, GetBounds().bottom - bottomInset);
}

bool TextField::IsClearButtonVisible() const noexcept
{
    return _clearButtonEnabled && ! _masked && ! _readOnly && ! _multiline && ! _text->empty() && HasFocus();
}

D2D1_RECT_F TextField::GetClearButtonRect() const noexcept
{
    constexpr float kClearButtonWidthDip = 30.0f;
    return D2D1::RectF(GetBounds().right - kClearButtonWidthDip, GetBounds().top, GetBounds().right, GetBounds().bottom);
}

bool TextField::IsPasswordRevealButtonVisible() const noexcept
{
    return _passwordRevealMode == PasswordRevealMode::Peek && _masked && IsEnabled() && ! _readOnly && ! _multiline && ! _text->empty() && HasFocus();
}

D2D1_RECT_F TextField::GetPasswordRevealButtonRect() const noexcept
{
    constexpr float kRevealButtonWidthDip = 30.0f;
    return D2D1::RectF(GetBounds().right - kRevealButtonWidthDip, GetBounds().top, GetBounds().right, GetBounds().bottom);
}

std::shared_ptr<const std::wstring> TextField::GetDisplayText() const
{
    if (! _masked || _text->empty() || _passwordRevealMode == PasswordRevealMode::Visible || _passwordRevealState == PasswordRevealState::Visible)
    {
        return _text;
    }

    // The mask is a pure function of its length: one buffer per dot count is kept and shared, never written.
    const size_t dotCount = GetSecretVisibleDotCount();
    if (! _maskedDisplayText || _maskedDisplayText->size() != dotCount)
        _maskedDisplayText = MakeSharedText(std::wstring(dotCount, L'\u2022'));
    return _maskedDisplayText;
}

bool TextField::UsesMaskedDisplayText() const noexcept
{
    return _masked && ! _text->empty() && _passwordRevealMode != PasswordRevealMode::Visible && _passwordRevealState != PasswordRevealState::Visible;
}

const std::vector<size_t>& TextField::GetMaskedSourceTextElementBoundaries() const noexcept
{
    if (! _maskedSourceTextElementBoundaries.empty())
    {
        return _maskedSourceTextElementBoundaries;
    }

    const bool perfEnabled = Debug::Perf::IsCaptureEnabled();
    const auto startedAt   = perfEnabled ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    _maskedSourceTextElementBoundaries.push_back(0u);
    size_t sourceIndex = 0u;
    while (sourceIndex < _text->size())
    {
        const size_t nextIndex = FindNextTextElementBoundary(Text(), sourceIndex);
        if (nextIndex <= sourceIndex)
        {
            _maskedSourceTextElementBoundaries.push_back(_text->size());
            break;
        }
        _maskedSourceTextElementBoundaries.push_back(nextIndex);
        sourceIndex = nextIndex;
    }
    if (perfEnabled)
    {
        Debug::Perf::Emit(L"dxui.textinput.masked_index_map_rebuild_us",
                          _maskLengthPolicy == PasswordMaskLengthPolicy::Exact ? L"exact" : L"concealed",
                          Debug::Perf::ElapsedUs(startedAt),
                          _text->size(),
                          _maskedSourceTextElementBoundaries.size() - 1u,
                          S_OK);
    }
    return _maskedSourceTextElementBoundaries;
}

size_t TextField::ControlTextIndexToDisplayTextIndex(size_t controlTextIndex) const noexcept
{
    const size_t clampedControlIndex = (std::min)(controlTextIndex, _text->size());
    if (! UsesMaskedDisplayText())
    {
        return clampedControlIndex;
    }

    const std::vector<size_t>& boundaries = GetMaskedSourceTextElementBoundaries();
    const size_t sourceElementCount       = boundaries.size() - 1u;
    const size_t snappedControlIndex      = SnapCaretIndexToTextElementBoundary(Text(), clampedControlIndex);
    const auto boundary                   = std::lower_bound(boundaries.begin(), boundaries.end(), snappedControlIndex);
    const size_t sourceElementIndex       = boundary == boundaries.end() ? sourceElementCount : static_cast<size_t>(boundary - boundaries.begin());
    const size_t displayLength            = _maskLengthPolicy == PasswordMaskLengthPolicy::Exact ? sourceElementCount : GetSecretVisibleDotCount();
    return ScaleMaskedTextIndex(sourceElementIndex, sourceElementCount, displayLength);
}

size_t TextField::DisplayTextIndexToControlTextIndex(size_t displayTextIndex) const noexcept
{
    if (! UsesMaskedDisplayText())
    {
        return (std::min)(displayTextIndex, _text->size());
    }

    const std::vector<size_t>& boundaries = GetMaskedSourceTextElementBoundaries();
    const size_t sourceElementCount       = boundaries.size() - 1u;
    const size_t displayLength            = _maskLengthPolicy == PasswordMaskLengthPolicy::Exact ? sourceElementCount : GetSecretVisibleDotCount();
    const size_t sourceElementIndex       = ScaleMaskedTextIndex((std::min)(displayTextIndex, displayLength), displayLength, sourceElementCount);
    return boundaries[(std::min)(sourceElementIndex, sourceElementCount)];
}

std::optional<std::pair<size_t, size_t>> TextField::ControlTextRangeToDisplayTextRange(size_t controlTextStartIndex, size_t controlTextEndIndex) const noexcept
{
    if (controlTextStartIndex >= controlTextEndIndex)
    {
        return std::nullopt;
    }

    const size_t displayStart = ControlTextIndexToDisplayTextIndex(controlTextStartIndex);
    const size_t displayEnd   = ControlTextIndexToDisplayTextIndex(controlTextEndIndex);
    if (displayStart >= displayEnd)
    {
        return std::nullopt;
    }
    return std::pair<size_t, size_t>{displayStart, displayEnd};
}

wil::com_ptr<IDWriteTextLayout> TextField::GetOrCreateMultilineLayout(const ControlHost* host,
                                                                      const std::shared_ptr<const std::wstring>& displayText,
                                                                      const D2D1_RECT_F& textRect) const noexcept
{
    // The width every multiline path wraps at (MultilinePaintLayoutWidthDip), and the height CreateMultilineTextLayout uses.
    const float widthDip            = MultilinePaintLayoutWidthDip(textRect);
    const float heightDip           = std::max(kMultilineLayoutHeightDip, std::max(1.0f, textRect.bottom - textRect.top));
    const std::wstring_view text    = SharedTextView(displayText);
    IDWriteTextFormat* const format = host ? host->GetTextFormat(_fontRole, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true) : nullptr;
    // The same buffer (an edit in place marks the cache dirty) of the same length: the display text is unchanged.
    const bool sameText =
        ! _cachedLayoutText.owner_before(displayText) && ! displayText.owner_before(_cachedLayoutText) && displayText && _cachedLayoutTextLength == text.size();
    if (_multilineLayoutDirty || ! _cachedMultilineLayout || ! sameText || _cachedLayoutSize.width != widthDip || _cachedLayoutSize.height != heightDip ||
        _cachedLayoutRole != _fontRole || _cachedLayoutFormat.get() != format)
    {
        _cachedMultilineLayout  = format ? CreateMultilineTextLayout(host, text, _fontRole, widthDip, heightDip) : wil::com_ptr<IDWriteTextLayout>();
        _cachedLayoutText       = displayText;
        _cachedLayoutTextLength = text.size();
        _cachedLayoutFormat     = format;
        _cachedLayoutSize       = D2D1::SizeF(widthDip, heightDip);
        _cachedLayoutRole       = _fontRole;
        _multilineLayoutDirty   = false;
        FillMultilineLineMetrics(_cachedMultilineLayout.get(), _cachedMultilineLineMetrics);
    }

    return _cachedMultilineLayout;
}

float TextField::GetMultilineFallbackLineHeightDip(const ControlHost* host) const noexcept
{
    IDWriteTextFormat* const format = host ? host->GetTextFormat(_fontRole, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true) : nullptr;
    if (! format)
        return EstimateMultilineFallbackLineHeightDip(host, _fontRole);
    if (_cachedFallbackLineHeightFormat.get() != format)
    {
        _cachedFallbackLineHeightDip    = EstimateMultilineFallbackLineHeightDip(host, _fontRole);
        _cachedFallbackLineHeightFormat = format;
    }
    return _cachedFallbackLineHeightDip;
}

wil::com_ptr<IDWriteTextLayout> TextField::GetOrCreateSingleLineLayout(
    const ControlHost* host, std::wstring_view text, float minimumWidthDip, float heightDip, DWRITE_READING_DIRECTION readingDirection) const noexcept
{
    if (_multiline)
    {
        return GetOrCreateSingleLineTextLayout(host, nullptr, text, _fontRole, minimumWidthDip, heightDip, readingDirection);
    }

    // A masked field's cache may take its plaintext (while revealed): it is then wiped whenever it is dropped or replaced.
    _singleLineLayoutCacheSecret = _singleLineLayoutCacheSecret || _masked;
    return GetOrCreateSingleLineTextLayout(
        host, &_singleLineLayoutCache, text, _fontRole, minimumWidthDip, heightDip, readingDirection, _singleLineLayoutCacheSecret);
}

void TextField::InvalidateSingleLineLayoutCache() const noexcept
{
    // Wiped only when it may hold a masked field's text: the field is masked, or was when the cache took its text.
    ClearSingleLineTextLayoutCache(_singleLineLayoutCache, _masked || _singleLineLayoutCacheSecret);
    _singleLineLayoutCacheSecret = false;
    _maskedSourceTextElementBoundaries.clear();
}

void TextField::InvalidateMultilineLayoutCache() const noexcept
{
    _multilineLayoutDirty = true;
    _cachedMultilineLayout.reset();
    _cachedLayoutText.reset();
    _cachedLayoutTextLength = 0u;
    _cachedLayoutFormat.reset();
    // Cleared, not released: the next layout's metrics reuse the storage.
    _cachedMultilineLineMetrics.clear();
}

void TextField::EnsureCaretVisible(const ControlHost* host, float availableWidthDip) const noexcept
{
    if (_multiline || _text->empty())
    {
        _horizontalScrollDip = 0.0f;
        return;
    }

    const SharedText displayTextBuffer           = GetDisplayText();
    const std::wstring& displayText              = *displayTextBuffer;
    const wil::com_ptr<IDWriteTextLayout> layout = GetOrCreateSingleLineLayout(host,
                                                                               displayText,
                                                                               std::max(1.0f, availableWidthDip + _horizontalScrollDip),
                                                                               std::max(1.0f, GetTextRect().bottom - GetTextRect().top),
                                                                               ResolveReadingDirection(GetFlowDirection()));
    const float caretOffset                      = MeasureCaretOffsetDip(layout.get(), displayText, ControlTextIndexToDisplayTextIndex(_caretIndex));
    const float padding                          = 6.0f;
    if (caretOffset < _horizontalScrollDip + padding)
    {
        _horizontalScrollDip = std::max(0.0f, caretOffset - padding);
    }
    else if (caretOffset > _horizontalScrollDip + availableWidthDip - padding)
    {
        _horizontalScrollDip = std::max(0.0f, caretOffset - availableWidthDip + padding);
    }
}

void TextField::EnsureMultilineCaretVisible(const ControlHost* host) noexcept
{
    if (! _multiline)
    {
        _multilineFirstVisibleLine = 0u;
        return;
    }

    const D2D1_RECT_F textRect         = GetTextRect();
    const SharedText displayTextBuffer = GetDisplayText();
    const std::wstring& displayText    = *displayTextBuffer;
    if (displayText.empty())
    {
        _multilineFirstVisibleLine = 0u;
        return;
    }

    const float viewportHeightDip                       = std::max(1.0f, textRect.bottom - textRect.top);
    const auto multilineLayout                          = GetOrCreateMultilineLayout(host, displayTextBuffer, textRect);
    const std::vector<DWRITE_LINE_METRICS>& lineMetrics = _cachedMultilineLineMetrics;
    const float fallbackLineHeightDip                   = lineMetrics.empty() ? GetMultilineFallbackLineHeightDip(host) : 0.0f;
    const MultilineViewportMetrics viewportMetrics      = BuildMultilineViewportMetrics(displayText, viewportHeightDip, lineMetrics, fallbackLineHeightDip);
    const size_t displayCaretIndex                      = ControlTextIndexToDisplayTextIndex(_caretIndex);
    const size_t caretLine =
        TryGetMultilineCaretLineIndex(multilineLayout.get(), lineMetrics, displayCaretIndex, displayText.size())
            .value_or(static_cast<size_t>(
                std::count(displayText.begin(), std::next(displayText.begin(), static_cast<std::wstring::difference_type>(displayCaretIndex)), L'\n')));
    const size_t maxFirstVisibleLine = ComputeMultilineMaxFirstVisibleLine(viewportMetrics);
    size_t firstVisibleLine          = ClampMultilineFirstVisibleLine(_multilineFirstVisibleLine, viewportMetrics);
    if (caretLine < firstVisibleLine)
    {
        firstVisibleLine = caretLine;
    }
    else if (caretLine >= firstVisibleLine + viewportMetrics.visibleLineCount)
    {
        firstVisibleLine = caretLine - viewportMetrics.visibleLineCount + 1u;
    }

    _multilineFirstVisibleLine = (std::min)(firstVisibleLine, maxFirstVisibleLine);
}

void TextField::OnFlowDirectionChanged() noexcept
{
    InvalidateSingleLineLayoutCache();
    Control::OnFlowDirectionChanged();
}

void TextField::OnHostDpiChanged(ControlHost& host) noexcept
{
    Control::OnHostDpiChanged(host);
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    if (SupportsTextInput() && host.GetFocusControl() == this)
    {
        host.SyncTextInput(this);
    }
}

void TextField::OnDensityChanged() noexcept
{
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    Control::OnDensityChanged();
}

void TextField::ResetCaretBlink(ControlHost& host) noexcept
{
    // Focus gain or input: the caret shows now. A read-only or disabled field's caret is steady and schedules nothing.
    CaretClockAccess::Restart(host, *this, ! _readOnly && IsEnabled());
}

bool TextField::NotifyChanged()
{
    const Failure::Scope failureScope;
    const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
    if (_multiline)
        InvalidateMeasure();
    if (selfLifetime.expired())
        return false;
    InvalidateSingleLineLayoutCache();
    InvalidateMultilineLayoutCache();
    FinishEditTransactionMetric();
    if (selfLifetime.expired())
        return false;

    auto onTextChanged = _onTextChanged;
    if (! onTextChanged)
    {
        RefreshAccessibilitySnapshot();
        return ! selfLifetime.expired();
    }

    // The callback reads a pinned buffer, valid however it edits or retires this field; the buffer is wiped once its last
    // owner releases it.
    const SharedText textSnapshot = _text;
    (*onTextChanged)(std::wstring_view(*textSnapshot));
    // Releasing the last callback capture can itself retire this control.
    onTextChanged.reset();
    const bool alive = ! selfLifetime.expired();
    if (alive)
    {
        // The callback may have changed this field again or reparented it. Publish only after
        // revalidation so UIA reads the committed value without touching a retired control.
        RefreshAccessibilitySnapshot();
    }
    return ! selfLifetime.expired();
}
} // namespace DxUi
