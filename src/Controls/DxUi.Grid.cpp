#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../Support/Diagnostics.h"
#include "DxUi.Internal.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <exception>
#include <format>
#include <new>
#include <ranges>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace DxUi
{
namespace
{
using GridModelQueryGuard = BorrowedControlModelGuard<Grid, IGridModel>;

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
            visuals.text = focused ? theme.selectionText : ResolveInactiveSelectionTextColor(theme, theme.selectionText, theme.surfaceBackground);
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
            visuals.text = focused ? theme.selectionText : ResolveInactiveSelectionTextColor(theme, theme.selectionText, theme.surfaceBackground);
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

// The ceiling of the layout tables: kCellTextLayoutMaxEntries unless a test lowered it (Grid::DebugSetTextLayoutEntryLimit).
[[nodiscard]] size_t ResolveCellTextLayoutEntryLimit(size_t debugLimit) noexcept
{
    return debugLimit != 0u ? debugLimit : kCellTextLayoutMaxEntries;
}

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

[[nodiscard]] std::vector<GridGroupDesc> CollectOrderedGroups(const IGridModel* model, const GridModelQueryGuard* guard = nullptr)
{
    std::vector<GridGroupDesc> groups;
    if (! model)
    {
        return groups;
    }

    const size_t rowCount = model->GetRowCount();
    if (guard && ! guard->IsCurrent())
    {
        return groups;
    }
    const size_t groupCount = model->GetGroupCount();
    if (guard && ! guard->IsCurrent())
    {
        return groups;
    }
    if (rowCount == 0u || groupCount == 0u)
    {
        return groups;
    }

    groups.reserve(groupCount);
    for (size_t groupIndex = 0u; groupIndex < groupCount; ++groupIndex)
    {
        GridGroupDesc group = model->GetGroup(groupIndex);
        if (guard && ! guard->IsCurrent())
        {
            return {};
        }
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

[[nodiscard]] std::vector<uint64_t> CollectVisibleOrderedRowIds(const IGridModel* model,
                                                                std::span<const GridGroupDesc> groups,
                                                                const GridModelQueryGuard* guard = nullptr)
{
    std::vector<uint64_t> rowIds;
    if (! model)
    {
        return rowIds;
    }

    const size_t rowCount = model->GetRowCount();
    if (guard && ! guard->IsCurrent())
    {
        return {};
    }
    rowIds.reserve(rowCount);

    const auto appendRows = [&](size_t beginRow, size_t endRow)
    {
        for (size_t rowIndex = beginRow; rowIndex < endRow; ++rowIndex)
        {
            rowIds.push_back(model->GetStableRowId(rowIndex));
            if (guard && ! guard->IsCurrent())
            {
                rowIds.clear();
                return false;
            }
        }
        return true;
    };

    size_t nextUngroupedRow = 0u;
    for (const GridGroupDesc& group : groups)
    {
        if (! appendRows(nextUngroupedRow, group.startRowIndex))
        {
            return {};
        }

        const size_t groupEnd = group.startRowIndex + group.rowCount;
        if (! group.collapsed)
        {
            if (! appendRows(group.startRowIndex, groupEnd))
            {
                return {};
            }
        }

        nextUngroupedRow = groupEnd;
    }

    if (! appendRows(nextUngroupedRow, rowCount))
    {
        return {};
    }
    if (guard && ! guard->IsCurrent())
    {
        return {};
    }
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

void DrawSortGlyph(ControlHost& host, const D2D1_RECT_F& rect, SortDirection direction, const D2D1_COLOR_F& color, bool rightToLeft)
{
    if (direction == SortDirection::None)
    {
        return;
    }

    const D2D1_RECT_F glyphRect =
        rightToLeft ? D2D1::RectF(rect.left, rect.top, rect.left + 20.0f, rect.bottom) : D2D1::RectF(rect.right - 20.0f, rect.top, rect.right, rect.bottom);
    DrawChevronGlyph(host, glyphRect, direction == SortDirection::Ascending ? ChevronDirection::Up : ChevronDirection::Down, color);
}

void DrawGroupDisclosureGlyph(ControlHost& host, const D2D1_RECT_F& rect, bool collapsed, const D2D1_COLOR_F& color, bool rightToLeft)
{
    const D2D1_RECT_F glyphRect = rightToLeft ? D2D1::RectF(rect.right - 24.0f, rect.top, rect.right - 4.0f, rect.bottom)
                                              : D2D1::RectF(rect.left + 4.0f, rect.top, rect.left + 24.0f, rect.bottom);
    DrawDisclosureChevron(host, glyphRect, collapsed ? 0.0f : 1.0f, color, rightToLeft ? ChevronDirection::Left : ChevronDirection::Right);
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

void IGridDelegate::OnGridFocusedRowChanged(Grid& /*sender*/, std::optional<uint64_t> /*rowId*/)
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
// A selection of up to this many ids is answered by a scan of the ids in selection order, as it always was, a larger one by a binary
// search of the ascending copy. A scan costs about 0.1 ns an id whatever order the questions come in. At 256 to 1,500 ids a search
// costs 7 to 10 ns when a processor can predict the questions (it learns which way each comparison goes) and 40 to 50 ns when it
// cannot, as with the ids of the rows on screen asked of a selection of hashed ids. In that order the two meet at about 500 to 650
// ids on the machine of Measurements/GridSelection/2026-10-01, so up to 1,024 a search can lose to the scan this model replaced and
// above it never does, in either order. The scan pays for that margin: where the search is predicted it is 3 to 9 times cheaper at
// 500 to 1,000 ids, which a paint of two dozen rows does not see. A selection of thousands of ids, which is what made a paint
// slow, is searched.
constexpr size_t kScanIds = 1024u;

// The room of a buffer for more than this many ids is given back, not kept, when the ids that replace its contents need at most half
// of it (Clear, SetSingle, SetRange, and a PreserveOrdered that drops ids). A selection that large comes from Ctrl+A or a long
// Shift+click, and the click after it would leave a model of one id holding 16 bytes for each row of the list: 3.2 MB after Ctrl+A
// over 200,000 rows. Room that is more than half used is kept, so reuse wastes no more than it uses, as a vector's growth does.
// 4,096 ids are 32 KiB a buffer, the most that a model keeps for the next selection. Giving room back costs a free and getting it
// again an allocation: 0.1 to 0.2 ms and about 0.3 ms at 200,000 ids, a microsecond or two at 20,000, against the 0.4 to 11 ms
// that the Ctrl+A over 200,000 ids took to make the selection.
constexpr size_t kReleaseIds = 4096u;

// Sorts `ids` unless they already ascend, which they do when a model's stable ids grow with its row order.
void SortRowIds(std::vector<uint64_t>& ids)
{
    if (! std::ranges::is_sorted(ids))
    {
        std::ranges::sort(ids);
    }
}

// Whether `ids` has so much more room than `needed` ids that the room is given back instead of reused.
[[nodiscard]] bool IsRoomWasted(const std::vector<uint64_t>& ids, size_t needed) noexcept
{
    return ids.capacity() > kReleaseIds && needed <= ids.capacity() / 2u;
}

// Frees the room of `ids` when IsRoomWasted says so, by swapping it with an empty vector, which allocates nothing.
void GiveBackWastedRoom(std::vector<uint64_t>& ids, size_t needed) noexcept
{
    if (IsRoomWasted(ids, needed))
    {
        std::vector<uint64_t>().swap(ids);
    }
}

// A table of bits for the ids of a selection, 16 to 32 to an id (a power of two in all), in which an id's bit is the top bits of its
// product with the golden ratio. An id whose bit is clear is not selected, for certain; one whose bit is set may be, and one id in
// 16 to 32 that is not selected is let through. PreserveOrdered asks about every row of a model, and nearly every row of a long
// list is not selected: the table answers those with one load where a binary search of hashed ids mispredicts about every other
// comparison, and only the rows it lets through pay for the exact answer. The table lives for as long as PreserveOrdered runs.
class SelectedIdFilter
{
public:
    explicit SelectedIdFilter(std::span<const uint64_t> ids)
        : _bitCount(ComputeBitCount(ids.size())),
          _words(_bitCount / 64u, uint64_t{0}),
          _shift(64u - static_cast<unsigned>(std::countr_zero(_bitCount)))
    {
        for (const uint64_t id : ids)
        {
            const size_t bit = BitOf(id);
            _words[bit >> 6u] |= uint64_t{1} << (bit & 63u);
        }
    }

    [[nodiscard]] bool MayContain(uint64_t id) const noexcept
    {
        const size_t bit = BitOf(id);
        return ((_words[bit >> 6u] >> (bit & 63u)) & 1u) != 0u;
    }

private:
    [[nodiscard]] static size_t ComputeBitCount(size_t idCount) noexcept
    {
        // Keep both the 16-bits-per-id multiplication and bit_ceil within their representable range. The exact membership
        // check after this filter preserves correctness if an unrealistically large input reaches the saturation case.
        constexpr size_t largestPowerOfTwo = size_t{1} << (std::numeric_limits<size_t>::digits - 1u);
        const size_t requestedBitCount     = idCount > (largestPowerOfTwo / 16u) ? largestPowerOfTwo : (std::max)(size_t{64}, idCount * 16u);
        return std::bit_ceil(requestedBitCount);
    }

    [[nodiscard]] size_t BitOf(uint64_t id) const noexcept
    {
        return static_cast<size_t>((id * 0x9E3779B97F4A7C15ull) >> _shift);
    }

    size_t _bitCount;
    std::vector<uint64_t> _words;
    unsigned _shift;
};
} // namespace

// Every mutator below leaves _sortedRowIds holding exactly the ids of _selectedRowIds, ascending (and as many times as
// _selectedRowIds holds each), because IsSelected reads only the sorted copy of a selection above kScanIds ids.
void GridSelectionModel::Clear() noexcept
{
    _selectedRowIds.clear();
    _sortedRowIds.clear();
    GiveBackWastedRoom(_selectedRowIds, 0u);
    GiveBackWastedRoom(_sortedRowIds, 0u);
    _anchorRowId.reset();
}

void GridSelectionModel::SetSingle(uint64_t rowId) noexcept
{
    const bool replaceBoth = IsRoomWasted(_selectedRowIds, 1u) || IsRoomWasted(_sortedRowIds, 1u);
    if (! replaceBoth && _selectedRowIds.capacity() >= 1u && _sortedRowIds.capacity() >= 1u)
    {
        // A one-element assign reuses both buffers, and uint64_t assignment cannot throw.
        _selectedRowIds.assign(1u, rowId);
        _sortedRowIds.assign(1u, rowId);
        _anchorRowId = rowId;
        return;
    }

    std::vector<uint64_t> replacementSelected;
    std::vector<uint64_t> replacementSorted;
    try
    {
        if (replaceBoth || _selectedRowIds.capacity() == 0u)
        {
            replacementSelected.reserve(1u);
            replacementSelected.push_back(rowId);
        }
        if (replaceBoth || _sortedRowIds.capacity() == 0u)
        {
            replacementSorted.reserve(1u);
            replacementSorted.push_back(rowId);
        }
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    catch (const std::length_error&)
    {
        return;
    }

    if (replaceBoth || ! replacementSelected.empty())
    {
        _selectedRowIds.swap(replacementSelected);
    }
    else
    {
        _selectedRowIds.assign(1u, rowId);
    }
    if (replaceBoth || ! replacementSorted.empty())
    {
        _sortedRowIds.swap(replacementSorted);
    }
    else
    {
        _sortedRowIds.assign(1u, rowId);
    }
    _anchorRowId = rowId;
}

void GridSelectionModel::SetAnchor(std::optional<uint64_t> rowId) noexcept
{
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
        // The anchor records the last row touched, including a row this toggle removes.
        _anchorRowId = rowId;
        return;
    }

    if (_selectedRowIds.size() == _selectedRowIds.max_size() || _sortedRowIds.size() == _sortedRowIds.max_size())
    {
        return;
    }
    const size_t requiredSize    = _selectedRowIds.size() + 1u;
    const auto geometricCapacity = [](const std::vector<uint64_t>& ids, size_t required) noexcept
    {
        const size_t maximum = ids.max_size();
        const size_t current = ids.capacity();
        const size_t growth  = (std::max)(size_t{1}, current / 2u);
        const size_t grown   = growth > maximum - current ? maximum : current + growth;
        return (std::max)(required, grown);
    };
    try
    {
        // Acquire all required room before either membership copy or its anchor changes. If the second reserve fails, the first
        // may have acquired extra capacity, but the observable selection and gesture anchor remain unchanged.
        if (_selectedRowIds.capacity() < requiredSize)
        {
            _selectedRowIds.reserve(geometricCapacity(_selectedRowIds, requiredSize));
        }
        if (_sortedRowIds.capacity() < requiredSize)
        {
            _sortedRowIds.reserve(geometricCapacity(_sortedRowIds, requiredSize));
        }
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    catch (const std::length_error&)
    {
        return;
    }

    // reserve may invalidate sortedIt; reacquire it only after both buffers are ready.
    const auto insertion = std::ranges::lower_bound(_sortedRowIds, rowId);
    _selectedRowIds.push_back(rowId);
    _sortedRowIds.insert(insertion, rowId);
    _anchorRowId = rowId;
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

    const auto [first, last] = std::minmax(anchorIt, currentIt);
    const size_t count       = static_cast<size_t>(last - first) + 1u;
    if (IsRoomWasted(_selectedRowIds, count) || IsRoomWasted(_sortedRowIds, count))
    {
        // A selection much smaller than the one that left its room behind gets room of its own. Both copies are made beside the
        // old ones and swapped in, so a failed allocation leaves the selection as it was, and the old room is freed with the
        // vectors that took it.
        std::vector<uint64_t> ordered(first, last + 1);
        std::vector<uint64_t> sorted(ordered);
        SortRowIds(sorted);
        _selectedRowIds.swap(ordered);
        _sortedRowIds.swap(sorted);
        _anchorRowId = anchorRowId;
        return;
    }

    // Both copies get their room before either changes, so a failed allocation leaves the selection as it was.
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
        if (_anchorRowId && std::ranges::find(orderedRowIds, *_anchorRowId) == orderedRowIds.end())
        {
            _anchorRowId.reset();
        }
        return;
    }

    // What stays is each occurrence in orderedRowIds of an id that is selected now, in that order. It is built beside the current
    // ids and moved in at the end, so a failed allocation leaves the selection as it was.
    std::vector<uint64_t> kept;
    kept.reserve((std::min)(orderedRowIds.size(), _selectedRowIds.size()));
    // One question for every row of the model. A scan of kScanIds ids would cost each row up to a hundred times a search, and the
    // search mispredicts about every other comparison over ids in no order, so the filter answers the rows that are not selected, and
    // the search the others, which keeps the cost of a long list at about that of reading it.
    const SelectedIdFilter filter(_sortedRowIds);
    for (const uint64_t rowId : orderedRowIds)
    {
        if (filter.MayContain(rowId) && std::ranges::binary_search(_sortedRowIds, rowId))
        {
            kept.push_back(rowId);
        }
    }
    if (kept == _selectedRowIds)
    {
        // Selection membership usually remains unchanged. A deselected Ctrl gesture can leave its anchor outside selection,
        // but the anchor must still correspond to a visible row.
        if (_anchorRowId && std::ranges::find(orderedRowIds, _anchorRowId.value()) == orderedRowIds.end())
        {
            _anchorRowId.reset();
        }
        return;
    }
    if (IsRoomWasted(kept, kept.size()))
    {
        // What was reserved for a selection that shrank much further than that is not kept.
        kept.shrink_to_fit();
    }
    std::vector<uint64_t> keptSorted(kept);
    SortRowIds(keptSorted);

    if (_anchorRowId && std::ranges::find(orderedRowIds, _anchorRowId.value()) == orderedRowIds.end())
    {
        _anchorRowId.reset();
    }
    _selectedRowIds = std::move(kept);
    _sortedRowIds   = std::move(keptSorted);
}

bool GridSelectionModel::IsSelected(uint64_t rowId) const noexcept
{
    if (_selectedRowIds.size() <= kScanIds)
    {
        return std::ranges::find(_selectedRowIds, rowId) != _selectedRowIds.end();
    }
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

#if DXUI_ENABLE_DIAGNOSTICS
GridSelectionBufferDebugState GridSelectionModel::DebugGetBuffers() const noexcept
{
    return {_selectedRowIds.capacity(), _sortedRowIds.capacity()};
}
#endif

Grid::Grid()
{
    SetFocusable(true);
}

void Grid::SetModel(IGridModel* model) noexcept
{
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    ++_modelBindingRevision;
    // Non-owning pointer assignment. Caller responsible for model lifetime.
    ++_modelRevision;
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    std::vector<uint64_t> previousSelection;
    try
    {
        previousSelection.assign(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    catch (const std::exception&)
    {
        return;
    }
    const std::optional<uint64_t> previousFocus = _currentRowId;
    ReleaseCellTextResources(); // Also returns tables the old model grew.
    _model = model;
    const GridModelQueryGuard guard(*this, lifetime, model);
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
    try
    {
        if (_model)
        {
            const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
            if (! guard.IsCurrent())
            {
                return;
            }
            ReconcileSelectionForVisibleRows(groups);
            if (! guard.IsCurrent())
            {
                return;
            }
        }
        else
        {
            _selectionModel.Clear();
            _currentRowId.reset();
            _focusedRowIndex.reset();
        }
    }
    catch (const std::bad_alloc&)
    {
        // Model snapshot allocation failure leaves the newly assigned model installed with cleared view state.
        return;
    }
    catch (const std::exception&)
    {
        // A model getter failure aborts this update without crossing the noexcept public boundary.
        return;
    }
    try
    {
        ClampScrollOffsets();
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    catch (const std::exception&)
    {
        return;
    }
    if (! guard.IsCurrent())
    {
        return;
    }
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        // The delegate may rebuild the controls and destroy this grid.
        try
        {
            _delegate->OnGridSelectionChanged(*this);
        }
        catch (const std::exception&)
        {
            return;
        }
        if (! guard.IsCurrent())
        {
            return;
        }
    }
    if (_delegate && previousFocus != _currentRowId)
    {
        try
        {
            _delegate->OnGridFocusedRowChanged(*this, _currentRowId);
        }
        catch (const std::exception&)
        {
            return;
        }
        if (! guard.IsCurrent())
        {
            return;
        }
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
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    ++_modelBindingRevision;
    ++_modelRevision;
    _delegate = delegate;
}

void Grid::SetSelectionMode(GridSelectionMode mode) noexcept
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    std::vector<uint64_t> previousSelection;
    try
    {
        previousSelection.assign(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    _selectionMode = mode;
    if (_selectionMode == GridSelectionMode::Single && _selectionModel.GetCount() > 1u)
    {
        const auto selection = _selectionModel.GetOrderedSelection();
        if (! selection.empty())
        {
            const uint64_t current = _currentRowId && _selectionModel.IsSelected(*_currentRowId) ? *_currentRowId : selection.back();
            _selectionModel.SetSingle(current);
            _currentRowId    = current;
            _focusedRowIndex = model ? model->FindRowByStableId(current) : std::nullopt;
            if (! guard.IsCurrent())
            {
                return;
            }
        }
    }
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        // The delegate may rebuild the controls and destroy this grid.
        try
        {
            _delegate->OnGridSelectionChanged(*this);
        }
        catch (const std::exception&)
        {
            return;
        }
        if (! guard.IsCurrent())
        {
            return;
        }
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
    const float normalized = std::max(kMinimumInteractiveTextRowHeightDip, rowHeightDip);
    if (! _effectiveRowHeightDip && _rowHeightBaseDip == normalized)
        return;
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _effectiveRowHeightDip        = std::nullopt;
    _rowHeightBaseDip             = normalized;
    OnDensityChanged();
}

void Grid::SetEffectiveRowHeightDip(float rowHeightDip) noexcept
{
    const float normalized = std::max(kMinimumInteractiveTextRowHeightDip, rowHeightDip);
    if (_effectiveRowHeightDip == normalized)
        return;
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _effectiveRowHeightDip        = normalized;
    OnDensityChanged();
}

void Grid::SetHeaderHeightDip(float headerHeightDip) noexcept
{
    const float normalized = std::max(0.0f, headerHeightDip);
    if (_headerHeightBaseDip == normalized)
        return;
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    _headerHeightBaseDip          = normalized;
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    try
    {
        EnsureColumnWidths();
        if (! guard.IsCurrent() || ! model)
        {
            return;
        }
        const size_t columnCount = model->GetColumnCount();
        if (! guard.IsCurrent() || columnCount == 0u)
        {
            return;
        }

        std::unordered_map<std::wstring, size_t> modelIndexById;
        modelIndexById.reserve(columnCount);
        std::vector<GridColumnDesc> columns;
        columns.reserve(columnCount);
        for (size_t modelIndex = 0; modelIndex < columnCount; ++modelIndex)
        {
            GridColumnDesc column = model->GetColumn(modelIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
            if (! column.id.empty())
            {
                modelIndexById.try_emplace(column.id, modelIndex);
            }
            columns.push_back(std::move(column));
        }

        std::vector<float> widths = _columnWidths;
        std::vector<bool> widthApplied(columnCount, false);
        std::vector<bool> orderUsed(columnCount, false);
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
                if (std::isfinite(entry.widthDip) && entry.widthDip > 0.0f)
                {
                    widths[modelIndex] = std::max(columns[modelIndex].minWidthDip, entry.widthDip);
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

        std::vector<size_t> displayOrder;
        displayOrder.reserve(columnCount);
        for (const auto& orderedColumn : orderedColumns)
        {
            displayOrder.push_back(orderedColumn.second);
        }
        for (size_t modelIndex = 0; modelIndex < columnCount; ++modelIndex)
        {
            if (! orderUsed[modelIndex])
            {
                displayOrder.push_back(modelIndex);
            }
        }
        if (! guard.IsCurrent())
        {
            return;
        }
        _columnWidths       = std::move(widths);
        _columnDisplayOrder = std::move(displayOrder);
        RebuildColumnDisplayIndexLookup();
        ClampScrollOffsets();
        if (guard.IsCurrent())
        {
            RefreshAccessibilitySnapshot();
        }
    }
    catch (const std::bad_alloc&)
    {
        // Leave the last committed layout intact if model metadata or its replacement layout cannot be allocated.
    }
    catch (const std::exception&)
    {
        // A throwing column getter aborts layout application without crossing this noexcept boundary.
    }
}

std::vector<GridColumnLayoutEntry> Grid::CaptureColumnLayout() const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    std::vector<GridColumnLayoutEntry> layout;
    EnsureColumnWidths();
    if (! guard.IsCurrent() || ! model)
    {
        return layout;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnCount == 0u)
    {
        return layout;
    }
    layout.reserve(columnCount);
    for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
    {
        const size_t modelIndex     = _columnDisplayOrder[displayIndex];
        const GridColumnDesc column = model->GetColumn(modelIndex);
        if (! guard.IsCurrent())
        {
            return {};
        }
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    try
    {
        if (! model || ! _delegate)
        {
            return;
        }
        const size_t groupCount = model->GetGroupCount();
        if (! guard.IsCurrent() || groupCount == 0u)
        {
            return;
        }
        std::unordered_map<uint64_t, bool> collapsedByStableId;
        collapsedByStableId.reserve(layout.size());
        for (const GridGroupLayoutEntry& entry : layout)
        {
            if (entry.groupStableId != 0u)
            {
                collapsedByStableId.try_emplace(entry.groupStableId, entry.collapsed);
            }
        }
        if (collapsedByStableId.empty())
        {
            return;
        }

        std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
        const std::optional<uint64_t> previousFocus = _currentRowId;
        std::vector<uint64_t> groupIds;
        const std::vector<GridGroupDesc> initialGroups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return;
        }
        groupIds.reserve((std::min)(collapsedByStableId.size(), initialGroups.size()));
        for (const GridGroupDesc& group : initialGroups)
        {
            if (collapsedByStableId.contains(group.stableId))
            {
                groupIds.push_back(group.stableId);
            }
        }

        IGridDelegate* const delegateBeforeCallbacks = _delegate;
        bool changed                                 = false;
        for (const uint64_t groupId : groupIds)
        {
            const auto requested = collapsedByStableId.find(groupId);
            if (requested == collapsedByStableId.end())
            {
                continue;
            }
            const std::vector<GridGroupDesc> currentGroups = CollectOrderedGroups(model, &guard);
            if (! guard.IsCurrent())
            {
                return;
            }
            const auto groupIt = std::ranges::find(currentGroups, groupId, &GridGroupDesc::stableId);
            if (groupIt == currentGroups.end() || groupIt->collapsed == requested->second)
            {
                continue;
            }

            try
            {
                delegateBeforeCallbacks->OnGridGroupToggled(*this, groupId, requested->second);
            }
            catch (const std::exception&)
            {
                return;
            }
            if (! guard.IsCurrent())
            {
                return;
            }
            previousSelection.assign(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
            changed = true;
            if (_delegate != delegateBeforeCallbacks)
            {
                break;
            }
        }

        if (! changed)
        {
            return;
        }
        const std::vector<GridGroupDesc> updatedGroups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return;
        }
        ReconcileSelectionForVisibleRows(updatedGroups);
        if (! guard.IsCurrent())
        {
            return;
        }
        _hoveredRow.reset();
        _hoveredColumn.reset();
        ClampScrollOffsets();
        if (! guard.IsCurrent())
        {
            return;
        }
        if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
        {
            _delegate->OnGridSelectionChanged(*this);
            if (! guard.IsCurrent())
            {
                return;
            }
        }
        if (_delegate && previousFocus != _currentRowId)
        {
            _delegate->OnGridFocusedRowChanged(*this, _currentRowId);
            if (! guard.IsCurrent())
            {
                return;
            }
        }
        RefreshAccessibilitySnapshot();
        if (guard.IsCurrent())
        {
            RequestInvalidate();
        }
    }
    catch (const std::bad_alloc&)
    {
        // Preserve the last committed group/selection state on allocation failure.
    }
    catch (const std::exception&)
    {
        // Model/delegate exceptions abort group-layout application at this noexcept boundary.
    }
}

std::vector<GridGroupLayoutEntry> Grid::CaptureGroupLayout() const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    std::vector<GridGroupLayoutEntry> layout;
    if (! model)
    {
        return layout;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return {};
    }
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
    const auto invalidateGeometry = ControlModelQueryAccess::InvalidateGeometry(*this);
    try
    {
        ++_modelRevision;
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        IGridModel* const model           = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        _lastPaintHadAnimatedVisibleCells = false;
        _animatedVisibleCellStateValid    = false;
        const bool hasGroups              = model && model->GetGroupCount() > 0u;
        if (! guard.IsCurrent())
        {
            return;
        }
        const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
        const std::optional<uint64_t> previousFocus = _currentRowId;
        if (hasGroups)
        {
            const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
            if (! guard.IsCurrent())
            {
                return;
            }
            ReconcileSelectionForVisibleRows(groups);
        }
        else
        {
            constexpr std::span<const GridGroupDesc> noGroups;
            ReconcileSelectionForVisibleRows(noGroups);
        }
        if (! guard.IsCurrent())
        {
            return;
        }
        if (model && _activeColumn)
        {
            const size_t columnCount = model->GetColumnCount();
            if (! guard.IsCurrent())
            {
                return;
            }
            if (_activeColumn.value() >= columnCount)
            {
                _activeColumn.reset();
            }
        }
        if (! model)
        {
            _activeColumn.reset();
        }
        ClampScrollOffsets();
        if (! guard.IsCurrent())
        {
            return;
        }
        if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
        {
            // The delegate may rebuild the controls and destroy this grid.
            if (! TryControlCallback([&] { _delegate->OnGridSelectionChanged(*this); }) || ! guard.IsCurrent())
            {
                return;
            }
        }
        if (_delegate && previousFocus != _currentRowId)
        {
            if (! TryControlCallback([&] { _delegate->OnGridFocusedRowChanged(*this, _currentRowId); }) || ! guard.IsCurrent())
            {
                return;
            }
        }
        RefreshAccessibilitySnapshot();
    }
    catch (const std::bad_alloc&)
    {
        // Keep the last committed selection/focus when rebuilding the model snapshot runs out of memory.
    }
    catch (const std::exception&)
    {
        // A model or delegate exception aborts this notification at the control boundary.
    }
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    GridVisibleWorkMetrics metrics{};
    if (! model)
    {
        return metrics;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return metrics;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnCount == 0u)
    {
        return metrics;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return metrics;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);

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
            model->GetCellData(item.rowIndex, GetModelColumnIndexForDisplayIndex(displayIndex), cellData);
            if (! guard.IsCurrent())
            {
                return {};
            }
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
    metrics.verticalScrollDip    = _verticalScrollDip;
    metrics.horizontalScrollDip  = _horizontalScrollDip;
    metrics.hasVerticalScrollbar = GetVerticalScrollableExtent() > 0.0f;
    if (! guard.IsCurrent())
    {
        return {};
    }
    metrics.hasHorizontalScrollbar = GetHorizontalScrollableExtent() > 0.0f;
    if (! guard.IsCurrent())
    {
        return {};
    }
    return metrics;
}

GridCellLayoutMetrics Grid::GetCellLayoutMetrics(const ControlHost& host, size_t rowIndex, size_t columnIndex) const
{
    GridCellLayoutMetrics metrics{};
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return metrics;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount || columnIndex >= columnCount)
    {
        return metrics;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top || columnIndex >= _columnWidths.size())
    {
        return metrics;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const float cellLeft = GetColumnLeftDip(columnIndex);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    const float rowTopDip           = bodyRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
    const D2D1_RECT_F cellRect      = D2D1::RectF(cellLeft, rowTopDip, cellLeft + _columnWidths[columnIndex], rowTopDip + _rowHeightDip);
    const GridColumnDesc columnDesc = model->GetColumn(columnIndex);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    GridCellData cellData;
    ResetGridCellData(cellData);
    model->GetCellData(rowIndex, columnIndex, cellData);
    if (! guard.IsCurrent())
    {
        return metrics;
    }
    return ComputeCellLayoutMetrics(host, cellRect, columnDesc, cellData);
}

size_t Grid::GetVisibleRowCount() const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return 0u;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return 0u;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return 0u;
    }
    size_t visibleRowCount = 0u;
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (! guard.IsCurrent())
        {
            return 0u;
        }
        if (item.kind == VisibleBodyItem::Kind::Row)
        {
            ++visibleRowCount;
        }
    }

    return visibleRowCount;
}

std::optional<size_t> Grid::GetVisibleRowAt(size_t visibleRowIndex) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    size_t currentVisibleRowIndex = 0u;
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }

    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    if (rowIndex >= rowCount)
    {
        return std::nullopt;
    }
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }

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
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        size_t rowCount = 0u;
        if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount)
        {
            return;
        }

        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return;
        }
        if (! FindVisibleRowOrdinal(rowIndex).has_value() || ! guard.IsCurrent())
        {
            return;
        }

        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return;
        }
        const float viewportHeight = std::max(0.0f, contentRect.bottom - contentRect.top);
        const float rowTop         = GetRowTopDip(groups, rowIndex);
        const float rowBottom      = rowTop + _rowHeightDip;
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
    catch (const std::bad_alloc&)
    {
        // Keep the last stable scroll position when visibility geometry cannot be prepared.
    }
    catch (const std::exception&)
    {
        // A failing borrowed-model query aborts this best-effort visibility request.
    }
}

size_t Grid::GetVisibleColumnCount() const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return 0u;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnCount == 0u)
    {
        return 0u;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return 0u;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent() || bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return 0u;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    if (! guard.IsCurrent())
    {
        return 0u;
    }
    return visibleColumns.endIndex - visibleColumns.beginIndex;
}

