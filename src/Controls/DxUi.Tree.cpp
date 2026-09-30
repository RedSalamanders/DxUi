#include "DxUi.Internal.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <unordered_set>
#include <vector>

#include "../Support/Diagnostics.h"

namespace DxUi
{
namespace
{
constexpr float kTreeBadgeMinWidthDip           = 28.0f;
constexpr float kTreeBadgeMinHeightDip          = 16.0f;
constexpr float kTreeBadgeMaxHeightDip          = 18.0f;
constexpr float kTreeBadgeHorizontalPaddingDip  = 16.0f;
constexpr float kTreeContentInsetDip            = 2.0f;
constexpr size_t kTreeBadgeWidthCacheMaxEntries = 64u;

struct TreeResolvedRowVisuals final
{
    D2D1_COLOR_F fill      = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F text      = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F icon      = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F expander  = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F badgeFill = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F badgeText = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    D2D1_COLOR_F focus     = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
    bool showFocus         = false;
    bool usesRainbow       = false;
};

[[nodiscard]] float ClampScroll(float value, float extent) noexcept
{
    return extent <= 0.0f ? 0.0f : (std::clamp)(value, 0.0f, extent);
}

[[nodiscard]] float ResolveDensityScaledTreeMetricDip(float baseDip, float minimumDip, Density density) noexcept
{
    const float scale = density == Density::Compact ? 0.82f : 1.0f;
    return (std::max)(minimumDip, baseDip * scale);
}

// `selected` paints the selection; `current` (the focused item) owns the focus ring. They are the same row unless the tree
// has a multi-selection, where every selected row takes the selection colors and only the current one shows focus.
[[nodiscard]] TreeResolvedRowVisuals ResolveTreeRowVisuals(const ThemePalette& theme,
                                                           std::wstring_view rainbowSeed,
                                                           AdornmentTone badgeTone,
                                                           bool selected,
                                                           bool current,
                                                           bool focused,
                                                           bool keyboardFocused,
                                                           bool hovered) noexcept
{
    TreeResolvedRowVisuals visuals{};
    visuals.text = theme.text;

    const bool allowRainbow = theme.rainbowMode && ! theme.highContrast && ! rainbowSeed.empty() && (selected || hovered);
    if (allowRainbow)
    {
        visuals.usesRainbow            = true;
        const D2D1_COLOR_F rainbowFill = RainbowTint(rainbowSeed, theme.dark);
        if (selected)
        {
            visuals.fill = focused ? rainbowFill : BlendColor(theme.surfaceBackground, rainbowFill, theme.dark ? 0.58f : 0.44f);
        }
        else
        {
            visuals.fill = BlendColor(theme.surfaceBackground, rainbowFill, theme.dark ? 0.28f : 0.18f);
        }
        visuals.text = ChooseContrastingTextColor(visuals.fill);
    }
    else if (selected)
    {
        visuals.fill = focused ? theme.selectionFill : theme.selectionInactiveFill;
        visuals.text = focused ? theme.selectionText : theme.text;
    }
    else if (hovered)
    {
        visuals.fill = theme.hoverFill;
    }

    visuals.icon                          = ResolveListIconColor(theme, visuals.text, selected);
    visuals.expander                      = visuals.text;
    const TreeBadgeVisualStyle badgeStyle = ResolveTreeBadgeVisualStyle(theme, badgeTone);
    visuals.badgeFill                     = badgeStyle.fill;
    visuals.badgeText                     = badgeStyle.text;
    visuals.focus                         = theme.focusStroke;
    visuals.showFocus                     = current && (keyboardFocused || (theme.highContrast && focused));
    return visuals;
}

[[nodiscard]] float Lerp(float from, float to, float t) noexcept
{
    return from + ((to - from) * std::clamp(t, 0.0f, 1.0f));
}

[[nodiscard]] D2D1_COLOR_F WithAlpha(const D2D1_COLOR_F& color, float alpha) noexcept
{
    D2D1_COLOR_F result = color;
    result.a *= std::clamp(alpha, 0.0f, 1.0f);
    return result;
}

[[nodiscard]] std::optional<size_t> FindVisibleItemIndexById(const std::vector<TreeItemData>& items, uint64_t itemId) noexcept
{
    for (size_t index = 0u; index < items.size(); ++index)
    {
        if (items[index].id == itemId)
        {
            return index;
        }
    }

    return std::nullopt;
}

[[nodiscard]] size_t CountDescendants(const std::vector<TreeItemData>& items, size_t parentIndex) noexcept
{
    if (parentIndex >= items.size())
    {
        return 0u;
    }

    const uint32_t parentDepth = items[parentIndex].depth;
    size_t count               = 0u;
    for (size_t index = parentIndex + 1u; index < items.size(); ++index)
    {
        if (items[index].depth <= parentDepth)
        {
            break;
        }
        ++count;
    }

    return count;
}

// Whether two selections hold the same items, whatever their order: a model that only moved rows changes no selection.
[[nodiscard]] bool SameSelectedItems(std::span<const uint64_t> first, std::span<const uint64_t> second)
{
    if (first.size() != second.size())
    {
        return false;
    }
    if (std::ranges::equal(first, second))
    {
        return true;
    }

    std::vector<uint64_t> sortedFirst(first.begin(), first.end());
    std::vector<uint64_t> sortedSecond(second.begin(), second.end());
    std::ranges::sort(sortedFirst);
    std::ranges::sort(sortedSecond);
    return sortedFirst == sortedSecond;
}

void DrawTreeRow(ControlHost& host,
                 const ThemePalette& theme,
                 const TreeItemLayoutMetrics& layout,
                 const TreeItemData& item,
                 bool selected,
                 bool current,
                 bool hovered,
                 bool focused,
                 bool keyboardFocused,
                 float expanderProgress,
                 float alpha) noexcept
{
    const TreeResolvedRowVisuals rowVisuals = ResolveTreeRowVisuals(theme, item.text, item.badgeTone, selected, current, focused, keyboardFocused, hovered);
    const D2D1_COLOR_F fill                 = WithAlpha(rowVisuals.fill, alpha);
    const D2D1_COLOR_F textColor            = WithAlpha(rowVisuals.text, alpha);
    const D2D1_COLOR_F transparent          = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);

    if (fill.a > 0.0f)
    {
        DrawRoundedRect(host, layout.rowRect, fill, transparent, 4.0f);
    }
    if (rowVisuals.showFocus)
    {
        const D2D1_ROUNDED_RECT focusRect = D2D1::RoundedRect(InflateRect(layout.rowRect, -1.5f, -1.5f), 4.0f, 4.0f);
        if (auto* dc = host.GetDeviceContext())
        {
            dc->DrawRoundedRectangle(&focusRect, host.GetSolidBrush(WithAlpha(rowVisuals.focus, alpha)), 1.0f);
        }
    }

    if (layout.hasExpander)
    {
        DrawDisclosureChevron(host, layout.expanderRect, expanderProgress, WithAlpha(rowVisuals.expander, alpha));
    }

    if (layout.hasIcon)
    {
        const D2D1_COLOR_F iconColor = WithAlpha(rowVisuals.icon, alpha);
        DrawCenteredText(host,
                         item.iconText,
                         layout.iconRect,
                         ResolveIconTextFontRole(item.iconText),
                         iconColor,
                         DWRITE_TEXT_ALIGNMENT_CENTER,
                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                         false);
    }

    if (layout.hasBadge)
    {
        DrawRoundedRect(host, layout.badgeRect, WithAlpha(rowVisuals.badgeFill, alpha), transparent, 9.0f);
        DrawCenteredText(host,
                         item.badgeText,
                         layout.badgeRect,
                         FontRole::Small,
                         WithAlpha(rowVisuals.badgeText, alpha),
                         DWRITE_TEXT_ALIGNMENT_CENTER,
                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                         false);
    }

