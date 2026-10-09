#include "Controls.Tests.DxUiTestHelpers.h"
#include "Controls.Tests.LocalizedLayoutFixture.h"

#include <cctype>
#include <fstream>
#include <functional>
#include <memory>
#include <new>
#include <string>

namespace
{

template <typename Container> void RequireOverlayDismissalSurvivesOwnerRetirement()
{
    class RetiringOverlay final : public DxUi::Control
    {
    public:
        explicit RetiringOverlay(size_t& calls) noexcept : _calls(calls)
        {
        }
        void Paint(DxUi::ControlHost&) const override
        {
        }

    protected:
        bool DismissOverlayOnPointerDown(DxUi::ControlHost& host, D2D1_POINT_2F) override
        {
            ++_calls;
            host.SetRoot({});
            return false;
        }

    private:
        size_t& _calls;
    };
    DxUi::WindowHost host;
    size_t calls = 0u;
    auto root    = std::make_unique<Container>();
    root->template AddChild<DxUi::Label>(L"Earlier sibling");
    root->template AddChild<RetiringOverlay>(calls);
    host.SetRoot(std::move(root));
    host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 160.0f));
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, 0u, MAKELPARAM(8, 8), handled));
    Require(handled && calls == 1u && ! host.GetRoot(), "overlay dismissal stops after a false-returning child retires its parent");
}

void TestPanelOverlayDismissalCanRetireItsOwner()
{
    RequireOverlayDismissalSurvivesOwnerRetirement<DxUi::Panel>();
}

void TestScrollPanelOverlayDismissalCanRetireItsOwner()
{
    RequireOverlayDismissalSurvivesOwnerRetirement<DxUi::ScrollPanel>();
}

void TestLocalizedActionLayout()
{
    RunLocalizedActionFixture([](const auto& sizes, float width, float gap, auto& bounds)
    {
        DxUi::MeasuredActionLayout result{};
        Require(SUCCEEDED(DxUi::ArrangeMeasuredActions(sizes, width, D2D1::SizeF(gap, gap), bounds, result)), "localized action layout succeeds");
        return result.heightDip;
    });
}

void TestPanelTakeChildPreservesInheritanceAndReparentsSafely()
{
    class InheritanceProbe final : public DxUi::Control
    {
    public:
        void Paint(DxUi::ControlHost&) const override
        {
        }
        size_t flowChanges    = 0u;
        size_t densityChanges = 0u;

    protected:
        void OnFlowDirectionChanged() noexcept override
        {
            ++flowChanges;
        }
        void OnDensityChanged() noexcept override
        {
            ++densityChanges;
        }
    };
    DxUi::WindowHost host;
    auto root     = std::make_unique<DxUi::Panel>();
    auto* panel   = root.get();
    auto theme    = host.GetTheme();
    theme.density = DxUi::Density::Standard;
    host.SetTheme(theme);
    panel->SetFlowDirection(DxUi::FlowDirection::RightToLeft);
    panel->SetDensity(DxUi::Density::Compact);
    auto* probe = panel->AddChild<InheritanceProbe>();
    host.SetRoot(std::move(root));
    const size_t oldFlowChanges    = probe->flowChanges;
    const size_t oldDensityChanges = probe->densityChanges;
    auto child                     = panel->TakeChild(0u);
    Require(child.get() == probe && ! panel->GetLogicalChild(0u), "TakeChild leaves an empty ownership slot");
    Require(child->GetFlowDirection() == DxUi::FlowDirection::RightToLeft && child->GetDensity() == DxUi::Density::Compact,
            "detached child retains its inherited settings without an explicit override");
    Require(! child->HasExplicitFlowDirection() && ! child->HasExplicitDensity(), "extraction preserves inheritance policy");
    Require(probe->flowChanges == oldFlowChanges && probe->densityChanges == oldDensityChanges, "detaching does not announce a spurious inheritance change");
    host.SetRoot(std::move(child));
    Require(probe->GetFlowDirection() == DxUi::FlowDirection::LeftToRight && probe->GetDensity() == host.GetTheme().density,
            "promoted root adopts its new owner settings");
    Require(probe->flowChanges == oldFlowChanges + 1u && probe->densityChanges == oldDensityChanges + 1u,
            "promotion announces each changed inherited setting once");
}

void TestPanelTakeChildRevalidatesAfterFocusRetirement()
{
    DxUi::WindowHost host;
    auto root   = std::make_unique<DxUi::Panel>();
    auto* panel = root.get();
    auto* child = panel->AddChild<DxUi::Button>(L"Extract");
    host.SetRoot(std::move(root));
    host.SetFocusControl(child);
    host.SetOnFocusChanged([&host](DxUi::Control*) { host.SetRoot({}); });
    auto taken = panel->TakeChild(0u);
    Require(! taken && ! host.GetRoot(), "extraction stops when blur retires its owner and requested child");
}

void TestTabTakeChildKeepsMetadataAndSelectionCoherent()
{
    DxUi::TabControl tabs;
    tabs.AddTab<DxUi::Label>(L"First", L"First page");
    auto* second = tabs.AddTab<DxUi::Label>(L"Second", L"Second page");
    tabs.AddTab<DxUi::Label>(L"Third", L"Third page");
    tabs.SetSelectedIndex(1u);
    DxUi::Panel& ownership = tabs;
    auto taken             = ownership.TakeChild(1u);
    Require(taken.get() == second && tabs.GetTabCount() == 2u && tabs.GetLogicalChildCount() == 2u,
            "polymorphic extraction removes the matching tab metadata and ownership slot");
    Require(tabs.GetTabTitle(1u) == L"Third" && tabs.GetSelectedIndex() == std::optional<size_t>{1u}, "the replacement page and its tab title remain aligned");
    ownership.ClearChildren();
    Require(tabs.GetTabCount() == 0u && tabs.GetLogicalChildCount() == 0u && ! tabs.GetSelectedIndex(),
            "polymorphic clear removes tab metadata as well as its pages");
}

void TestChildAndTabExtractionCancelLiveSliderDrafts()
{
    using namespace DxUi;
    for (const bool tabPage : {false, true})
    {
        WindowHost host;
        bool handled = false;
        static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0u, MAKELPARAM(320, 180), handled));
        std::unique_ptr<Panel> root = tabPage ? std::unique_ptr<Panel>(std::make_unique<TabControl>()) : std::make_unique<Panel>();
        auto* owner                 = root.get();
        auto* tabs                  = dynamic_cast<TabControl*>(owner);
        Panel* page                 = tabs ? tabs->AddTab<Panel>(L"Draft page") : owner->AddChild<Panel>();
        auto* slider                = page->AddChild<Slider>();
        slider->SetValue(0.25);
        size_t cancellations = 0u;
        slider->SetOnChange([&](SliderChange change)
        {
            if (change.phase == SliderChangePhase::Cancel)
                ++cancellations;
        });
        host.SetRoot(std::move(root));
        page->SetBounds(D2D1::RectF(0, 32, 300, 160));
        slider->SetBounds(D2D1::RectF(0, 40, 240, 80));
        Require(slider->OnMouseDown(host, D2D1::Point2F(180, 60), false, 0u), "extraction fixture begins a live slider draft");
        Require(slider->GetValue() != 0.25 && host.GetCapturedControl() == slider, "draft has changed value and owns capture");
        auto extracted = owner->TakeChild(0u);
        Require(extracted.get() == page && ! host.GetCapturedControl(), "extraction transfers the page and clears its capture");
        Require(slider->GetValue() == 0.25 && cancellations == 1u, "the still-live extracted slider restores the draft exactly once before losing its host");
        Require(! slider->OnMouseMove(host, D2D1::Point2F(220, 60), 0u), "an extracted slider cannot resume the canceled drag");
    }
}

void TestPanelExtractionRevalidatesAfterCaptureCancellationRetiresOwner()
{
    using namespace DxUi;
    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* owner  = root.get();
    auto* slider = owner->AddChild<Slider>();
    slider->SetBounds(D2D1::RectF(0, 0, 240, 40));
    slider->SetOnChange([&](SliderChange change)
    {
        if (change.phase == SliderChangePhase::Cancel)
            host.SetRoot({});
    });
    host.SetRoot(std::move(root));
    Require(slider->OnMouseDown(host, D2D1::Point2F(180, 20), false, 0u), "retirement fixture begins a captured drag");
    auto extracted = owner->TakeChild(0u);
    Require(! extracted && ! host.GetRoot() && ! host.GetCapturedControl(), "extraction stops after cancellation retires the requested control and owner");
}

void TestMeasuredActionsFailureAndDirectionContracts()
{
    using namespace DxUi;
    std::array<D2D1_SIZE_F, 4> sizes{{{100.0f, 32.0f}, {}, {140.0f, 48.0f}, {240.0f, 40.0f}}};
    std::array<D2D1_RECT_F, 5> bounds{};
    bounds.back() = D2D1::RectF(9.0f, 9.0f, 9.0f, 9.0f);
    MeasuredActionLayout result{};
    Require(SUCCEEDED(ArrangeMeasuredActions(sizes, 248.0f, D2D1::SizeF(8.0f, 6.0f), bounds, result)), "measured actions fit exact-width row");
    Require(result.rowCount == 2 && result.heightDip == 94.0f, "rows use their tallest child, with no trailing gap");
    Require(bounds[0].left == 0.0f && bounds[2].left == 108.0f && bounds[2].right == 248.0f && bounds[3].top == 54.0f,
            "hidden action creates no gap and next row follows tallest action");
    Require(bounds[1].right == 0.0f && bounds.back().left == 9.0f, "hidden output empty and unused output tail untouched");
    Require(SUCCEEDED(ArrangeMeasuredActions(sizes, 248.0f, D2D1::SizeF(8.0f, 6.0f), bounds, result, FlowDirection::RightToLeft)),
            "right-to-left action flow succeeds");
    Require(bounds[0].right == 248.0f && bounds[2].left == 0.0f && bounds[3].right == 248.0f, "RTL mirrors geometry without reordering indices");
    const auto saved     = bounds;
    const auto unchanged = [&]()
    {
        return std::equal(bounds.begin(),
                          bounds.end(),
                          saved.begin(),
                          [](const auto& a, const auto& b) { return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom; }) &&
               result.rowCount == 2 && result.heightDip == 94.0f;
    };
    for (const float invalid : {0.0f, -1.0f, (std::numeric_limits<float>::infinity)(), (std::numeric_limits<float>::quiet_NaN)(), 80.0f})
    {
        Require(FAILED(ArrangeMeasuredActions(sizes, invalid, D2D1::SizeF(8.0f, 6.0f), bounds, result)) && unchanged(),
                "invalid or narrower-than-measured viewport publishes nothing");
    }
    Require(FAILED(ArrangeMeasuredActions(sizes, 248.0f, D2D1::SizeF(8.0f, 6.0f), std::span(bounds).first(3), result)) && unchanged(),
            "insufficient capacity preserves all outputs");
    sizes[0].height = (std::numeric_limits<float>::max)();
    sizes[2].height = (std::numeric_limits<float>::max)();
    sizes[3].height = (std::numeric_limits<float>::max)();
    Require(ArrangeMeasuredActions(sizes, 248.0f, D2D1::SizeF(8.0f, 6.0f), bounds, result) == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) && unchanged(),
            "height overflow publishes nothing");
    Require(SUCCEEDED(ArrangeMeasuredActions({}, 248.0f, {}, bounds, result)) && result.rowCount == 0 && result.heightDip == 0.0f,
            "empty group has no rows or height");
    const std::array<D2D1_SIZE_F, 2> hidden{};
    Require(SUCCEEDED(ArrangeMeasuredActions(hidden, 248.0f, {}, bounds, result)) && result.rowCount == 0 && bounds[0].right == 0.0f,
            "all-hidden group has empty geometry");

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Conserver les deux versions du document");
    button->SetBounds(D2D1::RectF(10.0f, 20.0f, 200.0f, 100.0f));
    Require(! button->IsMultiline(), "single-line buttons remain the default");
    host.SetRoot(std::move(root));
    host.SetFocusControl(button);
    unsigned int clicks = 0;
    button->SetOnClick([&]() { ++clicks; });
    button->SetMultiline(true);
    button->SetMultiline(true);
    Require(button->IsMultiline() && button->HasFocus() && clicks == 0, "wrapping acknowledgement preserves focus and never invokes action");
    Require(button->OnKeyDown(host, VK_SPACE, 0) && clicks == 1, "wrapped button preserves keyboard invocation");
    button->SetEnabled(false);
    Require(! button->Invoke(host, false) && clicks == 1, "disabled wrapped action cannot invoke");
}

std::string RemoveAsciiWhitespace(const std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (const char ch : text)
    {
        if (std::isspace(static_cast<unsigned char>(ch)) == 0)
        {
            normalized.push_back(ch);
        }
    }
    return normalized;
}

void TestGroupedGridHeaderClickTogglesCollapsedStateAndRehomesSelection()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));
    grid->SetRowHeightDip(24.0f);
    grid->SetHeaderHeightDip(32.0f);
    static_cast<Panel*>(root.get())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 180.0f));

    GroupedGridModel model(6u);
    model.SetGroups({
        GroupedGridModel::Group{.stableId = 10u, .title = L"Favorites", .startRowIndex = 0u, .rowCount = 2u},
        GroupedGridModel::Group{.stableId = 20u, .title = L"Folders", .startRowIndex = 3u, .rowCount = 2u},
    });

    CollapsibleGroupedGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);
    host.SetRoot(std::move(root));
    grid->GetSelectionModel().SetSingle(model.GetStableRowId(1u));

    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 46.0f), false, 0), "grouped grid handles collapse toggle click");
    Require(delegate.groupToggleCount == 1u, "grouped grid reports one collapse toggle");
    Require(delegate.lastGroupStableId == 10u && delegate.lastGroupCollapsed, "grouped grid reports the collapsed group id and state");
    Require(model.IsGroupCollapsed(10u), "grouped grid delegate collapses the requested group");
    Require(delegate.selectionChangedCount == 1u, "grouped grid collapse notifies when selection moves out of a hidden row");
    Require(grid->GetSelectionModel().GetCount() == 1u, "grouped grid keeps one visible row selected after collapse");
    Require(grid->GetSelectionModel().GetOrderedSelection().front() == model.GetStableRowId(2u),
            "grouped grid rehomes selection to the nearest visible row after collapse");

    const GridVisibleWorkMetrics collapsedMetrics = grid->GetVisibleWorkMetrics();
    Require(collapsedMetrics.visibleRowCount == 4u, "grouped grid collapse updates visible-work metrics including partial rows");

    Require(grid->OnMouseDown(host, D2D1::Point2F(40.0f, 46.0f), false, 0), "grouped grid handles expand toggle click");
    Require(delegate.groupToggleCount == 2u, "grouped grid reports one expand toggle");
    Require(delegate.lastGroupStableId == 10u && ! delegate.lastGroupCollapsed, "grouped grid reports the expanded group id and state");
    Require(! model.IsGroupCollapsed(10u), "grouped grid delegate expands the requested group");
}

void TestToggleLayoutMetricsReserveTextLaneWhenLabelIsPresent()
{
    using namespace DxUi;

    Toggle toggle(L"Compare subdirectories");
    toggle.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));

    const ToggleLayoutMetrics metrics = toggle.GetLayoutMetrics();
    Require(! metrics.compactSwitchOnly, "labeled toggle keeps row layout");
    RequireRectHasArea(metrics.textRect, "labeled toggle preserves a text rect");
    RequireRectHasArea(metrics.trackRect, "labeled toggle preserves a track rect");
    Require(metrics.textRect.right <= metrics.trackRect.left - 8.0f, "labeled toggle reserves a gap between text and switch track");
    Require((metrics.backgroundRect.right - metrics.backgroundRect.left) >= 216.0f, "labeled toggle keeps full-row hover chrome");
}

