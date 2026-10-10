#pragma once

#include <DxUi/DxUi.h>

#include <array>
#include <atomic>
#include <cassert>
#include <memory>
#include <string>

namespace DxUi::Tests
{
struct PreparedTreeSourceWitness final
{
    std::atomic<size_t> destructions{0u};
};

class ThreePreparedTreeRows final : public IPreparedTreeAccessibilityRows
{
public:
    using Names = std::array<std::wstring, 3u>;
    static constexpr std::array<uint64_t, 3u> Ids{101u, 709u, 9001u};

    ThreePreparedTreeRows(Names names, std::shared_ptr<PreparedTreeSourceWitness> witness) : _names(std::move(names)), _witness(std::move(witness))
    {
    }
    ~ThreePreparedTreeRows() override
    {
        ++_witness->destructions;
    }
    [[nodiscard]] size_t GetCount() const noexcept override
    {
        return Ids.size();
    }
    [[nodiscard]] std::optional<TreeAccessibilityItemView> GetItem(size_t visibleIndex) const noexcept override
    {
        if (visibleIndex >= Ids.size())
            return std::nullopt;
        return TreeAccessibilityItemView{visibleIndex, Ids[visibleIndex], _names[visibleIndex], 0u, false, false};
    }
    [[nodiscard]] std::optional<TreeAccessibilityItemView> FindItem(uint64_t itemId) const noexcept override
    {
        for (size_t index = 0u; index < Ids.size(); ++index)
            if (Ids[index] == itemId)
                return GetItem(index);
        return std::nullopt;
    }

private:
    const Names _names;
    const std::shared_ptr<PreparedTreeSourceWitness> _witness;
};

// Positive fixture inputs are the concrete complete three-row source above, never a partially prepared model.
class ThreePreparedTreeModel final : public ITreeModel
{
public:
    explicit ThreePreparedTreeModel(std::shared_ptr<const ThreePreparedTreeRows> source)
    {
        Replace(std::move(source));
    }
    void Replace(std::shared_ptr<const ThreePreparedTreeRows> source)
    {
        assert(source && source->GetCount() == _items.size());
        for (size_t index = 0u; index < _items.size(); ++index)
        {
            const auto row = source->GetItem(index).value();
            assert(row.visibleIndex == index && row.itemId == ThreePreparedTreeRows::Ids[index]);
            _items[index] = TreeItemData{.id          = row.itemId,
                                         .text        = std::wstring(row.text),
                                         .depth       = static_cast<uint32_t>(row.depth),
                                         .hasChildren = row.hasChildren,
                                         .expanded    = row.expanded};
        }
        _source = std::move(source);
    }
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return _items.size();
    }
    void GetVisibleItem(size_t visibleIndex, TreeItemData& outItem) const override
    {
        outItem = _items.at(visibleIndex);
    }
    [[nodiscard]] std::optional<size_t> FindVisibleItemById(uint64_t itemId) const noexcept override
    {
        for (size_t index = 0u; index < _items.size(); ++index)
            if (_items[index].id == itemId)
                return index;
        return std::nullopt;
    }
    [[nodiscard]] std::shared_ptr<const IPreparedTreeAccessibilityRows> CapturePreparedAccessibilityRows() const noexcept override
    {
        return _source;
    }

private:
    std::array<TreeItemData, 3u> _items{};
    std::shared_ptr<const ThreePreparedTreeRows> _source;
};
} // namespace DxUi::Tests
