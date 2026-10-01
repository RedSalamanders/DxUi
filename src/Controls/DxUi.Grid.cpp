#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../Support/Diagnostics.h"
#include "DxUi.Internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <format>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

namespace DxUi
{
namespace
{
constexpr float kHeaderResizeHitDip          = 4.0f;
constexpr float kHeaderReorderStartDip       = 6.0f;
constexpr float kVisibleBoundaryEpsilonDip   = 0.001f;
constexpr uint64_t kSpinnerFrameDurationMs   = 120u;
constexpr uint64_t kMarqueeCycleDurationMs   = 1400u;
constexpr float kMarqueeBandFraction         = 0.32f;
constexpr std::wstring_view kSpinnerFrames[] = {L"|", L"/", L"-", L"\\"};
// A measured line counts as complete when it fits the text rectangle within this tolerance.
constexpr float kCellTextLineFitToleranceDip = 0.01f;

void ResetGridCellData(GridCellData& cellData) noexcept
{
    cellData.text.clear();
    cellData.iconText.clear();
    cellData.iconBitmap.reset();
    cellData.badgeText.clear();
    cellData.tooltipText.clear();
    cellData.iconIndex      = -1;
    cellData.kind           = GridCellKind::Text;
    cellData.iconFontRole   = FontRole::Small;
    cellData.textAlignment  = DWRITE_TEXT_ALIGNMENT_LEADING;
    cellData.multiline      = false;
    cellData.checked        = false;
    cellData.enabled        = true;
    cellData.hasSwatchValue = false;
    cellData.swatchArgb     = 0u;
    cellData.progress       = 0.0f;
    cellData.badgeTone      = AdornmentTone::Accent;
}

[[nodiscard]] bool IsAnimatedCell(const GridCellData& cellData) noexcept
{
    if (cellData.kind == GridCellKind::Spinner)
    {
        return true;
    }

    return cellData.kind == GridCellKind::Marquee && cellData.progress <= 0.0f;
}

[[nodiscard]] std::wstring BuildGridCellCopyText(const GridCellData& cellData)
{
    std::wstring text;
    if (cellData.kind == GridCellKind::Checkbox)
    {
        text.assign(cellData.checked ? L"[x]" : L"[ ]");
        if (! cellData.text.empty())
        {
            text.push_back(L' ');
        }
    }

    if (! cellData.text.empty())
    {
        text.append(cellData.text);
    }
    else if (cellData.kind == GridCellKind::ColorSwatch && cellData.hasSwatchValue)
    {
        text.append(std::format(L"#{:08X}", cellData.swatchArgb));
    }
    else if (cellData.kind == GridCellKind::IconText && ! cellData.iconText.empty())
    {
        text.append(cellData.iconText);
    }

    if (! cellData.badgeText.empty())
    {
        if (! text.empty())
        {
            text.append(L" ");
        }
        text.push_back(L'[');
        text.append(cellData.badgeText);
        text.push_back(L']');
    }

    return text;
}

[[nodiscard]] float ClampScroll(float value, float extent) noexcept
{
    if (! std::isfinite(value) || ! std::isfinite(extent) || extent <= 0.0f)
    {
        return 0.0f;
    }

    return std::clamp(value, 0.0f, extent);
}

[[nodiscard]] float SanitizeNonNegative(float value) noexcept
{
    return (std::isfinite(value) && value > 0.0f) ? value : 0.0f;
}

[[nodiscard]] D2D1_RECT_F NormalizeFiniteRect(const D2D1_RECT_F& rect) noexcept
{
    const float left   = std::isfinite(rect.left) ? rect.left : 0.0f;
    const float top    = std::isfinite(rect.top) ? rect.top : 0.0f;
    const float right  = std::isfinite(rect.right) ? std::max(left, rect.right) : left;
    const float bottom = std::isfinite(rect.bottom) ? std::max(top, rect.bottom) : top;
    return D2D1::RectF(left, top, right, bottom);
}

[[nodiscard]] D2D1_RECT_F ClipRectToRect(const D2D1_RECT_F& rect, const D2D1_RECT_F& clip) noexcept
{
    const D2D1_RECT_F normalizedRect = NormalizeFiniteRect(rect);
    const D2D1_RECT_F normalizedClip = NormalizeFiniteRect(clip);
    return NormalizeFiniteRect(D2D1::RectF(std::max(normalizedRect.left, normalizedClip.left),
                                           std::max(normalizedRect.top, normalizedClip.top),
                                           std::min(normalizedRect.right, normalizedClip.right),
                                           std::min(normalizedRect.bottom, normalizedClip.bottom)));
}

[[nodiscard]] float ResolveDensityScaledMetricDip(float baseDip, float minimumDip, Density density) noexcept
{
    const float scale = density == Density::Compact ? 0.82f : 1.0f;
    return std::max(minimumDip, baseDip * scale);
}

[[nodiscard]] bool IsNonEmptyRect(const D2D1_RECT_F& rect) noexcept
{
    return rect.right > rect.left && rect.bottom > rect.top;
}

[[nodiscard]] size_t ResolveVisibleRowStartOffset(float offsetDip, float rowHeightDip, size_t maxRowCount) noexcept
{
    const float safeRowHeightDip = std::max(1.0f, rowHeightDip);
    const float normalizedOffset = std::max(0.0f, offsetDip) + kVisibleBoundaryEpsilonDip;
    return std::min(maxRowCount, static_cast<size_t>(std::floor(normalizedOffset / safeRowHeightDip)));
}

[[nodiscard]] size_t ResolveVisibleRowEndOffset(float offsetDip, float rowHeightDip, size_t maxRowCount) noexcept
{
    const float safeRowHeightDip = std::max(1.0f, rowHeightDip);
    const float normalizedOffset = std::max(0.0f, offsetDip - kVisibleBoundaryEpsilonDip);
    return std::min(maxRowCount, static_cast<size_t>(std::ceil(normalizedOffset / safeRowHeightDip)));
}

struct GridResolvedRowVisuals final
{
    D2D1_COLOR_F fill  = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F text  = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    bool usesRainbow   = false;
    bool roundedFill   = false;
    bool showSeparator = true;
};

struct GridResolvedCellVisuals final
{
    std::optional<GridCheckboxVisualStyle> checkbox;
    std::optional<GridSwatchVisualStyle> swatch;
    std::optional<GridBadgeVisualStyle> badge;
};

[[nodiscard]] GridResolvedRowVisuals ResolveGridRowVisuals(
    const ThemePalette& theme, const GridRowStyle& rowStyle, size_t rowIndex, bool selected, bool focused, bool hovered, GridVisualMode visualMode) noexcept
{
    GridResolvedRowVisuals visuals{};
    visuals.text = theme.text;

    if (visualMode == GridVisualMode::FolderView)
    {
        visuals.roundedFill   = true;
        visuals.showSeparator = false;

        switch (rowStyle.tone)
        {
            case GridRowTone::Info:
                visuals.fill = theme.infoFill;
                visuals.text = theme.infoText;
                break;
            case GridRowTone::Warning:
                visuals.fill = theme.warningFill;
                visuals.text = theme.warningText;
                break;
            case GridRowTone::Error:
                visuals.fill = theme.errorFill;
                visuals.text = theme.errorText;
                break;
            case GridRowTone::None: break;
        }

        const bool allowSelectionRainbow =
            theme.rainbowMode && ! theme.highContrast && selected && (rowStyle.folderViewRainbowHash32.has_value() || ! rowStyle.rainbowSeed.empty());
        if (allowSelectionRainbow)
        {
            visuals.usesRainbow = true;
            visuals.fill   = rowStyle.folderViewRainbowHash32.has_value() ? RainbowFolderViewSelectionTint(rowStyle.folderViewRainbowHash32.value(), theme.dark)
                                                                          : RainbowMenuSelectionTint(rowStyle.rainbowSeed, theme.dark);
            visuals.fill.a = focused ? std::clamp(theme.selectionFill.a, 0.0f, 1.0f) : std::clamp(theme.selectionInactiveFill.a, 0.0f, 1.0f);
            visuals.text   = ChooseContrastingTextColor(CompositeOverBackground(visuals.fill, theme.surfaceBackground));
        }
        else if (selected)
        {
            visuals.fill = focused ? theme.selectionFill : theme.selectionInactiveFill;
            visuals.text = theme.selectionText;
        }
        else if (visuals.fill.a <= 0.0f && hovered)
        {
            visuals.fill = theme.hoverFill;
        }

        return visuals;
    }

    const D2D1_COLOR_F baseFill = ((rowIndex % 2u) == 0u) ? theme.surfaceBackground : BlendColor(theme.surfaceBackground, theme.windowBackground, 0.42f);
    visuals.fill                = baseFill;

    const bool allowRainbow = theme.rainbowMode && ! theme.highContrast && ! rowStyle.rainbowSeed.empty();
    if (allowRainbow)
    {
        visuals.usesRainbow            = true;
        const D2D1_COLOR_F rainbowFill = RainbowTint(rowStyle.rainbowSeed, theme.dark);
        if (selected)
        {
            visuals.fill = focused ? rainbowFill : BlendColor(baseFill, rainbowFill, theme.dark ? 0.58f : 0.44f);
        }
        else
        {
            const float tintAmount = ((rowIndex % 2u) == 0u) ? (theme.dark ? 0.32f : 0.20f) : (theme.dark ? 0.54f : 0.34f);
            visuals.fill           = BlendColor(baseFill, rainbowFill, tintAmount);
        }
        visuals.text = ChooseContrastingTextColor(visuals.fill);
    }
    else
    {
        switch (rowStyle.tone)
        {
            case GridRowTone::Info:
                visuals.fill = theme.infoFill;
                visuals.text = theme.infoText;
                break;
            case GridRowTone::Warning:
                visuals.fill = theme.warningFill;
                visuals.text = theme.warningText;
                break;
            case GridRowTone::Error:
                visuals.fill = theme.errorFill;
                visuals.text = theme.errorText;
                break;
            case GridRowTone::None: break;
        }

        if (selected)
        {
            visuals.fill = focused ? theme.selectionFill : theme.selectionInactiveFill;
            visuals.text = theme.selectionText;
        }
    }

    if (! selected && hovered)
    {
        visuals.fill = BlendColor(visuals.fill, theme.hoverFill, 0.55f);
        if (visuals.usesRainbow)
        {
            visuals.text = ChooseContrastingTextColor(visuals.fill);
        }
    }

    return visuals;
}

// Spinner and marquee cells draw their own single-line captions. Every other
// multiline cell is painted, and its tooltip decided, by the clamped layout.
[[nodiscard]] bool UsesMultilineCellText(const GridCellData& cellData) noexcept
{
    return cellData.multiline && cellData.kind != GridCellKind::Spinner && cellData.kind != GridCellKind::Marquee;
}

// Every character DirectWrite breaks a line on: CR, LF, NEL, VT, FF, LS and PS.
[[nodiscard]] bool IsCellTextLineBreak(wchar_t ch) noexcept
{
    return ch == L'\r' || ch == L'\n' || ch == L'\x85' || ch == L'\v' || ch == L'\f' || ch == L'\u2028' || ch == L'\u2029';
}

// The Basic Multilingual Plane's default-ignorable characters (Unicode's Default_Ignorable_Code_Point): soft hyphen,
// combining grapheme joiner, Arabic letter mark, Hangul fillers, Khmer inherent vowels, Mongolian variation selectors,
// zero-width space and joiners, directional marks, embeddings and isolates, word joiner and invisible operators,
// variation selectors and the byte order mark. They paint nothing.
[[nodiscard]] bool IsDefaultIgnorableText(wchar_t ch) noexcept
{
    return ch == L'\xAD' || ch == L'\x34F' || ch == L'\x61C' || (ch >= L'\x115F' && ch <= L'\x1160') || (ch >= L'\x17B4' && ch <= L'\x17B5') ||
           (ch >= L'\x180B' && ch <= L'\x180F') || (ch >= L'\x200B' && ch <= L'\x200F') || (ch >= L'\x202A' && ch <= L'\x202E') ||
           (ch >= L'\x2060' && ch <= L'\x206F') || ch == L'\x3164' || (ch >= L'\xFE00' && ch <= L'\xFE0F') || ch == L'\xFEFF' || ch == L'\xFFA0' ||
           (ch >= L'\xFFF0' && ch <= L'\xFFF8');
}

// Blank for the trailing-line trim: white space other than a line break, and characters that paint nothing.
[[nodiscard]] bool IsCellTextBlank(wchar_t ch) noexcept
{
    if (ch == L' ' || ch == L'\t')
        return true;
    if (ch < L'\x80' || IsCellTextLineBreak(ch))
        return false;
    if (IsDefaultIgnorableText(ch))
        return true;
    WORD type = 0;
    return GetStringTypeW(CT_CTYPE1, &ch, 1, &type) != FALSE && (type & C1_SPACE) != 0u;
}

// Trailing line breaks, and blank lines after them, only add empty DirectWrite
// lines. They are neither omitted content nor part of the vertically centred
// text. Blanks ending the last real line stay: the marker logic handles them.
[[nodiscard]] size_t FindCellTextContentEnd(std::wstring_view text) noexcept
{
    size_t end = text.size();
    for (;;)
    {
        size_t lineStart = end;
        while (lineStart > 0u && IsCellTextBlank(text[lineStart - 1u]))
        {
            --lineStart;
        }
        if (lineStart == 0u || ! IsCellTextLineBreak(text[lineStart - 1u]))
        {
            return lineStart == 0u ? 0u : end;
        }
        end = lineStart - 1u;
    }
}

// How text continues at index: 0 at no line break, 1 at a line break, 2 at a CR LF pair. The shaping loop reads the
// break that ends a prefix and, for a CR, whether an LF pairs with it, so a prefix entry records both.
[[nodiscard]] uint8_t CellTextBreakShapeAt(std::wstring_view text, size_t index) noexcept
{
    if (index >= text.size() || ! IsCellTextLineBreak(text[index]))
        return 0u;
    return text[index] == L'\r' && index + 1u < text.size() && text[index + 1u] == L'\n' ? 2u : 1u;
}

// Whether a unit can belong to right-to-left or explicitly directed text: Hebrew, Arabic and the other right-to-left
// scripts (their supplementary-plane blocks too, seen here by their high surrogates), their presentation forms, and
// the directional marks and controls. Only such text lets later text in a line reorder what shows before it.
[[nodiscard]] bool MayReorderCellText(wchar_t ch) noexcept
{
    return (ch >= L'\x0590' && ch <= L'\x08FF') || (ch >= L'\xFB1D' && ch <= L'\xFDFF') || (ch >= L'\xFE70' && ch <= L'\xFEFE') || ch == L'\x200E' ||
           ch == L'\x200F' || (ch >= L'\x202A' && ch <= L'\x202E') || (ch >= L'\x2066' && ch <= L'\x2069') || (ch >= L'\xD802' && ch <= L'\xD803') ||
           (ch >= L'\xD83A' && ch <= L'\xD83B');
}

// Multiline cell layouts: tables of kCellTextLayoutInitialEntries growing to kCellTextLayoutMaxEntries while the
// values of the current and the previous paint overflow a set (see DxUi.Internal.h). An entry keeps at most
// kMaxRetainedCellTextUnits units of key text; a layout that needs more (a whole long right-to-left line) is built per
// use.
constexpr size_t kCellTextLayoutInitialEntries = 32u;
constexpr size_t kCellTextLayoutMaxEntries     = 16384u;
constexpr size_t kMaxRetainedCellTextUnits     = 4096u;

// Cell boxes come from scrolled coordinates, so boxes of one size differ in their last float bits from row to row. A
// key rounds them to 1/64 DIP, so such cells share one entry: its layout keeps the exact box of the cell that built it,
// and the others differ from it by far less than a pixel.
[[nodiscard]] float RoundCellTextKeyDip(float value) noexcept
{
    return std::round(value * 64.0f) / 64.0f;
}

enum class CellTextDirection : uint8_t
{
    None,
    LeftToRight,
    RightToLeft,
};

// A supplementary-plane character's direction: the right-to-left blocks (U+10800 to U+10FFF: Phoenician, Kharoshthi,
// Avestan and the like; U+1E800 to U+1EFFF: Mende Kikakui, Adlam, the Arabic mathematical letters) read right to left;
// the emoji and symbol blocks, tags and variation selectors have no direction; the rest (historic scripts, CJK
// extensions, mathematical letters) read left to right.
[[nodiscard]] CellTextDirection ClassifySupplementaryDirection(char32_t codePoint) noexcept
{
    if ((codePoint >= 0x10800u && codePoint <= 0x10FFFu) || (codePoint >= 0x1E800u && codePoint <= 0x1EFFFu))
        return CellTextDirection::RightToLeft;
    if ((codePoint >= 0x1F000u && codePoint <= 0x1FBFFu) || (codePoint >= 0xE0000u && codePoint <= 0xE01EFu))
        return CellTextDirection::None;
    return CellTextDirection::LeftToRight;
}

// U+200E LEFT-TO-RIGHT MARK or U+200F RIGHT-TO-LEFT MARK when the text's last strong character runs against the
// paragraph direction. A trailing U+2026 ellipsis has no direction of its own and would otherwise take the paragraph's, landing
// before the start of right-to-left text in a left-to-right cell (and the reverse); after the mark it takes the
// direction of the text it ends. A surrogate pair is read as its character, so an emoji ending the text is passed over.
[[nodiscard]] std::optional<wchar_t> ResolveOmissionMarkDirection(std::wstring_view text, DWRITE_READING_DIRECTION paragraphDirection) noexcept
{
    size_t index = text.size();
    while (index > 0u)
    {
        const wchar_t unit          = text[--index];
        CellTextDirection direction = CellTextDirection::None;
        if (IS_LOW_SURROGATE(unit) && index > 0u && IS_HIGH_SURROGATE(text[index - 1u]))
        {
            const char32_t high = text[--index];
            direction           = ClassifySupplementaryDirection(0x10000u + ((high - 0xD800u) << 10u) + (static_cast<char32_t>(unit) - 0xDC00u));
        }
        else if (! IS_HIGH_SURROGATE(unit) && ! IS_LOW_SURROGATE(unit))
        {
            WORD type = 0;
            if (! GetStringTypeW(CT_CTYPE2, &unit, 1, &type))
                return std::nullopt;
            direction = type == C2_LEFTTORIGHT   ? CellTextDirection::LeftToRight
                        : type == C2_RIGHTTOLEFT ? CellTextDirection::RightToLeft
                                                 : CellTextDirection::None;
        }
        if (direction == CellTextDirection::LeftToRight)
            return paragraphDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT ? std::optional<wchar_t>(L'\x200E') : std::nullopt;
        if (direction == CellTextDirection::RightToLeft)
            return paragraphDirection == DWRITE_READING_DIRECTION_LEFT_TO_RIGHT ? std::optional<wchar_t>(L'\x200F') : std::nullopt;
    }
    return std::nullopt;
}

[[nodiscard]] GridResolvedCellVisuals ResolveGridCellVisuals(
    const ThemePalette& theme, const GridResolvedRowVisuals& rowVisuals, bool selected, bool hovered, const GridCellData& cellData) noexcept
{
    GridResolvedCellVisuals visuals{};
    if (cellData.kind == GridCellKind::Checkbox)
    {
        visuals.checkbox = ResolveGridCheckboxVisualStyle(theme, rowVisuals.fill, rowVisuals.text, cellData.enabled, hovered, selected, cellData.checked);
    }

    if (cellData.kind == GridCellKind::ColorSwatch)
    {
        visuals.swatch = ResolveGridSwatchVisualStyle(theme, rowVisuals.fill, rowVisuals.text, selected, cellData);
    }

    if (! cellData.badgeText.empty())
    {
        visuals.badge = ResolveGridBadgeVisualStyle(theme, rowVisuals.fill, rowVisuals.text, selected, cellData.badgeTone);
    }

    return visuals;
}

[[nodiscard]] std::wstring_view SpinnerFrameForTick(uint64_t tickMs) noexcept
{
    const size_t frameIndex = static_cast<size_t>((tickMs / kSpinnerFrameDurationMs) % std::size(kSpinnerFrames));
    return kSpinnerFrames[frameIndex];
}

[[nodiscard]] D2D1_RECT_F ComputeProgressFillRect(const D2D1_RECT_F& trackRect, float progress) noexcept
{
    const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
    return D2D1::RectF(trackRect.left, trackRect.top, trackRect.left + ((trackRect.right - trackRect.left) * clampedProgress), trackRect.bottom);
}

[[nodiscard]] D2D1_RECT_F ComputeMarqueeFillRect(const D2D1_RECT_F& trackRect, uint64_t tickMs) noexcept
{
    const float widthDip      = std::max(0.0f, trackRect.right - trackRect.left);
    const float bandWidthDip  = std::max(12.0f, widthDip * kMarqueeBandFraction);
    const float cyclePosition = static_cast<float>(tickMs % kMarqueeCycleDurationMs) / static_cast<float>(kMarqueeCycleDurationMs);
    const float travelDip     = widthDip + bandWidthDip;
    const float left          = trackRect.left - bandWidthDip + (travelDip * cyclePosition);
    return D2D1::RectF(left, trackRect.top, std::min(trackRect.right, left + bandWidthDip), trackRect.bottom);
}

[[nodiscard]] std::vector<GridGroupDesc> CollectOrderedGroups(const IGridModel* model)
{
    std::vector<GridGroupDesc> groups;
    if (! model)
    {
        return groups;
    }

    const size_t rowCount   = model->GetRowCount();
    const size_t groupCount = model->GetGroupCount();
    if (rowCount == 0u || groupCount == 0u)
    {
        return groups;
    }

    groups.reserve(groupCount);
    for (size_t groupIndex = 0u; groupIndex < groupCount; ++groupIndex)
    {
        GridGroupDesc group = model->GetGroup(groupIndex);
        if (group.rowCount == 0u || group.startRowIndex >= rowCount)
        {
            continue;
        }

        group.rowCount = std::min(group.rowCount, rowCount - group.startRowIndex);
        groups.push_back(std::move(group));
    }

    std::ranges::sort(groups,
                      [](const GridGroupDesc& lhs, const GridGroupDesc& rhs) noexcept
    {
        if (lhs.startRowIndex != rhs.startRowIndex)
        {
            return lhs.startRowIndex < rhs.startRowIndex;
        }
        return lhs.stableId < rhs.stableId;
    });

    std::vector<GridGroupDesc> sanitized;
    sanitized.reserve(groups.size());
    size_t nextAvailableRow = 0u;
    for (auto group : groups)
    {
        const size_t groupEnd = group.startRowIndex + group.rowCount;
        if (groupEnd <= nextAvailableRow)
        {
            continue;
        }

        if (group.startRowIndex < nextAvailableRow)
        {
            group.rowCount -= (nextAvailableRow - group.startRowIndex);
            group.startRowIndex = nextAvailableRow;
        }

        if (group.rowCount == 0u)
        {
            continue;
        }

        nextAvailableRow = group.startRowIndex + group.rowCount;
        sanitized.push_back(std::move(group));
    }

    return sanitized;
}

[[nodiscard]] std::vector<size_t> CollectVisibleRowIndices(size_t rowCount, std::span<const GridGroupDesc> groups)
{
    std::vector<size_t> visibleRows;
    visibleRows.reserve(rowCount);

    size_t nextUngroupedRow = 0u;
    for (const GridGroupDesc& group : groups)
    {
        for (size_t rowIndex = nextUngroupedRow; rowIndex < group.startRowIndex; ++rowIndex)
        {
            visibleRows.push_back(rowIndex);
        }

        if (! group.collapsed)
        {
            const size_t groupEnd = group.startRowIndex + group.rowCount;
            for (size_t rowIndex = group.startRowIndex; rowIndex < groupEnd; ++rowIndex)
            {
                visibleRows.push_back(rowIndex);
            }
        }

        nextUngroupedRow = group.startRowIndex + group.rowCount;
    }

    for (size_t rowIndex = nextUngroupedRow; rowIndex < rowCount; ++rowIndex)
    {
        visibleRows.push_back(rowIndex);
    }

    return visibleRows;
}

[[nodiscard]] std::vector<uint64_t> CollectVisibleOrderedRowIds(const IGridModel* model, std::span<const GridGroupDesc> groups)
{
    std::vector<uint64_t> rowIds;
    if (! model)
    {
        return rowIds;
    }

    const size_t rowCount = model->GetRowCount();
    rowIds.reserve(rowCount);

    const auto appendRows = [&](size_t beginRow, size_t endRow)
    {
        for (size_t rowIndex = beginRow; rowIndex < endRow; ++rowIndex)
        {
            rowIds.push_back(model->GetStableRowId(rowIndex));
        }
    };

    size_t nextUngroupedRow = 0u;
    for (const GridGroupDesc& group : groups)
    {
        appendRows(nextUngroupedRow, group.startRowIndex);

        const size_t groupEnd = group.startRowIndex + group.rowCount;
        if (! group.collapsed)
        {
            appendRows(group.startRowIndex, groupEnd);
        }

        nextUngroupedRow = groupEnd;
    }

    appendRows(nextUngroupedRow, rowCount);
    return rowIds;
}

[[nodiscard]] bool IsRowVisibleByGroupLayout(size_t rowIndex, std::span<const GridGroupDesc> groups) noexcept
{
    for (const GridGroupDesc& group : groups)
    {
        if (rowIndex < group.startRowIndex)
        {
            return true;
        }

        const size_t groupEnd = group.startRowIndex + group.rowCount;
        if (rowIndex < groupEnd)
        {
            return ! group.collapsed;
        }
    }

    return true;
}

[[nodiscard]] bool EqualRowSelection(std::span<const uint64_t> lhs, std::span<const uint64_t> rhs) noexcept
{
    return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

constexpr uint64_t kSortGlyphTransitionDurationMs = 140u;

[[nodiscard]] D2D1_COLOR_F WithOpacity(const D2D1_COLOR_F& color, float opacity) noexcept
{
    return D2D1::ColorF(color.r, color.g, color.b, std::clamp(opacity, 0.0f, 1.0f) * color.a);
}

void DrawSortGlyph(ControlHost& host, const D2D1_RECT_F& rect, SortDirection direction, const D2D1_COLOR_F& color)
{
    if (direction == SortDirection::None)
    {
        return;
    }

    const D2D1_RECT_F glyphRect = D2D1::RectF(rect.right - 20.0f, rect.top, rect.right, rect.bottom);
    DrawChevronGlyph(host, glyphRect, direction == SortDirection::Ascending ? ChevronDirection::Up : ChevronDirection::Down, color);
}

void DrawGroupDisclosureGlyph(ControlHost& host, const D2D1_RECT_F& rect, bool collapsed, const D2D1_COLOR_F& color)
{
    const D2D1_RECT_F glyphRect = D2D1::RectF(rect.left + 4.0f, rect.top, rect.left + 24.0f, rect.bottom);
    DrawDisclosureChevron(host, glyphRect, collapsed ? 0.0f : 1.0f, color);
}
} // namespace

bool IconTextUsesIconFont(std::wstring_view iconText) noexcept
{
    for (const wchar_t ch : iconText)
    {
        if (ch >= 0xE000 && ch <= 0xF8FF)
        {
            return true;
        }
    }

    return false;
}

FontRole ResolveIconTextFontRole(std::wstring_view iconText) noexcept
{
    return IconTextUsesIconFont(iconText) ? FontRole::Icon : FontRole::Small;
}

GridRowStyle IGridModel::GetRowStyle(size_t /*rowIndex*/) const
{
    return {};
}

size_t IGridModel::GetGroupCount() const noexcept
{
    return 0u;
}

GridGroupDesc IGridModel::GetGroup(size_t /*groupIndex*/) const
{
    return {};
}

uint64_t IGridModel::GetStableRowId(size_t rowIndex) const noexcept
{
    return static_cast<uint64_t>(rowIndex);
}

void IGridDelegate::OnGridSortRequested(const GridSortSpec& /*sortSpec*/)
{
}

void IGridDelegate::OnGridSelectionChanged(Grid& /*sender*/)
{
    OnGridSelectionChanged();
}

void IGridDelegate::OnGridSelectionChanged()
{
}

void IGridDelegate::OnGridCheckboxToggled(Grid& /*sender*/, size_t rowIndex, size_t columnIndex, bool checked)
{
    OnGridCheckboxToggled(rowIndex, columnIndex, checked);
}

void IGridDelegate::OnGridCheckboxToggled(size_t /*rowIndex*/, size_t /*columnIndex*/, bool /*checked*/)
{
}

void IGridDelegate::OnGridRowActivated(Grid& /*sender*/, size_t rowIndex)
{
    OnGridRowActivated(rowIndex);
}

void IGridDelegate::OnGridRowActivated(size_t /*rowIndex*/)
{
}

void IGridDelegate::OnGridContextMenu(Grid& /*sender*/, size_t rowIndex, POINT screenPoint)
{
    OnGridContextMenu(rowIndex, screenPoint);
}

void IGridDelegate::OnGridContextMenu(size_t /*rowIndex*/, POINT /*screenPoint*/)
{
}

void IGridDelegate::OnGridGroupToggled(Grid& /*sender*/, uint64_t groupStableId, bool collapsed)
{
    OnGridGroupToggled(groupStableId, collapsed);
}

void IGridDelegate::OnGridGroupToggled(uint64_t /*groupStableId*/, bool /*collapsed*/)
{
}

wil::com_ptr<ID2D1Bitmap1> IGridDelegate::GetGridIconBitmap(const Grid& /*sender*/,
                                                            int /*iconIndex*/,
                                                            float /*targetDipSize*/,
                                                            ID2D1DeviceContext* /*d2dContext*/)
{
    return nullptr;
}

namespace
{
// Sorts `ids` unless they already ascend, which they do when a model's stable ids grow with its row order.
void SortRowIds(std::vector<uint64_t>& ids)
{
    if (! std::ranges::is_sorted(ids))
    {
        std::ranges::sort(ids);
    }
}
} // namespace

// Every mutator below leaves _sortedRowIds holding exactly the ids of _selectedRowIds, ascending (and as many times as
// _selectedRowIds holds each), because IsSelected reads only the sorted copy.
void GridSelectionModel::Clear() noexcept
{
    _selectedRowIds.clear();
    _sortedRowIds.clear();
    _anchorRowId.reset();
}

void GridSelectionModel::SetSingle(uint64_t rowId) noexcept
{
    _selectedRowIds.assign(1u, rowId);
    _sortedRowIds.assign(1u, rowId);
    _anchorRowId = rowId;
}

void GridSelectionModel::Toggle(uint64_t rowId) noexcept
{
    const auto sortedIt = std::ranges::lower_bound(_sortedRowIds, rowId);
    if (sortedIt != _sortedRowIds.end() && *sortedIt == rowId)
    {
        // The first occurrence leaves the ordered ids and one occurrence the sorted ones, so an id held twice stays selected.
        _selectedRowIds.erase(std::ranges::find(_selectedRowIds, rowId));
        _sortedRowIds.erase(sortedIt);
        if (_anchorRowId == rowId)
        {
            _anchorRowId = _selectedRowIds.empty() ? std::optional<uint64_t>() : std::optional<uint64_t>(_selectedRowIds.front());
        }
        return;
    }

    _selectedRowIds.push_back(rowId);
    _sortedRowIds.insert(sortedIt, rowId);
    if (! _anchorRowId)
    {
        _anchorRowId = rowId;
    }
}

void GridSelectionModel::SetRange(const std::vector<uint64_t>& orderedRowIds, uint64_t anchorRowId, uint64_t currentRowId)
{
    const auto anchorIt  = std::ranges::find(orderedRowIds, anchorRowId);
    const auto currentIt = std::ranges::find(orderedRowIds, currentRowId);
    if (anchorIt == orderedRowIds.end() || currentIt == orderedRowIds.end())
    {
        SetSingle(currentRowId);
        return;
    }

    // Both copies get their room before either changes, so a failed allocation leaves the selection as it was.
    const auto [first, last] = std::minmax(anchorIt, currentIt);
    const size_t count       = static_cast<size_t>(last - first) + 1u;
    _selectedRowIds.reserve(count);
    _sortedRowIds.reserve(count);
    _selectedRowIds.assign(first, last + 1);
    _sortedRowIds.assign(first, last + 1);
    SortRowIds(_sortedRowIds);
    _anchorRowId = anchorRowId;
}

void GridSelectionModel::PreserveOrdered(const std::vector<uint64_t>& orderedRowIds)
{
    if (_selectedRowIds.empty())
    {
        return;
    }

    // What stays is each occurrence in orderedRowIds of an id that is selected now, in that order. It is built beside the current
    // ids and moved in at the end, so a failed allocation leaves the selection as it was.
    std::vector<uint64_t> kept;
    kept.reserve((std::min)(orderedRowIds.size(), _selectedRowIds.size()));
    for (const uint64_t rowId : orderedRowIds)
    {
        if (IsSelected(rowId))
        {
            kept.push_back(rowId);
        }
    }
    if (kept == _selectedRowIds)
    {
        // The usual data change leaves the selection as it is: its ascending copy is right, and so is the anchor, which is one of
        // these ids (every mutator keeps it so).
        return;
    }
    std::vector<uint64_t> keptSorted(kept);
    SortRowIds(keptSorted);

    if (_anchorRowId && ! std::ranges::binary_search(keptSorted, _anchorRowId.value()))
    {
        _anchorRowId = kept.empty() ? std::optional<uint64_t>() : std::optional<uint64_t>(kept.front());
    }
    _selectedRowIds = std::move(kept);
    _sortedRowIds   = std::move(keptSorted);
}

bool GridSelectionModel::IsSelected(uint64_t rowId) const noexcept
{
    return std::ranges::binary_search(_sortedRowIds, rowId);
}

std::optional<uint64_t> GridSelectionModel::GetAnchor() const noexcept
{
    return _anchorRowId;
}

size_t GridSelectionModel::GetCount() const noexcept
{
    return _selectedRowIds.size();
}

std::span<const uint64_t> GridSelectionModel::GetOrderedSelection() const noexcept
{
    return _selectedRowIds;
}

Grid::Grid()
{
    SetFocusable(true);
}

void Grid::SetModel(IGridModel* model) noexcept
{
    // Non-owning pointer assignment. Caller responsible for model lifetime.
    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    ReleaseCellTextResources(); // Also returns tables the old model grew.
    _model                            = model;
    _lastPaintHadAnimatedVisibleCells = false;
    _animatedVisibleCellStateValid    = false;
    _columnWidths.clear();
    _columnDisplayOrder.clear();
    _columnDisplayIndexByModel.clear();
    _hoveredRow.reset();
    _hoveredColumn.reset();
    _activeColumn.reset();
    _pressedHeaderColumn.reset();
    _dragReorderColumn.reset();
    _dragReorderTargetDisplayIndex = 0u;
    _resizeColumn.reset();
    _pressedHeaderOriginXDip    = 0.0f;
    _dragVerticalThumb          = false;
    _dragHorizontalThumb        = false;
    _verticalScrollbarHotPart   = ScrollbarHotPart::None;
    _horizontalScrollbarHotPart = ScrollbarHotPart::None;
    _dragThumbOffsetDip         = 0.0f;
    if (_model)
    {
        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
        ReconcileSelectionForVisibleRows(groups);
    }
    else
    {
        _selectionModel.Clear();
    }
    ClampScrollOffsets();
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();
}

void Grid::ReleaseCellTextResources() noexcept
{
    std::vector<CellTextLayoutCache>().swap(_cellTextLayouts);
    std::vector<CellDisplayLayoutCache>().swap(_cellDisplayLayouts);
    std::wstring().swap(_cellVisibleText);
    _cellEllipsis.reset();
    _cellEllipsisFormat.reset();
}

void Grid::PropagateHost(ControlHost* host) noexcept
{
    // The layouts and the ellipsis belong to the host whose text formats built them: a grid removed from it, or moved to
    // another host, keeps none, and a next paint (if there is one) rebuilds what it shows.
    if (GetHost() != host)
        ReleaseCellTextResources();
    Control::PropagateHost(host);
}

void Grid::OnHidden() noexcept
{
    // A painting grid keeps the layouts of its visible cells so that repaints and scrolling shape nothing. A grid that is
    // not painted has no next paint to release the ones it used last, so it returns them all until it is shown again.
    ReleaseCellTextResources();
    Control::OnHidden();
}

void Grid::SetDelegate(IGridDelegate* delegate) noexcept
{
    _delegate = delegate;
}

void Grid::SetSelectionMode(GridSelectionMode mode) noexcept
{
    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    _selectionMode = mode;
    if (_selectionMode == GridSelectionMode::Single && _selectionModel.GetCount() > 1u)
    {
        const auto selection = _selectionModel.GetOrderedSelection();
        if (! selection.empty())
        {
            _selectionModel.SetSingle(selection.front());
        }
    }
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();
}

void Grid::SetVisualMode(GridVisualMode mode) noexcept
{
    if (_visualMode == mode)
    {
        return;
    }

    _visualMode = mode;
    if (ControlHost* host = GetHost())
    {
        Invalidate(*host);
    }
}

void Grid::SetRowHeightDip(float rowHeightDip) noexcept
{
    _effectiveRowHeightDip = std::nullopt;
    _rowHeightBaseDip      = std::max(kMinimumInteractiveTextRowHeightDip, rowHeightDip);
    OnDensityChanged();
}

void Grid::SetEffectiveRowHeightDip(float rowHeightDip) noexcept
{
    _effectiveRowHeightDip = std::max(kMinimumInteractiveTextRowHeightDip, rowHeightDip);
    OnDensityChanged();
}

void Grid::SetHeaderHeightDip(float headerHeightDip) noexcept
{
    _headerHeightBaseDip = std::max(0.0f, headerHeightDip);
    OnDensityChanged();
}

void Grid::SetCellTextFontRole(FontRole fontRole) noexcept
{
    if (_cellTextFontRole == fontRole)
    {
        return;
    }

    _cellTextFontRole = fontRole;
    RequestInvalidate();
}

void Grid::SetIconSizeDip(float iconSizeDip) noexcept
{
    const float normalized = std::clamp(iconSizeDip, 1.0f, 64.0f);
    if (std::abs(_iconSizeDip - normalized) <= 0.01f)
    {
        return;
    }

    _iconSizeDip = normalized;
    RequestInvalidate();
}

void Grid::SetLineClamp(uint32_t lineClamp) noexcept
{
    const auto normalized = std::max(1u, lineClamp);
    if (_lineClamp != normalized)
    {
        _lineClamp = normalized;
        RequestInvalidate();
    }
}

void Grid::OnDensityChanged() noexcept
{
    Control::OnDensityChanged();
    _rowHeightDip = _effectiveRowHeightDip.value_or(ResolveDensityScaledMetricDip(_rowHeightBaseDip, kMinimumInteractiveTextRowHeightDip, GetDensity()));
    _groupHeaderHeightDip = ResolveDensityScaledMetricDip(_groupHeaderHeightBaseDip, kMinimumInteractiveTextRowHeightDip, GetDensity());
    _headerHeightDip =
        _headerHeightBaseDip <= 0.0f ? 0.0f : ResolveDensityScaledMetricDip(_headerHeightBaseDip, kMinimumInteractiveTextRowHeightDip, GetDensity());
}

void Grid::SetEmptyStateText(std::wstring text)
{
    _emptyStateText = std::move(text);
    RequestInvalidate();
}

std::wstring_view Grid::GetEmptyStateText() const noexcept
{
    return _emptyStateText;
}

void Grid::SetHeaderBusy(bool busy) noexcept
{
    _headerBusy = busy;
}

void Grid::SetHeaderBusyColumn(std::optional<size_t> columnIndex) noexcept
{
    _headerBusyColumn = columnIndex;
}

void Grid::SetSortSpec(const GridSortSpec& sortSpec) noexcept
{
    const bool changed = _sortSpec.columnIndex != sortSpec.columnIndex || _sortSpec.direction != sortSpec.direction;
    if (! changed)
    {
        return;
    }

    const GridSortSpec previousSortSpec = _sortSpec;
    _sortSpec                           = sortSpec;
    if (previousSortSpec.direction == SortDirection::None && sortSpec.direction == SortDirection::None)
    {
        _sortGlyphTransition = {};
        return;
    }

    _sortGlyphTransition.from        = previousSortSpec;
    _sortGlyphTransition.to          = sortSpec;
    _sortGlyphTransition.startTickMs = ::GetTickCount64();
    _sortGlyphTransition.active      = true;
}

GridSortSpec Grid::GetSortSpec() const noexcept
{
    return _sortSpec;
}

void Grid::ApplyColumnLayout(std::span<const GridColumnLayoutEntry> layout) noexcept
{
    EnsureColumnWidths();
    if (! _model || _model->GetColumnCount() == 0u)
    {
        return;
    }

    std::unordered_map<std::wstring, size_t> modelIndexById;
    modelIndexById.reserve(_model->GetColumnCount());
    for (size_t modelIndex = 0; modelIndex < _model->GetColumnCount(); ++modelIndex)
    {
        const GridColumnDesc column = _model->GetColumn(modelIndex);
        if (! column.id.empty())
        {
            modelIndexById.try_emplace(column.id, modelIndex);
        }
    }

    std::vector<bool> widthApplied(_model->GetColumnCount(), false);
    std::vector<bool> orderUsed(_model->GetColumnCount(), false);
    std::vector<std::pair<size_t, size_t>> orderedColumns;
    orderedColumns.reserve(layout.size());

    for (const GridColumnLayoutEntry& entry : layout)
    {
        if (entry.columnId.empty())
        {
            continue;
        }

        const auto it = modelIndexById.find(entry.columnId);
        if (it == modelIndexById.end())
        {
            continue;
        }

        const size_t modelIndex = it->second;
        if (! widthApplied[modelIndex])
        {
            const GridColumnDesc column = _model->GetColumn(modelIndex);
            if (std::isfinite(entry.widthDip) && entry.widthDip > 0.0f)
            {
                _columnWidths[modelIndex] = std::max(column.minWidthDip, entry.widthDip);
            }
            widthApplied[modelIndex] = true;
        }

        if (! orderUsed[modelIndex])
        {
            orderedColumns.emplace_back(entry.displayIndex, modelIndex);
            orderUsed[modelIndex] = true;
        }
    }

    std::stable_sort(orderedColumns.begin(), orderedColumns.end(), [](const auto& lhs, const auto& rhs) noexcept { return lhs.first < rhs.first; });

    _columnDisplayOrder.clear();
    _columnDisplayOrder.reserve(_model->GetColumnCount());
    for (const auto& orderedColumn : orderedColumns)
    {
        _columnDisplayOrder.push_back(orderedColumn.second);
    }
    for (size_t modelIndex = 0; modelIndex < _model->GetColumnCount(); ++modelIndex)
    {
        if (! orderUsed[modelIndex])
        {
            _columnDisplayOrder.push_back(modelIndex);
        }
    }

    RebuildColumnDisplayIndexLookup();
    ClampScrollOffsets();
    RefreshAccessibilitySnapshot();
}

std::vector<GridColumnLayoutEntry> Grid::CaptureColumnLayout() const
{
    std::vector<GridColumnLayoutEntry> layout;
    EnsureColumnWidths();
    if (! _model || _model->GetColumnCount() == 0u)
    {
        return layout;
    }

    layout.reserve(_model->GetColumnCount());
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        const size_t modelIndex     = _columnDisplayOrder[displayIndex];
        const GridColumnDesc column = _model->GetColumn(modelIndex);
        layout.push_back(GridColumnLayoutEntry{
            .columnId     = column.id,
            .displayIndex = displayIndex,
            .widthDip     = _columnWidths[modelIndex],
        });
    }
    return layout;
}

void Grid::ApplyGroupLayout(std::span<const GridGroupLayoutEntry> layout) noexcept
{
    if (! _model || ! _delegate || _model->GetGroupCount() == 0u)
    {
        return;
    }

    std::unordered_map<uint64_t, bool> collapsedByStableId;
    collapsedByStableId.reserve(layout.size());
    for (const GridGroupLayoutEntry& entry : layout)
    {
        if (entry.groupStableId == 0u)
        {
            continue;
        }

        collapsedByStableId.try_emplace(entry.groupStableId, entry.collapsed);
    }

    if (collapsedByStableId.empty())
    {
        return;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const std::vector<GridGroupDesc> currentGroups = CollectOrderedGroups(_model);
    bool changed                                   = false;
    for (const GridGroupDesc& group : currentGroups)
    {
        const auto it = collapsedByStableId.find(group.stableId);
        if (it == collapsedByStableId.end() || it->second == group.collapsed)
        {
            continue;
        }

        _delegate->OnGridGroupToggled(*this, group.stableId, it->second);
        changed = true;
    }

    if (! changed)
    {
        return;
    }

    ReconcileSelectionForVisibleRows(CollectOrderedGroups(_model));
    _hoveredRow.reset();
    _hoveredColumn.reset();
    ClampScrollOffsets();
    if (! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
}

std::vector<GridGroupLayoutEntry> Grid::CaptureGroupLayout() const
{
    std::vector<GridGroupLayoutEntry> layout;
    if (! _model || _model->GetGroupCount() == 0u)
    {
        return layout;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    layout.reserve(groups.size());
    for (const GridGroupDesc& group : groups)
    {
        layout.push_back(GridGroupLayoutEntry{
            .groupStableId = group.stableId,
            .collapsed     = group.collapsed,
        });
    }
    return layout;
}

void Grid::NotifyDataChanged()
{
    _lastPaintHadAnimatedVisibleCells = false;
    _animatedVisibleCellStateValid    = false;
    const bool hasGroups              = _model && _model->GetGroupCount() > 0u;
    if (! hasGroups && _selectionModel.GetCount() == 0u)
    {
        if (_model && _activeColumn && _activeColumn.value() >= _model->GetColumnCount())
        {
            _activeColumn.reset();
        }
        if (! _model)
        {
            _activeColumn.reset();
        }
        ClampScrollOffsets();
        RefreshAccessibilitySnapshot();
        return;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    if (hasGroups)
    {
        ReconcileSelectionForVisibleRows(CollectOrderedGroups(_model));
    }
    else
    {
        constexpr std::span<const GridGroupDesc> noGroups;
        ReconcileSelectionForVisibleRows(noGroups);
    }
    if (_model && _activeColumn && _activeColumn.value() >= _model->GetColumnCount())
    {
        _activeColumn.reset();
    }
    if (! _model)
    {
        _activeColumn.reset();
    }
    ClampScrollOffsets();
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();
}

GridSelectionModel& Grid::GetSelectionModel() noexcept
{
    return _selectionModel;
}

const GridSelectionModel& Grid::GetSelectionModel() const noexcept
{
    return _selectionModel;
}

void Grid::RefreshAccessibilitySnapshot() const noexcept
{
    if (ControlHost* const host = GetHost())
    {
        RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
    }
}

GridVisibleWorkMetrics Grid::GetVisibleWorkMetrics() const
{
    GridVisibleWorkMetrics metrics{};
    if (! _model || _model->GetRowCount() == 0u || _model->GetColumnCount() == 0u)
    {
        return metrics;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return metrics;
    }

    const std::vector<GridGroupDesc> groups             = CollectOrderedGroups(_model);
    const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
    const VisibleColumnSpan visibleColumns              = ComputeVisibleColumnSpan(bodyRect.right);

    for (const VisibleBodyItem& item : visibleBodyItems)
    {
        if (item.kind == VisibleBodyItem::Kind::Row)
        {
            ++metrics.visibleRowCount;
        }
        else
        {
            ++metrics.visibleGroupHeaderCount;
        }
    }
    metrics.visibleColumnCount = visibleColumns.endIndex - visibleColumns.beginIndex;
    metrics.visibleCellCount   = metrics.visibleRowCount * static_cast<uint64_t>(metrics.visibleColumnCount);
    GridCellData cellData;
    for (const VisibleBodyItem& item : visibleBodyItems)
    {
        if (item.kind != VisibleBodyItem::Kind::Row)
        {
            continue;
        }

        for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
        {
            ResetGridCellData(cellData);
            _model->GetCellData(item.rowIndex, GetModelColumnIndexForDisplayIndex(displayIndex), cellData);
            ++metrics.visibleCellDataReadCount;
            if (cellData.kind == GridCellKind::IconText && (! cellData.iconText.empty() || cellData.iconIndex >= 0))
            {
                ++metrics.visibleIconCellCount;
                if (cellData.iconBitmap)
                {
                    ++metrics.visibleBitmapIconCellCount;
                }
            }
            if (! cellData.badgeText.empty())
            {
                ++metrics.visibleBadgeCellCount;
            }
        }
    }
    metrics.verticalScrollDip      = _verticalScrollDip;
    metrics.horizontalScrollDip    = _horizontalScrollDip;
    metrics.hasVerticalScrollbar   = GetVerticalScrollableExtent() > 0.0f;
    metrics.hasHorizontalScrollbar = GetHorizontalScrollableExtent() > 0.0f;
    return metrics;
}

GridCellLayoutMetrics Grid::GetCellLayoutMetrics(const ControlHost& host, size_t rowIndex, size_t columnIndex) const
{
    GridCellLayoutMetrics metrics{};
    if (! _model || rowIndex >= _model->GetRowCount() || columnIndex >= _model->GetColumnCount())
    {
        return metrics;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top || columnIndex >= _columnWidths.size())
    {
        return metrics;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    const float cellLeft                    = GetColumnLeftDip(columnIndex);
    const float rowTopDip                   = bodyRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
    const D2D1_RECT_F cellRect              = D2D1::RectF(cellLeft, rowTopDip, cellLeft + _columnWidths[columnIndex], rowTopDip + _rowHeightDip);
    const GridColumnDesc columnDesc         = _model->GetColumn(columnIndex);
    GridCellData cellData;
    ResetGridCellData(cellData);
    _model->GetCellData(rowIndex, columnIndex, cellData);
    return ComputeCellLayoutMetrics(host, cellRect, columnDesc, cellData);
}

size_t Grid::GetVisibleRowCount() const
{
    if (! _model || _model->GetRowCount() == 0u)
    {
        return 0u;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    size_t visibleRowCount                  = 0u;
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (item.kind == VisibleBodyItem::Kind::Row)
        {
            ++visibleRowCount;
        }
    }

    return visibleRowCount;
}

std::optional<size_t> Grid::GetVisibleRowAt(size_t visibleRowIndex) const
{
    if (! _model || _model->GetRowCount() == 0u)
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    size_t currentVisibleRowIndex           = 0u;
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (item.kind != VisibleBodyItem::Kind::Row)
        {
            continue;
        }

        if (currentVisibleRowIndex == visibleRowIndex)
        {
            return item.rowIndex;
        }

        ++currentVisibleRowIndex;
    }

    return std::nullopt;
}

std::optional<size_t> Grid::FindVisibleRowOrdinal(size_t rowIndex) const
{
    if (! _model)
    {
        return std::nullopt;
    }

    const size_t rowCount = _model->GetRowCount();
    if (rowIndex >= rowCount)
    {
        return std::nullopt;
    }
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);

    size_t visibleOrdinal   = 0u;
    size_t nextUngroupedRow = 0u;
    for (const GridGroupDesc& group : groups)
    {
        if (rowIndex < group.startRowIndex)
        {
            return visibleOrdinal + (rowIndex - nextUngroupedRow);
        }

        if (group.startRowIndex > nextUngroupedRow)
        {
            visibleOrdinal += group.startRowIndex - nextUngroupedRow;
        }

        const size_t groupEnd = group.startRowIndex + group.rowCount;
        if (rowIndex < groupEnd)
        {
            if (group.collapsed)
            {
                return std::nullopt;
            }
            return visibleOrdinal + (rowIndex - group.startRowIndex);
        }

        if (! group.collapsed)
        {
            visibleOrdinal += group.rowCount;
        }
        nextUngroupedRow = groupEnd;
    }

    return visibleOrdinal + (rowIndex - nextUngroupedRow);
}

void Grid::EnsureRowVisible(size_t rowIndex) noexcept
{
    if (! _model || rowIndex >= _model->GetRowCount())
    {
        return;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    if (! FindVisibleRowOrdinal(rowIndex).has_value())
    {
        return;
    }

    const D2D1_RECT_F contentRect = GetContentRect();
    const float viewportHeight    = std::max(0.0f, contentRect.bottom - contentRect.top);
    const float rowTop            = GetRowTopDip(groups, rowIndex);
    const float rowBottom         = rowTop + _rowHeightDip;
    if (rowTop < _verticalScrollDip)
    {
        _verticalScrollDip = rowTop;
    }
    else if (viewportHeight > 0.0f && rowBottom > (_verticalScrollDip + viewportHeight))
    {
        _verticalScrollDip = rowBottom - viewportHeight;
    }

    ClampScrollOffsets();
}

size_t Grid::GetVisibleColumnCount() const
{
    if (! _model || _model->GetColumnCount() == 0u)
    {
        return 0u;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return 0u;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    return visibleColumns.endIndex - visibleColumns.beginIndex;
}

std::optional<size_t> Grid::GetVisibleColumnAt(size_t visibleColumnIndex) const
{
    if (! _model || _model->GetColumnCount() == 0u)
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return std::nullopt;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    const size_t visibleColumnCount        = visibleColumns.endIndex - visibleColumns.beginIndex;
    if (visibleColumnIndex >= visibleColumnCount)
    {
        return std::nullopt;
    }

    return GetModelColumnIndexForDisplayIndex(visibleColumns.beginIndex + visibleColumnIndex);
}

std::optional<size_t> Grid::FindVisibleColumnOrdinal(size_t columnIndex) const
{
    if (! _model || columnIndex >= _model->GetColumnCount())
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return std::nullopt;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
    {
        if (GetModelColumnIndexForDisplayIndex(displayIndex) == columnIndex)
        {
            return displayIndex - visibleColumns.beginIndex;
        }
    }

    return std::nullopt;
}

std::optional<size_t> Grid::FindHeaderColumnAtPoint(PointDip pointDip) const noexcept
{
    if (! _model || _model->GetColumnCount() == 0u)
    {
        return std::nullopt;
    }

    const D2D1_POINT_2F point     = pointDip.AsD2D();
    const D2D1_RECT_F bounds      = NormalizeFiniteRect(GetBounds());
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    if (contentRect.top <= bounds.top || point.y < bounds.top || point.y >= contentRect.top)
    {
        return std::nullopt;
    }

    const size_t visibleColumnCount = GetVisibleColumnCount();
    for (size_t visibleColumnIndex = 0u; visibleColumnIndex < visibleColumnCount; ++visibleColumnIndex)
    {
        const std::optional<size_t> columnIndex = GetVisibleColumnAt(visibleColumnIndex);
        if (! columnIndex)
        {
            continue;
        }

        const std::optional<D2D1_RECT_F> headerRect = GetVisibleColumnHeaderRect(columnIndex.value());
        if (headerRect && PointInRect(headerRect.value(), point))
        {
            return columnIndex;
        }
    }

    return std::nullopt;
}

std::optional<size_t> Grid::FindRowAtPoint(PointDip pointDip) const noexcept
{
    if (! _model || _model->GetRowCount() == 0u)
    {
        return std::nullopt;
    }

    const HitInfo hit = HitTestPoint(pointDip);
    if (hit.zone != HitZone::Cell)
    {
        return std::nullopt;
    }

    return hit.rowIndex;
}

std::optional<std::pair<size_t, size_t>> Grid::FindCellAtPoint(PointDip pointDip) const noexcept
{
    if (! _model || _model->GetRowCount() == 0u || _model->GetColumnCount() == 0u)
    {
        return std::nullopt;
    }

    const HitInfo hit = HitTestPoint(pointDip);
    if (hit.zone != HitZone::Cell)
    {
        return std::nullopt;
    }

    return std::make_pair(hit.rowIndex, hit.columnIndex);
}

std::optional<D2D1_RECT_F> Grid::GetVisibleColumnHeaderRect(size_t columnIndex) const
{
    if (! _model || columnIndex >= _model->GetColumnCount())
    {
        return std::nullopt;
    }

    if (! FindVisibleColumnOrdinal(columnIndex))
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bounds      = NormalizeFiniteRect(GetBounds());
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    if (contentRect.top <= bounds.top || columnIndex >= _columnWidths.size())
    {
        return std::nullopt;
    }

    const float headerLeftDip        = GetColumnLeftDip(columnIndex);
    const D2D1_RECT_F headerViewport = D2D1::RectF(bounds.left, bounds.top, contentRect.right, contentRect.top);
    const D2D1_RECT_F clippedRect =
        ClipRectToRect(D2D1::RectF(headerLeftDip, bounds.top, headerLeftDip + _columnWidths[columnIndex], contentRect.top), headerViewport);
    if (! IsNonEmptyRect(clippedRect))
    {
        return std::nullopt;
    }

    return clippedRect;
}

std::optional<D2D1_RECT_F> Grid::GetVisibleDisplayColumnHeaderRect(size_t displayIndex) const
{
    EnsureColumnWidths();
    if (! _model || displayIndex >= _columnDisplayOrder.size())
    {
        return std::nullopt;
    }

    return GetVisibleColumnHeaderRect(GetModelColumnIndexForDisplayIndex(displayIndex));
}

std::optional<D2D1_RECT_F> Grid::GetVisibleRowRect(size_t rowIndex) const
{
    if (! _model || rowIndex >= _model->GetRowCount())
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (item.kind == VisibleBodyItem::Kind::Row && item.rowIndex == rowIndex)
        {
            const D2D1_RECT_F clippedRect = ClipRectToRect(item.rectDip, GetContentRect());
            if (! IsNonEmptyRect(clippedRect))
            {
                return std::nullopt;
            }

            return clippedRect;
        }
    }

    return std::nullopt;
}

std::optional<D2D1_RECT_F> Grid::GetVisibleCellRect(size_t rowIndex, size_t columnIndex) const
{
    if (! _model || rowIndex >= _model->GetRowCount() || columnIndex >= _model->GetColumnCount())
    {
        return std::nullopt;
    }

    if (! FindVisibleRowOrdinal(rowIndex) || ! FindVisibleColumnOrdinal(columnIndex))
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top || columnIndex >= _columnWidths.size())
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    const float cellLeft                    = GetColumnLeftDip(columnIndex);
    const float rowTopDip                   = bodyRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
    const D2D1_RECT_F clippedRect =
        ClipRectToRect(D2D1::RectF(cellLeft, rowTopDip, cellLeft + _columnWidths[columnIndex], rowTopDip + _rowHeightDip), bodyRect);
    if (! IsNonEmptyRect(clippedRect))
    {
        return std::nullopt;
    }

    return clippedRect;
}

bool Grid::IsRowSelected(size_t rowIndex) const noexcept
{
    return _model && rowIndex < _model->GetRowCount() && _selectionModel.IsSelected(_model->GetStableRowId(rowIndex));
}

std::optional<size_t> Grid::GetPrimarySelectedRow() const noexcept
{
    if (! _model || _selectionModel.GetCount() == 0u)
    {
        return std::nullopt;
    }

    const auto selection = _selectionModel.GetOrderedSelection();
    if (selection.empty())
    {
        return std::nullopt;
    }

    return _model->FindRowByStableId(selection.back());
}

#if DXUI_ENABLE_DIAGNOSTICS
bool Grid::DebugHitTestPoint(PointDip pointDip, GridDebugHitInfo& out) const noexcept
{
    out = {};
    if (! _model)
    {
        return false;
    }

    const HitInfo hit    = HitTestPoint(pointDip);
    out.zone             = static_cast<uint32_t>(hit.zone);
    out.rowIndex         = hit.rowIndex;
    out.groupIndex       = hit.groupIndex;
    out.columnIndex      = hit.columnIndex;
    out.rectDip          = hit.rectDip;
    out.onScrollbarThumb = hit.onScrollbarThumb;
    out.isHeaderResize   = hit.zone == HitZone::HeaderResize;
    return true;
}

Grid::GridDebugPointerState Grid::DebugGetPointerState() const noexcept
{
    return GridDebugPointerState{
        .headerResizeDownCount                         = _debugHeaderResizeDownCount,
        .resizeMoveCount                               = _debugResizeMoveCount,
        .resizeActive                                  = _resizeColumn.has_value(),
        .lastResizeDeltaDip                            = _debugLastResizeDeltaDip,
        .lastResizeWidthDip                            = _debugLastResizeWidthDip,
        .pressedHeaderActive                           = _pressedHeaderColumn.has_value(),
        .pressedHeaderColumn                           = _pressedHeaderColumn.value_or(0u),
        .reorderActive                                 = _dragReorderColumn.has_value(),
        .reorderColumn                                 = _dragReorderColumn.value_or(0u),
        .reorderTargetDisplayIndex                     = _dragReorderTargetDisplayIndex,
        .headerReorderStartCount                       = _debugHeaderReorderStartCount,
        .headerReorderCommitCount                      = _debugHeaderReorderCommitCount,
        .headerReorderNoOpCount                        = _debugHeaderReorderNoOpCount,
        .lastHeaderReorderColumn                       = _debugLastHeaderReorderColumn,
        .lastHeaderReorderFromDisplayIndex             = _debugLastHeaderReorderFromDisplayIndex,
        .lastHeaderReorderRawTargetDisplayIndex        = _debugLastHeaderReorderRawTargetDisplayIndex,
        .lastHeaderReorderNormalizedTargetDisplayIndex = _debugLastHeaderReorderNormalizedTargetDisplayIndex,
    };
}

bool Grid::DebugGetRowVisualState(const ThemePalette& theme, size_t rowIndex, GridDebugRowVisualState& out) const noexcept
{
    out = {};
    if (! _model || rowIndex >= _model->GetRowCount())
    {
        return false;
    }

    const uint64_t rowId                 = _model->GetStableRowId(rowIndex);
    const bool rowSelected               = _selectionModel.IsSelected(rowId);
    const GridResolvedRowVisuals visuals = ResolveGridRowVisuals(
        theme, _model->GetRowStyle(rowIndex), rowIndex, rowSelected, HasFocus(), _hoveredRow && _hoveredRow.value() == rowIndex, _visualMode);
    const GridProgressVisualStyle progressStyle = ResolveGridProgressVisualStyle(theme, visuals.fill, visuals.text, rowSelected);
    out.fillArgb                                = PackColor(visuals.fill);
    out.textArgb                                = PackColor(visuals.text);
    out.iconArgb                                = PackColor(ResolveListIconColor(theme, visuals.text, rowSelected));
    out.busyArgb                                = PackColor(ResolveGridBusyColor(theme, visuals.text, rowSelected));
    out.progressTrackArgb                       = PackColor(progressStyle.track);
    out.progressFillArgb                        = PackColor(progressStyle.fill);
    out.usesRainbow                             = visuals.usesRainbow;
    out.selected                                = rowSelected;
    return true;
}

bool Grid::DebugGetCellVisualState(const ThemePalette& theme, size_t rowIndex, size_t columnIndex, GridDebugCellVisualState& out) const noexcept
{
    out = {};
    if (! _model || rowIndex >= _model->GetRowCount() || columnIndex >= _model->GetColumnCount())
    {
        return false;
    }

    GridCellData cellData{};
    _model->GetCellData(rowIndex, columnIndex, cellData);

    const uint64_t rowId                    = _model->GetStableRowId(rowIndex);
    const bool selected                     = _selectionModel.IsSelected(rowId);
    const bool hovered                      = _hoveredRow && _hoveredRow.value() == rowIndex;
    const GridResolvedRowVisuals rowVisuals = ResolveGridRowVisuals(theme, _model->GetRowStyle(rowIndex), rowIndex, selected, HasFocus(), hovered, _visualMode);
    const GridResolvedCellVisuals visuals   = ResolveGridCellVisuals(theme, rowVisuals, selected, hovered, cellData);

    if (visuals.checkbox.has_value())
    {
        out.hasCheckbox                 = true;
        out.checkboxIndicatorFillArgb   = PackColor(visuals.checkbox->indicatorFill);
        out.checkboxIndicatorBorderArgb = PackColor(visuals.checkbox->indicatorBorder);
        out.checkboxCheckArgb           = PackColor(visuals.checkbox->check);
    }

    if (cellData.kind == GridCellKind::IconText && (! cellData.iconText.empty() || cellData.iconIndex >= 0))
    {
        out.hasIcon          = true;
        out.iconUsesIconFont = IconTextUsesIconFont(cellData.iconText);
    }

    if (visuals.swatch.has_value())
    {
        out.hasSwatch        = true;
        out.swatchFillArgb   = PackColor(visuals.swatch->fill);
        out.swatchBorderArgb = PackColor(visuals.swatch->border);
    }

    if (visuals.badge.has_value())
    {
        out.hasBadge      = true;
        out.badgeFillArgb = PackColor(visuals.badge->fill);
        out.badgeTextArgb = PackColor(visuals.badge->text);
    }

    out.selected = selected;
    return true;
}

GridSortGlyphVisualState Grid::DebugGetSortGlyphVisualState(const ThemePalette& theme, size_t columnIndex, uint64_t nowTickMs) const noexcept
{
    return ResolveSortGlyphVisualState(theme, columnIndex, nowTickMs);
}

GridScrollbarVisualState Grid::DebugGetScrollbarVisualState(const ThemePalette& theme) const noexcept
{
    GridScrollbarVisualState state{};
    state.verticalTrackRect       = GetVerticalScrollbarRect();
    state.verticalThumbRect       = GetVerticalThumbRect();
    state.horizontalTrackRect     = GetHorizontalScrollbarRect();
    state.horizontalThumbRect     = GetHorizontalThumbRect();
    state.hasVerticalScrollbar    = IsNonEmptyRect(state.verticalTrackRect);
    state.hasHorizontalScrollbar  = IsNonEmptyRect(state.horizontalTrackRect);
    state.verticalTrackHovered    = _verticalScrollbarHotPart == ScrollbarHotPart::Track;
    state.verticalThumbHovered    = _verticalScrollbarHotPart == ScrollbarHotPart::Thumb;
    state.horizontalTrackHovered  = _horizontalScrollbarHotPart == ScrollbarHotPart::Track;
    state.horizontalThumbHovered  = _horizontalScrollbarHotPart == ScrollbarHotPart::Thumb;
    state.verticalThumbDragging   = _dragVerticalThumb;
    state.horizontalThumbDragging = _dragHorizontalThumb;
    const ScrollbarAnimationTargets verticalTargets =
        ResolveScrollbarAnimationTargets(state.verticalTrackHovered, state.verticalThumbHovered, state.verticalThumbDragging);
    const ScrollbarAnimationTargets horizontalTargets =
        ResolveScrollbarAnimationTargets(state.horizontalTrackHovered, state.horizontalThumbHovered, state.horizontalThumbDragging);
    state.verticalTrackHotProgress   = theme.reducedMotion ? verticalTargets.track : _verticalScrollbarAnimation.trackProgress;
    state.verticalThumbHotProgress   = theme.reducedMotion ? verticalTargets.thumb : _verticalScrollbarAnimation.thumbProgress;
    state.horizontalTrackHotProgress = theme.reducedMotion ? horizontalTargets.track : _horizontalScrollbarAnimation.trackProgress;
    state.horizontalThumbHotProgress = theme.reducedMotion ? horizontalTargets.thumb : _horizontalScrollbarAnimation.thumbProgress;

    const ResolvedScrollbarVisuals verticalVisuals =
        ResolveScrollbarVisuals(theme, verticalTargets, state.verticalTrackHotProgress, state.verticalThumbHotProgress);
    const ResolvedScrollbarVisuals horizontalVisuals =
        ResolveScrollbarVisuals(theme, horizontalTargets, state.horizontalTrackHotProgress, state.horizontalThumbHotProgress);
    state.verticalTrackArgb   = PackColor(verticalVisuals.track);
    state.verticalThumbArgb   = PackColor(verticalVisuals.thumb);
    state.horizontalTrackArgb = PackColor(horizontalVisuals.track);
    state.horizontalThumbArgb = PackColor(horizontalVisuals.thumb);
    return state;
}
#endif

bool Grid::RequestSelectRow(size_t rowIndex, UINT modifiers)
{
    if (! _model || rowIndex >= _model->GetRowCount() || ! FindVisibleRowOrdinal(rowIndex))
    {
        return false;
    }

    SelectRow(rowIndex, modifiers);
    return true;
}

bool Grid::RequestRemoveRowSelection(size_t rowIndex)
{
    if (! _model || rowIndex >= _model->GetRowCount() || ! FindVisibleRowOrdinal(rowIndex))
    {
        return false;
    }

    const uint64_t rowId = _model->GetStableRowId(rowIndex);
    if (! _selectionModel.IsSelected(rowId))
    {
        return true;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    if (_selectionMode == GridSelectionMode::Single || _selectionModel.GetCount() <= 1u)
    {
        _selectionModel.Clear();
    }
    else
    {
        _selectionModel.Toggle(rowId);
    }

    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();

    return true;
}

bool Grid::RequestToggleCheckboxCell(ControlHost& host, size_t rowIndex, size_t columnIndex)
{
    if (! _model || rowIndex >= _model->GetRowCount() || ! FindVisibleRowOrdinal(rowIndex))
    {
        return false;
    }

    return ToggleCheckboxCell(host, rowIndex, columnIndex);
}

const Grid::CellTextLayoutCache* Grid::PrepareCellTextLayout(
    const ControlHost& host, const GridCellData& cellData, float width, float height, std::optional<CellTextLayoutCache>& temporary) const
{
    // Trailing separators only add an empty DirectWrite line; measuring
    // without them keeps them from marking omitted content or shifting centring.
    const std::wstring_view content(cellData.text.data(), FindCellTextContentEnd(cellData.text));
    if (content.empty() || ! (width > 0.0f) || ! (height > 0.0f)) // Also rejects NaN, which sizes the shaped prefix below.
        return nullptr;
    const float keyWidth  = RoundCellTextKeyDip(width);
    const float keyHeight = RoundCellTextKeyDip(height);
    auto* factory         = host.GetWriteFactory();
    const uint32_t clamp  = std::max(1u, _lineClamp);
    const bool wrap       = clamp > 1u;
    auto* format          = host.GetTextFormat(_cellTextFontRole, cellData.textAlignment, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, wrap);
    if (! factory || ! format)
        return nullptr;
    // Identical cell values share one layout across rows and columns. A value is found by its first
    // kMaxRetainedCellTextUnits units and confirmed against the text its entry was built from (see CellTextLayoutCache),
    // so a value far longer than a cell can show is retained by the prefix shaped for it.
    const uint64_t extra   = (static_cast<uint64_t>(clamp) << 8u) | static_cast<uint64_t>(cellData.textAlignment);
    const uint64_t keyHash = HashTextLayoutKey(content.substr(0u, kMaxRetainedCellTextUnits), keyWidth, keyHeight, extra);
    ++_debugTextLayoutLookups;
    bool hit                   = false;
    CellTextLayoutCache& entry = FindTextLayoutEntry(_cellTextLayouts,
                                                     keyHash,
                                                     _cellTextPaintGeneration,
                                                     kCellTextLayoutInitialEntries,
                                                     kCellTextLayoutMaxEntries,
                                                     [&](const CellTextLayoutCache& candidate)
    {
        if (candidate.format.get() != format || candidate.width != keyWidth || candidate.height != keyHeight || candidate.lineClamp != clamp)
            return false;
        if (! candidate.prefixOnly)
            return content == candidate.text;
        const size_t length = candidate.text.size();
        return content.size() > length && CellTextBreakShapeAt(content, length) == candidate.breakShape && content.starts_with(candidate.text);
    },
                                                     hit);
    if (hit)
    {
        ++_debugTextLayoutHits;
        entry.lastUse = _cellTextPaintGeneration;
        return entry.paintHeight > 0.0f ? &entry : nullptr;
    }

    // Shape only what can be visible. A later paragraph cannot affect earlier ones, so whole paragraphs are added only
    // while the clamp has room. Within them a prefix is final once it lays out a line beyond the one after the clamp
    // (a line's breaks can depend on the text after it) or, unwrapped, once it overflows the cell with no text that
    // could reorder what shows first. The first prefix is sized for that at a quarter em per unit, the narrowest
    // common advance; one that falls short doubles a few times, then takes its whole paragraphs, so no value costs
    // much more than shaping those once.
    // Lines past what the cell's height can show never paint, and a line is at least 0.8 em tall, so a larger clamp
    // sizes nothing more.
    const auto paragraphEnd   = [content](size_t start) { return std::min(content.find_first_of(L"\r\n\x85\v\f\x2028\x2029", start), content.size()); };
    const float fontSize      = std::max(1.0f, format->GetFontSize());
    const uint32_t lineBudget = std::min(clamp, static_cast<uint32_t>(std::min(std::ceil(height / (fontSize * 0.8f)), 65536.0f)) + 1u);
    const size_t lineUnits    = static_cast<size_t>(std::min(std::ceil(width / (fontSize * 0.25f)), 65536.0f)) + 1u;
    size_t prefixUnits        = (wrap ? (static_cast<size_t>(lineBudget) + 2u) * lineUnits : lineUnits) + 32u;
    // A key as long as the text a cell can show is kept: a larger cell keeps a longer one.
    const size_t retainedUnits = std::max(kMaxRetainedCellTextUnits, 2u * prefixUnits);
    size_t doublings           = 0u;
    size_t paragraphs          = 1u;
    size_t paragraphsEnd       = paragraphEnd(0u);
    size_t measuredLength      = 0u;
    wil::com_ptr<IDWriteTextLayout> measured;
    DWRITE_TEXT_METRICS metrics{};
    for (;;)
    {
        measuredLength = std::min(paragraphsEnd, prefixUnits);
        if (measuredLength < paragraphsEnd && IS_HIGH_SURROGATE(content[measuredLength - 1u]))
            --measuredLength; // Never shape half of a surrogate pair.
        measured.reset();
        ++_debugTextLayoutCreations;
        _debugTextLayoutShapedUnits += measuredLength;
        if (FAILED(factory->CreateTextLayout(content.data(), static_cast<UINT32>(measuredLength), format, width, height, measured.put())) ||
            FAILED(measured->GetMetrics(&metrics)))
            return nullptr;
        if (measuredLength < paragraphsEnd)
        {
            if (metrics.lineCount > lineBudget + 1u)
                break;
            if (! wrap && metrics.width > width)
            {
                if (std::ranges::none_of(content.substr(0u, measuredLength), MayReorderCellText))
                    break;
                prefixUnits = paragraphsEnd; // Right-to-left text orders its whole line; shape it at once.
                continue;
            }
            prefixUnits = ++doublings < 3u ? prefixUnits * 2u : paragraphsEnd;
            continue;
        }
        if (paragraphsEnd >= content.size() || paragraphs >= lineBudget || metrics.lineCount >= lineBudget || metrics.height >= height)
            break;
        // Each explicit paragraph adds at least one line, so the first paragraphs that can show suffice.
        for (; paragraphs < lineBudget && paragraphsEnd < content.size(); ++paragraphs)
        {
            size_t next = paragraphsEnd + 1u;
            if (content[paragraphsEnd] == L'\r' && next < content.size() && content[next] == L'\n')
                ++next;
            paragraphsEnd = paragraphEnd(next);
        }
    }

    // The entry keeps the shaped text as its key: any value that starts with it (and continues the same way) lays out
    // alike. A key over the bound (a whole long right-to-left line) is laid out per use and never copied into the table.
    const std::wstring_view keyText = content.substr(0u, measuredLength);
    const bool retained             = keyText.size() <= retainedUnits;
    CellTextLayoutCache& cache      = retained ? entry : temporary.emplace();
    if (retained)
    {
        ReserveTextStorage(cache.text, keyText.size());
        cache.text.assign(keyText);
        cache.keyHash    = keyHash;
        cache.lastUse    = _cellTextPaintGeneration;
        cache.prefixOnly = measuredLength < content.size();
        cache.breakShape = cache.prefixOnly ? CellTextBreakShapeAt(content, measuredLength) : 0u;
    }
    cache.layout      = std::move(measured);
    cache.format      = format;
    cache.width       = keyWidth;
    cache.height      = keyHeight;
    cache.lineClamp   = clamp;
    cache.paintHeight = 0.0f;
    cache.truncated   = false;
    // If completing the entry throws, leave no half-built layout that a later paint would take for a finished one.
    // Deliberate early returns below keep their own entry state.
    const int exceptionsBefore = std::uncaught_exceptions();
    const auto abandonEntry    = wil::scope_exit([&cache, exceptionsBefore]() noexcept
    {
        if (std::uncaught_exceptions() > exceptionsBefore)
        {
            cache.layout.reset();
            cache.paintHeight = 0.0f;
        }
    });
    // DirectWrite line metrics preserve fallback-font/emoji line heights. A
    // font-size estimate can cut the last line even when the nominal count fits.
    std::array<DWRITE_LINE_METRICS, 64> localLines{};
    std::optional<std::vector<DWRITE_LINE_METRICS>> overflowLines;
    std::span<DWRITE_LINE_METRICS> lines(localLines);
    if (metrics.lineCount > localLines.size())
    {
        overflowLines.emplace(metrics.lineCount);
        lines = *overflowLines;
    }
    UINT32 count = 0u;
    if (FAILED(cache.layout->GetLineMetrics(lines.data(), static_cast<UINT32>(lines.size()), &count)))
    {
        cache.layout.reset();
        return nullptr;
    }
    // Complete lines only, while at least one fits. A first line taller
    // than the text rectangle still paints, centred and clipped like a
    // single-line cell, instead of leaving the cell blank.
    UINT32 visibleLines = 0u;
    for (UINT32 i = 0u; i < count && i < clamp; ++i)
    {
        if (visibleLines != 0u && cache.paintHeight + lines[i].height > height + kCellTextLineFitToleranceDip)
            break;
        cache.paintHeight += lines[i].height;
        ++visibleLines;
    }
    if (cache.paintHeight <= 0.0f)
        return nullptr; // Degenerate line metrics: the retained entry paints nothing.
    const bool omitted = visibleLines < count || measuredLength < content.size();
    // DirectWrite trims any overflow, so any overflow is a truncation the tooltip must offer (no slack).
    cache.truncated = omitted || cache.paintHeight > height + kCellTextLineFitToleranceDip || metrics.width > width;
    // Trimming shows the ellipsis where a line overflows, and the maximum height keeps later lines out.
    const auto trim = [&](IDWriteTextLayout& layout) -> bool
    {
        auto* ellipsisFormat = host.GetTextFormat(_cellTextFontRole, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, false);
        if (! ellipsisFormat)
            return false;
        if (! _cellEllipsis || _cellEllipsisFormat.get() != ellipsisFormat)
        {
            _cellEllipsis.reset();
            _cellEllipsisFormat = ellipsisFormat;
            if (FAILED(factory->CreateEllipsisTrimmingSign(ellipsisFormat, _cellEllipsis.put())))
                return false;
        }
        const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0u, 0u};
        return SUCCEEDED(layout.SetTrimming(&trimming, _cellEllipsis.get())) && SUCCEEDED(layout.SetMaxHeight(cache.paintHeight));
    };
    if (! omitted)
    {
        if (! trim(*cache.layout.get()))
        {
            cache.layout.reset();
            return nullptr;
        }
        return &cache;
    }

    // Height clipping does not trim later explicit paragraphs. Freeze the measured visible line boundaries and mark
    // the omitted tail. DirectWrite supplies Unicode-safe boundaries; the model remains untouched. No-wrap keeps the
    // marker on the last visible line, with horizontal ellipsis trimming if that line is already full. The marker
    // follows the last visible character, not the space or separator at which that line broke.
    std::wstring& visibleText = _cellVisibleText;
    visibleText.clear();
    size_t visibleLength = 2u; // The marker and its direction mark.
    for (UINT32 i = 0u; i < visibleLines; ++i)
        visibleLength += lines[i].length + 1u;
    ReserveTextStorage(visibleText, visibleLength);
    size_t offset = 0u;
    for (UINT32 i = 0u; i < visibleLines; ++i)
    {
        if (i != 0u)
            visibleText.push_back(L'\n');
        const UINT32 tailLength = (i + 1u == visibleLines) ? lines[i].trailingWhitespaceLength : lines[i].newlineLength;
        visibleText.append(content.substr(offset, lines[i].length - tailLength));
        offset += lines[i].length;
    }
    visibleText.push_back(L'\x2026');
    if (const std::optional<wchar_t> mark = ResolveOmissionMarkDirection(visibleText, format->GetReadingDirection()))
        visibleText.push_back(*mark);
    const auto buildVisibleLayout = [&](std::wstring_view text, wil::com_ptr<IDWriteTextLayout>& layout) -> bool
    {
        ++_debugTextLayoutCreations;
        _debugTextLayoutShapedUnits += text.size();
        return SUCCEEDED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format, width, cache.paintHeight, layout.put())) &&
               SUCCEEDED(layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP)) && trim(*layout.get());
    };
    if (visibleText.size() > kMaxRetainedCellTextUnits)
    {
        // A whole long right-to-left line: laid out for this entry alone, and the scratch text is not kept.
        cache.layout.reset();
        const bool built = buildVisibleLayout(visibleText, cache.layout);
        std::wstring().swap(visibleText);
        if (! built)
        {
            cache.layout.reset();
            return nullptr;
        }
        return &cache;
    }
    // Values whose visible text is the same (long values sharing their start) share its layout.
    const uint64_t displayHash      = HashTextLayoutKey(visibleText, keyWidth, cache.paintHeight, extra);
    bool shared                     = false;
    CellDisplayLayoutCache& display = FindTextLayoutEntry(_cellDisplayLayouts,
                                                          displayHash,
                                                          _cellTextPaintGeneration,
                                                          kCellTextLayoutInitialEntries,
                                                          kCellTextLayoutMaxEntries,
                                                          [&](const CellDisplayLayoutCache& candidate)
    { return candidate.format.get() == format && candidate.width == keyWidth && candidate.paintHeight == cache.paintHeight && candidate.text == visibleText; },
                                                          shared);
    display.lastUse                 = _cellTextPaintGeneration;
    if (! shared)
    {
        display.layout.reset();
        display.keyHash     = displayHash;
        display.format      = format;
        display.width       = keyWidth;
        display.paintHeight = cache.paintHeight;
        display.text.swap(visibleText); // The entry keeps the text; the scratch keeps the entry's old storage.
        wil::com_ptr<IDWriteTextLayout> built;
        if (! buildVisibleLayout(display.text, built))
        {
            cache.layout.reset();
            return nullptr;
        }
        display.layout = std::move(built); // Only a finished layout is shared.
    }
    cache.layout = display.layout;
    return &cache;
}

const Grid::CellTextLayoutCache* Grid::PrepareSingleLineCellLayout(const ControlHost& host,
                                                                   const GridCellData& cellData,
                                                                   const D2D1_RECT_F& textRect,
                                                                   std::optional<CellTextLayoutCache>& temporary) const
{
    const std::wstring_view text = cellData.text;
    const float width            = textRect.right - textRect.left;
    const float height           = textRect.bottom - textRect.top;
    if (text.empty() || ! (width > 0.0f) || ! (height > 0.0f))
        return nullptr;
    auto* factory = host.GetWriteFactory();
    // The format DrawCenteredText would use for the caption: unwrapped and centred vertically.
    auto* format = host.GetTextFormat(_cellTextFontRole, cellData.textAlignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
    if (! factory || ! format)
        return nullptr;
    // Keyed by the whole caption, in the multiline table (as a clamp of zero, which no multiline lookup has); a caption
    // longer than the retained bound is laid out per use.
    const float keyWidth   = RoundCellTextKeyDip(width);
    const float keyHeight  = RoundCellTextKeyDip(height);
    const uint64_t keyHash = HashTextLayoutKey(text.substr(0u, kMaxRetainedCellTextUnits), keyWidth, keyHeight, static_cast<uint64_t>(cellData.textAlignment));
    CellTextLayoutCache* slot = nullptr;
    if (text.size() <= kMaxRetainedCellTextUnits)
    {
        ++_debugTextLayoutLookups;
        bool hit                   = false;
        CellTextLayoutCache& entry = FindTextLayoutEntry(_cellTextLayouts,
                                                         keyHash,
                                                         _cellTextPaintGeneration,
                                                         kCellTextLayoutInitialEntries,
                                                         kCellTextLayoutMaxEntries,
                                                         [&](const CellTextLayoutCache& candidate)
        {
            return candidate.lineClamp == 0u && candidate.format.get() == format && candidate.width == keyWidth && candidate.height == keyHeight &&
                   candidate.text == text;
        },
                                                         hit);
        if (hit)
        {
            ++_debugTextLayoutHits;
            entry.lastUse = _cellTextPaintGeneration;
            return &entry;
        }
        slot = &entry;
    }

    // Shape only what can show. A leading-aligned caption on one paragraph starts at the cell's leading edge and is
    // clipped at the other, so a prefix that already overflows the cell, with no text that could reorder what shows
    // first, paints what the whole caption paints. Centred or trailing text overflows by the whole line, and several
    // paragraphs centre together, so they shape everything, as does a prefix still inside the cell after two doublings.
    wil::com_ptr<IDWriteTextLayout> layout;
    if (cellData.textAlignment == DWRITE_TEXT_ALIGNMENT_LEADING && text.find_first_of(L"\r\n\x85\v\f\x2028\x2029") == std::wstring_view::npos)
    {
        const float fontSize = std::max(1.0f, format->GetFontSize());
        size_t prefixUnits   = static_cast<size_t>(std::min(std::ceil(width / (fontSize * 0.25f)), 65536.0f)) + 33u;
        for (size_t doublings = 0u; doublings < 3u && prefixUnits < text.size(); ++doublings, prefixUnits *= 2u)
        {
            const size_t measured = IS_HIGH_SURROGATE(text[prefixUnits - 1u]) ? prefixUnits - 1u : prefixUnits;
            DWRITE_TEXT_METRICS metrics{};
            ++_debugTextLayoutCreations;
            _debugTextLayoutShapedUnits += measured;
            if (FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(measured), format, width, height, layout.put())) ||
                FAILED(layout->GetMetrics(&metrics)))
                return nullptr;
            if (metrics.width > width)
            {
                if (std::ranges::any_of(text.substr(0u, measured), MayReorderCellText))
                    layout.reset(); // Right-to-left text orders its whole line: shape all of it.
                break;
            }
            layout.reset();
        }
    }
    if (! layout)
    {
        ++_debugTextLayoutCreations;
        _debugTextLayoutShapedUnits += text.size();
        if (FAILED(factory->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), format, width, height, layout.put())))
            return nullptr;
    }

    CellTextLayoutCache& cache = slot ? *slot : temporary.emplace();
    if (slot)
    {
        ReserveTextStorage(cache.text, text.size());
        cache.text.assign(text);
        cache.keyHash    = keyHash;
        cache.lastUse    = _cellTextPaintGeneration;
        cache.prefixOnly = false;
        cache.breakShape = 0u;
    }
    cache.layout      = std::move(layout);
    cache.format      = format;
    cache.width       = keyWidth;
    cache.height      = keyHeight;
    cache.lineClamp   = 0u;
    cache.paintHeight = height;
    cache.truncated   = false;
    return &cache;
}