void TestToggleStateLabelsReserveTextLaneWithoutPrimaryLabel()
{
    using namespace DxUi;

    Toggle toggle;
    toggle.SetStateLabels(L"Detailed", L"Brief");
    toggle.SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));

    const ToggleLayoutMetrics metrics = toggle.GetLayoutMetrics();
    Require(! metrics.compactSwitchOnly, "state-labeled toggle keeps row layout without a primary label");
    RequireRectHasArea(metrics.textRect, "state-labeled toggle reserves a text rect");
    RequireRectHasArea(metrics.trackRect, "state-labeled toggle keeps a track rect");
    Require(metrics.textRect.right <= metrics.trackRect.left - 8.0f, "state-labeled toggle keeps the text lane clear of the switch track");
    Require(toggle.GetDisplayedText() == L"Detailed", "unchecked state-labeled toggle exposes the unchecked label text");
}

void TestToggleStateLabelsFollowCheckedState()
{
    using namespace DxUi;

    Toggle toggle;
    toggle.SetStateLabels(L"Descending", L"Ascending");
    Require(toggle.GetActiveStateLabel() == L"Descending", "toggle state labels expose the unchecked label first");

    toggle.SetChecked(true);
    Require(toggle.GetActiveStateLabel() == L"Ascending", "toggle state labels switch to the checked label");
    Require(toggle.GetDisplayedText() == L"Ascending", "toggle displayed text tracks the checked state label");
}

void TestFocusRingPaintPathsHandleMissingDeviceContext()
{
    using namespace DxUi;

    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* button   = root->AddChild<Button>(L"Apply");
    auto* toggle   = root->AddChild<Toggle>(L"Enabled");
    auto* checkbox = root->AddChild<Checkbox>(L"Selected");
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 28.0f));
    toggle->SetBounds(D2D1::RectF(0.0f, 36.0f, 220.0f, 72.0f));
    checkbox->SetBounds(D2D1::RectF(0.0f, 80.0f, 220.0f, 112.0f));
    host.SetRoot(std::move(root));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_KEYDOWN, VK_TAB, 0, handled));

    host.SetFocusControl(button);
    button->Paint(host);
    host.SetFocusControl(toggle);
    toggle->Paint(host);
    host.SetFocusControl(checkbox);
    checkbox->Paint(host);

    Require(true, "focus-ring paint paths tolerate a missing device context");
}

void TestScrollPanelThumbGutterDragThroughWindowHost()
{
    using namespace DxUi;

    WindowHost host;
    auto root       = std::make_unique<Panel>();
    auto* scroll    = root->AddChild<ScrollPanel>();
    auto* filler    = scroll->AddChild<Panel>();
    const auto rect = D2D1::RectF(0.0f, 0.0f, 100.0f, 100.0f);
    scroll->SetBounds(rect);
    filler->SetBounds(D2D1::RectF(0.0f, 0.0f, 88.0f, 300.0f));
    scroll->SetContentHeight(300.0f);
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(rect);

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(99, 1), handled));
    Require(handled, "scroll panel handles thumb gutter mouse-down as a thumb drag");

    static_cast<void>(host.HandleMessage(nullptr, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(99, 50), handled));
    Require(handled, "scroll panel handles captured thumb gutter mouse-move");
    Require(scroll->GetScrollOffset() > 1.0f, "scroll panel thumb gutter drag moves the scroll offset");

    static_cast<void>(host.HandleMessage(nullptr, WM_LBUTTONUP, 0, MAKELPARAM(99, 50), handled));
    Require(handled, "scroll panel handles captured thumb gutter mouse-up");
}

void TestScrollPanelScrollCallbackCanReplaceItsOwnCallable()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    std::wstring observed;
    observed.reserve(16u);
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 100.0f));
    scroll->SetContentHeight(360.0f);
    host.SetRoot(std::move(root));
    scroll->SetOnScrollChanged([scroll, &observed, payload = std::make_shared<std::wstring>(L"alive")](float)
    {
        scroll->SetOnScrollChanged({});
        observed = *payload;
    });

    scroll->SetScrollOffset(32.0f);
    Require(scroll->GetScrollOffset() == 32.0f, "scroll offset updates before the notification callback");
    Require(observed == L"alive", "the active scroll callable survives resetting its member");
}

void TestScrollPanelScrollDispatchDoesNotCopyCallbackTargets()
{
    struct Callback final
    {
        bool& rejectCopies;
        size_t& calls;
        Callback(bool& reject, size_t& count) noexcept : rejectCopies(reject), calls(count)
        {
        }
        Callback(const Callback& other) : rejectCopies(other.rejectCopies), calls(other.calls)
        {
            if (rejectCopies)
            {
                throw std::bad_alloc{};
            }
        }
        void operator()(float) const
        {
            ++calls;
        }
    };

    DxUi::ScrollPanel scroll;
    scroll.SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 100.0f));
    scroll.SetContentHeight(500.0f);
    bool rejectCopies = false;
    size_t calls      = 0u;
    scroll.SetOnScrollChanged(Callback{rejectCopies, calls});
    rejectCopies = true;
    scroll.SetScrollOffset(20.0f);
    scroll.SetScrollOffset(40.0f);
    Require(calls == 2u, "scroll dispatch invokes its registered target without a throwing target copy");
}

void TestScrollPanelScrollbarTrackCallbackCanDestroyItsOwner()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    std::wstring observed;
    observed.reserve(16u);
    const D2D1_RECT_F bounds = D2D1::RectF(0.0f, 0.0f, 120.0f, 100.0f);
    scroll->SetBounds(bounds);
    scroll->SetContentHeight(500.0f);
    host.SetRoot(std::move(root));

    D2D1_RECT_F thumb{};
    Require(scroll->DebugGetScrollbarThumbHitRect(thumb), "scroll panel exposes its initial scrollbar thumb");
    const D2D1_POINT_2F trackPoint = D2D1::Point2F(bounds.right - 2.0f, bounds.bottom - 2.0f);
    Require(trackPoint.y > thumb.bottom, "the chosen pointer point is on the track below the thumb");
    scroll->SetOnScrollChanged([&host, &observed, payload = std::make_shared<std::wstring>(L"alive")](float)
    {
        host.SetRoot({});
        observed = *payload;
    });

    Require(scroll->OnMouseDown(host, trackPoint, false, 0u), "scrollbar track press pages the scroll panel");
    Require(! host.GetRoot(), "scrollbar track callback destroys the owning control tree");
    Require(observed == L"alive", "track callback survives owner destruction before the pointer handler tail");
}

void TestScrollPanelScrollbarDragCallbackCanDestroyItsOwner()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    std::wstring observed;
    observed.reserve(16u);
    const D2D1_RECT_F bounds = D2D1::RectF(0.0f, 0.0f, 120.0f, 100.0f);
    scroll->SetBounds(bounds);
    scroll->SetContentHeight(500.0f);
    host.SetRoot(std::move(root));

    D2D1_RECT_F thumb{};
    Require(scroll->DebugGetScrollbarThumbHitRect(thumb), "scroll panel exposes its initial scrollbar thumb for drag");
    const D2D1_POINT_2F downPoint = D2D1::Point2F((thumb.left + thumb.right) * 0.5f, (thumb.top + thumb.bottom) * 0.5f);
    Require(scroll->OnMouseDown(host, downPoint, false, 0u), "scrollbar thumb press begins a logical drag");
    scroll->SetOnScrollChanged([&host, &observed, payload = std::make_shared<std::wstring>(L"alive")](float)
    {
        host.SetRoot({});
        observed = *payload;
    });

    const D2D1_POINT_2F movePoint = D2D1::Point2F(downPoint.x, bounds.bottom - 2.0f);
    Require(scroll->OnMouseMove(host, movePoint, MK_LBUTTON), "scrollbar thumb drag handles the logical pointer move");
    Require(! host.GetRoot(), "scrollbar drag callback destroys the owning control tree");
    Require(observed == L"alive", "drag callback survives owner destruction before the pointer handler tail");
}

struct ScrollPanelReentrancyProbeState
{
    size_t mouseDownCount  = 0u;
    size_t mouseMoveCount  = 0u;
    size_t hoverEnterCount = 0u;
};

class ScrollPanelClearingChild final : public DxUi::Control
{
public:
    ScrollPanelClearingChild(DxUi::ScrollPanel& owner, ScrollPanelReentrancyProbeState& state) noexcept : _owner(&owner), _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    bool OnMouseDown(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, bool rightButton, UINT /*modifiers*/) override
    {
        if (rightButton)
        {
            return false;
        }

        ScrollPanelReentrancyProbeState* const state = _state;
        DxUi::ScrollPanel* const owner               = _owner;
        ++state->mouseDownCount;
        owner->ClearChildren();
        return true;
    }

    bool OnMouseMove(DxUi::WindowHost& /*host*/, D2D1_POINT_2F /*point*/, UINT /*modifiers*/) override
    {
        ++_state->mouseMoveCount;
        return true;
    }

protected:
    void OnHoverChanged(DxUi::WindowHost& host, bool hovered) override
    {
        if (hovered)
        {
            ScrollPanelReentrancyProbeState* const state = _state;
            DxUi::ScrollPanel* const owner               = _owner;
            ++state->hoverEnterCount;
            Control::OnHoverChanged(host, hovered);
            owner->ClearChildren();
            return;
        }

        Control::OnHoverChanged(host, hovered);
    }

private:
    DxUi::ScrollPanel* _owner               = nullptr;
    ScrollPanelReentrancyProbeState* _state = nullptr;
};

void TestMenuBarLayoutCacheRecomputesHitRectsAfterLayoutInvalidations()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* menu = root->AddChild<MenuBar>();
    menu->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 28.0f));
    menu->SetItems({
        MenuBarItem{.text = L"File", .mnemonic = L'F', .enabled = true},
        MenuBarItem{.text = L"Edit", .mnemonic = L'E', .enabled = true},
        MenuBarItem{.text = L"View", .mnemonic = L'V', .enabled = true},
    });
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 40.0f));
    host.SetRoot(std::move(root));

    RECT firstRectPx{};
    RECT secondRectPx{};
    Require(menu->TryGetItemScreenRect(host, 0u, firstRectPx), "MenuBar exposes initial first item screen rect");
    Require(menu->TryGetItemScreenRect(host, 1u, secondRectPx), "MenuBar exposes initial second item screen rect");

    const float firstCenterX  = static_cast<float>(firstRectPx.left + firstRectPx.right) * 0.5f;
    const float secondCenterX = static_cast<float>(secondRectPx.left + secondRectPx.right) * 0.5f;
    Require(menu->HitTestPoint(host, MakePointDip(D2D1::Point2F(firstCenterX, 12.0f))).value_or(SIZE_MAX) == 0u,
            "MenuBar initial cached hit rect resolves the first item");
    Require(menu->HitTestPoint(host, MakePointDip(D2D1::Point2F(secondCenterX, 12.0f))).value_or(SIZE_MAX) == 1u,
            "MenuBar initial cached hit rect resolves the second item");

    menu->SetItems({
        MenuBarItem{.text = L"Extremely wide file menu item", .mnemonic = L'F', .enabled = true},
        MenuBarItem{.text = L"Edit", .mnemonic = L'E', .enabled = true},
        MenuBarItem{.text = L"View", .mnemonic = L'V', .enabled = true},
    });
    RECT widenedFirstRectPx{};
    RECT shiftedSecondRectPx{};
    Require(menu->TryGetItemScreenRect(host, 0u, widenedFirstRectPx), "MenuBar exposes widened first item screen rect");
    Require(menu->TryGetItemScreenRect(host, 1u, shiftedSecondRectPx), "MenuBar exposes shifted second item screen rect");
    Require((widenedFirstRectPx.right - widenedFirstRectPx.left) > (firstRectPx.right - firstRectPx.left) + 20,
            "SetItems invalidation recomputes cached item width");
    Require(shiftedSecondRectPx.left > secondRectPx.left + 20, "SetItems invalidation recomputes following item hit rects");

    menu->SetBounds(D2D1::RectF(40.0f, 0.0f, 400.0f, 28.0f));
    RECT movedFirstRectPx{};
    Require(menu->TryGetItemScreenRect(host, 0u, movedFirstRectPx), "MenuBar exposes moved first item screen rect");
    Require(movedFirstRectPx.left > widenedFirstRectPx.left + 20, "bounds invalidation recomputes cached item x positions");

    menu->SetFlowDirection(FlowDirection::RightToLeft);
    RECT rtlFirstRectPx{};
    Require(menu->TryGetItemScreenRect(host, 0u, rtlFirstRectPx), "MenuBar exposes RTL first item screen rect");
    Require(rtlFirstRectPx.right > 360, "RTL invalidation recomputes cached item rects from the right edge");
}

void TestTabControlHeaderCacheRecomputesRectsAfterLayoutInvalidations()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tabs = root->AddChild<TabControl>();
    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 140.0f));
    tabs->AddTab<Panel>(L"Alpha");
    tabs->AddTab<Panel>(L"Bravo");
    tabs->AddTab<Panel>(L"Charlie");
    tabs->AddTab<Panel>(L"Delta");
    tabs->AddTab<Panel>(L"Echo");
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 140.0f));
    host.SetRoot(std::move(root));

    const auto widthOf = [](const D2D1_RECT_F& rect) noexcept { return rect.right - rect.left; };

    Require(tabs->DebugHasOverflowButtons(), "narrow TabControl test setup starts with overflow buttons");
    const D2D1_RECT_F initialFirstRect = tabs->DebugGetTabRect(0u);
    RequireRectHasArea(initialFirstRect, "initial cached first-tab rect has area");

    Require(tabs->OnMouseWheel(host, D2D1::Point2F(110.0f, 12.0f), -120.0f, 0u), "TabControl header wheel scroll is handled");
    Require(tabs->DebugGetHeaderScrollOffsetDip() > 1.0f, "TabControl header wheel changes the scroll offset");
    const D2D1_RECT_F scrolledFirstRect = tabs->DebugGetTabRect(0u);
    Require(scrolledFirstRect.left < initialFirstRect.left - 1.0f, "scroll invalidation recomputes cached tab rect positions");

    const float beforeRenameWidth = widthOf(tabs->DebugGetTabRect(1u));
    tabs->SetTabTitle(1u, L"Bravo tab title that is deliberately much wider than the cached width");
    const float afterRenameWidth = widthOf(tabs->DebugGetTabRect(1u));
    Require(afterRenameWidth > beforeRenameWidth + 8.0f, "title invalidation recomputes cached tab rect widths");

    const float beforeClosableWidth = widthOf(tabs->DebugGetTabRect(2u));
    tabs->SetTabClosable(2u, true);
    const float afterClosableWidth = widthOf(tabs->DebugGetTabRect(2u));
    Require(afterClosableWidth > beforeClosableWidth + 4.0f, "closability invalidation recomputes cached tab rect widths");
    RequireRectHasArea(tabs->DebugGetCloseButtonRect(2u), "closability invalidation exposes the close-button rect");

    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 1200.0f, 140.0f));
    Require(! tabs->DebugHasOverflowButtons(), "bounds invalidation recomputes overflow state for a wide header");

    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 140.0f));
    tabs->SetFlowDirection(FlowDirection::RightToLeft);
    const D2D1_RECT_F rtlFirstRect = tabs->DebugGetTabRect(0u);
    Require(rtlFirstRect.right > 180.0f, "RTL invalidation recomputes cached tab rects from the right edge");
}

