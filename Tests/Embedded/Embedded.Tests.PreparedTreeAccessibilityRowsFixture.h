#pragma once

#include "Embedded.Tests.PreparedTreeAccessibilityFixture.h"

#include <thread>

#if __has_include(<DxUi/PreparedTreeAccessibility.h>)
#include <DxUi/PreparedTreeAccessibility.h>
#define DXUI_TEST_HAS_PREPARED_TREE_ROWS 1
#else
#define DXUI_TEST_HAS_PREPARED_TREE_ROWS 0
#endif

namespace DxUi::Tests
{
#if DXUI_TEST_HAS_PREPARED_TREE_ROWS
class LargePreparedTreeRows final : public IPreparedTreeAccessibilityRows
{
public:
    explicit LargePreparedTreeRows(const CountingLargeTreeModel& input)
    {
        _names.reserve(CountingLargeTreeModel::RowCount);
        for (size_t index = 0; index < CountingLargeTreeModel::RowCount; ++index)
            _names.emplace_back(input.NameAt(index));
    }
    [[nodiscard]] size_t GetCount() const noexcept override
    {
        return _names.size();
    }
    [[nodiscard]] std::optional<TreeAccessibilityItemView> GetItem(size_t index) const noexcept override
    {
        if (index >= _names.size())
            return std::nullopt;
        return TreeAccessibilityItemView{index, 100000 + 3 * index, _names[index], 0, false, false};
    }
    [[nodiscard]] std::optional<TreeAccessibilityItemView> FindItem(uint64_t id) const noexcept override
    {
        if (id < 100000 || (id - 100000) % 3 != 0 || (id - 100000) / 3 >= _names.size())
            return std::nullopt;
        return GetItem(static_cast<size_t>((id - 100000) / 3));
    }

private:
    std::vector<std::wstring> _names;
};
#endif

class PreparedCountingLargeTreeModel final : public ITreeModel
{
public:
    CountingLargeTreeModel model;
    explicit PreparedCountingLargeTreeModel(bool prepared)
    {
#if DXUI_TEST_HAS_PREPARED_TREE_ROWS
        if (prepared)
        {
            // Admission/model construction precede the measurement. The immutable source itself is built
            // off owner; the constructor joins before the source is exposed to any control/provider.
            std::thread worker([&]() { _prepared = std::make_shared<LargePreparedTreeRows>(model); });
            worker.join();
        }
#else
        (void)prepared;
#endif
    }
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return model.GetVisibleItemCount();
    }
    void GetVisibleItem(size_t index, TreeItemData& item) const override
    {
        model.GetVisibleItem(index, item);
    }
    [[nodiscard]] std::optional<size_t> FindVisibleItemById(uint64_t id) const noexcept override
    {
        return model.FindVisibleItemById(id);
    }
#if DXUI_TEST_HAS_PREPARED_TREE_ROWS
    [[nodiscard]] std::shared_ptr<const IPreparedTreeAccessibilityRows> CapturePreparedAccessibilityRows() const noexcept override
    {
        return _prepared;
    }

private:
    std::shared_ptr<const IPreparedTreeAccessibilityRows> _prepared;
#endif
};
} // namespace DxUi::Tests