bool Grid::IsSingleLineCellTextClipped(const ControlHost& host,
                                       const GridCellData& cellData,
                                       const D2D1_RECT_F& textRect,
                                       const D2D1_RECT_F& viewportRect) const
{
    const std::wstring_view text = cellData.text;
    if (text.empty())
        return false;
    const float availableWidthDip = std::max(0.0f, textRect.right - textRect.left);
    if (availableWidthDip <= 0.5f)
        return true;
    if (text.find_first_of(L"\r\n") != std::wstring_view::npos)
        return true;
    // Paint's own layout: a prefix shaped for a caption the cell cannot hold is already wider than the cell.
    std::optional<CellTextLayoutCache> temporary;
    const CellTextLayoutCache* prepared = PrepareSingleLineCellLayout(host, cellData, textRect, temporary);
    DWRITE_TEXT_METRICS metrics{};
    if (! prepared || FAILED(prepared->layout->GetMetrics(&metrics)))
        return false;
    const float textWidthDip = metrics.widthIncludingTrailingWhitespace;
    if (textWidthDip > availableWidthDip + 0.5f)
        return true;
    // Paint places the caption inside the full text rectangle by its alignment; a scrolled viewport can hide either end.
    float textLeft = textRect.left;
    if (cellData.textAlignment == DWRITE_TEXT_ALIGNMENT_TRAILING)
        textLeft = textRect.right - textWidthDip;
    else if (cellData.textAlignment == DWRITE_TEXT_ALIGNMENT_CENTER)
        textLeft = textRect.left + ((availableWidthDip - textWidthDip) * 0.5f);
    return textLeft < viewportRect.left - 0.5f || textLeft + textWidthDip > viewportRect.right + 0.5f;
}