void TestTabControlLayoutRevalidatesPagesAfterCallbacks()
{
    using namespace DxUi;

    class BoundsCallbackPage final : public Panel
    {
    public:
        explicit BoundsCallbackPage(std::function<void()> onBoundsChanged) : _onBoundsChanged(std::move(onBoundsChanged))
        {
        }

    protected:
        void OnBoundsChanged() noexcept override
        {
            std::function<void()> callback = std::move(_onBoundsChanged);
            if (callback)
            {
                callback();
            }
        }

    private:
        std::function<void()> _onBoundsChanged;
    };

    class DetachingHiddenPage final : public Panel
    {
    public:
        explicit DetachingHiddenPage(std::function<void(DetachingHiddenPage&)> onHidden) : _onHidden(std::move(onHidden))
        {
        }

        void DetachInto(std::unique_ptr<Control>& outPage)
        {
            Panel* const parent = GetParent();
            if (! parent)
            {
                return;
            }
            for (std::unique_ptr<Control>& child : parent->GetChildren())
            {
                if (child.get() == this)
                {
                    outPage = std::move(child);
                    Reparent(nullptr, nullptr);
                    return;
                }
            }
        }

    protected:
        void OnHidden() noexcept override
        {
            std::function<void(DetachingHiddenPage&)> callback = std::move(_onHidden);
            if (callback)
            {
                callback(*this);
            }
        }

    private:
        std::function<void(DetachingHiddenPage&)> _onHidden;
    };

    // The selected page's bounds callback removes an earlier sibling. Layout must re-find the selected page
    // instead of applying its old index to the sibling that shifted into that slot.
    {
        WindowHost host;
        auto root  = std::make_unique<Panel>();
        auto* tabs = root->AddChild<TabControl>();
        tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 180.0f));
        tabs->AddTab<Panel>(L"Alpha");
        size_t callbackCount = 0u;
        auto* beta           = tabs->AddTab<BoundsCallbackPage>(L"Bravo",
                                                                [&]
        {
            ++callbackCount;
            tabs->RemoveTab(0u);
        });
        auto* charlie        = tabs->AddTab<Panel>(L"Charlie");
        tabs->AddTab<Panel>(L"Delta");
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 180.0f));
        host.SetRoot(std::move(root));

        tabs->SetSelectedIndex(1u);
        Require(callbackCount == 1u, "selected page bounds callback runs once");
        Require(tabs->GetTabCount() == 3u && tabs->GetSelectedIndex() == 0u, "removing the earlier sibling preserves the selected page identity");
        Require(tabs->GetSelectedPage() == beta && beta->IsVisible(), "the selected page remains visible after its index shifts");
        Require(! charlie->IsVisible(), "the page shifted into the selected page's old index is not made visible");
    }

    // A newly added page can hide itself, detach while remaining alive, and replace the host root. AddTab must
    // detect that its owner died during SyncLayout and avoid touching _tabs or returning the detached page as a child.
    {
        WindowHost host;
        auto root  = std::make_unique<Panel>();
        auto* tabs = root->AddChild<TabControl>();
        tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 180.0f));
        tabs->AddTab<Panel>(L"Alpha");
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 180.0f));
        host.SetRoot(std::move(root));

        std::unique_ptr<Control> detachedPage;
        auto replacementRoot        = std::make_unique<Panel>();
        Panel* const replacementPtr = replacementRoot.get();
        auto* added                 = tabs->AddTab<DetachingHiddenPage>(L"Bravo",
                                                                        [&](DetachingHiddenPage& page)
        {
            page.DetachInto(detachedPage);
            host.SetRoot(std::move(replacementRoot));
        });

        Require(added == nullptr, "AddTab returns no page when its TabControl is destroyed during layout");
        Require(detachedPage != nullptr, "the hidden page can outlive the destroyed TabControl");
        Require(host.GetRoot() == replacementPtr, "the reentrant callback replaced the host root");
    }

    // Hiding the previous page can resize the tabs through a nested layout. The outer pass must use the new
    // content rectangle when it reaches the newly selected page.
    {
        WindowHost host;
        auto root  = std::make_unique<Panel>();
        auto* tabs = root->AddChild<TabControl>();
        tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 180.0f));
        size_t callbackCount = 0u;
        tabs->AddTab<DetachingHiddenPage>(L"Alpha",
                                          [&](DetachingHiddenPage&)
        {
            ++callbackCount;
            tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 720.0f, 280.0f));
        });
        auto* beta = tabs->AddTab<Panel>(L"Bravo");
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 720.0f, 280.0f));
        host.SetRoot(std::move(root));
        tabs->SetSelectedIndex(1u);
        Require(callbackCount == 1u, "hiding the old page runs the nested resize once");
        Require(beta->GetBounds().right == 720.0f && beta->GetBounds().bottom == 280.0f, "the outer layout preserves the bounds computed by the nested resize");
    }
}

void TestTabControlBodyDragReleaseOverCloseButtonDoesNotCloseTab()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tabs = root->AddChild<TabControl>();
    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    tabs->AddTab<Panel>(L"Alpha");
    tabs->AddTab<Panel>(L"Bravo");
    tabs->AddTab<Panel>(L"Charlie");
    tabs->SetTabClosable(0u, true);
    tabs->SetTabClosable(1u, true);
    tabs->SetTabClosable(2u, true);
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    host.SetRoot(std::move(root));

    size_t closeRequestedCount = 0u;
    std::optional<size_t> closeRequestedIndex;
    size_t closedCount = 0u;
    tabs->SetOnTabCloseRequested([&](size_t index)
    {
        ++closeRequestedCount;
        closeRequestedIndex = index;
        return false;
    });
    tabs->SetOnTabClosed([&](size_t) { ++closedCount; });

    const auto centerOf = [](const D2D1_RECT_F& rect) noexcept { return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f); };

    const D2D1_RECT_F firstTabRect   = tabs->DebugGetTabRect(0u);
    const D2D1_RECT_F thirdCloseRect = tabs->DebugGetCloseButtonRect(2u);
    RequireRectHasArea(firstTabRect, "TabControl test exposes the first tab body rect");
    RequireRectHasArea(thirdCloseRect, "TabControl test exposes the third tab close-button rect");

    const D2D1_POINT_2F firstTabBodyPoint = D2D1::Point2F(firstTabRect.left + 12.0f, (firstTabRect.top + firstTabRect.bottom) * 0.5f);
    const D2D1_POINT_2F thirdClosePoint   = centerOf(thirdCloseRect);

    Require(tabs->OnMouseDown(host, firstTabBodyPoint, false, 0u), "TabControl handles body mouse-down before a tab drag");
    Require(tabs->OnMouseMove(host, thirdClosePoint, 0u), "TabControl handles drag hover over another tab's close button");
    Require(! tabs->OnMouseUp(host, thirdClosePoint, false, 0u), "TabControl body-started drag release over a close button is not a close action");

    Require(tabs->GetTabCount() == 3u, "TabControl body-started drag release over a close button leaves all tabs open");
    Require(closeRequestedCount == 0u && ! closeRequestedIndex.has_value(), "TabControl does not request close after a body-started drag");
    Require(closedCount == 0u, "TabControl does not close a tab after a body-started drag");
}

void TestTabControlReorderingPolicyPreservesStableHostIndices()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tabs = root->AddChild<TabControl>();
    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    tabs->AddTab<Panel>(L"Folder");
    tabs->AddTab<Panel>(L"Preview");
    tabs->AddTab<Panel>(L"Terminal");
    tabs->SetTabReorderingEnabled(false);
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    host.SetRoot(std::move(root));

    Require(! tabs->IsTabReorderingEnabled(), "fixed-index TabControl host disables pointer reordering");
    const std::array expectedTitles{std::wstring_view(L"Folder"), std::wstring_view(L"Preview"), std::wstring_view(L"Terminal")};
    const auto centerOf = [](const D2D1_RECT_F& rect) noexcept { return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f); };

    for (size_t fromIndex = 0u; fromIndex < tabs->GetTabCount(); ++fromIndex)
    {
        for (size_t toIndex = 0u; toIndex < tabs->GetTabCount(); ++toIndex)
        {
            if (fromIndex == toIndex)
            {
                continue;
            }
            const D2D1_POINT_2F fromPoint = centerOf(tabs->DebugGetTabRect(fromIndex));
            const D2D1_POINT_2F toPoint   = centerOf(tabs->DebugGetTabRect(toIndex));
            Require(tabs->OnMouseDown(host, fromPoint, false, 0u), "fixed-index TabControl handles drag-start selection");
            Require(tabs->OnMouseMove(host, toPoint, 0u), "fixed-index TabControl handles cross-tab pointer movement");
            static_cast<void>(tabs->OnMouseUp(host, toPoint, false, 0u));
            Require(tabs->GetSelectedIndex() == fromIndex, "fixed-index TabControl pointer movement preserves the selected semantic page");
            for (size_t index = 0u; index < expectedTitles.size(); ++index)
            {
                Require(tabs->GetTabTitle(index) == expectedTitles[index],
                        "fixed-index TabControl preserves every semantic tab index across drag permutations");
            }
        }
    }

    tabs->SetTabVisible(1u, false);
    Require(! tabs->IsTabVisible(1u) && tabs->GetTabTitle(2u) == L"Terminal", "hidden fixed-index tab retains later semantic indices");
    tabs->SetSelectedIndex(2u);
    Require(tabs->GetSelectedIndex() == 2u && tabs->OnKeyDown(host, VK_LEFT, 0u) && tabs->GetSelectedIndex() == 0u,
            "fixed-index TabControl keyboard navigation skips a hidden semantic tab");
}

void TestTabControlReorderingReportsStableMove()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* tabs = root->AddChild<TabControl>();
    tabs->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    tabs->AddTab<Panel>(L"Alpha");
    tabs->AddTab<Panel>(L"Bravo");
    tabs->AddTab<Panel>(L"Charlie");
    root->SetBounds(D2D1::RectF(0.0f, 0.0f, 640.0f, 180.0f));
    host.SetRoot(std::move(root));

    std::optional<std::pair<size_t, size_t>> move;
    tabs->SetOnTabReordered([&](size_t fromIndex, size_t toIndex) noexcept { move = std::pair{fromIndex, toIndex}; });
    const D2D1_RECT_F first  = tabs->DebugGetTabRect(0u);
    const D2D1_RECT_F second = tabs->DebugGetTabRect(1u);
    const D2D1_POINT_2F from = D2D1::Point2F((first.left + first.right) * 0.5f, (first.top + first.bottom) * 0.5f);
    const D2D1_POINT_2F to   = D2D1::Point2F(second.left + 2.0f, (second.top + second.bottom) * 0.5f);
    Require(tabs->OnMouseDown(host, from, false, 0u), "reorder-reporting TabControl accepts the drag start");
    Require(tabs->OnMouseMove(host, to, 0u), "reorder-reporting TabControl accepts the drag move");
    static_cast<void>(tabs->OnMouseUp(host, to, false, 0u));
    Require(move == std::pair<size_t, size_t>{0u, 1u}, "TabControl reports the exact stable from/to move");
    Require(tabs->GetTabTitle(0u) == L"Bravo" && tabs->GetTabTitle(1u) == L"Alpha", "TabControl reorder notification matches the committed page order");
}

void TestToggleMouseActivationOnlyFiresToggledCallbackWithUpdatedState()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* toggle = root->AddChild<Toggle>(L"Compare subdirectories");
    toggle->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));

    size_t clickCount    = 0u;
    size_t toggledCount  = 0u;
    bool callbackChecked = false;
    toggle->SetOnClick([&clickCount] { ++clickCount; });
    toggle->SetOnToggled([&](bool checked)
    {
        ++toggledCount;
        callbackChecked = checked;
    });

    host.SetRoot(std::move(root));

    Require(toggle->OnMouseDown(host, D2D1::Point2F(32.0f, 20.0f), false, 0), "toggle handles mouse-down before activation");
    Require(toggle->OnMouseUp(host, D2D1::Point2F(32.0f, 20.0f), false, 0), "toggle handles mouse-up activation");
    Require(toggle->IsChecked(), "toggle activation updates checked state");
    Require(toggledCount == 1u, "toggle activation fires one toggled callback");
    Require(callbackChecked, "toggle callback observes the updated checked state");
    Require(clickCount == 0u, "toggle activation no longer double-fires the button click callback");
}

void TestToggleMouseActivationCanReplaceRootSafely()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* toggle = root->AddChild<Toggle>(L"Compare subdirectories");
    toggle->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));

    bool toggled = false;
    toggle->SetOnToggled([&](bool)
    {
        toggled = true;
        host.SetRoot(std::make_unique<Panel>());
    });

    host.SetRoot(std::move(root));

    Require(toggle->OnMouseDown(host, D2D1::Point2F(32.0f, 20.0f), false, 0), "toggle handles mouse-down before root replacement");
    Require(toggle->OnMouseUp(host, D2D1::Point2F(32.0f, 20.0f), false, 0), "toggle survives root replacement during mouse-up activation");
    Require(toggled, "toggle callback ran before replacing the root");
    Require(host.GetRoot() != nullptr, "toggle callback can replace the host root safely");
}

void TestMenuBarActivationCanReplaceRootSafely()
{
    using namespace DxUi;

    WindowHost host;
    auto root     = std::make_unique<Panel>();
    auto* menuBar = root->AddChild<MenuBar>();
    menuBar->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));
    menuBar->SetItems({MenuBarItem{.text = L"File", .mnemonic = L'F', .enabled = true}});

    bool callbackInvoked = false;
    menuBar->SetOnOpenItem([&](size_t, POINT, bool)
    {
        callbackInvoked = true;
        host.SetRoot(std::make_unique<Panel>());
    });

    host.SetRoot(std::move(root));
    const bool activated = menuBar->ActivateItem(host, 0u, false);

    Require(activated, "MenuBar reports the item activation that replaced its host root");
    Require(callbackInvoked, "MenuBar open callback runs before replacing the host root");
    Require(host.GetRoot() != nullptr, "MenuBar callback can replace the host root without post-callback access");
}

void TestColorSwatchStoresConfiguredArgbAndEmptyState()
{
    using namespace DxUi;

    ColorSwatch swatch;
    Require(! swatch.GetSwatchValue().has_value(), "color swatch starts without a configured color");

    swatch.SetSwatchValue(0x8044AA33u);
    Require(swatch.GetSwatchValue().has_value(), "color swatch stores an assigned color");
    Require(swatch.GetSwatchValue().value() == 0x8044AA33u, "color swatch preserves the assigned ARGB value");

    swatch.SetSwatchValue(std::nullopt);
    Require(! swatch.GetSwatchValue().has_value(), "color swatch clears back to the empty state");
}

[[nodiscard]] bool ComboItemsContainValue(const DxUi::ComboBox& combo, std::wstring_view value) noexcept
{
    for (const DxUi::ComboBox::Item& item : combo.GetItems())
    {
        if (item.value == value)
        {
            return true;
        }
    }
    return false;
}

void TestTagPickerWrapsBadgesInsideInputFrame()
{
    using namespace DxUi;

    WindowHost host;
    auto root         = std::make_unique<Panel>();
    auto* rootPanel   = root.get();
    auto* tagPicker   = root->AddChild<TagPicker>();
    const float width = 220.0f;
    tagPicker->SetOptions(L"All owners", {L"RedSalamander", L"RedSalamanderMonitor", L"FlipSequentialDiscard", L"ViewerText"});
    tagPicker->SetSelectedValues({L"RedSalamander", L"RedSalamanderMonitor", L"FlipSequentialDiscard"});
    host.SetRoot(std::move(root));
    rootPanel->SetBounds(D2D1::RectF(0.0f, 0.0f, width, 160.0f));

    const float preferredHeight = tagPicker->GetPreferredHeightDip(width);
    Require(preferredHeight > 32.0f, "tag picker grows taller than one row when selected badges wrap");
    tagPicker->SetBounds(D2D1::RectF(0.0f, 0.0f, width, preferredHeight));

    const D2D1_RECT_F pickerBounds = tagPicker->GetBounds();
    const auto rectInside          = [](const D2D1_RECT_F& inner, const D2D1_RECT_F& outer) noexcept
    { return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right && inner.bottom <= outer.bottom; };

    Require(tagPicker->DebugGetLaidOutDisplayTagCount() == 3u, "tag picker lays out every visible selected badge");
    for (size_t index = 0u; index < tagPicker->DebugGetLaidOutDisplayTagCount(); ++index)
    {
        const D2D1_RECT_F tagRect = tagPicker->DebugGetDisplayTagRect(index);
        RequireRectHasArea(tagRect, "tag picker badge rect has area");
        Require(rectInside(tagRect, pickerBounds), "tag picker badge rect remains inside the input frame");
    }

    const D2D1_RECT_F inputRect = tagPicker->DebugGetInputRect();
    RequireRectHasArea(inputRect, "tag picker embedded input rect has area");
    Require(rectInside(inputRect, pickerBounds), "tag picker embedded input remains inside the input frame");
    Require(inputRect.top > tagPicker->DebugGetDisplayTagRect(0u).top, "tag picker wraps the embedded input to a later row when badges need width");
}