    DrawCenteredText(host, item.text, layout.textRect, FontRole::Body, textColor, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
}

} // namespace

std::optional<size_t> ITreeModel::FindVisibleItemById(uint64_t itemId) const noexcept
{
    TreeItemData itemData;
    for (size_t visibleIndex = 0u; visibleIndex < GetVisibleItemCount(); ++visibleIndex)
    {
        GetVisibleItem(visibleIndex, itemData);
        if (itemData.id == itemId)
        {
            return visibleIndex;
        }
    }

    return std::nullopt;
}

void ITreeDelegate::OnTreeSelectionChanged(uint64_t /*itemId*/)
{
}

void ITreeDelegate::OnTreeItemInvoked(uint64_t /*itemId*/)
{
}

void ITreeDelegate::OnTreeToggleExpanded(uint64_t /*itemId*/, bool /*expanded*/)
{
}

void ITreeDelegate::OnTreeContextMenu(uint64_t /*itemId*/, POINT /*screenPoint*/)
{
}

void ITreeDelegate::OnTreeReorder(const TreeDrop& /*drop*/)
{
}

void ITreeDelegate::OnTreeSelectionSetChanged(std::span<const uint64_t> /*selectedItemIds*/)
{
}

void Tree::SetReorderEnabled(bool enabled) noexcept
{
    if (_reorderEnabled == enabled)
    {
        return;
    }
    _reorderEnabled = enabled;
    if (! enabled && _reorderArmed)
    {
        // Disabling mid-drag ends it like a cancel: no capture stays behind and the indicator is repainted away.
        ClearReorderDrag();
        if (ControlHost* const host = GetHost(); host && host->GetCapturedControl() == this)
        {
            host->ReleaseMouseCapture();
        }
        RequestInvalidate();
    }
}

void Tree::ClearReorderDrag() noexcept
{
    _reorderArmed              = false;
    _reorderDragging           = false;
    _reorderCollapsesSelection = false;
    _reorderSourceId           = 0u;
    _reorderSourceIndex        = 0u;
    _reorderSubtreeEnd         = 0u;
    _reorderDropIndex          = 0u;
    _reorderDrop.reset();
}

bool Tree::ResolveReorderSource() noexcept
{
    const std::optional<size_t> source = _model ? _model->FindVisibleItemById(_reorderSourceId) : std::nullopt;
    if (! source.has_value())
    {
        return false;
    }
    // Visible rows are in pre-order, so the row's visible descendants are the run after it that is nested deeper.
    TreeItemData item;
    _model->GetVisibleItem(source.value(), item);
    const uint32_t sourceDepth = item.depth;
    const size_t count         = _model->GetVisibleItemCount();
    size_t end                 = source.value() + 1u;
    for (; end < count; ++end)
    {
        _model->GetVisibleItem(end, item);
        if (item.depth <= sourceDepth)
        {
            break;
        }
    }
    _reorderSourceIndex = source.value();
    _reorderSubtreeEnd  = end;
    return true;
}

std::optional<TreeDrop> Tree::ResolveReorderDrop(D2D1_POINT_2F point, size_t& targetIndex) const noexcept
{
    if (! _model || ! _reorderArmed)
    {
        return std::nullopt;
    }
    const std::optional<size_t> index = FindVisibleItemAtPoint(point);
    if (! index.has_value())
    {
        return std::nullopt;
    }
    if (index.value() > _reorderSourceIndex && index.value() < _reorderSubtreeEnd)
    {
        return std::nullopt; // Into its own subtree: the row would become its own ancestor.
    }
    TreeItemData target;
    _model->GetVisibleItem(index.value(), target);
    if (target.id == _reorderSourceId)
    {
        return std::nullopt;
    }
    const std::optional<D2D1_RECT_F> rect = GetVisibleItemHitRect(index.value());
    if (! rect.has_value() || rect->bottom <= rect->top)
    {
        return std::nullopt;
    }
    const float span  = rect->bottom - rect->top;
    const float along = (point.y - rect->top) / span;
    TreeDrop drop;
    drop.sourceId = _reorderSourceId;
    drop.targetId = target.id;
    if (target.hasChildren && along > 0.25f && along < 0.75f)
    {
        drop.place = TreeDropPlace::Inside;
    }
    else if (along < 0.5f)
    {
        drop.place = TreeDropPlace::Before;
    }
    else
    {
        drop.place = TreeDropPlace::After;
    }
    targetIndex = index.value();
    return drop;
}

void Tree::UpdateReorderDrop(ControlHost& host, D2D1_POINT_2F point) noexcept
{
    size_t targetIndex                 = 0u;
    const std::optional<TreeDrop> drop = ResolveReorderDrop(point, targetIndex);
    const bool unchanged =
        drop.has_value() == _reorderDrop.has_value() &&
        (! drop.has_value() || (drop->targetId == _reorderDrop->targetId && drop->place == _reorderDrop->place && targetIndex == _reorderDropIndex));
    if (unchanged)
    {
        return; // Moving within one drop zone repaints nothing.
    }
    _reorderDrop      = drop;
    _reorderDropIndex = targetIndex;
    Invalidate(host);
}

Tree::Tree()
{
    SetFocusable(true);
}

void Tree::InvalidateTreeTextMeasurementCaches() const noexcept
{
    _badgeWidthCache.clear();
    _tooltipOverflowCache.valid = false;
    _tooltipOverflowCache.text.clear();
    _tooltipOverflowCache.tooltipText.clear();
    _tooltipOverflowCache.resolvedTooltipText.clear();
}

void Tree::SetEmptyStateText(std::wstring text)
{
    if (_emptyStateText == text)
        return;
    _emptyStateText = std::move(text);
    RequestInvalidate();
}

std::wstring_view Tree::GetEmptyStateText() const noexcept
{
    return _emptyStateText.empty() ? std::wstring_view(L"No data") : std::wstring_view(_emptyStateText);
}

void Tree::SetModel(ITreeModel* model) noexcept
{
    // A row drag cannot outlive the model it started in: ids in another model name other rows.
    if (_reorderArmed)
    {
        ClearReorderDrag();
        if (ControlHost* const host = GetHost(); host && host->GetCapturedControl() == this)
        {
            host->ReleaseMouseCapture();
        }
    }
    // Non-owning pointer assignment. Caller responsible for model lifetime.
    _model                    = model;
    _wheelDeltaRemainder      = 0.0f;
    _verticalScrollbarHotPart = ScrollbarHotPart::None;
    _dragVerticalThumb        = false;
    _dragThumbOffsetDip       = 0.0f;
    InvalidateTreeTextMeasurementCaches();
    ClearTreeExpansionAnimation();
    NotifyDataChanged();
}

void Tree::SetDelegate(ITreeDelegate* delegate) noexcept
{
    _delegate = delegate;
}

void Tree::SetRowHeightDip(float rowHeightDip) noexcept
{
    _rowHeightBaseDip = (std::max)(kMinimumInteractiveTextRowHeightDip, rowHeightDip);
    OnDensityChanged();
}

void Tree::SetIndentDip(float indentDip) noexcept
{
    _indentDip = (std::max)(10.0f, indentDip);
}

void Tree::NotifyDataChanged()
{
    if (_reorderArmed)
    {
        // Rows moved: the drop target is resolved again on the next pointer move, and the drag ends if its row is gone.
        _reorderDrop.reset();
        if (! ResolveReorderSource())
        {
            ClearReorderDrag();
            if (ControlHost* const host = GetHost(); host && host->GetCapturedControl() == this)
            {
                host->ReleaseMouseCapture();
            }
        }
    }
    InvalidateTreeTextMeasurementCaches();
    // Multi-select: selected items that left the visible rows (removed, or hidden by a collapsed ancestor) leave the
    // selection and the rest keep the model's order. The delegate hears of a change last, when nothing else is left to do.
    std::vector<uint64_t> previousSelection;
    if (_multiSelect && _selection.GetCount() > 0u)
    {
        const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
        previousSelection.assign(selection.begin(), selection.end());
        ReconcileSelectionWithModel();
    }
    if (_selectedItemId && (! _model || ! _model->FindVisibleItemById(_selectedItemId.value())))
    {
        _selectedItemId.reset();
    }
    ClampScrollOffset();
    if (GetVerticalScrollableExtent() <= 0.0f)
    {
        _wheelDeltaRemainder      = 0.0f;
        _verticalScrollbarHotPart = ScrollbarHotPart::None;
        _dragVerticalThumb        = false;
        _dragThumbOffsetDip       = 0.0f;
    }
    if (const std::optional<size_t> selectedIndex = FindSelectedVisibleIndex())
    {
        EnsureVisibleIndex(selectedIndex.value());
    }

    if (_treeExpansionAnimation && (! _model || _treeExpansionAnimation->afterItems.size() != _model->GetVisibleItemCount()))
    {
        ClearTreeExpansionAnimation();
    }
    RefreshAccessibilitySnapshot();
    if (! previousSelection.empty())
    {
        static_cast<void>(NotifySelectionSetChanged(previousSelection));
    }
}

void Tree::SetMultiSelectEnabled(bool enabled) noexcept
{
    if (_multiSelect == enabled)
    {
        return;
    }

    _multiSelect               = enabled;
    _reorderCollapsesSelection = false;
    if (enabled)
    {
        // The selected item carries over as the whole selection and the anchor.
        _selection.Clear();
        if (_selectedItemId)
        {
            _selection.SetSingle(_selectedItemId.value());
        }
    }
    else
    {
        // A single selection is its focused item: keep it when it is selected, else the last selected item, else none.
        if (! _selectedItemId || ! _selection.IsSelected(_selectedItemId.value()))
        {
            const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
            _selectedItemId                           = selection.empty() ? std::optional<uint64_t>() : std::optional<uint64_t>(selection.back());
        }
        _selection.Clear();
    }
    RefreshAccessibilitySnapshot();
    RequestInvalidate();
}

void Tree::SetSelectedItemId(std::optional<uint64_t> itemId) noexcept
{
    _selectedItemId = std::move(itemId);
    if (_multiSelect)
    {
        if (_selectedItemId)
        {
            _selection.SetSingle(_selectedItemId.value());
        }
        else
        {
            _selection.Clear();
        }
    }
    if (const std::optional<size_t> selectedIndex = FindSelectedVisibleIndex())
    {
        EnsureVisibleIndex(selectedIndex.value());
    }
    RefreshAccessibilitySnapshot();
}

std::optional<uint64_t> Tree::GetSelectedItemId() const noexcept
{
    return _selectedItemId;
}

void Tree::SetFocusedItemId(std::optional<uint64_t> itemId) noexcept
{
    if (! _multiSelect)
    {
        SetSelectedItemId(std::move(itemId));
        return;
    }

    _selectedItemId = std::move(itemId);
    if (const std::optional<size_t> focusedIndex = FindSelectedVisibleIndex())
    {
        EnsureVisibleIndex(focusedIndex.value());
    }
    RefreshAccessibilitySnapshot();
}

std::optional<uint64_t> Tree::GetFocusedItemId() const noexcept
{
    return _selectedItemId;
}

std::vector<uint64_t> Tree::GetSelectedItemIds() const
{
    if (_multiSelect)
    {
        const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
        return std::vector<uint64_t>(selection.begin(), selection.end());
    }
    return _selectedItemId ? std::vector<uint64_t>{_selectedItemId.value()} : std::vector<uint64_t>{};
}

bool Tree::IsItemSelected(uint64_t itemId) const noexcept
{
    return _multiSelect ? _selection.IsSelected(itemId) : (_selectedItemId.has_value() && _selectedItemId.value() == itemId);
}

void Tree::SetSelectedItemIds(std::span<const uint64_t> itemIds) noexcept
{
    const std::vector<uint64_t> visibleIds = CollectVisibleItemIds();
    const std::unordered_set<uint64_t> visible(visibleIds.begin(), visibleIds.end());
    // The last listed id that is a visible row is the focused item (and the anchor); the rest keep the model's order.
    std::optional<uint64_t> last;
    for (auto it = itemIds.rbegin(); it != itemIds.rend() && ! last; ++it)
    {
        if (visible.contains(*it))
        {
            last = *it;
        }
    }

    _selectedItemId = last;
    if (_multiSelect)
    {
        _selection.Clear();
        if (last)
        {
            _selection.SetSingle(last.value());
            for (const uint64_t itemId : itemIds)
            {
                if (itemId != last.value() && visible.contains(itemId) && ! _selection.IsSelected(itemId))
                {
                    _selection.Toggle(itemId);
                }
            }
            _selection.PreserveOrdered(visibleIds);
        }
    }
    if (const std::optional<size_t> selectedIndex = FindSelectedVisibleIndex())
    {
        EnsureVisibleIndex(selectedIndex.value());
    }
    RefreshAccessibilitySnapshot();
}

void Tree::RefreshAccessibilitySnapshot() const noexcept
{
    if (ControlHost* const host = GetHost())
    {
        RefreshWindowHostAccessibilitySnapshot(host->GetHwnd(), host);
    }
}

void Tree::OnDensityChanged() noexcept
{
    Control::OnDensityChanged();
    _rowHeightDip = ResolveDensityScaledTreeMetricDip(_rowHeightBaseDip, kMinimumInteractiveTextRowHeightDip, GetDensity());
    InvalidateTreeTextMeasurementCaches();
    ClampScrollOffset();
}

bool Tree::RequestSelectVisibleItem(size_t visibleIndex) noexcept
{
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }

    return SelectVisibleIndex(visibleIndex, true);
}