bool Grid::IsMultilineCellTextClipped(const ControlHost& host, const GridCellData& cellData, const D2D1_RECT_F& textRect, const D2D1_RECT_F& viewportRect) const
{
    if (FindCellTextContentEnd(cellData.text) == 0u)
        return false;
    if (! IsNonEmptyRect(textRect))
        return true;
    std::optional<CellTextLayoutCache> temporary;
    const CellTextLayoutCache* prepared = PrepareCellTextLayout(host, cellData, textRect.right - textRect.left, textRect.bottom - textRect.top, temporary);
    if (! prepared)
        return false;
    if (prepared->truncated)
        return true;
    // Paint lays text out against the full cell. As for single-line cells, a
    // horizontally scrolled viewport that hides part of that text clips it.
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(prepared->layout->GetMetrics(&metrics)))
        return false;
    const float textLeft = textRect.left + metrics.left;
    return textLeft < viewportRect.left - 0.5f || textLeft + metrics.width > viewportRect.right + 0.5f;
}

void Grid::DrawCellText(ControlHost& host, const GridCellData& cellData, const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color) const
{
    if (cellData.text.empty() || ! IsNonEmptyRect(bounds))
        return;
    auto* dc = host.GetDeviceContext();
    if (! dc)
        return;
    // A single-line caption draws its retained layout as DrawCenteredText would draw the text: at the rectangle's
    // origin, vertically centred by its format and clipped to the rectangle.
    if (! cellData.multiline)
    {
        std::optional<CellTextLayoutCache> temporary;
        const CellTextLayoutCache* prepared = PrepareSingleLineCellLayout(host, cellData, bounds, temporary);
        if (auto* brush = prepared ? host.GetSolidBrush(color) : nullptr)
            dc->DrawTextLayout(D2D1::Point2F(bounds.left, bounds.top), prepared->layout.get(), brush, kTextDrawOptions);
        return;
    }
    const float height = bounds.bottom - bounds.top;
    std::optional<CellTextLayoutCache> temporary;
    const CellTextLayoutCache* prepared = PrepareCellTextLayout(host, cellData, bounds.right - bounds.left, height, temporary);
    if (! prepared)
        return;
    auto* brush = host.GetSolidBrush(color);
    if (! brush)
        return;
    // Like DrawCenteredText, a first line taller than the text rectangle is
    // centred on it and clipped to it, so it never paints into other rows.
    const auto origin       = D2D1::Point2F(bounds.left, bounds.top + (height - prepared->paintHeight) * 0.5f);
    const bool clipToBounds = prepared->paintHeight > height + kCellTextLineFitToleranceDip;
    if (clipToBounds)
        dc->PushAxisAlignedClip(bounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    // No D2D1_DRAW_TEXT_OPTIONS_CLIP: the layout box is fractional (summed line heights, centred), and clipping to it
    // both shaved the last line's descender row and made Direct2D allocate per draw (most of the multiline Grid's
    // process-memory growth). Trimming and SetMaxHeight already bound the text; the tall-first-line case clips above.
    dc->DrawTextLayout(origin, prepared->layout.get(), brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    if (clipToBounds)
        dc->PopAxisAlignedClip();
}

void Grid::Paint(ControlHost& host) const
{
    ++_cellTextPaintGeneration;
    const auto releaseOffscreenLayouts = wil::scope_exit([&]() noexcept
    {
        EndTextLayoutPaint(_cellTextLayouts,
                           _cellTextPaintGeneration,
                           kCellTextLayoutInitialEntries,
                           [](CellTextLayoutCache& entry) noexcept
        {
            entry.layout.reset();
            entry.format.reset();
        });
        EndTextLayoutPaint(_cellDisplayLayouts,
                           _cellTextPaintGeneration,
                           kCellTextLayoutInitialEntries,
                           [](CellDisplayLayoutCache& entry) noexcept
        {
            entry.layout.reset();
            entry.format.reset();
        });
    });
    _lastPaintHadAnimatedVisibleCells  = false;
    _animatedVisibleCellStateValid     = true;
    auto* dc                           = host.GetDeviceContext();
    if (! dc)
    {
        return;
    }

#if DXUI_ENABLE_DIAGNOSTICS
    ++_debugPaintCount;
#endif

    const auto paintStartedAt         = std::chrono::steady_clock::now();
    uint64_t visibleItemCount         = 0u;
    uint64_t visibleCellDataReadCount = 0u;
    const auto emitPaintPerf          = wil::scope_exit([&]() noexcept
    {
        if (! _model)
        {
            return;
        }

        Debug::Perf::Emit(
            L"dxui.grid.paint_us", L"", Debug::Perf::ElapsedUs(paintStartedAt), visibleItemCount, static_cast<uint64_t>(_model->GetRowCount()), S_OK);
        Debug::Perf::Emit(L"dxui.grid.paint_cell_data_reads", L"", 0u, visibleCellDataReadCount, visibleItemCount, S_OK);
    });

    EnsureColumnWidths();
    const ThemePalette& theme                 = host.GetTheme();
    const GridSurfaceVisualStyle surfaceStyle = ResolveGridSurfaceVisualStyle(theme);
    const GridHeaderVisualStyle headerStyle   = ResolveGridHeaderVisualStyle(theme);
    const D2D1_RECT_F bounds                  = GetBounds();
    dc->FillRectangle(bounds, host.GetSolidBrush(surfaceStyle.fill));
    dc->DrawRectangle(bounds, host.GetSolidBrush(surfaceStyle.border), 1.0f);

    if (! _model)
    {
        const std::wstring_view emptyText = _emptyStateText.empty() ? std::wstring_view(L"No data") : std::wstring_view(_emptyStateText);
        DrawCenteredText(host, emptyText, bounds, FontRole::Body, surfaceStyle.emptyText);
        return;
    }

    _cachedGroups                = CollectOrderedGroups(_model);
    const D2D1_RECT_F bodyRect   = GetContentRect(_cachedGroups);
    const D2D1_RECT_F headerRect = D2D1::RectF(bounds.left, bounds.top, bodyRect.right, bounds.top + _headerHeightDip);
    dc->FillRectangle(headerRect, host.GetSolidBrush(surfaceStyle.headerFill));
    dc->DrawLine(D2D1::Point2F(headerRect.left, headerRect.bottom - 0.5f),
                 D2D1::Point2F(headerRect.right, headerRect.bottom - 0.5f),
                 host.GetSolidBrush(surfaceStyle.headerBorder),
                 1.0f);
    const bool reducedMotion                            = theme.reducedMotion;
    const uint64_t animationTickMs                      = reducedMotion ? 0u : ::GetTickCount64();
    bool needsAnimation                                 = false;
    const std::optional<size_t> busyHeaderColumn        = ResolveHeaderBusyColumn();
    const VisibleColumnSpan visibleColumns              = ComputeVisibleColumnSpan(bodyRect.right);
    const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(_cachedGroups, bodyRect);
    visibleItemCount                                    = static_cast<uint64_t>(visibleBodyItems.size());

    dc->PushAxisAlignedClip(headerRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    float x = visibleColumns.beginXDip;
    for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
    {
        const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
        const float width          = _columnWidths[columnIndex];
        const D2D1_RECT_F cellRect = D2D1::RectF(x, bounds.top, x + width, headerRect.bottom);
        x += width;

        const D2D1_RECT_F visibleCellRect = ClipRectToRect(cellRect, headerRect);
        if (! IsNonEmptyRect(visibleCellRect))
        {
            continue;
        }

        D2D1_COLOR_F fill = headerStyle.fill;
        if (_pressedHeaderColumn && _pressedHeaderColumn.value() == columnIndex)
        {
            fill = headerStyle.pressedFill;
        }
        else if (_hoveredColumn && _hoveredColumn.value() == columnIndex && ! _hoveredRow)
        {
            fill = headerStyle.hoveredFill;
        }
        dc->FillRectangle(visibleCellRect, host.GetSolidBrush(fill));
        if (displayIndex + 1 < _columnDisplayOrder.size())
        {
            dc->DrawLine(D2D1::Point2F(visibleCellRect.right - 0.5f, visibleCellRect.top),
                         D2D1::Point2F(visibleCellRect.right - 0.5f, visibleCellRect.bottom),
                         host.GetSolidBrush(headerStyle.separator),
                         1.0f);
        }

        const GridColumnDesc column = _model->GetColumn(columnIndex);
        const GridSortGlyphVisualState sortGlyphState =
            ResolveSortGlyphVisualState(theme, columnIndex, reducedMotion ? _sortGlyphTransition.startTickMs : animationTickMs);
        const bool drawBusyGlyph = busyHeaderColumn && busyHeaderColumn.value() == columnIndex;
        float titleRight         = visibleCellRect.right - 8.0f;
        if (sortGlyphState.reservesSpace)
        {
            titleRight -= 18.0f;
        }
        if (drawBusyGlyph)
        {
            titleRight -= 18.0f;
        }
        DrawCenteredText(
            host,
            column.title,
            D2D1::RectF(
                visibleCellRect.left + 8.0f, visibleCellRect.top + 2.0f, std::max(visibleCellRect.left + 24.0f, titleRight), visibleCellRect.bottom - 2.0f),
            FontRole::Header,
            headerStyle.titleText,
            DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        if (drawBusyGlyph)
        {
            needsAnimation  = ! reducedMotion;
            float busyRight = visibleCellRect.right - 8.0f;
            if (sortGlyphState.reservesSpace)
            {
                busyRight -= 18.0f;
            }
            DrawCenteredText(host,
                             SpinnerFrameForTick(animationTickMs),
                             D2D1::RectF(busyRight - 14.0f, visibleCellRect.top + 2.0f, busyRight, visibleCellRect.bottom - 2.0f),
                             FontRole::Header,
                             headerStyle.busyGlyph,
                             DWRITE_TEXT_ALIGNMENT_CENTER,
                             DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                             false);
        }
        if (sortGlyphState.previousAlpha > 0.0f)
        {
            DrawSortGlyph(host, visibleCellRect, sortGlyphState.previousDirection, WithOpacity(headerStyle.sortGlyph, sortGlyphState.previousAlpha));
        }
        if (sortGlyphState.currentAlpha > 0.0f)
        {
            DrawSortGlyph(host, visibleCellRect, sortGlyphState.currentDirection, WithOpacity(headerStyle.sortGlyph, sortGlyphState.currentAlpha));
        }
        if (sortGlyphState.animating)
        {
            needsAnimation = true;
        }
    }
    dc->PopAxisAlignedClip();

    GridCellData cellData;
    dc->PushAxisAlignedClip(bodyRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    for (const VisibleBodyItem& item : visibleBodyItems)
    {
        const D2D1_RECT_F visibleItemRect = ClipRectToRect(item.rectDip, bodyRect);
        if (! IsNonEmptyRect(visibleItemRect))
        {
            continue;
        }

        if (item.kind == VisibleBodyItem::Kind::GroupHeader)
        {
            const GridGroupDesc& group = _cachedGroups[item.groupIndex];
            dc->FillRectangle(visibleItemRect, host.GetSolidBrush(headerStyle.groupFill));
            dc->DrawLine(D2D1::Point2F(visibleItemRect.left, visibleItemRect.bottom - 0.5f),
                         D2D1::Point2F(visibleItemRect.right, visibleItemRect.bottom - 0.5f),
                         host.GetSolidBrush(headerStyle.groupSeparator),
                         1.0f);
            DrawGroupDisclosureGlyph(host, item.rectDip, group.collapsed, headerStyle.groupGlyph);
            DrawCenteredText(
                host,
                group.title,
                D2D1::RectF(
                    item.rectDip.left + 24.0f, item.rectDip.top + 2.0f, std::max(item.rectDip.left + 24.0f, bodyRect.right - 8.0f), item.rectDip.bottom - 2.0f),
                FontRole::Header,
                headerStyle.groupText,
                DWRITE_TEXT_ALIGNMENT_LEADING,
                DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                false);
            continue;
        }

        const size_t rowIndex                   = item.rowIndex;
        const uint64_t rowId                    = _model->GetStableRowId(rowIndex);
        const bool rowSelected                  = _selectionModel.IsSelected(rowId);
        const bool rowHovered                   = _hoveredRow && _hoveredRow.value() == rowIndex;
        const GridRowStyle rowStyle             = _model->GetRowStyle(rowIndex);
        const D2D1_RECT_F rowRect               = item.rectDip;
        const GridResolvedRowVisuals rowVisuals = ResolveGridRowVisuals(theme, rowStyle, rowIndex, rowSelected, HasFocus(), rowHovered, _visualMode);
        const D2D1_COLOR_F rowFill              = rowVisuals.fill;
        const D2D1_COLOR_F rowText              = rowVisuals.text;

        if (rowFill.a > 0.0f)
        {
            if (rowVisuals.roundedFill)
            {
                const D2D1_RECT_F roundedFillRect =
                    D2D1::RectF(visibleItemRect.left + 1.0f, visibleItemRect.top + 1.0f, visibleItemRect.right - 1.0f, visibleItemRect.bottom - 1.0f);
                if (IsNonEmptyRect(roundedFillRect))
                {
                    const D2D1_ROUNDED_RECT rounded = D2D1::RoundedRect(roundedFillRect, 2.0f, 2.0f);
                    dc->FillRoundedRectangle(&rounded, host.GetSolidBrush(rowFill));
                }
            }
            else
            {
                dc->FillRectangle(visibleItemRect, host.GetSolidBrush(rowFill));
            }
        }
        if (rowVisuals.showSeparator)
        {
            dc->DrawLine(D2D1::Point2F(visibleItemRect.left, visibleItemRect.bottom - 0.5f),
                         D2D1::Point2F(visibleItemRect.right, visibleItemRect.bottom - 0.5f),
                         host.GetSolidBrush(surfaceStyle.rowSeparator),
                         1.0f);
        }

        float cellX = visibleColumns.beginXDip;
        for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
        {
            const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float width          = _columnWidths[columnIndex];
            const D2D1_RECT_F cellRect = D2D1::RectF(cellX, rowRect.top, cellX + width, rowRect.bottom);
            cellX += width;

            const D2D1_RECT_F visibleCellRect = ClipRectToRect(cellRect, bodyRect);
            if (! IsNonEmptyRect(visibleCellRect))
            {
                continue;
            }

            if (displayIndex + 1 < _columnDisplayOrder.size())
            {
                dc->DrawLine(D2D1::Point2F(visibleCellRect.right - 0.5f, visibleCellRect.top),
                             D2D1::Point2F(visibleCellRect.right - 0.5f, visibleCellRect.bottom),
                             host.GetSolidBrush(surfaceStyle.columnSeparator),
                             1.0f);
            }
            ResetGridCellData(cellData);
            _model->GetCellData(rowIndex, columnIndex, cellData);
            ++visibleCellDataReadCount;
            const D2D1_RECT_F contentRect =
                D2D1::RectF(std::max(cellRect.left + 8.0f, bodyRect.left + 8.0f),
                            cellRect.top + 3.0f,
                            std::max(std::max(cellRect.left + 8.0f, bodyRect.left + 8.0f), std::min(cellRect.right - 8.0f, bodyRect.right - 8.0f)),
                            cellRect.bottom - 3.0f);
            if (cellData.kind == GridCellKind::Spinner)
            {
                const bool cellAnimates           = ! reducedMotion;
                _lastPaintHadAnimatedVisibleCells = _lastPaintHadAnimatedVisibleCells || cellAnimates;
                needsAnimation                    = needsAnimation || cellAnimates;
                std::wstring displayText;
                const std::wstring_view frame = SpinnerFrameForTick(animationTickMs);
                if (cellData.text.empty())
                {
                    displayText.assign(frame);
                }
                else
                {
                    displayText.assign(frame);
                    displayText.push_back(L' ');
                    displayText.append(cellData.text);
                }

                DrawCenteredText(host,
                                 displayText,
                                 contentRect,
                                 FontRole::Body,
                                 ResolveGridBusyColor(theme, rowText, rowSelected),
                                 cellData.textAlignment,
                                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                 false);
            }
            else if (cellData.kind == GridCellKind::Marquee)
            {
                const bool cellAnimates           = ! reducedMotion && cellData.progress <= 0.0f;
                _lastPaintHadAnimatedVisibleCells = _lastPaintHadAnimatedVisibleCells || cellAnimates;
                needsAnimation                    = needsAnimation || cellAnimates;
                const D2D1_RECT_F trackRect       = D2D1::RectF(contentRect.left, contentRect.top + 6.0f, contentRect.right, contentRect.bottom - 6.0f);
                if (trackRect.right > trackRect.left && trackRect.bottom > trackRect.top)
                {
                    const GridProgressVisualStyle progressStyle = ResolveGridProgressVisualStyle(theme, rowFill, rowText, rowSelected);
                    const D2D1_ROUNDED_RECT trackRounded        = D2D1::RoundedRect(trackRect, 4.0f, 4.0f);
                    dc->FillRoundedRectangle(&trackRounded, host.GetSolidBrush(progressStyle.track));

                    const D2D1_RECT_F fillRect =
                        (cellData.progress > 0.0f) ? ComputeProgressFillRect(trackRect, cellData.progress) : ComputeMarqueeFillRect(trackRect, animationTickMs);
                    if (fillRect.right > fillRect.left)
                    {
                        const D2D1_ROUNDED_RECT fillRounded = D2D1::RoundedRect(fillRect, 4.0f, 4.0f);
                        dc->FillRoundedRectangle(&fillRounded, host.GetSolidBrush(progressStyle.fill));
                    }
                }

                if (! cellData.text.empty())
                {
                    DrawCenteredText(
                        host, cellData.text, contentRect, FontRole::Small, rowText, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
                }
            }
            else
            {
                const GridColumnDesc columnDesc    = _model->GetColumn(columnIndex);
                const GridCellLayoutMetrics layout = ComputeCellLayoutMetrics(host, cellRect, columnDesc, cellData);
                GridResolvedCellVisuals cellVisuals{};
                if (layout.hasCheckbox || layout.hasSwatch || layout.hasBadge)
                {
                    cellVisuals = ResolveGridCellVisuals(theme, rowVisuals, rowSelected, rowHovered, cellData);
                }
                if (layout.hasCheckbox)
                {
                    const float indicatorSize =
                        std::min(layout.checkboxRect.right - layout.checkboxRect.left, layout.checkboxRect.bottom - layout.checkboxRect.top);
                    const D2D1_RECT_F indicatorRect = D2D1::RectF(
                        layout.checkboxRect.left, layout.checkboxRect.top, layout.checkboxRect.left + indicatorSize, layout.checkboxRect.top + indicatorSize);
                    if (cellVisuals.checkbox.has_value())
                    {
                        const GridCheckboxVisualStyle checkboxStyle = cellVisuals.checkbox.value();
                        DrawRoundedRect(host, indicatorRect, checkboxStyle.indicatorFill, checkboxStyle.indicatorBorder, 4.0f);
                        if (cellData.checked)
                        {
                            const D2D1_RECT_F checkRect = InflateRect(indicatorRect, -1.0f, -1.0f);
                            DrawCenteredText(host,
                                             GetCheckboxCheckGlyph(host),
                                             checkRect,
                                             GetCheckboxCheckFontRole(host),
                                             checkboxStyle.check,
                                             DWRITE_TEXT_ALIGNMENT_CENTER,
                                             DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                             false);
                        }
                    }
                }

                if (layout.hasIcon)
                {
                    bool drewBitmapIcon = false;
                    if (cellData.iconIndex >= 0 && _delegate)
                    {
                        const float targetDipSize =
                            std::max(1.0f, std::min(layout.iconRect.right - layout.iconRect.left, layout.iconRect.bottom - layout.iconRect.top));
                        auto bitmap = _delegate->GetGridIconBitmap(*this, cellData.iconIndex, targetDipSize, dc);
                        if (bitmap)
                        {
                            dc->DrawBitmap(bitmap.get(), layout.iconRect, 1.0f, D2D1_INTERPOLATION_MODE_LINEAR);
                            drewBitmapIcon = true;
                        }
                    }

                    if (! drewBitmapIcon && ! cellData.iconText.empty())
                    {
                        DrawCenteredText(host,
                                         cellData.iconText,
                                         layout.iconRect,
                                         ResolveIconTextFontRole(cellData.iconText),
                                         ResolveListIconColor(theme, rowText, rowSelected),
                                         DWRITE_TEXT_ALIGNMENT_CENTER,
                                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                         false);
                    }
                }

                if (layout.hasSwatch)
                {
                    if (cellVisuals.swatch.has_value())
                    {
                        const GridSwatchVisualStyle swatchStyle = cellVisuals.swatch.value();
                        DrawRoundedRect(host, layout.swatchRect, swatchStyle.fill, swatchStyle.border, 4.0f);
                    }
                }

                if (layout.hasBadge)
                {
                    if (cellVisuals.badge.has_value())
                    {
                        const GridBadgeVisualStyle badgeStyle = cellVisuals.badge.value();
                        DrawRoundedRect(host, layout.badgeRect, badgeStyle.fill, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), 9.0f);
                        DrawCenteredText(host,
                                         cellData.badgeText,
                                         layout.badgeRect,
                                         FontRole::Small,
                                         badgeStyle.text,
                                         DWRITE_TEXT_ALIGNMENT_CENTER,
                                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                         false);
                    }
                }

                DrawCellText(host, cellData, layout.textRect, rowText);
            }
        }
    }
    dc->PopAxisAlignedClip();

    if (needsAnimation)
    {
        host.RequestAnimation();
    }

    const D2D1_RECT_F verticalScrollbar = GetVerticalScrollbarRect();
    if (verticalScrollbar.right > verticalScrollbar.left && verticalScrollbar.bottom > verticalScrollbar.top)
    {
        const bool verticalTrackHovered                 = _verticalScrollbarHotPart == ScrollbarHotPart::Track;
        const bool verticalThumbHovered                 = _verticalScrollbarHotPart == ScrollbarHotPart::Thumb;
        const ScrollbarAnimationTargets verticalTargets = ResolveScrollbarAnimationTargets(verticalTrackHovered, verticalThumbHovered, _dragVerticalThumb);
        const ResolvedScrollbarVisuals visuals =
            ResolveScrollbarVisuals(theme,
                                    verticalTargets,
                                    theme.reducedMotion ? verticalTargets.track : _verticalScrollbarAnimation.trackProgress,
                                    theme.reducedMotion ? verticalTargets.thumb : _verticalScrollbarAnimation.thumbProgress);
        PaintScrollbar(host, verticalScrollbar, GetVerticalThumbRect(), visuals);
    }
    const D2D1_RECT_F horizontalScrollbar = GetHorizontalScrollbarRect();
    if (horizontalScrollbar.right > horizontalScrollbar.left && horizontalScrollbar.bottom > horizontalScrollbar.top)
    {
        const bool horizontalTrackHovered = _horizontalScrollbarHotPart == ScrollbarHotPart::Track;
        const bool horizontalThumbHovered = _horizontalScrollbarHotPart == ScrollbarHotPart::Thumb;
        const ScrollbarAnimationTargets horizontalTargets =
            ResolveScrollbarAnimationTargets(horizontalTrackHovered, horizontalThumbHovered, _dragHorizontalThumb);
        const ResolvedScrollbarVisuals visuals =
            ResolveScrollbarVisuals(theme,
                                    horizontalTargets,
                                    theme.reducedMotion ? horizontalTargets.track : _horizontalScrollbarAnimation.trackProgress,
                                    theme.reducedMotion ? horizontalTargets.thumb : _horizontalScrollbarAnimation.thumbProgress);
        PaintScrollbar(host, horizontalScrollbar, GetHorizontalThumbRect(), visuals);
    }
}

#if DXUI_ENABLE_DIAGNOSTICS
uint64_t Grid::DebugGetPaintCount() const noexcept
{
    return _debugPaintCount;
}

Grid::GridDebugTextLayoutStatistics Grid::DebugGetTextLayoutStatistics() const noexcept
{
    GridDebugTextLayoutStatistics statistics{};
    statistics.lookups         = _debugTextLayoutLookups;
    statistics.hits            = _debugTextLayoutHits;
    statistics.layoutCreations = _debugTextLayoutCreations;
    statistics.shapedUnits     = _debugTextLayoutShapedUnits;
    statistics.retainedLayouts =
        static_cast<size_t>(std::ranges::count_if(_cellTextLayouts, [](const CellTextLayoutCache& entry) { return entry.layout != nullptr; }));
    statistics.capacity = _cellTextLayouts.size();
    statistics.displayLayouts =
        static_cast<size_t>(std::ranges::count_if(_cellDisplayLayouts, [](const CellDisplayLayoutCache& entry) { return entry.layout != nullptr; }));
    statistics.displayCapacity = _cellDisplayLayouts.size();
    const auto heapUnits       = [](const std::wstring& text) noexcept { return HoldsTextStorage(text) ? text.capacity() : size_t{0u}; };
    for (const CellTextLayoutCache& entry : _cellTextLayouts)
        statistics.textUnits += heapUnits(entry.text);
    for (const CellDisplayLayoutCache& entry : _cellDisplayLayouts)
        statistics.textUnits += heapUnits(entry.text);
    statistics.textUnits += heapUnits(_cellVisibleText);
    statistics.tableBytes = (_cellTextLayouts.capacity() * sizeof(CellTextLayoutCache)) + (_cellDisplayLayouts.capacity() * sizeof(CellDisplayLayoutCache));
    statistics.ellipsis   = _cellEllipsis != nullptr || _cellEllipsisFormat != nullptr;
    return statistics;
}

void Grid::DebugSetScrollOffsets(float verticalScrollDip, float horizontalScrollDip) noexcept
{
    _verticalScrollDip   = verticalScrollDip;
    _horizontalScrollDip = horizontalScrollDip;
    ClampScrollOffsets();
}
#endif

bool Grid::Tick(ControlHost& host, uint64_t nowTickMs)
{
    if (host.GetTheme().reducedMotion)
    {
        return false;
    }

    const bool verticalScrollbarAnimating   = AdvanceScrollbarAnimation(host, _verticalScrollbarAnimation, nowTickMs);
    const bool horizontalScrollbarAnimating = AdvanceScrollbarAnimation(host, _horizontalScrollbarAnimation, nowTickMs);
    const bool animatedVisibleCells         = _animatedVisibleCellStateValid ? _lastPaintHadAnimatedVisibleCells : HasAnimatedVisibleCells();
    _lastPaintHadAnimatedVisibleCells       = animatedVisibleCells;
    _animatedVisibleCellStateValid          = true;
    const bool sortGlyphAnimating           = _sortGlyphTransition.active && ComputeSortGlyphTransitionProgress(nowTickMs) < 1.0f;
    const bool ticking =
        ResolveHeaderBusyColumn().has_value() || animatedVisibleCells || sortGlyphAnimating || verticalScrollbarAnimating || horizontalScrollbarAnimating;
    // Spinners, busy headers, the sort glyph and scrollbar transitions paint from the tick time. A sort glyph that
    // settles on this tick still needs its final frame, and this is the last tick the host issues for it.
    if (ticking || _sortGlyphTransition.active)
    {
        Invalidate(host);
    }
    return ticking;
}

bool Grid::HasAnimatedVisibleCells() const
{
    if (! _model || _model->GetRowCount() == 0u || _model->GetColumnCount() == 0u)
    {
        return false;
    }

    EnsureColumnWidths();
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return false;
    }

    const std::vector<GridGroupDesc> groups             = CollectOrderedGroups(_model);
    const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
    const VisibleColumnSpan visibleColumns              = ComputeVisibleColumnSpan(bodyRect.right);
    GridCellData cellData;
    for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
    {
        const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
        for (const VisibleBodyItem& item : visibleBodyItems)
        {
            if (item.kind != VisibleBodyItem::Kind::Row)
            {
                continue;
            }

            ResetGridCellData(cellData);
            _model->GetCellData(item.rowIndex, columnIndex, cellData);
            if (IsAnimatedCell(cellData))
            {
                return true;
            }
        }
    }

    return false;
}

float Grid::ComputeSortGlyphTransitionProgress(uint64_t nowTickMs) const noexcept
{
    if (! _sortGlyphTransition.active)
    {
        return 1.0f;
    }

    if (nowTickMs <= _sortGlyphTransition.startTickMs)
    {
        return 0.0f;
    }

    const uint64_t elapsedMs = nowTickMs - _sortGlyphTransition.startTickMs;
    return std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(kSortGlyphTransitionDurationMs), 0.0f, 1.0f);
}

GridSortGlyphVisualState Grid::ResolveSortGlyphVisualState(const ThemePalette& theme, size_t columnIndex, uint64_t nowTickMs) const noexcept
{
    GridSortGlyphVisualState state{};
    const bool currentColumnMatches = _sortSpec.direction != SortDirection::None && _sortSpec.columnIndex == columnIndex;
    if (! _sortGlyphTransition.active)
    {
        if (currentColumnMatches)
        {
            state.currentDirection = _sortSpec.direction;
            state.currentAlpha     = 1.0f;
            state.reservesSpace    = true;
        }
        return state;
    }

    const bool reducedMotion = theme.reducedMotion;
    const float progress     = reducedMotion ? 1.0f : ComputeSortGlyphTransitionProgress(nowTickMs);
    const bool animating     = ! reducedMotion && progress < 1.0f;

    const bool previousColumnMatches = _sortGlyphTransition.from.direction != SortDirection::None && _sortGlyphTransition.from.columnIndex == columnIndex;
    const bool targetColumnMatches   = _sortGlyphTransition.to.direction != SortDirection::None && _sortGlyphTransition.to.columnIndex == columnIndex;
    state.reservesSpace              = previousColumnMatches || targetColumnMatches;

    if (! animating)
    {
        if (targetColumnMatches)
        {
            state.currentDirection = _sortGlyphTransition.to.direction;
            state.currentAlpha     = 1.0f;
        }
        else if (currentColumnMatches)
        {
            state.currentDirection = _sortSpec.direction;
            state.currentAlpha     = 1.0f;
            state.reservesSpace    = true;
        }
        return state;
    }

    state.animating = true;
    if (previousColumnMatches)
    {
        state.previousDirection = _sortGlyphTransition.from.direction;
        state.previousAlpha     = 1.0f - progress;
    }
    if (targetColumnMatches)
    {
        state.currentDirection = _sortGlyphTransition.to.direction;
        state.currentAlpha     = progress;
    }
    return state;
}

std::optional<size_t> Grid::ResolveHeaderBusyColumn() const noexcept
{
    if (! _headerBusy || ! _model || _model->GetColumnCount() == 0u)
    {
        return std::nullopt;
    }

    if (_headerBusyColumn && _headerBusyColumn.value() < _model->GetColumnCount())
    {
        return _headerBusyColumn;
    }

    if (_sortSpec.direction != SortDirection::None && _sortSpec.columnIndex < _model->GetColumnCount())
    {
        return _sortSpec.columnIndex;
    }

    return 0u;
}

bool Grid::UpdateScrollbarHotState(const HitInfo& hit) noexcept
{
    const ScrollbarHotPart previousVerticalHotPart   = _verticalScrollbarHotPart;
    const ScrollbarHotPart previousHorizontalHotPart = _horizontalScrollbarHotPart;
    _verticalScrollbarHotPart                        = ScrollbarHotPart::None;
    _horizontalScrollbarHotPart                      = ScrollbarHotPart::None;

    if (hit.zone == HitZone::VerticalScrollbar)
    {
        _verticalScrollbarHotPart = hit.onScrollbarThumb ? ScrollbarHotPart::Thumb : ScrollbarHotPart::Track;
    }
    else if (hit.zone == HitZone::HorizontalScrollbar)
    {
        _horizontalScrollbarHotPart = hit.onScrollbarThumb ? ScrollbarHotPart::Thumb : ScrollbarHotPart::Track;
    }

    return _verticalScrollbarHotPart != previousVerticalHotPart || _horizontalScrollbarHotPart != previousHorizontalHotPart;
}

void Grid::SyncScrollbarAnimation(ControlHost& host) noexcept
{
    UpdateScrollbarAnimation(host,
                             _verticalScrollbarAnimation,
                             _verticalScrollbarHotPart == ScrollbarHotPart::Track,
                             _verticalScrollbarHotPart == ScrollbarHotPart::Thumb,
                             _dragVerticalThumb);
    UpdateScrollbarAnimation(host,
                             _horizontalScrollbarAnimation,
                             _horizontalScrollbarHotPart == ScrollbarHotPart::Track,
                             _horizontalScrollbarHotPart == ScrollbarHotPart::Thumb,
                             _dragHorizontalThumb);
}

GridCellLayoutMetrics Grid::ComputeCellLayoutMetrics(const ControlHost& host,
                                                     const D2D1_RECT_F& cellRect,
                                                     const GridColumnDesc& columnDesc,
                                                     const GridCellData& cellData) const noexcept
{
    GridCellLayoutMetrics metrics{};
    metrics.cellRect = cellRect;
    if (cellRect.right <= cellRect.left || cellRect.bottom <= cellRect.top)
    {
        return metrics;
    }

    float contentLeft                    = cellRect.left + 8.0f;
    float contentRight                   = cellRect.right - 8.0f;
    const float contentTop               = cellRect.top + 3.0f;
    const float contentBottom            = cellRect.bottom - 3.0f;
    const float contentHeight            = std::max(0.0f, contentBottom - contentTop);
    const bool dedicatedCheckboxColumn   = columnDesc.kind == GridColumnKind::Checkbox;
    const bool dedicatedStateImageColumn = columnDesc.kind == GridColumnKind::StateImage;

    metrics.hasCheckbox = cellData.kind == GridCellKind::Checkbox;
    metrics.hasIcon     = cellData.kind == GridCellKind::IconText && (! cellData.iconText.empty() || cellData.iconIndex >= 0);
    metrics.hasSwatch   = cellData.kind == GridCellKind::ColorSwatch && cellData.hasSwatchValue;
    metrics.hasBadge    = ! cellData.badgeText.empty();

    if (metrics.hasCheckbox)
    {
        const float indicatorSize = std::min(16.0f, std::max(12.0f, contentHeight));
        const float indicatorTop  = contentTop + std::max(0.0f, (contentHeight - indicatorSize) * 0.5f);
        if (dedicatedCheckboxColumn && ! metrics.hasBadge)
        {
            const float indicatorLeft = cellRect.left + std::max(0.0f, ((cellRect.right - cellRect.left) - indicatorSize) * 0.5f);
            metrics.checkboxRect      = D2D1::RectF(indicatorLeft, indicatorTop, indicatorLeft + indicatorSize, indicatorTop + indicatorSize);
            contentLeft               = metrics.checkboxRect.right;
            contentRight              = metrics.checkboxRect.left;
        }
        else
        {
            metrics.checkboxRect = D2D1::RectF(contentLeft, indicatorTop, contentLeft + indicatorSize, indicatorTop + indicatorSize);
            contentLeft          = metrics.checkboxRect.right + 6.0f;
        }
    }

    if (metrics.hasIcon)
    {
        const float iconSize = std::min(_iconSizeDip, std::max(14.0f, contentHeight));
        const float iconTop  = contentTop + std::max(0.0f, (contentHeight - iconSize) * 0.5f);
        if (dedicatedStateImageColumn && ! metrics.hasCheckbox && ! metrics.hasBadge)
        {
            const float iconLeft = cellRect.left + std::max(0.0f, ((cellRect.right - cellRect.left) - iconSize) * 0.5f);
            metrics.iconRect     = D2D1::RectF(iconLeft, iconTop, iconLeft + iconSize, iconTop + iconSize);
            contentLeft          = metrics.iconRect.right;
            contentRight         = metrics.iconRect.left;
        }
        else
        {
            metrics.iconRect = D2D1::RectF(contentLeft, iconTop, contentLeft + iconSize, iconTop + iconSize);
            contentLeft      = metrics.iconRect.right + 4.0f;
        }
    }

    if (metrics.hasSwatch)
    {
        const float swatchSize         = std::min(18.0f, std::max(12.0f, contentHeight));
        const float swatchTop          = contentTop + std::max(0.0f, (contentHeight - swatchSize) * 0.5f);
        const bool dedicatedSwatchCell = cellData.text.empty() && ! metrics.hasCheckbox && ! metrics.hasIcon && ! metrics.hasBadge;
        if (dedicatedSwatchCell)
        {
            const float swatchLeft = cellRect.left + std::max(0.0f, ((cellRect.right - cellRect.left) - swatchSize) * 0.5f);
            metrics.swatchRect     = D2D1::RectF(swatchLeft, swatchTop, swatchLeft + swatchSize, swatchTop + swatchSize);
            contentLeft            = metrics.swatchRect.right;
            contentRight           = metrics.swatchRect.left;
        }
        else
        {
            metrics.swatchRect = D2D1::RectF(contentLeft, swatchTop, contentLeft + swatchSize, swatchTop + swatchSize);
            contentLeft        = metrics.swatchRect.right + 6.0f;
        }
    }

    if (metrics.hasBadge)
    {
        const float badgeTextWidth = MeasureSingleLineTextWidthDip(&host, cellData.badgeText, FontRole::Small);
        const float badgeWidth     = std::clamp(badgeTextWidth + 16.0f, 28.0f, std::max(28.0f, contentRight - contentLeft));
        const float badgeHeight    = std::min(18.0f, std::max(16.0f, contentHeight));
        const float badgeTop       = contentTop + std::max(0.0f, (contentHeight - badgeHeight) * 0.5f);
        const float badgeLeft      = std::max(contentLeft + 12.0f, contentRight - badgeWidth);
        metrics.badgeRect          = D2D1::RectF(badgeLeft, badgeTop, contentRight, badgeTop + badgeHeight);
        contentRight               = metrics.badgeRect.left - 8.0f;
    }

    metrics.textRect = D2D1::RectF(contentLeft, contentTop, std::max(contentLeft, contentRight), contentBottom);
    return metrics;
}

bool Grid::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    if (_resizeColumn)
    {
        if (! _model || _resizeColumn.value() >= _model->GetColumnCount() || _resizeColumn.value() >= _columnWidths.size())
        {
            _resizeColumn.reset();
            return false;
        }

        EnsureColumnWidths();
        const float delta                    = point.x - _resizeOriginXDip;
        _columnWidths[_resizeColumn.value()] = std::max(_resizeInitialWidthDip + delta, _model->GetColumn(_resizeColumn.value()).minWidthDip);
        ++_debugResizeMoveCount;
        _debugLastResizeDeltaDip = delta;
        _debugLastResizeWidthDip = _columnWidths[_resizeColumn.value()];
        ClampScrollOffsets();
        Invalidate(host);
        return true;
    }

    if (_dragVerticalThumb)
    {
        const D2D1_RECT_F track = GetVerticalScrollbarRect();
        const D2D1_RECT_F thumb = GetVerticalThumbHitRect();
        const float available   = std::max(0.0f, (track.bottom - track.top) - (thumb.bottom - thumb.top));
        if (available > 0.0f)
        {
            const float thumbTop = std::clamp(point.y - _dragThumbOffsetDip, track.top, track.bottom - (thumb.bottom - thumb.top));
            _verticalScrollDip   = ((thumbTop - track.top) / available) * GetVerticalScrollableExtent();
            ClampScrollOffsets(false);
        }
        UpdateScrollbarHotState(HitInfo{.zone = HitZone::VerticalScrollbar, .onScrollbarThumb = true});
        SyncScrollbarAnimation(host);
        Invalidate(host);
        return true;
    }

    if (_dragHorizontalThumb)
    {
        const D2D1_RECT_F track = GetHorizontalScrollbarRect();
        const D2D1_RECT_F thumb = GetHorizontalThumbHitRect();
        const float available   = std::max(0.0f, (track.right - track.left) - (thumb.right - thumb.left));
        if (available > 0.0f)
        {
            const float thumbLeft = std::clamp(point.x - _dragThumbOffsetDip, track.left, track.right - (thumb.right - thumb.left));
            _horizontalScrollDip  = ((thumbLeft - track.left) / available) * GetHorizontalScrollableExtent();
            ClampScrollOffsets(false);
        }
        UpdateScrollbarHotState(HitInfo{.zone = HitZone::HorizontalScrollbar, .onScrollbarThumb = true});
        SyncScrollbarAnimation(host);
        Invalidate(host);
        return true;
    }

    if (_dragReorderColumn)
    {
        const size_t nextTargetDisplayIndex = ResolveHeaderReorderTargetDisplayIndex(point.x);
        if (nextTargetDisplayIndex != _dragReorderTargetDisplayIndex)
        {
            _dragReorderTargetDisplayIndex = nextTargetDisplayIndex;
            Invalidate(host);
        }
        return true;
    }

    if (_pressedHeaderColumn && std::fabs(point.x - _pressedHeaderOriginXDip) >= kHeaderReorderStartDip)
    {
        _dragReorderColumn             = _pressedHeaderColumn;
        _dragReorderTargetDisplayIndex = ResolveHeaderReorderTargetDisplayIndex(point.x);
        ++_debugHeaderReorderStartCount;
        _debugLastHeaderReorderColumn                       = _dragReorderColumn.value_or(0u);
        _debugLastHeaderReorderRawTargetDisplayIndex        = _dragReorderTargetDisplayIndex;
        _debugLastHeaderReorderNormalizedTargetDisplayIndex = _dragReorderTargetDisplayIndex;
        Invalidate(host);
        return true;
    }

    const HitInfo hit              = HitTestPoint(MakePointDip(point));
    const bool scrollbarHotChanged = UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    const std::optional<uint64_t> previousHoveredRow  = _hoveredRow;
    const std::optional<size_t> previousHoveredColumn = _hoveredColumn;
    _hoveredRow.reset();
    _hoveredColumn.reset();
    std::wstring tooltipText;
    if (hit.zone == HitZone::Cell)
    {
        _hoveredRow    = hit.rowIndex;
        _hoveredColumn = hit.columnIndex;
        GridCellData cellData{};
        _model->GetCellData(hit.rowIndex, hit.columnIndex, cellData);
        const GridColumnDesc columnDesc = _model->GetColumn(hit.columnIndex);
        const D2D1_RECT_F contentRect   = GetContentRect();
        bool visibleTextClipped         = false;
        if (UsesMultilineCellText(cellData))
        {
            // Paint lays multiline text out against the full cell rectangle, so
            // the tooltip consults that same prepared layout and its omissions.
            const GridCellLayoutMetrics cellMetrics = ComputeCellLayoutMetrics(host, hit.rectDip, columnDesc, cellData);
            visibleTextClipped                      = IsMultilineCellTextClipped(host, cellData, cellMetrics.textRect, contentRect);
        }
        else
        {
            // Paint lays single-line captions out against the full cell too (since the multiline clamp), so the
            // tooltip reads that same layout and treats a caption the viewport cuts as clipped.
            const GridCellLayoutMetrics cellMetrics = ComputeCellLayoutMetrics(host, hit.rectDip, columnDesc, cellData);
            visibleTextClipped                      = IsSingleLineCellTextClipped(host, cellData, cellMetrics.textRect, contentRect);
        }
        const bool repeatedExplicitTooltip =
            ! cellData.tooltipText.empty() && (cellData.tooltipText == cellData.text || cellData.tooltipText == BuildGridCellCopyText(cellData));
        if (! cellData.tooltipText.empty())
        {
            if (! repeatedExplicitTooltip || visibleTextClipped)
            {
                tooltipText = std::move(cellData.tooltipText);
            }
        }
        else if (visibleTextClipped)
        {
            tooltipText = std::move(cellData.text);
        }
    }
    else if (hit.zone == HitZone::Header)
    {
        _hoveredColumn                   = hit.columnIndex;
        const GridColumnDesc column      = _model->GetColumn(hit.columnIndex);
        const D2D1_RECT_F headerBounds   = D2D1::RectF(GetBounds().left, GetBounds().top, GetContentRect().right, GetBounds().top + _headerHeightDip);
        const D2D1_RECT_F headerClipRect = ClipRectToRect(hit.rectDip, headerBounds);
        const GridSortGlyphVisualState sortGlyphState = ResolveSortGlyphVisualState(host.GetTheme(), hit.columnIndex, ::GetTickCount64());
        float titleRight                              = headerClipRect.right - 8.0f;
        if (sortGlyphState.reservesSpace)
        {
            titleRight -= 18.0f;
        }
        const D2D1_RECT_F titleRect =
            D2D1::RectF(headerClipRect.left + 8.0f, headerClipRect.top + 2.0f, std::max(headerClipRect.left + 24.0f, titleRight), headerClipRect.bottom - 2.0f);
        const float titleWidthDip  = std::max(0.0f, titleRect.right - titleRect.left);
        const float titleHeightDip = std::max(1.0f, titleRect.bottom - titleRect.top);
        if (titleWidthDip <= 0.5f || MeasureSingleLineTextWidthDip(&host, column.title, FontRole::Header, titleHeightDip) > (titleWidthDip + 0.5f))
        {
            tooltipText = column.title;
        }
    }

    const bool hoverChanged   = _hoveredRow != previousHoveredRow || _hoveredColumn != previousHoveredColumn;
    const bool tooltipChanged = tooltipText.empty() ? host.BeginTooltipHideDelay() : host.SetTooltip(std::move(tooltipText), point);
    if (scrollbarHotChanged || hoverChanged || tooltipChanged)
    {
        Invalidate(host);
    }
    return hit.zone != HitZone::None;
}

bool Grid::OnMouseLeave(ControlHost& host)
{
    const bool hadHoverOrHotState = _hoveredRow.has_value() || _hoveredColumn.has_value() || _verticalScrollbarHotPart != ScrollbarHotPart::None ||
                                    _horizontalScrollbarHotPart != ScrollbarHotPart::None;
    _hoveredRow.reset();
    _hoveredColumn.reset();
    UpdateScrollbarHotState(HitInfo{});
    SyncScrollbarAnimation(host);
    const bool tooltipChanged = host.ClearTooltip();
    if (hadHoverOrHotState || tooltipChanged)
    {
        Invalidate(host);
    }
    return true;
}

bool Grid::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    if (! _model)
    {
        return false;
    }

    host.SetFocusControl(this);
    const HitInfo hit = HitTestPoint(MakePointDip(point));
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    switch (hit.zone)
    {
        case HitZone::HeaderResize:
            if (! rightButton)
            {
                EnsureColumnWidths();
                _resizeColumn          = hit.columnIndex;
                _resizeOriginXDip      = point.x;
                _resizeInitialWidthDip = _columnWidths[hit.columnIndex];
                ++_debugHeaderResizeDownCount;
                _debugLastResizeDeltaDip = 0.0f;
                _debugLastResizeWidthDip = _resizeInitialWidthDip;
                return true;
            }
            break;
        case HitZone::Header:
            if (! rightButton)
            {
                _pressedHeaderColumn     = hit.columnIndex;
                _pressedHeaderOriginXDip = point.x;
                Invalidate(host);
                return true;
            }
            break;
        case HitZone::GroupHeader:
            if (! rightButton && _delegate)
            {
                const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
                if (hit.groupIndex < groups.size())
                {
                    const GridGroupDesc& group = groups[hit.groupIndex];
                    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
                    _delegate->OnGridGroupToggled(*this, group.stableId, ! group.collapsed);
                    ReconcileSelectionForVisibleRows(CollectOrderedGroups(_model));
                    _hoveredRow.reset();
                    _hoveredColumn.reset();
                    host.ClearTooltip();
                    ClampScrollOffsets();
                    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
                    {
                        _delegate->OnGridSelectionChanged(*this);
                    }
                    Invalidate(host);
                }
            }
            return true;
        case HitZone::Cell:
        {
            _activeColumn                             = hit.columnIndex;
            const IGridModel* const modelBeforeSelect = _model;
            bool clickedCheckbox                      = false;
            std::optional<uint64_t> clickedCheckboxRowId;
            if (! rightButton && modelBeforeSelect && hit.rowIndex < modelBeforeSelect->GetRowCount() && hit.columnIndex < modelBeforeSelect->GetColumnCount())
            {
                GridCellData cellData{};
                modelBeforeSelect->GetCellData(hit.rowIndex, hit.columnIndex, cellData);
                if (cellData.kind == GridCellKind::Checkbox)
                {
                    const GridCellLayoutMetrics layoutMetrics = GetCellLayoutMetrics(host, hit.rowIndex, hit.columnIndex);
                    clickedCheckbox                           = layoutMetrics.hasCheckbox && PointInRect(layoutMetrics.checkboxRect, point);
                    if (clickedCheckbox)
                    {
                        clickedCheckboxRowId = modelBeforeSelect->GetStableRowId(hit.rowIndex);
                    }
                }
            }
            const bool preserveRightClickSelection = rightButton && _selectionMode == GridSelectionMode::Extended && modelBeforeSelect &&
                                                     hit.rowIndex < modelBeforeSelect->GetRowCount() && _selectionModel.GetCount() > 1u &&
                                                     _selectionModel.IsSelected(modelBeforeSelect->GetStableRowId(hit.rowIndex));
            if (! preserveRightClickSelection)
            {
                SelectRow(hit.rowIndex, modifiers);
            }
            if (! rightButton && clickedCheckbox && clickedCheckboxRowId.has_value() && _model == modelBeforeSelect && _model &&
                hit.columnIndex < _model->GetColumnCount())
            {
                if (const auto resolvedRowIndex = _model->FindRowByStableId(clickedCheckboxRowId.value()))
                {
                    if (ToggleCheckboxCell(host, resolvedRowIndex.value(), hit.columnIndex))
                    {
                        return true;
                    }
                    Invalidate(host);
                    return true;
                }
            }
            Invalidate(host);
            if (rightButton)
            {
                return OnContextMenu(host, false, point);
            }
            return true;
        }
        case HitZone::VerticalScrollbar:
            if (! rightButton)
            {
                const D2D1_RECT_F thumb = GetVerticalThumbHitRect();
                if (hit.onScrollbarThumb)
                {
                    _dragVerticalThumb  = true;
                    _dragThumbOffsetDip = point.y - thumb.top;
                    UpdateScrollbarHotState(HitInfo{.zone = HitZone::VerticalScrollbar, .onScrollbarThumb = true});
                    SyncScrollbarAnimation(host);
                    Invalidate(host);
                }
                else
                {
                    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
                    const float viewportDip       = std::max(1.0f, contentRect.bottom - contentRect.top);
                    const float extent            = SanitizeNonNegative(GetVerticalScrollableExtent());
                    const float pageStep          = ComputeScrollbarPageStepDip(hit.rectDip, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent);
                    _verticalScrollDip += (point.y < thumb.top) ? -pageStep : pageStep;
                    ClampScrollOffsets();
                    UpdateScrollbarHotState(HitInfo{.zone = HitZone::VerticalScrollbar, .onScrollbarThumb = false});
                    SyncScrollbarAnimation(host);
                    Invalidate(host);
                }
                return true;
            }
            break;
        case HitZone::HorizontalScrollbar:
            if (! rightButton)
            {
                const D2D1_RECT_F thumb = GetHorizontalThumbHitRect();
                if (hit.onScrollbarThumb)
                {
                    _dragHorizontalThumb = true;
                    _dragThumbOffsetDip  = point.x - thumb.left;
                    UpdateScrollbarHotState(HitInfo{.zone = HitZone::HorizontalScrollbar, .onScrollbarThumb = true});
                    SyncScrollbarAnimation(host);
                    Invalidate(host);
                }
                else
                {
                    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
                    const float viewportDip       = std::max(1.0f, contentRect.right - contentRect.left);
                    const float extent            = SanitizeNonNegative(GetHorizontalScrollableExtent());
                    const float pageStep = ComputeScrollbarPageStepDip(hit.rectDip, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent);
                    _horizontalScrollDip += (point.x < thumb.left) ? -pageStep : pageStep;
                    ClampScrollOffsets();
                    UpdateScrollbarHotState(HitInfo{.zone = HitZone::HorizontalScrollbar, .onScrollbarThumb = false});
                    SyncScrollbarAnimation(host);
                    Invalidate(host);
                }
                return true;
            }
            break;
        case HitZone::None: break;
    }
    return false;
}

void Grid::OnCaptureLost(ControlHost& host)
{
    const bool hadDrag =
        _resizeColumn.has_value() || _dragVerticalThumb || _dragHorizontalThumb || _dragReorderColumn.has_value() || _pressedHeaderColumn.has_value();
    _resizeColumn.reset();
    _dragVerticalThumb   = false;
    _dragHorizontalThumb = false;
    _dragThumbOffsetDip  = 0.0f;
    _dragReorderColumn.reset();
    _pressedHeaderColumn.reset();
    _pressedHeaderOriginXDip = 0.0f;
    UpdateScrollbarHotState(HitInfo{});
    SyncScrollbarAnimation(host);
    if (hadDrag)
    {
        Invalidate(host);
    }
}

bool Grid::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    if (! _model || rightButton)
    {
        return false;
    }

    host.SetFocusControl(this);
    const HitInfo hit = HitTestPoint(MakePointDip(point));
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    if (hit.zone != HitZone::Cell || hit.rowIndex >= _model->GetRowCount() || hit.columnIndex >= _model->GetColumnCount())
    {
        return OnMouseDown(host, point, rightButton, modifiers);
    }

    _activeColumn = hit.columnIndex;

    const IGridModel* const modelBeforeSelect = _model;
    bool clickedCheckbox                      = false;
    std::optional<uint64_t> clickedCheckboxRowId;
    GridCellData cellData{};
    modelBeforeSelect->GetCellData(hit.rowIndex, hit.columnIndex, cellData);
    if (cellData.kind == GridCellKind::Checkbox)
    {
        const GridCellLayoutMetrics layoutMetrics = GetCellLayoutMetrics(host, hit.rowIndex, hit.columnIndex);
        clickedCheckbox                           = layoutMetrics.hasCheckbox && PointInRect(layoutMetrics.checkboxRect, point);
        if (clickedCheckbox)
        {
            clickedCheckboxRowId = modelBeforeSelect->GetStableRowId(hit.rowIndex);
        }
    }

    SelectRow(hit.rowIndex, modifiers);
    if (clickedCheckbox && clickedCheckboxRowId.has_value() && _model == modelBeforeSelect && _model && hit.columnIndex < _model->GetColumnCount())
    {
        if (const auto resolvedRowIndex = _model->FindRowByStableId(clickedCheckboxRowId.value()))
        {
            if (ToggleCheckboxCell(host, resolvedRowIndex.value(), hit.columnIndex))
            {
                return true;
            }
            Invalidate(host);
            return true;
        }
    }
    if (clickedCheckbox)
    {
        Invalidate(host);
        return true;
    }

    Invalidate(host);
    if (_delegate)
    {
        _delegate->OnGridRowActivated(*this, hit.rowIndex);
    }
    return true;
}