void TestTagPickerSuggestionsTrackSelectedBadges()
{
    using namespace DxUi;

    TagPicker picker;
    picker.SetOptions(L"All owners", {L"Alpha", L"Beta", L"Gamma"});
    ComboBox* combo = picker.DebugGetEmbeddedCombo();
    Require(combo != nullptr, "tag picker exposes embedded combo for tests");

    Require(ComboItemsContainValue(*combo, L"All owners"), "tag picker initially offers the all option");
    Require(ComboItemsContainValue(*combo, L"Alpha"), "tag picker initially offers concrete options");

    picker.SetSelectedValues({L"Alpha"});
    Require(! ComboItemsContainValue(*combo, L"All owners"), "tag picker hides all option when a concrete badge is selected");
    Require(! ComboItemsContainValue(*combo, L"Alpha"), "tag picker removes selected badge from suggestions");
    Require(ComboItemsContainValue(*combo, L"Beta"), "tag picker keeps unselected badges in suggestions");

    Require(picker.RemoveDisplayTag(0u), "tag picker removes the selected badge");
    Require(ComboItemsContainValue(*combo, L"All owners"), "tag picker restores all option after the last badge is removed");
    Require(ComboItemsContainValue(*combo, L"Alpha"), "tag picker restores removed badge to suggestions");

    Require(picker.SelectOption(L"All owners"), "tag picker selects all option");
    Require(picker.GetDisplayTagCount() == 1u && picker.GetDisplayTagText(0u) == L"All owners", "tag picker collapses all selected values to all badge");
    Require(! ComboItemsContainValue(*combo, L"All owners"), "tag picker hides all option while all badge is active");
    Require(ComboItemsContainValue(*combo, L"Beta"), "tag picker keeps concrete suggestions available to replace all");

    Require(picker.SelectOption(L"Beta"), "tag picker selects a concrete option while all is active");
    const std::span<const std::wstring> selectedValues = picker.GetSelectedValues();
    Require(selectedValues.size() == 1u && selectedValues[0] == L"Beta", "tag picker replaces all badge with the picked concrete badge");
    Require(picker.GetDisplayTagCount() == 1u && picker.GetDisplayTagText(0u) == L"Beta", "tag picker display shows only the picked concrete badge");
    Require(! ComboItemsContainValue(*combo, L"Beta"), "tag picker removes newly selected concrete badge from suggestions");
    Require(ComboItemsContainValue(*combo, L"Alpha"), "tag picker keeps other concrete badges in suggestions");
}

void TestTagPickerKeyboardNavigationCommitsFilteredSuggestionOnEnter()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* panel  = root.get();
    auto* picker = root->AddChild<TagPicker>();
    picker->SetOptions(L"All languages", {L"Beta", L"Binary", L"Bravo", L"Gamma"});
    picker->SetBounds(D2D1::RectF(0.0f, 0.0f, 260.0f, 40.0f));
    ComboBox* combo = picker->DebugGetEmbeddedCombo();
    Require(combo != nullptr, "tag picker exposes embedded combo for keyboard tests");

    host.SetRoot(std::move(root));
    panel->SetBounds(D2D1::RectF(0.0f, 0.0f, 300.0f, 80.0f));
    host.SetFocusControl(combo);
    picker->SetInputText(L"B");

    Require(combo->OnKeyDown(host, VK_DOWN, 0), "tag picker down arrow opens filtered suggestions");
    Require(combo->DebugIsPopupOpen(), "tag picker keeps filtered suggestions open after first down arrow");
    Require(picker->GetSelectedValues().empty(), "tag picker does not add a badge when opening suggestions");

    Require(combo->OnKeyDown(host, VK_DOWN, 0), "tag picker down arrow changes highlighted filtered suggestion");
    Require(picker->GetSelectedValues().empty(), "tag picker arrow navigation does not add a badge");

    Require(combo->OnKeyDown(host, VK_RETURN, 0), "tag picker enter commits highlighted filtered suggestion");
    Require(! combo->DebugIsPopupOpen(), "tag picker closes suggestions after enter commits");
    const std::span<const std::wstring> selectedValues = picker->GetSelectedValues();
    Require(selectedValues.size() == 1u && selectedValues[0] == L"Binary", "tag picker enter adds the highlighted filtered badge");
    Require(! ComboItemsContainValue(*combo, L"Binary"), "tag picker removes the keyboard-committed badge from suggestions");
    Require(ComboItemsContainValue(*combo, L"Beta"), "tag picker keeps other matching badges in suggestions");
}

void TestToggleRightClickInvokesContextMenuWithoutChangingState()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* toggle = root->AddChild<Toggle>(L"Ascending");
    toggle->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 40.0f));

    size_t toggledCount = 0u;
    toggle->SetOnToggled([&](bool) { ++toggledCount; });

    RecordingContextMenuInvocation contextMenu;
    toggle->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 56.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, 0, MAKELPARAM(180, 20), handled));
    Require(handled, "toggle right-click is handled");
    Require(contextMenu.count == 1u, "toggle right-click invokes one context menu");
    Require(! contextMenu.lastKeyboardInvocation, "toggle right-click reports pointer invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{180, 20}, "toggle right-click uses the hit point as its screen anchor");
    Require(! toggle->IsChecked(), "toggle right-click does not change checked state");
    Require(toggledCount == 0u, "toggle right-click does not fire the toggled callback");
    Require(host.GetFocusControl() == toggle, "toggle right-click moves focus to the toggle");
}

void TestCheckboxRightClickInvokesContextMenuWithoutChangingState()
{
    using namespace DxUi;

    WindowHost host;
    auto root      = std::make_unique<Panel>();
    auto* checkbox = root->AddChild<Checkbox>(L"Selected");
    checkbox->SetBounds(D2D1::RectF(0.0f, 0.0f, 220.0f, 32.0f));

    size_t toggledCount = 0u;
    checkbox->SetOnToggled([&](bool) { ++toggledCount; });

    RecordingContextMenuInvocation contextMenu;
    checkbox->SetOnContextMenu([&](POINT point, bool keyboardInvocation) { contextMenu.Record(point, keyboardInvocation); });

    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 48.0f));

    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_RBUTTONDOWN, 0, MAKELPARAM(28, 16), handled));
    Require(handled, "checkbox right-click is handled");
    Require(contextMenu.count == 1u, "checkbox right-click invokes one context menu");
    Require(! contextMenu.lastKeyboardInvocation, "checkbox right-click reports pointer invocation");
    RequirePointNear(contextMenu.lastPoint, POINT{28, 16}, "checkbox right-click uses the hit point as its screen anchor");
    Require(! checkbox->IsChecked(), "checkbox right-click does not change checked state");
    Require(toggledCount == 0u, "checkbox right-click does not fire the toggled callback");
    Require(host.GetFocusControl() == checkbox, "checkbox right-click moves focus to the checkbox");
}

void TestControlContextMenuCallbackCanReplaceItsOwnCallable()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Context menu");
    std::wstring observed;
    observed.reserve(16u);
    button->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 36.0f));
    host.SetRoot(std::move(root));
    button->SetOnContextMenu([button, &observed, payload = std::make_shared<std::wstring>(L"alive")](POINT, bool)
    {
        button->SetOnContextMenu({});
        observed = *payload;
    });

    Require(button->OnContextMenu(host, true, {}), "Control dispatches its registered context-menu callback");
    Require(observed == L"alive", "the active context-menu callable survives resetting its member");
}

void TestControlAccessibleInvokeCallbackCanDestroyItsOwner()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Invoke");
    std::wstring observed;
    observed.reserve(16u);
    host.SetRoot(std::move(root));
    button->SetAccessibleInvoke([&host, &observed, payload = std::make_shared<std::wstring>(L"alive")](ControlHost&)
    {
        host.SetRoot({});
        observed = *payload;
    });

    Require(button->InvokeAccessible(host), "Control dispatches its accessible invoke callback");
    Require(! host.GetRoot(), "accessible invoke callback destroys its owning control tree");
    Require(observed == L"alive", "the active invoke callable survives owner destruction");
}

void TestControlAccessibleInvokeResultRetainsMutableStateAndReturnsTheCallbackResult()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Invoke result");
    host.SetRoot(std::move(root));

    std::vector<size_t> observed;
    button->SetAccessibleInvokeResult([count = size_t{0u}, &observed](ControlHost&) mutable
    {
        observed.push_back(++count);
        return count == 1u ? E_ACCESSDENIED : S_OK;
    });

    Require(button->SupportsAccessibleInvoke(), "the result callback exposes accessible Invoke");
    Require(button->InvokeAccessibleResult(host) == E_ACCESSDENIED, "the accessible callback's failure HRESULT is preserved");
    Require(button->InvokeAccessibleResult(host) == S_OK, "the accessible callback's success HRESULT is preserved");
    Require(observed == std::vector<size_t>{1u, 2u}, "the retained result callback keeps mutable closure state across invocations");
    Require(button->InvokeAccessible(host), "the existing bool invocation API remains a compatible success wrapper");
    button->SetAccessibleInvokeResult([](ControlHost&) -> HRESULT { throw std::bad_alloc{}; });
    Require(button->InvokeAccessibleResult(host) == E_OUTOFMEMORY, "allocation failure in the callback becomes a failed COM result");
    button->SetAccessibleInvokeResult([](ControlHost&) -> HRESULT { throw std::runtime_error("callback failed"); });
    Require(button->InvokeAccessibleResult(host) == E_FAIL, "a named application exception becomes a failed COM result");
    button->SetAccessibleInvoke({});
    Require(! button->SupportsAccessibleInvoke() && button->InvokeAccessibleResult(host) == UIA_E_NOTSUPPORTED,
            "clearing the compatible setter removes the canonical result callback");
}

void TestColorSwatchKeyboardCallbackCanDestroyItsOwner()
{
    using namespace DxUi;

    WindowHost host;
    auto root                             = std::make_unique<Panel>();
    auto* swatch                          = root->AddChild<ColorSwatch>(0xFF336699u);
    size_t callbackCount                  = 0u;
    uint64_t invalidationsAfterRetirement = 0u;
    host.SetRoot(std::move(root));
    swatch->SetOnClick([&host, &callbackCount, &invalidationsAfterRetirement]
    {
        ++callbackCount;
        host.SetRoot({});
        invalidationsAfterRetirement = host.DebugGetInvalidateCount();
    });

    const bool handled = swatch->OnKeyDown(host, VK_SPACE, 0u);
    Require(handled && callbackCount == 1u && ! host.GetRoot(), "ColorSwatch consumes keyboard activation when its callback destroys the owner");
    Require(host.DebugGetInvalidateCount() == invalidationsAfterRetirement, "a retired ColorSwatch does not request another repaint after keyboard activation");
}

void TestAccessibleInvokeReplacementCanRetireItsOwnerDuringOldCaptureCleanup()
{
    using namespace DxUi;

    WindowHost host;
    auto root                       = std::make_unique<Panel>();
    auto* button                    = root->AddChild<Button>(L"Replace accessible invoke");
    bool retiredByOldCaptureCleanup = false;
    host.SetRoot(std::move(root));
    button->SetAccessibleInvoke([payload = std::shared_ptr<int>(new int(1),
                                                                [&host, &retiredByOldCaptureCleanup](int* value) noexcept
    {
        delete value;
        retiredByOldCaptureCleanup = true;
        host.SetRoot({});
    })](ControlHost&) { static_cast<void>(*payload); });

    button->SetAccessibleInvoke([](ControlHost&) {});
    Require(retiredByOldCaptureCleanup && ! host.GetRoot(), "releasing the replaced accessible callback's sole payload owner can retire its control tree");
}

void TestPageIndicatorSelectionCallbackCanReplaceItsOwnCallable()
{
    using namespace DxUi;

    WindowHost host;
    auto root       = std::make_unique<Panel>();
    auto* indicator = root->AddChild<PageIndicator>();
    std::wstring observed;
    observed.reserve(16u);
    indicator->SetPageCount(3u);
    host.SetRoot(std::move(root));
    indicator->SetOnSelected([indicator, &observed, payload = std::make_shared<std::wstring>(L"alive")](uint32_t)
    {
        indicator->SetOnSelected({});
        observed = *payload;
    });

    Require(indicator->OnKeyDown(host, VK_RIGHT, 0u), "the next-page key dispatches selection");
    Require(indicator->GetSelectedIndex() == 1u, "selection updates before reporting the selected page");
    Require(observed == L"alive", "the active page-selection callable survives resetting its member");
}

void TestGridCheckboxCellClickTogglesThroughDelegate()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 360.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    CheckboxGridModel model(1u);
    model.SetRows({
        CheckboxGridModel::Row{.label = L"Alpha", .checked = false, .enabled = true},
        CheckboxGridModel::Row{.label = L"Beta", .checked = true, .enabled = true},
    });

    RecordingCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 1u);
    const D2D1_POINT_2F checkboxPoint =
        D2D1::Point2F((metrics.checkboxRect.left + metrics.checkboxRect.right) * 0.5f, (metrics.checkboxRect.top + metrics.checkboxRect.bottom) * 0.5f);

    Require(grid->OnMouseDown(host, checkboxPoint, false, 0), "grid checkbox click is handled");
    Require(delegate.toggleCount == 1u, "grid checkbox click notifies one toggle");
    Require(delegate.lastToggleRow == 0u && delegate.lastToggleColumn == 1u, "grid checkbox click targets the checkbox column");
    Require(delegate.lastToggleChecked, "grid checkbox click requests the checked state");
    Require(model.IsChecked(0u), "grid checkbox click updates the model state");
    Require(delegate.selectionChangedCount == 1u, "grid checkbox click also selects the row");
    Require(grid->GetSelectionModel().GetCount() == 1u && grid->GetSelectionModel().IsSelected(1u), "grid checkbox click keeps the hit row selected");
}