bool Tree::RequestExpandedState(size_t visibleIndex, bool expanded) noexcept
{
    if (! _model || ! _delegate || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }

    std::vector<TreeItemData> beforeItems = CaptureVisibleItems();
    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    if (! item.hasChildren)
    {
        return false;
    }

    if (item.expanded == expanded)
    {
        return true;
    }

    StartExpanderAnimation(item.id, item.expanded, expanded);
    const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
    ITreeDelegate* const delegate         = _delegate;
    delegate->OnTreeToggleExpanded(item.id, expanded);
    if (selfLifetime.expired())
    {
        return false;
    }
    BeginTreeExpansionAnimation(item.id, expanded, std::move(beforeItems), CaptureVisibleItems(), GetTickCount64());
    RefreshAccessibilitySnapshot();
    return true;
}

TreeItemLayoutMetrics Tree::GetItemLayoutMetrics(const ControlHost& host, size_t visibleIndex) const
{
    TreeItemLayoutMetrics metrics{};
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return metrics;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    return ComputeItemLayoutMetrics(host, visibleIndex, item);
}

#if DXUI_ENABLE_DIAGNOSTICS
float Tree::DebugGetVerticalScrollDip() const noexcept
{
    return _verticalScrollDip;
}

size_t Tree::DebugGetFirstVisibleIndex() const noexcept
{
    return GetFirstVisibleItemIndex();
}

std::optional<size_t> Tree::DebugGetSelectedVisibleIndex() const noexcept
{
    return FindSelectedVisibleIndex();
}

bool Tree::DebugGetRowVisualState(const ThemePalette& theme, size_t visibleIndex, bool keyboardFocusVisible, TreeDebugRowVisualState& out) const noexcept
{
    out = {};
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    const bool selected = IsItemSelected(item.id);
    const bool current  = _selectedItemId && _selectedItemId.value() == item.id;
    const bool hovered  = _hoveredVisibleIndex && _hoveredVisibleIndex.value() == visibleIndex;
    const TreeResolvedRowVisuals visuals =
        ResolveTreeRowVisuals(theme, item.text, item.badgeTone, selected, current, HasFocus(), keyboardFocusVisible && HasFocus(), hovered);
    out.fillArgb         = PackColor(visuals.fill);
    out.textArgb         = PackColor(visuals.text);
    out.iconArgb         = PackColor(visuals.icon);
    out.expanderArgb     = PackColor(visuals.expander);
    out.badgeFillArgb    = PackColor(visuals.badgeFill);
    out.badgeTextArgb    = PackColor(visuals.badgeText);
    out.focusArgb        = visuals.showFocus ? PackColor(visuals.focus) : 0u;
    out.showFocus        = visuals.showFocus;
    out.usesRainbow      = visuals.usesRainbow;
    out.selected         = selected;
    out.current          = current;
    out.iconUsesIconFont = ! item.iconText.empty() && IconTextUsesIconFont(item.iconText);
    return true;
}

TreeScrollbarVisualState Tree::DebugGetScrollbarVisualState(const ThemePalette& theme) const noexcept
{
    TreeScrollbarVisualState state{};
    state.verticalTrackRect     = GetVerticalScrollbarRect();
    state.verticalThumbRect     = GetVerticalThumbRect();
    state.hasVerticalScrollbar  = state.verticalTrackRect.right > state.verticalTrackRect.left && state.verticalTrackRect.bottom > state.verticalTrackRect.top;
    state.verticalTrackHovered  = _verticalScrollbarHotPart == ScrollbarHotPart::Track;
    state.verticalThumbHovered  = _verticalScrollbarHotPart == ScrollbarHotPart::Thumb;
    state.verticalThumbDragging = _dragVerticalThumb;
    const ScrollbarAnimationTargets targets =
        ResolveScrollbarAnimationTargets(state.verticalTrackHovered, state.verticalThumbHovered, state.verticalThumbDragging);
    state.verticalTrackHotProgress = theme.reducedMotion ? targets.track : _verticalScrollbarAnimation.trackProgress;
    state.verticalThumbHotProgress = theme.reducedMotion ? targets.thumb : _verticalScrollbarAnimation.thumbProgress;

    const ResolvedScrollbarVisuals visuals = ResolveScrollbarVisuals(theme, targets, state.verticalTrackHotProgress, state.verticalThumbHotProgress);
    state.verticalTrackArgb                = PackColor(visuals.track);
    state.verticalThumbArgb                = PackColor(visuals.thumb);
    return state;
}
#endif

void Tree::Paint(ControlHost& host) const
{
    const ThemePalette& theme                 = host.GetTheme();
    const TreeSurfaceVisualStyle surfaceStyle = ResolveTreeSurfaceVisualStyle(theme);
    DrawRoundedRect(host, GetBounds(), surfaceStyle.fill, surfaceStyle.border, 4.0f);

    const D2D1_RECT_F contentRect = GetContentRect();
    if (! _model || _model->GetVisibleItemCount() == 0u)
    {
        DrawCenteredText(host,
                         GetEmptyStateText(),
                         contentRect,
                         FontRole::Small,
                         surfaceStyle.emptyText,
                         DWRITE_TEXT_ALIGNMENT_CENTER,
                         DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                         false);
        return;
    }

    const uint64_t nowTickMs                        = GetTickCount64();
    const bool animateTreeExpansion                 = HasActiveTreeExpansionAnimation(nowTickMs) && _treeExpansionAnimation.has_value();
    const std::optional<size_t> hoveredVisibleIndex = _hoveredVisibleIndex;
    TreeItemData item;
    const VisibleSpan span = ComputeVisibleSpan(
        static_cast<uint64_t>(_model->GetVisibleItemCount()), _rowHeightDip, _verticalScrollDip, (std::max)(1.0f, contentRect.bottom - contentRect.top));
    for (uint64_t visibleIndex = span.beginIndex; visibleIndex < span.endIndex; ++visibleIndex)
    {
        _model->GetVisibleItem(static_cast<size_t>(visibleIndex), item);
        float rowTopDip = contentRect.top + (static_cast<float>(visibleIndex) * _rowHeightDip) - _verticalScrollDip;
        float rowAlpha  = 1.0f;

        if (animateTreeExpansion)
        {
            const auto& animation        = _treeExpansionAnimation.value();
            const float progress         = GetTreeExpansionProgress(nowTickMs);
            const auto beforeIndex       = FindVisibleItemIndexById(animation.beforeItems, item.id);
            const auto afterParentIndex  = FindVisibleItemIndexById(animation.afterItems, animation.itemId);
            const auto beforeParentIndex = FindVisibleItemIndexById(animation.beforeItems, animation.itemId);
            if (beforeIndex.has_value())
            {
                const float beforeTopDip = contentRect.top + (static_cast<float>(beforeIndex.value()) * _rowHeightDip) - _verticalScrollDip;
                rowTopDip                = Lerp(beforeTopDip, rowTopDip, progress);
            }
            else if (animation.toExpanded && beforeParentIndex.has_value() && afterParentIndex.has_value())
            {
                const float collapsedTopDip = contentRect.top + (static_cast<float>(beforeParentIndex.value() + 1u) * _rowHeightDip) - _verticalScrollDip;
                rowTopDip                   = Lerp(collapsedTopDip, rowTopDip, progress);
                rowAlpha                    = progress;
            }
        }

        const TreeItemLayoutMetrics layout = ComputeItemLayoutMetrics(host, rowTopDip, item);
        if (layout.rowRect.bottom <= contentRect.top || layout.rowRect.top >= contentRect.bottom)
        {
            continue;
        }

        const bool selected          = IsItemSelected(item.id);
        const bool current           = _selectedItemId && _selectedItemId.value() == item.id;
        const bool hovered           = hoveredVisibleIndex && hoveredVisibleIndex.value() == static_cast<size_t>(visibleIndex);
        const bool keyboardFocused   = current && HasFocus() && host.IsKeyboardFocusVisible();
        const float expanderProgress = theme.reducedMotion ? (item.expanded ? 1.0f : 0.0f) : GetExpanderProgress(item.id, item.expanded, nowTickMs);
        DrawTreeRow(host, theme, layout, item, selected, current, hovered, HasFocus(), keyboardFocused, expanderProgress, rowAlpha);
    }

    if (animateTreeExpansion && _treeExpansionAnimation.has_value() && ! _treeExpansionAnimation->toExpanded)
    {
        const auto& animation        = _treeExpansionAnimation.value();
        const auto beforeParentIndex = FindVisibleItemIndexById(animation.beforeItems, animation.itemId);
        const auto afterParentIndex  = FindVisibleItemIndexById(animation.afterItems, animation.itemId);
        if (beforeParentIndex.has_value() && afterParentIndex.has_value())
        {
            const float progress             = GetTreeExpansionProgress(nowTickMs);
            const size_t descendantCount     = CountDescendants(animation.beforeItems, beforeParentIndex.value());
            const float collapseTargetTopDip = contentRect.top + (static_cast<float>(afterParentIndex.value() + 1u) * _rowHeightDip) - _verticalScrollDip;
            for (size_t offset = 0u; offset < descendantCount; ++offset)
            {
                const size_t beforeIndex        = beforeParentIndex.value() + 1u + offset;
                const TreeItemData& removedItem = animation.beforeItems[beforeIndex];
                if (FindVisibleItemIndexById(animation.afterItems, removedItem.id).has_value())
                {
                    continue;
                }

                const float beforeTopDip           = contentRect.top + (static_cast<float>(beforeIndex) * _rowHeightDip) - _verticalScrollDip;
                const float rowTopDip              = Lerp(beforeTopDip, collapseTargetTopDip, progress);
                const TreeItemLayoutMetrics layout = ComputeItemLayoutMetrics(host, rowTopDip, removedItem);
                if (layout.rowRect.bottom <= contentRect.top || layout.rowRect.top >= contentRect.bottom)
                {
                    continue;
                }

                const bool selected          = IsItemSelected(removedItem.id);
                const bool current           = _selectedItemId && _selectedItemId.value() == removedItem.id;
                const bool keyboardFocused   = current && HasFocus() && host.IsKeyboardFocusVisible();
                const float expanderProgress = theme.reducedMotion ? 0.0f : GetExpanderProgress(removedItem.id, false, nowTickMs);
                DrawTreeRow(host, theme, layout, removedItem, selected, current, false, HasFocus(), keyboardFocused, expanderProgress, 1.0f - progress);
            }
        }
    }

    if (_reorderDrop.has_value())
    {
        // The drop is resolved with its row index and cleared whenever the model changes, so no id scan per paint.
        const std::optional<D2D1_RECT_F> rect = GetVisibleItemHitRect(_reorderDropIndex);
        if (rect.has_value())
        {
            if (_reorderDrop->place == TreeDropPlace::Inside)
            {
                D2D1_COLOR_F fill = theme.accent;
                fill.a            = 0.28f;
                DrawRoundedRect(host, rect.value(), fill, theme.accent, 2.0f);
            }
            else
            {
                const float y = _reorderDrop->place == TreeDropPlace::Before ? rect->top : rect->bottom;
                const D2D1_RECT_F line =
                    D2D1::RectF(contentRect.left + 4.0f, y - 1.0f, (std::max)(contentRect.left + 8.0f, contentRect.right - 4.0f), y + 1.0f);
                DrawRoundedRect(host, line, theme.accent, theme.accent, 1.0f);
            }
        }
    }

    if (GetVerticalScrollableExtent() > 0.0f)
    {
        const D2D1_RECT_F track                 = GetVerticalScrollbarRect();
        const bool trackHovered                 = _verticalScrollbarHotPart == ScrollbarHotPart::Track;
        const bool thumbHovered                 = _verticalScrollbarHotPart == ScrollbarHotPart::Thumb;
        const ScrollbarAnimationTargets targets = ResolveScrollbarAnimationTargets(trackHovered, thumbHovered, _dragVerticalThumb);
        const ResolvedScrollbarVisuals visuals  = ResolveScrollbarVisuals(theme,
                                                                          targets,
                                                                          theme.reducedMotion ? targets.track : _verticalScrollbarAnimation.trackProgress,
                                                                          theme.reducedMotion ? targets.thumb : _verticalScrollbarAnimation.thumbProgress);
        PaintScrollbar(host, track, GetVerticalThumbRect(), visuals);
    }
}