bool Grid::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT /*modifiers*/)
{
    const bool hadThumbDrag = _dragVerticalThumb || _dragHorizontalThumb;
    const bool hadDrag      = _resizeColumn.has_value() || _dragVerticalThumb || _dragHorizontalThumb || _dragReorderColumn.has_value();
    _resizeColumn.reset();
    _dragVerticalThumb       = false;
    _dragHorizontalThumb     = false;
    _dragThumbOffsetDip      = 0.0f;
    _pressedHeaderOriginXDip = 0.0f;

    if (rightButton)
    {
        return hadDrag;
    }

    UpdateScrollbarHotState(HitTestPoint(MakePointDip(point)));
    SyncScrollbarAnimation(host);
    if (hadThumbDrag)
    {
        ClampScrollOffsets();
        Invalidate(host);
    }

    if (_dragReorderColumn)
    {
        EnsureColumnWidths();
        const size_t draggedColumn         = _dragReorderColumn.value();
        const size_t rawTargetDisplayIndex = _dragReorderTargetDisplayIndex;
        _dragReorderColumn.reset();
        _pressedHeaderColumn.reset();
        Invalidate(host);

        if (draggedColumn < _columnDisplayIndexByModel.size() && ! _columnDisplayOrder.empty())
        {
            const size_t fromDisplayIndex                = _columnDisplayIndexByModel[draggedColumn];
            size_t normalizedTargetDisplayIndex          = std::min(rawTargetDisplayIndex, _columnDisplayOrder.size());
            _debugLastHeaderReorderColumn                = draggedColumn;
            _debugLastHeaderReorderFromDisplayIndex      = fromDisplayIndex;
            _debugLastHeaderReorderRawTargetDisplayIndex = rawTargetDisplayIndex;
            if (normalizedTargetDisplayIndex > fromDisplayIndex)
            {
                --normalizedTargetDisplayIndex;
            }
            _debugLastHeaderReorderNormalizedTargetDisplayIndex = normalizedTargetDisplayIndex;

            if (normalizedTargetDisplayIndex != fromDisplayIndex)
            {
                MoveColumnToDisplayIndex(draggedColumn, normalizedTargetDisplayIndex);
                ++_debugHeaderReorderCommitCount;
                Invalidate(host);
            }
            else
            {
                ++_debugHeaderReorderNoOpCount;
            }
        }
        else
        {
            ++_debugHeaderReorderNoOpCount;
        }
        RefreshAccessibilitySnapshot();
        return true;
    }

    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (_pressedHeaderColumn)
    {
        const size_t pressedColumn = _pressedHeaderColumn.value();
        _pressedHeaderColumn.reset();
        Invalidate(host);
        if (hit.zone == HitZone::Header && hit.columnIndex == pressedColumn)
        {
            GridSortSpec nextSort{};
            nextSort.columnIndex = pressedColumn;
            nextSort.direction   = (_sortSpec.columnIndex == pressedColumn) ? NextSortDirection(_sortSpec.direction) : SortDirection::Ascending;
            if (_delegate)
            {
                _delegate->OnGridSortRequested(nextSort);
            }
            else
            {
                SetSortSpec(nextSort);
            }
            return true;
        }
    }
    return hadDrag;
}

