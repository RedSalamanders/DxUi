#include "Controls.Tests.DxUiTestHelpers.h"
#include "Embedded.Tests.TooltipHoverPath.h"
#include "GraphicsFixture.h"
#include "Support/AnimationDispatcher.h"
#include "Support/WorkCounters.h"

#include <functional>
#include <iostream>
#include <utility>

namespace
{

struct ReentrantTooltipCaptureState
{
    size_t captureLostCount = 0u;
};

// Existing identity/RAII/reference traits intentionally delete these implicit operations.
// Preserve aggregate and overload behavior. Owner: repository maintainers; review: 2027-01-31.
#pragma warning(push)
#pragma warning(disable : 4625 4626 5026 5027)
class ReentrantTooltipCaptureControl final : public DxUi::Control
{
public:
    explicit ReentrantTooltipCaptureControl(ReentrantTooltipCaptureState& state) : _state(&state)
    {
    }

    void Paint(DxUi::WindowHost& /*host*/) const override
    {
    }

    void OnCaptureLost(DxUi::WindowHost& host) override
    {
        ++_state->captureLostCount;
        static_cast<void>(host.InspectTooltip(L"Newer callback inspection", D2D1::Point2F(40.0f, 40.0f)));
    }

private:
    ReentrantTooltipCaptureState* _state = nullptr;
};
#pragma warning(pop)

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

void RequireRectNear(const D2D1_RECT_F& actual, const D2D1_RECT_F& expected, const char* message)
{
    Require(std::abs(actual.left - expected.left) <= 0.5f && std::abs(actual.top - expected.top) <= 0.5f && std::abs(actual.right - expected.right) <= 0.5f &&
                std::abs(actual.bottom - expected.bottom) <= 0.5f,
            message);
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

void TestTooltipLayerClampsBothAxesInTinyViewport()
{
    using namespace DxUi;
    WindowHost host;
    bool handled = false;
    static_cast<void>(host.HandleMessage(nullptr, WM_SIZE, 0, MAKELPARAM(12, 9), handled));
    host.SetTooltip(std::wstring(2000u, L'W'), D2D1::Point2F(1000, -1000));
    const auto bounds = host.DebugGetTooltipBoundsDip();
    const auto client = host.GetClientBoundsDip();
    RequireRectHasArea(bounds, "tiny tooltip viewport still has a bounded visible surface");
    Require(bounds.left >= client.left && bounds.top >= client.top && bounds.right <= client.right && bounds.bottom <= client.bottom,
            "even a viewport smaller than normal padding contains the entire tooltip");
}

void TestTooltipInspectionScrollsWithoutChangingFocusOrValue()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Existing focus");
    button->SetBounds(D2D1::RectF(0, 0, 180, 28));
    window.Host().SetRoot(std::move(root));
    window.Host().SetFocusControl(button, false);
    std::wstring value = L"FIRST LINE\n";
    for (size_t line = 0u; line < 100u; ++line)
        value += L"Long inspection content that wraps inside this viewport.\n";
    value += L"LAST LINE";
    Require(window.Host().InspectTooltip(value, D2D1::Point2F(24, 40)), "full-value inspection opens");
    const auto bounds = window.Host().DebugGetTooltipBoundsDip();
    const auto client = window.Host().GetClientBoundsDip();
    Require(bounds.bottom <= client.bottom && bounds.right <= client.right, "tall inspection is bounded by the client viewport");
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages();
    RedrawWindow(window.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    window.PumpMessages();
    WindowHostBitmapCapture first;
    Require(window.Host().DebugCaptureBitmap(first), "capture the first inspection page without desktop activation");
    Require(! window.Host().BeginTooltipHideDelay(0u) && ! window.Host().SetTooltipDelayed(L"Hover", D2D1::Point2F()),
            "inspection survives hover deadlines and replacement");
    bool handled = false;
    static_cast<void>(window.Host().HandleMessage(window.Hwnd(), WM_KEYDOWN, VK_END, 0, handled));
    Require(handled, "host routes End to the inspection before the focused control");
    WindowHostBitmapCapture last;
    Require(window.Host().DebugCaptureBitmap(last) && first.bgraPixels != last.bgraPixels,
            "End reveals a different full-value page within the same bounded surface");
    Require(window.Host().DebugIsTooltipInspectionAtEnd(), "End places the actual final glyph cluster inside the clipped text viewport");
    Require(window.Host().HandleTooltipInspectionKey(VK_HOME), "Home returns to the first page");
    const auto inside = D2D1::Point2F((bounds.left + bounds.right) * 0.5f, bounds.top + 30.0f);
    Require(window.Host().HandleTooltipInspectionPointer(WM_LBUTTONDOWN, inside) &&
                window.Host().HandleTooltipInspectionPointer(WM_MOUSEMOVE, D2D1::Point2F(inside.x, inside.y - 80.0f)) &&
                window.Host().HandleTooltipInspectionPointer(WM_LBUTTONUP, inside),
            "touch-style drag scrolls the inspection without a control capture");
    WindowHostBitmapCapture dragged;
    Require(window.Host().DebugCaptureBitmap(dragged) && first.bgraPixels != dragged.bgraPixels, "drag reveals later inspection text");
    Require(window.Host().GetFocusControl() == button && window.Host().GetTooltipText() == value,
            "scrolling keeps logical focus and the complete Unicode value");
    Require(window.Host().HandleTooltipInspectionKey(VK_ESCAPE) && ! window.Host().HasTooltip(), "Escape dismisses persistent inspection");
}

void TestTooltipInspectionMirrorsMixedScriptLayoutAndPlacement()
{
    using namespace DxUi;
    AttachedHostWindow window;
    SetWindowPos(window.Hwnd(), nullptr, -32000, -32000, 920, 600, SWP_NOZORDER | SWP_NOACTIVATE);
    auto root   = std::make_unique<Panel>();
    auto* panel = root.get();
    window.Host().SetRoot(std::move(root));
    const D2D1_POINT_2F origin       = D2D1::Point2F(460.0f, 180.0f);
    constexpr std::wstring_view text = L"مرحبا — ACME 123";

    panel->SetFlowDirection(FlowDirection::LeftToRight);
    Require(window.Host().InspectTooltip(std::wstring(text), origin), "LTR mixed-script inspection opens");
    const auto ltrBounds = window.Host().DebugGetTooltipBoundsDip();
    Require(ltrBounds.left > origin.x, "LTR inspection prefers the right side of its origin");
    Require(window.Host().HandleTooltipInspectionKey(VK_ESCAPE), "LTR inspection closes");

    panel->SetFlowDirection(FlowDirection::RightToLeft);
    Require(window.Host().InspectTooltip(std::wstring(text), origin), "RTL mixed-script inspection opens");
    const auto rtlBounds = window.Host().DebugGetTooltipBoundsDip();
    Require(rtlBounds.right < origin.x, "RTL inspection mirrors placement to the left of its origin");
    Require(rtlBounds.right < ltrBounds.left, "RTL and LTR mixed-script inspection placements differ across the origin");
    Require(window.Host().GetTooltipText() == text, "flow mirroring preserves the original mixed-script text");
}

void TestTooltipInspectionOutsideDismissalConsumesWholeGesture()
{
    using namespace DxUi;
    AttachedHostWindow window;
    auto root    = std::make_unique<Panel>();
    auto* button = root->AddChild<Button>(L"Underlying action");
    button->SetBounds(D2D1::RectF(200.0f, 120.0f, 310.0f, 170.0f));
    size_t activations = 0u;
    button->SetOnClick([&] { ++activations; });
    window.Host().SetRoot(std::move(root));
    Require(window.Host().InspectTooltip(L"Persistent inspection", D2D1::Point2F(10.0f, 10.0f)), "inspection opens for dismissal test");
    constexpr LPARAM outside = MAKELPARAM(250, 140);
    SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, outside);
    Require(! window.Host().IsTooltipInspectionActive(), "outside down dismisses inspection");
    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, MK_LBUTTON, outside);
    SendMessageW(window.Hwnd(), WM_MOUSELEAVE, 0, 0);
    SendMessageW(window.Hwnd(), WM_RBUTTONUP, 0, outside);
    SendMessageW(window.Hwnd(), WM_LBUTTONUP, 0, outside);
    Require(activations == 0u, "native dismissal survives leave and unrelated release, consuming the initiating release");
    SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, outside);
    SendMessageW(window.Hwnd(), WM_LBUTTONUP, 0, outside);
    Require(activations == 1u, "the next independent native gesture reaches the underlying action");

    Require(window.Host().InspectTooltip(L"Inspection replaced during a gesture", D2D1::Point2F(10.0f, 10.0f)), "inspection reopens for replacement coverage");
    SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, outside);
    Require(window.Host().InspectTooltip(L"Replacement inspection", D2D1::Point2F(20.0f, 20.0f)), "a replacement inspection opens during dismissal");
    Require(GetCapture() == window.Hwnd(), "replacement preserves capture until the initiating gesture completes");
    SendMessageW(window.Hwnd(), WM_MOUSEMOVE, MK_LBUTTON, outside);
    SendMessageW(window.Hwnd(), WM_LBUTTONUP, 0, outside);
    Require(GetCapture() != window.Hwnd() && window.Host().GetTooltipText() == L"Replacement inspection",
            "the actual HWND capture-change raised by the old gesture release preserves the replacement inspection");
    SendMessageW(window.Hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, outside);
    Require(GetCapture() == window.Hwnd(), "a fresh outside dismissal owns the host capture");
    window.Host().ClearTooltip();
    Require(GetCapture() != window.Hwnd() && ! window.Host().HasTooltip(), "explicit clear releases only the dismissal capture and removes the overlay");
    SendMessageW(window.Hwnd(), WM_LBUTTONUP, 0, outside);
    Require(activations == 1u, "clearing during the dismissing press still cannot invoke the underlying action");
}