bool Tree::Tick(ControlHost& host, uint64_t nowTickMs)
{
    if (host.GetTheme().reducedMotion)
    {
        ClearTreeExpansionAnimation();
        _expanderAnimations.clear();
        return false;
    }

    bool hasActiveAnimation = false;
    bool needsFinalRepaint  = false;
    auto animationIt        = _expanderAnimations.begin();
    while (animationIt != _expanderAnimations.end())
    {
        const float progress = ComputeExpanderProgress(animationIt->itemId, animationIt->toExpanded, nowTickMs);
        if (progress > 0.0f && progress < 1.0f)
        {
            hasActiveAnimation = true;
            ++animationIt;
            continue;
        }

        needsFinalRepaint = true;
        animationIt       = _expanderAnimations.erase(animationIt);
    }

    if (HasActiveTreeExpansionAnimation(nowTickMs))
    {
        if (GetTreeExpansionProgress(nowTickMs) < 1.0f)
        {
            hasActiveAnimation = true;
        }
        else
        {
            ClearTreeExpansionAnimation();
            needsFinalRepaint = true;
        }
    }

    if (needsFinalRepaint)
    {
        hasActiveAnimation = true;
    }

    hasActiveAnimation = AdvanceScrollbarAnimation(host, _verticalScrollbarAnimation, nowTickMs) || hasActiveAnimation;
    if (hasActiveAnimation)
    {
        // Expander, expansion and scrollbar transitions paint from the tick time, including their settling frame.
        Invalidate(host);
    }
    return hasActiveAnimation;
}

bool Tree::OnMouseMove(ControlHost& host, D2D1_POINT_2F point, UINT /*modifiers*/)
{
    if (_dragVerticalThumb)
    {
        const D2D1_RECT_F track = GetVerticalScrollbarRect();
        const D2D1_RECT_F thumb = GetVerticalThumbHitRect();
        const float thumbHeight = (std::max)(0.0f, thumb.bottom - thumb.top);
        const float available   = (std::max)(0.0f, (track.bottom - track.top) - thumbHeight);
        const float extent      = GetVerticalScrollableExtent();
        if (available > 0.0f && extent > 0.0f)
        {
            const float thumbTop = (std::clamp)(point.y - _dragThumbOffsetDip, track.top, track.bottom - thumbHeight);
            _verticalScrollDip   = ((thumbTop - track.top) / available) * extent;
            ClampScrollOffset();
        }
        UpdateScrollbarHotState(HitInfo{.zone = HitZone::VerticalScrollbar, .onScrollbarThumb = true});
        host.ClearTooltip();
        Invalidate(host);
        return true;
    }

    if (_reorderArmed)
    {
        const float dx = point.x - _reorderPress.x;
        const float dy = point.y - _reorderPress.y;
        if (! _reorderDragging && (dx * dx) + (dy * dy) >= 16.0f)
        {
            _reorderDragging = true;
            host.ClearTooltip();
        }
        if (_reorderDragging)
        {
            UpdateReorderDrop(host, point);
        }
        return true;
    }

    const HitInfo hit                                = HitTestPoint(MakePointDip(point));
    const std::optional<size_t> previousHoveredIndex = _hoveredVisibleIndex;
    const ScrollbarHotPart previousHotPart           = _verticalScrollbarHotPart;
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    const std::optional<size_t> hoveredIndex =
        (hit.zone == HitZone::Item || hit.zone == HitZone::Expander) ? std::optional<size_t>(hit.visibleIndex) : std::optional<size_t>();
    _hoveredVisibleIndex = hoveredIndex;

    bool tooltipChanged = false;
    if (_hoveredVisibleIndex.has_value())
    {
        TreeItemData item;
        _model->GetVisibleItem(_hoveredVisibleIndex.value(), item);
        const TreeItemLayoutMetrics layout = ComputeItemLayoutMetrics(host, _hoveredVisibleIndex.value(), item);
        std::wstring tooltipText           = ResolveCachedTreeTooltipText(host, _hoveredVisibleIndex.value(), item, layout);
        tooltipChanged                     = tooltipText.empty() ? host.BeginTooltipHideDelay() : host.SetTooltip(std::move(tooltipText), point);
    }
    else
    {
        tooltipChanged = host.BeginTooltipHideDelay();
    }

    if (previousHoveredIndex != _hoveredVisibleIndex || previousHotPart != _verticalScrollbarHotPart || tooltipChanged)
    {
        Invalidate(host);
    }
    return hit.zone != HitZone::None;
}

bool Tree::OnMouseLeave(ControlHost& host)
{
    const bool hadHover        = _hoveredVisibleIndex.has_value();
    const bool hadHotScrollbar = _verticalScrollbarHotPart != ScrollbarHotPart::None;
    const bool hadTooltip      = host.HasTooltip();
    if (! hadHover && ! hadHotScrollbar && ! _dragVerticalThumb && ! hadTooltip)
    {
        return false;
    }

    _hoveredVisibleIndex.reset();
    UpdateScrollbarHotState(HitInfo{});
    SyncScrollbarAnimation(host);
    const bool tooltipChanged = host.BeginTooltipHideDelay();
    if (hadHover || hadHotScrollbar || _dragVerticalThumb || tooltipChanged)
    {
        Invalidate(host);
    }
    return true;
}

bool Tree::OnMouseDown(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (hit.zone == HitZone::None)
    {
        return false;
    }

    host.SetFocusControl(this);
    UpdateScrollbarHotState(hit);
    SyncScrollbarAnimation(host);
    if (rightButton)
    {
        if (_reorderArmed)
        {
            // A second button cancels the row drag: its release would otherwise drop the capture without a cancel.
            ClearReorderDrag();
            host.ReleaseMouseCapture();
            Invalidate(host);
        }
        return OnContextMenu(host, false, point);
    }

    if (hit.zone == HitZone::VerticalScrollbar)
    {
        if (hit.onScrollbarThumb)
        {
            _dragVerticalThumb  = true;
            _dragThumbOffsetDip = point.y - GetVerticalThumbHitRect().top;
            SyncScrollbarAnimation(host);
        }
        else
        {
            const D2D1_RECT_F thumb       = GetVerticalThumbHitRect();
            const D2D1_RECT_F contentRect = GetContentRect();
            const float viewportDip       = (std::max)(1.0f, contentRect.bottom - contentRect.top);
            const float extent            = GetVerticalScrollableExtent();
            const float pageStep = ComputeScrollbarPageStepDip(GetVerticalScrollbarRect(), ScrollbarOrientation::Vertical, viewportDip, viewportDip + extent);
            _verticalScrollDip += point.y < thumb.top ? -pageStep : pageStep;
            ClampScrollOffset();
            SyncScrollbarAnimation(host);
        }
        Invalidate(host);
        return true;
    }

    TreeItemData hitItem;
    _model->GetVisibleItem(hit.visibleIndex, hitItem);

    // With multi-select Shift and Ctrl are selection gestures (they never start a row drag), the expander moves the focus
    // and expands without touching the selection, and a plain press on a row of a multi-selection keeps it until the
    // release, so that the row can be dragged. Without multi-select every press selects the row alone, as it always did.
    SelectMode mode            = SelectMode::Replace;
    bool selectionGesture      = false;
    bool keepsSelectionForDrag = false;
    if (_multiSelect)
    {
        if (hit.zone == HitZone::Expander)
        {
            mode = SelectMode::FocusOnly;
        }
        else if (ModifiersContainShift(modifiers))
        {
            mode             = SelectMode::Range;
            selectionGesture = true;
        }
        else if (ModifiersContainCtrl(modifiers))
        {
            mode             = SelectMode::Toggle;
            selectionGesture = true;
        }
        else if (_reorderEnabled && _selection.GetCount() > 1u && _selection.IsSelected(hitItem.id))
        {
            mode                  = SelectMode::FocusOnly;
            keepsSelectionForDrag = true;
        }
    }
    if (! SelectVisibleIndex(hit.visibleIndex, mode, true))
    {
        return true;
    }
    if (hit.zone == HitZone::Expander)
    {
        const std::optional<size_t> currentIndex = _model ? _model->FindVisibleItemById(hitItem.id) : std::nullopt;
        if (! currentIndex.has_value() || ! ToggleExpanded(currentIndex.value()))
        {
            return true;
        }
        if (! host.GetTheme().reducedMotion)
        {
            host.RequestAnimation();
        }
    }
    else if (_reorderEnabled && hit.zone == HitZone::Item && ! selectionGesture)
    {
        // Resolved by id: the selection callback above may already have changed the model.
        _reorderSourceId = hitItem.id;
        if (ResolveReorderSource())
        {
            _reorderArmed              = true;
            _reorderDragging           = false;
            _reorderCollapsesSelection = keepsSelectionForDrag;
            _reorderPress              = point;
            _reorderDrop.reset();
            host.CaptureMouse(this);
        }
        else
        {
            ClearReorderDrag();
        }
    }
    Invalidate(host);
    return true;
}