bool Grid::OnMouseWheel(ControlHost& host, D2D1_POINT_2F /*point*/, float wheelDelta, UINT /*modifiers*/)
{
    if (GetVerticalScrollableExtent() <= 0.0f)
    {
        return false;
    }

    const float beforeScrollDip = _verticalScrollDip;
    _verticalScrollDip -= (wheelDelta / WHEEL_DELTA) * (_rowHeightDip * 3.0f);
    ClampScrollOffsets();
    if (std::fabs(_verticalScrollDip - beforeScrollDip) <= kVisibleBoundaryEpsilonDip)
    {
        return false;
    }

    Invalidate(host);
    return true;
}

bool Grid::OnKeyDown(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    if (! _model || _model->GetRowCount() == 0u)
    {
        return false;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    const std::vector<size_t> visibleRows   = CollectVisibleRowIndices(_model->GetRowCount(), groups);
    if (visibleRows.empty())
    {
        return false;
    }

    if (ModifiersContainCtrl(modifiers))
    {
        if (virtualKey == 'A')
        {
            return OnSelectAll(host);
        }
        if (virtualKey == 'C')
        {
            return OnCopy(host);
        }
    }

    size_t currentRow = visibleRows.front();
    if (_selectionModel.GetCount() > 0u)
    {
        const size_t selectedRow = _model->FindRowByStableId(_selectionModel.GetOrderedSelection().back()).value_or(visibleRows.front());
        if (std::ranges::find(visibleRows, selectedRow) != visibleRows.end())
        {
            currentRow = selectedRow;
        }
    }

    const auto toggleGroupFromKeyboard = [&](size_t groupIndex, bool collapsed) noexcept
    {
        if (! _delegate || groupIndex >= groups.size())
        {
            return false;
        }

        const GridGroupDesc& group = groups[groupIndex];
        if (group.collapsed == collapsed)
        {
            return true;
        }

        const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
        std::unordered_set<uint64_t> collapsedGroupRowIds;
        const size_t toggledGroupStart = group.startRowIndex;
        if (_model && group.startRowIndex < _model->GetRowCount())
        {
            const size_t toggledGroupSpan = (std::min)(group.rowCount, _model->GetRowCount() - group.startRowIndex);
            collapsedGroupRowIds.reserve(toggledGroupSpan);
            for (size_t rowIndex = group.startRowIndex; rowIndex < (group.startRowIndex + toggledGroupSpan); ++rowIndex)
            {
                collapsedGroupRowIds.insert(_model->GetStableRowId(rowIndex));
            }
        }
        _delegate->OnGridGroupToggled(*this, group.stableId, collapsed);

        const std::vector<GridGroupDesc> updatedGroups = CollectOrderedGroups(_model);

        if (collapsed && _model)
        {
            std::vector<uint64_t> survivingSelection;
            survivingSelection.reserve(previousSelection.size());
            for (const uint64_t rowId : previousSelection)
            {
                if (! collapsedGroupRowIds.contains(rowId))
                {
                    survivingSelection.push_back(rowId);
                }
            }
            _selectionModel.PreserveOrdered(survivingSelection);
            if (_selectionModel.GetCount() == 0u && ! survivingSelection.empty())
            {
                _selectionModel.SetSingle(survivingSelection.front());
            }
            else if (_selectionModel.GetCount() == 0u)
            {
                if (const auto fallbackRow = FindNearestVisibleRow(updatedGroups, toggledGroupStart))
                {
                    _selectionModel.SetSingle(_model->GetStableRowId(fallbackRow.value()));
                }
            }
        }
        // On expand: previously selected rows remain visible — no reconciliation needed.

        _hoveredRow.reset();
        _hoveredColumn.reset();
        host.ClearTooltip();

        if (const std::optional<size_t> selectedRow = GetPrimarySelectedRow(); selectedRow.has_value())
        {
            const D2D1_RECT_F contentRect = GetContentRect();
            const float viewportHeight    = contentRect.bottom - contentRect.top;
            const float rowTop            = GetRowTopDip(updatedGroups, selectedRow.value());
            const float rowBottom         = rowTop + _rowHeightDip;
            if (rowTop < _verticalScrollDip)
            {
                _verticalScrollDip = rowTop;
            }
            else if (rowBottom > (_verticalScrollDip + viewportHeight))
            {
                _verticalScrollDip = rowBottom - viewportHeight;
            }
        }

        ClampScrollOffsets();
        if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
        {
            _delegate->OnGridSelectionChanged(*this);
        }
        RefreshAccessibilitySnapshot();
        Invalidate(host);
        return true;
    };

    const auto findOwningExpandedGroup = [&]() noexcept -> std::optional<size_t>
    {
        for (size_t groupIndex = 0u; groupIndex < groups.size(); ++groupIndex)
        {
            const GridGroupDesc& group = groups[groupIndex];
            const size_t groupEnd      = group.startRowIndex + group.rowCount;
            if (currentRow >= group.startRowIndex && currentRow < groupEnd && ! group.collapsed)
            {
                return groupIndex;
            }
        }
        return std::nullopt;
    };

    const auto findAssociatedCollapsedGroup = [&]() noexcept -> std::optional<size_t>
    {
        for (size_t groupIndex = 0u; groupIndex < groups.size(); ++groupIndex)
        {
            const GridGroupDesc& group = groups[groupIndex];
            if (! group.collapsed)
            {
                continue;
            }

            const std::optional<size_t> fallbackRow = FindNearestVisibleRow(groups, group.startRowIndex);
            if (fallbackRow.has_value() && fallbackRow.value() == currentRow)
            {
                return groupIndex;
            }
        }
        return std::nullopt;
    };

    if (! ModifiersContainAlt(modifiers) && ! ModifiersContainCtrl(modifiers) && ! groups.empty())
    {
        if (virtualKey == VK_LEFT)
        {
            if (const auto groupIndex = findOwningExpandedGroup(); groupIndex.has_value())
            {
                return toggleGroupFromKeyboard(groupIndex.value(), true);
            }
        }
        else if (virtualKey == VK_RIGHT)
        {
            if (const auto groupIndex = findAssociatedCollapsedGroup(); groupIndex.has_value())
            {
                return toggleGroupFromKeyboard(groupIndex.value(), false);
            }
        }
    }

    if (virtualKey == VK_SPACE && ! ModifiersContainAlt(modifiers))
    {
        if (const auto checkboxColumn = ResolveCheckboxToggleColumn(currentRow))
        {
            static_cast<void>(ToggleCheckboxCell(host, currentRow, checkboxColumn.value()));
            return true;
        }
    }

    const auto currentVisibleIt      = std::ranges::find(visibleRows, currentRow);
    const size_t currentVisibleIndex = currentVisibleIt == visibleRows.end() ? 0u : static_cast<size_t>(std::distance(visibleRows.begin(), currentVisibleIt));
    size_t nextVisibleIndex          = currentVisibleIndex;
    switch (virtualKey)
    {
        case VK_UP: nextVisibleIndex = currentVisibleIndex == 0u ? 0u : currentVisibleIndex - 1u; break;
        case VK_DOWN: nextVisibleIndex = std::min(currentVisibleIndex + 1u, visibleRows.size() - 1u); break;
        case VK_HOME: nextVisibleIndex = 0u; break;
        case VK_END: nextVisibleIndex = visibleRows.size() - 1u; break;
        case VK_PRIOR: nextVisibleIndex = currentVisibleIndex > 10u ? currentVisibleIndex - 10u : 0u; break;
        case VK_NEXT: nextVisibleIndex = std::min(currentVisibleIndex + 10u, visibleRows.size() - 1u); break;
        case VK_RETURN:
            if (_delegate)
            {
                _delegate->OnGridRowActivated(*this, currentRow);
            }
            return true;
        default: return false;
    }

    const size_t nextRow = visibleRows[nextVisibleIndex];
    SelectRow(nextRow, modifiers);
    const D2D1_RECT_F contentRect = GetContentRect();
    const float rowTop            = GetRowTopDip(groups, nextRow);
    const float rowBottom         = rowTop + _rowHeightDip;
    if (rowTop < _verticalScrollDip)
    {
        _verticalScrollDip = rowTop;
    }
    else if (rowBottom > (_verticalScrollDip + (contentRect.bottom - contentRect.top)))
    {
        _verticalScrollDip = rowBottom - (contentRect.bottom - contentRect.top);
    }
    ClampScrollOffsets();
    Invalidate(host);
    return true;
}

bool Grid::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip)
{
    if (! _model || ! _delegate || _model->GetRowCount() == 0u)
    {
        return false;
    }

    size_t rowIndex         = 0u;
    D2D1_POINT_2F anchorDip = pointDip;
    if (keyboardInvocation)
    {
        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
        const std::vector<size_t> visibleRows   = CollectVisibleRowIndices(_model->GetRowCount(), groups);
        if (visibleRows.empty())
        {
            return false;
        }

        rowIndex = visibleRows.front();
        if (_selectionModel.GetCount() > 0u)
        {
            const size_t selectedRow = _model->FindRowByStableId(_selectionModel.GetOrderedSelection().back()).value_or(rowIndex);
            if (std::ranges::find(visibleRows, selectedRow) != visibleRows.end())
            {
                rowIndex = selectedRow;
            }
        }

        const D2D1_RECT_F contentRect = GetContentRect();
        const float rowTop            = contentRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
        const float rowBottom         = rowTop + _rowHeightDip;
        const float minX              = contentRect.left + 4.0f;
        const float maxX              = std::max(minX, contentRect.right - 4.0f);
        const float minY              = contentRect.top + 4.0f;
        const float maxY              = std::max(minY, contentRect.bottom - 4.0f);
        anchorDip                     = D2D1::Point2F(std::clamp(GetBounds().left + 16.0f, minX, maxX), std::clamp((rowTop + rowBottom) * 0.5f, minY, maxY));
    }
    else
    {
        const HitInfo hit = HitTestPoint(MakePointDip(pointDip));
        if (hit.zone != HitZone::Cell)
        {
            return false;
        }
        rowIndex = hit.rowIndex;
    }

    _delegate->OnGridContextMenu(*this, rowIndex, host.DipPointToScreenPoint(anchorDip));
    return true;
}