void TestTooltipInspectionCaptureCallbackWinsReentrantOpen()
{
    using namespace DxUi;
    AttachedHostWindow window;
    ReentrantTooltipCaptureState state;
    auto root     = std::make_unique<Panel>();
    auto* capture = root->AddChild<ReentrantTooltipCaptureControl>(state);
    window.Host().SetRoot(std::move(root));
    window.Host().CaptureMouse(capture);
    Require(window.Host().InspectTooltip(L"Outer inspection", D2D1::Point2F(20.0f, 20.0f)), "outer inspection returns the reentrant active state");
    Require(state.captureLostCount == 1u, "opening inspection cancels the current captured control exactly once");
    Require(window.Host().GetTooltipText() == L"Newer callback inspection", "a newer inspection opened by capture cancellation is preserved");
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

// A row's tooltip is anchored where the pointer entered the row (W1.6); it no longer follows the pointer within the row.
void TestTreeTooltipStaysAtRowEntryOriginWithinSameRow()
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
    RequireRectNear(secondBounds, firstBounds, "tree tooltip stays at the row-entry origin when the pointer moves within the same row");
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

// The tooltip hover path (kTooltipHoverPathMoves, TooltipHoverPathPixel, MakeTooltipHoverPathRoot) is shared with the
// FX14 performance fixture: Embedded.Tests.TooltipHoverPath.h.

// W1.6: while a tooltip waits for its show delay, a pointer moving inside the same control only moves the pending origin.
// That changes no pixels, so it must not invalidate the window; the tooltip still appears at the last pointer origin.
void TestTooltipPendingOriginMovesDoNotInvalidateNativeHost()
{
    using namespace DxUi;

    AttachedHostWindow window;
    constexpr std::wstring_view tooltipText = L"Pending origin help";
    window.Host().SetRoot(MakeTooltipHoverPathRoot(tooltipText));
    const UINT dpi   = GetDpiForWindow(window.Hwnd());
    const auto toDip = [dpi](POINT pixel) noexcept
    { return D2D1::Point2F(static_cast<float>(pixel.x) * 96.0f / static_cast<float>(dpi), static_cast<float>(pixel.y) * 96.0f / static_cast<float>(dpi)); };
    const auto movePixel = [&](POINT pixel) { SendMessageW(window.Hwnd(), WM_MOUSEMOVE, 0, MAKELPARAM(pixel.x, pixel.y)); };

    movePixel(TooltipHoverPathPixel(0u));
    Require(! window.Host().HasTooltip() && window.Host().DebugGetPendingTooltipText() == tooltipText, "entering the control schedules its tooltip");

    const uint64_t invalidationsBefore = window.Host().DebugGetInvalidateCount();
    for (size_t move = 1u; move <= kTooltipHoverPathMoves; ++move)
        movePixel(TooltipHoverPathPixel(move));
    const uint64_t pendingMoveInvalidations = window.Host().DebugGetInvalidateCount() - invalidationsBefore;
    std::cerr << "    [W1.6] native pending-origin moves: " << kTooltipHoverPathMoves << ", invalidations: " << pendingMoveInvalidations << '\n';
    Require(pendingMoveInvalidations == 0u, "50 moves inside one control during the hover delay cause 0 native invalidations");
    Require(! window.Host().HasTooltip() && window.Host().DebugGetPendingTooltipText() == tooltipText, "the tooltip is still pending after the moves");

    Require(window.Host().DebugAdvanceTooltipDelayForTest() && window.Host().HasTooltip(), "the injected clock passes the show delay");
    const D2D1_RECT_F shownBounds = window.Host().DebugGetTooltipBoundsDip();

    Require(window.Host().ClearTooltip(), "reference: clear the shown tooltip");
    Require(window.Host().SetTooltip(std::wstring(tooltipText), toDip(TooltipHoverPathPixel(0u))), "reference: tooltip at the entry origin");
    const D2D1_RECT_F entryBounds = window.Host().DebugGetTooltipBoundsDip();
    Require(window.Host().SetTooltip(std::wstring(tooltipText), toDip(TooltipHoverPathPixel(kTooltipHoverPathMoves))), "reference: tooltip at the last origin");
    const D2D1_RECT_F lastBounds = window.Host().DebugGetTooltipBoundsDip();
    RequireRectHasArea(shownBounds, "the shown tooltip has area");
    Require(lastBounds.left > entryBounds.left && lastBounds.top > entryBounds.top, "the hover path's entry and last origins place the tooltip differently");
    RequireRectNear(shownBounds, lastBounds, "the tooltip shows at the last pointer origin, not where the pointer entered");
}

// The embedded host reports invalidation through its preparation request and shares the native tooltip layer.
void TestTooltipPendingOriginMovesDoNotInvalidateEmbeddedHost()
{
    using namespace DxUi;

    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "create the embedded tooltip WARP device");
    std::shared_ptr<GraphicsDevice> pool;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), pool), "share the embedded tooltip graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(pool), "attach the embedded tooltip view");
    constexpr std::wstring_view tooltipText = L"Pending origin help";
    view.Controls().SetRoot(MakeTooltipHoverPathRoot(tooltipText));
    constexpr UINT width  = 320u;
    constexpr UINT height = 180u;
    RequireSucceeded(view.Prepare(width, height), "prepare the embedded tooltip view");
    const auto movePixel = [&view](POINT pixel)
    {
        return view.DispatchPointer(
            PointerEvent{.action = PointerAction::Move, .xPixels = static_cast<float>(pixel.x), .yPixels = static_cast<float>(pixel.y)});
    };

    static_cast<void>(movePixel(TooltipHoverPathPixel(0u)));
    Require(! view.Controls().HasTooltip() && view.Controls().DebugGetPendingTooltipText() == tooltipText,
            "entering the embedded control schedules its tooltip");
    RequireSucceeded(view.Prepare(width, height), "prepare after the tooltip was scheduled");
    Require(! view.NeedsPreparation(), "the embedded view is clean before the hover moves");

    size_t dirtyMoves = 0u;
    for (size_t move = 1u; move <= kTooltipHoverPathMoves; ++move)
    {
        static_cast<void>(movePixel(TooltipHoverPathPixel(move)));
        if (view.NeedsPreparation())
        {
            ++dirtyMoves;
            RequireSucceeded(view.Prepare(width, height), "prepare after a dirty hover move");
        }
    }
    std::cerr << "    [W1.6] embedded pending-origin moves: " << kTooltipHoverPathMoves << ", invalidations: " << dirtyMoves << '\n';
    Require(dirtyMoves == 0u, "50 moves inside one control during the hover delay cause 0 embedded invalidations");

    // The first tick after animation was requested starts the view's clock; the next one passes every show delay.
    static_cast<void>(view.AdvanceAnimation(1'000u));
    static_cast<void>(view.AdvanceAnimation(11'000u));
    Require(view.Controls().HasTooltip(), "the embedded tooltip shows after the view's clock passes the show delay");
    const D2D1_RECT_F shownBounds = view.Controls().DebugGetTooltipBoundsDip();
    Require(view.Controls().ClearTooltip(), "reference: clear the shown embedded tooltip");
    const POINT last = TooltipHoverPathPixel(kTooltipHoverPathMoves);
    Require(view.Controls().SetTooltip(std::wstring(tooltipText), D2D1::Point2F(static_cast<float>(last.x), static_cast<float>(last.y))),
            "reference: embedded tooltip at the last origin");
    RequireRectNear(shownBounds, view.Controls().DebugGetTooltipBoundsDip(), "the embedded tooltip shows at the last pointer origin");
}

// ---- W3.3 tooltip deadlines (Specs/Rendering/Rendering_Win32Host.md, "Tooltip deadlines") ----------------------------

using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;

// A control whose Tick runs a test's function, so a test can act on the tooltip from inside a wake.
class TooltipTickProbe final : public DxUi::Control
{
public:
    TooltipTickProbe()                                   = default;
    TooltipTickProbe(const TooltipTickProbe&)            = delete;
    TooltipTickProbe& operator=(const TooltipTickProbe&) = delete;
    TooltipTickProbe(TooltipTickProbe&&)                 = delete;
    TooltipTickProbe& operator=(TooltipTickProbe&&)      = delete;
    ~TooltipTickProbe() override                         = default;
    void Paint(DxUi::ControlHost&) const override
    {
    }
    bool Tick(DxUi::ControlHost& host, uint64_t nowTickMs) override
    {
        ++ticks;
        if (onTick)
            onTick(host, nowTickMs);
        return false;
    }

    size_t ticks = 0u;
    std::function<void(DxUi::ControlHost&, uint64_t)> onTick;
};

[[nodiscard]] uint64_t TooltipTimerWakes(const WorkCounters& delta) noexcept
{
    return delta.wakes.animationTimer + delta.wakes.deadlineTimer;
}

// Pumps this thread's messages until `reached` holds or `limitMs` of real time passed, blocking between messages. The test
// window sits off every monitor, so the system posts WM_MOUSELEAVE for it right after a move: it is dropped, as the FX14
// window runner drops it, so the pointer rests over the control.
template <class Condition> [[nodiscard]] bool PumpWithoutLeaveUntil(HWND hwnd, uint64_t limitMs, Condition&& reached)
{
    const uint64_t end = GetTickCount64() + limitMs;
    for (uint64_t now = GetTickCount64(); now < end; now = GetTickCount64())
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0u, 0u, PM_REMOVE))
        {
            if (message.message == WM_MOUSELEAVE && message.hwnd == hwnd)
                continue;
            static_cast<void>(DispatchQueuedMessageForTest(message));
            if (reached())
                return true;
        }
        static_cast<void>(MsgWaitForMultipleObjectsEx(0u, nullptr, static_cast<DWORD>(end - now), QS_ALLINPUT, MWMO_INPUTAVAILABLE));
    }
    return reached();
}