bool Tree::OnMouseDoubleClick(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT modifiers)
{
    if (rightButton)
    {
        return false;
    }

    const HitInfo hit = HitTestPoint(MakePointDip(point));
    if (hit.zone != HitZone::Item && hit.zone != HitZone::Expander)
    {
        return false;
    }

    host.SetFocusControl(this);
    TreeItemData item;
    _model->GetVisibleItem(hit.visibleIndex, item);
    // The first press of a double-click already made its selection gesture: with multi-select the second, with Ctrl or
    // Shift, only activates, so a Ctrl double-click cannot toggle the item off again.
    const bool selectionModifier = ModifiersContainCtrl(modifiers) || ModifiersContainShift(modifiers);
    if (! SelectVisibleIndex(hit.visibleIndex, _multiSelect && selectionModifier ? SelectMode::FocusOnly : SelectMode::Replace, true))
    {
        return true;
    }

    const std::optional<size_t> currentIndex = _model ? _model->FindVisibleItemById(item.id) : std::nullopt;
    if (! currentIndex.has_value())
    {
        return true;
    }

    TreeItemData currentItem;
    _model->GetVisibleItem(currentIndex.value(), currentItem);
    if (currentItem.hasChildren)
    {
        if (! ToggleExpanded(currentIndex.value()))
        {
            return true;
        }
        if (! host.GetTheme().reducedMotion)
        {
            host.RequestAnimation();
        }
    }
    else if (_delegate)
    {
        Invalidate(host);
        _delegate->OnTreeItemInvoked(currentItem.id);
        return true;
    }
    Invalidate(host);
    return true;
}

bool Tree::OnMouseUp(ControlHost& host, D2D1_POINT_2F point, bool rightButton, UINT /*modifiers*/)
{
    if (rightButton)
    {
        return false;
    }

    if (_reorderArmed)
    {
        const bool dragged = _reorderDragging;
        if (dragged)
        {
            // The release point decides the drop, not the last move (touch, synthetic or embedded input may differ).
            UpdateReorderDrop(host, point);
        }
        const bool commit   = dragged && _reorderDrop.has_value() && _delegate != nullptr;
        const TreeDrop drop = _reorderDrop.value_or(TreeDrop{});
        // A click (not a drag) on a row of a multi-selection selects that row alone once the pointer is up.
        const bool collapse       = ! dragged && _reorderCollapsesSelection;
        const uint64_t collapseId = _reorderSourceId;
        ClearReorderDrag();
        host.ReleaseMouseCapture();
        if (collapse)
        {
            const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
            if (! CollapseSelectionToItem(collapseId) || selfLifetime.expired())
            {
                return false;
            }
        }
        Invalidate(host);
        if (commit)
        {
            _delegate->OnTreeReorder(drop);
        }
        // A click that never became a drag reports unhandled, like the tree without reordering.
        return dragged;
    }

    const bool wasDragging = _dragVerticalThumb;
    _dragVerticalThumb     = false;
    _dragThumbOffsetDip    = 0.0f;
    UpdateScrollbarHotState(HitTestPoint(MakePointDip(point)));
    SyncScrollbarAnimation(host);
    if (wasDragging)
    {
        Invalidate(host);
    }
    return wasDragging;
}

void Tree::OnCaptureLost(ControlHost& host)
{
    if (_reorderArmed)
    {
        ClearReorderDrag();
        Invalidate(host);
    }
    if (_dragVerticalThumb)
    {
        _dragVerticalThumb  = false;
        _dragThumbOffsetDip = 0.0f;
        UpdateScrollbarHotState(HitInfo{});
        SyncScrollbarAnimation(host);
        Invalidate(host);
    }
}

bool Tree::OnMouseWheel(ControlHost& host, D2D1_POINT_2F point, float wheelDelta, UINT /*modifiers*/)
{
    if (! PointInRect(GetHitBounds(), point) || GetVerticalScrollableExtent() <= 0.0f)
    {
        return false;
    }

    _wheelDeltaRemainder += wheelDelta;
    const int wheelStepCount = static_cast<int>(_wheelDeltaRemainder / static_cast<float>(WHEEL_DELTA));
    if (wheelStepCount == 0)
    {
        return true;
    }

    _wheelDeltaRemainder -= static_cast<float>(wheelStepCount * WHEEL_DELTA);
    _verticalScrollDip -= static_cast<float>(wheelStepCount) * (_rowHeightDip * 3.0f);
    ClampScrollOffset();
    if (_reorderDragging)
    {
        // Scrolling moves rows under a still pointer: the drop follows the row now beneath it.
        UpdateReorderDrop(host, point);
    }
    Invalidate(host);
    return true;
}

bool Tree::OnKeyDown(ControlHost& host, UINT virtualKey, UINT modifiers)
{
    if (_reorderArmed && virtualKey == VK_ESCAPE)
    {
        ClearReorderDrag();
        host.ReleaseMouseCapture();
        Invalidate(host);
        return true;
    }

    if (! _model || _model->GetVisibleItemCount() == 0u)
    {
        return false;
    }

    if (_multiSelect && ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers) && virtualKey == 'A')
    {
        return OnSelectAll(host);
    }

    // With multi-select a movement key with Shift extends the selection from the anchor, and with Ctrl moves the focus
    // alone (Ctrl+Space then toggles the item it reached). Without it modifiers change nothing: a movement selects.
    SelectMode moveMode = SelectMode::Replace;
    if (_multiSelect)
    {
        moveMode = ModifiersContainShift(modifiers) ? SelectMode::Range : (ModifiersContainCtrl(modifiers) ? SelectMode::FocusOnly : SelectMode::Replace);
    }

    const auto landOn = [this, &host](size_t visibleIndex, SelectMode mode) -> bool
    {
        if (! SelectVisibleIndex(visibleIndex, mode, true))
        {
            return true;
        }
        if (const HWND hwnd = host.GetHwnd(); hwnd && IsWindow(hwnd) != FALSE && GetFocus() != hwnd)
        {
            static_cast<void>(SetFocus(hwnd));
        }
        Invalidate(host);
        return true;
    };
    const auto selectAndInvalidate = [&landOn, moveMode](size_t visibleIndex) -> bool { return landOn(visibleIndex, moveMode); };

    std::optional<size_t> currentIndex = FindSelectedVisibleIndex();
    if (! currentIndex)
    {
        // The first key starts from the first row. With multi-select it only takes the focus there, so that the key's
        // own gesture (Ctrl+Space, Shift+Down) decides what is selected.
        currentIndex = 0u;
        if (! SelectVisibleIndex(currentIndex.value(), _multiSelect ? SelectMode::FocusOnly : SelectMode::Replace, false))
        {
            return true;
        }
    }

    TreeItemData item;
    _model->GetVisibleItem(currentIndex.value(), item);
    const size_t itemCount = _model->GetVisibleItemCount();
    const size_t pageRows =
        (std::max<size_t>)(1u, static_cast<size_t>(std::floor((std::max)(1.0f, GetContentRect().bottom - GetContentRect().top) / _rowHeightDip)));

    switch (virtualKey)
    {
        case VK_UP: return currentIndex.value() > 0u ? selectAndInvalidate(currentIndex.value() - 1u) : true;
        case VK_DOWN: return currentIndex.value() + 1u < itemCount ? selectAndInvalidate(currentIndex.value() + 1u) : true;
        case VK_HOME: return selectAndInvalidate(0u);
        case VK_END: return selectAndInvalidate(itemCount - 1u);
        case VK_PRIOR: return selectAndInvalidate((currentIndex.value() > pageRows) ? (currentIndex.value() - pageRows) : 0u);
        case VK_NEXT: return selectAndInvalidate((std::min)(itemCount - 1u, currentIndex.value() + pageRows));
        case VK_LEFT:
            if (item.hasChildren && item.expanded)
            {
                if (! ToggleExpanded(currentIndex.value()))
                {
                    return true;
                }
                if (! host.GetTheme().reducedMotion)
                {
                    host.RequestAnimation();
                }
                Invalidate(host);
                return true;
            }
            if (item.parentId)
            {
                if (const std::optional<size_t> parentIndex = _model->FindVisibleItemById(item.parentId.value()))
                {
                    return selectAndInvalidate(parentIndex.value());
                }
            }
            return true;
        case VK_RIGHT:
            if (item.hasChildren && ! item.expanded)
            {
                if (! ToggleExpanded(currentIndex.value()))
                {
                    return true;
                }
                if (! host.GetTheme().reducedMotion)
                {
                    host.RequestAnimation();
                }
                Invalidate(host);
                return true;
            }
            if (item.hasChildren && item.expanded && currentIndex.value() + 1u < itemCount)
            {
                TreeItemData nextItem;
                _model->GetVisibleItem(currentIndex.value() + 1u, nextItem);
                if (nextItem.parentId && nextItem.parentId.value() == item.id)
                {
                    return selectAndInvalidate(currentIndex.value() + 1u);
                }
            }
            return true;
        case VK_RETURN:
        case VK_SPACE:
            if (_multiSelect && virtualKey == VK_SPACE && ModifiersContainCtrl(modifiers))
            {
                return landOn(currentIndex.value(), SelectMode::Toggle);
            }
            if (_delegate)
            {
                _delegate->OnTreeItemInvoked(item.id);
            }
            return true;
        default: return false;
    }
}

bool Tree::OnChar(ControlHost& host, wchar_t ch, UINT modifiers)
{
    if (! _model || _model->GetVisibleItemCount() == 0u || ch < 0x20)
    {
        return false;
    }
    if (_multiSelect && ModifiersContainCtrl(modifiers) && ! ModifiersContainAlt(modifiers))
    {
        return false; // Ctrl+Space (toggle) sends a space character too: a Ctrl chord is a command, not typeahead.
    }

    const uint64_t nowTickMs = GetTickCount64();
    if (_typeaheadBuffer.empty() || nowTickMs - _lastTypeaheadTickMs > kTypeaheadResetMs)
    {
        _typeaheadBuffer.clear();
    }
    _lastTypeaheadTickMs = nowTickMs;
    _typeaheadBuffer.push_back(ch);

    if (const std::optional<size_t> matchIndex = FindNextTypeaheadMatch(_typeaheadBuffer))
    {
        if (! SelectVisibleIndex(matchIndex.value(), true))
        {
            return true;
        }
        Invalidate(host);
        return true;
    }

    if (_typeaheadBuffer.size() > 1u)
    {
        _typeaheadBuffer.assign(1u, ch);
        if (const std::optional<size_t> matchIndex = FindNextTypeaheadMatch(_typeaheadBuffer))
        {
            if (! SelectVisibleIndex(matchIndex.value(), true))
            {
                return true;
            }
            Invalidate(host);
            return true;
        }
    }

    return false;
}