bool Grid::OnCopy(ControlHost& host)
{
    return host.CopyTextToClipboard(BuildSelectionTsv());
}

bool Grid::OnSelectAll(ControlHost& host)
{
    if (! _model || _selectionMode == GridSelectionMode::Single)
    {
        return false;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    const std::vector<uint64_t> allRows     = CollectVisibleOrderedRowIds(_model, groups);
    if (allRows.empty())
    {
        return false;
    }
    _selectionModel.SetRange(allRows, allRows.front(), allRows.back());
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();
    Invalidate(host);
    return true;
}

WindowHostCursorKind Grid::ResolveCursorKind(ControlHost& /*host*/, D2D1_POINT_2F pointDip) const noexcept
{
    if (_resizeColumn.has_value())
    {
        return WindowHostCursorKind::HorizontalResize;
    }

    const HitInfo hit = HitTestPoint(MakePointDip(pointDip));
    if (hit.zone == HitZone::HeaderResize)
    {
        return WindowHostCursorKind::HorizontalResize;
    }
    return WindowHostCursorKind::Default;
}

Grid::HitInfo Grid::HitTestPoint(PointDip pointDip) const noexcept
{
    const D2D1_POINT_2F point = pointDip.AsD2D();
    HitInfo hit{};
    if (! _model || ! PointInRect(GetBounds(), point))
    {
        return hit;
    }

    EnsureColumnWidths();
    if (PointInRect(GetVerticalScrollbarRect(), point))
    {
        hit.zone             = HitZone::VerticalScrollbar;
        hit.rectDip          = GetVerticalScrollbarRect();
        hit.onScrollbarThumb = PointInRect(GetVerticalThumbHitRect(), point);
        return hit;
    }
    if (PointInRect(GetHorizontalScrollbarRect(), point))
    {
        hit.zone             = HitZone::HorizontalScrollbar;
        hit.rectDip          = GetHorizontalScrollbarRect();
        hit.onScrollbarThumb = PointInRect(GetHorizontalThumbHitRect(), point);
        return hit;
    }

    if (point.y < (GetBounds().top + _headerHeightDip))
    {
        float x = GetBounds().left - _horizontalScrollDip;
        for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
        {
            const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float width          = _columnWidths[columnIndex];
            const D2D1_RECT_F cellRect = D2D1::RectF(x, GetBounds().top, x + width, GetBounds().top + _headerHeightDip);
            x += width;
            if (! PointInRect(cellRect, point))
            {
                continue;
            }
            hit.columnIndex = columnIndex;
            hit.rectDip     = cellRect;
            hit.zone        = (point.x >= (cellRect.right - kHeaderResizeHitDip)) ? HitZone::HeaderResize : HitZone::Header;
            return hit;
        }
        return hit;
    }

    const D2D1_RECT_F contentRect = GetContentRect();
    if (! PointInRect(contentRect, point))
    {
        return hit;
    }
    if (_model->GetRowCount() == 0u || _model->GetColumnCount() == 0u)
    {
        return hit;
    }

    const std::vector<GridGroupDesc> groups             = CollectOrderedGroups(_model);
    const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
    for (const VisibleBodyItem& item : visibleBodyItems)
    {
        if (! PointInRect(item.rectDip, point))
        {
            continue;
        }

        if (item.kind == VisibleBodyItem::Kind::GroupHeader)
        {
            hit.zone       = HitZone::GroupHeader;
            hit.groupIndex = item.groupIndex;
            hit.rectDip    = item.rectDip;
            return hit;
        }

        float x = GetBounds().left - _horizontalScrollDip;
        for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
        {
            const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float width          = _columnWidths[columnIndex];
            const D2D1_RECT_F cellRect = D2D1::RectF(x, item.rectDip.top, x + width, item.rectDip.bottom);
            x += width;
            if (! PointInRect(cellRect, point))
            {
                continue;
            }

            hit.zone        = HitZone::Cell;
            hit.rowIndex    = item.rowIndex;
            hit.columnIndex = columnIndex;
            hit.rectDip     = cellRect;
            return hit;
        }
    }
    return hit;
}

void Grid::ClampScrollOffsets(const bool normalizeVertical) noexcept
{
    if (! _model || _model->GetGroupCount() == 0u)
    {
        constexpr std::span<const GridGroupDesc> noGroups;
        _verticalScrollDip = ClampScroll(_verticalScrollDip, GetVerticalScrollableExtent(noGroups));
        if (normalizeVertical)
        {
            _verticalScrollDip = NormalizeVerticalScrollOffset(_verticalScrollDip, noGroups);
        }
        _horizontalScrollDip = ClampScroll(_horizontalScrollDip, GetHorizontalScrollableExtent());
        return;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    _verticalScrollDip                      = ClampScroll(_verticalScrollDip, GetVerticalScrollableExtent(groups));
    if (normalizeVertical)
    {
        _verticalScrollDip = NormalizeVerticalScrollOffset(_verticalScrollDip, groups);
    }
    _horizontalScrollDip = ClampScroll(_horizontalScrollDip, GetHorizontalScrollableExtent());
}

// Column width caching: _columnWidths is mutable and modified in const methods (EnsureColumnWidths).
// This is safe because DxUi follows a single-threaded rendering model — all ControlHost operations
// occur on the same UI thread. The mutable qualifier allows lazy initialization during const Paint() calls.
void Grid::EnsureColumnWidths() const
{
    if (! _model)
    {
        _columnWidths.clear();
        _columnDisplayOrder.clear();
        _columnDisplayIndexByModel.clear();
        return;
    }
    if (_columnWidths.size() == _model->GetColumnCount() && _columnDisplayOrder.size() == _model->GetColumnCount() &&
        _columnDisplayIndexByModel.size() == _model->GetColumnCount())
    {
        return;
    }

    _columnWidths.clear();
    _columnWidths.reserve(_model->GetColumnCount());
    for (size_t columnIndex = 0; columnIndex < _model->GetColumnCount(); ++columnIndex)
    {
        const GridColumnDesc column = _model->GetColumn(columnIndex);
        _columnWidths.push_back(std::max(column.minWidthDip, column.widthDip));
    }

    _columnDisplayOrder.resize(_model->GetColumnCount());
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        _columnDisplayOrder[displayIndex] = displayIndex;
    }
    RebuildColumnDisplayIndexLookup();
}

void Grid::SelectRow(size_t rowIndex, UINT modifiers)
{
    if (! _model || rowIndex >= _model->GetRowCount())
    {
        return;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const uint64_t rowId = _model->GetStableRowId(rowIndex);
    if (_selectionMode == GridSelectionMode::Single)
    {
        _selectionModel.SetSingle(rowId);
    }
    else if (ModifiersContainShift(modifiers) && _selectionModel.GetAnchor())
    {
        _selectionModel.SetRange(CollectVisibleOrderedRowIds(_model, CollectOrderedGroups(_model)), _selectionModel.GetAnchor().value(), rowId);
    }
    else if (ModifiersContainCtrl(modifiers))
    {
        _selectionModel.Toggle(rowId);
    }
    else
    {
        _selectionModel.SetSingle(rowId);
    }

    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        _delegate->OnGridSelectionChanged(*this);
    }
    RefreshAccessibilitySnapshot();
}

std::wstring Grid::BuildSelectionTsv() const
{
    if (! _model || _selectionModel.GetCount() == 0u)
    {
        return {};
    }

    std::wstring text;
    const auto selection                    = _selectionModel.GetOrderedSelection();
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    bool appendedRow                        = false;
    for (const uint64_t selectedRowId : selection)
    {
        const auto rowIndex = _model->FindRowByStableId(selectedRowId);
        if (! rowIndex || ! IsRowVisibleByGroupLayout(rowIndex.value(), groups))
        {
            continue;
        }
        if (appendedRow)
        {
            text.append(L"\r\n");
        }
        EnsureColumnWidths();
        for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
        {
            const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
            if (displayIndex > 0u)
            {
                text.push_back(L'\t');
            }
            GridCellData cellData{};
            _model->GetCellData(rowIndex.value(), columnIndex, cellData);
            text.append(BuildGridCellCopyText(cellData));
        }
        appendedRow = true;
    }
    return text;
}

std::optional<size_t> Grid::FindNearestVisibleRow(std::span<const GridGroupDesc> groups, size_t preferredRowIndex) const noexcept
{
    if (! _model || _model->GetRowCount() == 0u)
    {
        return std::nullopt;
    }

    const std::vector<size_t> visibleRows = CollectVisibleRowIndices(_model->GetRowCount(), groups);
    if (visibleRows.empty())
    {
        return std::nullopt;
    }

    for (const size_t rowIndex : visibleRows)
    {
        if (rowIndex >= preferredRowIndex)
        {
            return rowIndex;
        }
    }

    return visibleRows.back();
}

void Grid::ReconcileSelectionForVisibleRows(std::span<const GridGroupDesc> groups)
{
    if (! _model)
    {
        _selectionModel.Clear();
        return;
    }

    std::optional<size_t> preferredRowIndex;
    if (_selectionModel.GetCount() > 0u)
    {
        preferredRowIndex = _model->FindRowByStableId(_selectionModel.GetOrderedSelection().back());
    }

    const std::vector<uint64_t> visibleRowIds = CollectVisibleOrderedRowIds(_model, groups);
    _selectionModel.PreserveOrdered(visibleRowIds);
    if (_selectionModel.GetCount() == 0u && preferredRowIndex && ! visibleRowIds.empty())
    {
        if (const auto fallbackRowIndex = FindNearestVisibleRow(groups, preferredRowIndex.value()))
        {
            _selectionModel.SetSingle(_model->GetStableRowId(fallbackRowIndex.value()));
        }
    }
}

size_t Grid::CountGroupHeadersBeforeRow(std::span<const GridGroupDesc> groups, size_t rowIndex) const noexcept
{
    size_t count = 0u;
    for (const GridGroupDesc& group : groups)
    {
        if (group.startRowIndex > rowIndex)
        {
            break;
        }
        ++count;
    }
    return count;
}

size_t Grid::CountCollapsedRowsBeforeRow(std::span<const GridGroupDesc> groups, size_t rowIndex) const noexcept
{
    size_t count = 0u;
    for (const GridGroupDesc& group : groups)
    {
        if (! group.collapsed || group.startRowIndex >= rowIndex)
        {
            if (group.startRowIndex > rowIndex)
            {
                break;
            }
            continue;
        }

        count += std::min(group.rowCount, rowIndex - group.startRowIndex);
    }
    return count;
}

float Grid::GetRowTopDip(std::span<const GridGroupDesc> groups, size_t rowIndex) const noexcept
{
    const size_t visibleRowIndex = rowIndex - CountCollapsedRowsBeforeRow(groups, rowIndex);
    return (static_cast<float>(visibleRowIndex) * _rowHeightDip) + (static_cast<float>(CountGroupHeadersBeforeRow(groups, rowIndex)) * _groupHeaderHeightDip);
}

float Grid::GetBodyContentHeight(std::span<const GridGroupDesc> groups) const noexcept
{
    if (! _model)
    {
        return 0.0f;
    }

    size_t collapsedRowCount = 0u;
    for (const GridGroupDesc& group : groups)
    {
        if (group.collapsed)
        {
            collapsedRowCount += group.rowCount;
        }
    }

    return (static_cast<float>(_model->GetRowCount() - collapsedRowCount) * _rowHeightDip) + (static_cast<float>(groups.size()) * _groupHeaderHeightDip);
}

float Grid::NormalizeVerticalScrollOffset(float offsetDip) const noexcept
{
    return NormalizeVerticalScrollOffset(offsetDip, CollectOrderedGroups(_model));
}

float Grid::GetRawVerticalScrollableExtent(std::span<const GridGroupDesc> groups) const noexcept
{
    if (! _model)
    {
        return 0.0f;
    }

    const float bodyContentHeightDip = SanitizeNonNegative(GetBodyContentHeight(groups));
    const D2D1_RECT_F contentRect    = NormalizeFiniteRect(GetContentRect());
    const float viewportHeightDip    = std::max(0.0f, contentRect.bottom - contentRect.top);
    return std::max(0.0f, bodyContentHeightDip - viewportHeightDip);
}

float Grid::AlignVerticalScrollExtentToVisibleItemBoundary(float rawExtentDip, std::span<const GridGroupDesc> groups) const noexcept
{
    if (! _model || rawExtentDip <= 0.0f)
    {
        return 0.0f;
    }

    const size_t rowCount   = _model->GetRowCount();
    float sectionTopDip     = 0.0f;
    size_t nextUngroupedRow = 0u;
    std::optional<float> nextBoundaryDip;

    const auto consumeRows = [&](const size_t rowCountInSection) noexcept
    {
        if (rowCountInSection == 0u)
        {
            return;
        }

        const float safeRowHeightDip = std::max(1.0f, _rowHeightDip);
        if (! nextBoundaryDip.has_value())
        {
            const float sectionBottomDip = sectionTopDip + (static_cast<float>(rowCountInSection) * safeRowHeightDip);
            if (rawExtentDip <= sectionTopDip)
            {
                nextBoundaryDip = sectionTopDip;
            }
            else if (rawExtentDip < sectionBottomDip)
            {
                const size_t nextRowOffset = static_cast<size_t>(std::ceil(std::max(0.0f, rawExtentDip - sectionTopDip) / safeRowHeightDip));
                if (nextRowOffset < rowCountInSection)
                {
                    nextBoundaryDip = sectionTopDip + (static_cast<float>(nextRowOffset) * safeRowHeightDip);
                }
            }
        }

        sectionTopDip += static_cast<float>(rowCountInSection) * safeRowHeightDip;
    };

    for (const GridGroupDesc& group : groups)
    {
        if (group.startRowIndex > nextUngroupedRow)
        {
            consumeRows(group.startRowIndex - nextUngroupedRow);
        }

        if (! nextBoundaryDip.has_value() && rawExtentDip <= sectionTopDip)
        {
            nextBoundaryDip = sectionTopDip;
        }
        sectionTopDip += std::max(0.0f, _groupHeaderHeightDip);

        if (! group.collapsed)
        {
            consumeRows(group.rowCount);
        }

        nextUngroupedRow = group.startRowIndex + group.rowCount;
    }

    if (nextUngroupedRow < rowCount)
    {
        consumeRows(rowCount - nextUngroupedRow);
    }

    return nextBoundaryDip.value_or(rawExtentDip);
}

float Grid::NormalizeVerticalScrollOffset(float offsetDip, std::span<const GridGroupDesc> groups) const noexcept
{
    if (! _model)
    {
        return 0.0f;
    }

    const float rawVerticalExtentDip = GetRawVerticalScrollableExtent(groups);
    const float verticalExtentDip    = AlignVerticalScrollExtentToVisibleItemBoundary(rawVerticalExtentDip, groups);
    const float clampedOffsetDip     = ClampScroll(offsetDip, verticalExtentDip);
    if (clampedOffsetDip <= 0.0f)
    {
        return 0.0f;
    }
    if (clampedOffsetDip >= verticalExtentDip)
    {
        return verticalExtentDip;
    }
    if (clampedOffsetDip >= rawVerticalExtentDip)
    {
        return verticalExtentDip;
    }

    const size_t rowCount   = _model->GetRowCount();
    float bestBoundaryDip   = 0.0f;
    float sectionTopDip     = 0.0f;
    size_t nextUngroupedRow = 0u;

    const auto consumeRows = [&](const size_t rowCountInSection) noexcept
    {
        if (rowCountInSection == 0u)
        {
            return;
        }

        const float safeRowHeightDip = std::max(1.0f, _rowHeightDip);
        if (clampedOffsetDip >= sectionTopDip)
        {
            const float sectionOffsetDip  = clampedOffsetDip - sectionTopDip;
            const size_t rowsBeforeOffset = std::min(rowCountInSection, static_cast<size_t>(std::floor(sectionOffsetDip / safeRowHeightDip)));
            bestBoundaryDip               = std::max(bestBoundaryDip, sectionTopDip + (static_cast<float>(rowsBeforeOffset) * safeRowHeightDip));
        }
        sectionTopDip += static_cast<float>(rowCountInSection) * safeRowHeightDip;
    };

    for (const GridGroupDesc& group : groups)
    {
        if (group.startRowIndex > nextUngroupedRow)
        {
            consumeRows(group.startRowIndex - nextUngroupedRow);
        }

        if (sectionTopDip <= clampedOffsetDip)
        {
            bestBoundaryDip = std::max(bestBoundaryDip, sectionTopDip);
        }
        sectionTopDip += std::max(0.0f, _groupHeaderHeightDip);

        if (! group.collapsed)
        {
            consumeRows(group.rowCount);
        }

        nextUngroupedRow = group.startRowIndex + group.rowCount;
    }

    if (nextUngroupedRow < rowCount)
    {
        consumeRows(rowCount - nextUngroupedRow);
    }

    return ClampScroll(bestBoundaryDip, verticalExtentDip);
}

std::vector<Grid::VisibleBodyItem> Grid::BuildVisibleBodyItems(std::span<const GridGroupDesc> groups) const
{
    return BuildVisibleBodyItems(groups, GetContentRect(groups));
}

std::vector<Grid::VisibleBodyItem> Grid::BuildVisibleBodyItems(std::span<const GridGroupDesc> groups, const D2D1_RECT_F& bodyRect) const
{
    std::vector<VisibleBodyItem> visibleItems;
    if (! _model || _model->GetRowCount() == 0u)
    {
        return visibleItems;
    }

    const float viewportHeightDip = std::max(0.0f, bodyRect.bottom - bodyRect.top);
    if (viewportHeightDip <= 0.0f)
    {
        return visibleItems;
    }

    const float viewportTopDip    = _verticalScrollDip;
    const float viewportBottomDip = viewportTopDip + viewportHeightDip;
    const size_t rowCount         = _model->GetRowCount();
    const float rowHeightDip      = std::max(_rowHeightDip, 1.0f);
    const size_t visibleRowHint   = static_cast<size_t>(std::ceil(viewportHeightDip / rowHeightDip)) + 2u;
    visibleItems.reserve(std::min(rowCount, visibleRowHint) + std::min(groups.size(), visibleRowHint));

    const auto appendRows = [this, &bodyRect, viewportTopDip, viewportBottomDip, groups, &visibleItems](size_t startRowIndex, size_t sectionRowCount)
    {
        if (sectionRowCount == 0u)
        {
            return;
        }

        const float sectionTopDip    = GetRowTopDip(groups, startRowIndex);
        const float sectionBottomDip = sectionTopDip + (static_cast<float>(sectionRowCount) * _rowHeightDip);
        if (sectionBottomDip <= viewportTopDip || sectionTopDip >= viewportBottomDip)
        {
            return;
        }

        const float visibleTopDip    = std::max(sectionTopDip, viewportTopDip);
        const float visibleBottomDip = std::min(sectionBottomDip, viewportBottomDip);
        const size_t beginRowOffset  = ResolveVisibleRowStartOffset(visibleTopDip - sectionTopDip, _rowHeightDip, sectionRowCount);
        const size_t endRowOffset    = ResolveVisibleRowEndOffset(visibleBottomDip - sectionTopDip, _rowHeightDip, sectionRowCount);
        for (size_t rowOffset = beginRowOffset; rowOffset < endRowOffset; ++rowOffset)
        {
            const size_t rowIndex = startRowIndex + rowOffset;
            const float rowTopDip = bodyRect.top + sectionTopDip + (static_cast<float>(rowOffset) * _rowHeightDip) - _verticalScrollDip;
            visibleItems.push_back(VisibleBodyItem{.kind       = VisibleBodyItem::Kind::Row,
                                                   .rowIndex   = rowIndex,
                                                   .groupIndex = 0u,
                                                   .rectDip    = D2D1::RectF(GetBounds().left, rowTopDip, bodyRect.right, rowTopDip + _rowHeightDip)});
        }
    };

    size_t nextUngroupedRow = 0u;
    for (size_t groupIndex = 0u; groupIndex < groups.size(); ++groupIndex)
    {
        const GridGroupDesc& group = groups[groupIndex];
        if (group.startRowIndex >= nextUngroupedRow)
        {
            appendRows(nextUngroupedRow, group.startRowIndex - nextUngroupedRow);
        }

        const float headerTopDip    = GetRowTopDip(groups, group.startRowIndex) - _groupHeaderHeightDip;
        const float headerBottomDip = headerTopDip + _groupHeaderHeightDip;
        if (headerBottomDip > viewportTopDip && headerTopDip < viewportBottomDip)
        {
            const float top = bodyRect.top + headerTopDip - _verticalScrollDip;
            visibleItems.push_back(VisibleBodyItem{.kind       = VisibleBodyItem::Kind::GroupHeader,
                                                   .rowIndex   = group.startRowIndex,
                                                   .groupIndex = groupIndex,
                                                   .rectDip    = D2D1::RectF(GetBounds().left, top, bodyRect.right, top + _groupHeaderHeightDip)});
        }

        if (! group.collapsed)
        {
            appendRows(group.startRowIndex, group.rowCount);
        }
        nextUngroupedRow = group.startRowIndex + group.rowCount;
    }

    if (rowCount >= nextUngroupedRow)
    {
        appendRows(nextUngroupedRow, rowCount - nextUngroupedRow);
    }
    return visibleItems;
}

float Grid::GetVerticalScrollableExtent() const
{
    return GetVerticalScrollableExtent(CollectOrderedGroups(_model));
}

float Grid::GetVerticalScrollableExtent(std::span<const GridGroupDesc> groups) const
{
    return AlignVerticalScrollExtentToVisibleItemBoundary(GetRawVerticalScrollableExtent(groups), groups);
}

float Grid::GetHorizontalScrollableExtent() const noexcept
{
    if (! _model)
    {
        return 0.0f;
    }
    EnsureColumnWidths();
    float totalWidth = 0.0f;
    for (const float width : _columnWidths)
    {
        totalWidth += SanitizeNonNegative(width);
    }
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    const float viewportWidthDip  = std::max(0.0f, contentRect.right - contentRect.left);
    return std::max(0.0f, totalWidth - viewportWidthDip);
}

D2D1_RECT_F Grid::GetContentRect() const noexcept
{
    if (! _model)
    {
        return GetContentRect(std::span<const GridGroupDesc>{});
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model);
    return GetContentRect(groups);
}

D2D1_RECT_F Grid::GetContentRect(std::span<const GridGroupDesc> groups) const noexcept
{
    const D2D1_RECT_F bounds    = NormalizeFiniteRect(GetBounds());
    const float headerHeightDip = (std::isfinite(_headerHeightDip) && _headerHeightDip > 0.0f) ? _headerHeightDip : 0.0f;

    if (! _model)
    {
        return NormalizeFiniteRect(D2D1::RectF(bounds.left, bounds.top + headerHeightDip, bounds.right, bounds.bottom));
    }

    EnsureColumnWidths();
    float totalWidth = 0.0f;
    for (const float width : _columnWidths)
    {
        totalWidth += SanitizeNonNegative(width);
    }

    const float bodyContentHeightDip = SanitizeNonNegative(GetBodyContentHeight(groups));
    bool needVScroll                 = false;
    bool needHScroll                 = false;
    for (size_t iteration = 0u; iteration < 3u; ++iteration)
    {
        const float viewportWidthDip  = std::max(0.0f, (bounds.right - bounds.left) - (needVScroll ? kScrollbarThicknessDip : 0.0f));
        const float viewportHeightDip = std::max(0.0f, (bounds.bottom - bounds.top - headerHeightDip) - (needHScroll ? kScrollbarThicknessDip : 0.0f));
        const bool nextNeedVScroll    = bodyContentHeightDip > viewportHeightDip;
        const bool nextNeedHScroll    = totalWidth > viewportWidthDip;
        if (nextNeedVScroll == needVScroll && nextNeedHScroll == needHScroll)
        {
            break;
        }
        needVScroll = nextNeedVScroll;
        needHScroll = nextNeedHScroll;
    }

    return NormalizeFiniteRect(D2D1::RectF(bounds.left,
                                           bounds.top + headerHeightDip,
                                           bounds.right - (needVScroll ? kScrollbarThicknessDip : 0.0f),
                                           bounds.bottom - (needHScroll ? kScrollbarThicknessDip : 0.0f)));
}

D2D1_RECT_F Grid::GetVerticalScrollbarRect() const noexcept
{
    const D2D1_RECT_F content = NormalizeFiniteRect(GetContentRect());
    const D2D1_RECT_F bounds  = NormalizeFiniteRect(GetBounds());
    return NormalizeFiniteRect(D2D1::RectF(content.right, content.top, bounds.right, content.bottom));
}

D2D1_RECT_F Grid::GetHorizontalScrollbarRect() const noexcept
{
    const D2D1_RECT_F content = NormalizeFiniteRect(GetContentRect());
    const D2D1_RECT_F bounds  = NormalizeFiniteRect(GetBounds());
    return NormalizeFiniteRect(D2D1::RectF(content.left, content.bottom, content.right, bounds.bottom));
}

Grid::VisibleColumnSpan Grid::ComputeVisibleColumnSpan(float clipRightDip) const noexcept
{
    VisibleColumnSpan span{};
    if (_columnDisplayOrder.empty() || clipRightDip <= GetBounds().left)
    {
        return span;
    }

    float x           = GetBounds().left - _horizontalScrollDip;
    bool foundVisible = false;
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
        const float width        = _columnWidths[columnIndex];
        const float left         = x;
        const float right        = x + width;
        x                        = right;

        if (right <= GetBounds().left)
        {
            continue;
        }
        if (left >= clipRightDip)
        {
            break;
        }

        if (! foundVisible)
        {
            span.beginIndex = displayIndex;
            span.beginXDip  = left;
            foundVisible    = true;
        }
        span.endIndex = displayIndex + 1u;
    }

    if (! foundVisible)
    {
        span.beginIndex = 0u;
        span.endIndex   = 0u;
        span.beginXDip  = GetBounds().left;
    }

    return span;
}

