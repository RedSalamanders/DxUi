#include "Controls.Tests.UnseenMotion.h"

#include "Controls.Tests.DxUiTestHelpers.h"
#include "Support/AnimationDispatcher.h"
#include "Support/WorkCounters.h"

#include <iostream>
#include <memory>

// Unseen motion on the window host (W3.4). Every clause that needs no real-time pacing runs on the host's test clock; the
// hitch clause runs on the dispatcher's own clock, since the clamp under test is that clock's.
namespace
{
using DxUi::Detail::WorkCounters;
using DxUi::Detail::WorkCounterScope;

// A window host with a ScrollPanel whose content holds an indeterminate ProgressBar, a Slider and a focusable TextField.
struct ScrolledScene
{
    AttachedHostWindow window{DxUi::WindowHost::PresentationMode::HwndSwapChain, true};
    DxUi::ScrollPanel* scroll                      = nullptr;
    DxUi::ProgressBar* bar                         = nullptr;
    DxUi::Slider* slider                           = nullptr;
    DxUi::TextField* field                         = nullptr;
    uint64_t clock                                 = 50'000u;
    ScrolledScene(const ScrolledScene&)            = delete;
    ScrolledScene& operator=(const ScrolledScene&) = delete;
    ScrolledScene(ScrolledScene&&)                 = delete;
    ScrolledScene& operator=(ScrolledScene&&)      = delete;
    explicit ScrolledScene(bool withBar)
    {
        DxUi::WindowHost& host = window.Host();
        host.DebugSetAnimationClockForTest(clock);
        auto root = std::make_unique<DxUi::Panel>();
        scroll    = root->AddChild<DxUi::ScrollPanel>();
        scroll->SetBounds(D2D1::RectF(0.0f, 0.0f, 280.0f, 140.0f));
        if (withBar)
        {
            bar = scroll->AddChild<DxUi::ProgressBar>();
            bar->SetBounds(D2D1::RectF(8.0f, 8.0f, 250.0f, 24.0f));
        }
        slider = scroll->AddChild<DxUi::Slider>();
        slider->SetBounds(D2D1::RectF(8.0f, 40.0f, 250.0f, 72.0f));
        slider->SetMinimum(0.0);
        slider->SetMaximum(100.0);
        slider->SetValue(20.0);
        field = scroll->AddChild<DxUi::TextField>(L"caret");
        field->SetBounds(D2D1::RectF(8.0f, 84.0f, 250.0f, 116.0f));
        scroll->SetContentHeight(1'200.0f);
        host.SetRoot(std::move(root));
        ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
        if (bar)
            bar->SetIndeterminate(true);
        Render();
    }
    ~ScrolledScene()
    {
        window.Host().DebugSetAnimationClockForTest(std::nullopt);
    }
    void TickAt(uint64_t at)
    {
        clock = at;
        window.Host().DebugSetAnimationClockForTest(clock);
        static_cast<void>(window.Host().DebugAnimationTickForTest(clock));
    }
    // One whole frame, rendered now (the catch-up runs in it).
    void Render()
    {
        window.PumpMessages(20u);
        DxUi::WindowHostBitmapCapture capture;
        Require(window.Host().DebugCaptureBitmap(capture, true), "the unseen-motion window renders a frame");
    }
};

template <class Condition> [[nodiscard]] bool PumpUntilCondition(Condition&& reached)
{
    constexpr ULONGLONG kLimitMs = 20'000u;
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
            return true;
    }
    return false;
}
} // namespace

// A loop scrolled out of its ScrollPanel's viewport is held: its host schedules no tick and wakes nothing (0 Tick calls).
// Scrolled back, the frame that shows it ticks it first, at the frame's time, so it paints in phase with a bar that ticked
// all along on the same clock; after that frame the host waits on the dispatcher for the next one with no further input.
void TestUnseenMotionNativeLoopHoldsAndCatchesUpBeforePaint()
{
    ScrolledScene scene(true);
    ScrolledScene reference(true);
    DxUi::WindowHost& host         = scene.window.Host();
    const DxUi::ThemePalette theme = host.GetTheme();
    for (uint64_t at = 50'016u; at <= 50'160u; at += 16u)
    {
        scene.TickAt(at);
        reference.TickAt(at);
    }
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 1u, "the seen loop asks for every frame");

    uint64_t heldUnseen = 0u;
    {
        const WorkCounterScope scrolled;
        scene.scroll->SetScrollOffset(600.0f);
        scene.Render();
        heldUnseen = scrolled.Delta().frame.tickHeldUnseen;
    }
    for (uint64_t at = 50'176u; at <= 50'716u; at += 16u)
    {
        {
            // The counters are the thread's: the scene's wake is measured alone, the reference's outside the scope.
            const WorkCounterScope out;
            scene.TickAt(at);
            const WorkCounters work = out.Delta();
            Require(work.frame.tickCalls == 0u, "W3.4: 0 ticks from a loop scrolled out of view");
        }
        reference.TickAt(at);
    }
    Require(heldUnseen == 1u, "the scrolled-out loop is found unseen once, by the frame that scrolled it out");
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 0u && ! host.DebugHasActiveAnimationSubscription(),
            "W3.4: a host whose only motion is unseen holds no wake and no dispatcher subscription");

    scene.scroll->SetScrollOffset(0.0f);
    const WorkCounterScope shown;
    scene.Render();
    const WorkCounters work = shown.Delta();
    Require(work.frame.tickCatchUps == 1u && work.frame.catchUpLayoutRepeats == 0u, "W3.4: the frame that shows the loop ticks it once first");
    reference.Render();
    const D2D1_RECT_F caught   = scene.bar->DebugGetIndeterminateSegmentRect(theme);
    const D2D1_RECT_F expected = reference.bar->DebugGetIndeterminateSegmentRect(theme);
    Require(caught.left == expected.left && caught.right == expected.right, "W3.4: the shown loop is in phase with one that ticked all along");
    Require(host.DebugGetFrameScheduleWakeForTest().kind == 1u && host.DebugHasActiveAnimationSubscription(),
            "W3.4: after the catch-up frame the host is subscribed for the next frame without other input");
    std::cout << "  native unseen loop: " << work.frame.tickCatchUps << " catch-up tick(s), held from " << 50'176u << " to " << 50'716u << " ms\n";
}

