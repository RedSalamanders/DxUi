// Frozen consumer compile fixture for API revision 4. Update only with an explicit consumer-API review.
#include <DxUi/DxUi.h>

#include <memory>
#include <optional>
#include <type_traits>

using OptionalRow = std::optional<uint64_t>;

static_assert(std::is_same_v<decltype(&DxUi::Panel::TakeChild),
    std::unique_ptr<DxUi::Control> (DxUi::Panel::*)(size_t) noexcept>);
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
