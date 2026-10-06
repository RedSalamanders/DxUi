#include "../../src/Support/AnimationDispatcher.h"
#include "Controls.Tests.DxUiTestHelpers.h"

namespace
{

// A native tooltip's deadlines are on the UI thread's animation dispatcher clock, which a tick moves by the time since the last
// tick but by no more than the dispatcher's hitch clamp: a runner that stalls for a second moves it 50 ms. So the timer tests
// decide nothing by wall-clock time. They keep the dispatcher ticking with a subscription of their own, take a deadline from its
// clock and dispatch one message at a time, so that each tick is observed with the state it left.

// Dispatches this thread's messages one at a time, waiting for the next one without polling, until `reached` holds after one of
// them. The ten-second limit only ends a run whose ticks never come: a 100 ms delay takes about a tenth of a second.
template <class Condition> [[nodiscard]] bool DispatchMessagesUntil(Condition&& reached)
{
    constexpr ULONGLONG kLimitMs = 10'000u;
    const ULONGLONG started      = GetTickCount64();
    for (ULONGLONG elapsedMs = 0u; elapsedMs < kLimitMs; elapsedMs = GetTickCount64() - started)
    {
        MSG msg{};
        if (PeekMessageW(&msg, nullptr, 0u, 0u, PM_REMOVE) == FALSE)
        {
            static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(kLimitMs - elapsedMs), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
            continue;
        }
        static_cast<void>(DispatchQueuedMessageForTest(msg));
        if (reached())
        {
            return true;
        }
    }
    return false;
}

// Keeps the dispatcher ticking while it lives, so its clock stays the clock of its ticks: an idle dispatcher reads the wall clock.
class DispatcherTicks final
{
public:
    DispatcherTicks() noexcept : _subscriptionId(DxUi::Ui::AnimationDispatcher::GetInstance().Subscribe(&CountTick, &_tickCount))
    {
    }

    ~DispatcherTicks()
    {
        DxUi::Ui::AnimationDispatcher::GetInstance().Unsubscribe(_subscriptionId);
    }

    DispatcherTicks(const DispatcherTicks&)            = delete;
    DispatcherTicks& operator=(const DispatcherTicks&) = delete;

    // Whether the dispatcher delivered a tick, after which its clock is the time of its last tick.
    [[nodiscard]] bool WaitForTick()
    {
        const size_t ticksBefore = _tickCount;
        return _subscriptionId != 0u && DispatchMessagesUntil([this, ticksBefore] { return _tickCount != ticksBefore; });
    }

private:
    static bool CountTick(void* context, uint64_t /*nowTickMs*/) noexcept
    {
        ++*static_cast<size_t*>(context);
        return true;
    }