// W3.4 answer 3(a): the disclosure chevron runs on the host's clock; a stall while it is seen does not jump it, because the
// native clock adds at most its hitch clamp for one frame. A stall longer than the whole rotation leaves it unfinished.
void TestUnseenMotionNativeDisclosureDoesNotJumpOnAHitchWhileSeen()
{
    auto& dispatcher = DxUi::Ui::AnimationDispatcher::GetInstance();
    AttachedHostWindow window(DxUi::WindowHost::PresentationMode::HwndSwapChain, true);
    auto root    = std::make_unique<DxUi::Panel>();
    auto* button = root->AddChild<DxUi::Button>(L"Details");
    button->SetBounds(D2D1::RectF(8.0f, 8.0f, 200.0f, 40.0f));
    button->SetDisclosureExpanded(false);
    window.Host().SetRoot(std::move(root));
    ShowWindow(window.Hwnd(), SW_SHOWNOACTIVATE);
    window.PumpMessages(50u);
    button->SetDisclosureExpanded(true);
    uint64_t ticks = DxUi::Detail::Work().frame.tickCalls;
    Require(PumpUntilCondition([&] { return DxUi::Detail::Work().frame.tickCalls != ticks; }), "the rotation ticks");
    const float before = button->DebugGetDisclosureAnimationProgress();
    Require(before < 1.0f, "the rotation is in flight");
    const uint64_t clampMs = dispatcher.DebugGetHitchClampUsForTest() / 1'000u;
    // A stall twice the rotation's whole 240 ms: an unclamped clock would finish it at the next tick.
    Sleep(static_cast<DWORD>((std::max)(clampMs * 4u, uint64_t{480u})));
    ticks = DxUi::Detail::Work().frame.tickCalls;
    Require(PumpUntilCondition([&] { return DxUi::Detail::Work().frame.tickCalls != ticks; }), "the rotation ticks after the stall");
    const float after = button->DebugGetDisclosureAnimationProgress();
    std::cout << "  native disclosure across a stall: " << before << " -> " << after << " (clamp " << clampMs << " ms)\n";
    Require(after > before && after < 1.0f, "W3.4: a stall while seen moves the chevron by at most the clock's hitch clamp, not the stall");
}

// W3.4 additions B and answer 2, measured (not gated): what an unseen transition's landing and an unseen caret's flips cost
// today on the window host, whose every invalidation still marks the whole window (W4.9).
void TestUnseenMotionNativeCostOfUnseenWork()
{
    {
        ScrolledScene scene(false);
        DxUi::WindowHost& host = scene.window.Host();
        scene.scroll->SetScrollOffset(600.0f);
        scene.Render();
        Require(scene.slider->RequestValue(host, 80.0), "an unseen value animation starts");
        const auto wake = host.DebugGetFrameScheduleWakeForTest();
        Require(wake.kind == 2u, "an unseen transition waits for one deadline: its settle time");
        {
            const WorkCounterScope early;
            scene.TickAt(wake.dueTickMs - 1u);
            Require(early.Delta().frame.tickCalls == 0u, "nothing ticks before the settle time");
        }
        const WorkCounterScope landing;
        scene.TickAt(wake.dueTickMs);
        scene.window.PumpMessages(100u);
        const WorkCounters work = landing.Delta();
        Require(work.frame.tickCalls == 1u && scene.slider->DebugGetDisplayedValue() == 80.0, "W3.4: one Tick at the settle time lands it");
        Require(host.DebugGetFrameScheduleWakeForTest().kind == 0u, "and nothing is left scheduled");
        std::cout << "  native unseen landing: " << work.frame.tickCalls << " tick, " << work.frame.invalidations << " invalidation(s), " << work.frame.renders
                  << " render(s), " << work.frame.presents << " present(s)\n";
    }
    {
        ScrolledScene scene(false);
        DxUi::WindowHost& host = scene.window.Host();
        host.DebugSetCaretBlinkSettingsForTest(DxUi::ControlHost::CaretBlinkSettingsForTest{530u, 0u});
        host.SetFocusControl(scene.field, false);
        scene.scroll->SetScrollOffset(600.0f);
        scene.Render();
        const WorkCounterScope flips;
        size_t flipCount = 0u;
        for (; flipCount < 10u; ++flipCount)
        {
            const auto wake = host.DebugGetFrameScheduleWakeForTest();
            if (wake.kind != 2u)
                break;
            scene.TickAt(wake.dueTickMs);
            scene.window.PumpMessages(50u);
        }
        const WorkCounters work = flips.Delta();
        Require(flipCount == 10u, "an unseen focused caret keeps blinking (W3.2's rules are untouched by W3.4)");
        std::cout << "  native unseen caret: " << flipCount << " flips at 530 ms (" << (1'000.0 / 530.0) << " wakes/s): " << work.frame.tickCalls << " ticks, "
                  << work.frame.invalidations << " invalidations, " << work.frame.renders << " renders, " << work.frame.presents << " presents\n";
        host.SetFocusControl(nullptr, false);
        host.DebugSetCaretBlinkSettingsForTest(std::nullopt);
    }
}