void TestDisabledGridCheckboxCellClickSelectsWithoutTogglingAndInvalidates()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    CheckboxGridModel model(1u);
    model.SetRows({CheckboxGridModel::Row{.label = L"Disabled", .checked = false, .enabled = false}});

    RecordingCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);
    host.SetFocusControl(grid);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 1u);
    const D2D1_POINT_2F checkboxPoint =
        D2D1::Point2F((metrics.checkboxRect.left + metrics.checkboxRect.right) * 0.5f, (metrics.checkboxRect.top + metrics.checkboxRect.bottom) * 0.5f);
    const uint64_t invalidateCountBefore = host.DebugGetInvalidateCount();

    Require(grid->OnMouseDown(host, checkboxPoint, false, 0), "disabled grid checkbox click is handled");
    Require(delegate.toggleCount == 0u, "disabled grid checkbox click does not notify a toggle");
    Require(! model.IsChecked(0u), "disabled grid checkbox click leaves model state unchanged");
    Require(delegate.selectionChangedCount == 1u, "disabled grid checkbox click still selects the row");
    Require(grid->GetSelectionModel().GetCount() == 1u && grid->GetSelectionModel().IsSelected(1u), "disabled grid checkbox click keeps the hit row selected");
    Require(host.DebugGetInvalidateCount() > invalidateCountBefore, "disabled grid checkbox click invalidates the selected row for repaint");

    delegate.selectionChangedCount                  = 0u;
    const uint64_t doubleClickInvalidateCountBefore = host.DebugGetInvalidateCount();
    Require(grid->OnMouseDoubleClick(host, checkboxPoint, false, 0), "disabled grid checkbox double-click is handled");
    Require(delegate.toggleCount == 0u, "disabled grid checkbox double-click still does not notify a toggle");
    Require(delegate.rowActivatedCount == 0u, "disabled grid checkbox double-click does not activate the row");
    Require(delegate.selectionChangedCount == 0u, "disabled grid checkbox double-click keeps the existing selection stable");
    Require(host.DebugGetInvalidateCount() > doubleClickInvalidateCountBefore, "disabled grid checkbox double-click invalidates the selected row for repaint");
}

void TestGridCheckboxCellTextClickDoesNotToggle()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    CheckboxGridModel model(1u);
    model.SetRows({CheckboxGridModel::Row{.label = L"Alpha", .checked = false, .enabled = true}});

    RecordingCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 1u);
    const D2D1_POINT_2F textPoint =
        D2D1::Point2F((metrics.textRect.left + metrics.textRect.right) * 0.5f, (metrics.textRect.top + metrics.textRect.bottom) * 0.5f);

    Require(grid->OnMouseDown(host, textPoint, false, 0), "grid checkbox-row text click is handled");
    Require(delegate.toggleCount == 0u, "grid text click inside a checkbox cell does not toggle the checkbox");
    Require(! model.IsChecked(0u), "grid text click leaves checkbox state unchanged");
    Require(delegate.selectionChangedCount == 1u, "grid text click still selects the row");
}

void TestGridSpaceTogglesActiveCheckboxColumnAcrossRows()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    CheckboxGridModel model(1u);
    model.SetRows({
        CheckboxGridModel::Row{.label = L"Alpha", .checked = false, .enabled = true},
        CheckboxGridModel::Row{.label = L"Beta", .checked = false, .enabled = true},
    });

    RecordingCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);
    host.SetFocusControl(grid);

    const GridCellLayoutMetrics row0Metrics = grid->GetCellLayoutMetrics(host, 0u, 1u);
    const D2D1_POINT_2F row0CheckboxPoint   = D2D1::Point2F((row0Metrics.checkboxRect.left + row0Metrics.checkboxRect.right) * 0.5f,
                                                            (row0Metrics.checkboxRect.top + row0Metrics.checkboxRect.bottom) * 0.5f);

    Require(grid->OnMouseDown(host, row0CheckboxPoint, false, 0), "initial grid checkbox click is handled");
    Require(model.IsChecked(0u), "initial checkbox click checks the first row");

    delegate.toggleCount       = 0u;
    delegate.lastToggleRow     = 0u;
    delegate.lastToggleColumn  = 0u;
    delegate.lastToggleChecked = false;

    Require(grid->OnKeyDown(host, VK_DOWN, 0), "grid down key moves to the next row");
    Require(grid->GetSelectionModel().IsSelected(2u), "grid down key moves selection to the second row");
    Require(grid->OnKeyDown(host, VK_SPACE, 0), "grid space key toggles the active checkbox column");
    Require(delegate.toggleCount == 1u, "grid space key notifies one checkbox toggle");
    Require(delegate.lastToggleRow == 1u && delegate.lastToggleColumn == 1u, "grid space key preserves the active checkbox column across rows");
    Require(delegate.lastToggleChecked, "grid space key requests the checked state");
    Require(model.IsChecked(1u), "grid space key updates the second-row checkbox state");
}

void TestGridSpaceOnDisabledCheckboxColumnIsHandledWithoutToggling()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 160.0f));

    CheckboxGridModel model(1u);
    model.SetRows({
        CheckboxGridModel::Row{.label = L"Alpha", .checked = false, .enabled = true},
        CheckboxGridModel::Row{.label = L"Beta", .checked = false, .enabled = false},
    });

    RecordingCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);
    host.SetFocusControl(grid);

    const GridCellLayoutMetrics row0Metrics = grid->GetCellLayoutMetrics(host, 0u, 1u);
    const D2D1_POINT_2F row0CheckboxPoint   = D2D1::Point2F((row0Metrics.checkboxRect.left + row0Metrics.checkboxRect.right) * 0.5f,
                                                            (row0Metrics.checkboxRect.top + row0Metrics.checkboxRect.bottom) * 0.5f);

    Require(grid->OnMouseDown(host, row0CheckboxPoint, false, 0), "initial enabled grid checkbox click is handled");
    Require(model.IsChecked(0u), "initial enabled checkbox click checks the first row");

    delegate.toggleCount       = 0u;
    delegate.lastToggleRow     = 0u;
    delegate.lastToggleColumn  = 0u;
    delegate.lastToggleChecked = false;

    Require(grid->OnKeyDown(host, VK_DOWN, 0), "grid down key moves to the disabled checkbox row");
    Require(grid->GetSelectionModel().IsSelected(2u), "grid down key selects the disabled checkbox row");
    Require(grid->OnKeyDown(host, VK_SPACE, 0), "grid space key is consumed on a disabled checkbox column");
    Require(delegate.toggleCount == 0u, "grid space key on a disabled checkbox does not notify a toggle");
    Require(! model.IsChecked(1u), "grid space key on a disabled checkbox leaves the model state unchanged");
    Require(grid->GetSelectionModel().IsSelected(2u), "grid space key on a disabled checkbox preserves the selected row");
}

void TestDedicatedCheckboxColumnCentersIndicatorAndToggles()
{
    using namespace DxUi;

    WindowHost host;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 120.0f));

    DedicatedCheckboxColumnGridModel model;
    DedicatedCheckboxGridDelegate delegate(model);
    grid->SetModel(&model);
    grid->SetDelegate(&delegate);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    Require(metrics.hasCheckbox, "dedicated checkbox column reports checkbox presence");
    Require(! metrics.hasIcon, "dedicated checkbox column does not fabricate icon presence");
    Require(! metrics.hasBadge, "dedicated checkbox column does not fabricate badge presence");
    RequireRectHasArea(metrics.checkboxRect, "dedicated checkbox column checkbox rect has area");
    Require(metrics.textRect.right <= metrics.textRect.left + 0.5f, "dedicated checkbox column collapses the text rect");

    const float checkboxCenterX = (metrics.checkboxRect.left + metrics.checkboxRect.right) * 0.5f;
    const float cellCenterX     = (metrics.cellRect.left + metrics.cellRect.right) * 0.5f;
    RequireFloatNear(checkboxCenterX, cellCenterX, 1.0f, "dedicated checkbox indicator is centered within the column");

    const D2D1_POINT_2F checkboxPoint = D2D1::Point2F(checkboxCenterX, (metrics.checkboxRect.top + metrics.checkboxRect.bottom) * 0.5f);
    Require(grid->OnMouseDown(host, checkboxPoint, false, 0), "dedicated checkbox click is handled");
    Require(delegate.toggleCount == 1u, "dedicated checkbox click notifies one toggle");
    Require(delegate.lastToggleRow == 0u && delegate.lastToggleColumn == 0u, "dedicated checkbox click targets the dedicated column");
    Require(delegate.lastToggleChecked, "dedicated checkbox click requests the checked state");
    Require(model.IsChecked(), "dedicated checkbox click updates the model state");

    delegate.toggleCount       = 0u;
    delegate.lastToggleRow     = 0u;
    delegate.lastToggleColumn  = 0u;
    delegate.lastToggleChecked = true;
    host.SetFocusControl(grid);
    Require(grid->OnKeyDown(host, VK_SPACE, 0), "space toggles the dedicated checkbox column");
    Require(delegate.toggleCount == 1u, "space notifies one dedicated checkbox toggle");
    Require(delegate.lastToggleRow == 0u && delegate.lastToggleColumn == 0u, "space preserves the dedicated checkbox column");
    Require(! delegate.lastToggleChecked, "space requests the unchecked state from the dedicated checkbox column");
    Require(! model.IsChecked(), "space updates the dedicated checkbox model state");
}

void TestToggleMetricsMatchPreferencesWidthBudget()
{
    using namespace DxUi;

    Toggle toggle;
    toggle.SetStateLabels(L"Off", L"Pretty");
    toggle.SetBounds(D2D1::RectF(0.0f, 0.0f, 90.0f, 28.0f));

    const ToggleLayoutMetrics metrics = toggle.GetLayoutMetrics();
    RequireFloatNear(metrics.trackRect.right - metrics.trackRect.left, 34.0f, 0.0001f, "toggle track width matches shared preferences sizing budget");
    RequireFloatNear(metrics.trackRect.right, 85.0f, 0.0001f, "toggle track reserves the expected trailing padding inside a 90-dip row");
    RequireFloatNear(metrics.textRect.left, 7.0f, 0.0001f, "toggle text starts after the shared left padding");
    RequireFloatNear(metrics.textRect.right, 43.0f, 0.0001f, "toggle text rect preserves the expected room before the track");
}

void TestMnemonicTextIndexFindsFirstCaseInsensitiveMatch()
{
    using DxUi::FindMnemonicTextIndex;

    const auto match = FindMnemonicTextIndex(L"Find Files", L'f');
    Require(match.has_value() && match.value() == 0u, "mnemonic display helper finds first case-insensitive match");
}

void TestMnemonicTextIndexReturnsNoMatchWhenAbsent()
{
    using DxUi::FindMnemonicTextIndex;

    const auto match = FindMnemonicTextIndex(L"Search", L'z');
    Require(! match.has_value(), "mnemonic display helper returns no match when absent");
}

void TestMnemonicTextIndexUsesExplicitAmpersandMnemonic()
{
    using DxUi::FindMnemonicTextIndex;

    const auto match = FindMnemonicTextIndex(L"&Close", L'c');
    Require(match.has_value() && match.value() == 0u, "mnemonic display helper honors explicit ampersand mnemonics");
}

void TestMnemonicTextIndexTreatsEscapedAmpersandAsLiteralDisplayText()
{
    using DxUi::FindMnemonicTextIndex;

    const auto match = FindMnemonicTextIndex(L"Save && Exit", L'&');
    Require(match.has_value() && match.value() == 5u, "mnemonic display helper counts escaped ampersands in display coordinates");
}

void TestThroughputGraphHonorsMotionRainbowAndHighContrastContracts()
{
    using namespace DxUi;

    auto progress = std::make_shared<ProgressBar>();
    progress->SetSegmentedValues(80.0, 35.0, D2D1::ColorF(0.65f, 0.38f, 0.0f));
    Require(progress->HasSegmentedValues(), "progress bar retains the hosted two-segment verification model");
    progress->ClearSegmentedValues();
    Require(! progress->HasSegmentedValues(), "progress bar clears the verification segment for ordinary progress");

    const D2D1_COLOR_F normalizedStart = ThroughputGraphColorFromHue(0.0f, true);
    const D2D1_COLOR_F normalizedEnd   = ThroughputGraphColorFromHue(360.0f, true);
    Require(normalizedStart.r == normalizedEnd.r && normalizedStart.g == normalizedEnd.g && normalizedStart.b == normalizedEnd.b &&
                normalizedStart.a == normalizedEnd.a,
            "throughput graph exposes one normalized hue-to-color contract for graph and related stream UI");
    Require(! ShouldRenderThroughputGraphBands(false, true, false, 1u), "ordinary-theme throughput bands stay off for one admitted stream");
    Require(ShouldRenderThroughputGraphBands(false, true, false, 2u), "ordinary-theme throughput bands engage for concurrent admitted streams");
    Require(ShouldRenderThroughputGraphBands(true, true, false, 1u), "Rainbow throughput bands may color one admitted stream");
    Require(! ShouldRenderThroughputGraphBands(true, true, true, 2u), "High Contrast suppresses throughput hue bands");

    WindowHost host;
    ThemePalette palette{};
    palette.reducedMotion = false;
    palette.highContrast  = false;
    host.SetTheme(palette);

    auto root   = std::make_unique<Panel>();
    auto* graph = root->AddChild<ThroughputGraph>();
    graph->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 80.0f));
    graph->SetRainbowMode(true);
    graph->SetPerStreamBands(true);
    host.SetRoot(std::move(root));

    std::array<ThroughputGraphSample, 2u> samples{};
    samples[0].value          = 10.0;
    samples[0].hueDegrees     = 20.0f;
    samples[0].hueWeights[0]  = ThroughputGraphHueWeight{20.0f, 1.0, 0u};
    samples[0].hueWeightCount = 1u;
    samples[1].value          = 20.0;
    samples[1].hueDegrees     = 220.0f;
    samples[1].hueWeights[0]  = ThroughputGraphHueWeight{20.0f, 1.0, 0u};
    samples[1].hueWeights[1]  = ThroughputGraphHueWeight{220.0f, 1.0, 1u};
    samples[1].hueWeightCount = 2u;
    graph->SetSamples(samples);
    constexpr std::array<double, 2u> verificationSamples{{0.0, 7.0}};
    graph->SetSecondarySamples(verificationSamples);
    graph->SetSecondarySeriesColor(D2D1::ColorF(0.65f, 0.38f, 0.0f));
    graph->SetCurrentValueMarker(16.0, L"16 B/s");

    ThroughputGraphDebugState state = graph->GetDebugState();
    Require(state.sampleCount == 2u && state.hueBandCount == 3u, "throughput graph retains samples and per-stream hue bands");
    Require(state.secondarySeriesVisible && state.secondarySeriesColorCustomized,
            "throughput graph exposes the distinct themed verification-throughput series");
    Require(state.transitionActive, "throughput graph eases a changed latest sample when motion is enabled");
    Require(state.currentValueMarkerVisible && state.targetCurrentValue == 16.0, "throughput graph retains the current effective-bandwidth marker");
    static_cast<void>(graph->Tick(host, 100u));
    static_cast<void>(graph->Tick(host, 180u));
    state = graph->GetDebugState();
    Require(state.displayedLatestValue > 0.0 && state.displayedLatestValue < state.targetLatestValue,
            "throughput graph exposes an intermediate eased latest value");
    Require(state.displayedCurrentValue > 0.0 && state.displayedCurrentValue < state.targetCurrentValue,
            "throughput graph exposes an intermediate eased current-bandwidth marker");
    static_cast<void>(graph->Tick(host, 300u));
    state = graph->GetDebugState();
    Require(! state.transitionActive && state.displayedLatestValue == state.targetLatestValue,
            "throughput graph completes its bounded latest-value transition");
    Require(state.displayedCurrentValue == state.targetCurrentValue, "throughput graph completes its bounded current-bandwidth marker transition");

    graph->Paint(host);
    state = graph->GetDebugState();
    Require(state.usesRainbowStroke && ! state.highContrast, "normal-contrast throughput graph enables its rainbow stroke contract");

    palette.highContrast = true;
    host.SetTheme(palette);
    graph->Paint(host);
    state = graph->GetDebugState();
    Require(state.highContrast && ! state.usesRainbowStroke, "high contrast suppresses rainbow throughput strokes");

    palette.highContrast  = false;
    palette.reducedMotion = true;
    host.SetTheme(palette);
    samples[1].value = 40.0;
    graph->SetSamples(samples);
    graph->SetCurrentValueMarker(32.0, L"32 B/s");
    graph->Paint(host);
    state = graph->GetDebugState();
    Require(state.reducedMotion && ! state.transitionActive && state.displayedLatestValue == state.targetLatestValue,
            "reduced motion snaps throughput graph updates to the target value");
    Require(state.displayedCurrentValue == state.targetCurrentValue, "reduced motion snaps the current-bandwidth marker while keeping it visible");
}