    size_t _tickCount        = 0u;
    uint64_t _subscriptionId = 0u;
};

// A tick moves the dispatcher clock by at most the hitch clamp, so a delay of twice the clamp has ticks before its deadline
// however long the runner stalls.
[[nodiscard]] uint64_t GetHideDelayWithTicksBeforeItsDeadlineMs() noexcept
{
    return 2u * DxUi::Ui::AnimationDispatcher::GetInstance().DebugGetHitchClampUsForTest() / 1'000u;
}

void TestTooltipLayerTrackingUpdateReusesVisibleTooltip()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(320, 200), handled));
    Require(handled, "host size update handled for tooltip tracking");

    const std::wstring tooltipText = L"Tracking tooltip";
    Require(host.SetTooltip(tooltipText, D2D1::Point2F(32.0f, 28.0f)), "initial tooltip set changes the visible tooltip state");
    const D2D1_RECT_F initialBounds = host.DebugGetTooltipBoundsDip();

    Require(! host.SetTooltip(tooltipText, D2D1::Point2F(32.0f, 28.0f)), "setting the same tooltip text at the same point is a no-op");
    Require(host.SetTooltip(tooltipText, D2D1::Point2F(144.0f, 92.0f)), "tracking tooltip moves when only the origin changes");
    const D2D1_RECT_F movedBounds = host.DebugGetTooltipBoundsDip();

    RequireRectHasArea(initialBounds, "initial tracking tooltip bounds have area");
    RequireRectHasArea(movedBounds, "moved tracking tooltip bounds have area");
    RequireFloatNear(movedBounds.right - movedBounds.left,
                     initialBounds.right - initialBounds.left,
                     0.5f,
                     "tracking tooltip keeps stable width when only the origin changes");
    RequireFloatNear(movedBounds.bottom - movedBounds.top,
                     initialBounds.bottom - initialBounds.top,
                     0.5f,
                     "tracking tooltip keeps stable height when only the origin changes");
    Require(movedBounds.left > initialBounds.left, "tracking tooltip moves horizontally with the updated origin");
    Require(movedBounds.top > initialBounds.top, "tracking tooltip moves vertically with the updated origin");
    Require(host.GetTooltipText() == tooltipText, "tracking tooltip keeps the existing text while moving");
    Require(host.ClearTooltip(), "clearing a visible tracking tooltip changes the tooltip state");
    Require(! host.ClearTooltip(), "clearing an already hidden tooltip is a no-op");
}

void TestTooltipLayerPrefersBelowRightWhenSpaceAllows()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(320, 180), handled));
    Require(handled, "host size update handled for tooltip preferred placement");

    const D2D1_POINT_2F origin = D2D1::Point2F(40.0f, 30.0f);
    host.SetTooltip(L"Short tooltip", origin);

    const D2D1_RECT_F bounds       = host.DebugGetTooltipBoundsDip();
    const D2D1_RECT_F clientBounds = host.GetClientBoundsDip();
    RequireRectHasArea(bounds, "tooltip preferred placement has area");
    Require(bounds.left > origin.x, "tooltip prefers placing to the right when space allows");
    Require(bounds.top > origin.y, "tooltip prefers placing below when space allows");
    Require(bounds.right <= clientBounds.right - 7.5f, "tooltip preferred placement stays within the right viewport margin");
    Require(bounds.bottom <= clientBounds.bottom - 7.5f, "tooltip preferred placement stays within the bottom viewport margin");
}

void TestTooltipLayerFlipsAboveNearBottomEdge()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(320, 120), handled));
    Require(handled, "host size update handled for tooltip vertical flip");

    const D2D1_POINT_2F origin = D2D1::Point2F(80.0f, 108.0f);
    host.SetTooltip(L"Bottom edge tooltip", origin);

    const D2D1_RECT_F bounds       = host.DebugGetTooltipBoundsDip();
    const D2D1_RECT_F clientBounds = host.GetClientBoundsDip();
    RequireRectHasArea(bounds, "tooltip vertical flip has area");
    Require(bounds.bottom <= origin.y, "tooltip flips above the origin near the bottom edge");
    Require(bounds.top >= clientBounds.top + 7.5f, "tooltip vertical flip respects the top viewport margin");
}

void TestTooltipLayerFlipsLeftNearRightEdge()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(220, 160), handled));
    Require(handled, "host size update handled for tooltip horizontal flip");

    const D2D1_POINT_2F origin = D2D1::Point2F(210.0f, 36.0f);
    host.SetTooltip(L"Right edge tooltip", origin);

    const D2D1_RECT_F bounds       = host.DebugGetTooltipBoundsDip();
    const D2D1_RECT_F clientBounds = host.GetClientBoundsDip();
    RequireRectHasArea(bounds, "tooltip horizontal flip has area");
    Require(bounds.right <= origin.x, "tooltip flips left of the origin near the right edge");
    Require(bounds.left >= clientBounds.left + 7.5f, "tooltip horizontal flip respects the left viewport margin");
}