bool Tree::OnContextMenu(ControlHost& host, bool keyboardInvocation, D2D1_POINT_2F pointDip)
{
    if (! _model || ! _delegate || _model->GetVisibleItemCount() == 0u)
    {
        return false;
    }

    size_t visibleIndex     = 0u;
    D2D1_POINT_2F anchorDip = pointDip;
    if (keyboardInvocation)
    {
        visibleIndex                  = FindSelectedVisibleIndex().value_or(0u);
        const float previousScrollDip = _verticalScrollDip;
        EnsureVisibleIndex(visibleIndex);
        if (_verticalScrollDip != previousScrollDip)
        {
            Invalidate(host);
        }

        const D2D1_RECT_F contentRect = GetContentRect();
        const float rowTop            = contentRect.top + (static_cast<float>(visibleIndex) * _rowHeightDip) - _verticalScrollDip;
        const float rowBottom         = rowTop + _rowHeightDip;
        const float minX              = contentRect.left + 4.0f;
        const float maxX              = (std::max)(minX, contentRect.right - 4.0f);
        const float minY              = contentRect.top + 4.0f;
        const float maxY              = (std::max)(minY, contentRect.bottom - 4.0f);
        anchorDip = D2D1::Point2F((std::clamp)(GetBounds().left + 16.0f, minX, maxX), (std::clamp)((rowTop + rowBottom) * 0.5f, minY, maxY));
    }
    else
    {
        const HitInfo hit = HitTestPoint(MakePointDip(pointDip));
        if (hit.zone != HitZone::Item && hit.zone != HitZone::Expander)
        {
            return false;
        }

        visibleIndex = hit.visibleIndex;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    const POINT screenPoint = host.DipPointToScreenPoint(anchorDip);

    if (! keyboardInvocation)
    {
        const std::optional<uint64_t> previousSelection = _selectedItemId;
        // A right-click on a row of a multi-selection keeps it, so the command it opens applies to the selection (the
        // delegate compares the item with GetSelectedItemIds); any other row is selected alone first.
        const SelectMode mode = (_multiSelect && _selection.GetCount() > 1u && _selection.IsSelected(item.id)) ? SelectMode::FocusOnly : SelectMode::Replace;
        if (! SelectVisibleIndex(visibleIndex, mode, true))
        {
            return true;
        }
        if (_multiSelect || _selectedItemId != previousSelection)
        {
            Invalidate(host);
        }
    }

    if (! _model || ! _delegate || ! _model->FindVisibleItemById(item.id).has_value())
    {
        return true;
    }

    _delegate->OnTreeContextMenu(item.id, screenPoint);
    return true;
}

float Tree::MeasureCachedBadgeTextWidthDip(const ControlHost& host, std::wstring_view badgeText) const noexcept
{
    auto* textFormat = host.GetTextFormat(FontRole::Small);
    for (const TreeBadgeWidthCacheEntry& entry : _badgeWidthCache)
    {
        if (entry.text == badgeText && entry.textFormat == textFormat)
        {
            return entry.widthDip;
        }
    }

    const float widthDip = MeasureSingleLineTextWidthDip(&host, badgeText, FontRole::Small);
    if (Debug::Perf::IsCaptureEnabled())
    {
        Debug::Perf::Emit(L"dxui.tree.badge_width_cache_miss_count", L"", 0u, 1u, static_cast<uint64_t>(_badgeWidthCache.size()), S_OK);
    }
    if (_badgeWidthCache.size() >= kTreeBadgeWidthCacheMaxEntries)
    {
        _badgeWidthCache.erase(_badgeWidthCache.begin());
    }
    _badgeWidthCache.push_back(TreeBadgeWidthCacheEntry{.text = std::wstring(badgeText), .textFormat = textFormat, .widthDip = widthDip});
    return widthDip;
}

std::wstring Tree::ResolveCachedTreeTooltipText(const ControlHost& host,
                                                size_t visibleIndex,
                                                const TreeItemData& item,
                                                const TreeItemLayoutMetrics& layout) const
{
    const float availableWidthDip   = (std::max)(0.0f, layout.textRect.right - layout.textRect.left);
    auto* textFormat                = host.GetTextFormat(FontRole::Body);
    TreeTooltipOverflowCache& cache = _tooltipOverflowCache;
    if (cache.valid && cache.visibleIndex == visibleIndex && cache.itemId == item.id && cache.text == item.text && cache.tooltipText == item.tooltipText &&
        cache.textFormat == textFormat && cache.availableWidthDip == availableWidthDip)
    {
        return cache.resolvedTooltipText;
    }

    std::wstring resolvedTooltipText;
    if (! item.tooltipText.empty())
    {
        resolvedTooltipText = item.tooltipText;
    }
    else if (availableWidthDip > 0.0f && ! item.text.empty())
    {
        const float textWidthDip = MeasureSingleLineTextWidthDip(&host, item.text, FontRole::Body);
        if (textWidthDip > (availableWidthDip + 0.5f))
        {
            resolvedTooltipText = item.text;
        }
    }

    if (Debug::Perf::IsCaptureEnabled())
    {
        Debug::Perf::Emit(L"dxui.tree.tooltip_overflow_cache_miss_count", L"", 0u, 1u, resolvedTooltipText.empty() ? 0u : 1u, S_OK);
    }
    cache.valid               = true;
    cache.visibleIndex        = visibleIndex;
    cache.itemId              = item.id;
    cache.text                = item.text;
    cache.tooltipText         = item.tooltipText;
    cache.textFormat          = textFormat;
    cache.availableWidthDip   = availableWidthDip;
    cache.resolvedTooltipText = resolvedTooltipText;
    return cache.resolvedTooltipText;
}

TreeItemLayoutMetrics Tree::ComputeItemLayoutMetrics(const ControlHost& host, size_t visibleIndex, const TreeItemData& item) const noexcept
{
    TreeItemLayoutMetrics metrics{};
    const D2D1_RECT_F contentRect = GetContentRect();
    if (contentRect.right <= contentRect.left || contentRect.bottom <= contentRect.top)
    {
        return metrics;
    }

    const float rowTop  = contentRect.top + (static_cast<float>(visibleIndex) * _rowHeightDip) - _verticalScrollDip;
    metrics.rowRect     = D2D1::RectF(contentRect.left + 2.0f, rowTop, contentRect.right - 2.0f, rowTop + _rowHeightDip);
    metrics.hasExpander = item.hasChildren;
    metrics.hasIcon     = ! item.iconText.empty();
    metrics.hasBadge    = ! item.badgeText.empty();

    float contentLeft = contentRect.left + 8.0f + (static_cast<float>(item.depth) * _indentDip);
    if (metrics.hasExpander)
    {
        metrics.expanderRect = D2D1::RectF(contentLeft, metrics.rowRect.top + 6.0f, contentLeft + 12.0f, metrics.rowRect.bottom - 6.0f);
        contentLeft          = metrics.expanderRect.right + 4.0f;
    }

    if (metrics.hasIcon)
    {
        const float iconTop = metrics.rowRect.top + (std::max)(2.0f, (_rowHeightDip - 18.0f) * 0.5f);
        metrics.iconRect    = D2D1::RectF(contentLeft, iconTop, contentLeft + 18.0f, (std::min)(metrics.rowRect.bottom - 2.0f, iconTop + 18.0f));
        contentLeft         = metrics.iconRect.right + 4.0f;
    }

    float contentRight = contentRect.right - 8.0f;
    if (metrics.hasBadge)
    {
        const float badgeTextWidth = MeasureCachedBadgeTextWidthDip(host, item.badgeText);
        const float badgeWidth     = (std::clamp)(badgeTextWidth + kTreeBadgeHorizontalPaddingDip,
                                                  kTreeBadgeMinWidthDip,
                                                  (std::max)(kTreeBadgeMinWidthDip, contentRect.right - contentRect.left - 16.0f));
        const float badgeHeight    = (std::min)(kTreeBadgeMaxHeightDip, (std::max)(kTreeBadgeMinHeightDip, _rowHeightDip - 10.0f));
        const float badgeTop       = metrics.rowRect.top + (std::max)(2.0f, (_rowHeightDip - badgeHeight) * 0.5f);
        const float badgeLeft      = (std::max)(contentLeft + 20.0f, contentRight - badgeWidth);
        metrics.badgeRect          = D2D1::RectF(badgeLeft, badgeTop, contentRight, badgeTop + badgeHeight);
        contentRight               = metrics.badgeRect.left - 8.0f;
    }

    metrics.textRect = D2D1::RectF(contentLeft, metrics.rowRect.top, (std::max)(contentLeft, contentRight), metrics.rowRect.bottom);
    return metrics;
}

TreeItemLayoutMetrics Tree::ComputeItemLayoutMetrics(const ControlHost& host, float rowTopDip, const TreeItemData& item) const noexcept
{
    TreeItemLayoutMetrics metrics{};
    const D2D1_RECT_F contentRect = GetContentRect();
    if (contentRect.right <= contentRect.left || contentRect.bottom <= contentRect.top)
    {
        return metrics;
    }

    metrics.rowRect     = D2D1::RectF(contentRect.left + 2.0f, rowTopDip, contentRect.right - 2.0f, rowTopDip + _rowHeightDip);
    metrics.hasExpander = item.hasChildren;
    metrics.hasIcon     = ! item.iconText.empty();
    metrics.hasBadge    = ! item.badgeText.empty();

    float contentLeft = contentRect.left + 8.0f + (static_cast<float>(item.depth) * _indentDip);
    if (metrics.hasExpander)
    {
        metrics.expanderRect = D2D1::RectF(contentLeft, metrics.rowRect.top + 6.0f, contentLeft + 12.0f, metrics.rowRect.bottom - 6.0f);
        contentLeft          = metrics.expanderRect.right + 4.0f;
    }

    if (metrics.hasIcon)
    {
        const float iconTop = metrics.rowRect.top + (std::max)(2.0f, (_rowHeightDip - 18.0f) * 0.5f);
        metrics.iconRect    = D2D1::RectF(contentLeft, iconTop, contentLeft + 18.0f, (std::min)(metrics.rowRect.bottom - 2.0f, iconTop + 18.0f));
        contentLeft         = metrics.iconRect.right + 4.0f;
    }

    float contentRight = contentRect.right - 8.0f;
    if (metrics.hasBadge)
    {
        const float badgeTextWidth = MeasureCachedBadgeTextWidthDip(host, item.badgeText);
        const float badgeWidth     = (std::clamp)(badgeTextWidth + kTreeBadgeHorizontalPaddingDip,
                                                  kTreeBadgeMinWidthDip,
                                                  (std::max)(kTreeBadgeMinWidthDip, contentRect.right - contentRect.left - 16.0f));
        const float badgeHeight    = (std::min)(kTreeBadgeMaxHeightDip, (std::max)(kTreeBadgeMinHeightDip, _rowHeightDip - 10.0f));
        const float badgeTop       = metrics.rowRect.top + (std::max)(2.0f, (_rowHeightDip - badgeHeight) * 0.5f);
        const float badgeLeft      = (std::max)(contentLeft + 20.0f, contentRight - badgeWidth);
        metrics.badgeRect          = D2D1::RectF(badgeLeft, badgeTop, contentRight, badgeTop + badgeHeight);
        contentRight               = metrics.badgeRect.left - 8.0f;
    }

    metrics.textRect = D2D1::RectF(contentLeft, metrics.rowRect.top, (std::max)(contentLeft, contentRight), metrics.rowRect.bottom);
    return metrics;
}

Tree::HitInfo Tree::HitTestPoint(PointDip pointDip) const noexcept
{
    const D2D1_POINT_2F point = pointDip.AsD2D();
    if (! _model || _model->GetVisibleItemCount() == 0u || ! PointInRect(GetHitBounds(), point))
    {
        return {};
    }

    const D2D1_RECT_F scrollbarRect = GetVerticalScrollbarRect();
    if (scrollbarRect.right > scrollbarRect.left && PointInRect(scrollbarRect, point))
    {
        HitInfo hit;
        hit.zone             = HitZone::VerticalScrollbar;
        hit.rectDip          = scrollbarRect;
        hit.onScrollbarThumb = PointInRect(GetVerticalThumbHitRect(), point);
        return hit;
    }

    const D2D1_RECT_F contentRect = GetContentRect();
    if (! PointInRect(contentRect, point))
    {
        return {};
    }

    const float offsetDip = (point.y - contentRect.top) + _verticalScrollDip;
    if (offsetDip < 0.0f)
    {
        return {};
    }

    const size_t visibleIndex = static_cast<size_t>(offsetDip / _rowHeightDip);
    if (visibleIndex >= _model->GetVisibleItemCount())
    {
        return {};
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);

    const float rowTop = contentRect.top + (static_cast<float>(visibleIndex) * _rowHeightDip) - _verticalScrollDip;
    HitInfo hit;
    hit.zone         = HitZone::Item;
    hit.visibleIndex = visibleIndex;
    hit.rectDip      = D2D1::RectF(contentRect.left, rowTop, contentRect.right, rowTop + _rowHeightDip);

    const float indentLeft         = contentRect.left + 8.0f + (static_cast<float>(item.depth) * _indentDip);
    const D2D1_RECT_F expanderRect = D2D1::RectF(indentLeft, hit.rectDip.top + 6.0f, indentLeft + 12.0f, hit.rectDip.bottom - 6.0f);
    if (item.hasChildren && PointInRect(expanderRect, point))
    {
        hit.zone = HitZone::Expander;
    }
    return hit;
}

size_t Tree::GetFirstVisibleItemIndex() const noexcept
{
    if (! _model || _model->GetVisibleItemCount() == 0u || _rowHeightDip <= 0.0f)
    {
        return 0u;
    }

    const size_t firstVisibleIndex = static_cast<size_t>((std::max)(0.0f, _verticalScrollDip) / _rowHeightDip);
    return (std::min)(firstVisibleIndex, _model->GetVisibleItemCount() - 1u);
}

std::optional<D2D1_RECT_F> Tree::GetVisibleItemHitRect(size_t visibleIndex) const noexcept
{
    if (! _model || visibleIndex >= _model->GetVisibleItemCount() || _rowHeightDip <= 0.0f)
    {
        return std::nullopt;
    }

    const D2D1_RECT_F contentRect = GetContentRect();
    if (contentRect.right <= contentRect.left || contentRect.bottom <= contentRect.top)
    {
        return std::nullopt;
    }

    const float rowTop = contentRect.top + (static_cast<float>(visibleIndex) * _rowHeightDip) - _verticalScrollDip;
    const D2D1_RECT_F rowRect =
        D2D1::RectF(contentRect.left + 2.0f, rowTop, (std::max)(contentRect.left + 2.0f, contentRect.right - 2.0f), rowTop + _rowHeightDip);
    return (rowRect.bottom >= contentRect.top && rowRect.top <= contentRect.bottom) ? std::optional<D2D1_RECT_F>{rowRect} : std::nullopt;
}

std::optional<size_t> Tree::FindVisibleItemAtPoint(D2D1_POINT_2F pointDip) const noexcept
{
    if (! _model || _model->GetVisibleItemCount() == 0u || _rowHeightDip <= 0.0f || ! PointInRect(GetHitBounds(), pointDip))
    {
        return std::nullopt;
    }

    const D2D1_RECT_F scrollbarRect = GetVerticalScrollbarRect();
    if (scrollbarRect.right > scrollbarRect.left && PointInRect(scrollbarRect, pointDip))
    {
        return std::nullopt;
    }

    const D2D1_RECT_F contentRect = GetContentRect();
    if (! PointInRect(contentRect, pointDip))
    {
        return std::nullopt;
    }

    const float offsetDip = (pointDip.y - contentRect.top) + _verticalScrollDip;
    if (offsetDip < 0.0f)
    {
        return std::nullopt;
    }

    const size_t visibleIndex = static_cast<size_t>(offsetDip / _rowHeightDip);
    return visibleIndex < _model->GetVisibleItemCount() ? std::optional<size_t>{visibleIndex} : std::nullopt;
}

float Tree::GetVerticalScrollableExtent() const noexcept
{
    if (! _model)
    {
        return 0.0f;
    }

    const D2D1_RECT_F bounds   = GetBounds();
    const float viewportHeight = (std::max)(0.0f, (bounds.bottom - bounds.top) - (2.0f * kTreeContentInsetDip));
    return (std::max)(0.0f, (static_cast<float>(_model->GetVisibleItemCount()) * _rowHeightDip) - viewportHeight);
}

D2D1_RECT_F Tree::GetContentRect() const noexcept
{
    D2D1_RECT_F contentRect = GetBounds();
    contentRect.left += kTreeContentInsetDip;
    contentRect.top += kTreeContentInsetDip;
    contentRect.bottom -= kTreeContentInsetDip;
    contentRect.right -= GetVerticalScrollableExtent() > 0.0f ? (kScrollbarThicknessDip + kTreeContentInsetDip) : kTreeContentInsetDip;
    return contentRect;
}

D2D1_RECT_F Tree::GetVerticalScrollbarRect() const noexcept
{
    if (GetVerticalScrollableExtent() <= 0.0f)
    {
        return D2D1::RectF();
    }

    const D2D1_RECT_F bounds = GetBounds();
    return D2D1::RectF(bounds.right - kScrollbarThicknessDip - 2.0f, bounds.top + 2.0f, bounds.right - 2.0f, bounds.bottom - 2.0f);
}

D2D1_RECT_F Tree::GetVerticalThumbRect() const noexcept
{
    const D2D1_RECT_F track = GetVerticalScrollbarRect();
    if (track.right <= track.left || track.bottom <= track.top || ! _model || _model->GetVisibleItemCount() == 0u)
    {
        return D2D1::RectF();
    }

    const float viewportHeight = (std::max)(1.0f, GetContentRect().bottom - GetContentRect().top);
    const float totalHeight    = (std::max)(viewportHeight, static_cast<float>(_model->GetVisibleItemCount()) * _rowHeightDip);
    return ComputeScrollbarThumbRect(track, ScrollbarOrientation::Vertical, viewportHeight, totalHeight, _verticalScrollDip, GetVerticalScrollableExtent());
}

D2D1_RECT_F Tree::GetVerticalThumbHitRect() const noexcept
{
    const D2D1_RECT_F track = GetVerticalScrollbarRect();
    if (track.right <= track.left || track.bottom <= track.top || ! _model || _model->GetVisibleItemCount() == 0u)
    {
        return D2D1::RectF();
    }

    const float viewportHeight = (std::max)(1.0f, GetContentRect().bottom - GetContentRect().top);
    const float totalHeight    = (std::max)(viewportHeight, static_cast<float>(_model->GetVisibleItemCount()) * _rowHeightDip);
    return ComputeScrollbarThumbHitRect(track, ScrollbarOrientation::Vertical, viewportHeight, totalHeight, _verticalScrollDip, GetVerticalScrollableExtent());
}

void Tree::ClampScrollOffset() noexcept
{
    _verticalScrollDip = ClampScroll(_verticalScrollDip, GetVerticalScrollableExtent());
}

void Tree::UpdateScrollbarHotState(const HitInfo& hit) noexcept
{
    _verticalScrollbarHotPart = ScrollbarHotPart::None;
    if (hit.zone == HitZone::VerticalScrollbar)
    {
        _verticalScrollbarHotPart = hit.onScrollbarThumb ? ScrollbarHotPart::Thumb : ScrollbarHotPart::Track;
    }
}

void Tree::SyncScrollbarAnimation(ControlHost& host) noexcept
{
    UpdateScrollbarAnimation(host,
                             _verticalScrollbarAnimation,
                             _verticalScrollbarHotPart == ScrollbarHotPart::Track,
                             _verticalScrollbarHotPart == ScrollbarHotPart::Thumb,
                             _dragVerticalThumb);
}

std::optional<size_t> Tree::FindSelectedVisibleIndex() const noexcept
{
    return (_model && _selectedItemId) ? _model->FindVisibleItemById(_selectedItemId.value()) : std::nullopt;
}

void Tree::EnsureVisibleIndex(size_t visibleIndex) noexcept
{
    const float viewportHeight = (std::max)(1.0f, GetContentRect().bottom - GetContentRect().top);
    const float rowTop         = static_cast<float>(visibleIndex) * _rowHeightDip;
    const float rowBottom      = rowTop + _rowHeightDip;
    if (rowTop < _verticalScrollDip)
    {
        _verticalScrollDip = rowTop;
    }
    else if (rowBottom > _verticalScrollDip + viewportHeight)
    {
        _verticalScrollDip = rowBottom - viewportHeight;
    }
    ClampScrollOffset();
}

bool Tree::SelectVisibleIndex(size_t visibleIndex, bool notifyDelegate)
{
    return SelectVisibleIndex(visibleIndex, SelectMode::Replace, notifyDelegate);
}

bool Tree::SelectVisibleIndex(size_t visibleIndex, SelectMode mode, bool notifyDelegate)
{
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);

    // Without multi-select every gesture replaces the one selected item, which is the focused item.
    std::vector<uint64_t> previousSelection;
    if (_multiSelect)
    {
        const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
        previousSelection.assign(selection.begin(), selection.end());
        switch (mode)
        {
            case SelectMode::Replace: _selection.SetSingle(item.id); break;
            case SelectMode::Toggle:
                _selection.Toggle(item.id);
                _selection.PreserveOrdered(CollectVisibleItemIds()); // A toggled-on item joins at the end: restore the model's order.
                break;
            case SelectMode::Range:
            {
                // The anchor is the item a plain click or key last selected, or else the focused item.
                std::optional<uint64_t> anchor = _selection.GetAnchor();
                if (! anchor)
                {
                    anchor = _selectedItemId;
                }
                if (anchor)
                {
                    _selection.SetRange(CollectVisibleItemIds(), anchor.value(), item.id);
                }
                else
                {
                    _selection.SetSingle(item.id);
                }
                break;
            }
            case SelectMode::FocusOnly: break;
        }
    }
    _selectedItemId = item.id;
    EnsureVisibleIndex(visibleIndex);
    if (notifyDelegate && _delegate)
    {
        const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
        ITreeDelegate* const delegate         = _delegate;
        delegate->OnTreeSelectionChanged(item.id);
        if (selfLifetime.expired())
        {
            return false;
        }
        if (_multiSelect && ! NotifySelectionSetChanged(previousSelection))
        {
            return false;
        }
    }
    RefreshAccessibilitySnapshot();
    return true;
}