std::optional<size_t> Grid::GetVisibleColumnAt(size_t visibleColumnIndex) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnCount == 0u)
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent() || bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return std::nullopt;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const size_t visibleColumnCount = visibleColumns.endIndex - visibleColumns.beginIndex;
    if (visibleColumnIndex >= visibleColumnCount)
    {
        return std::nullopt;
    }

    return GetModelColumnIndexForDisplayIndex(visibleColumns.beginIndex + visibleColumnIndex);
}

std::optional<size_t> Grid::FindVisibleColumnOrdinal(size_t columnIndex) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnIndex >= columnCount)
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent() || bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
    {
        return std::nullopt;
    }

    const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
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
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        if (! model)
        {
            return std::nullopt;
        }
        const size_t columnCount = model->GetColumnCount();
        if (! guard.IsCurrent() || columnCount == 0u)
        {
            return std::nullopt;
        }

        const D2D1_POINT_2F point     = pointDip.AsD2D();
        const D2D1_RECT_F bounds      = NormalizeFiniteRect(GetBounds());
        const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
        if (! guard.IsCurrent() || contentRect.top <= bounds.top || point.y < bounds.top || point.y >= contentRect.top)
        {
            return std::nullopt;
        }

        const size_t visibleColumnCount = GetVisibleColumnCount();
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
        for (size_t visibleColumnIndex = 0u; visibleColumnIndex < visibleColumnCount; ++visibleColumnIndex)
        {
            const std::optional<size_t> columnIndex = GetVisibleColumnAt(visibleColumnIndex);
            if (! guard.IsCurrent())
            {
                return std::nullopt;
            }
            if (! columnIndex)
            {
                continue;
            }

            const std::optional<D2D1_RECT_F> headerRect = GetVisibleColumnHeaderRect(columnIndex.value());
            if (! guard.IsCurrent())
            {
                return std::nullopt;
            }
            if (headerRect && PointInRect(headerRect.value(), point))
            {
                return columnIndex;
            }
        }
        return std::nullopt;
    }
    catch (const std::bad_alloc&)
    {
        return std::nullopt;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

std::optional<size_t> Grid::FindRowAtPoint(PointDip pointDip) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return std::nullopt;
    }

    const HitInfo hit = HitTestPoint(pointDip);
    if (! guard.IsCurrent() || hit.zone != HitZone::Cell)
    {
        return std::nullopt;
    }

    return hit.rowIndex;
}