void TestTooltipLayerWrapsLongTextAndStaysClamped()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(190, 150), handled));
    Require(handled, "host size update handled for tooltip wrap");

    const D2D1_POINT_2F origin = D2D1::Point2F(96.0f, 56.0f);
    host.SetTooltip(L"Short tooltip", origin);
    const D2D1_RECT_F shortBounds = host.DebugGetTooltipBoundsDip();

    host.SetTooltip(L"This is a much longer tooltip string that should wrap onto multiple lines and remain fully visible inside the viewport.", origin);
    const D2D1_RECT_F longBounds   = host.DebugGetTooltipBoundsDip();
    const D2D1_RECT_F clientBounds = host.GetClientBoundsDip();

    RequireRectHasArea(longBounds, "wrapped tooltip has area");
    Require((longBounds.bottom - longBounds.top) > (shortBounds.bottom - shortBounds.top), "wrapped tooltip grows taller than the short tooltip");
    Require((longBounds.right - longBounds.left) <= (clientBounds.right - clientBounds.left) - 15.0f, "wrapped tooltip clamps width to the viewport");
    Require(longBounds.left >= clientBounds.left + 7.5f, "wrapped tooltip respects the left viewport margin");
    Require(longBounds.top >= clientBounds.top + 7.5f, "wrapped tooltip respects the top viewport margin");
    Require(longBounds.right <= clientBounds.right - 7.5f, "wrapped tooltip respects the right viewport margin");
    Require(longBounds.bottom <= clientBounds.bottom - 7.5f, "wrapped tooltip respects the bottom viewport margin");
}

void TestTooltipLayerHideDelayExpiresAfterTimerTicks()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    auto& dispatcher = Ui::AnimationDispatcher::GetInstance();
    DispatcherTicks ticks;
    Require(ticks.WaitForTick(), "tracking tooltip hide-delay test starts with the animation dispatcher ticking");
    const uint64_t hideDelayMs = GetHideDelayWithTicksBeforeItsDeadlineMs();

    const std::wstring tooltipText = L"Tracking tooltip";
    Require(window.Host().SetTooltip(tooltipText, D2D1::Point2F(24.0f, 24.0f)), "tracking tooltip hide-delay test starts with a visible tooltip");
    const uint64_t hideTickMs = dispatcher.GetCurrentTickMs() + hideDelayMs;
    Require(window.Host().BeginTooltipHideDelay(hideDelayMs), "tracking tooltip hide-delay scheduling succeeds");
    Require(window.Host().HasTooltip(), "tracking tooltip remains visible immediately after hide-delay scheduling");

    uint64_t lastTickMs        = dispatcher.GetCurrentTickMs();
    size_t ticksBeforeDeadline = 0u;
    bool visibleBeforeDeadline = true;
    const bool reachedDeadline = DispatchMessagesUntil([&]
    {
        const uint64_t tickMs = dispatcher.GetCurrentTickMs();
        if (tickMs == lastTickMs)
        {
            return false;
        }
        lastTickMs = tickMs;
        if (tickMs >= hideTickMs)
        {
            return true;
        }
        ++ticksBeforeDeadline;
        visibleBeforeDeadline = visibleBeforeDeadline && window.Host().HasTooltip();
        return false;
    });
    Require(ticksBeforeDeadline != 0u && visibleBeforeDeadline, "tracking tooltip remains visible before the hide delay elapses");
    Require(reachedDeadline, "the dispatcher's ticks reach the tracking tooltip's hide deadline");
    Require(! window.Host().HasTooltip(), "tracking tooltip clears after the hide delay elapses");
}