void TestScrollPanelChildCallbacksCanClearChildrenSafely()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* scroll = root->AddChild<ScrollPanel>();
    scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 100.0f));
    scroll->SetContentHeight(100.0f);
    ScrollPanelReentrancyProbeState captureState;
    auto* captureChild = scroll->AddChild<ScrollPanelClearingChild>(*scroll, captureState);
    captureChild->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));
    host.SetRoot(std::move(root));
    static_cast<Panel*>(host.GetRoot())->SetBounds(D2D1::RectF(0.0f, 0.0f, 140.0f, 100.0f));

    Require(scroll->OnMouseDown(host, D2D1::Point2F(12.0f, 12.0f), false, 0), "scroll panel forwards mouse-down to clearing child");
    Require(captureState.mouseDownCount == 1u, "clearing child receives one mouse-down before clearing children");
    Require(! scroll->OnMouseUp(host, D2D1::Point2F(12.0f, 12.0f), false, 0), "scroll panel does not reuse a cleared captured child on mouse-up");
    Require(captureState.mouseMoveCount == 0u, "cleared captured child is not reused after mouse-down");

    auto* hoverChild = scroll->AddChild<ScrollPanelClearingChild>(*scroll, captureState);
    hoverChild->SetBounds(D2D1::RectF(0.0f, 0.0f, 120.0f, 40.0f));
    Require(scroll->OnMouseMove(host, D2D1::Point2F(12.0f, 12.0f), 0), "scroll panel forwards hover-enter to clearing child");
    Require(captureState.hoverEnterCount == 1u, "clearing child receives one hover-enter before clearing children");
    Require(captureState.mouseMoveCount == 0u, "cleared hovered child is not reused for mouse-move");
}

void TestPanelCallbacksCanRemoveSiblingsAndDestroyTheirOwner()
{
    using namespace DxUi;

    class CallbackPanel final : public Panel
    {
    public:
        explicit CallbackPanel(std::function<void()> onHidden) : _onHidden(std::move(onHidden))
        {
        }

        size_t* hiddenCount = nullptr;
        void SetOnHidden(std::function<void()> callback)
        {
            _onHidden = std::move(callback);
        }
        void SetOnBoundsChanged(std::function<void()> callback)
        {
            _onBoundsChanged = std::move(callback);
        }

    protected:
        void OnBoundsChanged() noexcept override
        {
            std::function<void()> callback = std::move(_onBoundsChanged);
            if (callback)
            {
                callback();
            }
        }

        void OnHidden() noexcept override
        {
            if (hiddenCount)
            {
                ++*hiddenCount;
            }
            std::function<void()> callback = std::move(_onHidden);
            if (callback)
            {
                callback();
            }
        }

    private:
        std::function<void()> _onHidden;
        std::function<void()> _onBoundsChanged;
    };

    // Removing the next sibling from the first child's callback must not invalidate the iteration or skip the
    // surviving sibling that shifts into its place.
    {
        auto panel                   = std::make_unique<Panel>();
        size_t laterChildHiddenCount = 0u;
        panel->AddChild<CallbackPanel>([&] { panel->GetChildren()[1].reset(); });
        panel->AddChild<CallbackPanel>(std::function<void()>{});
        auto* later        = panel->AddChild<CallbackPanel>(std::function<void()>{});
        later->hiddenCount = &laterChildHiddenCount;
        panel->SetVisible(false);
        Require(laterChildHiddenCount == 1u, "Panel propagation reaches the sibling shifted into a removed slot");
    }

    // SetVisible and SetBounds must stop after a child callback destroys that child.
    {
        auto panel  = std::make_unique<Panel>();
        auto* child = panel->AddChild<CallbackPanel>([&] { panel->ClearChildren(); });
        child->SetVisible(true);
        child->SetVisible(false);
        Require(panel->GetLogicalChildCount() == 0u, "OnHidden may destroy its child without a stale post-callback access");
    }
    {
        auto panel  = std::make_unique<Panel>();
        auto* child = panel->AddChild<CallbackPanel>(std::function<void()>{});
        child->SetBounds(D2D1::RectF(0.0f, 0.0f, 10.0f, 10.0f));
        child->SetOnBoundsChanged([&] { panel->ClearChildren(); });
        child->SetBounds(D2D1::RectF(1.0f, 0.0f, 11.0f, 10.0f));
        Require(panel->GetLogicalChildCount() == 0u, "OnBoundsChanged may destroy its child without a stale invalidate");
    }

    // The child propagation loop must stop using the owner after a callback replaces the host root.
    {
        WindowHost host;
        auto root             = std::make_unique<Panel>();
        auto* panel           = root->AddChild<Panel>();
        auto replacement      = std::make_unique<Panel>();
        Panel* replacementPtr = replacement.get();
        panel->AddChild<CallbackPanel>([&] { host.SetRoot(std::move(replacement)); });
        host.SetRoot(std::move(root));
        panel->SetVisible(false);
        Require(host.GetRoot() == replacementPtr, "child callback can replace the host root during Panel propagation");
    }
}

enum class PanelTraversalKind : uint8_t
{
    Paint,
    PaintOverlay,
    Tick,
    HitTest,
    HitTestOverlay,
};

struct PanelTraversalCallbackState final
{
    DxUi::WindowHost* host = nullptr;
    DxUi::Panel* owner     = nullptr;
    bool replaceRoot       = false;
    std::unique_ptr<DxUi::Panel> replacementRoot;
    DxUi::Panel* replacementRootAddress = nullptr;
    DxUi::Control* triggerChildAddress  = nullptr;
    std::function<void()> retire;
    size_t triggerCalls = 0u;
    size_t siblingCalls = 0u;
};

class PanelTraversalTestPanel final : public DxUi::Panel
{
public:
    [[nodiscard]] DxUi::Control* TestHit(D2D1_POINT_2F point)
    {
        return HitTest(point);
    }

    [[nodiscard]] DxUi::Control* TestOverlayHit(D2D1_POINT_2F point)
    {
        return HitTestOverlay(point);
    }
};

class PanelTraversalCallbackChild final : public DxUi::Control
{
public:
    PanelTraversalCallbackChild(PanelTraversalCallbackState& state, bool triggersRetirement) noexcept : _state(&state), _triggersRetirement(triggersRetirement)
    {
    }

    void Paint(DxUi::ControlHost& /*host*/) const override
    {
        Visit();
    }

    void PaintOverlay(DxUi::ControlHost& /*host*/) const override
    {
        Visit();
    }

    bool Tick(DxUi::ControlHost& /*host*/, uint64_t /*nowTickMs*/) override
    {
        Visit();
        return false;
    }

protected:
    [[nodiscard]] DxUi::Control* HitTest(D2D1_POINT_2F /*point*/) override
    {
        Visit();
        return this;
    }

    [[nodiscard]] DxUi::Control* HitTestOverlay(D2D1_POINT_2F /*point*/) override
    {
        Visit();
        return this;
    }

private:
    void Visit() const
    {
        PanelTraversalCallbackState* const state = _state;
        if (! _triggersRetirement)
        {
            ++state->siblingCalls;
            return;
        }

        ++state->triggerCalls;
        // The callable and its capture storage belong to the test fixture, not this control. The callback may destroy
        // this child, its owner, and the entire old root before returning.
        state->retire();
    }

    PanelTraversalCallbackState* _state = nullptr;
    bool _triggersRetirement            = false;
};

void TestPanelTraversalCallbacksCanRetireOwnerSafely()
{
    using namespace DxUi;
    constexpr D2D1_POINT_2F point{5.0f, 5.0f};

    for (const PanelTraversalKind traversal : {PanelTraversalKind::Paint,
                                               PanelTraversalKind::PaintOverlay,
                                               PanelTraversalKind::Tick,
                                               PanelTraversalKind::HitTest,
                                               PanelTraversalKind::HitTestOverlay})
    {
        for (const bool replaceRoot : {false, true})
        {
            WindowHost host;
            PanelTraversalCallbackState state;
            state.host        = &host;
            state.replaceRoot = replaceRoot;
            auto root         = std::make_unique<PanelTraversalTestPanel>();
            auto* const owner = root.get();
            state.owner       = owner;
            owner->SetBounds(D2D1::RectF(0.0f, 0.0f, 40.0f, 40.0f));

            const bool reverseTraversal = traversal == PanelTraversalKind::HitTest || traversal == PanelTraversalKind::HitTestOverlay;
            if (reverseTraversal)
            {
                owner->AddChild<PanelTraversalCallbackChild>(state, false);
                state.triggerChildAddress = owner->AddChild<PanelTraversalCallbackChild>(state, true);
            }
            else
            {
                state.triggerChildAddress = owner->AddChild<PanelTraversalCallbackChild>(state, true);
                owner->AddChild<PanelTraversalCallbackChild>(state, false);
            }

            state.retire = [&state]()
            {
                if (state.replaceRoot)
                {
                    state.replacementRoot        = std::make_unique<Panel>();
                    state.replacementRootAddress = state.replacementRoot.get();
                    state.host->SetRoot(std::move(state.replacementRoot));
                    return;
                }

                state.owner->ClearChildren();
            };
            host.SetRoot(std::move(root));
            // SetRoot assigns the root's host-sized bounds (the default test host is 0x0), so restore the
            // explicit hit-test extent after installation.
            owner->SetBounds(D2D1::RectF(0.0f, 0.0f, 40.0f, 40.0f));

            Control* hit = nullptr;
            switch (traversal)
            {
                case PanelTraversalKind::Paint: owner->Paint(host); break;
                case PanelTraversalKind::PaintOverlay: owner->PaintOverlay(host); break;
                case PanelTraversalKind::Tick: static_cast<void>(owner->Tick(host, 1u)); break;
                case PanelTraversalKind::HitTest: hit = owner->TestHit(point); break;
                case PanelTraversalKind::HitTestOverlay: hit = owner->TestOverlayHit(point); break;
            }

            Require(state.triggerCalls == 1u, "Panel traversal invokes the retiring child exactly once");
            Require(state.siblingCalls == 0u, "Panel traversal does not call an old sibling after retirement");
            if (replaceRoot)
            {
                Require(host.GetRoot() == state.replacementRootAddress, "the replacement root remains current after Panel traversal returns");
                if (traversal == PanelTraversalKind::HitTest || traversal == PanelTraversalKind::HitTestOverlay)
                {
                    Require(hit == nullptr, "Panel hit testing never returns a retired control after root replacement");
                }
            }
            else
            {
                Require(host.GetRoot() == owner && owner->GetLogicalChildCount() == 0u,
                        "clearing children retires the hit child while leaving its parent alive");
                if (traversal == PanelTraversalKind::HitTest || traversal == PanelTraversalKind::HitTestOverlay)
                {
                    Require(hit != state.triggerChildAddress, "Panel hit testing does not return a retired child after ClearChildren");
                }
            }
        }
    }
}

void TestPageHostOutgoingPaintCanReplaceItsPageWithoutLeakingDrawingState()
{
    using namespace DxUi;

    struct State final
    {
        PageHost* pageHost               = nullptr;
        bool replaceOnPaint              = true;
        bool replaceFromOverlay          = false;
        size_t outgoingPaintCalls        = 0u;
        size_t outgoingOverlayPaintCalls = 0u;
        size_t incomingPaintCalls        = 0u;
        size_t incomingOverlayPaintCalls = 0u;
        Control* replacementPage         = nullptr;
    };

    class OutgoingPage final : public Panel
    {
    public:
        explicit OutgoingPage(State& state) noexcept : _state(&state)
        {
        }

        void Paint(ControlHost& /*host*/) const override
        {
            State* const state = _state;
            ++state->outgoingPaintCalls;
            if (! state->replaceFromOverlay)
            {
                ReplacePage(state);
            }
        }

        void PaintOverlay(ControlHost& /*host*/) const override
        {
            State* const state = _state;
            ++state->outgoingOverlayPaintCalls;
            if (state->replaceFromOverlay)
            {
                ReplacePage(state);
            }
        }

    private:
        static void ReplacePage(State* state)
        {
            if (! state->replaceOnPaint)
            {
                return;
            }
            state->replaceOnPaint  = false;
            auto replacement       = std::make_unique<Panel>();
            state->replacementPage = replacement.get();
            state->pageHost->SetPage(std::move(replacement));
        }

        State* _state = nullptr;
    };

    class IncomingPage final : public Panel
    {
    public:
        explicit IncomingPage(State& state) noexcept : _state(&state)
        {
        }

        void Paint(ControlHost& /*host*/) const override
        {
            ++_state->incomingPaintCalls;
        }

        void PaintOverlay(ControlHost& /*host*/) const override
        {
            ++_state->incomingOverlayPaintCalls;
        }

    private:
        State* _state = nullptr;
    };

    for (const bool replaceFromOverlay : {false, true})
    {
        State state;
        state.replaceFromOverlay = replaceFromOverlay;
        AttachedHostWindow window;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        WindowHost& host         = window.Host();
        auto root                = std::make_unique<Panel>();
        PageHost* const pageHost = root->AddChild<PageHost>();
        state.pageHost           = pageHost;
        host.SetRoot(std::move(root));
        pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
        pageHost->SetPage(std::make_unique<OutgoingPage>(state));
        pageHost->SetPage(std::make_unique<IncomingPage>(state));
        pageHost->DebugFreezeTransitionProgress(0.0f);
        Require(pageHost->HasActiveTransition(), "the outgoing page participates in an active transition");

        WindowHostBitmapCapture firstCapture;
        Require(! host.DebugCaptureBitmap(firstCapture), "page replacement during transition paint discards the incomplete frame");
        Require(state.outgoingPaintCalls == 1u, "the outgoing page paint callback runs once during the transition frame");
        Require(pageHost->GetPage() == state.replacementPage, "the page installed by the outgoing callback remains current");
        if (replaceFromOverlay)
        {
            Require(state.outgoingOverlayPaintCalls == 1u, "the outgoing page overlay callback runs once");
            Require(state.incomingOverlayPaintCalls == 0u, "replacing the outgoing page stops the stale incoming overlay paint");
        }
        else
        {
            Require(state.incomingPaintCalls == 0u, "replacing the outgoing page stops the stale incoming-page paint");
        }

        WindowHostBitmapCapture recoveredCapture;
        Require(host.DebugCaptureBitmap(recoveredCapture), "a later capture succeeds after the outgoing callback replaces its page");
        const auto pageBounds = pageHost->GetPage()->GetBounds();
        const auto hostBounds = pageHost->GetBounds();
        Require(pageBounds.left == hostBounds.left && pageBounds.top == hostBounds.top && pageBounds.right == hostBounds.right &&
                    pageBounds.bottom == hostBounds.bottom,
                "the next preparation lays out the replacement page without a resize or animation tick");
    }
}