std::vector<uint64_t> Tree::CollectVisibleItemIds() const
{
    std::vector<uint64_t> itemIds;
    if (! _model)
    {
        return itemIds;
    }

    const size_t itemCount = _model->GetVisibleItemCount();
    itemIds.reserve(itemCount);
    TreeItemData item;
    for (size_t visibleIndex = 0u; visibleIndex < itemCount; ++visibleIndex)
    {
        _model->GetVisibleItem(visibleIndex, item);
        itemIds.push_back(item.id);
    }
    return itemIds;
}

void Tree::ReconcileSelectionWithModel()
{
    if (_multiSelect)
    {
        _selection.PreserveOrdered(CollectVisibleItemIds());
    }
}

bool Tree::NotifySelectionSetChanged(const std::vector<uint64_t>& previous)
{
    const std::span<const uint64_t> current = _selection.GetOrderedSelection();
    if (! _delegate || SameSelectedItems(previous, current))
    {
        return true;
    }

    // The delegate gets its own copy: it may change the selection, or destroy the tree, from inside the call.
    const std::vector<uint64_t> selectedItemIds(current.begin(), current.end());
    const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
    ITreeDelegate* const delegate         = _delegate;
    delegate->OnTreeSelectionSetChanged(selectedItemIds);
    return ! selfLifetime.expired();
}