std::optional<std::pair<size_t, size_t>> Grid::FindCellAtPoint(PointDip pointDip) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return std::nullopt;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnCount == 0u)
    {
        return std::nullopt;
    }

    const HitInfo hit = HitTestPoint(pointDip);
    if (! guard.IsCurrent() || hit.zone != HitZone::Cell)
    {
        return std::nullopt;
    }

    return std::make_pair(hit.rowIndex, hit.columnIndex);
}

std::optional<D2D1_RECT_F> Grid::GetVisibleColumnHeaderRect(size_t columnIndex) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnIndex >= columnCount)
    {
        return std::nullopt;
    }

    if (! FindVisibleColumnOrdinal(columnIndex))
    {
        return std::nullopt;
    }
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const D2D1_RECT_F bounds      = NormalizeFiniteRect(GetBounds());
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    if (! guard.IsCurrent() || contentRect.top <= bounds.top || columnIndex >= _columnWidths.size())
    {
        return std::nullopt;
    }

    const float headerLeftDip = GetColumnLeftDip(columnIndex);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    EnsureColumnWidths();
    if (! guard.IsCurrent() || ! _model || displayIndex >= _columnDisplayOrder.size())
    {
        return std::nullopt;
    }

    const auto rect = GetVisibleColumnHeaderRect(GetModelColumnIndexForDisplayIndex(displayIndex));
    return guard.IsCurrent() ? rect : std::nullopt;
}

std::optional<D2D1_RECT_F> Grid::GetVisibleRowRect(size_t rowIndex) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    for (const VisibleBodyItem& item : BuildVisibleBodyItems(groups))
    {
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
        if (item.kind == VisibleBodyItem::Kind::Row && item.rowIndex == rowIndex)
        {
            const D2D1_RECT_F clippedRect = ClipRectToRect(item.rectDip, bodyRect);
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return std::nullopt;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount || columnIndex >= columnCount)
    {
        return std::nullopt;
    }

    if (! FindVisibleRowOrdinal(rowIndex) || ! FindVisibleColumnOrdinal(columnIndex))
    {
        return std::nullopt;
    }
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }

    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const D2D1_RECT_F bodyRect = GetContentRect();
    if (! guard.IsCurrent() || bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top || columnIndex >= _columnWidths.size())
    {
        return std::nullopt;
    }

    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const float cellLeft = GetColumnLeftDip(columnIndex);
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }
    const float rowTopDip = bodyRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }
    const uint64_t rowId = model->GetStableRowId(rowIndex);
    return guard.IsCurrent() && _selectionModel.IsSelected(rowId);
}

std::optional<size_t> Grid::GetPrimarySelectedRow() const noexcept
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model || _selectionModel.GetCount() == 0u)
    {
        return std::nullopt;
    }

    if (_currentRowId && _selectionModel.IsSelected(_currentRowId.value()))
    {
        const std::optional<size_t> currentRow = model->FindRowByStableId(_currentRowId.value());
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
        if (currentRow)
        {
            return currentRow;
        }
    }

    const auto selection = _selectionModel.GetOrderedSelection();
    if (selection.empty())
    {
        return std::nullopt;
    }
    const std::optional<size_t> resolved = model->FindRowByStableId(selection.back());
    return guard.IsCurrent() ? resolved : std::nullopt;
}

std::optional<uint64_t> Grid::GetFocusedRowId() const noexcept
{
    return _currentRowId;
}

void Grid::SetFocusedRowId(std::optional<uint64_t> rowId) noexcept
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    std::optional<size_t> focusedIndex;
    if (rowId && (! model || ! (focusedIndex = model->FindRowByStableId(*rowId))))
    {
        rowId.reset();
    }
    if (! guard.IsCurrent())
    {
        return;
    }
    const bool changed = _currentRowId != rowId;
    _currentRowId      = rowId;
    _focusedRowIndex   = rowId ? focusedIndex : std::nullopt;
    RefreshAccessibilitySnapshot();
    if (changed && guard.IsCurrent())
    {
        RequestInvalidate();
    }
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
    out                               = {};
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }

    const uint64_t rowId = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    const bool rowSelected = _selectionModel.IsSelected(rowId);
    GridRowStyle style{};
    if (! TryControlCallback([&] { style = model->GetRowStyle(rowIndex); }) || ! guard.IsCurrent())
    {
        return false;
    }
    const GridResolvedRowVisuals visuals =
        ResolveGridRowVisuals(theme, style, rowIndex, rowSelected, HasFocus(), _hoveredRow && _hoveredRow.value() == rowIndex, _visualMode);
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
    out                               = {};
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount    = 0u;
    size_t columnCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount ||
        ! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent() || columnIndex >= columnCount)
    {
        return false;
    }

    GridCellData cellData{};
    if (! TryControlCallback([&] { model->GetCellData(rowIndex, columnIndex, cellData); }) || ! guard.IsCurrent())
    {
        return false;
    }

    const uint64_t rowId = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    const bool selected = _selectionModel.IsSelected(rowId);
    const bool hovered  = _hoveredRow && _hoveredRow.value() == rowIndex;
    GridRowStyle rowStyle{};
    if (! TryControlCallback([&] { rowStyle = model->GetRowStyle(rowIndex); }) || ! guard.IsCurrent())
    {
        return false;
    }
    const GridResolvedRowVisuals rowVisuals = ResolveGridRowVisuals(theme, rowStyle, rowIndex, selected, HasFocus(), hovered, _visualMode);
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model || ! IsEnabled())
    {
        return false;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (rowIndex >= rowCount)
    {
        return false;
    }
    const std::optional<size_t> ordinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! ordinal)
    {
        return false;
    }

    // The caller revalidates its element when the selection delegate or UI Automation publication destroys this grid.
    return SelectRow(rowIndex, modifiers);
}

bool Grid::RequestAddRowSelection(size_t rowIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model || ! IsEnabled())
    {
        return false;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }
    const std::optional<size_t> ordinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! ordinal)
    {
        return false;
    }

    const uint64_t rowId = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    if (_selectionMode == GridSelectionMode::Single)
    {
        if (_selectionModel.GetCount() != 0u && ! _selectionModel.IsSelected(rowId))
        {
            return false;
        }
        if (_selectionModel.GetCount() == 0u)
        {
            _selectionModel.SetSingle(rowId);
        }
    }
    else if (! _selectionModel.IsSelected(rowId))
    {
        const auto previousAnchor = _selectionModel.GetAnchor();
        _selectionModel.Toggle(rowId);
        _selectionModel.SetAnchor(previousAnchor);
    }

    const std::optional<uint64_t> previousFocus = _currentRowId;
    _currentRowId                               = rowId;
    _focusedRowIndex                            = rowIndex;
    EnsureRowVisible(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        if (! TryControlCallback([&] { _delegate->OnGridSelectionChanged(*this); }) || ! guard.IsCurrent())
        {
            return false;
        }
    }
    if (_delegate && previousFocus != _currentRowId)
    {
        if (! TryControlCallback([&] { _delegate->OnGridFocusedRowChanged(*this, _currentRowId); }) || ! guard.IsCurrent())
        {
            return false;
        }
    }
    RefreshAccessibilitySnapshot();
    if (guard.IsCurrent())
    {
        if (ControlHost* host = GetHost())
        {
            Invalidate(*host);
        }
    }
    return guard.IsCurrent();
}

bool Grid::RequestRemoveRowSelection(size_t rowIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! IsEnabled() || ! model)
    {
        return false;
    }

    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }
    const std::optional<size_t> ordinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! ordinal)
    {
        return false;
    }
    const uint64_t rowId = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (! _selectionModel.IsSelected(rowId))
    {
        return true;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const auto previousAnchor = _selectionModel.GetAnchor();
    if (_selectionMode == GridSelectionMode::Single || _selectionModel.GetCount() <= 1u)
    {
        _selectionModel.Clear();
    }
    else
    {
        _selectionModel.Toggle(rowId);
    }
    _selectionModel.SetAnchor(previousAnchor);

    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        // The delegate may rebuild the controls and destroy this grid; the caller revalidates its element.
        if (! TryControlCallback([&] { _delegate->OnGridSelectionChanged(*this); }) || ! guard.IsCurrent())
        {
            return false;
        }
    }
    RefreshAccessibilitySnapshot();

    return guard.IsCurrent();
}

bool Grid::RequestFocusRow(size_t rowIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! IsEnabled() || ! model)
    {
        return false;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }
    const std::optional<size_t> ordinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! ordinal)
    {
        return false;
    }

    const std::optional<uint64_t> previousFocus = _currentRowId;
    const uint64_t rowId                        = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    _currentRowId    = rowId;
    _focusedRowIndex = rowIndex;
    EnsureRowVisible(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (_delegate && previousFocus != _currentRowId)
    {
        if (! TryControlCallback([&] { _delegate->OnGridFocusedRowChanged(*this, _currentRowId); }) || ! guard.IsCurrent())
        {
            return false;
        }
    }
    RefreshAccessibilitySnapshot();
    if (guard.IsCurrent())
    {
        if (ControlHost* host = GetHost())
        {
            Invalidate(*host);
        }
    }
    return guard.IsCurrent();
}

bool Grid::RequestFocusCell(size_t rowIndex, size_t columnIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! IsEnabled() || ! model)
    {
        return false;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }
    const size_t columnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnIndex >= columnCount)
    {
        return false;
    }
    const std::optional<size_t> ordinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! ordinal)
    {
        return false;
    }
    const uint64_t rowId = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }

    const auto revealColumn = [this, columnIndex, &guard]()
    {
        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return false;
        }
        const D2D1_RECT_F viewport = GetContentRect();
        if (! guard.IsCurrent())
        {
            return false;
        }
        const float viewportWidth = viewport.right - viewport.left;
        if (viewportWidth <= 0.0f || columnIndex >= _columnWidths.size())
        {
            return true;
        }

        const float cellLeft = GetColumnLeftDip(columnIndex);
        if (! guard.IsCurrent())
        {
            return false;
        }
        const float cellRight = cellLeft + _columnWidths[columnIndex];
        float scrollDip       = _horizontalScrollDip;
        if (_columnWidths[columnIndex] >= viewportWidth)
        {
            // Align the logical leading edge: left in LTR and right in RTL.
            scrollDip += IsRightToLeft() ? viewport.right - cellRight : cellLeft - viewport.left;
        }
        else if (cellLeft < viewport.left)
        {
            scrollDip += IsRightToLeft() ? viewport.left - cellLeft : cellLeft - viewport.left;
        }
        else if (cellRight > viewport.right)
        {
            scrollDip += IsRightToLeft() ? viewport.right - cellRight : cellRight - viewport.right;
        }
        const float horizontalExtent = GetHorizontalScrollableExtent();
        if (! guard.IsCurrent())
        {
            return false;
        }
        _horizontalScrollDip = ClampScroll(scrollDip, horizontalExtent);
        return true;
    };

    _activeColumn = columnIndex;
    if (! revealColumn() || ! guard.IsCurrent() || ! RequestFocusRow(rowIndex) || ! guard.IsCurrent())
    {
        return false;
    }

    // The row-focus callback may reorder model rows or reset transient Grid state. Revalidate the requested
    // cell before restoring its column and revealing it again.
    const size_t currentColumnCount = model->GetColumnCount();
    if (! guard.IsCurrent() || columnIndex >= currentColumnCount)
    {
        return false;
    }
    const std::optional<size_t> currentRow = model->FindRowByStableId(rowId);
    if (! guard.IsCurrent() || ! currentRow)
    {
        return false;
    }
    const std::optional<size_t> currentOrdinal = FindVisibleRowOrdinal(*currentRow);
    if (! guard.IsCurrent() || ! currentOrdinal || _currentRowId != rowId)
    {
        return false;
    }

    _activeColumn = columnIndex;
    if (! revealColumn() || ! guard.IsCurrent())
    {
        return false;
    }
    RefreshAccessibilitySnapshot();
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (ControlHost* host = GetHost())
    {
        Invalidate(*host);
    }
    return guard.IsCurrent();
}