void TestPageHostHitTestingRejectsAChildFromAReplacedPage()
{
    using namespace DxUi;

    struct State final
    {
        PageHost* pageHost  = nullptr;
        Control* oldPage    = nullptr;
        size_t hitTestCalls = 0u;
    } state;

    class ReplacingHitPage final : public Panel
    {
    public:
        explicit ReplacingHitPage(State& state) : _state(&state)
        {
            _staleChild = AddChild<Button>(L"retired");
        }

    protected:
        Control* HitTest(D2D1_POINT_2F /*point*/) override
        {
            State* const state        = _state;
            Control* const staleChild = _staleChild;
            ++state->hitTestCalls;
            state->pageHost->SetPage(std::make_unique<Panel>());
            return staleChild;
        }

    private:
        State* _state        = nullptr;
        Control* _staleChild = nullptr;
    };

    WindowHost host;
    auto theme          = host.GetTheme();
    theme.reducedMotion = true;
    host.SetTheme(theme);
    auto root      = std::make_unique<Panel>();
    auto* pageHost = root->AddChild<PageHost>();
    state.pageHost = pageHost;
    host.SetRoot(std::move(root));
    host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 60.0f));
    pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 80.0f, 60.0f));
    auto firstPage = std::make_unique<ReplacingHitPage>(state);
    state.oldPage  = firstPage.get();
    pageHost->SetPage(std::move(firstPage));

    const Control* const hit = host.DebugHitTestControl(D2D1::Point2F(5.0f, 5.0f));
    Require(state.hitTestCalls == 1u, "the current page's hit callback runs exactly once");
    Require(pageHost->GetPage() != state.oldPage && hit == nullptr,
            "the host rejects the old hit geometry when a callback replaces the page and retires the reported child");
    Require(host.DebugHitTestControl(D2D1::Point2F(5.0f, 5.0f)) == pageHost->GetPage(),
            "a fresh hit resolves the live replacement page after the stale query is rejected");
}

void TestScrollPanelQueriesAndPaintStopAfterRootRetirement()
{
    using namespace DxUi;

    struct QueryState final
    {
        WindowHost* host = nullptr;
        std::unique_ptr<Panel> replacementRoot;
        Panel* replacementAddress  = nullptr;
        size_t hitTestCalls        = 0u;
        size_t overlayHitTestCalls = 0u;
        size_t paintCalls          = 0u;
        bool retireFromOverlay     = false;

        void ReplaceRoot()
        {
            replacementAddress = replacementRoot.get();
            host->SetRoot(std::move(replacementRoot));
        }
    };

    class RetiringQueryChild final : public Control
    {
    public:
        explicit RetiringQueryChild(QueryState& state) noexcept : _state(&state)
        {
        }

        void Paint(ControlHost& /*host*/) const override
        {
        }

    protected:
        Control* HitTest(D2D1_POINT_2F /*point*/) override
        {
            QueryState* const state = _state;
            ++state->hitTestCalls;
            state->ReplaceRoot();
            return this;
        }

        Control* HitTestOverlay(D2D1_POINT_2F /*point*/) override
        {
            QueryState* const state = _state;
            ++state->overlayHitTestCalls;
            if (! state->retireFromOverlay)
                return nullptr;
            state->ReplaceRoot();
            return this;
        }

    private:
        QueryState* _state = nullptr;
    };

    // The ordinary child query retires the ScrollPanel while UpdateInnerHover is resolving a hit.
    {
        WindowHost host;
        QueryState state;
        state.host            = &host;
        state.replacementRoot = std::make_unique<Panel>();
        auto root             = std::make_unique<Panel>();
        auto* scroll          = root->AddChild<ScrollPanel>();
        scroll->AddChild<RetiringQueryChild>(state);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 80.0f));
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 80.0f));

        Require(scroll->OnMouseMove(host, D2D1::Point2F(5.0f, 5.0f), 0u), "ScrollPanel handles a move whose child query replaces the root");
        Require(host.GetRoot() == state.replacementAddress && state.hitTestCalls == 1u,
                "hover resolution stops after the queried child retires its ScrollPanel");
    }

    // The overlay query returns no dangling container pointer when a child replaces the root from HitTestOverlay.
    {
        WindowHost host;
        QueryState state;
        state.host              = &host;
        state.retireFromOverlay = true;
        state.replacementRoot   = std::make_unique<Panel>();
        auto root               = std::make_unique<Panel>();
        auto* scroll            = root->AddChild<ScrollPanel>();
        scroll->AddChild<RetiringQueryChild>(state);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 80.0f));
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 100.0f, 80.0f));

        Require(scroll->HitTestOverlay(D2D1::Point2F(5.0f, 5.0f)) == nullptr,
                "ScrollPanel overlay hit testing does not return itself after its child retires the owner");
        Require(host.GetRoot() == state.replacementAddress && state.overlayHitTestCalls == 1u,
                "overlay hit testing observes the replacement root after callback retirement");
    }

    // Paint exercises the same callback path with an attached, non-null rendering context and verifies recovery.
    {
        AttachedHostWindow window;
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        window.PumpMessages();
        WindowHost& host = window.Host();
        QueryState state;
        state.host            = &host;
        state.replacementRoot = std::make_unique<Panel>();
        class RetiringPaintChild final : public Control
        {
        public:
            explicit RetiringPaintChild(QueryState& state) noexcept : _state(&state)
            {
            }

            void Paint(ControlHost& /*host*/) const override
            {
                QueryState* const state = _state;
                ++state->paintCalls;
                state->ReplaceRoot();
            }

        private:
            QueryState* _state = nullptr;
        };

        auto root    = std::make_unique<Panel>();
        auto* scroll = root->AddChild<ScrollPanel>();
        scroll->AddChild<RetiringPaintChild>(state);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 320.0f, 200.0f));

        WindowHostBitmapCapture firstCapture;
        Require(! host.DebugCaptureBitmap(firstCapture), "ScrollPanel root retirement during paint discards the incomplete frame");
        Require(host.GetRoot() == state.replacementAddress && state.paintCalls == 1u, "ScrollPanel paint stops after its child replaces the host root");
        WindowHostBitmapCapture recoveredCapture;
        Require(host.DebugCaptureBitmap(recoveredCapture), "a later capture succeeds after ScrollPanel paint retirement");
    }
}

void TestPageHostCallbacksCanDestroyTheirOwnerSafely()
{
    using namespace DxUi;

    class CallbackPage final : public Panel
    {
    public:
        explicit CallbackPage(std::function<void()> onBounds = {}, std::function<void()> onHidden = {})
            : _onBounds(std::move(onBounds)),
              _onHidden(std::move(onHidden))
        {
        }

    protected:
        void OnBoundsChanged() noexcept override
        {
            std::function<void()> callback = std::move(_onBounds);
            if (callback)
            {
                callback();
            }
        }

        void OnHidden() noexcept override
        {
            std::function<void()> callback = std::move(_onHidden);
            if (callback)
            {
                callback();
            }
        }

    private:
        std::function<void()> _onBounds;
        std::function<void()> _onHidden;
    };

    // Incoming bounds are applied before SetPage installs the page. If that callback replaces the host root, the
    // outer SetPage must return without reading the destroyed PageHost or its former host.
    {
        WindowHost host;
        EnableMotionForTest(host);
        auto root      = std::make_unique<Panel>();
        auto* pageHost = root->AddChild<PageHost>();
        pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        host.SetRoot(std::move(root));

        auto replacement            = std::make_unique<Panel>();
        Panel* const replacementPtr = replacement.get();
        auto incoming               = std::make_unique<CallbackPage>([&] { host.SetRoot(std::move(replacement)); });
        pageHost->SetPage(std::move(incoming));
        Require(host.GetRoot() == replacementPtr, "incoming page bounds callback replaces the root during PageHost::SetPage");
    }

    // A current page can replace the host root while PageHost propagates OnHidden. The page and PageHost both die
    // during the callback; neither the page loop nor Control::SetVisible may access them afterward.
    {
        WindowHost host;
        auto root      = std::make_unique<Panel>();
        auto* pageHost = root->AddChild<PageHost>();
        pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        host.SetRoot(std::move(root));

        auto replacement            = std::make_unique<Panel>();
        Panel* const replacementPtr = replacement.get();
        pageHost->SetPage(std::make_unique<CallbackPage>(std::function<void()>{}, [&] { host.SetRoot(std::move(replacement)); }));
        pageHost->SetVisible(false);
        Require(host.GetRoot() == replacementPtr, "current page hidden callback replaces the root during PageHost propagation");
    }

    // Replacing a page during an active transition intentionally drops the older outgoing page before moving the
    // current page into the outgoing role. The role guard must account for that expected reset.
    {
        WindowHost host;
        EnableMotionForTest(host);
        auto root      = std::make_unique<Panel>();
        auto* pageHost = root->AddChild<PageHost>();
        pageHost->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        root->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 120.0f));
        host.SetRoot(std::move(root));

        pageHost->SetPage(std::make_unique<Panel>());
        pageHost->SetPage(std::make_unique<Panel>());
        Require(pageHost->HasActiveTransition(), "second page starts an animated transition");
        auto thirdPage              = std::make_unique<Panel>();
        Control* const thirdPagePtr = thirdPage.get();
        pageHost->SetPage(std::move(thirdPage));
        Require(pageHost->GetPage() == thirdPagePtr, "third page replaces the current page during an active transition");
        Require(pageHost->HasActiveTransition(), "third page keeps its transition active after replacing the prior outgoing page");
    }
}

// Each handler that focuses its own control, given a focus callback that replaces every control, touches the destroyed
// control no further (AddressSanitizer catches a handler that does).
void TestControlsThatFocusThemselvesLeaveAControlTheFocusCallbackDestroyed()
{
    using namespace DxUi;
    const D2D1_RECT_F bounds   = D2D1::RectF(0.0f, 40.0f, 360.0f, 120.0f);
    const D2D1_POINT_2F center = D2D1::Point2F(180.0f, 80.0f);
    const auto add             = [&]<typename TControl, typename... TArgs>(Panel& root, TArgs&&... args) -> TControl*
    {
        auto* control = root.AddChild<TControl>(std::forward<TArgs>(args)...);
        control->SetBounds(bounds);
        return control;
    };

    RequireFocusReplacementLeavesControlAlone("Button press", [&](Panel& root) {
        return add.operator()<Button>(root, L"Pressed");
    }, [&](WindowHost& host, Control& button) { static_cast<void>(button.OnMouseDown(host, center, false, 0u)); });
    RequireFocusReplacementLeavesControlAlone("Button right press", [&](Panel& root) {
        return add.operator()<Button>(root, L"Pressed");
    }, [&](WindowHost& host, Control& button) { static_cast<void>(button.OnMouseDown(host, center, true, 0u)); });
    RequireFocusReplacementLeavesControlAlone("Button invoke", [&](Panel& root) {
        return add.operator()<Button>(root, L"Invoked");
    }, [&](WindowHost& host, Control& button) { static_cast<void>(static_cast<Button&>(button).Invoke(host, true)); });
    RequireFocusReplacementLeavesControlAlone("Toggle mnemonic", [&](Panel& root) {
        return add.operator()<Toggle>(root, L"Toggled");
    }, [&](WindowHost& host, Control& toggle) { static_cast<void>(toggle.OnMnemonic(host)); });
    RequireFocusReplacementLeavesControlAlone("RadioButton mnemonic", [&](Panel& root) {
        return add.operator()<RadioButton>(root, L"Chosen");
    }, [&](WindowHost& host, Control& radio) { static_cast<void>(radio.OnMnemonic(host)); });
    RequireFocusReplacementLeavesControlAlone("PageIndicator press",
                                              [&](Panel& root)
    {
        auto* indicator = root.AddChild<PageIndicator>();
        indicator->SetBounds(D2D1::RectF(0.0f, 40.0f, 200.0f, 40.0f + PageIndicator::kStripHeightDip));
        indicator->SetPageCount(4);
        return indicator;
    },
                                              [&](WindowHost& host, Control& indicator)
    { static_cast<void>(indicator.OnMouseDown(host, static_cast<PageIndicator&>(indicator).DotCenter(2), false, 0u)); });
    RequireFocusReplacementLeavesControlAlone("Slider press", [&](Panel& root) {
        return add.operator()<Slider>(root);
    }, [&](WindowHost& host, Control& slider) { static_cast<void>(slider.OnMouseDown(host, center, false, 0u)); });
    // A swatch takes the focus only where the application made it focusable, as a clickable swatch.
    RequireFocusReplacementLeavesControlAlone("ColorSwatch press",
                                              [&](Panel& root)
    {
        auto* swatch = add.operator()<ColorSwatch>(root, 0xFF2266AAu);
        swatch->SetFocusable(true);
        return swatch;
    },
                                              [&](WindowHost& host, Control& swatch) { static_cast<void>(swatch.OnMouseDown(host, center, false, 0u)); });

    const auto addMenuBar = [&](Panel& root)
    {
        auto* menu = root.AddChild<MenuBar>();
        menu->SetBounds(D2D1::RectF(0.0f, 40.0f, 360.0f, 68.0f));
        menu->SetItems({MenuBarItem{.text = L"File", .mnemonic = L'F', .enabled = true}, MenuBarItem{.text = L"Edit", .mnemonic = L'E', .enabled = true}});
        menu->SetOnOpenItem([](size_t, POINT, bool) {});
        return menu;
    };
    RequireFocusReplacementLeavesControlAlone("MenuBar press",
                                              addMenuBar,
                                              [&](WindowHost& host, Control& menu)
    {
        RECT itemPx{};
        Require(static_cast<MenuBar&>(menu).TryGetItemScreenRect(host, 1u, itemPx), "MenuBar press: the second item has a rectangle");
        const D2D1_POINT_2F item = D2D1::Point2F(static_cast<float>(itemPx.left + itemPx.right) * 0.5f, 54.0f);
        static_cast<void>(menu.OnMouseDown(host, item, false, 0u));
    });
    RequireFocusReplacementLeavesControlAlone(
        "MenuBar mnemonic", addMenuBar, [&](WindowHost& host, Control& menu) { static_cast<void>(static_cast<MenuBar&>(menu).ActivateMnemonic(host, L'E')); });

    const auto addTabs = [&](Panel& root)
    {
        auto* tabs = root.AddChild<TabControl>();
        tabs->SetBounds(D2D1::RectF(0.0f, 40.0f, 640.0f, 220.0f));
        tabs->AddTab<Button>(L"Alpha", L"Alpha content");
        tabs->AddTab<Button>(L"Bravo", L"Bravo content");
        tabs->SetTabClosable(0u, true);
        return tabs;
    };
    const auto centerOf = [](const D2D1_RECT_F& rect) { return D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f); };
    RequireFocusReplacementLeavesControlAlone("TabControl tab press", addTabs, [&](WindowHost& host, Control& tabs) {
        static_cast<void>(tabs.OnMouseDown(host, centerOf(static_cast<TabControl&>(tabs).DebugGetTabRect(1u)), false, 0u));
    });
    RequireFocusReplacementLeavesControlAlone("TabControl close press", addTabs, [&](WindowHost& host, Control& tabs) {
        static_cast<void>(tabs.OnMouseDown(host, centerOf(static_cast<TabControl&>(tabs).DebugGetCloseButtonRect(0u)), false, 0u));
    });
    RequireFocusReplacementLeavesControlAlone(
        "TabControl End", addTabs, [&](WindowHost& host, Control& tabs) { static_cast<void>(tabs.OnKeyDown(host, VK_END, 0u)); });
    RequireFocusReplacementLeavesControlAlone("TabControl removing the focused tab",
                                              addTabs,
                                              [&](WindowHost& host, Control& tabs)
    {
        // Focus inside the first tab's content: removing that tab moves the focus to the tab control itself.
        auto& tabControl = static_cast<TabControl&>(tabs);
        host.SetFocusControl(tabControl.GetChildren()[0].get());
        tabControl.RemoveTab(0u);
    });
}