void TestTooltipDeadlinesUseCurrentDispatcherClockAfterIdleHostTick()
{
    using namespace DxUi;

    constexpr uint64_t staleTickGapMs = 3'000u;
    constexpr uint64_t hideDelayMs    = 1'000u;
    const uint64_t currentTickMs      = DxUi::Ui::AnimationDispatcher::GetInstance().GetCurrentTickMs();
    Require(currentTickMs > staleTickGapMs, "tooltip stale-tick regression requires a valid dispatcher clock epoch");
    const uint64_t staleTickMs = currentTickMs - staleTickGapMs;

    WindowHost host;
    static_cast<void>(host.DebugAnimationTickForTest(staleTickMs));
    Require(host.SetTooltip(L"Tracking tooltip", D2D1::Point2F(24.0f, 24.0f)), "tooltip stale-tick regression starts with a visible tracking tooltip");
    Require(host.BeginTooltipHideDelay(hideDelayMs), "tooltip stale-tick regression schedules a long hide delay");
    static_cast<void>(host.DebugAnimationTickForTest(currentTickMs));
    Require(host.HasTooltip(), "tracking tooltip hide delay is based on the current dispatcher clock instead of the host's stale last tick");

    static_cast<void>(host.ClearTooltip());
    static_cast<void>(host.DebugAnimationTickForTest(staleTickMs));
    Require(host.SetTooltipDelayed(L"Supplemental tooltip", D2D1::Point2F(48.0f, 36.0f)),
            "tooltip stale-tick regression schedules a delayed supplemental tooltip");
    static_cast<void>(host.DebugAnimationTickForTest(currentTickMs));
    Require(! host.HasTooltip(), "supplemental tooltip show delay does not expire from the host's stale last tick");
    Require(host.DebugGetPendingTooltipText() == L"Supplemental tooltip", "supplemental tooltip remains pending until the current dispatcher-clock deadline");
}

void TestTooltipLayerTrackingMoveCancelsPendingHideDelay()
{
    using namespace DxUi;

    AttachedHostWindow window;
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();

    auto& dispatcher = Ui::AnimationDispatcher::GetInstance();
    DispatcherTicks ticks;
    Require(ticks.WaitForTick(), "tracking tooltip cancel test starts with the animation dispatcher ticking");
    const uint64_t hideDelayMs = GetHideDelayWithTicksBeforeItsDeadlineMs();

    const std::wstring tooltipText = L"Tracking tooltip";
    Require(window.Host().SetTooltip(tooltipText, D2D1::Point2F(24.0f, 24.0f)), "tracking tooltip cancel test starts with a visible tooltip");
    const uint64_t hideTickMs = dispatcher.GetCurrentTickMs() + hideDelayMs;
    Require(window.Host().BeginTooltipHideDelay(hideDelayMs), "tracking tooltip cancel test schedules hide");

    // The pointer moves one tick into the delay, which the hitch clamp keeps before the deadline, so the hide is still pending.
    Require(ticks.WaitForTick() && dispatcher.GetCurrentTickMs() < hideTickMs && window.Host().HasTooltip(),
            "tracking tooltip is still visible one tick into its hide delay");
    Require(window.Host().SetTooltip(tooltipText, D2D1::Point2F(96.0f, 72.0f)), "tracking tooltip movement updates the tooltip and cancels the pending hide");

    Require(DispatchMessagesUntil([&] { return dispatcher.GetCurrentTickMs() >= hideTickMs; }), "the dispatcher's ticks pass the canceled hide deadline");
    Require(window.Host().HasTooltip(), "tracking tooltip movement cancels the pending hide delay");
    Require(window.Host().GetTooltipText() == tooltipText, "tracking tooltip keeps the same text after canceling the hide delay");
}