bool Grid::RequestToggleCheckboxCell(ControlHost& host, size_t rowIndex, size_t columnIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! IsEnabled() || ! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return false;
    }

    const auto visibleOrdinal = FindVisibleRowOrdinal(rowIndex);
    if (! guard.IsCurrent() || ! visibleOrdinal)
    {
        return false;
    }
    // The toggle owns its follow-up NotifyDataChanged, which intentionally advances the model revision.
    // Its result already accounts for retirement; an older query guard must not turn that accepted action into failure.
    return ToggleCheckboxCell(host, rowIndex, columnIndex) && guard.IsBindingCurrent();
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
    auto* format =
        host.GetTextFormat(_cellTextFontRole, cellData.textAlignment, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, wrap, ResolveReadingDirection(GetFlowDirection()));
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
                                                     ResolveCellTextLayoutEntryLimit(_debugTextLayoutEntryLimit),
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
                                                          ResolveCellTextLayoutEntryLimit(_debugTextLayoutEntryLimit),
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
    auto* format =
        host.GetTextFormat(_cellTextFontRole, cellData.textAlignment, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false, ResolveReadingDirection(GetFlowDirection()));
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
                                                         ResolveCellTextLayoutEntryLimit(_debugTextLayoutEntryLimit),
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
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        IGridModel* const paintModel      = _model;
        const GridModelQueryGuard guard(*this, lifetime, paintModel);
        ++_cellTextPaintGeneration;
        const auto releaseOffscreenLayouts = wil::scope_exit([&, lifetime]() noexcept
        {
            if (! guard.IsCurrent())
            {
                return;
            }
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
        const size_t paintRowCount        = paintModel ? paintModel->GetRowCount() : 0u;
        if (! guard.IsCurrent())
            return;
        const auto emitPaintPerf = wil::scope_exit([&]() noexcept
        {
            if (! guard.IsCurrent())
            {
                return;
            }

            Debug::Perf::Emit(L"dxui.grid.paint_us", L"", Debug::Perf::ElapsedUs(paintStartedAt), visibleItemCount, static_cast<uint64_t>(paintRowCount), S_OK);
            Debug::Perf::Emit(L"dxui.grid.paint_cell_data_reads", L"", 0u, visibleCellDataReadCount, visibleItemCount, S_OK);
        });

        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return;
        }
        const ThemePalette& theme                 = host.GetTheme();
        const GridSurfaceVisualStyle surfaceStyle = ResolveGridSurfaceVisualStyle(theme);
        const GridHeaderVisualStyle headerStyle   = ResolveGridHeaderVisualStyle(theme);
        const D2D1_RECT_F bounds                  = GetBounds();
        if (auto* brush = host.GetSolidBrush(surfaceStyle.fill))
        {
            dc->FillRectangle(bounds, brush);
        }
        if (auto* brush = host.GetSolidBrush(surfaceStyle.border))
        {
            dc->DrawRectangle(bounds, brush, 1.0f);
        }

        if (! paintModel)
        {
            const std::wstring_view emptyText = _emptyStateText.empty() ? std::wstring_view(L"No data") : std::wstring_view(_emptyStateText);
            DrawCenteredText(host,
                             emptyText,
                             bounds,
                             FontRole::Body,
                             surfaceStyle.emptyText,
                             DWRITE_TEXT_ALIGNMENT_CENTER,
                             DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                             false,
                             GetFlowDirection());
            return;
        }

        _cachedGroups = CollectOrderedGroups(paintModel, &guard);
        if (! guard.IsCurrent())
        {
            return;
        }
        const D2D1_RECT_F bodyRect   = GetContentRect(_cachedGroups);
        const D2D1_RECT_F headerRect = D2D1::RectF(bodyRect.left, bounds.top, bodyRect.right, bounds.top + _headerHeightDip);
        if (auto* brush = host.GetSolidBrush(surfaceStyle.headerFill))
        {
            dc->FillRectangle(headerRect, brush);
        }
        if (auto* brush = host.GetSolidBrush(surfaceStyle.headerBorder))
        {
            dc->DrawLine(D2D1::Point2F(headerRect.left, headerRect.bottom - 0.5f), D2D1::Point2F(headerRect.right, headerRect.bottom - 0.5f), brush, 1.0f);
        }
        const bool reducedMotion                            = theme.reducedMotion;
        const uint64_t animationTickMs                      = reducedMotion ? 0u : ::GetTickCount64();
        bool needsAnimation                                 = false;
        const std::optional<size_t> busyHeaderColumn        = ResolveHeaderBusyColumn();
        const VisibleColumnSpan visibleColumns              = ComputeVisibleColumnSpan(bodyRect.right);
        const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(_cachedGroups, bodyRect);
        if (! guard.IsCurrent())
        {
            return;
        }
        visibleItemCount = static_cast<uint64_t>(visibleBodyItems.size());

        dc->PushAxisAlignedClip(headerRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        auto popHeaderClip = wil::scope_exit([dc]() noexcept { dc->PopAxisAlignedClip(); });
        for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
        {
            const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float width          = _columnWidths[columnIndex];
            const float cellLeft       = GetColumnLeftDip(columnIndex);
            const D2D1_RECT_F cellRect = D2D1::RectF(cellLeft, bounds.top, cellLeft + width, headerRect.bottom);

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
            if (auto* brush = host.GetSolidBrush(fill))
            {
                dc->FillRectangle(visibleCellRect, brush);
            }
            if (displayIndex + 1 < _columnDisplayOrder.size())
            {
                if (auto* brush = host.GetSolidBrush(headerStyle.separator))
                {
                    const float separatorX = IsRightToLeft() ? visibleCellRect.left + 0.5f : visibleCellRect.right - 0.5f;
                    dc->DrawLine(D2D1::Point2F(separatorX, visibleCellRect.top), D2D1::Point2F(separatorX, visibleCellRect.bottom), brush, 1.0f);
                }
            }

            const GridColumnDesc column = paintModel->GetColumn(columnIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
            const GridSortGlyphVisualState sortGlyphState =
                ResolveSortGlyphVisualState(theme, columnIndex, reducedMotion ? _sortGlyphTransition.startTickMs : animationTickMs);
            const bool drawBusyGlyph = busyHeaderColumn && busyHeaderColumn.value() == columnIndex;
            float titleLeft          = visibleCellRect.left + 8.0f;
            float titleRight         = visibleCellRect.right - 8.0f;
            if (sortGlyphState.reservesSpace)
            {
                if (IsRightToLeft())
                    titleLeft += 18.0f;
                else
                    titleRight -= 18.0f;
            }
            if (drawBusyGlyph)
            {
                if (IsRightToLeft())
                    titleLeft += 18.0f;
                else
                    titleRight -= 18.0f;
            }
            DrawCenteredText(host,
                             column.title,
                             D2D1::RectF(titleLeft, visibleCellRect.top + 2.0f, std::max(titleLeft, titleRight), visibleCellRect.bottom - 2.0f),
                             FontRole::Header,
                             headerStyle.titleText,
                             DWRITE_TEXT_ALIGNMENT_LEADING,
                             DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                             false,
                             GetFlowDirection());
            if (drawBusyGlyph)
            {
                needsAnimation  = ! reducedMotion;
                float busyLeft  = visibleCellRect.left + 8.0f;
                float busyRight = visibleCellRect.right - 8.0f;
                if (IsRightToLeft())
                    busyLeft += sortGlyphState.reservesSpace ? 18.0f : 0.0f;
                else if (sortGlyphState.reservesSpace)
                    busyRight -= 18.0f;
                const D2D1_RECT_F busyRect = IsRightToLeft()
                                                 ? D2D1::RectF(busyLeft, visibleCellRect.top + 2.0f, busyLeft + 14.0f, visibleCellRect.bottom - 2.0f)
                                                 : D2D1::RectF(busyRight - 14.0f, visibleCellRect.top + 2.0f, busyRight, visibleCellRect.bottom - 2.0f);
                DrawCenteredText(host,
                                 SpinnerFrameForTick(animationTickMs),
                                 busyRect,
                                 FontRole::Header,
                                 headerStyle.busyGlyph,
                                 DWRITE_TEXT_ALIGNMENT_CENTER,
                                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                 false);
            }
            if (sortGlyphState.previousAlpha > 0.0f)
            {
                DrawSortGlyph(
                    host, visibleCellRect, sortGlyphState.previousDirection, WithOpacity(headerStyle.sortGlyph, sortGlyphState.previousAlpha), IsRightToLeft());
            }
            if (sortGlyphState.currentAlpha > 0.0f)
            {
                DrawSortGlyph(
                    host, visibleCellRect, sortGlyphState.currentDirection, WithOpacity(headerStyle.sortGlyph, sortGlyphState.currentAlpha), IsRightToLeft());
            }
            if (sortGlyphState.animating)
            {
                needsAnimation = true;
            }
        }
        dc->PopAxisAlignedClip();
        popHeaderClip.release();

        GridCellData cellData;
        dc->PushAxisAlignedClip(bodyRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        auto popBodyClip = wil::scope_exit([dc]() noexcept { dc->PopAxisAlignedClip(); });
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
                if (auto* brush = host.GetSolidBrush(headerStyle.groupFill))
                {
                    dc->FillRectangle(visibleItemRect, brush);
                }
                if (auto* brush = host.GetSolidBrush(headerStyle.groupSeparator))
                {
                    dc->DrawLine(D2D1::Point2F(visibleItemRect.left, visibleItemRect.bottom - 0.5f),
                                 D2D1::Point2F(visibleItemRect.right, visibleItemRect.bottom - 0.5f),
                                 brush,
                                 1.0f);
                }
                DrawGroupDisclosureGlyph(host, item.rectDip, group.collapsed, headerStyle.groupGlyph, IsRightToLeft());
                DrawCenteredText(host,
                                 group.title,
                                 IsRightToLeft() ? D2D1::RectF(bodyRect.left + 8.0f,
                                                               item.rectDip.top + 2.0f,
                                                               std::max(bodyRect.left + 8.0f, item.rectDip.right - 24.0f),
                                                               item.rectDip.bottom - 2.0f)
                                                 : D2D1::RectF(item.rectDip.left + 24.0f,
                                                               item.rectDip.top + 2.0f,
                                                               std::max(item.rectDip.left + 24.0f, bodyRect.right - 8.0f),
                                                               item.rectDip.bottom - 2.0f),
                                 FontRole::Header,
                                 headerStyle.groupText,
                                 DWRITE_TEXT_ALIGNMENT_LEADING,
                                 DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                 false,
                                 GetFlowDirection());
                continue;
            }

            const size_t rowIndex = item.rowIndex;
            const uint64_t rowId  = paintModel->GetStableRowId(rowIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
            const bool rowSelected      = _selectionModel.IsSelected(rowId);
            const bool rowHovered       = _hoveredRow && _hoveredRow.value() == rowIndex;
            const GridRowStyle rowStyle = paintModel->GetRowStyle(rowIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
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
                        if (auto* brush = host.GetSolidBrush(rowFill))
                        {
                            dc->FillRoundedRectangle(&rounded, brush);
                        }
                    }
                }
                else
                {
                    if (auto* brush = host.GetSolidBrush(rowFill))
                    {
                        dc->FillRectangle(visibleItemRect, brush);
                    }
                }
            }
            if (rowVisuals.showSeparator)
            {
                if (auto* brush = host.GetSolidBrush(surfaceStyle.rowSeparator))
                {
                    dc->DrawLine(D2D1::Point2F(visibleItemRect.left, visibleItemRect.bottom - 0.5f),
                                 D2D1::Point2F(visibleItemRect.right, visibleItemRect.bottom - 0.5f),
                                 brush,
                                 1.0f);
                }
            }

            for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
            {
                const size_t columnIndex   = GetModelColumnIndexForDisplayIndex(displayIndex);
                const float width          = _columnWidths[columnIndex];
                const float cellLeft       = GetColumnLeftDip(columnIndex);
                const D2D1_RECT_F cellRect = D2D1::RectF(cellLeft, rowRect.top, cellLeft + width, rowRect.bottom);

                const D2D1_RECT_F visibleCellRect = ClipRectToRect(cellRect, bodyRect);
                if (! IsNonEmptyRect(visibleCellRect))
                {
                    continue;
                }

                if (displayIndex + 1 < _columnDisplayOrder.size())
                {
                    if (auto* brush = host.GetSolidBrush(surfaceStyle.columnSeparator))
                    {
                        const float separatorX = IsRightToLeft() ? visibleCellRect.left + 0.5f : visibleCellRect.right - 0.5f;
                        dc->DrawLine(D2D1::Point2F(separatorX, visibleCellRect.top), D2D1::Point2F(separatorX, visibleCellRect.bottom), brush, 1.0f);
                    }
                }
                ResetGridCellData(cellData);
                paintModel->GetCellData(rowIndex, columnIndex, cellData);
                if (! guard.IsCurrent())
                {
                    return;
                }
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
                                     false,
                                     GetFlowDirection());
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
                        if (auto* brush = host.GetSolidBrush(progressStyle.track))
                        {
                            dc->FillRoundedRectangle(&trackRounded, brush);
                        }

                        D2D1_RECT_F fillRect = (cellData.progress > 0.0f) ? ComputeProgressFillRect(trackRect, cellData.progress)
                                                                          : ComputeMarqueeFillRect(trackRect, animationTickMs);
                        if (IsRightToLeft())
                        {
                            const float fillWidth = fillRect.right - fillRect.left;
                            fillRect.left         = trackRect.right - fillWidth;
                            fillRect.right        = trackRect.right;
                        }
                        if (fillRect.right > fillRect.left)
                        {
                            const D2D1_ROUNDED_RECT fillRounded = D2D1::RoundedRect(fillRect, 4.0f, 4.0f);
                            if (auto* brush = host.GetSolidBrush(progressStyle.fill))
                            {
                                dc->FillRoundedRectangle(&fillRounded, brush);
                            }
                        }
                    }

                    if (! cellData.text.empty())
                    {
                        DrawCenteredText(host,
                                         cellData.text,
                                         contentRect,
                                         FontRole::Small,
                                         rowText,
                                         DWRITE_TEXT_ALIGNMENT_CENTER,
                                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                                         false,
                                         GetFlowDirection());
                    }
                }
                else
                {
                    const GridColumnDesc columnDesc = paintModel->GetColumn(columnIndex);
                    if (! guard.IsCurrent())
                    {
                        return;
                    }
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
                        const D2D1_RECT_F indicatorRect = D2D1::RectF(layout.checkboxRect.left,
                                                                      layout.checkboxRect.top,
                                                                      layout.checkboxRect.left + indicatorSize,
                                                                      layout.checkboxRect.top + indicatorSize);
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
                            if (! guard.IsCurrent())
                            {
                                return;
                            }
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
                                             false,
                                             GetFlowDirection());
                        }
                    }

                    DrawCellText(host, cellData, layout.textRect, rowText);
                }
            }
        }
        dc->PopAxisAlignedClip();
        popBodyClip.release();

        if (needsAnimation)
        {
            host.RequestAnimation();
        }

        const D2D1_RECT_F verticalScrollbar = GetVerticalScrollbarRect();
        if (! guard.IsCurrent())
        {
            return;
        }
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
        if (! guard.IsCurrent())
        {
            return;
        }
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
    catch (const std::bad_alloc&)
    {
        // An incomplete frame is safer than allowing model allocation failure across the renderer boundary.
    }
    catch (const std::exception&)
    {
        // A throwing model/delegate query aborts the frame; the next invalidation may retry it.
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

void Grid::DebugSetTextLayoutEntryLimit(size_t maxEntries) noexcept
{
    // The tables take power-of-two sizes from kCellTextLayoutInitialEntries up, and a table at or above its ceiling stops
    // growing, so a ceiling between two sizes would let the next doubling pass it: round down to one.
    _debugTextLayoutEntryLimit = maxEntries == 0u ? 0u : std::max(kCellTextLayoutInitialEntries, std::bit_floor(maxEntries));
}

size_t Grid::DebugGetTextLayoutEntryLimit() const noexcept
{
    return ResolveCellTextLayoutEntryLimit(_debugTextLayoutEntryLimit);
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const GridModelQueryGuard guard(*this, lifetime, _model);
    if (host.GetTheme().reducedMotion)
    {
        return false;
    }

    const bool verticalScrollbarAnimating   = AdvanceScrollbarAnimation(host, _verticalScrollbarAnimation, nowTickMs);
    const bool horizontalScrollbarAnimating = AdvanceScrollbarAnimation(host, _horizontalScrollbarAnimation, nowTickMs);
    const bool animatedVisibleCells         = _animatedVisibleCellStateValid ? _lastPaintHadAnimatedVisibleCells : HasAnimatedVisibleCells();
    if (! guard.IsCurrent())
    {
        return false;
    }
    _lastPaintHadAnimatedVisibleCells = animatedVisibleCells;
    _animatedVisibleCellStateValid    = true;
    const bool sortGlyphAnimating     = _sortGlyphTransition.active && ComputeSortGlyphTransitionProgress(nowTickMs) < 1.0f;
    const bool headerBusy             = ResolveHeaderBusyColumn().has_value();
    if (! guard.IsCurrent())
    {
        return false;
    }
    const bool ticking = headerBusy || animatedVisibleCells || sortGlyphAnimating || verticalScrollbarAnimating || horizontalScrollbarAnimating;
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
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        if (! model)
        {
            return false;
        }

        size_t rowCount    = 0u;
        size_t columnCount = 0u;
        if (! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowCount == 0u ||
            ! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent() || columnCount == 0u)
        {
            return false;
        }

        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return false;
        }
        const D2D1_RECT_F bodyRect = GetContentRect();
        if (bodyRect.right <= bodyRect.left || bodyRect.bottom <= bodyRect.top)
        {
            return false;
        }

        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return false;
        }
        const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
        if (! guard.IsCurrent())
        {
            return false;
        }
        const VisibleColumnSpan visibleColumns = ComputeVisibleColumnSpan(bodyRect.right);
        GridCellData cellData;
        for (size_t displayIndex = visibleColumns.beginIndex; displayIndex < visibleColumns.endIndex; ++displayIndex)
        {
            const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
            if (! guard.IsCurrent())
            {
                return false;
            }
            for (const VisibleBodyItem& item : visibleBodyItems)
            {
                if (item.kind != VisibleBodyItem::Kind::Row)
                {
                    continue;
                }

                ResetGridCellData(cellData);
                if (! TryControlCallback([&] { model->GetCellData(item.rowIndex, columnIndex, cellData); }) || ! guard.IsCurrent())
                {
                    return false;
                }
                if (IsAnimatedCell(cellData))
                {
                    return true;
                }
            }
        }

        return false;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! _headerBusy || ! model)
    {
        return std::nullopt;
    }

    size_t columnCount = 0u;
    if (! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent() || columnCount == 0u)
    {
        return std::nullopt;
    }

    if (_headerBusyColumn && _headerBusyColumn.value() < columnCount)
    {
        return _headerBusyColumn;
    }

    if (_sortSpec.direction != SortDirection::None && _sortSpec.columnIndex < columnCount)
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
    if (IsRightToLeft())
    {
        const auto mirrorRect = [cellRect](D2D1_RECT_F& rect) noexcept
        {
            if (rect.right > rect.left)
            {
                const float left = cellRect.left + cellRect.right - rect.right;
                rect.right       = cellRect.left + cellRect.right - rect.left;
                rect.left        = left;
            }
        };
        mirrorRect(metrics.checkboxRect);
        mirrorRect(metrics.iconRect);
        mirrorRect(metrics.swatchRect);
        mirrorRect(metrics.badgeRect);
        mirrorRect(metrics.textRect);
    }
    return metrics;
}