D2D1_RECT_F Grid::GetVerticalThumbRect() const noexcept
{
    const D2D1_RECT_F track = NormalizeFiniteRect(GetVerticalScrollbarRect());
    const float extent      = SanitizeNonNegative(GetVerticalScrollableExtent());
    if (extent <= 0.0f)
    {
        return D2D1::RectF();
    }

    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    const float viewportDip       = std::max(1.0f, contentRect.bottom - contentRect.top);
    return ComputeScrollbarThumbRect(track, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent, _verticalScrollDip, extent);
}

D2D1_RECT_F Grid::GetHorizontalThumbRect() const noexcept
{
    const D2D1_RECT_F track = NormalizeFiniteRect(GetHorizontalScrollbarRect());
    const float extent      = SanitizeNonNegative(GetHorizontalScrollableExtent());
    if (extent <= 0.0f)
    {
        return D2D1::RectF();
    }

    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    const float viewportDip       = std::max(1.0f, contentRect.right - contentRect.left);
    return ComputeScrollbarThumbRect(track, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent, _horizontalScrollDip, extent);
}

D2D1_RECT_F Grid::GetVerticalThumbHitRect() const noexcept
{
    const D2D1_RECT_F track = NormalizeFiniteRect(GetVerticalScrollbarRect());
    const float extent      = SanitizeNonNegative(GetVerticalScrollableExtent());
    if (extent <= 0.0f)
    {
        return D2D1::RectF();
    }

    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    const float viewportDip       = std::max(1.0f, contentRect.bottom - contentRect.top);
    return ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent, _verticalScrollDip, extent);
}

