// Reviewed consumer compile fixture for API revision 4. Prepared Tree/interaction additions were reviewed with the P1 branch.
#include <DxUi/DxUi.h>
#include <DxUi/Embedded.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>

using OptionalRow = std::optional<uint64_t>;

static_assert(std::is_same_v<decltype(&DxUi::ITreeModel::CapturePreparedAccessibilityRows),
    std::shared_ptr<const DxUi::IPreparedTreeAccessibilityRows> (DxUi::ITreeModel::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::EmbeddedHost::PrepareInteraction),
    HRESULT (DxUi::EmbeddedHost::*)(UINT, UINT, float) noexcept>);

static_assert(std::is_same_v<decltype(&DxUi::Panel::TakeChild),
    std::unique_ptr<DxUi::Control> (DxUi::Panel::*)(size_t) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Panel::CompactChildren), size_t (DxUi::Panel::*)() noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::ControlHost::GetAnimationTickMs), uint64_t (DxUi::ControlHost::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::TabControl::TakeTab),
    std::unique_ptr<DxUi::Control> (DxUi::TabControl::*)(size_t) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::TabControl::TakeChild),
    std::unique_ptr<DxUi::Control> (DxUi::TabControl::*)(size_t) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::TabControl::ClearChildren), void (DxUi::TabControl::*)() noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::IGridDelegate::OnGridFocusedRowChanged),
    void (DxUi::IGridDelegate::*)(DxUi::Grid&, OptionalRow)>);
static_assert(std::is_same_v<decltype(&DxUi::Grid::SetFocusedRowId), void (DxUi::Grid::*)(OptionalRow) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Grid::GetFocusedRowId), OptionalRow (DxUi::Grid::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Grid::RequestFocusRow), bool (DxUi::Grid::*)(size_t)>);
static_assert(std::is_same_v<decltype(&DxUi::GridSelectionModel::SetAnchor),
    void (DxUi::GridSelectionModel::*)(OptionalRow) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::ITreeDelegate::OnTreeFocusedItemChanged),
    void (DxUi::ITreeDelegate::*)(DxUi::Tree&, OptionalRow)>);
static_assert(std::is_same_v<decltype(&DxUi::Tree::SetFocusedItemId), void (DxUi::Tree::*)(OptionalRow) noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Tree::GetFocusedItemId), OptionalRow (DxUi::Tree::*)() const noexcept>);
static_assert(std::is_same_v<decltype(&DxUi::Tree::RequestFocusVisibleItem), bool (DxUi::Tree::*)(size_t)>);

// A consumer's models and delegates as revision 4 lets one write them. A later pure virtual, or a changed signature of
// a virtual a consumer overrides, makes one of these abstract or its override ill-formed, and this fixture stops
// compiling.
class ConsumerGridModel final : public DxUi::IGridModel
{
public:
    [[nodiscard]] size_t GetRowCount() const noexcept override
    {
        return 0u;
    }
    [[nodiscard]] size_t GetColumnCount() const noexcept override
    {
        return 0u;
    }
    [[nodiscard]] DxUi::GridColumnDesc GetColumn(size_t /*columnIndex*/) const override
    {
        return {};
    }
    void GetCellData(size_t /*rowIndex*/, size_t /*columnIndex*/, DxUi::GridCellData& /*outCell*/) const override
    {
    }
    [[nodiscard]] std::optional<size_t> FindRowByStableId(uint64_t /*rowId*/) const noexcept override
    {
        return std::nullopt;
    }
};

class ConsumerGridDelegate final : public DxUi::IGridDelegate
{
public:
    using DxUi::IGridDelegate::OnGridSelectionChanged;
    void OnGridSelectionChanged(DxUi::Grid& /*sender*/) override
    {
    }
    void OnGridFocusedRowChanged(DxUi::Grid& /*sender*/, OptionalRow /*rowId*/) override
    {
    }
};

class ConsumerTreeModel final : public DxUi::ITreeModel
{
public:
    [[nodiscard]] size_t GetVisibleItemCount() const noexcept override
    {
        return 0u;
    }
    void GetVisibleItem(size_t /*visibleIndex*/, DxUi::TreeItemData& /*outItem*/) const override
    {
    }
};

class ConsumerTreeDelegate final : public DxUi::ITreeDelegate
{
public:
    // Revision 4: a membership change to a nonempty selection, with its primary member; focus-only moves arrive below.
    void OnTreeSelectionChanged(uint64_t /*itemId*/) override
    {
    }
    void OnTreeFocusedItemChanged(DxUi::Tree& /*sender*/, OptionalRow /*itemId*/) override
    {
    }
    void OnTreeSelectionSetChanged(std::span<const uint64_t> /*selectedItemIds*/) override
    {
    }
};

static_assert(! std::is_abstract_v<ConsumerGridModel> && ! std::is_abstract_v<ConsumerGridDelegate>);
static_assert(! std::is_abstract_v<ConsumerTreeModel> && ! std::is_abstract_v<ConsumerTreeDelegate>);

// Revision 4 inserted flowDirection between rootVerticalPlacement and minRootWidthDip: consumers name the members.
[[maybe_unused]] static const DxUi::ContextMenuSessionCallbacks kRevision4SessionCallbacks{
    .flowDirection = DxUi::FlowDirection::RightToLeft, .minRootWidthDip = 120.0f, .focusFirstNavigableItem = true};