bool Grid::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (_resizeColumn)
    {
        size_t columnCount = 0u;
        if (! model || ! TryControlCallback([&] { columnCount = model->GetColumnCount(); }))
        {
            _resizeColumn.reset();
            return false;
        }
        if (! guard.IsCurrent())
        {
            return false;
        }
        if (_resizeColumn.value() >= columnCount || _resizeColumn.value() >= _columnWidths.size())
        {
            _resizeColumn.reset();
            return false;
        }

        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return true;
        }
        const float delta = (IsRightToLeft() ? -1.0f : 1.0f) * (point.x - _resizeOriginXDip);
        GridColumnDesc column{};
        if (! TryControlCallback([&] { column = model->GetColumn(_resizeColumn.value()); }) || ! guard.IsCurrent())
        {
            return true;
        }
        _columnWidths[_resizeColumn.value()] = std::max(_resizeInitialWidthDip + delta, column.minWidthDip);
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
        if (! guard.IsCurrent())
        {
            return true;
        }
        const D2D1_RECT_F thumb = GetVerticalThumbHitRect();
        if (! guard.IsCurrent())
        {
            return true;
        }
        const float available = std::max(0.0f, (track.bottom - track.top) - (thumb.bottom - thumb.top));
        if (available > 0.0f)
        {
            const float thumbTop = std::clamp(point.y - _dragThumbOffsetDip, track.top, track.bottom - (thumb.bottom - thumb.top));
            const float extent   = GetVerticalScrollableExtent();
            if (! guard.IsCurrent())
            {
                return true;
            }
            _verticalScrollDip = ((thumbTop - track.top) / available) * extent;
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
        if (! guard.IsCurrent())
        {
            return true;
        }
        const D2D1_RECT_F thumb = GetHorizontalThumbHitRect();
        if (! guard.IsCurrent())
        {
            return true;
        }
        const float available = std::max(0.0f, (track.right - track.left) - (thumb.right - thumb.left));
        if (available > 0.0f)
        {
            const float thumbLeft     = std::clamp(point.x - _dragThumbOffsetDip, track.left, track.right - (thumb.right - thumb.left));
            const float thumbPosition = IsRightToLeft() ? (track.right - (thumbLeft + (thumb.right - thumb.left))) : (thumbLeft - track.left);
            const float extent        = GetHorizontalScrollableExtent();
            if (! guard.IsCurrent())
            {
                return true;
            }
            _horizontalScrollDip = (thumbPosition / available) * extent;
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
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (nextTargetDisplayIndex != _dragReorderTargetDisplayIndex)
        {
            _dragReorderTargetDisplayIndex = nextTargetDisplayIndex;
            Invalidate(host);
        }
        return true;
    }

    if (_pressedHeaderColumn && std::fabs(point.x - _pressedHeaderOriginXDip) >= kHeaderReorderStartDip)
    {
        const size_t nextTargetDisplayIndex = ResolveHeaderReorderTargetDisplayIndex(point.x);
        if (! guard.IsCurrent())
        {
            return true;
        }
        _dragReorderColumn             = _pressedHeaderColumn;
        _dragReorderTargetDisplayIndex = nextTargetDisplayIndex;
        ++_debugHeaderReorderStartCount;
        _debugLastHeaderReorderColumn                       = _dragReorderColumn.value_or(0u);
        _debugLastHeaderReorderRawTargetDisplayIndex        = _dragReorderTargetDisplayIndex;
        _debugLastHeaderReorderNormalizedTargetDisplayIndex = _dragReorderTargetDisplayIndex;
        Invalidate(host);
        return true;
    }

    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (! guard.IsCurrent())
    {
        return false;
    }
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
        GridColumnDesc columnDesc{};
        if (! model || ! TryControlCallback([&] { model->GetCellData(hit.rowIndex, hit.columnIndex, cellData); }) || ! guard.IsCurrent() ||
            ! TryControlCallback([&] { columnDesc = model->GetColumn(hit.columnIndex); }) || ! guard.IsCurrent())
        {
            return false;
        }
        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return false;
        }
        bool visibleTextClipped = false;
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
        _hoveredColumn = hit.columnIndex;
        GridColumnDesc column{};
        if (! model || ! TryControlCallback([&] { column = model->GetColumn(hit.columnIndex); }) || ! guard.IsCurrent())
        {
            return false;
        }
        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return false;
        }
        const D2D1_RECT_F headerBounds                = D2D1::RectF(GetBounds().left, GetBounds().top, contentRect.right, GetBounds().top + _headerHeightDip);
        const D2D1_RECT_F headerClipRect              = ClipRectToRect(hit.rectDip, headerBounds);
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
    if (! guard.IsCurrent())
    {
        return hit.zone != HitZone::None;
    }
    if (scrollbarHotChanged || hoverChanged || tooltipChanged)
    {
        Invalidate(host);
    }
    return hit.zone != HitZone::None;
}

bool Grid::OnMouseLeave(ControlHost& host)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const GridModelQueryGuard guard(*this, lifetime, _model);
    const bool hadHoverOrHotState = _hoveredRow.has_value() || _hoveredColumn.has_value() || _verticalScrollbarHotPart != ScrollbarHotPart::None ||
                                    _horizontalScrollbarHotPart != ScrollbarHotPart::None;
    _hoveredRow.reset();
    _hoveredColumn.reset();
    UpdateScrollbarHotState(HitInfo{});
    SyncScrollbarAnimation(host);
    const bool tooltipChanged = host.ClearTooltip();
    if (! guard.IsCurrent())
    {
        return true;
    }
    if (hadHoverOrHotState || tooltipChanged)
    {
        Invalidate(host);
    }
    return true;
}