D2D1_RECT_F Grid::GetHorizontalThumbHitRect() const noexcept
{
    const D2D1_RECT_F track = NormalizeFiniteRect(GetHorizontalScrollbarRect());
    const float extent      = SanitizeNonNegative(GetHorizontalScrollableExtent());
    if (extent <= 0.0f)
    {
        return D2D1::RectF();
    }

    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    const float viewportDip       = std::max(1.0f, contentRect.right - contentRect.left);
    return ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent, _horizontalScrollDip, extent);
}

float Grid::GetColumnLeftDip(size_t columnIndex) const noexcept
{
    if (columnIndex >= _columnDisplayIndexByModel.size())
    {
        return GetBounds().left - _horizontalScrollDip;
    }

    float left                = GetBounds().left - _horizontalScrollDip;
    const size_t displayIndex = _columnDisplayIndexByModel[columnIndex];
    for (size_t currentDisplayIndex = 0; currentDisplayIndex < displayIndex && currentDisplayIndex < _columnDisplayOrder.size(); ++currentDisplayIndex)
    {
        left += _columnWidths[_columnDisplayOrder[currentDisplayIndex]];
    }
    return left;
}

size_t Grid::GetModelColumnIndexForDisplayIndex(size_t displayIndex) const noexcept
{
    return displayIndex < _columnDisplayOrder.size() ? _columnDisplayOrder[displayIndex] : displayIndex;
}

size_t Grid::ResolveHeaderReorderTargetDisplayIndex(float xDip) const noexcept
{
    if (_columnDisplayOrder.empty())
    {
        return 0u;
    }

    float x = GetBounds().left - _horizontalScrollDip;
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
        const float width        = (columnIndex < _columnWidths.size()) ? _columnWidths[columnIndex] : 0.0f;
        const float midpoint     = x + (width * 0.5f);
        if (xDip < midpoint)
        {
            return displayIndex;
        }
        x += width;
    }

    return _columnDisplayOrder.size();
}

void Grid::MoveColumnToDisplayIndex(size_t columnIndex, size_t targetDisplayIndex) noexcept
{
    if (columnIndex >= _columnDisplayIndexByModel.size() || _columnDisplayOrder.empty())
    {
        return;
    }

    const size_t fromDisplayIndex = _columnDisplayIndexByModel[columnIndex];
    if (fromDisplayIndex >= _columnDisplayOrder.size())
    {
        return;
    }

    const size_t clampedTargetDisplayIndex = std::min(targetDisplayIndex, _columnDisplayOrder.size() - 1u);
    if (fromDisplayIndex == clampedTargetDisplayIndex)
    {
        return;
    }

    const size_t movedColumnIndex = _columnDisplayOrder[fromDisplayIndex];
    _columnDisplayOrder.erase(_columnDisplayOrder.begin() + static_cast<std::ptrdiff_t>(fromDisplayIndex));
    _columnDisplayOrder.insert(_columnDisplayOrder.begin() + static_cast<std::ptrdiff_t>(clampedTargetDisplayIndex), movedColumnIndex);
    RebuildColumnDisplayIndexLookup();
    ClampScrollOffsets();
}

void Grid::RebuildColumnDisplayIndexLookup() const noexcept
{
    _columnDisplayIndexByModel.assign(_columnDisplayOrder.size(), 0u);
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        const size_t modelIndex = _columnDisplayOrder[displayIndex];
        if (modelIndex < _columnDisplayIndexByModel.size())
        {
            _columnDisplayIndexByModel[modelIndex] = displayIndex;
        }
    }
}

SortDirection NextSortDirection(SortDirection current) noexcept
{
    switch (current)
    {
        case SortDirection::None: return SortDirection::Ascending;
        case SortDirection::Ascending: return SortDirection::Descending;
        case SortDirection::Descending: return SortDirection::None;
        default: return SortDirection::Ascending;
    }
}

VisibleSpan ComputeVisibleSpan(uint64_t totalItems, float itemExtentDip, float scrollOffsetDip, float viewportExtentDip) noexcept
{
    VisibleSpan span{};
    if (totalItems == 0u || itemExtentDip <= 0.0f || viewportExtentDip <= 0.0f)
    {
        return span;
    }

    const float clampedScroll = std::max(0.0f, scrollOffsetDip);
    span.beginIndex           = std::min<uint64_t>(static_cast<uint64_t>(clampedScroll / itemExtentDip), totalItems);
    const float visibleItems  = std::ceil(viewportExtentDip / itemExtentDip) + 1.0f;
    span.endIndex             = std::min<uint64_t>(totalItems, span.beginIndex + static_cast<uint64_t>(std::max(0.0f, visibleItems)));
    span.offsetDip            = std::fmod(clampedScroll, itemExtentDip);
    return span;
}

std::optional<size_t> Grid::ResolveCheckboxToggleColumn(size_t rowIndex) const
{
    if (! _model || rowIndex >= _model->GetRowCount())
    {
        return std::nullopt;
    }

    auto tryColumn = [&](size_t columnIndex) -> bool
    {
        if (columnIndex >= _model->GetColumnCount())
        {
            return false;
        }
        GridCellData cellData{};
        _model->GetCellData(rowIndex, columnIndex, cellData);
        return cellData.kind == GridCellKind::Checkbox;
    };

    if (_activeColumn && tryColumn(_activeColumn.value()))
    {
        return _activeColumn;
    }

    for (size_t columnIndex = 0; columnIndex < _model->GetColumnCount(); ++columnIndex)
    {
        if (tryColumn(columnIndex))
        {
            return columnIndex;
        }
    }

    return std::nullopt;
}

bool Grid::ToggleCheckboxCell(ControlHost& host, size_t rowIndex, size_t columnIndex)
{
    if (! _model || rowIndex >= _model->GetRowCount() || columnIndex >= _model->GetColumnCount())
    {
        return false;
    }

    GridCellData cellData{};
    _model->GetCellData(rowIndex, columnIndex, cellData);
    if (cellData.kind != GridCellKind::Checkbox || ! cellData.enabled)
    {
        return false;
    }

    _activeColumn = columnIndex;
    if (_delegate)
    {
        _delegate->OnGridCheckboxToggled(*this, rowIndex, columnIndex, ! cellData.checked);
        NotifyDataChanged();
    }
    Invalidate(host);
    return true;
}

} // namespace DxUi