bool Tree::CollapseSelectionToItem(uint64_t itemId)
{
    if (! _multiSelect)
    {
        return true;
    }

    // The item already holds the focus from the press: only the selection changes, so only its set callback is due.
    const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
    const std::vector<uint64_t> previousSelection(selection.begin(), selection.end());
    _selection.SetSingle(itemId);
    _selectedItemId = itemId;
    if (! NotifySelectionSetChanged(previousSelection))
    {
        return false;
    }
    RefreshAccessibilitySnapshot();
    return true;
}

bool Tree::RequestAddVisibleItemToSelection(size_t visibleIndex) noexcept
{
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }
    if (! _multiSelect)
    {
        return SelectVisibleIndex(visibleIndex, true);
    }

    // Adding a selected item changes nothing: this is not a toggle.
    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    return SelectVisibleIndex(visibleIndex, _selection.IsSelected(item.id) ? SelectMode::FocusOnly : SelectMode::Toggle, true);
}

bool Tree::RequestRemoveVisibleItemFromSelection(size_t visibleIndex) noexcept
{
    if (! _multiSelect || ! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return false;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    if (! _selection.IsSelected(item.id))
    {
        return true;
    }

    const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
    const std::vector<uint64_t> previousSelection(selection.begin(), selection.end());
    _selection.Toggle(item.id); // Removes it; the focused item stays where it is.
    if (! NotifySelectionSetChanged(previousSelection))
    {
        return false;
    }
    RefreshAccessibilitySnapshot();
    return true;
}

bool Tree::OnSelectAll(ControlHost& host)
{
    if (! _multiSelect || ! _model || _model->GetVisibleItemCount() == 0u)
    {
        return false;
    }

    const std::span<const uint64_t> selection = _selection.GetOrderedSelection();
    const std::vector<uint64_t> previousSelection(selection.begin(), selection.end());
    const std::vector<uint64_t> allItemIds = CollectVisibleItemIds();
    _selection.SetRange(allItemIds, allItemIds.front(), allItemIds.back());
    // The focused item stays; without one the first item takes it, and the delegate hears of that like any other move.
    const bool focusMoves = ! FindSelectedVisibleIndex().has_value();
    if (focusMoves)
    {
        _selectedItemId = allItemIds.front();
    }
    if (_delegate)
    {
        const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
        ITreeDelegate* const delegate         = _delegate;
        if (focusMoves)
        {
            delegate->OnTreeSelectionChanged(allItemIds.front());
            if (selfLifetime.expired())
            {
                return true;
            }
        }
        if (! NotifySelectionSetChanged(previousSelection))
        {
            return true;
        }
    }
    RefreshAccessibilitySnapshot();
    Invalidate(host);
    return true;
}

bool Tree::ToggleExpanded(size_t visibleIndex)
{
    if (! _model || visibleIndex >= _model->GetVisibleItemCount())
    {
        return true;
    }

    TreeItemData item;
    _model->GetVisibleItem(visibleIndex, item);
    const std::weak_ptr<int> selfLifetime = GetLifetimeToken();
    static_cast<void>(RequestExpandedState(visibleIndex, ! item.expanded));
    return ! selfLifetime.expired();
}

float Tree::ComputeExpanderProgress(uint64_t itemId, bool expanded, uint64_t nowTickMs) const noexcept
{
    const auto it = std::find_if(_expanderAnimations.begin(), _expanderAnimations.end(), [itemId](const ExpanderAnimationState& animation) {
        return animation.active && animation.itemId == itemId;
    });
    if (it == _expanderAnimations.end())
    {
        return expanded ? 1.0f : 0.0f;
    }

    if (nowTickMs <= it->startTickMs)
    {
        return it->fromProgress;
    }

    const uint64_t elapsedMs    = nowTickMs - it->startTickMs;
    const float transition      = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(_treeExpanderAnimationDurationMs), 0.0f, 1.0f);
    const float easedTransition = EvaluateEasing(EasingCurve::PointToPoint, transition);
    return Lerp(it->fromProgress, it->toProgress, easedTransition);
}

void Tree::StartExpanderAnimation(uint64_t itemId, bool fromExpanded, bool toExpanded) noexcept
{
    if (fromExpanded == toExpanded)
    {
        return;
    }

    const uint64_t nowTickMs = GetTickCount64();

    // Capture current visual progress before removing existing animation for smooth reversal.
    float currentProgress = fromExpanded ? 1.0f : 0.0f;
    const auto existingIt = std::find_if(
        _expanderAnimations.begin(), _expanderAnimations.end(), [itemId](const ExpanderAnimationState& animation) { return animation.itemId == itemId; });
    if (existingIt != _expanderAnimations.end())
    {
        currentProgress = ComputeExpanderProgress(itemId, fromExpanded, nowTickMs);
        _expanderAnimations.erase(existingIt);
    }

    const float targetProgress = toExpanded ? 1.0f : 0.0f;
    _expanderAnimations.push_back(ExpanderAnimationState{.itemId       = itemId,
                                                         .fromExpanded = fromExpanded,
                                                         .toExpanded   = toExpanded,
                                                         .fromProgress = currentProgress,
                                                         .toProgress   = targetProgress,
                                                         .startTickMs  = nowTickMs,
                                                         .active       = true});
}

float Tree::GetExpanderProgress(uint64_t itemId, bool expanded, uint64_t nowTickMs) const noexcept
{
    return ComputeExpanderProgress(itemId, expanded, nowTickMs);
}

std::vector<TreeItemData> Tree::CaptureVisibleItems() const
{
    std::vector<TreeItemData> items;
    if (! _model)
    {
        return items;
    }

    items.reserve(_model->GetVisibleItemCount());
    TreeItemData item;
    for (size_t visibleIndex = 0u; visibleIndex < _model->GetVisibleItemCount(); ++visibleIndex)
    {
        item = {};
        _model->GetVisibleItem(visibleIndex, item);
        items.push_back(std::move(item));
    }

    return items;
}

void Tree::BeginTreeExpansionAnimation(
    uint64_t itemId, bool toExpanded, std::vector<TreeItemData>&& beforeItems, std::vector<TreeItemData>&& afterItems, uint64_t nowTickMs) noexcept
{
    if (beforeItems.empty() || afterItems.empty())
    {
        ClearTreeExpansionAnimation();
        return;
    }

    if (beforeItems.size() == afterItems.size())
    {
        bool changed = false;
        for (size_t index = 0u; index < beforeItems.size(); ++index)
        {
            if (beforeItems[index].id != afterItems[index].id)
            {
                changed = true;
                break;
            }
        }
        if (! changed)
        {
            ClearTreeExpansionAnimation();
            return;
        }
    }

    TreeExpansionAnimationState animation{};
    animation.itemId        = itemId;
    animation.toExpanded    = toExpanded;
    animation.active        = true;
    animation.startTickMs   = nowTickMs;
    animation.beforeItems   = std::move(beforeItems);
    animation.afterItems    = std::move(afterItems);
    _treeExpansionAnimation = std::move(animation);
}

void Tree::ClearTreeExpansionAnimation() noexcept
{
    _treeExpansionAnimation.reset();
}

bool Tree::HasActiveTreeExpansionAnimation(uint64_t nowTickMs) const noexcept
{
    return _treeExpansionAnimation.has_value() && _treeExpansionAnimation->active && nowTickMs >= _treeExpansionAnimation->startTickMs;
}

float Tree::GetTreeExpansionProgress(uint64_t nowTickMs) const noexcept
{
    if (! _treeExpansionAnimation)
    {
        return 1.0f;
    }

    const uint64_t elapsedMs = nowTickMs > _treeExpansionAnimation->startTickMs ? (nowTickMs - _treeExpansionAnimation->startTickMs) : 0u;
    const float progress     = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(_treeExpansionAnimationDurationMs), 0.0f, 1.0f);
    return EvaluateEasing(EasingCurve::PointToPoint, progress);
}

std::optional<size_t> Tree::FindNextTypeaheadMatch(std::wstring_view prefix) const noexcept
{
    if (! _model || prefix.empty())
    {
        return std::nullopt;
    }

    const size_t itemCount = _model->GetVisibleItemCount();
    if (itemCount == 0u)
    {
        return std::nullopt;
    }

    const std::optional<size_t> currentIndex = FindSelectedVisibleIndex();
    const size_t startIndex                  = currentIndex ? ((currentIndex.value() + 1u) % itemCount) : 0u;
    TreeItemData item;
    for (size_t offset = 0u; offset < itemCount; ++offset)
    {
        const size_t visibleIndex = (startIndex + offset) % itemCount;
        _model->GetVisibleItem(visibleIndex, item);
        if (StartsWithInsensitive(item.text, prefix))
        {
            return visibleIndex;
        }
    }
    return std::nullopt;
}
} // namespace DxUi