bool Grid::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model)
    {
        return false;
    }

    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    if (! guard.IsCurrent())
    {
        return true;
    }
    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (! guard.IsCurrent())
    {
        return true;
    }
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    switch (hit.zone)
    {
        case HitZone::HeaderResize:
            if (! rightButton)
            {
                EnsureColumnWidths();
                if (! guard.IsCurrent())
                {
                    return true;
                }
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
                std::vector<GridGroupDesc> groups;
                if (! TryControlCallback([&] { groups = CollectOrderedGroups(model, &guard); }) || ! guard.IsCurrent())
                {
                    return true;
                }
                if (hit.groupIndex < groups.size())
                {
                    const GridGroupDesc& group = groups[hit.groupIndex];
                    if (! TryControlCallback([&] { _delegate->OnGridGroupToggled(*this, group.stableId, ! group.collapsed); }) || ! guard.IsCurrent())
                    {
                        return true;
                    }
                    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
                    const std::optional<uint64_t> previousFocus = _currentRowId;
                    std::vector<GridGroupDesc> updatedGroups;
                    if (! TryControlCallback([&] { updatedGroups = CollectOrderedGroups(model, &guard); }) || ! guard.IsCurrent())
                    {
                        return true;
                    }
                    ReconcileSelectionForVisibleRows(updatedGroups);
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    _hoveredRow.reset();
                    _hoveredColumn.reset();
                    host.ClearTooltip();
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    ClampScrollOffsets();
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    if (! NotifySelectionAndFocusChanges(previousSelection, previousFocus))
                    {
                        return true;
                    }
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    RefreshAccessibilitySnapshot();
                    if (guard.IsCurrent())
                    {
                        Invalidate(host);
                    }
                }
            }
            return true;
        case HitZone::Cell:
        {
            _activeColumn                             = hit.columnIndex;
            const IGridModel* const modelBeforeSelect = model;
            bool clickedCheckbox                      = false;
            std::optional<uint64_t> clickedCheckboxRowId;
            size_t rowCount    = 0u;
            size_t columnCount = 0u;
            if (! modelBeforeSelect || ! TryControlCallback([&] { rowCount = modelBeforeSelect->GetRowCount(); }) || ! guard.IsCurrent() ||
                hit.rowIndex >= rowCount || ! TryControlCallback([&] { columnCount = modelBeforeSelect->GetColumnCount(); }) || ! guard.IsCurrent())
            {
                return true;
            }
            if (! rightButton && hit.columnIndex < columnCount)
            {
                GridCellData cellData{};
                if (! TryControlCallback([&] { modelBeforeSelect->GetCellData(hit.rowIndex, hit.columnIndex, cellData); }) || ! guard.IsCurrent())
                {
                    return true;
                }
                if (cellData.kind == GridCellKind::Checkbox)
                {
                    const GridCellLayoutMetrics layoutMetrics = GetCellLayoutMetrics(host, hit.rowIndex, hit.columnIndex);
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    clickedCheckbox = layoutMetrics.hasCheckbox && PointInRect(layoutMetrics.checkboxRect, point);
                    if (clickedCheckbox)
                    {
                        clickedCheckboxRowId = modelBeforeSelect->GetStableRowId(hit.rowIndex);
                        if (! guard.IsCurrent())
                        {
                            return true;
                        }
                    }
                }
            }
            const bool preserveRightClickSelection = rightButton && _selectionMode == GridSelectionMode::Extended && modelBeforeSelect &&
                                                     hit.rowIndex < rowCount && _selectionModel.GetCount() > 1u && [&]()
            {
                const uint64_t rowId = modelBeforeSelect->GetStableRowId(hit.rowIndex);
                return guard.IsCurrent() && _selectionModel.IsSelected(rowId);
            }();
            if (! guard.IsCurrent())
            {
                return true;
            }
            if (! preserveRightClickSelection && ! SelectRow(hit.rowIndex, modifiers))
            {
                return true;
            }
            if (! guard.IsCurrent())
            {
                return true;
            }
            if (! rightButton && clickedCheckbox && clickedCheckboxRowId.has_value() && hit.columnIndex < columnCount)
            {
                const auto resolvedRowIndex = modelBeforeSelect->FindRowByStableId(clickedCheckboxRowId.value());
                if (! guard.IsCurrent())
                {
                    return true;
                }
                if (resolvedRowIndex)
                {
                    if (ToggleCheckboxCell(host, resolvedRowIndex.value(), hit.columnIndex))
                    {
                        return true;
                    }
                    if (! guard.IsCurrent())
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
                if (! guard.IsCurrent())
                {
                    return true;
                }
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
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    const float pageStep = ComputeScrollbarPageStepDip(hit.rectDip, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent);
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
                if (! guard.IsCurrent())
                {
                    return true;
                }
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
                    if (! guard.IsCurrent())
                    {
                        return true;
                    }
                    const float pageStep       = ComputeScrollbarPageStepDip(hit.rectDip, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent);
                    const bool pageTowardStart = IsRightToLeft() ? point.x > thumb.right : point.x < thumb.left;
                    _horizontalScrollDip += pageTowardStart ? -pageStep : pageStep;
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    GridModelQueryGuard guard(*this, lifetime, model);
    if (! model || rightButton)
    {
        return false;
    }

    if (! FocusControlAndSurvive(host, *this))
    {
        return true;
    }
    if (! guard.IsCurrent())
    {
        return true;
    }
    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (! guard.IsCurrent())
    {
        return true;
    }
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    size_t rowCount    = 0u;
    size_t columnCount = 0u;
    if (! guard.IsCurrent())
    {
        return true;
    }
    if (hit.zone != HitZone::Cell)
    {
        return OnMouseDown(host, point, rightButton, modifiers);
    }
    if (! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent())
    {
        return true;
    }
    if (hit.rowIndex >= rowCount)
    {
        return OnMouseDown(host, point, rightButton, modifiers);
    }
    if (! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent())
    {
        return true;
    }
    if (hit.columnIndex >= columnCount)
    {
        return OnMouseDown(host, point, rightButton, modifiers);
    }

    _activeColumn = hit.columnIndex;

    const IGridModel* const modelBeforeSelect = model;
    const uint64_t clickedRowId               = modelBeforeSelect->GetStableRowId(hit.rowIndex);
    if (! guard.IsCurrent())
    {
        return true;
    }
    GridColumnDesc clickedColumn{};
    if (! TryControlCallback([&] { clickedColumn = modelBeforeSelect->GetColumn(hit.columnIndex); }) || ! guard.IsCurrent())
    {
        return true;
    }
    const std::wstring clickedColumnId = std::move(clickedColumn.id);
    const auto resolveClickedColumn    = [&]() -> std::optional<size_t>
    {
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
        // Named columns retain their identity when a selection callback reorders the model's columns.
        // Models with unnamed columns retain their ordinal contract.
        if (clickedColumnId.empty())
        {
            size_t count = 0u;
            GridColumnDesc column{};
            if (! TryControlCallback([&] { count = modelBeforeSelect->GetColumnCount(); }) || ! guard.IsCurrent() || hit.columnIndex >= count ||
                ! TryControlCallback([&] { column = modelBeforeSelect->GetColumn(hit.columnIndex); }) || ! guard.IsCurrent())
            {
                return std::nullopt;
            }
            return column.id.empty() ? std::optional<size_t>(hit.columnIndex) : std::nullopt;
        }
        size_t count = 0u;
        if (! TryControlCallback([&] { count = modelBeforeSelect->GetColumnCount(); }) || ! guard.IsCurrent())
        {
            return std::nullopt;
        }
        for (size_t columnIndex = 0u; columnIndex < count; ++columnIndex)
        {
            GridColumnDesc column{};
            if (! TryControlCallback([&] { column = modelBeforeSelect->GetColumn(columnIndex); }) || ! guard.IsCurrent())
            {
                return std::nullopt;
            }
            if (column.id == clickedColumnId)
            {
                return columnIndex;
            }
        }
        return std::nullopt;
    };
    bool clickedCheckbox = false;
    std::optional<uint64_t> clickedCheckboxRowId;
    GridCellData cellData{};
    if (! TryControlCallback([&] { modelBeforeSelect->GetCellData(hit.rowIndex, hit.columnIndex, cellData); }) || ! guard.IsCurrent())
    {
        return true;
    }
    if (cellData.kind == GridCellKind::Checkbox)
    {
        const GridCellLayoutMetrics layoutMetrics = GetCellLayoutMetrics(host, hit.rowIndex, hit.columnIndex);
        if (! guard.IsCurrent())
        {
            return true;
        }
        clickedCheckbox = layoutMetrics.hasCheckbox && PointInRect(layoutMetrics.checkboxRect, point);
        if (clickedCheckbox)
        {
            clickedCheckboxRowId = modelBeforeSelect->GetStableRowId(hit.rowIndex);
            if (! guard.IsCurrent())
            {
                return true;
            }
        }
    }

    if (! clickedCheckbox && host.GetPointerDevice() == PointerDevice::Touch)
    {
        if (! SelectRow(hit.rowIndex, modifiers))
        {
            return true;
        }
        if (! guard.IsBindingCurrent())
        {
            return true;
        }
        // Selection delegates may notify a reorder in the same binding. Discard all ordinals and query its current identities.
        guard                  = GridModelQueryGuard(*this, lifetime, modelBeforeSelect);
        const auto columnIndex = resolveClickedColumn();
        if (! guard.IsCurrent() || ! columnIndex)
        {
            return true;
        }
        const auto rowIndex = modelBeforeSelect->FindRowByStableId(clickedRowId);
        if (! guard.IsCurrent() || ! rowIndex)
        {
            return true;
        }
        const auto rect = GetVisibleCellRect(*rowIndex, *columnIndex);
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (rect)
        {
            _activeColumn = *columnIndex;
            if (! TryControlCallback([&] { modelBeforeSelect->GetCellData(*rowIndex, *columnIndex, cellData); }) || ! guard.IsCurrent())
            {
                return true;
            }
            const D2D1_POINT_2F origin = D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
            static_cast<void>(host.InspectTooltip(BuildGridCellCopyText(cellData), origin));
        }
        return true;
    }

    if (! SelectRow(hit.rowIndex, modifiers))
    {
        return true;
    }
    if (! guard.IsBindingCurrent())
    {
        return true;
    }
    guard                  = GridModelQueryGuard(*this, lifetime, modelBeforeSelect);
    const auto columnIndex = resolveClickedColumn();
    if (! guard.IsCurrent())
    {
        return true;
    }
    if (clickedCheckbox && clickedCheckboxRowId.has_value() && columnIndex)
    {
        const auto resolvedRowIndex = modelBeforeSelect->FindRowByStableId(clickedCheckboxRowId.value());
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (resolvedRowIndex)
        {
            if (ToggleCheckboxCell(host, resolvedRowIndex.value(), *columnIndex))
            {
                return true;
            }
            if (! guard.IsCurrent())
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

    if (! guard.IsCurrent())
    {
        return true;
    }
    const auto rowIndex = modelBeforeSelect->FindRowByStableId(clickedRowId);
    if (! guard.IsCurrent() || ! rowIndex)
    {
        return true;
    }
    const auto visibleRowOrdinal = FindVisibleRowOrdinal(*rowIndex);
    if (! guard.IsCurrent() || ! visibleRowOrdinal)
    {
        return true;
    }
    Invalidate(host);
    if (_delegate)
    {
        static_cast<void>(TryControlCallback([&] { _delegate->OnGridRowActivated(*this, *rowIndex); }));
    }
    return true;
}

bool Grid::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT /*modifiers*/)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
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

    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (! guard.IsCurrent())
    {
        return hadDrag;
    }
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    if (hadThumbDrag)
    {
        ClampScrollOffsets();
        if (! guard.IsCurrent())
        {
            return true;
        }
        Invalidate(host);
    }

    if (_dragReorderColumn)
    {
        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return true;
        }
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
                if (! TryControlCallback([&] { _delegate->OnGridSortRequested(nextSort); }) || ! guard.IsCurrent())
                {
                    return true;
                }
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const GridModelQueryGuard guard(*this, lifetime, _model);
    const float extent = GetVerticalScrollableExtent();
    if (! guard.IsCurrent() || extent <= 0.0f)
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowCount == 0u)
    {
        return false;
    }

    std::vector<GridGroupDesc> groups;
    if (! TryControlCallback([&] { groups = CollectOrderedGroups(model, &guard); }) || ! guard.IsCurrent())
    {
        return false;
    }
    const std::vector<size_t> visibleRows = CollectVisibleRowIndices(rowCount, groups);
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

    if (virtualKey == VK_F1 && ! ModifiersContainAlt(modifiers))
    {
        const std::optional<size_t> focusedRow = _currentRowId ? model->FindRowByStableId(*_currentRowId) : std::nullopt;
        if (! guard.IsCurrent())
        {
            return true;
        }
        size_t columnCount = 0u;
        if (! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent())
        {
            return true;
        }
        const size_t columnIndex = _activeColumn && *_activeColumn < columnCount ? *_activeColumn : 0u;
        if (focusedRow && columnCount > 0u)
        {
            GridCellData cellData{};
            if (! TryControlCallback([&] { model->GetCellData(*focusedRow, columnIndex, cellData); }) || ! guard.IsCurrent())
            {
                return true;
            }
            const auto rect = GetVisibleCellRect(*focusedRow, columnIndex);
            if (! guard.IsCurrent())
            {
                return true;
            }
            if (rect)
            {
                const D2D1_POINT_2F origin = D2D1::Point2F((rect->left + rect->right) * 0.5f, (rect->top + rect->bottom) * 0.5f);
                static_cast<void>(host.InspectTooltip(BuildGridCellCopyText(cellData), origin));
            }
        }
        return true;
    }

    size_t currentRow = visibleRows.front();
    if (_currentRowId)
    {
        const std::optional<size_t> focusedRow = model->FindRowByStableId(*_currentRowId);
        if (! guard.IsCurrent())
        {
            return false;
        }
        if (focusedRow && std::ranges::find(visibleRows, *focusedRow) != visibleRows.end())
        {
            currentRow = *focusedRow;
        }
    }

    const auto toggleGroupFromKeyboard = [&](size_t groupIndex, bool collapsed)
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

        if (! TryControlCallback([&] { _delegate->OnGridGroupToggled(*this, group.stableId, collapsed); }) || ! guard.IsCurrent())
        {
            return true;
        }

        std::vector<GridGroupDesc> updatedGroups;
        if (! TryControlCallback([&] { updatedGroups = CollectOrderedGroups(model, &guard); }) || ! guard.IsCurrent())
        {
            return true;
        }
        const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
        const std::optional<uint64_t> previousFocus = _currentRowId;
        ReconcileSelectionForVisibleRows(updatedGroups);
        if (! guard.IsCurrent())
        {
            return true;
        }

        _hoveredRow.reset();
        _hoveredColumn.reset();
        host.ClearTooltip();
        if (! guard.IsCurrent())
        {
            return true;
        }

        const std::optional<size_t> selectedRow = _currentRowId ? model->FindRowByStableId(*_currentRowId) : std::nullopt;
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (selectedRow.has_value())
        {
            const D2D1_RECT_F contentRect = GetContentRect();
            if (! guard.IsCurrent())
            {
                return true;
            }
            const float viewportHeight = contentRect.bottom - contentRect.top;
            const float rowTop         = GetRowTopDip(updatedGroups, selectedRow.value());
            const float rowBottom      = rowTop + _rowHeightDip;
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
        if (! guard.IsCurrent())
        {
            return true;
        }
        // The selection's delegate may rebuild the controls and destroy this grid, and UI Automation event delivery may too.
        if (! NotifySelectionAndFocusChanges(previousSelection, previousFocus))
        {
            return true;
        }
        if (! guard.IsCurrent())
        {
            return true;
        }
        RefreshAccessibilitySnapshot();
        if (guard.IsCurrent())
        {
            Invalidate(host);
        }
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
        const bool collapseKey = IsRightToLeft() ? virtualKey == VK_RIGHT : virtualKey == VK_LEFT;
        const bool expandKey   = IsRightToLeft() ? virtualKey == VK_LEFT : virtualKey == VK_RIGHT;
        if (collapseKey)
        {
            if (const auto groupIndex = findOwningExpandedGroup(); groupIndex.has_value())
            {
                bool handled = false;
                if (! TryControlCallback([&] { handled = toggleGroupFromKeyboard(groupIndex.value(), true); }) || ! guard.IsCurrent())
                {
                    return true;
                }
                return handled;
            }
        }
        else if (expandKey)
        {
            if (const auto groupIndex = findAssociatedCollapsedGroup(); groupIndex.has_value())
            {
                bool handled = false;
                if (! TryControlCallback([&] { handled = toggleGroupFromKeyboard(groupIndex.value(), false); }) || ! guard.IsCurrent())
                {
                    return true;
                }
                return handled;
            }
        }
    }

    if (virtualKey == VK_SPACE && ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
    {
        static_cast<void>(SelectRow(currentRow, modifiers));
        return true;
    }

    if (virtualKey == VK_SPACE && ! ModifiersContainAlt(modifiers) && ! ModifiersContainCtrl(modifiers))
    {
        const auto checkboxColumn = ResolveCheckboxToggleColumn(currentRow);
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (checkboxColumn)
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
                static_cast<void>(TryControlCallback([&] { _delegate->OnGridRowActivated(*this, currentRow); }));
            }
            return true;
        default: return false;
    }

    const size_t nextRow = visibleRows[nextVisibleIndex];
    if (ModifiersContainCtrl(modifiers) && ! ModifiersContainShift(modifiers) && (virtualKey == VK_UP || virtualKey == VK_DOWN))
    {
        static_cast<void>(RequestFocusRow(nextRow));
        return true;
    }
    if (! SelectRow(nextRow, modifiers))
    {
        return true;
    }
    if (! guard.IsCurrent())
    {
        return true;
    }
    const D2D1_RECT_F contentRect = GetContentRect();
    if (! guard.IsCurrent())
    {
        return true;
    }
    const float rowTop    = GetRowTopDip(groups, nextRow);
    const float rowBottom = rowTop + _rowHeightDip;
    if (rowTop < _verticalScrollDip)
    {
        _verticalScrollDip = rowTop;
    }
    else if (rowBottom > (_verticalScrollDip + (contentRect.bottom - contentRect.top)))
    {
        _verticalScrollDip = rowBottom - (contentRect.bottom - contentRect.top);
    }
    ClampScrollOffsets();
    if (! guard.IsCurrent())
    {
        return true;
    }
    Invalidate(host);
    return true;
}

bool Grid::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! model || ! _delegate || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowCount == 0u)
    {
        return false;
    }

    size_t rowIndex         = 0u;
    D2D1_POINT_2F anchorDip = pointDip;
    if (keyboardInvocation)
    {
        std::vector<GridGroupDesc> groups;
        if (! TryControlCallback([&] { groups = CollectOrderedGroups(model, &guard); }) || ! guard.IsCurrent())
        {
            return false;
        }
        const std::vector<size_t> visibleRows = CollectVisibleRowIndices(rowCount, groups);
        if (visibleRows.empty())
        {
            return false;
        }

        rowIndex                               = visibleRows.front();
        const std::optional<size_t> primaryRow = _currentRowId ? model->FindRowByStableId(*_currentRowId) : std::nullopt;
        if (! guard.IsCurrent())
        {
            return false;
        }
        if (primaryRow)
        {
            if (std::ranges::find(visibleRows, primaryRow.value()) != visibleRows.end())
            {
                rowIndex = primaryRow.value();
            }
        }

        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return false;
        }
        const float rowTop    = contentRect.top + GetRowTopDip(groups, rowIndex) - _verticalScrollDip;
        const float rowBottom = rowTop + _rowHeightDip;
        const float minX      = contentRect.left + 4.0f;
        const float maxX      = std::max(minX, contentRect.right - 4.0f);
        const float minY      = contentRect.top + 4.0f;
        const float maxY      = std::max(minY, contentRect.bottom - 4.0f);
        anchorDip             = D2D1::Point2F(std::clamp(GetBounds().left + 16.0f, minX, maxX), std::clamp((rowTop + rowBottom) * 0.5f, minY, maxY));
    }
    else
    {
        const HitInfo hit = HitTestPoint(MakePointDip(pointDip));
        if (! guard.IsCurrent())
        {
            return false;
        }
        if (hit.zone != HitZone::Cell)
        {
            return false;
        }
        rowIndex = hit.rowIndex;
    }

    static_cast<void>(TryControlCallback([&] { _delegate->OnGridContextMenu(*this, rowIndex, host.DipPointToScreenPoint(anchorDip)); }));
    return true;
}

