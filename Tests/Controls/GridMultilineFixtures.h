#pragma once

#include "DxUiTestHelpers.h"
#include "DxUiTestMovedControls.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Fixtures shared by the suites that verify the Grid's bounded multiline cells (plan GridTextOverflow_2026-09-21): a model of
// arbitrary text cells, text builders and the ink measurements the pixel tests compare. Nothing here asserts a behavior of
// its own.
namespace GridMultilineFixtures
{
using DxUi::WindowHostBitmapCapture;
using DxUiTestMoved::CaptureWindow;
using DxUiTestMoved::ConfigureHostPlace;

// A rectangle of text cells, each its own value; a row's id is its index. Every cell is multiline unless told otherwise,
// and a column may be narrower than the grid's usual minimum.
class TextTableModel final : public DxUi::IGridModel
{
public:
    TextTableModel(std::vector<std::vector<std::wstring>> cells, std::vector<float> columnWidthsDip, bool multiline = true)
        : _cells(std::move(cells)),
          _columnWidthsDip(std::move(columnWidthsDip)),
          _multiline(multiline)
    {
    }

    void SetText(size_t rowIndex, size_t columnIndex, std::wstring text)
    {
        _cells.at(rowIndex).at(columnIndex) = std::move(text);
    }

    [[nodiscard]] const std::wstring& GetText(size_t rowIndex, size_t columnIndex) const
    {
        return _cells.at(rowIndex).at(columnIndex);
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _cells.size();
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnWidthsDip.size();
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        DxUi::GridColumnDesc column;
        column.id          = L"c" + std::to_wstring(columnIndex);
        column.title       = L"Colonne " + std::to_wstring(columnIndex);
        column.widthDip    = _columnWidthsDip.at(columnIndex);
        column.minWidthDip = (std::min)(column.minWidthDip, column.widthDip);
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        outCell.kind      = DxUi::GridCellKind::Text;
        outCell.text      = _cells.at(rowIndex).at(columnIndex);
        outCell.multiline = _multiline;
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return rowId < _cells.size() ? std::optional<size_t>(static_cast<size_t>(rowId)) : std::nullopt;
    }

private:
    std::vector<std::vector<std::wstring>> _cells;
    std::vector<float> _columnWidthsDip;
    bool _multiline = true;
};

// `unit` repeated until the text holds at least `units` UTF-16 units (a whole number of units of `unit`).
[[nodiscard]] inline std::wstring RepeatToUnits(std::wstring_view unit, size_t units)
{
    std::wstring text;
    text.reserve(units + unit.size());
    while (text.size() < units)
        text.append(unit);
    return text;
}

// A device-independent rectangle in the physical pixels of the host, rounded to the nearest pixel.
[[nodiscard]] inline RECT PixelRect(const DxUi::ControlHost& host, const D2D1_RECT_F& rectDip) noexcept
{
    return RECT{static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.left))),
                static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.top))),
                static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.right))),
                static_cast<LONG>(std::lround(host.DipsToPixels(rectDip.bottom)))};
}

// One pixel, packed as blue | green << 8 | red << 16 | alpha << 24; 0 outside the capture.
[[nodiscard]] inline uint32_t PixelAt(const WindowHostBitmapCapture& capture, LONG x, LONG y) noexcept
{
    if (x < 0 || y < 0 || static_cast<UINT>(x) >= capture.widthPx || static_cast<UINT>(y) >= capture.heightPx)
        return 0u;
    const size_t offset = ((static_cast<size_t>(y) * capture.widthPx) + static_cast<size_t>(x)) * 4u;
    uint32_t pixel      = 0u;
    std::memcpy(&pixel, &capture.bgraPixels[offset], sizeof(pixel));
    return pixel;
}

// The box (inclusive) and the number of the pixels that differ between two captures inside a region (physical pixels, right
// and bottom exclusive).
struct Difference
{
    LONG left       = LONG_MAX;
    LONG top        = LONG_MAX;
    LONG right      = LONG_MIN;
    LONG bottom     = LONG_MIN;
    uint64_t pixels = 0u;
};

[[nodiscard]] inline Difference MeasureDifference(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected, const RECT& region) noexcept
{
    Difference difference;
    for (LONG y = region.top; y < region.bottom; ++y)
    {
        for (LONG x = region.left; x < region.right; ++x)
        {
            if (PixelAt(actual, x, y) == PixelAt(expected, x, y))
                continue;
            ++difference.pixels;
            difference.left   = (std::min)(difference.left, x);
            difference.top    = (std::min)(difference.top, y);
            difference.right  = (std::max)(difference.right, x);
            difference.bottom = (std::max)(difference.bottom, y);
        }
    }
    return difference;
}

[[nodiscard]] inline Difference MeasureDifference(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected) noexcept
{
    return MeasureDifference(actual, expected, RECT{0, 0, static_cast<LONG>(actual.widthPx), static_cast<LONG>(actual.heightPx)});
}

// Names a difference for a failure message.
[[nodiscard]] inline std::string Describe(const Difference& difference)
{
    if (difference.pixels == 0u)
        return "identical";
    return std::format("{} pixels differ inside ({},{})-({},{})", difference.pixels, difference.left, difference.top, difference.right, difference.bottom);
}

// Fails naming what differs, so a failed pixel comparison says where and how much, not only that it failed.
inline void RequireIdentical(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected, const RECT& region, const std::string& what)
{
    const Difference difference = MeasureDifference(actual, expected, region);
    if (difference.pixels != 0u)
        std::cerr << "    [DIFFERENCE] " << what << ": " << Describe(difference) << '\n';
    Require(difference.pixels == 0u, what.c_str());
}

inline void RequireIdentical(const WindowHostBitmapCapture& actual, const WindowHostBitmapCapture& expected, const std::string& what)
{
    RequireIdentical(actual, expected, RECT{0, 0, static_cast<LONG>(actual.widthPx), static_cast<LONG>(actual.heightPx)}, what);
}
} // namespace GridMultilineFixtures