void TestGridTooltipTracksPointerWithinSameCell()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(520, 140), handled));
    Require(handled, "host size update handled for grid tooltip tracking");
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 520.0f, 140.0f));
    host.SetRoot(std::move(root));

    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"Tracked";
    cellData.tooltipText = L"Tracked tooltip text that should follow the pointer within the same cell.";
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(host, 0u, 0u);
    const float cellCenterX             = (metrics.cellRect.left + metrics.cellRect.right) * 0.5f;
    const float cellCenterY             = (metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f;
    const D2D1_POINT_2F firstPoint      = D2D1::Point2F(cellCenterX - 20.0f, cellCenterY);
    const D2D1_POINT_2F secondPoint     = D2D1::Point2F(cellCenterX + 20.0f, cellCenterY);

    Require(grid->OnMouseMove(host, firstPoint, 0), "grid long-text hover is handled for tooltip tracking");
    const D2D1_RECT_F firstBounds = host.DebugGetTooltipBoundsDip();
    const std::wstring firstTooltipText(host.GetTooltipText());

    Require(grid->OnMouseMove(host, secondPoint, 0), "grid repeated same-cell hover is handled for tooltip tracking");
    const D2D1_RECT_F secondBounds = host.DebugGetTooltipBoundsDip();

    RequireRectHasArea(firstBounds, "grid tooltip tracking starts with tooltip bounds");
    RequireRectHasArea(secondBounds, "grid tooltip tracking keeps tooltip bounds after pointer movement");
    Require(secondBounds.left > firstBounds.left, "grid tooltip tracking moves the tooltip when the pointer moves within the same cell");
    Require(host.GetTooltipText() == firstTooltipText, "grid tooltip tracking keeps the same tooltip text within the same hovered cell");
}

void TestInteractiveTooltipSurvivesEmptySupplementalTargetPass()
{
    using namespace DxUi;

    AttachedHostWindow window;
    auto root  = std::make_unique<Panel>();
    auto* grid = root->AddChild<Grid>();
    grid->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 140.0f));
    window.Host().SetRoot(std::move(root));

    const std::wstring tooltipText = L"Interactive grid tooltip";
    GridCellData cellData;
    cellData.kind        = GridCellKind::Text;
    cellData.text        = L"Tracked";
    cellData.tooltipText = tooltipText;
    SingleCellGridModel model(std::move(cellData));
    grid->SetModel(&model);

    const GridCellLayoutMetrics metrics = grid->GetCellLayoutMetrics(window.Host(), 0u, 0u);
    const int dpi                       = static_cast<int>(GetDpiForWindow(window.Hwnd()));
    const LONG x                        = MulDiv(static_cast<int>((metrics.cellRect.left + metrics.cellRect.right) * 0.5f), dpi, USER_DEFAULT_SCREEN_DPI);
    const LONG y                        = MulDiv(static_cast<int>((metrics.cellRect.top + metrics.cellRect.bottom) * 0.5f), dpi, USER_DEFAULT_SCREEN_DPI);

    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
    Require(window.Host().HasTooltip() && window.Host().GetTooltipText() == tooltipText,
            "an interactive control tooltip remains visible when the supplemental hit-test has no target");
}

void TestTreeTooltipTracksPointerWithinSameRow()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(420, 140), handled));
    Require(handled, "host size update handled for tree tooltip tracking");

    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 420.0f, 140.0f));
    host.SetRoot(std::move(root));

    MutableTreeModel model;
    model.SetVisibleItems({TreeItemData{
        .id          = 1u,
        .text        = L"Plugins",
        .tooltipText = L"Tracked tree tooltip",
    }});
    tree->SetModel(&model);

    const TreeItemLayoutMetrics metrics = tree->GetItemLayoutMetrics(host, 0u);
    const float rowCenterY              = (metrics.rowRect.top + metrics.rowRect.bottom) * 0.5f;
    const D2D1_POINT_2F firstPoint      = D2D1::Point2F(metrics.textRect.left + 12.0f, rowCenterY);
    const D2D1_POINT_2F secondPoint     = D2D1::Point2F(metrics.textRect.left + 152.0f, rowCenterY);

    Require(tree->OnMouseMove(host, firstPoint, 0), "tree tooltip hover is handled for tooltip tracking");
    const D2D1_RECT_F firstBounds = host.DebugGetTooltipBoundsDip();
    const std::wstring firstTooltipText(host.GetTooltipText());

    Require(tree->OnMouseMove(host, secondPoint, 0), "tree repeated same-row hover is handled for tooltip tracking");
    const D2D1_RECT_F secondBounds = host.DebugGetTooltipBoundsDip();

    RequireRectHasArea(firstBounds, "tree tooltip tracking starts with tooltip bounds");
    RequireRectHasArea(secondBounds, "tree tooltip tracking keeps tooltip bounds after pointer movement");
    Require(secondBounds.left > firstBounds.left, "tree tooltip tracking moves the tooltip when the pointer moves within the same row");
    Require(host.GetTooltipText() == firstTooltipText, "tree tooltip tracking keeps the same tooltip text within the same hovered row");
}