void SendHoverPathMove(HWND hwnd, size_t move)
{
    const POINT pixel = TooltipHoverPathPixel(move);
    SendMessageW(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(pixel.x, pixel.y));
}

// G1 for the tooltip on a native window in real time: FX14's scene (the W1.6 hover path) on a WARP window shown without
// activation, real dispatcher wakes counted. Waiting for the show and while shown, the host's timer wakes once at each
// deadline: no periodic timer, no Tick between them.
void TestTooltipDeadlinesWakeOnlyAtShowAndHideInRealTime()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    host.DebugSetTooltipHoverTimeForTest(400u);
    constexpr std::wstring_view tooltipText = L"Real-time tooltip help";
    host.SetRoot(MakeTooltipHoverPathRoot(tooltipText));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    static_cast<void>(PumpWithoutLeaveUntil(window.Hwnd(), 300u, [] { return false; })); // first frames

    SendHoverPathMove(window.Hwnd(), 0u);
    Require(! host.HasTooltip() && host.DebugGetPendingTooltipText() == tooltipText, "entering the control schedules its tooltip");
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 2u, "the pending tooltip waits for a deadline, not every frame");
    WorkCounters waiting{};
    {
        const WorkCounterScope scope;
        for (size_t move = 1u; move <= kTooltipHoverPathMoves; ++move)
            SendHoverPathMove(window.Hwnd(), move);
        Require(PumpWithoutLeaveUntil(window.Hwnd(), 3'000u, [&host] { return host.HasTooltip(); }), "the tooltip shows after its delay");
        waiting = scope.Delta();
    }
    Require(host.GetTooltipText() == tooltipText, "the shown tooltip has its text");
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 2u, "the shown tooltip waits for its hide deadline, not every frame");
    WorkCounters shown{};
    {
        const WorkCounterScope scope;
        for (size_t move = 1u; move <= kTooltipHoverPathMoves; ++move)
            SendHoverPathMove(window.Hwnd(), move);
        Require(PumpWithoutLeaveUntil(window.Hwnd(), 7'000u, [&host] { return ! host.HasTooltip(); }), "the tooltip hides after its display lifetime");
        shown = scope.Delta();
    }
    std::cout << "    [W3.3] native, real time: waiting for the show " << waiting.wakes.deadlineTimer << " deadline wakes, " << waiting.wakes.animationTimer
              << " periodic wakes, " << waiting.frame.tooltipDeadlineTicks << " tooltip ticks, " << waiting.frame.tickCalls << " Tick calls; shown "
              << shown.wakes.deadlineTimer << " deadline wakes, " << shown.wakes.animationTimer << " periodic wakes, " << shown.frame.tooltipDeadlineTicks
              << " tooltip ticks, " << shown.frame.tickCalls << " Tick calls\n";
    Require(waiting.wakes.animationTimer == 0u && shown.wakes.animationTimer == 0u, "no periodic wake while waiting or while shown");
    Require(waiting.frame.tooltipDeadlineTicks == 1u && shown.frame.tooltipDeadlineTicks == 1u, "the tooltip is ticked once at each deadline");
    Require(waiting.frame.tickCalls == 0u && shown.frame.tickCalls == 0u, "no control is ticked between the deadlines");
    Require(TooltipTimerWakes(waiting) + TooltipTimerWakes(shown) <= 2u, "G1: at most 2 wakes per tooltip shown, one show and one hide");
    {
        const WorkCounterScope after;
        static_cast<void>(PumpWithoutLeaveUntil(window.Hwnd(), 1'000u, [] { return false; }));
        Require(TooltipTimerWakes(after.Delta()) == 0u, "nothing is armed after the hide");
    }
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);
}

// Decision table, deterministic on a test clock: every exit from a waiting mode withdraws its deadline, and a deadline
// reached in a mode that no longer waits for it does nothing.
void TestTooltipDeadlinesFollowTheWaitingModes()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    host.DebugSetTooltipHoverTimeForTest(400u);
    host.SetRoot(std::make_unique<Panel>());
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    uint64_t now = 100'000u;
    host.DebugSetAnimationClockForTest(now);
    const auto tickAt = [&](uint64_t at)
    {
        now = at;
        host.DebugSetAnimationClockForTest(now);
        static_cast<void>(host.DebugAnimationTickForTest(now));
    };
    const auto wake = [&host] { return host.DebugGetFrameScheduleWakeForTest(); };

    Require(wake().kind == 0u, "Hidden: nothing is armed");
    Require(host.SetTooltipDelayed(L"First", D2D1::Point2F(20.0f, 20.0f)), "Hidden -> ShowScheduled");
    Require(wake().kind == 2u && wake().dueTickMs == now + 400u && host.DebugHasActiveAnimationSubscription(), "ShowScheduled arms the show deadline");
    {
        const WorkCounterScope early;
        tickAt(now + 399u);
        Require(! host.HasTooltip() && early.Delta().frame.tooltipDeadlineTicks == 0u, "before its deadline the layer is not ticked");
    }
    // A pending tooltip replaced by another control's starts its own delay.
    Require(host.SetTooltipDelayed(L"Second", D2D1::Point2F(30.0f, 30.0f)), "ShowScheduled -> ShowScheduled (replaced)");
    Require(wake().dueTickMs == now + 400u, "the replacement re-arms the show deadline from now");
    Require(host.BeginTooltipHideDelay(), "ShowScheduled -> Hidden (pointer left during the delay)");
    Require(wake().kind == 0u && ! host.DebugHasActiveAnimationSubscription(), "leaving during the delay cancels the show deadline");
    {
        const WorkCounterScope stale;
        tickAt(now + 1'000u);
        Require(! host.HasTooltip() && stale.Delta().frame.tooltipDeadlineTicks == 0u, "a stale wake at the old deadline shows nothing");
    }

    Require(host.SetTooltipDelayed(L"Third", D2D1::Point2F(30.0f, 30.0f)), "Hidden -> ShowScheduled again");
    const uint64_t showDue = wake().dueTickMs;
    {
        const WorkCounterScope show;
        tickAt(showDue);
        Require(host.HasTooltip() && host.GetTooltipText() == L"Third" && show.Delta().frame.tooltipDeadlineTicks == 1u,
                "ShowScheduled -> VisiblePassive at the deadline");
    }
    Require(wake().kind == 2u && wake().dueTickMs == showDue + 5'000u, "VisibleHideScheduled arms the 5 s hide deadline");
    Require(host.BeginTooltipHideDelay(100u), "a pointer leave shortens the hide");
    Require(wake().dueTickMs == now + 100u, "the leave's hide delay replaces the display lifetime");
    Require(host.SetTooltip(L"Third", D2D1::Point2F(60.0f, 60.0f)), "a tracking update returns to VisiblePassive");
    Require(wake().kind == 0u, "a tracking tooltip has no hide deadline");
    Require(host.BeginTooltipHideDelay(100u) && wake().kind == 2u, "VisiblePassive -> VisibleHideScheduled");
    Require(host.ClearTooltip() && wake().kind == 0u, "a dismissal (click, capture, leave of the host) withdraws the hide deadline");

    Require(host.SetTooltipDelayed(L"Fourth", D2D1::Point2F(30.0f, 30.0f)), "ShowScheduled once more");
    Require(host.InspectTooltip(L"Inspection", D2D1::Point2F(30.0f, 30.0f)), "an inspection replaces the pending tooltip");
    Require(wake().kind == 0u, "an inspection waits for no deadline");
    Require(host.HandleTooltipInspectionKey(VK_ESCAPE) && ! host.HasTooltip() && wake().kind == 0u, "Escape ends the inspection, nothing armed");

    Require(host.SetTooltipDelayed(L"Fifth", D2D1::Point2F(30.0f, 30.0f)), "ShowScheduled before a capture change");
    SendMessageW(window.Hwnd(), WM_CAPTURECHANGED, 0, 0);
    Require(host.DebugGetPendingTooltipText().empty() && wake().kind == 0u, "a capture change clears the tooltip and its deadline");
    host.DebugSetAnimationClockForTest(std::nullopt);
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);
}

// Lifetime: the tooltip dismissed and re-armed from inside a control's Tick in the wake of its deadline; the supplemental
// target destroyed while the show waits; the host detached while the hide waits.
void TestTooltipDeadlinesAreSafeAcrossLifetimeChanges()
{
    using namespace DxUi;
    {
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        auto& host  = window.Host();
        auto root   = std::make_unique<Panel>();
        auto* probe = root->AddChild<TooltipTickProbe>();
        host.SetRoot(std::move(root));
        host.DebugSetTooltipHoverTimeForTest(400u);
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        uint64_t now = 200'000u;
        host.DebugSetAnimationClockForTest(now);
        Require(host.SetTooltipDelayed(L"Original", D2D1::Point2F(20.0f, 20.0f)), "the original tooltip waits to show");
        const uint64_t due = host.DebugGetFrameScheduleWakeForTest().dueTickMs;
        probe->onTick      = [](ControlHost& owner, uint64_t)
        {
            static_cast<void>(owner.ClearTooltip());
            static_cast<void>(owner.SetTooltipDelayed(L"Re-armed", D2D1::Point2F(40.0f, 40.0f)));
        };
        host.RequestAnimationAt(*probe, due);
        {
            const WorkCounterScope wake;
            now = due;
            host.DebugSetAnimationClockForTest(now);
            static_cast<void>(host.DebugAnimationTickForTest(now));
            Require(probe->ticks == 1u && wake.Delta().frame.tooltipDeadlineTicks == 0u && ! host.HasTooltip(),
                    "a tooltip dismissed and re-armed inside the wake of its deadline is not shown by the old deadline");
        }
        Require(host.DebugGetPendingTooltipText() == L"Re-armed" && host.DebugGetFrameScheduleWakeForTest().dueTickMs == due + 400u,
                "the re-armed tooltip waits for its own delay");
        probe->onTick = [](ControlHost& owner, uint64_t) { static_cast<void>(owner.ClearTooltip()); };
        host.RequestAnimationAt(*probe, due + 400u);
        {
            const WorkCounterScope wake;
            now = due + 400u;
            host.DebugSetAnimationClockForTest(now);
            static_cast<void>(host.DebugAnimationTickForTest(now));
            Require(wake.Delta().frame.tooltipDeadlineTicks == 0u && ! host.HasTooltip() && host.DebugGetFrameScheduleWakeForTest().kind == 0u,
                    "a tooltip dismissed inside the wake of its deadline is neither shown nor left armed");
        }
        host.DebugSetAnimationClockForTest(std::nullopt);
        host.DebugSetTooltipHoverTimeForTest(std::nullopt);
    }
    {
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        auto& host = window.Host();
        host.SetRoot(MakeTooltipHoverPathRoot(L"Destroyed target help"));
        host.DebugSetTooltipHoverTimeForTest(400u);
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        host.DebugSetAnimationClockForTest(300'000u);
        SendHoverPathMove(window.Hwnd(), 0u);
        Require(host.DebugGetPendingTooltipText() == L"Destroyed target help", "the target's tooltip waits to show");
        auto* panel = dynamic_cast<Panel*>(host.GetRoot());
        Require(panel != nullptr && panel->TakeChild(0u) != nullptr, "the target control is removed and destroyed");
        const WorkCounterScope wake;
        host.DebugSetAnimationClockForTest(301'000u);
        static_cast<void>(host.DebugAnimationTickForTest(301'000u));
        Require(! host.HasTooltip() && wake.Delta().frame.tooltipDeadlineTicks == 0u && host.DebugGetFrameScheduleWakeForTest().kind == 0u,
                "a destroyed target's pending tooltip never shows and leaves nothing armed");
        host.DebugSetAnimationClockForTest(std::nullopt);
        host.DebugSetTooltipHoverTimeForTest(std::nullopt);
    }
    {
        AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
        auto& host = window.Host();
        host.SetRoot(std::make_unique<Panel>());
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        host.DebugSetAnimationClockForTest(400'000u);
        Require(host.SetTooltipDelayed(L"Detached", D2D1::Point2F(20.0f, 20.0f)) && host.DebugAdvanceTooltipDelayForTest() && host.HasTooltip(),
                "the tooltip is shown and waits for its hide");
        Require(host.DebugHasActiveAnimationSubscription() && host.DebugGetFrameScheduleWakeForTest().kind == 2u, "the hide deadline is armed");
        host.Detach();
        Require(! host.HasTooltip() && ! host.DebugHasActiveAnimationSubscription() && host.DebugGetFrameScheduleWakeForTest().kind == 0u,
                "detach clears the tooltip and leaves the dispatcher");
        const WorkCounterScope after;
        static_cast<void>(host.DebugAnimationTickForTest(500'000u));
        Require(after.Delta().frame.tooltipDeadlineTicks == 0u, "nothing fires after detach");
        host.DebugSetAnimationClockForTest(std::nullopt);
    }
}

// Hidden hosts: a show still waiting when the host is hidden is dropped (its pointer is stale when the host returns); a
// shown tooltip's hide that came due while hidden hides it at the first wake after the show.
void TestTooltipDeadlinesOnHiddenHosts()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    host.SetRoot(std::make_unique<Panel>());
    host.DebugSetTooltipHoverTimeForTest(400u);
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    uint64_t now = 500'000u;
    host.DebugSetAnimationClockForTest(now);
    const auto tickAt = [&](uint64_t at)
    {
        now = at;
        host.DebugSetAnimationClockForTest(now);
        static_cast<void>(host.DebugAnimationTickForTest(now));
    };

    {
        const WorkCounterScope dropped;
        Require(host.SetTooltipDelayed(L"Presentation", D2D1::Point2F(20.0f, 20.0f)), "a show waits");
        Require(host.SetNativePresentationVisible(false) == S_OK, "the presentation is hidden");
        tickAt(now + 1'000u);
        Require(host.SetNativePresentationVisible(true) == S_OK, "the presentation is shown");
        tickAt(now + 16u);
        Require(! host.HasTooltip() && host.DebugGetPendingTooltipText().empty() && dropped.Delta().frame.tooltipShowsDropped == 1u &&
                    dropped.Delta().frame.tooltipDeadlineTicks == 0u,
                "a show pending when the presentation was hidden is dropped, not shown on return");
    }
    {
        const WorkCounterScope dropped;
        Require(host.SetTooltipDelayed(L"Minimized", D2D1::Point2F(20.0f, 20.0f)), "a show waits");
        ShowWindow(window.Hwnd(), SW_SHOWMINNOACTIVE);
        tickAt(now + 1'000u);
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        tickAt(now + 16u);
        Require(! host.HasTooltip() && dropped.Delta().frame.tooltipShowsDropped == 1u, "a show pending when the window was minimized is dropped");
    }
    {
        const WorkCounterScope dropped;
        Require(host.SetTooltipDelayed(L"Hidden window", D2D1::Point2F(20.0f, 20.0f)), "a show waits");
        ShowWindow(window.Hwnd(), SW_HIDE);
        tickAt(now + 1'000u); // the show deadline's wake finds the window hidden
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        tickAt(now + 16u);
        Require(! host.HasTooltip() && dropped.Delta().frame.tooltipShowsDropped == 1u && dropped.Delta().frame.tooltipDeadlineTicks == 0u,
                "a show whose deadline came while the window was hidden is dropped at that wake");
    }
    {
        Require(host.SetTooltipDelayed(L"Held hide", D2D1::Point2F(20.0f, 20.0f)) && host.DebugAdvanceTooltipDelayForTest() && host.HasTooltip(),
                "a tooltip is shown");
        const uint64_t hideDue = host.DebugGetFrameScheduleWakeForTest().dueTickMs;
        Require(host.SetNativePresentationVisible(false) == S_OK, "the presentation is hidden while the hide waits");
        tickAt(hideDue + 100u);
        Require(host.HasTooltip(), "a hidden host holds the hide");
        const WorkCounterScope shown;
        Require(host.SetNativePresentationVisible(true) == S_OK && shown.Delta().frame.tickHeldWhileHidden == 1u, "the expired hide was held");
        tickAt(now + 16u);
        Require(! host.HasTooltip() && shown.Delta().frame.tooltipDeadlineTicks == 1u && host.DebugGetFrameScheduleWakeForTest().kind == 0u,
                "and hides the tooltip at the first wake after the show");
    }
    host.DebugSetAnimationClockForTest(std::nullopt);
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);

    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "create the hidden-view WARP device");
    std::shared_ptr<GraphicsDevice> pool;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), pool), "share the hidden-view graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(pool), "attach the hidden view");
    view.Controls().SetRoot(std::make_unique<Panel>());
    view.Controls().DebugSetTooltipHoverTimeForTest(400u);
    RequireSucceeded(view.Prepare(200u, 120u), "prepare the hidden view");
    static_cast<void>(view.AdvanceAnimation(1'000u)); // discovery
    const WorkCounterScope dropped;
    Require(view.Controls().SetTooltipDelayed(L"Embedded", D2D1::Point2F(20.0f, 20.0f)) && view.NeedsAnimation(), "an embedded show waits");
    view.SetVisible(false);
    view.SetVisible(true);
    Require(! view.NeedsAnimation() && view.Controls().DebugGetPendingTooltipText().empty() && dropped.Delta().frame.tooltipShowsDropped == 1u,
            "hiding the view drops the pending show");
    Require(view.Controls().SetTooltipDelayed(L"Embedded held hide", D2D1::Point2F(20.0f, 20.0f)), "a new embedded show waits");
    static_cast<void>(view.AdvanceAnimation(2'000u));
    static_cast<void>(view.AdvanceAnimation(2'000u + 400u));
    Require(view.Controls().HasTooltip(), "the embedded tooltip shows");
    view.SetVisible(false);
    view.SetVisible(true);
    static_cast<void>(view.AdvanceAnimation(60'000u)); // the timeline does not count the hidden time: this sets its baseline
    Require(view.Controls().HasTooltip(), "the hide keeps its place on the view's timeline");
    static_cast<void>(view.AdvanceAnimation(60'000u + 5'000u));
    Require(! view.Controls().HasTooltip() && ! view.NeedsAnimation(), "the hide fires when the timeline reaches it");
    view.Controls().DebugSetTooltipHoverTimeForTest(std::nullopt);
}

// The show delay is the user's mouse hover time, read once per host, clamped to 100-2,500 ms and read again on a settings
// change; tests replace the setting and never change it.
void TestTooltipHoverTimeIsReadOnceAndRefreshedOnSettingChange()
{
    using namespace DxUi;
    AttachedHostWindow window(WindowHost::PresentationMode::HwndSwapChain, true);
    auto& host = window.Host();
    host.DebugSetTooltipHoverTimeForTest(400u);
    Require(host.DebugGetTooltipShowDelayMsForTest() == 400u, "the first read takes the hover time");
    host.DebugSetTooltipHoverTimeForTest(1'000u);
    Require(host.DebugGetTooltipShowDelayMsForTest() == 400u, "later tooltips use the value read once, not a new read");
    SendMessageW(window.Hwnd(), WM_SETTINGCHANGE, SPI_SETMOUSEHOVERTIME, 0);
    Require(host.DebugGetTooltipShowDelayMsForTest() == 1'000u, "WM_SETTINGCHANGE reads the hover time again");
    host.DebugSetAnimationClockForTest(10'000u);
    Require(host.SetTooltipDelayed(L"Delay", D2D1::Point2F(10.0f, 10.0f)) && host.DebugGetFrameScheduleWakeForTest().dueTickMs == 11'000u,
            "the next tooltip waits for the refreshed delay");
    static_cast<void>(host.ClearTooltip());
    host.DebugSetAnimationClockForTest(std::nullopt);
    for (const auto& [setting, delay] : {std::pair<UINT, uint64_t>{20u, 100u}, std::pair<UINT, uint64_t>{9'000u, 2'500u}, std::pair<UINT, uint64_t>{0u, 500u}})
    {
        host.DebugSetTooltipHoverTimeForTest(setting);
        host.RefreshTooltipHoverTime();
        Require(host.DebugGetTooltipShowDelayMsForTest() == delay, "the show delay is clamped to 100-2,500 ms, 500 ms when unknown");
    }
    host.DebugSetTooltipHoverTimeForTest(std::nullopt);

    GraphicsFixture gpu;
    RequireSucceeded(gpu.Create(), "create the hover-time WARP device");
    std::shared_ptr<GraphicsDevice> pool;
    RequireSucceeded(GraphicsDevice::Create(gpu.device.get(), pool), "share the hover-time graphics");
    EmbeddedHost view;
    RequireSucceeded(view.Attach(pool), "attach the hover-time view");
    view.Controls().DebugSetTooltipHoverTimeForTest(300u);
    Require(view.Controls().DebugGetTooltipShowDelayMsForTest() == 300u, "an embedded host reads the hover time once");
    view.Controls().DebugSetTooltipHoverTimeForTest(700u);
    Require(view.Controls().DebugGetTooltipShowDelayMsForTest() == 300u, "and keeps it");
    view.Controls().RefreshTooltipHoverTime();
    Require(view.Controls().DebugGetTooltipShowDelayMsForTest() == 700u, "until its application relays a settings change");
    view.Controls().DebugSetTooltipHoverTimeForTest(std::nullopt);
}

} // namespace

void RunTooltipTests()
{
    DXUI_RUN_TEST(TestTooltipLayerTrackingUpdateReusesVisibleTooltip);
    DXUI_RUN_TEST(TestTooltipLayerPrefersBelowRightWhenSpaceAllows);
    DXUI_RUN_TEST(TestTooltipLayerFlipsAboveNearBottomEdge);
    DXUI_RUN_TEST(TestTooltipLayerFlipsLeftNearRightEdge);
    DXUI_RUN_TEST(TestTooltipLayerWrapsLongTextAndStaysClamped);
    DXUI_RUN_TEST(TestTooltipLayerClampsBothAxesInTinyViewport);
    DXUI_RUN_TEST(TestTooltipInspectionScrollsWithoutChangingFocusOrValue);
    DXUI_RUN_TEST(TestTooltipInspectionMirrorsMixedScriptLayoutAndPlacement);
    DXUI_RUN_TEST(TestTooltipInspectionOutsideDismissalConsumesWholeGesture);
    DXUI_RUN_TEST(TestTooltipInspectionCaptureCallbackWinsReentrantOpen);
    DXUI_RUN_TEST(TestTooltipLayerHideDelayExpiresAfterTimerTicks);
    DXUI_RUN_TEST(TestTooltipDeadlinesUseCurrentDispatcherClockAfterIdleHostTick);
    DXUI_RUN_TEST(TestTooltipLayerTrackingMoveCancelsPendingHideDelay);
    DXUI_RUN_TEST(TestGridTooltipTracksPointerWithinSameCell);
    DXUI_RUN_TEST(TestInteractiveTooltipSurvivesEmptySupplementalTargetPass);
    DXUI_RUN_TEST(TestTreeTooltipStaysAtRowEntryOriginWithinSameRow);
    DXUI_RUN_TEST(TestTreeTooltipFallsBackToClippedItemText);
    DXUI_RUN_TEST(TestPassiveSupplementalTooltipUsesHostHitTestingDelayLifetimeAndClickThrough);
    DXUI_RUN_TEST(TestTooltipPendingOriginMovesDoNotInvalidateNativeHost);
    DXUI_RUN_TEST(TestTooltipPendingOriginMovesDoNotInvalidateEmbeddedHost);
    DXUI_RUN_TEST(TestTooltipDeadlinesFollowTheWaitingModes);
    DXUI_RUN_TEST(TestTooltipDeadlinesAreSafeAcrossLifetimeChanges);
    DXUI_RUN_TEST(TestTooltipDeadlinesOnHiddenHosts);
    DXUI_RUN_TEST(TestTooltipHoverTimeIsReadOnceAndRefreshedOnSettingChange);
    DXUI_RUN_TEST(TestTooltipDeadlinesWakeOnlyAtShowAndHideInRealTime);
}