void TestMutableCallbackCaptureStatePersistsForControlAndButton()
{
    using namespace DxUi;

    WindowHost host;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Stateful callbacks");
    host.SetRoot(std::move(root));

    std::vector<size_t> contextCounts;
    button->SetOnContextMenu([count = size_t{0u}, &contextCounts](POINT, bool) mutable { contextCounts.push_back(++count); });
    Require(button->OnContextMenu(host, true, {}) && button->OnContextMenu(host, true, {}), "Control dispatches repeated context-menu requests");
    Require(contextCounts == std::vector<size_t>{1u, 2u}, "Control retains mutable context-menu callback state between dispatches");

    std::vector<size_t> invokeCounts;
    button->SetAccessibleInvoke([count = size_t{0u}, &invokeCounts](ControlHost&) mutable { invokeCounts.push_back(++count); });
    Require(button->InvokeAccessible(host) && button->InvokeAccessible(host), "Control dispatches repeated accessible invokes");
    Require(invokeCounts == std::vector<size_t>{1u, 2u}, "Control retains mutable accessible-invoke callback state between dispatches");

    std::vector<size_t> clickCounts;
    button->SetOnClick([count = size_t{0u}, &clickCounts]() mutable { clickCounts.push_back(++count); });
    Require(button->Invoke(host, false) && button->Invoke(host, false), "Button dispatches repeated click invocations");
    Require(clickCounts == std::vector<size_t>{1u, 2u}, "Button retains mutable click callback state between invocations");

    button->SetVariant(ButtonVariant::DropDown);
    std::vector<size_t> dropDownCounts;
    button->SetOnDropDownClick([count = size_t{0u}, &dropDownCounts]() mutable { dropDownCounts.push_back(++count); });
    Require(button->Invoke(host, false) && button->Invoke(host, false), "Drop-down Button dispatches repeated menu requests");
    Require(dropDownCounts == std::vector<size_t>{1u, 2u}, "Drop-down Button retains mutable callback state between invocations");
}

void TestMutableCallbackCaptureStatePersistsForToggleAndColorSwatch()
{
    using namespace DxUi;

    WindowHost host;
    Toggle toggle(L"Toggle");
    std::vector<std::pair<size_t, bool>> toggleStates;
    toggle.SetOnToggled([count = size_t{0u}, &toggleStates](bool checked) mutable { toggleStates.emplace_back(++count, checked); });
    Require(toggle.OnKeyDown(host, VK_SPACE, 0u) && toggle.OnKeyDown(host, VK_SPACE, 0u), "Toggle handles repeated Space activation");
    Require(toggleStates == std::vector<std::pair<size_t, bool>>{{1u, true}, {2u, false}}, "Toggle retains mutable OnToggled state across activations");

    ColorSwatch swatch(0xFF336699u);
    std::vector<size_t> clickCounts;
    swatch.SetOnClick([count = size_t{0u}, &clickCounts]() mutable { clickCounts.push_back(++count); });
    Require(swatch.OnKeyDown(host, VK_SPACE, 0u) && swatch.OnKeyDown(host, VK_SPACE, 0u), "ColorSwatch handles repeated keyboard activation");
    Require(clickCounts == std::vector<size_t>{1u, 2u}, "ColorSwatch retains mutable click callback state between activations");
}

void TestStackPanelLayoutStopsAfterChildCallbacksRetireItsTree()
{
    using namespace DxUi;

    struct State final
    {
        WindowHost* host  = nullptr;
        StackPanel* stack = nullptr;
        std::unique_ptr<Panel> replacementRoot;
        Panel* replacementAddress   = nullptr;
        bool replaceRoot            = false;
        bool changeGapOnFirstBounds = false;
        bool reflowOnFirstBounds    = false;
        bool firstCallbackArmed     = true;
        size_t firstBoundsCalls     = 0u;
        size_t siblingBoundsCalls   = 0u;

        void RetireOwner()
        {
            if (replaceRoot)
            {
                replacementAddress = replacementRoot.get();
                host->SetRoot(std::move(replacementRoot));
            }
            else
            {
                stack->ClearChildren();
            }
        }
    };

    class LayoutCallbackChild final : public Control
    {
    public:
        LayoutCallbackChild(State& state, bool retiresOwner) noexcept : _state(&state), _retiresOwner(retiresOwner)
        {
        }

        void Paint(ControlHost& /*host*/) const override
        {
        }

    protected:
        void OnBoundsChanged() noexcept override
        {
            State* const state = _state;
            if (_retiresOwner)
            {
                ++state->firstBoundsCalls;
                if (state->changeGapOnFirstBounds)
                {
                    if (state->firstCallbackArmed)
                    {
                        state->firstCallbackArmed = false;
                        state->stack->SetGap(12.0f);
                    }
                }
                else if (state->reflowOnFirstBounds)
                {
                    if (state->firstCallbackArmed)
                    {
                        state->firstCallbackArmed = false;
                        state->stack->SetOrientation(StackOrientation::Horizontal);
                        state->stack->SetFlowDirection(FlowDirection::RightToLeft);
                    }
                }
                else
                {
                    state->RetireOwner();
                }
            }
            else
            {
                ++state->siblingBoundsCalls;
            }
        }

    private:
        State* _state      = nullptr;
        bool _retiresOwner = false;
    };

    for (const bool replaceRoot : {false, true})
    {
        State state;
        state.replaceRoot     = replaceRoot;
        state.replacementRoot = std::make_unique<Panel>();
        WindowHost host;
        state.host               = &host;
        auto root                = std::make_unique<Panel>();
        Panel* const rootAddress = root.get();
        auto* stack              = root->AddChild<StackPanel>();
        state.stack              = stack;
        Control* const first     = stack->AddChild<LayoutCallbackChild>(state, true);
        Control* const second    = stack->AddChild<LayoutCallbackChild>(state, false);
        stack->SetChildExtent(first, 20.0f);
        stack->SetChildExtent(second, 20.0f);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 100.0f));
        stack->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 80.0f));

        stack->ApplyLayout();

        Require(state.firstBoundsCalls == 1u, "the first child bounds callback runs once during StackPanel layout");
        Require(state.siblingBoundsCalls == 0u, "the outer layout never applies stale bounds to a sibling after reentrant retirement");
        if (replaceRoot)
        {
            Require(host.GetRoot() == state.replacementAddress, "the replacement root survives StackPanel layout retirement");
        }
        else
        {
            Require(host.GetRoot() == rootAddress && stack->GetLogicalChildCount() == 0u,
                    "clearing children during layout leaves the StackPanel alive and empty");
        }
    }

    // A layout setting changed by the first child's bounds callback invalidates the captured geometry. The outer
    // pass stops before writing a sibling from its stale layout inputs.
    {
        State state;
        state.changeGapOnFirstBounds = true;
        WindowHost host;
        state.host            = &host;
        auto root             = std::make_unique<Panel>();
        auto* stack           = root->AddChild<StackPanel>();
        state.stack           = stack;
        Control* const first  = stack->AddChild<LayoutCallbackChild>(state, true);
        Control* const second = stack->AddChild<LayoutCallbackChild>(state, false);
        stack->SetChildExtent(first, 20.0f);
        stack->SetChildExtent(second, 20.0f);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 100.0f));
        stack->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 80.0f));

        stack->ApplyLayout();

        Require(state.firstBoundsCalls == 1u && stack->GetGap() == 12.0f, "the first child can change the StackPanel gap during its bounds callback");
        Require(state.siblingBoundsCalls == 0u && second->GetBounds().bottom == 0.0f, "the stale outer pass leaves later children for the new layout inputs");
    }

    // A flow change triggers a nested layout. The outer vertical pass must not overwrite the horizontal RTL result.
    {
        State state;
        state.reflowOnFirstBounds = true;
        WindowHost host;
        state.host            = &host;
        auto root             = std::make_unique<Panel>();
        auto* stack           = root->AddChild<StackPanel>();
        state.stack           = stack;
        Control* const first  = stack->AddChild<LayoutCallbackChild>(state, true);
        Control* const second = stack->AddChild<LayoutCallbackChild>(state, false);
        stack->SetChildExtent(first, 20.0f);
        stack->SetChildExtent(second, 20.0f);
        stack->SetGap(12.0f);
        host.SetRoot(std::move(root));
        host.GetRoot()->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 100.0f));
        stack->SetBounds(D2D1::RectF(0.0f, 0.0f, 240.0f, 80.0f));

        stack->ApplyLayout();

        const D2D1_RECT_F secondBounds = second->GetBounds();
        Require(stack->GetOrientation() == StackOrientation::Horizontal && stack->GetFlowDirection() == FlowDirection::RightToLeft,
                "the first child's callback can start a nested RTL horizontal layout");
        Require(state.siblingBoundsCalls == 1u && secondBounds.left == 188.0f && secondBounds.right == 208.0f,
                "the nested layout completes once and the stale outer pass does not overwrite its sibling bounds");
    }
}

void TestMutableCallbackCaptureStatePersistsForScrollPanel()
{
    DxUi::ScrollPanel scroll;
    scroll.SetBounds(D2D1::RectF(0.0f, 0.0f, 200.0f, 100.0f));
    scroll.SetContentHeight(500.0f);

    std::vector<size_t> callbackCounts;
    scroll.SetOnScrollChanged([count = size_t{0u}, &callbackCounts](float) mutable { callbackCounts.push_back(++count); });
    scroll.SetScrollOffset(20.0f);
    scroll.SetScrollOffset(40.0f);
    Require(callbackCounts == std::vector<size_t>{1u, 2u}, "ScrollPanel retains mutable callback state across offset changes");
}

} // namespace

void RunControlTests()
{
    DXUI_RUN_TEST(TestLocalizedActionLayout);
    DXUI_RUN_TEST(TestPanelTakeChildPreservesInheritanceAndReparentsSafely);
    DXUI_RUN_TEST(TestPanelTakeChildRevalidatesAfterFocusRetirement);
    DXUI_RUN_TEST(TestTabTakeChildKeepsMetadataAndSelectionCoherent);
    DXUI_RUN_TEST(TestChildAndTabExtractionCancelLiveSliderDrafts);
    DXUI_RUN_TEST(TestPanelExtractionRevalidatesAfterCaptureCancellationRetiresOwner);
    DXUI_RUN_TEST(TestPanelOverlayDismissalCanRetireItsOwner);
    DXUI_RUN_TEST(TestScrollPanelOverlayDismissalCanRetireItsOwner);
    DXUI_RUN_TEST(TestMeasuredActionsFailureAndDirectionContracts);
    DXUI_RUN_TEST(TestScrollPanelChildCallbacksCanClearChildrenSafely);
    DXUI_RUN_TEST(TestPanelCallbacksCanRemoveSiblingsAndDestroyTheirOwner);
    DXUI_RUN_TEST(TestPanelTraversalCallbacksCanRetireOwnerSafely);
    DXUI_RUN_TEST(TestPageHostOutgoingPaintCanReplaceItsPageWithoutLeakingDrawingState);
    DXUI_RUN_TEST(TestPageHostHitTestingRejectsAChildFromAReplacedPage);
    DXUI_RUN_TEST(TestScrollPanelQueriesAndPaintStopAfterRootRetirement);
    DXUI_RUN_TEST(TestMutableCallbackCaptureStatePersistsForControlAndButton);
    DXUI_RUN_TEST(TestMutableCallbackCaptureStatePersistsForToggleAndColorSwatch);
    DXUI_RUN_TEST(TestStackPanelLayoutStopsAfterChildCallbacksRetireItsTree);
    DXUI_RUN_TEST(TestMutableCallbackCaptureStatePersistsForScrollPanel);
    DXUI_RUN_TEST(TestPageHostCallbacksCanDestroyTheirOwnerSafely);
    DXUI_RUN_TEST(TestGroupedGridHeaderClickTogglesCollapsedStateAndRehomesSelection);
    DXUI_RUN_TEST(TestToggleLayoutMetricsReserveTextLaneWhenLabelIsPresent);
    DXUI_RUN_TEST(TestToggleStateLabelsReserveTextLaneWithoutPrimaryLabel);
    DXUI_RUN_TEST(TestToggleStateLabelsFollowCheckedState);
    DXUI_RUN_TEST(TestFocusRingPaintPathsHandleMissingDeviceContext);
    DXUI_RUN_TEST(TestScrollPanelThumbGutterDragThroughWindowHost);
    DXUI_RUN_TEST(TestScrollPanelScrollCallbackCanReplaceItsOwnCallable);
    DXUI_RUN_TEST(TestScrollPanelScrollDispatchDoesNotCopyCallbackTargets);
    DXUI_RUN_TEST(TestScrollPanelScrollbarTrackCallbackCanDestroyItsOwner);
    DXUI_RUN_TEST(TestScrollPanelScrollbarDragCallbackCanDestroyItsOwner);
    DXUI_RUN_TEST(TestMenuBarLayoutCacheRecomputesHitRectsAfterLayoutInvalidations);
    DXUI_RUN_TEST(TestTabControlHeaderCacheRecomputesRectsAfterLayoutInvalidations);
    DXUI_RUN_TEST(TestTabControlLayoutRevalidatesPagesAfterCallbacks);
    DXUI_RUN_TEST(TestTabControlBodyDragReleaseOverCloseButtonDoesNotCloseTab);
    DXUI_RUN_TEST(TestTabControlReorderingPolicyPreservesStableHostIndices);
    DXUI_RUN_TEST(TestTabControlReorderingReportsStableMove);
    DXUI_RUN_TEST(TestToggleMouseActivationOnlyFiresToggledCallbackWithUpdatedState);
    DXUI_RUN_TEST(TestToggleMouseActivationCanReplaceRootSafely);
    DXUI_RUN_TEST(TestMenuBarActivationCanReplaceRootSafely);
    DXUI_RUN_TEST(TestControlsThatFocusThemselvesLeaveAControlTheFocusCallbackDestroyed);
    DXUI_RUN_TEST(TestColorSwatchStoresConfiguredArgbAndEmptyState);
    DXUI_RUN_TEST(TestTagPickerWrapsBadgesInsideInputFrame);
    DXUI_RUN_TEST(TestTagPickerSuggestionsTrackSelectedBadges);
    DXUI_RUN_TEST(TestTagPickerKeyboardNavigationCommitsFilteredSuggestionOnEnter);
    DXUI_RUN_TEST(TestToggleRightClickInvokesContextMenuWithoutChangingState);
    DXUI_RUN_TEST(TestCheckboxRightClickInvokesContextMenuWithoutChangingState);
    DXUI_RUN_TEST(TestControlContextMenuCallbackCanReplaceItsOwnCallable);
    DXUI_RUN_TEST(TestControlAccessibleInvokeCallbackCanDestroyItsOwner);
    DXUI_RUN_TEST(TestControlAccessibleInvokeResultRetainsMutableStateAndReturnsTheCallbackResult);
    DXUI_RUN_TEST(TestColorSwatchKeyboardCallbackCanDestroyItsOwner);
    DXUI_RUN_TEST(TestAccessibleInvokeReplacementCanRetireItsOwnerDuringOldCaptureCleanup);
    DXUI_RUN_TEST(TestPageIndicatorSelectionCallbackCanReplaceItsOwnCallable);
    DXUI_RUN_TEST(TestMnemonicTextIndexUsesExplicitAmpersandMnemonic);
    DXUI_RUN_TEST(TestMnemonicTextIndexTreatsEscapedAmpersandAsLiteralDisplayText);
    DXUI_RUN_TEST(TestGridCheckboxCellClickTogglesThroughDelegate);
    DXUI_RUN_TEST(TestDisabledGridCheckboxCellClickSelectsWithoutTogglingAndInvalidates);
    DXUI_RUN_TEST(TestGridCheckboxCellTextClickDoesNotToggle);
    DXUI_RUN_TEST(TestGridSpaceTogglesActiveCheckboxColumnAcrossRows);
    DXUI_RUN_TEST(TestGridSpaceOnDisabledCheckboxColumnIsHandledWithoutToggling);
    DXUI_RUN_TEST(TestDedicatedCheckboxColumnCentersIndicatorAndToggles);
    DXUI_RUN_TEST(TestToggleMetricsMatchPreferencesWidthBudget);
    DXUI_RUN_TEST(TestMnemonicTextIndexFindsFirstCaseInsensitiveMatch);
    DXUI_RUN_TEST(TestMnemonicTextIndexReturnsNoMatchWhenAbsent);
    DXUI_RUN_TEST(TestThroughputGraphHonorsMotionRainbowAndHighContrastContracts);
}