bool Grid::OnCopy(ControlHost& host)
{
    return host.CopyTextToClipboard(BuildSelectionTsv());
}

bool Grid::OnSelectAll(ControlHost& host)
{
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        IGridModel* const model           = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        if (! model || _selectionMode == GridSelectionMode::Single)
        {
            return false;
        }

        const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return false;
        }
        const std::vector<uint64_t> allRows = CollectVisibleOrderedRowIds(model, groups, &guard);
        if (! guard.IsCurrent())
        {
            return false;
        }
        if (allRows.empty())
        {
            return false;
        }
        _selectionModel.SetRange(allRows, allRows.front(), allRows.back());
        const std::optional<uint64_t> previousFocus = _currentRowId;
        _currentRowId                               = allRows.back();
        _focusedRowIndex                            = model->FindRowByStableId(allRows.back());
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
        {
            // The delegate may rebuild the controls and destroy this grid.
            if (! TryControlCallback([&] { _delegate->OnGridSelectionChanged(*this); }) || ! guard.IsCurrent())
            {
                return true;
            }
        }
        if (_delegate && previousFocus != _currentRowId)
        {
            if (! TryControlCallback([&] { _delegate->OnGridFocusedRowChanged(*this, _currentRowId); }) || ! guard.IsCurrent())
            {
                return true;
            }
        }
        RefreshAccessibilitySnapshot();
        if (guard.IsCurrent())
        {
            Invalidate(host);
        }
        return true;
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    catch (const std::exception&)
    {
        return false;
    }
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
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        const D2D1_POINT_2F point = pointDip.AsD2D();
        HitInfo hit{};
        if (! model || ! PointInRect(GetBounds(), point))
        {
            return hit;
        }

        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return hit;
        }
        const D2D1_RECT_F verticalScrollbar = GetVerticalScrollbarRect();
        if (! guard.IsCurrent())
        {
            return hit;
        }
        if (PointInRect(verticalScrollbar, point))
        {
            hit.zone                    = HitZone::VerticalScrollbar;
            hit.rectDip                 = verticalScrollbar;
            const D2D1_RECT_F thumbRect = GetVerticalThumbHitRect();
            if (! guard.IsCurrent())
            {
                return {};
            }
            hit.onScrollbarThumb = PointInRect(thumbRect, point);
            return hit;
        }
        const D2D1_RECT_F horizontalScrollbar = GetHorizontalScrollbarRect();
        if (! guard.IsCurrent())
        {
            return hit;
        }
        if (PointInRect(horizontalScrollbar, point))
        {
            hit.zone                    = HitZone::HorizontalScrollbar;
            hit.rectDip                 = horizontalScrollbar;
            const D2D1_RECT_F thumbRect = GetHorizontalThumbHitRect();
            if (! guard.IsCurrent())
            {
                return {};
            }
            hit.onScrollbarThumb = PointInRect(thumbRect, point);
            return hit;
        }

        if (point.y < (GetBounds().top + _headerHeightDip))
        {
            for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
            {
                const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
                const float width        = _columnWidths[columnIndex];
                const float cellLeft     = GetColumnLeftDip(columnIndex);
                if (! guard.IsCurrent())
                {
                    return {};
                }
                const D2D1_RECT_F cellRect = D2D1::RectF(cellLeft, GetBounds().top, cellLeft + width, GetBounds().top + _headerHeightDip);
                if (! PointInRect(cellRect, point))
                {
                    continue;
                }
                hit.columnIndex       = columnIndex;
                hit.rectDip           = cellRect;
                const bool resizeEdge = IsRightToLeft() ? point.x <= (cellRect.left + kHeaderResizeHitDip) : point.x >= (cellRect.right - kHeaderResizeHitDip);
                hit.zone              = resizeEdge ? HitZone::HeaderResize : HitZone::Header;
                return hit;
            }
            return hit;
        }

        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return hit;
        }
        if (! PointInRect(contentRect, point))
        {
            return hit;
        }
        const size_t rowCount = model->GetRowCount();
        if (! guard.IsCurrent() || rowCount == 0u)
        {
            return hit;
        }
        const size_t columnCount = model->GetColumnCount();
        if (! guard.IsCurrent() || columnCount == 0u)
        {
            return hit;
        }

        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return hit;
        }
        const std::vector<VisibleBodyItem> visibleBodyItems = BuildVisibleBodyItems(groups);
        if (! guard.IsCurrent())
        {
            return hit;
        }
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

            for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
            {
                const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
                const float width        = _columnWidths[columnIndex];
                const float cellLeft     = GetColumnLeftDip(columnIndex);
                if (! guard.IsCurrent())
                {
                    return {};
                }
                const D2D1_RECT_F cellRect = D2D1::RectF(cellLeft, item.rectDip.top, cellLeft + width, item.rectDip.bottom);
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
    catch (const std::bad_alloc&)
    {
        return {};
    }
    catch (const std::exception&)
    {
        return {};
    }
}

void Grid::ClampScrollOffsets(const bool normalizeVertical) noexcept
{
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        IGridModel* const model           = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        const size_t groupCount = model ? model->GetGroupCount() : 0u;
        if (! guard.IsCurrent())
        {
            return;
        }
        if (! model || groupCount == 0u)
        {
            constexpr std::span<const GridGroupDesc> noGroups;
            const float verticalExtent = GetVerticalScrollableExtent(noGroups);
            if (! guard.IsCurrent())
            {
                return;
            }
            _verticalScrollDip = ClampScroll(_verticalScrollDip, verticalExtent);
            if (normalizeVertical)
            {
                _verticalScrollDip = NormalizeVerticalScrollOffset(_verticalScrollDip, noGroups);
            }
            const float horizontalExtent = GetHorizontalScrollableExtent();
            if (! guard.IsCurrent())
            {
                return;
            }
            _horizontalScrollDip = ClampScroll(_horizontalScrollDip, horizontalExtent);
            return;
        }

        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return;
        }
        _verticalScrollDip = ClampScroll(_verticalScrollDip, GetVerticalScrollableExtent(groups));
        if (! guard.IsCurrent())
        {
            return;
        }
        if (normalizeVertical)
        {
            _verticalScrollDip = NormalizeVerticalScrollOffset(_verticalScrollDip, groups);
        }
        const float horizontalExtent = GetHorizontalScrollableExtent();
        if (! guard.IsCurrent())
        {
            return;
        }
        _horizontalScrollDip = ClampScroll(_horizontalScrollDip, horizontalExtent);
    }
    catch (const std::bad_alloc&)
    {
        // Keep the existing offsets when a model snapshot cannot be built.
    }
    catch (const std::exception&)
    {
        // A failing model getter leaves the last committed offsets intact.
    }
}

// Column width caching: _columnWidths is mutable and modified in const methods (EnsureColumnWidths).
// This is safe because DxUi follows a single-threaded rendering model — all ControlHost operations
// occur on the same UI thread. The mutable qualifier allows lazy initialization during const Paint() calls.
void Grid::EnsureColumnWidths() const
{
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const IGridModel* const model = _model;
        if (! model)
        {
            _columnWidths.clear();
            _columnDisplayOrder.clear();
            _columnDisplayIndexByModel.clear();
            return;
        }
        const size_t columnCount = model->GetColumnCount();
        if (! guard.IsCurrent())
        {
            return;
        }
        if (_columnWidths.size() == columnCount && _columnDisplayOrder.size() == columnCount && _columnDisplayIndexByModel.size() == columnCount)
        {
            return;
        }

        std::vector<float> columnWidths;
        columnWidths.reserve(columnCount);
        for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex)
        {
            const GridColumnDesc column = model->GetColumn(columnIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
            columnWidths.push_back(std::max(column.minWidthDip, column.widthDip));
        }

        if (! guard.IsCurrent())
        {
            return;
        }
        std::vector<size_t> columnDisplayOrder(columnCount);
        for (size_t displayIndex = 0; displayIndex < columnDisplayOrder.size(); ++displayIndex)
        {
            columnDisplayOrder[displayIndex] = displayIndex;
        }
        _columnWidths       = std::move(columnWidths);
        _columnDisplayOrder = std::move(columnDisplayOrder);
        _columnDisplayIndexByModel.clear();
        RebuildColumnDisplayIndexLookup();
    }
    catch (const std::bad_alloc&)
    {
        // Keep the last committed width and display-order cache.
    }
    catch (const std::exception&)
    {
        // A failing borrowed-model getter leaves the previous cache usable.
    }
}

bool Grid::SelectRow(size_t rowIndex, UINT modifiers)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model)
    {
        return true;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (rowIndex >= rowCount)
    {
        return true;
    }

    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const std::optional<uint64_t> previousFocus = _currentRowId;
    const uint64_t rowId                        = model->GetStableRowId(rowIndex);
    if (! guard.IsCurrent())
    {
        return false;
    }
    if (_selectionMode == GridSelectionMode::Single)
    {
        if (ModifiersContainCtrl(modifiers) && _selectionModel.IsSelected(rowId))
        {
            _selectionModel.Clear();
            _selectionModel.SetAnchor(rowId);
        }
        else
        {
            _selectionModel.SetSingle(rowId);
        }
        _currentRowId    = rowId;
        _focusedRowIndex = rowIndex;
    }
    else if (ModifiersContainShift(modifiers) && _selectionModel.GetAnchor())
    {
        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return false;
        }
        const std::vector<uint64_t> visibleRows = CollectVisibleOrderedRowIds(model, groups, &guard);
        if (! guard.IsCurrent())
        {
            return false;
        }
        _selectionModel.SetRange(visibleRows, _selectionModel.GetAnchor().value(), rowId);
        _currentRowId    = rowId;
        _focusedRowIndex = rowIndex;
    }
    else if (ModifiersContainCtrl(modifiers))
    {
        _selectionModel.Toggle(rowId);
        _currentRowId    = rowId;
        _focusedRowIndex = rowIndex;
    }
    else
    {
        _selectionModel.SetSingle(rowId);
        _currentRowId    = rowId;
        _focusedRowIndex = rowIndex;
    }

    if (! NotifySelectionAndFocusChanges(previousSelection, previousFocus))
    {
        return false;
    }
    // UI Automation event delivery can dispatch a message that destroys this grid too.
    RefreshAccessibilitySnapshot();
    return guard.IsBindingCurrent();
}

bool Grid::NotifySelectionAndFocusChanges(std::span<const uint64_t> previousSelection, std::optional<uint64_t> previousFocus)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (_delegate && ! EqualRowSelection(previousSelection, _selectionModel.GetOrderedSelection()))
    {
        if (! TryControlCallback([&] { _delegate->OnGridSelectionChanged(*this); }) || ! guard.IsBindingCurrent())
        {
            return false;
        }
    }
    if (_delegate && previousFocus != _currentRowId)
    {
        static_cast<void>(TryControlCallback([&] { _delegate->OnGridFocusedRowChanged(*this, _currentRowId); }));
    }
    return guard.IsBindingCurrent();
}

std::wstring Grid::BuildSelectionTsv() const
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model || _selectionModel.GetCount() == 0u)
    {
        return {};
    }

    std::wstring text;
    const std::vector<uint64_t> selection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
    if (! guard.IsCurrent())
    {
        return {};
    }
    std::vector<size_t> selectedRows;
    selectedRows.reserve(selection.size());
    for (const uint64_t selectedRowId : selection)
    {
        const auto rowIndex = model->FindRowByStableId(selectedRowId);
        if (! guard.IsCurrent())
        {
            return {};
        }
        if (! rowIndex || ! IsRowVisibleByGroupLayout(rowIndex.value(), groups))
        {
            continue;
        }
        selectedRows.push_back(*rowIndex);
    }
    // Group layout preserves model order. Gesture/UIA insertion order must not change clipboard row order.
    std::ranges::sort(selectedRows);
    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return {};
    }
    bool appendedRow = false;
    for (const size_t rowIndex : selectedRows)
    {
        if (appendedRow)
        {
            text.append(L"\r\n");
        }
        for (size_t displayIndex = 0; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
        {
            const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
            if (displayIndex > 0u)
            {
                text.push_back(L'\t');
            }
            GridCellData cellData{};
            model->GetCellData(rowIndex, columnIndex, cellData);
            if (! guard.IsCurrent())
            {
                return {};
            }
            const std::wstring field = BuildGridCellCopyText(cellData);
            const bool quote         = field.find_first_of(L"\"\t\r\n\v\f\x85\u2028\u2029") != std::wstring::npos;
            if (quote)
            {
                text.push_back(L'"');
                for (const wchar_t codeUnit : field)
                {
                    text.push_back(codeUnit);
                    if (codeUnit == L'"')
                    {
                        text.push_back(L'"');
                    }
                }
                text.push_back(L'"');
            }
            else
            {
                text.append(field);
            }
        }
        appendedRow = true;
    }
    return text;
}

std::optional<size_t> Grid::FindNearestVisibleRow(std::span<const GridGroupDesc> groups, size_t preferredRowIndex) const noexcept
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model)
    {
        return std::nullopt;
    }

    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
    {
        return std::nullopt;
    }
    const std::vector<size_t> visibleRows = CollectVisibleRowIndices(rowCount, groups);
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    IGridModel* const model           = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    if (! model)
    {
        _selectionModel.Clear();
        _currentRowId.reset();
        _focusedRowIndex.reset();
        return;
    }
    const bool hadSelection = _selectionModel.GetCount() > 0u;
    std::optional<size_t> focusedRowIndex;
    if (_currentRowId)
    {
        focusedRowIndex = model->FindRowByStableId(*_currentRowId);
        if (! guard.IsCurrent())
        {
            return;
        }
    }
    const std::vector<uint64_t> previousSelection(_selectionModel.GetOrderedSelection().begin(), _selectionModel.GetOrderedSelection().end());
    const std::optional<size_t> selectedRowIndex = previousSelection.empty() ? std::nullopt : model->FindRowByStableId(previousSelection.back());
    if (! guard.IsCurrent())
    {
        return;
    }
    const size_t preferredRowIndex = focusedRowIndex.value_or(_focusedRowIndex.value_or(selectedRowIndex.value_or(0u)));

    const std::vector<uint64_t> visibleRowIds = CollectVisibleOrderedRowIds(model, groups, &guard);
    if (! guard.IsCurrent())
    {
        return;
    }
    _selectionModel.PreserveOrdered(visibleRowIds);

    if (focusedRowIndex && IsRowVisibleByGroupLayout(*focusedRowIndex, groups))
    {
        _focusedRowIndex = focusedRowIndex;
    }
    else if (_currentRowId || _focusedRowIndex)
    {
        _currentRowId.reset();
        _focusedRowIndex.reset();
        const auto replacementFocus = FindNearestVisibleRow(groups, preferredRowIndex);
        if (! guard.IsCurrent())
        {
            return;
        }
        if (replacementFocus)
        {
            _currentRowId = model->GetStableRowId(*replacementFocus);
            if (! guard.IsCurrent())
            {
                return;
            }
            _focusedRowIndex = *replacementFocus;
        }
    }
    if (_selectionModel.GetCount() == 0u && hadSelection && ! visibleRowIds.empty())
    {
        const auto fallbackRowIndex = FindNearestVisibleRow(groups, preferredRowIndex);
        if (! guard.IsCurrent())
        {
            return;
        }
        if (fallbackRowIndex)
        {
            const uint64_t replacementId = model->GetStableRowId(*fallbackRowIndex);
            if (! guard.IsCurrent())
            {
                return;
            }
            _selectionModel.SetSingle(replacementId);
            _currentRowId    = replacementId;
            _focusedRowIndex = *fallbackRowIndex;
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    if (! model)
    {
        return 0.0f;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent())
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

    return (static_cast<float>(rowCount - collapsedRowCount) * _rowHeightDip) + (static_cast<float>(groups.size()) * _groupHeaderHeightDip);
}

float Grid::NormalizeVerticalScrollOffset(float offsetDip) const noexcept
{
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        if (! model)
        {
            return 0.0f;
        }
        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return 0.0f;
        }
        return NormalizeVerticalScrollOffset(offsetDip, groups);
    }
    catch (const std::bad_alloc&)
    {
        return 0.0f;
    }
    catch (const std::exception&)
    {
        return 0.0f;
    }
}