void TestTreeTooltipFallsBackToClippedItemText()
{
    using namespace DxUi;

    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(180, 120), handled));
    Require(handled, "host size update handled for tree clipped-text tooltip");

    auto root  = std::make_unique<Panel>();
    auto* tree = root->AddChild<Tree>();
    tree->SetBounds(D2D1::RectF(0.0f, 0.0f, 180.0f, 120.0f));
    host.SetRoot(std::move(root));

    const std::wstring clippedText = L"Viewer plugin configuration with a very long label";

    MutableTreeModel model;
    model.SetVisibleItems({TreeItemData{
        .id        = 1u,
        .text      = clippedText,
        .badgeText = L"Live",
    }});
    tree->SetModel(&model);

    const TreeItemLayoutMetrics metrics = tree->GetItemLayoutMetrics(host, 0u);
    Require((metrics.textRect.right - metrics.textRect.left) < 120.0f, "tree clipped-text tooltip test narrows the visible text slot");

    const D2D1_POINT_2F hoverPoint = D2D1::Point2F(metrics.textRect.left + 8.0f, (metrics.rowRect.top + metrics.rowRect.bottom) * 0.5f);
    Require(tree->OnMouseMove(host, hoverPoint, 0), "tree clipped-text hover is handled");

    const D2D1_RECT_F tooltipBounds = host.DebugGetTooltipBoundsDip();
    RequireRectHasArea(tooltipBounds, "tree clipped-text hover exposes tooltip bounds");
    Require(host.GetTooltipText() == clippedText, "tree clipped-text hover falls back to the full item text");

    Require(tree->OnMouseLeave(host), "tree mouse leave clears clipped-text tooltip state");
    Require(host.HasTooltip(), "tree clipped-text tooltip begins a delayed hide on mouse leave");
}

