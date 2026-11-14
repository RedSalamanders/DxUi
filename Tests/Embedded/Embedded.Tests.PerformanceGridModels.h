#pragma once
// Grid models shared by the Controls suites and the FX3 performance fixture. Host-neutral: DxUi and the standard library.
#include <DxUi/DxUi.h>

#include <format>
#include <optional>
#include <string>

// The allocations these models make themselves (formatting their text), counted only while a test sets the probe to read its
// allocation counter: a gate subtracts them to keep what DxUi allocates (Embedded.Tests.PerformanceGridDataPaths.h). Dormant
// otherwise, so receipts and every other count are unchanged.
inline thread_local size_t (*g_gridModelAllocationProbe)() noexcept = nullptr;
inline thread_local size_t g_gridModelAllocations                   = 0u;

class GridModelAllocationScope final
{
public:
    GridModelAllocationScope() noexcept : _start(g_gridModelAllocationProbe ? g_gridModelAllocationProbe() : 0u)
    {
    }
    ~GridModelAllocationScope()
    {
        if (g_gridModelAllocationProbe)
            g_gridModelAllocations += g_gridModelAllocationProbe() - _start;
    }
    GridModelAllocationScope(const GridModelAllocationScope&)            = delete;
    GridModelAllocationScope& operator=(const GridModelAllocationScope&) = delete;

private:
    size_t _start = 0u;
};

// Models are owned in place and never copied. Owner: DxUi maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
// Rows "R<row>C<column>" in uniform columns; row ids are row indices.
class LargeGridModel final : public DxUi::IGridModel
{
public:
    LargeGridModel(size_t rowCount, size_t columnCount, float columnWidthDip) : _rowCount(rowCount), _columnCount(columnCount), _columnWidthDip(columnWidthDip)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rowCount;
    }

    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _columnCount;
    }

    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        const GridModelAllocationScope counted;
        DxUi::GridColumnDesc column;
        column.id       = std::format(L"col-{}", columnIndex);
        column.title    = std::format(L"Column {}", columnIndex);
        column.widthDip = _columnWidthDip;
        return column;
    }

    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        const GridModelAllocationScope counted;
        outCell.kind = DxUi::GridCellKind::Text;
        outCell.text = std::format(L"R{}C{}", rowIndex, columnIndex);
    }

    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        if (rowId >= _rowCount)
        {
            return std::nullopt;
        }

        return static_cast<size_t>(rowId);
    }

private:
    size_t _rowCount      = 0u;
    size_t _columnCount   = 0u;
    float _columnWidthDip = 120.0f;
};

// A LargeGridModel whose rows fall into equal consecutive groups ("Group <n>"), all expanded. Groups are computed on demand,
// so the model holds no per-group storage.
class UniformGroupedGridModel final : public DxUi::IGridModel
{
public:
    UniformGroupedGridModel(size_t rowCount, size_t groupCount, size_t columnCount, float columnWidthDip)
        : _rows(rowCount, columnCount, columnWidthDip),
          _groupCount(groupCount)
    {
    }

    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return _rows.GetRowCount();
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return _rows.GetColumnCount();
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t columnIndex) const override
    {
        return _rows.GetColumn(columnIndex);
    }
    void GetCellData(size_t rowIndex, size_t columnIndex, DxUi::GridCellData& outCell) const override
    {
        _rows.GetCellData(rowIndex, columnIndex, outCell);
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t rowId) const noexcept override
    {
        return _rows.FindRowByStableId(rowId);
    }
    [[nodiscard]] size_t GetGroupCount() const noexcept override
    {
        return _groupCount;
    }
    [[nodiscard]] DxUi::GridGroupDesc GetGroup(size_t groupIndex) const override
    {
        const GridModelAllocationScope counted;
        const size_t rowsPerGroup = _rows.GetRowCount() / _groupCount;
        const size_t start        = groupIndex * rowsPerGroup;
        const size_t count        = groupIndex + 1u == _groupCount ? _rows.GetRowCount() - start : rowsPerGroup;
        return DxUi::GridGroupDesc{.stableId = groupIndex, .title = std::format(L"Group {}", groupIndex), .startRowIndex = start, .rowCount = count};
    }

private:
    LargeGridModel _rows;
    size_t _groupCount = 1u;
};
#pragma warning(pop)