float Grid::GetRawVerticalScrollableExtent(std::span<const GridGroupDesc> groups) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    if (! _model)
    {
        return 0.0f;
    }

    const float bodyContentHeightDip = SanitizeNonNegative(GetBodyContentHeight(groups));
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    const float viewportHeightDip = std::max(0.0f, contentRect.bottom - contentRect.top);
    return std::max(0.0f, bodyContentHeightDip - viewportHeightDip);
}

float Grid::AlignVerticalScrollExtentToVisibleItemBoundary(float rawExtentDip, std::span<const GridGroupDesc> groups) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    if (! _model || rawExtentDip <= 0.0f)
    {
        return 0.0f;
    }

    const size_t rowCount = _model->GetRowCount();
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const D2D1_RECT_F bodyRect = GetContentRect(groups);
    if (! guard.IsCurrent())
    {
        return {};
    }
    return BuildVisibleBodyItems(groups, bodyRect);
}

std::vector<Grid::VisibleBodyItem> Grid::BuildVisibleBodyItems(std::span<const GridGroupDesc> groups, const D2D1_RECT_F& bodyRect) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const IGridModel* const model = _model;
    std::vector<VisibleBodyItem> visibleItems;
    if (! model)
    {
        return visibleItems;
    }
    const size_t rowCount = model->GetRowCount();
    if (! guard.IsCurrent() || rowCount == 0u)
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
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const std::vector<GridGroupDesc> groups = CollectOrderedGroups(_model, &guard);
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    return GetVerticalScrollableExtent(groups);
}

float Grid::GetVerticalScrollableExtent(std::span<const GridGroupDesc> groups) const
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const float rawExtent = GetRawVerticalScrollableExtent(groups);
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    const float alignedExtent = AlignVerticalScrollExtentToVisibleItemBoundary(rawExtent, groups);
    return guard.IsCurrent() ? alignedExtent : 0.0f;
}

float Grid::GetHorizontalScrollableExtent() const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    if (! _model)
    {
        return 0.0f;
    }
    EnsureColumnWidths();
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    float totalWidth = 0.0f;
    for (const float width : _columnWidths)
    {
        totalWidth += SanitizeNonNegative(width);
    }
    const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
    if (! guard.IsCurrent())
    {
        return 0.0f;
    }
    const float viewportWidthDip = std::max(0.0f, contentRect.right - contentRect.left);
    return std::max(0.0f, totalWidth - viewportWidthDip);
}

D2D1_RECT_F Grid::GetContentRect() const noexcept
{
    try
    {
        const std::weak_ptr<int> lifetime = GetLifetimeToken();
        const IGridModel* const model     = _model;
        const GridModelQueryGuard guard(*this, lifetime, model);
        if (! model)
        {
            return GetContentRect(std::span<const GridGroupDesc>{});
        }

        const std::vector<GridGroupDesc> groups = CollectOrderedGroups(model, &guard);
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        return GetContentRect(groups);
    }
    catch (const std::bad_alloc&)
    {
        // Geometry is unavailable when group snapshots cannot be allocated.
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        // A failing borrowed-model group query produces empty geometry at this noexcept boundary.
        return D2D1_RECT_F{};
    }
}

D2D1_RECT_F Grid::GetContentRect(std::span<const GridGroupDesc> groups) const noexcept
{
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const D2D1_RECT_F bounds    = NormalizeFiniteRect(GetBounds());
        const float headerHeightDip = (std::isfinite(_headerHeightDip) && _headerHeightDip > 0.0f) ? _headerHeightDip : 0.0f;

        if (! _model)
        {
            return NormalizeFiniteRect(D2D1::RectF(bounds.left, bounds.top + headerHeightDip, bounds.right, bounds.bottom));
        }

        EnsureColumnWidths();
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        float totalWidth = 0.0f;
        for (const float width : _columnWidths)
        {
            totalWidth += SanitizeNonNegative(width);
        }

        const float bodyContentHeightDip = SanitizeNonNegative(GetBodyContentHeight(groups));
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        bool needVScroll = false;
        bool needHScroll = false;
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

        const float contentLeft  = bounds.left + (IsRightToLeft() && needVScroll ? kScrollbarThicknessDip : 0.0f);
        const float contentRight = bounds.right - (! IsRightToLeft() && needVScroll ? kScrollbarThicknessDip : 0.0f);
        return NormalizeFiniteRect(
            D2D1::RectF(contentLeft, bounds.top + headerHeightDip, contentRight, bounds.bottom - (needHScroll ? kScrollbarThicknessDip : 0.0f)));
    }
    catch (const std::bad_alloc&)
    {
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        return D2D1_RECT_F{};
    }
}

D2D1_RECT_F Grid::GetVerticalScrollbarRect() const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const D2D1_RECT_F content = NormalizeFiniteRect(GetContentRect());
    if (! guard.IsCurrent())
    {
        return D2D1_RECT_F{};
    }
    const D2D1_RECT_F bounds = NormalizeFiniteRect(GetBounds());
    return IsRightToLeft() ? NormalizeFiniteRect(D2D1::RectF(bounds.left, content.top, content.left, content.bottom))
                           : NormalizeFiniteRect(D2D1::RectF(content.right, content.top, bounds.right, content.bottom));
}

D2D1_RECT_F Grid::GetHorizontalScrollbarRect() const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    const D2D1_RECT_F content = NormalizeFiniteRect(GetContentRect());
    if (! guard.IsCurrent())
    {
        return D2D1_RECT_F{};
    }
    const D2D1_RECT_F bounds = NormalizeFiniteRect(GetBounds());
    return NormalizeFiniteRect(D2D1::RectF(content.left, content.bottom, content.right, bounds.bottom));
}

Grid::VisibleColumnSpan Grid::ComputeVisibleColumnSpan(float clipRightDip) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    VisibleColumnSpan span{};
    if (_columnDisplayOrder.empty() || clipRightDip <= GetBounds().left)
    {
        return span;
    }

    if (IsRightToLeft())
    {
        const D2D1_RECT_F body = GetContentRect();
        if (! guard.IsCurrent())
        {
            return {};
        }
        bool foundVisible = false;
        for (size_t displayIndex = 0u; displayIndex < _columnDisplayOrder.size(); ++displayIndex)
        {
            const size_t columnIndex = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float left         = GetColumnLeftDip(columnIndex);
            if (! guard.IsCurrent())
            {
                return {};
            }
            const float right = left + _columnWidths[columnIndex];
            if (right <= body.left || left >= clipRightDip)
            {
                continue;
            }
            if (! foundVisible)
            {
                span.beginIndex = displayIndex;
                span.endIndex   = displayIndex + 1u;
                foundVisible    = true;
            }
            else
            {
                span.beginIndex = (std::min)(span.beginIndex, displayIndex);
                span.endIndex   = (std::max)(span.endIndex, displayIndex + 1u);
            }
        }
        span.beginXDip = foundVisible ? GetColumnLeftDip(GetModelColumnIndexForDisplayIndex(span.beginIndex)) : body.right;
        if (! guard.IsCurrent())
        {
            return {};
        }
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
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const D2D1_RECT_F track = NormalizeFiniteRect(GetVerticalScrollbarRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float extent = SanitizeNonNegative(GetVerticalScrollableExtent());
        if (! guard.IsCurrent() || extent <= 0.0f)
        {
            return D2D1::RectF();
        }

        const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float viewportDip = std::max(1.0f, contentRect.bottom - contentRect.top);
        return ComputeScrollbarThumbRect(track, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent, _verticalScrollDip, extent);
    }
    catch (const std::bad_alloc&)
    {
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        return D2D1_RECT_F{};
    }
}

D2D1_RECT_F Grid::GetHorizontalThumbRect() const noexcept
{
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const D2D1_RECT_F track = NormalizeFiniteRect(GetHorizontalScrollbarRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float extent = SanitizeNonNegative(GetHorizontalScrollableExtent());
        if (! guard.IsCurrent() || extent <= 0.0f)
        {
            return D2D1::RectF();
        }

        const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float viewportDip = std::max(1.0f, contentRect.right - contentRect.left);
        const float thumbValue  = IsRightToLeft() ? extent - _horizontalScrollDip : _horizontalScrollDip;
        return ComputeScrollbarThumbRect(track, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent, thumbValue, extent);
    }
    catch (const std::bad_alloc&)
    {
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        return D2D1_RECT_F{};
    }
}

D2D1_RECT_F Grid::GetVerticalThumbHitRect() const noexcept
{
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const D2D1_RECT_F track = NormalizeFiniteRect(GetVerticalScrollbarRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float extent = SanitizeNonNegative(GetVerticalScrollableExtent());
        if (! guard.IsCurrent() || extent <= 0.0f)
        {
            return D2D1::RectF();
        }

        const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float viewportDip = std::max(1.0f, contentRect.bottom - contentRect.top);
        return ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent, _verticalScrollDip, extent);
    }
    catch (const std::bad_alloc&)
    {
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        return D2D1_RECT_F{};
    }
}

D2D1_RECT_F Grid::GetHorizontalThumbHitRect() const noexcept
{
    try
    {
        const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
        const D2D1_RECT_F track = NormalizeFiniteRect(GetHorizontalScrollbarRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float extent = SanitizeNonNegative(GetHorizontalScrollableExtent());
        if (! guard.IsCurrent() || extent <= 0.0f)
        {
            return D2D1::RectF();
        }

        const D2D1_RECT_F contentRect = NormalizeFiniteRect(GetContentRect());
        if (! guard.IsCurrent())
        {
            return D2D1_RECT_F{};
        }
        const float viewportDip = std::max(1.0f, contentRect.right - contentRect.left);
        const float thumbValue  = IsRightToLeft() ? extent - _horizontalScrollDip : _horizontalScrollDip;
        return ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Horizontal, viewportDip, viewportDip + extent, thumbValue, extent);
    }
    catch (const std::bad_alloc&)
    {
        return D2D1_RECT_F{};
    }
    catch (const std::exception&)
    {
        return D2D1_RECT_F{};
    }
}

float Grid::GetColumnLeftDip(size_t columnIndex) const noexcept
{
    const GridModelQueryGuard guard(*this, GetLifetimeToken(), _model);
    if (columnIndex >= _columnDisplayIndexByModel.size())
    {
        return GetBounds().left - _horizontalScrollDip;
    }

    const size_t displayIndex = _columnDisplayIndexByModel[columnIndex];
    if (IsRightToLeft())
    {
        const D2D1_RECT_F contentRect = GetContentRect();
        if (! guard.IsCurrent())
        {
            return 0.0f;
        }
        float right = contentRect.right + _horizontalScrollDip;
        for (size_t currentDisplayIndex = 0u; currentDisplayIndex <= displayIndex && currentDisplayIndex < _columnDisplayOrder.size(); ++currentDisplayIndex)
        {
            right -= _columnWidths[_columnDisplayOrder[currentDisplayIndex]];
        }
        return right;
    }

    float left = GetBounds().left - _horizontalScrollDip;
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

    if (IsRightToLeft())
    {
        for (size_t reverseIndex = _columnDisplayOrder.size(); reverseIndex > 0u; --reverseIndex)
        {
            const size_t displayIndex = reverseIndex - 1u;
            const size_t columnIndex  = GetModelColumnIndexForDisplayIndex(displayIndex);
            const float left          = GetColumnLeftDip(columnIndex);
            const float midpoint      = left + (_columnWidths[columnIndex] * 0.5f);
            if (xDip < midpoint)
            {
                return displayIndex + 1u;
            }
        }
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
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount)
    {
        return std::nullopt;
    }

    auto tryColumn = [&](size_t columnIndex) -> bool
    {
        if (! guard.IsCurrent())
        {
            return false;
        }
        size_t columnCount = 0u;
        if (! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent() || columnIndex >= columnCount)
        {
            return false;
        }
        GridCellData cellData{};
        if (! TryControlCallback([&] { model->GetCellData(rowIndex, columnIndex, cellData); }) || ! guard.IsCurrent())
        {
            return false;
        }
        return cellData.kind == GridCellKind::Checkbox;
    };

    if (_activeColumn && tryColumn(_activeColumn.value()))
    {
        return _activeColumn;
    }
    if (! guard.IsCurrent())
    {
        return std::nullopt;
    }

    size_t columnCount = 0u;
    if (! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent())
    {
        return std::nullopt;
    }
    for (size_t columnIndex = 0; columnIndex < columnCount; ++columnIndex)
    {
        if (tryColumn(columnIndex))
        {
            return columnIndex;
        }
        if (! guard.IsCurrent())
        {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

bool Grid::ToggleCheckboxCell(ControlHost& host, size_t rowIndex, size_t columnIndex)
{
    const std::weak_ptr<int> lifetime = GetLifetimeToken();
    const IGridModel* const model     = _model;
    const GridModelQueryGuard guard(*this, lifetime, model);
    size_t rowCount    = 0u;
    size_t columnCount = 0u;
    if (! model || ! TryControlCallback([&] { rowCount = model->GetRowCount(); }) || ! guard.IsCurrent() || rowIndex >= rowCount ||
        ! TryControlCallback([&] { columnCount = model->GetColumnCount(); }) || ! guard.IsCurrent() || columnIndex >= columnCount)
    {
        return false;
    }

    GridCellData cellData{};
    if (! TryControlCallback([&] { model->GetCellData(rowIndex, columnIndex, cellData); }) || ! guard.IsCurrent())
    {
        return false;
    }
    if (cellData.kind != GridCellKind::Checkbox || ! cellData.enabled)
    {
        return false;
    }

    _activeColumn = columnIndex;
    if (_delegate)
    {
        if (! TryControlCallback([&] { _delegate->OnGridCheckboxToggled(*this, rowIndex, columnIndex, ! cellData.checked); }))
        {
            return false;
        }
        if (! guard.IsCurrent())
        {
            return true;
        }
        // A model change that moved the selection runs the selection's delegate, which may rebuild the controls and destroy
        // this grid, and so may UI Automation event delivery while it publishes.
        NotifyDataChanged();
        if (! guard.IsCurrent())
        {
            return true;
        }
        if (_model != model)
        {
            return true;
        }
    }
    if (guard.IsCurrent())
    {
        Invalidate(host);
    }
    return true;
}

} // namespace DxUi
