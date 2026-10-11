#pragma once

#include <DxUi/DxUi.h>

#include <string>
#include <vector>

namespace DxUi::Tests
{
// Deterministic 10,000-row tree input for consumer-side preparation measurements.
// Expected anchors are literals, independent of the model's ID/name generation:
// first: index 0, id 100000, suffix #00000; middle: index 5000, id 115000,
// suffix #05000; last: index 9999, id 129997, suffix #09999.
class CountingLargeTreeModel final : public ITreeModel
{
public:
    static constexpr size_t RowCount    = 10000;
    static constexpr size_t FirstIndex  = 0;
    static constexpr size_t MiddleIndex = 5000;
    static constexpr size_t LastIndex   = 9999;
    static constexpr uint64_t FirstId   = 100000;
    static constexpr uint64_t MiddleId  = 115000;
    static constexpr uint64_t LastId    = 129997;

    CountingLargeTreeModel()
    {
        _names.reserve(RowCount);
        for (size_t index = 0; index < RowCount; ++index)
        {
            std::wstring name = L"Prepared tree row ";
            name.append(1024 - name.size() - 6, L'x');
            name.push_back(L'#');
            std::wstring suffix = std::to_wstring(index);
            name.append(5 - suffix.size(), L'0');
            name.append(suffix);
            _names.push_back(std::move(name));
        }
    }

    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        ++visibleItemCountCalls;
        return RowCount;
    }

    void GetVisibleItem(size_t visibleIndex, TreeItemData& outItem) const override
    {
        ++visibleItemCalls;
        outItem = {};
        if (visibleIndex >= RowCount)
        {
            return;
        }

        outItem.id   = FirstId + 3 * visibleIndex;
        outItem.text = _names[visibleIndex];
    }

    [[nodiscard]] std::optional<size_t> FindVisibleItemById(uint64_t itemId) const noexcept override
    {
        ++findVisibleItemByIdCalls;
        if (itemId < FirstId)
        {
            return std::nullopt;
        }

        const uint64_t difference = itemId - FirstId;
        if (difference % 3 != 0)
        {
            return std::nullopt;
        }

        const uint64_t index = difference / 3;
        if (index >= RowCount)
        {
            return std::nullopt;
        }
        return static_cast<size_t>(index);
    }

    // Counters are deliberately owner-thread-only; reset immediately before each measured phase.
    void ResetCallCounts() const noexcept
    {
        visibleItemCountCalls    = 0;
        visibleItemCalls         = 0;
        findVisibleItemByIdCalls = 0;
    }

    // Immutable construction data only; a preparation worker borrows this while the owning model is held.
    [[nodiscard]] std::wstring_view NameAt(size_t index) const noexcept
    {
        return index < RowCount ? std::wstring_view(_names[index]) : std::wstring_view{};
    }

    mutable size_t visibleItemCountCalls    = 0;
    mutable size_t visibleItemCalls         = 0;
    mutable size_t findVisibleItemByIdCalls = 0;

private:
    std::vector<std::wstring> _names;
};
} // namespace DxUi::Tests