void TestPassiveSupplementalTooltipUsesHostHitTestingDelayLifetimeAndClickThrough()
{
    using namespace DxUi;

    AttachedHostWindow window;
    uint32_t clickCount = 0u;
    auto root           = std::make_unique<Panel>();
    auto* button        = root->AddChild<Button>(L"Open details");
    button->SetBounds(D2D1::RectF(16.0f, 12.0f, 176.0f, 52.0f));
    button->SetOnClick([&clickCount] { ++clickCount; });
    auto* passiveRegion = root->AddChild<Label>(L"3");
    passiveRegion->SetBounds(D2D1::RectF(24.0f, 18.0f, 72.0f, 46.0f));
    passiveRegion->SetTooltipText(L"Completed with partial results or warnings: 3");
    passiveRegion->SetAccessibleHelpText(L"Completed with partial results or warnings: 3");
    window.Host().SetRoot(std::move(root));
    // A host ticks its tooltip only while its window is visible.
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);

    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(40, 28));
    Require(! window.Host().HasTooltip(), "passive tooltip remains hidden before the shared show delay elapses");
    Require(window.Host().DebugGetPendingTooltipText() == L"Completed with partial results or warnings: 3",
            "real host hit testing reaches the deepest passive supplemental-tooltip region");
    // The show delay is the user's mouse hover time, at most 2.5 s on the dispatcher's clock, and the display lifetime runs five
    // seconds from the tick that shows the tooltip, so a tick past every show delay fixes when the lifetime ends.
    const uint64_t shownTickMs = Ui::AnimationDispatcher::GetInstance().GetCurrentTickMs() + 10'000u;
    static_cast<void>(window.Host().DebugAnimationTickForTest(shownTickMs));
    Require(window.Host().HasTooltip() && window.Host().GetTooltipText() == passiveRegion->GetAccessibleHelpText(),
            "passive pointer tooltip matches the region's accessibility HelpText after the show delay");
    static_cast<void>(window.Host().DebugAnimationTickForTest(shownTickMs + 4'999u));
    Require(window.Host().HasTooltip(), "stationary passive tooltip stays for its five-second display lifetime");
    static_cast<void>(window.Host().DebugAnimationTickForTest(shownTickMs + 5'000u));
    Require(! window.Host().HasTooltip(), "stationary passive tooltip auto-hides after the five-second display lifetime");

    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(40, 28));
    static_cast<void>(window.Host().DebugAdvanceTooltipDelayForTest());
    Require(window.Host().HasTooltip(), "pointer movement shows the passive tooltip again after auto-hide and the show delay");

    SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(40, 28));
    SendMessageW(window.Hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(40, 28));
    Require(clickCount == 1u, "passive tooltip region remains click-through to the underlying interactive control");

    // The click released the mouse capture it took, and losing capture clears the tooltip.
    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(40, 28));
    Require(! window.Host().DebugGetPendingTooltipText().empty(), "pointer movement can schedule the passive tooltip again after a click hid it");
    passiveRegion->SetTooltipText(L"Updated warning details: 3");
    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(40, 28));
    Require(window.Host().DebugGetPendingTooltipText() == L"Updated warning details: 3",
            "the next retained-tree hit test invalidates and replaces a pending passive-tooltip request after its text changes");

    auto replacementRoot = std::make_unique<Panel>();
    replacementRoot->AddChild<Button>(L"Replacement")->SetBounds(D2D1::RectF(16.0f, 12.0f, 176.0f, 52.0f));
    window.Host().SetRoot(std::move(replacementRoot));
    Require(window.Host().DebugGetPendingTooltipText().empty() && ! window.Host().HasTooltip(),
            "retained-tree rebuild cancels a pending passive tooltip without dereferencing the retired target");

    SendMessageW(window.Hwnd(), WM_MOUSELEAVE, 0, 0);
    Require(! window.Host().HasTooltip(), "host mouse-leave clears passive tooltip state");
}

} // namespace

void RunTooltipTests()
{
    DXUI_RUN_TEST(TestTooltipLayerTrackingUpdateReusesVisibleTooltip);
    DXUI_RUN_TEST(TestTooltipLayerPrefersBelowRightWhenSpaceAllows);
    DXUI_RUN_TEST(TestTooltipLayerFlipsAboveNearBottomEdge);
    DXUI_RUN_TEST(TestTooltipLayerFlipsLeftNearRightEdge);
    DXUI_RUN_TEST(TestTooltipLayerWrapsLongTextAndStaysClamped);
    DXUI_RUN_TEST(TestTooltipLayerHideDelayExpiresAfterTimerTicks);
    DXUI_RUN_TEST(TestTooltipDeadlinesUseCurrentDispatcherClockAfterIdleHostTick);
    DXUI_RUN_TEST(TestTooltipLayerTrackingMoveCancelsPendingHideDelay);
    DXUI_RUN_TEST(TestGridTooltipTracksPointerWithinSameCell);
    DXUI_RUN_TEST(TestInteractiveTooltipSurvivesEmptySupplementalTargetPass);
    DXUI_RUN_TEST(TestTreeTooltipTracksPointerWithinSameRow);
    DXUI_RUN_TEST(TestTreeTooltipFallsBackToClippedItemText);
    DXUI_RUN_TEST(TestPassiveSupplementalTooltipUsesHostHitTestingDelayLifetimeAndClickThrough);
}
